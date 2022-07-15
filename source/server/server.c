#include "server.h"
#include "ldlog.h"
#include "tick.h"
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "servercfg.h"
#include "h3.h"
#include "ciper.h"

extern int h_errno;
static agent_server_t g_agent_server;
static uint8_t g_agent_server_cnt = 0;
static int g_agent_epfd = -1;
static struct epoll_event g_agent_ev[LD_MAX_PROXY_PORT_NUM];
//static unsigned int g_send_buffer_size = LD_BUFFER_SIZE_1k * 256;
static uint64_t g_sess_timeout_ticks = 0;
static uint32_t g_sess_max_timeout = 60000;
uint64_t g_cur_ms = 0;
int g_session_epfd = -1;
uint32_t g_max_session_events = 2 * LD_DEFAULT_SESSION; //todo可配 session*2
struct epoll_event* g_session_events = NULL;
static char g_data_cache[LD_BUFFER_SIZE_2k]; //从fd最近读取的数据
proxy_config_t g_proxyser_cfg = {0};

/**
 * 下面三个宏主要是用于区分是服务端fd和客户端fd触发事件
 * 由于指针的最低位总是0，因此最低位为1是表示是服务端fd触发事件
 * 最低位为0时表示客户端触发事件
 *
 * SET_SVR_FD_FLAG 设置指针最低位为1
 * SET_CLI_FD_FLAG 设置指针最低位为0
 * GET_SEESION_PTR 获取正确的指针
 */
#define SET_SVR_FD_FLAG(ptr) (((unsigned long)ptr) | 0x1)
#define SET_CLI_FD_FLAG(ptr) ((unsigned long)ptr)
#define GET_SEESION_PTR(ptr) (void*)(((unsigned long)ptr) & -2L)
#define SVRFDFLAG_ISSET(ptr) (((unsigned long)ptr) & 0x1)

static int create_agent_server(agent_server_t *server);
static void close_agent_server(agent_server_t *server);
static int handle_req_form_cli(void *ev_ptr);
static int session_timeout(void* val, void* data);
static inline void epoll_session_mod(ld_session_t* session);
static void __on_session_close(ld_session_t* session, int* cli_fd, int* svr_fd, LISTHEAD* from_cli_data_list, LISTHEAD* from_svr_data_list);
static void on_session_close(ld_session_t* session);
static void on_session_err(ld_session_t* session);
static int on_data_relay(ld_session_t* session, struct epoll_event* session_event, int* cli_fd, int* svr_fd, LISTHEAD* from_cli_data_list, LISTHEAD* from_svr_data_list);
static int on_parse_header(ld_session_t* session, struct epoll_event* session_event);
static int on_process_header(ld_session_t* session);

static int on_process_header(ld_session_t* session) {
    assert(session);
    if (!strncasecmp((const char*)session->h->RequestMethod, "connect", 7)) {
        log_debug("on_process_header connect\n");
        const char *response = "HTTP/1.1 200 Connection Established\r\nConnection: Close\r\n\r\n";
        int res_len = strlen(response);
        buffer_t* packet = malloc(sizeof(buffer_t) + res_len);
        if (packet == NULL) {
            log_error("packet malloc failed!\n");
            on_session_err(session);
            return -1;
        }
        INIT_LIST_HEAD(&(packet->list));
        packet->size = res_len;
        packet->len = res_len;
        packet->off = 0;
        memcpy(packet->data, response, res_len);
        log_debug("encode from svr data, session:%p", session);
        ciper_encode((unsigned char*)packet->data, packet->len);
        list_add_tail(&(packet->list), &session->from_svr_data.data_list);
    } else {
        log_debug("on_process_header !connect\n");
        buffer_t* packet = malloc(sizeof(buffer_t) + session->req_head_cache_len);
        if (packet == NULL) {
            log_error("packet malloc failed!\n");
            on_session_err(session);
            return -1;
        }
        INIT_LIST_HEAD(&(packet->list));
        packet->size = session->req_head_cache_len;
        packet->len = session->req_head_cache_len;
        packet->off = 0;
        memcpy(packet->data, session->req_head_cache, session->req_head_cache_len);
        //req_head_cache 里面的数据已经解密过了
        //log_debug("decode from cli data, session:%p", session);
        //ciper_decode((unsigned char*)packet->data, packet->len);
        list_add_tail(&(packet->list), &session->from_cli_data.data_list);

    }
    return 0;
}

//处理请求头
static int on_parse_header(ld_session_t* session, struct epoll_event* session_event) {
    assert(session && session_event);
    //只处理cli_fd的读事件
    if (session_event->events & EPOLLIN) {
         errno = 0;
        int rdlen = 0;
        rdlen = read(session->cli_fd, session->req_head_cache + session->req_head_cache_len, LD_BUFFER_SIZE_8k - session->req_head_cache_len);
        if (rdlen < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                log_debug("%s some err ocur retry next time, %s cli_fd:%d svr_fd:%d\n",
                    session->event_fd == CLI_FD ? "client" : "server",
                    strerror(errno), session->cli_fd, session->svr_fd);
                return -2;
            }
            log_error("%s adnormal closed http connection, %s cli_fd:%d svr_fd:%d\n",
                session->event_fd == CLI_FD ? "client" : "server",
                strerror(errno), session->cli_fd, session->svr_fd);
            on_session_err(session);
            return -1;
        } else if (rdlen == 0) {
            log_debug("%s normal closed http connection, %s cli_fd:%d svr_fd:%d\n",
                session->event_fd == CLI_FD ? "client" : "server",
                strerror(errno), session->cli_fd, session->svr_fd);
            if (session->event_fd == CLI_FD) {
                session->cli_event_status = CLI_EVENT_STATUS_CLOSE;
            } else {
                session->svr_event_status = SVR_EVENT_STATUS_CLOSE;
            }
            
        } else { //rdlen > 0
            log_debug("decode header part len: %d, session: %p\n", rdlen, session);
            ciper_decode((unsigned char*)session->req_head_cache + session->req_head_cache_len, rdlen);
            session->req_head_cache_len += rdlen;
            if (session->req_head_cache_len < 4) {
                log_debug("header parse continue! session:%p\n", session);
                return -2; //continue
            }
            char* end_pos = strstr((const char*)session->req_head_cache, "\r\n\r\n");
            if (end_pos == NULL && session->req_head_cache_len == LD_BUFFER_SIZE_8k) {
                log_debug("header parse error! session:%p\n", session);
                return -1; //error
            } else if (end_pos == NULL) {
                log_debug("header parse continue! session:%p\n", session);
                return -2; //continue
            }
            if (end_pos != NULL) {
                log_debug("header parse end! session:%p, header: %s\n", session, session->req_head_cache);
                int ret = h3_request_header_parse(session->h, (const char *)session->req_head_cache, LD_BUFFER_SIZE_8k);
                if (ret == 0) {
                    log_debug("header parse success! session:%p\n", session);
                    return 0;
                } else {
                    log_debug("header parse error! session:%p\n", session);
                    return -1;
                }
            }

        }    
    }
    return -2; //continue
}
//处理cli_fd上的事件
static int on_data_relay(ld_session_t* session, struct epoll_event* session_event, int* cli_fd, int* svr_fd, LISTHEAD* from_cli_data_list, LISTHEAD* from_svr_data_list) {
    assert(session && session_event && from_cli_data_list && from_svr_data_list);
    log_debug("on_data_relay session:%p\n",session);
     //cli_fd 可写
    if (session_event->events & EPOLLOUT) {
        log_debug("on_data_relay write event, session:%p\n",session);
        int wrlen = 0;
        while (!list_empty_safe(from_svr_data_list)) {
            //写数据
            //写失败 on_session_error
            log_debug("on_data_relay datalist not empty, session:%p\n",session);
            buffer_t* packet;
            packet = list_entry(from_svr_data_list->next, typeof(*packet), list);
            errno = 0;
            wrlen = write(*cli_fd, packet->data + packet->off, packet->len - packet->off);
            if (wrlen == packet->len - packet->off) {
                list_del(&packet->list);
                free(packet);
            } else if ((wrlen >= 0) && (wrlen < packet->len - packet->off)) {
                packet->off +=  wrlen;
                break;
            } else { //wrlen < 0
                if (errno == EINTR || errno == EAGAIN) {
                    log_debug("%s some err ocur retry next time, %s cli_fd:%d svr_fd:%d\n",
                        session->event_fd == CLI_FD ? "client" : "server",
                        strerror(errno), session->cli_fd, session->svr_fd);
                    break;
                }
                log_error("%s adnormal closed http connection, %s cli_fd:%d svr_fd:%d\n",
                    session->event_fd == CLI_FD ? "client" : "server",
                    strerror(errno), session->cli_fd, session->svr_fd);
                on_session_err(session);//直接关掉session
                return -1; 
            }
        }

        //svr_fd已经关闭了，从svr接收的数据也全部发送完了
        if (list_empty_safe(from_svr_data_list) && *svr_fd == -1) {
            //关闭session
            free_server_session(session);
            log_debug("%s closed http connection, %s cli_fd:%d svr_fd:%d\n",
                    session->event_fd == CLI_FD ? "client" : "server",
                    strerror(errno), session->cli_fd, session->svr_fd);
            return 0;
        }
    }
    //cli_fd可读
    if (session_event->events & EPOLLIN) {
        log_debug("on_data_relay read event, session:%p\n",session);
        //读失败 on_session_error
        //读 0 session->cli_event_status == CLI_EVENT_STATUS_CLOSE 
        errno = 0;
        int rdlen = 0;
        rdlen = read(*cli_fd, g_data_cache, sizeof(g_data_cache));
        if (rdlen < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                log_debug("%s some err ocur retry next time, %s cli_fd:%d svr_fd:%d\n",
                    session->event_fd == CLI_FD ? "client" : "server",
                    strerror(errno), session->cli_fd, session->svr_fd);
                return 0;
            }
            log_error("%s adnormal closed http connection, %s cli_fd:%d svr_fd:%d\n",
                session->event_fd == CLI_FD ? "client" : "server",
                strerror(errno), session->cli_fd, session->svr_fd);
            on_session_err(session);
            return -1;
        } else if (rdlen == 0) {
            log_debug("%s normal closed http session :%p, %s cli_fd:%d svr_fd:%d\n",
                session->event_fd == CLI_FD ? "client" : "server", session,
                strerror(errno), session->cli_fd, session->svr_fd);
            if (session->event_fd == CLI_FD) {
                session->cli_event_status = CLI_EVENT_STATUS_CLOSE;
            } else {
                session->svr_event_status = SVR_EVENT_STATUS_CLOSE;
            }
            
        } else { //rdlen > 0
            buffer_t* packet = malloc(sizeof(buffer_t) + rdlen);
            if (packet == NULL) {
                log_error("packet malloc failed!\n");
                on_session_err(session);
                return -1;
            }
            INIT_LIST_HEAD(&(packet->list));
            packet->size = rdlen;
            packet->len = rdlen;
            packet->off = 0;
            memcpy(packet->data, g_data_cache, rdlen);
            if (session->event_fd == CLI_FD) {
                log_debug("decode from cli data, session:%p\n", session);
                ciper_decode((unsigned char*)packet->data, packet->len);
            } else {
                log_debug("encode from svr data, session:%p\n", session);
                ciper_encode((unsigned char*)packet->data, packet->len);
            }
            list_add_tail(&(packet->list), from_cli_data_list);
        }    
    }
    return 0;
}

static void __on_session_close(ld_session_t* session, int* cli_fd, int* svr_fd, LISTHEAD* from_cli_data_list, LISTHEAD* from_svr_data_list) {
    assert(session);
    if (*cli_fd > 0) {
        //待发送的数据要清理掉
        while (!list_empty_safe(from_svr_data_list)) {
            buffer_t* packet;
            packet = list_entry(from_svr_data_list->next, typeof(*packet), list);
            list_del(&packet->list);
            free(packet);
        }
        epoll_ctl(g_session_epfd, EPOLL_CTL_DEL, *cli_fd, NULL);
        close(*cli_fd);
        *cli_fd = -1;
    }
    if (*svr_fd > 0 && !list_empty_safe(from_cli_data_list)) {
        struct epoll_event ev;
        ev.events = EPOLLOUT;
        ev.data.ptr = (void*)SET_SVR_FD_FLAG(session);
        epoll_ctl(g_session_epfd, EPOLL_CTL_MOD, *svr_fd, &ev);
    } else {
        free_server_session(session);
    }
}

//只有一边fd转到close状态才能调用此函数
static void on_session_close(ld_session_t* session) {
    assert(session);
    if (!session->flag) {
        return;
    }
    log_debug("on_session_close session:%p\n",session);
    if ((session->event_fd == CLI_FD) && (session->cli_event_status == CLI_EVENT_STATUS_CLOSE)) {
        __on_session_close(session, &session->cli_fd, &session->svr_fd, &session->from_cli_data.data_list, &session->from_svr_data.data_list);
    }
    if ((session->event_fd == SVR_FD) && (session->svr_event_status == SVR_EVENT_STATUS_CLOSE)) {
        __on_session_close(session, &session->svr_fd, &session->cli_fd, &session->from_svr_data.data_list, &session->from_cli_data.data_list);
    }
}

static void on_session_err(ld_session_t* session) {
    assert(session);
    log_debug("on_session_err session:%p\n",session);
    free_server_session(session);
}

static inline void epoll_session_mod(ld_session_t* session) {
    assert(session);
    if (!session->flag) {
        return;
    }
    struct epoll_event ev1, ev2;
    ev1.events = EPOLLIN;
    ev2.events = EPOLLIN;
    ev1.data.ptr = (void*)SET_CLI_FD_FLAG(session);
    if (!list_empty_safe(&session->from_svr_data.data_list)) {
        ev1.events |= EPOLLOUT;
    }

    ev2.data.ptr = (void*)SET_SVR_FD_FLAG(session);
    if (!list_empty_safe(&session->from_cli_data.data_list)) {
        ev2.events |= EPOLLOUT;
    }

    if (session->cli_fd != -1) 
	{
        epoll_ctl(g_session_epfd, EPOLL_CTL_MOD, session->cli_fd, &ev1);
    }

    if (session->svr_fd != -1)
    {
        epoll_ctl(g_session_epfd, EPOLL_CTL_MOD, session->svr_fd, &ev2);
    }
}
   

 int handle_connections_event() {
    int i = 0;
    ld_session_t* session = NULL;
    int epfds = epoll_wait(g_session_epfd, g_session_events, g_max_session_events, LD_EPOLL_TIMEOUT_MS);
    if (epfds < 0) {
        if (errno == EINTR) {
            log_debug("errno == EINTR\n");
            return 0;
        }
        log_fault("epoll_wait error:%d\n", errno);
        return -1;
    }

    for (i = 0; i < epfds; ++i) {
        if (!(g_session_events[i].events & (EPOLLIN | EPOLLOUT))) {
            //没有读写事件,则设置为读写,通过上层读写来判断链接是否异常。
            g_session_events[i].events = (EPOLLIN | EPOLLOUT);
        }
        //获取会话指针
        session = (ld_session_t*)GET_SEESION_PTR(g_session_events[i].data.ptr);
        log_debug("handle_connections_event session:%p, i:%d\n", session, i);
        if (!session) {
            log_debug("BUG session is NULL\n");
            continue;
        }
        if (session->flag == 0) {
            log_debug("BUG session have close\n");
            //当前会话已经释放掉了
            continue;
        }
        //更新session最后一次有事件触发的时间
        session->update_ticks = g_cur_ms;
        //svr_fd的事件
        if (SVRFDFLAG_ISSET(g_session_events[i].data.ptr)) {
            session->event_fd = SVR_FD;
            log_debug("session->event_fd = SVR_FD, session->svr_event_status:%d\n",session->svr_event_status);
            //初始状态要去检查是否完成三次握手
            if (session->svr_event_status == SVR_EVENT_STATUS_INIT) {
                log_debug("session->svr_event_status == SVR_EVENT_STATUS_INIT session:%p\n", session);
                int error = 0, ret = 0;
                socklen_t len = 0;
                errno = 0;
                len = (int)sizeof(error);
                ret = getsockopt(session->svr_fd, SOL_SOCKET, SO_ERROR, &error, &len);
                if (ret < 0 || error) {
                    if (errno == EINPROGRESS) {
                        log_debug("svr handshaking!\n");
                        continue;
                    }
                    log_debug("getsockopt handlshake [%s (errno:%d)], [%s (error:%d)]\n", strerror(errno), errno, strerror(error), error);
                    on_session_err(session);
                    continue;
                }
                log_debug("svr begin relay data");
                session->svr_event_status = SVR_EVENT_STATUS_DATA_RELAY;
            }
            //svr_fd上的三次握手已完成，可以读写svr_fd了
            if (session->svr_event_status == SVR_EVENT_STATUS_DATA_RELAY) {
                int ret = 0;
                ret = on_data_relay(session, &g_session_events[i], &session->svr_fd, &session->cli_fd, &session->from_svr_data.data_list, &session->from_cli_data.data_list);
                if (ret < 0) {
                    //session had free
                    log_error("on_data_relay failed");
                    continue;
                }
            }

            if (session->svr_event_status == SVR_EVENT_STATUS_CLOSE) {
                on_session_close(session);
                continue;
            }

        } else { //cli_fd的事件
            session->event_fd = CLI_FD;
            log_debug(" session->event_fd == CLI_FD session:%p\n", session);
            if (session->cli_event_status == CLI_EVENT_STATUS_INIT) {
                log_debug("session->cli_event_status == CLI_EVENT_STATUS_INIT session:%p\n", session);
                session->cli_event_status = CLI_EVENT_STATUS_PARSE_HEAD;
            }
            if (session->cli_event_status == CLI_EVENT_STATUS_PARSE_HEAD) {
                log_debug(" session->cli_event_status == CLI_EVENT_STATUS_PARSE_HEAD session:%p\n", session);
                int ret = 0;
                //ret=0 success
                //ret=-1 fail
                //ret=-2 continue
                ret = on_parse_header(session, &g_session_events[i]);
                if (ret == 0) {
                    session->cli_event_status = CLI_EVENT_STATUS_PROCESS_HEAD;
                } else if (ret == -1) {
                    log_error("on_parse_header error, cli_fd=%d \n", session->cli_fd);
                    on_session_err(session);
                    continue;
                }
              
            }
            //如果是connect方法 需要给客户端回一条200的消息
            if (session->cli_event_status == CLI_EVENT_STATUS_PROCESS_HEAD) {
                log_debug(" session->cli_event_status == CLI_EVENT_STATUS_PROCESS_HEAD session:%p\n", session);
                int ret = on_process_header(session);
                if (ret == 0) {
                    session->cli_event_status = CLI_EVENT_STATUS_BUILD_SVR;
                } else {
                    log_error("on_process_header error, cli_fd=%d \n", session->cli_fd);
                    on_session_err(session);
                    continue;
                }
            }

            if (session->cli_event_status == CLI_EVENT_STATUS_BUILD_SVR) {
                log_debug(" session->cli_event_status == CLI_EVENT_STATUS_BUILD_SVR session:%p\n", session);
                struct sockaddr_in svr_addr;
                //与服务端建立连接
                session->svr_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (session->svr_fd < 0) {
                    log_error("create svr socket error, cli_fd=%d \n", session->cli_fd);
                    on_session_err(session);
                    continue;
                }

                int nonblock_flag = fcntl(session->svr_fd, F_GETFL);
                nonblock_flag |= O_NONBLOCK;
                if (fcntl(session->svr_fd, F_SETFL, nonblock_flag) < 0) //set nonblock
                {
                    log_error("svr_fd set nonblock error, svr_fd=%d, cli_fd=%d\n", session->svr_fd, session->cli_fd);
                    on_session_err(session);
                    continue;
                }

                int nodelay_flag = 1;
                if (setsockopt(session->svr_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay_flag, sizeof(int)) < 0) {
                    log_error("svr_fd set TCP_NODELAY error, svr_fd=%d, cli_fd=%d\n", session->svr_fd, session->cli_fd);
                    on_session_err(session);
                    continue;
                }

                bzero(&svr_addr, sizeof(svr_addr));
                svr_addr.sin_family = AF_INET;
                log_debug("request method is: %.*s\n", session->h->RequestMethodLen, session->h->RequestMethod);
                if (!strncasecmp((const char*)session->h->RequestMethod, "connect", 7)) {
                    log_debug("session->h->RequestMethod == connect\n");
                    svr_addr.sin_port = htons(443);  //https
                } else {
                    log_debug("session->h->RequestMethod != connect\n");
                    svr_addr.sin_port = htons(80);   //http
                }
                char* domain = NULL;
                char* strport = NULL;
                //获取host
                for (unsigned int i = 0; i < session->h->HeaderSize; ++i) {
                    if (!strncasecmp(session->h->Fields[i].FieldName, "Host", 4)) {
                        char* temp = strndup(session->h->Fields[i].Value, session->h->Fields[i].ValueLen);
                        char *split = strchr((const char*)temp, ':');
                        if (split != NULL) {
                            log_debug("domain len:%d, split:%p, value:%p",  split - temp, split, temp);
                            domain = strndup(session->h->Fields[i].Value, split - temp);
                            strport = strndup(split + 1 , temp + session->h->Fields[i].ValueLen - split -1);
                            log_debug("Host is: %s, port is:%s\n", domain, strport);
                        } else {
                            domain = temp;
                            log_debug("Host is: %s\n", domain);
                        }
                        break;
                    }
                }
                if (!domain) {
                    log_error("get host fail, session:%p\n", session);
                    on_session_err(session);
                    continue;
                }
                if (strport) {
                    char *ptr;
                    int port = strtol(strport, &ptr, 10);
                    free(strport);
                    log_debug("update port:%d", port);
                    svr_addr.sin_port = htons((uint16_t)port);
                }
                struct hostent *host = gethostbyname(domain);
                if (host == NULL) {
                    log_error("gethostbyname error, session:%p\n", session);
                    on_session_err(session);
                    continue;
                }
                char dst[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET, host->h_addr_list[0], dst, sizeof(dst));
                log_debug("host ip:%s host h_name:%s", dst, host->h_name);
                svr_addr.sin_addr.s_addr = inet_addr((const char*)dst);//((struct in_addr *)host->h_name)->s_addr;//inet_addr((const char*)domain);
                if (domain) {
                    free(domain);
                    domain = NULL;
                }
                socklen_t addrlen = (socklen_t)sizeof(svr_addr);
                if (connect(session->svr_fd, (struct sockaddr*)&svr_addr, addrlen) < 0) {
                    if (errno != EINPROGRESS){
                        log_error("connecion server error, svr_fd=%d, cli_fd=%d\n", session->svr_fd, session->cli_fd);
                        on_session_err(session);
                        continue;
                    }
                    log_debug("connecion server handshaking, svr_fd=%d, cli_fd=%d, errno:%d\n", session->svr_fd, session->cli_fd, errno);
                    //还在三次握手
                } else {
                    //握手成功转移状态
                    log_debug("connecion server handshake done, svr_fd=%d, cli_fd=%d\n", session->svr_fd, session->cli_fd);
                    session->svr_event_status = SVR_EVENT_STATUS_DATA_RELAY;
                }
                //转移状态
                session->cli_event_status = CLI_EVENT_STATUS_DATA_RELAY;
                struct epoll_event ev;
                ev.events = (EPOLLIN | EPOLLOUT);
			    ev.data.ptr = (void*)SET_SVR_FD_FLAG(session);
			    if (epoll_ctl(g_session_epfd, EPOLL_CTL_ADD, session->svr_fd, &ev) < 0) {
                    log_debug("epoll_ctl error, svr_fd=%d, cli_fd=%d\n", session->svr_fd, session->cli_fd);
				    on_session_err(session);
                    continue;
			    }
            }

            if (session->cli_event_status == CLI_EVENT_STATUS_DATA_RELAY) {
                log_debug("data relaying\n");
                int ret = 0;
                ret = on_data_relay(session, &g_session_events[i], &session->cli_fd, &session->svr_fd, &session->from_cli_data.data_list,  &session->from_svr_data.data_list);
                if (ret < 0) {
                    //session had free
                    log_error("on_data_relay failed");
                    continue;
                }
            }
            if (session->cli_event_status == CLI_EVENT_STATUS_CLOSE) {
                log_debug(" session->cli_event_status == CLI_EVENT_STATUS_CLOSE session:%p\n", session);
                on_session_close(session);
                continue;
            }
        }
        epoll_session_mod(session);
    }
    return 0;
}

static int session_timeout(void* val, void* data) {
    if (NULL == val || NULL == data) {
        log_error("input parameter is invalid\n");
        return -1;
    }
    ld_session_t* session = (ld_session_t*)val;
    uint64_t curtime = *(uint64_t*)data;
    if (curtime - session->update_ticks >= SESSION_ALIVE_MS) {
        free_server_session(session);
    }
    return 0;
}
   
/**
 * 处理超时连接
 */
void handle_session_timeout() {
    uint64_t curtime = g_cur_ms;
    if (curtime - g_sess_timeout_ticks > g_sess_max_timeout) {
        g_sess_timeout_ticks = curtime;
        //每1分钟检查session超时，如果session分配失败，分配失败后会将超时时间改为1s钟
        if (g_sess_max_timeout == 60000) {
            walk_session(session_timeout, (void*)&curtime);
        } else {
            //全部遍历一次，然后从新将超时时间设置为1分钟
            g_sess_max_timeout = 60000;
            walk_session_all(session_timeout, (void*)&curtime);
        }
    }
}

static int handle_req_form_cli(void *ev_ptr) {
    if (ev_ptr == NULL) {
        log_error("ev_ptr == NULL\n");
        return -1;
    }
    agent_server_t *request = (agent_server_t *)ev_ptr;
    int server_fd = request->fd;
    struct sockaddr_in client_addr;
    struct sockaddr_in serv_addr;
    unsigned char *pIp = NULL;
    unsigned short port = 0;
    socklen_t client_len = sizeof(client_addr);
    socklen_t serv_len = sizeof(serv_addr);
    //unsigned short dport;
    ld_session_t* session = NULL;

    int fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (fd < 0) {
        log_error("svr_fd=%d accept client request error:%s\n", server_fd, strerror(errno));
        goto error;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    if (getsockname(fd, (struct sockaddr *)&serv_addr, &serv_len) != 0) {
        log_error("getsockname failed!\n");
        goto error;
    }
    //dport = ntohs(serv_addr.sin_port);
    pIp = (unsigned char *)&client_addr.sin_addr.s_addr;
    port = ntohs(client_addr.sin_port);

    int nonblock_flag = fcntl(fd, F_GETFL);
    nonblock_flag |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, nonblock_flag) < 0) //set nonblock
    {
        log_error("svr_fd=%d set nonblock fd=%d srcip=%u.%u.%u.%u:%u error:%s\n", request->fd, fd, pIp[0], pIp[1], pIp[2], pIp[3], port, strerror(errno));
        goto error;
    }

    int nodelay_flag = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay_flag, sizeof(int)) < 0) {
        log_error("svr_fd=%d set TCP_NODELAY fd=%d error:%s\n", request->fd, fd, strerror(errno));
        goto error;
    }

    /*
    //扩大cli的发送缓存区,来减小write失败的情况
    log_debug("set SO_SNDBUF size=%u, dport=%u\n", g_send_buffer_size, request->port);
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &g_send_buffer_size, sizeof(g_send_buffer_size)) != 0) {
        log_error("set send buff fail, cli_fd=%d, dport=%u, buf_size=%u \n", fd, request->port, g_send_buffer_size);
    }
    */
    session = malloc_server_session();
    if (session == NULL) {
        g_sess_max_timeout = 1000;
        goto error;
    }
    log_debug("handle_req_form_cli session:%p\n",session);
    session->cli_fd = fd;
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = (void*)SET_CLI_FD_FLAG(session);
    if (!list_empty_safe(&session->from_svr_data.data_list)) {
        ev.events |= EPOLLOUT;
    }
    if (epoll_ctl(g_session_epfd, EPOLL_CTL_ADD, session->cli_fd, &ev) < 0) {
        log_error("http session add epoll error, cli_fd=%d\n", session->cli_fd); 
        goto error;
    }
    log_debug("svr_fd=%d accept http client connection fd=%d srcip=%u.%u.%u.%u:%u\n", request->fd, fd, pIp[0], pIp[1], pIp[2], pIp[3], port);
    return 0;

error:
    if (session == NULL && fd > 0) {
        close_linger(fd, 0);
    }
    if (session) {
        free_server_session(session);
    }
    return -1;
}

int handle_agent_event() {
    int i = 0;
    int nfds = 0;
    nfds = epoll_wait(g_agent_epfd, g_agent_ev, g_agent_server_cnt, LD_EPOLL_TIMEOUT_MS); //待验证是否会一直timeout
    if (nfds < 0) {
        if (errno == EINTR) {
            log_debug("errno == EINTR\n");
            return 0;
        }
        log_fault("epoll_wait error:%d\n", errno);
        goto error;
    }
    g_cur_ms = timer_ticks2ms(timer_get_ticks());
    for (i = 0; i < nfds; ++i) {
        if (g_agent_ev[i].events & EPOLLIN) {
            if (handle_req_form_cli((void *)g_agent_ev[i].data.ptr) < 0) {
                log_error("handle_req_form_cli failed");
                continue;
            }
        }
    }

    return 0;

error:
    if (g_agent_epfd > 0) {
        close_linger(g_agent_epfd, 0);
        g_agent_epfd = -1;
    }
    return -1;
}

static int create_agent_server(agent_server_t *server) {
    int fd = -1;
    struct sockaddr_in sockaddr;
    socklen_t optlen = 0, addrlen = 0;
    int opt = 1;
    assert(server != NULL);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        log_fault("socket error:%d\n", errno);
        goto error;
    }

    optlen = (socklen_t)sizeof(opt);
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, optlen) < 0) {
        log_fault("set socket option SO_REUSEADDR error:%d\n", errno);
        goto error;
    }

    int nonblock_flag = fcntl(fd, F_GETFL);
    nonblock_flag |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, nonblock_flag) < 0) //set nonblock
    {
        log_fault("set nonblock error:%d\n", errno);
        goto error;
    }

    int nodelay_flag = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay_flag, sizeof(int)) < 0) {
        log_fault("set TCP_NODELAY error:%d\n", errno);
        goto error;
    }

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(server->port);
    sockaddr.sin_addr.s_addr = inet_addr((const char *)server->addr);//htonl(INADDR_ANY);
    addrlen = (socklen_t)sizeof(sockaddr);
    if (bind(fd, (struct sockaddr *)&sockaddr, addrlen) < 0) {
        log_fault("bind error:%d, port=%u\n", errno, server->port);
        goto error;
    }

    if (listen(fd, LD_NET_INCOME_REQ_QUEUE_SIZE) < 0) {
        log_fault("listen error:%d\n", errno);
        goto error;
    }

    server->fd = fd;
    return 0;

error:
    if (fd > 0) {
        close_linger(fd, 0);
        fd = -1;
    }
    return -1;
}

static void close_agent_server(agent_server_t *server) {
    assert(server != NULL);
    int fd = server->fd;
    if (fd > 0) {
        close_linger(fd, 0);
    }
    server->fd = -1;
    server->addr = NULL;
}

int init_agent_service() {
    struct epoll_event ev = {0};
    memset(&g_agent_server, 0, sizeof(g_agent_server));
    g_agent_server_cnt = 1;
    g_agent_server.fd = -1;
    g_agent_server.port = (uint16_t)g_proxyser_cfg.local_port; 
    g_agent_server.addr = g_proxyser_cfg.local_addr;
    if (create_agent_server(&g_agent_server) < 0) {
        log_fault("create_agent_server failed!\n");
        goto error;
    }

    g_agent_epfd = epoll_create(LD_EPOLL_AGENT_MAX_NUM);
    if (g_agent_epfd < 0) {
        log_fault("create epoll fd error:%d\n", errno);
        goto error;
    }

    ev.events = EPOLLIN;
    ev.data.ptr = (void *)&g_agent_server;
    if (epoll_ctl(g_agent_epfd, EPOLL_CTL_ADD, g_agent_server.fd, &ev) < 0) {
        log_fault("epoll add event error:%d\n", errno);
        goto error;
    }
    log_fault("init_agent_service success!\n");
    return 0;

error:
    close_agent_service();
    return -1;
}

void close_agent_service() {
    if (g_agent_epfd > 0) {
        close(g_agent_epfd);
        g_agent_epfd = -1;
    }
    close_agent_server(&g_agent_server);
}