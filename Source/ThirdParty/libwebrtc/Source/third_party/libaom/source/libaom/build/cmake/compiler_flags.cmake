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
if(AOM_BUILD_CMAKE_COMPILER_FLAGS_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_COMPILER_FLAGS_CMAKE_
set(AOM_BUILD_CMAKE_COMPILER_FLAGS_CMAKE_ 1)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include("${AOM_ROOT}/build/cmake/compiler_tests.cmake")

# Strings used to cache flags.
set(AOM_C_FLAGS)
set(AOM_CXX_FLAGS)
set(AOM_EXE_LINKER_FLAGS)
set(AOM_FAILED_C_FLAGS)
set(AOM_FAILED_CXX_FLAGS)

# Sets variable named by $out_is_present to YES in the caller's scope when $flag
# is found in the string variable named by $flag_cache. Sets the var to NO
# otherwise.
function(is_flag_present flag_cache flag out_is_present)
  string(FIND "${${flag_cache}}" "${flag}" flag_pos)
  if(${flag_pos} EQUAL -1)
    set(${out_is_present} NO PARENT_SCOPE)
  else()
    set(${out_is_present} YES PARENT_SCOPE)
  endif()
endfunction()

# Appends $flag to $flags. Ignores scope via use of FORCE with set() call.
function(append_flag flags flag)
  string(FIND "${${flags}}" "${flag}" found)
  if(${found} EQUAL -1)
    set(${flags} "${${flags}} ${flag}" CACHE STRING "" FORCE)
  endif()
endfunction()

# Checks C compiler for support of $c_flag. Adds $c_flag to all
# $CMAKE_C_FLAGS_<CONFIG>s stored in AOM_C_CONFIGS when the compile test passes.
# Caches $c_flag in $AOM_C_FLAGS or $AOM_FAILED_C_FLAGS depending on test
# outcome.
function(add_c_flag_if_supported c_flag)
  if(DEBUG_CMAKE_DISABLE_COMPILER_TESTS)
    return()
  endif()

  is_flag_present(AOM_C_FLAGS "${c_flag}" flag_ok)
  is_flag_present(AOM_FAILED_C_FLAGS "${c_flag}" flag_failed)
  if(${flag_ok} OR ${flag_failed})
    return()
  endif()

  # Between 3.17.0 and 3.18.2 check_c_compiler_flag() sets a normal variable at
  # parent scope while check_cxx_source_compiles() continues to set an internal
  # cache variable, so we unset both to avoid the failure / success state
  # persisting between checks. See
  # https://gitlab.kitware.com/cmake/cmake/-/issues/21207.
  unset(C_FLAG_SUPPORTED)
  unset(C_FLAG_SUPPORTED CACHE)
  message("Checking C compiler flag support for: " ${c_flag})
  check_c_compiler_flag("${c_flag}" C_FLAG_SUPPORTED)

  if(${C_FLAG_SUPPORTED})
    append_flag(AOM_C_FLAGS "${c_flag}")
    foreach(config ${AOM_C_CONFIGS})
      unset(C_FLAG_FOUND)
      append_flag("${config}" "${c_flag}")
    endforeach()
  else()
    append_flag(AOM_FAILED_C_FLAGS "${c_flag}")
  endif()
endfunction()

# Checks C++ compiler for support of $cxx_flag. Adds $cxx_flag to all
# $CMAKE_CXX_FLAGS_<CONFIG>s stored in AOM_CXX_CONFIGS when the compile test
# passes. Caches $cxx_flag in $AOM_CXX_FLAGS or $AOM_FAILED_CXX_FLAGS depending
# on test outcome.
function(add_cxx_flag_if_supported cxx_flag)
  if(DEBUG_CMAKE_DISABLE_COMPILER_TESTS)
    return()
  endif()

  is_flag_present(AOM_CXX_FLAGS "${cxx_flag}" flag_ok)
  is_flag_present(AOM_FAILED_CXX_FLAGS "${cxx_flag}" flag_failed)
  if(${flag_ok} OR ${flag_failed})
    return()
  endif()

  # Between 3.17.0 and 3.18.2 check_cxx_compiler_flag() sets a normal variable
  # at parent scope while check_cxx_source_compiles() continues to set an
  # internal cache variable, so we unset both to avoid the failure / success
  # state persisting between checks. See
  # https://gitlab.kitware.com/cmake/cmake/-/issues/21207.
  unset(CXX_FLAG_SUPPORTED)
  unset(CXX_FLAG_SUPPORTED CACHE)
  message("Checking C++ compiler flag support for: " ${cxx_flag})
  check_cxx_compiler_flag("${cxx_flag}" CXX_FLAG_SUPPORTED)

  if(${CXX_FLAG_SUPPORTED})
    append_flag(AOM_CXX_FLAGS "${cxx_flag}")
    foreach(config ${AOM_CXX_CONFIGS})
      unset(CXX_FLAG_FOUND)
      append_flag("${config}" "${cxx_flag}")
    endforeach()
  else()
    append_flag(AOM_FAILED_CXX_FLAGS "${cxx_flag}")
  endif()
endfunction()

# Convenience method for adding a flag to both the C and C++ compiler command
# lines.
function(add_compiler_flag_if_supported flag)
  add_c_flag_if_supported(${flag})
  add_cxx_flag_if_supported(${flag})
endfunction()

# Checks C compiler for support of $c_flag and terminates generation when
# support is not present.
function(require_c_flag c_flag update_c_flags)
  if(DEBUG_CMAKE_DISABLE_COMPILER_TESTS)
    return()
  endif()

  is_flag_present(AOM_C_FLAGS "${c_flag}" flag_ok)
  if(${flag_ok})
    return()
  endif()

  if(NOT "${AOM_EXE_LINKER_FLAGS}" STREQUAL "")
    aom_push_var(CMAKE_EXE_LINKER_FLAGS "${AOM_EXE_LINKER_FLAGS}")
  endif()

  unset(HAVE_C_FLAG CACHE)
  message("Checking C compiler flag support for: " ${c_flag})
  check_c_compiler_flag("${c_flag}" HAVE_C_FLAG)
  if(NOT HAVE_C_FLAG)
    message(
      FATAL_ERROR "${PROJECT_NAME} requires support for C flag: ${c_flag}.")
  endif()

  if(NOT "${AOM_EXE_LINKER_FLAGS}" STREQUAL "")
    aom_pop_var(CMAKE_EXE_LINKER_FLAGS)
  endif()

  append_flag(AOM_C_FLAGS "${c_flag}")
  if(update_c_flags)
    foreach(config ${AOM_C_CONFIGS})
      set(${config} "${${config}} ${c_flag}" CACHE STRING "" FORCE)
    endforeach()
  endif()
endfunction()

# Checks CXX compiler for support of $cxx_flag and terminates generation when
# support is not present.
function(require_cxx_flag cxx_flag update_cxx_flags)
  if(DEBUG_CMAKE_DISABLE_COMPILER_TESTS)
    return()
  endif()

  is_flag_present(AOM_CXX_FLAGS "${cxx_flag}" flag_ok)
  if(${flag_ok})
    return()
  endif()

  if(NOT "${AOM_EXE_LINKER_FLAGS}" STREQUAL "")
    aom_push_var(CMAKE_EXE_LINKER_FLAGS "${AOM_EXE_LINKER_FLAGS}")
  endif()

  unset(HAVE_CXX_FLAG CACHE)
  message("Checking C++ compiler flag support for: " ${cxx_flag})
  check_cxx_compiler_flag("${cxx_flag}" HAVE_CXX_FLAG)
  if(NOT HAVE_CXX_FLAG)
    message(
      FATAL_ERROR "${PROJECT_NAME} requires support for C++ flag: ${cxx_flag}.")
  endif()

  if(NOT "${AOM_EXE_LINKER_FLAGS}" STREQUAL "")
    aom_pop_var(CMAKE_EXE_LINKER_FLAGS)
  endif()

  append_flag(AOM_CXX_FLAGS "${cxx_flag}")
  if(update_cxx_flags)
    foreach(config ${AOM_CXX_CONFIGS})
      set(${config} "${${config}} ${cxx_flag}" CACHE STRING "" FORCE)
    endforeach()
  endif()
endfunction()

# Checks for support of $flag by both the C and CXX compilers. Terminates
# generation when support is not present in both compilers.
function(require_compiler_flag flag update_cmake_flags)
  require_c_flag(${flag} ${update_cmake_flags})
  require_cxx_flag(${flag} ${update_cmake_flags})
endfunction()

# Checks only non-MSVC targets for support of $c_flag and terminates generation
# when support is not present.
function(require_c_flag_nomsvc c_flag update_c_flags)
  if(NOT MSVC)
    require_c_flag(${c_flag} ${update_c_flags})
  endif()
endfunction()

# Checks only non-MSVC targets for support of $cxx_flag and terminates
# generation when support is not present.
function(require_cxx_flag_nomsvc cxx_flag update_cxx_flags)
  if(NOT MSVC)
    require_cxx_flag(${cxx_flag} ${update_cxx_flags})
  endif()
endfunction()

# Checks only non-MSVC targets for support of $flag by both the C and CXX
# compilers. Terminates generation when support is not present in both
# compilers.
function(require_compiler_flag_nomsvc flag update_cmake_flags)
  require_c_flag_nomsvc(${flag} ${update_cmake_flags})
  require_cxx_flag_nomsvc(${flag} ${update_cmake_flags})
endfunction()

# Adds $preproc_def to C compiler command line (as -D$preproc_def) if not
# already present.
function(add_c_preproc_definition preproc_def)
  set(preproc_def "-D${preproc_def}")
  is_flag_present(AOM_C_FLAGS "${preproc_def}" flag_cached)
  if(${flag_cached})
    return()
  endif()

  foreach(config ${AOM_C_CONFIGS})
    set(${config} "${${config}} ${preproc_def}" CACHE STRING "" FORCE)
  endforeach()
endfunction()

# Adds $preproc_def to CXX compiler command line (as -D$preproc_def) if not
# already present.
function(add_cxx_preproc_definition preproc_def)
  set(preproc_def "-D${preproc_def}")
  is_flag_present(AOM_CXX_FLAGS "${preproc_def}" flag_cached)
  if(${flag_cached})
    return()
  endif()

  foreach(config ${AOM_CXX_CONFIGS})
    set(${config} "${${config}} ${preproc_def}" CACHE STRING "" FORCE)
  endforeach()
endfunction()

# Adds $preproc_def to C and CXX compiler command line (as -D$preproc_def) if
# not already present.
function(add_preproc_definition preproc_def)
  add_c_preproc_definition(${preproc_def})
  add_cxx_preproc_definition(${preproc_def})
endfunction()

# Adds $flag to assembler command line.
function(append_as_flag flag)
  is_flag_present(AOM_AS_FLAGS "${flag}" flag_cached)
  if(${flag_cached})
    return()
  endif()
  append_flag(AOM_AS_FLAGS "${flag}")
endfunction()

# Adds $flag to the C compiler command line.
function(append_c_flag flag)
  is_flag_present(AOM_C_FLAGS "${flag}" flag_cached)
  if(${flag_cached})
    return()
  endif()

  foreach(config ${AOM_C_CONFIGS})
    append_flag(${config} "${flag}")
  endforeach()
endfunction()

# Adds $flag to the CXX compiler command line.
function(append_cxx_flag flag)
  is_flag_present(AOM_CXX_FLAGS "${flag}" flag_cached)
  if(${flag_cached})
    return()
  endif()

  foreach(config ${AOM_CXX_CONFIGS})
    append_flag(${config} "${flag}")
  endforeach()
endfunction()

# Adds $flag to the C and CXX compiler command lines.
function(append_compiler_flag flag)
  append_c_flag(${flag})
  append_cxx_flag(${flag})
endfunction()

# Adds $flag to the executable linker command line when not present.
function(append_exe_linker_flag flag)
  is_flag_present(AOM_EXE_LINKER_FLAGS "${flag}" flag_cached)
  if(${flag_cached})
    return()
  endif()

  append_flag(AOM_EXE_LINKER_FLAGS "${flag}")
  foreach(config ${AOM_EXE_LINKER_CONFIGS})
    append_flag(${config} "${flag}")
  endforeach()
endfunction()

# Adds $flag to the link flags for $target.
function(append_link_flag_to_target target flag)
  unset(target_link_flags)
  get_target_property(target_link_flags ${target} LINK_FLAGS)

  if(target_link_flags)
    is_flag_present(target_link_flags "${flag}" flag_found)
    if(${flag_found})
      return()
    endif()
    set(target_link_flags "${target_link_flags} ${flag}")
  else()
    set(target_link_flags "${flag}")
  endif()

  set_target_properties(${target} PROPERTIES LINK_FLAGS ${target_link_flags})
endfunction()

# Adds $flag to executable linker flags, and makes sure C/CXX builds still work.
function(require_linker_flag flag)
  if(DEBUG_CMAKE_DISABLE_COMPILER_TESTS)
    return()
  endif()

  append_exe_linker_flag(${flag})

  unset(c_passed)
  aom_check_c_compiles("LINKER_FLAG_C_TEST(${flag})" "" c_passed)
  unset(cxx_passed)
  aom_check_cxx_compiles("LINKER_FLAG_CXX_TEST(${flag})" "" cxx_passed)

  if(NOT c_passed OR NOT cxx_passed)
    message(FATAL_ERROR "Linker flag test for ${flag} failed.")
  endif()
endfunction()

# Appends flags in $AOM_EXTRA_<TYPE>_FLAGS variables to the flags used at build
# time.
function(set_user_flags)

  # Linker flags are handled first because some C/CXX flags require that a
  # linker flag is present at link time.
  if(AOM_EXTRA_EXE_LINKER_FLAGS)
    is_flag_present(AOM_EXE_LINKER_FLAGS "${AOM_EXTRA_EXE_LINKER_FLAGS}"
                    extra_present)
    if(NOT ${extra_present})
      require_linker_flag("${AOM_EXTRA_EXE_LINKER_FLAGS}")
    endif()
  endif()
  if(AOM_EXTRA_AS_FLAGS)

    # TODO(tomfinegan): assembler flag testing would be a good thing to have.
    is_flag_present(AOM_AS_FLAGS "${AOM_EXTRA_AS_FLAGS}" extra_present)
    if(NOT ${extra_present})
      append_flag(AOM_AS_FLAGS "${AOM_EXTRA_AS_FLAGS}")
    endif()
  endif()
  if(AOM_EXTRA_C_FLAGS)
    is_flag_present(AOM_C_FLAGS "${AOM_EXTRA_C_FLAGS}" extra_present)
    if(NOT ${extra_present})
      require_c_flag("${AOM_EXTRA_C_FLAGS}" YES)
    endif()
  endif()
  if(AOM_EXTRA_CXX_FLAGS)
    is_flag_present(AOM_CXX_FLAGS "${AOM_EXTRA_CXX_FLAGS}" extra_present)
    if(NOT ${extra_present})
      require_cxx_flag("${AOM_EXTRA_CXX_FLAGS}" YES)
    endif()
  endif()
endfunction()
