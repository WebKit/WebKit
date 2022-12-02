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
if(AOM_BUILD_CMAKE_TOOLCHAINS_ARM_IOS_COMMON_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_ARM_IOS_COMMON_CMAKE_
set(AOM_BUILD_CMAKE_ARM_IOS_COMMON_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Darwin")
set(CMAKE_OSX_SYSROOT iphoneos)
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_FLAGS_INIT "-arch ${CMAKE_SYSTEM_PROCESSOR}")
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS_INIT "-arch ${CMAKE_SYSTEM_PROCESSOR}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-arch ${CMAKE_SYSTEM_PROCESSOR}")

# No runtime cpu detect for arm*-ios targets.
set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE STRING "")

# TODO(tomfinegan): Handle bit code embedding.
