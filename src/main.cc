#include "rknn_yolo.h"

extern "C"
{
#include "rkmedia_vi_venc.h"
#include "rtsp_push.h"
}

bool quit;

RTSPPusherContext *rtsp_ctx;

RknnDetector detector("/userdata/rknn_yolov5_demo/model/rv1109_rv1126/yolov5s_relu_rv1109_rv1126_out_opt.rknn");

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
    SafeQueue *queue = (SafeQueue *)args;

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

        unsigned char *tmp_data = (unsigned char *)malloc(stImageInfo.u32Width * stImageInfo.u32Height * 3);
        memcpy(tmp_data, RK_MPI_MB_GetPtr(mb), stImageInfo.u32Width * stImageInfo.u32Height * 3);
        RK_MPI_MB_ReleaseBuffer(mb);

        printf("正在检测......\n");
        detector.inferAndDraw((unsigned char *)tmp_data, 1920, 1080);
        safe_queue_enqueue(queue, tmp_data);
    }

    return NULL;
}

/**
 * @brief 编码rgb888帧数据
 */
static void *encodeRgb888ToH264(void *arg)
{
    SafeQueue *queue = (SafeQueue *)arg;
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
        rtsp_pusher_push_frame(rtsp_ctx, (uint8_t *)RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb));

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
    SafeQueue *queue = safe_queue_create(10);
    void *img_data;
    ret = rkmedia_init();
    // rtsp推流器初始化
    rtsp_ctx = rtsp_pusher_init("rtsp://192.168.1.10/live/stream", u32Width, u32Height, fps);
    // 目标检测初始化
    if (!detector.isInitialized())
    {
        std::cerr << "ERROR: Failed to initialize RknnDetector." << std::endl;
        return -1;
    }

    if (ret)
    {
        printf("rkmedia_init failed!\n");
        return -1;
    }

    printf("开始采集数据\n");
    pthread_t collect_thread;
    pthread_create(&collect_thread, NULL, collectMpiBuffer, queue);
    ret = mpi_vi_start();

    venc_start(quit, queue);

    printf("开始推流\n");
    pthread_t push_thread;
    pthread_create(&push_thread, NULL, encodeRgb888ToH264, &detector);

    while (!quit)
    {
        usleep(500000);
    }
    safe_queue_destroy(queue);
    rkmedia_deinit();
    rtsp_pusher_cleanup(rtsp_ctx);

    return 0;
}