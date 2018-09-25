/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/arm/mem_neon.h"
#include "vpx_dsp/arm/sum_neon.h"

void vpx_sad4x4x4d_neon(const uint8_t *src, int src_stride,
                        const uint8_t *const ref[4], int ref_stride,
                        uint32_t *res) {
  int i;
  const uint8x16_t src_u8 = load_unaligned_u8q(src, src_stride);
  for (i = 0; i < 4; ++i) {
    const uint8x16_t ref_u8 = load_unaligned_u8q(ref[i], ref_stride);
    uint16x8_t abs = vabdl_u8(vget_low_u8(src_u8), vget_low_u8(ref_u8));
    abs = vabal_u8(abs, vget_high_u8(src_u8), vget_high_u8(ref_u8));
    res[i] = vget_lane_u32(horizontal_add_uint16x8(abs), 0);
  }
}

void vpx_sad4x8x4d_neon(const uint8_t *src, int src_stride,
                        const uint8_t *const ref[4], int ref_stride,
                        uint32_t *res) {
  int i;
  const uint8x16_t src_0 = load_unaligned_u8q(src, src_stride);
  const uint8x16_t src_1 = load_unaligned_u8q(src + 4 * src_stride, src_stride);
  for (i = 0; i < 4; ++i) {
    const uint8x16_t ref_0 = load_unaligned_u8q(ref[i], ref_stride);
    const uint8x16_t ref_1 =
        load_unaligned_u8q(ref[i] + 4 * ref_stride, ref_stride);
    uint16x8_t abs = vabdl_u8(vget_low_u8(src_0), vget_low_u8(ref_0));
    abs = vabal_u8(abs, vget_high_u8(src_0), vget_high_u8(ref_0));
    abs = vabal_u8(abs, vget_low_u8(src_1), vget_low_u8(ref_1));
    abs = vabal_u8(abs, vget_high_u8(src_1), vget_high_u8(ref_1));
    res[i] = vget_lane_u32(horizontal_add_uint16x8(abs), 0);
  }
}

static INLINE void sad8x_4d(const uint8_t *a, int a_stride,
                            const uint8_t *const b[4], int b_stride,
                            uint32_t *result, const int height) {
  int i, j;
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };
  const uint8_t *b_loop[4] = { b[0], b[1], b[2], b[3] };

  for (i = 0; i < height; ++i) {
    const uint8x8_t a_u8 = vld1_u8(a);
    a += a_stride;
    for (j = 0; j < 4; ++j) {
      const uint8x8_t b_u8 = vld1_u8(b_loop[j]);
      b_loop[j] += b_stride;
      sum[j] = vabal_u8(sum[j], a_u8, b_u8);
    }
  }

  for (j = 0; j < 4; ++j) {
    result[j] = vget_lane_u32(horizontal_add_uint16x8(sum[j]), 0);
  }
}

void vpx_sad8x4x4d_neon(const uint8_t *src, int src_stride,
                        const uint8_t *const ref[4], int ref_stride,
                        uint32_t *res) {
  sad8x_4d(src, src_stride, ref, ref_stride, res, 4);
}

void vpx_sad8x8x4d_neon(const uint8_t *src, int src_stride,
                        const uint8_t *const ref[4], int ref_stride,
                        uint32_t *res) {
  sad8x_4d(src, src_stride, ref, ref_stride, res, 8);
}

void vpx_sad8x16x4d_neon(const uint8_t *src, int src_stride,
                         const uint8_t *const ref[4], int ref_stride,
                         uint32_t *res) {
  sad8x_4d(src, src_stride, ref, ref_stride, res, 16);
}

static INLINE void sad16x_4d(const uint8_t *a, int a_stride,
                             const uint8_t *const b[4], int b_stride,
                             uint32_t *result, const int height) {
  int i, j;
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };
  const uint8_t *b_loop[4] = { b[0], b[1], b[2], b[3] };

  for (i = 0; i < height; ++i) {
    const uint8x16_t a_u8 = vld1q_u8(a);
    a += a_stride;
    for (j = 0; j < 4; ++j) {
      const uint8x16_t b_u8 = vld1q_u8(b_loop[j]);
      b_loop[j] += b_stride;
      sum[j] = vabal_u8(sum[j], vget_low_u8(a_u8), vget_low_u8(b_u8));
      sum[j] = vabal_u8(sum[j], vget_high_u8(a_u8), vget_high_u8(b_u8));
    }
  }

  for (j = 0; j < 4; ++j) {
    result[j] = vget_lane_u32(horizontal_add_uint16x8(sum[j]), 0);
  }
}

void vpx_sad16x8x4d_neon(const uint8_t *src, int src_stride,
                         const uint8_t *const ref[4], int ref_stride,
                         uint32_t *res) {
  sad16x_4d(src, src_stride, ref, ref_stride, res, 8);
}

void vpx_sad16x16x4d_neon(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t *res) {
  sad16x_4d(src, src_stride, ref, ref_stride, res, 16);
}

void vpx_sad16x32x4d_neon(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t *res) {
  sad16x_4d(src, src_stride, ref, ref_stride, res, 32);
}

static INLINE void sad32x_4d(const uint8_t *a, int a_stride,
                             const uint8_t *const b[4], int b_stride,
                             uint32_t *result, const int height) {
  int i, j;
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };
  const uint8_t *b_loop[4] = { b[0], b[1], b[2], b[3] };

  for (i = 0; i < height; ++i) {
    const uint8x16_t a_0 = vld1q_u8(a);
    const uint8x16_t a_1 = vld1q_u8(a + 16);
    a += a_stride;
    for (j = 0; j < 4; ++j) {
      const uint8x16_t b_0 = vld1q_u8(b_loop[j]);
      const uint8x16_t b_1 = vld1q_u8(b_loop[j] + 16);
      b_loop[j] += b_stride;
      sum[j] = vabal_u8(sum[j], vget_low_u8(a_0), vget_low_u8(b_0));
      sum[j] = vabal_u8(sum[j], vget_high_u8(a_0), vget_high_u8(b_0));
      sum[j] = vabal_u8(sum[j], vget_low_u8(a_1), vget_low_u8(b_1));
      sum[j] = vabal_u8(sum[j], vget_high_u8(a_1), vget_high_u8(b_1));
    }
  }

  for (j = 0; j < 4; ++j) {
    result[j] = vget_lane_u32(horizontal_add_uint16x8(sum[j]), 0);
  }
}

void vpx_sad32x16x4d_neon(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t *res) {
  sad32x_4d(src, src_stride, ref, ref_stride, res, 16);
}

void vpx_sad32x32x4d_neon(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t *res) {
  sad32x_4d(src, src_stride, ref, ref_stride, res, 32);
}

void vpx_sad32x64x4d_neon(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t *res) {
  sad32x_4d(src, src_stride, ref, ref_stride, res, 64);
}

static INLINE void sum64x(const uint8x16_t a_0, const uint8x16_t a_1,
                          const uint8x16_t b_0, const uint8x16_t b_1,
                          uint16x8_t *sum) {
  *sum = vabal_u8(*sum, vget_low_u8(a_0), vget_low_u8(b_0));
  *sum = vabal_u8(*sum, vget_high_u8(a_0), vget_high_u8(b_0));
  *sum = vabal_u8(*sum, vget_low_u8(a_1), vget_low_u8(b_1));
  *sum = vabal_u8(*sum, vget_high_u8(a_1), vget_high_u8(b_1));
}

static INLINE void sad64x_4d(const uint8_t *a, int a_stride,
                             const uint8_t *const b[4], int b_stride,
                             uint32_t *result, const int height) {
  int i;
  uint16x8_t sum_0 = vdupq_n_u16(0);
  uint16x8_t sum_1 = vdupq_n_u16(0);
  uint16x8_t sum_2 = vdupq_n_u16(0);
  uint16x8_t sum_3 = vdupq_n_u16(0);
  uint16x8_t sum_4 = vdupq_n_u16(0);
  uint16x8_t sum_5 = vdupq_n_u16(0);
  uint16x8_t sum_6 = vdupq_n_u16(0);
  uint16x8_t sum_7 = vdupq_n_u16(0);
  const uint8_t *b_loop[4] = { b[0], b[1], b[2], b[3] };

  for (i = 0; i < height; ++i) {
    const uint8x16_t a_0 = vld1q_u8(a);
    const uint8x16_t a_1 = vld1q_u8(a + 16);
    const uint8x16_t a_2 = vld1q_u8(a + 32);
    const uint8x16_t a_3 = vld1q_u8(a + 48);
    a += a_stride;
    sum64x(a_0, a_1, vld1q_u8(b_loop[0]), vld1q_u8(b_loop[0] + 16), &sum_0);
    sum64x(a_2, a_3, vld1q_u8(b_loop[0] + 32), vld1q_u8(b_loop[0] + 48),
           &sum_1);
    b_loop[0] += b_stride;
    sum64x(a_0, a_1, vld1q_u8(b_loop[1]), vld1q_u8(b_loop[1] + 16), &sum_2);
    sum64x(a_2, a_3, vld1q_u8(b_loop[1] + 32), vld1q_u8(b_loop[1] + 48),
           &sum_3);
    b_loop[1] += b_stride;
    sum64x(a_0, a_1, vld1q_u8(b_loop[2]), vld1q_u8(b_loop[2] + 16), &sum_4);
    sum64x(a_2, a_3, vld1q_u8(b_loop[2] + 32), vld1q_u8(b_loop[2] + 48),
           &sum_5);
    b_loop[2] += b_stride;
    sum64x(a_0, a_1, vld1q_u8(b_loop[3]), vld1q_u8(b_loop[3] + 16), &sum_6);
    sum64x(a_2, a_3, vld1q_u8(b_loop[3] + 32), vld1q_u8(b_loop[3] + 48),
           &sum_7);
    b_loop[3] += b_stride;
  }

  result[0] = vget_lane_u32(horizontal_add_long_uint16x8(sum_0, sum_1), 0);
  result[1] = vget_lane_u32(horizontal_add_long_uint16x8(sum_2, sum_3), 0);
  result[2] = vget_lane_u32(horizontal_add_long_uint16x8(sum_4, sum_5), 0);
  result[3] = vget_lane_u32(horizontal_add_long_uint16x8(sum_6, sum_7), 0);
}

void vpx_sad64x32x4d_neon(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t *res) {
  sad64x_4d(src, src_stride, ref, ref_stride, res, 32);
}

void vpx_sad64x64x4d_neon(const uint8_t *src, int src_stride,
                          const uint8_t *const ref[4], int ref_stride,
                          uint32_t *res) {
  sad64x_4d(src, src_stride, ref, ref_stride, res, 64);
}
