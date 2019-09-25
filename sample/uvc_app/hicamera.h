#ifndef __HI_CAMERA_H__
#define __HI_CAMERA_H__

typedef enum histream_type_e
{
    HI_FORMAT_H264  = 0x1,
    HI_FORMAT_MJPEG = 0x2,
    HI_FORMAT_MJPG = 0x2,
    HI_FORMAT_YUV = 0x3,
    HI_FORMAT_YUV420 = 0x3
} histream_type_e;

typedef enum histream_resolution_e
{
    HI_RESOLUTION_1080 = 0x1,
    HI_RESOLUTION_720 = 0x2,
    HI_RESOLUTION_360 = 0x3
} histream_resolution_e;

typedef struct encoder_property
{
    unsigned int  format;
    unsigned int  width;
    unsigned int  height;
    unsigned char compsite;
} encoder_property;

#endif //__HI_CAMERA_H__
