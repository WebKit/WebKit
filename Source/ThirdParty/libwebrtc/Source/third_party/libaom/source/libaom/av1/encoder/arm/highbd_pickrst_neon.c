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
#include <stdint.h>

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "av1/encoder/arm/pickrst_neon.h"
#include "av1/encoder/pickrst.h"

static inline void highbd_calc_proj_params_r0_r1_neon(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int64_t H[2][2], int64_t C[2]) {
  assert(width % 8 == 0);
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);

  int64x2_t h00_lo = vdupq_n_s64(0);
  int64x2_t h00_hi = vdupq_n_s64(0);
  int64x2_t h11_lo = vdupq_n_s64(0);
  int64x2_t h11_hi = vdupq_n_s64(0);
  int64x2_t h01_lo = vdupq_n_s64(0);
  int64x2_t h01_hi = vdupq_n_s64(0);
  int64x2_t c0_lo = vdupq_n_s64(0);
  int64x2_t c0_hi = vdupq_n_s64(0);
  int64x2_t c1_lo = vdupq_n_s64(0);
  int64x2_t c1_hi = vdupq_n_s64(0);

  do {
    const uint16_t *src_ptr = src;
    const uint16_t *dat_ptr = dat;
    int32_t *flt0_ptr = flt0;
    int32_t *flt1_ptr = flt1;
    int w = width;

    do {
      uint16x8_t s = vld1q_u16(src_ptr);
      uint16x8_t d = vld1q_u16(dat_ptr);
      int32x4_t f0_lo = vld1q_s32(flt0_ptr);
      int32x4_t f0_hi = vld1q_s32(flt0_ptr + 4);
      int32x4_t f1_lo = vld1q_s32(flt1_ptr);
      int32x4_t f1_hi = vld1q_s32(flt1_ptr + 4);

      int32x4_t u_lo =
          vreinterpretq_s32_u32(vshll_n_u16(vget_low_u16(d), SGRPROJ_RST_BITS));
      int32x4_t u_hi = vreinterpretq_s32_u32(
          vshll_n_u16(vget_high_u16(d), SGRPROJ_RST_BITS));
      int32x4_t s_lo =
          vreinterpretq_s32_u32(vshll_n_u16(vget_low_u16(s), SGRPROJ_RST_BITS));
      int32x4_t s_hi = vreinterpretq_s32_u32(
          vshll_n_u16(vget_high_u16(s), SGRPROJ_RST_BITS));
      s_lo = vsubq_s32(s_lo, u_lo);
      s_hi = vsubq_s32(s_hi, u_hi);

      f0_lo = vsubq_s32(f0_lo, u_lo);
      f0_hi = vsubq_s32(f0_hi, u_hi);
      f1_lo = vsubq_s32(f1_lo, u_lo);
      f1_hi = vsubq_s32(f1_hi, u_hi);

      h00_lo = vmlal_s32(h00_lo, vget_low_s32(f0_lo), vget_low_s32(f0_lo));
      h00_lo = vmlal_s32(h00_lo, vget_high_s32(f0_lo), vget_high_s32(f0_lo));
      h00_hi = vmlal_s32(h00_hi, vget_low_s32(f0_hi), vget_low_s32(f0_hi));
      h00_hi = vmlal_s32(h00_hi, vget_high_s32(f0_hi), vget_high_s32(f0_hi));

      h11_lo = vmlal_s32(h11_lo, vget_low_s32(f1_lo), vget_low_s32(f1_lo));
      h11_lo = vmlal_s32(h11_lo, vget_high_s32(f1_lo), vget_high_s32(f1_lo));
      h11_hi = vmlal_s32(h11_hi, vget_low_s32(f1_hi), vget_low_s32(f1_hi));
      h11_hi = vmlal_s32(h11_hi, vget_high_s32(f1_hi), vget_high_s32(f1_hi));

      h01_lo = vmlal_s32(h01_lo, vget_low_s32(f0_lo), vget_low_s32(f1_lo));
      h01_lo = vmlal_s32(h01_lo, vget_high_s32(f0_lo), vget_high_s32(f1_lo));
      h01_hi = vmlal_s32(h01_hi, vget_low_s32(f0_hi), vget_low_s32(f1_hi));
      h01_hi = vmlal_s32(h01_hi, vget_high_s32(f0_hi), vget_high_s32(f1_hi));

      c0_lo = vmlal_s32(c0_lo, vget_low_s32(f0_lo), vget_low_s32(s_lo));
      c0_lo = vmlal_s32(c0_lo, vget_high_s32(f0_lo), vget_high_s32(s_lo));
      c0_hi = vmlal_s32(c0_hi, vget_low_s32(f0_hi), vget_low_s32(s_hi));
      c0_hi = vmlal_s32(c0_hi, vget_high_s32(f0_hi), vget_high_s32(s_hi));

      c1_lo = vmlal_s32(c1_lo, vget_low_s32(f1_lo), vget_low_s32(s_lo));
      c1_lo = vmlal_s32(c1_lo, vget_high_s32(f1_lo), vget_high_s32(s_lo));
      c1_hi = vmlal_s32(c1_hi, vget_low_s32(f1_hi), vget_low_s32(s_hi));
      c1_hi = vmlal_s32(c1_hi, vget_high_s32(f1_hi), vget_high_s32(s_hi));

      src_ptr += 8;
      dat_ptr += 8;
      flt0_ptr += 8;
      flt1_ptr += 8;
      w -= 8;
    } while (w != 0);

    src += src_stride;
    dat += dat_stride;
    flt0 += flt0_stride;
    flt1 += flt1_stride;
  } while (--height != 0);

  H[0][0] = horizontal_add_s64x2(vaddq_s64(h00_lo, h00_hi)) / size;
  H[0][1] = horizontal_add_s64x2(vaddq_s64(h01_lo, h01_hi)) / size;
  H[1][1] = horizontal_add_s64x2(vaddq_s64(h11_lo, h11_hi)) / size;
  H[1][0] = H[0][1];
  C[0] = horizontal_add_s64x2(vaddq_s64(c0_lo, c0_hi)) / size;
  C[1] = horizontal_add_s64x2(vaddq_s64(c1_lo, c1_hi)) / size;
}

static inline void highbd_calc_proj_params_r0_neon(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int64_t H[2][2], int64_t C[2]) {
  assert(width % 8 == 0);
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);

  int64x2_t h00_lo = vdupq_n_s64(0);
  int64x2_t h00_hi = vdupq_n_s64(0);
  int64x2_t c0_lo = vdupq_n_s64(0);
  int64x2_t c0_hi = vdupq_n_s64(0);

  do {
    const uint16_t *src_ptr = src;
    const uint16_t *dat_ptr = dat;
    int32_t *flt0_ptr = flt0;
    int w = width;

    do {
      uint16x8_t s = vld1q_u16(src_ptr);
      uint16x8_t d = vld1q_u16(dat_ptr);
      int32x4_t f0_lo = vld1q_s32(flt0_ptr);
      int32x4_t f0_hi = vld1q_s32(flt0_ptr + 4);

      int32x4_t u_lo =
          vreinterpretq_s32_u32(vshll_n_u16(vget_low_u16(d), SGRPROJ_RST_BITS));
      int32x4_t u_hi = vreinterpretq_s32_u32(
          vshll_n_u16(vget_high_u16(d), SGRPROJ_RST_BITS));
      int32x4_t s_lo =
          vreinterpretq_s32_u32(vshll_n_u16(vget_low_u16(s), SGRPROJ_RST_BITS));
      int32x4_t s_hi = vreinterpretq_s32_u32(
          vshll_n_u16(vget_high_u16(s), SGRPROJ_RST_BITS));
      s_lo = vsubq_s32(s_lo, u_lo);
      s_hi = vsubq_s32(s_hi, u_hi);

      f0_lo = vsubq_s32(f0_lo, u_lo);
      f0_hi = vsubq_s32(f0_hi, u_hi);

      h00_lo = vmlal_s32(h00_lo, vget_low_s32(f0_lo), vget_low_s32(f0_lo));
      h00_lo = vmlal_s32(h00_lo, vget_high_s32(f0_lo), vget_high_s32(f0_lo));
      h00_hi = vmlal_s32(h00_hi, vget_low_s32(f0_hi), vget_low_s32(f0_hi));
      h00_hi = vmlal_s32(h00_hi, vget_high_s32(f0_hi), vget_high_s32(f0_hi));

      c0_lo = vmlal_s32(c0_lo, vget_low_s32(f0_lo), vget_low_s32(s_lo));
      c0_lo = vmlal_s32(c0_lo, vget_high_s32(f0_lo), vget_high_s32(s_lo));
      c0_hi = vmlal_s32(c0_hi, vget_low_s32(f0_hi), vget_low_s32(s_hi));
      c0_hi = vmlal_s32(c0_hi, vget_high_s32(f0_hi), vget_high_s32(s_hi));

      src_ptr += 8;
      dat_ptr += 8;
      flt0_ptr += 8;
      w -= 8;
    } while (w != 0);

    src += src_stride;
    dat += dat_stride;
    flt0 += flt0_stride;
  } while (--height != 0);

  H[0][0] = horizontal_add_s64x2(vaddq_s64(h00_lo, h00_hi)) / size;
  C[0] = horizontal_add_s64x2(vaddq_s64(c0_lo, c0_hi)) / size;
}

static inline void highbd_calc_proj_params_r1_neon(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt1, int flt1_stride,
    int64_t H[2][2], int64_t C[2]) {
  assert(width % 8 == 0);
  const int size = width * height;
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);

  int64x2_t h11_lo = vdupq_n_s64(0);
  int64x2_t h11_hi = vdupq_n_s64(0);
  int64x2_t c1_lo = vdupq_n_s64(0);
  int64x2_t c1_hi = vdupq_n_s64(0);

  do {
    const uint16_t *src_ptr = src;
    const uint16_t *dat_ptr = dat;
    int32_t *flt1_ptr = flt1;
    int w = width;

    do {
      uint16x8_t s = vld1q_u16(src_ptr);
      uint16x8_t d = vld1q_u16(dat_ptr);
      int32x4_t f1_lo = vld1q_s32(flt1_ptr);
      int32x4_t f1_hi = vld1q_s32(flt1_ptr + 4);

      int32x4_t u_lo =
          vreinterpretq_s32_u32(vshll_n_u16(vget_low_u16(d), SGRPROJ_RST_BITS));
      int32x4_t u_hi = vreinterpretq_s32_u32(
          vshll_n_u16(vget_high_u16(d), SGRPROJ_RST_BITS));
      int32x4_t s_lo =
          vreinterpretq_s32_u32(vshll_n_u16(vget_low_u16(s), SGRPROJ_RST_BITS));
      int32x4_t s_hi = vreinterpretq_s32_u32(
          vshll_n_u16(vget_high_u16(s), SGRPROJ_RST_BITS));
      s_lo = vsubq_s32(s_lo, u_lo);
      s_hi = vsubq_s32(s_hi, u_hi);

      f1_lo = vsubq_s32(f1_lo, u_lo);
      f1_hi = vsubq_s32(f1_hi, u_hi);

      h11_lo = vmlal_s32(h11_lo, vget_low_s32(f1_lo), vget_low_s32(f1_lo));
      h11_lo = vmlal_s32(h11_lo, vget_high_s32(f1_lo), vget_high_s32(f1_lo));
      h11_hi = vmlal_s32(h11_hi, vget_low_s32(f1_hi), vget_low_s32(f1_hi));
      h11_hi = vmlal_s32(h11_hi, vget_high_s32(f1_hi), vget_high_s32(f1_hi));

      c1_lo = vmlal_s32(c1_lo, vget_low_s32(f1_lo), vget_low_s32(s_lo));
      c1_lo = vmlal_s32(c1_lo, vget_high_s32(f1_lo), vget_high_s32(s_lo));
      c1_hi = vmlal_s32(c1_hi, vget_low_s32(f1_hi), vget_low_s32(s_hi));
      c1_hi = vmlal_s32(c1_hi, vget_high_s32(f1_hi), vget_high_s32(s_hi));

      src_ptr += 8;
      dat_ptr += 8;
      flt1_ptr += 8;
      w -= 8;
    } while (w != 0);

    src += src_stride;
    dat += dat_stride;
    flt1 += flt1_stride;
  } while (--height != 0);

  H[1][1] = horizontal_add_s64x2(vaddq_s64(h11_lo, h11_hi)) / size;
  C[1] = horizontal_add_s64x2(vaddq_s64(c1_lo, c1_hi)) / size;
}

// The function calls 3 subfunctions for the following cases :
// 1) When params->r[0] > 0 and params->r[1] > 0. In this case all elements
//    of C and H need to be computed.
// 2) When only params->r[0] > 0. In this case only H[0][0] and C[0] are
//    non-zero and need to be computed.
// 3) When only params->r[1] > 0. In this case only H[1][1] and C[1] are
//    non-zero and need to be computed.
void av1_calc_proj_params_high_bd_neon(const uint8_t *src8, int width,
                                       int height, int src_stride,
                                       const uint8_t *dat8, int dat_stride,
                                       int32_t *flt0, int flt0_stride,
                                       int32_t *flt1, int flt1_stride,
                                       int64_t H[2][2], int64_t C[2],
                                       const sgr_params_type *params) {
  if ((params->r[0] > 0) && (params->r[1] > 0)) {
    highbd_calc_proj_params_r0_r1_neon(src8, width, height, src_stride, dat8,
                                       dat_stride, flt0, flt0_stride, flt1,
                                       flt1_stride, H, C);
  } else if (params->r[0] > 0) {
    highbd_calc_proj_params_r0_neon(src8, width, height, src_stride, dat8,
                                    dat_stride, flt0, flt0_stride, H, C);
  } else if (params->r[1] > 0) {
    highbd_calc_proj_params_r1_neon(src8, width, height, src_stride, dat8,
                                    dat_stride, flt1, flt1_stride, H, C);
  }
}

static inline void hadd_update_4_stats_neon(const int64_t *const src,
                                            const int32x4_t *deltas,
                                            int64_t *const dst) {
  int64x2_t delta0_s64 = vpaddlq_s32(deltas[0]);
  int64x2_t delta1_s64 = vpaddlq_s32(deltas[1]);
  int64x2_t delta2_s64 = vpaddlq_s32(deltas[2]);
  int64x2_t delta3_s64 = vpaddlq_s32(deltas[3]);

#if AOM_ARCH_AARCH64
  int64x2_t delta01 = vpaddq_s64(delta0_s64, delta1_s64);
  int64x2_t delta23 = vpaddq_s64(delta2_s64, delta3_s64);

  int64x2_t src0 = vld1q_s64(src);
  int64x2_t src1 = vld1q_s64(src + 2);
  vst1q_s64(dst, vaddq_s64(src0, delta01));
  vst1q_s64(dst + 2, vaddq_s64(src1, delta23));
#else
  dst[0] = src[0] + horizontal_add_s64x2(delta0_s64);
  dst[1] = src[1] + horizontal_add_s64x2(delta1_s64);
  dst[2] = src[2] + horizontal_add_s64x2(delta2_s64);
  dst[3] = src[3] + horizontal_add_s64x2(delta3_s64);
#endif
}

static inline void compute_stats_win5_highbd_neon(
    const int16_t *const d, const int32_t d_stride, const int16_t *const s,
    const int32_t s_stride, const int32_t width, const int32_t height,
    int64_t *const M, int64_t *const H, aom_bit_depth_t bit_depth) {
  const int32_t wiener_win = WIENER_WIN_CHROMA;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  const int32_t w16 = width & ~15;
  const int32_t h8 = height & ~7;
  int16x8_t mask[2];
  mask[0] = vld1q_s16(&(mask_16bit[16]) - width % 16);
  mask[1] = vld1q_s16(&(mask_16bit[16]) - width % 16 + 8);
  int32_t i, j, x, y;

  const int32_t num_bit_left =
      32 - 1 /* sign */ - 2 * bit_depth /* energy */ + 2 /* SIMD */;
  const int32_t h_allowed =
      (1 << num_bit_left) / (w16 + ((w16 != width) ? 16 : 0));

  // Step 1: Calculate the top edge of the whole matrix, i.e., the top
  // edge of each triangle and square on the top row.
  j = 0;
  do {
    const int16_t *s_t = s;
    const int16_t *d_t = d;
    int32_t height_t = 0;
    int64x2_t sum_m[WIENER_WIN_CHROMA] = { vdupq_n_s64(0) };
    int64x2_t sum_h[WIENER_WIN_CHROMA] = { vdupq_n_s64(0) };
    int16x8_t src[2], dgd[2];

    do {
      const int32_t h_t =
          ((height - height_t) < h_allowed) ? (height - height_t) : h_allowed;
      int32x4_t row_m[WIENER_WIN_CHROMA] = { vdupq_n_s32(0) };
      int32x4_t row_h[WIENER_WIN_CHROMA] = { vdupq_n_s32(0) };

      y = h_t;
      do {
        x = 0;
        while (x < w16) {
          src[0] = vld1q_s16(s_t + x + 0);
          src[1] = vld1q_s16(s_t + x + 8);
          dgd[0] = vld1q_s16(d_t + x + 0);
          dgd[1] = vld1q_s16(d_t + x + 8);
          stats_top_win5_neon(src, dgd, d_t + j + x, d_stride, row_m, row_h);
          x += 16;
        }

        if (w16 != width) {
          src[0] = vld1q_s16(s_t + w16 + 0);
          src[1] = vld1q_s16(s_t + w16 + 8);
          dgd[0] = vld1q_s16(d_t + w16 + 0);
          dgd[1] = vld1q_s16(d_t + w16 + 8);
          src[0] = vandq_s16(src[0], mask[0]);
          src[1] = vandq_s16(src[1], mask[1]);
          dgd[0] = vandq_s16(dgd[0], mask[0]);
          dgd[1] = vandq_s16(dgd[1], mask[1]);
          stats_top_win5_neon(src, dgd, d_t + j + w16, d_stride, row_m, row_h);
        }

        s_t += s_stride;
        d_t += d_stride;
      } while (--y);

      sum_m[0] = vpadalq_s32(sum_m[0], row_m[0]);
      sum_m[1] = vpadalq_s32(sum_m[1], row_m[1]);
      sum_m[2] = vpadalq_s32(sum_m[2], row_m[2]);
      sum_m[3] = vpadalq_s32(sum_m[3], row_m[3]);
      sum_m[4] = vpadalq_s32(sum_m[4], row_m[4]);
      sum_h[0] = vpadalq_s32(sum_h[0], row_h[0]);
      sum_h[1] = vpadalq_s32(sum_h[1], row_h[1]);
      sum_h[2] = vpadalq_s32(sum_h[2], row_h[2]);
      sum_h[3] = vpadalq_s32(sum_h[3], row_h[3]);
      sum_h[4] = vpadalq_s32(sum_h[4], row_h[4]);

      height_t += h_t;
    } while (height_t < height);

#if AOM_ARCH_AARCH64
    int64x2_t sum_m0 = vpaddq_s64(sum_m[0], sum_m[1]);
    int64x2_t sum_m2 = vpaddq_s64(sum_m[2], sum_m[3]);
    vst1q_s64(&M[wiener_win * j + 0], sum_m0);
    vst1q_s64(&M[wiener_win * j + 2], sum_m2);
    M[wiener_win * j + 4] = vaddvq_s64(sum_m[4]);

    int64x2_t sum_h0 = vpaddq_s64(sum_h[0], sum_h[1]);
    int64x2_t sum_h2 = vpaddq_s64(sum_h[2], sum_h[3]);
    vst1q_s64(&H[wiener_win * j + 0], sum_h0);
    vst1q_s64(&H[wiener_win * j + 2], sum_h2);
    H[wiener_win * j + 4] = vaddvq_s64(sum_h[4]);
#else
    M[wiener_win * j + 0] = horizontal_add_s64x2(sum_m[0]);
    M[wiener_win * j + 1] = horizontal_add_s64x2(sum_m[1]);
    M[wiener_win * j + 2] = horizontal_add_s64x2(sum_m[2]);
    M[wiener_win * j + 3] = horizontal_add_s64x2(sum_m[3]);
    M[wiener_win * j + 4] = horizontal_add_s64x2(sum_m[4]);

    H[wiener_win * j + 0] = horizontal_add_s64x2(sum_h[0]);
    H[wiener_win * j + 1] = horizontal_add_s64x2(sum_h[1]);
    H[wiener_win * j + 2] = horizontal_add_s64x2(sum_h[2]);
    H[wiener_win * j + 3] = horizontal_add_s64x2(sum_h[3]);
    H[wiener_win * j + 4] = horizontal_add_s64x2(sum_h[4]);
#endif  // AOM_ARCH_AARCH64
  } while (++j < wiener_win);

  // Step 2: Calculate the left edge of each square on the top row.
  j = 1;
  do {
    const int16_t *d_t = d;
    int32_t height_t = 0;
    int64x2_t sum_h[WIENER_WIN_CHROMA - 1] = { vdupq_n_s64(0) };
    int16x8_t dgd[2];

    do {
      const int32_t h_t =
          ((height - height_t) < h_allowed) ? (height - height_t) : h_allowed;
      int32x4_t row_h[WIENER_WIN_CHROMA - 1] = { vdupq_n_s32(0) };

      y = h_t;
      do {
        x = 0;
        while (x < w16) {
          dgd[0] = vld1q_s16(d_t + j + x + 0);
          dgd[1] = vld1q_s16(d_t + j + x + 8);
          stats_left_win5_neon(dgd, d_t + x, d_stride, row_h);
          x += 16;
        }

        if (w16 != width) {
          dgd[0] = vld1q_s16(d_t + j + x + 0);
          dgd[1] = vld1q_s16(d_t + j + x + 8);
          dgd[0] = vandq_s16(dgd[0], mask[0]);
          dgd[1] = vandq_s16(dgd[1], mask[1]);
          stats_left_win5_neon(dgd, d_t + x, d_stride, row_h);
        }

        d_t += d_stride;
      } while (--y);

      sum_h[0] = vpadalq_s32(sum_h[0], row_h[0]);
      sum_h[1] = vpadalq_s32(sum_h[1], row_h[1]);
      sum_h[2] = vpadalq_s32(sum_h[2], row_h[2]);
      sum_h[3] = vpadalq_s32(sum_h[3], row_h[3]);

      height_t += h_t;
    } while (height_t < height);

#if AOM_ARCH_AARCH64
    int64x2_t sum_h0 = vpaddq_s64(sum_h[0], sum_h[1]);
    int64x2_t sum_h1 = vpaddq_s64(sum_h[2], sum_h[3]);
    vst1_s64(&H[1 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h0));
    vst1_s64(&H[2 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h0));
    vst1_s64(&H[3 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h1));
    vst1_s64(&H[4 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h1));
#else
    H[1 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[0]);
    H[2 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[1]);
    H[3 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[2]);
    H[4 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[3]);
#endif  // AOM_ARCH_AARCH64
  } while (++j < wiener_win);

  // Step 3: Derive the top edge of each triangle along the diagonal. No
  // triangle in top row.
  {
    const int16_t *d_t = d;

    if (height % 2) {
      int32x4_t deltas[(WIENER_WIN + 1) * 2] = { vdupq_n_s32(0) };
      int32x4_t deltas_tr[(WIENER_WIN + 1) * 2] = { vdupq_n_s32(0) };
      int16x8_t ds[WIENER_WIN * 2];

      load_s16_8x4(d_t, d_stride, &ds[0], &ds[2], &ds[4], &ds[6]);
      load_s16_8x4(d_t + width, d_stride, &ds[1], &ds[3], &ds[5], &ds[7]);
      d_t += 4 * d_stride;

      step3_win5_oneline_neon(&d_t, d_stride, width, height, ds, deltas);
      transpose_arrays_s32_8x8(deltas, deltas_tr);

      update_5_stats_neon(H + 0 * wiener_win * wiener_win2 + 0 * wiener_win,
                          deltas_tr[0], vgetq_lane_s32(deltas_tr[4], 0),
                          H + 1 * wiener_win * wiener_win2 + 1 * wiener_win);

      update_5_stats_neon(H + 1 * wiener_win * wiener_win2 + 1 * wiener_win,
                          deltas_tr[1], vgetq_lane_s32(deltas_tr[5], 0),
                          H + 2 * wiener_win * wiener_win2 + 2 * wiener_win);

      update_5_stats_neon(H + 2 * wiener_win * wiener_win2 + 2 * wiener_win,
                          deltas_tr[2], vgetq_lane_s32(deltas_tr[6], 0),
                          H + 3 * wiener_win * wiener_win2 + 3 * wiener_win);

      update_5_stats_neon(H + 3 * wiener_win * wiener_win2 + 3 * wiener_win,
                          deltas_tr[3], vgetq_lane_s32(deltas_tr[7], 0),
                          H + 4 * wiener_win * wiener_win2 + 4 * wiener_win);

    } else {
      int32x4_t deltas[WIENER_WIN_CHROMA * 2] = { vdupq_n_s32(0) };
      int16x8_t ds[WIENER_WIN_CHROMA * 2];

      ds[0] = load_unaligned_s16_4x2(d_t + 0 * d_stride, width);
      ds[1] = load_unaligned_s16_4x2(d_t + 1 * d_stride, width);
      ds[2] = load_unaligned_s16_4x2(d_t + 2 * d_stride, width);
      ds[3] = load_unaligned_s16_4x2(d_t + 3 * d_stride, width);

      step3_win5_neon(d_t + 4 * d_stride, d_stride, width, height, ds, deltas);

      transpose_elems_inplace_s32_4x4(&deltas[0], &deltas[1], &deltas[2],
                                      &deltas[3]);

      update_5_stats_neon(H + 0 * wiener_win * wiener_win2 + 0 * wiener_win,
                          deltas[0], vgetq_lane_s32(deltas[4], 0),
                          H + 1 * wiener_win * wiener_win2 + 1 * wiener_win);

      update_5_stats_neon(H + 1 * wiener_win * wiener_win2 + 1 * wiener_win,
                          deltas[1], vgetq_lane_s32(deltas[4], 1),
                          H + 2 * wiener_win * wiener_win2 + 2 * wiener_win);

      update_5_stats_neon(H + 2 * wiener_win * wiener_win2 + 2 * wiener_win,
                          deltas[2], vgetq_lane_s32(deltas[4], 2),
                          H + 3 * wiener_win * wiener_win2 + 3 * wiener_win);

      update_5_stats_neon(H + 3 * wiener_win * wiener_win2 + 3 * wiener_win,
                          deltas[3], vgetq_lane_s32(deltas[4], 3),
                          H + 4 * wiener_win * wiener_win2 + 4 * wiener_win);
    }
  }

  // Step 4: Derive the top and left edge of each square. No square in top and
  // bottom row.

  {
    y = h8;

    int16x4_t d_s[12];
    int16x4_t d_e[12];
    const int16_t *d_t = d;
    int16x4_t zeros = vdup_n_s16(0);
    load_s16_4x4(d_t, d_stride, &d_s[0], &d_s[1], &d_s[2], &d_s[3]);
    load_s16_4x4(d_t + width, d_stride, &d_e[0], &d_e[1], &d_e[2], &d_e[3]);
    int32x4_t deltas[6][18] = { { vdupq_n_s32(0) }, { vdupq_n_s32(0) } };

    while (y >= 8) {
      load_s16_4x8(d_t + 4 * d_stride, d_stride, &d_s[4], &d_s[5], &d_s[6],
                   &d_s[7], &d_s[8], &d_s[9], &d_s[10], &d_s[11]);
      load_s16_4x8(d_t + width + 4 * d_stride, d_stride, &d_e[4], &d_e[5],
                   &d_e[6], &d_e[7], &d_e[8], &d_e[9], &d_e[10], &d_e[11]);

      int16x8_t s_tr[8], e_tr[8];
      transpose_elems_s16_4x8(d_s[0], d_s[1], d_s[2], d_s[3], d_s[4], d_s[5],
                              d_s[6], d_s[7], &s_tr[0], &s_tr[1], &s_tr[2],
                              &s_tr[3]);
      transpose_elems_s16_4x8(d_s[8], d_s[9], d_s[10], d_s[11], zeros, zeros,
                              zeros, zeros, &s_tr[4], &s_tr[5], &s_tr[6],
                              &s_tr[7]);

      transpose_elems_s16_4x8(d_e[0], d_e[1], d_e[2], d_e[3], d_e[4], d_e[5],
                              d_e[6], d_e[7], &e_tr[0], &e_tr[1], &e_tr[2],
                              &e_tr[3]);
      transpose_elems_s16_4x8(d_e[8], d_e[9], d_e[10], d_e[11], zeros, zeros,
                              zeros, zeros, &e_tr[4], &e_tr[5], &e_tr[6],
                              &e_tr[7]);

      int16x8_t start_col0[5], start_col1[5], start_col2[5], start_col3[5];
      start_col0[0] = s_tr[0];
      start_col0[1] = vextq_s16(s_tr[0], s_tr[4], 1);
      start_col0[2] = vextq_s16(s_tr[0], s_tr[4], 2);
      start_col0[3] = vextq_s16(s_tr[0], s_tr[4], 3);
      start_col0[4] = vextq_s16(s_tr[0], s_tr[4], 4);

      start_col1[0] = s_tr[1];
      start_col1[1] = vextq_s16(s_tr[1], s_tr[5], 1);
      start_col1[2] = vextq_s16(s_tr[1], s_tr[5], 2);
      start_col1[3] = vextq_s16(s_tr[1], s_tr[5], 3);
      start_col1[4] = vextq_s16(s_tr[1], s_tr[5], 4);

      start_col2[0] = s_tr[2];
      start_col2[1] = vextq_s16(s_tr[2], s_tr[6], 1);
      start_col2[2] = vextq_s16(s_tr[2], s_tr[6], 2);
      start_col2[3] = vextq_s16(s_tr[2], s_tr[6], 3);
      start_col2[4] = vextq_s16(s_tr[2], s_tr[6], 4);

      start_col3[0] = s_tr[3];
      start_col3[1] = vextq_s16(s_tr[3], s_tr[7], 1);
      start_col3[2] = vextq_s16(s_tr[3], s_tr[7], 2);
      start_col3[3] = vextq_s16(s_tr[3], s_tr[7], 3);
      start_col3[4] = vextq_s16(s_tr[3], s_tr[7], 4);

      // i = 1, j = 2;
      sub_deltas_step4(start_col0, start_col1, deltas[0]);

      // i = 1, j = 3;
      sub_deltas_step4(start_col0, start_col2, deltas[1]);

      // i = 1, j = 4
      sub_deltas_step4(start_col0, start_col3, deltas[2]);

      // i = 2, j =3
      sub_deltas_step4(start_col1, start_col2, deltas[3]);

      // i = 2, j = 4
      sub_deltas_step4(start_col1, start_col3, deltas[4]);

      // i = 3, j = 4
      sub_deltas_step4(start_col2, start_col3, deltas[5]);

      int16x8_t end_col0[5], end_col1[5], end_col2[5], end_col3[5];
      end_col0[0] = e_tr[0];
      end_col0[1] = vextq_s16(e_tr[0], e_tr[4], 1);
      end_col0[2] = vextq_s16(e_tr[0], e_tr[4], 2);
      end_col0[3] = vextq_s16(e_tr[0], e_tr[4], 3);
      end_col0[4] = vextq_s16(e_tr[0], e_tr[4], 4);

      end_col1[0] = e_tr[1];
      end_col1[1] = vextq_s16(e_tr[1], e_tr[5], 1);
      end_col1[2] = vextq_s16(e_tr[1], e_tr[5], 2);
      end_col1[3] = vextq_s16(e_tr[1], e_tr[5], 3);
      end_col1[4] = vextq_s16(e_tr[1], e_tr[5], 4);

      end_col2[0] = e_tr[2];
      end_col2[1] = vextq_s16(e_tr[2], e_tr[6], 1);
      end_col2[2] = vextq_s16(e_tr[2], e_tr[6], 2);
      end_col2[3] = vextq_s16(e_tr[2], e_tr[6], 3);
      end_col2[4] = vextq_s16(e_tr[2], e_tr[6], 4);

      end_col3[0] = e_tr[3];
      end_col3[1] = vextq_s16(e_tr[3], e_tr[7], 1);
      end_col3[2] = vextq_s16(e_tr[3], e_tr[7], 2);
      end_col3[3] = vextq_s16(e_tr[3], e_tr[7], 3);
      end_col3[4] = vextq_s16(e_tr[3], e_tr[7], 4);

      // i = 1, j = 2;
      add_deltas_step4(end_col0, end_col1, deltas[0]);

      // i = 1, j = 3;
      add_deltas_step4(end_col0, end_col2, deltas[1]);

      // i = 1, j = 4
      add_deltas_step4(end_col0, end_col3, deltas[2]);

      // i = 2, j =3
      add_deltas_step4(end_col1, end_col2, deltas[3]);

      // i = 2, j = 4
      add_deltas_step4(end_col1, end_col3, deltas[4]);

      // i = 3, j = 4
      add_deltas_step4(end_col2, end_col3, deltas[5]);

      d_s[0] = d_s[8];
      d_s[1] = d_s[9];
      d_s[2] = d_s[10];
      d_s[3] = d_s[11];
      d_e[0] = d_e[8];
      d_e[1] = d_e[9];
      d_e[2] = d_e[10];
      d_e[3] = d_e[11];

      d_t += 8 * d_stride;
      y -= 8;
    }

    if (h8 != height) {
      const int16x8_t mask_h = vld1q_s16(&mask_16bit[16] - (height % 8));

      load_s16_4x8(d_t + 4 * d_stride, d_stride, &d_s[4], &d_s[5], &d_s[6],
                   &d_s[7], &d_s[8], &d_s[9], &d_s[10], &d_s[11]);
      load_s16_4x8(d_t + width + 4 * d_stride, d_stride, &d_e[4], &d_e[5],
                   &d_e[6], &d_e[7], &d_e[8], &d_e[9], &d_e[10], &d_e[11]);
      int16x8_t s_tr[8], e_tr[8];
      transpose_elems_s16_4x8(d_s[0], d_s[1], d_s[2], d_s[3], d_s[4], d_s[5],
                              d_s[6], d_s[7], &s_tr[0], &s_tr[1], &s_tr[2],
                              &s_tr[3]);
      transpose_elems_s16_4x8(d_s[8], d_s[9], d_s[10], d_s[11], zeros, zeros,
                              zeros, zeros, &s_tr[4], &s_tr[5], &s_tr[6],
                              &s_tr[7]);
      transpose_elems_s16_4x8(d_e[0], d_e[1], d_e[2], d_e[3], d_e[4], d_e[5],
                              d_e[6], d_e[7], &e_tr[0], &e_tr[1], &e_tr[2],
                              &e_tr[3]);
      transpose_elems_s16_4x8(d_e[8], d_e[9], d_e[10], d_e[11], zeros, zeros,
                              zeros, zeros, &e_tr[4], &e_tr[5], &e_tr[6],
                              &e_tr[7]);

      int16x8_t start_col0[5], start_col1[5], start_col2[5], start_col3[5];
      start_col0[0] = vandq_s16(s_tr[0], mask_h);
      start_col0[1] = vandq_s16(vextq_s16(s_tr[0], s_tr[4], 1), mask_h);
      start_col0[2] = vandq_s16(vextq_s16(s_tr[0], s_tr[4], 2), mask_h);
      start_col0[3] = vandq_s16(vextq_s16(s_tr[0], s_tr[4], 3), mask_h);
      start_col0[4] = vandq_s16(vextq_s16(s_tr[0], s_tr[4], 4), mask_h);

      start_col1[0] = vandq_s16(s_tr[1], mask_h);
      start_col1[1] = vandq_s16(vextq_s16(s_tr[1], s_tr[5], 1), mask_h);
      start_col1[2] = vandq_s16(vextq_s16(s_tr[1], s_tr[5], 2), mask_h);
      start_col1[3] = vandq_s16(vextq_s16(s_tr[1], s_tr[5], 3), mask_h);
      start_col1[4] = vandq_s16(vextq_s16(s_tr[1], s_tr[5], 4), mask_h);

      start_col2[0] = vandq_s16(s_tr[2], mask_h);
      start_col2[1] = vandq_s16(vextq_s16(s_tr[2], s_tr[6], 1), mask_h);
      start_col2[2] = vandq_s16(vextq_s16(s_tr[2], s_tr[6], 2), mask_h);
      start_col2[3] = vandq_s16(vextq_s16(s_tr[2], s_tr[6], 3), mask_h);
      start_col2[4] = vandq_s16(vextq_s16(s_tr[2], s_tr[6], 4), mask_h);

      start_col3[0] = vandq_s16(s_tr[3], mask_h);
      start_col3[1] = vandq_s16(vextq_s16(s_tr[3], s_tr[7], 1), mask_h);
      start_col3[2] = vandq_s16(vextq_s16(s_tr[3], s_tr[7], 2), mask_h);
      start_col3[3] = vandq_s16(vextq_s16(s_tr[3], s_tr[7], 3), mask_h);
      start_col3[4] = vandq_s16(vextq_s16(s_tr[3], s_tr[7], 4), mask_h);

      // i = 1, j = 2;
      sub_deltas_step4(start_col0, start_col1, deltas[0]);

      // i = 1, j = 3;
      sub_deltas_step4(start_col0, start_col2, deltas[1]);

      // i = 1, j = 4
      sub_deltas_step4(start_col0, start_col3, deltas[2]);

      // i = 2, j = 3
      sub_deltas_step4(start_col1, start_col2, deltas[3]);

      // i = 2, j = 4
      sub_deltas_step4(start_col1, start_col3, deltas[4]);

      // i = 3, j = 4
      sub_deltas_step4(start_col2, start_col3, deltas[5]);

      int16x8_t end_col0[5], end_col1[5], end_col2[5], end_col3[5];
      end_col0[0] = vandq_s16(e_tr[0], mask_h);
      end_col0[1] = vandq_s16(vextq_s16(e_tr[0], e_tr[4], 1), mask_h);
      end_col0[2] = vandq_s16(vextq_s16(e_tr[0], e_tr[4], 2), mask_h);
      end_col0[3] = vandq_s16(vextq_s16(e_tr[0], e_tr[4], 3), mask_h);
      end_col0[4] = vandq_s16(vextq_s16(e_tr[0], e_tr[4], 4), mask_h);

      end_col1[0] = vandq_s16(e_tr[1], mask_h);
      end_col1[1] = vandq_s16(vextq_s16(e_tr[1], e_tr[5], 1), mask_h);
      end_col1[2] = vandq_s16(vextq_s16(e_tr[1], e_tr[5], 2), mask_h);
      end_col1[3] = vandq_s16(vextq_s16(e_tr[1], e_tr[5], 3), mask_h);
      end_col1[4] = vandq_s16(vextq_s16(e_tr[1], e_tr[5], 4), mask_h);

      end_col2[0] = vandq_s16(e_tr[2], mask_h);
      end_col2[1] = vandq_s16(vextq_s16(e_tr[2], e_tr[6], 1), mask_h);
      end_col2[2] = vandq_s16(vextq_s16(e_tr[2], e_tr[6], 2), mask_h);
      end_col2[3] = vandq_s16(vextq_s16(e_tr[2], e_tr[6], 3), mask_h);
      end_col2[4] = vandq_s16(vextq_s16(e_tr[2], e_tr[6], 4), mask_h);

      end_col3[0] = vandq_s16(e_tr[3], mask_h);
      end_col3[1] = vandq_s16(vextq_s16(e_tr[3], e_tr[7], 1), mask_h);
      end_col3[2] = vandq_s16(vextq_s16(e_tr[3], e_tr[7], 2), mask_h);
      end_col3[3] = vandq_s16(vextq_s16(e_tr[3], e_tr[7], 3), mask_h);
      end_col3[4] = vandq_s16(vextq_s16(e_tr[3], e_tr[7], 4), mask_h);

      // i = 1, j = 2;
      add_deltas_step4(end_col0, end_col1, deltas[0]);

      // i = 1, j = 3;
      add_deltas_step4(end_col0, end_col2, deltas[1]);

      // i = 1, j = 4
      add_deltas_step4(end_col0, end_col3, deltas[2]);

      // i = 2, j =3
      add_deltas_step4(end_col1, end_col2, deltas[3]);

      // i = 2, j = 4
      add_deltas_step4(end_col1, end_col3, deltas[4]);

      // i = 3, j = 4
      add_deltas_step4(end_col2, end_col3, deltas[5]);
    }

    int32x4_t delta[6][2];
    int32_t single_delta[6];

    delta[0][0] = horizontal_add_4d_s32x4(&deltas[0][0]);
    delta[1][0] = horizontal_add_4d_s32x4(&deltas[1][0]);
    delta[2][0] = horizontal_add_4d_s32x4(&deltas[2][0]);
    delta[3][0] = horizontal_add_4d_s32x4(&deltas[3][0]);
    delta[4][0] = horizontal_add_4d_s32x4(&deltas[4][0]);
    delta[5][0] = horizontal_add_4d_s32x4(&deltas[5][0]);

    delta[0][1] = horizontal_add_4d_s32x4(&deltas[0][5]);
    delta[1][1] = horizontal_add_4d_s32x4(&deltas[1][5]);
    delta[2][1] = horizontal_add_4d_s32x4(&deltas[2][5]);
    delta[3][1] = horizontal_add_4d_s32x4(&deltas[3][5]);
    delta[4][1] = horizontal_add_4d_s32x4(&deltas[4][5]);
    delta[5][1] = horizontal_add_4d_s32x4(&deltas[5][5]);

    single_delta[0] = horizontal_add_s32x4(deltas[0][4]);
    single_delta[1] = horizontal_add_s32x4(deltas[1][4]);
    single_delta[2] = horizontal_add_s32x4(deltas[2][4]);
    single_delta[3] = horizontal_add_s32x4(deltas[3][4]);
    single_delta[4] = horizontal_add_s32x4(deltas[4][4]);
    single_delta[5] = horizontal_add_s32x4(deltas[5][4]);

    int idx = 0;
    for (i = 1; i < wiener_win - 1; i++) {
      for (j = i + 1; j < wiener_win; j++) {
        update_4_stats_neon(
            H + (i - 1) * wiener_win * wiener_win2 + (j - 1) * wiener_win,
            delta[idx][0], H + i * wiener_win * wiener_win2 + j * wiener_win);
        H[i * wiener_win * wiener_win2 + j * wiener_win + 4] =
            H[(i - 1) * wiener_win * wiener_win2 + (j - 1) * wiener_win + 4] +
            single_delta[idx];

        H[(i * wiener_win + 1) * wiener_win2 + j * wiener_win] =
            H[((i - 1) * wiener_win + 1) * wiener_win2 + (j - 1) * wiener_win] +
            vgetq_lane_s32(delta[idx][1], 0);
        H[(i * wiener_win + 2) * wiener_win2 + j * wiener_win] =
            H[((i - 1) * wiener_win + 2) * wiener_win2 + (j - 1) * wiener_win] +
            vgetq_lane_s32(delta[idx][1], 1);
        H[(i * wiener_win + 3) * wiener_win2 + j * wiener_win] =
            H[((i - 1) * wiener_win + 3) * wiener_win2 + (j - 1) * wiener_win] +
            vgetq_lane_s32(delta[idx][1], 2);
        H[(i * wiener_win + 4) * wiener_win2 + j * wiener_win] =
            H[((i - 1) * wiener_win + 4) * wiener_win2 + (j - 1) * wiener_win] +
            vgetq_lane_s32(delta[idx][1], 3);

        idx++;
      }
    }
  }

  // Step 5: Derive other points of each square. No square in bottom row.
  i = 0;
  do {
    const int16_t *const di = d + i;

    j = i + 1;
    do {
      const int16_t *const dj = d + j;
      int32x4_t deltas[WIENER_WIN_CHROMA - 1][WIENER_WIN_CHROMA - 1] = {
        { vdupq_n_s32(0) }, { vdupq_n_s32(0) }
      };
      int16x8_t d_is[WIN_CHROMA], d_ie[WIN_CHROMA];
      int16x8_t d_js[WIN_CHROMA], d_je[WIN_CHROMA];

      x = 0;
      while (x < w16) {
        load_square_win5_neon(di + x, dj + x, d_stride, height, d_is, d_ie,
                              d_js, d_je);
        derive_square_win5_neon(d_is, d_ie, d_js, d_je, deltas);
        x += 16;
      }

      if (w16 != width) {
        load_square_win5_neon(di + x, dj + x, d_stride, height, d_is, d_ie,
                              d_js, d_je);
        d_is[0] = vandq_s16(d_is[0], mask[0]);
        d_is[1] = vandq_s16(d_is[1], mask[1]);
        d_is[2] = vandq_s16(d_is[2], mask[0]);
        d_is[3] = vandq_s16(d_is[3], mask[1]);
        d_is[4] = vandq_s16(d_is[4], mask[0]);
        d_is[5] = vandq_s16(d_is[5], mask[1]);
        d_is[6] = vandq_s16(d_is[6], mask[0]);
        d_is[7] = vandq_s16(d_is[7], mask[1]);
        d_ie[0] = vandq_s16(d_ie[0], mask[0]);
        d_ie[1] = vandq_s16(d_ie[1], mask[1]);
        d_ie[2] = vandq_s16(d_ie[2], mask[0]);
        d_ie[3] = vandq_s16(d_ie[3], mask[1]);
        d_ie[4] = vandq_s16(d_ie[4], mask[0]);
        d_ie[5] = vandq_s16(d_ie[5], mask[1]);
        d_ie[6] = vandq_s16(d_ie[6], mask[0]);
        d_ie[7] = vandq_s16(d_ie[7], mask[1]);
        derive_square_win5_neon(d_is, d_ie, d_js, d_je, deltas);
      }

      hadd_update_4_stats_neon(
          H + (i * wiener_win + 0) * wiener_win2 + j * wiener_win, deltas[0],
          H + (i * wiener_win + 1) * wiener_win2 + j * wiener_win + 1);
      hadd_update_4_stats_neon(
          H + (i * wiener_win + 1) * wiener_win2 + j * wiener_win, deltas[1],
          H + (i * wiener_win + 2) * wiener_win2 + j * wiener_win + 1);
      hadd_update_4_stats_neon(
          H + (i * wiener_win + 2) * wiener_win2 + j * wiener_win, deltas[2],
          H + (i * wiener_win + 3) * wiener_win2 + j * wiener_win + 1);
      hadd_update_4_stats_neon(
          H + (i * wiener_win + 3) * wiener_win2 + j * wiener_win, deltas[3],
          H + (i * wiener_win + 4) * wiener_win2 + j * wiener_win + 1);
    } while (++j < wiener_win);
  } while (++i < wiener_win - 1);

  // Step 6: Derive other points of each upper triangle along the diagonal.
  i = 0;
  do {
    const int16_t *const di = d + i;
    int32x4_t deltas[WIENER_WIN_CHROMA * 2 + 1] = { vdupq_n_s32(0) };
    int16x8_t d_is[WIN_CHROMA], d_ie[WIN_CHROMA];

    x = 0;
    while (x < w16) {
      load_triangle_win5_neon(di + x, d_stride, height, d_is, d_ie);
      derive_triangle_win5_neon(d_is, d_ie, deltas);
      x += 16;
    }

    if (w16 != width) {
      load_triangle_win5_neon(di + x, d_stride, height, d_is, d_ie);
      d_is[0] = vandq_s16(d_is[0], mask[0]);
      d_is[1] = vandq_s16(d_is[1], mask[1]);
      d_is[2] = vandq_s16(d_is[2], mask[0]);
      d_is[3] = vandq_s16(d_is[3], mask[1]);
      d_is[4] = vandq_s16(d_is[4], mask[0]);
      d_is[5] = vandq_s16(d_is[5], mask[1]);
      d_is[6] = vandq_s16(d_is[6], mask[0]);
      d_is[7] = vandq_s16(d_is[7], mask[1]);
      d_ie[0] = vandq_s16(d_ie[0], mask[0]);
      d_ie[1] = vandq_s16(d_ie[1], mask[1]);
      d_ie[2] = vandq_s16(d_ie[2], mask[0]);
      d_ie[3] = vandq_s16(d_ie[3], mask[1]);
      d_ie[4] = vandq_s16(d_ie[4], mask[0]);
      d_ie[5] = vandq_s16(d_ie[5], mask[1]);
      d_ie[6] = vandq_s16(d_ie[6], mask[0]);
      d_ie[7] = vandq_s16(d_ie[7], mask[1]);
      derive_triangle_win5_neon(d_is, d_ie, deltas);
    }

    // Row 1: 4 points
    hadd_update_4_stats_neon(
        H + (i * wiener_win + 0) * wiener_win2 + i * wiener_win, deltas,
        H + (i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1);

    // Row 2: 3 points
    int64x2_t delta4_s64 = vpaddlq_s32(deltas[4]);
    int64x2_t delta5_s64 = vpaddlq_s32(deltas[5]);

#if AOM_ARCH_AARCH64
    int64x2_t deltas45 = vpaddq_s64(delta4_s64, delta5_s64);
    int64x2_t src =
        vld1q_s64(H + (i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1);
    int64x2_t dst = vaddq_s64(src, deltas45);
    vst1q_s64(H + (i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2, dst);
#else
    H[(i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2 + 0] =
        H[(i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1 + 0] +
        horizontal_add_s64x2(delta4_s64);
    H[(i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2 + 1] =
        H[(i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1 + 1] +
        horizontal_add_s64x2(delta5_s64);
#endif  // AOM_ARCH_AARCH64

    H[(i * wiener_win + 2) * wiener_win2 + i * wiener_win + 4] =
        H[(i * wiener_win + 1) * wiener_win2 + i * wiener_win + 3] +
        horizontal_long_add_s32x4(deltas[6]);

    // Row 3: 2 points
    int64x2_t delta7_s64 = vpaddlq_s32(deltas[7]);
    int64x2_t delta8_s64 = vpaddlq_s32(deltas[8]);

#if AOM_ARCH_AARCH64
    int64x2_t deltas78 = vpaddq_s64(delta7_s64, delta8_s64);
    vst1q_s64(H + (i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3,
              vaddq_s64(dst, deltas78));
#else
    H[(i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3 + 0] =
        H[(i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2 + 0] +
        horizontal_add_s64x2(delta7_s64);
    H[(i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3 + 1] =
        H[(i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2 + 1] +
        horizontal_add_s64x2(delta8_s64);
#endif  // AOM_ARCH_AARCH64

    // Row 4: 1 point
    H[(i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4] =
        H[(i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3] +
        horizontal_long_add_s32x4(deltas[9]);
  } while (++i < wiener_win);
}

static inline void hadd_update_6_stats_neon(const int64_t *const src,
                                            const int32x4_t *deltas,
                                            int64_t *const dst) {
  int64x2_t delta0_s64 = vpaddlq_s32(deltas[0]);
  int64x2_t delta1_s64 = vpaddlq_s32(deltas[1]);
  int64x2_t delta2_s64 = vpaddlq_s32(deltas[2]);
  int64x2_t delta3_s64 = vpaddlq_s32(deltas[3]);
  int64x2_t delta4_s64 = vpaddlq_s32(deltas[4]);
  int64x2_t delta5_s64 = vpaddlq_s32(deltas[5]);

#if AOM_ARCH_AARCH64
  int64x2_t delta01 = vpaddq_s64(delta0_s64, delta1_s64);
  int64x2_t delta23 = vpaddq_s64(delta2_s64, delta3_s64);
  int64x2_t delta45 = vpaddq_s64(delta4_s64, delta5_s64);

  int64x2_t src0 = vld1q_s64(src);
  int64x2_t src1 = vld1q_s64(src + 2);
  int64x2_t src2 = vld1q_s64(src + 4);

  vst1q_s64(dst, vaddq_s64(src0, delta01));
  vst1q_s64(dst + 2, vaddq_s64(src1, delta23));
  vst1q_s64(dst + 4, vaddq_s64(src2, delta45));
#else
  dst[0] = src[0] + horizontal_add_s64x2(delta0_s64);
  dst[1] = src[1] + horizontal_add_s64x2(delta1_s64);
  dst[2] = src[2] + horizontal_add_s64x2(delta2_s64);
  dst[3] = src[3] + horizontal_add_s64x2(delta3_s64);
  dst[4] = src[4] + horizontal_add_s64x2(delta4_s64);
  dst[5] = src[5] + horizontal_add_s64x2(delta5_s64);
#endif
}

static inline void compute_stats_win7_highbd_neon(
    const int16_t *const d, const int32_t d_stride, const int16_t *const s,
    const int32_t s_stride, const int32_t width, const int32_t height,
    int64_t *const M, int64_t *const H, aom_bit_depth_t bit_depth) {
  const int32_t wiener_win = WIENER_WIN;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  const int32_t w16 = width & ~15;
  const int32_t h8 = height & ~7;
  int16x8_t mask[2];
  mask[0] = vld1q_s16(&(mask_16bit[16]) - width % 16);
  mask[1] = vld1q_s16(&(mask_16bit[16]) - width % 16 + 8);
  int32_t i, j, x, y;

  const int32_t num_bit_left =
      32 - 1 /* sign */ - 2 * bit_depth /* energy */ + 2 /* SIMD */;
  const int32_t h_allowed =
      (1 << num_bit_left) / (w16 + ((w16 != width) ? 16 : 0));

  // Step 1: Calculate the top edge of the whole matrix, i.e., the top
  // edge of each triangle and square on the top row.
  j = 0;
  do {
    const int16_t *s_t = s;
    const int16_t *d_t = d;
    int32_t height_t = 0;
    int64x2_t sum_m[WIENER_WIN] = { vdupq_n_s64(0) };
    int64x2_t sum_h[WIENER_WIN] = { vdupq_n_s64(0) };
    int16x8_t src[2], dgd[2];

    do {
      const int32_t h_t =
          ((height - height_t) < h_allowed) ? (height - height_t) : h_allowed;
      int32x4_t row_m[WIENER_WIN * 2] = { vdupq_n_s32(0) };
      int32x4_t row_h[WIENER_WIN * 2] = { vdupq_n_s32(0) };

      y = h_t;
      do {
        x = 0;
        while (x < w16) {
          src[0] = vld1q_s16(s_t + x);
          src[1] = vld1q_s16(s_t + x + 8);
          dgd[0] = vld1q_s16(d_t + x);
          dgd[1] = vld1q_s16(d_t + x + 8);
          stats_top_win7_neon(src, dgd, d_t + j + x, d_stride, row_m, row_h);
          x += 16;
        }

        if (w16 != width) {
          src[0] = vld1q_s16(s_t + w16);
          src[1] = vld1q_s16(s_t + w16 + 8);
          dgd[0] = vld1q_s16(d_t + w16);
          dgd[1] = vld1q_s16(d_t + w16 + 8);
          src[0] = vandq_s16(src[0], mask[0]);
          src[1] = vandq_s16(src[1], mask[1]);
          dgd[0] = vandq_s16(dgd[0], mask[0]);
          dgd[1] = vandq_s16(dgd[1], mask[1]);
          stats_top_win7_neon(src, dgd, d_t + j + w16, d_stride, row_m, row_h);
        }

        s_t += s_stride;
        d_t += d_stride;
      } while (--y);

      sum_m[0] = vpadalq_s32(sum_m[0], row_m[0]);
      sum_m[1] = vpadalq_s32(sum_m[1], row_m[1]);
      sum_m[2] = vpadalq_s32(sum_m[2], row_m[2]);
      sum_m[3] = vpadalq_s32(sum_m[3], row_m[3]);
      sum_m[4] = vpadalq_s32(sum_m[4], row_m[4]);
      sum_m[5] = vpadalq_s32(sum_m[5], row_m[5]);
      sum_m[6] = vpadalq_s32(sum_m[6], row_m[6]);

      sum_h[0] = vpadalq_s32(sum_h[0], row_h[0]);
      sum_h[1] = vpadalq_s32(sum_h[1], row_h[1]);
      sum_h[2] = vpadalq_s32(sum_h[2], row_h[2]);
      sum_h[3] = vpadalq_s32(sum_h[3], row_h[3]);
      sum_h[4] = vpadalq_s32(sum_h[4], row_h[4]);
      sum_h[5] = vpadalq_s32(sum_h[5], row_h[5]);
      sum_h[6] = vpadalq_s32(sum_h[6], row_h[6]);

      height_t += h_t;
    } while (height_t < height);

#if AOM_ARCH_AARCH64
    vst1q_s64(M + wiener_win * j + 0, vpaddq_s64(sum_m[0], sum_m[1]));
    vst1q_s64(M + wiener_win * j + 2, vpaddq_s64(sum_m[2], sum_m[3]));
    vst1q_s64(M + wiener_win * j + 4, vpaddq_s64(sum_m[4], sum_m[5]));
    M[wiener_win * j + 6] = vaddvq_s64(sum_m[6]);

    vst1q_s64(H + wiener_win * j + 0, vpaddq_s64(sum_h[0], sum_h[1]));
    vst1q_s64(H + wiener_win * j + 2, vpaddq_s64(sum_h[2], sum_h[3]));
    vst1q_s64(H + wiener_win * j + 4, vpaddq_s64(sum_h[4], sum_h[5]));
    H[wiener_win * j + 6] = vaddvq_s64(sum_h[6]);
#else
    M[wiener_win * j + 0] = horizontal_add_s64x2(sum_m[0]);
    M[wiener_win * j + 1] = horizontal_add_s64x2(sum_m[1]);
    M[wiener_win * j + 2] = horizontal_add_s64x2(sum_m[2]);
    M[wiener_win * j + 3] = horizontal_add_s64x2(sum_m[3]);
    M[wiener_win * j + 4] = horizontal_add_s64x2(sum_m[4]);
    M[wiener_win * j + 5] = horizontal_add_s64x2(sum_m[5]);
    M[wiener_win * j + 6] = horizontal_add_s64x2(sum_m[6]);

    H[wiener_win * j + 0] = horizontal_add_s64x2(sum_h[0]);
    H[wiener_win * j + 1] = horizontal_add_s64x2(sum_h[1]);
    H[wiener_win * j + 2] = horizontal_add_s64x2(sum_h[2]);
    H[wiener_win * j + 3] = horizontal_add_s64x2(sum_h[3]);
    H[wiener_win * j + 4] = horizontal_add_s64x2(sum_h[4]);
    H[wiener_win * j + 5] = horizontal_add_s64x2(sum_h[5]);
    H[wiener_win * j + 6] = horizontal_add_s64x2(sum_h[6]);
#endif  // AOM_ARCH_AARCH64
  } while (++j < wiener_win);

  // Step 2: Calculate the left edge of each square on the top row.
  j = 1;
  do {
    const int16_t *d_t = d;
    int32_t height_t = 0;
    int64x2_t sum_h[WIENER_WIN - 1] = { vdupq_n_s64(0) };
    int16x8_t dgd[2];

    do {
      const int32_t h_t =
          ((height - height_t) < h_allowed) ? (height - height_t) : h_allowed;
      int32x4_t row_h[WIENER_WIN - 1] = { vdupq_n_s32(0) };

      y = h_t;
      do {
        x = 0;
        while (x < w16) {
          dgd[0] = vld1q_s16(d_t + j + x + 0);
          dgd[1] = vld1q_s16(d_t + j + x + 8);
          stats_left_win7_neon(dgd, d_t + x, d_stride, row_h);
          x += 16;
        }

        if (w16 != width) {
          dgd[0] = vld1q_s16(d_t + j + x + 0);
          dgd[1] = vld1q_s16(d_t + j + x + 8);
          dgd[0] = vandq_s16(dgd[0], mask[0]);
          dgd[1] = vandq_s16(dgd[1], mask[1]);
          stats_left_win7_neon(dgd, d_t + x, d_stride, row_h);
        }

        d_t += d_stride;
      } while (--y);

      sum_h[0] = vpadalq_s32(sum_h[0], row_h[0]);
      sum_h[1] = vpadalq_s32(sum_h[1], row_h[1]);
      sum_h[2] = vpadalq_s32(sum_h[2], row_h[2]);
      sum_h[3] = vpadalq_s32(sum_h[3], row_h[3]);
      sum_h[4] = vpadalq_s32(sum_h[4], row_h[4]);
      sum_h[5] = vpadalq_s32(sum_h[5], row_h[5]);

      height_t += h_t;
    } while (height_t < height);

#if AOM_ARCH_AARCH64
    int64x2_t sum_h0 = vpaddq_s64(sum_h[0], sum_h[1]);
    int64x2_t sum_h2 = vpaddq_s64(sum_h[2], sum_h[3]);
    int64x2_t sum_h4 = vpaddq_s64(sum_h[4], sum_h[5]);
    vst1_s64(&H[1 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h0));
    vst1_s64(&H[2 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h0));
    vst1_s64(&H[3 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h2));
    vst1_s64(&H[4 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h2));
    vst1_s64(&H[5 * wiener_win2 + j * wiener_win], vget_low_s64(sum_h4));
    vst1_s64(&H[6 * wiener_win2 + j * wiener_win], vget_high_s64(sum_h4));
#else
    H[1 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[0]);
    H[2 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[1]);
    H[3 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[2]);
    H[4 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[3]);
    H[5 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[4]);
    H[6 * wiener_win2 + j * wiener_win] = horizontal_add_s64x2(sum_h[5]);
#endif  // AOM_ARCH_AARCH64

  } while (++j < wiener_win);

  // Step 3: Derive the top edge of each triangle along the diagonal. No
  // triangle in top row.
  {
    const int16_t *d_t = d;
    // Pad to call transpose function.
    int32x4_t deltas[(WIENER_WIN + 1) * 2] = { vdupq_n_s32(0) };
    int32x4_t deltas_tr[(WIENER_WIN + 1) * 2] = { vdupq_n_s32(0) };
    int16x8_t ds[WIENER_WIN * 2];

    load_s16_8x6(d_t, d_stride, &ds[0], &ds[2], &ds[4], &ds[6], &ds[8],
                 &ds[10]);
    load_s16_8x6(d_t + width, d_stride, &ds[1], &ds[3], &ds[5], &ds[7], &ds[9],
                 &ds[11]);

    d_t += 6 * d_stride;

    step3_win7_neon(d_t, d_stride, width, height, ds, deltas);
    transpose_arrays_s32_8x8(deltas, deltas_tr);

    update_8_stats_neon(H + 0 * wiener_win * wiener_win2 + 0 * wiener_win,
                        deltas_tr[0], deltas_tr[4],
                        H + 1 * wiener_win * wiener_win2 + 1 * wiener_win);
    update_8_stats_neon(H + 1 * wiener_win * wiener_win2 + 1 * wiener_win,
                        deltas_tr[1], deltas_tr[5],
                        H + 2 * wiener_win * wiener_win2 + 2 * wiener_win);
    update_8_stats_neon(H + 2 * wiener_win * wiener_win2 + 2 * wiener_win,
                        deltas_tr[2], deltas_tr[6],
                        H + 3 * wiener_win * wiener_win2 + 3 * wiener_win);
    update_8_stats_neon(H + 3 * wiener_win * wiener_win2 + 3 * wiener_win,
                        deltas_tr[3], deltas_tr[7],
                        H + 4 * wiener_win * wiener_win2 + 4 * wiener_win);
    update_8_stats_neon(H + 4 * wiener_win * wiener_win2 + 4 * wiener_win,
                        deltas_tr[8], deltas_tr[12],
                        H + 5 * wiener_win * wiener_win2 + 5 * wiener_win);
    update_8_stats_neon(H + 5 * wiener_win * wiener_win2 + 5 * wiener_win,
                        deltas_tr[9], deltas_tr[13],
                        H + 6 * wiener_win * wiener_win2 + 6 * wiener_win);
  }

  // Step 4: Derive the top and left edge of each square. No square in top and
  // bottom row.

  i = 1;
  do {
    j = i + 1;
    do {
      const int16_t *di = d + i - 1;
      const int16_t *dj = d + j - 1;
      int32x4_t deltas[(2 * WIENER_WIN - 1) * 2] = { vdupq_n_s32(0) };
      int16x8_t dd[WIENER_WIN * 2], ds[WIENER_WIN * 2];

      dd[5] = vdupq_n_s16(0);  // Initialize to avoid warning.
      const int16_t dd0_values[] = { di[0 * d_stride],
                                     di[1 * d_stride],
                                     di[2 * d_stride],
                                     di[3 * d_stride],
                                     di[4 * d_stride],
                                     di[5 * d_stride],
                                     0,
                                     0 };
      dd[0] = vld1q_s16(dd0_values);
      const int16_t dd1_values[] = { di[0 * d_stride + width],
                                     di[1 * d_stride + width],
                                     di[2 * d_stride + width],
                                     di[3 * d_stride + width],
                                     di[4 * d_stride + width],
                                     di[5 * d_stride + width],
                                     0,
                                     0 };
      dd[1] = vld1q_s16(dd1_values);
      const int16_t ds0_values[] = { dj[0 * d_stride],
                                     dj[1 * d_stride],
                                     dj[2 * d_stride],
                                     dj[3 * d_stride],
                                     dj[4 * d_stride],
                                     dj[5 * d_stride],
                                     0,
                                     0 };
      ds[0] = vld1q_s16(ds0_values);
      int16_t ds1_values[] = { dj[0 * d_stride + width],
                               dj[1 * d_stride + width],
                               dj[2 * d_stride + width],
                               dj[3 * d_stride + width],
                               dj[4 * d_stride + width],
                               dj[5 * d_stride + width],
                               0,
                               0 };
      ds[1] = vld1q_s16(ds1_values);

      y = 0;
      while (y < h8) {
        // 00s 10s 20s 30s 40s 50s 60s 70s  00e 10e 20e 30e 40e 50e 60e 70e
        dd[0] = vsetq_lane_s16(di[6 * d_stride], dd[0], 6);
        dd[0] = vsetq_lane_s16(di[7 * d_stride], dd[0], 7);
        dd[1] = vsetq_lane_s16(di[6 * d_stride + width], dd[1], 6);
        dd[1] = vsetq_lane_s16(di[7 * d_stride + width], dd[1], 7);

        // 00s 10s 20s 30s 40s 50s 60s 70s  00e 10e 20e 30e 40e 50e 60e 70e
        // 01s 11s 21s 31s 41s 51s 61s 71s  01e 11e 21e 31e 41e 51e 61e 71e
        ds[0] = vsetq_lane_s16(dj[6 * d_stride], ds[0], 6);
        ds[0] = vsetq_lane_s16(dj[7 * d_stride], ds[0], 7);
        ds[1] = vsetq_lane_s16(dj[6 * d_stride + width], ds[1], 6);
        ds[1] = vsetq_lane_s16(dj[7 * d_stride + width], ds[1], 7);

        load_more_16_neon(di + 8 * d_stride, width, &dd[0], &dd[2]);
        load_more_16_neon(dj + 8 * d_stride, width, &ds[0], &ds[2]);
        load_more_16_neon(di + 9 * d_stride, width, &dd[2], &dd[4]);
        load_more_16_neon(dj + 9 * d_stride, width, &ds[2], &ds[4]);
        load_more_16_neon(di + 10 * d_stride, width, &dd[4], &dd[6]);
        load_more_16_neon(dj + 10 * d_stride, width, &ds[4], &ds[6]);
        load_more_16_neon(di + 11 * d_stride, width, &dd[6], &dd[8]);
        load_more_16_neon(dj + 11 * d_stride, width, &ds[6], &ds[8]);
        load_more_16_neon(di + 12 * d_stride, width, &dd[8], &dd[10]);
        load_more_16_neon(dj + 12 * d_stride, width, &ds[8], &ds[10]);
        load_more_16_neon(di + 13 * d_stride, width, &dd[10], &dd[12]);
        load_more_16_neon(dj + 13 * d_stride, width, &ds[10], &ds[12]);

        madd_neon(&deltas[0], dd[0], ds[0]);
        madd_neon(&deltas[1], dd[1], ds[1]);
        madd_neon(&deltas[2], dd[0], ds[2]);
        madd_neon(&deltas[3], dd[1], ds[3]);
        madd_neon(&deltas[4], dd[0], ds[4]);
        madd_neon(&deltas[5], dd[1], ds[5]);
        madd_neon(&deltas[6], dd[0], ds[6]);
        madd_neon(&deltas[7], dd[1], ds[7]);
        madd_neon(&deltas[8], dd[0], ds[8]);
        madd_neon(&deltas[9], dd[1], ds[9]);
        madd_neon(&deltas[10], dd[0], ds[10]);
        madd_neon(&deltas[11], dd[1], ds[11]);
        madd_neon(&deltas[12], dd[0], ds[12]);
        madd_neon(&deltas[13], dd[1], ds[13]);
        madd_neon(&deltas[14], dd[2], ds[0]);
        madd_neon(&deltas[15], dd[3], ds[1]);
        madd_neon(&deltas[16], dd[4], ds[0]);
        madd_neon(&deltas[17], dd[5], ds[1]);
        madd_neon(&deltas[18], dd[6], ds[0]);
        madd_neon(&deltas[19], dd[7], ds[1]);
        madd_neon(&deltas[20], dd[8], ds[0]);
        madd_neon(&deltas[21], dd[9], ds[1]);
        madd_neon(&deltas[22], dd[10], ds[0]);
        madd_neon(&deltas[23], dd[11], ds[1]);
        madd_neon(&deltas[24], dd[12], ds[0]);
        madd_neon(&deltas[25], dd[13], ds[1]);

        dd[0] = vextq_s16(dd[12], vdupq_n_s16(0), 2);
        dd[1] = vextq_s16(dd[13], vdupq_n_s16(0), 2);
        ds[0] = vextq_s16(ds[12], vdupq_n_s16(0), 2);
        ds[1] = vextq_s16(ds[13], vdupq_n_s16(0), 2);

        di += 8 * d_stride;
        dj += 8 * d_stride;
        y += 8;
      }

      deltas[0] = hadd_four_32_neon(deltas[0], deltas[2], deltas[4], deltas[6]);
      deltas[1] = hadd_four_32_neon(deltas[1], deltas[3], deltas[5], deltas[7]);
      deltas[2] =
          hadd_four_32_neon(deltas[8], deltas[10], deltas[12], deltas[12]);
      deltas[3] =
          hadd_four_32_neon(deltas[9], deltas[11], deltas[13], deltas[13]);
      deltas[4] =
          hadd_four_32_neon(deltas[14], deltas[16], deltas[18], deltas[20]);
      deltas[5] =
          hadd_four_32_neon(deltas[15], deltas[17], deltas[19], deltas[21]);
      deltas[6] =
          hadd_four_32_neon(deltas[22], deltas[24], deltas[22], deltas[24]);
      deltas[7] =
          hadd_four_32_neon(deltas[23], deltas[25], deltas[23], deltas[25]);
      deltas[0] = vsubq_s32(deltas[1], deltas[0]);
      deltas[1] = vsubq_s32(deltas[3], deltas[2]);
      deltas[2] = vsubq_s32(deltas[5], deltas[4]);
      deltas[3] = vsubq_s32(deltas[7], deltas[6]);

      if (h8 != height) {
        const int16_t ds0_vals[] = {
          dj[0 * d_stride], dj[0 * d_stride + width],
          dj[1 * d_stride], dj[1 * d_stride + width],
          dj[2 * d_stride], dj[2 * d_stride + width],
          dj[3 * d_stride], dj[3 * d_stride + width]
        };
        ds[0] = vld1q_s16(ds0_vals);

        ds[1] = vsetq_lane_s16(dj[4 * d_stride], ds[1], 0);
        ds[1] = vsetq_lane_s16(dj[4 * d_stride + width], ds[1], 1);
        ds[1] = vsetq_lane_s16(dj[5 * d_stride], ds[1], 2);
        ds[1] = vsetq_lane_s16(dj[5 * d_stride + width], ds[1], 3);
        const int16_t dd4_vals[] = {
          -di[1 * d_stride], di[1 * d_stride + width],
          -di[2 * d_stride], di[2 * d_stride + width],
          -di[3 * d_stride], di[3 * d_stride + width],
          -di[4 * d_stride], di[4 * d_stride + width]
        };
        dd[4] = vld1q_s16(dd4_vals);

        dd[5] = vsetq_lane_s16(-di[5 * d_stride], dd[5], 0);
        dd[5] = vsetq_lane_s16(di[5 * d_stride + width], dd[5], 1);
        do {
          dd[0] = vdupq_n_s16(-di[0 * d_stride]);
          dd[2] = dd[3] = vdupq_n_s16(di[0 * d_stride + width]);
          dd[0] = dd[1] = vzipq_s16(dd[0], dd[2]).val[0];

          ds[4] = vdupq_n_s16(dj[0 * d_stride]);
          ds[6] = ds[7] = vdupq_n_s16(dj[0 * d_stride + width]);
          ds[4] = ds[5] = vzipq_s16(ds[4], ds[6]).val[0];

          dd[5] = vsetq_lane_s16(-di[6 * d_stride], dd[5], 2);
          dd[5] = vsetq_lane_s16(di[6 * d_stride + width], dd[5], 3);
          ds[1] = vsetq_lane_s16(dj[6 * d_stride], ds[1], 4);
          ds[1] = vsetq_lane_s16(dj[6 * d_stride + width], ds[1], 5);

          madd_neon_pairwise(&deltas[0], dd[0], ds[0]);
          madd_neon_pairwise(&deltas[1], dd[1], ds[1]);
          madd_neon_pairwise(&deltas[2], dd[4], ds[4]);
          madd_neon_pairwise(&deltas[3], dd[5], ds[5]);

          int32_t tmp0 = vgetq_lane_s32(vreinterpretq_s32_s16(ds[0]), 0);
          ds[0] = vextq_s16(ds[0], ds[1], 2);
          ds[1] = vextq_s16(ds[1], ds[0], 2);
          ds[1] = vreinterpretq_s16_s32(
              vsetq_lane_s32(tmp0, vreinterpretq_s32_s16(ds[1]), 3));
          int32_t tmp1 = vgetq_lane_s32(vreinterpretq_s32_s16(dd[4]), 0);
          dd[4] = vextq_s16(dd[4], dd[5], 2);
          dd[5] = vextq_s16(dd[5], dd[4], 2);
          dd[5] = vreinterpretq_s16_s32(
              vsetq_lane_s32(tmp1, vreinterpretq_s32_s16(dd[5]), 3));
          di += d_stride;
          dj += d_stride;
        } while (++y < height);
      }

      // Writing one more element on the top edge of a square falls to
      // the next square in the same row or the first element in the next
      // row, which will just be overwritten later.
      update_8_stats_neon(
          H + (i - 1) * wiener_win * wiener_win2 + (j - 1) * wiener_win,
          deltas[0], deltas[1],
          H + i * wiener_win * wiener_win2 + j * wiener_win);

      H[(i * wiener_win + 1) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 1) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[2], 0);
      H[(i * wiener_win + 2) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 2) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[2], 1);
      H[(i * wiener_win + 3) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 3) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[2], 2);
      H[(i * wiener_win + 4) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 4) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[2], 3);
      H[(i * wiener_win + 5) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 5) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[3], 0);
      H[(i * wiener_win + 6) * wiener_win2 + j * wiener_win] =
          H[((i - 1) * wiener_win + 6) * wiener_win2 + (j - 1) * wiener_win] +
          vgetq_lane_s32(deltas[3], 1);
    } while (++j < wiener_win);
  } while (++i < wiener_win - 1);

  // Step 5: Derive other points of each square. No square in bottom row.
  i = 0;
  do {
    const int16_t *const di = d + i;

    j = i + 1;
    do {
      const int16_t *const dj = d + j;
      int32x4_t deltas[WIENER_WIN - 1][WIN_7] = { { vdupq_n_s32(0) },
                                                  { vdupq_n_s32(0) } };
      int16x8_t d_is[WIN_7];
      int16x8_t d_ie[WIN_7];
      int16x8_t d_js[WIN_7];
      int16x8_t d_je[WIN_7];

      x = 0;
      while (x < w16) {
        load_square_win7_neon(di + x, dj + x, d_stride, height, d_is, d_ie,
                              d_js, d_je);
        derive_square_win7_neon(d_is, d_ie, d_js, d_je, deltas);
        x += 16;
      }

      if (w16 != width) {
        load_square_win7_neon(di + x, dj + x, d_stride, height, d_is, d_ie,
                              d_js, d_je);
        d_is[0] = vandq_s16(d_is[0], mask[0]);
        d_is[1] = vandq_s16(d_is[1], mask[1]);
        d_is[2] = vandq_s16(d_is[2], mask[0]);
        d_is[3] = vandq_s16(d_is[3], mask[1]);
        d_is[4] = vandq_s16(d_is[4], mask[0]);
        d_is[5] = vandq_s16(d_is[5], mask[1]);
        d_is[6] = vandq_s16(d_is[6], mask[0]);
        d_is[7] = vandq_s16(d_is[7], mask[1]);
        d_is[8] = vandq_s16(d_is[8], mask[0]);
        d_is[9] = vandq_s16(d_is[9], mask[1]);
        d_is[10] = vandq_s16(d_is[10], mask[0]);
        d_is[11] = vandq_s16(d_is[11], mask[1]);
        d_ie[0] = vandq_s16(d_ie[0], mask[0]);
        d_ie[1] = vandq_s16(d_ie[1], mask[1]);
        d_ie[2] = vandq_s16(d_ie[2], mask[0]);
        d_ie[3] = vandq_s16(d_ie[3], mask[1]);
        d_ie[4] = vandq_s16(d_ie[4], mask[0]);
        d_ie[5] = vandq_s16(d_ie[5], mask[1]);
        d_ie[6] = vandq_s16(d_ie[6], mask[0]);
        d_ie[7] = vandq_s16(d_ie[7], mask[1]);
        d_ie[8] = vandq_s16(d_ie[8], mask[0]);
        d_ie[9] = vandq_s16(d_ie[9], mask[1]);
        d_ie[10] = vandq_s16(d_ie[10], mask[0]);
        d_ie[11] = vandq_s16(d_ie[11], mask[1]);
        derive_square_win7_neon(d_is, d_ie, d_js, d_je, deltas);
      }

      hadd_update_6_stats_neon(
          H + (i * wiener_win + 0) * wiener_win2 + j * wiener_win, deltas[0],
          H + (i * wiener_win + 1) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 1) * wiener_win2 + j * wiener_win, deltas[1],
          H + (i * wiener_win + 2) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 2) * wiener_win2 + j * wiener_win, deltas[2],
          H + (i * wiener_win + 3) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 3) * wiener_win2 + j * wiener_win, deltas[3],
          H + (i * wiener_win + 4) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 4) * wiener_win2 + j * wiener_win, deltas[4],
          H + (i * wiener_win + 5) * wiener_win2 + j * wiener_win + 1);
      hadd_update_6_stats_neon(
          H + (i * wiener_win + 5) * wiener_win2 + j * wiener_win, deltas[5],
          H + (i * wiener_win + 6) * wiener_win2 + j * wiener_win + 1);
    } while (++j < wiener_win);
  } while (++i < wiener_win - 1);

  // Step 6: Derive other points of each upper triangle along the diagonal.
  i = 0;
  do {
    const int16_t *const di = d + i;
    int32x4_t deltas[WIENER_WIN * (WIENER_WIN - 1)] = { vdupq_n_s32(0) };
    int16x8_t d_is[WIN_7], d_ie[WIN_7];

    x = 0;
    while (x < w16) {
      load_triangle_win7_neon(di + x, d_stride, height, d_is, d_ie);
      derive_triangle_win7_neon(d_is, d_ie, deltas);
      x += 16;
    }

    if (w16 != width) {
      load_triangle_win7_neon(di + x, d_stride, height, d_is, d_ie);
      d_is[0] = vandq_s16(d_is[0], mask[0]);
      d_is[1] = vandq_s16(d_is[1], mask[1]);
      d_is[2] = vandq_s16(d_is[2], mask[0]);
      d_is[3] = vandq_s16(d_is[3], mask[1]);
      d_is[4] = vandq_s16(d_is[4], mask[0]);
      d_is[5] = vandq_s16(d_is[5], mask[1]);
      d_is[6] = vandq_s16(d_is[6], mask[0]);
      d_is[7] = vandq_s16(d_is[7], mask[1]);
      d_is[8] = vandq_s16(d_is[8], mask[0]);
      d_is[9] = vandq_s16(d_is[9], mask[1]);
      d_is[10] = vandq_s16(d_is[10], mask[0]);
      d_is[11] = vandq_s16(d_is[11], mask[1]);
      d_ie[0] = vandq_s16(d_ie[0], mask[0]);
      d_ie[1] = vandq_s16(d_ie[1], mask[1]);
      d_ie[2] = vandq_s16(d_ie[2], mask[0]);
      d_ie[3] = vandq_s16(d_ie[3], mask[1]);
      d_ie[4] = vandq_s16(d_ie[4], mask[0]);
      d_ie[5] = vandq_s16(d_ie[5], mask[1]);
      d_ie[6] = vandq_s16(d_ie[6], mask[0]);
      d_ie[7] = vandq_s16(d_ie[7], mask[1]);
      d_ie[8] = vandq_s16(d_ie[8], mask[0]);
      d_ie[9] = vandq_s16(d_ie[9], mask[1]);
      d_ie[10] = vandq_s16(d_ie[10], mask[0]);
      d_ie[11] = vandq_s16(d_ie[11], mask[1]);
      derive_triangle_win7_neon(d_is, d_ie, deltas);
    }

    // Row 1: 6 points
    hadd_update_6_stats_neon(
        H + (i * wiener_win + 0) * wiener_win2 + i * wiener_win, deltas,
        H + (i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1);

    // Row 2: 5 points
    hadd_update_4_stats_neon(
        H + (i * wiener_win + 1) * wiener_win2 + i * wiener_win + 1, deltas + 6,
        H + (i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2);
    H[(i * wiener_win + 2) * wiener_win2 + i * wiener_win + 6] =
        H[(i * wiener_win + 1) * wiener_win2 + i * wiener_win + 5] +
        horizontal_long_add_s32x4(deltas[10]);

    // Row 3: 4 points
    hadd_update_4_stats_neon(
        H + (i * wiener_win + 2) * wiener_win2 + i * wiener_win + 2,
        deltas + 11,
        H + (i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3);

    // Row 4: 3 points
#if AOM_ARCH_AARCH64
    int64x2_t delta15_s64 = vpaddlq_s32(deltas[15]);
    int64x2_t delta16_s64 = vpaddlq_s32(deltas[16]);
    int64x2_t delta1516 = vpaddq_s64(delta15_s64, delta16_s64);

    int64x2_t h0 =
        vld1q_s64(H + (i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3);
    vst1q_s64(H + (i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4,
              vaddq_s64(h0, delta1516));
#else
    H[(i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4 + 0] =
        H[(i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3 + 0] +
        horizontal_long_add_s32x4(deltas[15]);
    H[(i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4 + 1] =
        H[(i * wiener_win + 3) * wiener_win2 + i * wiener_win + 3 + 1] +
        horizontal_long_add_s32x4(deltas[16]);
#endif  // AOM_ARCH_AARCH64

    H[(i * wiener_win + 4) * wiener_win2 + i * wiener_win + 6] =
        H[(i * wiener_win + 3) * wiener_win2 + i * wiener_win + 5] +
        horizontal_long_add_s32x4(deltas[17]);

    // Row 5: 2 points
    int64x2_t delta18_s64 = vpaddlq_s32(deltas[18]);
    int64x2_t delta19_s64 = vpaddlq_s32(deltas[19]);

#if AOM_ARCH_AARCH64
    int64x2_t delta1819 = vpaddq_s64(delta18_s64, delta19_s64);

    int64x2_t h1 =
        vld1q_s64(H + (i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4);
    vst1q_s64(H + (i * wiener_win + 5) * wiener_win2 + i * wiener_win + 5,
              vaddq_s64(h1, delta1819));
#else
    H[(i * wiener_win + 5) * wiener_win2 + i * wiener_win + 5] =
        H[(i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4] +
        horizontal_add_s64x2(delta18_s64);
    H[(i * wiener_win + 5) * wiener_win2 + i * wiener_win + 5 + 1] =
        H[(i * wiener_win + 4) * wiener_win2 + i * wiener_win + 4 + 1] +
        horizontal_add_s64x2(delta19_s64);
#endif  // AOM_ARCH_AARCH64

    // Row 6: 1 points
    H[(i * wiener_win + 6) * wiener_win2 + i * wiener_win + 6] =
        H[(i * wiener_win + 5) * wiener_win2 + i * wiener_win + 5] +
        horizontal_long_add_s32x4(deltas[20]);
  } while (++i < wiener_win);
}

static inline void sub_avg_block_highbd_neon(const uint16_t *src,
                                             const int32_t src_stride,
                                             const uint16_t avg,
                                             const int32_t width,
                                             const int32_t height, int16_t *dst,
                                             const int32_t dst_stride) {
  const uint16x8_t a = vdupq_n_u16(avg);

  int32_t i = height + 1;
  do {
    int32_t j = 0;
    while (j < width) {
      const uint16x8_t s = vld1q_u16(src + j);
      const uint16x8_t d = vsubq_u16(s, a);
      vst1q_s16(dst + j, vreinterpretq_s16_u16(d));
      j += 8;
    }

    src += src_stride;
    dst += dst_stride;
  } while (--i);
}

static inline uint16_t highbd_find_average_neon(const uint16_t *src,
                                                int src_stride, int width,
                                                int height) {
  assert(width > 0);
  assert(height > 0);

  uint64x2_t sum_u64 = vdupq_n_u64(0);
  uint64_t sum = 0;
  const uint16x8_t mask =
      vreinterpretq_u16_s16(vld1q_s16(&mask_16bit[16] - (width % 8)));

  int h = height;
  do {
    uint32x4_t sum_u32[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

    int w = width;
    const uint16_t *row = src;
    while (w >= 32) {
      uint16x8_t s0 = vld1q_u16(row + 0);
      uint16x8_t s1 = vld1q_u16(row + 8);
      uint16x8_t s2 = vld1q_u16(row + 16);
      uint16x8_t s3 = vld1q_u16(row + 24);

      s0 = vaddq_u16(s0, s1);
      s2 = vaddq_u16(s2, s3);
      sum_u32[0] = vpadalq_u16(sum_u32[0], s0);
      sum_u32[1] = vpadalq_u16(sum_u32[1], s2);

      row += 32;
      w -= 32;
    }

    if (w >= 16) {
      uint16x8_t s0 = vld1q_u16(row + 0);
      uint16x8_t s1 = vld1q_u16(row + 8);

      s0 = vaddq_u16(s0, s1);
      sum_u32[0] = vpadalq_u16(sum_u32[0], s0);

      row += 16;
      w -= 16;
    }

    if (w >= 8) {
      uint16x8_t s0 = vld1q_u16(row);
      sum_u32[1] = vpadalq_u16(sum_u32[1], s0);

      row += 8;
      w -= 8;
    }

    if (w) {
      uint16x8_t s0 = vandq_u16(vld1q_u16(row), mask);
      sum_u32[1] = vpadalq_u16(sum_u32[1], s0);

      row += 8;
      w -= 8;
    }

    sum_u64 = vpadalq_u32(sum_u64, vaddq_u32(sum_u32[0], sum_u32[1]));

    src += src_stride;
  } while (--h != 0);

  return (uint16_t)((horizontal_add_u64x2(sum_u64) + sum) / (height * width));
}

void av1_compute_stats_highbd_neon(int32_t wiener_win, const uint8_t *dgd8,
                                   const uint8_t *src8, int16_t *dgd_avg,
                                   int16_t *src_avg, int32_t h_start,
                                   int32_t h_end, int32_t v_start,
                                   int32_t v_end, int32_t dgd_stride,
                                   int32_t src_stride, int64_t *M, int64_t *H,
                                   aom_bit_depth_t bit_depth) {
  const int32_t wiener_win2 = wiener_win * wiener_win;
  const int32_t wiener_halfwin = (wiener_win >> 1);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const int32_t width = h_end - h_start;
  const int32_t height = v_end - v_start;
  const uint16_t *dgd_start = dgd + h_start + v_start * dgd_stride;
  const uint16_t avg =
      highbd_find_average_neon(dgd_start, dgd_stride, width, height);
  const int32_t d_stride = (width + 2 * wiener_halfwin + 15) & ~15;
  const int32_t s_stride = (width + 15) & ~15;

  sub_avg_block_highbd_neon(src + v_start * src_stride + h_start, src_stride,
                            avg, width, height, src_avg, s_stride);
  sub_avg_block_highbd_neon(
      dgd + (v_start - wiener_halfwin) * dgd_stride + h_start - wiener_halfwin,
      dgd_stride, avg, width + 2 * wiener_halfwin, height + 2 * wiener_halfwin,
      dgd_avg, d_stride);

  if (wiener_win == WIENER_WIN) {
    compute_stats_win7_highbd_neon(dgd_avg, d_stride, src_avg, s_stride, width,
                                   height, M, H, bit_depth);
  } else if (wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_win5_highbd_neon(dgd_avg, d_stride, src_avg, s_stride, width,
                                   height, M, H, bit_depth);
  }

  // H is a symmetric matrix, so we only need to fill out the upper triangle.
  // We can copy it down to the lower triangle outside the (i, j) loops.
  if (bit_depth == AOM_BITS_8) {
    diagonal_copy_stats_neon(wiener_win2, H);
  } else if (bit_depth == AOM_BITS_10) {  // bit_depth == AOM_BITS_10
    const int32_t k4 = wiener_win2 & ~3;

    int32_t k = 0;
    do {
      int64x2_t dst = div4_neon(vld1q_s64(M + k));
      vst1q_s64(M + k, dst);
      dst = div4_neon(vld1q_s64(M + k + 2));
      vst1q_s64(M + k + 2, dst);
      H[k * wiener_win2 + k] /= 4;
      k += 4;
    } while (k < k4);

    H[k * wiener_win2 + k] /= 4;

    for (; k < wiener_win2; ++k) {
      M[k] /= 4;
    }

    div4_diagonal_copy_stats_neon(wiener_win2, H);
  } else {  // bit_depth == AOM_BITS_12
    const int32_t k4 = wiener_win2 & ~3;

    int32_t k = 0;
    do {
      int64x2_t dst = div16_neon(vld1q_s64(M + k));
      vst1q_s64(M + k, dst);
      dst = div16_neon(vld1q_s64(M + k + 2));
      vst1q_s64(M + k + 2, dst);
      H[k * wiener_win2 + k] /= 16;
      k += 4;
    } while (k < k4);

    H[k * wiener_win2 + k] /= 16;

    for (; k < wiener_win2; ++k) {
      M[k] /= 16;
    }

    div16_diagonal_copy_stats_neon(wiener_win2, H);
  }
}
int64_t av1_highbd_pixel_proj_error_neon(
    const uint8_t *src8, int width, int height, int src_stride,
    const uint8_t *dat8, int dat_stride, int32_t *flt0, int flt0_stride,
    int32_t *flt1, int flt1_stride, int xq[2], const sgr_params_type *params) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dat = CONVERT_TO_SHORTPTR(dat8);
  int64_t sse = 0;
  int64x2_t sse_s64 = vdupq_n_s64(0);

  if (params->r[0] > 0 && params->r[1] > 0) {
    int32x2_t xq_v = vld1_s32(xq);
    int32x2_t xq_sum_v = vshl_n_s32(vpadd_s32(xq_v, xq_v), 4);

    do {
      int j = 0;
      int32x4_t sse_s32 = vdupq_n_s32(0);

      do {
        const uint16x8_t d = vld1q_u16(&dat[j]);
        const uint16x8_t s = vld1q_u16(&src[j]);
        int32x4_t flt0_0 = vld1q_s32(&flt0[j]);
        int32x4_t flt0_1 = vld1q_s32(&flt0[j + 4]);
        int32x4_t flt1_0 = vld1q_s32(&flt1[j]);
        int32x4_t flt1_1 = vld1q_s32(&flt1[j + 4]);

        int32x4_t d_s32_lo = vreinterpretq_s32_u32(
            vmull_lane_u16(vget_low_u16(d), vreinterpret_u16_s32(xq_sum_v), 0));
        int32x4_t d_s32_hi = vreinterpretq_s32_u32(vmull_lane_u16(
            vget_high_u16(d), vreinterpret_u16_s32(xq_sum_v), 0));

        int32x4_t v0 = vsubq_s32(
            vdupq_n_s32(1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1)),
            d_s32_lo);
        int32x4_t v1 = vsubq_s32(
            vdupq_n_s32(1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1)),
            d_s32_hi);

        v0 = vmlaq_lane_s32(v0, flt0_0, xq_v, 0);
        v1 = vmlaq_lane_s32(v1, flt0_1, xq_v, 0);
        v0 = vmlaq_lane_s32(v0, flt1_0, xq_v, 1);
        v1 = vmlaq_lane_s32(v1, flt1_1, xq_v, 1);

        int16x4_t vr0 = vshrn_n_s32(v0, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS);
        int16x4_t vr1 = vshrn_n_s32(v1, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS);

        int16x8_t e = vaddq_s16(vcombine_s16(vr0, vr1),
                                vreinterpretq_s16_u16(vsubq_u16(d, s)));
        int16x4_t e_lo = vget_low_s16(e);
        int16x4_t e_hi = vget_high_s16(e);

        sse_s32 = vmlal_s16(sse_s32, e_lo, e_lo);
        sse_s32 = vmlal_s16(sse_s32, e_hi, e_hi);

        j += 8;
      } while (j <= width - 8);

      for (int k = j; k < width; ++k) {
        int32_t v = 1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1);
        v += xq[0] * (flt0[k]) + xq[1] * (flt1[k]);
        v -= (xq[1] + xq[0]) * (int32_t)(dat[k] << 4);
        int32_t e =
            (v >> (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS)) + dat[k] - src[k];
        sse += ((int64_t)e * e);
      }

      sse_s64 = vpadalq_s32(sse_s64, sse_s32);

      dat += dat_stride;
      src += src_stride;
      flt0 += flt0_stride;
      flt1 += flt1_stride;
    } while (--height != 0);
  } else if (params->r[0] > 0 || params->r[1] > 0) {
    int xq_active = (params->r[0] > 0) ? xq[0] : xq[1];
    int32_t *flt = (params->r[0] > 0) ? flt0 : flt1;
    int flt_stride = (params->r[0] > 0) ? flt0_stride : flt1_stride;
    int32x4_t xq_v = vdupq_n_s32(xq_active);

    do {
      int j = 0;
      int32x4_t sse_s32 = vdupq_n_s32(0);
      do {
        const uint16x8_t d0 = vld1q_u16(&dat[j]);
        const uint16x8_t s0 = vld1q_u16(&src[j]);
        int32x4_t flt0_0 = vld1q_s32(&flt[j]);
        int32x4_t flt0_1 = vld1q_s32(&flt[j + 4]);

        uint16x8_t d_u16 = vshlq_n_u16(d0, 4);
        int32x4_t sub0 = vreinterpretq_s32_u32(
            vsubw_u16(vreinterpretq_u32_s32(flt0_0), vget_low_u16(d_u16)));
        int32x4_t sub1 = vreinterpretq_s32_u32(
            vsubw_u16(vreinterpretq_u32_s32(flt0_1), vget_high_u16(d_u16)));

        int32x4_t v0 = vmlaq_s32(
            vdupq_n_s32(1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1)), sub0,
            xq_v);
        int32x4_t v1 = vmlaq_s32(
            vdupq_n_s32(1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1)), sub1,
            xq_v);

        int16x4_t vr0 = vshrn_n_s32(v0, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS);
        int16x4_t vr1 = vshrn_n_s32(v1, SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS);

        int16x8_t e = vaddq_s16(vcombine_s16(vr0, vr1),
                                vreinterpretq_s16_u16(vsubq_u16(d0, s0)));
        int16x4_t e_lo = vget_low_s16(e);
        int16x4_t e_hi = vget_high_s16(e);

        sse_s32 = vmlal_s16(sse_s32, e_lo, e_lo);
        sse_s32 = vmlal_s16(sse_s32, e_hi, e_hi);

        j += 8;
      } while (j <= width - 8);

      for (int k = j; k < width; ++k) {
        int32_t v = 1 << (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS - 1);
        v += xq_active * (int32_t)((uint32_t)flt[k] - (uint16_t)(dat[k] << 4));
        const int32_t e =
            (v >> (SGRPROJ_RST_BITS + SGRPROJ_PRJ_BITS)) + dat[k] - src[k];
        sse += ((int64_t)e * e);
      }

      sse_s64 = vpadalq_s32(sse_s64, sse_s32);

      dat += dat_stride;
      flt += flt_stride;
      src += src_stride;
    } while (--height != 0);
  } else {
    do {
      int j = 0;

      do {
        const uint16x8_t d = vld1q_u16(&dat[j]);
        const uint16x8_t s = vld1q_u16(&src[j]);

        uint16x8_t diff = vabdq_u16(d, s);
        uint16x4_t diff_lo = vget_low_u16(diff);
        uint16x4_t diff_hi = vget_high_u16(diff);

        uint32x4_t sqr_lo = vmull_u16(diff_lo, diff_lo);
        uint32x4_t sqr_hi = vmull_u16(diff_hi, diff_hi);

        sse_s64 = vpadalq_s32(sse_s64, vreinterpretq_s32_u32(sqr_lo));
        sse_s64 = vpadalq_s32(sse_s64, vreinterpretq_s32_u32(sqr_hi));

        j += 8;
      } while (j <= width - 8);

      for (int k = j; k < width; ++k) {
        int32_t e = dat[k] - src[k];
        sse += e * e;
      }

      dat += dat_stride;
      src += src_stride;
    } while (--height != 0);
  }

  sse += horizontal_add_s64x2(sse_s64);
  return sse;
}
