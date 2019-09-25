#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <inttypes.h>

#include "hi_common.h"
#include "hi_comm_video.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "hi_comm_vpss.h"
#include "mpi_sys.h"
#include "mpi_vb.h"
#include "mpi_vpss.h"
#include "mpi_vgs.h"

#ifdef CONFIG_HI_TDE_SUPPORT
#include "hi_tde_api.h"
#endif

#include "log.h"
#include "config_svc.h"
#include "frame_cache.h"
#include "sample_comm.h"
#include "sample_yuv.h"

typedef struct sample_getyuv_s
{
    HI_BOOL bThreadStart;
    HI_S32  s32VpssGrp;
    HI_S32  s32VpssChn;
} SAMPLE_GETYUV_PARA_S;

typedef struct hiDUMP_MEMBUF_S
{
    VB_BLK  hBlock;
    VB_POOL hPool;
    HI_U32  u32PoolId;
    HI_U64  u64PhyAddr;
    HI_U8*  pVirAddr;
    HI_S32  s32Mdev;
} DUMP_MEMBUF_S;

static SAMPLE_GETYUV_PARA_S g_stYUVPara;
static pthread_t g_stYUVPid;

static VIDEO_FRAME_INFO_S g_stFrame;

static VB_POOL g_hPool = VB_INVALID_POOLID;
static DUMP_MEMBUF_S g_stMem = {0};
static VGS_HANDLE g_hHandle = -1;
static HI_U32 g_u32BlkSize = 0;

#ifdef CONFIG_HI_TDE_SUPPORT
static VB_POOL g_hPool2 = VB_INVALID_POOLID;
static DUMP_MEMBUF_S g_stMem2 = {0};
static HI_U32 g_u32BlkSize2 = 0;
#endif

static HI_CHAR *g_pUserPageAddr = HI_NULL;
static HI_U32 g_u32Size = 0;

static FILE *g_pfd = HI_NULL;

static HI_S32 __do_init_yuv = 0;

HI_VOID set_yuv_property(HI_VOID)
{
    __do_init_yuv = 0;
}

static HI_VOID sample_yuv_get_buf_size(const VIDEO_FRAME_S *pVFrame, HI_U32 *pSize)
{
    PIXEL_FORMAT_E enPixelFormat = pVFrame->enPixelFormat;

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == enPixelFormat)
    {
        *pSize = pVFrame->u32Stride[0] * pVFrame->u32Height * 3 / 2;
    }
    else if (PIXEL_FORMAT_YVU_SEMIPLANAR_422 == enPixelFormat)
    {
        *pSize = pVFrame->u32Stride[0] * pVFrame->u32Height * 2;
    }
    else if (PIXEL_FORMAT_YUV_400 == enPixelFormat)
    {
        *pSize = pVFrame->u32Stride[0] * pVFrame->u32Height;
    }

    return;
}

#ifdef CONFIG_HI_TDE_SUPPORT
static HI_S32 sample_yuv_prepare_vb(const VIDEO_FRAME_S *pVFrameInfo, VIDEO_FRAME_S *pVFrame)
{
    HI_U32 u32Width = pVFrameInfo->u32Width;
    HI_U32 u32Height = pVFrameInfo->u32Height;
    HI_U32 u32Align = 32;
    PIXEL_FORMAT_E enPixelFormat = pVFrameInfo->enPixelFormat;
    DATA_BITWIDTH_E enBitWidth = DYNAMIC_RANGE_SDR8;
    COMPRESS_MODE_E enCmpMode = COMPRESS_MODE_NONE;
    VB_CAL_CONFIG_S stCalConfig = { 0 };
    VB_POOL_CONFIG_S stVbPoolCfg = { 0 };

    COMMON_GetPicBufferConfig(u32Width, u32Height, enPixelFormat, enBitWidth, enCmpMode, u32Align, &stCalConfig);

    g_u32BlkSize2 = stCalConfig.u32VBSize;

    stVbPoolCfg.u64BlkSize  = g_u32BlkSize2;
    stVbPoolCfg.u32BlkCnt   = 1;
    stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;

    /*create comm vb pool*/
    g_hPool2 = HI_MPI_VB_CreatePool(&stVbPoolCfg);
    if (VB_INVALID_POOLID == g_hPool2)
    {
        printf("HI_MPI_VB_CreatePool failed!\n");
        return HI_FAILURE;
    }

    g_stMem2.hPool = g_hPool2;

    while (VB_INVALID_HANDLE == (g_stMem2.hBlock = HI_MPI_VB_GetBlock(g_stMem2.hPool, g_u32BlkSize2, HI_NULL)))
    {
        ;
    }

    g_stMem2.u64PhyAddr = HI_MPI_VB_Handle2PhysAddr(g_stMem2.hBlock);

    g_stMem2.pVirAddr = (HI_U8*)HI_MPI_SYS_Mmap(g_stMem2.u64PhyAddr, g_u32BlkSize2);
    if (HI_NULL == g_stMem2.pVirAddr)
    {
        printf("HI_MPI_SYS_Mmap failed!\n");
        HI_MPI_VB_ReleaseBlock(g_stMem2.hBlock);
        g_stMem2.hPool = VB_INVALID_POOLID;
        HI_MPI_VB_DestroyPool(g_hPool2);
        g_hPool2 = VB_INVALID_POOLID;
        return HI_FAILURE;
    }

    pVFrame->u64PhyAddr[0] = g_stMem2.u64PhyAddr;
    pVFrame->u64PhyAddr[1] = pVFrame->u64PhyAddr[0] + stCalConfig.u32MainYSize;

    pVFrame->u64VirAddr[0] = (HI_U64)(HI_UL)g_stMem2.pVirAddr;
    pVFrame->u64VirAddr[1] = pVFrame->u64VirAddr[0] + stCalConfig.u32MainYSize;

    pVFrame->u32Width  = u32Width;
    pVFrame->u32Height = u32Height;
    pVFrame->u32Stride[0] = stCalConfig.u32MainStride * 2; //packed 422
    pVFrame->u32Stride[1] = stCalConfig.u32MainStride;

    pVFrame->enCompressMode = COMPRESS_MODE_NONE;
    pVFrame->enPixelFormat  = pVFrameInfo->enPixelFormat;
    pVFrame->enVideoFormat  = VIDEO_FORMAT_LINEAR;
    pVFrame->enDynamicRange = pVFrameInfo->enDynamicRange;

    pVFrame->u64PTS = pVFrameInfo->u64PTS;
    pVFrame->u32TimeRef = pVFrameInfo->u32TimeRef;

    return HI_SUCCESS;
}

static HI_VOID sample_yuv_release_vb(HI_VOID)
{
    if (HI_NULL != g_stMem2.pVirAddr)
    {
        HI_MPI_SYS_Munmap((HI_VOID*)g_stMem2.pVirAddr, g_u32BlkSize2);
        g_stMem2.pVirAddr = HI_NULL;
        g_stMem2.u64PhyAddr = 0;
    }

    if (VB_INVALID_POOLID != g_stMem2.hPool)
    {
        HI_MPI_VB_ReleaseBlock(g_stMem2.hBlock);
        g_stMem2.hPool = VB_INVALID_POOLID;
    }

    if (VB_INVALID_POOLID != g_hPool2)
    {
        HI_MPI_VB_DestroyPool(g_hPool2);
        g_hPool2 = VB_INVALID_POOLID;
    }

    return;
}

static HI_S32 sample_yuv_do_tde_job(const VIDEO_FRAME_S *pVFrameIn, VIDEO_FRAME_S *pVFrameOut)
{
    HI_S32 s32Ret = HI_FAILURE;
    TDE_HANDLE tdeHandle;
    TDE2_OPT_S stOpt = { 0 };
    TDE2_SURFACE_S stSurface = { 0 };
    TDE2_SURFACE_S stDestSurface = { 0 };
    TDE2_RECT_S stSrcRect = { 0 };
    TDE2_RECT_S stDestRect = { 0 };

    tdeHandle = HI_TDE2_BeginJob();
    if (HI_ERR_TDE_INVALID_HANDLE == tdeHandle)
    {
        printf("HI_TDE2_BeginJob failed!\n");
        return HI_FAILURE;
    }

    /*prepare the in/out image info for TDE.*/
    stSurface.PhyAddr = pVFrameIn->u64PhyAddr[0];
    stSurface.enColorFmt = TDE2_COLOR_FMT_JPG_YCbCr422MBHP;
    stSurface.u32Height = pVFrameIn->u32Height;
    stSurface.u32Width = pVFrameIn->u32Width;
    stSurface.u32Stride = pVFrameIn->u32Stride[0];
    stSurface.CbCrPhyAddr = pVFrameIn->u64PhyAddr[1];
    stSurface.u32CbCrStride = pVFrameIn->u32Stride[1];

    stSrcRect.s32Xpos = 0;
    stSrcRect.s32Ypos = 0;
    stSrcRect.u32Width = stSurface.u32Width;
    stSrcRect.u32Height = stSurface.u32Height;

    stDestSurface.PhyAddr = pVFrameOut->u64PhyAddr[0];
    stDestSurface.enColorFmt = TDE2_COLOR_FMT_PKGVYUY;
    stDestSurface.u32Height = pVFrameOut->u32Height;
    stDestSurface.u32Width = pVFrameOut->u32Width;
    stDestSurface.u32Stride = pVFrameOut->u32Stride[0];

    stDestRect.s32Xpos = 0;
    stDestRect.s32Ypos = 0;
    stDestRect.u32Width = stDestSurface.u32Width;
    stDestRect.u32Height = stDestSurface.u32Height;

    s32Ret = HI_TDE2_Bitblit(tdeHandle, HI_NULL, HI_NULL, &stSurface, &stSrcRect, &stDestSurface, &stDestRect, &stOpt);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_TDE2_Bitblit failed:0x%x\n", s32Ret);
        HI_TDE2_CancelJob(tdeHandle);
        return s32Ret;
    }

    s32Ret = HI_TDE2_EndJob(tdeHandle, HI_FALSE, HI_TRUE, 1000);
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_TDE2_EndJob failed:0x%x\n", s32Ret);
        HI_TDE2_CancelJob(tdeHandle);
        return s32Ret;
    }

    return HI_SUCCESS;
}

static HI_S32 sample_yuv_sp422_to_p422(const VIDEO_FRAME_S *pVFrame, frame_node_t *pFNode)
{
    HI_S32 s32Ret = HI_FAILURE;
    VIDEO_FRAME_INFO_S stFrmInfo = { 0 };
    HI_U8 *node_ptr = pFNode->mem;
    HI_U32 h = 0;
    HI_CHAR *y_ptr = HI_NULL;

    s32Ret = HI_TDE2_Open();
    if (s32Ret != HI_SUCCESS)
    {
        printf("HI_TDE2_Open failed:0x%x\n", s32Ret);
        return s32Ret;
    }

    if (sample_yuv_prepare_vb(pVFrame, &stFrmInfo.stVFrame) != HI_SUCCESS)
    {
        HI_TDE2_Close();
        return HI_FAILURE;
    }

    /*convert sp422 to packed422(YUYV) by TDE.*/
    if (sample_yuv_do_tde_job(pVFrame, &stFrmInfo.stVFrame) != HI_SUCCESS)
    {
        sample_yuv_release_vb();
        HI_TDE2_Close();
        return HI_FAILURE;
    }

    HI_TDE2_Close();

    pFNode->used = 0;
    for (h = 0; h < stFrmInfo.stVFrame.u32Height; ++h)
    {
        y_ptr = (HI_CHAR *)(HI_UL)stFrmInfo.stVFrame.u64VirAddr[0] + h * stFrmInfo.stVFrame.u32Stride[0];

        if (pFNode->used + stFrmInfo.stVFrame.u32Width <= pFNode->length)
        {
            memcpy(node_ptr + pFNode->used, y_ptr, stFrmInfo.stVFrame.u32Width * 2);
            pFNode->used += stFrmInfo.stVFrame.u32Width * 2;
        }
    }

    sample_yuv_release_vb();

    return HI_SUCCESS;
}
#else /*else if #ifdef CONFIG_HI_TDE_SUPPORT*/
static HI_S32 sample_yuv_sp422_to_p422(const VIDEO_FRAME_S *pVFrame, frame_node_t *pFNode)
{
    HI_U32 w = 0;
    HI_U32 h = 0;
    HI_CHAR *pVBufVirt_Y = HI_NULL;
    HI_CHAR *pVBufVirt_C = HI_NULL;
    HI_CHAR *y_ptr = HI_NULL;
    HI_CHAR *uv_ptr = HI_NULL;
    HI_U8 *node_ptr = pFNode->mem;
    pFNode->used = 0;

    pVBufVirt_Y = g_pUserPageAddr;
    pVBufVirt_C = pVBufVirt_Y + pVFrame->u32Stride[0] * pVFrame->u32Height;

    for (h = 0; h < pVFrame->u32Height; ++h)
    {
        y_ptr = pVBufVirt_Y + h * pVFrame->u32Stride[0];
        uv_ptr = pVBufVirt_C + h * pVFrame->u32Stride[1];

        for (w = 0; w < pVFrame->u32Width; w += 2)
        {
            if (pFNode->used + 4 <= pFNode->length)
            {

                node_ptr[pFNode->used] = *(y_ptr + w);
                node_ptr[pFNode->used + 2] = *(y_ptr + w + 1);
                node_ptr[pFNode->used + 1] = *(uv_ptr + w + 1);
                node_ptr[pFNode->used + 3] = *(uv_ptr + w);
                pFNode->used += 4;
            }
        }
    }

    return HI_SUCCESS;
}
#endif /*end of #ifdef CONFIG_HI_TDE_SUPPORT*/

static HI_VOID sample_yuv_sp420_to_p420(const VIDEO_FRAME_S *pVFrame, frame_node_t *pFNode)
{
    HI_U32 w = 0;
    HI_U32 h = 0;
    HI_CHAR *pVBufVirt_Y = HI_NULL;
    HI_CHAR *pVBufVirt_C = HI_NULL;
    HI_CHAR *y_ptr = HI_NULL;
    HI_CHAR *uv_ptr = HI_NULL;
    HI_U8 *node_ptr = pFNode->mem;
    pFNode->used = 0;
#if (1 == UVC_SAVE_FILE)
    HI_U8 TmpBuff[8192];
#endif

    pVBufVirt_Y = g_pUserPageAddr;
    pVBufVirt_C = pVBufVirt_Y + pVFrame->u32Stride[0] * pVFrame->u32Height;

    for (h = 0; h < pVFrame->u32Height; ++h)
    {
        y_ptr = pVBufVirt_Y + h * pVFrame->u32Stride[0];

        if (pFNode->used + pVFrame->u32Width <= pFNode->length)
        {
            memcpy(node_ptr + pFNode->used, y_ptr, pVFrame->u32Width);
            pFNode->used += pVFrame->u32Width;
        }
#if (1 == UVC_SAVE_FILE)
        fwrite(y_ptr, pVFrame->u32Width, 1, g_pfd);
        fflush(g_pfd);
#endif
    }

    for (h = 0; h < pVFrame->u32Height / 2; ++h)
    {
        uv_ptr = pVBufVirt_C + h * pVFrame->u32Stride[1];

        for (w = 0; w < pVFrame->u32Width; w += 2)
        {
            if (pFNode->used + 1 <= pFNode->length)
            {
                node_ptr[pFNode->used] = *(uv_ptr + w + 1);
                pFNode->used++;
            }
#if (1 == UVC_SAVE_FILE)
            TmpBuff[w / 2] = *(uv_ptr + w + 1);
#endif
        }
#if (1 == UVC_SAVE_FILE)
        fwrite(TmpBuff, pVFrame->u32Width / 2, 1, g_pfd);
        fflush(g_pfd);
#endif
    }

    for (h = 0; h < pVFrame->u32Height / 2; ++h)
    {
        uv_ptr = pVBufVirt_C + h * pVFrame->u32Stride[1];

        for (w = 0; w < pVFrame->u32Width; w += 2)
        {
            if (pFNode->used + 1 <= pFNode->length)
            {
                node_ptr[pFNode->used] = *(uv_ptr + w);
                pFNode->used++;
            }
#if (1 == UVC_SAVE_FILE)
            TmpBuff[w / 2] = *(uv_ptr + w);
#endif
        }
#if (1 == UVC_SAVE_FILE)
        fwrite(TmpBuff, pVFrame->u32Width / 2, 1, g_pfd);
        fflush(g_pfd);
#endif
    }

    return;
}

/*When saving a file, sp420 will be denoted by p420 and sp422 will be denoted by p422 in the name of the file.*/
static HI_VOID sample_yuv_dump(VIDEO_FRAME_S *pVBuf, FILE *g_pfd)
{
    PIXEL_FORMAT_E enPixelFormat = pVBuf->enPixelFormat;

    uvc_cache_t *uvc_cache = HI_NULL;
    frame_node_t *fnode = HI_NULL;

    sample_yuv_get_buf_size(pVBuf, &g_u32Size);
    g_pUserPageAddr = (HI_CHAR*)HI_MPI_SYS_Mmap(pVBuf->u64PhyAddr[0], g_u32Size);
    if (HI_NULL == g_pUserPageAddr)
    {
        goto ERR;
    }

    //get free cache node
    uvc_cache = uvc_cache_get();
    if (uvc_cache)
    {
        get_node_from_queue(uvc_cache->free_queue, &fnode);
    }

    if (!fnode)
    {
        goto ERR;
    }

    if (PIXEL_FORMAT_YVU_SEMIPLANAR_422 == enPixelFormat)
    {
        if (sample_yuv_sp422_to_p422(pVBuf, fnode) != HI_SUCCESS)
        {
            goto ERR;
        }
    }
    else if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == enPixelFormat)
    {
        sample_yuv_sp420_to_p420(pVBuf, fnode);
    }
    else
    {
    }

ERR:
    if (fnode)
    {
        put_node_to_queue(uvc_cache->ok_queue, fnode);
    }
    if (g_pUserPageAddr)
    {
        HI_MPI_SYS_Munmap(g_pUserPageAddr, g_u32Size);
        g_pUserPageAddr = HI_NULL;
    }
    return;
}

static HI_S32 VPSS_Restore(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{
    if (VB_INVALID_POOLID != g_stFrame.u32PoolId)
    {
        if (HI_MPI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &g_stFrame))
        {
            printf("HI_MPI_VPSS_ReleaseChnFrame failed!");
        }

        g_stFrame.u32PoolId = VB_INVALID_POOLID;
    }

    if (-1 != g_hHandle)
    {
        HI_MPI_VGS_CancelJob(g_hHandle);
        g_hHandle = -1;
    }

    if (HI_NULL != g_stMem.pVirAddr)
    {
        HI_MPI_SYS_Munmap((HI_VOID*)g_stMem.pVirAddr, g_u32BlkSize);
        g_stMem.pVirAddr = HI_NULL;
        g_stMem.u64PhyAddr = 0;
    }

    if (VB_INVALID_POOLID != g_stMem.hPool)
    {
        HI_MPI_VB_ReleaseBlock(g_stMem.hBlock);
        g_stMem.hPool = VB_INVALID_POOLID;
    }

    if (VB_INVALID_POOLID != g_hPool)
    {
        HI_MPI_VB_DestroyPool(g_hPool);
        g_hPool = VB_INVALID_POOLID;
    }

    if (HI_NULL != g_pUserPageAddr)
    {
        HI_MPI_SYS_Munmap(g_pUserPageAddr, g_u32Size);
        g_pUserPageAddr = HI_NULL;
    }

    if (HI_NULL != g_pfd)
    {
        fclose(g_pfd);
        g_pfd = HI_NULL;
    }

    return HI_SUCCESS;
}

HI_VOID* __loop_yuv_frame_thread(HI_VOID *p)
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 s32MilliSec = 200;
    HI_BOOL bSendToVgs = HI_FALSE;
    VIDEO_FRAME_INFO_S stFrmInfo;
    VGS_TASK_ATTR_S stTask;
    SAMPLE_GETYUV_PARA_S *pstPara = (SAMPLE_GETYUV_PARA_S*)p;
    HI_U32 u32Width = 0;
    HI_U32 u32Height = 0;
    HI_U32 u32Align = 32;
    PIXEL_FORMAT_E enPixelFormat;
    DATA_BITWIDTH_E enBitWidth;
    COMPRESS_MODE_E enCmpMode;
    VB_CAL_CONFIG_S stCalConfig;
    VB_POOL_CONFIG_S stVbPoolCfg;

    VPSS_GRP Grp = pstPara->s32VpssGrp; /* VpssGrp */
    VPSS_CHN Chn = pstPara->s32VpssChn; /* VpssChn */
    VPSS_CHN_ATTR_S stChnAttr;
#if (1 == UVC_SAVE_FILE)
    HI_CHAR szYuvName[128];
#endif

    if (HI_MPI_VPSS_GetChnAttr(Grp, Chn, &stChnAttr) != HI_SUCCESS)
    {
        printf("HI_MPI_VPSS_GetChnAttr failed!\n");
        return HI_NULL;
    }

    stChnAttr.u32Depth = 2;
    if (HI_MPI_VPSS_SetChnAttr(Grp, Chn, &stChnAttr) != HI_SUCCESS)
    {
        printf("HI_MPI_VPSS_SetChnAttr failed!\n");
        return HI_NULL;
    }

#if (1 == UVC_SAVE_FILE)
    snprintf(szYuvName, 128, "output/vpss_grp%d_chn%d_%dx%d_%s.yuv", Grp, Chn,
                stChnAttr.u32Width, stChnAttr.u32Height, "P420");
    g_pfd = fopen(szYuvName, "wb");
    if (HI_NULL == g_pfd)
    {
        printf("open file %s err!\n", szYuvName);
        return HI_NULL;
    }
#endif

    /* get frame first */
    while (pstPara->bThreadStart)
    {
        if ((s32Ret = HI_MPI_VPSS_GetChnFrame(Grp, Chn, &g_stFrame, s32MilliSec)) != HI_SUCCESS)
        {
            printf("Get frame from VPSS fail(0x%x)!\n", s32Ret);
            return HI_NULL;
        }

        bSendToVgs = (COMPRESS_MODE_NONE != g_stFrame.stVFrame.enCompressMode);

        if (bSendToVgs)
        {
            enCmpMode = COMPRESS_MODE_NONE;
            enPixelFormat = g_stFrame.stVFrame.enPixelFormat;
            enBitWidth = (DYNAMIC_RANGE_SDR8 == g_stFrame.stVFrame.enDynamicRange) ? DATA_BITWIDTH_8 : DATA_BITWIDTH_10;
            u32Width  = g_stFrame.stVFrame.u32Width;
            u32Height = g_stFrame.stVFrame.u32Height;

            COMMON_GetPicBufferConfig(u32Width, u32Height, enPixelFormat, enBitWidth, enCmpMode, u32Align, &stCalConfig);

            g_u32BlkSize = stCalConfig.u32VBSize;

            memset(&stVbPoolCfg, 0, sizeof(VB_POOL_CONFIG_S));
            stVbPoolCfg.u64BlkSize  = g_u32BlkSize;
            stVbPoolCfg.u32BlkCnt   = 1;
            stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;

            /*create comm vb pool*/
            g_hPool = HI_MPI_VB_CreatePool(&stVbPoolCfg);
            if (VB_INVALID_POOLID == g_hPool)
            {
                printf("HI_MPI_VB_CreatePool failed! \n");
                VPSS_Restore(Grp, Chn);
                return HI_NULL;
            }

            g_stMem.hPool = g_hPool;

            while ((g_stMem.hBlock = HI_MPI_VB_GetBlock(g_stMem.hPool, g_u32BlkSize, HI_NULL)) == VB_INVALID_HANDLE)
            {
                ;
            }

            g_stMem.u64PhyAddr = HI_MPI_VB_Handle2PhysAddr(g_stMem.hBlock);

            g_stMem.pVirAddr = (HI_U8*)HI_MPI_SYS_Mmap(g_stMem.u64PhyAddr, g_u32BlkSize);
            if (HI_NULL == g_stMem.pVirAddr)
            {
                printf("Mem dev may not open\n");
                VPSS_Restore(Grp, Chn);
                return HI_NULL;
            }

            memset(&stFrmInfo.stVFrame, 0, sizeof(VIDEO_FRAME_S));
            stFrmInfo.stVFrame.u64PhyAddr[0] = g_stMem.u64PhyAddr;
            stFrmInfo.stVFrame.u64PhyAddr[1] = stFrmInfo.stVFrame.u64PhyAddr[0] + stCalConfig.u32MainYSize;

            stFrmInfo.stVFrame.u64VirAddr[0] = (HI_U64)(HI_UL)g_stMem.pVirAddr;
            stFrmInfo.stVFrame.u64VirAddr[1] = stFrmInfo.stVFrame.u64VirAddr[0] + stCalConfig.u32MainYSize;

            stFrmInfo.stVFrame.u32Width  = u32Width;
            stFrmInfo.stVFrame.u32Height = u32Height;
            stFrmInfo.stVFrame.u32Stride[0] = stCalConfig.u32MainStride;
            stFrmInfo.stVFrame.u32Stride[1] = stCalConfig.u32MainStride;

            stFrmInfo.stVFrame.enCompressMode = COMPRESS_MODE_NONE;
            stFrmInfo.stVFrame.enPixelFormat  = g_stFrame.stVFrame.enPixelFormat;
            stFrmInfo.stVFrame.enVideoFormat  = VIDEO_FORMAT_LINEAR;
            stFrmInfo.stVFrame.enDynamicRange = g_stFrame.stVFrame.enDynamicRange;

            stFrmInfo.stVFrame.u64PTS = g_stFrame.stVFrame.u64PTS;
            stFrmInfo.stVFrame.u32TimeRef = g_stFrame.stVFrame.u32TimeRef;

            stFrmInfo.u32PoolId = g_hPool;
            stFrmInfo.enModId = HI_ID_VGS;

            s32Ret = HI_MPI_VGS_BeginJob(&g_hHandle);
            if (s32Ret != HI_SUCCESS)
            {
                printf("HI_MPI_VGS_BeginJob failed\n");
                g_hHandle = -1;
                VPSS_Restore(Grp, Chn);
                return HI_NULL;
            }

            memcpy(&stTask.stImgIn, &g_stFrame, sizeof(VIDEO_FRAME_INFO_S));
            memcpy(&stTask.stImgOut, &stFrmInfo, sizeof(VIDEO_FRAME_INFO_S));
            s32Ret = HI_MPI_VGS_AddScaleTask(g_hHandle, &stTask, VGS_SCLCOEF_NORMAL);
            if (s32Ret != HI_SUCCESS)
            {
                printf("HI_MPI_VGS_AddScaleTask failed\n");
                VPSS_Restore(Grp, Chn);
                return HI_NULL;
            }

            s32Ret = HI_MPI_VGS_EndJob(g_hHandle);
            if (s32Ret != HI_SUCCESS)
            {
                printf("HI_MPI_VGS_EndJob failed\n");
                VPSS_Restore(Grp, Chn);
                return HI_NULL;
            }

            g_hHandle = -1;

            /* save VGS frame to file */
            sample_yuv_dump(&stFrmInfo.stVFrame, g_pfd);

            HI_MPI_VB_ReleaseBlock(g_stMem.hBlock);

            g_stMem.hPool = VB_INVALID_POOLID;
            g_hHandle = -1;

            if (HI_NULL != g_stMem.pVirAddr)
            {
                HI_MPI_SYS_Munmap((HI_VOID*)g_stMem.pVirAddr, g_u32BlkSize);
                g_stMem.u64PhyAddr = HI_NULL;
            }

            if (g_hPool != VB_INVALID_POOLID)
            {
                HI_MPI_VB_DestroyPool(g_hPool);
                g_hPool = VB_INVALID_POOLID;
            }
        }
        else
        {
            sample_yuv_dump(&g_stFrame.stVFrame, g_pfd);
        }

        /* release frame after using */
        s32Ret = HI_MPI_VPSS_ReleaseChnFrame(Grp, Chn, &g_stFrame);
        if (s32Ret != HI_SUCCESS)
        {
            printf("HI_MPI_VPSS_ReleaseChnFrame failed!\n");
            VPSS_Restore(Grp, Chn);
            return HI_NULL;
        }

        g_stFrame.u32PoolId = VB_INVALID_POOLID;
    }

    VPSS_Restore(Grp, Chn);

    return HI_NULL;
}

HI_S32 __SAMPLE_StartGetYUV(HI_S32 s32VpssGrp, HI_S32 s32VpssChn)
{
    g_stYUVPara.bThreadStart = HI_TRUE;
    g_stYUVPara.s32VpssChn = s32VpssChn;
    g_stYUVPara.s32VpssGrp = s32VpssGrp;
    return pthread_create(&g_stYUVPid, HI_NULL, __loop_yuv_frame_thread, (HI_VOID *)&g_stYUVPara);
}

/******************************************************************************
* funciton : stop get venc stream process.
******************************************************************************/
HI_S32 __SAMPLE_StopGetYUV(HI_VOID)
{
    if (HI_TRUE == g_stYUVPara.bThreadStart)
    {
        g_stYUVPara.bThreadStart = HI_FALSE;
        pthread_join(g_stYUVPid, HI_NULL);
    }

    return HI_SUCCESS;
}
