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
#include <assert.h>

#include "config/av1_rtcd.h"

#include "av1/common/convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"

// 128-bit xmmwords are written as [ ... ] with the MSB on the left.
// 256-bit ymmwords are written as two xmmwords, [ ... ][ ... ] with the MSB
// on the left.
// A row of, say, 16-bit pixels with values p0, p1, p2, ..., p14, p15 will be
// loaded and stored as [ p15 ... p9 p8 ][ p7 ... p1 p0 ].
void av1_highbd_wiener_convolve_add_src_avx2(
    const uint8_t *src8, ptrdiff_t src_stride, uint8_t *dst8,
    ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4,
    const int16_t *filter_y, int y_step_q4, int w, int h,
    const WienerConvolveParams *conv_params, int bd) {
  assert(x_step_q4 == 16 && y_step_q4 == 16);
  assert(!(w & 7));
  assert(bd + FILTER_BITS - conv_params->round_0 + 2 <= 16);
  (void)x_step_q4;
  (void)y_step_q4;

  const uint16_t *const src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *const dst = CONVERT_TO_SHORTPTR(dst8);

  DECLARE_ALIGNED(32, uint16_t,
                  temp[(MAX_SB_SIZE + SUBPEL_TAPS - 1) * MAX_SB_SIZE]);
  int intermediate_height = h + SUBPEL_TAPS - 1;
  const int center_tap = ((SUBPEL_TAPS - 1) / 2);
  const uint16_t *const src_ptr = src - center_tap * src_stride - center_tap;

  const __m128i zero_128 = _mm_setzero_si128();
  const __m256i zero_256 = _mm256_setzero_si256();

  // Add an offset to account for the "add_src" part of the convolve function.
  const __m128i offset = _mm_insert_epi16(zero_128, 1 << FILTER_BITS, 3);

  const __m256i clamp_low = zero_256;

  /* Horizontal filter */
  {
    const __m256i clamp_high_ep =
        _mm256_set1_epi16(WIENER_CLAMP_LIMIT(conv_params->round_0, bd) - 1);

    // coeffs [ f7 f6 f5 f4 f3 f2 f1 f0 ]
    const __m128i coeffs_x = _mm_add_epi16(xx_loadu_128(filter_x), offset);

    // coeffs [ f3 f2 f3 f2 f1 f0 f1 f0 ]
    const __m128i coeffs_0123 = _mm_unpacklo_epi32(coeffs_x, coeffs_x);
    // coeffs [ f7 f6 f7 f6 f5 f4 f5 f4 ]
    const __m128i coeffs_4567 = _mm_unpackhi_epi32(coeffs_x, coeffs_x);

    // coeffs [ f1 f0 f1 f0 f1 f0 f1 f0 ]
    const __m128i coeffs_01_128 = _mm_unpacklo_epi64(coeffs_0123, coeffs_0123);
    // coeffs [ f3 f2 f3 f2 f3 f2 f3 f2 ]
    const __m128i coeffs_23_128 = _mm_unpackhi_epi64(coeffs_0123, coeffs_0123);
    // coeffs [ f5 f4 f5 f4 f5 f4 f5 f4 ]
    const __m128i coeffs_45_128 = _mm_unpacklo_epi64(coeffs_4567, coeffs_4567);
    // coeffs [ f7 f6 f7 f6 f7 f6 f7 f6 ]
    const __m128i coeffs_67_128 = _mm_unpackhi_epi64(coeffs_4567, coeffs_4567);

    // coeffs [ f1 f0 f1 f0 f1 f0 f1 f0 ][ f1 f0 f1 f0 f1 f0 f1 f0 ]
    const __m256i coeffs_01 = yy_set_m128i(coeffs_01_128, coeffs_01_128);
    // coeffs [ f3 f2 f3 f2 f3 f2 f3 f2 ][ f3 f2 f3 f2 f3 f2 f3 f2 ]
    const __m256i coeffs_23 = yy_set_m128i(coeffs_23_128, coeffs_23_128);
    // coeffs [ f5 f4 f5 f4 f5 f4 f5 f4 ][ f5 f4 f5 f4 f5 f4 f5 f4 ]
    const __m256i coeffs_45 = yy_set_m128i(coeffs_45_128, coeffs_45_128);
    // coeffs [ f7 f6 f7 f6 f7 f6 f7 f6 ][ f7 f6 f7 f6 f7 f6 f7 f6 ]
    const __m256i coeffs_67 = yy_set_m128i(coeffs_67_128, coeffs_67_128);

    const __m256i round_const = _mm256_set1_epi32(
        (1 << (conv_params->round_0 - 1)) + (1 << (bd + FILTER_BITS - 1)));

    for (int i = 0; i < intermediate_height; ++i) {
      for (int j = 0; j < w; j += 16) {
        const uint16_t *src_ij = src_ptr + i * src_stride + j;

        // Load 16-bit src data
        const __m256i src_0 = yy_loadu_256(src_ij + 0);
        const __m256i src_1 = yy_loadu_256(src_ij + 1);
        const __m256i src_2 = yy_loadu_256(src_ij + 2);
        const __m256i src_3 = yy_loadu_256(src_ij + 3);
        const __m256i src_4 = yy_loadu_256(src_ij + 4);
        const __m256i src_5 = yy_loadu_256(src_ij + 5);
        const __m256i src_6 = yy_loadu_256(src_ij + 6);
        const __m256i src_7 = yy_loadu_256(src_ij + 7);

        // Multiply src data by filter coeffs and sum pairs
        const __m256i res_0 = _mm256_madd_epi16(src_0, coeffs_01);
        const __m256i res_1 = _mm256_madd_epi16(src_1, coeffs_01);
        const __m256i res_2 = _mm256_madd_epi16(src_2, coeffs_23);
        const __m256i res_3 = _mm256_madd_epi16(src_3, coeffs_23);
        const __m256i res_4 = _mm256_madd_epi16(src_4, coeffs_45);
        const __m256i res_5 = _mm256_madd_epi16(src_5, coeffs_45);
        const __m256i res_6 = _mm256_madd_epi16(src_6, coeffs_67);
        const __m256i res_7 = _mm256_madd_epi16(src_7, coeffs_67);

        // Calculate scalar product for even- and odd-indices separately,
        // increasing to 32-bit precision
        const __m256i res_even_sum = _mm256_add_epi32(
            _mm256_add_epi32(res_0, res_4), _mm256_add_epi32(res_2, res_6));
        const __m256i res_even = _mm256_srai_epi32(
            _mm256_add_epi32(res_even_sum, round_const), conv_params->round_0);

        const __m256i res_odd_sum = _mm256_add_epi32(
            _mm256_add_epi32(res_1, res_5), _mm256_add_epi32(res_3, res_7));
        const __m256i res_odd = _mm256_srai_epi32(
            _mm256_add_epi32(res_odd_sum, round_const), conv_params->round_0);

        // Reduce to 16-bit precision and pack even- and odd-index results
        // back into one register. The _mm256_packs_epi32 intrinsic returns
        // a register with the pixels ordered as follows:
        // [ 15 13 11 9 14 12 10 8 ] [ 7 5 3 1 6 4 2 0 ]
        const __m256i res = _mm256_packs_epi32(res_even, res_odd);
        const __m256i res_clamped =
            _mm256_min_epi16(_mm256_max_epi16(res, clamp_low), clamp_high_ep);

        // Store in a temporary array
        yy_storeu_256(temp + i * MAX_SB_SIZE + j, res_clamped);
      }
    }
  }

  /* Vertical filter */
  {
    const __m256i clamp_high = _mm256_set1_epi16((1 << bd) - 1);

    // coeffs [ f7 f6 f5 f4 f3 f2 f1 f0 ]
    const __m128i coeffs_y = _mm_add_epi16(xx_loadu_128(filter_y), offset);

    // coeffs [ f3 f2 f3 f2 f1 f0 f1 f0 ]
    const __m128i coeffs_0123 = _mm_unpacklo_epi32(coeffs_y, coeffs_y);
    // coeffs [ f7 f6 f7 f6 f5 f4 f5 f4 ]
    const __m128i coeffs_4567 = _mm_unpackhi_epi32(coeffs_y, coeffs_y);

    // coeffs [ f1 f0 f1 f0 f1 f0 f1 f0 ]
    const __m128i coeffs_01_128 = _mm_unpacklo_epi64(coeffs_0123, coeffs_0123);
    // coeffs [ f3 f2 f3 f2 f3 f2 f3 f2 ]
    const __m128i coeffs_23_128 = _mm_unpackhi_epi64(coeffs_0123, coeffs_0123);
    // coeffs [ f5 f4 f5 f4 f5 f4 f5 f4 ]
    const __m128i coeffs_45_128 = _mm_unpacklo_epi64(coeffs_4567, coeffs_4567);
    // coeffs [ f7 f6 f7 f6 f7 f6 f7 f6 ]
    const __m128i coeffs_67_128 = _mm_unpackhi_epi64(coeffs_4567, coeffs_4567);

    // coeffs [ f1 f0 f1 f0 f1 f0 f1 f0 ][ f1 f0 f1 f0 f1 f0 f1 f0 ]
    const __m256i coeffs_01 = yy_set_m128i(coeffs_01_128, coeffs_01_128);
    // coeffs [ f3 f2 f3 f2 f3 f2 f3 f2 ][ f3 f2 f3 f2 f3 f2 f3 f2 ]
    const __m256i coeffs_23 = yy_set_m128i(coeffs_23_128, coeffs_23_128);
    // coeffs [ f5 f4 f5 f4 f5 f4 f5 f4 ][ f5 f4 f5 f4 f5 f4 f5 f4 ]
    const __m256i coeffs_45 = yy_set_m128i(coeffs_45_128, coeffs_45_128);
    // coeffs [ f7 f6 f7 f6 f7 f6 f7 f6 ][ f7 f6 f7 f6 f7 f6 f7 f6 ]
    const __m256i coeffs_67 = yy_set_m128i(coeffs_67_128, coeffs_67_128);

    const __m256i round_const =
        _mm256_set1_epi32((1 << (conv_params->round_1 - 1)) -
                          (1 << (bd + conv_params->round_1 - 1)));

    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; j += 16) {
        const uint16_t *temp_ij = temp + i * MAX_SB_SIZE + j;

        // Load 16-bit data from the output of the horizontal filter in
        // which the pixels are ordered as follows:
        // [ 15 13 11 9 14 12 10 8 ] [ 7 5 3 1 6 4 2 0 ]
        const __m256i data_0 = yy_loadu_256(temp_ij + 0 * MAX_SB_SIZE);
        const __m256i data_1 = yy_loadu_256(temp_ij + 1 * MAX_SB_SIZE);
        const __m256i data_2 = yy_loadu_256(temp_ij + 2 * MAX_SB_SIZE);
        const __m256i data_3 = yy_loadu_256(temp_ij + 3 * MAX_SB_SIZE);
        const __m256i data_4 = yy_loadu_256(temp_ij + 4 * MAX_SB_SIZE);
        const __m256i data_5 = yy_loadu_256(temp_ij + 5 * MAX_SB_SIZE);
        const __m256i data_6 = yy_loadu_256(temp_ij + 6 * MAX_SB_SIZE);
        const __m256i data_7 = yy_loadu_256(temp_ij + 7 * MAX_SB_SIZE);

        // Filter the even-indices, increasing to 32-bit precision
        const __m256i src_0 = _mm256_unpacklo_epi16(data_0, data_1);
        const __m256i src_2 = _mm256_unpacklo_epi16(data_2, data_3);
        const __m256i src_4 = _mm256_unpacklo_epi16(data_4, data_5);
        const __m256i src_6 = _mm256_unpacklo_epi16(data_6, data_7);

        const __m256i res_0 = _mm256_madd_epi16(src_0, coeffs_01);
        const __m256i res_2 = _mm256_madd_epi16(src_2, coeffs_23);
        const __m256i res_4 = _mm256_madd_epi16(src_4, coeffs_45);
        const __m256i res_6 = _mm256_madd_epi16(src_6, coeffs_67);

        const __m256i res_even = _mm256_add_epi32(
            _mm256_add_epi32(res_0, res_2), _mm256_add_epi32(res_4, res_6));

        // Filter the odd-indices, increasing to 32-bit precision
        const __m256i src_1 = _mm256_unpackhi_epi16(data_0, data_1);
        const __m256i src_3 = _mm256_unpackhi_epi16(data_2, data_3);
        const __m256i src_5 = _mm256_unpackhi_epi16(data_4, data_5);
        const __m256i src_7 = _mm256_unpackhi_epi16(data_6, data_7);

        const __m256i res_1 = _mm256_madd_epi16(src_1, coeffs_01);
        const __m256i res_3 = _mm256_madd_epi16(src_3, coeffs_23);
        const __m256i res_5 = _mm256_madd_epi16(src_5, coeffs_45);
        const __m256i res_7 = _mm256_madd_epi16(src_7, coeffs_67);

        const __m256i res_odd = _mm256_add_epi32(
            _mm256_add_epi32(res_1, res_3), _mm256_add_epi32(res_5, res_7));

        // Pixels are currently in the following order:
        // res_even order: [ 14 12 10 8 ] [ 6 4 2 0 ]
        // res_odd order:  [ 15 13 11 9 ] [ 7 5 3 1 ]
        //
        // Rearrange the pixels into the following order:
        // res_lo order: [ 11 10  9  8 ] [ 3 2 1 0 ]
        // res_hi order: [ 15 14 13 12 ] [ 7 6 5 4 ]
        const __m256i res_lo = _mm256_unpacklo_epi32(res_even, res_odd);
        const __m256i res_hi = _mm256_unpackhi_epi32(res_even, res_odd);

        const __m256i res_lo_round = _mm256_srai_epi32(
            _mm256_add_epi32(res_lo, round_const), conv_params->round_1);
        const __m256i res_hi_round = _mm256_srai_epi32(
            _mm256_add_epi32(res_hi, round_const), conv_params->round_1);

        // Reduce to 16-bit precision and pack into the correct order:
        // [ 15 14 13 12 11 10 9 8 ][ 7 6 5 4 3 2 1 0 ]
        const __m256i res_16bit =
            _mm256_packs_epi32(res_lo_round, res_hi_round);
        const __m256i res_16bit_clamped = _mm256_min_epi16(
            _mm256_max_epi16(res_16bit, clamp_low), clamp_high);

        // Store in the dst array
        yy_storeu_256(dst + i * dst_stride + j, res_16bit_clamped);
      }
    }
  }
}
