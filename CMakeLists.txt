# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(SUPPORTED_TARGETS esp32s2)
set(IDF_TARGET esp32s2)
project(model_car_remote)
