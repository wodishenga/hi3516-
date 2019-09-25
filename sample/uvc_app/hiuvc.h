#ifndef __HIUVC_H__
#define __HIUVC_H__

typedef struct hiuvc
{
    int (*init)();
    int (*open)();
    int (*close)();
    int (*run)();
} hiuvc;

hiuvc* get_hiuvc();
void release_hiuvc(hiuvc *uvc);

#endif //__HIUVC_H__
