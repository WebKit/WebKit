#
# Copyright (c) 2018, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_MINGW_GCC_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_MINGW_GCC_CMAKE_
set(AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_MINGW_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_PROCESSOR "armv7")
set(CMAKE_SYSTEM_NAME "Windows")

if("${CROSS}" STREQUAL "")

  # Default the cross compiler prefix to one used by MSYS2.
  set(CROSS armv7-w64-mingw32-)
endif()

if(NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER ${CROSS}gcc)
endif()
if(NOT CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER ${CROSS}g++)
endif()
if(NOT CMAKE_AR)
  set(CMAKE_AR ${CROSS}ar CACHE FILEPATH Archiver)
endif()
if(NOT CMAKE_RANLIB)
  set(CMAKE_RANLIB ${CROSS}ranlib CACHE FILEPATH Indexer)
endif()

# No runtime cpu detect for armv7-mingw-gcc.
set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE STRING "")
