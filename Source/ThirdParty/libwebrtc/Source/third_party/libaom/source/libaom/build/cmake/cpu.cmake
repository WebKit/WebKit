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

if("${AOM_TARGET_CPU}" STREQUAL "arm64")
  set(AOM_ARCH_ARM 1)
  set(AOM_ARCH_AARCH64 1)
  set(RTCD_ARCH_ARM "yes")

  set(ARM64_FLAVORS "NEON;ARM_CRC32;NEON_DOTPROD;NEON_I8MM;SVE")
  set(AOM_ARM_CRC32_DEFAULT_FLAG "-march=armv8-a+crc")
  set(AOM_NEON_DOTPROD_DEFAULT_FLAG "-march=armv8.2-a+dotprod")
  set(AOM_NEON_I8MM_DEFAULT_FLAG "-march=armv8.2-a+dotprod+i8mm")
  set(AOM_SVE_DEFAULT_FLAG "-march=armv8.2-a+dotprod+i8mm+sve")

  # Check that the compiler flag to enable each flavor is supported by the
  # compiler. This may not be the case for new architecture features on old
  # compiler versions.
  foreach(flavor ${ARM64_FLAVORS})
    if(ENABLE_${flavor} AND NOT DEFINED AOM_${flavor}_FLAG)
      set(AOM_${flavor}_FLAG "${AOM_${flavor}_DEFAULT_FLAG}")
      unset(FLAG_SUPPORTED)
      check_c_compiler_flag("${AOM_${flavor}_FLAG}" FLAG_SUPPORTED)
      if(NOT ${FLAG_SUPPORTED})
        set(ENABLE_${flavor} 0)
      endif()
    endif()
  endforeach()

  # SVE requires that the Neon-SVE bridge header is also available.
  if(ENABLE_SVE)
    set(OLD_CMAKE_REQURED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${AOM_SVE_FLAG}")
    aom_check_source_compiles("arm_neon_sve_bridge_available" "
#ifndef __ARM_NEON_SVE_BRIDGE
#error 1
#endif
#include <arm_sve.h>
#include <arm_neon_sve_bridge.h>" HAVE_SVE_HEADERS)
    set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQURED_FLAGS})
    if(HAVE_SVE_HEADERS EQUAL 0)
      set(ENABLE_SVE 0)
    endif()
  endif()

  foreach(flavor ${ARM64_FLAVORS})
    if(ENABLE_${flavor})
      set(HAVE_${flavor} 1)
      set(RTCD_HAVE_${flavor} "yes")
    else()
      set(HAVE_${flavor} 0)
      string(TOLOWER ${flavor} flavor)
      set(AOM_RTCD_FLAGS ${AOM_RTCD_FLAGS} --disable-${flavor})
    endif()
  endforeach()

elseif("${AOM_TARGET_CPU}" MATCHES "^arm")
  set(AOM_ARCH_ARM 1)
  set(RTCD_ARCH_ARM "yes")

  if(ENABLE_NEON)
    set(HAVE_NEON 1)
    set(RTCD_HAVE_NEON "yes")
  else()
    set(HAVE_NEON 0)
    set(AOM_RTCD_FLAGS ${AOM_RTCD_FLAGS} --disable-neon)
  endif()

elseif("${AOM_TARGET_CPU}" MATCHES "ppc")
  set(AOM_ARCH_PPC 1)
  set(RTCD_ARCH_PPC "yes")

  if(ENABLE_VSX)
    set(HAVE_VSX 1)
    set(RTCD_HAVE_VSX "yes")
  else()
    set(HAVE_VSX 0)
    set(AOM_RTCD_FLAGS ${AOM_RTCD_FLAGS} --disable-vsx)
  endif()
elseif("${AOM_TARGET_CPU}" MATCHES "^x86")
  if("${AOM_TARGET_CPU}" STREQUAL "x86")
    set(AOM_ARCH_X86 1)
    set(RTCD_ARCH_X86 "yes")
  elseif("${AOM_TARGET_CPU}" STREQUAL "x86_64")
    set(AOM_ARCH_X86_64 1)
    set(RTCD_ARCH_X86_64 "yes")
  endif()

  set(X86_FLAVORS "MMX;SSE;SSE2;SSE3;SSSE3;SSE4_1;SSE4_2;AVX;AVX2")
  foreach(flavor ${X86_FLAVORS})
    if(ENABLE_${flavor} AND NOT disable_remaining_flavors)
      set(HAVE_${flavor} 1)
      set(RTCD_HAVE_${flavor} "yes")
    else()
      set(disable_remaining_flavors 1)
      set(HAVE_${flavor} 0)
      string(TOLOWER ${flavor} flavor)
      set(AOM_RTCD_FLAGS ${AOM_RTCD_FLAGS} --disable-${flavor})
    endif()
  endforeach()
endif()
