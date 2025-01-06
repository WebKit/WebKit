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
#include <stdio.h>

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/sum_squares_sse2.h"
#include "config/aom_dsp_rtcd.h"

static inline __m128i xx_loadh_64(__m128i a, const void *b) {
  const __m128d ad = _mm_castsi128_pd(a);
  return _mm_castpd_si128(_mm_loadh_pd(ad, (double *)b));
}

static inline uint64_t xx_cvtsi128_si64(__m128i a) {
#if AOM_ARCH_X86_64
  return (uint64_t)_mm_cvtsi128_si64(a);
#else
  {
    uint64_t tmp;
    _mm_storel_epi64((__m128i *)&tmp, a);
    return tmp;
  }
#endif
}

static inline __m128i sum_squares_i16_4x4_sse2(const int16_t *src, int stride) {
  const __m128i v_val_0_w = xx_loadl_64(src + 0 * stride);
  const __m128i v_val_2_w = xx_loadl_64(src + 2 * stride);
  const __m128i v_val_01_w = xx_loadh_64(v_val_0_w, src + 1 * stride);
  const __m128i v_val_23_w = xx_loadh_64(v_val_2_w, src + 3 * stride);
  const __m128i v_sq_01_d = _mm_madd_epi16(v_val_01_w, v_val_01_w);
  const __m128i v_sq_23_d = _mm_madd_epi16(v_val_23_w, v_val_23_w);

  return _mm_add_epi32(v_sq_01_d, v_sq_23_d);
}

uint64_t aom_sum_squares_2d_i16_4x4_sse2(const int16_t *src, int stride) {
  const __m128i v_sum_0123_d = sum_squares_i16_4x4_sse2(src, stride);
  __m128i v_sum_d =
      _mm_add_epi32(v_sum_0123_d, _mm_srli_epi64(v_sum_0123_d, 32));
  v_sum_d = _mm_add_epi32(v_sum_d, _mm_srli_si128(v_sum_d, 8));
  return (uint64_t)_mm_cvtsi128_si32(v_sum_d);
}

uint64_t aom_sum_sse_2d_i16_4x4_sse2(const int16_t *src, int stride, int *sum) {
  const __m128i one_reg = _mm_set1_epi16(1);
  const __m128i v_val_0_w = xx_loadl_64(src + 0 * stride);
  const __m128i v_val_2_w = xx_loadl_64(src + 2 * stride);
  __m128i v_val_01_w = xx_loadh_64(v_val_0_w, src + 1 * stride);
  __m128i v_val_23_w = xx_loadh_64(v_val_2_w, src + 3 * stride);

  __m128i v_sum_0123_d = _mm_add_epi16(v_val_01_w, v_val_23_w);
  v_sum_0123_d = _mm_madd_epi16(v_sum_0123_d, one_reg);
  v_sum_0123_d = _mm_add_epi32(v_sum_0123_d, _mm_srli_si128(v_sum_0123_d, 8));
  v_sum_0123_d = _mm_add_epi32(v_sum_0123_d, _mm_srli_si128(v_sum_0123_d, 4));
  *sum = _mm_cvtsi128_si32(v_sum_0123_d);

  const __m128i v_sq_01_d = _mm_madd_epi16(v_val_01_w, v_val_01_w);
  const __m128i v_sq_23_d = _mm_madd_epi16(v_val_23_w, v_val_23_w);
  __m128i v_sq_0123_d = _mm_add_epi32(v_sq_01_d, v_sq_23_d);
  v_sq_0123_d = _mm_add_epi32(v_sq_0123_d, _mm_srli_si128(v_sq_0123_d, 8));
  v_sq_0123_d = _mm_add_epi32(v_sq_0123_d, _mm_srli_si128(v_sq_0123_d, 4));
  return (uint64_t)_mm_cvtsi128_si32(v_sq_0123_d);
}

uint64_t aom_sum_squares_2d_i16_4xn_sse2(const int16_t *src, int stride,
                                         int height) {
  int r = 0;
  __m128i v_acc_q = _mm_setzero_si128();
  do {
    const __m128i v_acc_d = sum_squares_i16_4x4_sse2(src, stride);
    v_acc_q = _mm_add_epi32(v_acc_q, v_acc_d);
    src += stride << 2;
    r += 4;
  } while (r < height);
  const __m128i v_zext_mask_q = _mm_set1_epi64x(~0u);
  __m128i v_acc_64 = _mm_add_epi64(_mm_srli_epi64(v_acc_q, 32),
                                   _mm_and_si128(v_acc_q, v_zext_mask_q));
  v_acc_64 = _mm_add_epi64(v_acc_64, _mm_srli_si128(v_acc_64, 8));
  return xx_cvtsi128_si64(v_acc_64);
}

uint64_t aom_sum_sse_2d_i16_4xn_sse2(const int16_t *src, int stride, int height,
                                     int *sum) {
  int r = 0;
  uint64_t sse = 0;
  do {
    int curr_sum = 0;
    sse += aom_sum_sse_2d_i16_4x4_sse2(src, stride, &curr_sum);
    *sum += curr_sum;
    src += stride << 2;
    r += 4;
  } while (r < height);
  return sse;
}

#ifdef __GNUC__
// This prevents GCC/Clang from inlining this function into
// aom_sum_squares_2d_i16_sse2, which in turn saves some stack
// maintenance instructions in the common case of 4x4.
__attribute__((noinline))
#endif
uint64_t
aom_sum_squares_2d_i16_nxn_sse2(const int16_t *src, int stride, int width,
                                int height) {
  int r = 0;

  const __m128i v_zext_mask_q = _mm_set1_epi64x(~0u);
  __m128i v_acc_q = _mm_setzero_si128();

  do {
    __m128i v_acc_d = _mm_setzero_si128();
    int c = 0;
    do {
      const int16_t *b = src + c;

      const __m128i v_val_0_w = xx_load_128(b + 0 * stride);
      const __m128i v_val_1_w = xx_load_128(b + 1 * stride);
      const __m128i v_val_2_w = xx_load_128(b + 2 * stride);
      const __m128i v_val_3_w = xx_load_128(b + 3 * stride);

      const __m128i v_sq_0_d = _mm_madd_epi16(v_val_0_w, v_val_0_w);
      const __m128i v_sq_1_d = _mm_madd_epi16(v_val_1_w, v_val_1_w);
      const __m128i v_sq_2_d = _mm_madd_epi16(v_val_2_w, v_val_2_w);
      const __m128i v_sq_3_d = _mm_madd_epi16(v_val_3_w, v_val_3_w);

      const __m128i v_sum_01_d = _mm_add_epi32(v_sq_0_d, v_sq_1_d);
      const __m128i v_sum_23_d = _mm_add_epi32(v_sq_2_d, v_sq_3_d);

      const __m128i v_sum_0123_d = _mm_add_epi32(v_sum_01_d, v_sum_23_d);

      v_acc_d = _mm_add_epi32(v_acc_d, v_sum_0123_d);
      c += 8;
    } while (c < width);

    v_acc_q = _mm_add_epi64(v_acc_q, _mm_and_si128(v_acc_d, v_zext_mask_q));
    v_acc_q = _mm_add_epi64(v_acc_q, _mm_srli_epi64(v_acc_d, 32));

    src += 4 * stride;
    r += 4;
  } while (r < height);

  v_acc_q = _mm_add_epi64(v_acc_q, _mm_srli_si128(v_acc_q, 8));
  return xx_cvtsi128_si64(v_acc_q);
}

#ifdef __GNUC__
// This prevents GCC/Clang from inlining this function into
// aom_sum_sse_2d_i16_nxn_sse2, which in turn saves some stack
// maintenance instructions in the common case of 4x4.
__attribute__((noinline))
#endif
uint64_t
aom_sum_sse_2d_i16_nxn_sse2(const int16_t *src, int stride, int width,
                            int height, int *sum) {
  int r = 0;
  uint64_t result;
  const __m128i zero_reg = _mm_setzero_si128();
  const __m128i one_reg = _mm_set1_epi16(1);

  __m128i v_sse_total = zero_reg;
  __m128i v_sum_total = zero_reg;

  do {
    int c = 0;
    __m128i v_sse_row = zero_reg;
    do {
      const int16_t *b = src + c;

      __m128i v_val_0_w = xx_load_128(b + 0 * stride);
      __m128i v_val_1_w = xx_load_128(b + 1 * stride);
      __m128i v_val_2_w = xx_load_128(b + 2 * stride);
      __m128i v_val_3_w = xx_load_128(b + 3 * stride);

      const __m128i v_sq_0_d = _mm_madd_epi16(v_val_0_w, v_val_0_w);
      const __m128i v_sq_1_d = _mm_madd_epi16(v_val_1_w, v_val_1_w);
      const __m128i v_sq_2_d = _mm_madd_epi16(v_val_2_w, v_val_2_w);
      const __m128i v_sq_3_d = _mm_madd_epi16(v_val_3_w, v_val_3_w);
      const __m128i v_sq_01_d = _mm_add_epi32(v_sq_0_d, v_sq_1_d);
      const __m128i v_sq_23_d = _mm_add_epi32(v_sq_2_d, v_sq_3_d);
      const __m128i v_sq_0123_d = _mm_add_epi32(v_sq_01_d, v_sq_23_d);
      v_sse_row = _mm_add_epi32(v_sse_row, v_sq_0123_d);

      const __m128i v_sum_01 = _mm_add_epi16(v_val_0_w, v_val_1_w);
      const __m128i v_sum_23 = _mm_add_epi16(v_val_2_w, v_val_3_w);
      __m128i v_sum_0123_d = _mm_add_epi16(v_sum_01, v_sum_23);
      v_sum_0123_d = _mm_madd_epi16(v_sum_0123_d, one_reg);
      v_sum_total = _mm_add_epi32(v_sum_total, v_sum_0123_d);

      c += 8;
    } while (c < width);

    const __m128i v_sse_row_low = _mm_unpacklo_epi32(v_sse_row, zero_reg);
    const __m128i v_sse_row_hi = _mm_unpackhi_epi32(v_sse_row, zero_reg);
    v_sse_row = _mm_add_epi64(v_sse_row_low, v_sse_row_hi);
    v_sse_total = _mm_add_epi64(v_sse_total, v_sse_row);
    src += 4 * stride;
    r += 4;
  } while (r < height);

  v_sum_total = _mm_add_epi32(v_sum_total, _mm_srli_si128(v_sum_total, 8));
  v_sum_total = _mm_add_epi32(v_sum_total, _mm_srli_si128(v_sum_total, 4));
  *sum += _mm_cvtsi128_si32(v_sum_total);

  v_sse_total = _mm_add_epi64(v_sse_total, _mm_srli_si128(v_sse_total, 8));
  xx_storel_64(&result, v_sse_total);
  return result;
}

uint64_t aom_sum_squares_2d_i16_sse2(const int16_t *src, int stride, int width,
                                     int height) {
  // 4 elements per row only requires half an XMM register, so this
  // must be a special case, but also note that over 75% of all calls
  // are with size == 4, so it is also the common case.
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_squares_2d_i16_4x4_sse2(src, stride);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    return aom_sum_squares_2d_i16_4xn_sse2(src, stride, height);
  } else if (LIKELY((width & 7) == 0 && (height & 3) == 0)) {
    // Generic case
    return aom_sum_squares_2d_i16_nxn_sse2(src, stride, width, height);
  } else {
    return aom_sum_squares_2d_i16_c(src, stride, width, height);
  }
}

uint64_t aom_sum_sse_2d_i16_sse2(const int16_t *src, int src_stride, int width,
                                 int height, int *sum) {
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_sse_2d_i16_4x4_sse2(src, src_stride, sum);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    return aom_sum_sse_2d_i16_4xn_sse2(src, src_stride, height, sum);
  } else if (LIKELY((width & 7) == 0 && (height & 3) == 0)) {
    // Generic case
    return aom_sum_sse_2d_i16_nxn_sse2(src, src_stride, width, height, sum);
  } else {
    return aom_sum_sse_2d_i16_c(src, src_stride, width, height, sum);
  }
}

//////////////////////////////////////////////////////////////////////////////
// 1D version
//////////////////////////////////////////////////////////////////////////////

static uint64_t aom_sum_squares_i16_64n_sse2(const int16_t *src, uint32_t n) {
  const __m128i v_zext_mask_q = _mm_set1_epi64x(~0u);
  __m128i v_acc0_q = _mm_setzero_si128();
  __m128i v_acc1_q = _mm_setzero_si128();

  const int16_t *const end = src + n;

  assert(n % 64 == 0);

  while (src < end) {
    const __m128i v_val_0_w = xx_load_128(src);
    const __m128i v_val_1_w = xx_load_128(src + 8);
    const __m128i v_val_2_w = xx_load_128(src + 16);
    const __m128i v_val_3_w = xx_load_128(src + 24);
    const __m128i v_val_4_w = xx_load_128(src + 32);
    const __m128i v_val_5_w = xx_load_128(src + 40);
    const __m128i v_val_6_w = xx_load_128(src + 48);
    const __m128i v_val_7_w = xx_load_128(src + 56);

    const __m128i v_sq_0_d = _mm_madd_epi16(v_val_0_w, v_val_0_w);
    const __m128i v_sq_1_d = _mm_madd_epi16(v_val_1_w, v_val_1_w);
    const __m128i v_sq_2_d = _mm_madd_epi16(v_val_2_w, v_val_2_w);
    const __m128i v_sq_3_d = _mm_madd_epi16(v_val_3_w, v_val_3_w);
    const __m128i v_sq_4_d = _mm_madd_epi16(v_val_4_w, v_val_4_w);
    const __m128i v_sq_5_d = _mm_madd_epi16(v_val_5_w, v_val_5_w);
    const __m128i v_sq_6_d = _mm_madd_epi16(v_val_6_w, v_val_6_w);
    const __m128i v_sq_7_d = _mm_madd_epi16(v_val_7_w, v_val_7_w);

    const __m128i v_sum_01_d = _mm_add_epi32(v_sq_0_d, v_sq_1_d);
    const __m128i v_sum_23_d = _mm_add_epi32(v_sq_2_d, v_sq_3_d);
    const __m128i v_sum_45_d = _mm_add_epi32(v_sq_4_d, v_sq_5_d);
    const __m128i v_sum_67_d = _mm_add_epi32(v_sq_6_d, v_sq_7_d);

    const __m128i v_sum_0123_d = _mm_add_epi32(v_sum_01_d, v_sum_23_d);
    const __m128i v_sum_4567_d = _mm_add_epi32(v_sum_45_d, v_sum_67_d);

    const __m128i v_sum_d = _mm_add_epi32(v_sum_0123_d, v_sum_4567_d);

    v_acc0_q = _mm_add_epi64(v_acc0_q, _mm_and_si128(v_sum_d, v_zext_mask_q));
    v_acc1_q = _mm_add_epi64(v_acc1_q, _mm_srli_epi64(v_sum_d, 32));

    src += 64;
  }

  v_acc0_q = _mm_add_epi64(v_acc0_q, v_acc1_q);
  v_acc0_q = _mm_add_epi64(v_acc0_q, _mm_srli_si128(v_acc0_q, 8));
  return xx_cvtsi128_si64(v_acc0_q);
}

uint64_t aom_sum_squares_i16_sse2(const int16_t *src, uint32_t n) {
  if (n % 64 == 0) {
    return aom_sum_squares_i16_64n_sse2(src, n);
  } else if (n > 64) {
    const uint32_t k = n & ~63u;
    return aom_sum_squares_i16_64n_sse2(src, k) +
           aom_sum_squares_i16_c(src + k, n - k);
  } else {
    return aom_sum_squares_i16_c(src, n);
  }
}

// Accumulate sum of 16-bit elements in the vector
static inline int32_t mm_accumulate_epi16(__m128i vec_a) {
  __m128i vtmp = _mm_srli_si128(vec_a, 8);
  vec_a = _mm_add_epi16(vec_a, vtmp);
  vtmp = _mm_srli_si128(vec_a, 4);
  vec_a = _mm_add_epi16(vec_a, vtmp);
  vtmp = _mm_srli_si128(vec_a, 2);
  vec_a = _mm_add_epi16(vec_a, vtmp);
  return _mm_extract_epi16(vec_a, 0);
}

// Accumulate sum of 32-bit elements in the vector
static inline int32_t mm_accumulate_epi32(__m128i vec_a) {
  __m128i vtmp = _mm_srli_si128(vec_a, 8);
  vec_a = _mm_add_epi32(vec_a, vtmp);
  vtmp = _mm_srli_si128(vec_a, 4);
  vec_a = _mm_add_epi32(vec_a, vtmp);
  return _mm_cvtsi128_si32(vec_a);
}

uint64_t aom_var_2d_u8_sse2(uint8_t *src, int src_stride, int width,
                            int height) {
  uint8_t *srcp;
  uint64_t s = 0, ss = 0;
  __m128i vzero = _mm_setzero_si128();
  __m128i v_acc_sum = vzero;
  __m128i v_acc_sqs = vzero;
  int i, j;

  // Process 16 elements in a row
  for (i = 0; i < width - 15; i += 16) {
    srcp = src + i;
    // Process 8 columns at a time
    for (j = 0; j < height - 7; j += 8) {
      __m128i vsrc[8];
      for (int k = 0; k < 8; k++) {
        vsrc[k] = _mm_loadu_si128((__m128i *)srcp);
        srcp += src_stride;
      }
      for (int k = 0; k < 8; k++) {
        __m128i vsrc0 = _mm_unpacklo_epi8(vsrc[k], vzero);
        __m128i vsrc1 = _mm_unpackhi_epi8(vsrc[k], vzero);
        v_acc_sum = _mm_add_epi16(v_acc_sum, vsrc0);
        v_acc_sum = _mm_add_epi16(v_acc_sum, vsrc1);

        __m128i vsqs0 = _mm_madd_epi16(vsrc0, vsrc0);
        __m128i vsqs1 = _mm_madd_epi16(vsrc1, vsrc1);
        v_acc_sqs = _mm_add_epi32(v_acc_sqs, vsqs0);
        v_acc_sqs = _mm_add_epi32(v_acc_sqs, vsqs1);
      }

      // Update total sum and clear the vectors
      s += mm_accumulate_epi16(v_acc_sum);
      ss += mm_accumulate_epi32(v_acc_sqs);
      v_acc_sum = vzero;
      v_acc_sqs = vzero;
    }

    // Process remaining rows (height not a multiple of 8)
    for (; j < height; j++) {
      __m128i vsrc = _mm_loadu_si128((__m128i *)srcp);
      __m128i vsrc0 = _mm_unpacklo_epi8(vsrc, vzero);
      __m128i vsrc1 = _mm_unpackhi_epi8(vsrc, vzero);
      v_acc_sum = _mm_add_epi16(v_acc_sum, vsrc0);
      v_acc_sum = _mm_add_epi16(v_acc_sum, vsrc1);

      __m128i vsqs0 = _mm_madd_epi16(vsrc0, vsrc0);
      __m128i vsqs1 = _mm_madd_epi16(vsrc1, vsrc1);
      v_acc_sqs = _mm_add_epi32(v_acc_sqs, vsqs0);
      v_acc_sqs = _mm_add_epi32(v_acc_sqs, vsqs1);

      srcp += src_stride;
    }

    // Update total sum and clear the vectors
    s += mm_accumulate_epi16(v_acc_sum);
    ss += mm_accumulate_epi32(v_acc_sqs);
    v_acc_sum = vzero;
    v_acc_sqs = vzero;
  }

  // Process the remaining area using C
  srcp = src;
  for (int k = 0; k < height; k++) {
    for (int m = i; m < width; m++) {
      uint8_t val = srcp[m];
      s += val;
      ss += val * val;
    }
    srcp += src_stride;
  }
  return (ss - s * s / (width * height));
}

#if CONFIG_AV1_HIGHBITDEPTH
uint64_t aom_var_2d_u16_sse2(uint8_t *src, int src_stride, int width,
                             int height) {
  uint16_t *srcp1 = CONVERT_TO_SHORTPTR(src), *srcp;
  uint64_t s = 0, ss = 0;
  __m128i vzero = _mm_setzero_si128();
  __m128i v_acc_sum = vzero;
  __m128i v_acc_sqs = vzero;
  int i, j;

  // Process 8 elements in a row
  for (i = 0; i < width - 8; i += 8) {
    srcp = srcp1 + i;
    // Process 8 columns at a time
    for (j = 0; j < height - 8; j += 8) {
      __m128i vsrc[8];
      for (int k = 0; k < 8; k++) {
        vsrc[k] = _mm_loadu_si128((__m128i *)srcp);
        srcp += src_stride;
      }
      for (int k = 0; k < 8; k++) {
        __m128i vsrc0 = _mm_unpacklo_epi16(vsrc[k], vzero);
        __m128i vsrc1 = _mm_unpackhi_epi16(vsrc[k], vzero);
        v_acc_sum = _mm_add_epi32(vsrc0, v_acc_sum);
        v_acc_sum = _mm_add_epi32(vsrc1, v_acc_sum);

        __m128i vsqs0 = _mm_madd_epi16(vsrc[k], vsrc[k]);
        v_acc_sqs = _mm_add_epi32(v_acc_sqs, vsqs0);
      }

      // Update total sum and clear the vectors
      s += mm_accumulate_epi32(v_acc_sum);
      ss += mm_accumulate_epi32(v_acc_sqs);
      v_acc_sum = vzero;
      v_acc_sqs = vzero;
    }

    // Process remaining rows (height not a multiple of 8)
    for (; j < height; j++) {
      __m128i vsrc = _mm_loadu_si128((__m128i *)srcp);
      __m128i vsrc0 = _mm_unpacklo_epi16(vsrc, vzero);
      __m128i vsrc1 = _mm_unpackhi_epi16(vsrc, vzero);
      v_acc_sum = _mm_add_epi32(vsrc0, v_acc_sum);
      v_acc_sum = _mm_add_epi32(vsrc1, v_acc_sum);

      __m128i vsqs0 = _mm_madd_epi16(vsrc, vsrc);
      v_acc_sqs = _mm_add_epi32(v_acc_sqs, vsqs0);
      srcp += src_stride;
    }

    // Update total sum and clear the vectors
    s += mm_accumulate_epi32(v_acc_sum);
    ss += mm_accumulate_epi32(v_acc_sqs);
    v_acc_sum = vzero;
    v_acc_sqs = vzero;
  }

  // Process the remaining area using C
  srcp = srcp1;
  for (int k = 0; k < height; k++) {
    for (int m = i; m < width; m++) {
      uint16_t val = srcp[m];
      s += val;
      ss += val * val;
    }
    srcp += src_stride;
  }
  return (ss - s * s / (width * height));
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
