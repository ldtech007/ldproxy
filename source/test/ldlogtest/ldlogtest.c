#include <stdio.h>
#include "ldlog.h"
int main()
{
	int ret = 0;
	ret = ldlog_init();
    if (ret < 0) {
        ldlog_uninit();
        printf("ldlog_init fail!\n");
        return -1;
    }
 
    ldlog_set_level(LOG_ERROR);
    log_error("this is log_error!!\n");
    log_debug("this is log_debug!!\n");
    log_fault("this is log_fault!!\n");
    log_fault("this is log_fault1!!\n");
	return 0;
}