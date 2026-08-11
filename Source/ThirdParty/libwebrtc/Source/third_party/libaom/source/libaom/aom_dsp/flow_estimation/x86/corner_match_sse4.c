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

#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <assert.h>

#include <smmintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"
#include "aom_dsp/flow_estimation/corner_match.h"

DECLARE_ALIGNED(16, static const uint16_t, ones_array[8]) = { 1, 1, 1, 1,
                                                              1, 1, 1, 1 };

#if MATCH_SZ != 16
#error "Need to apply pixel mask in corner_match_sse4.c if MATCH_SZ != 16"
#endif

/* Compute mean and standard deviation of pixels in a window of size
   MATCH_SZ by MATCH_SZ centered at (x, y).
   Store results into *mean and *one_over_stddev

   Note: The output of this function is scaled by MATCH_SZ, as in
   *mean = MATCH_SZ * <true mean> and
   *one_over_stddev = 1 / (MATCH_SZ * <true stddev>)

   Combined with the fact that we return 1/stddev rather than the standard
   deviation itself, this allows us to completely avoid divisions in
   aom_compute_correlation, which is much hotter than this function is.

   Returns true if this feature point is usable, false otherwise.
*/
bool aom_compute_mean_stddev_sse4_1(const unsigned char *frame, int stride,
                                    int x, int y, double *mean,
                                    double *one_over_stddev) {
  // 8 16-bit partial sums of pixels
  // Each lane sums at most 2*MATCH_SZ pixels, which can have values up to 255,
  // and is therefore at most 2*MATCH_SZ*255, which is > 2^8 but < 2^16.
  // Thus this value is safe to store in 16 bits.
  __m128i sum_vec = _mm_setzero_si128();

  // 8 32-bit partial sums of squares
  __m128i sumsq_vec_l = _mm_setzero_si128();
  __m128i sumsq_vec_r = _mm_setzero_si128();

  frame += (y - MATCH_SZ_BY2) * stride + (x - MATCH_SZ_BY2);

  for (int i = 0; i < MATCH_SZ; ++i) {
    const __m128i v = _mm_loadu_si128((__m128i *)frame);
    const __m128i v_l = _mm_cvtepu8_epi16(v);
    const __m128i v_r = _mm_cvtepu8_epi16(_mm_srli_si128(v, 8));

    sum_vec = _mm_add_epi16(sum_vec, _mm_add_epi16(v_l, v_r));
    sumsq_vec_l = _mm_add_epi32(sumsq_vec_l, _mm_madd_epi16(v_l, v_l));
    sumsq_vec_r = _mm_add_epi32(sumsq_vec_r, _mm_madd_epi16(v_r, v_r));

    frame += stride;
  }

  // Reduce sum_vec and sumsq_vec into single values
  // Start by reducing each vector to 4x32-bit values, hadd() to perform four
  // additions, then perform the last two additions in scalar code.
  const __m128i ones = _mm_load_si128((__m128i *)ones_array);
  const __m128i partial_sum = _mm_madd_epi16(sum_vec, ones);
  const __m128i partial_sumsq = _mm_add_epi32(sumsq_vec_l, sumsq_vec_r);
  const __m128i tmp = _mm_hadd_epi32(partial_sum, partial_sumsq);
  const int sum = _mm_extract_epi32(tmp, 0) + _mm_extract_epi32(tmp, 1);
  const int sumsq = _mm_extract_epi32(tmp, 2) + _mm_extract_epi32(tmp, 3);

  *mean = (double)sum / MATCH_SZ;
  const double variance = sumsq - (*mean) * (*mean);
  if (variance < MIN_FEATURE_VARIANCE) {
    *one_over_stddev = 0.0;
    return false;
  }
  *one_over_stddev = 1.0 / sqrt(variance);
  return true;
}

/* Compute corr(frame1, frame2) over a window of size MATCH_SZ by MATCH_SZ.
   To save on computation, the mean and (1 divided by the) standard deviation
   of the window in each frame are precomputed and passed into this function
   as arguments.
*/
double aom_compute_correlation_sse4_1(const unsigned char *frame1, int stride1,
                                      int x1, int y1, double mean1,
                                      double one_over_stddev1,
                                      const unsigned char *frame2, int stride2,
                                      int x2, int y2, double mean2,
                                      double one_over_stddev2) {
  // 8 32-bit partial sums of products
  __m128i cross_vec_l = _mm_setzero_si128();
  __m128i cross_vec_r = _mm_setzero_si128();

  frame1 += (y1 - MATCH_SZ_BY2) * stride1 + (x1 - MATCH_SZ_BY2);
  frame2 += (y2 - MATCH_SZ_BY2) * stride2 + (x2 - MATCH_SZ_BY2);

  for (int i = 0; i < MATCH_SZ; ++i) {
    const __m128i v1 = _mm_loadu_si128((__m128i *)frame1);
    const __m128i v2 = _mm_loadu_si128((__m128i *)frame2);

    const __m128i v1_l = _mm_cvtepu8_epi16(v1);
    const __m128i v1_r = _mm_cvtepu8_epi16(_mm_srli_si128(v1, 8));
    const __m128i v2_l = _mm_cvtepu8_epi16(v2);
    const __m128i v2_r = _mm_cvtepu8_epi16(_mm_srli_si128(v2, 8));

    cross_vec_l = _mm_add_epi32(cross_vec_l, _mm_madd_epi16(v1_l, v2_l));
    cross_vec_r = _mm_add_epi32(cross_vec_r, _mm_madd_epi16(v1_r, v2_r));

    frame1 += stride1;
    frame2 += stride2;
  }

  // Sum cross_vec into a single value
  const __m128i tmp = _mm_add_epi32(cross_vec_l, cross_vec_r);
  const int cross = _mm_extract_epi32(tmp, 0) + _mm_extract_epi32(tmp, 1) +
                    _mm_extract_epi32(tmp, 2) + _mm_extract_epi32(tmp, 3);

  // Note: In theory, the calculations here "should" be
  //   covariance = cross / N^2 - mean1 * mean2
  //   correlation = covariance / (stddev1 * stddev2).
  //
  // However, because of the scaling in aom_compute_mean_stddev, the
  // lines below actually calculate
  //   covariance * N^2 = cross - (mean1 * N) * (mean2 * N)
  //   correlation = (covariance * N^2) / ((stddev1 * N) * (stddev2 * N))
  //
  // ie. we have removed the need for a division, and still end up with the
  // correct unscaled correlation (ie, in the range [-1, +1])
  const double covariance = cross - mean1 * mean2;
  const double correlation = covariance * (one_over_stddev1 * one_over_stddev2);
  return correlation;
}
