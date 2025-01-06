/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <emmintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/synonyms.h"

static unsigned int sad4xh_sse2(const uint8_t *a, int a_stride,
                                const uint8_t *b, int b_stride, int width,
                                int height) {
  int i;
  assert(width == 4);
  (void)width;

  __m128i sad = _mm_setzero_si128();
  for (i = 0; i < height; i += 4) {
    __m128i x0 = xx_loadl_32(a + 0 * a_stride);
    __m128i x1 = xx_loadl_32(a + 1 * a_stride);
    __m128i x2 = xx_loadl_32(a + 2 * a_stride);
    __m128i x3 = xx_loadl_32(a + 3 * a_stride);
    __m128i x_lo = _mm_unpacklo_epi32(x0, x1);
    __m128i x_hi = _mm_unpacklo_epi32(x2, x3);

    __m128i x = _mm_unpacklo_epi64(x_lo, x_hi);

    x0 = xx_loadl_32(b + 0 * b_stride);
    x1 = xx_loadl_32(b + 1 * b_stride);
    x2 = xx_loadl_32(b + 2 * b_stride);
    x3 = xx_loadl_32(b + 3 * b_stride);
    x_lo = _mm_unpacklo_epi32(x0, x1);
    x_hi = _mm_unpacklo_epi32(x2, x3);

    __m128i y = _mm_unpacklo_epi64(x_lo, x_hi);

    __m128i sad4x4 = _mm_sad_epu8(x, y);
    sad = _mm_add_epi32(sad, sad4x4);

    a += 4 * a_stride;
    b += 4 * b_stride;
  }

  // At this point, we have two 32-bit partial SADs at bit[0:31] and [64:95].
  const unsigned int res =
      (unsigned int)(_mm_cvtsi128_si32(sad) +
                     _mm_cvtsi128_si32(_mm_srli_si128(sad, 8)));

  return res;
}

static unsigned int sad8xh_sse2(const uint8_t *a, int a_stride,
                                const uint8_t *b, int b_stride, int width,
                                int height) {
  int i;
  assert(width == 8);
  (void)width;

  __m128i sad = _mm_setzero_si128();
  for (i = 0; i < height; i += 2) {
    __m128i x0 = xx_loadl_64(a + 0 * a_stride);
    __m128i x1 = xx_loadl_64(a + 1 * a_stride);

    __m128i x = _mm_unpacklo_epi64(x0, x1);

    x0 = xx_loadl_64(b + 0 * b_stride);
    x1 = xx_loadl_64(b + 1 * b_stride);

    __m128i y = _mm_unpacklo_epi64(x0, x1);

    __m128i sad8x2 = _mm_sad_epu8(x, y);
    sad = _mm_add_epi32(sad, sad8x2);

    a += 2 * a_stride;
    b += 2 * b_stride;
  }

  const unsigned int res =
      (unsigned int)(_mm_cvtsi128_si32(sad) +
                     _mm_cvtsi128_si32(_mm_srli_si128(sad, 8)));

  return res;
}

static unsigned int sad16xh_sse2(const uint8_t *a, int a_stride,
                                 const uint8_t *b, int b_stride, int width,
                                 int height) {
  int i;
  assert(width == 16);
  (void)width;

  __m128i sad = _mm_setzero_si128();
  for (i = 0; i < height; ++i) {
    __m128i x = xx_loadu_128(a);
    __m128i y = xx_loadu_128(b);

    __m128i sad16x1 = _mm_sad_epu8(x, y);
    sad = _mm_add_epi32(sad, sad16x1);

    a += a_stride;
    b += b_stride;
  }

  const unsigned int res =
      (unsigned int)(_mm_cvtsi128_si32(sad) +
                     _mm_cvtsi128_si32(_mm_srli_si128(sad, 8)));

  return res;
}

static unsigned int sad32xh_sse2(const uint8_t *a, int a_stride,
                                 const uint8_t *b, int b_stride, int width,
                                 int height) {
  int i, j;
  assert(width == 32);
  (void)width;

  __m128i sad = _mm_setzero_si128();
  for (i = 0; i < height; ++i) {
    for (j = 0; j < 2; ++j) {
      __m128i x = xx_loadu_128(a + j * 16);
      __m128i y = xx_loadu_128(b + j * 16);

      __m128i sad32_half = _mm_sad_epu8(x, y);
      sad = _mm_add_epi32(sad, sad32_half);
    }

    a += a_stride;
    b += b_stride;
  }

  const unsigned int res =
      (unsigned int)(_mm_cvtsi128_si32(sad) +
                     _mm_cvtsi128_si32(_mm_srli_si128(sad, 8)));

  return res;
}

static unsigned int sad64xh_sse2(const uint8_t *a, int a_stride,
                                 const uint8_t *b, int b_stride, int width,
                                 int height) {
  int i, j;
  assert(width == 64);
  (void)width;

  __m128i sad = _mm_setzero_si128();
  for (i = 0; i < height; ++i) {
    for (j = 0; j < 4; ++j) {
      __m128i x = xx_loadu_128(a + j * 16);
      __m128i y = xx_loadu_128(b + j * 16);

      __m128i sad64_quarter = _mm_sad_epu8(x, y);
      sad = _mm_add_epi32(sad, sad64_quarter);
    }

    a += a_stride;
    b += b_stride;
  }

  const unsigned int res =
      (unsigned int)(_mm_cvtsi128_si32(sad) +
                     _mm_cvtsi128_si32(_mm_srli_si128(sad, 8)));

  return res;
}

static unsigned int sad128xh_sse2(const uint8_t *a, int a_stride,
                                  const uint8_t *b, int b_stride, int width,
                                  int height) {
  int i, j;
  assert(width == 128);
  (void)width;

  __m128i sad = _mm_setzero_si128();
  for (i = 0; i < height; ++i) {
    for (j = 0; j < 8; ++j) {
      __m128i x = xx_loadu_128(a + j * 16);
      __m128i y = xx_loadu_128(b + j * 16);

      __m128i sad64_quarter = _mm_sad_epu8(x, y);
      sad = _mm_add_epi32(sad, sad64_quarter);
    }

    a += a_stride;
    b += b_stride;
  }

  const unsigned int res =
      (unsigned int)(_mm_cvtsi128_si32(sad) +
                     _mm_cvtsi128_si32(_mm_srli_si128(sad, 8)));

  return res;
}

#define DIST_WTD_SADMXN_SSE2(m, n)                                            \
  unsigned int aom_dist_wtd_sad##m##x##n##_avg_sse2(                          \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    uint8_t comp_pred[m * n];                                                 \
    aom_dist_wtd_comp_avg_pred(comp_pred, second_pred, m, n, ref, ref_stride, \
                               jcp_param);                                    \
    return sad##m##xh_sse2(src, src_stride, comp_pred, m, m, n);              \
  }

DIST_WTD_SADMXN_SSE2(128, 128)
DIST_WTD_SADMXN_SSE2(128, 64)
DIST_WTD_SADMXN_SSE2(64, 128)
DIST_WTD_SADMXN_SSE2(64, 64)
DIST_WTD_SADMXN_SSE2(64, 32)
DIST_WTD_SADMXN_SSE2(32, 64)
DIST_WTD_SADMXN_SSE2(32, 32)
DIST_WTD_SADMXN_SSE2(32, 16)
DIST_WTD_SADMXN_SSE2(16, 32)
DIST_WTD_SADMXN_SSE2(16, 16)
DIST_WTD_SADMXN_SSE2(16, 8)
DIST_WTD_SADMXN_SSE2(8, 16)
DIST_WTD_SADMXN_SSE2(8, 8)
DIST_WTD_SADMXN_SSE2(8, 4)
DIST_WTD_SADMXN_SSE2(4, 8)
DIST_WTD_SADMXN_SSE2(4, 4)
#if !CONFIG_REALTIME_ONLY
DIST_WTD_SADMXN_SSE2(4, 16)
DIST_WTD_SADMXN_SSE2(16, 4)
DIST_WTD_SADMXN_SSE2(8, 32)
DIST_WTD_SADMXN_SSE2(32, 8)
DIST_WTD_SADMXN_SSE2(16, 64)
DIST_WTD_SADMXN_SSE2(64, 16)
#endif
