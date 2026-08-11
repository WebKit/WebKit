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

#include <immintrin.h>  // AVX2
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"
#include "aom_dsp/x86/transpose_sse2.h"

#include "config/av1_rtcd.h"
#include "av1/common/restoration.h"
#include "av1/encoder/pickrst.h"

#if CONFIG_AV1_HIGHBITDEPTH
static inline void acc_stat_highbd_avx2(int64_t *dst, const uint16_t *dgd,
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

static inline void acc_stat_highbd_win7_one_line_avx2(
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
        const __m256i dgd_ijkl = _mm256_set1_epi32(loadu_int32(dgd_ijk + l));
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
        const __m256i dgd_ijkl = _mm256_set1_epi32((int)D1);

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

static inline void compute_stats_highbd_win7_opt_avx2(
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

static inline void acc_stat_highbd_win5_one_line_avx2(
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
        const __m256i dgd_ijkl = _mm256_set1_epi32(loadu_int32(dgd_ijk + l));
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
        const __m256i dgd_ijkl = _mm256_set1_epi32((int)D1);

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

static inline void compute_stats_highbd_win5_opt_avx2(
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
                                   const uint8_t *src8, int16_t *dgd_avg,
                                   int16_t *src_avg, int h_start, int h_end,
                                   int v_start, int v_end, int dgd_stride,
                                   int src_stride, int64_t *M, int64_t *H,
                                   aom_bit_depth_t bit_depth) {
  if (wiener_win == WIENER_WIN) {
    (void)dgd_avg;
    (void)src_avg;
    compute_stats_highbd_win7_opt_avx2(dgd8, src8, h_start, h_end, v_start,
                                       v_end, dgd_stride, src_stride, M, H,
                                       bit_depth);
  } else if (wiener_win == WIENER_WIN_CHROMA) {
    (void)dgd_avg;
    (void)src_avg;
    compute_stats_highbd_win5_opt_avx2(dgd8, src8, h_start, h_end, v_start,
                                       v_end, dgd_stride, src_stride, M, H,
                                       bit_depth);
  } else {
    av1_compute_stats_highbd_c(wiener_win, dgd8, src8, dgd_avg, src_avg,
                               h_start, h_end, v_start, v_end, dgd_stride,
                               src_stride, M, H, bit_depth);
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static inline void madd_and_accum_avx2(__m256i src, __m256i dgd, __m256i *sum) {
  *sum = _mm256_add_epi32(*sum, _mm256_madd_epi16(src, dgd));
}

static inline __m256i convert_and_add_avx2(__m256i src) {
  const __m256i s0 = _mm256_cvtepi32_epi64(_mm256_castsi256_si128(src));
  const __m256i s1 = _mm256_cvtepi32_epi64(_mm256_extracti128_si256(src, 1));
  return _mm256_add_epi64(s0, s1);
}

static inline __m256i hadd_four_32_to_64_avx2(__m256i src0, __m256i src1,
                                              __m256i *src2, __m256i *src3) {
  // 00 01 10 11 02 03 12 13
  const __m256i s_0 = _mm256_hadd_epi32(src0, src1);
  // 20 21 30 31 22 23 32 33
  const __m256i s_1 = _mm256_hadd_epi32(*src2, *src3);
  // 00+01 10+11 20+21 30+31 02+03 12+13 22+23 32+33
  const __m256i s_2 = _mm256_hadd_epi32(s_0, s_1);
  return convert_and_add_avx2(s_2);
}

static inline __m128i add_64bit_lvl_avx2(__m256i src0, __m256i src1) {
  // 00 10 02 12
  const __m256i t0 = _mm256_unpacklo_epi64(src0, src1);
  // 01 11 03 13
  const __m256i t1 = _mm256_unpackhi_epi64(src0, src1);
  // 00+01 10+11 02+03 12+13
  const __m256i sum = _mm256_add_epi64(t0, t1);
  // 00+01 10+11
  const __m128i sum0 = _mm256_castsi256_si128(sum);
  // 02+03 12+13
  const __m128i sum1 = _mm256_extracti128_si256(sum, 1);
  // 00+01+02+03 10+11+12+13
  return _mm_add_epi64(sum0, sum1);
}

static inline __m128i convert_32_to_64_add_avx2(__m256i src0, __m256i src1) {
  // 00 01 02 03
  const __m256i s0 = convert_and_add_avx2(src0);
  // 10 11 12 13
  const __m256i s1 = convert_and_add_avx2(src1);
  return add_64bit_lvl_avx2(s0, s1);
}

static inline int32_t calc_sum_of_register(__m256i src) {
  const __m128i src_l = _mm256_castsi256_si128(src);
  const __m128i src_h = _mm256_extracti128_si256(src, 1);
  const __m128i sum = _mm_add_epi32(src_l, src_h);
  const __m128i dst0 = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
  const __m128i dst1 = _mm_add_epi32(dst0, _mm_srli_si128(dst0, 4));
  return _mm_cvtsi128_si32(dst1);
}

static inline void transpose_64bit_4x4_avx2(const __m256i *const src,
                                            __m256i *const dst) {
  // Unpack 64 bit elements. Goes from:
  // src[0]: 00 01 02 03
  // src[1]: 10 11 12 13
  // src[2]: 20 21 22 23
  // src[3]: 30 31 32 33
  // to:
  // reg0:    00 10 02 12
  // reg1:    20 30 22 32
  // reg2:    01 11 03 13
  // reg3:    21 31 23 33
  const __m256i reg0 = _mm256_unpacklo_epi64(src[0], src[1]);
  const __m256i reg1 = _mm256_unpacklo_epi64(src[2], src[3]);
  const __m256i reg2 = _mm256_unpackhi_epi64(src[0], src[1]);
  const __m256i reg3 = _mm256_unpackhi_epi64(src[2], src[3]);

  // Unpack 64 bit elements resulting in:
  // dst[0]: 00 10 20 30
  // dst[1]: 01 11 21 31
  // dst[2]: 02 12 22 32
  // dst[3]: 03 13 23 33
  dst[0] = _mm256_inserti128_si256(reg0, _mm256_castsi256_si128(reg1), 1);
  dst[1] = _mm256_inserti128_si256(reg2, _mm256_castsi256_si128(reg3), 1);
  dst[2] = _mm256_inserti128_si256(reg1, _mm256_extracti128_si256(reg0, 1), 0);
  dst[3] = _mm256_inserti128_si256(reg3, _mm256_extracti128_si256(reg2, 1), 0);
}

// When we load 32 values of int8_t type and need less than 32 values for
// processing, the below mask is used to make the extra values zero.
static const int8_t mask_8bit[32] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 16 bytes
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 16 bytes
};

// When we load 16 values of int16_t type and need less than 16 values for
// processing, the below mask is used to make the extra values zero.
static const int16_t mask_16bit[32] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 16 bytes
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,   // 16 bytes
};

static inline uint8_t calc_dgd_buf_avg_avx2(const uint8_t *src, int32_t h_start,
                                            int32_t h_end, int32_t v_start,
                                            int32_t v_end, int32_t stride) {
  const uint8_t *src_temp = src + v_start * stride + h_start;
  const __m256i zero = _mm256_setzero_si256();
  const int32_t width = h_end - h_start;
  const int32_t height = v_end - v_start;
  const int32_t wd_beyond_mul32 = width & 31;
  const int32_t wd_mul32 = width - wd_beyond_mul32;
  __m128i mask_low, mask_high;
  __m256i ss = zero;

  // When width is not multiple of 32, it still loads 32 and to make the data
  // which is extra (beyond required) as zero using the below mask.
  if (wd_beyond_mul32 >= 16) {
    mask_low = _mm_set1_epi8(-1);
    mask_high = _mm_loadu_si128((__m128i *)(&mask_8bit[32 - wd_beyond_mul32]));
  } else {
    mask_low = _mm_loadu_si128((__m128i *)(&mask_8bit[16 - wd_beyond_mul32]));
    mask_high = _mm_setzero_si128();
  }
  const __m256i mask =
      _mm256_inserti128_si256(_mm256_castsi128_si256(mask_low), mask_high, 1);

  int32_t proc_ht = 0;
  do {
    // Process width in multiple of 32.
    int32_t proc_wd = 0;
    while (proc_wd < wd_mul32) {
      const __m256i s_0 = _mm256_loadu_si256((__m256i *)(src_temp + proc_wd));
      const __m256i sad_0 = _mm256_sad_epu8(s_0, zero);
      ss = _mm256_add_epi32(ss, sad_0);
      proc_wd += 32;
    }

    // Process the remaining width.
    if (wd_beyond_mul32) {
      const __m256i s_0 = _mm256_loadu_si256((__m256i *)(src_temp + proc_wd));
      const __m256i s_m_0 = _mm256_and_si256(s_0, mask);
      const __m256i sad_0 = _mm256_sad_epu8(s_m_0, zero);
      ss = _mm256_add_epi32(ss, sad_0);
    }
    src_temp += stride;
    proc_ht++;
  } while (proc_ht < height);

  const uint32_t sum = calc_sum_of_register(ss);
  const uint8_t avg = sum / (width * height);
  return avg;
}

// Fill (src-avg) or (dgd-avg) buffers. Note that when n = (width % 16) is not
// 0, it writes (16 - n) more data than required.
static inline void sub_avg_block_avx2(const uint8_t *src, int32_t src_stride,
                                      uint8_t avg, int32_t width,
                                      int32_t height, int16_t *dst,
                                      int32_t dst_stride,
                                      int use_downsampled_wiener_stats) {
  const __m256i avg_reg = _mm256_set1_epi16(avg);

  int32_t proc_ht = 0;
  do {
    int ds_factor =
        use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;
    if (use_downsampled_wiener_stats &&
        (height - proc_ht < WIENER_STATS_DOWNSAMPLE_FACTOR)) {
      ds_factor = height - proc_ht;
    }

    int32_t proc_wd = 0;
    while (proc_wd < width) {
      const __m128i s = _mm_loadu_si128((__m128i *)(src + proc_wd));
      const __m256i ss = _mm256_cvtepu8_epi16(s);
      const __m256i d = _mm256_sub_epi16(ss, avg_reg);
      _mm256_storeu_si256((__m256i *)(dst + proc_wd), d);
      proc_wd += 16;
    }

    src += ds_factor * src_stride;
    dst += ds_factor * dst_stride;
    proc_ht += ds_factor;
  } while (proc_ht < height);
}

// Fills lower-triangular elements of H buffer from upper triangular elements of
// the same
static inline void fill_lower_triag_elements_avx2(const int32_t wiener_win2,
                                                  int64_t *const H) {
  for (int32_t i = 0; i < wiener_win2 - 1; i += 4) {
    __m256i in[4], out[4];

    in[0] = _mm256_loadu_si256((__m256i *)(H + (i + 0) * wiener_win2 + i + 1));
    in[1] = _mm256_loadu_si256((__m256i *)(H + (i + 1) * wiener_win2 + i + 1));
    in[2] = _mm256_loadu_si256((__m256i *)(H + (i + 2) * wiener_win2 + i + 1));
    in[3] = _mm256_loadu_si256((__m256i *)(H + (i + 3) * wiener_win2 + i + 1));

    transpose_64bit_4x4_avx2(in, out);

    _mm_storel_epi64((__m128i *)(H + (i + 1) * wiener_win2 + i),
                     _mm256_castsi256_si128(out[0]));
    _mm_storeu_si128((__m128i *)(H + (i + 2) * wiener_win2 + i),
                     _mm256_castsi256_si128(out[1]));
    _mm256_storeu_si256((__m256i *)(H + (i + 3) * wiener_win2 + i), out[2]);
    _mm256_storeu_si256((__m256i *)(H + (i + 4) * wiener_win2 + i), out[3]);

    for (int32_t j = i + 5; j < wiener_win2; j += 4) {
      in[0] = _mm256_loadu_si256((__m256i *)(H + (i + 0) * wiener_win2 + j));
      in[1] = _mm256_loadu_si256((__m256i *)(H + (i + 1) * wiener_win2 + j));
      in[2] = _mm256_loadu_si256((__m256i *)(H + (i + 2) * wiener_win2 + j));
      in[3] = _mm256_loadu_si256((__m256i *)(H + (i + 3) * wiener_win2 + j));

      transpose_64bit_4x4_avx2(in, out);

      _mm256_storeu_si256((__m256i *)(H + (j + 0) * wiener_win2 + i), out[0]);
      _mm256_storeu_si256((__m256i *)(H + (j + 1) * wiener_win2 + i), out[1]);
      _mm256_storeu_si256((__m256i *)(H + (j + 2) * wiener_win2 + i), out[2]);
      _mm256_storeu_si256((__m256i *)(H + (j + 3) * wiener_win2 + i), out[3]);
    }
  }
}

// Fill H buffer based on loop_count.
#define INIT_H_VALUES(d, loop_count)                           \
  for (int g = 0; g < (loop_count); g++) {                     \
    const __m256i dgd0 =                                       \
        _mm256_loadu_si256((__m256i *)((d) + (g * d_stride))); \
    madd_and_accum_avx2(dgd_mul_df, dgd0, &sum_h[g]);          \
  }

// Fill M & H buffer.
#define INIT_MH_VALUES(d)                                      \
  for (int g = 0; g < wiener_win; g++) {                       \
    const __m256i dgds_0 =                                     \
        _mm256_loadu_si256((__m256i *)((d) + (g * d_stride))); \
    madd_and_accum_avx2(src_mul_df, dgds_0, &sum_m[g]);        \
    madd_and_accum_avx2(dgd_mul_df, dgds_0, &sum_h[g]);        \
  }

// Update the dgd pointers appropriately.
#define INITIALIZATION(wiener_window_sz)                                 \
  j = i / (wiener_window_sz);                                            \
  const int16_t *d_window = d + j;                                       \
  const int16_t *d_current_row =                                         \
      d + j + ((i % (wiener_window_sz)) * d_stride);                     \
  int proc_ht = v_start;                                                 \
  downsample_factor =                                                    \
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1; \
  __m256i sum_h[wiener_window_sz];                                       \
  memset(sum_h, 0, sizeof(sum_h));

// Update the downsample factor appropriately.
#define UPDATE_DOWNSAMPLE_FACTOR                              \
  int proc_wd = 0;                                            \
  if (use_downsampled_wiener_stats &&                         \
      ((v_end - proc_ht) < WIENER_STATS_DOWNSAMPLE_FACTOR)) { \
    downsample_factor = v_end - proc_ht;                      \
  }                                                           \
  const __m256i df_reg = _mm256_set1_epi16(downsample_factor);

#define CALCULATE_REMAINING_H_WIN5                                             \
  while (j < wiener_win) {                                                     \
    d_window = d;                                                              \
    d_current_row = d + (i / wiener_win) + ((i % wiener_win) * d_stride);      \
    const __m256i zero = _mm256_setzero_si256();                               \
    sum_h[0] = zero;                                                           \
    sum_h[1] = zero;                                                           \
    sum_h[2] = zero;                                                           \
    sum_h[3] = zero;                                                           \
    sum_h[4] = zero;                                                           \
                                                                               \
    proc_ht = v_start;                                                         \
    downsample_factor =                                                        \
        use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;     \
    do {                                                                       \
      UPDATE_DOWNSAMPLE_FACTOR;                                                \
                                                                               \
      /* Process the amount of width multiple of 16.*/                         \
      while (proc_wd < wd_mul16) {                                             \
        const __m256i dgd =                                                    \
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));          \
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);            \
        INIT_H_VALUES(d_window + j + proc_wd, 5)                               \
                                                                               \
        proc_wd += 16;                                                         \
      };                                                                       \
                                                                               \
      /* Process the remaining width here. */                                  \
      if (wd_beyond_mul16) {                                                   \
        const __m256i dgd =                                                    \
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));          \
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);                  \
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);       \
        INIT_H_VALUES(d_window + j + proc_wd, 5)                               \
      }                                                                        \
      proc_ht += downsample_factor;                                            \
      d_window += downsample_factor * d_stride;                                \
      d_current_row += downsample_factor * d_stride;                           \
    } while (proc_ht < v_end);                                                 \
    const __m256i s_h0 =                                                       \
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);     \
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + (wiener_win * j)), \
                        s_h0);                                                 \
    const __m256i s_m_h = convert_and_add_avx2(sum_h[4]);                      \
    const __m128i s_m_h0 = add_64bit_lvl_avx2(s_m_h, s_m_h);                   \
    _mm_storel_epi64(                                                          \
        (__m128i *)(H + (i * wiener_win2) + (wiener_win * j) + 4), s_m_h0);    \
    j++;                                                                       \
  }

#define CALCULATE_REMAINING_H_WIN7                                             \
  while (j < wiener_win) {                                                     \
    d_window = d;                                                              \
    d_current_row = d + (i / wiener_win) + ((i % wiener_win) * d_stride);      \
    const __m256i zero = _mm256_setzero_si256();                               \
    sum_h[0] = zero;                                                           \
    sum_h[1] = zero;                                                           \
    sum_h[2] = zero;                                                           \
    sum_h[3] = zero;                                                           \
    sum_h[4] = zero;                                                           \
    sum_h[5] = zero;                                                           \
    sum_h[6] = zero;                                                           \
                                                                               \
    proc_ht = v_start;                                                         \
    downsample_factor =                                                        \
        use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;     \
    do {                                                                       \
      UPDATE_DOWNSAMPLE_FACTOR;                                                \
                                                                               \
      /* Process the amount of width multiple of 16.*/                         \
      while (proc_wd < wd_mul16) {                                             \
        const __m256i dgd =                                                    \
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));          \
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);            \
        INIT_H_VALUES(d_window + j + proc_wd, 7)                               \
                                                                               \
        proc_wd += 16;                                                         \
      };                                                                       \
                                                                               \
      /* Process the remaining width here. */                                  \
      if (wd_beyond_mul16) {                                                   \
        const __m256i dgd =                                                    \
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));          \
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);                  \
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);       \
        INIT_H_VALUES(d_window + j + proc_wd, 7)                               \
      }                                                                        \
      proc_ht += downsample_factor;                                            \
      d_window += downsample_factor * d_stride;                                \
      d_current_row += downsample_factor * d_stride;                           \
    } while (proc_ht < v_end);                                                 \
    const __m256i s_h1 =                                                       \
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);     \
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + (wiener_win * j)), \
                        s_h1);                                                 \
    const __m256i s_h2 =                                                       \
        hadd_four_32_to_64_avx2(sum_h[4], sum_h[5], &sum_h[6], &sum_h[6]);     \
    _mm256_storeu_si256(                                                       \
        (__m256i *)(H + (i * wiener_win2) + (wiener_win * j) + 4), s_h2);      \
    j++;                                                                       \
  }

// The buffers H(auto-covariance) and M(cross-correlation) are used to estimate
// the filter tap values required for wiener filtering. Here, the buffer H is of
// size ((wiener_window_size^2)*(wiener_window_size^2)) and M is of size
// (wiener_window_size*wiener_window_size). H is a symmetric matrix where the
// value above the diagonal (upper triangle) are equal to the values below the
// diagonal (lower triangle). The calculation of elements/stats of H(upper
// triangle) and M is done in steps as described below where each step fills
// specific values of H and M.
// Once the upper triangular elements of H matrix are derived, the same will be
// copied to lower triangular using the function
// fill_lower_triag_elements_avx2().
// Example: Wiener window size =
// WIENER_WIN_CHROMA (5) M buffer = [M0 M1 M2 ---- M23 M24] H buffer = Hxy
// (x-row, y-column) [H00 H01 H02 ---- H023 H024] [H10 H11 H12 ---- H123 H124]
// [H30 H31 H32 ---- H323 H324]
// [H40 H41 H42 ---- H423 H424]
// [H50 H51 H52 ---- H523 H524]
// [H60 H61 H62 ---- H623 H624]
//            ||
//            ||
// [H230 H231 H232 ---- H2323 H2324]
// [H240 H241 H242 ---- H2423 H2424]
// In Step 1, whole M buffers (i.e., M0 to M24) and the first row of H (i.e.,
// H00 to H024) is filled. The remaining rows of H buffer are filled through
// steps 2 to 6.
static void compute_stats_win5_avx2(const int16_t *const d, int32_t d_stride,
                                    const int16_t *const s, int32_t s_stride,
                                    int32_t width, int v_start, int v_end,
                                    int64_t *const M, int64_t *const H,
                                    int use_downsampled_wiener_stats) {
  const int32_t wiener_win = WIENER_WIN_CHROMA;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  // Amount of width which is beyond multiple of 16. This case is handled
  // appropriately to process only the required width towards the end.
  const int32_t wd_mul16 = width & ~15;
  const int32_t wd_beyond_mul16 = width - wd_mul16;
  const __m256i mask =
      _mm256_loadu_si256((__m256i *)(&mask_16bit[16 - wd_beyond_mul16]));
  int downsample_factor;

  // Step 1: Full M (i.e., M0 to M24) and first row H (i.e., H00 to H024)
  // values are filled here. Here, the loop over 'j' is executed for values 0
  // to 4 (wiener_win-1). When the loop executed for a specific 'j', 5 values of
  // M and H are filled as shown below.
  // j=0: M0-M4 and H00-H04, j=1: M5-M9 and H05-H09 are filled etc,.
  int j = 0;
  do {
    const int16_t *s_t = s;
    const int16_t *d_t = d;
    __m256i sum_m[WIENER_WIN_CHROMA] = { _mm256_setzero_si256() };
    __m256i sum_h[WIENER_WIN_CHROMA] = { _mm256_setzero_si256() };
    downsample_factor =
        use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;
    int proc_ht = v_start;
    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i src = _mm256_loadu_si256((__m256i *)(s_t + proc_wd));
        const __m256i dgd = _mm256_loadu_si256((__m256i *)(d_t + proc_wd));
        const __m256i src_mul_df = _mm256_mullo_epi16(src, df_reg);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_MH_VALUES(d_t + j + proc_wd)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i src = _mm256_loadu_si256((__m256i *)(s_t + proc_wd));
        const __m256i dgd = _mm256_loadu_si256((__m256i *)(d_t + proc_wd));
        const __m256i src_mask = _mm256_and_si256(src, mask);
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i src_mul_df = _mm256_mullo_epi16(src_mask, df_reg);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_MH_VALUES(d_t + j + proc_wd)
      }
      proc_ht += downsample_factor;
      s_t += downsample_factor * s_stride;
      d_t += downsample_factor * d_stride;
    } while (proc_ht < v_end);

    const __m256i s_m =
        hadd_four_32_to_64_avx2(sum_m[0], sum_m[1], &sum_m[2], &sum_m[3]);
    const __m128i s_m_h = convert_32_to_64_add_avx2(sum_m[4], sum_h[4]);
    _mm256_storeu_si256((__m256i *)(M + wiener_win * j), s_m);
    _mm_storel_epi64((__m128i *)&M[wiener_win * j + 4], s_m_h);

    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    _mm256_storeu_si256((__m256i *)(H + wiener_win * j), s_h);
    _mm_storeh_epi64((__m128i *)&H[wiener_win * j + 4], s_m_h);
  } while (++j < wiener_win);

  // The below steps are designed to fill remaining rows of H buffer. Here, aim
  // is to fill only upper triangle elements correspond to each row and lower
  // triangle elements are copied from upper-triangle elements. Also, as
  // mentioned in Step 1, the core function is designed to fill 5
  // elements/stats/values of H buffer.
  //
  // Step 2: Here, the rows 1, 6, 11, 16 and 21 are filled. As we need to fill
  // only upper-triangle elements, H10 from row1, H60-H64 and H65 from row6,etc,
  // are need not be filled. As the core function process 5 values, in first
  // iteration of 'j' only 4 values to be filled i.e., H11-H14 from row1,H66-H69
  // from row6, etc.
  for (int i = 1; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN_CHROMA)

    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (1 * d_stride), 4)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (1 * d_stride), 4)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + i), s_h);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN5
  }

  // Step 3: Here, the rows 2, 7, 12, 17 and 22 are filled. As we need to fill
  // only upper-triangle elements, H20-H21 from row2, H70-H74 and H75-H76 from
  // row7, etc, are need not be filled. As the core function process 5 values,
  // in first iteration of 'j' only 3 values to be filled i.e., H22-H24 from
  // row2, H77-H79 from row7, etc.
  for (int i = 2; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN_CHROMA)

    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (2 * d_stride), 3)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (2 * d_stride), 3)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + i), s_h);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN5
  }

  // Step 4: Here, the rows 3, 8, 13, 18 and 23 are filled. As we need to fill
  // only upper-triangle elements, H30-H32 from row3, H80-H84 and H85-H87 from
  // row8, etc, are need not be filled. As the core function process 5 values,
  // in first iteration of 'j' only 2 values to be filled i.e., H33-H34 from
  // row3, H88-89 from row8, etc.
  for (int i = 3; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN_CHROMA)

    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (3 * d_stride), 2)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (3 * d_stride), 2)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m128i s_h = convert_32_to_64_add_avx2(sum_h[0], sum_h[1]);
    _mm_storeu_si128((__m128i *)(H + (i * wiener_win2) + i), s_h);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN5
  }

  // Step 5: Here, the rows 4, 9, 14, 19 and 24 are filled. As we need to fill
  // only upper-triangle elements, H40-H43 from row4, H90-H94 and H95-H98 from
  // row9, etc, are need not be filled. As the core function process 5 values,
  // in first iteration of 'j' only 1 values to be filled i.e., H44 from row4,
  // H99 from row9, etc.
  for (int i = 4; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN_CHROMA)
    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (4 * d_stride), 1)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (4 * d_stride), 1)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m128i s_h = convert_32_to_64_add_avx2(sum_h[0], sum_h[1]);
    _mm_storeu_si128((__m128i *)(H + (i * wiener_win2) + i), s_h);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN5
  }

  // Step 6: Here, the rows 5, 10, 15 and 20 are filled. As we need to fill only
  // upper-triangle elements, H50-H54 from row5, H100-H104 and H105-H109 from
  // row10,etc, are need not be filled. The first iteration of 'j' fills H55-H59
  // from row5 and H1010-H1014 from row10, etc.
  for (int i = 5; i < wiener_win2; i += wiener_win) {
    // Derive j'th iteration from where the H buffer filling needs to be
    // started.
    j = i / wiener_win;
    int shift = 0;
    do {
      // Update the dgd pointers appropriately.
      int proc_ht = v_start;
      const int16_t *d_window = d + (i / wiener_win);
      const int16_t *d_current_row =
          d + (i / wiener_win) + ((i % wiener_win) * d_stride);
      downsample_factor =
          use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;
      __m256i sum_h[WIENER_WIN_CHROMA] = { _mm256_setzero_si256() };
      do {
        UPDATE_DOWNSAMPLE_FACTOR

        // Process the amount of width multiple of 16.
        while (proc_wd < wd_mul16) {
          const __m256i dgd =
              _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
          const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
          INIT_H_VALUES(d_window + shift + proc_wd, 5)

          proc_wd += 16;
        }

        // Process the remaining width here.
        if (wd_beyond_mul16) {
          const __m256i dgd =
              _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
          const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
          const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
          INIT_H_VALUES(d_window + shift + proc_wd, 5)
        }
        proc_ht += downsample_factor;
        d_window += downsample_factor * d_stride;
        d_current_row += downsample_factor * d_stride;
      } while (proc_ht < v_end);

      const __m256i s_h =
          hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
      _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + (wiener_win * j)),
                          s_h);
      const __m256i s_m_h = convert_and_add_avx2(sum_h[4]);
      const __m128i s_m_h0 = add_64bit_lvl_avx2(s_m_h, s_m_h);
      _mm_storel_epi64(
          (__m128i *)(H + (i * wiener_win2) + (wiener_win * j) + 4), s_m_h0);
      shift++;
    } while (++j < wiener_win);
  }

  fill_lower_triag_elements_avx2(wiener_win2, H);
}

// The buffers H(auto-covariance) and M(cross-correlation) are used to estimate
// the filter tap values required for wiener filtering. Here, the buffer H is of
// size ((wiener_window_size^2)*(wiener_window_size^2)) and M is of size
// (wiener_window_size*wiener_window_size). H is a symmetric matrix where the
// value above the diagonal (upper triangle) are equal to the values below the
// diagonal (lower triangle). The calculation of elements/stats of H(upper
// triangle) and M is done in steps as described below where each step fills
// specific values of H and M.
// Example:
// Wiener window size = WIENER_WIN (7)
// M buffer = [M0 M1 M2 ---- M47 M48]
// H buffer = Hxy (x-row, y-column)
// [H00 H01 H02 ---- H047 H048]
// [H10 H11 H12 ---- H147 H148]
// [H30 H31 H32 ---- H347 H348]
// [H40 H41 H42 ---- H447 H448]
// [H50 H51 H52 ---- H547 H548]
// [H60 H61 H62 ---- H647 H648]
//            ||
//            ||
// [H470 H471 H472 ---- H4747 H4748]
// [H480 H481 H482 ---- H4847 H4848]
// In Step 1, whole M buffers (i.e., M0 to M48) and the first row of H (i.e.,
// H00 to H048) is filled. The remaining rows of H buffer are filled through
// steps 2 to 8.
static void compute_stats_win7_avx2(const int16_t *const d, int32_t d_stride,
                                    const int16_t *const s, int32_t s_stride,
                                    int32_t width, int v_start, int v_end,
                                    int64_t *const M, int64_t *const H,
                                    int use_downsampled_wiener_stats) {
  const int32_t wiener_win = WIENER_WIN;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  // Amount of width which is beyond multiple of 16. This case is handled
  // appropriately to process only the required width towards the end.
  const int32_t wd_mul16 = width & ~15;
  const int32_t wd_beyond_mul16 = width - wd_mul16;
  const __m256i mask =
      _mm256_loadu_si256((__m256i *)(&mask_16bit[16 - wd_beyond_mul16]));
  int downsample_factor;

  // Step 1: Full M (i.e., M0 to M48) and first row H (i.e., H00 to H048)
  // values are filled here. Here, the loop over 'j' is executed for values 0
  // to 6. When the loop executed for a specific 'j', 7 values of M and H are
  // filled as shown below.
  // j=0: M0-M6 and H00-H06, j=1: M7-M13 and H07-H013 are filled etc,.
  int j = 0;
  do {
    const int16_t *s_t = s;
    const int16_t *d_t = d;
    __m256i sum_m[WIENER_WIN] = { _mm256_setzero_si256() };
    __m256i sum_h[WIENER_WIN] = { _mm256_setzero_si256() };
    downsample_factor =
        use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;
    int proc_ht = v_start;
    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i src = _mm256_loadu_si256((__m256i *)(s_t + proc_wd));
        const __m256i dgd = _mm256_loadu_si256((__m256i *)(d_t + proc_wd));
        const __m256i src_mul_df = _mm256_mullo_epi16(src, df_reg);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_MH_VALUES(d_t + j + proc_wd)

        proc_wd += 16;
      }

      if (wd_beyond_mul16) {
        const __m256i src = _mm256_loadu_si256((__m256i *)(s_t + proc_wd));
        const __m256i dgd = _mm256_loadu_si256((__m256i *)(d_t + proc_wd));
        const __m256i src_mask = _mm256_and_si256(src, mask);
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i src_mul_df = _mm256_mullo_epi16(src_mask, df_reg);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_MH_VALUES(d_t + j + proc_wd)
      }
      proc_ht += downsample_factor;
      s_t += downsample_factor * s_stride;
      d_t += downsample_factor * d_stride;
    } while (proc_ht < v_end);

    const __m256i s_m0 =
        hadd_four_32_to_64_avx2(sum_m[0], sum_m[1], &sum_m[2], &sum_m[3]);
    const __m256i s_m1 =
        hadd_four_32_to_64_avx2(sum_m[4], sum_m[5], &sum_m[6], &sum_m[6]);
    _mm256_storeu_si256((__m256i *)(M + wiener_win * j + 0), s_m0);
    _mm_storeu_si128((__m128i *)(M + wiener_win * j + 4),
                     _mm256_castsi256_si128(s_m1));
    _mm_storel_epi64((__m128i *)&M[wiener_win * j + 6],
                     _mm256_extracti128_si256(s_m1, 1));

    const __m256i sh_0 =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    const __m256i sh_1 =
        hadd_four_32_to_64_avx2(sum_h[4], sum_h[5], &sum_h[6], &sum_h[6]);
    _mm256_storeu_si256((__m256i *)(H + wiener_win * j + 0), sh_0);
    _mm_storeu_si128((__m128i *)(H + wiener_win * j + 4),
                     _mm256_castsi256_si128(sh_1));
    _mm_storel_epi64((__m128i *)&H[wiener_win * j + 6],
                     _mm256_extracti128_si256(sh_1, 1));
  } while (++j < wiener_win);

  // The below steps are designed to fill remaining rows of H buffer. Here, aim
  // is to fill only upper triangle elements correspond to each row and lower
  // triangle elements are copied from upper-triangle elements. Also, as
  // mentioned in Step 1, the core function is designed to fill 7
  // elements/stats/values of H buffer.
  //
  // Step 2: Here, the rows 1, 8, 15, 22, 29, 36 and 43 are filled. As we need
  // to fill only upper-triangle elements, H10 from row1, H80-H86 and H87 from
  // row8, etc. are need not be filled. As the core function process 7 values,
  // in first iteration of 'j' only 6 values to be filled i.e., H11-H16 from
  // row1 and H88-H813 from row8, etc.
  for (int i = 1; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN)

    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (1 * d_stride), 6)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (1 * d_stride), 6)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + i), s_h);
    const __m128i s_h0 = convert_32_to_64_add_avx2(sum_h[4], sum_h[5]);
    _mm_storeu_si128((__m128i *)(H + (i * wiener_win2) + i + 4), s_h0);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN7
  }

  // Step 3: Here, the rows 2, 9, 16, 23, 30, 37 and 44 are filled. As we need
  // to fill only upper-triangle elements, H20-H21 from row2, H90-H96 and
  // H97-H98 from row9, etc. are need not be filled. As the core function
  // process 7 values, in first iteration of 'j' only 5 values to be filled
  // i.e., H22-H26 from row2 and H99-H913 from row9, etc.
  for (int i = 2; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN)
    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (2 * d_stride), 5)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (2 * d_stride), 5)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + i), s_h);
    const __m256i s_m_h = convert_and_add_avx2(sum_h[4]);
    const __m128i s_m_h0 = add_64bit_lvl_avx2(s_m_h, s_m_h);
    _mm_storel_epi64((__m128i *)(H + (i * wiener_win2) + i + 4), s_m_h0);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN7
  }

  // Step 4: Here, the rows 3, 10, 17, 24, 31, 38 and 45 are filled. As we need
  // to fill only upper-triangle elements, H30-H32 from row3, H100-H106 and
  // H107-H109 from row10, etc. are need not be filled. As the core function
  // process 7 values, in first iteration of 'j' only 4 values to be filled
  // i.e., H33-H36 from row3 and H1010-H1013 from row10, etc.
  for (int i = 3; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN)

    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (3 * d_stride), 4)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (3 * d_stride), 4)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + i), s_h);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN7
  }

  // Step 5: Here, the rows 4, 11, 18, 25, 32, 39 and 46 are filled. As we need
  // to fill only upper-triangle elements, H40-H43 from row4, H110-H116 and
  // H117-H1110 from row10, etc. are need not be filled. As the core function
  // process 7 values, in first iteration of 'j' only 3 values to be filled
  // i.e., H44-H46 from row4 and H1111-H1113 from row11, etc.
  for (int i = 4; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN)

    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (4 * d_stride), 3)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (4 * d_stride), 3)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + i), s_h);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN7
  }

  // Step 6: Here, the rows 5, 12, 19, 26, 33, 40 and 47 are filled. As we need
  // to fill only upper-triangle elements, H50-H54 from row5, H120-H126 and
  // H127-H1211 from row12, etc. are need not be filled. As the core function
  // process 7 values, in first iteration of 'j' only 2 values to be filled
  // i.e., H55-H56 from row5 and H1212-H1213 from row12, etc.
  for (int i = 5; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN)
    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (5 * d_stride), 2)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (5 * d_stride), 2)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + i), s_h);

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN7
  }

  // Step 7: Here, the rows 6, 13, 20, 27, 34, 41 and 48 are filled. As we need
  // to fill only upper-triangle elements, H60-H65 from row6, H130-H136 and
  // H137-H1312 from row13, etc. are need not be filled. As the core function
  // process 7 values, in first iteration of 'j' only 1 value to be filled
  // i.e., H66 from row6 and H1313 from row13, etc.
  for (int i = 6; i < wiener_win2; i += wiener_win) {
    // Update the dgd pointers appropriately and also derive the 'j'th iteration
    // from where the H buffer filling needs to be started.
    INITIALIZATION(WIENER_WIN)
    do {
      UPDATE_DOWNSAMPLE_FACTOR

      // Process the amount of width multiple of 16.
      while (proc_wd < wd_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (6 * d_stride), 1)

        proc_wd += 16;
      }

      // Process the remaining width here.
      if (wd_beyond_mul16) {
        const __m256i dgd =
            _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
        const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
        const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
        INIT_H_VALUES(d_window + proc_wd + (6 * d_stride), 1)
      }
      proc_ht += downsample_factor;
      d_window += downsample_factor * d_stride;
      d_current_row += downsample_factor * d_stride;
    } while (proc_ht < v_end);
    const __m256i s_h =
        hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
    xx_storel_64(&H[(i * wiener_win2) + i], _mm256_castsi256_si128(s_h));

    // process the remaining 'j' iterations.
    j++;
    CALCULATE_REMAINING_H_WIN7
  }

  // Step 8: Here, the rows 7, 14, 21, 28, 35 and 42 are filled. As we need
  // to fill only upper-triangle elements, H70-H75 from row7, H140-H146 and
  // H147-H1413 from row14, etc. are need not be filled. The first iteration of
  // 'j' fills H77-H713 from row7 and H1414-H1420 from row14, etc.
  for (int i = 7; i < wiener_win2; i += wiener_win) {
    // Derive j'th iteration from where the H buffer filling needs to be
    // started.
    j = i / wiener_win;
    int shift = 0;
    do {
      // Update the dgd pointers appropriately.
      int proc_ht = v_start;
      const int16_t *d_window = d + (i / WIENER_WIN);
      const int16_t *d_current_row =
          d + (i / WIENER_WIN) + ((i % WIENER_WIN) * d_stride);
      downsample_factor =
          use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;
      __m256i sum_h[WIENER_WIN] = { _mm256_setzero_si256() };
      do {
        UPDATE_DOWNSAMPLE_FACTOR

        // Process the amount of width multiple of 16.
        while (proc_wd < wd_mul16) {
          const __m256i dgd =
              _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
          const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd, df_reg);
          INIT_H_VALUES(d_window + shift + proc_wd, 7)

          proc_wd += 16;
        }

        // Process the remaining width here.
        if (wd_beyond_mul16) {
          const __m256i dgd =
              _mm256_loadu_si256((__m256i *)(d_current_row + proc_wd));
          const __m256i dgd_mask = _mm256_and_si256(dgd, mask);
          const __m256i dgd_mul_df = _mm256_mullo_epi16(dgd_mask, df_reg);
          INIT_H_VALUES(d_window + shift + proc_wd, 7)
        }
        proc_ht += downsample_factor;
        d_window += downsample_factor * d_stride;
        d_current_row += downsample_factor * d_stride;
      } while (proc_ht < v_end);

      const __m256i sh_0 =
          hadd_four_32_to_64_avx2(sum_h[0], sum_h[1], &sum_h[2], &sum_h[3]);
      const __m256i sh_1 =
          hadd_four_32_to_64_avx2(sum_h[4], sum_h[5], &sum_h[6], &sum_h[6]);
      _mm256_storeu_si256((__m256i *)(H + (i * wiener_win2) + (wiener_win * j)),
                          sh_0);
      _mm_storeu_si128(
          (__m128i *)(H + (i * wiener_win2) + (wiener_win * j) + 4),
          _mm256_castsi256_si128(sh_1));
      _mm_storel_epi64((__m128i *)&H[(i * wiener_win2) + (wiener_win * j) + 6],
                       _mm256_extracti128_si256(sh_1, 1));
      shift++;
    } while (++j < wiener_win);
  }

  fill_lower_triag_elements_avx2(wiener_win2, H);
}

void av1_compute_stats_avx2(int wiener_win, const uint8_t *dgd,
                            const uint8_t *src, int16_t *dgd_avg,
                            int16_t *src_avg, int h_start, int h_end,
                            int v_start, int v_end, int dgd_stride,
                            int src_stride, int64_t *M, int64_t *H,
                            int use_downsampled_wiener_stats) {
  if (wiener_win != WIENER_WIN && wiener_win != WIENER_WIN_CHROMA) {
    // Currently, libaom supports Wiener filter processing with window sizes as
    // WIENER_WIN_CHROMA(5) and WIENER_WIN(7). For any other window size, SIMD
    // support is not facilitated. Hence, invoke C function for the same.
    av1_compute_stats_c(wiener_win, dgd, src, dgd_avg, src_avg, h_start, h_end,
                        v_start, v_end, dgd_stride, src_stride, M, H,
                        use_downsampled_wiener_stats);
    return;
  }

  const int32_t wiener_halfwin = wiener_win >> 1;
  const uint8_t avg =
      calc_dgd_buf_avg_avx2(dgd, h_start, h_end, v_start, v_end, dgd_stride);
  const int32_t width = h_end - h_start;
  const int32_t height = v_end - v_start;
  const int32_t d_stride = (width + 2 * wiener_halfwin + 15) & ~15;
  const int32_t s_stride = (width + 15) & ~15;

  // Based on the sf 'use_downsampled_wiener_stats', process either once for
  // UPDATE_DOWNSAMPLE_FACTOR or for each row.
  sub_avg_block_avx2(src + v_start * src_stride + h_start, src_stride, avg,
                     width, height, src_avg, s_stride,
                     use_downsampled_wiener_stats);

  // Compute (dgd-avg) buffer here which is used to fill H buffer.
  sub_avg_block_avx2(
      dgd + (v_start - wiener_halfwin) * dgd_stride + h_start - wiener_halfwin,
      dgd_stride, avg, width + 2 * wiener_halfwin, height + 2 * wiener_halfwin,
      dgd_avg, d_stride, 0);
  if (wiener_win == WIENER_WIN) {
    compute_stats_win7_avx2(dgd_avg, d_stride, src_avg, s_stride, width,
                            v_start, v_end, M, H, use_downsampled_wiener_stats);
  } else if (wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_win5_avx2(dgd_avg, d_stride, src_avg, s_stride, width,
                            v_start, v_end, M, H, use_downsampled_wiener_stats);
  }
}

static inline __m256i pair_set_epi16(int a, int b) {
  return _mm256_set1_epi32(
      (int32_t)(((uint16_t)(a)) | (((uint32_t)(uint16_t)(b)) << 16)));
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
        pair_set_epi16(xq_active, -xq_active * (1 << SGRPROJ_RST_BITS));
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
static inline void calc_proj_params_r0_r1_avx2(
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
static inline void calc_proj_params_r0_avx2(const uint8_t *src8, int width,
                                            int height, int src_stride,
                                            const uint8_t *dat8, int dat_stride,
                                            int32_t *flt0, int flt0_stride,
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
static inline void calc_proj_params_r1_avx2(const uint8_t *src8, int width,
                                            int height, int src_stride,
                                            const uint8_t *dat8, int dat_stride,
                                            int32_t *flt1, int flt1_stride,
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

#if CONFIG_AV1_HIGHBITDEPTH
static inline void calc_proj_params_r0_r1_high_bd_avx2(
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

static inline void calc_proj_params_r0_high_bd_avx2(
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

static inline void calc_proj_params_r1_high_bd_avx2(
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
