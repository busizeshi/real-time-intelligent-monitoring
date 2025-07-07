cmake_minimum_required(VERSION 3.10)
project(real-time-intelligent-monitoring)

add_definitions(-g -O0 -ggdb -gdwarf -funwind-tables -rdynamic)
add_definitions(-Wno-write-strings -Wno-return-type)

add_definitions(-DRKAIQ)

# 配置交叉编译链
set(TOOLCHAIN_DIR /opt/atk-dlrv1126-toolchain)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/usr/bin/arm-linux-gnueabihf-g++)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/usr/bin/arm-linux-gnueabihf-gcc)
set(SYSROOT ${TOOLCHAIN_DIR}/arm-buildroot-linux-gnueabihf/sysroot/usr/include)
set(CMAKE_SYSROOT ${TOOLCHAIN_DIR}/arm-buildroot-linux-gnueabihf/sysroot)

# 设置第三方库
set(AIQ_DIR ${TOOLCHAIN_DIR}/arm-buildroot-linux-gnueabihf/sysroot/usr/include/rkaiq)
set(OPENCV_LIBS opencv_core opencv_imgcodecs opencv_imgproc opencv_features2d opencv_flann opencv_highgui opencv_freetype)
set(RKMEDIA_LIBS easymedia rga rkaiq rockx sample_common_isp)
set(FFMPEG_LIBS avcodec avformat avutil swscale swresample)
include_directories(${CMAKE_SOURCE_DIR}/rv1126/vi/common)
include_directories(${CMAKE_SOURCE_DIR}/rv1126/vi/include/easymedia)
include_directories(${CMAKE_SOURCE_DIR}/rv1126/vi/include/rkmedia)
include_directories(${AIQ_DIR}/uAPI)
include_directories(${AIQ_DIR}/xcore)
include_directories(${AIQ_DIR}/common)
include_directories(${AIQ_DIR}/algos)
include_directories(${AIQ_DIR}/iq_parser)

include_directories(include)

aux_source_directory(src SRC_LIST)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} ${OPENCV_LIBS} ${RKMEDIA_LIBS} ${FFMPEG_LIBS} pthread dl)
