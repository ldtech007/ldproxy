#ifndef __LD_CIPER_H__
#define __LD_CIPER_H__

#define SECRET_KEY_LEN 256

int ciper_init(unsigned char* secretkey, int len);

unsigned char* ciper_encode(unsigned char* data, int len);

unsigned char* ciper_decode(unsigned char* data, int len);

#endif