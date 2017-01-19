include_directories(../../include)

add_library(
  ecdh

  OBJECT

  ecdh.c
)

add_executable(
  ecdh_test

  ecdh_test.cc

  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(ecdh_test crypto)
add_dependencies(all_tests ecdh_test)
