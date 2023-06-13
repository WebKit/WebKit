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
if(AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_
set(AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Linux")

if("${CROSS}" STREQUAL "")

  # Default the cross compiler prefix to one used by Debian and other package
  # management systems.
  set(CROSS arm-linux-gnueabihf-)
endif()

if(NOT ${CROSS} MATCHES hf-$)
  set(AOM_EXTRA_TOOLCHAIN_FLAGS "-mfloat-abi=softfp")
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
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3 \
                          ${AOM_EXTRA_TOOLCHAIN_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=vfpv3 \
                            ${AOM_EXTRA_TOOLCHAIN_FLAGS}")
set(AOM_AS_FLAGS --defsym ARCHITECTURE=7 -march=armv7-a -mfpu=neon
                 ${AOM_EXTRA_TOOLCHAIN_FLAGS})
set(CMAKE_SYSTEM_PROCESSOR "armv7")

set(AOM_NEON_INTRIN_FLAG "-mfpu=neon ${AOM_EXTRA_TOOLCHAIN_FLAGS}")

# No runtime cpu detect for armv7-linux-gcc.
set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE STRING "")
