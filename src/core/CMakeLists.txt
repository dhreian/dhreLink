add_library(dhrelink_core STATIC)
add_library(dhreLink::core ALIAS dhrelink_core)

target_include_directories(dhrelink_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
  target_sources(dhrelink_core PRIVATE src/shared_audio_stream_win.cpp)
  target_compile_definitions(dhrelink_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
  target_sources(dhrelink_core PRIVATE src/shared_audio_stream_stub.cpp)
endif()

if(MSVC)
  target_compile_options(dhrelink_core PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(dhrelink_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
