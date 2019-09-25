#ifndef __HI_UAC_H__
#define __HI_UAC_H__

typedef struct hiuac
{
    int (*init)();
    int (*open)();
    int (*close)();
    int (*run)();
} hiuac;

hiuac* get_hiuac();
void release_hiuac(hiuac *uvc);

#endif //__HI_UAC_H__
