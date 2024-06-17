/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_ARM_PICKRST_SVE_H_
#define AOM_AV1_ENCODER_ARM_PICKRST_SVE_H_

#include <arm_neon.h>
#include <arm_sve.h>

#include "aom_dsp/arm/aom_neon_sve_bridge.h"

// Swap each half of the dgd vectors so that we can accumulate the result of
// the dot-products directly in the destination matrix.
static INLINE int16x8x2_t transpose_dgd(int16x8_t dgd0, int16x8_t dgd1) {
  int16x8_t dgd_trn0 = vreinterpretq_s16_s64(
      vzip1q_s64(vreinterpretq_s64_s16(dgd0), vreinterpretq_s64_s16(dgd1)));
  int16x8_t dgd_trn1 = vreinterpretq_s16_s64(
      vzip2q_s64(vreinterpretq_s64_s16(dgd0), vreinterpretq_s64_s16(dgd1)));

  return (struct int16x8x2_t){ dgd_trn0, dgd_trn1 };
}

static INLINE void compute_M_one_row_win5(int16x8_t src, int16x8_t dgd[5],
                                          int64_t *M, int row) {
  const int wiener_win = 5;

  int64x2_t m01 = vld1q_s64(M + row * wiener_win + 0);
  int16x8x2_t dgd01 = transpose_dgd(dgd[0], dgd[1]);

  int64x2_t cross_corr01 = aom_svdot_lane_s16(m01, dgd01.val[0], src, 0);
  cross_corr01 = aom_svdot_lane_s16(cross_corr01, dgd01.val[1], src, 1);
  vst1q_s64(M + row * wiener_win + 0, cross_corr01);

  int64x2_t m23 = vld1q_s64(M + row * wiener_win + 2);
  int16x8x2_t dgd23 = transpose_dgd(dgd[2], dgd[3]);

  int64x2_t cross_corr23 = aom_svdot_lane_s16(m23, dgd23.val[0], src, 0);
  cross_corr23 = aom_svdot_lane_s16(cross_corr23, dgd23.val[1], src, 1);
  vst1q_s64(M + row * wiener_win + 2, cross_corr23);

  int64x2_t m4 = aom_sdotq_s16(vdupq_n_s64(0), src, dgd[4]);
  M[row * wiener_win + 4] += vaddvq_s64(m4);
}

static INLINE void compute_M_one_row_win7(int16x8_t src, int16x8_t dgd[7],
                                          int64_t *M, int row) {
  const int wiener_win = 7;

  int64x2_t m01 = vld1q_s64(M + row * wiener_win + 0);
  int16x8x2_t dgd01 = transpose_dgd(dgd[0], dgd[1]);

  int64x2_t cross_corr01 = aom_svdot_lane_s16(m01, dgd01.val[0], src, 0);
  cross_corr01 = aom_svdot_lane_s16(cross_corr01, dgd01.val[1], src, 1);
  vst1q_s64(M + row * wiener_win + 0, cross_corr01);

  int64x2_t m23 = vld1q_s64(M + row * wiener_win + 2);
  int16x8x2_t dgd23 = transpose_dgd(dgd[2], dgd[3]);

  int64x2_t cross_corr23 = aom_svdot_lane_s16(m23, dgd23.val[0], src, 0);
  cross_corr23 = aom_svdot_lane_s16(cross_corr23, dgd23.val[1], src, 1);
  vst1q_s64(M + row * wiener_win + 2, cross_corr23);

  int64x2_t m45 = vld1q_s64(M + row * wiener_win + 4);
  int16x8x2_t dgd45 = transpose_dgd(dgd[4], dgd[5]);

  int64x2_t cross_corr45 = aom_svdot_lane_s16(m45, dgd45.val[0], src, 0);
  cross_corr45 = aom_svdot_lane_s16(cross_corr45, dgd45.val[1], src, 1);
  vst1q_s64(M + row * wiener_win + 4, cross_corr45);

  int64x2_t m6 = aom_sdotq_s16(vdupq_n_s64(0), src, dgd[6]);
  M[row * wiener_win + 6] += vaddvq_s64(m6);
}

static INLINE void compute_H_one_col(int16x8_t *dgd, int col, int64_t *H,
                                     const int wiener_win,
                                     const int wiener_win2) {
  for (int row0 = 0; row0 < wiener_win; row0++) {
    for (int row1 = row0; row1 < wiener_win; row1++) {
      int auto_cov_idx =
          (col * wiener_win + row0) * wiener_win2 + (col * wiener_win) + row1;

      int64x2_t auto_cov = aom_sdotq_s16(vdupq_n_s64(0), dgd[row0], dgd[row1]);
      H[auto_cov_idx] += vaddvq_s64(auto_cov);
    }
  }
}

static INLINE void compute_H_two_rows_win5(int16x8_t *dgd0, int16x8_t *dgd1,
                                           int row0, int row1, int64_t *H) {
  for (int col0 = 0; col0 < 5; col0++) {
    int auto_cov_idx = (row0 * 5 + col0) * 25 + (row1 * 5);

    int64x2_t h01 = vld1q_s64(H + auto_cov_idx);
    int16x8x2_t dgd01 = transpose_dgd(dgd1[0], dgd1[1]);

    int64x2_t auto_cov01 = aom_svdot_lane_s16(h01, dgd01.val[0], dgd0[col0], 0);
    auto_cov01 = aom_svdot_lane_s16(auto_cov01, dgd01.val[1], dgd0[col0], 1);
    vst1q_s64(H + auto_cov_idx, auto_cov01);

    int64x2_t h23 = vld1q_s64(H + auto_cov_idx + 2);
    int16x8x2_t dgd23 = transpose_dgd(dgd1[2], dgd1[3]);

    int64x2_t auto_cov23 = aom_svdot_lane_s16(h23, dgd23.val[0], dgd0[col0], 0);
    auto_cov23 = aom_svdot_lane_s16(auto_cov23, dgd23.val[1], dgd0[col0], 1);
    vst1q_s64(H + auto_cov_idx + 2, auto_cov23);

    int64x2_t auto_cov4 = aom_sdotq_s16(vdupq_n_s64(0), dgd0[col0], dgd1[4]);
    H[auto_cov_idx + 4] += vaddvq_s64(auto_cov4);
  }
}

static INLINE void compute_H_two_rows_win7(int16x8_t *dgd0, int16x8_t *dgd1,
                                           int row0, int row1, int64_t *H) {
  for (int col0 = 0; col0 < 7; col0++) {
    int auto_cov_idx = (row0 * 7 + col0) * 49 + (row1 * 7);

    int64x2_t h01 = vld1q_s64(H + auto_cov_idx);
    int16x8x2_t dgd01 = transpose_dgd(dgd1[0], dgd1[1]);

    int64x2_t auto_cov01 = aom_svdot_lane_s16(h01, dgd01.val[0], dgd0[col0], 0);
    auto_cov01 = aom_svdot_lane_s16(auto_cov01, dgd01.val[1], dgd0[col0], 1);
    vst1q_s64(H + auto_cov_idx, auto_cov01);

    int64x2_t h23 = vld1q_s64(H + auto_cov_idx + 2);
    int16x8x2_t dgd23 = transpose_dgd(dgd1[2], dgd1[3]);

    int64x2_t auto_cov23 = aom_svdot_lane_s16(h23, dgd23.val[0], dgd0[col0], 0);
    auto_cov23 = aom_svdot_lane_s16(auto_cov23, dgd23.val[1], dgd0[col0], 1);
    vst1q_s64(H + auto_cov_idx + 2, auto_cov23);

    int64x2_t h45 = vld1q_s64(H + auto_cov_idx + 4);
    int16x8x2_t dgd45 = transpose_dgd(dgd1[4], dgd1[5]);

    int64x2_t auto_cov45 = aom_svdot_lane_s16(h45, dgd45.val[0], dgd0[col0], 0);
    auto_cov45 = aom_svdot_lane_s16(auto_cov45, dgd45.val[1], dgd0[col0], 1);
    vst1q_s64(H + auto_cov_idx + 4, auto_cov45);

    int64x2_t auto_cov6 = aom_sdotq_s16(vdupq_n_s64(0), dgd0[col0], dgd1[6]);
    H[auto_cov_idx + 6] += vaddvq_s64(auto_cov6);
  }
}

#endif  // AOM_AV1_ENCODER_ARM_PICKRST_SVE_H_
