# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

add_compile_options(
    "-DCFG_TUH_XINPUT=1"
    "-DCFG_TUH_ENUMERATION_BUFSIZE=512"
    "-DCFG_TUH_ENDPOINT_MAX=16"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32Transmitter)