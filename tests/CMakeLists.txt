add_executable(dhrelink_transport_test transport_test.cpp)
target_link_libraries(dhrelink_transport_test PRIVATE dhreLink::core)

if(MSVC)
  target_compile_options(dhrelink_transport_test PRIVATE /W4 /permissive-)
endif()

add_test(NAME dhrelink_transport COMMAND dhrelink_transport_test)

if(WIN32 AND TARGET dhrelink_obs)
  if(NOT EXISTS "${OBS_RUNTIME_DLL}")
    message(FATAL_ERROR "OBS module smoke test requires -DOBS_RUNTIME_DLL=<path-to-obs.dll>")
  endif()

  add_executable(dhrelink_obs_module_test obs_module_test.cpp)
  target_compile_definitions(dhrelink_obs_module_test PRIVATE UNICODE _UNICODE)
  if(MSVC)
    target_compile_options(dhrelink_obs_module_test PRIVATE /W4 /permissive-)
  endif()
  add_test(
    NAME dhrelink_obs_module
    COMMAND dhrelink_obs_module_test
      "${OBS_RUNTIME_DLL}"
      "$<TARGET_FILE:dhrelink_obs>"
      "${CMAKE_SOURCE_DIR}/src/obs/data"
  )
endif()

if(WIN32 AND TARGET dhrelink_sender)
  add_executable(
    dhrelink_vst_editor_test
    vst_editor_test.cpp
    "${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
  )
  set_target_properties(dhrelink_vst_editor_test PROPERTIES CXX_STANDARD 17)
  target_link_libraries(dhrelink_vst_editor_test PRIVATE sdk_hosting user32)
  if(MSVC)
    target_compile_options(dhrelink_vst_editor_test PRIVATE /W4 /permissive-)
  endif()
  add_test(
    NAME dhrelink_vst_editor
    COMMAND dhrelink_vst_editor_test
      "$<TARGET_FILE_DIR:dhrelink_sender>/../.."
  )
endif()
