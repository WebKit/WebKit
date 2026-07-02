/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>
#include <smmintrin.h>

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"
#include "aom_dsp/x86/sum_squares_sse2.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

static uint64_t aom_sum_squares_2d_i16_nxn_avx2(const int16_t *src, int stride,
                                                int width, int height) {
  uint64_t result;
  __m256i v_acc_q = _mm256_setzero_si256();
  const __m256i v_zext_mask_q = _mm256_set1_epi64x(~0u);
  for (int col = 0; col < height; col += 4) {
    __m256i v_acc_d = _mm256_setzero_si256();
    for (int row = 0; row < width; row += 16) {
      const int16_t *tempsrc = src + row;
      const __m256i v_val_0_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 0 * stride));
      const __m256i v_val_1_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 1 * stride));
      const __m256i v_val_2_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 2 * stride));
      const __m256i v_val_3_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 3 * stride));

      const __m256i v_sq_0_d = _mm256_madd_epi16(v_val_0_w, v_val_0_w);
      const __m256i v_sq_1_d = _mm256_madd_epi16(v_val_1_w, v_val_1_w);
      const __m256i v_sq_2_d = _mm256_madd_epi16(v_val_2_w, v_val_2_w);
      const __m256i v_sq_3_d = _mm256_madd_epi16(v_val_3_w, v_val_3_w);

      const __m256i v_sum_01_d = _mm256_add_epi32(v_sq_0_d, v_sq_1_d);
      const __m256i v_sum_23_d = _mm256_add_epi32(v_sq_2_d, v_sq_3_d);
      const __m256i v_sum_0123_d = _mm256_add_epi32(v_sum_01_d, v_sum_23_d);

      v_acc_d = _mm256_add_epi32(v_acc_d, v_sum_0123_d);
    }
    v_acc_q =
        _mm256_add_epi64(v_acc_q, _mm256_and_si256(v_acc_d, v_zext_mask_q));
    v_acc_q = _mm256_add_epi64(v_acc_q, _mm256_srli_epi64(v_acc_d, 32));
    src += 4 * stride;
  }
  __m128i lower_64_2_Value = _mm256_castsi256_si128(v_acc_q);
  __m128i higher_64_2_Value = _mm256_extracti128_si256(v_acc_q, 1);
  __m128i result_64_2_int = _mm_add_epi64(lower_64_2_Value, higher_64_2_Value);

  result_64_2_int = _mm_add_epi64(
      result_64_2_int, _mm_unpackhi_epi64(result_64_2_int, result_64_2_int));

  xx_storel_64(&result, result_64_2_int);

  return result;
}

uint64_t aom_sum_squares_2d_i16_avx2(const int16_t *src, int stride, int width,
                                     int height) {
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_squares_2d_i16_4x4_sse2(src, stride);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    return aom_sum_squares_2d_i16_4xn_sse2(src, stride, height);
  } else if (LIKELY(width == 8 && (height & 3) == 0)) {
    return aom_sum_squares_2d_i16_nxn_sse2(src, stride, width, height);
  } else if (LIKELY(((width & 15) == 0) && ((height & 3) == 0))) {
    return aom_sum_squares_2d_i16_nxn_avx2(src, stride, width, height);
  } else {
    return aom_sum_squares_2d_i16_c(src, stride, width, height);
  }
}

static uint64_t aom_sum_sse_2d_i16_nxn_avx2(const int16_t *src, int stride,
                                            int width, int height, int *sum) {
  uint64_t result;
  const __m256i zero_reg = _mm256_setzero_si256();
  const __m256i one_reg = _mm256_set1_epi16(1);

  __m256i v_sse_total = zero_reg;
  __m256i v_sum_total = zero_reg;

  for (int col = 0; col < height; col += 4) {
    __m256i v_sse_row = zero_reg;
    for (int row = 0; row < width; row += 16) {
      const int16_t *tempsrc = src + row;
      const __m256i v_val_0_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 0 * stride));
      const __m256i v_val_1_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 1 * stride));
      const __m256i v_val_2_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 2 * stride));
      const __m256i v_val_3_w =
          _mm256_loadu_si256((const __m256i *)(tempsrc + 3 * stride));

      const __m256i v_sum_01 = _mm256_add_epi16(v_val_0_w, v_val_1_w);
      const __m256i v_sum_23 = _mm256_add_epi16(v_val_2_w, v_val_3_w);
      __m256i v_sum_0123 = _mm256_add_epi16(v_sum_01, v_sum_23);
      v_sum_0123 = _mm256_madd_epi16(v_sum_0123, one_reg);
      v_sum_total = _mm256_add_epi32(v_sum_total, v_sum_0123);

      const __m256i v_sq_0_d = _mm256_madd_epi16(v_val_0_w, v_val_0_w);
      const __m256i v_sq_1_d = _mm256_madd_epi16(v_val_1_w, v_val_1_w);
      const __m256i v_sq_2_d = _mm256_madd_epi16(v_val_2_w, v_val_2_w);
      const __m256i v_sq_3_d = _mm256_madd_epi16(v_val_3_w, v_val_3_w);
      const __m256i v_sq_01_d = _mm256_add_epi32(v_sq_0_d, v_sq_1_d);
      const __m256i v_sq_23_d = _mm256_add_epi32(v_sq_2_d, v_sq_3_d);
      const __m256i v_sq_0123_d = _mm256_add_epi32(v_sq_01_d, v_sq_23_d);
      v_sse_row = _mm256_add_epi32(v_sse_row, v_sq_0123_d);
    }
    const __m256i v_sse_row_low = _mm256_unpacklo_epi32(v_sse_row, zero_reg);
    const __m256i v_sse_row_hi = _mm256_unpackhi_epi32(v_sse_row, zero_reg);
    v_sse_row = _mm256_add_epi64(v_sse_row_low, v_sse_row_hi);
    v_sse_total = _mm256_add_epi64(v_sse_total, v_sse_row);
    src += 4 * stride;
  }

  const __m128i v_sum_total_low = _mm256_castsi256_si128(v_sum_total);
  const __m128i v_sum_total_hi = _mm256_extracti128_si256(v_sum_total, 1);
  __m128i sum_128bit = _mm_add_epi32(v_sum_total_hi, v_sum_total_low);
  sum_128bit = _mm_add_epi32(sum_128bit, _mm_srli_si128(sum_128bit, 8));
  sum_128bit = _mm_add_epi32(sum_128bit, _mm_srli_si128(sum_128bit, 4));
  *sum += _mm_cvtsi128_si32(sum_128bit);

  __m128i v_sse_total_lo = _mm256_castsi256_si128(v_sse_total);
  __m128i v_sse_total_hi = _mm256_extracti128_si256(v_sse_total, 1);
  __m128i sse_128bit = _mm_add_epi64(v_sse_total_lo, v_sse_total_hi);

  sse_128bit =
      _mm_add_epi64(sse_128bit, _mm_unpackhi_epi64(sse_128bit, sse_128bit));

  xx_storel_64(&result, sse_128bit);

  return result;
}

uint64_t aom_sum_sse_2d_i16_avx2(const int16_t *src, int src_stride, int width,
                                 int height, int *sum) {
  if (LIKELY(width == 4 && height == 4)) {
    return aom_sum_sse_2d_i16_4x4_sse2(src, src_stride, sum);
  } else if (LIKELY(width == 4 && (height & 3) == 0)) {
    return aom_sum_sse_2d_i16_4xn_sse2(src, src_stride, height, sum);
  } else if (LIKELY(width == 8 && (height & 3) == 0)) {
    return aom_sum_sse_2d_i16_nxn_sse2(src, src_stride, width, height, sum);
  } else if (LIKELY(((width & 15) == 0) && ((height & 3) == 0))) {
    return aom_sum_sse_2d_i16_nxn_avx2(src, src_stride, width, height, sum);
  } else {
    return aom_sum_sse_2d_i16_c(src, src_stride, width, height, sum);
  }
}

// Accumulate sum of 16-bit elements in the vector
static inline int32_t mm256_accumulate_epi16(__m256i vec_a) {
  __m128i vtmp1 = _mm256_extracti128_si256(vec_a, 1);
  __m128i vtmp2 = _mm256_castsi256_si128(vec_a);
  vtmp1 = _mm_add_epi16(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 8);
  vtmp1 = _mm_add_epi16(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 4);
  vtmp1 = _mm_add_epi16(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 2);
  vtmp1 = _mm_add_epi16(vtmp1, vtmp2);
  return _mm_extract_epi16(vtmp1, 0);
}

// Accumulate sum of 32-bit elements in the vector
static inline int32_t mm256_accumulate_epi32(__m256i vec_a) {
  __m128i vtmp1 = _mm256_extracti128_si256(vec_a, 1);
  __m128i vtmp2 = _mm256_castsi256_si128(vec_a);
  vtmp1 = _mm_add_epi32(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 8);
  vtmp1 = _mm_add_epi32(vtmp1, vtmp2);
  vtmp2 = _mm_srli_si128(vtmp1, 4);
  vtmp1 = _mm_add_epi32(vtmp1, vtmp2);
  return _mm_cvtsi128_si32(vtmp1);
}

uint64_t aom_var_2d_u8_avx2(uint8_t *src, int src_stride, int width,
                            int height) {
  uint8_t *srcp;
  uint64_t s = 0, ss = 0;
  __m256i vzero = _mm256_setzero_si256();
  __m256i v_acc_sum = vzero;
  __m256i v_acc_sqs = vzero;
  int i, j;

  // Process 32 elements in a row
  for (i = 0; i < width - 31; i += 32) {
    srcp = src + i;
    // Process 8 columns at a time
    for (j = 0; j < height - 7; j += 8) {
      __m256i vsrc[8];
      for (int k = 0; k < 8; k++) {
        vsrc[k] = _mm256_loadu_si256((__m256i *)srcp);
        srcp += src_stride;
      }
      for (int k = 0; k < 8; k++) {
        __m256i vsrc0 = _mm256_unpacklo_epi8(vsrc[k], vzero);
        __m256i vsrc1 = _mm256_unpackhi_epi8(vsrc[k], vzero);
        v_acc_sum = _mm256_add_epi16(v_acc_sum, vsrc0);
        v_acc_sum = _mm256_add_epi16(v_acc_sum, vsrc1);

        __m256i vsqs0 = _mm256_madd_epi16(vsrc0, vsrc0);
        __m256i vsqs1 = _mm256_madd_epi16(vsrc1, vsrc1);
        v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs0);
        v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs1);
      }

      // Update total sum and clear the vectors
      s += mm256_accumulate_epi16(v_acc_sum);
      ss += mm256_accumulate_epi32(v_acc_sqs);
      v_acc_sum = vzero;
      v_acc_sqs = vzero;
    }

    // Process remaining rows (height not a multiple of 8)
    for (; j < height; j++) {
      __m256i vsrc = _mm256_loadu_si256((__m256i *)srcp);
      __m256i vsrc0 = _mm256_unpacklo_epi8(vsrc, vzero);
      __m256i vsrc1 = _mm256_unpackhi_epi8(vsrc, vzero);
      v_acc_sum = _mm256_add_epi16(v_acc_sum, vsrc0);
      v_acc_sum = _mm256_add_epi16(v_acc_sum, vsrc1);

      __m256i vsqs0 = _mm256_madd_epi16(vsrc0, vsrc0);
      __m256i vsqs1 = _mm256_madd_epi16(vsrc1, vsrc1);
      v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs0);
      v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs1);

      srcp += src_stride;
    }

    // Update total sum and clear the vectors
    s += mm256_accumulate_epi16(v_acc_sum);
    ss += mm256_accumulate_epi32(v_acc_sqs);
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
uint64_t aom_var_2d_u16_avx2(uint8_t *src, int src_stride, int width,
                             int height) {
  uint16_t *srcp1 = CONVERT_TO_SHORTPTR(src), *srcp;
  uint64_t s = 0, ss = 0;
  __m256i vzero = _mm256_setzero_si256();
  __m256i v_acc_sum = vzero;
  __m256i v_acc_sqs = vzero;
  int i, j;

  // Process 16 elements in a row
  for (i = 0; i < width - 15; i += 16) {
    srcp = srcp1 + i;
    // Process 8 columns at a time
    for (j = 0; j < height - 8; j += 8) {
      __m256i vsrc[8];
      for (int k = 0; k < 8; k++) {
        vsrc[k] = _mm256_loadu_si256((__m256i *)srcp);
        srcp += src_stride;
      }
      for (int k = 0; k < 8; k++) {
        __m256i vsrc0 = _mm256_unpacklo_epi16(vsrc[k], vzero);
        __m256i vsrc1 = _mm256_unpackhi_epi16(vsrc[k], vzero);
        v_acc_sum = _mm256_add_epi32(vsrc0, v_acc_sum);
        v_acc_sum = _mm256_add_epi32(vsrc1, v_acc_sum);

        __m256i vsqs0 = _mm256_madd_epi16(vsrc[k], vsrc[k]);
        v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs0);
      }

      // Update total sum and clear the vectors
      s += mm256_accumulate_epi32(v_acc_sum);
      ss += mm256_accumulate_epi32(v_acc_sqs);
      v_acc_sum = vzero;
      v_acc_sqs = vzero;
    }

    // Process remaining rows (height not a multiple of 8)
    for (; j < height; j++) {
      __m256i vsrc = _mm256_loadu_si256((__m256i *)srcp);
      __m256i vsrc0 = _mm256_unpacklo_epi16(vsrc, vzero);
      __m256i vsrc1 = _mm256_unpackhi_epi16(vsrc, vzero);
      v_acc_sum = _mm256_add_epi32(vsrc0, v_acc_sum);
      v_acc_sum = _mm256_add_epi32(vsrc1, v_acc_sum);

      __m256i vsqs0 = _mm256_madd_epi16(vsrc, vsrc);
      v_acc_sqs = _mm256_add_epi32(v_acc_sqs, vsqs0);
      srcp += src_stride;
    }

    // Update total sum and clear the vectors
    s += mm256_accumulate_epi32(v_acc_sum);
    ss += mm256_accumulate_epi32(v_acc_sqs);
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
