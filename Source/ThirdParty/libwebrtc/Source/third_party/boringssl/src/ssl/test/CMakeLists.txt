include_directories(../../include)

add_executable(
  bssl_shim

  async_bio.cc
  bssl_shim.cc
  handshake_util.cc
  packeted_bio.cc
  settings_writer.cc
  test_config.cc
  test_state.cc

  $<TARGET_OBJECTS:test_support>
)

add_dependencies(bssl_shim global_target)

target_link_libraries(bssl_shim ssl crypto)

if(UNIX AND NOT APPLE AND NOT ANDROID)
  add_executable(
    handshaker

    async_bio.cc
    handshake_util.cc
    handshaker.cc
    packeted_bio.cc
    settings_writer.cc
    test_config.cc
    test_state.cc

    $<TARGET_OBJECTS:test_support>
  )

  add_dependencies(handshaker global_target)

  target_link_libraries(handshaker ssl crypto)
else()
  # Declare a dummy target for run_tests to depend on.
  add_custom_target(handshaker)
endif()
