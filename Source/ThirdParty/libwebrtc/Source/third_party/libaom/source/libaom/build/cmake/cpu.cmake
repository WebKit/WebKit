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
elseif("${AOM_TARGET_CPU}" MATCHES "^mips")
  set(ARCH_MIPS 1)
  set(RTCD_ARCH_MIPS "yes")

  if("${AOM_TARGET_CPU}" STREQUAL "mips32")
    set(HAVE_MIPS32 1)
    set(RTCD_HAVE_MIPS32 "yes")
  elseif("${AOM_TARGET_CPU}" STREQUAL "mips64")
    set(HAVE_MIPS64 1)
    set(RTCD_HAVE_MIPS64 "yes")
  endif()

  # HAVE_DSPR2 is set by mips toolchain files.
  if(ENABLE_DSPR2 AND HAVE_DSPR2)
    set(RTCD_HAVE_DSPR2 "yes")
  else()
    set(HAVE_DSPR2 0)
    set(AOM_RTCD_FLAGS ${AOM_RTCD_FLAGS} --disable-dspr2)
  endif()

  # HAVE_MSA is set by mips toolchain files.
  if(ENABLE_MSA AND HAVE_MSA)
    set(RTCD_HAVE_MSA "yes")
  else()
    set(HAVE_MSA 0)
    set(AOM_RTCD_FLAGS ${AOM_RTCD_FLAGS} --disable-msa)
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
