cmake_minimum_required(VERSION 3.28...3.30)

# This file is copied into a pinned OBS plugin template checkout by
# scripts/build-obs-macos.sh. Its bootstrap builds the matching libobs SDK.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
project(${_name} VERSION ${_version} LANGUAGES C CXX)

if(NOT DEFINED DHRELINK_SOURCE_ROOT OR
   NOT EXISTS "${DHRELINK_SOURCE_ROOT}/src/obs/source/dhrelink_obs.cpp")
  message(FATAL_ERROR "Set DHRELINK_SOURCE_ROOT to the dhreLink source checkout")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(dhreLink MODULE
  "${DHRELINK_SOURCE_ROOT}/src/obs/source/dhrelink_obs.cpp"
  "${DHRELINK_SOURCE_ROOT}/src/core/src/shared_audio_stream_posix.cpp")
target_include_directories(dhreLink PRIVATE
  "${DHRELINK_SOURCE_ROOT}/src/core/include"
  "${DHRELINK_SOURCE_ROOT}/src/obs/include")
target_compile_features(dhreLink PRIVATE cxx_std_20)

find_package(libobs REQUIRED)
target_link_libraries(dhreLink PRIVATE OBS::libobs)

target_add_resource(dhreLink "${DHRELINK_SOURCE_ROOT}/assets/brand/dhreian-mark-purple-512.png")
set_target_properties_plugin(dhreLink PROPERTIES OUTPUT_NAME dhreLink)
