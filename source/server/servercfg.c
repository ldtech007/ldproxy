#include "servercfg.h"
#include "parson.h"
#include <stdio.h>
#include <stdlib.h>
#include "base64.h"
#include <string.h>
#include <assert.h>
#include "ldlog.h"

#define CFG_PATH "/etc/ldproxy/server.json"
int load_ser_proxy(proxy_config_t* proxyser_cfg) {
    assert(proxyser_cfg);
    memset(proxyser_cfg, 0, sizeof(proxy_config_t));
    JSON_Value *schema = json_parse_string("{\"local_addr\":\"\",\"local_port\":0,\"secretkey\":\"\"}");
    JSON_Value *data = json_parse_file(CFG_PATH);
    const char *encode_secretkey;
    if (data == NULL || json_validate(schema, data) != JSONSuccess) {
        log_fault("json_validate ERROR\n");
        goto error;
    }
    proxyser_cfg->local_addr = (unsigned char *)strdup(json_object_get_string((json_object(data)), "local_addr"));
    if (!proxyser_cfg->local_addr) {
        log_fault("get proxyser_cfg.local_addr ERROR\n");
        goto error;
    }
    proxyser_cfg->local_port = json_object_get_number((json_object(data)), "local_port");
    if (!proxyser_cfg->local_port) {
        log_fault("get proxyser_cfg.local_port ERROR\n");
        goto error;
    }
   
    encode_secretkey = json_object_get_string((json_object(data)), "secretkey");
    if (!encode_secretkey) {
        log_fault("get encode_secretkey ERROR\n");
        goto error;
    }
    log_fault("encode_secretkey:%s\n", encode_secretkey);
    unsigned long decode_len = SECRET_KEY_LEN;
    int ret = 0;
    ret = Base64Decode((unsigned char *)encode_secretkey, (unsigned long)strlen(encode_secretkey), (unsigned char *)proxyser_cfg->secretkey, &decode_len);
    if (ret < 0) {
        log_fault("get decode_secretkey ERROR\n");
        goto error;
    }
    log_fault("local_addr:%s, local_port:%d\n",proxyser_cfg->local_addr, proxyser_cfg->local_port);
    if (schema) {
        json_value_free(schema);
    }
    if (data) {
        json_value_free(data);
    }
    return 0;
error:
    if (schema) {
        json_value_free(schema);
    }
    if (data) {
        json_value_free(data);
    }
    if (proxyser_cfg->local_addr) {
        free(proxyser_cfg->local_addr);
        proxyser_cfg->local_addr = NULL;
    }
  
    return -1;
}