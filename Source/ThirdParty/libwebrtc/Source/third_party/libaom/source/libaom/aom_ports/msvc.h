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

#ifndef AOM_AOM_PORTS_MSVC_H_
#define AOM_AOM_PORTS_MSVC_H_
#ifdef _MSC_VER

#include "config/aom_config.h"

#if _MSC_VER < 1900  // VS2015 provides snprintf
#define snprintf _snprintf
#endif  // _MSC_VER < 1900

#if _MSC_VER < 1800  // VS2013 provides round
#include <math.h>
static INLINE double round(double x) {
  if (x < 0)
    return ceil(x - 0.5);
  else
    return floor(x + 0.5);
}

static INLINE float roundf(float x) {
  if (x < 0)
    return (float)ceil(x - 0.5f);
  else
    return (float)floor(x + 0.5f);
}

static INLINE long lroundf(float x) {
  if (x < 0)
    return (long)(x - 0.5f);
  else
    return (long)(x + 0.5f);
}
#endif  // _MSC_VER < 1800

#if HAVE_AVX
#include <immintrin.h>
// Note:
// _mm256_insert_epi16 intrinsics is available from vs2017.
// We define this macro for vs2015 and earlier. The
// intrinsics used here are in vs2015 document:
// https://msdn.microsoft.com/en-us/library/hh977022.aspx
// Input parameters:
// a: __m256i,
// d: int16_t,
// indx: imm8 (0 - 15)
#if _MSC_VER <= 1900
#define _mm256_insert_epi16(a, d, indx)                                      \
  _mm256_insertf128_si256(                                                   \
      a,                                                                     \
      _mm_insert_epi16(_mm256_extractf128_si256(a, indx >> 3), d, indx % 8), \
      indx >> 3)

static INLINE int _mm256_extract_epi32(__m256i a, const int i) {
  return a.m256i_i32[i & 7];
}
static INLINE __m256i _mm256_insert_epi32(__m256i a, int b, const int i) {
  __m256i c = a;
  c.m256i_i32[i & 7] = b;
  return c;
}
#endif  // _MSC_VER <= 1900
#endif  // HAVE_AVX
#endif  // _MSC_VER
#endif  // AOM_AOM_PORTS_MSVC_H_
