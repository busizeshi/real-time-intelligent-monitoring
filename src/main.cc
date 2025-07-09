#include "rknn_yolo.h"

extern "C"
{
#include "rkmedia_vi_venc.h"
#include "rtsp_push.h"
}

bool quit;

static RTSPPusher *pusher;

static RknnDetector *detector;

static ByteBuffer *buf;

static void sigterm_handler(int sig)
{
    fprintf(stderr, "signal %d\n", sig);
    quit = true;
}

/**
 * @brief 获取mpi数据线程
 *
 * @param args 接收参数
 */
void *collectMpiBuffer(void *args)
{
    MEDIA_BUFFER mb = NULL;

    while (!quit)
    {
        mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, 0, -1);
        if (!mb)
        {
            printf("RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
            break;
        }

        MB_IMAGE_INFO_S stImageInfo = {0};
        int ret = RK_MPI_MB_GetImageInfo(mb, &stImageInfo);
        if (ret)
            printf("Warn: Get image info failed! ret = %d\n", ret);

        printf("Get Frame:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
               "timestamp:%lld, ImgInfo:<wxh %dx%d, fmt 0x%x>\n",
               RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
               RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb),
               RK_MPI_MB_GetTimestamp(mb), stImageInfo.u32Width,
               stImageInfo.u32Height, stImageInfo.enImgType);

        unsigned char *tmp_data = (unsigned char *)malloc(u32Width * u32Height * 3);
        memcpy(tmp_data, RK_MPI_MB_GetPtr(mb), u32Width * u32Height * 3);

        printf("正在检测......\n");
        detector->inferAndDraw((unsigned char *)RK_MPI_MB_GetPtr(mb), 1920, 1080);
        byte_buffer_write(buf, (unsigned char *)RK_MPI_MB_GetPtr(mb), u32Height * u32Width * 3);
        RK_MPI_MB_ReleaseBuffer(mb);
    }

    return NULL;
}

/**
 * @brief 编码rgb888帧数据
 */
static void *encodeRgb888ToH264(void *arg)
{
    printf("编码rgb888帧数据\n");

    MEDIA_BUFFER mb = NULL;
    while (!quit)
    {
        mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, 0, -1);
        if (!mb)
        {
            printf("RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
            break;
        }

        printf("Get packet:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
               "timestamp:%lld\n",
               RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
               RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb),
               RK_MPI_MB_GetTimestamp(mb));
        if (rtsp_pusher_push_frame(pusher, (unsigned char *)RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb)) < 0)
        {
            fprintf(stderr, "Failed to push frame, exiting.\n");
            break;
        }

        RK_MPI_MB_ReleaseBuffer(mb);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigterm_handler);

    // 初始化相关变量
    int ret = 0;
    quit = false;

    // rkmedia初始化
    ret = rkmedia_init();
    if (ret)
    {
        printf("rkmedia_init failed!\n");
        return -1;
    }

    // rtsp推流器初始化
    pusher = rtsp_pusher_init("rtsp://192.168.1.10/live/stream", u32Width, u32Height, 60);

    // 目标检测初始化
    detector = new RknnDetector("/userdata/rknn_yolov5_demo/model/rv1109_rv1126/yolov5s_relu_rv1109_rv1126_out_opt.rknn");
    if (!detector->isInitialized())
    {
        std::cerr << "ERROR: Failed to initialize RknnDetector." << std::endl;
        return -1;
    }

    buf = byte_buffer_create(u32Width * u32Height * 3 * 20);

    printf("开始采集数据\n");
    pthread_t collect_thread;
    pthread_create(&collect_thread, NULL, collectMpiBuffer, NULL);
    ret = mpi_vi_start();

    printf("开始推流\n");
    pthread_t push_thread;
    pthread_create(&push_thread, NULL, encodeRgb888ToH264, NULL);
    venc_start(quit, buf);

    while (!quit)
    {
        usleep(500000);
    }
    rkmedia_deinit();
    rtsp_pusher_close(pusher);

    return 0;
}