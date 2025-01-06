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

#include <arm_neon.h>
#include <assert.h>
#include <stdbool.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/scale.h"
#include "av1/common/warped_motion.h"
#include "config/av1_rtcd.h"
#include "highbd_warp_plane_neon.h"

static AOM_FORCE_INLINE int16x8_t
highbd_horizontal_filter_4x1_f4(int16x8_t rv0, int16x8_t rv1, int16x8_t rv2,
                                int16x8_t rv3, int bd, int sx, int alpha) {
  int16x8_t f[4];
  load_filters_4(f, sx, alpha);

  int32x4_t m0 = vmull_s16(vget_low_s16(f[0]), vget_low_s16(rv0));
  m0 = vmlal_s16(m0, vget_high_s16(f[0]), vget_high_s16(rv0));
  int32x4_t m1 = vmull_s16(vget_low_s16(f[1]), vget_low_s16(rv1));
  m1 = vmlal_s16(m1, vget_high_s16(f[1]), vget_high_s16(rv1));
  int32x4_t m2 = vmull_s16(vget_low_s16(f[2]), vget_low_s16(rv2));
  m2 = vmlal_s16(m2, vget_high_s16(f[2]), vget_high_s16(rv2));
  int32x4_t m3 = vmull_s16(vget_low_s16(f[3]), vget_low_s16(rv3));
  m3 = vmlal_s16(m3, vget_high_s16(f[3]), vget_high_s16(rv3));

  int32x4_t m0123[] = { m0, m1, m2, m3 };

  const int round0 = (bd == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;

  int32x4_t res = horizontal_add_4d_s32x4(m0123);
  res = vaddq_s32(res, vdupq_n_s32(1 << offset_bits_horiz));
  res = vrshlq_s32(res, vdupq_n_s32(-round0));
  return vcombine_s16(vmovn_s32(res), vdup_n_s16(0));
}

static AOM_FORCE_INLINE int16x8_t highbd_horizontal_filter_8x1_f8(
    int16x8_t rv0, int16x8_t rv1, int16x8_t rv2, int16x8_t rv3, int16x8_t rv4,
    int16x8_t rv5, int16x8_t rv6, int16x8_t rv7, int bd, int sx, int alpha) {
  int16x8_t f[8];
  load_filters_8(f, sx, alpha);

  int32x4_t m0 = vmull_s16(vget_low_s16(f[0]), vget_low_s16(rv0));
  m0 = vmlal_s16(m0, vget_high_s16(f[0]), vget_high_s16(rv0));
  int32x4_t m1 = vmull_s16(vget_low_s16(f[1]), vget_low_s16(rv1));
  m1 = vmlal_s16(m1, vget_high_s16(f[1]), vget_high_s16(rv1));
  int32x4_t m2 = vmull_s16(vget_low_s16(f[2]), vget_low_s16(rv2));
  m2 = vmlal_s16(m2, vget_high_s16(f[2]), vget_high_s16(rv2));
  int32x4_t m3 = vmull_s16(vget_low_s16(f[3]), vget_low_s16(rv3));
  m3 = vmlal_s16(m3, vget_high_s16(f[3]), vget_high_s16(rv3));
  int32x4_t m4 = vmull_s16(vget_low_s16(f[4]), vget_low_s16(rv4));
  m4 = vmlal_s16(m4, vget_high_s16(f[4]), vget_high_s16(rv4));
  int32x4_t m5 = vmull_s16(vget_low_s16(f[5]), vget_low_s16(rv5));
  m5 = vmlal_s16(m5, vget_high_s16(f[5]), vget_high_s16(rv5));
  int32x4_t m6 = vmull_s16(vget_low_s16(f[6]), vget_low_s16(rv6));
  m6 = vmlal_s16(m6, vget_high_s16(f[6]), vget_high_s16(rv6));
  int32x4_t m7 = vmull_s16(vget_low_s16(f[7]), vget_low_s16(rv7));
  m7 = vmlal_s16(m7, vget_high_s16(f[7]), vget_high_s16(rv7));

  int32x4_t m0123[] = { m0, m1, m2, m3 };
  int32x4_t m4567[] = { m4, m5, m6, m7 };

  const int round0 = (bd == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;

  int32x4_t res0 = horizontal_add_4d_s32x4(m0123);
  int32x4_t res1 = horizontal_add_4d_s32x4(m4567);
  res0 = vaddq_s32(res0, vdupq_n_s32(1 << offset_bits_horiz));
  res1 = vaddq_s32(res1, vdupq_n_s32(1 << offset_bits_horiz));
  res0 = vrshlq_s32(res0, vdupq_n_s32(-round0));
  res1 = vrshlq_s32(res1, vdupq_n_s32(-round0));
  return vcombine_s16(vmovn_s32(res0), vmovn_s32(res1));
}

static AOM_FORCE_INLINE int16x8_t
highbd_horizontal_filter_4x1_f1(int16x8_t rv0, int16x8_t rv1, int16x8_t rv2,
                                int16x8_t rv3, int bd, int sx) {
  int16x8_t f = load_filters_1(sx);

  int32x4_t m0 = vmull_s16(vget_low_s16(f), vget_low_s16(rv0));
  m0 = vmlal_s16(m0, vget_high_s16(f), vget_high_s16(rv0));
  int32x4_t m1 = vmull_s16(vget_low_s16(f), vget_low_s16(rv1));
  m1 = vmlal_s16(m1, vget_high_s16(f), vget_high_s16(rv1));
  int32x4_t m2 = vmull_s16(vget_low_s16(f), vget_low_s16(rv2));
  m2 = vmlal_s16(m2, vget_high_s16(f), vget_high_s16(rv2));
  int32x4_t m3 = vmull_s16(vget_low_s16(f), vget_low_s16(rv3));
  m3 = vmlal_s16(m3, vget_high_s16(f), vget_high_s16(rv3));

  int32x4_t m0123[] = { m0, m1, m2, m3 };

  const int round0 = (bd == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;

  int32x4_t res = horizontal_add_4d_s32x4(m0123);
  res = vaddq_s32(res, vdupq_n_s32(1 << offset_bits_horiz));
  res = vrshlq_s32(res, vdupq_n_s32(-round0));
  return vcombine_s16(vmovn_s32(res), vdup_n_s16(0));
}

static AOM_FORCE_INLINE int16x8_t highbd_horizontal_filter_8x1_f1(
    int16x8_t rv0, int16x8_t rv1, int16x8_t rv2, int16x8_t rv3, int16x8_t rv4,
    int16x8_t rv5, int16x8_t rv6, int16x8_t rv7, int bd, int sx) {
  int16x8_t f = load_filters_1(sx);

  int32x4_t m0 = vmull_s16(vget_low_s16(f), vget_low_s16(rv0));
  m0 = vmlal_s16(m0, vget_high_s16(f), vget_high_s16(rv0));
  int32x4_t m1 = vmull_s16(vget_low_s16(f), vget_low_s16(rv1));
  m1 = vmlal_s16(m1, vget_high_s16(f), vget_high_s16(rv1));
  int32x4_t m2 = vmull_s16(vget_low_s16(f), vget_low_s16(rv2));
  m2 = vmlal_s16(m2, vget_high_s16(f), vget_high_s16(rv2));
  int32x4_t m3 = vmull_s16(vget_low_s16(f), vget_low_s16(rv3));
  m3 = vmlal_s16(m3, vget_high_s16(f), vget_high_s16(rv3));
  int32x4_t m4 = vmull_s16(vget_low_s16(f), vget_low_s16(rv4));
  m4 = vmlal_s16(m4, vget_high_s16(f), vget_high_s16(rv4));
  int32x4_t m5 = vmull_s16(vget_low_s16(f), vget_low_s16(rv5));
  m5 = vmlal_s16(m5, vget_high_s16(f), vget_high_s16(rv5));
  int32x4_t m6 = vmull_s16(vget_low_s16(f), vget_low_s16(rv6));
  m6 = vmlal_s16(m6, vget_high_s16(f), vget_high_s16(rv6));
  int32x4_t m7 = vmull_s16(vget_low_s16(f), vget_low_s16(rv7));
  m7 = vmlal_s16(m7, vget_high_s16(f), vget_high_s16(rv7));

  int32x4_t m0123[] = { m0, m1, m2, m3 };
  int32x4_t m4567[] = { m4, m5, m6, m7 };

  const int round0 = (bd == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;

  int32x4_t res0 = horizontal_add_4d_s32x4(m0123);
  int32x4_t res1 = horizontal_add_4d_s32x4(m4567);
  res0 = vaddq_s32(res0, vdupq_n_s32(1 << offset_bits_horiz));
  res1 = vaddq_s32(res1, vdupq_n_s32(1 << offset_bits_horiz));
  res0 = vrshlq_s32(res0, vdupq_n_s32(-round0));
  res1 = vrshlq_s32(res1, vdupq_n_s32(-round0));
  return vcombine_s16(vmovn_s32(res0), vmovn_s32(res1));
}

static AOM_FORCE_INLINE int32x4_t vertical_filter_4x1_f1(const int16x8_t *tmp,
                                                         int sy) {
  const int16x8_t f = load_filters_1(sy);
  const int16x4_t f0123 = vget_low_s16(f);
  const int16x4_t f4567 = vget_high_s16(f);

  int32x4_t m0123 = vmull_lane_s16(vget_low_s16(tmp[0]), f0123, 0);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[1]), f0123, 1);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[2]), f0123, 2);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[3]), f0123, 3);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[4]), f4567, 0);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[5]), f4567, 1);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[6]), f4567, 2);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[7]), f4567, 3);
  return m0123;
}

static AOM_FORCE_INLINE int32x4x2_t vertical_filter_8x1_f1(const int16x8_t *tmp,
                                                           int sy) {
  const int16x8_t f = load_filters_1(sy);
  const int16x4_t f0123 = vget_low_s16(f);
  const int16x4_t f4567 = vget_high_s16(f);

  int32x4_t m0123 = vmull_lane_s16(vget_low_s16(tmp[0]), f0123, 0);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[1]), f0123, 1);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[2]), f0123, 2);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[3]), f0123, 3);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[4]), f4567, 0);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[5]), f4567, 1);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[6]), f4567, 2);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(tmp[7]), f4567, 3);

  int32x4_t m4567 = vmull_lane_s16(vget_high_s16(tmp[0]), f0123, 0);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(tmp[1]), f0123, 1);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(tmp[2]), f0123, 2);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(tmp[3]), f0123, 3);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(tmp[4]), f4567, 0);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(tmp[5]), f4567, 1);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(tmp[6]), f4567, 2);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(tmp[7]), f4567, 3);
  return (int32x4x2_t){ { m0123, m4567 } };
}

static AOM_FORCE_INLINE int32x4_t vertical_filter_4x1_f4(const int16x8_t *tmp,
                                                         int sy, int gamma) {
  int16x8_t s0, s1, s2, s3;
  transpose_elems_s16_4x8(
      vget_low_s16(tmp[0]), vget_low_s16(tmp[1]), vget_low_s16(tmp[2]),
      vget_low_s16(tmp[3]), vget_low_s16(tmp[4]), vget_low_s16(tmp[5]),
      vget_low_s16(tmp[6]), vget_low_s16(tmp[7]), &s0, &s1, &s2, &s3);

  int16x8_t f[4];
  load_filters_4(f, sy, gamma);

  int32x4_t m0 = vmull_s16(vget_low_s16(s0), vget_low_s16(f[0]));
  m0 = vmlal_s16(m0, vget_high_s16(s0), vget_high_s16(f[0]));
  int32x4_t m1 = vmull_s16(vget_low_s16(s1), vget_low_s16(f[1]));
  m1 = vmlal_s16(m1, vget_high_s16(s1), vget_high_s16(f[1]));
  int32x4_t m2 = vmull_s16(vget_low_s16(s2), vget_low_s16(f[2]));
  m2 = vmlal_s16(m2, vget_high_s16(s2), vget_high_s16(f[2]));
  int32x4_t m3 = vmull_s16(vget_low_s16(s3), vget_low_s16(f[3]));
  m3 = vmlal_s16(m3, vget_high_s16(s3), vget_high_s16(f[3]));

  int32x4_t m0123[] = { m0, m1, m2, m3 };
  return horizontal_add_4d_s32x4(m0123);
}

static AOM_FORCE_INLINE int32x4x2_t vertical_filter_8x1_f8(const int16x8_t *tmp,
                                                           int sy, int gamma) {
  int16x8_t s0 = tmp[0];
  int16x8_t s1 = tmp[1];
  int16x8_t s2 = tmp[2];
  int16x8_t s3 = tmp[3];
  int16x8_t s4 = tmp[4];
  int16x8_t s5 = tmp[5];
  int16x8_t s6 = tmp[6];
  int16x8_t s7 = tmp[7];
  transpose_elems_inplace_s16_8x8(&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

  int16x8_t f[8];
  load_filters_8(f, sy, gamma);

  int32x4_t m0 = vmull_s16(vget_low_s16(s0), vget_low_s16(f[0]));
  m0 = vmlal_s16(m0, vget_high_s16(s0), vget_high_s16(f[0]));
  int32x4_t m1 = vmull_s16(vget_low_s16(s1), vget_low_s16(f[1]));
  m1 = vmlal_s16(m1, vget_high_s16(s1), vget_high_s16(f[1]));
  int32x4_t m2 = vmull_s16(vget_low_s16(s2), vget_low_s16(f[2]));
  m2 = vmlal_s16(m2, vget_high_s16(s2), vget_high_s16(f[2]));
  int32x4_t m3 = vmull_s16(vget_low_s16(s3), vget_low_s16(f[3]));
  m3 = vmlal_s16(m3, vget_high_s16(s3), vget_high_s16(f[3]));
  int32x4_t m4 = vmull_s16(vget_low_s16(s4), vget_low_s16(f[4]));
  m4 = vmlal_s16(m4, vget_high_s16(s4), vget_high_s16(f[4]));
  int32x4_t m5 = vmull_s16(vget_low_s16(s5), vget_low_s16(f[5]));
  m5 = vmlal_s16(m5, vget_high_s16(s5), vget_high_s16(f[5]));
  int32x4_t m6 = vmull_s16(vget_low_s16(s6), vget_low_s16(f[6]));
  m6 = vmlal_s16(m6, vget_high_s16(s6), vget_high_s16(f[6]));
  int32x4_t m7 = vmull_s16(vget_low_s16(s7), vget_low_s16(f[7]));
  m7 = vmlal_s16(m7, vget_high_s16(s7), vget_high_s16(f[7]));

  int32x4_t m0123[] = { m0, m1, m2, m3 };
  int32x4_t m4567[] = { m4, m5, m6, m7 };

  int32x4x2_t ret;
  ret.val[0] = horizontal_add_4d_s32x4(m0123);
  ret.val[1] = horizontal_add_4d_s32x4(m4567);
  return ret;
}

void av1_highbd_warp_affine_neon(const int32_t *mat, const uint16_t *ref,
                                 int width, int height, int stride,
                                 uint16_t *pred, int p_col, int p_row,
                                 int p_width, int p_height, int p_stride,
                                 int subsampling_x, int subsampling_y, int bd,
                                 ConvolveParams *conv_params, int16_t alpha,
                                 int16_t beta, int16_t gamma, int16_t delta) {
  highbd_warp_affine_common(mat, ref, width, height, stride, pred, p_col, p_row,
                            p_width, p_height, p_stride, subsampling_x,
                            subsampling_y, bd, conv_params, alpha, beta, gamma,
                            delta);
}
