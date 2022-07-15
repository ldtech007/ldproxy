#include "h3.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "ldlog.h"
#define SS(constStr) constStr, sizeof(constStr)-1
#define SL(constStr) constStr, strlen(constStr)


int main() {
    int ret = ldlog_init();
    assert(ret == 0);
    ldlog_set_level(LOG_DEBUG);
    char *headerbody = "GET /method HTTP/1.1" CRLF
        "Host: github.com" CRLF
        "Connection: keep-alive" CRLF
        "Content-Length: 12611" CRLF
        "Cache-Control: no-cache" CRLF
        CRLF
        ;

    RequestHeader *h;
    h = h3_request_header_new();

    assert(h!=NULL);

    h3_request_header_parse(h, SL(headerbody));

    h3_request_header_free(h);
    return 0;
}