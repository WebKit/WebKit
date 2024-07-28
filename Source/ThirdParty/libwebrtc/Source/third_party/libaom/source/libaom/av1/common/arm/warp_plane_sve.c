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

#include "aom_dsp/arm/aom_neon_sve_bridge.h"
#include "warp_plane_neon.h"

DECLARE_ALIGNED(16, static const uint8_t, usdot_permute_idx[48]) = {
  0, 1, 2,  3,  1, 2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6,
  4, 5, 6,  7,  5, 6,  7,  8,  6,  7,  8,  9,  7,  8,  9,  10,
  8, 9, 10, 11, 9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14
};

static AOM_FORCE_INLINE int16x8_t horizontal_filter_4x1_f4(const uint8x16_t in,
                                                           int sx, int alpha) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  // Loading the 8 filter taps
  int16x8_t f[4];
  load_filters_4(f, sx, alpha);

  int8x16_t f01_u8 = vcombine_s8(vmovn_s16(f[0]), vmovn_s16(f[1]));
  int8x16_t f23_u8 = vcombine_s8(vmovn_s16(f[2]), vmovn_s16(f[3]));

  uint8x8_t in0 = vget_low_u8(in);
  uint8x8_t in1 = vget_low_u8(vextq_u8(in, in, 1));
  uint8x8_t in2 = vget_low_u8(vextq_u8(in, in, 2));
  uint8x8_t in3 = vget_low_u8(vextq_u8(in, in, 3));

  int32x4_t m01 = vusdotq_s32(vdupq_n_s32(0), vcombine_u8(in0, in1), f01_u8);
  int32x4_t m23 = vusdotq_s32(vdupq_n_s32(0), vcombine_u8(in2, in3), f23_u8);

  int32x4_t tmp_res_low = vpaddq_s32(m01, m23);

  tmp_res_low = vaddq_s32(tmp_res_low, add_const);

  uint16x8_t res =
      vcombine_u16(vqrshrun_n_s32(tmp_res_low, ROUND0_BITS), vdup_n_u16(0));
  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_8x1_f8(const uint8x16_t in,
                                                           int sx, int alpha) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  // Loading the 8 filter taps
  int16x8_t f[8];
  load_filters_8(f, sx, alpha);

  int8x16_t f01_u8 = vcombine_s8(vmovn_s16(f[0]), vmovn_s16(f[1]));
  int8x16_t f23_u8 = vcombine_s8(vmovn_s16(f[2]), vmovn_s16(f[3]));
  int8x16_t f45_u8 = vcombine_s8(vmovn_s16(f[4]), vmovn_s16(f[5]));
  int8x16_t f67_u8 = vcombine_s8(vmovn_s16(f[6]), vmovn_s16(f[7]));

  uint8x8_t in0 = vget_low_u8(in);
  uint8x8_t in1 = vget_low_u8(vextq_u8(in, in, 1));
  uint8x8_t in2 = vget_low_u8(vextq_u8(in, in, 2));
  uint8x8_t in3 = vget_low_u8(vextq_u8(in, in, 3));
  uint8x8_t in4 = vget_low_u8(vextq_u8(in, in, 4));
  uint8x8_t in5 = vget_low_u8(vextq_u8(in, in, 5));
  uint8x8_t in6 = vget_low_u8(vextq_u8(in, in, 6));
  uint8x8_t in7 = vget_low_u8(vextq_u8(in, in, 7));

  int32x4_t m01 = vusdotq_s32(vdupq_n_s32(0), vcombine_u8(in0, in1), f01_u8);
  int32x4_t m23 = vusdotq_s32(vdupq_n_s32(0), vcombine_u8(in2, in3), f23_u8);
  int32x4_t m45 = vusdotq_s32(vdupq_n_s32(0), vcombine_u8(in4, in5), f45_u8);
  int32x4_t m67 = vusdotq_s32(vdupq_n_s32(0), vcombine_u8(in6, in7), f67_u8);

  int32x4_t tmp_res_low = vpaddq_s32(m01, m23);
  int32x4_t tmp_res_high = vpaddq_s32(m45, m67);

  tmp_res_low = vaddq_s32(tmp_res_low, add_const);
  tmp_res_high = vaddq_s32(tmp_res_high, add_const);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(tmp_res_low, ROUND0_BITS),
                                vqrshrun_n_s32(tmp_res_high, ROUND0_BITS));
  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_4x1_f1(const uint8x16_t in,
                                                           int sx) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  int16x8_t f_s16 =
      vld1q_s16((int16_t *)(av1_warped_filter + (sx >> WARPEDDIFF_PREC_BITS)));

  int8x16_t f_s8 = vcombine_s8(vmovn_s16(f_s16), vmovn_s16(f_s16));

  uint8x16_t perm0 = vld1q_u8(&usdot_permute_idx[0]);
  uint8x16_t perm1 = vld1q_u8(&usdot_permute_idx[16]);

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  uint8x16_t in_0123 = vqtbl1q_u8(in, perm0);
  uint8x16_t in_4567 = vqtbl1q_u8(in, perm1);

  int32x4_t m0123 = vusdotq_laneq_s32(vdupq_n_s32(0), in_0123, f_s8, 0);
  m0123 = vusdotq_laneq_s32(m0123, in_4567, f_s8, 1);

  int32x4_t tmp_res_low = m0123;

  tmp_res_low = vaddq_s32(tmp_res_low, add_const);

  uint16x8_t res =
      vcombine_u16(vqrshrun_n_s32(tmp_res_low, ROUND0_BITS), vdup_n_u16(0));
  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE int16x8_t horizontal_filter_8x1_f1(const uint8x16_t in,
                                                           int sx) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  int16x8_t f_s16 =
      vld1q_s16((int16_t *)(av1_warped_filter + (sx >> WARPEDDIFF_PREC_BITS)));

  int8x16_t f_s8 = vcombine_s8(vmovn_s16(f_s16), vmovn_s16(f_s16));

  uint8x16_t perm0 = vld1q_u8(&usdot_permute_idx[0]);
  uint8x16_t perm1 = vld1q_u8(&usdot_permute_idx[16]);
  uint8x16_t perm2 = vld1q_u8(&usdot_permute_idx[32]);

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  uint8x16_t in_0123 = vqtbl1q_u8(in, perm0);
  uint8x16_t in_4567 = vqtbl1q_u8(in, perm1);
  uint8x16_t in_89ab = vqtbl1q_u8(in, perm2);

  int32x4_t m0123 = vusdotq_laneq_s32(vdupq_n_s32(0), in_0123, f_s8, 0);
  m0123 = vusdotq_laneq_s32(m0123, in_4567, f_s8, 1);

  int32x4_t m4567 = vusdotq_laneq_s32(vdupq_n_s32(0), in_4567, f_s8, 0);
  m4567 = vusdotq_laneq_s32(m4567, in_89ab, f_s8, 1);

  int32x4_t tmp_res_low = m0123;
  int32x4_t tmp_res_high = m4567;

  tmp_res_low = vaddq_s32(tmp_res_low, add_const);
  tmp_res_high = vaddq_s32(tmp_res_high, add_const);

  uint16x8_t res = vcombine_u16(vqrshrun_n_s32(tmp_res_low, ROUND0_BITS),
                                vqrshrun_n_s32(tmp_res_high, ROUND0_BITS));
  return vreinterpretq_s16_u16(res);
}

static AOM_FORCE_INLINE void vertical_filter_4x1_f1(const int16x8_t *src,
                                                    int32x4_t *res, int sy) {
  int16x4_t s0 = vget_low_s16(src[0]);
  int16x4_t s1 = vget_low_s16(src[1]);
  int16x4_t s2 = vget_low_s16(src[2]);
  int16x4_t s3 = vget_low_s16(src[3]);
  int16x4_t s4 = vget_low_s16(src[4]);
  int16x4_t s5 = vget_low_s16(src[5]);
  int16x4_t s6 = vget_low_s16(src[6]);
  int16x4_t s7 = vget_low_s16(src[7]);

  int16x8_t f =
      vld1q_s16((int16_t *)(av1_warped_filter + (sy >> WARPEDDIFF_PREC_BITS)));

  int32x4_t m0123 = vmull_lane_s16(s0, vget_low_s16(f), 0);
  m0123 = vmlal_lane_s16(m0123, s1, vget_low_s16(f), 1);
  m0123 = vmlal_lane_s16(m0123, s2, vget_low_s16(f), 2);
  m0123 = vmlal_lane_s16(m0123, s3, vget_low_s16(f), 3);
  m0123 = vmlal_lane_s16(m0123, s4, vget_high_s16(f), 0);
  m0123 = vmlal_lane_s16(m0123, s5, vget_high_s16(f), 1);
  m0123 = vmlal_lane_s16(m0123, s6, vget_high_s16(f), 2);
  m0123 = vmlal_lane_s16(m0123, s7, vget_high_s16(f), 3);

  *res = m0123;
}

static AOM_FORCE_INLINE void vertical_filter_4x1_f4(const int16x8_t *src,
                                                    int32x4_t *res, int sy,
                                                    int gamma) {
  int16x8_t s0, s1, s2, s3;
  transpose_elems_s16_4x8(
      vget_low_s16(src[0]), vget_low_s16(src[1]), vget_low_s16(src[2]),
      vget_low_s16(src[3]), vget_low_s16(src[4]), vget_low_s16(src[5]),
      vget_low_s16(src[6]), vget_low_s16(src[7]), &s0, &s1, &s2, &s3);

  int16x8_t f[4];
  load_filters_4(f, sy, gamma);

  int64x2_t m0 = aom_sdotq_s16(vdupq_n_s64(0), s0, f[0]);
  int64x2_t m1 = aom_sdotq_s16(vdupq_n_s64(0), s1, f[1]);
  int64x2_t m2 = aom_sdotq_s16(vdupq_n_s64(0), s2, f[2]);
  int64x2_t m3 = aom_sdotq_s16(vdupq_n_s64(0), s3, f[3]);

  int64x2_t m01 = vpaddq_s64(m0, m1);
  int64x2_t m23 = vpaddq_s64(m2, m3);

  *res = vcombine_s32(vmovn_s64(m01), vmovn_s64(m23));
}

static AOM_FORCE_INLINE void vertical_filter_8x1_f1(const int16x8_t *src,
                                                    int32x4_t *res_low,
                                                    int32x4_t *res_high,
                                                    int sy) {
  int16x8_t s0 = src[0];
  int16x8_t s1 = src[1];
  int16x8_t s2 = src[2];
  int16x8_t s3 = src[3];
  int16x8_t s4 = src[4];
  int16x8_t s5 = src[5];
  int16x8_t s6 = src[6];
  int16x8_t s7 = src[7];

  int16x8_t f =
      vld1q_s16((int16_t *)(av1_warped_filter + (sy >> WARPEDDIFF_PREC_BITS)));

  int32x4_t m0123 = vmull_lane_s16(vget_low_s16(s0), vget_low_s16(f), 0);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s1), vget_low_s16(f), 1);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s2), vget_low_s16(f), 2);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s3), vget_low_s16(f), 3);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s4), vget_high_s16(f), 0);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s5), vget_high_s16(f), 1);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s6), vget_high_s16(f), 2);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s7), vget_high_s16(f), 3);

  int32x4_t m4567 = vmull_lane_s16(vget_high_s16(s0), vget_low_s16(f), 0);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s1), vget_low_s16(f), 1);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s2), vget_low_s16(f), 2);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s3), vget_low_s16(f), 3);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s4), vget_high_s16(f), 0);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s5), vget_high_s16(f), 1);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s6), vget_high_s16(f), 2);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s7), vget_high_s16(f), 3);

  *res_low = m0123;
  *res_high = m4567;
}

static AOM_FORCE_INLINE void vertical_filter_8x1_f8(const int16x8_t *src,
                                                    int32x4_t *res_low,
                                                    int32x4_t *res_high, int sy,
                                                    int gamma) {
  int16x8_t s0 = src[0];
  int16x8_t s1 = src[1];
  int16x8_t s2 = src[2];
  int16x8_t s3 = src[3];
  int16x8_t s4 = src[4];
  int16x8_t s5 = src[5];
  int16x8_t s6 = src[6];
  int16x8_t s7 = src[7];
  transpose_elems_inplace_s16_8x8(&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);

  int16x8_t f[8];
  load_filters_8(f, sy, gamma);

  int64x2_t m0 = aom_sdotq_s16(vdupq_n_s64(0), s0, f[0]);
  int64x2_t m1 = aom_sdotq_s16(vdupq_n_s64(0), s1, f[1]);
  int64x2_t m2 = aom_sdotq_s16(vdupq_n_s64(0), s2, f[2]);
  int64x2_t m3 = aom_sdotq_s16(vdupq_n_s64(0), s3, f[3]);
  int64x2_t m4 = aom_sdotq_s16(vdupq_n_s64(0), s4, f[4]);
  int64x2_t m5 = aom_sdotq_s16(vdupq_n_s64(0), s5, f[5]);
  int64x2_t m6 = aom_sdotq_s16(vdupq_n_s64(0), s6, f[6]);
  int64x2_t m7 = aom_sdotq_s16(vdupq_n_s64(0), s7, f[7]);

  int64x2_t m01 = vpaddq_s64(m0, m1);
  int64x2_t m23 = vpaddq_s64(m2, m3);
  int64x2_t m45 = vpaddq_s64(m4, m5);
  int64x2_t m67 = vpaddq_s64(m6, m7);

  *res_low = vcombine_s32(vmovn_s64(m01), vmovn_s64(m23));
  *res_high = vcombine_s32(vmovn_s64(m45), vmovn_s64(m67));
}

void av1_warp_affine_sve(const int32_t *mat, const uint8_t *ref, int width,
                         int height, int stride, uint8_t *pred, int p_col,
                         int p_row, int p_width, int p_height, int p_stride,
                         int subsampling_x, int subsampling_y,
                         ConvolveParams *conv_params, int16_t alpha,
                         int16_t beta, int16_t gamma, int16_t delta) {
  av1_warp_affine_common(mat, ref, width, height, stride, pred, p_col, p_row,
                         p_width, p_height, p_stride, subsampling_x,
                         subsampling_y, conv_params, alpha, beta, gamma, delta);
}
