#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <pthread.h>
#include "log.h"
#include "config_svc.h"
#include "hicamera.h"
#include "hiuvc.h"
#include "hiuac.h"
#include "histream.h"
#include "hiaudio.h"
#include "camera.h"
#include "hi_type.h"
/* -------------------------------------------------------------------------- */

extern unsigned int g_uac;
static pthread_t g_stUVCPid;
static pthread_t g_stUACPid;

static int __init()
{
    sample_venc_config();

    if (get_hiuvc()->init() != 0 ||
        histream_init() != 0)
    {
        return -1;
    }

    if (g_uac)
    {
#ifdef HI_UAC_COMPILE
        sample_audio_config();
#endif

        if (get_hiuac()->init() != 0 ||
            hiaudio_init() != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int __open()
{
    if (get_hiuvc()->open() != 0)
    {
        return -1;
    }

    if (g_uac)
    {
        if (get_hiuac()->open() != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int __close()
{
    get_hiuvc()->close();
    histream_shutdown();
    if (g_uac)
    {
        get_hiuac()->close();
        hiaudio_shutdown();
    }
    return 0;
}

HI_VOID* uvc_thread(HI_VOID *p)
{
    get_hiuvc()->run();
    return NULL;
}

HI_VOID* uac_thread(HI_VOID *p)
{
    get_hiuac()->run();
    return NULL;
}

static int __run()
{
    pthread_create(&g_stUVCPid, 0, uvc_thread, NULL);
    pthread_create(&g_stUACPid, 0, uac_thread, NULL);
    pthread_join(g_stUVCPid, NULL);
    pthread_join(g_stUACPid, NULL);
    return 0;
}

/* -------------------------------------------------------------------------- */

static hicamera __hi_camera =
{
    .init = __init,
    .open = __open,
    .close = __close,
    .run = __run,
};

hicamera* get_hicamera()
{
    return &__hi_camera;
}

void release_hicamera(hicamera *camera)
{
}

