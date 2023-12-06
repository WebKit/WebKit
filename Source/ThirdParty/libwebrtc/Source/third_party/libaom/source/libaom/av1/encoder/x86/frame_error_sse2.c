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
#include "av1/encoder/global_motion.h"
#include "config/av1_rtcd.h"

#if CONFIG_AV1_HIGHBITDEPTH
int64_t av1_calc_highbd_frame_error_sse2(const uint16_t *const ref,
                                         int ref_stride,
                                         const uint16_t *const dst,
                                         int dst_stride, int p_width,
                                         int p_height, int bd) {
  const int b = bd - 8;
  const __m128i shift = _mm_cvtsi32_si128(b);
  const __m128i bmask = _mm_set1_epi16((1 << b) - 1);
  const __m128i v = _mm_set1_epi16(1 << b);

  int64_t sum_error = 0;
  int i, j;
  __m128i row_error, col_error;
  const __m128i zero = _mm_setzero_si128();
  const __m128i dup_256 = _mm_set1_epi16(256);
  const __m128i dup_257 = _mm_set1_epi16(257);
  col_error = zero;
  for (i = 0; i < (p_height); i++) {
    row_error = zero;
    for (j = 0; j < (p_width / 16); j++) {
      const __m128i ref_1 =
          _mm_load_si128((__m128i *)(ref + (j * 16) + (i * ref_stride)));
      const __m128i dst_1 =
          _mm_load_si128((__m128i *)(dst + (j * 16) + (i * dst_stride)));
      const __m128i ref_2 =
          _mm_load_si128((__m128i *)(ref + (j * 16 + 8) + (i * ref_stride)));
      const __m128i dst_2 =
          _mm_load_si128((__m128i *)(dst + (j * 16 + 8) + (i * dst_stride)));

      const __m128i diff_1 = _mm_sub_epi16(dst_1, ref_1);
      const __m128i diff_2 = _mm_sub_epi16(dst_2, ref_2);

      const __m128i e1_1 = _mm_sra_epi16(diff_1, shift);
      const __m128i e2_1 = _mm_and_si128(diff_1, bmask);
      const __m128i e1_2 = _mm_sra_epi16(diff_2, shift);
      const __m128i e2_2 = _mm_and_si128(diff_2, bmask);

      // For each 16-bit element in e1 and e2, we need to accumulate
      // the value:
      //   error_measure_lut[256 + e1] * (v - e2) +
      //   error_measure_lut[257 + e1] * e2
      // To do this, we first synthesize two 16-bit gathers, then
      // interleave the factors in such a way that we can use _mm_madd_epi16
      // to do eight 16x16->32 multiplies in one go
      const __m128i idx1_1 = _mm_add_epi16(e1_1, dup_256);
      const __m128i error1_1 =
          _mm_set_epi16(error_measure_lut[_mm_extract_epi16(idx1_1, 7)],
                        error_measure_lut[_mm_extract_epi16(idx1_1, 6)],
                        error_measure_lut[_mm_extract_epi16(idx1_1, 5)],
                        error_measure_lut[_mm_extract_epi16(idx1_1, 4)],
                        error_measure_lut[_mm_extract_epi16(idx1_1, 3)],
                        error_measure_lut[_mm_extract_epi16(idx1_1, 2)],
                        error_measure_lut[_mm_extract_epi16(idx1_1, 1)],
                        error_measure_lut[_mm_extract_epi16(idx1_1, 0)]);

      const __m128i idx2_1 = _mm_add_epi16(e1_1, dup_257);
      const __m128i error2_1 =
          _mm_set_epi16(error_measure_lut[_mm_extract_epi16(idx2_1, 7)],
                        error_measure_lut[_mm_extract_epi16(idx2_1, 6)],
                        error_measure_lut[_mm_extract_epi16(idx2_1, 5)],
                        error_measure_lut[_mm_extract_epi16(idx2_1, 4)],
                        error_measure_lut[_mm_extract_epi16(idx2_1, 3)],
                        error_measure_lut[_mm_extract_epi16(idx2_1, 2)],
                        error_measure_lut[_mm_extract_epi16(idx2_1, 1)],
                        error_measure_lut[_mm_extract_epi16(idx2_1, 0)]);

      const __m128i error_lo_1 = _mm_unpacklo_epi16(error1_1, error2_1);
      const __m128i error_hi_1 = _mm_unpackhi_epi16(error1_1, error2_1);

      const __m128i idx1_2 = _mm_add_epi16(e1_2, dup_256);
      const __m128i error1_2 =
          _mm_set_epi16(error_measure_lut[_mm_extract_epi16(idx1_2, 7)],
                        error_measure_lut[_mm_extract_epi16(idx1_2, 6)],
                        error_measure_lut[_mm_extract_epi16(idx1_2, 5)],
                        error_measure_lut[_mm_extract_epi16(idx1_2, 4)],
                        error_measure_lut[_mm_extract_epi16(idx1_2, 3)],
                        error_measure_lut[_mm_extract_epi16(idx1_2, 2)],
                        error_measure_lut[_mm_extract_epi16(idx1_2, 1)],
                        error_measure_lut[_mm_extract_epi16(idx1_2, 0)]);

      const __m128i idx2_2 = _mm_add_epi16(e1_2, dup_257);
      const __m128i error2_2 =
          _mm_set_epi16(error_measure_lut[_mm_extract_epi16(idx2_2, 7)],
                        error_measure_lut[_mm_extract_epi16(idx2_2, 6)],
                        error_measure_lut[_mm_extract_epi16(idx2_2, 5)],
                        error_measure_lut[_mm_extract_epi16(idx2_2, 4)],
                        error_measure_lut[_mm_extract_epi16(idx2_2, 3)],
                        error_measure_lut[_mm_extract_epi16(idx2_2, 2)],
                        error_measure_lut[_mm_extract_epi16(idx2_2, 1)],
                        error_measure_lut[_mm_extract_epi16(idx2_2, 0)]);

      const __m128i error_lo_2 = _mm_unpacklo_epi16(error1_2, error2_2);
      const __m128i error_hi_2 = _mm_unpackhi_epi16(error1_2, error2_2);

      // Compute multipliers
      const __m128i e2_inv_1 = _mm_sub_epi16(v, e2_1);
      const __m128i mul_lo_1 = _mm_unpacklo_epi16(e2_inv_1, e2_1);
      const __m128i mul_hi_1 = _mm_unpackhi_epi16(e2_inv_1, e2_1);

      const __m128i e2_inv_2 = _mm_sub_epi16(v, e2_2);
      const __m128i mul_lo_2 = _mm_unpacklo_epi16(e2_inv_2, e2_2);
      const __m128i mul_hi_2 = _mm_unpackhi_epi16(e2_inv_2, e2_2);

      // Multiply and accumulate
      const __m128i result1_1 = _mm_madd_epi16(error_lo_1, mul_lo_1);
      const __m128i result2_1 = _mm_madd_epi16(error_hi_1, mul_hi_1);
      const __m128i result1_2 = _mm_madd_epi16(error_lo_2, mul_lo_2);
      const __m128i result2_2 = _mm_madd_epi16(error_hi_2, mul_hi_2);

      const __m128i partial_sum =
          _mm_add_epi32(_mm_add_epi32(result1_1, result2_1),
                        _mm_add_epi32(result1_2, result2_2));

      row_error = _mm_add_epi32(row_error, partial_sum);
    }

    const __m128i col_error_lo = _mm_unpacklo_epi32(row_error, zero);
    const __m128i col_error_hi = _mm_unpackhi_epi32(row_error, zero);
    const __m128i col_error_temp = _mm_add_epi64(col_error_lo, col_error_hi);
    col_error = _mm_add_epi64(col_error, col_error_temp);
    // Error summation for remaining width, which is not multiple of 16
    if (p_width & 0xf) {
      for (int l = j * 16; l < p_width; ++l) {
        sum_error += (int64_t)highbd_error_measure(
            dst[l + i * dst_stride] - ref[l + i * ref_stride], bd);
      }
    }
  }
  int64_t sum_error_d_0, sum_error_d_1;
  xx_storel_64(&sum_error_d_0, col_error);
  xx_storel_64(&sum_error_d_1, _mm_srli_si128(col_error, 8));
  sum_error = (sum_error + sum_error_d_0 + sum_error_d_1);
  return sum_error;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

int64_t av1_calc_frame_error_sse2(const uint8_t *const ref, int ref_stride,
                                  const uint8_t *const dst, int dst_stride,
                                  int p_width, int p_height) {
  int64_t sum_error = 0;
  int i, j;
  __m128i row_error, col_error;
  const __m128i zero = _mm_setzero_si128();
  const __m128i dup_256 = _mm_set1_epi16(256);
  col_error = zero;
  for (i = 0; i < (p_height); i++) {
    row_error = zero;
    for (j = 0; j < (p_width / 16); j++) {
      const __m128i ref_8 =
          _mm_load_si128((__m128i *)(ref + (j * 16) + (i * ref_stride)));
      const __m128i dst_8 =
          _mm_load_si128((__m128i *)(dst + (j * 16) + (i * dst_stride)));
      const __m128i ref_16_lo = _mm_unpacklo_epi8(ref_8, zero);
      const __m128i ref_16_hi = _mm_unpackhi_epi8(ref_8, zero);
      const __m128i dst_16_lo = _mm_unpacklo_epi8(dst_8, zero);
      const __m128i dst_16_hi = _mm_unpackhi_epi8(dst_8, zero);

      const __m128i diff_1 =
          _mm_add_epi16(_mm_sub_epi16(dst_16_lo, ref_16_lo), dup_256);
      const __m128i diff_2 =
          _mm_add_epi16(_mm_sub_epi16(dst_16_hi, ref_16_hi), dup_256);

      const __m128i error_1_lo =
          _mm_set_epi32(error_measure_lut[_mm_extract_epi16(diff_1, 3)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 2)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 1)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 0)]);
      const __m128i error_1_hi =
          _mm_set_epi32(error_measure_lut[_mm_extract_epi16(diff_1, 7)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 6)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 5)],
                        error_measure_lut[_mm_extract_epi16(diff_1, 4)]);
      const __m128i error_2_lo =
          _mm_set_epi32(error_measure_lut[_mm_extract_epi16(diff_2, 3)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 2)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 1)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 0)]);
      const __m128i error_2_hi =
          _mm_set_epi32(error_measure_lut[_mm_extract_epi16(diff_2, 7)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 6)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 5)],
                        error_measure_lut[_mm_extract_epi16(diff_2, 4)]);

      const __m128i error_1 = _mm_add_epi32(error_1_lo, error_1_hi);
      const __m128i error_2 = _mm_add_epi32(error_2_lo, error_2_hi);
      const __m128i error_1_2 = _mm_add_epi32(error_1, error_2);

      row_error = _mm_add_epi32(row_error, error_1_2);
    }
    const __m128i col_error_lo = _mm_unpacklo_epi32(row_error, zero);
    const __m128i col_error_hi = _mm_unpackhi_epi32(row_error, zero);
    const __m128i col_error_temp = _mm_add_epi64(col_error_lo, col_error_hi);
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
