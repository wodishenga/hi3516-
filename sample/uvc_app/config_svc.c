#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "log.h"
#include "config_svc.h"
#include "iniparser.h"

#define CONFIG_MAX_PATH 0xff

typedef struct config_svc_t
{
    char *        config_file;
    unsigned char is_multi_payload;
} config_svc_t;

static config_svc_t *__config_svc = 0;

int create_config_svc(char *config_path)
{
    assert(config_path != 0);

    if (__config_svc != 0)
    {
        free(__config_svc);
    }

    __config_svc = (config_svc_t*) malloc(sizeof * __config_svc);
    if (__config_svc == 0)
    {
        LOG("create config svc failure\n");
        goto ERR;
    }

    __config_svc->config_file = malloc(sizeof(char) * CONFIG_MAX_PATH);
    if (__config_svc->config_file == 0)
    {
        LOG("malloc config file memory failure\n");
        goto ERR;
    }

    if (strlen(config_path) > CONFIG_MAX_PATH)
    {
        LOG("config path is too length\n");
        goto ERR;
    }

    strcpy(__config_svc->config_file, config_path);

    get_all_default_conf();

    return 0;

ERR:
    if (__config_svc)
    {
        if (__config_svc->config_file)
        {
            free(__config_svc->config_file);
        }

        free(__config_svc);
    }

    __config_svc = 0;

    return -1;
}

void release_cofnig_svc()
{
    if (__config_svc)
    {
        if (__config_svc->config_file)
        {
            free(__config_svc->config_file);
        }

        free(__config_svc);
    }

    __config_svc = 0;
}

int get_config_value(const char *key, int default_value)
{
    dictionary *config_dic;
    int value = default_value;

    assert(key != 0);
    assert(__config_svc != 0);

    config_dic = iniparser_load(__config_svc->config_file);

    if (config_dic == 0)
    {
        LOG("open config file failuer[%s], returen default value:%d\n", __config_svc->config_file, default_value);

        return value;
    }

    value = iniparser_getint(config_dic, key, default_value);

    iniparser_freedict(config_dic);

    return value;
}

int get_all_default_conf()
{
    __config_svc->is_multi_payload = get_config_value("stream:is_multi_payload", 0);

    return 0;
}

int is_multi_payload_conf()
{
    return __config_svc->is_multi_payload;
}
