#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_TOOLCHAINS_X86_64_MINGW_GCC_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_X86_64_MINGW_GCC_CMAKE_
set(AOM_BUILD_CMAKE_TOOLCHAINS_X86_64_MINGW_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_PROCESSOR "x86_64")
set(CMAKE_SYSTEM_NAME "Windows")

if("${CROSS}" STREQUAL "")
  set(CROSS x86_64-w64-mingw32-)
endif()

set(CMAKE_C_COMPILER ${CROSS}gcc)
set(CMAKE_CXX_COMPILER ${CROSS}g++)
set(CMAKE_AR ${CROSS}ar CACHE FILEPATH Archiver)
set(CMAKE_RANLIB ${CROSS}ranlib CACHE FILEPATH Indexer)
