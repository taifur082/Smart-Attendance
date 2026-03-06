cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "1.0.0")
set(PROJECT_NAME "esp32-c6-rc522-scanner")
project(${PROJECT_NAME}-v${PROJECT_VER})

set(EXTRA_COMPONENT_DIRS components)
