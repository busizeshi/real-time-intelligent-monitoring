#include "rknn_yolo.h"

extern "C"
{
#include "rkmedia_vi_venc.h"
#include "rtsp_push.h"
}

bool quit;

static RTSPPusher *pusher;

static RknnDetector *detector;

static TSBuffer buffer;

static int count = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void sigterm_handler(int sig)
{
    fprintf(stderr, "signal %d\n", sig);
    quit = true;
}

int venc_start()
{
    RK_U32 u32FrameId = 0;
    RK_S32 s32ReadSize = 0;
    // RK_U64 u64TimePeriod = 1000000 / fps; // us
    RK_U64 u64TimePeriod = 0; // us
    MB_IMAGE_INFO_S stImageInfo = {u32Width, u32Height, u32Width, u32Height,
                                   IMAGE_TYPE_RGB888};

    while (!quit)
    {
        MEDIA_BUFFER mb =
            RK_MPI_MB_CreateImageBuffer(&stImageInfo, RK_TRUE, MB_FLAG_NOCACHED); // 用于分配一块用于存放图像帧的缓冲区
        if (!mb)
        {
            printf("ERROR: no space left!\n");
            break;
        }

        size_t n = u32Height * u32Width * 3;

        tsbuffer_read(&buffer, (unsigned char *)RK_MPI_MB_GetPtr(mb), n);

        RK_MPI_MB_SetSize(mb, u32Width * u32Height * 3);
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
        pthread_mutex_lock(&lock);
        printf("--------------------------------采集中-----------------------------\n");
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

        printf("\n\nGet Frame:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
               "timestamp:%lld, ImgInfo:<wxh %dx%d, fmt 0x%x>\n\n",
               RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
               RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb),
               RK_MPI_MB_GetTimestamp(mb), stImageInfo.u32Width,
               stImageInfo.u32Height, stImageInfo.enImgType);

        printf("正在检测......\n");
        detector->inferAndDraw((unsigned char *)RK_MPI_MB_GetPtr(mb), 1920, 1080);

        tsbuffer_write(&buffer, (unsigned char *)RK_MPI_MB_GetPtr(mb), 1920 * 1080 * 3);
        RK_MPI_MB_ReleaseBuffer(mb);
        printf("--------------------------------采集完成-----------------------------\n");
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

/**
 * @brief 编码rgb888帧数据
 */
static void *encodeRgb888ToH264(void *arg)
{

    MEDIA_BUFFER mb = NULL;
    while (!quit)
    {
        pthread_mutex_lock(&lock);
        printf("--------------------------------推流中-----------------------------\n");
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
        printf("--------------------------------推流完成-----------------------------\n");
        pthread_mutex_unlock(&lock);
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

    tsbuffer_init(&buffer, 1024 * 1920 * 3 * 10);

    printf("开始采集数据\n");
    pthread_t collect_thread;
    pthread_create(&collect_thread, NULL, collectMpiBuffer, NULL);
    ret = mpi_vi_start();

    printf("开始推流\n");
    pthread_t push_thread;
    pthread_create(&push_thread, NULL, encodeRgb888ToH264, NULL);
    venc_start();

    while (!quit)
    {
        usleep(500000);
    }
    rkmedia_deinit();
    rtsp_pusher_close(pusher);
    tsbuffer_destroy(&buffer);

    return 0;
}