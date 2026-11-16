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
if(AOM_BUILD_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_
set(AOM_BUILD_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Linux")

if("${CROSS}" STREQUAL "")

  # Default the cross compiler prefix to one used by Debian and other package
  # management systems.
  set(CROSS aarch64-linux-gnu-)
endif()

if(NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER ${CROSS}gcc)
endif()
if(NOT CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER ${CROSS}g++)
endif()
if(NOT CMAKE_ASM_COMPILER)
  set(CMAKE_ASM_COMPILER ${CROSS}as)
endif()
set(CMAKE_C_FLAGS_INIT "-march=armv8-a")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a")
set(AOM_AS_FLAGS "-march=armv8-a")
set(CMAKE_SYSTEM_PROCESSOR "arm64")

# No intrinsics flag required for arm64-linux-gcc.
set(AOM_NEON_INTRIN_FLAG "")
