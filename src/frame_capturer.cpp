#include "frame_capturer.h"

#include "frame_capturer.h"
#include "common/sample_common.h" // 假设这个文件存在且包含了ISP相关函数
#include <unistd.h>
#include <cstdio>

FrameCapturer::FrameCapturer(const CaptureConfig& config, FrameCallback callback)
    : m_config(config),
      m_frameCallback(callback),
      m_threadId(0),
      m_isRunning(false) {}

FrameCapturer::~FrameCapturer() {
    Stop();
    DeInit();
    printf("FrameCapturer destructed.\n");
}

bool FrameCapturer::Init() {
    if (m_config.enable_aiq) {
#ifdef RKAIQ
        printf("##### AIQ enabled. XML dir: %s\n", m_config.iq_files_path.c_str());
        rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
        SAMPLE_COMM_ISP_Init(m_config.camera_id, hdr_mode, m_config.multi_ctx, m_config.iq_files_path.c_str());
        SAMPLE_COMM_ISP_Run(m_config.camera_id);
        SAMPLE_COMM_ISP_SetFrameRate(m_config.camera_id, 30); // 帧率可以加入配置
#else
        printf("Warning: RKAIQ is enabled in config, but not compiled. AIQ will not run.\n");
#endif
    }

    int ret = RK_MPI_SYS_Init();
    if (ret) {
        printf("ERROR: RK_MPI_SYS_Init failed! ret = %d\n", ret);
        return false;
    }

    VI_CHN_ATTR_S vi_chn_attr;
    vi_chn_attr.pcVideoNode = const_cast<char*>(m_config.device_name.c_str());
    vi_chn_attr.u32BufCnt = 3;
    vi_chn_attr.u32Width = m_config.width;
    vi_chn_attr.u32Height = m_config.height;
    vi_chn_attr.enPixFmt = m_config.format;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
    vi_chn_attr.enBufType = VI_CHN_BUF_TYPE_MMAP;

    ret = RK_MPI_VI_SetChnAttr(m_config.camera_id, 0, &vi_chn_attr);
    ret |= RK_MPI_VI_EnableChn(m_config.camera_id, 0);
    if (ret) {
        printf("ERROR: Create VI[chn 0] failed! ret = %d\n", ret);
        RK_MPI_SYS_Exit(); // 清理已初始化的部分
        return false;
    }

    printf("FrameCapturer initialized successfully.\n");
    return true;
}

void FrameCapturer::DeInit() {
#ifdef RKAIQ
    if (m_config.enable_aiq) {
        SAMPLE_COMM_ISP_Stop(m_config.camera_id);
    }
#endif
    RK_MPI_SYS_Exit();
    printf("FrameCapturer de-initialized.\n");
}

bool FrameCapturer::Start() {
    if (m_isRunning) {
        printf("Warning: FrameCapturer is already running.\n");
        return true;
    }

    m_isRunning = true;
    if (pthread_create(&m_threadId, nullptr, &FrameCapturer::WorkerThreadEntry, this) != 0) {
        printf("ERROR: Failed to create worker thread.\n");
        m_isRunning = false;
        return false;
    }

    int ret = RK_MPI_VI_StartStream(m_config.camera_id, 0);
    if (ret) {
        printf("ERROR: Start VI[0] failed! ret=%d\n", ret);
        m_isRunning = false;
        pthread_join(m_threadId, nullptr); // 等待线程退出
        return false;
    }
    
    printf("FrameCapturer started.\n");
    return true;
}

void FrameCapturer::Stop() {
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false; // 向线程发送停止信号

    if (m_threadId != 0) {
        pthread_join(m_threadId, nullptr);
        m_threadId = 0;
    }
    
    RK_MPI_VI_DisableChn(m_config.camera_id, 0);
    printf("FrameCapturer stopped.\n");
}

bool FrameCapturer::IsRunning() const {
    return m_isRunning;
}

void* FrameCapturer::WorkerThreadEntry(void* arg) {
    auto* capturer = static_cast<FrameCapturer*>(arg);
    capturer->WorkerThreadLoop();
    return nullptr;
}

void FrameCapturer::WorkerThreadLoop() {
    FILE* save_file = nullptr;
    if (!m_config.output_path.empty()) {
        save_file = fopen(m_config.output_path.c_str(), "w");
        if (!save_file) {
            printf("ERROR: Failed to open output file: %s\n", m_config.output_path.c_str());
        }
    }

    int frame_id = 0;
    int save_cnt = m_config.frame_count_to_save;
    MEDIA_BUFFER mb = nullptr;

    while (m_isRunning) {
        mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, 0, -1);
        if (!mb) {
            printf("Warning: RK_MPI_SYS_GetMediaBuffer get null buffer! Maybe stopping...\n");
            usleep(10000); // 避免空转
            continue;
        }

        // 使用者通过回调函数处理帧
        if (m_frameCallback) {
            m_frameCallback(mb);
        }

        // 内部也可以执行保存文件的逻辑
        if (save_file) {
            fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), save_file);
            printf("# Frame %d saved to %s\n", frame_id++, m_config.output_path.c_str());
        }

        RK_MPI_MB_ReleaseBuffer(mb);

        if (save_cnt > 0) {
            save_cnt--;
            if (save_cnt == 0) {
                printf("Reached target frame count. Stopping...\n");
                m_isRunning = false; // 通知主线程和其他部分可以停止了
                break;
            }
        }
    }

    if (save_file) {
        fclose(save_file);
    }
}