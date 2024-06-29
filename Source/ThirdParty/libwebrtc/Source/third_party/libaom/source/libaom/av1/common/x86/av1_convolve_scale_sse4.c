/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <smmintrin.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "av1/common/convolve.h"

// A specialised version of hfilter, the horizontal filter for
// av1_convolve_2d_scale_sse4_1. This version only supports 8 tap filters.
static void hfilter8(const uint8_t *src, int src_stride, int16_t *dst, int w,
                     int h, int subpel_x_qn, int x_step_qn,
                     const InterpFilterParams *filter_params, int round) {
  const int bd = 8;
  const int ntaps = 8;

  src -= ntaps / 2 - 1;

  int32_t round_add32 = (1 << round) / 2 + (1 << (bd + FILTER_BITS - 1));
  const __m128i round_add = _mm_set1_epi32(round_add32);
  const __m128i round_shift = _mm_cvtsi32_si128(round);

  int x_qn = subpel_x_qn;
  for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
    const uint8_t *const src_col = src + (x_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(filter_params, filter_idx);

    // Load the filter coefficients
    const __m128i coefflo = _mm_loadu_si128((__m128i *)filter);
    const __m128i zero = _mm_castps_si128(_mm_setzero_ps());

    int y;
    for (y = 0; y <= h - 4; y += 4) {
      const uint8_t *const src0 = src_col + y * src_stride;
      const uint8_t *const src1 = src0 + 1 * src_stride;
      const uint8_t *const src2 = src0 + 2 * src_stride;
      const uint8_t *const src3 = src0 + 3 * src_stride;

      // Load up source data. This is 8-bit input data; each load is just
      // loading the lower half of the register and gets 8 pixels
      const __m128i data08 = _mm_loadl_epi64((__m128i *)src0);
      const __m128i data18 = _mm_loadl_epi64((__m128i *)src1);
      const __m128i data28 = _mm_loadl_epi64((__m128i *)src2);
      const __m128i data38 = _mm_loadl_epi64((__m128i *)src3);

      // Now zero-extend up to 16-bit precision by interleaving with
      // zeros. Drop the upper half of each register (which just had zeros)
      const __m128i data0lo = _mm_unpacklo_epi8(data08, zero);
      const __m128i data1lo = _mm_unpacklo_epi8(data18, zero);
      const __m128i data2lo = _mm_unpacklo_epi8(data28, zero);
      const __m128i data3lo = _mm_unpacklo_epi8(data38, zero);

      // Multiply by coefficients
      const __m128i conv0lo = _mm_madd_epi16(data0lo, coefflo);
      const __m128i conv1lo = _mm_madd_epi16(data1lo, coefflo);
      const __m128i conv2lo = _mm_madd_epi16(data2lo, coefflo);
      const __m128i conv3lo = _mm_madd_epi16(data3lo, coefflo);

      // Reduce horizontally and add
      const __m128i conv01lo = _mm_hadd_epi32(conv0lo, conv1lo);
      const __m128i conv23lo = _mm_hadd_epi32(conv2lo, conv3lo);
      const __m128i conv = _mm_hadd_epi32(conv01lo, conv23lo);

      // Divide down by (1 << round), rounding to nearest.
      __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_add), round_shift);

      shifted = _mm_packus_epi32(shifted, shifted);
      // Write transposed to the output
      _mm_storel_epi64((__m128i *)(dst + y + x * h), shifted);
    }
    for (; y < h; ++y) {
      const uint8_t *const src_row = src_col + y * src_stride;

      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < ntaps; ++k) {
        sum += filter[k] * src_row[k];
      }

      dst[y + x * h] = ROUND_POWER_OF_TWO(sum, round);
    }
  }
}

static __m128i convolve_16_8(const int16_t *src, __m128i coeff) {
  __m128i data = _mm_loadu_si128((__m128i *)src);
  return _mm_madd_epi16(data, coeff);
}

// A specialised version of vfilter, the vertical filter for
// av1_convolve_2d_scale_sse4_1. This version only supports 8 tap filters.
static void vfilter8(const int16_t *src, int src_stride, uint8_t *dst,
                     int dst_stride, int w, int h, int subpel_y_qn,
                     int y_step_qn, const InterpFilterParams *filter_params,
                     const ConvolveParams *conv_params, int bd) {
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int ntaps = 8;

  const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

  const int32_t sub32 = ((1 << (offset_bits - conv_params->round_1)) +
                         (1 << (offset_bits - conv_params->round_1 - 1)));
  const __m128i sub = _mm_set1_epi16(sub32);

  CONV_BUF_TYPE *dst16 = conv_params->dst;
  const int dst16_stride = conv_params->dst_stride;
  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  const __m128i bits_shift = _mm_cvtsi32_si128(bits);
  const __m128i bits_const = _mm_set1_epi16(((1 << bits) >> 1));
  const __m128i round_shift_add =
      _mm_set1_epi32(((1 << conv_params->round_1) >> 1));
  const __m128i res_add_const = _mm_set1_epi32(1 << offset_bits);

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi16((short)w0);
  const __m128i wt1 = _mm_set1_epi16((short)w1);
  const __m128i wt = _mm_unpacklo_epi16(wt0, wt1);

  int y_qn = subpel_y_qn;
  for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
    const int16_t *src_y = src + (y_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(filter_params, filter_idx);

    const __m128i coeff0716 = _mm_loadu_si128((__m128i *)filter);
    int x;
    for (x = 0; x <= w - 4; x += 4) {
      const int16_t *const src0 = src_y + x * src_stride;
      const int16_t *const src1 = src0 + 1 * src_stride;
      const int16_t *const src2 = src0 + 2 * src_stride;
      const int16_t *const src3 = src0 + 3 * src_stride;

      // Load the source data for the three rows, adding the three registers of
      // convolved products to one as we go (conv0..conv3) to avoid the
      // register pressure getting too high.
      const __m128i conv0 = convolve_16_8(src0, coeff0716);
      const __m128i conv1 = convolve_16_8(src1, coeff0716);
      const __m128i conv2 = convolve_16_8(src2, coeff0716);
      const __m128i conv3 = convolve_16_8(src3, coeff0716);

      // Now reduce horizontally to get one lane for each result
      const __m128i conv01 = _mm_hadd_epi32(conv0, conv1);
      const __m128i conv23 = _mm_hadd_epi32(conv2, conv3);
      __m128i conv = _mm_hadd_epi32(conv01, conv23);

      conv = _mm_add_epi32(conv, res_add_const);
      // Divide down by (1 << round_1), rounding to nearest and subtract sub32.
      __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_shift_add), round_shift);

      uint8_t *dst_x = dst + y * dst_stride + x;
      __m128i result;
      __m128i shifted_16 = _mm_packus_epi32(shifted, shifted);

      if (conv_params->is_compound) {
        CONV_BUF_TYPE *dst_16_x = dst16 + y * dst16_stride + x;
        if (conv_params->do_average) {
          const __m128i p_16 = _mm_loadl_epi64((__m128i *)dst_16_x);
          if (conv_params->use_dist_wtd_comp_avg) {
            const __m128i p_16_lo = _mm_unpacklo_epi16(p_16, shifted_16);
            const __m128i wt_res_lo = _mm_madd_epi16(p_16_lo, wt);
            const __m128i shifted_32 =
                _mm_srai_epi32(wt_res_lo, DIST_PRECISION_BITS);
            shifted_16 = _mm_packus_epi32(shifted_32, shifted_32);
          } else {
            shifted_16 = _mm_srai_epi16(_mm_add_epi16(p_16, shifted_16), 1);
          }
          const __m128i subbed = _mm_sub_epi16(shifted_16, sub);
          result = _mm_sra_epi16(_mm_add_epi16(subbed, bits_const), bits_shift);
          const __m128i result_8 = _mm_packus_epi16(result, result);
          *(int *)dst_x = _mm_cvtsi128_si32(result_8);
        } else {
          _mm_storel_epi64((__m128i *)dst_16_x, shifted_16);
        }
      } else {
        const __m128i subbed = _mm_sub_epi16(shifted_16, sub);
        result = _mm_sra_epi16(_mm_add_epi16(subbed, bits_const), bits_shift);
        const __m128i result_8 = _mm_packus_epi16(result, result);
        *(int *)dst_x = _mm_cvtsi128_si32(result_8);
      }
    }
    for (; x < w; ++x) {
      const int16_t *src_x = src_y + x * src_stride;
      int32_t sum = 1 << offset_bits;
      for (int k = 0; k < ntaps; ++k) sum += filter[k] * src_x[k];
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1);

      if (conv_params->is_compound) {
        if (conv_params->do_average) {
          int32_t tmp = dst16[y * dst16_stride + x];
          if (conv_params->use_dist_wtd_comp_avg) {
            tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
            tmp = tmp >> DIST_PRECISION_BITS;
          } else {
            tmp += res;
            tmp = tmp >> 1;
          }
          /* Subtract round offset and convolve round */
          tmp = tmp - sub32;
          dst[y * dst_stride + x] = clip_pixel(ROUND_POWER_OF_TWO(tmp, bits));
        } else {
          dst16[y * dst16_stride + x] = res;
        }
      } else {
        /* Subtract round offset and convolve round */
        int32_t tmp = res - ((1 << (offset_bits - conv_params->round_1)) +
                             (1 << (offset_bits - conv_params->round_1 - 1)));
        dst[y * dst_stride + x] = clip_pixel(ROUND_POWER_OF_TWO(tmp, bits));
      }
    }
  }
}
void av1_convolve_2d_scale_sse4_1(const uint8_t *src, int src_stride,
                                  uint8_t *dst8, int dst8_stride, int w, int h,
                                  const InterpFilterParams *filter_params_x,
                                  const InterpFilterParams *filter_params_y,
                                  const int subpel_x_qn, const int x_step_qn,
                                  const int subpel_y_qn, const int y_step_qn,
                                  ConvolveParams *conv_params) {
  int16_t tmp[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE];
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;

  const int xtaps = filter_params_x->taps;
  const int ytaps = filter_params_y->taps;
  const int fo_vert = ytaps / 2 - 1;
  assert((xtaps == 8) && (ytaps == 8));
  (void)xtaps;

  // horizontal filter
  hfilter8(src - fo_vert * src_stride, src_stride, tmp, w, im_h, subpel_x_qn,
           x_step_qn, filter_params_x, conv_params->round_0);

  // vertical filter (input is transposed)
  vfilter8(tmp, im_h, dst8, dst8_stride, w, h, subpel_y_qn, y_step_qn,
           filter_params_y, conv_params, 8);
}

// A specialised version of hfilter, the horizontal filter for
// av1_highbd_convolve_2d_scale_sse4_1. This version only supports 8 tap
// filters.
static void highbd_hfilter8(const uint16_t *src, int src_stride, int16_t *dst,
                            int w, int h, int subpel_x_qn, int x_step_qn,
                            const InterpFilterParams *filter_params, int round,
                            int bd) {
  const int ntaps = 8;

  src -= ntaps / 2 - 1;

  int32_t round_add32 = (1 << round) / 2 + (1 << (bd + FILTER_BITS - 1));
  const __m128i round_add = _mm_set1_epi32(round_add32);
  const __m128i round_shift = _mm_cvtsi32_si128(round);

  int x_qn = subpel_x_qn;
  for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
    const uint16_t *const src_col = src + (x_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(filter_params, filter_idx);

    // Load the filter coefficients
    const __m128i coefflo = _mm_loadu_si128((__m128i *)filter);

    int y;
    for (y = 0; y <= h - 4; y += 4) {
      const uint16_t *const src0 = src_col + y * src_stride;
      const uint16_t *const src1 = src0 + 1 * src_stride;
      const uint16_t *const src2 = src0 + 2 * src_stride;
      const uint16_t *const src3 = src0 + 3 * src_stride;

      // Load up source data. This is 16-bit input data, so each load gets the 8
      // pixels we need.
      const __m128i data0lo = _mm_loadu_si128((__m128i *)src0);
      const __m128i data1lo = _mm_loadu_si128((__m128i *)src1);
      const __m128i data2lo = _mm_loadu_si128((__m128i *)src2);
      const __m128i data3lo = _mm_loadu_si128((__m128i *)src3);

      // Multiply by coefficients
      const __m128i conv0lo = _mm_madd_epi16(data0lo, coefflo);
      const __m128i conv1lo = _mm_madd_epi16(data1lo, coefflo);
      const __m128i conv2lo = _mm_madd_epi16(data2lo, coefflo);
      const __m128i conv3lo = _mm_madd_epi16(data3lo, coefflo);

      // Reduce horizontally and add
      const __m128i conv01lo = _mm_hadd_epi32(conv0lo, conv1lo);
      const __m128i conv23lo = _mm_hadd_epi32(conv2lo, conv3lo);
      const __m128i conv = _mm_hadd_epi32(conv01lo, conv23lo);

      // Divide down by (1 << round), rounding to nearest.
      __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_add), round_shift);

      shifted = _mm_packus_epi32(shifted, shifted);
      // Write transposed to the output
      _mm_storel_epi64((__m128i *)(dst + y + x * h), shifted);
    }
    for (; y < h; ++y) {
      const uint16_t *const src_row = src_col + y * src_stride;

      int32_t sum = (1 << (bd + FILTER_BITS - 1));
      for (int k = 0; k < ntaps; ++k) {
        sum += filter[k] * src_row[k];
      }

      dst[y + x * h] = ROUND_POWER_OF_TWO(sum, round);
    }
  }
}
// A specialised version of vfilter, the vertical filter for
// av1_highbd_convolve_2d_scale_sse4_1. This version only supports 8 tap
// filters.
static void highbd_vfilter8(const int16_t *src, int src_stride, uint16_t *dst,
                            int dst_stride, int w, int h, int subpel_y_qn,
                            int y_step_qn,
                            const InterpFilterParams *filter_params,
                            const ConvolveParams *conv_params, int bd) {
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int ntaps = 8;

  const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);

  const int32_t sub32 = ((1 << (offset_bits - conv_params->round_1)) +
                         (1 << (offset_bits - conv_params->round_1 - 1)));
  const __m128i sub = _mm_set1_epi32(sub32);

  CONV_BUF_TYPE *dst16 = conv_params->dst;
  const int dst16_stride = conv_params->dst_stride;
  const __m128i clip_pixel_ =
      _mm_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const int bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  const __m128i bits_shift = _mm_cvtsi32_si128(bits);
  const __m128i bits_const = _mm_set1_epi32(((1 << bits) >> 1));
  const __m128i round_shift_add =
      _mm_set1_epi32(((1 << conv_params->round_1) >> 1));
  const __m128i res_add_const = _mm_set1_epi32(1 << offset_bits);
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  __m128i round_bits_shift = _mm_cvtsi32_si128(round_bits);
  __m128i round_bits_const = _mm_set1_epi32(((1 << round_bits) >> 1));

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi32(w0);
  const __m128i wt1 = _mm_set1_epi32(w1);

  int y_qn = subpel_y_qn;
  for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
    const int16_t *src_y = src + (y_qn >> SCALE_SUBPEL_BITS);
    const int filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
    assert(filter_idx < SUBPEL_SHIFTS);
    const int16_t *filter =
        av1_get_interp_filter_subpel_kernel(filter_params, filter_idx);

    const __m128i coeff0716 = _mm_loadu_si128((__m128i *)filter);
    int x;
    for (x = 0; x <= w - 4; x += 4) {
      const int16_t *const src0 = src_y + x * src_stride;
      const int16_t *const src1 = src0 + 1 * src_stride;
      const int16_t *const src2 = src0 + 2 * src_stride;
      const int16_t *const src3 = src0 + 3 * src_stride;

      // Load the source data for the three rows, adding the three registers of
      // convolved products to one as we go (conv0..conv3) to avoid the
      // register pressure getting too high.
      const __m128i conv0 = convolve_16_8(src0, coeff0716);
      const __m128i conv1 = convolve_16_8(src1, coeff0716);
      const __m128i conv2 = convolve_16_8(src2, coeff0716);
      const __m128i conv3 = convolve_16_8(src3, coeff0716);

      // Now reduce horizontally to get one lane for each result
      const __m128i conv01 = _mm_hadd_epi32(conv0, conv1);
      const __m128i conv23 = _mm_hadd_epi32(conv2, conv3);
      __m128i conv = _mm_hadd_epi32(conv01, conv23);
      conv = _mm_add_epi32(conv, res_add_const);

      // Divide down by (1 << round_1), rounding to nearest and subtract sub32.
      __m128i shifted =
          _mm_sra_epi32(_mm_add_epi32(conv, round_shift_add), round_shift);

      uint16_t *dst_x = dst + y * dst_stride + x;

      __m128i result;
      if (conv_params->is_compound) {
        CONV_BUF_TYPE *dst_16_x = dst16 + y * dst16_stride + x;
        if (conv_params->do_average) {
          __m128i p_32 =
              _mm_cvtepu16_epi32(_mm_loadl_epi64((__m128i *)dst_16_x));

          if (conv_params->use_dist_wtd_comp_avg) {
            shifted = _mm_add_epi32(_mm_mullo_epi32(p_32, wt0),
                                    _mm_mullo_epi32(shifted, wt1));
            shifted = _mm_srai_epi32(shifted, DIST_PRECISION_BITS);
          } else {
            shifted = _mm_srai_epi32(_mm_add_epi32(p_32, shifted), 1);
          }
          result = _mm_sub_epi32(shifted, sub);
          result = _mm_sra_epi32(_mm_add_epi32(result, round_bits_const),
                                 round_bits_shift);

          result = _mm_packus_epi32(result, result);
          result = _mm_min_epi16(result, clip_pixel_);
          _mm_storel_epi64((__m128i *)dst_x, result);
        } else {
          __m128i shifted_16 = _mm_packus_epi32(shifted, shifted);
          _mm_storel_epi64((__m128i *)dst_16_x, shifted_16);
        }
      } else {
        result = _mm_sub_epi32(shifted, sub);
        result = _mm_sra_epi16(_mm_add_epi32(result, bits_const), bits_shift);
        result = _mm_packus_epi32(result, result);
        result = _mm_min_epi16(result, clip_pixel_);
        _mm_storel_epi64((__m128i *)dst_x, result);
      }
    }

    for (; x < w; ++x) {
      const int16_t *src_x = src_y + x * src_stride;
      int32_t sum = 1 << offset_bits;
      for (int k = 0; k < ntaps; ++k) sum += filter[k] * src_x[k];
      CONV_BUF_TYPE res = ROUND_POWER_OF_TWO(sum, conv_params->round_1);
      if (conv_params->is_compound) {
        if (conv_params->do_average) {
          int32_t tmp = dst16[y * dst16_stride + x];
          if (conv_params->use_dist_wtd_comp_avg) {
            tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
            tmp = tmp >> DIST_PRECISION_BITS;
          } else {
            tmp += res;
            tmp = tmp >> 1;
          }
          /* Subtract round offset and convolve round */
          tmp = tmp - ((1 << (offset_bits - conv_params->round_1)) +
                       (1 << (offset_bits - conv_params->round_1 - 1)));
          dst[y * dst_stride + x] =
              clip_pixel_highbd(ROUND_POWER_OF_TWO(tmp, bits), bd);
        } else {
          dst16[y * dst16_stride + x] = res;
        }
      } else {
        /* Subtract round offset and convolve round */
        int32_t tmp = res - ((1 << (offset_bits - conv_params->round_1)) +
                             (1 << (offset_bits - conv_params->round_1 - 1)));
        dst[y * dst_stride + x] =
            clip_pixel_highbd(ROUND_POWER_OF_TWO(tmp, bits), bd);
      }
    }
  }
}

void av1_highbd_convolve_2d_scale_sse4_1(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int x_step_qn, const int subpel_y_qn, const int y_step_qn,
    ConvolveParams *conv_params, int bd) {
  // TODO(yaowu): Move this out of stack
  DECLARE_ALIGNED(16, int16_t,
                  tmp[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE]);
  int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) +
             filter_params_y->taps;
  const int xtaps = filter_params_x->taps;
  const int ytaps = filter_params_y->taps;
  const int fo_vert = ytaps / 2 - 1;

  memset(tmp, 0, sizeof(tmp));
  assert((xtaps == 8) && (ytaps == 8));
  (void)xtaps;

  // horizontal filter
  highbd_hfilter8(src - fo_vert * src_stride, src_stride, tmp, w, im_h,
                  subpel_x_qn, x_step_qn, filter_params_x, conv_params->round_0,
                  bd);

  // vertical filter (input is transposed)
  highbd_vfilter8(tmp, im_h, dst, dst_stride, w, h, subpel_y_qn, y_step_qn,
                  filter_params_y, conv_params, bd);
}
