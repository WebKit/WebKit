/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tmmintrin.h>
#include <smmintrin.h>
#include <assert.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/convolve_sse2.h"
#include "aom_dsp/x86/convolve_sse4_1.h"
#include "av1/common/convolve.h"

void av1_highbd_dist_wtd_convolve_2d_copy_sse4_1(const uint16_t *src,
                                                 int src_stride, uint16_t *dst0,
                                                 int dst_stride0, int w, int h,
                                                 ConvolveParams *conv_params,
                                                 int bd) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;

  const int bits =
      FILTER_BITS * 2 - conv_params->round_1 - conv_params->round_0;
  const __m128i left_shift = _mm_cvtsi32_si128(bits);
  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;
  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi32(w0);
  const __m128i wt1 = _mm_set1_epi32(w1);
  const __m128i zero = _mm_setzero_si128();
  int i, j;

  const int offset_0 =
      bd + 2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = (1 << offset_0) + (1 << (offset_0 - 1));
  const __m128i offset_const = _mm_set1_epi32(offset);
  const __m128i offset_const_16b = _mm_set1_epi16(offset);
  const int rounding_shift =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m128i rounding_const = _mm_set1_epi32((1 << rounding_shift) >> 1);
  const __m128i clip_pixel_to_bd =
      _mm_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));

  assert(bits <= 4);

  if (!(w % 8)) {
    for (i = 0; i < h; i += 1) {
      for (j = 0; j < w; j += 8) {
        const __m128i src_16bit =
            _mm_loadu_si128((__m128i *)(&src[i * src_stride + j]));
        const __m128i res = _mm_sll_epi16(src_16bit, left_shift);
        if (do_average) {
          const __m128i data_0 =
              _mm_loadu_si128((__m128i *)(&dst[i * dst_stride + j]));

          const __m128i data_ref_0_lo = _mm_unpacklo_epi16(data_0, zero);
          const __m128i data_ref_0_hi = _mm_unpackhi_epi16(data_0, zero);

          const __m128i res_32b_lo = _mm_unpacklo_epi16(res, zero);
          const __m128i res_unsigned_lo =
              _mm_add_epi32(res_32b_lo, offset_const);

          const __m128i comp_avg_res_lo =
              highbd_comp_avg_sse4_1(&data_ref_0_lo, &res_unsigned_lo, &wt0,
                                     &wt1, use_dist_wtd_comp_avg);

          const __m128i res_32b_hi = _mm_unpackhi_epi16(res, zero);
          const __m128i res_unsigned_hi =
              _mm_add_epi32(res_32b_hi, offset_const);

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
          const __m128i res_unsigned_16b =
              _mm_adds_epu16(res, offset_const_16b);

          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]),
                          res_unsigned_16b);
        }
      }
    }
  } else if (!(w % 4)) {
    for (i = 0; i < h; i += 2) {
      for (j = 0; j < w; j += 4) {
        const __m128i src_row_0 =
            _mm_loadl_epi64((__m128i *)(&src[i * src_stride + j]));
        const __m128i src_row_1 =
            _mm_loadl_epi64((__m128i *)(&src[i * src_stride + j + src_stride]));
        const __m128i src_10 = _mm_unpacklo_epi64(src_row_0, src_row_1);

        const __m128i res = _mm_sll_epi16(src_10, left_shift);

        if (do_average) {
          const __m128i data_0 =
              _mm_loadl_epi64((__m128i *)(&dst[i * dst_stride + j]));
          const __m128i data_1 = _mm_loadl_epi64(
              (__m128i *)(&dst[i * dst_stride + j + dst_stride]));

          const __m128i data_ref_0 = _mm_unpacklo_epi16(data_0, zero);
          const __m128i data_ref_1 = _mm_unpacklo_epi16(data_1, zero);

          const __m128i res_32b = _mm_unpacklo_epi16(res, zero);
          const __m128i res_unsigned_lo = _mm_add_epi32(res_32b, offset_const);

          const __m128i res_32b_hi = _mm_unpackhi_epi16(res, zero);
          const __m128i res_unsigned_hi =
              _mm_add_epi32(res_32b_hi, offset_const);

          const __m128i comp_avg_res_lo = highbd_comp_avg_sse4_1(
              &data_ref_0, &res_unsigned_lo, &wt0, &wt1, use_dist_wtd_comp_avg);
          const __m128i comp_avg_res_hi = highbd_comp_avg_sse4_1(
              &data_ref_1, &res_unsigned_hi, &wt0, &wt1, use_dist_wtd_comp_avg);

          const __m128i round_result_lo = highbd_convolve_rounding_sse2(
              &comp_avg_res_lo, &offset_const, &rounding_const, rounding_shift);
          const __m128i round_result_hi = highbd_convolve_rounding_sse2(
              &comp_avg_res_hi, &offset_const, &rounding_const, rounding_shift);

          const __m128i res_16b =
              _mm_packus_epi32(round_result_lo, round_result_hi);
          const __m128i res_clip = _mm_min_epi16(res_16b, clip_pixel_to_bd);

          const __m128i res_1 = _mm_srli_si128(res_clip, 8);

          _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_clip);
          _mm_storel_epi64(
              (__m128i *)(&dst0[i * dst_stride0 + j + dst_stride0]), res_1);
        } else {
          const __m128i res_unsigned_16b =
              _mm_adds_epu16(res, offset_const_16b);

          const __m128i res_1 = _mm_srli_si128(res_unsigned_16b, 8);

          _mm_storel_epi64((__m128i *)(&dst[i * dst_stride + j]),
                           res_unsigned_16b);
          _mm_storel_epi64((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                           res_1);
        }
      }
    }
  }
}

void av1_highbd_dist_wtd_convolve_2d_sse4_1(
    const uint16_t *src, int src_stride, uint16_t *dst0, int dst_stride0, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd) {
  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  int im_h = h + filter_params_y->taps - 1;
  int im_stride = MAX_SB_SIZE;
  int i, j;
  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi32(w0);
  const __m128i wt1 = _mm_set1_epi32(w1);

  const int offset_0 =
      bd + 2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = (1 << offset_0) + (1 << (offset_0 - 1));
  const __m128i offset_const = _mm_set1_epi32(offset);
  const int rounding_shift =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m128i rounding_const = _mm_set1_epi32((1 << rounding_shift) >> 1);
  const __m128i clip_pixel_to_bd =
      _mm_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));

  // Check that, even with 12-bit input, the intermediate values will fit
  // into an unsigned 16-bit intermediate array.
  assert(bd + FILTER_BITS + 2 - conv_params->round_0 <= 16);

  /* Horizontal filter */
  {
    const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
        filter_params_x, subpel_x_qn & SUBPEL_MASK);
    const __m128i coeffs_x = _mm_loadu_si128((__m128i *)x_filter);

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
        ((1 << conv_params->round_0) >> 1) + (1 << (bd + FILTER_BITS - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0);

    for (i = 0; i < im_h; ++i) {
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
        res_even =
            _mm_sra_epi32(_mm_add_epi32(res_even, round_const), round_shift);

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
        res_odd =
            _mm_sra_epi32(_mm_add_epi32(res_odd, round_const), round_shift);

        // Pack in the column order 0, 2, 4, 6, 1, 3, 5, 7
        __m128i res = _mm_packs_epi32(res_even, res_odd);
        _mm_storeu_si128((__m128i *)&im_block[i * im_stride + j], res);
      }
    }
  }

  /* Vertical filter */
  {
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        filter_params_y, subpel_y_qn & SUBPEL_MASK);
    const __m128i coeffs_y = _mm_loadu_si128((__m128i *)y_filter);

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

    const __m128i round_const = _mm_set1_epi32(
        ((1 << conv_params->round_1) >> 1) -
        (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
    const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

    for (i = 0; i < h; ++i) {
      for (j = 0; j < w; j += 8) {
        // Filter even-index pixels
        const int16_t *data = &im_block[i * im_stride + j];
        const __m128i src_0 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_2 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_4 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_6 =
            _mm_unpacklo_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_01);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_23);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_45);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_67);

        const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                               _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m128i src_1 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 0 * im_stride),
                               *(__m128i *)(data + 1 * im_stride));
        const __m128i src_3 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 2 * im_stride),
                               *(__m128i *)(data + 3 * im_stride));
        const __m128i src_5 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 4 * im_stride),
                               *(__m128i *)(data + 5 * im_stride));
        const __m128i src_7 =
            _mm_unpackhi_epi16(*(__m128i *)(data + 6 * im_stride),
                               *(__m128i *)(data + 7 * im_stride));

        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_01);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_23);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_45);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_67);

        const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                              _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        const __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        const __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        const __m128i res_lo_round =
            _mm_sra_epi32(_mm_add_epi32(res_lo, round_const), round_shift);

        const __m128i res_unsigned_lo =
            _mm_add_epi32(res_lo_round, offset_const);

        if (w < 8) {
          if (do_average) {
            const __m128i data_0 =
                _mm_loadl_epi64((__m128i *)(&dst[i * dst_stride + j]));

            const __m128i data_ref_0 = _mm_cvtepu16_epi32(data_0);

            const __m128i comp_avg_res =
                highbd_comp_avg_sse4_1(&data_ref_0, &res_unsigned_lo, &wt0,
                                       &wt1, use_dist_wtd_comp_avg);

            const __m128i round_result = highbd_convolve_rounding_sse2(
                &comp_avg_res, &offset_const, &rounding_const, rounding_shift);

            const __m128i res_16b =
                _mm_packus_epi32(round_result, round_result);
            const __m128i res_clip = _mm_min_epi16(res_16b, clip_pixel_to_bd);

            _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_clip);
          } else {
            const __m128i res_16b =
                _mm_packus_epi32(res_unsigned_lo, res_unsigned_lo);
            _mm_storel_epi64((__m128i *)(&dst[i * dst_stride + j]), res_16b);
          }
        } else {
          const __m128i res_hi_round =
              _mm_sra_epi32(_mm_add_epi32(res_hi, round_const), round_shift);

          const __m128i res_unsigned_hi =
              _mm_add_epi32(res_hi_round, offset_const);

          if (do_average) {
            const __m128i data_lo =
                _mm_loadl_epi64((__m128i *)(&dst[i * dst_stride + j]));
            const __m128i data_hi =
                _mm_loadl_epi64((__m128i *)(&dst[i * dst_stride + j + 4]));

            const __m128i data_ref_0_lo = _mm_cvtepu16_epi32(data_lo);
            const __m128i data_ref_0_hi = _mm_cvtepu16_epi32(data_hi);

            const __m128i comp_avg_res_lo =
                highbd_comp_avg_sse4_1(&data_ref_0_lo, &res_unsigned_lo, &wt0,
                                       &wt1, use_dist_wtd_comp_avg);
            const __m128i comp_avg_res_hi =
                highbd_comp_avg_sse4_1(&data_ref_0_hi, &res_unsigned_hi, &wt0,
                                       &wt1, use_dist_wtd_comp_avg);

            const __m128i round_result_lo =
                highbd_convolve_rounding_sse2(&comp_avg_res_lo, &offset_const,
                                              &rounding_const, rounding_shift);
            const __m128i round_result_hi =
                highbd_convolve_rounding_sse2(&comp_avg_res_hi, &offset_const,
                                              &rounding_const, rounding_shift);

            const __m128i res_16b =
                _mm_packus_epi32(round_result_lo, round_result_hi);
            const __m128i res_clip = _mm_min_epi16(res_16b, clip_pixel_to_bd);

            _mm_store_si128((__m128i *)(&dst0[i * dst_stride0 + j]), res_clip);
          } else {
            const __m128i res_16b =
                _mm_packus_epi32(res_unsigned_lo, res_unsigned_hi);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_16b);
          }
        }
      }
    }
  }
}
