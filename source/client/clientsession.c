#include "clientsession.h"
#include "ldlog.h"
#include <sys/epoll.h>
#include <stdlib.h>
#include <assert.h>

uint32_t g_max_session = LD_DEFAULT_SESSION; //todo可配
static ld_session_t *g_sessions = NULL;
static unsigned int s_index = 0;
extern uint64_t g_cur_ms;
extern int g_session_epfd;
static ld_session_t* __malloc_client_session(ld_session_t* session_pool, unsigned int max_session);
static void __walk_session(ld_session_t* session_pool, uint32_t max_session, session_walk_fun_t pfnwalk, void* data, uint32_t *start, uint32_t *end);
static void __walk_session_all(ld_session_t* session_pool, uint32_t max_session, session_walk_fun_t pfnwalk, void* data);

static void __walk_session_all(ld_session_t* session_pool, uint32_t max_session, session_walk_fun_t pfnwalk, void* data) {
    assert(session_pool && pfnwalk);
    uint32_t i = 0;
    int ret = 0;
    for (i = 0; i < max_session; ++i) {
        if (session_pool[i].flag == 0) {
            continue;
        }
        ret = pfnwalk((void*)&session_pool[i], data);
        if (ret >= 0) {
            continue;
        }
        return;
    }
}

void walk_session_all(session_walk_fun_t pfnwalk, void* data) {
    if (pfnwalk == NULL) {
        return;
    }
    __walk_session_all(g_sessions, g_max_session, pfnwalk, data);
}

//防止遍历时间过长，每次遍历20%
static void __walk_session(ld_session_t* session_pool, uint32_t max_session, session_walk_fun_t pfnwalk, void* data, uint32_t *start, uint32_t *end) {
    assert(session_pool && pfnwalk);
    uint32_t i = 0;
    int ret = 0;

	if (!max_session)
		return;
	*end = (uint32_t)(*start +( max_session * 0.2))%max_session;
    for (i = *start; i != *end; i = (i+1)%max_session) {
        if (session_pool[i].flag == 0) {
            continue;
        }
        ret = pfnwalk((void*)&session_pool[i], data);
        if (ret >= 0) {
            continue;
        }
		*start = *end;
        return;
    }
	*start = *end;
}

void walk_session(session_walk_fun_t pfnwalk, void* data) {
    if (pfnwalk == NULL) {
        return;
    }
	static uint32_t i_start = 0;
	static uint32_t i_end = 0;

    __walk_session(g_sessions, g_max_session, pfnwalk, data, &i_start, &i_end);
}

static ld_session_t* __malloc_client_session(ld_session_t* session_pool, unsigned int max_session) {
    assert(session_pool);
    unsigned int i = 0;
	unsigned int count= 0;
    for (i = s_index; i < max_session && session_pool[i].flag; ++i)
        count++;
	
    if (i >= max_session)
    {
		if(count < max_session)
		{
			for (i = 0; i < s_index && session_pool[i].flag; ++i)
				count++;

			if (i >= s_index)
				return NULL;
		}
		
		if (i >= max_session)
			return NULL;
    }
	
	s_index = (i+1) % max_session;

    memset(&session_pool[i], 0, sizeof(ld_session_t));
    session_pool[i].flag = 1;
	session_pool[i].cli_fd = -1;
	session_pool[i].svr_fd = -1;
    session_pool[i].event_fd = UNKNOWN;
    INIT_LIST_HEAD(&(session_pool[i].from_cli_data.data_list));
    INIT_LIST_HEAD(&(session_pool[i].from_svr_data.data_list));
    session_pool[i].cli_event_status = CLI_EVENT_STATUS_INIT;
    session_pool[i].svr_event_status = SVR_EVENT_STATUS_INIT;
    session_pool[i].update_ticks = g_cur_ms;
    
    return &(session_pool[i]);
}

ld_session_t* malloc_client_session() {

    return __malloc_client_session(g_sessions, g_max_session);
}

void free_client_session(ld_session_t* session) {
    if (session == NULL) {
       return;
    }
    if (session->flag == 0) {
        return;
    }
    //释放链表
    PLISTNODE pos = NULL;
    PLISTNODE pos2 = NULL;
    buffer_t* packet = NULL;
    list_for_each_safe(pos, pos2, &(session->from_cli_data.data_list)) {
        packet = list_entry(pos, buffer_t, list);
        list_del(pos);
        free(packet);
    }
    list_for_each_safe(pos, pos2, &(session->from_svr_data.data_list)) {
        packet = list_entry(pos, buffer_t, list);
        list_del(pos);
        free(packet);
    }
    if (session->cli_fd > 0) {
        epoll_ctl(g_session_epfd, EPOLL_CTL_DEL, session->cli_fd, NULL);
        close(session->cli_fd);
        session->cli_fd = -1;
    }
    if (session->svr_fd > 0) {
        epoll_ctl(g_session_epfd, EPOLL_CTL_DEL, session->svr_fd, NULL);
        close(session->svr_fd);
        session->svr_fd = -1;
    }
    session->flag = 0;
}

int client_session_pool_init(unsigned int max_sesssion) {
    unsigned int sessions_len;
    g_max_session = max_sesssion;
    sessions_len =g_max_session * sizeof(ld_session_t);
    g_sessions = (ld_session_t *)malloc(sessions_len);
    if (NULL == g_sessions)
    {
        log_fault("malloc %u fail\n", sessions_len);
        return -1;
    }

    memset(g_sessions, 0, sessions_len);    
    return 0;
}

void client_session_pool_uninit()
{
    if (NULL != g_sessions)
    for (int i = 0; i < g_max_session; ++i) { 
        free_client_session(&g_sessions[i]);
    }
    free(g_sessions);
    g_sessions = NULL;
}
