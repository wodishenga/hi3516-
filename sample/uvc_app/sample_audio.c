#include "hi_type.h"
#include "hiaudio.h"
#include "sample_comm.h"
#include <pthread.h>

#include <alsa/asoundlib.h>

#define SAMPLE_DBG(s32Ret)\
    do{\
        printf("s32Ret=%#x,fuc:%s,line:%d\n", s32Ret, __FUNCTION__, __LINE__);\
    }while(0)

#define PCM_DEVICE_NAME "default"
#define AACLC_SAMPLES_PER_FRAME 1024
#define ALSA_SAVE_FILE 0

typedef struct tagSAMPLE_AI2ALSA_S
{
    HI_BOOL bStart;
    HI_S32  AiDev;
    HI_S32  AiChn;
    AIO_ATTR_S stAioAttr;
    FILE* pfd;
    pthread_t stAiPid;
} SAMPLE_AI2ALSA_S;

static snd_pcm_t * gs_handlePlayback = HI_NULL;
static snd_pcm_hw_params_t * gs_paramsPlay = HI_NULL;

static HI_S32 gs_s32AiDev = -1;
static HI_S32 gs_s32AiChn = -1;
static HI_S32 gs_s32AiChnCnt = -1;

static SAMPLE_AI2ALSA_S gs_stSampleAiAlsaSend[AI_DEV_MAX_NUM * AI_MAX_CHN_NUM];

static HI_BOOL gs_bAioReSample = HI_FALSE;
static AUDIO_SAMPLE_RATE_E gs_enOutSampleRate = AUDIO_SAMPLE_RATE_BUTT;

/******************************************************************************
* function : Open alsa File
******************************************************************************/
#if ALSA_SAVE_FILE
static FILE* _SAMPLE_AUDIO_OpenAlsaFile(AI_CHN AiChn)
{
    FILE* pfd;
    HI_CHAR aszFileName[FILE_NAME_LEN] = {0};

    /* create file for save stream*/
#ifdef __HuaweiLite__
    snprintf(aszFileName, FILE_NAME_LEN, "/sharefs/Alsa_chn%d.pcm", AiChn);
#else
    snprintf(aszFileName, FILE_NAME_LEN, "Alsa_chn%d.pcm", AiChn);
#endif
    pfd = fopen(aszFileName, "w+");
    if (NULL == pfd)
    {
        printf("%s: open file %s failed\n", __FUNCTION__, aszFileName);
        return NULL;
    }
    printf("open stream file:\"%s\" for alsa ok\n", aszFileName);
    return pfd;
}
#endif

/******************************************************************************
* function : send frame to alsa
******************************************************************************/
HI_S32 _SAMPLE_AUDIO_SendFrameToAlsa(AUDIO_FRAME_S* pstFrame)
{
    AUDIO_FRAME_S stFrame;
    int size;
    unsigned char *buffer;
    int err;
    int dir;
    unsigned int channels = 0;
    snd_pcm_uframes_t frames;
    HI_BOOL bStatusSendFinish = HI_FALSE;

    if(HI_NULL == pstFrame)
    {
        printf("%s: pstFrame is null\n", __FUNCTION__);
        return HI_FAILURE;
    }

    if(AUDIO_BIT_WIDTH_16 != pstFrame->enBitwidth)
    {
        printf("%s: enBitwidth is not 16 bits\n", __FUNCTION__);
        return HI_FAILURE;
    }

    memcpy(&stFrame, pstFrame, sizeof(AUDIO_FRAME_S));

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(gs_paramsPlay, &frames, &dir);
    snd_pcm_hw_params_get_channels(gs_paramsPlay, &channels);
    size = frames * 2 * channels;
    buffer = (unsigned char *) malloc(size);
    if(HI_NULL == buffer)
    {
        printf("%s: buffer malloc fail\n", __FUNCTION__);
        return HI_FAILURE;
    }

    /* copy data to buf */
    if(1 == channels)
    {
        memcpy(buffer , stFrame.u64VirAddr[0], size);
    }
    else
    {
        free(buffer);
        printf("%s: channels is invalid\n", __FUNCTION__);
        return HI_FAILURE;
    }

    /* send data to alsa driver */
    while(!bStatusSendFinish)
    {
        err = snd_pcm_writei(gs_handlePlayback, buffer, frames);
        if (err == -EPIPE)
        {
            /* EPIPE means underrun */
            printf("underrun occurred, err = %d\n", err);
            snd_pcm_prepare(gs_handlePlayback);
        }
        else if (err < 0)
        {
            printf("error from writei: %s\n", snd_strerror(err));
            free(buffer);
            return HI_FAILURE;
        }
        else if (err != (int)frames)
        {
            printf("short write, write %d frames\n", err);
            free(buffer);
            return HI_FAILURE;
        }
        else
        {
            bStatusSendFinish = HI_TRUE;
        }
    }

    free(buffer);

    return HI_SUCCESS;
}

/******************************************************************************
* function : get frame from Ai, send it to alsa
******************************************************************************/
void* _SAMPLE_AUDIO_AlsaSendProc(void* parg)
{
    HI_S32 s32Ret;
    HI_S32 AiFd;
    SAMPLE_AI2ALSA_S* pstAiAlsaCtl = (SAMPLE_AI2ALSA_S*)parg;
    AUDIO_FRAME_S stFrame;
    AEC_FRAME_S   stAecFrm;
    fd_set read_fds;
    struct timeval TimeoutVal;
    AI_CHN_PARAM_S stAiChnPara;

    //alsa
    int err;
    unsigned int val;
    int dir;
    int open_mode = 0;
    snd_pcm_info_t * info;
    snd_pcm_hw_params_t * paramsTemp;

    /* Open PCM device for playback. */
    err = snd_pcm_open(&gs_handlePlayback, PCM_DEVICE_NAME, SND_PCM_STREAM_PLAYBACK, open_mode);
    if (err < 0)
    {
        printf("audio open error: %s\n", snd_strerror(err));
        return NULL;
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_info_alloca(&info);

    err = snd_pcm_info(gs_handlePlayback, info);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    /* Fill it in with default values. */
    snd_pcm_hw_params_alloca(&gs_paramsPlay);
    err = snd_pcm_hw_params_any(gs_handlePlayback, gs_paramsPlay);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    err = snd_pcm_hw_params_set_access(gs_handlePlayback, gs_paramsPlay, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    /* Signed 16-bit little-endian format */
    err = snd_pcm_hw_params_set_format(gs_handlePlayback, gs_paramsPlay, SND_PCM_FORMAT_S16_LE);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    /* channels */
    val = pstAiAlsaCtl->stAioAttr.enSoundmode + 1;  //mono:1, stereo:2
    err = snd_pcm_hw_params_set_channels(gs_handlePlayback, gs_paramsPlay, val);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    /* period (default: 1024) */
    val = pstAiAlsaCtl->stAioAttr.u32PtNumPerFrm;
    dir = SND_PCM_STREAM_PLAYBACK;
    err = snd_pcm_hw_params_set_period_size(gs_handlePlayback, gs_paramsPlay, val, dir);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    /* sampling rate */
    val = pstAiAlsaCtl->stAioAttr.enSamplerate;
    err = snd_pcm_hw_params_set_rate_near(gs_handlePlayback, gs_paramsPlay, &val, &dir);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    /* Write the parameters to the driver */
    err = snd_pcm_hw_params(gs_handlePlayback, gs_paramsPlay);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    /* Get the parameters from the driver */
    snd_pcm_hw_params_alloca(&paramsTemp);
    err = snd_pcm_hw_params_current(gs_handlePlayback, paramsTemp);
    if (err < 0)
    {
        printf("[Func]:%s [Line]:%d info error: %s", __FUNCTION__, __LINE__, snd_strerror(err));
        return NULL;
    }

    s32Ret = HI_MPI_AI_GetChnParam(pstAiAlsaCtl->AiDev, pstAiAlsaCtl->AiChn, &stAiChnPara);
    if (HI_SUCCESS != s32Ret)
    {
        printf("%s: Get ai chn param failed\n", __FUNCTION__);
        return NULL;
    }

    stAiChnPara.u32UsrFrmDepth = 30;

    s32Ret = HI_MPI_AI_SetChnParam(pstAiAlsaCtl->AiDev, pstAiAlsaCtl->AiChn, &stAiChnPara);
    if (HI_SUCCESS != s32Ret)
    {
        printf("%s: set ai chn param failed\n", __FUNCTION__);
        return NULL;
    }

    FD_ZERO(&read_fds);
    AiFd = HI_MPI_AI_GetFd(pstAiAlsaCtl->AiDev, pstAiAlsaCtl->AiChn);
    FD_SET(AiFd, &read_fds);

    while (pstAiAlsaCtl->bStart)
    {
        TimeoutVal.tv_sec = 1;
        TimeoutVal.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(AiFd, &read_fds);

        s32Ret = select(AiFd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            break;
        }
        else if (0 == s32Ret)
        {
            printf("%s: get ai frame select time out\n", __FUNCTION__);
            break;
        }

        if (FD_ISSET(AiFd, &read_fds))
        {
            /* get frame from ai chn */
            memset(&stAecFrm, 0, sizeof(AEC_FRAME_S));
            s32Ret = HI_MPI_AI_GetFrame(pstAiAlsaCtl->AiDev, pstAiAlsaCtl->AiChn, &stFrame, &stAecFrm, HI_FALSE);

            if (HI_SUCCESS != s32Ret )
            {
#if 0
                printf("%s: HI_MPI_AI_GetFrame(%d, %d), failed with %#x!\n", \
                       __FUNCTION__, pstAiAlsaCtl->AiDev, pstAiAlsaCtl->AiChn, s32Ret);
                pstAiCtl->bStart = HI_FALSE;
                return NULL;
#else
                continue;
#endif
            }

            /* send frame to alsa */
            if (1)
            {
                s32Ret = _SAMPLE_AUDIO_SendFrameToAlsa(&stFrame);
                if (HI_SUCCESS != s32Ret )
                {
                    printf("%s: SendFrame failed with %#x!\n", __FUNCTION__, s32Ret);
                    pstAiAlsaCtl->bStart = HI_FALSE;
                    return NULL;
                }
            }

            /* save data to file */
#if ALSA_SAVE_FILE
            (HI_VOID)fwrite(stFrame.u64VirAddr[0], 1, stFrame.u32Len, pstAiAlsaCtl->pfd);
            fflush(pstAiAlsaCtl->pfd);
#endif

            /* release frame */
            s32Ret = HI_MPI_AI_ReleaseFrame(pstAiAlsaCtl->AiDev, pstAiAlsaCtl->AiChn, &stFrame, &stAecFrm);
            if (HI_SUCCESS != s32Ret )
            {
                printf("LINE:%d, %s: HI_MPI_AI_ReleaseFrame(%d, %d), failed with %#x!\n", \
                       __LINE__, __FUNCTION__, pstAiAlsaCtl->AiDev, pstAiAlsaCtl->AiChn, s32Ret);

                snd_pcm_close(gs_handlePlayback);
                return NULL;
            }
        }
    }

    pstAiAlsaCtl->bStart = HI_FALSE;
    snd_pcm_close(gs_handlePlayback);
    return NULL;
}


/******************************************************************************
* function : Create the thread to get frame from ai and send to alsa
******************************************************************************/
HI_S32 _SAMPLE_AUDIO_CreatTrdAlsaSend(AUDIO_DEV AiDev, AI_CHN AiChn, AIO_ATTR_S stAioAttr, FILE* pAlsaFd)
{
    SAMPLE_AI2ALSA_S* pstAiAlsa = NULL;

    pstAiAlsa = &gs_stSampleAiAlsaSend[AiDev * AI_MAX_CHN_NUM + AiChn];
    pstAiAlsa->AiDev = AiDev;
    pstAiAlsa->AiChn = AiChn;
    pstAiAlsa->stAioAttr = stAioAttr;
    pstAiAlsa->bStart = HI_TRUE;
    pstAiAlsa->pfd = pAlsaFd;

    pthread_create(&pstAiAlsa->stAiPid, 0, _SAMPLE_AUDIO_AlsaSendProc, pstAiAlsa);

    return HI_SUCCESS;
}

/******************************************************************************
* function : Destory the thread to get frame from ai and send to alsa
******************************************************************************/
HI_S32 _SAMPLE_AUDIO_DestoryTrdAlsaSend(AUDIO_DEV AiDev, AI_CHN AiChn)
{
    SAMPLE_AI2ALSA_S* pstAiAlsa = NULL;

    pstAiAlsa = &gs_stSampleAiAlsaSend[AiDev * AI_MAX_CHN_NUM + AiChn];
    if (pstAiAlsa->bStart)
    {
        pstAiAlsa->bStart = HI_FALSE;
        pthread_join(pstAiAlsa->stAiPid, 0);
    }
    fclose(pstAiAlsa->pfd);

    return HI_SUCCESS;
}

static HI_S32 sample_audio_init(HI_VOID)
{
#if 0
    HI_S32 s32Ret = HI_FAILURE;

    s32Ret = HI_MPI_SYS_Init();

    if (HI_SUCCESS != s32Ret)
    {
        printf("HI_MPI_SYS_Init failed!\n");
        return HI_FAILURE;
    }
#endif

    return 0;
}

static HI_S32 sample_audio_startup(HI_VOID)
{
    HI_S32 s32Ret;
    HI_S32 s32AiChnCnt;
    AI_CHN AiChn = 0;
    AIO_ATTR_S stAioAttr;
    FILE* pfd = NULL;
    AUDIO_DEV AiDev = SAMPLE_AUDIO_INNER_AI_DEV;

    stAioAttr.enSamplerate   = AUDIO_SAMPLE_RATE_48000;
    stAioAttr.enBitwidth     = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode     = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode    = AUDIO_SOUND_MODE_MONO;
    stAioAttr.u32EXFlag      = 0;
    stAioAttr.u32FrmNum      = 30;
    stAioAttr.u32PtNumPerFrm = AACLC_SAMPLES_PER_FRAME;
    stAioAttr.u32ChnCnt      = 1;
    stAioAttr.u32ClkSel      = 0;
    stAioAttr.enI2sType = AIO_I2STYPE_INNERCODEC;

    /* config internal audio codec */
    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_DBG(s32Ret);
        goto AIAO_ERR3;
    }

#if ALSA_SAVE_FILE
    /* open alsa file */
    pfd = _SAMPLE_AUDIO_OpenAlsaFile(AiChn);
    if (!pfd)
    {
        SAMPLE_DBG(HI_FAILURE);
        goto AIAO_ERR3;
    }
#endif

    /* enable AI channle */
    s32AiChnCnt = stAioAttr.u32ChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, gs_enOutSampleRate, gs_bAioReSample, NULL, 0);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto AIAO_ERR3;
    }

    /* create alsa send thread */
    s32Ret = _SAMPLE_AUDIO_CreatTrdAlsaSend(AiDev, AiChn, stAioAttr, pfd);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
        goto AIAO_ERR2;
    }

    gs_s32AiDev = AiDev;
    gs_s32AiChn = AiChn;
    gs_s32AiChnCnt = s32AiChnCnt;
    goto AIAO_ERR3;

    s32Ret = _SAMPLE_AUDIO_DestoryTrdAlsaSend(AiDev, AiChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }

AIAO_ERR2:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, gs_bAioReSample, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }

AIAO_ERR3:

    return s32Ret;
}

static HI_S32 sample_audio_shutdown(HI_VOID)
{
    HI_S32 s32Ret;

    s32Ret = _SAMPLE_AUDIO_DestoryTrdAlsaSend(gs_s32AiDev, gs_s32AiChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }

    s32Ret |= SAMPLE_COMM_AUDIO_StopAi(gs_s32AiDev, gs_s32AiChnCnt, gs_bAioReSample, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_DBG(s32Ret);
    }

    return s32Ret;
}

static struct audio_control_ops audio_sc_ops = {
    .init = sample_audio_init,
    .startup = sample_audio_startup,
    .shutdown = sample_audio_shutdown,
};

HI_VOID sample_audio_config(HI_VOID)
{
    hiaudio_register_mpi_ops(&audio_sc_ops);
}

