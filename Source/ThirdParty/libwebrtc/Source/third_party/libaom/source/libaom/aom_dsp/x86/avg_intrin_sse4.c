/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <smmintrin.h>

#include "config/aom_dsp_rtcd.h"

// ref: [0 - 510]
// src: [0 - 510]
// bwl: {2, 3, 4, 5}
int aom_vector_var_sse4_1(const int16_t *ref, const int16_t *src, int bwl) {
  const int width = 4 << bwl;
  assert(width % 16 == 0);

  const __m128i k_one_epi16 = _mm_set1_epi16((int16_t)1);
  __m128i mean = _mm_setzero_si128();
  __m128i sse = _mm_setzero_si128();

  for (int i = 0; i < width; i += 16) {
    const __m128i src_line = _mm_loadu_si128((const __m128i *)src);
    const __m128i ref_line = _mm_loadu_si128((const __m128i *)ref);
    const __m128i src_line2 = _mm_loadu_si128((const __m128i *)(src + 8));
    const __m128i ref_line2 = _mm_loadu_si128((const __m128i *)(ref + 8));
    __m128i diff = _mm_sub_epi16(ref_line, src_line);
    const __m128i diff2 = _mm_sub_epi16(ref_line2, src_line2);
    __m128i diff_sqr = _mm_madd_epi16(diff, diff);
    const __m128i diff_sqr2 = _mm_madd_epi16(diff2, diff2);

    diff = _mm_add_epi16(diff, diff2);
    diff_sqr = _mm_add_epi32(diff_sqr, diff_sqr2);
    sse = _mm_add_epi32(sse, diff_sqr);
    mean = _mm_add_epi16(mean, diff);

    src += 16;
    ref += 16;
  }

  // m0 m1 m2 m3
  mean = _mm_madd_epi16(mean, k_one_epi16);
  // m0+m1 m2+m3 s0+s1 s2+s3
  __m128i result = _mm_hadd_epi32(mean, sse);
  // m0+m1+m2+m3 s0+s1+s2+s3 x x
  result = _mm_add_epi32(result, _mm_bsrli_si128(result, 4));

  // (mean * mean): dynamic range 31 bits.
  const int mean_int = _mm_extract_epi32(result, 0);
  const int sse_int = _mm_extract_epi32(result, 2);
  const unsigned int mean_abs = abs(mean_int);
  const int var = sse_int - ((mean_abs * mean_abs) >> (bwl + 2));
  return var;
}
