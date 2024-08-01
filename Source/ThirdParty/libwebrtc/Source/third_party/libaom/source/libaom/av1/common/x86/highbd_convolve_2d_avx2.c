/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
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

#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

void av1_highbd_convolve_2d_sr_ssse3(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd);

void av1_highbd_convolve_2d_sr_avx2(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams *filter_params_x,
                                    const InterpFilterParams *filter_params_y,
                                    const int subpel_x_qn,
                                    const int subpel_y_qn,
                                    ConvolveParams *conv_params, int bd) {
  if (filter_params_x->taps == 12) {
    av1_highbd_convolve_2d_sr_ssse3(src, src_stride, dst, dst_stride, w, h,
                                    filter_params_x, filter_params_y,
                                    subpel_x_qn, subpel_y_qn, conv_params, bd);
    return;
  }

  DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = 8;
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  // Check that, even with 12-bit input, the intermediate values will fit
  // into an unsigned 16-bit intermediate array.
  assert(bd + FILTER_BITS + 2 - conv_params->round_0 <= 16);

  __m256i s[8], coeffs_y[4], coeffs_x[4];

  const __m256i round_const_x = _mm256_set1_epi32(
      ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
  const __m128i round_shift_x = _mm_cvtsi32_si128(conv_params->round_0);

  const __m256i round_const_y = _mm256_set1_epi32(
      ((1 << conv_params->round_1) >> 1) -
      (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
  const __m128i round_shift_y = _mm_cvtsi32_si128(conv_params->round_1);

  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);
  const __m256i clip_pixel =
      _mm256_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m256i zero = _mm256_setzero_si256();

  prepare_coeffs(filter_params_x, subpel_x_qn, coeffs_x);
  prepare_coeffs(filter_params_y, subpel_y_qn, coeffs_y);

  for (j = 0; j < w; j += 8) {
    /* Horizontal filter */
    {
      for (i = 0; i < im_h; i += 2) {
        const __m256i row0 =
            _mm256_loadu_si256((__m256i *)&src_ptr[i * src_stride + j]);
        __m256i row1 = _mm256_setzero_si256();
        if (i + 1 < im_h)
          row1 =
              _mm256_loadu_si256((__m256i *)&src_ptr[(i + 1) * src_stride + j]);

        const __m256i r0 = _mm256_permute2x128_si256(row0, row1, 0x20);
        const __m256i r1 = _mm256_permute2x128_si256(row0, row1, 0x31);

        // even pixels
        s[0] = _mm256_alignr_epi8(r1, r0, 0);
        s[1] = _mm256_alignr_epi8(r1, r0, 4);
        s[2] = _mm256_alignr_epi8(r1, r0, 8);
        s[3] = _mm256_alignr_epi8(r1, r0, 12);

        __m256i res_even = convolve(s, coeffs_x);
        res_even = _mm256_sra_epi32(_mm256_add_epi32(res_even, round_const_x),
                                    round_shift_x);

        // odd pixels
        s[0] = _mm256_alignr_epi8(r1, r0, 2);
        s[1] = _mm256_alignr_epi8(r1, r0, 6);
        s[2] = _mm256_alignr_epi8(r1, r0, 10);
        s[3] = _mm256_alignr_epi8(r1, r0, 14);

        __m256i res_odd = convolve(s, coeffs_x);
        res_odd = _mm256_sra_epi32(_mm256_add_epi32(res_odd, round_const_x),
                                   round_shift_x);

        __m256i res_even1 = _mm256_packs_epi32(res_even, res_even);
        __m256i res_odd1 = _mm256_packs_epi32(res_odd, res_odd);
        __m256i res = _mm256_unpacklo_epi16(res_even1, res_odd1);

        _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);
      }
    }

    /* Vertical filter */
    {
      __m256i s0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));
      __m256i s1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));
      __m256i s2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));
      __m256i s3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));
      __m256i s4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));
      __m256i s5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));

      s[0] = _mm256_unpacklo_epi16(s0, s1);
      s[1] = _mm256_unpacklo_epi16(s2, s3);
      s[2] = _mm256_unpacklo_epi16(s4, s5);

      s[4] = _mm256_unpackhi_epi16(s0, s1);
      s[5] = _mm256_unpackhi_epi16(s2, s3);
      s[6] = _mm256_unpackhi_epi16(s4, s5);

      for (i = 0; i < h; i += 2) {
        const int16_t *data = &im_block[i * im_stride];

        const __m256i s6 =
            _mm256_loadu_si256((__m256i *)(data + 6 * im_stride));
        const __m256i s7 =
            _mm256_loadu_si256((__m256i *)(data + 7 * im_stride));

        s[3] = _mm256_unpacklo_epi16(s6, s7);
        s[7] = _mm256_unpackhi_epi16(s6, s7);

        const __m256i res_a = convolve(s, coeffs_y);
        __m256i res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a, round_const_y), round_shift_y);

        res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a_round, round_const_bits), round_shift_bits);

        if (w - j > 4) {
          const __m256i res_b = convolve(s + 4, coeffs_y);
          __m256i res_b_round = _mm256_sra_epi32(
              _mm256_add_epi32(res_b, round_const_y), round_shift_y);
          res_b_round =
              _mm256_sra_epi32(_mm256_add_epi32(res_b_round, round_const_bits),
                               round_shift_bits);

          __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);
          res_16bit = _mm256_min_epi16(res_16bit, clip_pixel);
          res_16bit = _mm256_max_epi16(res_16bit, zero);

          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j],
                           _mm256_castsi256_si128(res_16bit));
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           _mm256_extracti128_si256(res_16bit, 1));
        } else if (w == 4) {
          res_a_round = _mm256_packs_epi32(res_a_round, res_a_round);
          res_a_round = _mm256_min_epi16(res_a_round, clip_pixel);
          res_a_round = _mm256_max_epi16(res_a_round, zero);

          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j],
                           _mm256_castsi256_si128(res_a_round));
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           _mm256_extracti128_si256(res_a_round, 1));
        } else {
          res_a_round = _mm256_packs_epi32(res_a_round, res_a_round);
          res_a_round = _mm256_min_epi16(res_a_round, clip_pixel);
          res_a_round = _mm256_max_epi16(res_a_round, zero);

          xx_storel_32(&dst[i * dst_stride + j],
                       _mm256_castsi256_si128(res_a_round));
          xx_storel_32(&dst[i * dst_stride + j + dst_stride],
                       _mm256_extracti128_si256(res_a_round, 1));
        }

        s[0] = s[1];
        s[1] = s[2];
        s[2] = s[3];

        s[4] = s[5];
        s[5] = s[6];
        s[6] = s[7];
      }
    }
  }
}
