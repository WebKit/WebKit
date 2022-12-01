/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>
#include <stdio.h>
#include "aom/aom_integer.h"
#include "av1/common/common.h"

int64_t av1_highbd_block_error_avx2(const tran_low_t *coeff,
                                    const tran_low_t *dqcoeff,
                                    intptr_t block_size, int64_t *ssz,
                                    int bps) {
  int i;
  int64_t temp1[8];
  int64_t error = 0, sqcoeff = 0;
  const int shift = 2 * (bps - 8);
  const int rounding = shift > 0 ? 1 << (shift - 1) : 0;

  for (i = 0; i < block_size; i += 16) {
    __m256i mm256_coeff = _mm256_loadu_si256((__m256i *)(coeff + i));
    __m256i mm256_coeff2 = _mm256_loadu_si256((__m256i *)(coeff + i + 8));
    __m256i mm256_dqcoeff = _mm256_loadu_si256((__m256i *)(dqcoeff + i));
    __m256i mm256_dqcoeff2 = _mm256_loadu_si256((__m256i *)(dqcoeff + i + 8));

    __m256i diff1 = _mm256_sub_epi32(mm256_coeff, mm256_dqcoeff);
    __m256i diff2 = _mm256_sub_epi32(mm256_coeff2, mm256_dqcoeff2);
    __m256i diff1h = _mm256_srli_epi64(diff1, 32);
    __m256i diff2h = _mm256_srli_epi64(diff2, 32);
    __m256i res = _mm256_mul_epi32(diff1, diff1);
    __m256i res1 = _mm256_mul_epi32(diff1h, diff1h);
    __m256i res2 = _mm256_mul_epi32(diff2, diff2);
    __m256i res3 = _mm256_mul_epi32(diff2h, diff2h);
    __m256i res_diff = _mm256_add_epi64(_mm256_add_epi64(res, res1),
                                        _mm256_add_epi64(res2, res3));
    __m256i mm256_coeffh = _mm256_srli_epi64(mm256_coeff, 32);
    __m256i mm256_coeffh2 = _mm256_srli_epi64(mm256_coeff2, 32);
    res = _mm256_mul_epi32(mm256_coeff, mm256_coeff);
    res1 = _mm256_mul_epi32(mm256_coeffh, mm256_coeffh);
    res2 = _mm256_mul_epi32(mm256_coeff2, mm256_coeff2);
    res3 = _mm256_mul_epi32(mm256_coeffh2, mm256_coeffh2);
    __m256i res_sqcoeff = _mm256_add_epi64(_mm256_add_epi64(res, res1),
                                           _mm256_add_epi64(res2, res3));
    _mm256_storeu_si256((__m256i *)temp1, res_diff);
    _mm256_storeu_si256((__m256i *)temp1 + 1, res_sqcoeff);

    error += temp1[0] + temp1[1] + temp1[2] + temp1[3];
    sqcoeff += temp1[4] + temp1[5] + temp1[6] + temp1[7];
  }
  assert(error >= 0 && sqcoeff >= 0);
  error = (error + rounding) >> shift;
  sqcoeff = (sqcoeff + rounding) >> shift;

  *ssz = sqcoeff;
  return error;
}
