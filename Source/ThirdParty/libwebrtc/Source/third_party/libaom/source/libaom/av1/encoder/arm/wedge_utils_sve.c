/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "aom_dsp/arm/sum_neon.h"
#include "av1/common/reconinter.h"

uint64_t av1_wedge_sse_from_residuals_sve(const int16_t *r1, const int16_t *d,
                                          const uint8_t *m, int N) {
  assert(N % 64 == 0);

  // Predicate pattern with first 8 elements true.
  const svbool_t pattern = svptrue_pat_b16(SV_VL8);
  int64x2_t sse[2] = { vdupq_n_s64(0), vdupq_n_s64(0) };

  int i = 0;
  do {
    int32x4_t sum[4];
    int16x8_t sum_s16[2];

    const int16x8_t r1_l = vld1q_s16(r1 + i);
    const int16x8_t r1_h = vld1q_s16(r1 + i + 8);
    const int16x8_t d_l = vld1q_s16(d + i);
    const int16x8_t d_h = vld1q_s16(d + i + 8);

    // Use a zero-extending load to widen the vector elements.
    const int16x8_t m_l = svget_neonq_s16(svld1ub_s16(pattern, m + i));
    const int16x8_t m_h = svget_neonq_s16(svld1ub_s16(pattern, m + i + 8));

    sum[0] = vshll_n_s16(vget_low_s16(r1_l), WEDGE_WEIGHT_BITS);
    sum[1] = vshll_n_s16(vget_high_s16(r1_l), WEDGE_WEIGHT_BITS);
    sum[2] = vshll_n_s16(vget_low_s16(r1_h), WEDGE_WEIGHT_BITS);
    sum[3] = vshll_n_s16(vget_high_s16(r1_h), WEDGE_WEIGHT_BITS);

    sum[0] = vmlal_s16(sum[0], vget_low_s16(m_l), vget_low_s16(d_l));
    sum[1] = vmlal_s16(sum[1], vget_high_s16(m_l), vget_high_s16(d_l));
    sum[2] = vmlal_s16(sum[2], vget_low_s16(m_h), vget_low_s16(d_h));
    sum[3] = vmlal_s16(sum[3], vget_high_s16(m_h), vget_high_s16(d_h));

    sum_s16[0] = vcombine_s16(vqmovn_s32(sum[0]), vqmovn_s32(sum[1]));
    sum_s16[1] = vcombine_s16(vqmovn_s32(sum[2]), vqmovn_s32(sum[3]));

    sse[0] = aom_sdotq_s16(sse[0], sum_s16[0], sum_s16[0]);
    sse[1] = aom_sdotq_s16(sse[1], sum_s16[1], sum_s16[1]);

    i += 16;
  } while (i < N);

  const uint64_t csse =
      (uint64_t)horizontal_add_s64x2(vaddq_s64(sse[0], sse[1]));
  return ROUND_POWER_OF_TWO(csse, 2 * WEDGE_WEIGHT_BITS);
}

int8_t av1_wedge_sign_from_residuals_sve(const int16_t *ds, const uint8_t *m,
                                         int N, int64_t limit) {
  assert(N % 16 == 0);

  // Predicate pattern with first 8 elements true.
  svbool_t pattern = svptrue_pat_b16(SV_VL8);
  int64x2_t acc_l = vdupq_n_s64(0);
  int64x2_t acc_h = vdupq_n_s64(0);

  do {
    const int16x8_t ds_l = vld1q_s16(ds);
    const int16x8_t ds_h = vld1q_s16(ds + 8);

    // Use a zero-extending load to widen the vector elements.
    const int16x8_t m_l = svget_neonq_s16(svld1ub_s16(pattern, m));
    const int16x8_t m_h = svget_neonq_s16(svld1ub_s16(pattern, m + 8));

    acc_l = aom_sdotq_s16(acc_l, ds_l, m_l);
    acc_h = aom_sdotq_s16(acc_h, ds_h, m_h);

    ds += 16;
    m += 16;
    N -= 16;
  } while (N != 0);

  const int64x2_t sum = vaddq_s64(acc_l, acc_h);
  return horizontal_add_s64x2(sum) > limit;
}
