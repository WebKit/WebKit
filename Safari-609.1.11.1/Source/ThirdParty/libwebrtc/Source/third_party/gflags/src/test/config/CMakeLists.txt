## gflags package configuration tests

cmake_minimum_required (VERSION 2.8)

project (gflags_${TEST_NAME})

find_package (gflags REQUIRED)

add_executable (foo main.cc)
target_link_libraries (foo gflags)
