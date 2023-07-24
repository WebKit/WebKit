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

#include <math.h>

#include <immintrin.h>
#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"
#include "aom_dsp/flow_estimation/corner_match.h"

DECLARE_ALIGNED(16, static const uint8_t,
                byte_mask[16]) = { 255, 255, 255, 255, 255, 255, 255, 255,
                                   255, 255, 255, 255, 255, 0,   0,   0 };
#if MATCH_SZ != 13
#error "Need to change byte_mask in corner_match_sse4.c if MATCH_SZ != 13"
#endif

/* Compute corr(frame1, frame2) * MATCH_SZ * stddev(frame1), where the
correlation/standard deviation are taken over MATCH_SZ by MATCH_SZ windows
of each image, centered at (x1, y1) and (x2, y2) respectively.
*/
double av1_compute_cross_correlation_avx2(const unsigned char *frame1,
                                          int stride1, int x1, int y1,
                                          const unsigned char *frame2,
                                          int stride2, int x2, int y2) {
  int i, stride1_i = 0, stride2_i = 0;
  __m256i temp1, sum_vec, sumsq2_vec, cross_vec, v, v1_1, v2_1;
  const __m128i mask = _mm_load_si128((__m128i *)byte_mask);
  const __m256i zero = _mm256_setzero_si256();
  __m128i v1, v2;

  sum_vec = zero;
  sumsq2_vec = zero;
  cross_vec = zero;

  frame1 += (y1 - MATCH_SZ_BY2) * stride1 + (x1 - MATCH_SZ_BY2);
  frame2 += (y2 - MATCH_SZ_BY2) * stride2 + (x2 - MATCH_SZ_BY2);

  for (i = 0; i < MATCH_SZ; ++i) {
    v1 = _mm_and_si128(_mm_loadu_si128((__m128i *)&frame1[stride1_i]), mask);
    v1_1 = _mm256_cvtepu8_epi16(v1);
    v2 = _mm_and_si128(_mm_loadu_si128((__m128i *)&frame2[stride2_i]), mask);
    v2_1 = _mm256_cvtepu8_epi16(v2);

    v = _mm256_insertf128_si256(_mm256_castsi128_si256(v1), v2, 1);
    sumsq2_vec = _mm256_add_epi32(sumsq2_vec, _mm256_madd_epi16(v2_1, v2_1));

    sum_vec = _mm256_add_epi16(sum_vec, _mm256_sad_epu8(v, zero));
    cross_vec = _mm256_add_epi32(cross_vec, _mm256_madd_epi16(v1_1, v2_1));
    stride1_i += stride1;
    stride2_i += stride2;
  }
  __m256i sum_vec1 = _mm256_srli_si256(sum_vec, 8);
  sum_vec = _mm256_add_epi32(sum_vec, sum_vec1);
  int sum1_acc = _mm_cvtsi128_si32(_mm256_castsi256_si128(sum_vec));
  int sum2_acc = _mm256_extract_epi32(sum_vec, 4);

  __m256i unp_low = _mm256_unpacklo_epi64(sumsq2_vec, cross_vec);
  __m256i unp_hig = _mm256_unpackhi_epi64(sumsq2_vec, cross_vec);
  temp1 = _mm256_add_epi32(unp_low, unp_hig);

  __m128i low_sumsq = _mm256_castsi256_si128(temp1);
  low_sumsq = _mm_add_epi32(low_sumsq, _mm256_extractf128_si256(temp1, 1));
  low_sumsq = _mm_add_epi32(low_sumsq, _mm_srli_epi64(low_sumsq, 32));
  int sumsq2_acc = _mm_cvtsi128_si32(low_sumsq);
  int cross_acc = _mm_extract_epi32(low_sumsq, 2);

  int var2 = sumsq2_acc * MATCH_SZ_SQ - sum2_acc * sum2_acc;
  int cov = cross_acc * MATCH_SZ_SQ - sum1_acc * sum2_acc;
  return cov / sqrt((double)var2);
}
