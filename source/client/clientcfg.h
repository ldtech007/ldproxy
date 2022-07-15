#ifndef __LD_CONFIG_H__
#define __LD_CONFIG_H__
#define SECRET_KEY_LEN 256
typedef struct {
    unsigned char secretkey[SECRET_KEY_LEN];
    unsigned char* local_addr;
    int local_port;
    unsigned char* server_addr;
    int server_port;
} proxy_config_t;

int load_cli_proxy(proxy_config_t* proxycli_cfg);
#endif