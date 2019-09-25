#ifndef __SAMPLE_VIO_H__
#define __SAMPLE_VIO_H__

#include "hi_common.h"

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

HI_VOID SAMPLE_VIO_MsgInit(HI_VOID);
HI_VOID SAMPLE_VIO_MsgExit(HI_VOID);

void SAMPLE_VIO_HandleSig(HI_S32 signo);
HI_S32 SAMPLE_VIO_ViOnlineVpssOnlineRoute(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_WDR_LDC_DIS_SPREAD(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ViDoublePipeRoute(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ViWdrSwitch(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ViVpssLowDelay(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_Rotate(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_FPN(HI_U32 u32VoIntfType);

HI_S32 SAMPLE_VIO_ResoSwitch(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ViDoubleWdrPipe(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_SetUsrPic(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_VPSS_VO_MIPI_TX(HI_U32 u32VoIntfType);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* End of #ifndef __SAMPLE_VIO_H__*/
