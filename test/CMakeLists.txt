cmake_minimum_required(VERSION 3.10)
project(project_test)
include_directories(/home/jwd/development/project_demo/real-time-intelligent-monitoring/include)
add_executable(project_test demo1.c /home/jwd/development/project_demo/real-time-intelligent-monitoring/src/tools.c)