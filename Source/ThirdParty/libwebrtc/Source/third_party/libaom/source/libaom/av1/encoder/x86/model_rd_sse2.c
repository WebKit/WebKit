/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <emmintrin.h>

#include "config/av1_rtcd.h"

void av1_interp_cubic_rate_dist_sse2(const double *p1, const double *p2,
                                     double x, double rate_dist_f[2]) {
  const __m128d half = _mm_set1_pd(0.5);
  const __m128d two = _mm_set1_pd(2.0);
  const __m128d three = _mm_set1_pd(3.0);
  const __m128d four = _mm_set1_pd(4.0);
  const __m128d five = _mm_set1_pd(5.0);

  const __m128d reg_x = _mm_set1_pd(x);
  const __m128d reg_p0 = _mm_set_pd(p2[0], p1[0]);
  const __m128d reg_p1 = _mm_set_pd(p2[1], p1[1]);
  const __m128d reg_p2 = _mm_set_pd(p2[2], p1[2]);
  const __m128d reg_p3 = _mm_set_pd(p2[3], p1[3]);

  // To ensure that results are bit-identical to the C code, we need to perform
  // exactly the same sequence of operations here as in the C code.
  // reg_res_0 = x * (3.0 * (p[1] - p[2]) + p[3] - p[0])
  __m128d reg_res_0 = _mm_sub_pd(reg_p1, reg_p2);
  reg_res_0 = _mm_mul_pd(three, reg_res_0);
  reg_res_0 = _mm_add_pd(reg_res_0, reg_p3);
  reg_res_0 = _mm_sub_pd(reg_res_0, reg_p0);
  reg_res_0 = _mm_mul_pd(reg_x, reg_res_0);

  // reg_res_1 = 2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2]- p[3]
  const __m128d regp0_x_2 = _mm_mul_pd(two, reg_p0);
  const __m128d regp1_x_5 = _mm_mul_pd(five, reg_p1);
  const __m128d regp2_x_4 = _mm_mul_pd(four, reg_p2);
  __m128d reg_res_1 = _mm_sub_pd(regp0_x_2, regp1_x_5);
  reg_res_1 = _mm_add_pd(reg_res_1, regp2_x_4);
  reg_res_1 = _mm_sub_pd(reg_res_1, reg_p3);

  // reg_res_2 = x * (reg_res_1 + reg_res_0)
  __m128d reg_res_2 = _mm_add_pd(reg_res_1, reg_res_0);
  reg_res_2 = _mm_mul_pd(reg_x, reg_res_2);

  // reg_res_3 = p[2] - p[0] + reg_res_2
  __m128d reg_res_3 = _mm_sub_pd(reg_p2, reg_p0);
  reg_res_3 = _mm_add_pd(reg_res_3, reg_res_2);

  // reg_res_4 = p[1] + 0.5 * x * reg_res_3
  __m128d reg_res_4 = _mm_mul_pd(_mm_mul_pd(half, reg_x), reg_res_3);
  reg_res_4 = _mm_add_pd(reg_p1, reg_res_4);

  _mm_storeu_pd(rate_dist_f, reg_res_4);
}
