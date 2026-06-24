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
#ifndef AOM_AV1_COMMON_ARM_WARP_PLANE_NEON_H_
#define AOM_AV1_COMMON_ARM_WARP_PLANE_NEON_H_

#include <assert.h>
#include <arm_neon.h>
#include <memory.h>
#include <math.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "config/av1_rtcd.h"
#include "av1/common/warped_motion.h"
#include "av1/common/scale.h"

static AOM_FORCE_INLINE void vertical_filter_4x1_f4(const int16x8_t *src,
                                                    int32x4_t *res, int sy,
                                                    int gamma);

static AOM_FORCE_INLINE void vertical_filter_8x1_f8(const int16x8_t *src,
                                                    int32x4_t *res_low,
                                                    int32x4_t *res_high, int sy,
                                                    int gamma);

static AOM_FORCE_INLINE void vertical_filter_4x1_f1(const int16x8_t *src,
                                                    int32x4_t *res, int sy) {
  int16_t *f_ptr =
      (int16_t *)(av1_warped_filter + (sy >> WARPEDDIFF_PREC_BITS));
  int16x8_t f = vld1q_s16(f_ptr);

  int32x4_t m0123;
  if (f_ptr[0] != 0) {
    m0123 = vmull_lane_s16(vget_low_s16(src[0]), vget_low_s16(f), 0);
  } else {
    m0123 = vdupq_n_s32(0);
  }
  if (f_ptr[1] != 0) {
    m0123 = vmlal_lane_s16(m0123, vget_low_s16(src[1]), vget_low_s16(f), 1);
  }

  m0123 = vmlal_lane_s16(m0123, vget_low_s16(src[2]), vget_low_s16(f), 2);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(src[3]), vget_low_s16(f), 3);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(src[4]), vget_high_s16(f), 0);
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(src[5]), vget_high_s16(f), 1);

  if (f_ptr[6] != 0) {
    m0123 = vmlal_lane_s16(m0123, vget_low_s16(src[6]), vget_high_s16(f), 2);
  }
  if (f_ptr[7] != 0) {
    m0123 = vmlal_lane_s16(m0123, vget_low_s16(src[7]), vget_high_s16(f), 3);
  }

  *res = m0123;
}

static AOM_FORCE_INLINE void vertical_filter_8x1_f1(const int16x8_t *s,
                                                    int32x4_t *res_low,
                                                    int32x4_t *res_high,
                                                    int sy) {
  int16_t *f_ptr =
      (int16_t *)(av1_warped_filter + (sy >> WARPEDDIFF_PREC_BITS));
  int16x8_t f = vld1q_s16(f_ptr);

  int32x4_t m0123, m4567;
  if (f_ptr[0] != 0) {
    m0123 = vmull_lane_s16(vget_low_s16(s[0]), vget_low_s16(f), 0);
    m4567 = vmull_lane_s16(vget_high_s16(s[0]), vget_low_s16(f), 0);
  } else {
    m0123 = vdupq_n_s32(0);
    m4567 = vdupq_n_s32(0);
  }
  if (f_ptr[1] != 0) {
    m0123 = vmlal_lane_s16(m0123, vget_low_s16(s[1]), vget_low_s16(f), 1);
    m4567 = vmlal_lane_s16(m4567, vget_high_s16(s[1]), vget_low_s16(f), 1);
  }
  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s[2]), vget_low_s16(f), 2);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s[2]), vget_low_s16(f), 2);

  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s[3]), vget_low_s16(f), 3);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s[3]), vget_low_s16(f), 3);

  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s[4]), vget_high_s16(f), 0);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s[4]), vget_high_s16(f), 0);

  m0123 = vmlal_lane_s16(m0123, vget_low_s16(s[5]), vget_high_s16(f), 1);
  m4567 = vmlal_lane_s16(m4567, vget_high_s16(s[5]), vget_high_s16(f), 1);

  if (f_ptr[6] != 0) {
    m0123 = vmlal_lane_s16(m0123, vget_low_s16(s[6]), vget_high_s16(f), 2);
    m4567 = vmlal_lane_s16(m4567, vget_high_s16(s[6]), vget_high_s16(f), 2);
  }
  if (f_ptr[7] != 0) {
    m0123 = vmlal_lane_s16(m0123, vget_low_s16(s[7]), vget_high_s16(f), 3);
    m4567 = vmlal_lane_s16(m4567, vget_high_s16(s[7]), vget_high_s16(f), 3);
  }

  *res_low = m0123;
  *res_high = m4567;
}

static AOM_FORCE_INLINE void load_filters_4(int16x8_t out[], int offset,
                                            int stride) {
  out[0] = vld1q_s16(
      av1_warped_filter[(offset + 0 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[1] = vld1q_s16(
      av1_warped_filter[(offset + 1 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[2] = vld1q_s16(
      av1_warped_filter[(offset + 2 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[3] = vld1q_s16(
      av1_warped_filter[(offset + 3 * stride) >> WARPEDDIFF_PREC_BITS]);
}

static AOM_FORCE_INLINE void load_filters_8(int16x8_t out[], int offset,
                                            int stride) {
  out[0] = vld1q_s16(
      av1_warped_filter[(offset + 0 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[1] = vld1q_s16(
      av1_warped_filter[(offset + 1 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[2] = vld1q_s16(
      av1_warped_filter[(offset + 2 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[3] = vld1q_s16(
      av1_warped_filter[(offset + 3 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[4] = vld1q_s16(
      av1_warped_filter[(offset + 4 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[5] = vld1q_s16(
      av1_warped_filter[(offset + 5 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[6] = vld1q_s16(
      av1_warped_filter[(offset + 6 * stride) >> WARPEDDIFF_PREC_BITS]);
  out[7] = vld1q_s16(
      av1_warped_filter[(offset + 7 * stride) >> WARPEDDIFF_PREC_BITS]);
}

static AOM_FORCE_INLINE int clamp_iy(int iy, int height) {
  return clamp(iy, 0, height - 1);
}

static inline bool warp_affine_special_case(const uint8_t *ref, int32_t ix4,
                                            int32_t iy4, int width, int height,
                                            int stride, const int height_limit,
                                            int16x8_t tmp[]) {
  const int bd = 8;
  const int reduce_bits_horiz = ROUND0_BITS;

  if (ix4 <= -7) {
    for (int k = 0; k < height_limit; ++k) {
      int iy = clamp_iy(iy4 + k - 7, height);
      int16_t dup_val =
          (1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
          ref[iy * stride] * (1 << (FILTER_BITS - reduce_bits_horiz));
      tmp[k] = vdupq_n_s16(dup_val);
    }
    return true;
  } else if (ix4 >= width + 6) {
    for (int k = 0; k < height_limit; ++k) {
      int iy = clamp_iy(iy4 + k - 7, height);
      int16_t dup_val = (1 << (bd + FILTER_BITS - reduce_bits_horiz - 1)) +
                        ref[iy * stride + (width - 1)] *
                            (1 << (FILTER_BITS - reduce_bits_horiz));
      tmp[k] = vdupq_n_s16(dup_val);
    }
    return true;
  }

  return false;
}

#define APPLY_HORIZONTAL_SHIFT(fn, ...)                                \
  do {                                                                 \
    if (out_of_boundary_left >= 0 || out_of_boundary_right >= 0) {     \
      for (int k = 0; k < height_limit; ++k) {                         \
        const int iy = clamp_iy(iy4 + k - 7, height);                  \
        const uint8_t *src = ref + iy * stride + ix4 - 7;              \
        uint8x16_t src_1 = vld1q_u8(src);                              \
                                                                       \
        if (out_of_boundary_left >= 0) {                               \
          int limit = out_of_boundary_left + 1;                        \
          uint8x16_t cmp_vec = vdupq_n_u8(out_of_boundary_left);       \
          uint8x16_t vec_dup = vdupq_n_u8(*(src + limit));             \
          uint8x16_t mask_val = vcleq_u8(indx, cmp_vec);               \
          src_1 = vbslq_u8(mask_val, vec_dup, src_1);                  \
        }                                                              \
        if (out_of_boundary_right >= 0) {                              \
          int limit = 15 - (out_of_boundary_right + 1);                \
          uint8x16_t cmp_vec = vdupq_n_u8(15 - out_of_boundary_right); \
          uint8x16_t vec_dup = vdupq_n_u8(*(src + limit));             \
          uint8x16_t mask_val = vcgeq_u8(indx, cmp_vec);               \
          src_1 = vbslq_u8(mask_val, vec_dup, src_1);                  \
        }                                                              \
        tmp[k] = (fn)(src_1, __VA_ARGS__);                             \
      }                                                                \
    } else {                                                           \
      for (int k = 0; k < height_limit; ++k) {                         \
        const int iy = clamp_iy(iy4 + k - 7, height);                  \
        const uint8_t *src = ref + iy * stride + ix4 - 7;              \
        uint8x16_t src_1 = vld1q_u8(src);                              \
        tmp[k] = (fn)(src_1, __VA_ARGS__);                             \
      }                                                                \
    }                                                                  \
  } while (0)

static AOM_FORCE_INLINE void warp_affine_vertical(
    uint8_t *pred, int p_width, int p_height, int p_stride, int is_compound,
    uint16_t *dst, int dst_stride, int do_average, int use_dist_wtd_comp_avg,
    int16_t gamma, int16_t delta, const int64_t y4, const int i, const int j,
    int16x8_t tmp[], const int fwd, const int bwd) {
  const int bd = 8;
  const int reduce_bits_horiz = ROUND0_BITS;
  const int offset_bits_vert = bd + 2 * FILTER_BITS - reduce_bits_horiz;
  int add_const_vert;
  if (is_compound) {
    add_const_vert =
        (1 << offset_bits_vert) + (1 << (COMPOUND_ROUND1_BITS - 1));
  } else {
    add_const_vert =
        (1 << offset_bits_vert) + (1 << (2 * FILTER_BITS - ROUND0_BITS - 1));
  }
  const int sub_constant = (1 << (bd - 1)) + (1 << bd);

  const int offset_bits = bd + 2 * FILTER_BITS - ROUND0_BITS;
  const int res_sub_const =
      (1 << (2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS - 1)) -
      (1 << (offset_bits - COMPOUND_ROUND1_BITS)) -
      (1 << (offset_bits - COMPOUND_ROUND1_BITS - 1));

  int32_t sy4 = y4 & ((1 << WARPEDMODEL_PREC_BITS) - 1);
  sy4 += gamma * (-4) + delta * (-4) + (1 << (WARPEDDIFF_PREC_BITS - 1)) +
         (WARPEDPIXEL_PREC_SHIFTS << WARPEDDIFF_PREC_BITS);
  sy4 &= ~((1 << WARP_PARAM_REDUCE_BITS) - 1);

  if (p_width > 4) {
    for (int k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
      int sy = sy4 + delta * (k + 4);
      const int16x8_t *v_src = tmp + (k + 4);

      int32x4_t res_lo, res_hi;
      if (gamma == 0) {
        vertical_filter_8x1_f1(v_src, &res_lo, &res_hi, sy);
      } else {
        vertical_filter_8x1_f8(v_src, &res_lo, &res_hi, sy, gamma);
      }

      res_lo = vaddq_s32(res_lo, vdupq_n_s32(add_const_vert));
      res_hi = vaddq_s32(res_hi, vdupq_n_s32(add_const_vert));

      if (is_compound) {
        uint16_t *const p = (uint16_t *)&dst[(i + k + 4) * dst_stride + j];
        int16x8_t res_s16 =
            vcombine_s16(vshrn_n_s32(res_lo, COMPOUND_ROUND1_BITS),
                         vshrn_n_s32(res_hi, COMPOUND_ROUND1_BITS));
        if (do_average) {
          int16x8_t tmp16 = vreinterpretq_s16_u16(vld1q_u16(p));
          if (use_dist_wtd_comp_avg) {
            int32x4_t tmp32_lo = vmull_n_s16(vget_low_s16(tmp16), fwd);
            int32x4_t tmp32_hi = vmull_n_s16(vget_high_s16(tmp16), fwd);
            tmp32_lo = vmlal_n_s16(tmp32_lo, vget_low_s16(res_s16), bwd);
            tmp32_hi = vmlal_n_s16(tmp32_hi, vget_high_s16(res_s16), bwd);
            tmp16 = vcombine_s16(vshrn_n_s32(tmp32_lo, DIST_PRECISION_BITS),
                                 vshrn_n_s32(tmp32_hi, DIST_PRECISION_BITS));
          } else {
            tmp16 = vhaddq_s16(tmp16, res_s16);
          }
          int16x8_t res = vaddq_s16(tmp16, vdupq_n_s16(res_sub_const));
          uint8x8_t res8 = vqshrun_n_s16(
              res, 2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS);
          vst1_u8(&pred[(i + k + 4) * p_stride + j], res8);
        } else {
          vst1q_u16(p, vreinterpretq_u16_s16(res_s16));
        }
      } else {
        int16x8_t res16 =
            vcombine_s16(vshrn_n_s32(res_lo, 2 * FILTER_BITS - ROUND0_BITS),
                         vshrn_n_s32(res_hi, 2 * FILTER_BITS - ROUND0_BITS));
        res16 = vsubq_s16(res16, vdupq_n_s16(sub_constant));

        uint8_t *const p = (uint8_t *)&pred[(i + k + 4) * p_stride + j];
        vst1_u8(p, vqmovun_s16(res16));
      }
    }
  } else {
    // p_width == 4
    for (int k = -4; k < AOMMIN(4, p_height - i - 4); ++k) {
      int sy = sy4 + delta * (k + 4);
      const int16x8_t *v_src = tmp + (k + 4);

      int32x4_t res_lo;
      if (gamma == 0) {
        vertical_filter_4x1_f1(v_src, &res_lo, sy);
      } else {
        vertical_filter_4x1_f4(v_src, &res_lo, sy, gamma);
      }

      res_lo = vaddq_s32(res_lo, vdupq_n_s32(add_const_vert));

      if (is_compound) {
        uint16_t *const p = (uint16_t *)&dst[(i + k + 4) * dst_stride + j];

        int16x4_t res_lo_s16 = vshrn_n_s32(res_lo, COMPOUND_ROUND1_BITS);
        if (do_average) {
          uint8_t *const dst8 = &pred[(i + k + 4) * p_stride + j];
          int16x4_t tmp16_lo = vreinterpret_s16_u16(vld1_u16(p));
          if (use_dist_wtd_comp_avg) {
            int32x4_t tmp32_lo = vmull_n_s16(tmp16_lo, fwd);
            tmp32_lo = vmlal_n_s16(tmp32_lo, res_lo_s16, bwd);
            tmp16_lo = vshrn_n_s32(tmp32_lo, DIST_PRECISION_BITS);
          } else {
            tmp16_lo = vhadd_s16(tmp16_lo, res_lo_s16);
          }
          int16x4_t res = vadd_s16(tmp16_lo, vdup_n_s16(res_sub_const));
          uint8x8_t res8 = vqshrun_n_s16(
              vcombine_s16(res, vdup_n_s16(0)),
              2 * FILTER_BITS - ROUND0_BITS - COMPOUND_ROUND1_BITS);
          vst1_lane_u32((uint32_t *)dst8, vreinterpret_u32_u8(res8), 0);
        } else {
          uint16x4_t res_u16_low = vreinterpret_u16_s16(res_lo_s16);
          vst1_u16(p, res_u16_low);
        }
      } else {
        int16x4_t res16 = vshrn_n_s32(res_lo, 2 * FILTER_BITS - ROUND0_BITS);
        res16 = vsub_s16(res16, vdup_n_s16(sub_constant));

        uint8_t *const p = (uint8_t *)&pred[(i + k + 4) * p_stride + j];
        uint8x8_t val = vqmovun_s16(vcombine_s16(res16, vdup_n_s16(0)));
        vst1_lane_u32((uint32_t *)p, vreinterpret_u32_u8(val), 0);
      }
    }
  }
}

#endif  // AOM_AV1_COMMON_ARM_WARP_PLANE_NEON_H_
