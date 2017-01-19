include_directories(../../include)

add_library(
  ripemd_decrepit

  OBJECT

  ripemd.c
)

add_executable(
  ripemd_test

  ripemd_test.cc

  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(ripemd_test crypto)
target_link_libraries(ripemd_test decrepit)
add_dependencies(all_tests ripemd_test)
