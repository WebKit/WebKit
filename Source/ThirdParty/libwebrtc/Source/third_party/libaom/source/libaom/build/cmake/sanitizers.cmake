#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_SANITIZERS_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_SANITIZERS_CMAKE_
set(AOM_BUILD_CMAKE_SANITIZERS_CMAKE_ 1)

if(MSVC OR NOT SANITIZE)
  return()
endif()

include("${AOM_ROOT}/build/cmake/compiler_flags.cmake")

string(TOLOWER ${SANITIZE} SANITIZE)

# Require the sanitizer requested. cfi sanitizer requires all the flags in order
# for the compiler to accept it.
if("${SANITIZE}" MATCHES "cfi" AND CMAKE_C_COMPILER_ID MATCHES "Clang")
  require_linker_flag("-fsanitize=${SANITIZE} -flto -fno-sanitize-trap=cfi \
    -fuse-ld=gold" YES)
  require_compiler_flag("-fsanitize=${SANITIZE} -flto -fvisibility=hidden \
    -fno-sanitize-trap=cfi" YES)
else()
  require_linker_flag("-fsanitize=${SANITIZE}")
  require_compiler_flag("-fsanitize=${SANITIZE}" YES)
  if("${SANITIZE}" MATCHES "integer|undefined")
    require_compiler_flag("-fsanitize=float-cast-overflow" YES)
  endif()
endif()

# Make callstacks accurate.
require_compiler_flag("-fno-omit-frame-pointer -fno-optimize-sibling-calls" YES)

# Fix link errors due to missing rt compiler lib in 32-bit builds.
# http://llvm.org/bugs/show_bug.cgi?id=17693
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 4
     AND "${SANITIZE}" MATCHES "integer|undefined")
    require_linker_flag("--rtlib=compiler-rt -lgcc_s")
  endif()
endif()
