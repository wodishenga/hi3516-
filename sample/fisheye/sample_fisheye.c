

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>

#include "mpi_vgs.h"
#include "mpi_gdc.h"
#include "hi_comm_gdc.h"
#include "hi_comm_vgs.h"
#include "sample_comm.h"


PAYLOAD_TYPE_E g_enVencType   = PT_H265;
SAMPLE_RC_E    g_enRcMode     = SAMPLE_RC_CBR;
PIC_SIZE_E     g_enPicSize    = PIC_1080P;

HI_U16 g_au16LMFCoef[128] = {0, 15, 31, 47, 63, 79, 95, 111, 127, 143, 159, 175,
191, 207, 223, 239, 255, 271, 286, 302, 318, 334, 350, 365, 381, 397, 412,
428, 443, 459, 474, 490, 505, 520, 536, 551, 566, 581, 596, 611, 626, 641,
656, 670, 685, 699, 713, 728, 742, 756, 769, 783, 797, 810, 823, 836, 848,
861, 873, 885, 896, 908, 919, 929, 940, 950, 959, 969, 984, 998, 1013, 1027,
1042, 1056, 1071, 1085, 1100, 1114, 1129, 1143, 1158, 1172, 1187, 1201, 1215,
1230, 1244, 1259, 1273, 1288, 1302, 1317, 1331, 1346, 1360, 1375, 1389, 1404,
1418, 1433, 1447, 1462, 1476, 1491, 1505, 1519, 1534, 1548, 1563, 1577, 1592,
1606, 1621, 1635, 1650, 1664, 1679, 1693, 1708, 1722, 1737, 1751, 1766, 1780, 1795, 1809, 1823, 1838};


pthread_t ThreadId;
HI_BOOL bSetFisheyeAttr = HI_FALSE;

typedef struct hiFISHEYE_SET_ATTR_THREAD_INFO
{
    VI_PIPE     ViPipe;
    VI_CHN         ViChn;
}FISHEYE_SET_ATTR_THRD_INFO;

SAMPLE_VO_CONFIG_S g_stVoConfig = {0};

/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_FISHEYE_Usage(char* sPrgNm)
{
    printf("Usage : %s <index>\n", sPrgNm);
    printf("index:\n");
    printf("\t 0) fisheye normal-fisheye-mode under wall mount.\n");
    return;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_FISHEYE_HandleSig(HI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_VO_HdmiStop();
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

/******************************************************************************
 * Function:    SAMPLE_VIO_FISHEYE_StartViVo
 * Description: online mode / offline mode. Embeded isp, phychn preview
******************************************************************************/
static HI_S32 SAMPLE_FISHEYE_StartViVoVenc(SAMPLE_VI_CONFIG_S* pstViConfig,VI_PIPE ViPipe, VI_CHN ViExtChn, VENC_CHN VencChn, VO_CHN VoChn,SIZE_S *pstDstSize)
{
    HI_S32              s32Ret          = HI_SUCCESS;
    VI_CHN_ATTR_S       stChnAttr;
    VI_EXT_CHN_ATTR_S   stExtChnAttr;
    VENC_GOP_ATTR_S     stGopAttr;
    VO_LAYER            VoLayer         = g_stVoConfig.VoDev;
    VI_CHN              ViChn           = pstViConfig->astViInfo[0].stChnInfo.ViChn;
    HI_U32 u32Profile           = 0;
    HI_BOOL bRcnRefShareBuf     = HI_FALSE;

    if(HI_NULL == pstViConfig)
    {
        SAMPLE_PRT("pstViConfig is NULL\n");
        return HI_FAILURE;
    }
    /******************************************
     step 1: start vi dev & chn to capture
    ******************************************/
    memset(&stChnAttr,0,sizeof(VI_CHN_ATTR_S));

    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        goto exit1;
    }

    s32Ret = HI_MPI_VI_GetChnAttr(ViPipe, ViChn, &stChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get vi chn:%d attr failed with:0x%x!\n", ViChn, s32Ret);
        goto exit2;
    }

    stExtChnAttr.s32BindChn                   = ViChn;
    stExtChnAttr.enCompressMode               = stChnAttr.enCompressMode;
    stExtChnAttr.enPixFormat                  = stChnAttr.enPixelFormat;
    stExtChnAttr.stFrameRate.s32SrcFrameRate  = stChnAttr.stFrameRate.s32SrcFrameRate;
    stExtChnAttr.stFrameRate.s32DstFrameRate  = stChnAttr.stFrameRate.s32DstFrameRate;
    stExtChnAttr.stSize.u32Width              = stChnAttr.stSize.u32Width;
    stExtChnAttr.stSize.u32Height             = stChnAttr.stSize.u32Height;
    stExtChnAttr.u32Depth                     = 0;
    stExtChnAttr.enDynamicRange               = stChnAttr.enDynamicRange;
    stExtChnAttr.enSource                     = VI_EXT_CHN_SOURCE_TAIL;

    pstDstSize->u32Width = stExtChnAttr.stSize.u32Width;
    pstDstSize->u32Height= stExtChnAttr.stSize.u32Height;

    /* start vi dev extern chn */
    s32Ret = HI_MPI_VI_SetExtChnAttr(ViPipe, ViExtChn, &stExtChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set vi extern chn attr failed with: 0x%x.!\n", s32Ret);
        goto exit2;
    }

    /*Enable ext-channel*/
    s32Ret = HI_MPI_VI_EnableChn(ViPipe,ViExtChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("enable vi extern chn failed with: 0x%x.!\n", s32Ret);
        goto exit2;
    }

    /******************************************
    step 2: start VENC
    ******************************************/
    s32Ret = SAMPLE_COMM_VENC_GetGopAttr(VENC_GOPMODE_NORMALP,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO Get GopAttr failed!\n");
        goto exit3;
    }

    s32Ret = SAMPLE_COMM_VENC_Start(VencChn, g_enVencType, g_enPicSize, g_enRcMode, u32Profile, bRcnRefShareBuf,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO start VENC failed with %#x!\n", s32Ret);
        goto exit3;
    }

    /******************************************
    step 3: start VO
    ******************************************/
    s32Ret = SAMPLE_COMM_VO_StartVO(&g_stVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO start VO failed with %#x!\n", s32Ret);
        goto exit4;
    }


    /******************************************
    step 4: Bind Venc
    ******************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VENC(ViPipe,ViExtChn, VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIU_FISHEYE_BindVenc failed with %#x!\n", s32Ret);
        goto exit5;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VO(ViPipe,ViExtChn,VoLayer,VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("VIO sys bind failed with %#x!\n", s32Ret);
        goto exit1;
    }

    return HI_SUCCESS;

exit5:
    SAMPLE_COMM_VO_StopVO(&g_stVoConfig);
exit4:
    SAMPLE_COMM_VENC_Stop(VencChn);
exit3:
    s32Ret = HI_MPI_VI_DisableChn(ViPipe, ViExtChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_DisableChn extchn:%d failed with %#x\n", ViExtChn, s32Ret);
    }
 exit2:
    SAMPLE_COMM_VI_StopVi(pstViConfig);
 exit1:
    return s32Ret;
}

HI_S32 SAMPLE_FISHEYE_StopViVoVenc(SAMPLE_VI_CONFIG_S* pstViConfig, VI_PIPE ViPipe, VI_CHN ViExtChn,VO_CHN VoChn, VENC_CHN VencChn)
{
    HI_S32   s32Ret     = HI_SUCCESS;
    VO_LAYER VoLayer = g_stVoConfig.VoDev;

    s32Ret = HI_MPI_VI_DisableChn(ViPipe,ViExtChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VI_DisableChn extchn:%d failed with %#x\n", ViExtChn, s32Ret);
        return HI_FAILURE;
    }

    SAMPLE_COMM_VI_UnBind_VENC(ViPipe, ViExtChn, VencChn);
    SAMPLE_COMM_VI_UnBind_VO(ViPipe,ViExtChn,VoLayer,VoChn);

    SAMPLE_COMM_VO_StopVO(&g_stVoConfig);

    SAMPLE_COMM_VI_StopVi(pstViConfig);

    return HI_SUCCESS;
}

HI_S32 SAMPLE_FISHEYE_StartViVo(SAMPLE_VI_CONFIG_S* pstViConfig, SAMPLE_VO_CONFIG_S* pstVoConfig)
{
    HI_S32  s32Ret;

    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VO_StartVO(pstVoConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO start VO failed with %#x!\n", s32Ret);
        goto EXIT;
    }

    return s32Ret;

EXIT:
    SAMPLE_COMM_VI_StopVi(pstViConfig);

    return s32Ret;
}

HI_S32 SAMPLE_FISHEYE_StopViVo(SAMPLE_VI_CONFIG_S* pstViConfig, SAMPLE_VO_CONFIG_S* pstVoConfig)
{
    SAMPLE_COMM_VO_StopVO(pstVoConfig);

    SAMPLE_COMM_VI_StopVi(pstViConfig);

    return HI_SUCCESS;
}


/******************************************************************************
* function : vi/vpss: offline/online fisheye mode VI-VO. Embeded isp, phychn channel preview.
******************************************************************************/
HI_S32 SAMPLE_FISHEYE_Normal(HI_VOID)
{
    VI_DEV             ViDev         = 0;
    VI_PIPE            ViPipe        = 0;
    VI_CHN             ViChn         = 0;
    VI_CHN             ViExtChn    = VI_EXT_CHN_START;
    VENC_CHN           VencChn     = 0;
    VO_CHN             VoChn       = 0;
    SIZE_S             stSize;
    HI_U32             u64BlkSize;
    VB_CONFIG_S        stVbConf;
    SAMPLE_VI_CONFIG_S stViConfig;
    HI_S32             s32ChnNum   = 1;
    HI_S32             s32Ret      = HI_SUCCESS;
    SIZE_S             stDstSize;
    FISHEYE_ATTR_S     stFisheyeAttr;

    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, 0);

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = VI_ONLINE_VPSS_OFFLINE;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = DYNAMIC_RANGE_SDR8;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = VIDEO_FORMAT_LINEAR;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = COMPRESS_MODE_NONE;

    /************************************************
        step1:  Get  input size
      *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &g_enPicSize);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(g_enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /******************************************
      step    1: mpp system init
     ******************************************/
    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt = 128;

    /* comm video buffer */
    u64BlkSize = COMMON_GetPicBufferSize(stSize.u32Width,stSize.u32Height,SAMPLE_PIXEL_FORMAT,DATA_BITWIDTH_8,COMPRESS_MODE_NONE,0);
    stVbConf.astCommPool[0].u64BlkSize = u64BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt  =  15;

    /*vb for vi raw*/
    u64BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, HI_FALSE, 0);
    stVbConf.astCommPool[1].u64BlkSize  = u64BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    /******************************************
      step  1: start VI VO  VENC
     ******************************************/
    s32Ret = SAMPLE_FISHEYE_StartViVoVenc(&stViConfig,ViPipe, ViExtChn, VencChn,VoChn,&stDstSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_FISHEYE_StartViVo failed witfh %d\n", s32Ret);
        goto exit;
    }

     /******************************************
     step   2: stream venc process -- get stream, then save it to file.
    ******************************************/
    s32Ret = SAMPLE_COMM_VENC_StartGetStream(&VencChn,s32ChnNum);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VENC_StartGetStream failed witfh %d\n", s32Ret);
        goto exit1;
    }

    /******************************************
      step  3: set fisheye Attr
     ******************************************/

    stFisheyeAttr.bEnable             = HI_TRUE;
    stFisheyeAttr.bLMF                = HI_FALSE;
    stFisheyeAttr.bBgColor            = HI_TRUE;
    stFisheyeAttr.u32BgColor          = COLOR_RGB_BLUE;
    stFisheyeAttr.s32HorOffset        = 161;
    stFisheyeAttr.s32VerOffset        = 183;
    stFisheyeAttr.u32TrapezoidCoef    = 2;
    stFisheyeAttr.s32FanStrength      = 0;
    stFisheyeAttr.enMountMode         = FISHEYE_WALL_MOUNT;
    stFisheyeAttr.u32RegionNum        = 1;

    stFisheyeAttr.astFishEyeRegionAttr[0].enViewMode            = FISHEYE_VIEW_NORMAL;
    stFisheyeAttr.astFishEyeRegionAttr[0].u32InRadius           = 0;
    stFisheyeAttr.astFishEyeRegionAttr[0].u32OutRadius          = 1021;
    stFisheyeAttr.astFishEyeRegionAttr[0].u32Pan                = 180;
    stFisheyeAttr.astFishEyeRegionAttr[0].u32Tilt               = 180;
    stFisheyeAttr.astFishEyeRegionAttr[0].u32HorZoom            = 4095;
    stFisheyeAttr.astFishEyeRegionAttr[0].u32VerZoom            = 4095;
    stFisheyeAttr.astFishEyeRegionAttr[0].stOutRect.s32X        = 0;
    stFisheyeAttr.astFishEyeRegionAttr[0].stOutRect.s32Y        = 0;
    stFisheyeAttr.astFishEyeRegionAttr[0].stOutRect.u32Width    = stDstSize.u32Width;
    stFisheyeAttr.astFishEyeRegionAttr[0].stOutRect.u32Height    = stDstSize.u32Height;

    s32Ret =  HI_MPI_VI_SetExtChnFisheye(ViPipe,ViExtChn,&stFisheyeAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set fisheye attr failed with s32Ret:0x%x!\n", s32Ret);
        goto exit1;
    }

    printf("\nplease press enter, disable fisheye\n");
    getchar();

    stFisheyeAttr.bEnable = HI_FALSE;
    s32Ret =  HI_MPI_VI_SetExtChnFisheye(ViPipe,ViExtChn,&stFisheyeAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set fisheye attr failed with s32Ret:0x%x!\n", s32Ret);
        goto exit1;
    }

    printf("\nplease press enter, enable fisheye\n");
    getchar();

    stFisheyeAttr.bEnable = HI_TRUE;
    s32Ret =  HI_MPI_VI_SetExtChnFisheye(ViPipe,ViExtChn,&stFisheyeAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set fisheye attr failed with s32Ret:0x%x!\n", s32Ret);
        goto exit1;
    }

    PAUSE();

    SAMPLE_COMM_VENC_StopGetStream();

exit1:
    SAMPLE_FISHEYE_StopViVoVenc(&stViConfig, ViPipe, ViExtChn, VencChn,VoChn);

exit:
    SAMPLE_COMM_SYS_Exit();
    return s32Ret;

}

/******************************************************************************
* function    : main()
* Description : video fisheye preview sample
******************************************************************************/
#ifdef __HuaweiLite__
    int app_main(int argc, char* argv[])
#else
    int main(int argc, char* argv[])
#endif
{
    HI_S32             s32Ret        = HI_FAILURE;

    if (argc < 2 || argc > 2)
    {
        SAMPLE_FISHEYE_Usage(argv[0]);
        return HI_FAILURE;
    }

    if (!strncmp(argv[1], "-h", 2))
    {
        SAMPLE_FISHEYE_Usage(argv[0]);
        return HI_SUCCESS;
    }

#ifndef __HuaweiLite__
    signal(SIGINT, SAMPLE_FISHEYE_HandleSig);
    signal(SIGTERM, SAMPLE_FISHEYE_HandleSig);
#endif

    g_enVencType = PT_H265;
    g_stVoConfig.enVoIntfType = VO_INTF_HDMI;

    SAMPLE_COMM_VO_GetDefConfig(&g_stVoConfig);

    switch (*argv[1])
    {
        /* VI/VPSS - VO. Embeded isp, phychn channel preview. */
        case '0':
            s32Ret = SAMPLE_FISHEYE_Normal();
            break;

        default:
            SAMPLE_PRT("the index is invaild!\n");
            SAMPLE_FISHEYE_Usage(argv[0]);
            return HI_FAILURE;
    }

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("program exit normally!\n");
    }
    else
    {
        SAMPLE_PRT("program exit abnormally!\n");
    }

    return s32Ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

