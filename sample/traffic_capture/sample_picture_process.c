#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs/imgcodecs_c.h>


#include <opencv2/highgui.hpp>
#include <opencv2/core/utility.hpp>



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
#include <sys/prctl.h>

#include "sample_comm.h"
#include "sample_picture_process.h"


#define WIDTH 1920
#define HEIGHT 1076


using namespace cv;
using namespace std;

static HI_U32 u32Size = 0;
static HI_CHAR* pUserPageAddr[2] = {HI_NULL, HI_NULL};


//1:是第一帧图片，0:不是第一帧图片
HI_S32 firstFrame = 1;
//是不是检测到火焰
HI_S32 ifFire = 0;

Mat preMask;

Mat m; 


/*图片降噪以及提高黑度值*/
HI_S32
SAMPLE_Noise_Reduction(VI_PIPE SnapPipe)
{
	HI_S32                  s32Ret              = HI_SUCCESS;
	ISP_NR_ATTR_S stNRAttr;
    ISP_SHARPEN_ATTR_S stSharpenAttr;
	/*1.图片降噪*/
	s32Ret = HI_MPI_ISP_GetNRAttr(SnapPipe, &stNRAttr);
	if (HI_SUCCESS != s32Ret) {
		SAMPLE_PRT("HI_MPI_ISP_GetNRAttr failed with %#x\n", s32Ret);
		return s32Ret;
    }
	stNRAttr.bEnable = HI_TRUE;
	stNRAttr.enOpType = OP_TYPE_MANUAL ;
	stNRAttr.stManual.au8ChromaStr[0] = 3;
	stNRAttr.stManual.au8ChromaStr[1] = 3;
	stNRAttr.stManual.au8ChromaStr[2] = 3;
	stNRAttr.stManual.au8ChromaStr[3] = 3;
	
	stNRAttr.stManual.u8FineStr = 0x80;
	stNRAttr.bNrLscEnable = HI_FALSE;

	stNRAttr.stManual.u16CoringWgt = 0x0;

	stNRAttr.stManual.au16CoarseStr[0] = 0x360;
	stNRAttr.stManual.au16CoarseStr[1] = 0x360;
	stNRAttr.stManual.au16CoarseStr[2] = 0x360;
	stNRAttr.stManual.au16CoarseStr[3] = 0x360;

	for (int i = 0; i < 4; i++) {
		stNRAttr.stWdr.au8FusionFrameStr[i] = 0x50;  
		stNRAttr.stWdr.au8WDRFrameStr[i] = 0x50;
	}

	for (int i = 0; i < 33; i++) {
		stNRAttr.au16CoringRatio[i] = 0;
	}

	s32Ret = HI_MPI_ISP_SetNRAttr(SnapPipe, &stNRAttr);
	if (HI_SUCCESS != s32Ret) {
		SAMPLE_PRT("HI_MPI_ISP_SetNRAttr failed with %#x\n", s32Ret);
		return s32Ret;
	}
	
   /*2.图片锐化*/
	s32Ret = HI_MPI_ISP_GetIspSharpenAttr(SnapPipe, &stSharpenAttr);
	if (HI_SUCCESS != s32Ret) {
		SAMPLE_PRT("HI_MPI_ISP_GetIspSharpenAttr failed with %#x!\n", s32Ret);
		return s32Ret;
	}
	stSharpenAttr.bEnable = HI_TRUE;
	stSharpenAttr.enOpType = OP_TYPE_MANUAL;

	for(int i = 0; i < 32; i++) {
		stSharpenAttr.stManual.au16TextureStr[i] = 0;
	}

	for(int i = 0; i< 32 ; i++) {
		stSharpenAttr.stManual.au16EdgeStr[i] = 0;
	}
	
	for(int i = 0; i < 32; i++) {
		stSharpenAttr.stManual.au8LumaWgt[i] = 0;
	}
	stSharpenAttr.stManual.u16TextureFreq = 4090;
	stSharpenAttr.stManual.u16EdgeFreq = 96;
	stSharpenAttr.stManual.u8OverShoot = 127;
	stSharpenAttr.stManual.u8UnderShoot = 127;
	
    s32Ret = HI_MPI_ISP_SetIspSharpenAttr(SnapPipe, &stSharpenAttr);
	if (HI_SUCCESS != s32Ret) {
		SAMPLE_PRT("HI_MPI_ISP_SetIspSharpenAttr failed with %#x!\n", s32Ret);
		return s32Ret;
	}

	/*3.提高图片黑度值*/
	ISP_BLACK_LEVEL_S stBlackLevel;

    s32Ret = HI_MPI_ISP_GetBlackLevelAttr(SnapPipe, &stBlackLevel);
	if (HI_SUCCESS != s32Ret) {
		SAMPLE_PRT("HI_MPI_ISP_GetBlackLevelAttr failed with %#x!\n", s32Ret);
		return s32Ret;
	}

	stBlackLevel.enOpType = OP_TYPE_MANUAL;
	for(int i = 0; i < 4; i++) {
		stBlackLevel.au16BlackLevel[i] = 3950;
	}
	
	s32Ret = HI_MPI_ISP_SetBlackLevelAttr(SnapPipe, &stBlackLevel);
	if (HI_SUCCESS != s32Ret) {
		SAMPLE_PRT("HI_MPI_ISP_SetBlackLevelAttr failed with %#x!\n", s32Ret);
		return s32Ret;
	}
	    
	return  s32Ret;
}


/*yuv420格式转化为opencv支持的Mat格式*/
HI_VOID 
SAMPLE_Yuv420_to_Mat(Mat &dst, unsigned char* pYUV420, int width, int height)
{
	if (!pYUV420) {
		return;
	}
 
	IplImage *yuvimage, *rgbimg, *yimg, *uimg, *vimg, *uuimg, *vvimg;
 
	int nWidth = width;
	int nHeight = height;
	rgbimg = cvCreateImage(cvSize(nWidth, nHeight), IPL_DEPTH_8U, 3);
	yuvimage = cvCreateImage(cvSize(nWidth, nHeight), IPL_DEPTH_8U, 3);

	yimg = cvCreateImageHeader(cvSize(nWidth, nHeight), IPL_DEPTH_8U, 1);
	uimg = cvCreateImageHeader(cvSize(nWidth / 2, nHeight / 2), IPL_DEPTH_8U, 1);
	vimg = cvCreateImageHeader(cvSize(nWidth / 2, nHeight / 2), IPL_DEPTH_8U, 1);
 
	uuimg = cvCreateImage(cvSize(nWidth, nHeight), IPL_DEPTH_8U, 1);
	vvimg = cvCreateImage(cvSize(nWidth, nHeight), IPL_DEPTH_8U, 1);
 
	cvSetData(yimg, pYUV420, nWidth);
	cvSetData(uimg, pYUV420 + nWidth * nHeight, nWidth / 2);
	cvSetData(vimg, pYUV420 + long(nWidth*nHeight*1.25), nWidth / 2);
	cvResize(uimg, uuimg, CV_INTER_LINEAR);
	cvResize(vimg, vvimg, CV_INTER_LINEAR);

	cvMerge(yimg, uuimg, vvimg, NULL, yuvimage);
	cvCvtColor(yuvimage, rgbimg, CV_YCrCb2RGB);

	cvReleaseImage(&uuimg);
	cvReleaseImage(&vvimg);
	cvReleaseImageHeader(&yimg);
	cvReleaseImageHeader(&uimg);
	cvReleaseImageHeader(&vimg);

	cvReleaseImage(&yuvimage);
	dst = cvarrToMat(rgbimg, true);
	cvReleaseImage(&rgbimg);
}



/*火焰检测算法, 是火焰，返回1；不是火焰，返回0*/
HI_S32 
SAMPLE_Flame_Detection(Mat src)
{
 
	int row = src.rows;
	int col = src.cols;
	ifFire = 0;

	Mat curGrayFrame = Mat(src.size(), CV_8UC1);
	//将RGB转化成Gray
	cvtColor(src, curGrayFrame, COLOR_RGB2GRAY);

	//去掉白色时间信息
	Mat roiGrayImg = Mat(curGrayFrame, {0, (int)(row*0.15), col, (int)(row*0.7)});

	Mat mask = Mat(roiGrayImg.size(), CV_8UC1);

	threshold(roiGrayImg, mask, 50.0, 255.0, THRESH_BINARY);

	int counter = countNonZero(mask);

	if(!firstFrame){
		if(counter > 5) {
			Mat difMask = Mat(mask.size(), CV_8UC1);
			absdiff(preMask ,mask ,difMask);
			int count = countNonZero(difMask);
			if(count >5){
				ifFire = 1;
   			}
  		}
 	} else {
  		firstFrame = 0;
	}

	preMask = mask.clone();

	return ifFire;
 
}

HI_S32 
SAMPLE_Picture_Processing(VIDEO_FRAME_S* pVBuf)
{
	HI_S32 s32Ret = HI_SUCCESS;
	unsigned int w, h;
	char* pVBufVirt_Y;
	char* pVBufVirt_C;
	char* pMemContent;
	FILE* pfd = HI_NULL;
		
	unsigned char TmpBuff[20480];
	    
	HI_U32 phy_addr;
	PIXEL_FORMAT_E	enPixelFormat = pVBuf->enPixelFormat;
	VIDEO_FORMAT_E	enVideoFormat = pVBuf->enVideoFormat;
	HI_U32 u32UvHeight = 0;
	HI_CHAR szYuvName[20];

	/*创建一个临时文件保存yuv数据*/
	sprintf(szYuvName, "%s", "./picture.yuv");
	pfd = fopen(szYuvName, "wb+");
	if(pfd == NULL) {
		SAMPLE_PRT("fopen picture.yuv error\n");
		return -1 ;
	}
	
	/*1.获取yuv图像数据*/
	u32Size = (pVBuf->u32Stride[0]) * (pVBuf->u32Height) * 3 / 2;
	u32UvHeight = pVBuf->u32Height / 2;
	phy_addr = pVBuf->u64PhyAddr[0];
	pUserPageAddr[0] = (HI_CHAR*) HI_MPI_SYS_Mmap(phy_addr, u32Size);
	if (HI_NULL == pUserPageAddr[0]) {
		return -2;
	}
	
	pVBufVirt_Y = pUserPageAddr[0];
	pVBufVirt_C = pVBufVirt_Y + (pVBuf->u32Stride[0]) * (pVBuf->u32Height);
	
	fflush(stderr);
	if(VIDEO_FORMAT_TILE_16x8 == enVideoFormat) {
			for (h = 0; h < pVBuf->u32Height; h++) {
				pMemContent = pVBufVirt_Y + h * pVBuf->u32Stride[0];
				fwrite(pMemContent, pVBuf->u32Stride[0], 1, pfd);
			}
	} else {
			for (h = 0; h < pVBuf->u32Height; h++) {
				pMemContent = pVBufVirt_Y + h * pVBuf->u32Stride[0];
				fwrite(pMemContent, pVBuf->u32Width, 1, pfd);
			}
	}
	
	if(PIXEL_FORMAT_YUV_400 != enPixelFormat && VIDEO_FORMAT_TILE_16x8 != enVideoFormat) {
		fflush(pfd);
		fflush(stderr);
	
		for (h = 0; h < u32UvHeight; h++) {
				pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];
				pMemContent += 1;
		
				for (w = 0; w < pVBuf->u32Width / 2; w++) {
					TmpBuff[w] = *pMemContent;
					pMemContent += 2;
				}
				fwrite(TmpBuff, pVBuf->u32Width / 2, 1, pfd);
			}
			fflush(pfd);
			fflush(stderr);
			for (h = 0; h < u32UvHeight; h++) {
				pMemContent = pVBufVirt_C + h * pVBuf->u32Stride[1];
	
				for (w = 0; w < pVBuf->u32Width / 2; w++) {
					TmpBuff[w] = *pMemContent;
					pMemContent += 2;
				}
				fwrite(TmpBuff, pVBuf->u32Width / 2, 1, pfd);
			}
		}
	fflush(pfd);
	
	fprintf(stderr, "done %d!\n", pVBuf->u32TimeRef);
	fflush(stderr);
	
	HI_MPI_SYS_Munmap(pUserPageAddr[0], u32Size);
	pUserPageAddr[0] = HI_NULL;
		
	//文件内位置定位到文件头
	fseek(pfd, 0, SEEK_SET);
	unsigned char *pBuf = new unsigned char[WIDTH*HEIGHT*3/2];
	if(pBuf == NULL) {
		SAMPLE_PRT("malloc buf error!\n");
		return -1;
	}
	/*2.读取图片二进制数据进行处理*/
	int count = fread(pBuf, 1, (WIDTH * HEIGHT * 3) / 2, pfd);
	if(count == 0) {
		SAMPLE_PRT("fread  error!\n");	
		return -1;
	}
		
	/*3.把yuv格式的图像转化为opencv能够处理Mat格式数据*/
	SAMPLE_Yuv420_to_Mat(m, pBuf, WIDTH, HEIGHT);

	/*4.火焰检测*/
	s32Ret = SAMPLE_Flame_Detection(m);
		
	fclose(pfd);
	free(pBuf);
		
	return s32Ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


