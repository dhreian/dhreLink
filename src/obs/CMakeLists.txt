set(OBS_SOURCE_DIR "" CACHE PATH "Path to the matching OBS Studio source tree")
set(OBS_LIBOBS_LIBRARY "" CACHE FILEPATH "Path to the x64 obs.lib import library")
set(OBS_RUNTIME_DLL "" CACHE FILEPATH "Path to the matching x64 obs.dll runtime")

if(NOT EXISTS "${OBS_SOURCE_DIR}/libobs/obs-module.h")
  message(FATAL_ERROR "DHRELINK_BUILD_OBS requires -DOBS_SOURCE_DIR=<path-to-obs-studio>")
endif()

if(NOT EXISTS "${OBS_LIBOBS_LIBRARY}")
  message(FATAL_ERROR "DHRELINK_BUILD_OBS requires -DOBS_LIBOBS_LIBRARY=<path-to-libobs-library>")
endif()

add_library(dhrelink_obs MODULE source/dhrelink_obs.cpp)
configure_file(include/obsconfig.h "${CMAKE_CURRENT_BINARY_DIR}/generated/obsconfig.h" COPYONLY)
target_include_directories(
  dhrelink_obs
  PRIVATE
    "${OBS_SOURCE_DIR}/libobs"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(dhrelink_obs PRIVATE dhreLink::core "${OBS_LIBOBS_LIBRARY}")
target_compile_features(dhrelink_obs PRIVATE cxx_std_20)
target_compile_definitions(dhrelink_obs PRIVATE UNICODE _UNICODE)
if(MSVC)
  target_compile_options(dhrelink_obs PRIVATE /utf-8)
endif()
set_target_properties(dhrelink_obs PROPERTIES PREFIX "" OUTPUT_NAME "dhreLink")

install(TARGETS dhrelink_obs
  RUNTIME DESTINATION "obs/dhreLink/bin/64bit"
  LIBRARY DESTINATION "obs/dhreLink/bin/64bit"
  COMPONENT obs
)

install(
  FILES
    data/dhreLink-dark.svg
    data/dhreLink-light.svg
  DESTINATION "obs/dhreLink/data"
  COMPONENT obs
)
