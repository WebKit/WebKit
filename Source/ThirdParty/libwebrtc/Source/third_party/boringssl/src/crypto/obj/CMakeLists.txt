include_directories(../../include)

add_library(
  obj

  OBJECT

  obj.c
  obj_xref.c
)

add_executable(
  obj_test

  obj_test.cc

  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(obj_test crypto)
add_dependencies(all_tests obj_test)
