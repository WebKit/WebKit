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

#include <smmintrin.h>
#include <assert.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/x86/convolve_sse2.h"
#include "aom_dsp/x86/convolve_sse4_1.h"

void av1_highbd_dist_wtd_convolve_y_sse4_1(
    const uint16_t *src, int src_stride, uint16_t *dst0, int dst_stride0, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    ConvolveParams *conv_params, int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_vert * src_stride;
  const int bits = FILTER_BITS - conv_params->round_0;

  assert(bits >= 0);
  int i, j;
  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi32(w0);
  const __m128i wt1 = _mm_set1_epi32(w1);
  const __m128i round_const_y =
      _mm_set1_epi32(((1 << conv_params->round_1) >> 1));
  const __m128i round_shift_y = _mm_cvtsi32_si128(conv_params->round_1);
  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);

  const int offset_0 =
      bd + 2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = (1 << offset_0) + (1 << (offset_0 - 1));
  const __m128i offset_const = _mm_set1_epi32(offset);
  const int rounding_shift =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m128i rounding_const = _mm_set1_epi32((1 << rounding_shift) >> 1);
  const __m128i clip_pixel_to_bd =
      _mm_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m128i zero = _mm_setzero_si128();
  __m128i s[16], coeffs_y[4];

  prepare_coeffs(filter_params_y, subpel_y_qn, coeffs_y);

  for (j = 0; j < w; j += 8) {
    const uint16_t *data = &src_ptr[j];
    /* Vertical filter */
    {
      __m128i s0 = _mm_loadu_si128((__m128i *)(data + 0 * src_stride));
      __m128i s1 = _mm_loadu_si128((__m128i *)(data + 1 * src_stride));
      __m128i s2 = _mm_loadu_si128((__m128i *)(data + 2 * src_stride));
      __m128i s3 = _mm_loadu_si128((__m128i *)(data + 3 * src_stride));
      __m128i s4 = _mm_loadu_si128((__m128i *)(data + 4 * src_stride));
      __m128i s5 = _mm_loadu_si128((__m128i *)(data + 5 * src_stride));
      __m128i s6 = _mm_loadu_si128((__m128i *)(data + 6 * src_stride));

      s[0] = _mm_unpacklo_epi16(s0, s1);
      s[1] = _mm_unpacklo_epi16(s2, s3);
      s[2] = _mm_unpacklo_epi16(s4, s5);

      s[4] = _mm_unpackhi_epi16(s0, s1);
      s[5] = _mm_unpackhi_epi16(s2, s3);
      s[6] = _mm_unpackhi_epi16(s4, s5);

      s[0 + 8] = _mm_unpacklo_epi16(s1, s2);
      s[1 + 8] = _mm_unpacklo_epi16(s3, s4);
      s[2 + 8] = _mm_unpacklo_epi16(s5, s6);

      s[4 + 8] = _mm_unpackhi_epi16(s1, s2);
      s[5 + 8] = _mm_unpackhi_epi16(s3, s4);
      s[6 + 8] = _mm_unpackhi_epi16(s5, s6);

      for (i = 0; i < h; i += 2) {
        data = &src_ptr[i * src_stride + j];

        __m128i s7 = _mm_loadu_si128((__m128i *)(data + 7 * src_stride));
        __m128i s8 = _mm_loadu_si128((__m128i *)(data + 8 * src_stride));

        s[3] = _mm_unpacklo_epi16(s6, s7);
        s[7] = _mm_unpackhi_epi16(s6, s7);

        s[3 + 8] = _mm_unpacklo_epi16(s7, s8);
        s[7 + 8] = _mm_unpackhi_epi16(s7, s8);

        const __m128i res_a0 = convolve(s, coeffs_y);
        __m128i res_a_round0 = _mm_sll_epi32(res_a0, round_shift_bits);
        res_a_round0 = _mm_sra_epi32(_mm_add_epi32(res_a_round0, round_const_y),
                                     round_shift_y);

        const __m128i res_a1 = convolve(s + 8, coeffs_y);
        __m128i res_a_round1 = _mm_sll_epi32(res_a1, round_shift_bits);
        res_a_round1 = _mm_sra_epi32(_mm_add_epi32(res_a_round1, round_const_y),
                                     round_shift_y);

        __m128i res_unsigned_lo_0 = _mm_add_epi32(res_a_round0, offset_const);
        __m128i res_unsigned_lo_1 = _mm_add_epi32(res_a_round1, offset_const);

        if (w - j < 8) {
          if (do_average) {
            const __m128i data_0 =
                _mm_loadl_epi64((__m128i *)(&dst[i * dst_stride + j]));
            const __m128i data_1 = _mm_loadl_epi64(
                (__m128i *)(&dst[i * dst_stride + j + dst_stride]));

            const __m128i data_ref_0 = _mm_unpacklo_epi16(data_0, zero);
            const __m128i data_ref_1 = _mm_unpacklo_epi16(data_1, zero);

            const __m128i comp_avg_res_0 =
                highbd_comp_avg_sse4_1(&data_ref_0, &res_unsigned_lo_0, &wt0,
                                       &wt1, use_dist_wtd_comp_avg);
            const __m128i comp_avg_res_1 =
                highbd_comp_avg_sse4_1(&data_ref_1, &res_unsigned_lo_1, &wt0,
                                       &wt1, use_dist_wtd_comp_avg);

            const __m128i round_result_0 =
                highbd_convolve_rounding_sse2(&comp_avg_res_0, &offset_const,
                                              &rounding_const, rounding_shift);
            const __m128i round_result_1 =
                highbd_convolve_rounding_sse2(&comp_avg_res_1, &offset_const,
                                              &rounding_const, rounding_shift);

            const __m128i res_16b_0 =
                _mm_packus_epi32(round_result_0, round_result_0);
            const __m128i res_clip_0 =
                _mm_min_epi16(res_16b_0, clip_pixel_to_bd);
            const __m128i res_16b_1 =
                _mm_packus_epi32(round_result_1, round_result_1);
            const __m128i res_clip_1 =
                _mm_min_epi16(res_16b_1, clip_pixel_to_bd);

            _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]),
                             res_clip_0);
            _mm_storel_epi64(
                (__m128i *)(&dst0[i * dst_stride0 + j + dst_stride0]),
                res_clip_1);

          } else {
            __m128i res_16b_0 =
                _mm_packus_epi32(res_unsigned_lo_0, res_unsigned_lo_0);

            __m128i res_16b_1 =
                _mm_packus_epi32(res_unsigned_lo_1, res_unsigned_lo_1);

            _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_16b_0);
            _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                             res_16b_1);
          }
        } else {
          const __m128i res_b0 = convolve(s + 4, coeffs_y);
          __m128i res_b_round0 = _mm_sll_epi32(res_b0, round_shift_bits);
          res_b_round0 = _mm_sra_epi32(
              _mm_add_epi32(res_b_round0, round_const_y), round_shift_y);

          const __m128i res_b1 = convolve(s + 4 + 8, coeffs_y);
          __m128i res_b_round1 = _mm_sll_epi32(res_b1, round_shift_bits);
          res_b_round1 = _mm_sra_epi32(
              _mm_add_epi32(res_b_round1, round_const_y), round_shift_y);

          __m128i res_unsigned_hi_0 = _mm_add_epi32(res_b_round0, offset_const);
          __m128i res_unsigned_hi_1 = _mm_add_epi32(res_b_round1, offset_const);

          if (do_average) {
            const __m128i data_0 =
                _mm_loadu_si128((__m128i *)(&dst[i * dst_stride + j]));
            const __m128i data_1 = _mm_loadu_si128(
                (__m128i *)(&dst[i * dst_stride + j + dst_stride]));
            const __m128i data_ref_0_lo_0 = _mm_unpacklo_epi16(data_0, zero);
            const __m128i data_ref_0_lo_1 = _mm_unpacklo_epi16(data_1, zero);

            const __m128i data_ref_0_hi_0 = _mm_unpackhi_epi16(data_0, zero);
            const __m128i data_ref_0_hi_1 = _mm_unpackhi_epi16(data_1, zero);

            const __m128i comp_avg_res_lo_0 =
                highbd_comp_avg_sse4_1(&data_ref_0_lo_0, &res_unsigned_lo_0,
                                       &wt0, &wt1, use_dist_wtd_comp_avg);
            const __m128i comp_avg_res_lo_1 =
                highbd_comp_avg_sse4_1(&data_ref_0_lo_1, &res_unsigned_lo_1,
                                       &wt0, &wt1, use_dist_wtd_comp_avg);
            const __m128i comp_avg_res_hi_0 =
                highbd_comp_avg_sse4_1(&data_ref_0_hi_0, &res_unsigned_hi_0,
                                       &wt0, &wt1, use_dist_wtd_comp_avg);
            const __m128i comp_avg_res_hi_1 =
                highbd_comp_avg_sse4_1(&data_ref_0_hi_1, &res_unsigned_hi_1,
                                       &wt0, &wt1, use_dist_wtd_comp_avg);

            const __m128i round_result_lo_0 =
                highbd_convolve_rounding_sse2(&comp_avg_res_lo_0, &offset_const,
                                              &rounding_const, rounding_shift);
            const __m128i round_result_lo_1 =
                highbd_convolve_rounding_sse2(&comp_avg_res_lo_1, &offset_const,
                                              &rounding_const, rounding_shift);
            const __m128i round_result_hi_0 =
                highbd_convolve_rounding_sse2(&comp_avg_res_hi_0, &offset_const,
                                              &rounding_const, rounding_shift);
            const __m128i round_result_hi_1 =
                highbd_convolve_rounding_sse2(&comp_avg_res_hi_1, &offset_const,
                                              &rounding_const, rounding_shift);

            const __m128i res_16b_0 =
                _mm_packus_epi32(round_result_lo_0, round_result_hi_0);
            const __m128i res_clip_0 =
                _mm_min_epi16(res_16b_0, clip_pixel_to_bd);

            const __m128i res_16b_1 =
                _mm_packus_epi32(round_result_lo_1, round_result_hi_1);
            const __m128i res_clip_1 =
                _mm_min_epi16(res_16b_1, clip_pixel_to_bd);

            _mm_store_si128((__m128i *)(&dst0[i * dst_stride0 + j]),
                            res_clip_0);
            _mm_store_si128(
                (__m128i *)(&dst0[i * dst_stride0 + j + dst_stride0]),
                res_clip_1);
          } else {
            __m128i res_16bit0 =
                _mm_packus_epi32(res_unsigned_lo_0, res_unsigned_hi_0);
            __m128i res_16bit1 =
                _mm_packus_epi32(res_unsigned_lo_1, res_unsigned_hi_1);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_16bit0);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                            res_16bit1);
          }
        }
        s[0] = s[1];
        s[1] = s[2];
        s[2] = s[3];

        s[4] = s[5];
        s[5] = s[6];
        s[6] = s[7];

        s[0 + 8] = s[1 + 8];
        s[1 + 8] = s[2 + 8];
        s[2 + 8] = s[3 + 8];

        s[4 + 8] = s[5 + 8];
        s[5 + 8] = s[6 + 8];
        s[6 + 8] = s[7 + 8];

        s6 = s8;
      }
    }
  }
}

void av1_highbd_dist_wtd_convolve_x_sse4_1(
    const uint16_t *src, int src_stride, uint16_t *dst0, int dst_stride0, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params, int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_horiz;
  const int bits = FILTER_BITS - conv_params->round_1;

  int i, j;
  __m128i s[4], coeffs_x[4];

  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;
  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi32(w0);
  const __m128i wt1 = _mm_set1_epi32(w1);
  const __m128i zero = _mm_setzero_si128();

  const __m128i round_const_x =
      _mm_set1_epi32(((1 << conv_params->round_0) >> 1));
  const __m128i round_shift_x = _mm_cvtsi32_si128(conv_params->round_0);
  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);

  const int offset_0 =
      bd + 2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = (1 << offset_0) + (1 << (offset_0 - 1));
  const __m128i offset_const = _mm_set1_epi32(offset);
  const int rounding_shift =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m128i rounding_const = _mm_set1_epi32((1 << rounding_shift) >> 1);
  const __m128i clip_pixel_to_bd =
      _mm_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));

  assert(bits >= 0);
  prepare_coeffs(filter_params_x, subpel_x_qn, coeffs_x);

  for (j = 0; j < w; j += 8) {
    /* Horizontal filter */
    for (i = 0; i < h; i += 1) {
      const __m128i row00 =
          _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + j]);
      const __m128i row01 =
          _mm_loadu_si128((__m128i *)&src_ptr[i * src_stride + (j + 8)]);

      // even pixels
      s[0] = _mm_alignr_epi8(row01, row00, 0);
      s[1] = _mm_alignr_epi8(row01, row00, 4);
      s[2] = _mm_alignr_epi8(row01, row00, 8);
      s[3] = _mm_alignr_epi8(row01, row00, 12);

      __m128i res_even = convolve(s, coeffs_x);
      res_even =
          _mm_sra_epi32(_mm_add_epi32(res_even, round_const_x), round_shift_x);

      // odd pixels
      s[0] = _mm_alignr_epi8(row01, row00, 2);
      s[1] = _mm_alignr_epi8(row01, row00, 6);
      s[2] = _mm_alignr_epi8(row01, row00, 10);
      s[3] = _mm_alignr_epi8(row01, row00, 14);

      __m128i res_odd = convolve(s, coeffs_x);
      res_odd =
          _mm_sra_epi32(_mm_add_epi32(res_odd, round_const_x), round_shift_x);

      res_even = _mm_sll_epi32(res_even, round_shift_bits);
      res_odd = _mm_sll_epi32(res_odd, round_shift_bits);

      __m128i res1 = _mm_unpacklo_epi32(res_even, res_odd);
      __m128i res_unsigned_lo = _mm_add_epi32(res1, offset_const);
      if (w - j < 8) {
        if (do_average) {
          const __m128i data_0 =
              _mm_loadl_epi64((__m128i *)(&dst[i * dst_stride + j]));
          const __m128i data_ref_0 = _mm_unpacklo_epi16(data_0, zero);

          const __m128i comp_avg_res = highbd_comp_avg_sse4_1(
              &data_ref_0, &res_unsigned_lo, &wt0, &wt1, use_dist_wtd_comp_avg);
          const __m128i round_result = highbd_convolve_rounding_sse2(
              &comp_avg_res, &offset_const, &rounding_const, rounding_shift);

          const __m128i res_16b = _mm_packus_epi32(round_result, round_result);
          const __m128i res_clip = _mm_min_epi16(res_16b, clip_pixel_to_bd);
          _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_clip);
        } else {
          __m128i res_16b = _mm_packus_epi32(res_unsigned_lo, res_unsigned_lo);
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_16b);
        }
      } else {
        __m128i res2 = _mm_unpackhi_epi32(res_even, res_odd);
        __m128i res_unsigned_hi = _mm_add_epi32(res2, offset_const);
        if (do_average) {
          const __m128i data_0 =
              _mm_loadu_si128((__m128i *)(&dst[i * dst_stride + j]));
          const __m128i data_ref_0_lo = _mm_unpacklo_epi16(data_0, zero);
          const __m128i data_ref_0_hi = _mm_unpackhi_epi16(data_0, zero);

          const __m128i comp_avg_res_lo =
              highbd_comp_avg_sse4_1(&data_ref_0_lo, &res_unsigned_lo, &wt0,
                                     &wt1, use_dist_wtd_comp_avg);
          const __m128i comp_avg_res_hi =
              highbd_comp_avg_sse4_1(&data_ref_0_hi, &res_unsigned_hi, &wt0,
                                     &wt1, use_dist_wtd_comp_avg);

          const __m128i round_result_lo = highbd_convolve_rounding_sse2(
              &comp_avg_res_lo, &offset_const, &rounding_const, rounding_shift);
          const __m128i round_result_hi = highbd_convolve_rounding_sse2(
              &comp_avg_res_hi, &offset_const, &rounding_const, rounding_shift);

          const __m128i res_16b =
              _mm_packus_epi32(round_result_lo, round_result_hi);
          const __m128i res_clip = _mm_min_epi16(res_16b, clip_pixel_to_bd);
          _mm_store_si128((__m128i *)(&dst0[i * dst_stride0 + j]), res_clip);
        } else {
          __m128i res_16b = _mm_packus_epi32(res_unsigned_lo, res_unsigned_hi);
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_16b);
        }
      }
    }
  }
}
