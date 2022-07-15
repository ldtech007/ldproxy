#include "ciper.h"
#include <assert.h>

static unsigned char encodekey[SECRET_KEY_LEN];
static unsigned char decodekey[SECRET_KEY_LEN];

unsigned char* ciper_encode(unsigned char* data, int len) {
    assert(data && len > 0);
    for (int i = 0; i < len; ++i) {
        data[i] = encodekey[data[i]];
    }
    return data;
}

unsigned char* ciper_decode(unsigned char* data, int len) {
    assert(data && len > 0);
    for (int i = 0; i < len; ++i) {
        data[i] = decodekey[data[i]];
    }
    return data;

}


int ciper_init(unsigned char* secretkey, int len) {
    assert(secretkey);
    if (len != SECRET_KEY_LEN) {
        return -1;
    }
    for (int i = 0; i < len; ++i) {
        encodekey[i] = secretkey[i];
        decodekey[secretkey[i]] = i;
    }
    return 0;
} 