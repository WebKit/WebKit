/*
 * Copyright (c) 2023 The WebM project authors. All rights reserved.
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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

static inline uint32_t highbd_sad4xh_neon(const uint8_t *src_ptr,
                                          int src_stride,
                                          const uint8_t *ref_ptr,
                                          int ref_stride, int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);
  uint32x4_t sum = vdupq_n_u32(0);

  do {
    uint16x4_t s = vld1_u16(src16_ptr);
    uint16x4_t r = vld1_u16(ref16_ptr);
    sum = vabal_u16(sum, s, r);

    src16_ptr += src_stride;
    ref16_ptr += ref_stride;
  } while (--h != 0);

  return horizontal_add_u32x4(sum);
}

static inline uint32_t highbd_sad8xh_neon(const uint8_t *src_ptr,
                                          int src_stride,
                                          const uint8_t *ref_ptr,
                                          int ref_stride, int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);

  // 'h_overflow' is the number of 8-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 16 8-wide rows.
  const int h_overflow = 16;
  // If block height 'h' is smaller than this limit, use 'h' instead.
  const int h_limit = h < h_overflow ? h : h_overflow;
  assert(h % h_limit == 0);

  uint32x4_t sum_u32 = vdupq_n_u32(0);

  do {
    uint16x8_t sum_u16 = vdupq_n_u16(0);

    int i = h_limit;
    do {
      uint16x8_t s0 = vld1q_u16(src16_ptr);
      uint16x8_t r0 = vld1q_u16(ref16_ptr);
      sum_u16 = vabaq_u16(sum_u16, s0, r0);

      src16_ptr += src_stride;
      ref16_ptr += ref_stride;
    } while (--i != 0);

    sum_u32 = vpadalq_u16(sum_u32, sum_u16);

    h -= h_limit;
  } while (h != 0);

  return horizontal_add_u32x4(sum_u32);
}

static inline uint32_t highbd_sadwxh_neon(const uint8_t *src_ptr,
                                          int src_stride,
                                          const uint8_t *ref_ptr,
                                          int ref_stride, int w, int h,
                                          const int h_overflow) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);

  const int h_limit = h < h_overflow ? h : h_overflow;
  assert(h % h_limit == 0);

  uint32x4_t sum_u32 = vdupq_n_u32(0);

  do {
    uint16x8_t sum_u16[2] = { vdupq_n_u16(0), vdupq_n_u16(0) };

    int i = h_limit;
    do {
      int j = 0;
      do {
        uint16x8_t s0 = vld1q_u16(src16_ptr + j);
        uint16x8_t r0 = vld1q_u16(ref16_ptr + j);
        sum_u16[0] = vabaq_u16(sum_u16[0], s0, r0);

        uint16x8_t s1 = vld1q_u16(src16_ptr + j + 8);
        uint16x8_t r1 = vld1q_u16(ref16_ptr + j + 8);
        sum_u16[1] = vabaq_u16(sum_u16[1], s1, r1);

        j += 16;
      } while (j < w);

      src16_ptr += src_stride;
      ref16_ptr += ref_stride;
    } while (--i != 0);

    sum_u32 = vpadalq_u16(sum_u32, sum_u16[0]);
    sum_u32 = vpadalq_u16(sum_u32, sum_u16[1]);

    h -= h_limit;
  } while (h != 0);
  return horizontal_add_u32x4(sum_u32);
}

static inline uint32_t highbd_sad16xh_neon(const uint8_t *src_ptr,
                                           int src_stride,
                                           const uint8_t *ref_ptr,
                                           int ref_stride, int h) {
  // 'h_overflow' is the number of 16-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 16 16-wide rows using two accumulators.
  const int h_overflow = 16;
  return highbd_sadwxh_neon(src_ptr, src_stride, ref_ptr, ref_stride, 16, h,
                            h_overflow);
}

static inline uint32_t highbd_sad32xh_neon(const uint8_t *src_ptr,
                                           int src_stride,
                                           const uint8_t *ref_ptr,
                                           int ref_stride, int h) {
  // 'h_overflow' is the number of 32-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 8 32-wide rows using two accumulators.
  const int h_overflow = 8;
  return highbd_sadwxh_neon(src_ptr, src_stride, ref_ptr, ref_stride, 32, h,
                            h_overflow);
}

static inline uint32_t highbd_sad64xh_neon(const uint8_t *src_ptr,
                                           int src_stride,
                                           const uint8_t *ref_ptr,
                                           int ref_stride, int h) {
  // 'h_overflow' is the number of 64-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 4 64-wide rows using two accumulators.
  const int h_overflow = 4;
  return highbd_sadwxh_neon(src_ptr, src_stride, ref_ptr, ref_stride, 64, h,
                            h_overflow);
}

static inline uint32_t highbd_sad128xh_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h) {
  // 'h_overflow' is the number of 128-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 2 128-wide rows using two accumulators.
  const int h_overflow = 2;
  return highbd_sadwxh_neon(src_ptr, src_stride, ref_ptr, ref_stride, 128, h,
                            h_overflow);
}

#define HBD_SAD_WXH_NEON(w, h)                                            \
  unsigned int aom_highbd_sad##w##x##h##_neon(                            \
      const uint8_t *src, int src_stride, const uint8_t *ref,             \
      int ref_stride) {                                                   \
    return highbd_sad##w##xh_neon(src, src_stride, ref, ref_stride, (h)); \
  }

HBD_SAD_WXH_NEON(4, 4)
HBD_SAD_WXH_NEON(4, 8)

HBD_SAD_WXH_NEON(8, 4)
HBD_SAD_WXH_NEON(8, 8)
HBD_SAD_WXH_NEON(8, 16)

HBD_SAD_WXH_NEON(16, 8)
HBD_SAD_WXH_NEON(16, 16)
HBD_SAD_WXH_NEON(16, 32)

HBD_SAD_WXH_NEON(32, 16)
HBD_SAD_WXH_NEON(32, 32)
HBD_SAD_WXH_NEON(32, 64)

HBD_SAD_WXH_NEON(64, 32)
HBD_SAD_WXH_NEON(64, 64)
HBD_SAD_WXH_NEON(64, 128)

HBD_SAD_WXH_NEON(128, 64)
HBD_SAD_WXH_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SAD_WXH_NEON(4, 16)

HBD_SAD_WXH_NEON(8, 32)

HBD_SAD_WXH_NEON(16, 4)
HBD_SAD_WXH_NEON(16, 64)

HBD_SAD_WXH_NEON(32, 8)

HBD_SAD_WXH_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef HBD_SAD_WXH_NEON

#define HBD_SAD_SKIP_WXH_NEON(w, h)                             \
  unsigned int aom_highbd_sad_skip_##w##x##h##_neon(            \
      const uint8_t *src, int src_stride, const uint8_t *ref,   \
      int ref_stride) {                                         \
    return 2 * highbd_sad##w##xh_neon(src, 2 * src_stride, ref, \
                                      2 * ref_stride, (h) / 2); \
  }

HBD_SAD_SKIP_WXH_NEON(8, 16)

HBD_SAD_SKIP_WXH_NEON(16, 16)
HBD_SAD_SKIP_WXH_NEON(16, 32)

HBD_SAD_SKIP_WXH_NEON(32, 16)
HBD_SAD_SKIP_WXH_NEON(32, 32)
HBD_SAD_SKIP_WXH_NEON(32, 64)

HBD_SAD_SKIP_WXH_NEON(64, 32)
HBD_SAD_SKIP_WXH_NEON(64, 64)
HBD_SAD_SKIP_WXH_NEON(64, 128)

HBD_SAD_SKIP_WXH_NEON(128, 64)
HBD_SAD_SKIP_WXH_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SAD_SKIP_WXH_NEON(4, 16)

HBD_SAD_SKIP_WXH_NEON(8, 32)

HBD_SAD_SKIP_WXH_NEON(16, 64)

HBD_SAD_SKIP_WXH_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef HBD_SAD_SKIP_WXH_NEON

static inline uint32_t highbd_sad8xh_avg_neon(const uint8_t *src_ptr,
                                              int src_stride,
                                              const uint8_t *ref_ptr,
                                              int ref_stride, int h,
                                              const uint8_t *second_pred) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);
  const uint16_t *pred16_ptr = CONVERT_TO_SHORTPTR(second_pred);

  // 'h_overflow' is the number of 8-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 16 8-wide rows.
  const int h_overflow = 16;
  // If block height 'h' is smaller than this limit, use 'h' instead.
  const int h_limit = h < h_overflow ? h : h_overflow;
  assert(h % h_limit == 0);

  uint32x4_t sum_u32 = vdupq_n_u32(0);

  do {
    uint16x8_t sum_u16 = vdupq_n_u16(0);

    int i = h_limit;
    do {
      uint16x8_t s = vld1q_u16(src16_ptr);
      uint16x8_t r = vld1q_u16(ref16_ptr);
      uint16x8_t p = vld1q_u16(pred16_ptr);

      uint16x8_t avg = vrhaddq_u16(r, p);
      sum_u16 = vabaq_u16(sum_u16, s, avg);

      src16_ptr += src_stride;
      ref16_ptr += ref_stride;
      pred16_ptr += 8;
    } while (--i != 0);

    sum_u32 = vpadalq_u16(sum_u32, sum_u16);

    h -= h_limit;
  } while (h != 0);

  return horizontal_add_u32x4(sum_u32);
}

static inline uint32_t highbd_sad16xh_avg_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *ref_ptr,
                                               int ref_stride, int h,
                                               const uint8_t *second_pred) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);
  const uint16_t *pred16_ptr = CONVERT_TO_SHORTPTR(second_pred);

  // 'h_overflow' is the number of 16-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 16 16-wide rows using two accumulators.
  const int h_overflow = 16;
  // If block height 'h' is smaller than this limit, use 'h' instead.
  const int h_limit = h < h_overflow ? h : h_overflow;
  assert(h % h_limit == 0);

  uint32x4_t sum_u32 = vdupq_n_u32(0);

  do {
    uint16x8_t sum_u16[2] = { vdupq_n_u16(0), vdupq_n_u16(0) };

    int i = h_limit;
    do {
      uint16x8_t s0 = vld1q_u16(src16_ptr);
      uint16x8_t r0 = vld1q_u16(ref16_ptr);
      uint16x8_t p0 = vld1q_u16(pred16_ptr);

      uint16x8_t avg0 = vrhaddq_u16(r0, p0);
      sum_u16[0] = vabaq_u16(sum_u16[0], s0, avg0);

      uint16x8_t s1 = vld1q_u16(src16_ptr + 8);
      uint16x8_t r1 = vld1q_u16(ref16_ptr + 8);
      uint16x8_t p1 = vld1q_u16(pred16_ptr + 8);

      uint16x8_t avg1 = vrhaddq_u16(r1, p1);
      sum_u16[1] = vabaq_u16(sum_u16[1], s1, avg1);

      src16_ptr += src_stride;
      ref16_ptr += ref_stride;
      pred16_ptr += 16;
    } while (--i != 0);

    sum_u32 = vpadalq_u16(sum_u32, sum_u16[0]);
    sum_u32 = vpadalq_u16(sum_u32, sum_u16[1]);

    h -= h_limit;
  } while (h != 0);

  return horizontal_add_u32x4(sum_u32);
}

static inline uint32_t highbd_sadwxh_avg_neon(const uint8_t *src_ptr,
                                              int src_stride,
                                              const uint8_t *ref_ptr,
                                              int ref_stride,
                                              const uint8_t *second_pred, int w,
                                              int h, const int h_overflow) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr = CONVERT_TO_SHORTPTR(ref_ptr);
  const uint16_t *pred16_ptr = CONVERT_TO_SHORTPTR(second_pred);

  const int h_limit = h < h_overflow ? h : h_overflow;
  assert(h % h_limit == 0);

  uint32x4_t sum_u32 = vdupq_n_u32(0);

  do {
    uint16x8_t sum_u16[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                              vdupq_n_u16(0) };

    int i = h_limit;
    do {
      int j = 0;
      do {
        uint16x8_t s0 = vld1q_u16(src16_ptr + j);
        uint16x8_t r0 = vld1q_u16(ref16_ptr + j);
        uint16x8_t p0 = vld1q_u16(pred16_ptr + j);

        uint16x8_t avg0 = vrhaddq_u16(r0, p0);
        sum_u16[0] = vabaq_u16(sum_u16[0], s0, avg0);

        uint16x8_t s1 = vld1q_u16(src16_ptr + j + 8);
        uint16x8_t r1 = vld1q_u16(ref16_ptr + j + 8);
        uint16x8_t p1 = vld1q_u16(pred16_ptr + j + 8);

        uint16x8_t avg1 = vrhaddq_u16(r1, p1);
        sum_u16[1] = vabaq_u16(sum_u16[1], s1, avg1);

        uint16x8_t s2 = vld1q_u16(src16_ptr + j + 16);
        uint16x8_t r2 = vld1q_u16(ref16_ptr + j + 16);
        uint16x8_t p2 = vld1q_u16(pred16_ptr + j + 16);

        uint16x8_t avg2 = vrhaddq_u16(r2, p2);
        sum_u16[2] = vabaq_u16(sum_u16[2], s2, avg2);

        uint16x8_t s3 = vld1q_u16(src16_ptr + j + 24);
        uint16x8_t r3 = vld1q_u16(ref16_ptr + j + 24);
        uint16x8_t p3 = vld1q_u16(pred16_ptr + j + 24);

        uint16x8_t avg3 = vrhaddq_u16(r3, p3);
        sum_u16[3] = vabaq_u16(sum_u16[3], s3, avg3);

        j += 32;
      } while (j < w);

      src16_ptr += src_stride;
      ref16_ptr += ref_stride;
      pred16_ptr += w;
    } while (--i != 0);

    sum_u32 = vpadalq_u16(sum_u32, sum_u16[0]);
    sum_u32 = vpadalq_u16(sum_u32, sum_u16[1]);
    sum_u32 = vpadalq_u16(sum_u32, sum_u16[2]);
    sum_u32 = vpadalq_u16(sum_u32, sum_u16[3]);

    h -= h_limit;
  } while (h != 0);

  return horizontal_add_u32x4(sum_u32);
}

static inline uint32_t highbd_sad32xh_avg_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *ref_ptr,
                                               int ref_stride, int h,
                                               const uint8_t *second_pred) {
  // 'h_overflow' is the number of 32-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 16 32-wide rows using four accumulators.
  const int h_overflow = 16;
  return highbd_sadwxh_avg_neon(src_ptr, src_stride, ref_ptr, ref_stride,
                                second_pred, 32, h, h_overflow);
}

static inline uint32_t highbd_sad64xh_avg_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *ref_ptr,
                                               int ref_stride, int h,
                                               const uint8_t *second_pred) {
  // 'h_overflow' is the number of 64-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 8 64-wide rows using four accumulators.
  const int h_overflow = 8;
  return highbd_sadwxh_avg_neon(src_ptr, src_stride, ref_ptr, ref_stride,
                                second_pred, 64, h, h_overflow);
}

static inline uint32_t highbd_sad128xh_avg_neon(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *ref_ptr,
                                                int ref_stride, int h,
                                                const uint8_t *second_pred) {
  // 'h_overflow' is the number of 128-wide rows we can process before 16-bit
  // accumulators overflow. After hitting this limit accumulate into 32-bit
  // elements. 65535 / 4095 ~= 16, so 4 128-wide rows using four accumulators.
  const int h_overflow = 4;
  return highbd_sadwxh_avg_neon(src_ptr, src_stride, ref_ptr, ref_stride,
                                second_pred, 128, h, h_overflow);
}

#define HBD_SAD_WXH_AVG_NEON(w, h)                                            \
  uint32_t aom_highbd_sad##w##x##h##_avg_neon(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred) {                                           \
    return highbd_sad##w##xh_avg_neon(src, src_stride, ref, ref_stride, (h),  \
                                      second_pred);                           \
  }

HBD_SAD_WXH_AVG_NEON(8, 8)
HBD_SAD_WXH_AVG_NEON(8, 16)

HBD_SAD_WXH_AVG_NEON(16, 8)
HBD_SAD_WXH_AVG_NEON(16, 16)
HBD_SAD_WXH_AVG_NEON(16, 32)

HBD_SAD_WXH_AVG_NEON(32, 16)
HBD_SAD_WXH_AVG_NEON(32, 32)
HBD_SAD_WXH_AVG_NEON(32, 64)

HBD_SAD_WXH_AVG_NEON(64, 32)
HBD_SAD_WXH_AVG_NEON(64, 64)
HBD_SAD_WXH_AVG_NEON(64, 128)

HBD_SAD_WXH_AVG_NEON(128, 64)
HBD_SAD_WXH_AVG_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SAD_WXH_AVG_NEON(8, 32)

HBD_SAD_WXH_AVG_NEON(16, 64)

HBD_SAD_WXH_AVG_NEON(32, 8)

HBD_SAD_WXH_AVG_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef HBD_SAD_WXH_AVG_NEON
