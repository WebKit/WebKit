/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
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

static INLINE int16x8_t load_filters_1(int ofs) {
  const int ofs0 = ROUND_POWER_OF_TWO(ofs, WARPEDDIFF_PREC_BITS);

  const int16_t *base =
      (int16_t *)av1_warped_filter + WARPEDPIXEL_PREC_SHIFTS * 8;
  return vld1q_s16(base + ofs0 * 8);
}

static INLINE void load_filters_4(int16x8_t out[], int ofs, int stride) {
  const int ofs0 = ROUND_POWER_OF_TWO(ofs + stride * 0, WARPEDDIFF_PREC_BITS);
  const int ofs1 = ROUND_POWER_OF_TWO(ofs + stride * 1, WARPEDDIFF_PREC_BITS);
  const int ofs2 = ROUND_POWER_OF_TWO(ofs + stride * 2, WARPEDDIFF_PREC_BITS);
  const int ofs3 = ROUND_POWER_OF_TWO(ofs + stride * 3, WARPEDDIFF_PREC_BITS);

  const int16_t *base =
      (int16_t *)av1_warped_filter + WARPEDPIXEL_PREC_SHIFTS * 8;
  out[0] = vld1q_s16(base + ofs0 * 8);
  out[1] = vld1q_s16(base + ofs1 * 8);
  out[2] = vld1q_s16(base + ofs2 * 8);
  out[3] = vld1q_s16(base + ofs3 * 8);
}

static INLINE void load_filters_8(int16x8_t out[], int ofs, int stride) {
  const int ofs0 = ROUND_POWER_OF_TWO(ofs + stride * 0, WARPEDDIFF_PREC_BITS);
  const int ofs1 = ROUND_POWER_OF_TWO(ofs + stride * 1, WARPEDDIFF_PREC_BITS);
  const int ofs2 = ROUND_POWER_OF_TWO(ofs + stride * 2, WARPEDDIFF_PREC_BITS);
  const int ofs3 = ROUND_POWER_OF_TWO(ofs + stride * 3, WARPEDDIFF_PREC_BITS);
  const int ofs4 = ROUND_POWER_OF_TWO(ofs + stride * 4, WARPEDDIFF_PREC_BITS);
  const int ofs5 = ROUND_POWER_OF_TWO(ofs + stride * 5, WARPEDDIFF_PREC_BITS);
  const int ofs6 = ROUND_POWER_OF_TWO(ofs + stride * 6, WARPEDDIFF_PREC_BITS);
  const int ofs7 = ROUND_POWER_OF_TWO(ofs + stride * 7, WARPEDDIFF_PREC_BITS);

  const int16_t *base =
      (int16_t *)av1_warped_filter + WARPEDPIXEL_PREC_SHIFTS * 8;
  out[0] = vld1q_s16(base + ofs0 * 8);
  out[1] = vld1q_s16(base + ofs1 * 8);
  out[2] = vld1q_s16(base + ofs2 * 8);
  out[3] = vld1q_s16(base + ofs3 * 8);
  out[4] = vld1q_s16(base + ofs4 * 8);
  out[5] = vld1q_s16(base + ofs5 * 8);
  out[6] = vld1q_s16(base + ofs6 * 8);
  out[7] = vld1q_s16(base + ofs7 * 8);
}

static INLINE int16x8_t warp_affine_horizontal_step_4x1_f4_neon(
    int bd, int sx, int alpha, uint16x8x2_t in) {
  int16x8_t f[4];
  load_filters_4(f, sx, alpha);

  int16x8_t rv0 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 0);
  int16x8_t rv1 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 1);
  int16x8_t rv2 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 2);
  int16x8_t rv3 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 3);

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

static INLINE int16x8_t warp_affine_horizontal_step_8x1_f8_neon(
    int bd, int sx, int alpha, uint16x8x2_t in) {
  const int round0 = (bd == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;
  const int offset_bits_horiz = bd + FILTER_BITS - 1;

  int16x8_t f[8];
  load_filters_8(f, sx, alpha);

  int16x8_t rv0 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 0);
  int16x8_t rv1 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 1);
  int16x8_t rv2 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 2);
  int16x8_t rv3 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 3);
  int16x8_t rv4 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 4);
  int16x8_t rv5 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 5);
  int16x8_t rv6 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 6);
  int16x8_t rv7 = vextq_s16(vreinterpretq_s16_u16(in.val[0]),
                            vreinterpretq_s16_u16(in.val[1]), 7);

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

  int32x4_t res0 = horizontal_add_4d_s32x4(m0123);
  int32x4_t res1 = horizontal_add_4d_s32x4(m4567);
  res0 = vaddq_s32(res0, vdupq_n_s32(1 << offset_bits_horiz));
  res1 = vaddq_s32(res1, vdupq_n_s32(1 << offset_bits_horiz));
  res0 = vrshlq_s32(res0, vdupq_n_s32(-round0));
  res1 = vrshlq_s32(res1, vdupq_n_s32(-round0));
  return vcombine_s16(vmovn_s32(res0), vmovn_s32(res1));
}

static INLINE void warp_affine_horizontal_neon(const uint16_t *ref, int width,
                                               int height, int stride,
                                               int p_width, int16_t alpha,
                                               int16_t beta, int iy4, int sx4,
                                               int ix4, int16x8_t tmp[],
                                               int bd) {
  const int round0 = (bd == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;

  if (ix4 <= -7) {
    for (int k = 0; k < 15; ++k) {
      int iy = clamp(iy4 + k - 7, 0, height - 1);
      int32_t dup_val = (1 << (bd + FILTER_BITS - round0 - 1)) +
                        ref[iy * stride] * (1 << (FILTER_BITS - round0));
      tmp[k] = vdupq_n_s16(dup_val);
    }
    return;
  } else if (ix4 >= width + 6) {
    for (int k = 0; k < 15; ++k) {
      int iy = clamp(iy4 + k - 7, 0, height - 1);
      int32_t dup_val =
          (1 << (bd + FILTER_BITS - round0 - 1)) +
          ref[iy * stride + (width - 1)] * (1 << (FILTER_BITS - round0));
      tmp[k] = vdupq_n_s16(dup_val);
    }
    return;
  }

  for (int k = 0; k < 15; ++k) {
    const int iy = clamp(iy4 + k - 7, 0, height - 1);
    uint16x8x2_t in = vld1q_u16_x2(ref + iy * stride + ix4 - 7);

    const int out_of_boundary_left = -(ix4 - 6);
    const int out_of_boundary_right = (ix4 + 8) - width;

    const uint16_t k0[16] = { 0, 1, 2,  3,  4,  5,  6,  7,
                              8, 9, 10, 11, 12, 13, 14, 15 };
    const uint16x8_t indx0 = vld1q_u16(&k0[0]);
    const uint16x8_t indx1 = vld1q_u16(&k0[8]);

    if (out_of_boundary_left >= 0) {
      uint16x8_t cmp_vec = vdupq_n_u16(out_of_boundary_left);
      uint16x8_t vec_dup = vdupq_n_u16(ref[iy * stride]);
      uint16x8_t mask0 = vcleq_u16(indx0, cmp_vec);
      uint16x8_t mask1 = vcleq_u16(indx1, cmp_vec);
      in.val[0] = vbslq_u16(mask0, vec_dup, in.val[0]);
      in.val[1] = vbslq_u16(mask1, vec_dup, in.val[1]);
    }
    if (out_of_boundary_right >= 0) {
      uint16x8_t cmp_vec = vdupq_n_u16(15 - out_of_boundary_right);
      uint16x8_t vec_dup = vdupq_n_u16(ref[iy * stride + width - 1]);
      uint16x8_t mask0 = vcgeq_u16(indx0, cmp_vec);
      uint16x8_t mask1 = vcgeq_u16(indx1, cmp_vec);
      in.val[0] = vbslq_u16(mask0, vec_dup, in.val[0]);
      in.val[1] = vbslq_u16(mask1, vec_dup, in.val[1]);
    }

    const int sx = sx4 + beta * (k - 3);
    if (p_width == 4) {
      tmp[k] = warp_affine_horizontal_step_4x1_f4_neon(bd, sx, alpha, in);
    } else {
      tmp[k] = warp_affine_horizontal_step_8x1_f8_neon(bd, sx, alpha, in);
    }
  }
}

static INLINE uint16x4_t clip_pixel_highbd_vec(int32x4_t val, int bd) {
  const int limit = (1 << bd) - 1;
  return vqmovun_s32(vminq_s32(val, vdupq_n_s32(limit)));
}

static INLINE int32x4_t
warp_affine_vertical_filter_4x1_f1_neon(const int16x8_t *tmp, int sy) {
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

static INLINE int32x4x2_t
warp_affine_vertical_filter_8x1_f1_neon(const int16x8_t *tmp, int sy) {
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

static INLINE int32x4_t warp_affine_vertical_filter_4x1_f4_neon(
    const int16x8_t *tmp, int sy, int gamma) {
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

static INLINE int32x4x2_t warp_affine_vertical_filter_8x1_f8_neon(
    const int16x8_t *tmp, int sy, int gamma) {
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

static INLINE void warp_affine_vertical_step_4x1_f4_neon(
    uint16_t *pred, int p_stride, int bd, uint16_t *dst, int dst_stride,
    bool is_compound, bool do_average, bool use_dist_wtd_comp_avg, int fwd,
    int bwd, int16_t gamma, const int16x8_t *tmp, int i, int sy, int j) {
  int32x4_t sum0 =
      gamma == 0 ? warp_affine_vertical_filter_4x1_f1_neon(tmp, sy)
                 : warp_affine_vertical_filter_4x1_f4_neon(tmp, sy, gamma);

  const int round0 = (bd == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;
  const int offset_bits_vert = bd + 2 * FILTER_BITS - round0;

  sum0 = vaddq_s32(sum0, vdupq_n_s32(1 << offset_bits_vert));

  uint16_t *dst16 = &pred[i * p_stride + j];

  if (!is_compound) {
    const int reduce_bits_vert = 2 * FILTER_BITS - round0;
    sum0 = vrshlq_s32(sum0, vdupq_n_s32(-reduce_bits_vert));

    const int res_sub_const = (1 << (bd - 1)) + (1 << bd);
    sum0 = vsubq_s32(sum0, vdupq_n_s32(res_sub_const));
    uint16x4_t res0 = clip_pixel_highbd_vec(sum0, bd);
    vst1_u16(dst16, res0);
    return;
  }

  sum0 = vrshrq_n_s32(sum0, COMPOUND_ROUND1_BITS);

  uint16_t *p = &dst[i * dst_stride + j];

  if (!do_average) {
    vst1_u16(p, vqmovun_s32(sum0));
    return;
  }

  uint16x4_t p0 = vld1_u16(p);
  int32x4_t p_vec0 = vreinterpretq_s32_u32(vmovl_u16(p0));
  if (use_dist_wtd_comp_avg) {
    p_vec0 = vmulq_n_s32(p_vec0, fwd);
    p_vec0 = vmlaq_n_s32(p_vec0, sum0, bwd);
    p_vec0 = vshrq_n_s32(p_vec0, DIST_PRECISION_BITS);
  } else {
    p_vec0 = vhaddq_s32(p_vec0, sum0);
  }

  const int offset_bits = bd + 2 * FILTER_BITS - round0;
  const int round1 = COMPOUND_ROUND1_BITS;
  const int res_sub_const =
      (1 << (offset_bits - round1)) + (1 << (offset_bits - round1 - 1));
  const int round_bits = 2 * FILTER_BITS - round0 - round1;

  p_vec0 = vsubq_s32(p_vec0, vdupq_n_s32(res_sub_const));
  p_vec0 = vrshlq_s32(p_vec0, vdupq_n_s32(-round_bits));
  uint16x4_t res0 = clip_pixel_highbd_vec(p_vec0, bd);
  vst1_u16(dst16, res0);
}

static INLINE void warp_affine_vertical_step_8x1_f8_neon(
    uint16_t *pred, int p_stride, int bd, uint16_t *dst, int dst_stride,
    bool is_compound, bool do_average, bool use_dist_wtd_comp_avg, int fwd,
    int bwd, int16_t gamma, const int16x8_t *tmp, int i, int sy, int j) {
  int32x4x2_t sums =
      gamma == 0 ? warp_affine_vertical_filter_8x1_f1_neon(tmp, sy)
                 : warp_affine_vertical_filter_8x1_f8_neon(tmp, sy, gamma);
  int32x4_t sum0 = sums.val[0];
  int32x4_t sum1 = sums.val[1];

  const int round0 = (bd == 12) ? ROUND0_BITS + 2 : ROUND0_BITS;
  const int offset_bits_vert = bd + 2 * FILTER_BITS - round0;

  sum0 = vaddq_s32(sum0, vdupq_n_s32(1 << offset_bits_vert));
  sum1 = vaddq_s32(sum1, vdupq_n_s32(1 << offset_bits_vert));

  uint16_t *dst16 = &pred[i * p_stride + j];

  if (!is_compound) {
    const int reduce_bits_vert = 2 * FILTER_BITS - round0;
    sum0 = vrshlq_s32(sum0, vdupq_n_s32(-reduce_bits_vert));
    sum1 = vrshlq_s32(sum1, vdupq_n_s32(-reduce_bits_vert));

    const int res_sub_const = (1 << (bd - 1)) + (1 << bd);
    sum0 = vsubq_s32(sum0, vdupq_n_s32(res_sub_const));
    sum1 = vsubq_s32(sum1, vdupq_n_s32(res_sub_const));
    uint16x4_t res0 = clip_pixel_highbd_vec(sum0, bd);
    uint16x4_t res1 = clip_pixel_highbd_vec(sum1, bd);
    vst1_u16(dst16, res0);
    vst1_u16(dst16 + 4, res1);
    return;
  }

  sum0 = vrshrq_n_s32(sum0, COMPOUND_ROUND1_BITS);
  sum1 = vrshrq_n_s32(sum1, COMPOUND_ROUND1_BITS);

  uint16_t *p = &dst[i * dst_stride + j];

  if (!do_average) {
    vst1_u16(p, vqmovun_s32(sum0));
    vst1_u16(p + 4, vqmovun_s32(sum1));
    return;
  }

  uint16x8_t p0 = vld1q_u16(p);
  int32x4_t p_vec0 = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(p0)));
  int32x4_t p_vec1 = vreinterpretq_s32_u32(vmovl_u16(vget_high_u16(p0)));
  if (use_dist_wtd_comp_avg) {
    p_vec0 = vmulq_n_s32(p_vec0, fwd);
    p_vec1 = vmulq_n_s32(p_vec1, fwd);
    p_vec0 = vmlaq_n_s32(p_vec0, sum0, bwd);
    p_vec1 = vmlaq_n_s32(p_vec1, sum1, bwd);
    p_vec0 = vshrq_n_s32(p_vec0, DIST_PRECISION_BITS);
    p_vec1 = vshrq_n_s32(p_vec1, DIST_PRECISION_BITS);
  } else {
    p_vec0 = vhaddq_s32(p_vec0, sum0);
    p_vec1 = vhaddq_s32(p_vec1, sum1);
  }

  const int offset_bits = bd + 2 * FILTER_BITS - round0;
  const int round1 = COMPOUND_ROUND1_BITS;
  const int res_sub_const =
      (1 << (offset_bits - round1)) + (1 << (offset_bits - round1 - 1));
  const int round_bits = 2 * FILTER_BITS - round0 - round1;

  p_vec0 = vsubq_s32(p_vec0, vdupq_n_s32(res_sub_const));
  p_vec1 = vsubq_s32(p_vec1, vdupq_n_s32(res_sub_const));

  p_vec0 = vrshlq_s32(p_vec0, vdupq_n_s32(-round_bits));
  p_vec1 = vrshlq_s32(p_vec1, vdupq_n_s32(-round_bits));
  uint16x4_t res0 = clip_pixel_highbd_vec(p_vec0, bd);
  uint16x4_t res1 = clip_pixel_highbd_vec(p_vec1, bd);
  vst1_u16(dst16, res0);
  vst1_u16(dst16 + 4, res1);
}

static INLINE void warp_affine_vertical_neon(
    uint16_t *pred, int p_width, int p_height, int p_stride, int bd,
    uint16_t *dst, int dst_stride, bool is_compound, bool do_average,
    bool use_dist_wtd_comp_avg, int fwd, int bwd, int16_t gamma, int16_t delta,
    const int16x8_t *tmp, int i, int sy4, int j) {
  int limit_height = p_height > 4 ? 8 : 4;

  if (p_width > 4) {
    // p_width == 8
    for (int k = 0; k < limit_height; ++k) {
      int sy = sy4 + delta * k;
      warp_affine_vertical_step_8x1_f8_neon(
          pred, p_stride, bd, dst, dst_stride, is_compound, do_average,
          use_dist_wtd_comp_avg, fwd, bwd, gamma, tmp + k, i + k, sy, j);
    }
  } else {
    // p_width == 4
    for (int k = 0; k < limit_height; ++k) {
      int sy = sy4 + delta * k;
      warp_affine_vertical_step_4x1_f4_neon(
          pred, p_stride, bd, dst, dst_stride, is_compound, do_average,
          use_dist_wtd_comp_avg, fwd, bwd, gamma, tmp + k, i + k, sy, j);
    }
  }
}

void av1_highbd_warp_affine_neon(const int32_t *mat, const uint16_t *ref,
                                 int width, int height, int stride,
                                 uint16_t *pred, int p_col, int p_row,
                                 int p_width, int p_height, int p_stride,
                                 int subsampling_x, int subsampling_y, int bd,
                                 ConvolveParams *conv_params, int16_t alpha,
                                 int16_t beta, int16_t gamma, int16_t delta) {
  uint16_t *const dst = conv_params->dst;
  const int dst_stride = conv_params->dst_stride;
  const bool is_compound = conv_params->is_compound;
  const bool do_average = conv_params->do_average;
  const bool use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;
  const int fwd = conv_params->fwd_offset;
  const int bwd = conv_params->bck_offset;

  assert(IMPLIES(is_compound, dst != NULL));

  for (int i = 0; i < p_height; i += 8) {
    for (int j = 0; j < p_width; j += 8) {
      // Calculate the center of this 8x8 block,
      // project to luma coordinates (if in a subsampled chroma plane),
      // apply the affine transformation,
      // then convert back to the original coordinates (if necessary)
      const int32_t src_x = (j + 4 + p_col) << subsampling_x;
      const int32_t src_y = (i + 4 + p_row) << subsampling_y;
      const int64_t dst_x =
          (int64_t)mat[2] * src_x + (int64_t)mat[3] * src_y + (int64_t)mat[0];
      const int64_t dst_y =
          (int64_t)mat[4] * src_x + (int64_t)mat[5] * src_y + (int64_t)mat[1];
      const int64_t x4 = dst_x >> subsampling_x;
      const int64_t y4 = dst_y >> subsampling_y;

      const int32_t ix4 = (int32_t)(x4 >> WARPEDMODEL_PREC_BITS);
      int32_t sx4 = x4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
      const int32_t iy4 = (int32_t)(y4 >> WARPEDMODEL_PREC_BITS);
      int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);

      sx4 += alpha * (-4) + beta * (-4);
      sy4 += gamma * (-4) + delta * (-4);

      sx4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);
      sy4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);

      // Each horizontal filter result is formed by the sum of up to eight
      // multiplications by filter values and then a shift. Although both the
      // inputs and filters are loaded as int16, the input data is at most bd
      // bits and the filters are at most 8 bits each. Additionally since we
      // know all possible filter values we know that the sum of absolute
      // filter values will fit in at most 9 bits. With this in mind we can
      // conclude that the sum of each filter application will fit in bd + 9
      // bits. The shift following the summation is ROUND0_BITS (which is 3),
      // +2 for 12-bit, which gives us a final storage of:
      // bd ==  8: ( 8 + 9) - 3 => 14 bits
      // bd == 10: (10 + 9) - 3 => 16 bits
      // bd == 12: (12 + 9) - 5 => 16 bits
      // So it is safe to use int16x8_t as the intermediate storage type here.
      int16x8_t tmp[15];

      warp_affine_horizontal_neon(ref, width, height, stride, p_width, alpha,
                                  beta, iy4, sx4, ix4, tmp, bd);
      warp_affine_vertical_neon(pred, p_width, p_height, p_stride, bd, dst,
                                dst_stride, is_compound, do_average,
                                use_dist_wtd_comp_avg, fwd, bwd, gamma, delta,
                                tmp, i, sy4, j);
    }
  }
}
