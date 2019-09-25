#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "config_svc.h"
#include "camera.h"
#include "frame_cache.h"

unsigned int g_bulk = 0;
unsigned int g_uac = 1;

void SAMPLE_UVC_Usage(char *sPrgNm)
{
    printf("Usage : %s <param>\n", sPrgNm);
    printf("param:\n");
    printf("\t -h        --for help.\n");
    printf("\t -bulkmode --use bulkmode.\n");
    return;
}

int main(int argc, char *argv[])
{
    int i = argc;

    g_bulk = 0;

    while (i > 1)
    {
        if (strcmp(argv[i-1], "-bulkmode") == 0)
        {
            g_bulk = 1;
        }

        if (strcmp(argv[i-1], "-h") == 0)
        {
            SAMPLE_UVC_Usage(argv[0]);
        }

        i--;
    }

    if (create_config_svc("./uvc_app.conf") != 0)
    {
        goto ERR;
    }

    if (create_uvc_cache() != 0)
    {
        goto ERR;
    }

    if (g_uac)
    {
        if (create_uac_cache() != 0)
        {
            goto ERR;
        }
    }

    if (get_hicamera()->init() != 0 ||
        get_hicamera()->open() != 0 ||
        get_hicamera()->run() != 0)
    {
        goto ERR;
    }

    printf("UVC sample exit!\n");

ERR:
    get_hicamera()->close();
    release_cofnig_svc();
    destroy_uvc_cache();
    if (g_uac)
    {
        destroy_uac_cache();
    }

    printf("exit app normally\n");
    return 0;
}
