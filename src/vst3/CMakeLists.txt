set(VST3_SDK_ROOT "" CACHE PATH "Path to the Steinberg VST3 SDK root")

if(NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "DHRELINK_BUILD_VST3 requires -DVST3_SDK_ROOT=<path-to-vst3sdk>")
endif()

if(NOT TARGET sdk)
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
  set(SMTG_CREATE_BUNDLE_FOR_WINDOWS ON CACHE BOOL "" FORCE)
  set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
  set(SMTG_CREATE_MODULE_INFO ON CACHE BOOL "" FORCE)
  set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)
  set(SMTG_USE_STATIC_CRT ON CACHE BOOL "" FORCE)
  add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk" EXCLUDE_FROM_ALL)
endif()

smtg_enable_vst3_sdk()

set(dhrelink_sender_sources
  source/cids.hpp
  source/sender_controller.cpp
  source/sender_controller.hpp
  source/sender_editor.cpp
  source/sender_editor.hpp
  source/sender_entry.cpp
  source/sender_processor.cpp
  source/sender_processor.hpp
  resources/dhrelink_resources.rc
  resources/resource.h
  resources/fonts/Quintessential-Regular.ttf
  resources/fonts/Montserrat-Variable.ttf
  resources/fonts/Quintessential-OFL.txt
  resources/fonts/Montserrat-OFL.txt
)

set_source_files_properties(
  resources/fonts/Quintessential-Regular.ttf
  resources/fonts/Montserrat-Variable.ttf
  resources/fonts/Quintessential-OFL.txt
  resources/fonts/Montserrat-OFL.txt
  PROPERTIES HEADER_FILE_ONLY TRUE
)

smtg_add_vst3plugin(
  dhrelink_sender
  PACKAGE_NAME "dhreLink Sender"
  SOURCES_LIST ${dhrelink_sender_sources}
)
target_link_libraries(dhrelink_sender PRIVATE sdk dhreLink::core user32 gdi32)
target_compile_features(dhrelink_sender PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(dhrelink_sender PRIVATE /utf-8)
endif()

add_custom_command(
  TARGET dhrelink_sender
  POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory
    "$<TARGET_FILE_DIR:dhrelink_sender>/../Resources"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/Quintessential-OFL.txt"
    "$<TARGET_FILE_DIR:dhrelink_sender>/../Resources/Quintessential-OFL.txt"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/Montserrat-OFL.txt"
    "$<TARGET_FILE_DIR:dhrelink_sender>/../Resources/Montserrat-OFL.txt"
  VERBATIM
)

install(
  DIRECTORY "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/dhreLink Sender.vst3"
  DESTINATION vst3
  COMPONENT vst3
)
