
#ifndef LDLOG_H
#define LDLOG_H

#include <stdarg.h>
#include <stdio.h>


enum { LOG_CLOSE, LOG_DEBUG, LOG_ERROR};
int ldlog_init();
void ldlog_uninit();

void ldlog_set_level(int level);

void ldlog_log(int level, const char *file, int line, const char *fmt, ...);
void ldlog_fault(const char *file, int line, const char *fmt, ...);

#define log_debug(...) ldlog_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) ldlog_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fault(...) ldlog_fault(__FILE__, __LINE__, __VA_ARGS__)


#endif
