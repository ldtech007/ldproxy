#ifndef __LD_SESSION_H__
#define __LD_SESSION_H__
#include "list.h"
#include <stdint.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "ldlog.h"

#define LD_BUFFER_SIZE_4      4
#define LD_BUFFER_SIZE_8      8
#define LD_BUFFER_SIZE_16      16
#define LD_BUFFER_SIZE_32      32
#define LD_BUFFER_SIZE_64      64
#define LD_BUFFER_SIZE_128     128
#define LD_BUFFER_SIZE_256     256
#define LD_BUFFER_SIZE_512     512
#define LD_BUFFER_SIZE_1024    1024
#define LD_BUFFER_SIZE_2048    2048
#define LD_BUFFER_SIZE_4096    4096
#define LD_BUFFER_SIZE_8192    8192
#define LD_BUFFER_SIZE_1k      (LD_BUFFER_SIZE_1024)
#define LD_BUFFER_SIZE_2k      (LD_BUFFER_SIZE_2048)
#define LD_BUFFER_SIZE_4k      (LD_BUFFER_SIZE_4096)
#define LD_BUFFER_SIZE_8k      (LD_BUFFER_SIZE_8192)
#define LD_BUFFER_SIZE_16k     (LD_BUFFER_SIZE_8k*2)

#define LD_DEFAULT_SESSION 256
#define LD_MAX_SESSION 2048
#define SESSION_ALIVE_MS 300000    //5分钟

enum {
    CLI_EVENT_STATUS_INIT,
    CLI_EVENT_STATUS_BUILD_SVR,
    CLI_EVENT_STATUS_DATA_RELAY,
    CLI_EVENT_STATUS_CLOSE,
    SVR_EVENT_STATUS_INIT,
    SVR_EVENT_STATUS_DATA_RELAY,
    SVR_EVENT_STATUS_CLOSE
};

enum {
    UNKNOWN,
    CLI_FD,
    SVR_FD
};
typedef struct {
    LISTNODE list;     //链表结点
    uint32_t size;      //data缓冲区总大小
    uint32_t len;       //实际数据长度
    uint32_t off;       //有效数据的起始位置的偏移
    char     data[0];   //数据缓存区
}buffer_t;
typedef struct 
{
    LISTHEAD data_list; //未发送完的数据缓存
} flow_t;
typedef struct
{
    uint16_t flag; //session是否被使用
    uint16_t event_fd; //触发events的fd
    uint16_t cli_event_status;
    uint16_t svr_event_status;
	uint64_t update_ticks; //上次操作session的时间
    int cli_fd;
    int svr_fd;
	flow_t from_cli_data;  //cli_fd收数据->svr_fd写数据
    flow_t from_svr_data; //svr_fd收数据->cli_fd写数据
} ld_session_t;

static inline int close_linger(int fd, int timeout)
{
	struct linger so_linger;
	so_linger.l_onoff = 1;
	so_linger.l_linger = timeout;
	
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) != 0) {
		log_error("setsockopt SO_LINGER failed, fd=%d, timeout=%d, err=%d(%s)\n", fd, timeout, errno, strerror(errno));
	}
	return close(fd);
}

typedef int session_walk_fun_t(void *val, void *data);
int client_session_pool_init(unsigned int max_sesssion);
void client_session_pool_uninit();
ld_session_t* malloc_client_session();
void free_client_session(ld_session_t* session);
void walk_session(session_walk_fun_t pfnwalk, void* data);
void walk_session_all(session_walk_fun_t pfnwalk, void* data);
#endif