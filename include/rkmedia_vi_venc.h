#ifndef _RKMEDIA_VI_VENC_H_
#define _RKMEDIA_VI_VENC_H_

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "common/sample_common.h"
#include "rkmedia_api.h"
#include "safe_queue.h"

extern RK_U32 u32Width;
extern RK_U32 u32Height;
extern int frameCnt;
extern RK_CHAR *pDeviceName;
extern RK_CHAR *pIqfilesPath;
extern RK_S32 s32CamId;
extern RK_BOOL bMultictx;
extern int fps;
extern VI_CHN_ATTR_S vi_chn_attr;

/**
 * @brief RKMedia 初始化
 */
int rkmedia_init();

/**
 * @brief mpi开始采集摄像头数据
 */
int mpi_vi_start();

/**
 * @brief mpi停止采集摄像头数据，并释放资源
 */
int mpi_vi_stop();

#endif