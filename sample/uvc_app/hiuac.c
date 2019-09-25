#include <pthread.h>
#include <unistd.h>
#include "log.h"
#include "config_svc.h"
#include "hicamera.h"
#include "hiuac.h"
#include "hiaudio.h"
#include "uac_gadgete.h"

static int __init()
{
    return 0;
}

static int __open()
{
    return 0;
}

static int __close()
{
    hiaudio_shutdown();
    return 0;
}

static int __run()
{
#if 0
    int status = 0;
    int running = 1;

    while (running)
    {
        status = run_uac_device();

        if (status < 0)
        {
            break;
        }
        // be careful. if return code is timeout,
        // maybe the host is disconnected,so here to start device again
        // it maybe to find a another nice method to checking host connects or disconnects
        else if (status == 0)
        {
            sleep(1);
            status = run_uac_device();
            if (status == 0)
            {
                get_hiuac()->close();
                if (get_hiuac()->open() != 0)
                {
                    break;
                }
            }
        }
    }
#endif

    hiaudio_init();
    hiaudio_startup();

    return 0;
}

/* ---------------------------------------------------------------------- */

static hiuac __hi_uac =
{
    .init = __init,
    .open = __open,
    .close = __close,
    .run = __run,
};

hiuac* get_hiuac()
{
    return &__hi_uac;
}

void release_hiuac(hiuac *uvc)
{
}
