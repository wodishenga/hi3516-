#ifndef __CAMERA_H__
#define __CAMERA_H__

typedef struct hicamera
{
    int (*init)();
    int (*open)();
    int (*close)();
    int (*run)();
} hicamera;

hicamera* get_hicamera();
void release_hicamera(hicamera *camera);

void sample_venc_config(void);
void sample_audio_config(void);

#endif //__CAMERA_H__

