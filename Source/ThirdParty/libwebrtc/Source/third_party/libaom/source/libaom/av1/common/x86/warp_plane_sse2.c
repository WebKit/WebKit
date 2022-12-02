/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <emmintrin.h>

#include "aom_dsp/x86/synonyms.h"
#include "av1/common/warped_motion.h"
#include "config/av1_rtcd.h"

int64_t av1_calc_frame_error_sse2(const uint8_t *const ref, int ref_stride,
                                  const uint8_t *const dst, int p_width,
                                  int p_height, int dst_stride) {
  int64_t sum_error = 0;
  int i, j;
  __m128i row_error, col_error;
  __m128i zero = _mm_set1_epi16(0);
  __m128i dup_255 = _mm_set1_epi16(255);
  col_error = zero;
  for (i = 0; i < (p_height); i++) {
    row_error = zero;
    for (j = 0; j < (p_width / 16); j++) {
      __m128i ref_8 =
          _mm_load_si128((__m128i *)(ref + (j * 16) + (i * ref_stride)));
      __m128i dst_8 =
          _mm_load_si128((__m128i *)(dst + (j * 16) + (i * dst_stride)));
      __m128i ref_16_lo = _mm_unpacklo_epi8(ref_8, zero);
      __m128i ref_16_hi = _mm_unpackhi_epi8(ref_8, zero);
      __m128i dst_16_lo = _mm_unpacklo_epi8(dst_8, zero);
      __m128i dst_16_hi = _mm_unpackhi_epi8(dst_8, zero);

      __m128i diff_1 =
          _mm_add_epi16(_mm_sub_epi16(dst_16_lo, ref_16_lo), dup_255);
      __m128i diff_2 =
          _mm_add_epi16(_mm_sub_epi16(dst_16_hi, ref_16_hi), dup_255);

      __m128i error_1_lo =
          _mm_set_epi32(error_measure_lut[_mm_extract_epi16(diff_1, 3)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 2)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 1)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 0)]);
      __m128i error_1_hi =
          _mm_set_epi32(error_measure_lut[_mm_extract_epi16(diff_1, 7)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 6)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 5)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 4)]);
      __m128i error_2_lo =
          _mm_set_epi32(error_measure_lut[_mm_extract_epi16(diff_2, 3)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 2)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 1)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 0)]);
      __m128i error_2_hi =
          _mm_set_epi32(error_measure_lut[_mm_extract_epi16(diff_2, 7)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 6)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 5)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 4)]);

      __m128i error_1 = _mm_add_epi32(error_1_lo, error_1_hi);
      __m128i error_2 = _mm_add_epi32(error_2_lo, error_2_hi);
      __m128i error_1_2 = _mm_add_epi32(error_1, error_2);

      row_error = _mm_add_epi32(row_error, error_1_2);
    }
    __m128i col_error_lo = _mm_unpacklo_epi32(row_error, zero);
    __m128i col_error_hi = _mm_unpackhi_epi32(row_error, zero);
    __m128i col_error_temp = _mm_add_epi64(col_error_lo, col_error_hi);
    col_error = _mm_add_epi64(col_error, col_error_temp);
    // Error summation for remaining width, which is not multiple of 16
    if (p_width & 0xf) {
      for (int l = j * 16; l < p_width; ++l) {
        sum_error += (int64_t)error_measure(dst[l + i * dst_stride] -
                                            ref[l + i * ref_stride]);
      }
    }
  }
  int64_t sum_error_d_0, sum_error_d_1;
  xx_storel_64(&sum_error_d_0, col_error);
  xx_storel_64(&sum_error_d_1, _mm_srli_si128(col_error, 8));
  sum_error = (sum_error + sum_error_d_0 + sum_error_d_1);
  return sum_error;
}
