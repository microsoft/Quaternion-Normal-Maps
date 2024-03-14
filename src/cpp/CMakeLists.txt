# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.15)
project(QLogTools)

find_package(OpenImageIO REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(convertNormalToQLog convertNormalToQLog.cpp)

target_link_libraries( convertNormalToQLog
  PRIVATE
    OpenImageIO::OpenImageIO
)