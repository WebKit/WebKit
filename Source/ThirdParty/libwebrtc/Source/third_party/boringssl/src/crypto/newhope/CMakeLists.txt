add_library(
  newhope

  OBJECT

  error_correction.c
  newhope.c
  ntt.c
  poly.c
  precomp.c
  reduce.c
)

add_executable(
  newhope_test

  newhope_test.cc
  $<TARGET_OBJECTS:test_support>
)

add_executable(
  newhope_statistical_test

  newhope_statistical_test.cc
  $<TARGET_OBJECTS:test_support>
)

add_executable(
  newhope_vectors_test

  newhope_vectors_test.cc
  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(newhope_test crypto)
target_link_libraries(newhope_statistical_test crypto)
target_link_libraries(newhope_vectors_test crypto)
add_dependencies(all_tests newhope_test)
add_dependencies(all_tests newhope_statistical_test)
add_dependencies(all_tests newhope_vectors_test)
