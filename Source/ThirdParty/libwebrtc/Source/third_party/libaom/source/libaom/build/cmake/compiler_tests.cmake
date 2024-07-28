#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_COMPILER_TESTS_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_COMPILER_TESTS_CMAKE_
set(AOM_BUILD_CMAKE_COMPILER_TESTS_CMAKE_ 1)

include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

# CMake passes command line flags like this:
#
# * $compiler $lang_flags $lang_flags_config ...
#
# To ensure the flags tested here and elsewhere are obeyed a list of active
# build configuration types is built, and flags are applied to the flag strings
# for each configuration currently active for C and CXX builds as determined by
# reading $CMAKE_CONFIGURATION_TYPES and $CMAKE_BUILD_TYPE. When
# $CMAKE_CONFIGURATION_TYPES is non-empty a multi- configuration generator is in
# use: currently this includes MSVC and Xcode. For other generators
# $CMAKE_BUILD_TYPE is used. For both cases AOM_<LANG>_CONFIGS is populated with
# CMake string variable names that contain flags for the currently available
# configuration(s).
unset(AOM_C_CONFIGS)
unset(AOM_CXX_CONFIGS)
list(LENGTH CMAKE_CONFIGURATION_TYPES num_configs)
if(${num_configs} GREATER 0)
  foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config)
    list(APPEND AOM_C_CONFIGS "CMAKE_C_FLAGS_${config}")
    list(APPEND AOM_CXX_CONFIGS "CMAKE_CXX_FLAGS_${config}")
    list(APPEND AOM_EXE_LINKER_CONFIGS "CMAKE_EXE_LINKER_FLAGS_${config}")
  endforeach()
else()
  string(TOUPPER ${CMAKE_BUILD_TYPE} config)
  set(AOM_C_CONFIGS "CMAKE_C_FLAGS_${config}")
  set(AOM_CXX_CONFIGS "CMAKE_CXX_FLAGS_${config}")
  set(AOM_EXE_LINKER_CONFIGS "CMAKE_EXE_LINKER_FLAGS_${config}")
endif()

# The basic main() function used in all compile tests.
set(AOM_C_MAIN "\nint main(void) { return 0; }")
set(AOM_CXX_MAIN "\nint main() { return 0; }")

# Strings containing the names of passed and failed tests.
set(AOM_C_PASSED_TESTS)
set(AOM_C_FAILED_TESTS)
set(AOM_CXX_PASSED_TESTS)
set(AOM_CXX_FAILED_TESTS)

function(aom_push_var var new_value)
  set(SAVED_${var} ${${var}} PARENT_SCOPE)
  set(${var} "${${var}} ${new_value}" PARENT_SCOPE)
endfunction()

function(aom_pop_var var)
  set(var ${SAVED_${var}} PARENT_SCOPE)
  unset(SAVED_${var} PARENT_SCOPE)
endfunction()

# Confirms $test_source compiles and stores $test_name in one of
# $AOM_C_PASSED_TESTS or $AOM_C_FAILED_TESTS depending on out come. When the
# test passes $result_var is set to 1. When it fails $result_var is unset. The
# test is not run if the test name is found in either of the passed or failed
# test variables.
function(aom_check_c_compiles test_name test_source result_var)
  if(DEBUG_CMAKE_DISABLE_COMPILER_TESTS)
    return()
  endif()

  unset(C_TEST_PASSED CACHE)
  unset(C_TEST_FAILED CACHE)
  string(FIND "${AOM_C_PASSED_TESTS}" "${test_name}" C_TEST_PASSED)
  string(FIND "${AOM_C_FAILED_TESTS}" "${test_name}" C_TEST_FAILED)
  if(${C_TEST_PASSED} EQUAL -1 AND ${C_TEST_FAILED} EQUAL -1)
    unset(C_TEST_COMPILED CACHE)
    message("Running C compiler test: ${test_name}")
    check_c_source_compiles("${test_source} ${AOM_C_MAIN}" C_TEST_COMPILED)
    set(${result_var} ${C_TEST_COMPILED} PARENT_SCOPE)

    if(C_TEST_COMPILED)
      set(AOM_C_PASSED_TESTS
          "${AOM_C_PASSED_TESTS} ${test_name}"
          CACHE STRING "" FORCE)
    else()
      set(AOM_C_FAILED_TESTS
          "${AOM_C_FAILED_TESTS} ${test_name}"
          CACHE STRING "" FORCE)
      message("C Compiler test ${test_name} failed.")
    endif()
  elseif(NOT ${C_TEST_PASSED} EQUAL -1)
    set(${result_var} 1 PARENT_SCOPE)
  else() # ${C_TEST_FAILED} NOT EQUAL -1
    unset(${result_var} PARENT_SCOPE)
  endif()
endfunction()

# Confirms $test_source compiles and stores $test_name in one of
# $AOM_CXX_PASSED_TESTS or $AOM_CXX_FAILED_TESTS depending on out come. When the
# test passes $result_var is set to 1. When it fails $result_var is unset. The
# test is not run if the test name is found in either of the passed or failed
# test variables.
function(aom_check_cxx_compiles test_name test_source result_var)
  if(DEBUG_CMAKE_DISABLE_COMPILER_TESTS)
    return()
  endif()

  unset(CXX_TEST_PASSED CACHE)
  unset(CXX_TEST_FAILED CACHE)
  string(FIND "${AOM_CXX_PASSED_TESTS}" "${test_name}" CXX_TEST_PASSED)
  string(FIND "${AOM_CXX_FAILED_TESTS}" "${test_name}" CXX_TEST_FAILED)
  if(${CXX_TEST_PASSED} EQUAL -1 AND ${CXX_TEST_FAILED} EQUAL -1)
    unset(CXX_TEST_COMPILED CACHE)
    message("Running CXX compiler test: ${test_name}")
    check_cxx_source_compiles("${test_source} ${AOM_CXX_MAIN}"
                              CXX_TEST_COMPILED)
    set(${result_var} ${CXX_TEST_COMPILED} PARENT_SCOPE)

    if(CXX_TEST_COMPILED)
      set(AOM_CXX_PASSED_TESTS
          "${AOM_CXX_PASSED_TESTS} ${test_name}"
          CACHE STRING "" FORCE)
    else()
      set(AOM_CXX_FAILED_TESTS
          "${AOM_CXX_FAILED_TESTS} ${test_name}"
          CACHE STRING "" FORCE)
      message("CXX Compiler test ${test_name} failed.")
    endif()
  elseif(NOT ${CXX_TEST_PASSED} EQUAL -1)
    set(${result_var} 1 PARENT_SCOPE)
  else() # ${CXX_TEST_FAILED} NOT EQUAL -1
    unset(${result_var} PARENT_SCOPE)
  endif()
endfunction()

# Convenience function that confirms $test_source compiles as C and C++.
# $result_var is set to 1 when both tests are successful, and 0 when one or both
# tests fail. Note: This function is intended to be used to write to result
# variables that are expanded via configure_file(). $result_var is set to 1 or 0
# to allow direct usage of the value in generated source files.
function(aom_check_source_compiles test_name test_source result_var)
  unset(C_PASSED)
  unset(CXX_PASSED)
  aom_check_c_compiles(${test_name} ${test_source} C_PASSED)
  aom_check_cxx_compiles(${test_name} ${test_source} CXX_PASSED)
  if(C_PASSED AND CXX_PASSED)
    set(${result_var} 1 PARENT_SCOPE)
  else()
    set(${result_var} 0 PARENT_SCOPE)
  endif()
endfunction()

# When inline support is detected for the current compiler the supported
# inlining keyword is written to $result in caller scope.
function(aom_get_inline result)
  aom_check_source_compiles("inline_check_1"
                            "static inline void function(void) {}"
                            HAVE_INLINE_1)
  if(HAVE_INLINE_1 EQUAL 1)
    set(${result} "inline" PARENT_SCOPE)
    return()
  endif()

  # Check __inline.
  aom_check_source_compiles("inline_check_2"
                            "static __inline void function(void) {}"
                            HAVE_INLINE_2)
  if(HAVE_INLINE_2 EQUAL 1)
    set(${result} "__inline" PARENT_SCOPE)
  endif()
endfunction()
