#include <stdio.h>
#include <math.h>
//#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "mpi_sys.h"
#include "mpi_vi.h"
#include "mpi_vpss.h"
#include "mpi_vb.h"
#include "mpi_vgs.h"
#include "hi_comm_vb.h"
#include "hi_comm_gdc.h"
#include "hi_buffer.h"

#include "fisheye_calibrate.h"

#define FISHEYE_CALIBRATE_LEVEL LEVEL_1
HI_U32  u32OrigDepth = 0;
static volatile HI_BOOL bQuit = HI_FALSE;   /* bQuit may be set in the signal handler */

static void usage(void)
{
    printf(
        "\n"
        "*************************************************\n"
#ifndef __HuaweiLite__
        "Usage: ./fisheye_calibrate [ViPipe] [ViPhyChn] [ViExtChn] .\n"
#else
        "Usage: fisheye_cal [ViPipe] [ViPhyChn] [ViExtChn] .\n"
#endif
        "1)ViPipe: \n"
        "   the source ViPipe\n"
        "2)ViPhyChn: \n"
        "   the source physic channel bind the exetend channel\n"
        "3)ViExtChn: \n"
        "   the extend channel to execute fisheye correction\n"
        "*)Example:\n"
#ifndef __HuaweiLite__
        "   e.g : ./fisheye_calibrate 0 0 %d\n"
#else
        "   e.g : fisheye_cal 0 0 %d\n"
#endif
        "*************************************************\n"
        "\n", VI_EXT_CHN_START);
}

static void SigHandler(HI_S32 signo)
{
    if (HI_TRUE == bQuit)
    {
        return;
    }

    if (SIGINT == signo || SIGTERM == signo)
    {
        bQuit = HI_TRUE;
        fclose(stdin);  /* close stdin, so getchar will return EOF */
    }
}

HI_S32 FISHEYE_Calibrate_MapVirtAddr( VIDEO_FRAME_INFO_S* pstFrame)
{
    HI_S32 size = 0;
    HI_U64 phy_addr = 0;
    PIXEL_FORMAT_E enPixelFormat;
    HI_U32 u32YSize;
    HI_U32 u32UVInterval = 0;

    enPixelFormat = pstFrame->stVFrame.enPixelFormat;
    phy_addr = pstFrame->stVFrame.u64PhyAddr[0];

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == enPixelFormat)
    {
        size = (pstFrame->stVFrame.u32Stride[0]) * (pstFrame->stVFrame.u32Height) * 3 / 2;
        u32YSize = pstFrame->stVFrame.u32Stride[0] * pstFrame->stVFrame.u32Height;
        u32UVInterval = 0;
    }
    else if (PIXEL_FORMAT_YVU_PLANAR_420 == enPixelFormat)
    {

        size = (pstFrame->stVFrame.u32Stride[0]) * (pstFrame->stVFrame.u32Height) * 3 / 2;
        u32YSize = pstFrame->stVFrame.u32Stride[0] * pstFrame->stVFrame.u32Height;
        u32UVInterval = pstFrame->stVFrame.u32Width * pstFrame->stVFrame.u32Height / 2;
    }
    else
    {
        printf("not support pixelformat: %d\n", enPixelFormat);
        return HI_FAILURE;
    }


    pstFrame->stVFrame.u64VirAddr[0] = (HI_U64)(HI_UL) HI_MPI_SYS_MmapCache(phy_addr, size);

    if (HI_NULL == pstFrame->stVFrame.u64VirAddr[0])
    {
        return HI_FAILURE;
    }

    pstFrame->stVFrame.u64VirAddr[1] = pstFrame->stVFrame.u64VirAddr[0] + u32YSize;
    pstFrame->stVFrame.u64VirAddr[2] = pstFrame->stVFrame.u64VirAddr[1] + u32UVInterval;

    return HI_SUCCESS;
}


HI_VOID FISHEYE_Calibrate_SaveSP42XToPlanar(FILE* pfile, VIDEO_FRAME_S* pVBuf)
{
    unsigned int w, h;
    char* pVBufVirt_Y;
    char* pVBufVirt_C;
    char* pMemContent;
    unsigned char* TmpBuff;
    PIXEL_FORMAT_E  enPixelFormat = pVBuf->enPixelFormat;
    HI_U32 u32UvHeight;
    HI_U32 u32LineSize = 2408;

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == enPixelFormat)
    {
        u32UvHeight = pVBuf->u32Height / 2;
    }
    else
    {
        u32UvHeight = pVBuf->u32Height;
    }

    pVBufVirt_Y = (char*)(HI_UL)pVBuf->u64VirAddr[0];
    pVBufVirt_C = (char*)(pVBufVirt_Y + (pVBuf->u32Stride[0]) * (pVBuf->u32Height));

    TmpBuff = (unsigned char*)malloc(u32LineSize);

    if (NULL == TmpBuff)
    {
        printf("Func:%s line:%d -- unable alloc %dB memory for tmp buffer\n",
               __FUNCTION__, __LINE__, u32LineSize);
        return;
    }

    /* save Y ----------------------------------------------------------------*/

    for (h = 0; h < pVBuf->u32Height; h++)
    {
        pMemContent = pVBufVirt_Y + h * pVBuf->u32Stride[0];
        fwrite(pMemContent, 1, pVBuf->u32Width, pfile);
    }

    /* save U ----------------------------------------------------------------*/
    for (h = 0; h < u32UvHeight; h++)
    {
        pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];

        pMemContent += 1;

        for (w = 0; w < pVBuf->u32Width / 2; w++)
        {
            TmpBuff[w] = *pMemContent;
            pMemContent += 2;
        }

        fwrite(TmpBuff, 1, pVBuf->u32Width / 2, pfile);
    }

    /* save V ----------------------------------------------------------------*/
    for (h = 0; h < u32UvHeight; h++)
    {
        pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];

        for (w = 0; w < pVBuf->u32Width / 2; w++)
        {
            TmpBuff[w] = *pMemContent;
            pMemContent += 2;
        }

        fwrite(TmpBuff, 1, pVBuf->u32Width / 2, pfile);
    }

    free(TmpBuff);

    return;
}




HI_S32 FISHEYE_CALIBRATE_MISC_GETVB(VIDEO_FRAME_INFO_S* pstOutFrame, VIDEO_FRAME_INFO_S* pstInFrame,
                                    VB_BLK* pstVbBlk, VB_POOL pool)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32Size;
    VB_BLK VbBlk = VB_INVALID_HANDLE;
    HI_U64 u64PhyAddr;
    HI_U64 u64VirAddr = 0;
    HI_U32 u32Width, u32Height;
    HI_U32 u32Align;
    PIXEL_FORMAT_E  enPixelFormat;
    DATA_BITWIDTH_E enBitWidth;
    COMPRESS_MODE_E enCmpMode;
    VB_CAL_CONFIG_S stCalConfig;
    HI_U32 u32UVInterval;

    u32Align      = 0;
    enBitWidth    = DATA_BITWIDTH_8;
    enCmpMode     = COMPRESS_MODE_NONE;
    u32Width      = pstInFrame->stVFrame.u32Width;
    u32Height     = pstInFrame->stVFrame.u32Height;
    enPixelFormat = pstInFrame->stVFrame.enPixelFormat;

    COMMON_GetPicBufferConfig(u32Width, u32Height, enPixelFormat, enBitWidth, enCmpMode, u32Align, &stCalConfig);

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pstInFrame->stVFrame.enPixelFormat)
    {
        u32UVInterval = 0;
    }
    else if (PIXEL_FORMAT_YVU_PLANAR_420 == pstInFrame->stVFrame.enPixelFormat)
    {
        u32UVInterval = stCalConfig.u32MainYSize >> 2;
    }
    else
    {
        printf("Error!!!, not support PixelFormat: %d\n", pstInFrame->stVFrame.enPixelFormat);
        s32Ret = HI_FAILURE;
        goto end0;
    }

    u32Size = stCalConfig.u32VBSize;

    VbBlk = HI_MPI_VB_GetBlock(pool, u32Size, HI_NULL);
    *pstVbBlk = VbBlk;

    if (VB_INVALID_HANDLE == VbBlk)
    {
        printf("HI_MPI_VB_GetBlock err! size:%d\n", u32Size);
        s32Ret = HI_FAILURE;
        goto end0;
    }

    u64PhyAddr = HI_MPI_VB_Handle2PhysAddr(VbBlk);

    if (0U == u64PhyAddr)
    {
        printf("HI_MPI_VB_Handle2PhysAddr err!\n");
        s32Ret = HI_FAILURE;
        goto end1;
    }

    u64VirAddr = (HI_U64)(HI_UL) HI_MPI_SYS_Mmap(u64PhyAddr, u32Size);

    if (0U == u64VirAddr)
    {
        printf("HI_MPI_SYS_Mmap err!\n");
        s32Ret = HI_FAILURE;
        goto end1;
    }

    pstOutFrame->u32PoolId = HI_MPI_VB_Handle2PoolId(VbBlk);

    if (VB_INVALID_POOLID == pstOutFrame->u32PoolId)
    {
        printf("u32PoolId err!\n");
        s32Ret = HI_FAILURE;
        goto end1;
    }

    pstOutFrame->stVFrame.u64PhyAddr[0] = u64PhyAddr;

    //printf("\nuser u32phyaddr = 0x%x\n", pstOutFrame->stVFrame.u32PhyAddr[0]);
    pstOutFrame->stVFrame.u64PhyAddr[1] = pstOutFrame->stVFrame.u64PhyAddr[0] + stCalConfig.u32MainYSize;
    pstOutFrame->stVFrame.u64PhyAddr[2] = pstOutFrame->stVFrame.u64PhyAddr[1] + u32UVInterval;

    pstOutFrame->stVFrame.u64VirAddr[0] = u64VirAddr;
    pstOutFrame->stVFrame.u64VirAddr[1] = pstOutFrame->stVFrame.u64VirAddr[0] + stCalConfig.u32MainYSize;
    pstOutFrame->stVFrame.u64VirAddr[2] = pstOutFrame->stVFrame.u64VirAddr[1] + u32UVInterval;

    pstOutFrame->stVFrame.u32Width  = u32Width;
    pstOutFrame->stVFrame.u32Height = u32Height;
    pstOutFrame->stVFrame.u32Stride[0] = stCalConfig.u32MainStride;
    pstOutFrame->stVFrame.u32Stride[1] = stCalConfig.u32MainStride;
    pstOutFrame->stVFrame.u32Stride[2] = stCalConfig.u32MainStride;
    pstOutFrame->stVFrame.enField = VIDEO_FIELD_FRAME;
    pstOutFrame->stVFrame.enCompressMode = COMPRESS_MODE_NONE;
    pstOutFrame->stVFrame.enPixelFormat = pstInFrame->stVFrame.enPixelFormat;

end1:

    if (VbBlk != VB_INVALID_HANDLE)
    {
        HI_MPI_VB_ReleaseBlock(VbBlk);
    }

end0:
    return s32Ret;
}




#ifdef __HuaweiLite__
HI_S32 fisheye_calibrate(int argc, char* argv[])
#else
HI_S32 main(int argc, char* argv[])
#endif
{
    HI_U32              u32Width        = 0;
    HI_U32              u32Height       = 0;
    VB_POOL             hPool           = VB_INVALID_POOLID;
    HI_U32              u32BlkSize      = 0;
    HI_S32	            s32Ret          = HI_SUCCESS;

    VB_BLK              VbBlk           = VB_INVALID_HANDLE;
    VIDEO_FRAME_INFO_S  stFrame;
    VIDEO_FRAME_INFO_S*  pstVFramIn     = HI_NULL;
    VIDEO_FRAME_INFO_S  stVFramOut;
    HI_S32              s32MilliSec     = 2000;

    HI_CHAR*            pcPixFrm        = NULL;
    HI_CHAR             szYuvName[128]  = {0};

    CALIBTATE_OUTPUT_S  stOutCalibrate;
    VI_CHN_ATTR_S       stChnAttr;
    FISHEYE_ATTR_S      stFisheyeAttr   = {0};
    VI_PIPE             ViPipe          = 0;
    VI_CHN              ViChn           = 0;
    VI_CHN              ViExtChn        = VI_EXT_CHN_START;
    HI_S32              i               = 0;
    HI_U32              u32OutRadious   = 0;
    FILE*                pfdOut         = NULL;
    VB_POOL_CONFIG_S stVbPoolCfg;

    printf("\nNOTICE: This tool only can be used for TESTING !!!\n");
    printf("NOTICE: This tool only only support PIXEL_FORMAT_YVU_SEMIPLANAR_420\n");
    printf("NOTICE: This tool only only support DYNAMIC_RANGE_SDR8\n");
    printf("NOTICE: This tool only only support COMPRESS_MODE_NONE\n");

    if (argc > 1)
    {
        if (!strncmp(argv[1], "-h", 2))
        {
            usage();
            exit(HI_SUCCESS);
        }

        ViPipe = atoi(argv[1]);
    }

    if (argc > 2)
    {
        ViChn = atoi(argv[2]);
    }

    if (argc > 3)
    {
        ViExtChn = atoi(argv[3]);
    }

    if (argc < 4)
    {
        printf("err para\n");
        usage();
        return HI_FAILURE;
    }

    /* register signal handler */
    signal(SIGINT, SigHandler);
    signal(SIGTERM, SigHandler);

    memset(&stOutCalibrate, 0, sizeof(CALIBTATE_OUTPUT_S));
    memset(&stFisheyeAttr, 0, sizeof(FISHEYE_ATTR_S));

    if ((ViPipe < 0) || (ViPipe >= VI_MAX_PIPE_NUM))
    {
        printf("Not Correct ViPipe Index\n");
        return HI_FAILURE;
    }

    if ((ViChn < 0) || (ViChn >= VI_MAX_PHY_CHN_NUM))
    {
        printf("Not Correct Physic Channel\n");
        return HI_FAILURE;
    }

    if ((ViExtChn < VI_EXT_CHN_START) || (ViExtChn >= VI_MAX_EXT_CHN_NUM))
    {
        printf("Not Correct Extend Channel\n");
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_GetChnAttr(ViPipe, ViChn, &stChnAttr);

    if (HI_SUCCESS != s32Ret)
    {
        printf("get chn attr error!!!\n");
        return HI_FAILURE;
    }

    u32OrigDepth = stChnAttr.u32Depth;
    stChnAttr.u32Depth = 1;
    s32Ret = HI_MPI_VI_SetChnAttr(ViPipe, ViChn, &stChnAttr);

    if (HI_SUCCESS != s32Ret)
    {
        printf("set chn attr error!!!\n");
        return HI_FAILURE;
    }


    usleep(90000);

    if (HI_TRUE == bQuit)
    {
        stChnAttr.u32Depth = u32OrigDepth;
        s32Ret = HI_MPI_VI_SetChnAttr(ViPipe, ViChn, &stChnAttr);

        if (HI_SUCCESS != s32Ret)
        {
            printf("set chn attr error!!!\n");
            return HI_FAILURE;
        }

        return HI_FAILURE;
    }

    if (HI_MPI_VI_GetChnFrame(ViPipe, ViChn, &stFrame, s32MilliSec))
    {
        printf("HI_MPI_VI_GetFrame err, vi chn %d \n", ViChn);
        goto END1;
    }

    pstVFramIn = &stFrame;

    if((COMPRESS_MODE_NONE != pstVFramIn->stVFrame.enCompressMode) || (DYNAMIC_RANGE_SDR8 != pstVFramIn->stVFrame.enDynamicRange)
        || (PIXEL_FORMAT_YVU_SEMIPLANAR_420 != pstVFramIn->stVFrame.enPixelFormat) )
    {
        printf("enCompressMode or enDynamicRange or enPixelFormat not support!!! \n");
        goto END2;
    }

    u32Width = pstVFramIn->stVFrame.u32Width;

    u32Height = pstVFramIn->stVFrame.u32Height;

    pcPixFrm = (PIXEL_FORMAT_YUV_400 == stFrame.stVFrame.enPixelFormat ) ? "single" :
               ((PIXEL_FORMAT_YVU_SEMIPLANAR_420 == stFrame.stVFrame.enPixelFormat) ? "p420" : "p422");

    snprintf(szYuvName, sizeof(szYuvName) - 1, "./fisheye_calibrate_out_%d_%d_%d_%s.yuv", ViChn, u32Width, u32Height, pcPixFrm);

    pfdOut = fopen(szYuvName, "wb");

    if (NULL == pfdOut)
    {
        printf("open file %s err\n", szYuvName);
        //goto END2;
    }

    //Save VI PIC For UserMode
    u32BlkSize = COMMON_GetPicBufferSize(pstVFramIn->stVFrame.u32Width, pstVFramIn->stVFrame.u32Height, pstVFramIn->stVFrame.enPixelFormat, DATA_BITWIDTH_8, COMPRESS_MODE_NONE, 32);
    memset(&stVbPoolCfg, 0, sizeof(VB_POOL_CONFIG_S));
    stVbPoolCfg.u64BlkSize  = u32BlkSize;
    stVbPoolCfg.u32BlkCnt   = 4;
    stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;
    hPool   = HI_MPI_VB_CreatePool(&stVbPoolCfg);

    if (hPool == VB_INVALID_POOLID)
    {
        printf("HI_MPI_VB_CreatePool failed! \n");
        s32Ret = HI_FAILURE;
        goto END2;
    }

    memcpy(&stVFramOut, pstVFramIn, sizeof(VIDEO_FRAME_INFO_S));

    if (HI_SUCCESS != FISHEYE_CALIBRATE_MISC_GETVB(&stVFramOut, pstVFramIn, &VbBlk, hPool))
    {
        printf("FAILURE--fun:%s line:%d\n", __FUNCTION__, __LINE__);
        goto END3;
    }

    s32Ret = FISHEYE_Calibrate_MapVirtAddr(pstVFramIn);

    if (HI_SUCCESS != s32Ret)
    {
        printf("Map Virt Addr Failed!\n");
        goto END4;
    }

    if (HI_TRUE == bQuit)
    {
        goto END5;
    }

    printf("Compute Calibrate Result.....\n");
    s32Ret = HI_FISHEYE_ComputeCalibrateResult(&pstVFramIn->stVFrame, FISHEYE_CALIBRATE_LEVEL, &stOutCalibrate);

    if (HI_SUCCESS != s32Ret)
    {
        printf("Mark Result Failed!\n");
        goto END5;
    }

    if (HI_TRUE == bQuit)
    {
        goto END5;
    }

    printf(" Radius_X=%d,\n Radius_Y=%d,\n Radius=%d,\n OffsetH=%d,\n OffsetV=%d. \n",
           stOutCalibrate.stCalibrateResult.s32Radius_X, stOutCalibrate.stCalibrateResult.s32Radius_Y, stOutCalibrate.stCalibrateResult.u32Radius,
           stOutCalibrate.stCalibrateResult.s32OffsetH, stOutCalibrate.stCalibrateResult.s32OffsetV);

    printf("Mark Calibrate Result.....\n");
    s32Ret = HI_FISHEYE_MarkCalibrateResult(&pstVFramIn->stVFrame, &stVFramOut.stVFrame, &stOutCalibrate.stCalibrateResult);

    if (HI_SUCCESS != s32Ret)
    {
        printf("Mark Result Failed!\n");
        goto END5;
    }

    s32Ret = HI_MPI_VI_GetExtChnFisheye(ViPipe, ViExtChn, &stFisheyeAttr);

    if (HI_SUCCESS != s32Ret)
    {
        printf("Get Fisheye Attr Failed!\n");
        goto END5;
    }

    if (stOutCalibrate.stCalibrateResult.s32OffsetH > 511)
    {
        stFisheyeAttr.s32HorOffset = 511;
    }
    else if (stOutCalibrate.stCalibrateResult.s32OffsetH < -511)
    {
        stFisheyeAttr.s32HorOffset = -511;
    }
    else
    {
        stFisheyeAttr.s32HorOffset = stOutCalibrate.stCalibrateResult.s32OffsetH;
    }

    if (stOutCalibrate.stCalibrateResult.s32OffsetV > 511)
    {
        stFisheyeAttr.s32VerOffset = 511;
    }
    else if (stOutCalibrate.stCalibrateResult.s32OffsetV < -511)
    {
        stFisheyeAttr.s32VerOffset = -511;
    }
    else
    {
        stFisheyeAttr.s32VerOffset = stOutCalibrate.stCalibrateResult.s32OffsetV;
    }

    u32OutRadious = stOutCalibrate.stCalibrateResult.u32Radius;

    //printf("u32RegionNum:%d u32OutRadious:%d \n",stFisheyeAttr.u32RegionNum,u32OutRadious);
    for (i = 0; i < stFisheyeAttr.u32RegionNum; i ++)
    {
        stFisheyeAttr.astFishEyeRegionAttr[i].u32OutRadius = u32OutRadious;
    }


    s32Ret = HI_MPI_VI_SetExtChnFisheye(ViPipe, ViExtChn, &stFisheyeAttr);

    if (HI_SUCCESS != s32Ret)
    {
        printf("Set Fisheye Attr Failed!\n");
        goto END5;
    }

    FISHEYE_Calibrate_SaveSP42XToPlanar(pfdOut, &stVFramOut.stVFrame);

END5:

    HI_MPI_SYS_Munmap((HI_VOID*)(HI_UL)stVFramOut.stVFrame.u64VirAddr[0], u32BlkSize);

END4:

    if (VbBlk != VB_INVALID_HANDLE)
    {
        HI_MPI_VB_ReleaseBlock(VbBlk);
    }

END3:

    if (hPool != VB_INVALID_POOLID)
    {
        HI_MPI_VB_DestroyPool(hPool);
    }

END2:

    HI_MPI_VI_ReleaseChnFrame(ViPipe, ViChn, pstVFramIn);

END1:

    if (-1U != u32OrigDepth)
    {
        stChnAttr.u32Depth = u32OrigDepth;
        s32Ret = HI_MPI_VI_SetChnAttr(ViPipe, ViChn, &stChnAttr);

        if (HI_SUCCESS != s32Ret)
        {
            printf("set chn attr error!!!\n");
            return HI_FAILURE;
        }
    }

    if (NULL != pfdOut)
    {
        fclose(pfdOut);
    }

    printf("Calibrate Finished.....\n");

    return s32Ret;
}
