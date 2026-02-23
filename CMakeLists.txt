cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED PROJECT_NAME)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    set(PROJECT_VER "1.0.0")
    set(PROJECT_NAME "esp32-c6-epc-scanner")
    add_compile_definitions(PROJECT_VER="${PROJECT_VER}")
    project(${PROJECT_NAME}-v${PROJECT_VER})
else()
    message(FATAL_ERROR "This must be a project's main CMakeLists.txt.")
endif()

set(EXTRA_COMPONENT_DIRS components)