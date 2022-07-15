#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "base64.h"


#define SECRET_KEY_LEN 256
unsigned char secret_key[SECRET_KEY_LEN + 1] = {0};
unsigned char encode_secret_key[2 * SECRET_KEY_LEN + 1] = {0};
unsigned char decode_secret_key[SECRET_KEY_LEN + 1] = {0};
static void swap(unsigned char *left, unsigned char *right) {
    char temp = *left;
    *left = *right;
    *right = temp;
}
static int check_key_valid(unsigned char* secretkey, int len) {
    for (unsigned int i = 0 ; i < len; ++i) {
        if (secretkey[i] == i) {
            return -1;
        }
    }
    return 0;
}
static void generate_secret_key(){
    time_t t;
    srand((unsigned) time(&t));
    unsigned long i;
    for (i = 0; i < SECRET_KEY_LEN; ++i) {
        secret_key[i] = i;
    }
    for (i = 0; i < SECRET_KEY_LEN; ++i) {
        int v = rand() % 256; // 0 ~255
        while (secret_key[i] == secret_key[v]  || i == secret_key[v] || secret_key[i] == v) {
            v = rand() % 256;
        }
        swap(secret_key + i , secret_key + v);
    }
    int ret = check_key_valid(secret_key, SECRET_KEY_LEN);
    if (ret < 0) {
        printf("generate secret failed!\n");
    }
    printf("generate secret success!\n");
    //for (i = 0; i < SECRET_KEY_LEN; ++i) {
    //    printf("%u ", secret_key[i]);
    //}
    //printf("\n");
    unsigned long encode_len = 2 * SECRET_KEY_LEN + 1;
    ret = Base64Encode((unsigned char *)secret_key, (unsigned long)SECRET_KEY_LEN, (unsigned char *)encode_secret_key, &encode_len);
    if (ret < 0) {
        printf("secret_key base64encode failed\n");
    }
    printf("secret_key after base64encode encode_len is:%lu encode_secret_key:\n", encode_len);
    printf("%s\n",encode_secret_key);
    //unsigned long decode_len = SECRET_KEY_LEN + 1;
    //ret = Base64Decode((unsigned char *)encode_secret_key, (unsigned long)encode_len, (unsigned char *)decode_secret_key, &decode_len);
    //if (ret < 0) {
    //    printf("encode_secret_key base64decode failed\n");
    //}
    //printf("encode_secret_key after base64decode decode_len is:%lu\n", decode_len);
    //printf("decode_secret_key:\n");
    //for (i = 0; i < decode_len; ++i) {
    //    printf("%u ", decode_secret_key[i]);
    //}
    //printf("\n");
}

int main() {
    generate_secret_key();
    return 0;
}