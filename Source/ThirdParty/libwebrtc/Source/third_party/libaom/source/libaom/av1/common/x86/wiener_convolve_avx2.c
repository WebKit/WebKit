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
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"

// 128-bit xmmwords are written as [ ... ] with the MSB on the left.
// 256-bit ymmwords are written as two xmmwords, [ ... ][ ... ] with the MSB
// on the left.
// A row of, say, 8-bit pixels with values p0, p1, p2, ..., p30, p31 will be
// loaded and stored as [ p31 ... p17 p16 ][ p15 ... p1 p0 ].

// Exploiting the range of wiener filter coefficients,
// horizontal filtering can be done in 16 bit intermediate precision.
// The details are as follows :
// Consider the horizontal wiener filter coefficients of the following form :
//      [C0, C1, C2, 2^(FILTER_BITS) -2 * (C0 + C1 + C2), C2, C1, C0]
// Subtracting  2^(FILTER_BITS) from the centre tap we get the following  :
//      [C0, C1, C2,     -2 * (C0 + C1 + C2),             C2, C1, C0]
// The sum of the product "C0 * p0 + C1 * p1 + C2 * p2 -2 * (C0 + C1 + C2) * p3
// + C2 * p4 + C1 * p5 + C0 * p6" would be in the range of signed 16 bit
// precision. Finally, after rounding the above result by round_0, we multiply
// the centre pixel by 2^(FILTER_BITS - round_0) and add it to get the
// horizontal filter output.

void av1_wiener_convolve_add_src_avx2(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const int16_t *filter_x, int x_step_q4,
                                      const int16_t *filter_y, int y_step_q4,
                                      int w, int h,
                                      const WienerConvolveParams *conv_params) {
  const int bd = 8;
  assert(x_step_q4 == 16 && y_step_q4 == 16);
  assert(!(w & 7));
  (void)x_step_q4;
  (void)y_step_q4;

  DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + SUBPEL_TAPS) * 8]);
  int im_h = h + SUBPEL_TAPS - 2;
  int im_stride = 8;
  memset(im_block + (im_h * im_stride), 0, MAX_SB_SIZE);
  int i, j;
  const int center_tap = (SUBPEL_TAPS - 1) / 2;
  const uint8_t *const src_ptr = src - center_tap * src_stride - center_tap;

  __m256i filt[4], coeffs_h[4], coeffs_v[4], filt_center;

  assert(conv_params->round_0 > 0);

  filt[0] = _mm256_load_si256((__m256i const *)filt1_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)filt2_global_avx2);
  filt[2] = _mm256_load_si256((__m256i const *)filt3_global_avx2);
  filt[3] = _mm256_load_si256((__m256i const *)filt4_global_avx2);

  filt_center = _mm256_load_si256((__m256i const *)filt_center_global_avx2);

  const __m128i coeffs_x = _mm_loadu_si128((__m128i *)filter_x);
  const __m256i filter_coeffs_x = _mm256_broadcastsi128_si256(coeffs_x);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs_h[0] =
      _mm256_shuffle_epi8(filter_coeffs_x, _mm256_set1_epi16(0x0200u));
  // coeffs 2 3 2 3 2 3 2 3
  coeffs_h[1] =
      _mm256_shuffle_epi8(filter_coeffs_x, _mm256_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs_h[2] =
      _mm256_shuffle_epi8(filter_coeffs_x, _mm256_set1_epi16(0x0a08u));
  // coeffs 6 7 6 7 6 7 6 7
  coeffs_h[3] =
      _mm256_shuffle_epi8(filter_coeffs_x, _mm256_set1_epi16(0x0e0cu));

  const __m256i round_const_h =
      _mm256_set1_epi16((1 << (conv_params->round_0 - 1)));
  const __m256i round_const_horz =
      _mm256_set1_epi16((1 << (bd + FILTER_BITS - conv_params->round_0 - 1)));
  const __m256i clamp_low = _mm256_setzero_si256();
  const __m256i clamp_high =
      _mm256_set1_epi16(WIENER_CLAMP_LIMIT(conv_params->round_0, bd) - 1);
  const __m128i round_shift_h = _mm_cvtsi32_si128(conv_params->round_0);

  // Add an offset to account for the "add_src" part of the convolve function.
  const __m128i zero_128 = _mm_setzero_si128();
  const __m128i offset_0 = _mm_insert_epi16(zero_128, 1 << FILTER_BITS, 3);
  const __m128i coeffs_y = _mm_add_epi16(xx_loadu_128(filter_y), offset_0);

  const __m256i filter_coeffs_y = _mm256_broadcastsi128_si256(coeffs_y);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs_v[0] = _mm256_shuffle_epi32(filter_coeffs_y, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs_v[1] = _mm256_shuffle_epi32(filter_coeffs_y, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs_v[2] = _mm256_shuffle_epi32(filter_coeffs_y, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs_v[3] = _mm256_shuffle_epi32(filter_coeffs_y, 0xff);

  const __m256i round_const_v =
      _mm256_set1_epi32((1 << (conv_params->round_1 - 1)) -
                        (1 << (bd + conv_params->round_1 - 1)));
  const __m128i round_shift_v = _mm_cvtsi32_si128(conv_params->round_1);

  for (j = 0; j < w; j += 8) {
    for (i = 0; i < im_h; i += 2) {
      __m256i data = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));

      // Load the next line
      if (i + 1 < im_h)
        data = _mm256_inserti128_si256(
            data,
            _mm_loadu_si128(
                (__m128i *)&src_ptr[(i * src_stride) + j + src_stride]),
            1);

      __m256i res = convolve_lowbd_x(data, coeffs_h, filt);

      res =
          _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h);

      __m256i data_0 = _mm256_shuffle_epi8(data, filt_center);

      // multiply the center pixel by 2^(FILTER_BITS - round_0) and add it to
      // the result
      data_0 = _mm256_slli_epi16(data_0, FILTER_BITS - conv_params->round_0);
      res = _mm256_add_epi16(res, data_0);
      res = _mm256_add_epi16(res, round_const_horz);
      const __m256i res_clamped =
          _mm256_min_epi16(_mm256_max_epi16(res, clamp_low), clamp_high);
      _mm256_store_si256((__m256i *)&im_block[i * im_stride], res_clamped);
    }

    /* Vertical filter */
    {
      __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));
      __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));
      __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));
      __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));
      __m256i src_4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));
      __m256i src_5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));

      __m256i s[8];
      s[0] = _mm256_unpacklo_epi16(src_0, src_1);
      s[1] = _mm256_unpacklo_epi16(src_2, src_3);
      s[2] = _mm256_unpacklo_epi16(src_4, src_5);

      s[4] = _mm256_unpackhi_epi16(src_0, src_1);
      s[5] = _mm256_unpackhi_epi16(src_2, src_3);
      s[6] = _mm256_unpackhi_epi16(src_4, src_5);

      for (i = 0; i < h - 1; i += 2) {
        const int16_t *data = &im_block[i * im_stride];

        const __m256i s6 =
            _mm256_loadu_si256((__m256i *)(data + 6 * im_stride));
        const __m256i s7 =
            _mm256_loadu_si256((__m256i *)(data + 7 * im_stride));

        s[3] = _mm256_unpacklo_epi16(s6, s7);
        s[7] = _mm256_unpackhi_epi16(s6, s7);

        __m256i res_a = convolve(s, coeffs_v);
        __m256i res_b = convolve(s + 4, coeffs_v);

        const __m256i res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a, round_const_v), round_shift_v);
        const __m256i res_b_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_b, round_const_v), round_shift_v);

        /* rounding code */
        // 16 bit conversion
        const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);
        // 8 bit conversion and saturation to uint8
        const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);

        // Store values into the destination buffer
        __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
        __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];

        _mm_storel_epi64(p_0, res_0);
        _mm_storel_epi64(p_1, res_1);

        s[0] = s[1];
        s[1] = s[2];
        s[2] = s[3];

        s[4] = s[5];
        s[5] = s[6];
        s[6] = s[7];
      }
      if (h - i) {
        s[0] = _mm256_permute2x128_si256(s[0], s[4], 0x20);
        s[1] = _mm256_permute2x128_si256(s[1], s[5], 0x20);
        s[2] = _mm256_permute2x128_si256(s[2], s[6], 0x20);

        const int16_t *data = &im_block[i * im_stride];
        const __m128i s6_ = _mm_loadu_si128((__m128i *)(data + 6 * im_stride));
        const __m128i s7_ = _mm_loadu_si128((__m128i *)(data + 7 * im_stride));

        __m128i s3 = _mm_unpacklo_epi16(s6_, s7_);
        __m128i s7 = _mm_unpackhi_epi16(s6_, s7_);

        s[3] = _mm256_inserti128_si256(_mm256_castsi128_si256(s3), s7, 1);
        __m256i convolveres = convolve(s, coeffs_v);

        const __m256i res_round = _mm256_sra_epi32(
            _mm256_add_epi32(convolveres, round_const_v), round_shift_v);

        /* rounding code */
        // 16 bit conversion
        __m128i reslo = _mm256_castsi256_si128(res_round);
        __m128i reshi = _mm256_extracti128_si256(res_round, 1);
        const __m128i res_16bit = _mm_packus_epi32(reslo, reshi);

        // 8 bit conversion and saturation to uint8
        const __m128i res_8b = _mm_packus_epi16(res_16bit, res_16bit);
        __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
        _mm_storel_epi64(p_0, res_8b);
      }
    }
  }
}
