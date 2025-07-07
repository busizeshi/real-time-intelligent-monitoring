#ifndef SIMPLE_LOGGER_H
#define SIMPLE_LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream> // 用于在main函数中拼接字符串

// --- 全局变量 ---
// 使用一个全局的互斥锁来保护文件写入
static std::mutex g_log_mutex;
// 全局的日志文件流对象
static std::ofstream g_log_file;

/**
 * @brief 初始化日志记录器
 * @param filename 日志文件的路径
 * @return 如果成功打开文件则返回 true，否则返回 false
 */
inline bool init_logger(const std::string& filename) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_log_file.open(filename, std::ios::out | std::ios::app);
    if (!g_log_file.is_open()) {
        std::cerr << "Error: Failed to open log file: " << filename << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief 关闭日志记录器
 */
inline void close_logger() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_file.is_open()) {
        g_log_file.close();
    }
}

/**
 * @brief 核心日志记录函数 (内部使用)
 * @param level_str 日志级别字符串 ("INFO", "WARNING", "ERROR")
 * @param message 要记录的消息
 */
inline void log_core(const std::string& level_str, const std::string& message) {
    std::lock_guard<std::mutex> lock(g_log_mutex);

    if (!g_log_file.is_open()) {
        return; // 如果文件未打开，不执行任何操作
    }

    // 1. 获取并格式化当前时间
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    // 2. 写入 时间、级别和消息
    g_log_file << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
               << " [" << level_str << "] "
               << message << std::endl; // std::endl 会自动刷新缓冲区
}

/**
 * @brief 记录一条 INFO 级别的日志
 * @param message 日志消息
 */
inline void log_info(const std::string& message) {
    log_core("INFO   ", message);
}

/**
 * @brief 记录一条 WARNING 级别的日志
 * @param message 日志消息
 */
inline void log_warn(const std::string& message) {
    log_core("WARNING", message);
}

/**
 * @brief 记录一条 ERROR 级别的日志
 * @param message 日志消息
 */
inline void log_error(const std::string& message) {
    log_core("ERROR  ", message);
}

#endif // SIMPLE_LOGGER_H