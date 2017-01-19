include_directories(../../include)

add_library(
  pool

  OBJECT

  pool.c
)

add_executable(
  pool_test

  pool_test.cc

  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(pool_test crypto)
add_dependencies(all_tests pool_test)
