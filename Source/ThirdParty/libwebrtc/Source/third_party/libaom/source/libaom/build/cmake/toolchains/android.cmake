#
# Copyright (c) 2019, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_TOOLCHAINS_ANDROID_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_ANDROID_CMAKE_
set(AOM_BUILD_CMAKE_TOOLCHAINS_ANDROID_CMAKE_ 1)

if(NOT ANDROID_PLATFORM)
  set(ANDROID_PLATFORM android-24)
endif()

# Choose target architecture with:
#
# -DANDROID_ABI={armeabi-v7a,armeabi-v7a with NEON,arm64-v8a,x86,x86_64}
if(NOT ANDROID_ABI)
  set(ANDROID_ABI arm64-v8a)
endif()

# Toolchain files don't have access to cached variables:
# https://gitlab.kitware.com/cmake/cmake/issues/16170. Set an intermediate
# environment variable when loaded the first time.
if(AOM_ANDROID_NDK_PATH)
  set(ENV{_AOM_ANDROID_NDK_PATH} "${AOM_ANDROID_NDK_PATH}")
else()
  set(AOM_ANDROID_NDK_PATH "$ENV{_AOM_ANDROID_NDK_PATH}")
endif()

if("${AOM_ANDROID_NDK_PATH}" STREQUAL "")
  message(FATAL_ERROR "AOM_ANDROID_NDK_PATH not set.")
  return()
endif()

include("${AOM_ANDROID_NDK_PATH}/build/cmake/android.toolchain.cmake")

if(ANDROID_ABI MATCHES "^armeabi")
  set(AOM_NEON_INTRIN_FLAG "-mfpu=neon")
endif()

if(ANDROID_ABI MATCHES "^arm")
  set(CMAKE_ASM_COMPILER as)
elseif(ANDROID_ABI MATCHES "^x86")
  set(CMAKE_ASM_NASM_COMPILER yasm)
endif()

set(CMAKE_SYSTEM_NAME "Android")
