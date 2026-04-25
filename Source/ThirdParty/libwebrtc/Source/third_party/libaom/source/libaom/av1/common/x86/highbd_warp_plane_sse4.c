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

#include <smmintrin.h>

#include "config/av1_rtcd.h"

#include "av1/common/warped_motion.h"

static const uint8_t warp_highbd_arrange_bytes[16] = { 0,  2,  4,  6, 8, 10,
                                                       12, 14, 1,  3, 5, 7,
                                                       9,  11, 13, 15 };

static const uint8_t highbd_shuffle_alpha0_mask0[16] = {
  0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3
};
static const uint8_t highbd_shuffle_alpha0_mask1[16] = {
  4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7
};
static const uint8_t highbd_shuffle_alpha0_mask2[16] = { 8,  9,  10, 11, 8,  9,
                                                         10, 11, 8,  9,  10, 11,
                                                         8,  9,  10, 11 };
static const uint8_t highbd_shuffle_alpha0_mask3[16] = { 12, 13, 14, 15, 12, 13,
                                                         14, 15, 12, 13, 14, 15,
                                                         12, 13, 14, 15 };

static inline void highbd_prepare_horizontal_filter_coeff(int alpha, int sx,
                                                          __m128i *coeff) {
  // Filter even-index pixels
  const __m128i tmp_0 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sx + 0 * alpha) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_2 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sx + 2 * alpha) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_4 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sx + 4 * alpha) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_6 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sx + 6 * alpha) >> WARPEDDIFF_PREC_BITS)));

  // coeffs 0 1 0 1 2 3 2 3 for pixels 0, 2
  const __m128i tmp_8 = _mm_unpacklo_epi32(tmp_0, tmp_2);
  // coeffs 0 1 0 1 2 3 2 3 for pixels 4, 6
  const __m128i tmp_10 = _mm_unpacklo_epi32(tmp_4, tmp_6);
  // coeffs 4 5 4 5 6 7 6 7 for pixels 0, 2
  const __m128i tmp_12 = _mm_unpackhi_epi32(tmp_0, tmp_2);
  // coeffs 4 5 4 5 6 7 6 7 for pixels 4, 6
  const __m128i tmp_14 = _mm_unpackhi_epi32(tmp_4, tmp_6);

  // coeffs 0 1 0 1 0 1 0 1 for pixels 0, 2, 4, 6
  coeff[0] = _mm_unpacklo_epi64(tmp_8, tmp_10);
  // coeffs 2 3 2 3 2 3 2 3 for pixels 0, 2, 4, 6
  coeff[2] = _mm_unpackhi_epi64(tmp_8, tmp_10);
  // coeffs 4 5 4 5 4 5 4 5 for pixels 0, 2, 4, 6
  coeff[4] = _mm_unpacklo_epi64(tmp_12, tmp_14);
  // coeffs 6 7 6 7 6 7 6 7 for pixels 0, 2, 4, 6
  coeff[6] = _mm_unpackhi_epi64(tmp_12, tmp_14);

  // Filter odd-index pixels
  const __m128i tmp_1 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sx + 1 * alpha) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_3 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sx + 3 * alpha) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_5 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sx + 5 * alpha) >> WARPEDDIFF_PREC_BITS)));
  const __m128i tmp_7 =
      _mm_loadu_si128((__m128i *)(av1_warped_filter +
                                  ((sx + 7 * alpha) >> WARPEDDIFF_PREC_BITS)));

  const __m128i tmp_9 = _mm_unpacklo_epi32(tmp_1, tmp_3);
  const __m128i tmp_11 = _mm_unpacklo_epi32(tmp_5, tmp_7);
  const __m128i tmp_13 = _mm_unpackhi_epi32(tmp_1, tmp_3);
  const __m128i tmp_15 = _mm_unpackhi_epi32(tmp_5, tmp_7);

  coeff[1] = _mm_unpacklo_epi64(tmp_9, tmp_11);
  coeff[3] = _mm_unpackhi_epi64(tmp_9, tmp_11);
  coeff[5] = _mm_unpacklo_epi64(tmp_13, tmp_15);
  coeff[7] = _mm_unpackhi_epi64(tmp_13, tmp_15);
}

static inline void highbd_prepare_horizontal_filter_coeff_alpha0(
    int sx, __m128i *coeff) {
  // Filter coeff
  const __m128i tmp_0 = _mm_loadu_si128(
      (__m128i *)(av1_warped_filter + (sx >> WARPEDDIFF_PREC_BITS)));

  coeff[0] = _mm_shuffle_epi8(
      tmp_0, _mm_loadu_si128((__m128i *)highbd_shuffle_alpha0_mask0));
  coeff[2] = _mm_shuffle_epi8(
      tmp_0, _mm_loadu_si128((__m128i *)highbd_shuffle_alpha0_mask1));
  coeff[4] = _mm_shuffle_epi8(
      tmp_0, _mm_loadu_si128((__m128i *)highbd_shuffle_alpha0_mask2));
  coeff[6] = _mm_shuffle_epi8(
      tmp_0, _mm_loadu_si128((__m128i *)highbd_shuffle_alpha0_mask3));

  coeff[1] = coeff[0];
  coeff[3] = coeff[2];
  coeff[5] = coeff[4];
  coeff[7] = coeff[6];
}

static inline void highbd_filter_src_pixels(
    const __m128i *src, const __m128i *src2, __m128i *tmp, __m128i *coeff,
    const int offset_bits_horiz, const int reduce_bits_horiz, int k) {
  const __m128i src_1 = *src;
  const __m128i src2_1 = *src2;

  const __m128i round_const = _mm_set1_epi32((1 << offset_bits_horiz) +
                                             ((1 << reduce_bits_horiz) >> 1));

  const __m128i res_0 = _mm_madd_epi16(src_1, coeff[0]);
  const __m128i res_2 =
      _mm_madd_epi16(_mm_alignr_epi8(src2_1, src_1, 4), coeff[2]);
  const __m128i res_4 =
      _mm_madd_epi16(_mm_alignr_epi8(src2_1, src_1, 8), coeff[4]);
  const __m128i res_6 =
      _mm_madd_epi16(_mm_alignr_epi8(src2_1, src_1, 12), coeff[6]);

  __m128i res_even =
      _mm_add_epi32(_mm_add_epi32(res_0, res_4), _mm_add_epi32(res_2, res_6));
  res_even = _mm_sra_epi32(_mm_add_epi32(res_even, round_const),
                           _mm_cvtsi32_si128(reduce_bits_horiz));

  const __m128i res_1 =
      _mm_madd_epi16(_mm_alignr_epi8(src2_1, src_1, 2), coeff[1]);
  const __m128i res_3 =
      _mm_madd_epi16(_mm_alignr_epi8(src2_1, src_1, 6), coeff[3]);
  const __m128i res_5 =
      _mm_madd_epi16(_mm_alignr_epi8(src2_1, src_1, 10), coeff[5]);
  const __m128i res_7 =
      _mm_madd_epi16(_mm_alignr_epi8(src2_1, src_1, 14), coeff[7]);

  __m128i res_odd =
      _mm_add_epi32(_mm_add_epi32(res_1, res_5), _mm_add_epi32(res_3, res_7));
  res_odd = _mm_sra_epi32(_mm_add_epi32(res_odd, round_const),
                          _mm_cvtsi32_si128(reduce_bits_horiz));

  // Combine results into one register.
  // We store the columns in the order 0, 2, 4, 6, 1, 3, 5, 7
  // as this order helps with the vertical filter.
  tmp[k + 7] = _mm_packs_epi32(res_even, res_odd);
}

static inline void highbd_horiz_filter(const __m128i *src, const __m128i *src2,
                                       __m128i *tmp, int sx, int alpha, int k,
                                       const int offset_bits_horiz,
                                       const int reduce_bits_horiz) {
  __m128i coeff[8];
  highbd_prepare_horizontal_filter_coeff(alpha, sx, coeff);
  highbd_filter_src_pixels(src, src2, tmp, coeff, offset_bits_horiz,
                           reduce_bits_horiz, k);
}

static inline void highbd_warp_horizontal_filter_alpha0_beta0(
    const uint16_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  (void)beta;
  (void)alpha;
  int k;

  __m128i coeff[8];
  highbd_prepare_horizontal_filter_coeff_alpha0(sx4, coeff);

  for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
    int iy = iy4 + k;
    if (iy < 0)
      iy = 0;
    else if (iy > height - 1)
      iy = height - 1;

    // Load source pixels
    const __m128i src =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
    const __m128i src2 =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 + 1));
    highbd_filter_src_pixels(&src, &src2, tmp, coeff, offset_bits_horiz,
                             reduce_bits_horiz, k);
  }
}

static inline void highbd_warp_horizontal_filter_alpha0(
    const uint16_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  (void)alpha;
  int k;
  for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
    int iy = iy4 + k;
    if (iy < 0)
      iy = 0;
    else if (iy > height - 1)
      iy = height - 1;
    int sx = sx4 + beta * (k + 4);

    // Load source pixels
    const __m128i src =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
    const __m128i src2 =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 + 1));

    __m128i coeff[8];
    highbd_prepare_horizontal_filter_coeff_alpha0(sx, coeff);
    highbd_filter_src_pixels(&src, &src2, tmp, coeff, offset_bits_horiz,
                             reduce_bits_horiz, k);
  }
}

static inline void highbd_warp_horizontal_filter_beta0(
    const uint16_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  (void)beta;
  int k;
  __m128i coeff[8];
  highbd_prepare_horizontal_filter_coeff(alpha, sx4, coeff);

  for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
    int iy = iy4 + k;
    if (iy < 0)
      iy = 0;
    else if (iy > height - 1)
      iy = height - 1;

    // Load source pixels
    const __m128i src =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
    const __m128i src2 =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 + 1));
    highbd_filter_src_pixels(&src, &src2, tmp, coeff, offset_bits_horiz,
                             reduce_bits_horiz, k);
  }
}

static inline void highbd_warp_horizontal_filter(
    const uint16_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  int k;
  for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
    int iy = iy4 + k;
    if (iy < 0)
      iy = 0;
    else if (iy > height - 1)
      iy = height - 1;
    int sx = sx4 + beta * (k + 4);

    // Load source pixels
    const __m128i src =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
    const __m128i src2 =
        _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 + 1));

    highbd_horiz_filter(&src, &src2, tmp, sx, alpha, k, offset_bits_horiz,
                        reduce_bits_horiz);
  }
}

static inline void highbd_prepare_warp_horizontal_filter(
    const uint16_t *ref, __m128i *tmp, int stride, int32_t ix4, int32_t iy4,
    int32_t sx4, int alpha, int beta, int p_height, int height, int i,
    const int offset_bits_horiz, const int reduce_bits_horiz) {
  if (alpha == 0 && beta == 0)
    highbd_warp_horizontal_filter_alpha0_beta0(
        ref, tmp, stride, ix4, iy4, sx4, alpha, beta, p_height, height, i,
        offset_bits_horiz, reduce_bits_horiz);

  else if (alpha == 0 && beta != 0)
    highbd_warp_horizontal_filter_alpha0(ref, tmp, stride, ix4, iy4, sx4, alpha,
                                         beta, p_height, height, i,
                                         offset_bits_horiz, reduce_bits_horiz);

  else if (alpha != 0 && beta == 0)
    highbd_warp_horizontal_filter_beta0(ref, tmp, stride, ix4, iy4, sx4, alpha,
                                        beta, p_height, height, i,
                                        offset_bits_horiz, reduce_bits_horiz);
  else
    highbd_warp_horizontal_filter(ref, tmp, stride, ix4, iy4, sx4, alpha, beta,
                                  p_height, height, i, offset_bits_horiz,
                                  reduce_bits_horiz);
}

void av1_highbd_warp_affine_sse4_1(const int32_t *mat, const uint16_t *ref,
                                   int width, int height, int stride,
                                   uint16_t *pred, int p_col, int p_row,
                                   int p_width, int p_height, int p_stride,
                                   int subsampling_x, int subsampling_y, int bd,
                                   ConvolveParams *conv_params, int16_t alpha,
                                   int16_t beta, int16_t gamma, int16_t delta) {
  __m128i tmp[15];
  int i, j, k;
  const int reduce_bits_horiz = conv_params->round_0;
  const int reduce_bits_vert = conv_params->is_compound
                                   ? conv_params->round_1
                                   : 2 * FILTER_BITS - reduce_bits_horiz;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;
  assert(IMPLIES(conv_params->is_compound, conv_params->dst != NULL));
  assert(!(bd == 12 && reduce_bits_horiz < 5));
  assert(IMPLIES(conv_params->do_average, conv_params->is_compound));

  // Check that, even with 12-bit input, the intermediate values will fit
  // into an unsigned 16-bit intermediate array.
  assert(bd + FILTER_BITS + 2 - conv_params->round_0 <= 16);

  const int offset_bits_vert = bd + 2 * FILTER_BITS - reduce_bits_horiz;
  const __m128i clip_pixel =
      _mm_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m128i reduce_bits_vert_shift = _mm_cvtsi32_si128(reduce_bits_vert);
  const __m128i reduce_bits_vert_const =
      _mm_set1_epi32(((1 << reduce_bits_vert) >> 1));
  const __m128i res_add_const = _mm_set1_epi32(1 << offset_bits_vert);
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const __m128i res_sub_const =
      _mm_set1_epi32(-(1 << (offset_bits - conv_params->round_1)) -
                     (1 << (offset_bits - conv_params->round_1 - 1)));
  __m128i round_bits_shift = _mm_cvtsi32_si128(round_bits);
  __m128i round_bits_const = _mm_set1_epi32(((1 << round_bits) >> 1));

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m128i wt0 = _mm_set1_epi32(w0);
  const __m128i wt1 = _mm_set1_epi32(w1);

  /* Note: For this code to work, the left/right frame borders need to be
  extended by at least 13 pixels each. By the time we get here, other
  code will have set up this border, but we allow an explicit check
  for debugging purposes.
  */
  /*for (i = 0; i < height; ++i) {
  for (j = 0; j < 13; ++j) {
  assert(ref[i * stride - 13 + j] == ref[i * stride]);
  assert(ref[i * stride + width + j] == ref[i * stride + (width - 1)]);
  }
  }*/

  for (i = 0; i < p_height; i += 8) {
    for (j = 0; j < p_width; j += 8) {
      const int32_t src_x = (p_col + j + 4) << subsampling_x;
      const int32_t src_y = (p_row + i + 4) << subsampling_y;
      const int64_t dst_x =
          (int64_t)mat[2] * src_x + (int64_t)mat[3] * src_y + (int64_t)mat[0];
      const int64_t dst_y =
          (int64_t)mat[4] * src_x + (int64_t)mat[5] * src_y + (int64_t)mat[1];
      const int64_t x4 = dst_x >> subsampling_x;
      const int64_t y4 = dst_y >> subsampling_y;

      int32_t ix4 = (int32_t)(x4 >> WARPEDMODEL_PREC_BITS);
      int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      int32_t iy4 = (int32_t)(y4 >> WARPEDMODEL_PREC_BITS);
      int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      // Add in all the constant terms, including rounding and offset
      sx4 += alpha * (-4) + beta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
             (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);
      sy4 += gamma * (-4) + delta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
             (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);

      sx4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);
      sy4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);

      // Horizontal filter
      // If the block is aligned such that, after clamping, every sample
      // would be taken from the leftmost/rightmost column, then we can
      // skip the expensive horizontal filter.
      if (ix4 <= -7) {
        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          tmp[k + 7] = _mm_set1_epi16(
              (1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
              ref[iy * stride] * (1 << (FILTER_BITS - reduce_bits_horiz)));
        }
      } else if (ix4 >= width + 6) {
        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          tmp[k + 7] =
              _mm_set1_epi16((1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
                             ref[iy * stride + (width - 1)] *
                                 (1 << (FILTER_BITS - reduce_bits_horiz)));
        }
      } else if (((ix4 - 7) < 0) || ((ix4 + 9) > width)) {
        const int out_of_boundary_left = -(ix4 - 6);
        const int out_of_boundary_right = (ix4 + 8) - width;

        for (k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          int sx = sx4 + beta * (k + 4);

          // Load source pixels
          const __m128i src =
              _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 - 7));
          const __m128i src2 =
              _mm_loadu_si128((__m128i *)(ref + iy * stride + ix4 + 1));

          const __m128i src_01 = _mm_shuffle_epi8(
              src, _mm_loadu_si128((__m128i *)warp_highbd_arrange_bytes));
          const __m128i src2_01 = _mm_shuffle_epi8(
              src2, _mm_loadu_si128((__m128i *)warp_highbd_arrange_bytes));

          __m128i src_lo = _mm_unpacklo_epi64(src_01, src2_01);
          __m128i src_hi = _mm_unpackhi_epi64(src_01, src2_01);

          if (out_of_boundary_left >= 0) {
            const __m128i shuffle_reg_left =
                _mm_loadu_si128((__m128i *)warp_pad_left[out_of_boundary_left]);
            src_lo = _mm_shuffle_epi8(src_lo, shuffle_reg_left);
            src_hi = _mm_shuffle_epi8(src_hi, shuffle_reg_left);
          }

          if (out_of_boundary_right >= 0) {
            const __m128i shuffle_reg_right = _mm_loadu_si128(
                (__m128i *)warp_pad_right[out_of_boundary_right]);
            src_lo = _mm_shuffle_epi8(src_lo, shuffle_reg_right);
            src_hi = _mm_shuffle_epi8(src_hi, shuffle_reg_right);
          }

          const __m128i src_padded = _mm_unpacklo_epi8(src_lo, src_hi);
          const __m128i src2_padded = _mm_unpackhi_epi8(src_lo, src_hi);

          highbd_horiz_filter(&src_padded, &src2_padded, tmp, sx, alpha, k,
                              offset_bits_horiz, reduce_bits_horiz);
        }
      } else {
        highbd_prepare_warp_horizontal_filter(
            ref, tmp, stride, ix4, iy4, sx4, alpha, beta, p_height, height, i,
            offset_bits_horiz, reduce_bits_horiz);
      }

      // Vertical filter
      for (k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
        int sy = sy4 + delta * (k + 4);

        // Load from tmp and rearrange pairs of consecutive rows into the
        // column order 0 0 2 2 4 4 6 6; 1 1 3 3 5 5 7 7
        const __m128i *src = tmp + (k + 4);
        const __m128i src_0 = _mm_unpacklo_epi16(src[0], src[1]);
        const __m128i src_2 = _mm_unpacklo_epi16(src[2], src[3]);
        const __m128i src_4 = _mm_unpacklo_epi16(src[4], src[5]);
        const __m128i src_6 = _mm_unpacklo_epi16(src[6], src[7]);

        // Filter even-index pixels
        const __m128i tmp_0 = _mm_loadu_si128(
            (__m128i *)(av1_warped_filter +
                        ((sy + 0 * gamma) >> WARPEDDIFF_PREC_BITS)));
        const __m128i tmp_2 = _mm_loadu_si128(
            (__m128i *)(av1_warped_filter +
                        ((sy + 2 * gamma) >> WARPEDDIFF_PREC_BITS)));
        const __m128i tmp_4 = _mm_loadu_si128(
            (__m128i *)(av1_warped_filter +
                        ((sy + 4 * gamma) >> WARPEDDIFF_PREC_BITS)));
        const __m128i tmp_6 = _mm_loadu_si128(
            (__m128i *)(av1_warped_filter +
                        ((sy + 6 * gamma) >> WARPEDDIFF_PREC_BITS)));

        const __m128i tmp_8 = _mm_unpacklo_epi32(tmp_0, tmp_2);
        const __m128i tmp_10 = _mm_unpacklo_epi32(tmp_4, tmp_6);
        const __m128i tmp_12 = _mm_unpackhi_epi32(tmp_0, tmp_2);
        const __m128i tmp_14 = _mm_unpackhi_epi32(tmp_4, tmp_6);

        const __m128i coeff_0 = _mm_unpacklo_epi64(tmp_8, tmp_10);
        const __m128i coeff_2 = _mm_unpackhi_epi64(tmp_8, tmp_10);
        const __m128i coeff_4 = _mm_unpacklo_epi64(tmp_12, tmp_14);
        const __m128i coeff_6 = _mm_unpackhi_epi64(tmp_12, tmp_14);

        const __m128i res_0 = _mm_madd_epi16(src_0, coeff_0);
        const __m128i res_2 = _mm_madd_epi16(src_2, coeff_2);
        const __m128i res_4 = _mm_madd_epi16(src_4, coeff_4);
        const __m128i res_6 = _mm_madd_epi16(src_6, coeff_6);

        const __m128i res_even = _mm_add_epi32(_mm_add_epi32(res_0, res_2),
                                               _mm_add_epi32(res_4, res_6));

        // Filter odd-index pixels
        const __m128i src_1 = _mm_unpackhi_epi16(src[0], src[1]);
        const __m128i src_3 = _mm_unpackhi_epi16(src[2], src[3]);
        const __m128i src_5 = _mm_unpackhi_epi16(src[4], src[5]);
        const __m128i src_7 = _mm_unpackhi_epi16(src[6], src[7]);

        const __m128i tmp_1 = _mm_loadu_si128(
            (__m128i *)(av1_warped_filter +
                        ((sy + 1 * gamma) >> WARPEDDIFF_PREC_BITS)));
        const __m128i tmp_3 = _mm_loadu_si128(
            (__m128i *)(av1_warped_filter +
                        ((sy + 3 * gamma) >> WARPEDDIFF_PREC_BITS)));
        const __m128i tmp_5 = _mm_loadu_si128(
            (__m128i *)(av1_warped_filter +
                        ((sy + 5 * gamma) >> WARPEDDIFF_PREC_BITS)));
        const __m128i tmp_7 = _mm_loadu_si128(
            (__m128i *)(av1_warped_filter +
                        ((sy + 7 * gamma) >> WARPEDDIFF_PREC_BITS)));

        const __m128i tmp_9 = _mm_unpacklo_epi32(tmp_1, tmp_3);
        const __m128i tmp_11 = _mm_unpacklo_epi32(tmp_5, tmp_7);
        const __m128i tmp_13 = _mm_unpackhi_epi32(tmp_1, tmp_3);
        const __m128i tmp_15 = _mm_unpackhi_epi32(tmp_5, tmp_7);

        const __m128i coeff_1 = _mm_unpacklo_epi64(tmp_9, tmp_11);
        const __m128i coeff_3 = _mm_unpackhi_epi64(tmp_9, tmp_11);
        const __m128i coeff_5 = _mm_unpacklo_epi64(tmp_13, tmp_15);
        const __m128i coeff_7 = _mm_unpackhi_epi64(tmp_13, tmp_15);

        const __m128i res_1 = _mm_madd_epi16(src_1, coeff_1);
        const __m128i res_3 = _mm_madd_epi16(src_3, coeff_3);
        const __m128i res_5 = _mm_madd_epi16(src_5, coeff_5);
        const __m128i res_7 = _mm_madd_epi16(src_7, coeff_7);

        const __m128i res_odd = _mm_add_epi32(_mm_add_epi32(res_1, res_3),
                                              _mm_add_epi32(res_5, res_7));

        // Rearrange pixels back into the order 0 ... 7
        __m128i res_lo = _mm_unpacklo_epi32(res_even, res_odd);
        __m128i res_hi = _mm_unpackhi_epi32(res_even, res_odd);

        if (conv_params->is_compound) {
          __m128i *const p =
              (__m128i *)&conv_params
                  ->dst[(i + k + 4) * conv_params->dst_stride + j];
          res_lo = _mm_add_epi32(res_lo, res_add_const);
          res_lo = _mm_sra_epi32(_mm_add_epi32(res_lo, reduce_bits_vert_const),
                                 reduce_bits_vert_shift);

          if (conv_params->do_average) {
            __m128i *const dst16 = (__m128i *)&pred[(i + k + 4) * p_stride + j];
            __m128i p_32 = _mm_cvtepu16_epi32(_mm_loadl_epi64(p));

            if (conv_params->use_dist_wtd_comp_avg) {
              res_lo = _mm_add_epi32(_mm_mullo_epi32(p_32, wt0),
                                     _mm_mullo_epi32(res_lo, wt1));
              res_lo = _mm_srai_epi32(res_lo, DIST_PRECISION_BITS);
            } else {
              res_lo = _mm_srai_epi32(_mm_add_epi32(p_32, res_lo), 1);
            }

            __m128i res32_lo = _mm_add_epi32(res_lo, res_sub_const);
            res32_lo = _mm_sra_epi32(_mm_add_epi32(res32_lo, round_bits_const),
                                     round_bits_shift);

            __m128i res16_lo = _mm_packus_epi32(res32_lo, res32_lo);
            res16_lo = _mm_min_epi16(res16_lo, clip_pixel);
            _mm_storel_epi64(dst16, res16_lo);
          } else {
            res_lo = _mm_packus_epi32(res_lo, res_lo);
            _mm_storel_epi64(p, res_lo);
          }
          if (p_width > 4) {
            __m128i *const p4 =
                (__m128i *)&conv_params
                    ->dst[(i + k + 4) * conv_params->dst_stride + j + 4];

            res_hi = _mm_add_epi32(res_hi, res_add_const);
            res_hi =
                _mm_sra_epi32(_mm_add_epi32(res_hi, reduce_bits_vert_const),
                              reduce_bits_vert_shift);
            if (conv_params->do_average) {
              __m128i *const dst16_4 =
                  (__m128i *)&pred[(i + k + 4) * p_stride + j + 4];
              __m128i p4_32 = _mm_cvtepu16_epi32(_mm_loadl_epi64(p4));

              if (conv_params->use_dist_wtd_comp_avg) {
                res_hi = _mm_add_epi32(_mm_mullo_epi32(p4_32, wt0),
                                       _mm_mullo_epi32(res_hi, wt1));
                res_hi = _mm_srai_epi32(res_hi, DIST_PRECISION_BITS);
              } else {
                res_hi = _mm_srai_epi32(_mm_add_epi32(p4_32, res_hi), 1);
              }

              __m128i res32_hi = _mm_add_epi32(res_hi, res_sub_const);
              res32_hi = _mm_sra_epi32(
                  _mm_add_epi32(res32_hi, round_bits_const), round_bits_shift);
              __m128i res16_hi = _mm_packus_epi32(res32_hi, res32_hi);
              res16_hi = _mm_min_epi16(res16_hi, clip_pixel);
              _mm_storel_epi64(dst16_4, res16_hi);
            } else {
              res_hi = _mm_packus_epi32(res_hi, res_hi);
              _mm_storel_epi64(p4, res_hi);
            }
          }
        } else {
          // Round and pack into 8 bits
          const __m128i round_const =
              _mm_set1_epi32(-(1 << (bd + reduce_bits_vert - 1)) +
                             ((1 << reduce_bits_vert) >> 1));

          const __m128i res_lo_round = _mm_srai_epi32(
              _mm_add_epi32(res_lo, round_const), reduce_bits_vert);
          const __m128i res_hi_round = _mm_srai_epi32(
              _mm_add_epi32(res_hi, round_const), reduce_bits_vert);

          __m128i res_16bit = _mm_packs_epi32(res_lo_round, res_hi_round);
          // Clamp res_16bit to the range [0, 2^bd - 1]
          const __m128i max_val = _mm_set1_epi16((1 << bd) - 1);
          const __m128i zero = _mm_setzero_si128();
          res_16bit = _mm_max_epi16(_mm_min_epi16(res_16bit, max_val), zero);

          // Store, blending with 'pred' if needed
          __m128i *const p = (__m128i *)&pred[(i + k + 4) * p_stride + j];

          // Note: If we're outputting a 4x4 block, we need to be very careful
          // to only output 4 pixels at this point, to avoid encode/decode
          // mismatches when encoding with multiple threads.
          if (p_width == 4) {
            _mm_storel_epi64(p, res_16bit);
          } else {
            _mm_storeu_si128(p, res_16bit);
          }
        }
      }
    }
  }
}
