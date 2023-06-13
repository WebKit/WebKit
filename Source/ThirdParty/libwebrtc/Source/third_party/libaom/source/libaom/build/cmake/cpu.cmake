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

if("${AOM_TARGET_CPU}" MATCHES "^arm")
  set(ARCH_ARM 1)
  set(RTCD_ARCH_ARM "yes")

  if(ENABLE_NEON)
    set(HAVE_NEON 1)
    set(RTCD_HAVE_NEON "yes")
  else()
    set(HAVE_NEON 0)
    set(AOM_RTCD_FLAGS ${AOM_RTCD_FLAGS} --disable-neon)
  endif()

  check_c_source_compiles("
    #if !defined(__ARM_FEATURE_CRC32) || __ARM_FEATURE_CRC32 != 1
    #error \"CRC32 is unavailable.\"
    #endif
    int main(void) { return 0; }" HAVE_CRC32)
  if(HAVE_CRC32)
    set(HAVE_ARM_CRC32 1)
  else()
    set(HAVE_ARM_CRC32 0)
    set(AOM_RTCD_FLAGS ${AOM_RTCD_FLAGS} --disable-arm_crc32)
  endif()

elseif("${AOM_TARGET_CPU}" MATCHES "ppc")
  set(ARCH_PPC 1)
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
    set(ARCH_X86 1)
    set(RTCD_ARCH_X86 "yes")
  elseif("${AOM_TARGET_CPU}" STREQUAL "x86_64")
    set(ARCH_X86_64 1)
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
