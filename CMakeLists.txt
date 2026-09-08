cmake_minimum_required(VERSION 3.25)

project(dhreLink VERSION 1.0.0 LANGUAGES CXX)

option(DHRELINK_BUILD_TESTS "Build core transport tests" ON)
option(DHRELINK_BUILD_VST3 "Build the VST3 sender" OFF)
option(DHRELINK_BUILD_OBS "Build the OBS receiver" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(src/core)

if(DHRELINK_BUILD_VST3)
  add_subdirectory(src/vst3)
endif()

if(DHRELINK_BUILD_OBS)
  add_subdirectory(src/obs)
endif()

if(DHRELINK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
