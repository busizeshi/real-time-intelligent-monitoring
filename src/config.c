#include "config.h"

const char *const model_path = "config/model/yolov5s_relu_rv1109_rv1126_out_opt.rknn";
const char *const label_txt = "config/model/coco_80_labels_list.txt";
const char *const log_path = "/tmp/real-time-intelligent-monitoring/log.txt";
const int num_threads = 3;
const int frame_width = 1920;
const int frame_height = 1080;
const int fps = 30;

extern const char *const iq_files="/etc/iqfiles/";