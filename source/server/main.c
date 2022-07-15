#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include "tick.h"
#include "ldlog.h"
#include "serversession.h"
#include "server.h"
#include "servercfg.h"
#include <signal.h>
#include "ciper.h"

int g_run = 0;
extern int g_session_epfd;
extern uint32_t g_max_session;
extern uint32_t g_max_session_events;
extern struct epoll_event* g_session_events;
extern proxy_config_t g_proxyser_cfg;


static void signal_handler(int signo) {
    switch (signo) {
    
    case 40: /* SIGRTMIN+6 开启error调试 */
        ldlog_set_level(LOG_ERROR);
        break;
    case 41: /* SIGRTMIN+7 开启debug调试 */
        ldlog_set_level(LOG_DEBUG);
        break;
    case 42: /* SIGRTMIN+8 关闭调试 */
        ldlog_set_level(LOG_CLOSE);
        break;
    default:
        break;
    }
}


int main(int argc, char* argv[]) {
    int ret = 0;
    ret = ldlog_init();
    if (ret < 0) {
        printf("ldlog_init failed!\n");
        goto error;
    }
    //安装信号
    signal(SIGPIPE, SIG_IGN);
    signal(SIGRTMIN + 6, signal_handler);
    signal(SIGRTMIN + 7, signal_handler);
    signal(SIGRTMIN + 8, signal_handler);
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == NULL) {
            continue;
        }

        if (strncmp(argv[i], "-d", 2) == 0) {
            log_fault("ldproxy run in debug model!\n");
            ldlog_set_level(LOG_DEBUG);
        }
    }

    timer_ticks_init();
    ret = load_ser_proxy(&g_proxyser_cfg);
    if (ret < 0) {
        log_fault("load_cli_proxy failed\n");
        goto error;
    } 
    ret = ciper_init(g_proxyser_cfg.secretkey, SECRET_KEY_LEN);
    if (ret < 0) {
        log_fault("ciper_init failed\n");
        goto error;
    }
    ret = server_session_pool_init(g_max_session);
    if (ret < 0) {
        log_fault("server_session_pool_init failed\n");
        goto error;
    }
    ret = init_agent_service();
    if (ret < 0) {
        log_fault("init_agent_service failed\n");
        goto error;
    }
    g_session_epfd = epoll_create(1);
    if (g_session_epfd < 0) {
        log_fault("init g_session_epfd error %d\n", errno);
        goto error;
    }
    int events_len = g_max_session_events * sizeof(struct epoll_event);
    g_session_events = (struct epoll_event*)malloc(events_len);
    if (NULL == g_session_events) {
        log_fault("g_session_events malloc error, num=%d\n", g_max_session_events);
        goto error;
    }
    memset(g_session_events, 0, events_len);
    g_run = 1; //todo可配
    while (g_run) {
        ret = 0;
        ret = handle_agent_event();
        if (ret < 0) {
            log_fault("handle_agent_event fail\n");
            goto error;
        }
        ret = handle_connections_event();
        if (ret < 0) {
            log_fault("handle_connections_event fail\n");
            goto error;
        }
        handle_session_timeout();
    }

error:
    g_run = 0;
    if (g_session_epfd > 0) {
        close(g_session_epfd);
        g_session_epfd = -1;
    }
    if (g_proxyser_cfg.local_addr) {
        free(g_proxyser_cfg.local_addr);
        g_proxyser_cfg.local_addr = NULL;
    }
    close_agent_service();
    server_session_pool_uninit();
    ldlog_uninit();
    return ret;
}