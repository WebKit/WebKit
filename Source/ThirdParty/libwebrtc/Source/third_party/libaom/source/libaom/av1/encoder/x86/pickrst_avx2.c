/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>  // AVX2
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"
#include "aom_dsp/x86/transpose_sse2.h"

#include "config/av1_rtcd.h"
#include "av1/common/restoration.h"
#include "av1/encoder/pickrst.h"

static INLINE void acc_stat_avx2(int32_t *dst, const uint8_t *src,
                                 const __m128i *shuffle, const __m256i *kl) {
  const __m128i s = _mm_shuffle_epi8(xx_loadu_128(src), *shuffle);
  const __m256i d0 = _mm256_madd_epi16(*kl, _mm256_cvtepu8_epi16(s));
  const __m256i dst0 = yy_load_256(dst);
  const __m256i r0 = _mm256_add_epi32(dst0, d0);
  yy_store_256(dst, r0);
}

static INLINE void acc_stat_win7_one_line_avx2(
    const uint8_t *dgd, const uint8_t *src, int h_start, int h_end,
    int dgd_stride, const __m128i *shuffle, int32_t *sumX,
    int32_t sumY[WIENER_WIN][WIENER_WIN], int32_t M_int[WIENER_WIN][WIENER_WIN],
    int32_t H_int[WIENER_WIN2][WIENER_WIN * 8]) {
  int j, k, l;
  const int wiener_win = WIENER_WIN;
  // Main loop handles two pixels at a time
  // We can assume that h_start is even, since it will always be aligned to
  // a tile edge + some number of restoration units, and both of those will
  // be 64-pixel aligned.
  // However, at the edge of the image, h_end may be odd, so we need to handle
  // that case correctly.
  assert(h_start % 2 == 0);
  const int h_end_even = h_end & ~1;
  const int has_odd_pixel = h_end & 1;
  for (j = h_start; j < h_end_even; j += 2) {
    const uint8_t X1 = src[j];
    const uint8_t X2 = src[j + 1];
    *sumX += X1 + X2;
    const uint8_t *dgd_ij = dgd + j;
    for (k = 0; k < wiener_win; k++) {
      const uint8_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int32_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint8_t D1 = dgd_ijk[l];
        const uint8_t D2 = dgd_ijk[l + 1];
        sumY[k][l] += D1 + D2;
        M_int[k][l] += D1 * X1 + D2 * X2;

        const __m256i kl =
            _mm256_cvtepu8_epi16(_mm_set1_epi16(loadu_uint16(dgd_ijk + l)));
        acc_stat_avx2(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 5 * 8, dgd_ij + 5 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 6 * 8, dgd_ij + 6 * dgd_stride, shuffle, &kl);
      }
    }
  }
  // If the width is odd, add in the final pixel
  if (has_odd_pixel) {
    const uint8_t X1 = src[j];
    *sumX += X1;
    const uint8_t *dgd_ij = dgd + j;
    for (k = 0; k < wiener_win; k++) {
      const uint8_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int32_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint8_t D1 = dgd_ijk[l];
        sumY[k][l] += D1;
        M_int[k][l] += D1 * X1;

        // The `acc_stat_avx2` function wants its input to have interleaved
        // copies of two pixels, but we only have one. However, the pixels
        // are (effectively) used as inputs to a multiply-accumulate.
        // So if we set the extra pixel slot to 0, then it is effectively
        // ignored.
        const __m256i kl = _mm256_cvtepu8_epi16(_mm_set1_epi16((uint16_t)D1));
        acc_stat_avx2(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 5 * 8, dgd_ij + 5 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 6 * 8, dgd_ij + 6 * dgd_stride, shuffle, &kl);
      }
    }
  }
}

static INLINE void compute_stats_win7_opt_avx2(
    const uint8_t *dgd, const uint8_t *src, int h_start, int h_end, int v_start,
    int v_end, int dgd_stride, int src_stride, int64_t *M, int64_t *H,
    int use_downsampled_wiener_stats) {
  int i, j, k, l, m, n;
  const int wiener_win = WIENER_WIN;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  uint8_t avg = find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  int32_t M_int32[WIENER_WIN][WIENER_WIN] = { { 0 } };
  int64_t M_int64[WIENER_WIN][WIENER_WIN] = { { 0 } };
  int32_t M_int32_row[WIENER_WIN][WIENER_WIN] = { { 0 } };

  DECLARE_ALIGNED(32, int32_t,
                  H_int32[WIENER_WIN2][WIENER_WIN * 8]) = { { 0 } };
  DECLARE_ALIGNED(32, int32_t,
                  H_int32_row[WIENER_WIN2][WIENER_WIN * 8]) = { { 0 } };
  int64_t H_int64[WIENER_WIN2][WIENER_WIN * 8] = { { 0 } };
  int32_t sumY[WIENER_WIN][WIENER_WIN] = { { 0 } };
  int32_t sumX = 0;
  const uint8_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;
  int downsample_factor =
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;
  int32_t sumX_row = 0;
  int32_t sumY_row[WIENER_WIN][WIENER_WIN] = { { 0 } };

  const __m128i shuffle = xx_loadu_128(g_shuffle_stats_data);
  for (j = v_start; j < v_end; j += 64) {
    const int vert_end = AOMMIN(64, v_end - j) + j;
    for (i = j; i < vert_end; i = i + downsample_factor) {
      if (use_downsampled_wiener_stats &&
          (vert_end - i < WIENER_STATS_DOWNSAMPLE_FACTOR)) {
        downsample_factor = vert_end - i;
      }
      sumX_row = 0;
      memset(sumY_row, 0, sizeof(int32_t) * WIENER_WIN * WIENER_WIN);
      memset(M_int32_row, 0, sizeof(int32_t) * WIENER_WIN * WIENER_WIN);
      memset(H_int32_row, 0, sizeof(int32_t) * WIENER_WIN2 * (WIENER_WIN * 8));
      acc_stat_win7_one_line_avx2(
          dgd_win + i * dgd_stride, src + i * src_stride, h_start, h_end,
          dgd_stride, &shuffle, &sumX_row, sumY_row, M_int32_row, H_int32_row);
      sumX += sumX_row * downsample_factor;

      // Scale M matrix based on the downsampling factor
      for (k = 0; k < wiener_win; ++k) {
        for (l = 0; l < wiener_win; ++l) {
          sumY[k][l] += (sumY_row[k][l] * downsample_factor);
          M_int32[k][l] += (M_int32_row[k][l] * downsample_factor);
        }
      }
      // Scale H matrix based on the downsampling factor
      for (k = 0; k < WIENER_WIN2; ++k) {
        for (l = 0; l < WIENER_WIN * 8; ++l) {
          H_int32[k][l] += (H_int32_row[k][l] * downsample_factor);
        }
      }
    }
    for (k = 0; k < wiener_win; ++k) {
      for (l = 0; l < wiener_win; ++l) {
        M_int64[k][l] += M_int32[k][l];
        M_int32[k][l] = 0;
      }
    }
    for (k = 0; k < WIENER_WIN2; ++k) {
      for (l = 0; l < WIENER_WIN * 8; ++l) {
        H_int64[k][l] += H_int32[k][l];
        H_int32[k][l] = 0;
      }
    }
  }

  const int64_t avg_square_sum = (int64_t)avg * (int64_t)avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      const int32_t idx0 = l * wiener_win + k;
      M[idx0] =
          M_int64[k][l] + (avg_square_sum - (int64_t)avg * (sumX + sumY[k][l]));
      int64_t *H_ = H + idx0 * wiener_win2;
      int64_t *H_int_ = &H_int64[idx0][0];
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H_[m * wiener_win + n] = H_int_[n * 8 + m] + avg_square_sum -
                                   (int64_t)avg * (sumY[k][l] + sumY[n][m]);
        }
      }
    }
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE void acc_stat_highbd_avx2(int64_t *dst, const uint16_t *dgd,
                                        const __m256i *shuffle,
                                        const __m256i *dgd_ijkl) {
  // Load two 128-bit chunks from dgd
  const __m256i s0 = _mm256_inserti128_si256(
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)dgd)),
      _mm_loadu_si128((__m128i *)(dgd + 4)), 1);
  // s0 = [11 10 9 8 7 6 5 4] [7 6 5 4 3 2 1 0] as u16 (values are dgd indices)
  // The weird order is so the shuffle stays within 128-bit lanes

  // Shuffle 16x u16 values within lanes according to the mask:
  // [0 1 1 2 2 3 3 4] [0 1 1 2 2 3 3 4]
  // (Actually we shuffle u8 values as there's no 16-bit shuffle)
  const __m256i s1 = _mm256_shuffle_epi8(s0, *shuffle);
  // s1 = [8 7 7 6 6 5 5 4] [4 3 3 2 2 1 1 0] as u16 (values are dgd indices)

  // Multiply 16x 16-bit integers in dgd_ijkl and s1, resulting in 16x 32-bit
  // integers then horizontally add pairs of these integers resulting in 8x
  // 32-bit integers
  const __m256i d0 = _mm256_madd_epi16(*dgd_ijkl, s1);
  // d0 = [a b c d] [e f g h] as u32

  // Take the lower-half of d0, extend to u64, add it on to dst (H)
  const __m256i d0l = _mm256_cvtepu32_epi64(_mm256_extracti128_si256(d0, 0));
  // d0l = [a b] [c d] as u64
  const __m256i dst0 = yy_load_256(dst);
  yy_store_256(dst, _mm256_add_epi64(d0l, dst0));

  // Take the upper-half of d0, extend to u64, add it on to dst (H)
  const __m256i d0h = _mm256_cvtepu32_epi64(_mm256_extracti128_si256(d0, 1));
  // d0h = [e f] [g h] as u64
  const __m256i dst1 = yy_load_256(dst + 4);
  yy_store_256(dst + 4, _mm256_add_epi64(d0h, dst1));
}

static INLINE void acc_stat_highbd_win7_one_line_avx2(
    const uint16_t *dgd, const uint16_t *src, int h_start, int h_end,
    int dgd_stride, const __m256i *shuffle, int32_t *sumX,
    int32_t sumY[WIENER_WIN][WIENER_WIN], int64_t M_int[WIENER_WIN][WIENER_WIN],
    int64_t H_int[WIENER_WIN2][WIENER_WIN * 8]) {
  int j, k, l;
  const int wiener_win = WIENER_WIN;
  // Main loop handles two pixels at a time
  // We can assume that h_start is even, since it will always be aligned to
  // a tile edge + some number of restoration units, and both of those will
  // be 64-pixel aligned.
  // However, at the edge of the image, h_end may be odd, so we need to handle
  // that case correctly.
  assert(h_start % 2 == 0);
  const int h_end_even = h_end & ~1;
  const int has_odd_pixel = h_end & 1;
  for (j = h_start; j < h_end_even; j += 2) {
    const uint16_t X1 = src[j];
    const uint16_t X2 = src[j + 1];
    *sumX += X1 + X2;
    const uint16_t *dgd_ij = dgd + j;
    for (k = 0; k < wiener_win; k++) {
      const uint16_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int64_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint16_t D1 = dgd_ijk[l];
        const uint16_t D2 = dgd_ijk[l + 1];
        sumY[k][l] += D1 + D2;
        M_int[k][l] += D1 * X1 + D2 * X2;

        // Load two u16 values from dgd_ijkl combined as a u32,
        // then broadcast to 8x u32 slots of a 256
        const __m256i dgd_ijkl = _mm256_set1_epi32(loadu_uint32(dgd_ijk + l));
        // dgd_ijkl = [y x y x y x y x] [y x y x y x y x] where each is a u16

        acc_stat_highbd_avx2(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 5 * 8, dgd_ij + 5 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 6 * 8, dgd_ij + 6 * dgd_stride, shuffle,
                             &dgd_ijkl);
      }
    }
  }
  // If the width is odd, add in the final pixel
  if (has_odd_pixel) {
    const uint16_t X1 = src[j];
    *sumX += X1;
    const uint16_t *dgd_ij = dgd + j;
    for (k = 0; k < wiener_win; k++) {
      const uint16_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int64_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint16_t D1 = dgd_ijk[l];
        sumY[k][l] += D1;
        M_int[k][l] += D1 * X1;

        // The `acc_stat_highbd_avx2` function wants its input to have
        // interleaved copies of two pixels, but we only have one. However, the
        // pixels are (effectively) used as inputs to a multiply-accumulate. So
        // if we set the extra pixel slot to 0, then it is effectively ignored.
        const __m256i dgd_ijkl = _mm256_set1_epi32((uint32_t)D1);

        acc_stat_highbd_avx2(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 5 * 8, dgd_ij + 5 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 6 * 8, dgd_ij + 6 * dgd_stride, shuffle,
                             &dgd_ijkl);
      }
    }
  }
}

static INLINE void compute_stats_highbd_win7_opt_avx2(
    const uint8_t *dgd8, const uint8_t *src8, int h_start, int h_end,
    int v_start, int v_end, int dgd_stride, int src_stride, int64_t *M,
    int64_t *H, aom_bit_depth_t bit_depth) {
  int i, j, k, l, m, n;
  const int wiener_win = WIENER_WIN;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const uint16_t avg =
      find_average_highbd(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  int64_t M_int[WIENER_WIN][WIENER_WIN] = { { 0 } };
  DECLARE_ALIGNED(32, int64_t, H_int[WIENER_WIN2][WIENER_WIN * 8]) = { { 0 } };
  int32_t sumY[WIENER_WIN][WIENER_WIN] = { { 0 } };
  int32_t sumX = 0;
  const uint16_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  const __m256i shuffle = yy_loadu_256(g_shuffle_stats_highbd_data);
  for (j = v_start; j < v_end; j += 64) {
    const int vert_end = AOMMIN(64, v_end - j) + j;
    for (i = j; i < vert_end; i++) {
      acc_stat_highbd_win7_one_line_avx2(
          dgd_win + i * dgd_stride, src + i * src_stride, h_start, h_end,
          dgd_stride, &shuffle, &sumX, sumY, M_int, H_int);
    }
  }

  uint8_t bit_depth_divider = 1;
  if (bit_depth == AOM_BITS_12)
    bit_depth_divider = 16;
  else if (bit_depth == AOM_BITS_10)
    bit_depth_divider = 4;

  const int64_t avg_square_sum = (int64_t)avg * (int64_t)avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      const int32_t idx0 = l * wiener_win + k;
      M[idx0] = (M_int[k][l] +
                 (avg_square_sum - (int64_t)avg * (sumX + sumY[k][l]))) /
                bit_depth_divider;
      int64_t *H_ = H + idx0 * wiener_win2;
      int64_t *H_int_ = &H_int[idx0][0];
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H_[m * wiener_win + n] =
              (H_int_[n * 8 + m] +
               (avg_square_sum - (int64_t)avg * (sumY[k][l] + sumY[n][m]))) /
              bit_depth_divider;
        }
      }
    }
  }
}

static INLINE void acc_stat_highbd_win5_one_line_avx2(
    const uint16_t *dgd, const uint16_t *src, int h_start, int h_end,
    int dgd_stride, const __m256i *shuffle, int32_t *sumX,
    int32_t sumY[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA],
    int64_t M_int[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA],
    int64_t H_int[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8]) {
  int j, k, l;
  const int wiener_win = WIENER_WIN_CHROMA;
  // Main loop handles two pixels at a time
  // We can assume that h_start is even, since it will always be aligned to
  // a tile edge + some number of restoration units, and both of those will
  // be 64-pixel aligned.
  // However, at the edge of the image, h_end may be odd, so we need to handle
  // that case correctly.
  assert(h_start % 2 == 0);
  const int h_end_even = h_end & ~1;
  const int has_odd_pixel = h_end & 1;
  for (j = h_start; j < h_end_even; j += 2) {
    const uint16_t X1 = src[j];
    const uint16_t X2 = src[j + 1];
    *sumX += X1 + X2;
    const uint16_t *dgd_ij = dgd + j;
    for (k = 0; k < wiener_win; k++) {
      const uint16_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int64_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint16_t D1 = dgd_ijk[l];
        const uint16_t D2 = dgd_ijk[l + 1];
        sumY[k][l] += D1 + D2;
        M_int[k][l] += D1 * X1 + D2 * X2;

        // Load two u16 values from dgd_ijkl combined as a u32,
        // then broadcast to 8x u32 slots of a 256
        const __m256i dgd_ijkl = _mm256_set1_epi32(loadu_uint32(dgd_ijk + l));
        // dgd_ijkl = [x y x y x y x y] [x y x y x y x y] where each is a u16

        acc_stat_highbd_avx2(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle,
                             &dgd_ijkl);
      }
    }
  }
  // If the width is odd, add in the final pixel
  if (has_odd_pixel) {
    const uint16_t X1 = src[j];
    *sumX += X1;
    const uint16_t *dgd_ij = dgd + j;
    for (k = 0; k < wiener_win; k++) {
      const uint16_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int64_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint16_t D1 = dgd_ijk[l];
        sumY[k][l] += D1;
        M_int[k][l] += D1 * X1;

        // The `acc_stat_highbd_avx2` function wants its input to have
        // interleaved copies of two pixels, but we only have one. However, the
        // pixels are (effectively) used as inputs to a multiply-accumulate. So
        // if we set the extra pixel slot to 0, then it is effectively ignored.
        const __m256i dgd_ijkl = _mm256_set1_epi32((uint32_t)D1);

        acc_stat_highbd_avx2(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle,
                             &dgd_ijkl);
        acc_stat_highbd_avx2(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle,
                             &dgd_ijkl);
      }
    }
  }
}

static INLINE void compute_stats_highbd_win5_opt_avx2(
    const uint8_t *dgd8, const uint8_t *src8, int h_start, int h_end,
    int v_start, int v_end, int dgd_stride, int src_stride, int64_t *M,
    int64_t *H, aom_bit_depth_t bit_depth) {
  int i, j, k, l, m, n;
  const int wiener_win = WIENER_WIN_CHROMA;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const uint16_t avg =
      find_average_highbd(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  int64_t M_int64[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  DECLARE_ALIGNED(
      32, int64_t,
      H_int64[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8]) = { { 0 } };
  int32_t sumY[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  int32_t sumX = 0;
  const uint16_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  const __m256i shuffle = yy_loadu_256(g_shuffle_stats_highbd_data);
  for (j = v_start; j < v_end; j += 64) {
    const int vert_end = AOMMIN(64, v_end - j) + j;
    for (i = j; i < vert_end; i++) {
      acc_stat_highbd_win5_one_line_avx2(
          dgd_win + i * dgd_stride, src + i * src_stride, h_start, h_end,
          dgd_stride, &shuffle, &sumX, sumY, M_int64, H_int64);
    }
  }

  uint8_t bit_depth_divider = 1;
  if (bit_depth == AOM_BITS_12)
    bit_depth_divider = 16;
  else if (bit_depth == AOM_BITS_10)
    bit_depth_divider = 4;

  const int64_t avg_square_sum = (int64_t)avg * (int64_t)avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      const int32_t idx0 = l * wiener_win + k;
      M[idx0] = (M_int64[k][l] +
                 (avg_square_sum - (int64_t)avg * (sumX + sumY[k][l]))) /
                bit_depth_divider;
      int64_t *H_ = H + idx0 * wiener_win2;
      int64_t *H_int_ = &H_int64[idx0][0];
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H_[m * wiener_win + n] =
              (H_int_[n * 8 + m] +
               (avg_square_sum - (int64_t)avg * (sumY[k][l] + sumY[n][m]))) /
              bit_depth_divider;
        }
      }
    }
  }
}

void av1_compute_stats_highbd_avx2(int wiener_win, const uint8_t *dgd8,
                                   const uint8_t *src8, int h_start, int h_end,
                                   int v_start, int v_end, int dgd_stride,
                                   int src_stride, int64_t *M, int64_t *H,
                                   aom_bit_depth_t bit_depth) {
  if (wiener_win == WIENER_WIN) {
    compute_stats_highbd_win7_opt_avx2(dgd8, src8, h_start, h_end, v_start,
                                       v_end, dgd_stride, src_stride, M, H,
                                       bit_depth);
  } else if (wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_highbd_win5_opt_avx2(dgd8, src8, h_start, h_end, v_start,
                                       v_end, dgd_stride, src_stride, M, H,
                                       bit_depth);
  } else {
    av1_compute_stats_highbd_c(wiener_win, dgd8, src8, h_start, h_end, v_start,
                               v_end, dgd_stride, src_stride, M, H, bit_depth);
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static INLINE void acc_stat_win5_one_line_avx2(
    const uint8_t *dgd, const uint8_t *src, int h_start, int h_end,
    int dgd_stride, const __m128i *shuffle, int32_t *sumX,
    int32_t sumY[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA],
    int32_t M_int[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA],
    int32_t H_int[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8]) {
  int j, k, l;
  const int wiener_win = WIENER_WIN_CHROMA;
  // Main loop handles two pixels at a time
  // We can assume that h_start is even, since it will always be aligned to
  // a tile edge + some number of restoration units, and both of those will
  // be 64-pixel aligned.
  // However, at the edge of the image, h_end may be odd, so we need to handle
  // that case correctly.
  assert(h_start % 2 == 0);
  const int h_end_even = h_end & ~1;
  const int has_odd_pixel = h_end & 1;
  for (j = h_start; j < h_end_even; j += 2) {
    const uint8_t X1 = src[j];
    const uint8_t X2 = src[j + 1];
    *sumX += X1 + X2;
    const uint8_t *dgd_ij = dgd + j;
    for (k = 0; k < wiener_win; k++) {
      const uint8_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int32_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint8_t D1 = dgd_ijk[l];
        const uint8_t D2 = dgd_ijk[l + 1];
        sumY[k][l] += D1 + D2;
        M_int[k][l] += D1 * X1 + D2 * X2;

        const __m256i kl =
            _mm256_cvtepu8_epi16(_mm_set1_epi16(loadu_uint16(dgd_ijk + l)));
        acc_stat_avx2(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle, &kl);
      }
    }
  }
  // If the width is odd, add in the final pixel
  if (has_odd_pixel) {
    const uint8_t X1 = src[j];
    *sumX += X1;
    const uint8_t *dgd_ij = dgd + j;
    for (k = 0; k < wiener_win; k++) {
      const uint8_t *dgd_ijk = dgd_ij + k * dgd_stride;
      for (l = 0; l < wiener_win; l++) {
        int32_t *H_ = &H_int[(l * wiener_win + k)][0];
        const uint8_t D1 = dgd_ijk[l];
        sumY[k][l] += D1;
        M_int[k][l] += D1 * X1;

        // The `acc_stat_avx2` function wants its input to have interleaved
        // copies of two pixels, but we only have one. However, the pixels
        // are (effectively) used as inputs to a multiply-accumulate.
        // So if we set the extra pixel slot to 0, then it is effectively
        // ignored.
        const __m256i kl = _mm256_cvtepu8_epi16(_mm_set1_epi16((uint16_t)D1));
        acc_stat_avx2(H_ + 0 * 8, dgd_ij + 0 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 1 * 8, dgd_ij + 1 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 2 * 8, dgd_ij + 2 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 3 * 8, dgd_ij + 3 * dgd_stride, shuffle, &kl);
        acc_stat_avx2(H_ + 4 * 8, dgd_ij + 4 * dgd_stride, shuffle, &kl);
      }
    }
  }
}

static INLINE void compute_stats_win5_opt_avx2(
    const uint8_t *dgd, const uint8_t *src, int h_start, int h_end, int v_start,
    int v_end, int dgd_stride, int src_stride, int64_t *M, int64_t *H,
    int use_downsampled_wiener_stats) {
  int i, j, k, l, m, n;
  const int wiener_win = WIENER_WIN_CHROMA;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  uint8_t avg = find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  int32_t M_int32[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  int32_t M_int32_row[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  int64_t M_int64[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  DECLARE_ALIGNED(
      32, int32_t,
      H_int32[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8]) = { { 0 } };
  DECLARE_ALIGNED(
      32, int32_t,
      H_int32_row[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8]) = { { 0 } };
  int64_t H_int64[WIENER_WIN2_CHROMA][WIENER_WIN_CHROMA * 8] = { { 0 } };
  int32_t sumY[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };
  int32_t sumX = 0;
  const uint8_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;
  int downsample_factor =
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;
  int32_t sumX_row = 0;
  int32_t sumY_row[WIENER_WIN_CHROMA][WIENER_WIN_CHROMA] = { { 0 } };

  const __m128i shuffle = xx_loadu_128(g_shuffle_stats_data);
  for (j = v_start; j < v_end; j += 64) {
    const int vert_end = AOMMIN(64, v_end - j) + j;
    for (i = j; i < vert_end; i = i + downsample_factor) {
      if (use_downsampled_wiener_stats &&
          (vert_end - i < WIENER_STATS_DOWNSAMPLE_FACTOR)) {
        downsample_factor = vert_end - i;
      }
      sumX_row = 0;
      memset(sumY_row, 0,
             sizeof(int32_t) * WIENER_WIN_CHROMA * WIENER_WIN_CHROMA);
      memset(M_int32_row, 0,
             sizeof(int32_t) * WIENER_WIN_CHROMA * WIENER_WIN_CHROMA);
      memset(H_int32_row, 0,
             sizeof(int32_t) * WIENER_WIN2_CHROMA * (WIENER_WIN_CHROMA * 8));
      acc_stat_win5_one_line_avx2(
          dgd_win + i * dgd_stride, src + i * src_stride, h_start, h_end,
          dgd_stride, &shuffle, &sumX_row, sumY_row, M_int32_row, H_int32_row);
      sumX += sumX_row * downsample_factor;

      // Scale M matrix based on the downsampling factor
      for (k = 0; k < wiener_win; ++k) {
        for (l = 0; l < wiener_win; ++l) {
          sumY[k][l] += (sumY_row[k][l] * downsample_factor);
          M_int32[k][l] += (M_int32_row[k][l] * downsample_factor);
        }
      }
      // Scale H matrix based on the downsampling factor
      for (k = 0; k < WIENER_WIN2_CHROMA; ++k) {
        for (l = 0; l < WIENER_WIN_CHROMA * 8; ++l) {
          H_int32[k][l] += (H_int32_row[k][l] * downsample_factor);
        }
      }
    }
    for (k = 0; k < wiener_win; ++k) {
      for (l = 0; l < wiener_win; ++l) {
        M_int64[k][l] += M_int32[k][l];
        M_int32[k][l] = 0;
      }
    }
    for (k = 0; k < WIENER_WIN2_CHROMA; ++k) {
      for (l = 0; l < WIENER_WIN_CHROMA * 8; ++l) {
        H_int64[k][l] += H_int32[k][l];
        H_int32[k][l] = 0;
      }
    }
  }

  const int64_t avg_square_sum = (int64_t)avg * (int64_t)avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      const int32_t idx0 = l * wiener_win + k;
      M[idx0] =
          M_int64[k][l] + (avg_square_sum - (int64_t)avg * (sumX + sumY[k][l]));
      int64_t *H_ = H + idx0 * wiener_win2;
      int64_t *H_int_ = &H_int64[idx0][0];
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H_[m * wiener_win + n] = H_int_[n * 8 + m] + avg_square_sum -
                                   (int64_t)avg * (sumY[k][l] + sumY[n][m]);
        }
      }
    }
  }
}

void av1_compute_stats_avx2(int wiener_win, const uint8_t *dgd,
                            const uint8_t *src, int h_start, int h_end,
                            int v_start, int v_end, int dgd_stride,
                            int src_stride, int64_t *M, int64_t *H,
                            int use_downsampled_wiener_stats) {
  if (wiener_win == WIENER_WIN) {
    compute_stats_win7_opt_avx2(dgd, src, h_start, h_end, v_start, v_end,
                                dgd_stride, src_stride, M, H,
                                use_downsampled_wiener_stats);
  } else if (wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_win5_opt_avx2(dgd, src, h_start, h_end, v_start, v_end,
                                dgd_stride, src_stride, M, H,
                                use_downsampled_wiener_stats);
  } else {
    av1_compute_stats_c(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                        dgd_stride, src_stride, M, H,
                        use_downsampled_wiener_stats);
  }
}

static INLINE __m256i pair_set_epi16(int a, int b) {
  return _mm256_set1_epi32(
      (int32_t)(((uint16_t)(a)) | (((uint32_t)(b)) << 16)));
}

int64_t av1_lowbd_pixel_proj_error_avx2(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params) {
  int i, j, k;
  const int32_t shift = SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS;
  const __m256i rounding = _mm256_set1_epi32(1 << (shift - 1));
  __m256i sum64 = _mm256_setzero_si256();
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  int64_t err = 0;
  if (params->r[0] > 0 && params->r[1] > 0) {
    __m256i xq_coeff = pair_set_epi16(xq[0], xq[1]);
    for (i = 0; i < height; ++i) {
      __m256i sum32 = _mm256_setzero_si256();
      for (j = 0; j <= width - 16; j += 16) {
        const __m256i d0 = _mm256_cvtepu8_epi16(xx_loadu_128(dat + j));
        const __m256i s0 = _mm256_cvtepu8_epi16(xx_loadu_128(src + j));
        const __m256i flt0_16b = _mm256_permute4x64_epi64(
            _mm256_packs_epi32(yy_loadu_256(flt0 + j),
                               yy_loadu_256(flt0 + j + 8)),
            0xd8);
        const __m256i flt1_16b = _mm256_permute4x64_epi64(
            _mm256_packs_epi32(yy_loadu_256(flt1 + j),
                               yy_loadu_256(flt1 + j + 8)),
            0xd8);
        const __m256i u0 = _mm256_slli_epi16(d0, SGRPROJ_RST_BITS);
        const __m256i flt0_0_sub_u = _mm256_sub_epi16(flt0_16b, u0);
        const __m256i flt1_0_sub_u = _mm256_sub_epi16(flt1_16b, u0);
        const __m256i v0 = _mm256_madd_epi16(
            xq_coeff, _mm256_unpacklo_epi16(flt0_0_sub_u, flt1_0_sub_u));
        const __m256i v1 = _mm256_madd_epi16(
            xq_coeff, _mm256_unpackhi_epi16(flt0_0_sub_u, flt1_0_sub_u));
        const __m256i vr0 =
            _mm256_srai_epi32(_mm256_add_epi32(v0, rounding), shift);
        const __m256i vr1 =
            _mm256_srai_epi32(_mm256_add_epi32(v1, rounding), shift);
        const __m256i e0 = _mm256_sub_epi16(
            _mm256_add_epi16(_mm256_packs_epi32(vr0, vr1), d0), s0);
        const __m256i err0 = _mm256_madd_epi16(e0, e0);
        sum32 = _mm256_add_epi32(sum32, err0);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = (int32_t)(dat[k] << SGRPROJ_RST_BITS);
        int32_t v = xq[0] * (flt0[k] - u) + xq[1] * (flt1[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
      const __m256i sum64_0 =
          _mm256_cvtepi32_epi64(_mm256_castsi256_si128(sum32));
      const __m256i sum64_1 =
          _mm256_cvtepi32_epi64(_mm256_extracti128_si256(sum32, 1));
      sum64 = _mm256_add_epi64(sum64, sum64_0);
      sum64 = _mm256_add_epi64(sum64, sum64_1);
    }
  } else if (params->r[0] > 0 || params->r[1] > 0) {
    const int xq_active = (params->r[0] > 0) ? xq[0] : xq[1];
    const __m256i xq_coeff =
        pair_set_epi16(xq_active, (-xq_active * (1 << SGRPROJ_RST_BITS)));
    const int32_t *flt = (params->r[0] > 0) ? flt0 : flt1;
    const int flt_stride = (params->r[0] > 0) ? flt0_stride : flt1_stride;
    for (i = 0; i < height; ++i) {
      __m256i sum32 = _mm256_setzero_si256();
      for (j = 0; j <= width - 16; j += 16) {
        const __m256i d0 = _mm256_cvtepu8_epi16(xx_loadu_128(dat + j));
        const __m256i s0 = _mm256_cvtepu8_epi16(xx_loadu_128(src + j));
        const __m256i flt_16b = _mm256_permute4x64_epi64(
            _mm256_packs_epi32(yy_loadu_256(flt + j),
                               yy_loadu_256(flt + j + 8)),
            0xd8);
        const __m256i v0 =
            _mm256_madd_epi16(xq_coeff, _mm256_unpacklo_epi16(flt_16b, d0));
        const __m256i v1 =
            _mm256_madd_epi16(xq_coeff, _mm256_unpackhi_epi16(flt_16b, d0));
        const __m256i vr0 =
            _mm256_srai_epi32(_mm256_add_epi32(v0, rounding), shift);
        const __m256i vr1 =
            _mm256_srai_epi32(_mm256_add_epi32(v1, rounding), shift);
        const __m256i e0 = _mm256_sub_epi16(
            _mm256_add_epi16(_mm256_packs_epi32(vr0, vr1), d0), s0);
        const __m256i err0 = _mm256_madd_epi16(e0, e0);
        sum32 = _mm256_add_epi32(sum32, err0);
      }
      for (k = j; k < width; ++k) {
        const int32_t u = (int32_t)(dat[k] << SGRPROJ_RST_BITS);
        int32_t v = xq_active * (flt[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
      flt += flt_stride;
      const __m256i sum64_0 =
          _mm256_cvtepi32_epi64(_mm256_castsi256_si128(sum32));
      const __m256i sum64_1 =
          _mm256_cvtepi32_epi64(_mm256_extracti128_si256(sum32, 1));
      sum64 = _mm256_add_epi64(sum64, sum64_0);
      sum64 = _mm256_add_epi64(sum64, sum64_1);
    }
  } else {
    __m256i sum32 = _mm256_setzero_si256();
    for (i = 0; i < height; ++i) {
      for (j = 0; j <= width - 16; j += 16) {
        const __m256i d0 = _mm256_cvtepu8_epi16(xx_loadu_128(dat + j));
        const __m256i s0 = _mm256_cvtepu8_epi16(xx_loadu_128(src + j));
        const __m256i diff0 = _mm256_sub_epi16(d0, s0);
        const __m256i err0 = _mm256_madd_epi16(diff0, diff0);
        sum32 = _mm256_add_epi32(sum32, err0);
      }
      for (k = j; k < width; ++k) {
        const int32_t e = (int32_t)(dat[k]) - src[k];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
    }
    const __m256i sum64_0 =
        _mm256_cvtepi32_epi64(_mm256_castsi256_si128(sum32));
    const __m256i sum64_1 =
        _mm256_cvtepi32_epi64(_mm256_extracti128_si256(sum32, 1));
    sum64 = _mm256_add_epi64(sum64_0, sum64_1);
  }
  int64_t sum[4];
  yy_storeu_256(sum, sum64);
  err += sum[0] + sum[1] + sum[2] + sum[3];
  return err;
}

// When params->r[0] > 0 and params->r[1] > 0. In this case all elements of
// C and H need to be computed.
static AOM_INLINE void calc_proj_params_r0_r1_avx2(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  __m256i h00, h01, h11, c0, c1;
  const __m256i zero = _mm256_setzero_si256();
  h01 = h11 = c0 = c1 = h00 = zero;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; j += 8) {
      const __m256i u_load = _mm256_cvtepu8_epi32(
          _mm_loadl_epi64((__m128i *)(dat + i * dat_stride + j)));
      const __m256i s_load = _mm256_cvtepu8_epi32(
          _mm_loadl_epi64((__m128i *)(src + i * src_stride + j)));
      __m256i f1 = _mm256_loadu_si256((__m256i *)(flt0 + i * flt0_stride + j));
      __m256i f2 = _mm256_loadu_si256((__m256i *)(flt1 + i * flt1_stride + j));
      __m256i d = _mm256_slli_epi32(u_load, SGRPROJ_RST_BITS);
      __m256i s = _mm256_slli_epi32(s_load, SGRPROJ_RST_BITS);
      s = _mm256_sub_epi32(s, d);
      f1 = _mm256_sub_epi32(f1, d);
      f2 = _mm256_sub_epi32(f2, d);

      const __m256i h00_even = _mm256_mul_epi32(f1, f1);
      const __m256i h00_odd = _mm256_mul_epi32(_mm256_srli_epi64(f1, 32),
                                               _mm256_srli_epi64(f1, 32));
      h00 = _mm256_add_epi64(h00, h00_even);
      h00 = _mm256_add_epi64(h00, h00_odd);

      const __m256i h01_even = _mm256_mul_epi32(f1, f2);
      const __m256i h01_odd = _mm256_mul_epi32(_mm256_srli_epi64(f1, 32),
                                               _mm256_srli_epi64(f2, 32));
      h01 = _mm256_add_epi64(h01, h01_even);
      h01 = _mm256_add_epi64(h01, h01_odd);

      const __m256i h11_even = _mm256_mul_epi32(f2, f2);
      const __m256i h11_odd = _mm256_mul_epi32(_mm256_srli_epi64(f2, 32),
                                               _mm256_srli_epi64(f2, 32));
      h11 = _mm256_add_epi64(h11, h11_even);
      h11 = _mm256_add_epi64(h11, h11_odd);

      const __m256i c0_even = _mm256_mul_epi32(f1, s);
      const __m256i c0_odd =
          _mm256_mul_epi32(_mm256_srli_epi64(f1, 32), _mm256_srli_epi64(s, 32));
      c0 = _mm256_add_epi64(c0, c0_even);
      c0 = _mm256_add_epi64(c0, c0_odd);

      const __m256i c1_even = _mm256_mul_epi32(f2, s);
      const __m256i c1_odd =
          _mm256_mul_epi32(_mm256_srli_epi64(f2, 32), _mm256_srli_epi64(s, 32));
      c1 = _mm256_add_epi64(c1, c1_even);
      c1 = _mm256_add_epi64(c1, c1_odd);
    }
  }

  __m256i c_low = _mm256_unpacklo_epi64(c0, c1);
  const __m256i c_high = _mm256_unpackhi_epi64(c0, c1);
  c_low = _mm256_add_epi64(c_low, c_high);
  const __m128i c_128bit = _mm_add_epi64(_mm256_extracti128_si256(c_low, 1),
                                         _mm256_castsi256_si128(c_low));

  __m256i h0x_low = _mm256_unpacklo_epi64(h00, h01);
  const __m256i h0x_high = _mm256_unpackhi_epi64(h00, h01);
  h0x_low = _mm256_add_epi64(h0x_low, h0x_high);
  const __m128i h0x_128bit = _mm_add_epi64(_mm256_extracti128_si256(h0x_low, 1),
                                           _mm256_castsi256_si128(h0x_low));

  // Using the symmetric properties of H,  calculations of H[1][0] are not
  // needed.
  __m256i h1x_low = _mm256_unpacklo_epi64(zero, h11);
  const __m256i h1x_high = _mm256_unpackhi_epi64(zero, h11);
  h1x_low = _mm256_add_epi64(h1x_low, h1x_high);
  const __m128i h1x_128bit = _mm_add_epi64(_mm256_extracti128_si256(h1x_low, 1),
                                           _mm256_castsi256_si128(h1x_low));

  xx_storeu_128(C, c_128bit);
  xx_storeu_128(H[0], h0x_128bit);
  xx_storeu_128(H[1], h1x_128bit);

  H[0][0] /= size;
  H[0][1] /= size;
  H[1][1] /= size;

  // Since H is a symmetric matrix
  H[1][0] = H[0][1];
  C[0] /= size;
  C[1] /= size;
}

// When only params->r[0] > 0. In this case only H[0][0] and C[0] are
// non-zero and need to be computed.
static AOM_INLINE void calc_proj_params_r0_avx2(const uint8_t *src8, int width,
                                                int height, int src_stride,
                                                const uint8_t *dat8,
                                                int dat_stride, int32_t *flt0,
                                                int flt0_stride,
                                                int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  __m256i h00, c0;
  const __m256i zero = _mm256_setzero_si256();
  c0 = h00 = zero;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; j += 8) {
      const __m256i u_load = _mm256_cvtepu8_epi32(
          _mm_loadl_epi64((__m128i *)(dat + i * dat_stride + j)));
      const __m256i s_load = _mm256_cvtepu8_epi32(
          _mm_loadl_epi64((__m128i *)(src + i * src_stride + j)));
      __m256i f1 = _mm256_loadu_si256((__m256i *)(flt0 + i * flt0_stride + j));
      __m256i d = _mm256_slli_epi32(u_load, SGRPROJ_RST_BITS);
      __m256i s = _mm256_slli_epi32(s_load, SGRPROJ_RST_BITS);
      s = _mm256_sub_epi32(s, d);
      f1 = _mm256_sub_epi32(f1, d);

      const __m256i h00_even = _mm256_mul_epi32(f1, f1);
      const __m256i h00_odd = _mm256_mul_epi32(_mm256_srli_epi64(f1, 32),
                                               _mm256_srli_epi64(f1, 32));
      h00 = _mm256_add_epi64(h00, h00_even);
      h00 = _mm256_add_epi64(h00, h00_odd);

      const __m256i c0_even = _mm256_mul_epi32(f1, s);
      const __m256i c0_odd =
          _mm256_mul_epi32(_mm256_srli_epi64(f1, 32), _mm256_srli_epi64(s, 32));
      c0 = _mm256_add_epi64(c0, c0_even);
      c0 = _mm256_add_epi64(c0, c0_odd);
    }
  }
  const __m128i h00_128bit = _mm_add_epi64(_mm256_extracti128_si256(h00, 1),
                                           _mm256_castsi256_si128(h00));
  const __m128i h00_val =
      _mm_add_epi64(h00_128bit, _mm_srli_si128(h00_128bit, 8));

  const __m128i c0_128bit = _mm_add_epi64(_mm256_extracti128_si256(c0, 1),
                                          _mm256_castsi256_si128(c0));
  const __m128i c0_val = _mm_add_epi64(c0_128bit, _mm_srli_si128(c0_128bit, 8));

  const __m128i c = _mm_unpacklo_epi64(c0_val, _mm256_castsi256_si128(zero));
  const __m128i h0x = _mm_unpacklo_epi64(h00_val, _mm256_castsi256_si128(zero));

  xx_storeu_128(C, c);
  xx_storeu_128(H[0], h0x);

  H[0][0] /= size;
  C[0] /= size;
}

// When only params->r[1] > 0. In this case only H[1][1] and C[1] are
// non-zero and need to be computed.
static AOM_INLINE void calc_proj_params_r1_avx2(const uint8_t *src8, int width,
                                                int height, int src_stride,
                                                const uint8_t *dat8,
                                                int dat_stride, int32_t *flt1,
                                                int flt1_stride,
                                                int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint8_t *src = src8;
  const uint8_t *dat = dat8;
  __m256i h11, c1;
  const __m256i zero = _mm256_setzero_si256();
  c1 = h11 = zero;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; j += 8) {
      const __m256i u_load = _mm256_cvtepu8_epi32(
          _mm_loadl_epi64((__m128i *)(dat + i * dat_stride + j)));
      const __m256i s_load = _mm256_cvtepu8_epi32(
          _mm_loadl_epi64((__m128i *)(src + i * src_stride + j)));
      __m256i f2 = _mm256_loadu_si256((__m256i *)(flt1 + i * flt1_stride + j));
      __m256i d = _mm256_slli_epi32(u_load, SGRPROJ_RST_BITS);
      __m256i s = _mm256_slli_epi32(s_load, SGRPROJ_RST_BITS);
      s = _mm256_sub_epi32(s, d);
      f2 = _mm256_sub_epi32(f2, d);

      const __m256i h11_even = _mm256_mul_epi32(f2, f2);
      const __m256i h11_odd = _mm256_mul_epi32(_mm256_srli_epi64(f2, 32),
                                               _mm256_srli_epi64(f2, 32));
      h11 = _mm256_add_epi64(h11, h11_even);
      h11 = _mm256_add_epi64(h11, h11_odd);

      const __m256i c1_even = _mm256_mul_epi32(f2, s);
      const __m256i c1_odd =
          _mm256_mul_epi32(_mm256_srli_epi64(f2, 32), _mm256_srli_epi64(s, 32));
      c1 = _mm256_add_epi64(c1, c1_even);
      c1 = _mm256_add_epi64(c1, c1_odd);
    }
  }

  const __m128i h11_128bit = _mm_add_epi64(_mm256_extracti128_si256(h11, 1),
                                           _mm256_castsi256_si128(h11));
  const __m128i h11_val =
      _mm_add_epi64(h11_128bit, _mm_srli_si128(h11_128bit, 8));

  const __m128i c1_128bit = _mm_add_epi64(_mm256_extracti128_si256(c1, 1),
                                          _mm256_castsi256_si128(c1));
  const __m128i c1_val = _mm_add_epi64(c1_128bit, _mm_srli_si128(c1_128bit, 8));

  const __m128i c = _mm_unpacklo_epi64(_mm256_castsi256_si128(zero), c1_val);
  const __m128i h1x = _mm_unpacklo_epi64(_mm256_castsi256_si128(zero), h11_val);

  xx_storeu_128(C, c);
  xx_storeu_128(H[1], h1x);

  H[1][1] /= size;
  C[1] /= size;
}

// AVX2 variant of av1_calc_proj_params_c.
void av1_calc_proj_params_avx2(const uint8_t *src8, int width, int height,
                               int src_stride, const uint8_t *dat8,
                               int dat_stride, int32_t *flt0, int flt0_stride,
                               int32_t *flt1, int flt1_stride, int64_t H[2][2],
                               int64_t C[2], const sgr_params_type *params) {
  if ((params->r[0] > 0) && (params->r[1] > 0)) {
    calc_proj_params_r0_r1_avx2(src8, width, height, src_stride, dat8,
                                dat_stride, flt0, flt0_stride, flt1,
                                flt1_stride, H, C);
  } else if (params->r[0] > 0) {
    calc_proj_params_r0_avx2(src8, width, height, src_stride, dat8, dat_stride,
                             flt0, flt0_stride, H, C);
  } else if (params->r[1] > 0) {
    calc_proj_params_r1_avx2(src8, width, height, src_stride, dat8, dat_stride,
                             flt1, flt1_stride, H, C);
  }
}

static AOM_INLINE void calc_proj_params_r0_r1_high_bd_avx2(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  __m256i h00, h01, h11, c0, c1;
  const __m256i zero = _mm256_setzero_si256();
  h01 = h11 = c0 = c1 = h00 = zero;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; j += 8) {
      const __m256i u_load = _mm256_cvtepu16_epi32(
          _mm_load_si128((__m128i *)(dat + i * dat_stride + j)));
      const __m256i s_load = _mm256_cvtepu16_epi32(
          _mm_load_si128((__m128i *)(src + i * src_stride + j)));
      __m256i f1 = _mm256_loadu_si256((__m256i *)(flt0 + i * flt0_stride + j));
      __m256i f2 = _mm256_loadu_si256((__m256i *)(flt1 + i * flt1_stride + j));
      __m256i d = _mm256_slli_epi32(u_load, SGRPROJ_RST_BITS);
      __m256i s = _mm256_slli_epi32(s_load, SGRPROJ_RST_BITS);
      s = _mm256_sub_epi32(s, d);
      f1 = _mm256_sub_epi32(f1, d);
      f2 = _mm256_sub_epi32(f2, d);

      const __m256i h00_even = _mm256_mul_epi32(f1, f1);
      const __m256i h00_odd = _mm256_mul_epi32(_mm256_srli_epi64(f1, 32),
                                               _mm256_srli_epi64(f1, 32));
      h00 = _mm256_add_epi64(h00, h00_even);
      h00 = _mm256_add_epi64(h00, h00_odd);

      const __m256i h01_even = _mm256_mul_epi32(f1, f2);
      const __m256i h01_odd = _mm256_mul_epi32(_mm256_srli_epi64(f1, 32),
                                               _mm256_srli_epi64(f2, 32));
      h01 = _mm256_add_epi64(h01, h01_even);
      h01 = _mm256_add_epi64(h01, h01_odd);

      const __m256i h11_even = _mm256_mul_epi32(f2, f2);
      const __m256i h11_odd = _mm256_mul_epi32(_mm256_srli_epi64(f2, 32),
                                               _mm256_srli_epi64(f2, 32));
      h11 = _mm256_add_epi64(h11, h11_even);
      h11 = _mm256_add_epi64(h11, h11_odd);

      const __m256i c0_even = _mm256_mul_epi32(f1, s);
      const __m256i c0_odd =
          _mm256_mul_epi32(_mm256_srli_epi64(f1, 32), _mm256_srli_epi64(s, 32));
      c0 = _mm256_add_epi64(c0, c0_even);
      c0 = _mm256_add_epi64(c0, c0_odd);

      const __m256i c1_even = _mm256_mul_epi32(f2, s);
      const __m256i c1_odd =
          _mm256_mul_epi32(_mm256_srli_epi64(f2, 32), _mm256_srli_epi64(s, 32));
      c1 = _mm256_add_epi64(c1, c1_even);
      c1 = _mm256_add_epi64(c1, c1_odd);
    }
  }

  __m256i c_low = _mm256_unpacklo_epi64(c0, c1);
  const __m256i c_high = _mm256_unpackhi_epi64(c0, c1);
  c_low = _mm256_add_epi64(c_low, c_high);
  const __m128i c_128bit = _mm_add_epi64(_mm256_extracti128_si256(c_low, 1),
                                         _mm256_castsi256_si128(c_low));

  __m256i h0x_low = _mm256_unpacklo_epi64(h00, h01);
  const __m256i h0x_high = _mm256_unpackhi_epi64(h00, h01);
  h0x_low = _mm256_add_epi64(h0x_low, h0x_high);
  const __m128i h0x_128bit = _mm_add_epi64(_mm256_extracti128_si256(h0x_low, 1),
                                           _mm256_castsi256_si128(h0x_low));

  // Using the symmetric properties of H,  calculations of H[1][0] are not
  // needed.
  __m256i h1x_low = _mm256_unpacklo_epi64(zero, h11);
  const __m256i h1x_high = _mm256_unpackhi_epi64(zero, h11);
  h1x_low = _mm256_add_epi64(h1x_low, h1x_high);
  const __m128i h1x_128bit = _mm_add_epi64(_mm256_extracti128_si256(h1x_low, 1),
                                           _mm256_castsi256_si128(h1x_low));

  xx_storeu_128(C, c_128bit);
  xx_storeu_128(H[0], h0x_128bit);
  xx_storeu_128(H[1], h1x_128bit);

  H[0][0] /= size;
  H[0][1] /= size;
  H[1][1] /= size;

  // Since H is a symmetric matrix
  H[1][0] = H[0][1];
  C[0] /= size;
  C[1] /= size;
}

static AOM_INLINE void calc_proj_params_r0_high_bd_avx2(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  __m256i h00, c0;
  const __m256i zero = _mm256_setzero_si256();
  c0 = h00 = zero;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; j += 8) {
      const __m256i u_load = _mm256_cvtepu16_epi32(
          _mm_load_si128((__m128i *)(dat + i * dat_stride + j)));
      const __m256i s_load = _mm256_cvtepu16_epi32(
          _mm_load_si128((__m128i *)(src + i * src_stride + j)));
      __m256i f1 = _mm256_loadu_si256((__m256i *)(flt0 + i * flt0_stride + j));
      __m256i d = _mm256_slli_epi32(u_load, SGRPROJ_RST_BITS);
      __m256i s = _mm256_slli_epi32(s_load, SGRPROJ_RST_BITS);
      s = _mm256_sub_epi32(s, d);
      f1 = _mm256_sub_epi32(f1, d);

      const __m256i h00_even = _mm256_mul_epi32(f1, f1);
      const __m256i h00_odd = _mm256_mul_epi32(_mm256_srli_epi64(f1, 32),
                                               _mm256_srli_epi64(f1, 32));
      h00 = _mm256_add_epi64(h00, h00_even);
      h00 = _mm256_add_epi64(h00, h00_odd);

      const __m256i c0_even = _mm256_mul_epi32(f1, s);
      const __m256i c0_odd =
          _mm256_mul_epi32(_mm256_srli_epi64(f1, 32), _mm256_srli_epi64(s, 32));
      c0 = _mm256_add_epi64(c0, c0_even);
      c0 = _mm256_add_epi64(c0, c0_odd);
    }
  }
  const __m128i h00_128bit = _mm_add_epi64(_mm256_extracti128_si256(h00, 1),
                                           _mm256_castsi256_si128(h00));
  const __m128i h00_val =
      _mm_add_epi64(h00_128bit, _mm_srli_si128(h00_128bit, 8));

  const __m128i c0_128bit = _mm_add_epi64(_mm256_extracti128_si256(c0, 1),
                                          _mm256_castsi256_si128(c0));
  const __m128i c0_val = _mm_add_epi64(c0_128bit, _mm_srli_si128(c0_128bit, 8));

  const __m128i c = _mm_unpacklo_epi64(c0_val, _mm256_castsi256_si128(zero));
  const __m128i h0x = _mm_unpacklo_epi64(h00_val, _mm256_castsi256_si128(zero));

  xx_storeu_128(C, c);
  xx_storeu_128(H[0], h0x);

  H[0][0] /= size;
  C[0] /= size;
}

static AOM_INLINE void calc_proj_params_r1_high_bd_avx2(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt1, int flt1_stride,
    int64_t H[2][2], int64_t C[2]) {
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  __m256i h11, c1;
  const __m256i zero = _mm256_setzero_si256();
  c1 = h11 = zero;

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; j += 8) {
      const __m256i u_load = _mm256_cvtepu16_epi32(
          _mm_load_si128((__m128i *)(dat + i * dat_stride + j)));
      const __m256i s_load = _mm256_cvtepu16_epi32(
          _mm_load_si128((__m128i *)(src + i * src_stride + j)));
      __m256i f2 = _mm256_loadu_si256((__m256i *)(flt1 + i * flt1_stride + j));
      __m256i d = _mm256_slli_epi32(u_load, SGRPROJ_RST_BITS);
      __m256i s = _mm256_slli_epi32(s_load, SGRPROJ_RST_BITS);
      s = _mm256_sub_epi32(s, d);
      f2 = _mm256_sub_epi32(f2, d);

      const __m256i h11_even = _mm256_mul_epi32(f2, f2);
      const __m256i h11_odd = _mm256_mul_epi32(_mm256_srli_epi64(f2, 32),
                                               _mm256_srli_epi64(f2, 32));
      h11 = _mm256_add_epi64(h11, h11_even);
      h11 = _mm256_add_epi64(h11, h11_odd);

      const __m256i c1_even = _mm256_mul_epi32(f2, s);
      const __m256i c1_odd =
          _mm256_mul_epi32(_mm256_srli_epi64(f2, 32), _mm256_srli_epi64(s, 32));
      c1 = _mm256_add_epi64(c1, c1_even);
      c1 = _mm256_add_epi64(c1, c1_odd);
    }
  }

  const __m128i h11_128bit = _mm_add_epi64(_mm256_extracti128_si256(h11, 1),
                                           _mm256_castsi256_si128(h11));
  const __m128i h11_val =
      _mm_add_epi64(h11_128bit, _mm_srli_si128(h11_128bit, 8));

  const __m128i c1_128bit = _mm_add_epi64(_mm256_extracti128_si256(c1, 1),
                                          _mm256_castsi256_si128(c1));
  const __m128i c1_val = _mm_add_epi64(c1_128bit, _mm_srli_si128(c1_128bit, 8));

  const __m128i c = _mm_unpacklo_epi64(_mm256_castsi256_si128(zero), c1_val);
  const __m128i h1x = _mm_unpacklo_epi64(_mm256_castsi256_si128(zero), h11_val);

  xx_storeu_128(C, c);
  xx_storeu_128(H[1], h1x);

  H[1][1] /= size;
  C[1] /= size;
}

// AVX2 variant of av1_calc_proj_params_high_bd_c.
void av1_calc_proj_params_high_bd_avx2(const uint8_t *src8, int width,
                                       int height, int src_stride,
                                       const uint8_t *dat8, int dat_stride,
                                       int32_t *flt0, int flt0_stride,
                                       int32_t *flt1, int flt1_stride,
                                       int64_t H[2][2], int64_t C[2],
                                       const sgr_params_type *params) {
  if ((params->r[0] > 0) && (params->r[1] > 0)) {
    calc_proj_params_r0_r1_high_bd_avx2(src8, width, height, src_stride, dat8,
                                        dat_stride, flt0, flt0_stride, flt1,
                                        flt1_stride, H, C);
  } else if (params->r[0] > 0) {
    calc_proj_params_r0_high_bd_avx2(src8, width, height, src_stride, dat8,
                                     dat_stride, flt0, flt0_stride, H, C);
  } else if (params->r[1] > 0) {
    calc_proj_params_r1_high_bd_avx2(src8, width, height, src_stride, dat8,
                                     dat_stride, flt1, flt1_stride, H, C);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
int64_t av1_highbd_pixel_proj_error_avx2(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params) {
  int i, j, k;
  const int32_t shift = SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS;
  const __m256i rounding = _mm256_set1_epi32(1 << (shift - 1));
  __m256i sum64 = _mm256_setzero_si256();
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  int64_t err = 0;
  if (params->r[0] > 0 && params->r[1] > 0) {  // Both filters are enabled
    const __m256i xq0 = _mm256_set1_epi32(xq[0]);
    const __m256i xq1 = _mm256_set1_epi32(xq[1]);
    for (i = 0; i < height; ++i) {
      __m256i sum32 = _mm256_setzero_si256();
      for (j = 0; j <= width - 16; j += 16) {  // Process 16 pixels at a time
        // Load 16 pixels each from source image and corrupted image
        const __m256i s0 = yy_loadu_256(src + j);
        const __m256i d0 = yy_loadu_256(dat + j);
        // s0 = [15 14 13 12 11 10 9 8] [7 6 5 4 3 2 1 0] as u16 (indices)

        // Shift-up each pixel to match filtered image scaling
        const __m256i u0 = _mm256_slli_epi16(d0, SGRPROJ_RST_BITS);

        // Split u0 into two halves and pad each from u16 to i32
        const __m256i u0l = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(u0));
        const __m256i u0h =
            _mm256_cvtepu16_epi32(_mm256_extracti128_si256(u0, 1));
        // u0h, u0l = [15 14 13 12] [11 10 9 8], [7 6 5 4] [3 2 1 0] as u32

        // Load 16 pixels from each filtered image
        const __m256i flt0l = yy_loadu_256(flt0 + j);
        const __m256i flt0h = yy_loadu_256(flt0 + j + 8);
        const __m256i flt1l = yy_loadu_256(flt1 + j);
        const __m256i flt1h = yy_loadu_256(flt1 + j + 8);
        // flt?l, flt?h = [15 14 13 12] [11 10 9 8], [7 6 5 4] [3 2 1 0] as u32

        // Subtract shifted corrupt image from each filtered image
        const __m256i flt0l_subu = _mm256_sub_epi32(flt0l, u0l);
        const __m256i flt0h_subu = _mm256_sub_epi32(flt0h, u0h);
        const __m256i flt1l_subu = _mm256_sub_epi32(flt1l, u0l);
        const __m256i flt1h_subu = _mm256_sub_epi32(flt1h, u0h);

        // Multiply basis vectors by appropriate coefficients
        const __m256i v0l = _mm256_mullo_epi32(flt0l_subu, xq0);
        const __m256i v0h = _mm256_mullo_epi32(flt0h_subu, xq0);
        const __m256i v1l = _mm256_mullo_epi32(flt1l_subu, xq1);
        const __m256i v1h = _mm256_mullo_epi32(flt1h_subu, xq1);

        // Add together the contributions from the two basis vectors
        const __m256i vl = _mm256_add_epi32(v0l, v1l);
        const __m256i vh = _mm256_add_epi32(v0h, v1h);

        // Right-shift v with appropriate rounding
        const __m256i vrl =
            _mm256_srai_epi32(_mm256_add_epi32(vl, rounding), shift);
        const __m256i vrh =
            _mm256_srai_epi32(_mm256_add_epi32(vh, rounding), shift);
        // vrh, vrl = [15 14 13 12] [11 10 9 8], [7 6 5 4] [3 2 1 0]

        // Saturate each i32 to an i16 then combine both halves
        // The permute (control=[3 1 2 0]) fixes weird ordering from AVX lanes
        const __m256i vr =
            _mm256_permute4x64_epi64(_mm256_packs_epi32(vrl, vrh), 0xd8);
        // intermediate = [15 14 13 12 7 6 5 4] [11 10 9 8 3 2 1 0]
        // vr = [15 14 13 12 11 10 9 8] [7 6 5 4 3 2 1 0]

        // Add twin-subspace-sgr-filter to corrupt image then subtract source
        const __m256i e0 = _mm256_sub_epi16(_mm256_add_epi16(vr, d0), s0);

        // Calculate squared error and add adjacent values
        const __m256i err0 = _mm256_madd_epi16(e0, e0);

        sum32 = _mm256_add_epi32(sum32, err0);
      }

      const __m256i sum32l =
          _mm256_cvtepu32_epi64(_mm256_castsi256_si128(sum32));
      sum64 = _mm256_add_epi64(sum64, sum32l);
      const __m256i sum32h =
          _mm256_cvtepu32_epi64(_mm256_extracti128_si256(sum32, 1));
      sum64 = _mm256_add_epi64(sum64, sum32h);

      // Process remaining pixels in this row (modulo 16)
      for (k = j; k < width; ++k) {
        const int32_t u = (int32_t)(dat[k] << SGRPROJ_RST_BITS);
        int32_t v = xq[0] * (flt0[k] - u) + xq[1] * (flt1[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
    }
  } else if (params->r[0] > 0 || params->r[1] > 0) {  // Only one filter enabled
    const int32_t xq_on = (params->r[0] > 0) ? xq[0] : xq[1];
    const __m256i xq_active = _mm256_set1_epi32(xq_on);
    const __m256i xq_inactive =
        _mm256_set1_epi32(-xq_on * (1 << SGRPROJ_RST_BITS));
    const int32_t *flt = (params->r[0] > 0) ? flt0 : flt1;
    const int flt_stride = (params->r[0] > 0) ? flt0_stride : flt1_stride;
    for (i = 0; i < height; ++i) {
      __m256i sum32 = _mm256_setzero_si256();
      for (j = 0; j <= width - 16; j += 16) {
        // Load 16 pixels from source image
        const __m256i s0 = yy_loadu_256(src + j);
        // s0 = [15 14 13 12 11 10 9 8] [7 6 5 4 3 2 1 0] as u16

        // Load 16 pixels from corrupted image and pad each u16 to i32
        const __m256i d0 = yy_loadu_256(dat + j);
        const __m256i d0h =
            _mm256_cvtepu16_epi32(_mm256_extracti128_si256(d0, 1));
        const __m256i d0l = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(d0));
        // d0 = [15 14 13 12 11 10 9 8] [7 6 5 4 3 2 1 0] as u16
        // d0h, d0l = [15 14 13 12] [11 10 9 8], [7 6 5 4] [3 2 1 0] as i32

        // Load 16 pixels from the filtered image
        const __m256i flth = yy_loadu_256(flt + j + 8);
        const __m256i fltl = yy_loadu_256(flt + j);
        // flth, fltl = [15 14 13 12] [11 10 9 8], [7 6 5 4] [3 2 1 0] as i32

        const __m256i flth_xq = _mm256_mullo_epi32(flth, xq_active);
        const __m256i fltl_xq = _mm256_mullo_epi32(fltl, xq_active);
        const __m256i d0h_xq = _mm256_mullo_epi32(d0h, xq_inactive);
        const __m256i d0l_xq = _mm256_mullo_epi32(d0l, xq_inactive);

        const __m256i vh = _mm256_add_epi32(flth_xq, d0h_xq);
        const __m256i vl = _mm256_add_epi32(fltl_xq, d0l_xq);

        // Shift this down with appropriate rounding
        const __m256i vrh =
            _mm256_srai_epi32(_mm256_add_epi32(vh, rounding), shift);
        const __m256i vrl =
            _mm256_srai_epi32(_mm256_add_epi32(vl, rounding), shift);
        // vrh, vrl = [15 14 13 12] [11 10 9 8], [7 6 5 4] [3 2 1 0] as i32

        // Saturate each i32 to an i16 then combine both halves
        // The permute (control=[3 1 2 0]) fixes weird ordering from AVX lanes
        const __m256i vr =
            _mm256_permute4x64_epi64(_mm256_packs_epi32(vrl, vrh), 0xd8);
        // intermediate = [15 14 13 12 7 6 5 4] [11 10 9 8 3 2 1 0] as u16
        // vr = [15 14 13 12 11 10 9 8] [7 6 5 4 3 2 1 0] as u16

        // Subtract twin-subspace-sgr filtered from source image to get error
        const __m256i e0 = _mm256_sub_epi16(_mm256_add_epi16(vr, d0), s0);

        // Calculate squared error and add adjacent values
        const __m256i err0 = _mm256_madd_epi16(e0, e0);

        sum32 = _mm256_add_epi32(sum32, err0);
      }

      const __m256i sum32l =
          _mm256_cvtepu32_epi64(_mm256_castsi256_si128(sum32));
      sum64 = _mm256_add_epi64(sum64, sum32l);
      const __m256i sum32h =
          _mm256_cvtepu32_epi64(_mm256_extracti128_si256(sum32, 1));
      sum64 = _mm256_add_epi64(sum64, sum32h);

      // Process remaining pixels in this row (modulo 16)
      for (k = j; k < width; ++k) {
        const int32_t u = (int32_t)(dat[k] << SGRPROJ_RST_BITS);
        int32_t v = xq_on * (flt[k] - u);
        const int32_t e = ROUND_POWER_OF_TWO(v, shift) + dat[k] - src[k];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
      flt += flt_stride;
    }
  } else {  // Neither filter is enabled
    for (i = 0; i < height; ++i) {
      __m256i sum32 = _mm256_setzero_si256();
      for (j = 0; j <= width - 32; j += 32) {
        // Load 2x16 u16 from source image
        const __m256i s0l = yy_loadu_256(src + j);
        const __m256i s0h = yy_loadu_256(src + j + 16);

        // Load 2x16 u16 from corrupted image
        const __m256i d0l = yy_loadu_256(dat + j);
        const __m256i d0h = yy_loadu_256(dat + j + 16);

        // Subtract corrupted image from source image
        const __m256i diffl = _mm256_sub_epi16(d0l, s0l);
        const __m256i diffh = _mm256_sub_epi16(d0h, s0h);

        // Square error and add adjacent values
        const __m256i err0l = _mm256_madd_epi16(diffl, diffl);
        const __m256i err0h = _mm256_madd_epi16(diffh, diffh);

        sum32 = _mm256_add_epi32(sum32, err0l);
        sum32 = _mm256_add_epi32(sum32, err0h);
      }

      const __m256i sum32l =
          _mm256_cvtepu32_epi64(_mm256_castsi256_si128(sum32));
      sum64 = _mm256_add_epi64(sum64, sum32l);
      const __m256i sum32h =
          _mm256_cvtepu32_epi64(_mm256_extracti128_si256(sum32, 1));
      sum64 = _mm256_add_epi64(sum64, sum32h);

      // Process remaining pixels (modulu 16)
      for (k = j; k < width; ++k) {
        const int32_t e = (int32_t)(dat[k]) - src[k];
        err += ((int64_t)e * e);
      }
      dat += dat_stride;
      src += src_stride;
    }
  }

  // Sum 4 values from sum64l and sum64h into err
  int64_t sum[4];
  yy_storeu_256(sum, sum64);
  err += sum[0] + sum[1] + sum[2] + sum[3];
  return err;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
