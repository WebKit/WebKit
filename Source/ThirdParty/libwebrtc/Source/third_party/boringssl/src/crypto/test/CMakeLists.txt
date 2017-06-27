add_library(
  test_support

  OBJECT

  file_test.cc
  malloc.cc
  test_util.cc
)

add_library(
  gtest_main

  OBJECT

  gtest_main.cc
)
