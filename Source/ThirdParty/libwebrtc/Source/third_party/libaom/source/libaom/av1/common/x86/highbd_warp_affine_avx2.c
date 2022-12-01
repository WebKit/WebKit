/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <immintrin.h>

#include "config/av1_rtcd.h"

#include "av1/common/warped_motion.h"

void av1_highbd_warp_affine_avx2(const int32_t *mat, const uint16_t *ref,
                                 int width, int height, int stride,
                                 uint16_t *pred, int p_col, int p_row,
                                 int p_width, int p_height, int p_stride,
                                 int subsampling_x, int subsampling_y, int bd,
                                 ConvolveParams *conv_params, int16_t alpha,
                                 int16_t beta, int16_t gamma, int16_t delta) {
  __m256i tmp[15];
  const int reduce_bits_horiz =
      conv_params->round_0 +
      AOMMAX(bd + FILTER_BITS - conv_params->round_0 - 14, 0);
  const int reduce_bits_vert = conv_params->is_compound
                                   ? conv_params->round_1
                                   : 2 * FILTER_BITS - reduce_bits_horiz;
  const int max_bits_horiz = bd + FILTER_BITS + 1 - reduce_bits_horiz;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;
  const int offset_bits_vert = bd + 2 * FILTER_BITS - reduce_bits_horiz;
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  (void)max_bits_horiz;
  assert(IMPLIES(conv_params->is_compound, conv_params->dst != NULL));

  const __m256i clip_pixel =
      _mm256_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m128i reduce_bits_vert_shift = _mm_cvtsi32_si128(reduce_bits_vert);
  const __m256i reduce_bits_vert_const =
      _mm256_set1_epi32(((1 << reduce_bits_vert) >> 1));
  const __m256i res_add_const = _mm256_set1_epi32(1 << offset_bits_vert);
  const __m256i res_sub_const =
      _mm256_set1_epi32(-(1 << (offset_bits - conv_params->round_1)) -
                        (1 << (offset_bits - conv_params->round_1 - 1)));
  __m128i round_bits_shift = _mm_cvtsi32_si128(round_bits);
  __m256i round_bits_const = _mm256_set1_epi32(((1 << round_bits) >> 1));

  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m256i wt0 = _mm256_set1_epi32(w0);
  const __m256i wt1 = _mm256_set1_epi32(w1);

  __m256i v_rbhoriz = _mm256_set1_epi32(1 << (reduce_bits_horiz - 1));
  __m256i v_zeros = _mm256_setzero_si256();
  int ohoriz = 1 << offset_bits_horiz;
  int mhoriz = 1 << max_bits_horiz;
  (void)mhoriz;
  int sx;

  for (int i = 0; i < p_height; i += 8) {
    for (int j = 0; j < p_width; j += 8) {
      // Calculate the center of this 8x8 block,
      // project to luma coordinates (if in a subsampled chroma plane),
      // apply the affine transformation,
      // then convert back to the original coordinates (if necessary)
      const int32_t src_x = (p_col + j + 4) << subsampling_x;
      const int32_t src_y = (p_row + i + 4) << subsampling_y;
      const int64_t dst_x =
          (int64_t)mat[2] * src_x + (int64_t)mat[3] * src_y + (int64_t)mat[0];
      const int64_t dst_y =
          (int64_t)mat[4] * src_x + (int64_t)mat[5] * src_y + (int64_t)mat[1];
      const int64_t x4 = dst_x >> subsampling_x;
      const int64_t y4 = dst_y >> subsampling_y;

      const int16_t ix4 = (int32_t)(x4 >> WARPEDMODEL_PREC_BITS);
      int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      const int16_t iy4 = (int32_t)(y4 >> WARPEDMODEL_PREC_BITS);
      int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      sx4 += alpha * (-4) + beta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
             (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);
      sy4 += gamma * (-4) + delta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
             (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);

      sx4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);
      sy4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);

      // Horizontal filter
      if (ix4 <= -7) {
        for (int k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          tmp[k + 7] = _mm256_cvtepi16_epi32(_mm_set1_epi16(
              (1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
              ref[iy * stride] * (1 << (FILTER_BITS - reduce_bits_horiz))));
        }
      } else if (ix4 >= width + 6) {
        for (int k = -7; k < AOMMIN(8, p_height - i); ++k) {
          int iy = iy4 + k;
          if (iy < 0)
            iy = 0;
          else if (iy > height - 1)
            iy = height - 1;
          tmp[k + 7] = _mm256_cvtepi16_epi32(
              _mm_set1_epi16((1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
                             ref[iy * stride + (width - 1)] *
                                 (1 << (FILTER_BITS - reduce_bits_horiz))));
        }
      } else if (((ix4 - 7) < 0) || ((ix4 + 9) > width)) {
        int32_t tmp1[8];
        for (int k = -7; k < AOMMIN(8, p_height - i); ++k) {
          const int iy = clamp(iy4 + k, 0, height - 1);

          sx = sx4 + beta * (k + 4);
          for (int l = -4; l < 4; ++l) {
            int ix = ix4 + l - 3;
            const int offs = sx >> WARPEDDIFF_PREC_BITS;
            const int16_t *coeffs = av1_warped_filter[offs];

            int32_t sum = 1 << offset_bits_horiz;
            for (int m = 0; m < 8; ++m) {
              const int sample_x = clamp(ix + m, 0, width - 1);
              sum += ref[iy * stride + sample_x] * coeffs[m];
            }
            sum = ROUND_POWER_OF_TWO(sum, reduce_bits_horiz);
            tmp1[(l + 4) / 2 + ((l + 4) % 2) * 4] = sum;
            sx += alpha;
          }
          tmp[k + 7] = _mm256_loadu_si256((__m256i *)tmp1);
        }
      } else {
        if (beta == 0 && alpha == 0) {
          sx = sx4;
          __m128i v_01 = _mm_loadu_si128(
              (__m128i *)
                  av1_warped_filter[sx >>
                                    WARPEDDIFF_PREC_BITS]);  // A7A6A5A4A3A2A1A0
          __m256i v_c01 = _mm256_broadcastd_epi32(v_01);     // A1A0A1A0A1A0A1A0
          __m256i v_c23 = _mm256_broadcastd_epi32(
              _mm_shuffle_epi32(v_01, 1));  // A3A2A3A2A3A2A3A2
          __m256i v_c45 = _mm256_broadcastd_epi32(
              _mm_shuffle_epi32(v_01, 2));  // A5A4A5A4A5A4A5A4
          __m256i v_c67 = _mm256_broadcastd_epi32(
              _mm_shuffle_epi32(v_01, 3));  // A7A6A7A6A7A6A7A6
          for (int k = -7; k < AOMMIN(8, p_height - i); ++k) {
            int iy = iy4 + k;
            if (iy < 0)
              iy = 0;
            else if (iy > height - 1)
              iy = height - 1;
            iy = iy * stride;

            __m256i v_refl = _mm256_inserti128_si256(
                _mm256_set1_epi16(0),
                _mm_loadu_si128((__m128i *)&ref[iy + ix4 - 7]), 0);
            v_refl = _mm256_inserti128_si256(
                v_refl, _mm_loadu_si128((__m128i *)&ref[iy + ix4 + 1]),
                1);  // R15 .. R0

            __m256i v_ref = _mm256_permute4x64_epi64(v_refl, 0xEE);

            __m256i v_refu =
                _mm256_alignr_epi8(v_ref, v_refl, 2);  // R8R15R14...R2R1
            v_refl = _mm256_inserti128_si256(
                v_refl, _mm256_extracti128_si256(v_refu, 0), 1);
            v_refu = _mm256_inserti128_si256(
                v_refu, _mm256_extracti128_si256(v_ref, 0), 0);

            __m256i v_sum = _mm256_set1_epi32(ohoriz);
            __m256i parsum = _mm256_madd_epi16(
                v_c01, _mm256_alignr_epi8(v_refu, v_refl,
                                          0));  // R8R7R6..R1R7R6R5..R1R0
            __m256i v_sum1 = _mm256_add_epi32(v_sum, parsum);

            parsum = _mm256_madd_epi16(
                v_c23,
                _mm256_alignr_epi8(v_refu, v_refl, 4));  // R10R9..R3R9R8..R3R2
            __m256i v_sum2 = _mm256_add_epi32(v_sum1, parsum);
            parsum = _mm256_madd_epi16(
                v_c45, _mm256_alignr_epi8(v_refu, v_refl,
                                          8));  // R12R11..R5R11R10..R5R4
            __m256i v_sum3 = _mm256_add_epi32(v_sum2, parsum);
            parsum = _mm256_madd_epi16(
                v_c67, _mm256_alignr_epi8(v_refu, v_refl,
                                          12));  // R14R13..R7R13R12..R7R6
            __m256i v_sum4 = _mm256_add_epi32(v_sum3, parsum);

            tmp[k + 7] = _mm256_srai_epi32(_mm256_add_epi32(v_sum4, v_rbhoriz),
                                           reduce_bits_horiz);
          }
        } else if (alpha == 0) {
          for (int k = -7; k < AOMMIN(8, p_height - i); ++k) {
            int iy = iy4 + k;
            if (iy < 0)
              iy = 0;
            else if (iy > height - 1)
              iy = height - 1;
            iy = iy * stride;

            sx = sx4 + beta * (k + 4);

            __m128i v_01 = _mm_loadu_si128(
                (__m128i *)av1_warped_filter
                    [sx >> WARPEDDIFF_PREC_BITS]);          // A7A6A5A4A3A2A1A0
            __m256i v_c01 = _mm256_broadcastd_epi32(v_01);  // A1A0A1A0A1A0A1A0
            __m256i v_c23 = _mm256_broadcastd_epi32(
                _mm_shuffle_epi32(v_01, 1));  // A3A2A3A2A3A2A3A2
            __m256i v_c45 = _mm256_broadcastd_epi32(
                _mm_shuffle_epi32(v_01, 2));  // A5A4A5A4A5A4A5A4
            __m256i v_c67 = _mm256_broadcastd_epi32(
                _mm_shuffle_epi32(v_01, 3));  // A7A6A7A6A7A6A7A6

            __m256i v_refl = _mm256_inserti128_si256(
                _mm256_set1_epi16(0),
                _mm_loadu_si128((__m128i *)&ref[iy + ix4 - 7]), 0);
            v_refl = _mm256_inserti128_si256(
                v_refl, _mm_loadu_si128((__m128i *)&ref[iy + ix4 + 1]),
                1);  // R15 .. R0

            __m256i v_ref = _mm256_permute4x64_epi64(v_refl, 0xEE);

            __m256i v_refu =
                _mm256_alignr_epi8(v_ref, v_refl, 2);  // R8R15R14...R2R1

            v_refl = _mm256_inserti128_si256(
                v_refl, _mm256_extracti128_si256(v_refu, 0), 1);
            v_refu = _mm256_inserti128_si256(
                v_refu, _mm256_extracti128_si256(v_ref, 0), 0);

            __m256i v_sum = _mm256_set1_epi32(ohoriz);
            __m256i parsum =
                _mm256_madd_epi16(v_c01, _mm256_alignr_epi8(v_refu, v_refl, 0));
            __m256i v_sum1 = _mm256_add_epi32(v_sum, parsum);

            parsum =
                _mm256_madd_epi16(v_c23, _mm256_alignr_epi8(v_refu, v_refl, 4));
            __m256i v_sum2 = _mm256_add_epi32(v_sum1, parsum);
            parsum =
                _mm256_madd_epi16(v_c45, _mm256_alignr_epi8(v_refu, v_refl, 8));
            __m256i v_sum3 = _mm256_add_epi32(v_sum2, parsum);
            parsum = _mm256_madd_epi16(v_c67,
                                       _mm256_alignr_epi8(v_refu, v_refl, 12));
            __m256i v_sum4 = _mm256_add_epi32(v_sum3, parsum);

            tmp[k + 7] = _mm256_srai_epi32(_mm256_add_epi32(v_sum4, v_rbhoriz),
                                           reduce_bits_horiz);
          }
        } else if (beta == 0) {
          sx = sx4;
          __m256i v_coeff01 = _mm256_inserti128_si256(
              v_zeros,
              _mm_loadu_si128(
                  (__m128i *)av1_warped_filter[(sx) >> WARPEDDIFF_PREC_BITS]),
              0);
          v_coeff01 = _mm256_inserti128_si256(
              v_coeff01,
              _mm_loadu_si128(
                  (__m128i *)
                      av1_warped_filter[(sx + alpha) >> WARPEDDIFF_PREC_BITS]),
              1);  // B7B6..B1B0A7A6..A1A0
          __m256i v_coeff23 = _mm256_inserti128_si256(
              v_zeros,
              _mm_loadu_si128(
                  (__m128i *)av1_warped_filter[(sx + 2 * alpha) >>
                                               WARPEDDIFF_PREC_BITS]),
              0);
          v_coeff23 = _mm256_inserti128_si256(
              v_coeff23,
              _mm_loadu_si128(
                  (__m128i *)av1_warped_filter[(sx + 3 * alpha) >>
                                               WARPEDDIFF_PREC_BITS]),
              1);  // D7D6..D1D0C7C6..C1C0
          __m256i v_coeff45 = _mm256_inserti128_si256(
              v_zeros,
              _mm_loadu_si128(
                  (__m128i *)av1_warped_filter[(sx + 4 * alpha) >>
                                               WARPEDDIFF_PREC_BITS]),
              0);
          v_coeff45 = _mm256_inserti128_si256(
              v_coeff45,
              _mm_loadu_si128(
                  (__m128i *)av1_warped_filter[(sx + 5 * alpha) >>
                                               WARPEDDIFF_PREC_BITS]),
              1);  // F7F6..F1F0E7E6..E1E0
          __m256i v_coeff67 = _mm256_inserti128_si256(
              v_zeros,
              _mm_loadu_si128(
                  (__m128i *)av1_warped_filter[(sx + 6 * alpha) >>
                                               WARPEDDIFF_PREC_BITS]),
              0);
          v_coeff67 = _mm256_inserti128_si256(
              v_coeff67,
              _mm_loadu_si128(
                  (__m128i *)av1_warped_filter[(sx + 7 * alpha) >>
                                               WARPEDDIFF_PREC_BITS]),
              1);  // H7H6..H1H0G7G6..G1G0

          __m256i v_c0123 = _mm256_unpacklo_epi32(
              v_coeff01,
              v_coeff23);  // D3D2B3B2D1D0B1B0C3C2A3A2C1C0A1A0
          __m256i v_c0123u = _mm256_unpackhi_epi32(
              v_coeff01,
              v_coeff23);  // D7D6B7B6D5D4B5B4C7C6A7A6C5C4A5A4
          __m256i v_c4567 = _mm256_unpacklo_epi32(
              v_coeff45,
              v_coeff67);  // H3H2F3F2H1H0F1F0G3G2E3E2G1G0E1E0
          __m256i v_c4567u = _mm256_unpackhi_epi32(
              v_coeff45,
              v_coeff67);  // H7H6F7F6H5H4F5F4G7G6E7E6G5G4E5E4

          __m256i v_c01 = _mm256_unpacklo_epi64(
              v_c0123, v_c4567);  // H1H0F1F0D1D0B1B0G1G0E1E0C1C0A1A0
          __m256i v_c23 =
              _mm256_unpackhi_epi64(v_c0123, v_c4567);  // H3H2 ... A3A2
          __m256i v_c45 =
              _mm256_unpacklo_epi64(v_c0123u, v_c4567u);  // H5H4 ... A5A4
          __m256i v_c67 =
              _mm256_unpackhi_epi64(v_c0123u, v_c4567u);  // H7H6 ... A7A6

          for (int k = -7; k < AOMMIN(8, p_height - i); ++k) {
            int iy = iy4 + k;
            if (iy < 0)
              iy = 0;
            else if (iy > height - 1)
              iy = height - 1;
            iy = iy * stride;

            __m256i v_refl = _mm256_inserti128_si256(
                _mm256_set1_epi16(0),
                _mm_loadu_si128((__m128i *)&ref[iy + ix4 - 7]), 0);
            v_refl = _mm256_inserti128_si256(
                v_refl, _mm_loadu_si128((__m128i *)&ref[iy + ix4 + 1]),
                1);  // R15 .. R0

            __m256i v_ref = _mm256_permute4x64_epi64(v_refl, 0xEE);

            __m256i v_refu =
                _mm256_alignr_epi8(v_ref, v_refl, 2);  // R8R15R14...R2R1

            v_refl = _mm256_inserti128_si256(
                v_refl, _mm256_extracti128_si256(v_refu, 0), 1);
            v_refu = _mm256_inserti128_si256(
                v_refu, _mm256_extracti128_si256(v_ref, 0), 0);

            __m256i v_sum = _mm256_set1_epi32(ohoriz);
            __m256i parsum = _mm256_madd_epi16(
                v_c01, _mm256_alignr_epi8(v_refu, v_refl,
                                          0));  // R8R7R6..R1R7R6R5..R1R0
            __m256i v_sum1 = _mm256_add_epi32(v_sum, parsum);

            parsum = _mm256_madd_epi16(
                v_c23,
                _mm256_alignr_epi8(v_refu, v_refl, 4));  // R10R9..R3R9R8..R3R2
            __m256i v_sum2 = _mm256_add_epi32(v_sum1, parsum);
            parsum = _mm256_madd_epi16(
                v_c45, _mm256_alignr_epi8(v_refu, v_refl,
                                          8));  // R12R11..R5R11R10..R5R4
            __m256i v_sum3 = _mm256_add_epi32(v_sum2, parsum);
            parsum = _mm256_madd_epi16(
                v_c67, _mm256_alignr_epi8(v_refu, v_refl,
                                          12));  // R14R13..R7R13R12..R7R6
            __m256i v_sum4 = _mm256_add_epi32(v_sum3, parsum);

            tmp[k + 7] = _mm256_srai_epi32(_mm256_add_epi32(v_sum4, v_rbhoriz),
                                           reduce_bits_horiz);
          }

        } else {
          for (int k = -7; k < AOMMIN(8, p_height - i); ++k) {
            int iy = iy4 + k;
            if (iy < 0)
              iy = 0;
            else if (iy > height - 1)
              iy = height - 1;
            iy = iy * stride;

            sx = sx4 + beta * (k + 4);

            __m256i v_coeff01 = _mm256_inserti128_si256(
                v_zeros,
                _mm_loadu_si128(
                    (__m128i *)av1_warped_filter[(sx) >> WARPEDDIFF_PREC_BITS]),
                0);
            v_coeff01 = _mm256_inserti128_si256(
                v_coeff01,
                _mm_loadu_si128(
                    (__m128i *)av1_warped_filter[(sx + alpha) >>
                                                 WARPEDDIFF_PREC_BITS]),
                1);  // B7B6..B1B0A7A6..A1A0
            __m256i v_coeff23 = _mm256_inserti128_si256(
                v_zeros,
                _mm_loadu_si128(
                    (__m128i *)av1_warped_filter[(sx + 2 * alpha) >>
                                                 WARPEDDIFF_PREC_BITS]),
                0);
            v_coeff23 = _mm256_inserti128_si256(
                v_coeff23,
                _mm_loadu_si128(
                    (__m128i *)av1_warped_filter[(sx + 3 * alpha) >>
                                                 WARPEDDIFF_PREC_BITS]),
                1);  // D7D6..D1D0C7C6..C1C0
            __m256i v_coeff45 = _mm256_inserti128_si256(
                v_zeros,
                _mm_loadu_si128(
                    (__m128i *)av1_warped_filter[(sx + 4 * alpha) >>
                                                 WARPEDDIFF_PREC_BITS]),
                0);
            v_coeff45 = _mm256_inserti128_si256(
                v_coeff45,
                _mm_loadu_si128(
                    (__m128i *)av1_warped_filter[(sx + 5 * alpha) >>
                                                 WARPEDDIFF_PREC_BITS]),
                1);  // F7F6..F1F0E7E6..E1E0
            __m256i v_coeff67 = _mm256_inserti128_si256(
                v_zeros,
                _mm_loadu_si128(
                    (__m128i *)av1_warped_filter[(sx + 6 * alpha) >>
                                                 WARPEDDIFF_PREC_BITS]),
                0);
            v_coeff67 = _mm256_inserti128_si256(
                v_coeff67,
                _mm_loadu_si128(
                    (__m128i *)av1_warped_filter[(sx + 7 * alpha) >>
                                                 WARPEDDIFF_PREC_BITS]),
                1);  // H7H6..H1H0G7G6..G1G0

            __m256i v_c0123 = _mm256_unpacklo_epi32(
                v_coeff01,
                v_coeff23);  // D3D2B3B2D1D0B1B0C3C2A3A2C1C0A1A0
            __m256i v_c0123u = _mm256_unpackhi_epi32(
                v_coeff01,
                v_coeff23);  // D7D6B7B6D5D4B5B4C7C6A7A6C5C4A5A4
            __m256i v_c4567 = _mm256_unpacklo_epi32(
                v_coeff45,
                v_coeff67);  // H3H2F3F2H1H0F1F0G3G2E3E2G1G0E1E0
            __m256i v_c4567u = _mm256_unpackhi_epi32(
                v_coeff45,
                v_coeff67);  // H7H6F7F6H5H4F5F4G7G6E7E6G5G4E5E4

            __m256i v_c01 = _mm256_unpacklo_epi64(
                v_c0123, v_c4567);  // H1H0F1F0D1D0B1B0G1G0E1E0C1C0A1A0
            __m256i v_c23 =
                _mm256_unpackhi_epi64(v_c0123, v_c4567);  // H3H2 ... A3A2
            __m256i v_c45 =
                _mm256_unpacklo_epi64(v_c0123u, v_c4567u);  // H5H4 ... A5A4
            __m256i v_c67 =
                _mm256_unpackhi_epi64(v_c0123u, v_c4567u);  // H7H6 ... A7A6

            __m256i v_refl = _mm256_inserti128_si256(
                _mm256_set1_epi16(0),
                _mm_loadu_si128((__m128i *)&ref[iy + ix4 - 7]), 0);
            v_refl = _mm256_inserti128_si256(
                v_refl, _mm_loadu_si128((__m128i *)&ref[iy + ix4 + 1]),
                1);  // R15 .. R0

            __m256i v_ref = _mm256_permute4x64_epi64(v_refl, 0xEE);

            __m256i v_refu =
                _mm256_alignr_epi8(v_ref, v_refl, 2);  // R8R15R14...R2R1

            v_refl = _mm256_inserti128_si256(
                v_refl, _mm256_extracti128_si256(v_refu, 0), 1);
            v_refu = _mm256_inserti128_si256(
                v_refu, _mm256_extracti128_si256(v_ref, 0), 0);

            __m256i v_sum = _mm256_set1_epi32(ohoriz);
            __m256i parsum =
                _mm256_madd_epi16(v_c01, _mm256_alignr_epi8(v_refu, v_refl, 0));
            __m256i v_sum1 = _mm256_add_epi32(v_sum, parsum);

            parsum =
                _mm256_madd_epi16(v_c23, _mm256_alignr_epi8(v_refu, v_refl, 4));
            __m256i v_sum2 = _mm256_add_epi32(v_sum1, parsum);
            parsum =
                _mm256_madd_epi16(v_c45, _mm256_alignr_epi8(v_refu, v_refl, 8));
            __m256i v_sum3 = _mm256_add_epi32(v_sum2, parsum);
            parsum = _mm256_madd_epi16(v_c67,
                                       _mm256_alignr_epi8(v_refu, v_refl, 12));
            __m256i v_sum4 = _mm256_add_epi32(v_sum3, parsum);

            tmp[k + 7] = _mm256_srai_epi32(_mm256_add_epi32(v_sum4, v_rbhoriz),
                                           reduce_bits_horiz);
          }
        }
      }

      // Vertical filter
      for (int k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
        int sy = sy4 + delta * (k + 4);
        const __m256i *src = tmp + (k + 4);

        __m256i v_coeff01 = _mm256_inserti128_si256(
            v_zeros,
            _mm_loadu_si128(
                (__m128i *)av1_warped_filter[(sy) >> WARPEDDIFF_PREC_BITS]),
            0);
        v_coeff01 = _mm256_inserti128_si256(
            v_coeff01,
            _mm_loadu_si128(
                (__m128i *)
                    av1_warped_filter[(sy + gamma) >> WARPEDDIFF_PREC_BITS]),
            1);
        __m256i v_coeff23 = _mm256_inserti128_si256(
            v_zeros,
            _mm_loadu_si128((__m128i *)av1_warped_filter[(sy + 2 * gamma) >>
                                                         WARPEDDIFF_PREC_BITS]),
            0);
        v_coeff23 = _mm256_inserti128_si256(
            v_coeff23,
            _mm_loadu_si128((__m128i *)av1_warped_filter[(sy + 3 * gamma) >>
                                                         WARPEDDIFF_PREC_BITS]),
            1);
        __m256i v_coeff45 = _mm256_inserti128_si256(
            v_zeros,
            _mm_loadu_si128((__m128i *)av1_warped_filter[(sy + 4 * gamma) >>
                                                         WARPEDDIFF_PREC_BITS]),
            0);
        v_coeff45 = _mm256_inserti128_si256(
            v_coeff45,
            _mm_loadu_si128((__m128i *)av1_warped_filter[(sy + 5 * gamma) >>
                                                         WARPEDDIFF_PREC_BITS]),
            1);
        __m256i v_coeff67 = _mm256_inserti128_si256(
            v_zeros,
            _mm_loadu_si128((__m128i *)av1_warped_filter[(sy + 6 * gamma) >>
                                                         WARPEDDIFF_PREC_BITS]),
            0);
        v_coeff67 = _mm256_inserti128_si256(
            v_coeff67,
            _mm_loadu_si128((__m128i *)av1_warped_filter[(sy + 7 * gamma) >>
                                                         WARPEDDIFF_PREC_BITS]),
            1);

        __m256i v_c0123 = _mm256_unpacklo_epi32(
            v_coeff01,
            v_coeff23);  // D3D2B3B2D1D0B1B0C3C2A3A2C1C0A1A0
        __m256i v_c0123u = _mm256_unpackhi_epi32(
            v_coeff01,
            v_coeff23);  // D7D6B7B6D5D4B5B4C7C6A7A6C5C4A5A4
        __m256i v_c4567 = _mm256_unpacklo_epi32(
            v_coeff45,
            v_coeff67);  // H3H2F3F2H1H0F1F0G3G2E3E2G1G0E1E0
        __m256i v_c4567u = _mm256_unpackhi_epi32(
            v_coeff45,
            v_coeff67);  // H7H6F7F6H5H4F5F4G7G6E7E6G5G4E5E4

        __m256i v_c01 = _mm256_unpacklo_epi64(
            v_c0123, v_c4567);  // H1H0F1F0D1D0B1B0G1G0E1E0C1C0A1A0
        __m256i v_c23 =
            _mm256_unpackhi_epi64(v_c0123, v_c4567);  // H3H2 ... A3A2
        __m256i v_c45 =
            _mm256_unpacklo_epi64(v_c0123u, v_c4567u);  // H5H4 ... A5A4
        __m256i v_c67 =
            _mm256_unpackhi_epi64(v_c0123u, v_c4567u);  // H7H6 ... A7A6

        __m256i v_src01l =
            _mm256_unpacklo_epi32(src[0], src[1]);  // T13T03T11T01T12T02T10T00
        __m256i v_src01u =
            _mm256_unpackhi_epi32(src[0], src[1]);  // T17T07T15T05T16T06T14T04
        __m256i v_sum =
            _mm256_madd_epi16(_mm256_packus_epi32(v_src01l, v_src01u),
                              v_c01);  // S7S5S3S1S6S4S2S0

        __m256i v_src23l = _mm256_unpacklo_epi32(src[2], src[3]);
        __m256i v_src23u = _mm256_unpackhi_epi32(src[2], src[3]);
        v_sum = _mm256_add_epi32(
            v_sum,
            _mm256_madd_epi16(_mm256_packus_epi32(v_src23l, v_src23u), v_c23));

        __m256i v_src45l = _mm256_unpacklo_epi32(src[4], src[5]);
        __m256i v_src45u = _mm256_unpackhi_epi32(src[4], src[5]);
        v_sum = _mm256_add_epi32(
            v_sum,
            _mm256_madd_epi16(_mm256_packus_epi32(v_src45l, v_src45u), v_c45));

        __m256i v_src67l = _mm256_unpacklo_epi32(src[6], src[7]);
        __m256i v_src67u = _mm256_unpackhi_epi32(src[6], src[7]);
        v_sum = _mm256_add_epi32(
            v_sum,
            _mm256_madd_epi16(_mm256_packus_epi32(v_src67l, v_src67u), v_c67));

        // unpack S7S5S3S1S6S4S2S0 to S7S6S5S4S3S2S1S0

        __m256i v_suml =
            _mm256_permute4x64_epi64(v_sum, 0xD8);  // S7S5S6S4S3S1S2S0
        __m256i v_sumh =
            _mm256_permute4x64_epi64(v_sum, 0x32);      // S2S0S7S5S2S0S3S1
        v_sum = _mm256_unpacklo_epi32(v_suml, v_sumh);  // S7S6S5S4S3S2S1S0

        if (conv_params->is_compound) {
          __m128i *const p =
              (__m128i *)&conv_params
                  ->dst[(i + k + 4) * conv_params->dst_stride + j];

          v_sum = _mm256_add_epi32(v_sum, res_add_const);
          v_sum =
              _mm256_sra_epi32(_mm256_add_epi32(v_sum, reduce_bits_vert_const),
                               reduce_bits_vert_shift);
          if (conv_params->do_average) {
            __m128i *const dst16 = (__m128i *)&pred[(i + k + 4) * p_stride + j];
            __m256i p_32 = _mm256_cvtepu16_epi32(_mm_loadu_si128(p));

            if (conv_params->use_dist_wtd_comp_avg) {
              v_sum = _mm256_add_epi32(_mm256_mullo_epi32(p_32, wt0),
                                       _mm256_mullo_epi32(v_sum, wt1));
              v_sum = _mm256_srai_epi32(v_sum, DIST_PRECISION_BITS);
            } else {
              v_sum = _mm256_srai_epi32(_mm256_add_epi32(p_32, v_sum), 1);
            }

            __m256i v_sum1 = _mm256_add_epi32(v_sum, res_sub_const);
            v_sum1 = _mm256_sra_epi32(
                _mm256_add_epi32(v_sum1, round_bits_const), round_bits_shift);

            __m256i v_sum16 = _mm256_packus_epi32(v_sum1, v_sum1);
            v_sum16 = _mm256_permute4x64_epi64(v_sum16, 0xD8);
            v_sum16 = _mm256_min_epi16(v_sum16, clip_pixel);
            _mm_storeu_si128(dst16, _mm256_extracti128_si256(v_sum16, 0));
          } else {
            v_sum = _mm256_packus_epi32(v_sum, v_sum);
            __m256i v_sum16 = _mm256_permute4x64_epi64(v_sum, 0xD8);
            _mm_storeu_si128(p, _mm256_extracti128_si256(v_sum16, 0));
          }
        } else {
          // Round and pack into 8 bits
          const __m256i round_const =
              _mm256_set1_epi32(-(1 << (bd + reduce_bits_vert - 1)) +
                                ((1 << reduce_bits_vert) >> 1));

          __m256i v_sum1 = _mm256_srai_epi32(
              _mm256_add_epi32(v_sum, round_const), reduce_bits_vert);

          v_sum1 = _mm256_packus_epi32(v_sum1, v_sum1);
          __m256i v_sum16 = _mm256_permute4x64_epi64(v_sum1, 0xD8);
          // Clamp res_16bit to the range [0, 2^bd - 1]
          const __m256i max_val = _mm256_set1_epi16((1 << bd) - 1);
          const __m256i zero = _mm256_setzero_si256();
          v_sum16 = _mm256_max_epi16(_mm256_min_epi16(v_sum16, max_val), zero);

          __m128i *const p = (__m128i *)&pred[(i + k + 4) * p_stride + j];

          _mm_storeu_si128(p, _mm256_extracti128_si256(v_sum16, 0));
        }
      }
    }
  }
}
