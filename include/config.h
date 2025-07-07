// config.h

#ifndef CONFIG_H
#define CONFIG_H

extern const char *const model_path;; // 模型权重文件地址
extern const char *const log_path;   // 日志文件地址
extern const char *const label_txt;  // 标签文件地址

extern const int num_threads;

extern const int frame_width;
extern const int frame_height;
extern const int channels;

extern const int fps; 

extern const char *const iq_files;

#endif // CONFIG_H