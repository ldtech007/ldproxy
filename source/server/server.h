#ifndef __LD_SERVER_H__
#define __LD_SERVER_H__
#include "serversession.h"

#define LD_MAX_PROXY_PORT_NUM 1
#define LD_NET_INCOME_REQ_QUEUE_SIZE   128
#define LD_EPOLL_AGENT_MAX_NUM        ((LD_MAX_PROXY_PORT_NUM) * 2)
#define LD_EPOLL_TIMEOUT_MS    30
typedef struct {
    int fd;   // IPv4套接字
    unsigned char * addr;
    uint16_t port;
} agent_server_t;

int init_agent_service();
void close_agent_service();
int handle_agent_event();
int handle_connections_event();
void handle_session_timeout();

#endif