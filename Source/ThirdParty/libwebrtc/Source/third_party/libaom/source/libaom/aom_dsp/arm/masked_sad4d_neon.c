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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "mem_neon.h"
#include "sum_neon.h"

static INLINE uint16x8_t masked_sad_16x1_neon(uint16x8_t sad,
                                              const uint8x16_t s0,
                                              const uint8x16_t a0,
                                              const uint8x16_t b0,
                                              const uint8x16_t m0) {
  uint8x16_t m0_inv = vsubq_u8(vdupq_n_u8(AOM_BLEND_A64_MAX_ALPHA), m0);
  uint16x8_t blend_u16_lo = vmull_u8(vget_low_u8(m0), vget_low_u8(a0));
  uint16x8_t blend_u16_hi = vmull_u8(vget_high_u8(m0), vget_high_u8(a0));
  blend_u16_lo = vmlal_u8(blend_u16_lo, vget_low_u8(m0_inv), vget_low_u8(b0));
  blend_u16_hi = vmlal_u8(blend_u16_hi, vget_high_u8(m0_inv), vget_high_u8(b0));

  uint8x8_t blend_u8_lo = vrshrn_n_u16(blend_u16_lo, AOM_BLEND_A64_ROUND_BITS);
  uint8x8_t blend_u8_hi = vrshrn_n_u16(blend_u16_hi, AOM_BLEND_A64_ROUND_BITS);
  uint8x16_t blend_u8 = vcombine_u8(blend_u8_lo, blend_u8_hi);
  return vpadalq_u8(sad, vabdq_u8(blend_u8, s0));
}

static INLINE void masked_inv_sadwxhx4d_large_neon(
    const uint8_t *src, int src_stride, const uint8_t *const ref[4],
    int ref_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, uint32_t res[4], int width, int height, int h_overflow) {
  uint32x4_t sum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0) };
  int h_limit = height > h_overflow ? h_overflow : height;

  int ref_offset = 0;
  int i = 0;
  do {
    uint16x8_t sum_lo[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                             vdupq_n_u16(0) };
    uint16x8_t sum_hi[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                             vdupq_n_u16(0) };

    do {
      int j = 0;
      do {
        uint8x16_t s0 = vld1q_u8(src + j);
        uint8x16_t p0 = vld1q_u8(second_pred + j);
        uint8x16_t m0 = vld1q_u8(mask + j);
        sum_lo[0] = masked_sad_16x1_neon(sum_lo[0], s0, p0,
                                         vld1q_u8(ref[0] + ref_offset + j), m0);
        sum_lo[1] = masked_sad_16x1_neon(sum_lo[1], s0, p0,
                                         vld1q_u8(ref[1] + ref_offset + j), m0);
        sum_lo[2] = masked_sad_16x1_neon(sum_lo[2], s0, p0,
                                         vld1q_u8(ref[2] + ref_offset + j), m0);
        sum_lo[3] = masked_sad_16x1_neon(sum_lo[3], s0, p0,
                                         vld1q_u8(ref[3] + ref_offset + j), m0);

        uint8x16_t s1 = vld1q_u8(src + j + 16);
        uint8x16_t p1 = vld1q_u8(second_pred + j + 16);
        uint8x16_t m1 = vld1q_u8(mask + j + 16);
        sum_hi[0] = masked_sad_16x1_neon(
            sum_hi[0], s1, p1, vld1q_u8(ref[0] + ref_offset + j + 16), m1);
        sum_hi[1] = masked_sad_16x1_neon(
            sum_hi[1], s1, p1, vld1q_u8(ref[1] + ref_offset + j + 16), m1);
        sum_hi[2] = masked_sad_16x1_neon(
            sum_hi[2], s1, p1, vld1q_u8(ref[2] + ref_offset + j + 16), m1);
        sum_hi[3] = masked_sad_16x1_neon(
            sum_hi[3], s1, p1, vld1q_u8(ref[3] + ref_offset + j + 16), m1);

        j += 32;
      } while (j < width);

      src += src_stride;
      ref_offset += ref_stride;
      second_pred += width;
      mask += mask_stride;
    } while (++i < h_limit);

    sum[0] = vpadalq_u16(sum[0], sum_lo[0]);
    sum[0] = vpadalq_u16(sum[0], sum_hi[0]);
    sum[1] = vpadalq_u16(sum[1], sum_lo[1]);
    sum[1] = vpadalq_u16(sum[1], sum_hi[1]);
    sum[2] = vpadalq_u16(sum[2], sum_lo[2]);
    sum[2] = vpadalq_u16(sum[2], sum_hi[2]);
    sum[3] = vpadalq_u16(sum[3], sum_lo[3]);
    sum[3] = vpadalq_u16(sum[3], sum_hi[3]);

    h_limit += h_overflow;
  } while (i < height);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}

static INLINE void masked_inv_sad128xhx4d_neon(
    const uint8_t *src, int src_stride, const uint8_t *const ref[4],
    int ref_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, uint32_t res[4], int h) {
  masked_inv_sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, second_pred,
                                  mask, mask_stride, res, 128, h, 32);
}

static INLINE void masked_inv_sad64xhx4d_neon(
    const uint8_t *src, int src_stride, const uint8_t *const ref[4],
    int ref_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, uint32_t res[4], int h) {
  masked_inv_sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, second_pred,
                                  mask, mask_stride, res, 64, h, 64);
}

static INLINE void masked_sadwxhx4d_large_neon(
    const uint8_t *src, int src_stride, const uint8_t *const ref[4],
    int ref_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, uint32_t res[4], int width, int height, int h_overflow) {
  uint32x4_t sum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0) };
  int h_limit = height > h_overflow ? h_overflow : height;

  int ref_offset = 0;
  int i = 0;
  do {
    uint16x8_t sum_lo[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                             vdupq_n_u16(0) };
    uint16x8_t sum_hi[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                             vdupq_n_u16(0) };

    do {
      int j = 0;
      do {
        uint8x16_t s0 = vld1q_u8(src + j);
        uint8x16_t p0 = vld1q_u8(second_pred + j);
        uint8x16_t m0 = vld1q_u8(mask + j);
        sum_lo[0] = masked_sad_16x1_neon(
            sum_lo[0], s0, vld1q_u8(ref[0] + ref_offset + j), p0, m0);
        sum_lo[1] = masked_sad_16x1_neon(
            sum_lo[1], s0, vld1q_u8(ref[1] + ref_offset + j), p0, m0);
        sum_lo[2] = masked_sad_16x1_neon(
            sum_lo[2], s0, vld1q_u8(ref[2] + ref_offset + j), p0, m0);
        sum_lo[3] = masked_sad_16x1_neon(
            sum_lo[3], s0, vld1q_u8(ref[3] + ref_offset + j), p0, m0);

        uint8x16_t s1 = vld1q_u8(src + j + 16);
        uint8x16_t p1 = vld1q_u8(second_pred + j + 16);
        uint8x16_t m1 = vld1q_u8(mask + j + 16);
        sum_hi[0] = masked_sad_16x1_neon(
            sum_hi[0], s1, vld1q_u8(ref[0] + ref_offset + j + 16), p1, m1);
        sum_hi[1] = masked_sad_16x1_neon(
            sum_hi[1], s1, vld1q_u8(ref[1] + ref_offset + j + 16), p1, m1);
        sum_hi[2] = masked_sad_16x1_neon(
            sum_hi[2], s1, vld1q_u8(ref[2] + ref_offset + j + 16), p1, m1);
        sum_hi[3] = masked_sad_16x1_neon(
            sum_hi[3], s1, vld1q_u8(ref[3] + ref_offset + j + 16), p1, m1);

        j += 32;
      } while (j < width);

      src += src_stride;
      ref_offset += ref_stride;
      second_pred += width;
      mask += mask_stride;
    } while (++i < h_limit);

    sum[0] = vpadalq_u16(sum[0], sum_lo[0]);
    sum[0] = vpadalq_u16(sum[0], sum_hi[0]);
    sum[1] = vpadalq_u16(sum[1], sum_lo[1]);
    sum[1] = vpadalq_u16(sum[1], sum_hi[1]);
    sum[2] = vpadalq_u16(sum[2], sum_lo[2]);
    sum[2] = vpadalq_u16(sum[2], sum_hi[2]);
    sum[3] = vpadalq_u16(sum[3], sum_lo[3]);
    sum[3] = vpadalq_u16(sum[3], sum_hi[3]);

    h_limit += h_overflow;
  } while (i < height);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}

static INLINE void masked_sad128xhx4d_neon(const uint8_t *src, int src_stride,
                                           const uint8_t *const ref[4],
                                           int ref_stride,
                                           const uint8_t *second_pred,
                                           const uint8_t *mask, int mask_stride,
                                           uint32_t res[4], int h) {
  masked_sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, second_pred,
                              mask, mask_stride, res, 128, h, 32);
}

static INLINE void masked_sad64xhx4d_neon(const uint8_t *src, int src_stride,
                                          const uint8_t *const ref[4],
                                          int ref_stride,
                                          const uint8_t *second_pred,
                                          const uint8_t *mask, int mask_stride,
                                          uint32_t res[4], int h) {
  masked_sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, second_pred,
                              mask, mask_stride, res, 64, h, 64);
}

static INLINE void masked_inv_sad32xhx4d_neon(
    const uint8_t *src, int src_stride, const uint8_t *const ref[4],
    int ref_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, uint32_t res[4], int h) {
  uint16x8_t sum_lo[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                           vdupq_n_u16(0) };
  uint16x8_t sum_hi[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                           vdupq_n_u16(0) };

  int ref_offset = 0;
  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t m0 = vld1q_u8(mask);
    sum_lo[0] = masked_sad_16x1_neon(sum_lo[0], s0, p0,
                                     vld1q_u8(ref[0] + ref_offset), m0);
    sum_lo[1] = masked_sad_16x1_neon(sum_lo[1], s0, p0,
                                     vld1q_u8(ref[1] + ref_offset), m0);
    sum_lo[2] = masked_sad_16x1_neon(sum_lo[2], s0, p0,
                                     vld1q_u8(ref[2] + ref_offset), m0);
    sum_lo[3] = masked_sad_16x1_neon(sum_lo[3], s0, p0,
                                     vld1q_u8(ref[3] + ref_offset), m0);

    uint8x16_t s1 = vld1q_u8(src + 16);
    uint8x16_t p1 = vld1q_u8(second_pred + 16);
    uint8x16_t m1 = vld1q_u8(mask + 16);
    sum_hi[0] = masked_sad_16x1_neon(sum_hi[0], s1, p1,
                                     vld1q_u8(ref[0] + ref_offset + 16), m1);
    sum_hi[1] = masked_sad_16x1_neon(sum_hi[1], s1, p1,
                                     vld1q_u8(ref[1] + ref_offset + 16), m1);
    sum_hi[2] = masked_sad_16x1_neon(sum_hi[2], s1, p1,
                                     vld1q_u8(ref[2] + ref_offset + 16), m1);
    sum_hi[3] = masked_sad_16x1_neon(sum_hi[3], s1, p1,
                                     vld1q_u8(ref[3] + ref_offset + 16), m1);

    src += src_stride;
    ref_offset += ref_stride;
    second_pred += 32;
    mask += mask_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_long_add_4d_u16x8(sum_lo, sum_hi));
}

static INLINE void masked_sad32xhx4d_neon(const uint8_t *src, int src_stride,
                                          const uint8_t *const ref[4],
                                          int ref_stride,
                                          const uint8_t *second_pred,
                                          const uint8_t *mask, int mask_stride,
                                          uint32_t res[4], int h) {
  uint16x8_t sum_lo[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                           vdupq_n_u16(0) };
  uint16x8_t sum_hi[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                           vdupq_n_u16(0) };

  int ref_offset = 0;
  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t m0 = vld1q_u8(mask);
    sum_lo[0] = masked_sad_16x1_neon(sum_lo[0], s0,
                                     vld1q_u8(ref[0] + ref_offset), p0, m0);
    sum_lo[1] = masked_sad_16x1_neon(sum_lo[1], s0,
                                     vld1q_u8(ref[1] + ref_offset), p0, m0);
    sum_lo[2] = masked_sad_16x1_neon(sum_lo[2], s0,
                                     vld1q_u8(ref[2] + ref_offset), p0, m0);
    sum_lo[3] = masked_sad_16x1_neon(sum_lo[3], s0,
                                     vld1q_u8(ref[3] + ref_offset), p0, m0);

    uint8x16_t s1 = vld1q_u8(src + 16);
    uint8x16_t p1 = vld1q_u8(second_pred + 16);
    uint8x16_t m1 = vld1q_u8(mask + 16);
    sum_hi[0] = masked_sad_16x1_neon(
        sum_hi[0], s1, vld1q_u8(ref[0] + ref_offset + 16), p1, m1);
    sum_hi[1] = masked_sad_16x1_neon(
        sum_hi[1], s1, vld1q_u8(ref[1] + ref_offset + 16), p1, m1);
    sum_hi[2] = masked_sad_16x1_neon(
        sum_hi[2], s1, vld1q_u8(ref[2] + ref_offset + 16), p1, m1);
    sum_hi[3] = masked_sad_16x1_neon(
        sum_hi[3], s1, vld1q_u8(ref[3] + ref_offset + 16), p1, m1);

    src += src_stride;
    ref_offset += ref_stride;
    second_pred += 32;
    mask += mask_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_long_add_4d_u16x8(sum_lo, sum_hi));
}

static INLINE void masked_inv_sad16xhx4d_neon(
    const uint8_t *src, int src_stride, const uint8_t *const ref[4],
    int ref_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, uint32_t res[4], int h) {
  uint16x8_t sum_u16[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                            vdupq_n_u16(0) };
  uint32x4_t sum_u32[4];

  int ref_offset = 0;
  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t m0 = vld1q_u8(mask);
    sum_u16[0] = masked_sad_16x1_neon(sum_u16[0], s0, p0,
                                      vld1q_u8(ref[0] + ref_offset), m0);
    sum_u16[1] = masked_sad_16x1_neon(sum_u16[1], s0, p0,
                                      vld1q_u8(ref[1] + ref_offset), m0);
    sum_u16[2] = masked_sad_16x1_neon(sum_u16[2], s0, p0,
                                      vld1q_u8(ref[2] + ref_offset), m0);
    sum_u16[3] = masked_sad_16x1_neon(sum_u16[3], s0, p0,
                                      vld1q_u8(ref[3] + ref_offset), m0);

    src += src_stride;
    ref_offset += ref_stride;
    second_pred += 16;
    mask += mask_stride;
  } while (--i != 0);

  sum_u32[0] = vpaddlq_u16(sum_u16[0]);
  sum_u32[1] = vpaddlq_u16(sum_u16[1]);
  sum_u32[2] = vpaddlq_u16(sum_u16[2]);
  sum_u32[3] = vpaddlq_u16(sum_u16[3]);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum_u32));
}

static INLINE void masked_sad16xhx4d_neon(const uint8_t *src, int src_stride,
                                          const uint8_t *const ref[4],
                                          int ref_stride,
                                          const uint8_t *second_pred,
                                          const uint8_t *mask, int mask_stride,
                                          uint32_t res[4], int h) {
  uint16x8_t sum_u16[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                            vdupq_n_u16(0) };
  uint32x4_t sum_u32[4];

  int ref_offset = 0;
  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t m0 = vld1q_u8(mask);
    sum_u16[0] = masked_sad_16x1_neon(sum_u16[0], s0,
                                      vld1q_u8(ref[0] + ref_offset), p0, m0);
    sum_u16[1] = masked_sad_16x1_neon(sum_u16[1], s0,
                                      vld1q_u8(ref[1] + ref_offset), p0, m0);
    sum_u16[2] = masked_sad_16x1_neon(sum_u16[2], s0,
                                      vld1q_u8(ref[2] + ref_offset), p0, m0);
    sum_u16[3] = masked_sad_16x1_neon(sum_u16[3], s0,
                                      vld1q_u8(ref[3] + ref_offset), p0, m0);

    src += src_stride;
    ref_offset += ref_stride;
    second_pred += 16;
    mask += mask_stride;
  } while (--i != 0);

  sum_u32[0] = vpaddlq_u16(sum_u16[0]);
  sum_u32[1] = vpaddlq_u16(sum_u16[1]);
  sum_u32[2] = vpaddlq_u16(sum_u16[2]);
  sum_u32[3] = vpaddlq_u16(sum_u16[3]);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum_u32));
}

static INLINE uint16x8_t masked_sad_8x1_neon(uint16x8_t sad, const uint8x8_t s0,
                                             const uint8x8_t a0,
                                             const uint8x8_t b0,
                                             const uint8x8_t m0) {
  uint8x8_t m0_inv = vsub_u8(vdup_n_u8(AOM_BLEND_A64_MAX_ALPHA), m0);
  uint16x8_t blend_u16 = vmull_u8(m0, a0);
  blend_u16 = vmlal_u8(blend_u16, m0_inv, b0);

  uint8x8_t blend_u8 = vrshrn_n_u16(blend_u16, AOM_BLEND_A64_ROUND_BITS);
  return vabal_u8(sad, blend_u8, s0);
}

static INLINE void masked_inv_sad8xhx4d_neon(
    const uint8_t *src, int src_stride, const uint8_t *const ref[4],
    int ref_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, uint32_t res[4], int h) {
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };

  int ref_offset = 0;
  int i = h;
  do {
    uint8x8_t s0 = vld1_u8(src);
    uint8x8_t p0 = vld1_u8(second_pred);
    uint8x8_t m0 = vld1_u8(mask);
    sum[0] =
        masked_sad_8x1_neon(sum[0], s0, p0, vld1_u8(ref[0] + ref_offset), m0);
    sum[1] =
        masked_sad_8x1_neon(sum[1], s0, p0, vld1_u8(ref[1] + ref_offset), m0);
    sum[2] =
        masked_sad_8x1_neon(sum[2], s0, p0, vld1_u8(ref[2] + ref_offset), m0);
    sum[3] =
        masked_sad_8x1_neon(sum[3], s0, p0, vld1_u8(ref[3] + ref_offset), m0);

    src += src_stride;
    ref_offset += ref_stride;
    second_pred += 8;
    mask += mask_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_add_4d_u16x8(sum));
}

static INLINE void masked_sad8xhx4d_neon(const uint8_t *src, int src_stride,
                                         const uint8_t *const ref[4],
                                         int ref_stride,
                                         const uint8_t *second_pred,
                                         const uint8_t *mask, int mask_stride,
                                         uint32_t res[4], int h) {
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };

  int ref_offset = 0;
  int i = h;
  do {
    uint8x8_t s0 = vld1_u8(src);
    uint8x8_t p0 = vld1_u8(second_pred);
    uint8x8_t m0 = vld1_u8(mask);

    sum[0] =
        masked_sad_8x1_neon(sum[0], s0, vld1_u8(ref[0] + ref_offset), p0, m0);
    sum[1] =
        masked_sad_8x1_neon(sum[1], s0, vld1_u8(ref[1] + ref_offset), p0, m0);
    sum[2] =
        masked_sad_8x1_neon(sum[2], s0, vld1_u8(ref[2] + ref_offset), p0, m0);
    sum[3] =
        masked_sad_8x1_neon(sum[3], s0, vld1_u8(ref[3] + ref_offset), p0, m0);

    src += src_stride;
    ref_offset += ref_stride;
    second_pred += 8;
    mask += mask_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_add_4d_u16x8(sum));
}

static INLINE void masked_inv_sad4xhx4d_neon(
    const uint8_t *src, int src_stride, const uint8_t *const ref[4],
    int ref_stride, const uint8_t *second_pred, const uint8_t *mask,
    int mask_stride, uint32_t res[4], int h) {
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };

  int ref_offset = 0;
  int i = h / 2;
  do {
    uint8x8_t s = load_unaligned_u8(src, src_stride);
    uint8x8_t r0 = load_unaligned_u8(ref[0] + ref_offset, ref_stride);
    uint8x8_t r1 = load_unaligned_u8(ref[1] + ref_offset, ref_stride);
    uint8x8_t r2 = load_unaligned_u8(ref[2] + ref_offset, ref_stride);
    uint8x8_t r3 = load_unaligned_u8(ref[3] + ref_offset, ref_stride);
    uint8x8_t p0 = vld1_u8(second_pred);
    uint8x8_t m0 = load_unaligned_u8(mask, mask_stride);

    sum[0] = masked_sad_8x1_neon(sum[0], s, p0, r0, m0);
    sum[1] = masked_sad_8x1_neon(sum[1], s, p0, r1, m0);
    sum[2] = masked_sad_8x1_neon(sum[2], s, p0, r2, m0);
    sum[3] = masked_sad_8x1_neon(sum[3], s, p0, r3, m0);

    src += 2 * src_stride;
    ref_offset += 2 * ref_stride;
    second_pred += 2 * 4;
    mask += 2 * mask_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_add_4d_u16x8(sum));
}

static INLINE void masked_sad4xhx4d_neon(const uint8_t *src, int src_stride,
                                         const uint8_t *const ref[4],
                                         int ref_stride,
                                         const uint8_t *second_pred,
                                         const uint8_t *mask, int mask_stride,
                                         uint32_t res[4], int h) {
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };

  int ref_offset = 0;
  int i = h / 2;
  do {
    uint8x8_t s = load_unaligned_u8(src, src_stride);
    uint8x8_t r0 = load_unaligned_u8(ref[0] + ref_offset, ref_stride);
    uint8x8_t r1 = load_unaligned_u8(ref[1] + ref_offset, ref_stride);
    uint8x8_t r2 = load_unaligned_u8(ref[2] + ref_offset, ref_stride);
    uint8x8_t r3 = load_unaligned_u8(ref[3] + ref_offset, ref_stride);
    uint8x8_t p0 = vld1_u8(second_pred);
    uint8x8_t m0 = load_unaligned_u8(mask, mask_stride);

    sum[0] = masked_sad_8x1_neon(sum[0], s, r0, p0, m0);
    sum[1] = masked_sad_8x1_neon(sum[1], s, r1, p0, m0);
    sum[2] = masked_sad_8x1_neon(sum[2], s, r2, p0, m0);
    sum[3] = masked_sad_8x1_neon(sum[3], s, r3, p0, m0);

    src += 2 * src_stride;
    ref_offset += 2 * ref_stride;
    second_pred += 2 * 4;
    mask += 2 * mask_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_add_4d_u16x8(sum));
}

#define MASKED_SAD4D_WXH_NEON(w, h)                                            \
  void aom_masked_sad##w##x##h##x4d_neon(                                      \
      const uint8_t *src, int src_stride, const uint8_t *ref[4],               \
      int ref_stride, const uint8_t *second_pred, const uint8_t *msk,          \
      int msk_stride, int invert_mask, uint32_t res[4]) {                      \
    if (invert_mask) {                                                         \
      masked_inv_sad##w##xhx4d_neon(src, src_stride, ref, ref_stride,          \
                                    second_pred, msk, msk_stride, res, h);     \
    } else {                                                                   \
      masked_sad##w##xhx4d_neon(src, src_stride, ref, ref_stride, second_pred, \
                                msk, msk_stride, res, h);                      \
    }                                                                          \
  }

MASKED_SAD4D_WXH_NEON(4, 8)
MASKED_SAD4D_WXH_NEON(4, 4)

MASKED_SAD4D_WXH_NEON(8, 16)
MASKED_SAD4D_WXH_NEON(8, 8)
MASKED_SAD4D_WXH_NEON(8, 4)

MASKED_SAD4D_WXH_NEON(16, 32)
MASKED_SAD4D_WXH_NEON(16, 16)
MASKED_SAD4D_WXH_NEON(16, 8)

MASKED_SAD4D_WXH_NEON(32, 64)
MASKED_SAD4D_WXH_NEON(32, 32)
MASKED_SAD4D_WXH_NEON(32, 16)

MASKED_SAD4D_WXH_NEON(64, 128)
MASKED_SAD4D_WXH_NEON(64, 64)
MASKED_SAD4D_WXH_NEON(64, 32)

MASKED_SAD4D_WXH_NEON(128, 128)
MASKED_SAD4D_WXH_NEON(128, 64)

#if !CONFIG_REALTIME_ONLY
MASKED_SAD4D_WXH_NEON(4, 16)
MASKED_SAD4D_WXH_NEON(16, 4)
MASKED_SAD4D_WXH_NEON(8, 32)
MASKED_SAD4D_WXH_NEON(32, 8)
MASKED_SAD4D_WXH_NEON(16, 64)
MASKED_SAD4D_WXH_NEON(64, 16)
#endif
