/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AOM_PORTS_BITOPS_H_
#define AOM_AOM_PORTS_BITOPS_H_

#include <assert.h>

#include "aom_ports/msvc.h"
#include "config/aom_config.h"

#ifdef _MSC_VER
#if defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#define USE_MSC_INTRINSICS
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// get_msb:
// Returns (int)floor(log2(n)). n must be > 0.
// These versions of get_msb() are only valid when n != 0 because all
// of the optimized versions are undefined when n == 0:

// GCC compiler: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
// MSVC: https://learn.microsoft.com/en-us/cpp/intrinsics/compiler-intrinsics

// use GNU builtins where available.
#if defined(__GNUC__) && \
    ((__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || __GNUC__ >= 4)
static INLINE int get_msb(unsigned int n) {
  assert(n != 0);
  return 31 ^ __builtin_clz(n);
}
#elif defined(USE_MSC_INTRINSICS)
#pragma intrinsic(_BitScanReverse)

static INLINE int get_msb(unsigned int n) {
  unsigned long first_set_bit;
  assert(n != 0);
  _BitScanReverse(&first_set_bit, n);
  return first_set_bit;
}
#undef USE_MSC_INTRINSICS
#else
static INLINE int get_msb(unsigned int n) {
  int log = 0;
  unsigned int value = n;

  assert(n != 0);

  for (int shift = 16; shift != 0; shift >>= 1) {
    const unsigned int x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  return log;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AOM_PORTS_BITOPS_H_
