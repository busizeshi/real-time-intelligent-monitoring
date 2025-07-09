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
    VENC_RC_MODE_E enEncoderMode = VENC_RC_MODE_H264CBR;
    pDeviceName = "rkispp_scale0";
    pIqfilesPath = "/etc/iqfiles/";
    s32CamId = 0;
    bMultictx = RK_FALSE;
    fps = 30;

    printf("\n---------------------初始化rkmedia api-----------------------------\n");
    // todo 输出初始化信息

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