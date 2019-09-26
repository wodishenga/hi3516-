
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs_c.h>


#include <opencv2/highgui.hpp>
#include <opencv2/core/utility.hpp>

#ifndef __SAMPLE_PICTURE_PROCESS_H__
#define __SAMPLE_PICTURE_PROCESS_H__


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#ifndef SAMPLE_PRT
#define SAMPLE_PRT(fmt...)   \
    do {\
        printf("[%s]-%d: ", __FUNCTION__, __LINE__);\
        printf(fmt);\
    }while(0)
#endif

#ifndef PAUSE
#define PAUSE()  do {\
        printf("---------------press Enter key to exit!---------------\n");\
        getchar();\
    } while (0)
#endif

using namespace cv;
using namespace std;


/*ISP图像处理函数*/
extern HI_S32 SAMPLE_Noise_Reduction(VI_PIPE ViPipe);

/*把yuv420格式的图像转化为mat格式*/
extern HI_VOID SAMPLE_Yuv420_to_Mat(Mat &dst, unsigned char* pYUV420, int width, int height);

/*火焰检测算法*/
extern HI_S32 SAMPLE_Flame_Detection(Mat src);

/*图像处理以及算法识别流程*/
extern HI_S32 SAMPLE_Picture_Processing(VIDEO_FRAME_S* pVBuf);



#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* End of #ifndef __SAMPLE_PICPROCESS_H__*/


