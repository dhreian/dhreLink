set(VST3_SDK_ROOT "" CACHE PATH "Path to the Steinberg VST3 SDK root")

if(NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "DHRELINK_BUILD_VST3 requires -DVST3_SDK_ROOT=<path-to-vst3sdk>")
endif()

if(APPLE)
  enable_language(OBJCXX)
endif()

if(NOT TARGET sdk)
  set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
  set(SMTG_CREATE_BUNDLE_FOR_WINDOWS ON CACHE BOOL "" FORCE)
  set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
  set(SMTG_CREATE_MODULE_INFO ON CACHE BOOL "" FORCE)
  if(APPLE)
    set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
  else()
    set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)
  endif()
  set(SMTG_USE_STATIC_CRT ON CACHE BOOL "" FORCE)
  add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk" EXCLUDE_FROM_ALL)
endif()

smtg_enable_vst3_sdk()

set(dhrelink_sender_sources
  source/cids.hpp
  source/sender_controller.cpp
  source/sender_controller.hpp
  source/sender_editor.hpp
  source/sender_entry.cpp
  source/sender_processor.cpp
  source/sender_processor.hpp
  resources/fonts/Quintessential-Regular.ttf
  resources/fonts/Montserrat-Regular.ttf
  resources/fonts/Montserrat-Bold.ttf
  resources/fonts/Quintessential-OFL.txt
  resources/fonts/Montserrat-OFL.txt
)

if(WIN32)
  list(APPEND dhrelink_sender_sources
    source/sender_editor.cpp
    resources/dhrelink_resources.rc
    resources/resource.h
    "${PROJECT_SOURCE_DIR}/assets/brand/dhreian-mark-purple.ico")
elseif(APPLE)
  list(APPEND dhrelink_sender_sources source/sender_editor_mac.mm)
endif()

set_source_files_properties(
  resources/fonts/Quintessential-Regular.ttf
  resources/fonts/Montserrat-Regular.ttf
  resources/fonts/Montserrat-Bold.ttf
  resources/fonts/Quintessential-OFL.txt
  resources/fonts/Montserrat-OFL.txt
  PROPERTIES HEADER_FILE_ONLY TRUE
)

if(WIN32)
  set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/assets/brand/dhreian-mark-purple.ico"
    PROPERTIES HEADER_FILE_ONLY TRUE)
  set(SMTG_PACKAGE_ICON_PATH
    "${PROJECT_SOURCE_DIR}/assets/brand/dhreian-mark-purple.ico")
endif()
smtg_add_vst3plugin(
  dhrelink_sender
  PACKAGE_NAME "dhreLink Sender"
  SOURCES_LIST ${dhrelink_sender_sources}
)
target_link_libraries(dhrelink_sender PRIVATE sdk dhreLink::core)
if(WIN32)
  target_link_libraries(dhrelink_sender PRIVATE user32 gdi32)
elseif(APPLE)
  target_link_libraries(dhrelink_sender PRIVATE "-framework AppKit")
  set_target_properties(dhrelink_sender PROPERTIES
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${DHRELINK_PRODUCT_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${DHRELINK_PRODUCT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${DHRELINK_PRODUCT_VERSION}")
  set_source_files_properties(source/sender_editor_mac.mm PROPERTIES
    COMPILE_FLAGS "-fno-objc-arc")
endif()
target_compile_features(dhrelink_sender PRIVATE cxx_std_20)
target_compile_definitions(dhrelink_sender PRIVATE
  "DHRELINK_VERSION=\"${DHRELINK_PRODUCT_VERSION}\"")
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
