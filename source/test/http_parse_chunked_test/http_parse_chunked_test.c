#include <stdio.h>
#include <string.h>
#include "http_parse_chunked.h"

int main() {

    unsigned char test[] = "23\r\nThis is the data in the first chunk\r\n1a\r\nand this is the second one\r\n3\r\ncon\r\n8\r\nsequence\r\n0\r\n\r\n";
    int len = strlen((const char *)test);
    printf("len:%d\n", len);
    http_chunked_t ctx;
    ctx.state = 0;
    ctx.size = 0;
    ctx.length = 0;
    ld_buf_t b;
    b.buf = test;
    b.pos = test;
    b.last = test + 20;
    int ret = http_parse_chunked(&b, &ctx);
    if (ret == LD_DONE) {
        printf("done\n");
    } else if (ret == LD_AGAIN) {
        printf("again\n");
    } else {
        printf("error\n");
    }
    printf("b.buf:%p,b.pos:%p,b.last:%p\n",b.buf,b.pos,b.last);
    b.last = test + len;
    ret = http_parse_chunked(&b, &ctx);
    if (ret == LD_DONE) {
        printf("done\n");
    } else if (ret == LD_AGAIN) {
        printf("again\n");
    } else {
        printf("error\n");
    }
    printf("b.buf:%p,b.pos:%p,b.last:%p\n",b.buf,b.pos,b.last);
    return 0;
}