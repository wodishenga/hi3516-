#include <pthread.h>
#include <unistd.h>
#include "log.h"
#include "config_svc.h"
#include "hicamera.h"
#include "hiuvc.h"
#include "uvc_gadgete.h"

static pthread_t g_stUVC_Send_Data_Pid;

static int __init()
{
    return 0;
}

static int __open()
{
    const char *devpath = "/dev/video0";

    return open_uvc_device(devpath);
}

static int __close()
{
    return close_uvc_device();
}

void* uvc_send_data_thread(void *p)
{
    int status = 0;
    int running = 1;

    while (running)
    {
        status = run_uvc_data();
    }

    RLOG("uvc_send_data_thread exit, status: %d.\n", status);

    return NULL;
}

static int __run()
{
    int status = 0;
    int running = 1;

    pthread_create(&g_stUVC_Send_Data_Pid, 0, uvc_send_data_thread, NULL);

    while (running)
    {
        status = run_uvc_device();

        if (status < 0)
        {
            break;
        }
        // be careful. if return code is timeout,
        // maybe the host is disconnected,so here to start device again
        // it maybe to find a another nice method to checking host connects or disconnects
        #if 0
        else if (status == 0)
        {
            sleep(1);
            status = run_uvc_device();
            if (status == 0)
            {
                get_hiuvc()->close();
                if (get_hiuvc()->open() != 0)
                {
                    break;
                }
            }
        }
        #endif
    }

    pthread_join(g_stUVC_Send_Data_Pid, NULL);

    return 0;
}

/* ---------------------------------------------------------------------- */

static hiuvc __hi_uvc =
{
    .init = __init,
    .open = __open,
    .close = __close,
    .run = __run,
};

hiuvc* get_hiuvc()
{
    return &__hi_uvc;
}

void release_hiuvc(hiuvc *uvc)
{
}
