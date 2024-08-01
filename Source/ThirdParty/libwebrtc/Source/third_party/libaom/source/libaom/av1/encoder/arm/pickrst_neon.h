/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_AV1_ENCODER_ARM_PICKRST_NEON_H_
#define AOM_AV1_ENCODER_ARM_PICKRST_NEON_H_

#include <arm_neon.h>

#include "av1/common/restoration.h"

// Aligned sizes for Wiener filters.
#define WIENER_WIN2_ALIGN2 ALIGN_POWER_OF_TWO(WIENER_WIN2, 2)
#define WIENER_WIN2_ALIGN3 ALIGN_POWER_OF_TWO(WIENER_WIN2, 3)
#define WIENER_WIN2_REDUCED ((WIENER_WIN_REDUCED) * (WIENER_WIN_REDUCED))
#define WIENER_WIN2_REDUCED_ALIGN2 ALIGN_POWER_OF_TWO(WIENER_WIN2_REDUCED, 2)
#define WIENER_WIN2_REDUCED_ALIGN3 ALIGN_POWER_OF_TWO(WIENER_WIN2_REDUCED, 3)

// Compute 8 values of M (cross correlation) for a single source pixel and
// accumulate.
static INLINE void update_M_1pixel(int32_t *M_s32, int16x4_t src_avg,
                                   int16x8_t dgd_avg) {
  int32x4_t lo = vld1q_s32(M_s32 + 0);
  int32x4_t hi = vld1q_s32(M_s32 + 4);

  lo = vmlal_s16(lo, vget_low_s16(dgd_avg), src_avg);
  hi = vmlal_s16(hi, vget_high_s16(dgd_avg), src_avg);

  vst1q_s32(M_s32 + 0, lo);
  vst1q_s32(M_s32 + 4, hi);
}

// Compute 8 values of M (cross correlation) for two source pixels and
// accumulate.
static INLINE void update_M_2pixels(int32_t *M_s32, int16x4_t src_avg0,
                                    int16x4_t src_avg1, int16x8_t dgd_avg0,
                                    int16x8_t dgd_avg1) {
  int32x4_t lo = vld1q_s32(M_s32 + 0);
  int32x4_t hi = vld1q_s32(M_s32 + 4);

  lo = vmlal_s16(lo, vget_low_s16(dgd_avg0), src_avg0);
  hi = vmlal_s16(hi, vget_high_s16(dgd_avg0), src_avg0);
  lo = vmlal_s16(lo, vget_low_s16(dgd_avg1), src_avg1);
  hi = vmlal_s16(hi, vget_high_s16(dgd_avg1), src_avg1);

  vst1q_s32(M_s32 + 0, lo);
  vst1q_s32(M_s32 + 4, hi);
}

static INLINE void update_H_1pixel(int32_t *H_s32, const int16_t *dgd_avg,
                                   int width, int height) {
  for (int i = 0; i < height; i += 4) {
    int16x4_t di = vld1_s16(dgd_avg + i);

    for (int j = i; j < width; j += 4) {
      int16x4_t dj = vld1_s16(dgd_avg + j);
      int32x4_t h0 = vld1q_s32(H_s32 + 0 * width + j);
      int32x4_t h1 = vld1q_s32(H_s32 + 1 * width + j);
      int32x4_t h2 = vld1q_s32(H_s32 + 2 * width + j);
      int32x4_t h3 = vld1q_s32(H_s32 + 3 * width + j);

      h0 = vmlal_lane_s16(h0, dj, di, 0);
      h1 = vmlal_lane_s16(h1, dj, di, 1);
      h2 = vmlal_lane_s16(h2, dj, di, 2);
      h3 = vmlal_lane_s16(h3, dj, di, 3);

      vst1q_s32(H_s32 + 0 * width + j, h0);
      vst1q_s32(H_s32 + 1 * width + j, h1);
      vst1q_s32(H_s32 + 2 * width + j, h2);
      vst1q_s32(H_s32 + 3 * width + j, h3);
    }
    H_s32 += 4 * width;
  }
}

static INLINE void update_H_5x5_2pixels(int32_t *H_s32, const int16_t *dgd_avg0,
                                        const int16_t *dgd_avg1) {
  for (int i = 0; i < 24; i += 4) {
    int16x4_t di0 = vld1_s16(dgd_avg0 + i);
    int16x4_t di1 = vld1_s16(dgd_avg1 + i);

    for (int j = i + 0; j < WIENER_WIN2_REDUCED_ALIGN2; j += 4) {
      int16x4_t dj0 = vld1_s16(dgd_avg0 + j);
      int16x4_t dj1 = vld1_s16(dgd_avg1 + j);
      int32x4_t h0 = vld1q_s32(H_s32 + 0 * WIENER_WIN2_REDUCED_ALIGN2 + j);
      int32x4_t h1 = vld1q_s32(H_s32 + 1 * WIENER_WIN2_REDUCED_ALIGN2 + j);
      int32x4_t h2 = vld1q_s32(H_s32 + 2 * WIENER_WIN2_REDUCED_ALIGN2 + j);
      int32x4_t h3 = vld1q_s32(H_s32 + 3 * WIENER_WIN2_REDUCED_ALIGN2 + j);

      h0 = vmlal_lane_s16(h0, dj0, di0, 0);
      h0 = vmlal_lane_s16(h0, dj1, di1, 0);
      h1 = vmlal_lane_s16(h1, dj0, di0, 1);
      h1 = vmlal_lane_s16(h1, dj1, di1, 1);
      h2 = vmlal_lane_s16(h2, dj0, di0, 2);
      h2 = vmlal_lane_s16(h2, dj1, di1, 2);
      h3 = vmlal_lane_s16(h3, dj0, di0, 3);
      h3 = vmlal_lane_s16(h3, dj1, di1, 3);

      vst1q_s32(H_s32 + 0 * WIENER_WIN2_REDUCED_ALIGN2 + j, h0);
      vst1q_s32(H_s32 + 1 * WIENER_WIN2_REDUCED_ALIGN2 + j, h1);
      vst1q_s32(H_s32 + 2 * WIENER_WIN2_REDUCED_ALIGN2 + j, h2);
      vst1q_s32(H_s32 + 3 * WIENER_WIN2_REDUCED_ALIGN2 + j, h3);
    }
    H_s32 += 4 * WIENER_WIN2_REDUCED_ALIGN2;
  }
}

static INLINE void update_H_7x7_2pixels(int32_t *H_s32, const int16_t *dgd_avg0,
                                        const int16_t *dgd_avg1) {
  for (int i = 0; i < 48; i += 4) {
    int16x4_t di0 = vld1_s16(dgd_avg0 + i);
    int16x4_t di1 = vld1_s16(dgd_avg1 + i);

    int32x4_t h0 = vld1q_s32(H_s32 + 0 * WIENER_WIN2_ALIGN2 + i);
    int32x4_t h1 = vld1q_s32(H_s32 + 1 * WIENER_WIN2_ALIGN2 + i);
    int32x4_t h2 = vld1q_s32(H_s32 + 2 * WIENER_WIN2_ALIGN2 + i);
    int32x4_t h3 = vld1q_s32(H_s32 + 3 * WIENER_WIN2_ALIGN2 + i);

    h0 = vmlal_lane_s16(h0, di0, di0, 0);
    h0 = vmlal_lane_s16(h0, di1, di1, 0);
    h1 = vmlal_lane_s16(h1, di0, di0, 1);
    h1 = vmlal_lane_s16(h1, di1, di1, 1);
    h2 = vmlal_lane_s16(h2, di0, di0, 2);
    h2 = vmlal_lane_s16(h2, di1, di1, 2);
    h3 = vmlal_lane_s16(h3, di0, di0, 3);
    h3 = vmlal_lane_s16(h3, di1, di1, 3);

    vst1q_s32(H_s32 + 0 * WIENER_WIN2_ALIGN2 + i, h0);
    vst1q_s32(H_s32 + 1 * WIENER_WIN2_ALIGN2 + i, h1);
    vst1q_s32(H_s32 + 2 * WIENER_WIN2_ALIGN2 + i, h2);
    vst1q_s32(H_s32 + 3 * WIENER_WIN2_ALIGN2 + i, h3);

    for (int j = i + 4; j < WIENER_WIN2_ALIGN2; j += 4) {
      int16x4_t dj0 = vld1_s16(dgd_avg0 + j);
      int16x4_t dj1 = vld1_s16(dgd_avg1 + j);
      h0 = vld1q_s32(H_s32 + 0 * WIENER_WIN2_ALIGN2 + j);
      h1 = vld1q_s32(H_s32 + 1 * WIENER_WIN2_ALIGN2 + j);
      h2 = vld1q_s32(H_s32 + 2 * WIENER_WIN2_ALIGN2 + j);
      h3 = vld1q_s32(H_s32 + 3 * WIENER_WIN2_ALIGN2 + j);

      h0 = vmlal_lane_s16(h0, dj0, di0, 0);
      h0 = vmlal_lane_s16(h0, dj1, di1, 0);
      h1 = vmlal_lane_s16(h1, dj0, di0, 1);
      h1 = vmlal_lane_s16(h1, dj1, di1, 1);
      h2 = vmlal_lane_s16(h2, dj0, di0, 2);
      h2 = vmlal_lane_s16(h2, dj1, di1, 2);
      h3 = vmlal_lane_s16(h3, dj0, di0, 3);
      h3 = vmlal_lane_s16(h3, dj1, di1, 3);

      vst1q_s32(H_s32 + 0 * WIENER_WIN2_ALIGN2 + j, h0);
      vst1q_s32(H_s32 + 1 * WIENER_WIN2_ALIGN2 + j, h1);
      vst1q_s32(H_s32 + 2 * WIENER_WIN2_ALIGN2 + j, h2);
      vst1q_s32(H_s32 + 3 * WIENER_WIN2_ALIGN2 + j, h3);
    }
    H_s32 += 4 * WIENER_WIN2_ALIGN2;
  }
}

// Widen 32-bit src data and accumulate into 64-bit dst. Clear src data.
static INLINE void accumulate_and_clear(int64_t *dst, int32_t *src,
                                        int length) {
  do {
    int32x4_t s32 = vld1q_s32(src);
    vst1q_s32(src, vdupq_n_s32(0));
    src += 4;

    int64x2_t d_lo = vld1q_s64(dst + 0);
    int64x2_t d_hi = vld1q_s64(dst + 2);

    d_lo = vaddw_s32(d_lo, vget_low_s32(s32));
    d_hi = vaddw_s32(d_hi, vget_high_s32(s32));

    vst1q_s64(dst + 0, d_lo);
    vst1q_s64(dst + 2, d_hi);

    dst += 4;
    length -= 4;
  } while (length > 0);
}

#endif  // AOM_AV1_ENCODER_ARM_PICKRST_NEON_H_
