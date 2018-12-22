add_library(
  test_support

  OBJECT

  file_test.cc
  malloc.cc
  test_util.cc
  wycheproof_util.cc
)

add_dependencies(test_support global_target)

add_library(
  boringssl_gtest_main

  OBJECT

  gtest_main.cc
)

add_dependencies(boringssl_gtest_main global_target)
