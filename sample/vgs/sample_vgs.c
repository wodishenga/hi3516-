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
#include <sys/prctl.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "sample_comm.h"
#include "loadbmp.h"

#ifdef __HuaweiLite__
#define SAMPLE_VGS_READ_PATH "./sharefs/data/input/1920_1080_p420.yuv"
#define SAMPLE_VGS_SAVE_PATH "./sharefs/data/output"
#define SAMPLE_VGS_BITMAP_PATH "./sharefs/data/input/mm.bmp"
#else
#define SAMPLE_VGS_READ_PATH "./data/input/1920_1080_p420.yuv"
#define SAMPLE_VGS_SAVE_PATH "./data/output"
#define SAMPLE_VGS_BITMAP_PATH "./data/input/mm.bmp"
#endif

typedef struct stVGS_FUNCTION_PARAM
{
    HI_BOOL bScale;
    HI_BOOL bCover;
    HI_BOOL bOsd;
    HI_BOOL bDrawLine;
    HI_BOOL bRotate;

    VGS_SCLCOEF_MODE_E *penVgsSclCoefMode;
    VGS_ADD_COVER_S *pstVgsAddCover;
    VGS_ADD_OSD_S *pstVgsAddOsd;
    VGS_DRAW_LINE_S *pstVgsDrawLine;
    ROTATION_E *penRotationAngle;

    RGN_HANDLE *pRgnHandle;
    SAMPLE_VB_BASE_INFO_S *pstInImgVbInfo;
    SAMPLE_VB_BASE_INFO_S *pstOutImgVbInfo;
    HI_S32 s32SampleNum;
} SAMPLE_VGS_FUNC_PARAM;

typedef struct stVGS_VB_INFO
{
    VB_BLK VbHandle;
    HI_U8 *pu8VirAddr;
    HI_U32 u32VbSize;
    HI_BOOL bVbUsed;
} SAMPLE_VGS_VB_INFO;

HI_U8 *pTemp = HI_NULL;
SAMPLE_VGS_VB_INFO g_stInImgVbInfo;
SAMPLE_VGS_VB_INFO g_stOutImgVbInfo;

/******************************************************************************
* function : show usage
******************************************************************************/
HI_VOID SAMPLE_VGS_Usage(HI_CHAR *sPrgNm)
{
    printf("\n/*****************************************/\n");
    printf("Usage: %s <index>\n", sPrgNm);
    printf("index:\n");
    printf("\t0) FILE -> VGS(Scale) -> FILE.\n");
    printf("\t1) FILE -> VGS(Cover+OSD) -> FILE.\n");
    printf("\t2) FILE -> VGS(DrawLine) -> FILE.\n");
    printf("\t3) FILE -> VGS(Rotate) -> FILE.\n");
    printf("/*****************************************/\n");
    return;
}

HI_VOID SAMPLE_VGS_Release(HI_VOID)
{
    if (pTemp != HI_NULL)
    {
        free(pTemp);
        pTemp = HI_NULL;
    }

    if (HI_TRUE == g_stInImgVbInfo.bVbUsed)
    {
        HI_MPI_SYS_Munmap((HI_VOID*)g_stInImgVbInfo.pu8VirAddr, g_stInImgVbInfo.u32VbSize);
        HI_MPI_VB_ReleaseBlock(g_stInImgVbInfo.VbHandle);
        g_stInImgVbInfo.bVbUsed = HI_FALSE;
    }

    if (HI_TRUE == g_stOutImgVbInfo.bVbUsed)
    {
        HI_MPI_SYS_Munmap((HI_VOID*)g_stOutImgVbInfo.pu8VirAddr, g_stOutImgVbInfo.u32VbSize);
        HI_MPI_VB_ReleaseBlock(g_stOutImgVbInfo.VbHandle);
        g_stOutImgVbInfo.bVbUsed = HI_FALSE;
    }

    return;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
HI_VOID SAMPLE_VGS_HandleSig(HI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_VGS_Release();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(-1);
}

static inline HI_VOID SAMPLE_VGS_GetYUVBufferCfg(const SAMPLE_VB_BASE_INFO_S *pstVbBaseInfo, VB_CAL_CONFIG_S *pstVbCalConfig)
{
    COMMON_GetPicBufferConfig(pstVbBaseInfo->u32Width, pstVbBaseInfo->u32Height, pstVbBaseInfo->enPixelFormat,
                              DATA_BITWIDTH_8, pstVbBaseInfo->enCompressMode, pstVbBaseInfo->u32Align, pstVbCalConfig);

    return;
}

static HI_S32 SAMPLE_VGS_GetFrameVb(const SAMPLE_VB_BASE_INFO_S *pstVbInfo, const VB_CAL_CONFIG_S *pstVbCalConfig,
                                    VIDEO_FRAME_INFO_S *pstFrameInfo, SAMPLE_VGS_VB_INFO *pstVgsVbInfo)
{
    HI_U64 u64PhyAddr = 0;

    pstVgsVbInfo->VbHandle = HI_MPI_VB_GetBlock(VB_INVALID_POOLID, pstVbCalConfig->u32VBSize, HI_NULL);
    if (VB_INVALID_HANDLE == pstVgsVbInfo->VbHandle)
    {
        SAMPLE_PRT("HI_MPI_VB_GetBlock failed!\n");
        return HI_FAILURE;
    }
    pstVgsVbInfo->bVbUsed = HI_TRUE;

    u64PhyAddr = HI_MPI_VB_Handle2PhysAddr(pstVgsVbInfo->VbHandle);
    if (0 == u64PhyAddr)
    {
        SAMPLE_PRT("HI_MPI_VB_Handle2PhysAddr failed!.\n");
        HI_MPI_VB_ReleaseBlock(pstVgsVbInfo->VbHandle);
        pstVgsVbInfo->bVbUsed = HI_FALSE;
        return HI_FAILURE;
    }

    pstVgsVbInfo->pu8VirAddr = (HI_U8*)HI_MPI_SYS_Mmap(u64PhyAddr, pstVbCalConfig->u32VBSize);
    if (HI_NULL == pstVgsVbInfo->pu8VirAddr)
    {
        SAMPLE_PRT("HI_MPI_SYS_Mmap failed!.\n");
        HI_MPI_VB_ReleaseBlock(pstVgsVbInfo->VbHandle);
        pstVgsVbInfo->bVbUsed = HI_FALSE;
        return HI_FAILURE;
    }
    pstVgsVbInfo->u32VbSize = pstVbCalConfig->u32VBSize;

    pstFrameInfo->enModId = HI_ID_VGS;
    pstFrameInfo->u32PoolId = HI_MPI_VB_Handle2PoolId(pstVgsVbInfo->VbHandle);

    pstFrameInfo->stVFrame.u32Width       = pstVbInfo->u32Width;
    pstFrameInfo->stVFrame.u32Height      = pstVbInfo->u32Height;
    pstFrameInfo->stVFrame.enField        = VIDEO_FIELD_FRAME;
    pstFrameInfo->stVFrame.enPixelFormat  = pstVbInfo->enPixelFormat;
    pstFrameInfo->stVFrame.enVideoFormat  = VIDEO_FORMAT_LINEAR;
    pstFrameInfo->stVFrame.enCompressMode = pstVbInfo->enCompressMode;
    pstFrameInfo->stVFrame.enDynamicRange = DYNAMIC_RANGE_SDR8;
    pstFrameInfo->stVFrame.enColorGamut   = COLOR_GAMUT_BT601;

    pstFrameInfo->stVFrame.u32HeaderStride[0]  = pstVbCalConfig->u32HeadStride;
    pstFrameInfo->stVFrame.u32HeaderStride[1]  = pstVbCalConfig->u32HeadStride;
    pstFrameInfo->stVFrame.u32HeaderStride[2]  = pstVbCalConfig->u32HeadStride;
    pstFrameInfo->stVFrame.u64HeaderPhyAddr[0] = u64PhyAddr;
    pstFrameInfo->stVFrame.u64HeaderPhyAddr[1] = pstFrameInfo->stVFrame.u64HeaderPhyAddr[0] + pstVbCalConfig->u32HeadYSize;
    pstFrameInfo->stVFrame.u64HeaderPhyAddr[2] = pstFrameInfo->stVFrame.u64HeaderPhyAddr[1];
    pstFrameInfo->stVFrame.u64HeaderVirAddr[0] = (HI_U64)(HI_UL)pstVgsVbInfo->pu8VirAddr;
    pstFrameInfo->stVFrame.u64HeaderVirAddr[1] = pstFrameInfo->stVFrame.u64HeaderVirAddr[0] + pstVbCalConfig->u32HeadYSize;
    pstFrameInfo->stVFrame.u64HeaderVirAddr[2] = pstFrameInfo->stVFrame.u64HeaderVirAddr[1];

    pstFrameInfo->stVFrame.u32Stride[0]  = pstVbCalConfig->u32MainStride;
    pstFrameInfo->stVFrame.u32Stride[1]  = pstVbCalConfig->u32MainStride;
    pstFrameInfo->stVFrame.u32Stride[2]  = pstVbCalConfig->u32MainStride;
    pstFrameInfo->stVFrame.u64PhyAddr[0] = pstFrameInfo->stVFrame.u64HeaderPhyAddr[0] + pstVbCalConfig->u32HeadSize;
    pstFrameInfo->stVFrame.u64PhyAddr[1] = pstFrameInfo->stVFrame.u64PhyAddr[0] + pstVbCalConfig->u32MainYSize;
    pstFrameInfo->stVFrame.u64PhyAddr[2] = pstFrameInfo->stVFrame.u64PhyAddr[1];
    pstFrameInfo->stVFrame.u64VirAddr[0] = pstFrameInfo->stVFrame.u64HeaderVirAddr[0] + pstVbCalConfig->u32HeadSize;
    pstFrameInfo->stVFrame.u64VirAddr[1] = pstFrameInfo->stVFrame.u64VirAddr[0] + pstVbCalConfig->u32MainYSize;
    pstFrameInfo->stVFrame.u64VirAddr[2] = pstFrameInfo->stVFrame.u64VirAddr[1];

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_VGS_ReadPlanarToSP42X(FILE *pFile, VIDEO_FRAME_S *pstFrame)
{
    HI_U8 *pLuma = (HI_U8*)(HI_UL)pstFrame->u64VirAddr[0];
    HI_U8 *pChroma = (HI_U8*)(HI_UL)pstFrame->u64VirAddr[1];
    HI_U32 u32Width = pstFrame->u32Width;
    HI_U32 u32LumaHeight = pstFrame->u32Height;
    HI_U32 u32ChromaHeight = u32LumaHeight;
    HI_U32 u32LumaStride = pstFrame->u32Stride[0];
    HI_U32 u32ChromaStride = u32LumaStride >> 1;

    HI_U8 *pDst = HI_NULL;
    HI_U32 u32Row = 0;
    HI_U32 u32List = 0;

    /* Y--------------------------------------------------*/
    pDst = pLuma;
    for (u32Row = 0; u32Row < u32LumaHeight; ++u32Row)
    {
        fread(pDst, u32Width, 1, pFile);
        pDst += u32LumaStride;
    }

    if (PIXEL_FORMAT_YUV_400 == pstFrame->enPixelFormat)
    {
        return HI_SUCCESS;
    }
    else if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pstFrame->enPixelFormat)
    {
        u32ChromaHeight = u32ChromaHeight >> 1;
    }

    pTemp = (HI_U8*)malloc(u32ChromaStride);
    if (HI_NULL == pTemp)
    {
        SAMPLE_PRT("vgs malloc failed!.\n");
        return HI_FAILURE;
    }
    memset(pTemp, 0, u32ChromaStride);

    /* U--------------------------------------------------*/
    pDst = pChroma + 1;
    for (u32Row = 0; u32Row < u32ChromaHeight; ++u32Row)
    {
        fread(pTemp, u32Width/2, 1, pFile);
        for (u32List = 0; u32List < u32ChromaStride; ++u32List)
        {
            *pDst = *(pTemp + u32List);
            pDst += 2;
        }
        pDst = pChroma + 1;
        pDst += (u32Row + 1) * u32LumaStride;
    }

    /* V--------------------------------------------------*/
    pDst = pChroma;
    for (u32Row = 0; u32Row < u32ChromaHeight; ++u32Row)
    {
        fread(pTemp, u32Width/2, 1, pFile);
        for(u32List = 0; u32List < u32ChromaStride; ++u32List)
        {
            *pDst = *(pTemp + u32List);
            pDst += 2;
        }
        pDst = pChroma;
        pDst += (u32Row + 1) * u32LumaStride;
    }

    free(pTemp);
    pTemp = HI_NULL;

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_VGS_GetFrameFromFile(SAMPLE_VGS_FUNC_PARAM *pParam, VB_CAL_CONFIG_S *pstInImgVbCalConfig,
                                          VIDEO_FRAME_INFO_S *pstFrameInfo)
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_CHAR szInFileName[128] = SAMPLE_VGS_READ_PATH;
    FILE *pFileRead = HI_NULL;

    pFileRead = fopen(szInFileName, "rb");
    if (HI_NULL == pFileRead)
    {
        SAMPLE_PRT("can't open file %s\n", szInFileName);
        goto EXIT;
    }

    s32Ret = SAMPLE_VGS_GetFrameVb(pParam->pstInImgVbInfo, pstInImgVbCalConfig, pstFrameInfo, &g_stInImgVbInfo);
    if (s32Ret != HI_SUCCESS)
    {
        goto EXIT1;
    }

    s32Ret = SAMPLE_VGS_ReadPlanarToSP42X(pFileRead, &pstFrameInfo->stVFrame);
    if (s32Ret != HI_SUCCESS)
    {
        goto EXIT2;
    }
    else
    {
        goto EXIT1;
    }

EXIT2:
    HI_MPI_SYS_Munmap((HI_VOID*)(HI_UL)pstFrameInfo->stVFrame.u64HeaderVirAddr[0], pstInImgVbCalConfig->u32VBSize);
    HI_MPI_VB_ReleaseBlock(g_stInImgVbInfo.VbHandle);
    g_stInImgVbInfo.bVbUsed = HI_FALSE;
EXIT1:
    fclose(pFileRead);
EXIT:
    return s32Ret;
}

static HI_S32 SAMPLE_VGS_SaveSP42XToPlanar(FILE *pFile, VIDEO_FRAME_S *pstFrame)
{
    HI_U32 u32LumaWidth = pstFrame->u32Width;
    HI_U32 u32ChromaWidth = u32LumaWidth >> 1;
    HI_U32 u32LumaHeight = pstFrame->u32Height;
    HI_U32 u32ChromaHeight = u32LumaHeight;
    HI_U32 u32LumaStride = pstFrame->u32Stride[0];
    HI_U8 *pLuma = (HI_U8*)(HI_UL)pstFrame->u64VirAddr[0];
    HI_U8 *pChroma = (HI_U8*)(HI_UL)pstFrame->u64VirAddr[1];

    HI_U8 *pDst = HI_NULL;
    HI_U32 u32Row = 0;
    HI_U32 u32List = 0;

    /* Y--------------------------------------------------*/
    pDst = pLuma;
    for (u32Row = 0; u32Row < u32LumaHeight; ++u32Row)
    {
        fwrite(pDst, 1, u32LumaWidth, pFile);
        pDst += u32LumaStride;
    }

    if (PIXEL_FORMAT_YUV_400 == pstFrame->enPixelFormat)
    {
        return HI_SUCCESS;
    }
    else if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pstFrame->enPixelFormat)
    {
        u32ChromaHeight = u32ChromaHeight >> 1;
    }

    pTemp = (HI_U8*)malloc(u32ChromaWidth);
    if (HI_NULL == pTemp)
    {
        SAMPLE_PRT("vgs malloc failed!.\n");
        return HI_FAILURE;
    }
    memset(pTemp, 0, u32ChromaWidth);

    /* U--------------------------------------------------*/
    for (u32Row = 0; u32Row < u32ChromaHeight; ++u32Row)
    {
        pDst = pChroma + u32Row * u32LumaStride + 1;
        for (u32List = 0; u32List < u32ChromaWidth; ++u32List)
        {
            *(pTemp + u32List) = *pDst;
            pDst += 2;
        }
        fwrite(pTemp, 1, u32ChromaWidth, pFile);
    }

    /* V--------------------------------------------------*/
    for (u32Row = 0; u32Row < u32ChromaHeight; ++u32Row)
    {
        pDst = pChroma + u32Row * u32LumaStride;
        for (u32List = 0; u32List < u32ChromaWidth; ++u32List)
        {
            *(pTemp + u32List) = *pDst;
            pDst += 2;
        }
        fwrite(pTemp, 1, u32ChromaWidth, pFile);
    }

    free(pTemp);
    pTemp = HI_NULL;

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_VGS_LoadBmp(const HI_CHAR *szFileName, BITMAP_S *pstBitmap, HI_BOOL bFil, HI_U32 u32FilColor,
                                 const SIZE_S *pstSize, HI_U32 u32Stride, PIXEL_FORMAT_E enPixelFormat)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;
    HI_S32 i = 0;
    HI_S32 j = 0;
    HI_U16 *pu16Temp = HI_NULL;

    if (GetBmpInfo(szFileName, &bmpFileHeader, &bmpInfo) < 0)
    {
        SAMPLE_PRT("GetBmpInfo err!\n");
        return HI_FAILURE;
    }

    if (HI_NULL == pstBitmap->pData)
    {
        SAMPLE_PRT("Bitmap's data is null!\n");
        return HI_FAILURE;
    }

    if (PIXEL_FORMAT_ARGB_1555 == enPixelFormat)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;
    }
    else if (PIXEL_FORMAT_ARGB_4444 == enPixelFormat)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB4444;
    }
    else if (PIXEL_FORMAT_ARGB_8888 == enPixelFormat)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB8888;
    }
    else
    {
        SAMPLE_PRT("Pixel format is not support!\n");
        return HI_FAILURE;
    }

    if (CreateSurfaceByCanvas(szFileName, &Surface, (HI_U8*)pstBitmap->pData, pstSize->u32Width, pstSize->u32Height, u32Stride))
    {
        return HI_FAILURE;
    }

    pstBitmap->u32Width = Surface.u16Width;
    pstBitmap->u32Height = Surface.u16Height;
    pstBitmap->enPixelFormat = enPixelFormat;

    pu16Temp = (HI_U16*)pstBitmap->pData;

    if (bFil)
    {
        for (i = 0; i < pstBitmap->u32Height; ++i)
        {
            for (j = 0; j < pstBitmap->u32Width; ++j)
            {
                if (u32FilColor == *pu16Temp)
                {
                    *pu16Temp &= 0x7FFF;
                }
                pu16Temp++;
            }
        }
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_VGS_PrepareOsdInfo(VGS_ADD_OSD_S *pstVgsAddOsd, RGN_HANDLE *pRgnHandle)
{
    HI_S32 s32Ret = HI_FAILURE;

    RGN_ATTR_S stRgnAttr;
    RGN_CANVAS_INFO_S stRgnCanvasInfo;
    BITMAP_S stBitmap;
    SIZE_S stSize;

    stSize.u32Width = pstVgsAddOsd->stRect.u32Width;
    stSize.u32Height = pstVgsAddOsd->stRect.u32Height;

    stRgnAttr.enType = OVERLAYEX_RGN;
    stRgnAttr.unAttr.stOverlayEx.enPixelFmt = pstVgsAddOsd->enPixelFmt;
    stRgnAttr.unAttr.stOverlayEx.u32BgColor = pstVgsAddOsd->u32BgColor;
    stRgnAttr.unAttr.stOverlayEx.stSize.u32Width = stSize.u32Width;
    stRgnAttr.unAttr.stOverlayEx.stSize.u32Height = stSize.u32Height;
    stRgnAttr.unAttr.stOverlayEx.u32CanvasNum =2;
    s32Ret = HI_MPI_RGN_Create(*pRgnHandle, &stRgnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_Create failed, s32Ret:0x%x", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_RGN_GetCanvasInfo(*pRgnHandle, &stRgnCanvasInfo);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_GetCanvasInfo failed, s32Ret:0x%x", s32Ret);
        return s32Ret;
    }

    stBitmap.pData = (HI_VOID*)(HI_UL)stRgnCanvasInfo.u64VirtAddr;
    s32Ret = SAMPLE_VGS_LoadBmp(SAMPLE_VGS_BITMAP_PATH, &stBitmap, HI_FALSE, 0, &stSize, stRgnCanvasInfo.u32Stride, pstVgsAddOsd->enPixelFmt);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_VGS_LoadBmp failed, s32Ret:0x%x", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_RGN_UpdateCanvas(*pRgnHandle);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_UpdateCanvas failed, s32Ret:0x%x", s32Ret);
        return s32Ret;
    }

    pstVgsAddOsd->u64PhyAddr = stRgnCanvasInfo.u64PhyAddr;
    pstVgsAddOsd->u32Stride = stRgnCanvasInfo.u32Stride;

    return s32Ret;
}

static HI_S32 SAMPLE_VGS_COMMON_FUNCTION(SAMPLE_VGS_FUNC_PARAM *pParam)
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_BOOL bSameVB = HI_FALSE;

    VGS_HANDLE hHandle = -1;
    VGS_TASK_ATTR_S stVgsTaskAttr;

    FILE *pFileWrite = HI_NULL;
    HI_CHAR szOutFileName[128];

    VB_CONFIG_S stVbConf;
    VB_CAL_CONFIG_S stInImgVbCalConfig;
    VB_CAL_CONFIG_S stOutImgVbCalConfig;

    if (HI_NULL == pParam)
    {
        return s32Ret;
    }

    if (pParam->pstInImgVbInfo && !pParam->pstOutImgVbInfo)
    {
        bSameVB = HI_TRUE;
    }

    /************************************************
    step1:  Init SYS and common VB
    *************************************************/
    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt = bSameVB ? 1 : 2;

    SAMPLE_VGS_GetYUVBufferCfg(pParam->pstInImgVbInfo, &stInImgVbCalConfig);
    stVbConf.astCommPool[0].u64BlkSize = stInImgVbCalConfig.u32VBSize;
    stVbConf.astCommPool[0].u32BlkCnt = 2;

    if (!bSameVB)
    {
        SAMPLE_VGS_GetYUVBufferCfg(pParam->pstOutImgVbInfo, &stOutImgVbCalConfig);
        stVbConf.astCommPool[1].u64BlkSize = stOutImgVbCalConfig.u32VBSize;
        stVbConf.astCommPool[1].u32BlkCnt = 2;
    }

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_Init failed, s32Ret:0x%x\n", s32Ret);
        goto EXIT;
    }

    /************************************************
    step2:  Get frame
    *************************************************/
    s32Ret = SAMPLE_VGS_GetFrameFromFile(pParam, &stInImgVbCalConfig, &stVgsTaskAttr.stImgIn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_VGS_GetFrameFromFile failed, s32Ret:0x%x\n", s32Ret);
        goto EXIT;
    }

    if (bSameVB)
    {
        snprintf(szOutFileName, 128, "%s/vgs_sample%d_%dx%d_p420.yuv", SAMPLE_VGS_SAVE_PATH, pParam->s32SampleNum,
                pParam->pstInImgVbInfo->u32Width, pParam->pstInImgVbInfo->u32Height);
    }
    else
    {
        snprintf(szOutFileName, 128, "%s/vgs_sample%d_%dx%d_p420.yuv", SAMPLE_VGS_SAVE_PATH, pParam->s32SampleNum,
                pParam->pstOutImgVbInfo->u32Width, pParam->pstOutImgVbInfo->u32Height);
    }

    pFileWrite = fopen(szOutFileName, "w+");
    if (HI_NULL == pFileWrite)
    {
        SAMPLE_PRT("can't open file %s\n", szOutFileName);
        goto EXIT1;
    }

    if (bSameVB)
    {
        memcpy(&stVgsTaskAttr.stImgOut, &stVgsTaskAttr.stImgIn, sizeof(VIDEO_FRAME_INFO_S));
    }
    else
    {
        s32Ret = SAMPLE_VGS_GetFrameVb(pParam->pstOutImgVbInfo, &stOutImgVbCalConfig, &stVgsTaskAttr.stImgOut, &g_stOutImgVbInfo);
        if (s32Ret != HI_SUCCESS)
        {
            goto EXIT2;
        }
    }

    /************************************************
    step3:  Create VGS job
    *************************************************/
    s32Ret = HI_MPI_VGS_BeginJob(&hHandle);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VGS_BeginJob failed, s32Ret:0x%x", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step4:  Add VGS task
    *************************************************/
    if (pParam->bScale)
    {
        s32Ret = HI_MPI_VGS_AddScaleTask(hHandle, &stVgsTaskAttr, *pParam->penVgsSclCoefMode);
        if (s32Ret != HI_SUCCESS)
        {
            HI_MPI_VGS_CancelJob(hHandle);
            SAMPLE_PRT("HI_MPI_VGS_AddScaleTask failed, s32Ret:0x%x", s32Ret);
            goto EXIT3;
        }
    }

    if (pParam->bCover)
    {
        s32Ret = HI_MPI_VGS_AddCoverTask(hHandle, &stVgsTaskAttr, pParam->pstVgsAddCover);
        if (s32Ret != HI_SUCCESS)
        {
            HI_MPI_VGS_CancelJob(hHandle);
            SAMPLE_PRT("HI_MPI_VGS_AddCoverTask failed, s32Ret:0x%x", s32Ret);
            goto EXIT3;
        }
    }

    if (pParam->bOsd)
    {
        s32Ret = SAMPLE_VGS_PrepareOsdInfo(pParam->pstVgsAddOsd, pParam->pRgnHandle);
        if (s32Ret != HI_SUCCESS)
        {
            HI_MPI_VGS_CancelJob(hHandle);
            SAMPLE_PRT("SAMPLE_VGS_PrepareOsdInfo failed, s32Ret:0x%x", s32Ret);
            goto EXIT3;
        }

        s32Ret = HI_MPI_VGS_AddOsdTask(hHandle, &stVgsTaskAttr, pParam->pstVgsAddOsd);
        if (s32Ret != HI_SUCCESS)
        {
            HI_MPI_VGS_CancelJob(hHandle);
            SAMPLE_PRT("HI_MPI_VGS_AddOsdTask failed, s32Ret:0x%x", s32Ret);
            goto EXIT3;
        }
    }

    if (pParam->bDrawLine)
    {
        s32Ret = HI_MPI_VGS_AddDrawLineTask(hHandle, &stVgsTaskAttr, pParam->pstVgsDrawLine);
        if (s32Ret != HI_SUCCESS)
        {
            HI_MPI_VGS_CancelJob(hHandle);
            SAMPLE_PRT("HI_MPI_VGS_AddDrawLineTask failed, s32Ret:0x%x", s32Ret);
            goto EXIT3;
        }
    }

    if (pParam->bRotate)
    {
        s32Ret = HI_MPI_VGS_AddRotationTask(hHandle, &stVgsTaskAttr, *pParam->penRotationAngle);
        if (s32Ret != HI_SUCCESS)
        {
            HI_MPI_VGS_CancelJob(hHandle);
            SAMPLE_PRT("HI_MPI_VGS_AddRotationTask failed, s32Ret:0x%x", s32Ret);
            goto EXIT3;
        }
    }

    /************************************************
    step5:  Start VGS work
    *************************************************/
    s32Ret = HI_MPI_VGS_EndJob(hHandle);
    if (s32Ret != HI_SUCCESS)
    {
        HI_MPI_VGS_CancelJob(hHandle);
        SAMPLE_PRT("HI_MPI_VGS_EndJob failed, s32Ret:0x%x", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step6:  Save the frame to file
    *************************************************/
    s32Ret = SAMPLE_VGS_SaveSP42XToPlanar(pFileWrite, &stVgsTaskAttr.stImgOut.stVFrame);
    if (s32Ret != HI_SUCCESS)
    {
        goto EXIT3;
    }

    fflush(pFileWrite);

    /************************************************
    step7:  Exit
    *************************************************/
EXIT3:
    if (!bSameVB)
    {
        HI_MPI_SYS_Munmap((HI_VOID*)(HI_UL)stVgsTaskAttr.stImgOut.stVFrame.u64HeaderVirAddr[0], stOutImgVbCalConfig.u32VBSize);
        HI_MPI_VB_ReleaseBlock(g_stOutImgVbInfo.VbHandle);
        g_stOutImgVbInfo.bVbUsed = HI_FALSE;
    }
EXIT2:
    fclose(pFileWrite);
EXIT1:
    HI_MPI_SYS_Munmap((HI_VOID*)(HI_UL)stVgsTaskAttr.stImgIn.stVFrame.u64HeaderVirAddr[0], stInImgVbCalConfig.u32VBSize);
    HI_MPI_VB_ReleaseBlock(g_stInImgVbInfo.VbHandle);
    g_stInImgVbInfo.bVbUsed = HI_FALSE;
EXIT:
    if (pParam->pRgnHandle)
    {
        HI_MPI_RGN_Destroy(*pParam->pRgnHandle);
    }
    SAMPLE_COMM_SYS_Exit();
    return s32Ret;
}

static HI_S32 SAMPLE_VGS_Scale(HI_VOID)
{
    HI_S32 s32Ret = HI_FAILURE;
    SAMPLE_VGS_FUNC_PARAM stVgsFuncParam;
    SAMPLE_VB_BASE_INFO_S stInImgVbInfo;
    SAMPLE_VB_BASE_INFO_S stOutImgVbInfo;
    VGS_SCLCOEF_MODE_E enVgsSclCoefMode = VGS_SCLCOEF_NORMAL;

    stInImgVbInfo.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stInImgVbInfo.u32Width = 1920;
    stInImgVbInfo.u32Height = 1080;
    stInImgVbInfo.u32Align = 0;
    stInImgVbInfo.enCompressMode = COMPRESS_MODE_NONE;

    memcpy(&stOutImgVbInfo, &stInImgVbInfo, sizeof(SAMPLE_VB_BASE_INFO_S));
    stOutImgVbInfo.u32Width = 1280;
    stOutImgVbInfo.u32Height = 720;

    memset(&stVgsFuncParam, 0, sizeof(SAMPLE_VGS_FUNC_PARAM));
    stVgsFuncParam.bScale = HI_TRUE;
    stVgsFuncParam.penVgsSclCoefMode = &enVgsSclCoefMode;
    stVgsFuncParam.pstInImgVbInfo = &stInImgVbInfo;
    stVgsFuncParam.pstOutImgVbInfo = &stOutImgVbInfo;
    stVgsFuncParam.s32SampleNum = 0;

    s32Ret = SAMPLE_VGS_COMMON_FUNCTION(&stVgsFuncParam);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("VGS Sample %d failed, s32Ret:0x%x", stVgsFuncParam.s32SampleNum, s32Ret);
    }

    return s32Ret;
}

static HI_S32 SAMPLE_VGS_Cover_Osd(HI_VOID)
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 i = 0;
    RGN_HANDLE rgnHandle = 0;
    SAMPLE_VGS_FUNC_PARAM stVgsFuncParam;
    SAMPLE_VB_BASE_INFO_S stInImgVbInfo;

    VGS_ADD_COVER_S stVgsAddCover;
    VGS_ADD_OSD_S stVgsAddOsd;
    POINT_S stPoint[4] = { {50, 50}, {50, 200}, {500, 50}, {500, 200} };

    stInImgVbInfo.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stInImgVbInfo.u32Width = 1920;
    stInImgVbInfo.u32Height = 1080;
    stInImgVbInfo.u32Align = 0;
    stInImgVbInfo.enCompressMode = COMPRESS_MODE_NONE;

    stVgsAddCover.enCoverType = COVER_QUAD_RANGLE;
    stVgsAddCover.u32Color = 0xFFFFFF;
    stVgsAddCover.stQuadRangle.bSolid = HI_TRUE;
    stVgsAddCover.stQuadRangle.u32Thick = 4;
    for (; i < 4; ++i)
    {
        stVgsAddCover.stQuadRangle.stPoint[i] = stPoint[i];
    }

    stVgsAddOsd.stRect.s32X = 600;
    stVgsAddOsd.stRect.s32Y = 100;
    stVgsAddOsd.stRect.u32Width = 180;
    stVgsAddOsd.stRect.u32Height = 144;
    stVgsAddOsd.u32BgColor = 0xFF0000;
    stVgsAddOsd.enPixelFmt = PIXEL_FORMAT_ARGB_8888;
    stVgsAddOsd.u64PhyAddr = 0;
    stVgsAddOsd.u32Stride = 0;
    stVgsAddOsd.u32BgAlpha = 255;
    stVgsAddOsd.u32FgAlpha = 128;
    stVgsAddOsd.bOsdRevert = HI_FALSE;
    stVgsAddOsd.stOsdRevert.stSrcRect = stVgsAddOsd.stRect;
    stVgsAddOsd.stOsdRevert.enColorRevertMode = VGS_COLOR_REVERT_RGB;

    memset(&stVgsFuncParam, 0, sizeof(SAMPLE_VGS_FUNC_PARAM));
    stVgsFuncParam.bCover = HI_TRUE;
    stVgsFuncParam.pstVgsAddCover = &stVgsAddCover;
    stVgsFuncParam.bOsd = HI_TRUE;
    stVgsFuncParam.pstVgsAddOsd = &stVgsAddOsd;
    stVgsFuncParam.pRgnHandle = &rgnHandle;
    stVgsFuncParam.pstInImgVbInfo = &stInImgVbInfo;
    stVgsFuncParam.pstOutImgVbInfo = HI_NULL;
    stVgsFuncParam.s32SampleNum = 1;

    s32Ret = SAMPLE_VGS_COMMON_FUNCTION(&stVgsFuncParam);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("VGS Sample %d failed, s32Ret:0x%x", stVgsFuncParam.s32SampleNum, s32Ret);
    }

    return s32Ret;
}

static HI_S32 SAMPLE_VGS_DrawLine(HI_VOID)
{
    HI_S32 s32Ret = HI_FAILURE;
    SAMPLE_VGS_FUNC_PARAM stVgsFuncParam;
    SAMPLE_VB_BASE_INFO_S stInImgVbInfo;
    VGS_DRAW_LINE_S stVgsDrawLine;

    stInImgVbInfo.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stInImgVbInfo.u32Width = 1920;
    stInImgVbInfo.u32Height = 1080;
    stInImgVbInfo.u32Align = 0;
    stInImgVbInfo.enCompressMode = COMPRESS_MODE_NONE;

    stVgsDrawLine.stStartPoint.s32X = 50;
    stVgsDrawLine.stStartPoint.s32Y = 50;
    stVgsDrawLine.stEndPoint.s32X = 1600;
    stVgsDrawLine.stEndPoint.s32Y = 900;
    stVgsDrawLine.u32Thick = 2;
    stVgsDrawLine.u32Color = 0x0000FF;

    memset(&stVgsFuncParam, 0, sizeof(SAMPLE_VGS_FUNC_PARAM));
    stVgsFuncParam.bDrawLine = HI_TRUE;
    stVgsFuncParam.pstVgsDrawLine = &stVgsDrawLine;
    stVgsFuncParam.pstInImgVbInfo = &stInImgVbInfo;
    stVgsFuncParam.pstOutImgVbInfo = HI_NULL;
    stVgsFuncParam.s32SampleNum = 2;

    s32Ret = SAMPLE_VGS_COMMON_FUNCTION(&stVgsFuncParam);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("VGS Sample %d failed, s32Ret:0x%x", stVgsFuncParam.s32SampleNum, s32Ret);
    }

    return s32Ret;
}

static HI_S32 SAMPLE_VGS_Rotate(HI_VOID)
{
    HI_S32 s32Ret = HI_FAILURE;
    SAMPLE_VGS_FUNC_PARAM stVgsFuncParam;
    ROTATION_E enRotationAngle = ROTATION_90;
    SAMPLE_VB_BASE_INFO_S stInImgVbInfo;
    SAMPLE_VB_BASE_INFO_S stOutImgVbInfo;

    stInImgVbInfo.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stInImgVbInfo.u32Width = 1920;
    stInImgVbInfo.u32Height = 1080;
    stInImgVbInfo.u32Align = 0;
    stInImgVbInfo.enCompressMode = COMPRESS_MODE_NONE;

    memcpy(&stOutImgVbInfo, &stInImgVbInfo, sizeof(SAMPLE_VB_BASE_INFO_S));
    stOutImgVbInfo.u32Width = 1080;
    stOutImgVbInfo.u32Height = 1920;

    memset(&stVgsFuncParam, 0, sizeof(SAMPLE_VGS_FUNC_PARAM));
    stVgsFuncParam.bRotate = HI_TRUE;
    stVgsFuncParam.penRotationAngle = &enRotationAngle;
    stVgsFuncParam.pstInImgVbInfo = &stInImgVbInfo;
    stVgsFuncParam.pstOutImgVbInfo = &stOutImgVbInfo;
    stVgsFuncParam.s32SampleNum = 3;

    s32Ret = SAMPLE_VGS_COMMON_FUNCTION(&stVgsFuncParam);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("VGS Sample %d failed, s32Ret:0x%x", stVgsFuncParam.s32SampleNum, s32Ret);
    }

    return s32Ret;
}

/******************************************************************************
* function    : main()
* Description : video vgs sample
******************************************************************************/
#ifdef __HuaweiLite__
HI_S32 app_main(HI_S32 argc, HI_CHAR *argv[])
#else
HI_S32 main(HI_S32 argc, HI_CHAR *argv[])
#endif
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 s32Index = -1;

    if (argc < 2 || argc > 2)
    {
        SAMPLE_VGS_Usage(argv[0]);
        return s32Ret;
    }

    if (!strncmp(argv[1], "-h", 2))
    {
        SAMPLE_VGS_Usage(argv[0]);
        return HI_SUCCESS;
    }

#ifndef __HuaweiLite__
    signal(SIGINT, SAMPLE_VGS_HandleSig);
    signal(SIGTERM, SAMPLE_VGS_HandleSig);
#endif

    s32Index = atoi(argv[1]);
    if (!s32Index && strncmp(argv[1], "0", 1))
    {
        s32Index = -1;
    }

    switch (s32Index)
    {
        case 0:
            s32Ret = SAMPLE_VGS_Scale();
            break;
        case 1:
            s32Ret = SAMPLE_VGS_Cover_Osd();
            break;
        case 2:
            s32Ret = SAMPLE_VGS_DrawLine();
            break;
        case 3:
            s32Ret = SAMPLE_VGS_Rotate();
            break;
        default:
            SAMPLE_PRT("the index is invaild!\n");
            SAMPLE_VGS_Usage(argv[0]);
            s32Ret = HI_FAILURE;
            break;
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
