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

#include <tmmintrin.h>
#include <assert.h>

#include "config/av1_rtcd.h"

#include "av1/common/convolve.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"

void av1_highbd_wiener_convolve_add_src_ssse3(
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

  DECLARE_ALIGNED(16, uint16_t,
                  temp[(MAX_SB_SIZE + SUBPEL_TAPS - 1) * MAX_SB_SIZE]);
  int intermediate_height = h + SUBPEL_TAPS - 1;
  int i, j;
  const int center_tap = ((SUBPEL_TAPS - 1) / 2);
  const uint16_t *const src_ptr = src - center_tap * src_stride - center_tap;

  const __m128i zero = _mm_setzero_si128();
  // Add an offset to account for the "add_src" part of the convolve function.
  const __m128i offset = _mm_insert_epi16(zero, 1 << FILTER_BITS, 3);

  /* Horizontal filter */
  {
    const __m128i coeffs_x =
        _mm_add_epi16(_mm_loadu_si128((__m128i *)filter_x), offset);

    // coeffs 0 1 0 1 2 3 2 3
    const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_x, coeffs_x);
    // coeffs 4 5 4 5 6 7 6 7
    const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_x, coeffs_x);

    // coeffs 0 1 0 1 0 1 0 1
    const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

    const __m128i round_const = _mm_set1_epi32(
        (1 << (conv_params->round_0 - 1)) + (1 << (bd + FILTER_BITS - 1)));

    for (i = 0; i < intermediate_height; ++i) {
      for (j = 0; j < w; j += 8) {
        const __m128i data =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);
        const __m128i data2 =
            _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j + 8]);

        // Filter even-index pixels
        const __m128i res_0 = _mm_madd_epi16(data, coeff_01);
        const __m128i res_2 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 4), coeff_23);
        const __m128i res_4 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 8), coeff_45);
        const __m128i res_6 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 12), coeff_67);

        __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_4),
                                         _mm_add_epi32(res_2, res_6));
        res_even = _mm_srai_epi32(_mm_add_epi32(res_even, round_const),
                                  conv_params->round_0);

        // Filter odd-index pixels
        const __m128i res_1 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 2), coeff_01);
        const __m128i res_3 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 6), coeff_23);
        const __m128i res_5 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 10), coeff_45);
        const __m128i res_7 =
            _mm_madd_epi16(_mm_alignr_epi8(data2, data, 14), coeff_67);

        __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_5),
                                        _mm_add_epi32(res_3, res_7));
        res_odd = _mm_srai_epi32(_mm_add_epi32(res_odd, round_const),
                                 conv_params->round_0);

        // Pack in the column order 0, 2, 4, 6, 1, 3, 5, 7
        const __m128i maxval =
            _mm_set1_epi16((WIENER_CLAMP_LIMIT(conv_params->round_0, bd)) - 1);
        __m128i res = _mm_packs_epi32(res_even, res_odd);
        res = _mm_min_epi16(_mm_max_epi16(res, zero), maxval);
        _mm_storeu_si128((__m128i *)&temp[i * MAX_SB_SIZE + j], res);
      }
    }
  }

  /* Vertical filter */
  {
    const __m128i coeffs_y =
        _mm_add_epi16(_mm_loadu_si128((__m128i *)filter_y), offset);

    // coeffs 0 1 0 1 2 3 2 3
    const __m128i tmp_0 = _mm_unpacklo_epi32(coeffs_y, coeffs_y);
    // coeffs 4 5 4 5 6 7 6 7
    const __m128i tmp_1 = _mm_unpackhi_epi32(coeffs_y, coeffs_y);

    // coeffs 0 1 0 1 0 1 0 1
    const __m128i coeff_01 = _mm_unpacklo_epi64(tmp_0, tmp_0);
    // coeffs 2 3 2 3 2 3 2 3
    const __m128i coeff_23 = _mm_unpackhi_epi64(tmp_0, tmp_0);
    // coeffs 4 5 4 5 4 5 4 5
    const __m128i coeff_45 = _mm_unpacklo_epi64(tmp_1, tmp_1);
    // coeffs 6 7 6 7 6 7 6 7
    const __m128i coeff_67 = _mm_unpackhi_epi64(tmp_1, tmp_1);

    const __m128i round_const =
        _mm_set1_epi32((1 << (conv_params->round_1 - 1)) -
                       (1 << (bd + conv_params->round_1 - 1)));

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        // Filter even-index pixels
        const uint16_t *data = &temp[i * MAX_SB_SIZE + j];
        const __m128i src_0 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 0 * MAX_SB_SIZE),
                               *(__m128i *)(data + 1 * MAX_SB_SIZE));
        const __m128i src_2 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 2 * MAX_SB_SIZE),
                               *(__m128i *)(data + 3 * MAX_SB_SIZE));
        const __m128i src_4 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 4 * MAX_SB_SIZE),
                               *(__m128i *)(data + 5 * MAX_SB_SIZE));
        const __m128i src_6 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 6 * MAX_SB_SIZE),
                               *(__m128i *)(data + 7 * MAX_SB_SIZE));

        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                               _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m128i src_1 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 0 * MAX_SB_SIZE),
                               *(__m128i *)(data + 1 * MAX_SB_SIZE));
        const __m128i src_3 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 2 * MAX_SB_SIZE),
                               *(__m128i *)(data + 3 * MAX_SB_SIZE));
        const __m128i src_5 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 4 * MAX_SB_SIZE),
                               *(__m128i *)(data + 5 * MAX_SB_SIZE));
        const __m128i src_7 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 6 * MAX_SB_SIZE),
                               *(__m128i *)(data + 7 * MAX_SB_SIZE));

        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                              _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        const __m128i res_lo_round = _mm_srai_epi32(
            _mm_add_epi32(res_lo, round_const), conv_params->round_1);
        const __m128i res_hi_round = _mm_srai_epi32(
            _mm_add_epi32(res_hi, round_const), conv_params->round_1);

        const __m128i maxval = _mm_set1_epi16((1 << bd) - 1);
        __m128i res_16bit = _mm_packs_epi32(res_lo_round, res_hi_round);
        res_16bit = _mm_min_epi16(_mm_max_epi16(res_16bit, zero), maxval);

        __m128i *const p = (__m128i *)&dst[i * dst_stride + j];
        _mm_storeu_si128(p, res_16bit);
      }
    }
  }
}
