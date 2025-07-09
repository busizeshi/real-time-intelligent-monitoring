#include "rkmedia_vi_venc.h"

RK_U32 u32Width;
RK_U32 u32Height;
int frameCnt;
RK_CHAR *pDeviceName;
RK_CHAR *pOutPath;
RK_CHAR *pIqfilesPath;
RK_S32 s32CamId;
RK_BOOL bMultictx;
int fps;
VI_CHN_ATTR_S vi_chn_attr;
bool quit;
VENC_CHN_ATTR_S venc_chn_attr;

int rkmedia_init()
{
    int ret = 0;

    u32Width = 1920;
    u32Height = 1080;
    frameCnt = -1;
    pDeviceName = "rkispp_scale0";
    pIqfilesPath = "/etc/iqfiles/";
    s32CamId = 0;
    bMultictx = RK_FALSE;
    fps = 30;

    printf("#####Device: %s\n", pDeviceName);
    printf("#####Resolution: %dx%d\n", u32Width, u32Height);
    printf("#####Frame Count to save: %d\n", frameCnt);
    printf("#####Output Path: %s\n", pOutPath);
    printf("#CameraIdx: %d\n\n", s32CamId);

#ifdef RKAIQ
    printf("#####Aiq xml dirpath: %s\n\n", pIqfilesPath);
    printf("#bMultictx: %d\n\n", bMultictx);
    rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
    SAMPLE_COMM_ISP_Init(s32CamId, hdr_mode, bMultictx, pIqfilesPath);
    SAMPLE_COMM_ISP_Run(s32CamId);
    SAMPLE_COMM_ISP_SetFrameRate(s32CamId, fps);
#endif

    RK_MPI_SYS_Init();

    VENC_CHN_ATTR_S venc_chn_attr = {0};
    venc_chn_attr.stVencAttr.u32PicWidth = u32Width;
    venc_chn_attr.stVencAttr.u32PicHeight = u32Height;
    venc_chn_attr.stVencAttr.u32VirWidth = u32Width;
    venc_chn_attr.stVencAttr.u32VirHeight = u32Height;
    venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_BGR888;
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
    venc_chn_attr.stVencAttr.u32Profile = 77;
    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 2 * fps;
    venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = u32Width * u32Height;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = fps;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = fps;
    ret = RK_MPI_VENC_CreateChn(0, &venc_chn_attr);
    if (ret)
    {
        printf("ERROR: Create venc failed!\n");
        exit(0);
    }

    vi_chn_attr.pcVideoNode = pDeviceName;
    vi_chn_attr.u32BufCnt = 3;
    vi_chn_attr.u32Width = u32Width;
    vi_chn_attr.u32Height = u32Height;
    vi_chn_attr.enPixFmt = IMAGE_TYPE_BGR888;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
    vi_chn_attr.enBufType = VI_CHN_BUF_TYPE_MMAP;
    ret = RK_MPI_VI_SetChnAttr(s32CamId, 0, &vi_chn_attr);
    ret |= RK_MPI_VI_EnableChn(s32CamId, 0);
    if (ret)
    {
        printf("Create VI[0] failed! ret=%d\n", ret);
        return -1;
    }

    return 0;
}

int mpi_vi_start()
{
    int ret = RK_MPI_VI_StartStream(s32CamId, 0);
    if (ret)
    {
        printf("Start VI[0] failed! ret=%d\n", ret);
        return -1;
    }
}

int rkmedia_deinit()
{
    RK_MPI_VI_DisableChn(s32CamId, 0);
    RK_MPI_VENC_DestroyChn(0);

#ifdef RKAIQ
    SAMPLE_COMM_ISP_Stop(s32CamId);
#endif
}

int venc_start(bool quit, SafeQueue *queue)
{
    RK_U32 u32FrameId = 0;
    RK_S32 s32ReadSize = 0;
    RK_S32 s32FrameSize = 0;
    RK_U64 u64TimePeriod = 1000000 / fps; // us
    MB_IMAGE_INFO_S stImageInfo = {u32Width, u32Height, u32Width, u32Height,
                                   IMAGE_TYPE_BGR888};

    while (!quit)
    {
        MEDIA_BUFFER mb =
            RK_MPI_MB_CreateImageBuffer(&stImageInfo, RK_TRUE, MB_FLAG_NOCACHED); // 用于分配一块用于存放图像帧的缓冲区
        if (!mb)
        {
            printf("ERROR: no space left!\n");
            break;
        }

        // One frame size for nv12 image.
        s32FrameSize = u32Width * u32Height * 3;

        void *rtspDataPtr = RK_MPI_MB_GetPtr(mb);

        while (queue->size < 0)
        {
            usleep(1000);
        }

        rtspDataPtr = safe_queue_dequeue(queue);

        RK_MPI_MB_SetSize(mb, s32FrameSize);
        RK_MPI_MB_SetTimestamp(mb, u32FrameId * u64TimePeriod);
        printf("#Send frame[%d] fd=%d to out...\n", u32FrameId++,
               RK_MPI_MB_GetFD(mb));
        RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, 0, mb);
        // mb must be release. The encoder has internal references to the data sent
        // in. Therefore, mb cannot be reused directly
        RK_MPI_MB_ReleaseBuffer(mb);

        usleep(u64TimePeriod);
    }
}