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

#include "warp_plane_neon.h"

static AOM_FORCE_INLINE int16x8_t horizontal_filter_4x1_f4(const uint8x16_t in,
                                                           int sx, int alpha) {
  const int32x4_t add_const = vdupq_n_s32(1 << (8 + FILTER_BITS - 1));

  // Loading the 8 filter taps
  int16x8_t f[4];
  load_filters_4(f, sx, alpha);

  int16x8_t in16_lo = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(in)));
  int16x8_t in16_hi = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(in)));

  int16x8_t m0 = vmulq_s16(f[0], in16_lo);
  int16x8_t m1 = vmulq_s16(f[1], vextq_s16(in16_lo, in16_hi, 1));
  int16x8_t m2 = vmulq_s16(f[2], vextq_s16(in16_lo, in16_hi, 2));
  int16x8_t m3 = vmulq_s16(f[3], vextq_s16(in16_lo, in16_hi, 3));

  int32x4_t m0123_pairs[] = { vpaddlq_s16(m0), vpaddlq_s16(m1), vpaddlq_s16(m2),
                              vpaddlq_s16(m3) };

  int32x4_t tmp_res_low = horizontal_add_4d_s32x4(m0123_pairs);

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

  int16x8_t in16_lo = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(in)));
  int16x8_t in16_hi = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(in)));

  int16x8_t m0 = vmulq_s16(f[0], in16_lo);
  int16x8_t m1 = vmulq_s16(f[1], vextq_s16(in16_lo, in16_hi, 1));
  int16x8_t m2 = vmulq_s16(f[2], vextq_s16(in16_lo, in16_hi, 2));
  int16x8_t m3 = vmulq_s16(f[3], vextq_s16(in16_lo, in16_hi, 3));
  int16x8_t m4 = vmulq_s16(f[4], vextq_s16(in16_lo, in16_hi, 4));
  int16x8_t m5 = vmulq_s16(f[5], vextq_s16(in16_lo, in16_hi, 5));
  int16x8_t m6 = vmulq_s16(f[6], vextq_s16(in16_lo, in16_hi, 6));
  int16x8_t m7 = vmulq_s16(f[7], vextq_s16(in16_lo, in16_hi, 7));

  int32x4_t m0123_pairs[] = { vpaddlq_s16(m0), vpaddlq_s16(m1), vpaddlq_s16(m2),
                              vpaddlq_s16(m3) };
  int32x4_t m4567_pairs[] = { vpaddlq_s16(m4), vpaddlq_s16(m5), vpaddlq_s16(m6),
                              vpaddlq_s16(m7) };

  int32x4_t tmp_res_low = horizontal_add_4d_s32x4(m0123_pairs);
  int32x4_t tmp_res_high = horizontal_add_4d_s32x4(m4567_pairs);

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

  int16x8_t in16_lo = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(in)));
  int16x8_t in16_hi = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(in)));

  int16x8_t m0 = vmulq_s16(f_s16, in16_lo);
  int16x8_t m1 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 1));
  int16x8_t m2 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 2));
  int16x8_t m3 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 3));

  int32x4_t m0123_pairs[] = { vpaddlq_s16(m0), vpaddlq_s16(m1), vpaddlq_s16(m2),
                              vpaddlq_s16(m3) };

  int32x4_t tmp_res_low = horizontal_add_4d_s32x4(m0123_pairs);

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

  int16x8_t in16_lo = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(in)));
  int16x8_t in16_hi = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(in)));

  int16x8_t m0 = vmulq_s16(f_s16, in16_lo);
  int16x8_t m1 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 1));
  int16x8_t m2 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 2));
  int16x8_t m3 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 3));
  int16x8_t m4 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 4));
  int16x8_t m5 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 5));
  int16x8_t m6 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 6));
  int16x8_t m7 = vmulq_s16(f_s16, vextq_s16(in16_lo, in16_hi, 7));

  int32x4_t m0123_pairs[] = { vpaddlq_s16(m0), vpaddlq_s16(m1), vpaddlq_s16(m2),
                              vpaddlq_s16(m3) };
  int32x4_t m4567_pairs[] = { vpaddlq_s16(m4), vpaddlq_s16(m5), vpaddlq_s16(m6),
                              vpaddlq_s16(m7) };

  int32x4_t tmp_res_low = horizontal_add_4d_s32x4(m0123_pairs);
  int32x4_t tmp_res_high = horizontal_add_4d_s32x4(m4567_pairs);

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

  int32x4_t m0 = vmull_s16(vget_low_s16(s0), vget_low_s16(f[0]));
  m0 = vmlal_s16(m0, vget_high_s16(s0), vget_high_s16(f[0]));
  int32x4_t m1 = vmull_s16(vget_low_s16(s1), vget_low_s16(f[1]));
  m1 = vmlal_s16(m1, vget_high_s16(s1), vget_high_s16(f[1]));
  int32x4_t m2 = vmull_s16(vget_low_s16(s2), vget_low_s16(f[2]));
  m2 = vmlal_s16(m2, vget_high_s16(s2), vget_high_s16(f[2]));
  int32x4_t m3 = vmull_s16(vget_low_s16(s3), vget_low_s16(f[3]));
  m3 = vmlal_s16(m3, vget_high_s16(s3), vget_high_s16(f[3]));

  int32x4_t m0123_pairs[] = { m0, m1, m2, m3 };

  *res = horizontal_add_4d_s32x4(m0123_pairs);
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

  int32x4_t m0123_pairs[] = { m0, m1, m2, m3 };
  int32x4_t m4567_pairs[] = { m4, m5, m6, m7 };

  *res_low = horizontal_add_4d_s32x4(m0123_pairs);
  *res_high = horizontal_add_4d_s32x4(m4567_pairs);
}

void av1_warp_affine_neon(const int32_t *mat, const uint8_t *ref, int width,
                          int height, int stride, uint8_t *pred, int p_col,
                          int p_row, int p_width, int p_height, int p_stride,
                          int subsampling_x, int subsampling_y,
                          ConvolveParams *conv_params, int16_t alpha,
                          int16_t beta, int16_t gamma, int16_t delta) {
  av1_warp_affine_common(mat, ref, width, height, stride, pred, p_col, p_row,
                         p_width, p_height, p_stride, subsampling_x,
                         subsampling_y, conv_params, alpha, beta, gamma, delta);
}
