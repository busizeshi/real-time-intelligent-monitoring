#include "rknn_yolo.h"

extern "C"
{
#include "rkmedia_vi_venc.h"
}

bool quit;

FILE *fp = fopen("../input.rgb", "w");
RknnDetector detector("/userdata/rknn_yolov5_demo/model/rv1109_rv1126/yolov5s_relu_rv1109_rv1126_out_opt.rknn");

static void sigterm_handler(int sig)
{
    fprintf(stderr, "signal %d\n", sig);
    quit = true;
}

void *GetMediaBuffer(void *args)
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

        unsigned char *tmp_data = (unsigned char *)malloc(stImageInfo.u32Width * stImageInfo.u32Height * 3);
        memcpy(tmp_data, RK_MPI_MB_GetPtr(mb), stImageInfo.u32Width * stImageInfo.u32Height * 3);
        RK_MPI_MB_ReleaseBuffer(mb);

        printf("正在检测......\n");
        detector.inferAndDraw((unsigned char *)tmp_data, 1920, 1080);
        fwrite(tmp_data, 1, 1920 * 1080 * 3, fp);
        free(tmp_data);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sigterm_handler);
    // 目标检测初始化
    if (!detector.isInitialized())
    {
        std::cerr << "ERROR: Failed to initialize RknnDetector." << std::endl;
        return -1;
    }

    // 初始化相关变量
    int ret = 0;
    quit = false;
    SafeQueue *queue = safe_queue_create(10);
    ret = rkmedia_init();
    void *img_data;
    if (ret)
    {
        printf("rkmedia_init failed!\n");
        return -1;
    }

    printf("开始采集数据\n");
    pthread_t read_thread;
    pthread_create(&read_thread, NULL, GetMediaBuffer, &detector);
    ret = mpi_vi_start();

    while (!quit)
    {
        usleep(500000);
    }
    safe_queue_destroy(queue);
    fclose(fp);

    return 0;
}