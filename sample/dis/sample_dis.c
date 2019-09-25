

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
#include <sys/ioctl.h>
#ifdef HI_GYRODIS_SUPPORT
#include <linux/spi/spidev.h>
#include "hi_comm_motionsensor.h"
#include "motionsensor_ext.h"
#include "motionsensor_chip_cmd.h"
#include "motionsensor_mng_cmd.h"
#include "hi_comm_motionfusion.h"
#include "mpi_motionfusion.h"
#endif
#include "sample_comm.h"

HI_VOID SAMPLE_DIS_Usage(HI_CHAR *sPrgNm)
{
    printf("Usage : %s <index>\n", sPrgNm);
    printf("index:\n");
    printf("\t 0)DIS-4DOF_GME.VI-VO VENC.\n");
    printf("\t 1)DIS-6DOF_GME.VI-VO VENC.\n");
#ifdef HI_GYRODIS_SUPPORT
    printf("\t 2)DIS_GYRO.VI-VO VENC.\n");
#endif
    return;
}

#ifdef HI_GYRODIS_SUPPORT
#define X_BUF_LEN               (400)
#define GYRO_BUF_LEN            ((4*4*X_BUF_LEN)+8*X_BUF_LEN)
HI_S32 g_s32MotionsensorDevFd = -1;
HI_S32 g_s32MotionsensorMngDevFd = -1;
HI_BOOL g_bGyroStarted = HI_FALSE;
MSENSOR_BUF_ATTR_S  g_stMotionsensorAttr;


HI_S32 SAMPLE_SPI_init(void)
{
    HI_S32  fd, s32Ret;
    HI_S32 s32mode = SPI_MODE_3/* | SPI_LSB_FIRST | SPI_LOOP | SPI_CS_HIGH*/;
    HI_S32 s32bits = 8;
    HI_U64 u64speed = 10000000;
    //HI_BOOL endianmode = 0;


    fd = open("/dev/spidev1.0", O_RDWR);

    if (fd < 0)
    {
        perror("open");
        return HI_FAILURE;
    }

    /*set spi mode */
    s32Ret = ioctl(fd, SPI_IOC_WR_MODE, &s32mode);//SPI_IOC_WR_MODE32

    if (s32Ret == HI_FAILURE)
    {
        perror("can't set spi mode");
        close(fd);
        return HI_FAILURE;
    }

    s32Ret = ioctl(fd, SPI_IOC_RD_MODE, &s32mode);//SPI_IOC_RD_MODE32

    if (s32Ret == HI_FAILURE)
    {
        perror("can't get spi mode");
        close(fd);
        return HI_FAILURE;

    }

    /*
     * bits per word
     */
    s32Ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &s32bits);

    if (s32Ret == HI_FAILURE)
    {
        perror("can't set bits per word");
        close(fd);
        return HI_FAILURE;
    }

    s32Ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &s32bits);

    if (s32Ret == HI_FAILURE)
    {
        perror("can't get bits per word");
        close(fd);
        return HI_FAILURE;
    }

    /*set spi max speed*/
    s32Ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &u64speed);

    if (s32Ret == HI_FAILURE)
    {
        perror("can't set bits max speed HZ");
        close(fd);
        return HI_FAILURE;
    }

    s32Ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &u64speed);

    if (s32Ret == HI_FAILURE)
    {
        perror("can't set bits max speed HZ");
        close(fd);
        return HI_FAILURE;
    }

    close(fd);
    return HI_SUCCESS;
}

HI_S32 SAMPLE_MOTIONFUSION_SetParam(HI_VOID)
{
    HI_S32 s32Ret                   = HI_SUCCESS;
    HI_BOOL bEnDrift                = HI_TRUE;
    IMU_DRIFT aGyroDrift            = {0,0,0};
    HI_BOOL bEnSixSideCal           = HI_FALSE;
    IMU_MATRIX aRotationMatrix      = {-1,0,0,0,1,0,0,0,-1};
    MFUSION_ATTR_S stMFusionAttr;
    HI_U32 u32FusionID = 0;

    stMFusionAttr.u32DeviceMask      = MFUSION_DEVICE_GYRO;
    stMFusionAttr.u32TemperatureMask = MFUSION_TEMP_GYRO;
    s32Ret = HI_MPI_MOTIONFUSION_SetAttr(u32FusionID, &stMFusionAttr);
    if(HI_SUCCESS != s32Ret)
    {
        goto end;
    }

    s32Ret = HI_MPI_MOTIONFUSION_SetGyroDrift(u32FusionID,bEnDrift,aGyroDrift);
    if(HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_MOTIONFUSION_SetAttr failed!\n");
        goto end;
    }

    s32Ret = HI_MPI_MOTIONFUSION_SetGyroSixSideCal(u32FusionID, bEnSixSideCal,aRotationMatrix);
    if(HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_MOTIONFUSION_SetAttr failed!\n");
        goto end;
    }

end:
    return s32Ret;
}


HI_S32 SAMPLE_Motionsensor_Init()
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32BufSize = 0;
    MSENSOR_PARAM_S stMSensorParamSet;

    u32BufSize = GYRO_BUF_LEN;

    g_s32MotionsensorDevFd = open("/dev/motionsensor_chip", O_RDWR);
    if (g_s32MotionsensorDevFd < 0)
    {
        SAMPLE_PRT("Error: cannot open MotionSensor device.may not load motionsensor driver !\n");
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_SYS_MmzAlloc(&g_stMotionsensorAttr.u64PhyAddr, (HI_VOID **)&g_stMotionsensorAttr.pVirAddr,
                                 "MotionsensorData", NULL, u32BufSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("alloc mmz for Motionsensor failed,s32Ret:%x !\n",s32Ret);
        s32Ret =  HI_ERR_VI_NOMEM;
        return s32Ret;
    }

    memset((HI_VOID *)g_stMotionsensorAttr.pVirAddr, 0, u32BufSize);

    g_stMotionsensorAttr.u32Buflen = u32BufSize;

    //set device work mode
    stMSensorParamSet.stMSensorAttr.u32DeviceMask = MSENSOR_DEVICE_GYRO | MSENSOR_DEVICE_ACC;
    stMSensorParamSet.stMSensorAttr.u32TemperatureMask = MSENSOR_TEMP_GYRO | MSENSOR_TEMP_ACC;
    //set gyro samplerate and full scale range
    stMSensorParamSet.stMSensorConfig.stGyroConfig.u64ODR = 1000 * GRADIENT ;
    stMSensorParamSet.stMSensorConfig.stGyroConfig.u64FSR = 250 * GRADIENT;
    //set accel samplerate and full scale range
    stMSensorParamSet.stMSensorConfig.stAccConfig.u64ODR = 1000 * GRADIENT ;
    stMSensorParamSet.stMSensorConfig.stAccConfig.u64FSR = 4 * GRADIENT;

    printf("+++fun:%s line:%d\n",__func__,__LINE__);

    memcpy(&stMSensorParamSet.stMSensorBufAttr,&g_stMotionsensorAttr,sizeof(MSENSOR_BUF_ATTR_S));

    //stStatus.u8AxisMode = MODE_3_AXIS_GYRO_FIFO;
    //stStatus.u8AxisMode.u32DeviceMask = MOTIONSENSOR_DEVICE_GYRO | MOTIONSENSOR_DEVICE_ACC;


    s32Ret =  ioctl(g_s32MotionsensorDevFd, MSENSOR_CMD_INIT, &stMSensorParamSet);
    if(s32Ret)
    {
        perror("MSENSOR_CMD_INIT");
        return -1;
    }

    return s32Ret;
}

HI_S32 SAMPLE_Motionsensor_DeInit(void)
{
    HI_S32 s32Ret = HI_SUCCESS;

    if(g_s32MotionsensorDevFd < 0)
    {
        return HI_FAILURE;
    }

    s32Ret = ioctl(g_s32MotionsensorDevFd, MSENSOR_CMD_DEINIT, NULL);
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("motionsensor deinit failed , s32Ret:0x%x !\n",s32Ret);
    }

    s32Ret = HI_MPI_SYS_MmzFree(g_stMotionsensorAttr.u64PhyAddr, (HI_VOID *)g_stMotionsensorAttr.pVirAddr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("motionsensor mmz free failed, s32Ret:0x%x !\n",s32Ret);
    }

    g_stMotionsensorAttr.u64PhyAddr = 0;
    g_stMotionsensorAttr.pVirAddr   = (HI_VOID*)NULL;

    close(g_s32MotionsensorDevFd);
    g_s32MotionsensorDevFd = -1;

    return s32Ret;
}

HI_S32 SAMPLE_Motionsensor_Start()
{
    HI_S32 s32Ret = HI_SUCCESS;
    s32Ret =  ioctl(g_s32MotionsensorDevFd, MSENSOR_CMD_START, NULL);
    if(s32Ret)
    {
        perror("IOCTL_CMD_START_MPU");
        return -1;
    }

    g_bGyroStarted = HI_TRUE;
    return s32Ret;
}

HI_S32 SAMPLE_Motionsensor_Stop()
{
    HI_S32 s32Ret = HI_SUCCESS;
    s32Ret = ioctl(g_s32MotionsensorDevFd, MSENSOR_CMD_STOP, NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("stop motionsensor failed!\n");
    }

    return s32Ret;
}
#endif

/******************************************************************************
* funciton : Get param by diffrent sensor
******************************************************************************/
HI_S32 SAMPLE_DIS_GetParamBySensor(SAMPLE_SNS_TYPE_E enMode, DIS_CONFIG_S *pstDISCfg, DIS_ATTR_S *pstDISAttr)
{
    HI_S32 s32Ret = HI_SUCCESS;

    if (NULL == pstDISCfg || NULL == pstDISAttr)
    {
        return HI_FAILURE;
    }

    switch (enMode)
    {
        default:
            pstDISCfg->u32FrameRate = 30;
            pstDISAttr->s32Timelag  = 33333;
            break;
    }

    return s32Ret;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_DIS_HandleSig(HI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo)
    {
#ifdef HI_GYRODIS_SUPPORT
        if(HI_TRUE == g_bGyroStarted)
        {
            SAMPLE_Motionsensor_Stop();
            SAMPLE_Motionsensor_DeInit();
        }
#endif
        SAMPLE_COMM_VENC_StopGetStream();
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_VO_HdmiStop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

HI_S32 SAMPLE_DIS_4DOF_GME(HI_VOID)
{
    HI_S32 s32Ret               = HI_SUCCESS;
    HI_U32 u32BlkSize;
    SIZE_S stSize;
    VB_CONFIG_S stVbConfig;
    PIC_SIZE_E  enPicSize       = PIC_3840x2160;
    VI_VPSS_MODE_E     enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;

    VI_DEV  ViDev               = 0;
    VI_PIPE ViPipe              = 0;
    VI_CHN  ViChn               = 0;
    HI_S32  s32WorkSnsId        = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

    VPSS_GRP VpssGrp            = 0;
    VPSS_CHN VpssChn            = 0;
    VPSS_GRP_ATTR_S         stVpssGrpAttr       = {0};
    VPSS_CHN_ATTR_S         stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL                 abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};

    VO_CHN VoChn                = 0;
    SAMPLE_VO_CONFIG_S stVoConfig;

    HI_U32 u32Profile           = 0;
    HI_BOOL bRcnRefShareBuf     = HI_FALSE;
    HI_U32 s32ChnNum            = 1;
    VENC_CHN VencChn            = 0;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E enRcMode        = SAMPLE_RC_CBR;
    PAYLOAD_TYPE_E enPayLoad    = PT_H265;

    DIS_CONFIG_S stDISConfig     = {0};
    DIS_ATTR_S stDISAttr         = {0};

    /************************************************
    step 1:  get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);

    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = s32WorkSnsId;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, 0);
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
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
    step 2:  get input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step 3: init SYS and common VB
    *************************************************/
    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    stVbConfig.u32MaxPoolCnt                = 128;
    stVbConfig.astCommPool[0].u64BlkSize    = u32BlkSize;
    stVbConfig.astCommPool[0].u32BlkCnt     = stViConfig.s32WorkingViNum * 20;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, 0);
    stVbConfig.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConfig.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("init sys fail.s32Ret:0x%x !\n", s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        goto EXIT;
    }

    /************************************************
    step 4: start VI
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 5: set DIS config & attribute
    *************************************************/
    stDISConfig.enMode              = DIS_MODE_4_DOF_GME;
    stDISConfig.enMotionLevel       = DIS_MOTION_LEVEL_NORMAL;
    stDISConfig.u32CropRatio        = 80;
    stDISConfig.u32BufNum           = 5;
    stDISConfig.u32FrameRate        = 30;
    stDISConfig.enPdtType           = DIS_PDT_TYPE_DV;
    stDISConfig.u32GyroOutputRange  = 0;
    stDISConfig.bScale              = HI_FALSE;
    stDISConfig.bCameraSteady       = HI_FALSE;
    stDISConfig.u32GyroDataBitWidth = 0;

    stDISAttr.bEnable               = HI_TRUE;
    stDISAttr.u32MovingSubjectLevel = 0;
    stDISAttr.s32RollingShutterCoef = 0;
    stDISAttr.s32Timelag            = 0;
    stDISAttr.u32ViewAngle          = 1000;
    stDISAttr.bStillCrop            = HI_FALSE;
    stDISAttr.u32HorizontalLimit    = 512;
    stDISAttr.u32VerticalLimit      = 512;
    stDISAttr.bGdcBypass            = HI_FALSE;

    s32Ret = SAMPLE_DIS_GetParamBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &stDISConfig, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_DIS_GetParamBySensor failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = HI_MPI_VI_SetChnDISConfig(ViPipe, ViChn, &stDISConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis config failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 6:  start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.enPixelFormat                  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.enDynamicRange                 = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;

    abChnEnable[0]                               = HI_TRUE;
    stVpssChnAttr[0].u32Width                    = stSize.u32Width;
    stVpssChnAttr[0].u32Height                   = stSize.u32Height;
    stVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    stVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_NONE;
    stVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    stVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    stVpssChnAttr[0].u32Depth                    = 1;
    stVpssChnAttr[0].bMirror                     = HI_FALSE;
    stVpssChnAttr[0].bFlip                       = HI_FALSE;
    stVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr, stVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 7:  start VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT2;
    }

    /************************************************
    step 8:  VO bind VPSS
    *************************************************/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step 9:  VI bind VPSS
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vi bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT4;
    }

    /************************************************
    step 10:  start VENC
    *************************************************/
    s32Ret = SAMPLE_COMM_VENC_GetGopAttr(VENC_GOPMODE_NORMALP, &stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("venc get Gop attr failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = SAMPLE_COMM_VENC_Start(VencChn, enPayLoad, enPicSize, enRcMode, u32Profile,bRcnRefShareBuf, &stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start venc failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn, VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vpss bind venc failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT6;
    }

    /************************************************
    step 11: stream VENC process -- get stream, then save it to file.
    *************************************************/
    s32Ret = SAMPLE_COMM_VENC_StartGetStream(&VencChn,s32ChnNum);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("venc start get stream failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT7;
    }

    printf("\nplease hit the Enter key to Disable DIS!\n");
    getchar();

    stDISAttr.bEnable = HI_FALSE;
    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT8;
    }

    printf("\nplease hit the Enter key to enable DIS!\n");
    getchar();

    stDISAttr.bEnable = HI_TRUE;
    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT8;
    }

    printf("\nplease hit the Enter key to exit!\n");
    getchar();

    /************************************************
    step 12: exit process
    *************************************************/
EXIT8:
    SAMPLE_COMM_VENC_StopGetStream();
EXIT7:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp, VpssChn, VencChn);
EXIT6:
    SAMPLE_COMM_VENC_Stop(VencChn);
EXIT5:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe, ViChn, VpssGrp);
EXIT4:
    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
EXIT3:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

HI_S32 SAMPLE_DIS_6DOF_GME(HI_VOID)
{
    HI_S32 s32Ret               = HI_SUCCESS;
    HI_U32 u32BlkSize;
    SIZE_S stSize;
    VB_CONFIG_S stVbConfig;
    PIC_SIZE_E  enPicSize       = PIC_3840x2160;
    VI_VPSS_MODE_E     enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;

    VI_DEV  ViDev               = 0;
    VI_PIPE ViPipe              = 0;
    VI_CHN  ViChn               = 0;
    HI_S32  s32WorkSnsId        = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

    VPSS_GRP VpssGrp            = 0;
    VPSS_CHN VpssChn            = 0;
    VPSS_GRP_ATTR_S         stVpssGrpAttr       = {0};
    VPSS_CHN_ATTR_S         stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL                 abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};

    VO_CHN VoChn                = 0;
    SAMPLE_VO_CONFIG_S stVoConfig;

    HI_U32 u32Profile           = 0;
    HI_BOOL bRcnRefShareBuf     = HI_FALSE;
    HI_U32 s32ChnNum            = 1;
    VENC_CHN VencChn            = 0;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E enRcMode        = SAMPLE_RC_CBR;
    PAYLOAD_TYPE_E enPayLoad    = PT_H265;

    DIS_CONFIG_S stDISConfig     = {0};
    DIS_ATTR_S stDISAttr         = {0};

    /************************************************
    step 1:  get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = s32WorkSnsId;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, 0);
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
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
    step 2:  get input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step 3: init SYS and common VB
    *************************************************/

    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    stVbConfig.u32MaxPoolCnt                = 128;
    stVbConfig.astCommPool[0].u64BlkSize    = u32BlkSize;
    stVbConfig.astCommPool[0].u32BlkCnt     = stViConfig.s32WorkingViNum * 20;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, 0);
    stVbConfig.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConfig.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("init sys fail.s32Ret:0x%x !\n", s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        goto EXIT;
    }


    /************************************************
    step 4: start VI
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 5: set DIS config & attribute
    *************************************************/
    stDISConfig.enMode              = DIS_MODE_6_DOF_GME;
    stDISConfig.enMotionLevel       = DIS_MOTION_LEVEL_NORMAL;
    stDISConfig.u32CropRatio        = 80;
    stDISConfig.u32BufNum           = 5;
    stDISConfig.u32FrameRate        = 30;
    stDISConfig.enPdtType           = DIS_PDT_TYPE_IPC;
    stDISConfig.u32GyroOutputRange  = 0;
    stDISConfig.bScale              = HI_TRUE;
    stDISConfig.bCameraSteady       = HI_FALSE;
    stDISConfig.u32GyroDataBitWidth = 0;

    stDISAttr.bEnable               = HI_TRUE;
    stDISAttr.u32MovingSubjectLevel = 0;
    stDISAttr.s32RollingShutterCoef = 0;
    stDISAttr.s32Timelag            = 0;
    stDISAttr.u32ViewAngle          = 1000;
    stDISAttr.bStillCrop            = HI_FALSE;
    stDISAttr.u32HorizontalLimit    = 512;
    stDISAttr.u32VerticalLimit      = 512;
    stDISAttr.bGdcBypass            = HI_FALSE;

    s32Ret = SAMPLE_DIS_GetParamBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &stDISConfig, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_DIS_GetParamBySensor failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = HI_MPI_VI_SetChnDISConfig(ViPipe, ViChn, &stDISConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis config failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 6:  start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.enPixelFormat                  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.enDynamicRange                 = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;

    abChnEnable[0]                               = HI_TRUE;
    stVpssChnAttr[0].u32Width                    = stSize.u32Width;
    stVpssChnAttr[0].u32Height                   = stSize.u32Height;
    stVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    stVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_NONE;
    stVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    stVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    stVpssChnAttr[0].u32Depth                    = 1;
    stVpssChnAttr[0].bMirror                     = HI_FALSE;
    stVpssChnAttr[0].bFlip                       = HI_FALSE;
    stVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr, stVpssChnAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 7:  start VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT2;
    }

    /************************************************
    step 8:  VO bind VPSS
    *************************************************/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step 9:  VI bind VPSS
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vi bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT4;
    }

    /************************************************
    step 10:  start VENC
    *************************************************/
    s32Ret = SAMPLE_COMM_VENC_GetGopAttr(VENC_GOPMODE_NORMALP, &stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("venc get Gop attr failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = SAMPLE_COMM_VENC_Start(VencChn, enPayLoad, enPicSize, enRcMode, u32Profile, bRcnRefShareBuf,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start venc failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn, VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vpss bind venc failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT6;
    }

    /************************************************
    step 11: stream VENC process -- get stream, then save it to file.
    *************************************************/
    s32Ret = SAMPLE_COMM_VENC_StartGetStream(&VencChn,s32ChnNum);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("venc start get stream failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT7;
    }

    printf("\nplease hit the Enter key to Disable DIS!\n");
    getchar();

    stDISAttr.bEnable = HI_FALSE;
    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT8;
    }

    printf("\nplease hit the Enter key to enable DIS!\n");
    getchar();

    stDISAttr.bEnable = HI_TRUE;
    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT8;
    }

    printf("\nplease hit the Enter key to exit!\n");
    getchar();

    /************************************************
    step 12: exit process
    *************************************************/
EXIT8:
    SAMPLE_COMM_VENC_StopGetStream();
EXIT7:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp, VpssChn, VencChn);
EXIT6:
    SAMPLE_COMM_VENC_Stop(VencChn);
EXIT5:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe, ViChn, VpssGrp);
EXIT4:
    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
EXIT3:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

#ifdef HI_GYRODIS_SUPPORT
HI_S32 SAMPLE_DIS_Gyro(HI_VOID)
{
    HI_S32 s32Ret               = HI_SUCCESS;
    HI_U32 u32BlkSize;
    SIZE_S stSize;
    VB_CONFIG_S stVbConfig;
    PIC_SIZE_E  enPicSize       = PIC_3840x2160;
    VI_VPSS_MODE_E     enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;

    VI_DEV  ViDev               = 0;
    VI_PIPE ViPipe              = 0;
    VI_CHN  ViChn               = 0;
    HI_S32  s32WorkSnsId        = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

    VPSS_GRP VpssGrp            = 0;
    VPSS_CHN VpssChn            = 0;
    VPSS_GRP_ATTR_S         stVpssGrpAttr       = {0};
    VPSS_CHN_ATTR_S         stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
    HI_BOOL                 abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};

    VO_CHN VoChn                = 0;
    SAMPLE_VO_CONFIG_S stVoConfig;

    HI_U32 u32Profile           = 0;
    HI_BOOL bRcnRefShareBuf     = HI_FALSE;
    HI_U32 s32ChnNum            = 1;
    VENC_CHN VencChn            = 0;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E enRcMode        = SAMPLE_RC_CBR;
    PAYLOAD_TYPE_E enPayLoad    = PT_H265;

    DIS_CONFIG_S stDISConfig     = {0};
    DIS_ATTR_S stDISAttr         = {0};

    VI_LDCV2_ATTR_S stLDCV2Attr  = {0};

    /************************************************
    step 1:  get all sensors information
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                           = 1;

    stViConfig.as32WorkingViId[0]                        = s32WorkSnsId;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, 0);
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enMastPipeMode;
    ;
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
    step 2:  get input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /************************************************
    step 3: init SYS and common VB
    *************************************************/
    memset(&stVbConfig, 0, sizeof(VB_CONFIG_S));

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_420, DATA_BITWIDTH_8, COMPRESS_MODE_SEG, 0);
    stVbConfig.u32MaxPoolCnt                = 128;
    stVbConfig.astCommPool[0].u64BlkSize    = u32BlkSize;
    stVbConfig.astCommPool[0].u32BlkCnt     = stViConfig.s32WorkingViNum * 20;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, 0);
    stVbConfig.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConfig.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("init sys fail.s32Ret:0x%x !\n", s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        goto EXIT;
    }

    /************************************************
    step 4: start VI
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 5: init and start gyro
    *************************************************/
    s32Ret = SAMPLE_SPI_init();
    if(HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("init spi fail.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = SAMPLE_Motionsensor_Init();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("init gyro fail.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = SAMPLE_Motionsensor_Start();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start gyro fail.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 6: set ldcV2 config & attribute
    *************************************************/
    stLDCV2Attr.bEnable = HI_TRUE;
    stLDCV2Attr.stAttr.s32FocalLenX = 411053/2;
    stLDCV2Attr.stAttr.s32FocalLenY = 410778/2;
    stLDCV2Attr.stAttr.s32CoorShiftX = 189478/2;
    stLDCV2Attr.stAttr.s32CoorShiftY = 111680/2;

    stLDCV2Attr.stAttr.as32SrcCaliRatio[0][0] = 100000;
    stLDCV2Attr.stAttr.as32SrcCaliRatio[0][1] = -53548;
    stLDCV2Attr.stAttr.as32SrcCaliRatio[0][2] = 24828;
    stLDCV2Attr.stAttr.as32SrcCaliRatio[0][3] = 0;
    stLDCV2Attr.stAttr.s32SrcJunPt = 800000;

    stLDCV2Attr.stAttr.as32DstCaliRatio[0][0] = 0;
    stLDCV2Attr.stAttr.as32DstCaliRatio[0][1] = 0;
    stLDCV2Attr.stAttr.as32DstCaliRatio[0][2] = 0;
    stLDCV2Attr.stAttr.as32DstCaliRatio[0][3] = 0;
    stLDCV2Attr.stAttr.as32DstCaliRatio[1][0] = 99996;
    stLDCV2Attr.stAttr.as32DstCaliRatio[1][1] = 53783;
    stLDCV2Attr.stAttr.as32DstCaliRatio[1][2] = 56290;
    stLDCV2Attr.stAttr.as32DstCaliRatio[1][3] = 115816;
    stLDCV2Attr.stAttr.as32DstCaliRatio[2][0] = 104266;
    stLDCV2Attr.stAttr.as32DstCaliRatio[2][1] = -2382;
    stLDCV2Attr.stAttr.as32DstCaliRatio[2][2] = 303710;
    stLDCV2Attr.stAttr.as32DstCaliRatio[2][3] = -249381;

    stLDCV2Attr.stAttr.as32DstJunPt[0] = 0;
    stLDCV2Attr.stAttr.as32DstJunPt[1] = 46644;

    s32Ret = HI_MPI_VI_SetChnLDCV2Attr(ViPipe, ViChn, &stLDCV2Attr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set ldcV2 config failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 6: set DIS config & attribute
    *************************************************/
    stDISConfig.enMode              = DIS_MODE_GYRO;
    stDISConfig.enMotionLevel       = DIS_MOTION_LEVEL_NORMAL;
    stDISConfig.u32CropRatio        = 80;
    stDISConfig.u32BufNum           = 5;
    stDISConfig.enPdtType           = DIS_PDT_TYPE_IPC;
    stDISConfig.u32GyroOutputRange  = 25000;
    stDISConfig.u32GyroDataBitWidth = 15;
    stDISConfig.bScale              = HI_FALSE;
    stDISConfig.bCameraSteady       = HI_TRUE;

    stDISAttr.bEnable               = HI_TRUE;
    stDISAttr.u32MovingSubjectLevel = 0;
    stDISAttr.s32RollingShutterCoef = 0;
    stDISAttr.u32ViewAngle          = 120;
    stDISAttr.bStillCrop            = HI_FALSE;
    stDISAttr.u32HorizontalLimit    = 512;
    stDISAttr.u32VerticalLimit      = 512;

    s32Ret = SAMPLE_DIS_GetParamBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &stDISConfig, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_DIS_GetParamBySensor failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = HI_MPI_VI_SetChnDISConfig(ViPipe, ViChn, &stDISConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis config failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    s32Ret = SAMPLE_MOTIONFUSION_SetParam();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("motionfusion set param fail.s32Ret:0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 7:  start VPSS
    *************************************************/
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.enPixelFormat                  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssGrpAttr.enDynamicRange                 = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;

    abChnEnable[0]                               = HI_TRUE;
    stVpssChnAttr[0].u32Width                    = stSize.u32Width;
    stVpssChnAttr[0].u32Height                   = stSize.u32Height;
    stVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    stVpssChnAttr[0].enCompressMode              = COMPRESS_MODE_NONE;
    stVpssChnAttr[0].enDynamicRange              = DYNAMIC_RANGE_SDR8;
    stVpssChnAttr[0].enPixelFormat               = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssChnAttr[0].enVideoFormat               = VIDEO_FORMAT_LINEAR;
    stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    stVpssChnAttr[0].u32Depth                    = 1;
    stVpssChnAttr[0].bMirror                     = HI_FALSE;
    stVpssChnAttr[0].bFlip                       = HI_FALSE;
    stVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr, stVpssChnAttr);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 8:  start VO
    *************************************************/
    SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);

    s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT2;
    }

    /************************************************
    step 9:  VO bind VPSS
    *************************************************/
    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vo bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step 10:  VI bind VPSS
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vi bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT4;
    }

    /************************************************
    step 11:  start VENC
    *************************************************/
    s32Ret = SAMPLE_COMM_VENC_GetGopAttr(VENC_GOPMODE_NORMALP, &stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("venc get Gop attr failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = SAMPLE_COMM_VENC_Start(VencChn, enPayLoad, enPicSize, enRcMode, u32Profile, bRcnRefShareBuf,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start venc failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn, VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("vpss bind venc failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT6;
    }

    /************************************************
    step 12: stream VENC process -- get stream, then save it to file.
    *************************************************/
    s32Ret = SAMPLE_COMM_VENC_StartGetStream(&VencChn,s32ChnNum);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("venc start get stream failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT7;
    }

    printf("\nplease hit the Enter key to Disable DIS!\n");
    getchar();

    stDISAttr.bEnable= HI_FALSE;
    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT8;
    }

    printf("\nplease hit the Enter key to enable DIS!\n");
    getchar();

    stDISAttr.bEnable= HI_TRUE;
    s32Ret = HI_MPI_VI_SetChnDISAttr(ViPipe, ViChn, &stDISAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("set dis attr failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT8;
    }

    printf("\nplease hit the Enter key to exit!\n");
    getchar();

    /************************************************
    step 13: exit process
    *************************************************/
EXIT8:
    SAMPLE_COMM_VENC_StopGetStream();
EXIT7:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp, VpssChn, VencChn);
EXIT6:
    SAMPLE_COMM_VENC_Stop(VencChn);
EXIT5:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe, ViChn, VpssGrp);
EXIT4:
    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp, VpssChn, stVoConfig.VoDev, VoChn);
EXIT3:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_Motionsensor_Stop();
    SAMPLE_Motionsensor_DeInit();
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}
#endif

/******************************************************************************
* function    : main()
* Description :
******************************************************************************/
#ifdef __HuaweiLite__
    int app_main(int argc, char *argv[])
#else
    int main(int argc, char* argv[])
#endif
{
    HI_S32 s32Ret = HI_FAILURE;
    if (argc < 2 || argc > 2)
    {
        SAMPLE_DIS_Usage(argv[0]);
        return HI_FAILURE;
    }

    if (!strncmp(argv[1], "-h", 2))
    {
        SAMPLE_DIS_Usage(argv[0]);
        return HI_SUCCESS;
    }

#ifndef __HuaweiLite__
    signal(SIGINT, SAMPLE_DIS_HandleSig);
    signal(SIGTERM, SAMPLE_DIS_HandleSig);
#endif

    switch (*argv[1])
    {
        case '0':
            s32Ret = SAMPLE_DIS_4DOF_GME();
            break;
        case '1':
            s32Ret = SAMPLE_DIS_6DOF_GME();
            break;

#ifdef HI_GYRODIS_SUPPORT
        case '2':
            s32Ret = SAMPLE_DIS_Gyro();
            break;
#endif
        default:
            SAMPLE_PRT("the index is invaild!\n");
            SAMPLE_DIS_Usage(argv[0]);
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

