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

#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include <smmintrin.h>

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
double av1_compute_cross_correlation_sse4_1(const unsigned char *frame1,
                                            int stride1, int x1, int y1,
                                            const unsigned char *frame2,
                                            int stride2, int x2, int y2) {
  int i;
  // 2 16-bit partial sums in lanes 0, 4 (== 2 32-bit partial sums in lanes 0,
  // 2)
  __m128i sum1_vec = _mm_setzero_si128();
  __m128i sum2_vec = _mm_setzero_si128();
  // 4 32-bit partial sums of squares
  __m128i sumsq2_vec = _mm_setzero_si128();
  __m128i cross_vec = _mm_setzero_si128();

  const __m128i mask = _mm_load_si128((__m128i *)byte_mask);
  const __m128i zero = _mm_setzero_si128();

  frame1 += (y1 - MATCH_SZ_BY2) * stride1 + (x1 - MATCH_SZ_BY2);
  frame2 += (y2 - MATCH_SZ_BY2) * stride2 + (x2 - MATCH_SZ_BY2);

  for (i = 0; i < MATCH_SZ; ++i) {
    const __m128i v1 =
        _mm_and_si128(_mm_loadu_si128((__m128i *)&frame1[i * stride1]), mask);
    const __m128i v2 =
        _mm_and_si128(_mm_loadu_si128((__m128i *)&frame2[i * stride2]), mask);

    // Using the 'sad' intrinsic here is a bit faster than adding
    // v1_l + v1_r and v2_l + v2_r, plus it avoids the need for a 16->32 bit
    // conversion step later, for a net speedup of ~10%
    sum1_vec = _mm_add_epi16(sum1_vec, _mm_sad_epu8(v1, zero));
    sum2_vec = _mm_add_epi16(sum2_vec, _mm_sad_epu8(v2, zero));

    const __m128i v1_l = _mm_cvtepu8_epi16(v1);
    const __m128i v1_r = _mm_cvtepu8_epi16(_mm_srli_si128(v1, 8));
    const __m128i v2_l = _mm_cvtepu8_epi16(v2);
    const __m128i v2_r = _mm_cvtepu8_epi16(_mm_srli_si128(v2, 8));

    sumsq2_vec = _mm_add_epi32(
        sumsq2_vec,
        _mm_add_epi32(_mm_madd_epi16(v2_l, v2_l), _mm_madd_epi16(v2_r, v2_r)));
    cross_vec = _mm_add_epi32(
        cross_vec,
        _mm_add_epi32(_mm_madd_epi16(v1_l, v2_l), _mm_madd_epi16(v1_r, v2_r)));
  }

  // Now we can treat the four registers (sum1_vec, sum2_vec, sumsq2_vec,
  // cross_vec)
  // as holding 4 32-bit elements each, which we want to sum horizontally.
  // We do this by transposing and then summing vertically.
  __m128i tmp_0 = _mm_unpacklo_epi32(sum1_vec, sum2_vec);
  __m128i tmp_1 = _mm_unpackhi_epi32(sum1_vec, sum2_vec);
  __m128i tmp_2 = _mm_unpacklo_epi32(sumsq2_vec, cross_vec);
  __m128i tmp_3 = _mm_unpackhi_epi32(sumsq2_vec, cross_vec);

  __m128i tmp_4 = _mm_unpacklo_epi64(tmp_0, tmp_2);
  __m128i tmp_5 = _mm_unpackhi_epi64(tmp_0, tmp_2);
  __m128i tmp_6 = _mm_unpacklo_epi64(tmp_1, tmp_3);
  __m128i tmp_7 = _mm_unpackhi_epi64(tmp_1, tmp_3);

  __m128i res =
      _mm_add_epi32(_mm_add_epi32(tmp_4, tmp_5), _mm_add_epi32(tmp_6, tmp_7));

  int sum1 = _mm_extract_epi32(res, 0);
  int sum2 = _mm_extract_epi32(res, 1);
  int sumsq2 = _mm_extract_epi32(res, 2);
  int cross = _mm_extract_epi32(res, 3);

  int var2 = sumsq2 * MATCH_SZ_SQ - sum2 * sum2;
  int cov = cross * MATCH_SZ_SQ - sum1 * sum2;
  return cov / sqrt((double)var2);
}
