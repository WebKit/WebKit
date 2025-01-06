/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <immintrin.h>

#include "config/av1_rtcd.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/temporal_filter.h"

#define SSE_STRIDE (BW + 2)

DECLARE_ALIGNED(32, static const uint32_t, sse_bytemask[4][8]) = {
  { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0 },
  { 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0 },
  { 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 },
  { 0, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }
};

DECLARE_ALIGNED(32, static const uint8_t, shufflemask_16b[2][16]) = {
  { 0, 1, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 10, 11, 10, 11 }
};

#define CALC_X_GRADIENT(AC, GI, DF, out) \
  out = _mm256_abs_epi16(                \
      _mm256_add_epi16(_mm256_add_epi16(AC, GI), _mm256_slli_epi16(DF, 1)));

#define CALC_Y_GRADIENT(AC, GI, BH, out) \
  out = _mm256_abs_epi16(                \
      _mm256_add_epi16(_mm256_sub_epi16(AC, GI), _mm256_slli_epi16(BH, 1)));

double av1_estimate_noise_from_single_plane_avx2(const uint8_t *src, int height,
                                                 int width, int stride,
                                                 int edge_thresh) {
  int count = 0;
  int64_t accum = 0;
  // w32 stores width multiple of 32.
  const int w32 = (width - 1) & ~0x1f;
  const __m256i zero = _mm256_setzero_si256();
  const __m256i edge_threshold = _mm256_set1_epi16(edge_thresh);
  __m256i num_accumulator = zero;
  __m256i sum_accumulator = zero;

  //  A | B | C
  //  D | E | F
  //  G | H | I
  // g_x = (A - C) + (G - I) + 2*(D - F)
  // g_y = (A + C) - (G + I) + 2*(B - H)
  // v   = 4*E - 2*(D+F+B+H) + (A+C+G+I)

  // Process the width multiple of 32 here.
  for (int w = 1; w < w32; w += 32) {
    int h = 1;
    const int start_idx = h * stride + w;
    const int stride_0 = start_idx - stride;

    __m256i num_accum_row_lvl = zero;
    const __m256i A = _mm256_loadu_si256((__m256i *)(&src[stride_0 - 1]));
    const __m256i C = _mm256_loadu_si256((__m256i *)(&src[stride_0 + 1]));
    const __m256i D = _mm256_loadu_si256((__m256i *)(&src[start_idx - 1]));
    const __m256i F = _mm256_loadu_si256((__m256i *)(&src[start_idx + 1]));
    __m256i B = _mm256_loadu_si256((__m256i *)(&src[stride_0]));
    __m256i E = _mm256_loadu_si256((__m256i *)(&src[start_idx]));

    const __m256i A_lo = _mm256_unpacklo_epi8(A, zero);
    const __m256i A_hi = _mm256_unpackhi_epi8(A, zero);
    const __m256i C_lo = _mm256_unpacklo_epi8(C, zero);
    const __m256i C_hi = _mm256_unpackhi_epi8(C, zero);
    const __m256i D_lo = _mm256_unpacklo_epi8(D, zero);
    const __m256i D_hi = _mm256_unpackhi_epi8(D, zero);
    const __m256i F_lo = _mm256_unpacklo_epi8(F, zero);
    const __m256i F_hi = _mm256_unpackhi_epi8(F, zero);

    __m256i sub_AC_lo = _mm256_sub_epi16(A_lo, C_lo);
    __m256i sub_AC_hi = _mm256_sub_epi16(A_hi, C_hi);
    __m256i sum_AC_lo = _mm256_add_epi16(A_lo, C_lo);
    __m256i sum_AC_hi = _mm256_add_epi16(A_hi, C_hi);
    __m256i sub_DF_lo = _mm256_sub_epi16(D_lo, F_lo);
    __m256i sub_DF_hi = _mm256_sub_epi16(D_hi, F_hi);
    __m256i sum_DF_lo = _mm256_add_epi16(D_lo, F_lo);
    __m256i sum_DF_hi = _mm256_add_epi16(D_hi, F_hi);

    for (; h < height - 1; h++) {
      __m256i sum_GI_lo, sub_GI_lo, sum_GI_hi, sub_GI_hi, gx_lo, gy_lo, gx_hi,
          gy_hi;
      const int k = h * stride + w;
      const __m256i G = _mm256_loadu_si256((__m256i *)(&src[k + stride - 1]));
      const __m256i H = _mm256_loadu_si256((__m256i *)(&src[k + stride]));
      const __m256i I = _mm256_loadu_si256((__m256i *)(&src[k + stride + 1]));

      const __m256i B_lo = _mm256_unpacklo_epi8(B, zero);
      const __m256i B_hi = _mm256_unpackhi_epi8(B, zero);
      const __m256i G_lo = _mm256_unpacklo_epi8(G, zero);
      const __m256i G_hi = _mm256_unpackhi_epi8(G, zero);
      const __m256i I_lo = _mm256_unpacklo_epi8(I, zero);
      const __m256i I_hi = _mm256_unpackhi_epi8(I, zero);
      const __m256i H_lo = _mm256_unpacklo_epi8(H, zero);
      const __m256i H_hi = _mm256_unpackhi_epi8(H, zero);

      sub_GI_lo = _mm256_sub_epi16(G_lo, I_lo);
      sub_GI_hi = _mm256_sub_epi16(G_hi, I_hi);
      sum_GI_lo = _mm256_add_epi16(G_lo, I_lo);
      sum_GI_hi = _mm256_add_epi16(G_hi, I_hi);
      const __m256i sub_BH_lo = _mm256_sub_epi16(B_lo, H_lo);
      const __m256i sub_BH_hi = _mm256_sub_epi16(B_hi, H_hi);

      CALC_X_GRADIENT(sub_AC_lo, sub_GI_lo, sub_DF_lo, gx_lo)
      CALC_Y_GRADIENT(sum_AC_lo, sum_GI_lo, sub_BH_lo, gy_lo)

      const __m256i ga_lo = _mm256_add_epi16(gx_lo, gy_lo);

      CALC_X_GRADIENT(sub_AC_hi, sub_GI_hi, sub_DF_hi, gx_hi)
      CALC_Y_GRADIENT(sum_AC_hi, sum_GI_hi, sub_BH_hi, gy_hi)

      const __m256i ga_hi = _mm256_add_epi16(gx_hi, gy_hi);

      __m256i cmp_lo = _mm256_cmpgt_epi16(edge_threshold, ga_lo);
      __m256i cmp_hi = _mm256_cmpgt_epi16(edge_threshold, ga_hi);
      const __m256i comp_reg = _mm256_add_epi16(cmp_lo, cmp_hi);

      // v = 4*E -2*(D+F+B+H) + (A+C+G+I)
      if (_mm256_movemask_epi8(comp_reg) != 0) {
        const __m256i sum_BH_lo = _mm256_add_epi16(B_lo, H_lo);
        const __m256i sum_BH_hi = _mm256_add_epi16(B_hi, H_hi);

        // 2*(D+F+B+H)
        const __m256i sum_DFBH_lo =
            _mm256_slli_epi16(_mm256_add_epi16(sum_DF_lo, sum_BH_lo), 1);
        // (A+C+G+I)
        const __m256i sum_ACGI_lo = _mm256_add_epi16(sum_AC_lo, sum_GI_lo);
        const __m256i sum_DFBH_hi =
            _mm256_slli_epi16(_mm256_add_epi16(sum_DF_hi, sum_BH_hi), 1);
        const __m256i sum_ACGI_hi = _mm256_add_epi16(sum_AC_hi, sum_GI_hi);

        // Convert E register values from 8bit to 16bit
        const __m256i E_lo = _mm256_unpacklo_epi8(E, zero);
        const __m256i E_hi = _mm256_unpackhi_epi8(E, zero);

        // 4*E - 2*(D+F+B+H)+ (A+C+G+I)
        const __m256i var_lo_0 = _mm256_abs_epi16(_mm256_add_epi16(
            _mm256_sub_epi16(_mm256_slli_epi16(E_lo, 2), sum_DFBH_lo),
            sum_ACGI_lo));
        const __m256i var_hi_0 = _mm256_abs_epi16(_mm256_add_epi16(
            _mm256_sub_epi16(_mm256_slli_epi16(E_hi, 2), sum_DFBH_hi),
            sum_ACGI_hi));
        cmp_lo = _mm256_srli_epi16(cmp_lo, 15);
        cmp_hi = _mm256_srli_epi16(cmp_hi, 15);
        const __m256i var_lo = _mm256_mullo_epi16(var_lo_0, cmp_lo);
        const __m256i var_hi = _mm256_mullo_epi16(var_hi_0, cmp_hi);

        num_accum_row_lvl = _mm256_add_epi16(num_accum_row_lvl, cmp_lo);
        num_accum_row_lvl = _mm256_add_epi16(num_accum_row_lvl, cmp_hi);

        sum_accumulator = _mm256_add_epi32(sum_accumulator,
                                           _mm256_unpacklo_epi16(var_lo, zero));
        sum_accumulator = _mm256_add_epi32(sum_accumulator,
                                           _mm256_unpackhi_epi16(var_lo, zero));
        sum_accumulator = _mm256_add_epi32(sum_accumulator,
                                           _mm256_unpacklo_epi16(var_hi, zero));
        sum_accumulator = _mm256_add_epi32(sum_accumulator,
                                           _mm256_unpackhi_epi16(var_hi, zero));
      }
      sub_AC_lo = sub_DF_lo;
      sub_AC_hi = sub_DF_hi;
      sub_DF_lo = sub_GI_lo;
      sub_DF_hi = sub_GI_hi;
      sum_AC_lo = sum_DF_lo;
      sum_AC_hi = sum_DF_hi;
      sum_DF_lo = sum_GI_lo;
      sum_DF_hi = sum_GI_hi;
      B = E;
      E = H;
    }
    const __m256i num_0 = _mm256_unpacklo_epi16(num_accum_row_lvl, zero);
    const __m256i num_1 = _mm256_unpackhi_epi16(num_accum_row_lvl, zero);
    num_accumulator =
        _mm256_add_epi32(num_accumulator, _mm256_add_epi32(num_0, num_1));
  }

  // Process the remaining width here.
  for (int h = 1; h < height - 1; ++h) {
    for (int w = w32 + 1; w < width - 1; ++w) {
      const int k = h * stride + w;

      // Compute sobel gradients
      const int g_x = (src[k - stride - 1] - src[k - stride + 1]) +
                      (src[k + stride - 1] - src[k + stride + 1]) +
                      2 * (src[k - 1] - src[k + 1]);
      const int g_y = (src[k - stride - 1] - src[k + stride - 1]) +
                      (src[k - stride + 1] - src[k + stride + 1]) +
                      2 * (src[k - stride] - src[k + stride]);
      const int ga = abs(g_x) + abs(g_y);

      if (ga < edge_thresh) {
        // Find Laplacian
        const int v =
            4 * src[k] -
            2 * (src[k - 1] + src[k + 1] + src[k - stride] + src[k + stride]) +
            (src[k - stride - 1] + src[k - stride + 1] + src[k + stride - 1] +
             src[k + stride + 1]);
        accum += abs(v);
        ++count;
      }
    }
  }

  // s0 s1 n0 n1 s2 s3 n2 n3
  __m256i sum_avx = _mm256_hadd_epi32(sum_accumulator, num_accumulator);
  __m128i sum_avx_lo = _mm256_castsi256_si128(sum_avx);
  __m128i sum_avx_hi = _mm256_extractf128_si256(sum_avx, 1);
  // s0+s2 s1+s3 n0+n2 n1+n3
  __m128i sum_avx_1 = _mm_add_epi32(sum_avx_lo, sum_avx_hi);
  // s0+s2+s1+s3 n0+n2+n1+n3
  __m128i result = _mm_add_epi32(_mm_srli_si128(sum_avx_1, 4), sum_avx_1);

  accum += _mm_cvtsi128_si32(result);
  count += _mm_extract_epi32(result, 2);

  // If very few smooth pels, return -1 since the estimate is unreliable.
  return (count < 16) ? -1.0 : (double)accum / (6 * count) * SQRT_PI_BY_2;
}

static AOM_FORCE_INLINE void get_squared_error_16x16_avx2(
    const uint8_t *frame1, const unsigned int stride, const uint8_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    uint16_t *frame_sse, const unsigned int sse_stride) {
  (void)block_width;
  const uint8_t *src1 = frame1;
  const uint8_t *src2 = frame2;
  uint16_t *dst = frame_sse;
  for (int i = 0; i < block_height; i++) {
    __m128i vf1_128, vf2_128;
    __m256i vf1, vf2, vdiff1, vsqdiff1;

    vf1_128 = _mm_loadu_si128((__m128i *)(src1));
    vf2_128 = _mm_loadu_si128((__m128i *)(src2));
    vf1 = _mm256_cvtepu8_epi16(vf1_128);
    vf2 = _mm256_cvtepu8_epi16(vf2_128);
    vdiff1 = _mm256_sub_epi16(vf1, vf2);
    vsqdiff1 = _mm256_mullo_epi16(vdiff1, vdiff1);

    _mm256_storeu_si256((__m256i *)(dst), vsqdiff1);
    // Set zero to uninitialized memory to avoid uninitialized loads later
    *(int *)(dst + 16) = _mm_cvtsi128_si32(_mm_setzero_si128());

    src1 += stride, src2 += stride2;
    dst += sse_stride;
  }
}

static AOM_FORCE_INLINE void get_squared_error_32x32_avx2(
    const uint8_t *frame1, const unsigned int stride, const uint8_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    uint16_t *frame_sse, const unsigned int sse_stride) {
  (void)block_width;
  const uint8_t *src1 = frame1;
  const uint8_t *src2 = frame2;
  uint16_t *dst = frame_sse;
  for (int i = 0; i < block_height; i++) {
    __m256i vsrc1, vsrc2, vmin, vmax, vdiff, vdiff1, vdiff2, vres1, vres2;

    vsrc1 = _mm256_loadu_si256((__m256i *)src1);
    vsrc2 = _mm256_loadu_si256((__m256i *)src2);
    vmax = _mm256_max_epu8(vsrc1, vsrc2);
    vmin = _mm256_min_epu8(vsrc1, vsrc2);
    vdiff = _mm256_subs_epu8(vmax, vmin);

    __m128i vtmp1 = _mm256_castsi256_si128(vdiff);
    __m128i vtmp2 = _mm256_extracti128_si256(vdiff, 1);
    vdiff1 = _mm256_cvtepu8_epi16(vtmp1);
    vdiff2 = _mm256_cvtepu8_epi16(vtmp2);

    vres1 = _mm256_mullo_epi16(vdiff1, vdiff1);
    vres2 = _mm256_mullo_epi16(vdiff2, vdiff2);
    _mm256_storeu_si256((__m256i *)(dst), vres1);
    _mm256_storeu_si256((__m256i *)(dst + 16), vres2);
    // Set zero to uninitialized memory to avoid uninitialized loads later
    *(int *)(dst + 32) = _mm_cvtsi128_si32(_mm_setzero_si128());

    src1 += stride;
    src2 += stride2;
    dst += sse_stride;
  }
}

static AOM_FORCE_INLINE __m256i xx_load_and_pad(uint16_t *src, int col,
                                                int block_width) {
  __m128i v128tmp = _mm_loadu_si128((__m128i *)(src));
  if (col == 0) {
    // For the first column, replicate the first element twice to the left
    v128tmp = _mm_shuffle_epi8(v128tmp, *(__m128i *)shufflemask_16b[0]);
  }
  if (col == block_width - 4) {
    // For the last column, replicate the last element twice to the right
    v128tmp = _mm_shuffle_epi8(v128tmp, *(__m128i *)shufflemask_16b[1]);
  }
  return _mm256_cvtepu16_epi32(v128tmp);
}

static AOM_FORCE_INLINE int32_t xx_mask_and_hadd(__m256i vsum, int i) {
  // Mask the required 5 values inside the vector
  __m256i vtmp = _mm256_and_si256(vsum, *(__m256i *)sse_bytemask[i]);
  __m128i v128a, v128b;
  // Extract 256b as two 128b registers A and B
  v128a = _mm256_castsi256_si128(vtmp);
  v128b = _mm256_extracti128_si256(vtmp, 1);
  // A = [A0+B0, A1+B1, A2+B2, A3+B3]
  v128a = _mm_add_epi32(v128a, v128b);
  // B = [A2+B2, A3+B3, 0, 0]
  v128b = _mm_srli_si128(v128a, 8);
  // A = [A0+B0+A2+B2, A1+B1+A3+B3, X, X]
  v128a = _mm_add_epi32(v128a, v128b);
  // B = [A1+B1+A3+B3, 0, 0, 0]
  v128b = _mm_srli_si128(v128a, 4);
  // A = [A0+B0+A2+B2+A1+B1+A3+B3, X, X, X]
  v128a = _mm_add_epi32(v128a, v128b);
  return _mm_extract_epi32(v128a, 0);
}

// AVX2 implementation of approx_exp()
static inline __m256 approx_exp_avx2(__m256 y) {
#define A ((1 << 23) / 0.69314718056f)  // (1 << 23) / ln(2)
#define B \
  127  // Offset for the exponent according to IEEE floating point standard.
#define C 60801  // Magic number controls the accuracy of approximation
  const __m256 multiplier = _mm256_set1_ps(A);
  const __m256i offset = _mm256_set1_epi32(B * (1 << 23) - C);

  y = _mm256_mul_ps(y, multiplier);
  y = _mm256_castsi256_ps(_mm256_add_epi32(_mm256_cvttps_epi32(y), offset));
  return y;
#undef A
#undef B
#undef C
}

static void apply_temporal_filter(
    const uint8_t *frame1, const unsigned int stride, const uint8_t *frame2,
    const unsigned int stride2, const int block_width, const int block_height,
    const int *subblock_mses, unsigned int *accumulator, uint16_t *count,
    uint16_t *frame_sse, uint32_t *luma_sse_sum,
    const double inv_num_ref_pixels, const double decay_factor,
    const double inv_factor, const double weight_factor, double *d_factor,
    int tf_wgt_calc_lvl) {
  assert(((block_width == 16) || (block_width == 32)) &&
         ((block_height == 16) || (block_height == 32)));

  uint32_t acc_5x5_sse[BH][BW];

  if (block_width == 32) {
    get_squared_error_32x32_avx2(frame1, stride, frame2, stride2, block_width,
                                 block_height, frame_sse, SSE_STRIDE);
  } else {
    get_squared_error_16x16_avx2(frame1, stride, frame2, stride2, block_width,
                                 block_height, frame_sse, SSE_STRIDE);
  }

  __m256i vsrc[5];

  // Traverse 4 columns at a time
  // First and last columns will require padding
  for (int col = 0; col < block_width; col += 4) {
    uint16_t *src = (col) ? frame_sse + col - 2 : frame_sse;

    // Load and pad(for first and last col) 3 rows from the top
    for (int i = 2; i < 5; i++) {
      vsrc[i] = xx_load_and_pad(src, col, block_width);
      src += SSE_STRIDE;
    }

    // Copy first row to first 2 vectors
    vsrc[0] = vsrc[2];
    vsrc[1] = vsrc[2];

    for (int row = 0; row < block_height; row++) {
      __m256i vsum = _mm256_setzero_si256();

      // Add 5 consecutive rows
      for (int i = 0; i < 5; i++) {
        vsum = _mm256_add_epi32(vsum, vsrc[i]);
      }

      // Push all elements by one element to the top
      for (int i = 0; i < 4; i++) {
        vsrc[i] = vsrc[i + 1];
      }

      // Load next row to the last element
      if (row <= block_height - 4) {
        vsrc[4] = xx_load_and_pad(src, col, block_width);
        src += SSE_STRIDE;
      } else {
        vsrc[4] = vsrc[3];
      }

      // Accumulate the sum horizontally
      for (int i = 0; i < 4; i++) {
        acc_5x5_sse[row][col + i] = xx_mask_and_hadd(vsum, i);
      }
    }
  }

  double subblock_mses_scaled[4];
  double d_factor_decayed[4];
  for (int idx = 0; idx < 4; idx++) {
    subblock_mses_scaled[idx] = subblock_mses[idx] * inv_factor;
    d_factor_decayed[idx] = d_factor[idx] * decay_factor;
  }
  if (tf_wgt_calc_lvl == 0) {
    for (int i = 0, k = 0; i < block_height; i++) {
      const int y_blk_raster_offset = (i >= block_height / 2) * 2;
      for (int j = 0; j < block_width; j++, k++) {
        const int pixel_value = frame2[i * stride2 + j];
        uint32_t diff_sse = acc_5x5_sse[i][j] + luma_sse_sum[i * BW + j];

        const double window_error = diff_sse * inv_num_ref_pixels;
        const int subblock_idx = y_blk_raster_offset + (j >= block_width / 2);
        const double combined_error =
            weight_factor * window_error + subblock_mses_scaled[subblock_idx];

        double scaled_error = combined_error * d_factor_decayed[subblock_idx];
        scaled_error = AOMMIN(scaled_error, 7);
        const int weight = (int)(exp(-scaled_error) * TF_WEIGHT_SCALE);

        count[k] += weight;
        accumulator[k] += weight * pixel_value;
      }
    }
  } else {
    __m256d subblock_mses_reg[4];
    __m256d d_factor_mul_n_decay_qr_invs[4];
    const __m256 zero = _mm256_set1_ps(0.0f);
    const __m256 point_five = _mm256_set1_ps(0.5f);
    const __m256 seven = _mm256_set1_ps(7.0f);
    const __m256d inv_num_ref_pixel_256bit = _mm256_set1_pd(inv_num_ref_pixels);
    const __m256d weight_factor_256bit = _mm256_set1_pd(weight_factor);
    const __m256 tf_weight_scale = _mm256_set1_ps((float)TF_WEIGHT_SCALE);
    // Maintain registers to hold mse and d_factor at subblock level.
    subblock_mses_reg[0] = _mm256_set1_pd(subblock_mses_scaled[0]);
    subblock_mses_reg[1] = _mm256_set1_pd(subblock_mses_scaled[1]);
    subblock_mses_reg[2] = _mm256_set1_pd(subblock_mses_scaled[2]);
    subblock_mses_reg[3] = _mm256_set1_pd(subblock_mses_scaled[3]);
    d_factor_mul_n_decay_qr_invs[0] = _mm256_set1_pd(d_factor_decayed[0]);
    d_factor_mul_n_decay_qr_invs[1] = _mm256_set1_pd(d_factor_decayed[1]);
    d_factor_mul_n_decay_qr_invs[2] = _mm256_set1_pd(d_factor_decayed[2]);
    d_factor_mul_n_decay_qr_invs[3] = _mm256_set1_pd(d_factor_decayed[3]);

    for (int i = 0; i < block_height; i++) {
      const int y_blk_raster_offset = (i >= block_height / 2) * 2;
      uint32_t *luma_sse_sum_temp = luma_sse_sum + i * BW;
      for (int j = 0; j < block_width; j += 8) {
        const __m256i acc_sse =
            _mm256_lddqu_si256((__m256i *)(acc_5x5_sse[i] + j));
        const __m256i luma_sse =
            _mm256_lddqu_si256((__m256i *)((luma_sse_sum_temp + j)));

        // uint32_t diff_sse = acc_5x5_sse[i][j] + luma_sse_sum[i * BW + j];
        const __m256i diff_sse = _mm256_add_epi32(acc_sse, luma_sse);

        const __m256d diff_sse_pd_1 =
            _mm256_cvtepi32_pd(_mm256_castsi256_si128(diff_sse));
        const __m256d diff_sse_pd_2 =
            _mm256_cvtepi32_pd(_mm256_extracti128_si256(diff_sse, 1));

        // const double window_error = diff_sse * inv_num_ref_pixels;
        const __m256d window_error_1 =
            _mm256_mul_pd(diff_sse_pd_1, inv_num_ref_pixel_256bit);
        const __m256d window_error_2 =
            _mm256_mul_pd(diff_sse_pd_2, inv_num_ref_pixel_256bit);

        // const int subblock_idx = y_blk_raster_offset + (j >= block_width /
        // 2);
        const int subblock_idx = y_blk_raster_offset + (j >= block_width / 2);
        const __m256d blk_error = subblock_mses_reg[subblock_idx];

        // const double combined_error =
        // weight_factor *window_error + subblock_mses_scaled[subblock_idx];
        const __m256d combined_error_1 = _mm256_add_pd(
            _mm256_mul_pd(window_error_1, weight_factor_256bit), blk_error);

        const __m256d combined_error_2 = _mm256_add_pd(
            _mm256_mul_pd(window_error_2, weight_factor_256bit), blk_error);

        // d_factor_decayed[subblock_idx]
        const __m256d d_fact_mul_n_decay =
            d_factor_mul_n_decay_qr_invs[subblock_idx];

        // double scaled_error = combined_error *
        // d_factor_decayed[subblock_idx];
        const __m256d scaled_error_1 =
            _mm256_mul_pd(combined_error_1, d_fact_mul_n_decay);
        const __m256d scaled_error_2 =
            _mm256_mul_pd(combined_error_2, d_fact_mul_n_decay);

        const __m128 scaled_error_ps_1 = _mm256_cvtpd_ps(scaled_error_1);
        const __m128 scaled_error_ps_2 = _mm256_cvtpd_ps(scaled_error_2);

        const __m256 scaled_error_ps = _mm256_insertf128_ps(
            _mm256_castps128_ps256(scaled_error_ps_1), scaled_error_ps_2, 0x1);

        // scaled_error = AOMMIN(scaled_error, 7);
        const __m256 scaled_diff_ps = _mm256_min_ps(scaled_error_ps, seven);
        const __m256 minus_scaled_diff_ps = _mm256_sub_ps(zero, scaled_diff_ps);
        // const int weight =
        //(int)(approx_exp((float)-scaled_error) * TF_WEIGHT_SCALE + 0.5f);
        const __m256 exp_result = approx_exp_avx2(minus_scaled_diff_ps);
        const __m256 scale_weight_exp_result =
            _mm256_mul_ps(exp_result, tf_weight_scale);
        const __m256 round_result =
            _mm256_add_ps(scale_weight_exp_result, point_five);
        __m256i weights_in_32bit = _mm256_cvttps_epi32(round_result);

        __m128i weights_in_16bit =
            _mm_packus_epi32(_mm256_castsi256_si128(weights_in_32bit),
                             _mm256_extractf128_si256(weights_in_32bit, 0x1));

        // count[k] += weight;
        // accumulator[k] += weight * pixel_value;
        const int stride_idx = i * stride2 + j;
        const __m128i count_array =
            _mm_loadu_si128((__m128i *)(count + stride_idx));
        _mm_storeu_si128((__m128i *)(count + stride_idx),
                         _mm_add_epi16(count_array, weights_in_16bit));

        const __m256i accumulator_array =
            _mm256_loadu_si256((__m256i *)(accumulator + stride_idx));
        const __m128i pred_values =
            _mm_loadl_epi64((__m128i *)(frame2 + stride_idx));

        const __m256i pred_values_u32 = _mm256_cvtepu8_epi32(pred_values);
        const __m256i mull_frame2_weight_u32 =
            _mm256_mullo_epi32(pred_values_u32, weights_in_32bit);
        _mm256_storeu_si256(
            (__m256i *)(accumulator + stride_idx),
            _mm256_add_epi32(accumulator_array, mull_frame2_weight_u32));
      }
    }
  }
}

void av1_apply_temporal_filter_avx2(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double *noise_levels, const MV *subblock_mvs,
    const int *subblock_mses, const int q_factor, const int filter_strength,
    int tf_wgt_calc_lvl, const uint8_t *pred, uint32_t *accum,
    uint16_t *count) {
  const int is_high_bitdepth = frame_to_filter->flags & YV12_FLAG_HIGHBITDEPTH;
  assert(block_size == BLOCK_32X32 && "Only support 32x32 block with avx2!");
  assert(TF_WINDOW_LENGTH == 5 && "Only support window length 5 with avx2!");
  assert(!is_high_bitdepth && "Only support low bit-depth with avx2!");
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);
  (void)is_high_bitdepth;

  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  const int min_frame_size = AOMMIN(frame_height, frame_width);
  // Variables to simplify combined error calculation.
  const double inv_factor = 1.0 / ((TF_WINDOW_BLOCK_BALANCE_WEIGHT + 1) *
                                   TF_SEARCH_ERROR_NORM_WEIGHT);
  const double weight_factor =
      (double)TF_WINDOW_BLOCK_BALANCE_WEIGHT * inv_factor;
  // Adjust filtering based on q.
  // Larger q -> stronger filtering -> larger weight.
  // Smaller q -> weaker filtering -> smaller weight.
  double q_decay = pow((double)q_factor / TF_Q_DECAY_THRESHOLD, 2);
  q_decay = CLIP(q_decay, 1e-5, 1);
  if (q_factor >= TF_QINDEX_CUTOFF) {
    // Max q_factor is 255, therefore the upper bound of q_decay is 8.
    // We do not need a clip here.
    q_decay = 0.5 * pow((double)q_factor / 64, 2);
  }
  // Smaller strength -> smaller filtering weight.
  double s_decay = pow((double)filter_strength / TF_STRENGTH_THRESHOLD, 2);
  s_decay = CLIP(s_decay, 1e-5, 1);
  double d_factor[4] = { 0 };
  uint16_t frame_sse[SSE_STRIDE * BH] = { 0 };
  uint32_t luma_sse_sum[BW * BH] = { 0 };

  for (int subblock_idx = 0; subblock_idx < 4; subblock_idx++) {
    // Larger motion vector -> smaller filtering weight.
    const MV mv = subblock_mvs[subblock_idx];
    const double distance = sqrt(pow(mv.row, 2) + pow(mv.col, 2));
    double distance_threshold = min_frame_size * TF_SEARCH_DISTANCE_THRESHOLD;
    distance_threshold = AOMMAX(distance_threshold, 1);
    d_factor[subblock_idx] = distance / distance_threshold;
    d_factor[subblock_idx] = AOMMAX(d_factor[subblock_idx], 1);
  }

  // Handle planes in sequence.
  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const uint32_t plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const uint32_t plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const uint32_t frame_stride = frame_to_filter->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;

    const uint8_t *ref = frame_to_filter->buffers[plane] + frame_offset;
    const int ss_x_shift =
        mbd->plane[plane].subsampling_x - mbd->plane[AOM_PLANE_Y].subsampling_x;
    const int ss_y_shift =
        mbd->plane[plane].subsampling_y - mbd->plane[AOM_PLANE_Y].subsampling_y;
    const int num_ref_pixels = TF_WINDOW_LENGTH * TF_WINDOW_LENGTH +
                               ((plane) ? (1 << (ss_x_shift + ss_y_shift)) : 0);
    const double inv_num_ref_pixels = 1.0 / num_ref_pixels;
    // Larger noise -> larger filtering weight.
    const double n_decay = 0.5 + log(2 * noise_levels[plane] + 5.0);
    // Decay factors for non-local mean approach.
    const double decay_factor = 1 / (n_decay * q_decay * s_decay);

    // Filter U-plane and V-plane using Y-plane. This is because motion
    // search is only done on Y-plane, so the information from Y-plane
    // will be more accurate. The luma sse sum is reused in both chroma
    // planes.
    if (plane == AOM_PLANE_U) {
      for (unsigned int i = 0, k = 0; i < plane_h; i++) {
        for (unsigned int j = 0; j < plane_w; j++, k++) {
          for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
            for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
              const int yy = (i << ss_y_shift) + ii;  // Y-coord on Y-plane.
              const int xx = (j << ss_x_shift) + jj;  // X-coord on Y-plane.
              luma_sse_sum[i * BW + j] += frame_sse[yy * SSE_STRIDE + xx];
            }
          }
        }
      }
    }

    apply_temporal_filter(ref, frame_stride, pred + plane_offset, plane_w,
                          plane_w, plane_h, subblock_mses, accum + plane_offset,
                          count + plane_offset, frame_sse, luma_sse_sum,
                          inv_num_ref_pixels, decay_factor, inv_factor,
                          weight_factor, d_factor, tf_wgt_calc_lvl);
    plane_offset += plane_h * plane_w;
  }
}
