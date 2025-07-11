#include "rknn_yolo.h"
#include "logger.h"

extern "C"
{
#include "rkmedia_vi_venc.h"
#include "rtsp_push.h"
#include "tools.h"
}

#define INTERVAL_MS 15
#define HEIGHT 1080
#define WIDTH 1920
#define log_file_path "../log.txt"

bool quit;

static RTSPPusher *pusher;

static TSBuffer buffer;

static RknnDetector *detector;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *detect_func(void *args)
{
    while (!quit)
    {
        unsigned char *buf = (unsigned char *)malloc(WIDTH * HEIGHT * 3 / 2);
        unsigned char *rgb = (unsigned char *)malloc(WIDTH * HEIGHT * 3);
        size_t n = tsbuffer_read(&buffer, buf, WIDTH * HEIGHT * 3 / 2);
        nv12_to_rgb888(buf, rgb, WIDTH, HEIGHT);
        detector->inferAndDraw(rgb, WIDTH, HEIGHT);
        free(buf);
        free(rgb);
    }
}

static void sigterm_handler(int sig)
{
    close_logger();
    rkmedia_deinit();
    rtsp_pusher_close(pusher);
    tsbuffer_destroy(&buffer);
    fprintf(stderr, "signal %d\n", sig);
    quit = true;
}

/**
 * @brief 获取mpi数据线程
 *
 * @param args 接收参数
 */
void *collectMpiAndDector(void *args)
{
    MEDIA_BUFFER mb = NULL;

    long long last_time = get_time_ms();

    while (!quit)
    {
        mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, 0, -1);
        if (!mb)
        {
            log_error("RK_MPI_SYS_GetMediaBuffer get null buffer!");
            break;
        }

        long long now = get_time_ms();

        char time_str[64];
        get_time_str(now, time_str, 64);
        // printf("当前获取帧的时间是%s\n", time_str);
        if (now - last_time >= INTERVAL_MS)
        {
            last_time += INTERVAL_MS;

            tsbuffer_write(&buffer, (unsigned char *)RK_MPI_MB_GetPtr(mb), WIDTH * HEIGHT * 3 / 2);
        }

        RK_MPI_MB_SetSize(mb, WIDTH * HEIGHT * 3 / 2);

        RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, 0, mb);
        RK_MPI_MB_ReleaseBuffer(mb);
    }

    return NULL;
}

/**
 * @brief 编码rgb888帧数据为h264数据并推流
 */
static void *encodeRgb888ToH264(void *arg)
{

    MEDIA_BUFFER mb = NULL;
    while (!quit)
    {
        mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, 0, -1);
        if (!mb)
        {
            printf("RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
            break;
        }

        printf("编码时间为%lld ms\n", RK_MPI_MB_GetTimestamp(mb));
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
    if (argc < 3)
    {
        log_warn("缺少参数");
    }

    char *rtsp_url = argv[1];
    printf("rtsp_url: %s\n", rtsp_url);
    char *model_path = argv[2];
    printf("model_path: %s\n", model_path);

    signal(SIGINT, sigterm_handler);

    // 初始化相关变量
    int ret = 0;
    quit = false;

    init_logger(log_file_path);

    tsbuffer_init(&buffer, WIDTH * HEIGHT * 3 * 20);

    // rkmedia初始化
    ret = rkmedia_init();
    if (ret)
    {
        log_error("rkmedia_init failed!");
        return -1;
    }
    // rtsp推流器初始化
    pusher = rtsp_pusher_init(rtsp_url, WIDTH, HEIGHT, 30);
    // 目标检测初始化
    detector = new RknnDetector(model_path);
    if (!detector->isInitialized())
    {
        log_error("Failed to initialize RknnDetector.");
        return -1;
    }

    log_info("开始采集数据");
    pthread_t collect_thread;
    pthread_create(&collect_thread, NULL, collectMpiAndDector, NULL);
    ret = mpi_vi_start();

    log_info("开始编码推流");
    pthread_t push_thread;
    pthread_create(&push_thread, NULL, encodeRgb888ToH264, NULL);

    log_info("开始检测");
    pthread_t detect_thread;
    pthread_create(&detect_thread, NULL, detect_func, detector);

    while (!quit)
    {
        usleep(500000);
    }
    pthread_join(detect_thread, NULL);
    pthread_join(push_thread, NULL);
    pthread_join(collect_thread, NULL);

    return 0;
}