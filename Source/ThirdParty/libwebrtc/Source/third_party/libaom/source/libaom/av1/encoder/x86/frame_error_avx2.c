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

#include <immintrin.h>
#include "config/av1_rtcd.h"
#include "av1/common/warped_motion.h"
#include "av1/encoder/global_motion.h"
#include "aom_dsp/x86/synonyms.h"

#if CONFIG_AV1_HIGHBITDEPTH
int64_t av1_calc_highbd_frame_error_avx2(const uint16_t *const ref,
                                         int ref_stride,
                                         const uint16_t *const dst,
                                         int dst_stride, int p_width,
                                         int p_height, int bd) {
  const int b = bd - 8;
  const __m128i shift = _mm_cvtsi32_si128(b);
  const __m256i bmask = _mm256_set1_epi16((1 << b) - 1);
  const __m256i v = _mm256_set1_epi16(1 << b);

  int64_t sum_error = 0;
  int i, j;
  __m256i row_error, col_error;
  const __m256i zero = _mm256_setzero_si256();
  const __m256i dup_256 = _mm256_set1_epi16(256);
  const __m256i dup_257 = _mm256_set1_epi16(257);
  col_error = zero;
  for (i = 0; i < (p_height / 2); i++) {
    row_error = _mm256_setzero_si256();
    for (j = 0; j < (p_width / 16); j++) {
      const __m256i ref_1_16 = _mm256_load_si256(
          (__m256i *)(ref + (j * 16) + (((i * 2) + 0) * ref_stride)));
      const __m256i dst_1_16 = _mm256_load_si256(
          (__m256i *)(dst + (j * 16) + (((i * 2) + 0) * dst_stride)));
      const __m256i ref_2_16 = _mm256_load_si256(
          (__m256i *)(ref + (j * 16) + (((i * 2) + 1) * ref_stride)));
      const __m256i dst_2_16 = _mm256_load_si256(
          (__m256i *)(dst + (j * 16) + (((i * 2) + 1) * dst_stride)));

      const __m256i diff_1 = _mm256_sub_epi16(dst_1_16, ref_1_16);
      const __m256i diff_2 = _mm256_sub_epi16(dst_2_16, ref_2_16);

      const __m256i e1_1 = _mm256_sra_epi16(diff_1, shift);
      const __m256i e2_1 = _mm256_and_si256(diff_1, bmask);
      const __m256i e1_2 = _mm256_sra_epi16(diff_2, shift);
      const __m256i e2_2 = _mm256_and_si256(diff_2, bmask);

      // We need to fetch two 16-bit error values per pixel, so that we can
      // interpolate, but AVX2 only supports 32-bit gathers. Therefore we
      // need to expand each register of indices into two 8x32-bit registers,
      // gather, then re-combine into 16x16-bit registers.
      const __m256i idx1_1 = _mm256_add_epi16(e1_1, dup_256);
      const __m256i idx2_1 = _mm256_add_epi16(e1_1, dup_257);
      const __m256i idx1_2 = _mm256_add_epi16(e1_2, dup_256);
      const __m256i idx2_2 = _mm256_add_epi16(e1_2, dup_257);

      const __m256i idx1_lo_1 = _mm256_unpacklo_epi16(idx1_1, zero);
      const __m256i idx1_hi_1 = _mm256_unpackhi_epi16(idx1_1, zero);
      const __m256i idx2_lo_1 = _mm256_unpacklo_epi16(idx2_1, zero);
      const __m256i idx2_hi_1 = _mm256_unpackhi_epi16(idx2_1, zero);

      const __m256i idx1_lo_2 = _mm256_unpacklo_epi16(idx1_2, zero);
      const __m256i idx1_hi_2 = _mm256_unpackhi_epi16(idx1_2, zero);
      const __m256i idx2_lo_2 = _mm256_unpacklo_epi16(idx2_2, zero);
      const __m256i idx2_hi_2 = _mm256_unpackhi_epi16(idx2_2, zero);

      const __m256i error_1_lo_1 =
          _mm256_i32gather_epi32(error_measure_lut, idx1_lo_1, 4);
      const __m256i error_1_hi_1 =
          _mm256_i32gather_epi32(error_measure_lut, idx1_hi_1, 4);
      const __m256i error_2_lo_1 =
          _mm256_i32gather_epi32(error_measure_lut, idx2_lo_1, 4);
      const __m256i error_2_hi_1 =
          _mm256_i32gather_epi32(error_measure_lut, idx2_hi_1, 4);
      const __m256i error_1_lo_2 =
          _mm256_i32gather_epi32(error_measure_lut, idx1_lo_2, 4);
      const __m256i error_1_hi_2 =
          _mm256_i32gather_epi32(error_measure_lut, idx1_hi_2, 4);
      const __m256i error_2_lo_2 =
          _mm256_i32gather_epi32(error_measure_lut, idx2_lo_2, 4);
      const __m256i error_2_hi_2 =
          _mm256_i32gather_epi32(error_measure_lut, idx2_hi_2, 4);

      const __m256i error_1_1 = _mm256_packus_epi32(error_1_lo_1, error_1_hi_1);
      const __m256i error_2_1 = _mm256_packus_epi32(error_2_lo_1, error_2_hi_1);
      const __m256i error_1_2 = _mm256_packus_epi32(error_1_lo_2, error_1_hi_2);
      const __m256i error_2_2 = _mm256_packus_epi32(error_2_lo_2, error_2_hi_2);

      // Interleave the error and multiplier arrays
      // The unpack instructions implicitly reorder the pixels, but the
      // reordering is consistent between the two arrays being multiplied,
      // and we sum everything into one value at the end, so this does not
      // affect the final result.
      const __m256i e2_inv_1 = _mm256_sub_epi16(v, e2_1);
      const __m256i e2_inv_2 = _mm256_sub_epi16(v, e2_2);

      const __m256i error_lo_1 = _mm256_unpacklo_epi16(error_1_1, error_2_1);
      const __m256i error_hi_1 = _mm256_unpackhi_epi16(error_1_1, error_2_1);
      const __m256i mul_lo_1 = _mm256_unpacklo_epi16(e2_inv_1, e2_1);
      const __m256i mul_hi_1 = _mm256_unpackhi_epi16(e2_inv_1, e2_1);

      const __m256i error_lo_2 = _mm256_unpacklo_epi16(error_1_2, error_2_2);
      const __m256i error_hi_2 = _mm256_unpackhi_epi16(error_1_2, error_2_2);
      const __m256i mul_lo_2 = _mm256_unpacklo_epi16(e2_inv_2, e2_2);
      const __m256i mul_hi_2 = _mm256_unpackhi_epi16(e2_inv_2, e2_2);

      const __m256i result_lo_1 = _mm256_madd_epi16(error_lo_1, mul_lo_1);
      const __m256i result_hi_1 = _mm256_madd_epi16(error_hi_1, mul_hi_1);
      const __m256i result_lo_2 = _mm256_madd_epi16(error_lo_2, mul_lo_2);
      const __m256i result_hi_2 = _mm256_madd_epi16(error_hi_2, mul_hi_2);

      const __m256i partial_sum =
          _mm256_add_epi32(_mm256_add_epi32(result_lo_1, result_hi_1),
                           _mm256_add_epi32(result_lo_2, result_hi_2));

      row_error = _mm256_add_epi32(row_error, partial_sum);
    }
    const __m256i col_error_lo = _mm256_unpacklo_epi32(row_error, zero);
    const __m256i col_error_hi = _mm256_unpackhi_epi32(row_error, zero);
    const __m256i col_error_temp = _mm256_add_epi64(col_error_lo, col_error_hi);
    col_error = _mm256_add_epi64(col_error, col_error_temp);
    // Error summation for remaining width, which is not multiple of 16
    if (p_width & 0xf) {
      for (int k = 0; k < 2; ++k) {
        for (int l = j * 16; l < p_width; ++l) {
          sum_error += (int64_t)highbd_error_measure(
              dst[l + ((i * 2) + k) * dst_stride] -
                  ref[l + ((i * 2) + k) * ref_stride],
              bd);
        }
      }
    }
  }
  const __m128i sum_error_q =
      _mm_add_epi64(_mm256_castsi256_si128(col_error),
                    _mm256_extracti128_si256(col_error, 1));
  int64_t sum_error_d_0, sum_error_d_1;
  xx_storel_64(&sum_error_d_0, sum_error_q);
  xx_storel_64(&sum_error_d_1, _mm_srli_si128(sum_error_q, 8));
  sum_error = (sum_error + sum_error_d_0 + sum_error_d_1);
  // Error summation for remaining height, which is not multiple of 2
  if (p_height & 0x1) {
    for (int k = i * 2; k < p_height; ++k) {
      for (int l = 0; l < p_width; ++l) {
        sum_error += (int64_t)highbd_error_measure(
            dst[l + k * dst_stride] - ref[l + k * ref_stride], bd);
      }
    }
  }
  return sum_error;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

int64_t av1_calc_frame_error_avx2(const uint8_t *const ref, int ref_stride,
                                  const uint8_t *const dst, int dst_stride,
                                  int p_width, int p_height) {
  int64_t sum_error = 0;
  int i, j;
  __m256i row_error, col_error;
  const __m256i zero = _mm256_setzero_si256();
  const __m256i dup_256 = _mm256_set1_epi16(256);
  col_error = zero;

  for (i = 0; i < (p_height / 4); i++) {
    row_error = _mm256_setzero_si256();
    for (j = 0; j < (p_width / 16); j++) {
      const __m256i ref_1_16 = _mm256_cvtepu8_epi16(_mm_load_si128(
          (__m128i *)(ref + (j * 16) + (((i * 4) + 0) * ref_stride))));
      const __m256i dst_1_16 = _mm256_cvtepu8_epi16(_mm_load_si128(
          (__m128i *)(dst + (j * 16) + (((i * 4) + 0) * dst_stride))));
      const __m256i ref_2_16 = _mm256_cvtepu8_epi16(_mm_load_si128(
          (__m128i *)(ref + (j * 16) + (((i * 4) + 1) * ref_stride))));
      const __m256i dst_2_16 = _mm256_cvtepu8_epi16(_mm_load_si128(
          (__m128i *)(dst + (j * 16) + (((i * 4) + 1) * dst_stride))));
      const __m256i ref_3_16 = _mm256_cvtepu8_epi16(_mm_load_si128(
          (__m128i *)(ref + (j * 16) + (((i * 4) + 2) * ref_stride))));
      const __m256i dst_3_16 = _mm256_cvtepu8_epi16(_mm_load_si128(
          (__m128i *)(dst + (j * 16) + (((i * 4) + 2) * dst_stride))));
      const __m256i ref_4_16 = _mm256_cvtepu8_epi16(_mm_load_si128(
          (__m128i *)(ref + (j * 16) + (((i * 4) + 3) * ref_stride))));
      const __m256i dst_4_16 = _mm256_cvtepu8_epi16(_mm_load_si128(
          (__m128i *)(dst + (j * 16) + (((i * 4) + 3) * dst_stride))));

      const __m256i diff_1 =
          _mm256_add_epi16(_mm256_sub_epi16(dst_1_16, ref_1_16), dup_256);
      const __m256i diff_2 =
          _mm256_add_epi16(_mm256_sub_epi16(dst_2_16, ref_2_16), dup_256);
      const __m256i diff_3 =
          _mm256_add_epi16(_mm256_sub_epi16(dst_3_16, ref_3_16), dup_256);
      const __m256i diff_4 =
          _mm256_add_epi16(_mm256_sub_epi16(dst_4_16, ref_4_16), dup_256);

      const __m256i diff_1_lo = _mm256_unpacklo_epi16(diff_1, zero);
      const __m256i diff_1_hi = _mm256_unpackhi_epi16(diff_1, zero);
      const __m256i diff_2_lo = _mm256_unpacklo_epi16(diff_2, zero);
      const __m256i diff_2_hi = _mm256_unpackhi_epi16(diff_2, zero);
      const __m256i diff_3_lo = _mm256_unpacklo_epi16(diff_3, zero);
      const __m256i diff_3_hi = _mm256_unpackhi_epi16(diff_3, zero);
      const __m256i diff_4_lo = _mm256_unpacklo_epi16(diff_4, zero);
      const __m256i diff_4_hi = _mm256_unpackhi_epi16(diff_4, zero);

      const __m256i error_1_lo =
          _mm256_i32gather_epi32(error_measure_lut, diff_1_lo, 4);
      const __m256i error_1_hi =
          _mm256_i32gather_epi32(error_measure_lut, diff_1_hi, 4);
      const __m256i error_2_lo =
          _mm256_i32gather_epi32(error_measure_lut, diff_2_lo, 4);
      const __m256i error_2_hi =
          _mm256_i32gather_epi32(error_measure_lut, diff_2_hi, 4);
      const __m256i error_3_lo =
          _mm256_i32gather_epi32(error_measure_lut, diff_3_lo, 4);
      const __m256i error_3_hi =
          _mm256_i32gather_epi32(error_measure_lut, diff_3_hi, 4);
      const __m256i error_4_lo =
          _mm256_i32gather_epi32(error_measure_lut, diff_4_lo, 4);
      const __m256i error_4_hi =
          _mm256_i32gather_epi32(error_measure_lut, diff_4_hi, 4);

      const __m256i error_1 = _mm256_add_epi32(error_1_lo, error_1_hi);
      const __m256i error_2 = _mm256_add_epi32(error_2_lo, error_2_hi);
      const __m256i error_3 = _mm256_add_epi32(error_3_lo, error_3_hi);
      const __m256i error_4 = _mm256_add_epi32(error_4_lo, error_4_hi);

      const __m256i error_1_2 = _mm256_add_epi32(error_1, error_2);
      const __m256i error_3_4 = _mm256_add_epi32(error_3, error_4);

      const __m256i error_1_2_3_4 = _mm256_add_epi32(error_1_2, error_3_4);
      row_error = _mm256_add_epi32(row_error, error_1_2_3_4);
    }
    const __m256i col_error_lo = _mm256_unpacklo_epi32(row_error, zero);
    const __m256i col_error_hi = _mm256_unpackhi_epi32(row_error, zero);
    const __m256i col_error_temp = _mm256_add_epi64(col_error_lo, col_error_hi);
    col_error = _mm256_add_epi64(col_error, col_error_temp);
    // Error summation for remaining width, which is not multiple of 16
    if (p_width & 0xf) {
      for (int k = 0; k < 4; ++k) {
        for (int l = j * 16; l < p_width; ++l) {
          sum_error +=
              (int64_t)error_measure(dst[l + ((i * 4) + k) * dst_stride] -
                                     ref[l + ((i * 4) + k) * ref_stride]);
        }
      }
    }
  }
  const __m128i sum_error_q =
      _mm_add_epi64(_mm256_castsi256_si128(col_error),
                    _mm256_extracti128_si256(col_error, 1));
  int64_t sum_error_d_0, sum_error_d_1;
  xx_storel_64(&sum_error_d_0, sum_error_q);
  xx_storel_64(&sum_error_d_1, _mm_srli_si128(sum_error_q, 8));
  sum_error = (sum_error + sum_error_d_0 + sum_error_d_1);
  // Error summation for remaining height, which is not multiple of 4
  if (p_height & 0x3) {
    for (int k = i * 4; k < p_height; ++k) {
      for (int l = 0; l < p_width; ++l) {
        sum_error += (int64_t)error_measure(dst[l + k * dst_stride] -
                                            ref[l + k * ref_stride]);
      }
    }
  }
  return sum_error;
}
