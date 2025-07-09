cmake_minimum_required(VERSION 3.10)
project(real-time-intelligent-monitoring)

# 配置交叉编译链P
set(TOOLCHAIN_DIR /opt/atk-dlrv1126-toolchain)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/usr/bin/arm-linux-gnueabihf-g++)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/usr/bin/arm-linux-gnueabihf-gcc)
set(SYSROOT ${TOOLCHAIN_DIR}/arm-buildroot-linux-gnueabihf/sysroot/usr/include)
set(CMAKE_SYSROOT ${TOOLCHAIN_DIR}/arm-buildroot-linux-gnueabihf/sysroot)

# 设置变量
set(AIQ_DIR ${TOOLCHAIN_DIR}/arm-buildroot-linux-gnueabihf/sysroot/usr/include/rkaiq)
set(RK_VI ${CMAKE_SOURCE_DIR}/rv1126/vi)
set(RKNN_YOLO ${CMAKE_SOURCE_DIR}/rv1126/rknn)
set(RKNN_API_LIB ${RKNN_YOLO}/librknn_api/lib/librknn_api.so)

# 设置第三方库
set(OPENCV_LIBS opencv_core opencv_imgcodecs opencv_imgproc opencv_features2d opencv_flann opencv_highgui opencv_freetype)
set(RKMEDIA_LIBS easymedia rga rkaiq rockx sample_common_isp)
set(FFMPEG_LIBS avcodec avformat avutil swscale swresample)

# 设置本地库
link_directories(lib)
link_directories(${RKNN_YOLO}/librknn_api/lib)

# 添加头文件
include_directories(${SYSROOT})
include_directories(${RK_VI})
include_directories(${RK_VI}/common)
include_directories(${RK_VI}/include/easymedia)
include_directories(${RK_VI}/include/rkmedia)
include_directories(${AIQ_DIR}/uAPI)
include_directories(${AIQ_DIR}/xcore)
include_directories(${AIQ_DIR}/common)
include_directories(${AIQ_DIR}/algos)
include_directories(${AIQ_DIR}/iq_parser)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${RKNN_YOLO}/3rdparty)
include_directories(${RKNN_YOLO}/librknn_api/include)
include_directories(${RKNN_YOLO}/3rdparty/rga/include)
include_directories(${RKNN_YOLO}/3rdparty/drm/include)
include_directories(${RKNN_YOLO}/3rdparty/drm/include/libdrm)
include_directories(${RKNN_YOLO}/3rdparty/stb)
include_directories(${RKNN_YOLO}/3rdparty/CImg)
include_directories(${RKNN_YOLO}/include)

aux_source_directory(src SRC_LIST)
aux_source_directory(src/rknn_yolo SRC_LIST)

add_definitions(-DRKAIQ)

add_definitions(-g -O0 -ggdb -gdwarf -funwind-tables -rdynamic)
add_definitions(-Wno-write-strings -Wno-return-type)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} ${OPENCV_LIBS} ${RKMEDIA_LIBS} ${FFMPEG_LIBS} ${RKNN_API_LIB} pthread dl)

set(INSTALL_DIR ${CMAKE_SOURCE_DIR}/install/real_time_monitoring)
install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_DIR}/bin)