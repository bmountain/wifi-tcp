cmake_minimum_required(VERSION 3.20)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(
  COMPONENTS
  "main esptool_py wifi task format"
  CACHE STRING
  "List of components to include"
  )

project(wifi_tcp_test)

set(CMAKE_CXX_STANDARD 23)
