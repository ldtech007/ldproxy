#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "ldlog.h"

#define LD_LOG_PATH "/var/log"
#define LD_FAULT_LOG_PATH LD_LOG_PATH "/ldproxy_fault.log"
#define FILE_PATH_MAX 512


static struct {
    FILE *fd;          //实时日志
    FILE *fault_fd;    //启动日志
    int level;
} L;

static const char *level_names[] = { "LOG_CLOSE", "DEBUG", "ERROR"};

void ldlog_set_level(int level) { L.level = level; }
//返回值0 成功 
//-1 失败 
int ldlog_init() {
    int ret = 0;
    char logfile[FILE_PATH_MAX] = {0};
    char timestap[32] = {0};
    if (access(LD_LOG_PATH, F_OK) != 0) {
        ret = mkdir(LD_LOG_PATH, 0644);
        if (ret < 0) {
            return -1;
        }
    }
    L.fault_fd = fopen(LD_FAULT_LOG_PATH, "w");
     /* Get current time */
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    timestap[strftime(timestap, sizeof(timestap), "%Y-%m-%d_%H_%M_%S", lt)] = '\0';
    snprintf(logfile, sizeof(logfile), "%s/ldproxy_%d_%s.log", LD_LOG_PATH, getpid(), timestap);
    L.fd = fopen(logfile, "w");
    if (!L.fault_fd || !L.fd) {
        if (L.fd) {
            fclose(L.fd);
        }
        if (L.fault_fd) {
            fclose(L.fault_fd);
        }
        return -1;
    }
    L.level = LOG_CLOSE;
    return 0;
}
void ldlog_uninit() {

    L.level = LOG_CLOSE;

    if (L.fd) {
        fclose(L.fd);
    }

    if (L.fault_fd) {
        fclose(L.fault_fd);
    }
}


void ldlog_log(int level, const char *file, int line, const char *fmt, ...) {
    if ((L.level == LOG_CLOSE) || (level < L.level)) {
        return;
    }
  
    /* Get current time */
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    /* Log to file */
    if (L.fd) {
        va_list args;
        char buf[32];
        buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
        fprintf(L.fd, "%s %-5s %s:%d: ", buf, level_names[level], file, line);
        va_start(args, fmt);
        vfprintf(L.fd, fmt, args);
        va_end(args);
        fprintf(L.fd, "\n");
        fflush(L.fd);
    }
}

void ldlog_fault(const char *file, int line, const char *fmt, ...){
     /* Get current time */
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    /* Log to file */
    if (L.fault_fd) {
        va_list args;
        char buf[32];
        buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
        fprintf(L.fault_fd, "%s %s:%d: ", buf, file, line);
        va_start(args, fmt);
        vfprintf(L.fault_fd, fmt, args);
        va_end(args);
        fprintf(L.fault_fd, "\n");
        fflush(L.fault_fd);
    }
}