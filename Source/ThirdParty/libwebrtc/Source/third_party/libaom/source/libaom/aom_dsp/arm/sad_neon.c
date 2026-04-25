/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
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

static inline unsigned int sad128xh_neon(const uint8_t *src_ptr, int src_stride,
                                         const uint8_t *ref_ptr, int ref_stride,
                                         int h) {
  // We use 8 accumulators to prevent overflow for large values of 'h', as well
  // as enabling optimal UADALP instruction throughput on CPUs that have either
  // 2 or 4 Neon pipes.
  uint16x8_t sum[8] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0), vdupq_n_u16(0) };

  int i = h;
  do {
    uint8x16_t s0, s1, s2, s3, s4, s5, s6, s7;
    uint8x16_t r0, r1, r2, r3, r4, r5, r6, r7;
    uint8x16_t diff0, diff1, diff2, diff3, diff4, diff5, diff6, diff7;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    diff0 = vabdq_u8(s0, r0);
    sum[0] = vpadalq_u8(sum[0], diff0);

    s1 = vld1q_u8(src_ptr + 16);
    r1 = vld1q_u8(ref_ptr + 16);
    diff1 = vabdq_u8(s1, r1);
    sum[1] = vpadalq_u8(sum[1], diff1);

    s2 = vld1q_u8(src_ptr + 32);
    r2 = vld1q_u8(ref_ptr + 32);
    diff2 = vabdq_u8(s2, r2);
    sum[2] = vpadalq_u8(sum[2], diff2);

    s3 = vld1q_u8(src_ptr + 48);
    r3 = vld1q_u8(ref_ptr + 48);
    diff3 = vabdq_u8(s3, r3);
    sum[3] = vpadalq_u8(sum[3], diff3);

    s4 = vld1q_u8(src_ptr + 64);
    r4 = vld1q_u8(ref_ptr + 64);
    diff4 = vabdq_u8(s4, r4);
    sum[4] = vpadalq_u8(sum[4], diff4);

    s5 = vld1q_u8(src_ptr + 80);
    r5 = vld1q_u8(ref_ptr + 80);
    diff5 = vabdq_u8(s5, r5);
    sum[5] = vpadalq_u8(sum[5], diff5);

    s6 = vld1q_u8(src_ptr + 96);
    r6 = vld1q_u8(ref_ptr + 96);
    diff6 = vabdq_u8(s6, r6);
    sum[6] = vpadalq_u8(sum[6], diff6);

    s7 = vld1q_u8(src_ptr + 112);
    r7 = vld1q_u8(ref_ptr + 112);
    diff7 = vabdq_u8(s7, r7);
    sum[7] = vpadalq_u8(sum[7], diff7);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  uint32x4_t sum_u32 = vpaddlq_u16(sum[0]);
  sum_u32 = vpadalq_u16(sum_u32, sum[1]);
  sum_u32 = vpadalq_u16(sum_u32, sum[2]);
  sum_u32 = vpadalq_u16(sum_u32, sum[3]);
  sum_u32 = vpadalq_u16(sum_u32, sum[4]);
  sum_u32 = vpadalq_u16(sum_u32, sum[5]);
  sum_u32 = vpadalq_u16(sum_u32, sum[6]);
  sum_u32 = vpadalq_u16(sum_u32, sum[7]);

  return horizontal_add_u32x4(sum_u32);
}

static inline unsigned int sad64xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };

  int i = h;
  do {
    uint8x16_t s0, s1, s2, s3, r0, r1, r2, r3;
    uint8x16_t diff0, diff1, diff2, diff3;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    diff0 = vabdq_u8(s0, r0);
    sum[0] = vpadalq_u8(sum[0], diff0);

    s1 = vld1q_u8(src_ptr + 16);
    r1 = vld1q_u8(ref_ptr + 16);
    diff1 = vabdq_u8(s1, r1);
    sum[1] = vpadalq_u8(sum[1], diff1);

    s2 = vld1q_u8(src_ptr + 32);
    r2 = vld1q_u8(ref_ptr + 32);
    diff2 = vabdq_u8(s2, r2);
    sum[2] = vpadalq_u8(sum[2], diff2);

    s3 = vld1q_u8(src_ptr + 48);
    r3 = vld1q_u8(ref_ptr + 48);
    diff3 = vabdq_u8(s3, r3);
    sum[3] = vpadalq_u8(sum[3], diff3);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  uint32x4_t sum_u32 = vpaddlq_u16(sum[0]);
  sum_u32 = vpadalq_u16(sum_u32, sum[1]);
  sum_u32 = vpadalq_u16(sum_u32, sum[2]);
  sum_u32 = vpadalq_u16(sum_u32, sum[3]);

  return horizontal_add_u32x4(sum_u32);
}

static inline unsigned int sad32xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  uint16x8_t sum[2] = { vdupq_n_u16(0), vdupq_n_u16(0) };

  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src_ptr);
    uint8x16_t r0 = vld1q_u8(ref_ptr);
    uint8x16_t diff0 = vabdq_u8(s0, r0);
    sum[0] = vpadalq_u8(sum[0], diff0);

    uint8x16_t s1 = vld1q_u8(src_ptr + 16);
    uint8x16_t r1 = vld1q_u8(ref_ptr + 16);
    uint8x16_t diff1 = vabdq_u8(s1, r1);
    sum[1] = vpadalq_u8(sum[1], diff1);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_u16x8(vaddq_u16(sum[0], sum[1]));
}

static inline unsigned int sad16xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h;
  do {
    uint8x16_t s = vld1q_u8(src_ptr);
    uint8x16_t r = vld1q_u8(ref_ptr);

    uint8x16_t diff = vabdq_u8(s, r);
    sum = vpadalq_u8(sum, diff);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_u16x8(sum);
}

static inline unsigned int sad8xh_neon(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       int h) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h;
  do {
    uint8x8_t s = vld1_u8(src_ptr);
    uint8x8_t r = vld1_u8(ref_ptr);

    sum = vabal_u8(sum, s, r);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_u16x8(sum);
}

static inline unsigned int sad4xh_neon(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       int h) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h / 2;
  do {
    uint8x8_t s = load_unaligned_u8(src_ptr, src_stride);
    uint8x8_t r = load_unaligned_u8(ref_ptr, ref_stride);

    sum = vabal_u8(sum, s, r);

    src_ptr += 2 * src_stride;
    ref_ptr += 2 * ref_stride;
  } while (--i != 0);

  return horizontal_add_u16x8(sum);
}

#define SAD_WXH_NEON(w, h)                                                   \
  unsigned int aom_sad##w##x##h##_neon(const uint8_t *src, int src_stride,   \
                                       const uint8_t *ref, int ref_stride) { \
    return sad##w##xh_neon(src, src_stride, ref, ref_stride, (h));           \
  }

SAD_WXH_NEON(4, 4)
SAD_WXH_NEON(4, 8)

SAD_WXH_NEON(8, 4)
SAD_WXH_NEON(8, 8)
SAD_WXH_NEON(8, 16)

SAD_WXH_NEON(16, 8)
SAD_WXH_NEON(16, 16)
SAD_WXH_NEON(16, 32)

SAD_WXH_NEON(32, 16)
SAD_WXH_NEON(32, 32)
SAD_WXH_NEON(32, 64)

SAD_WXH_NEON(64, 32)
SAD_WXH_NEON(64, 64)
SAD_WXH_NEON(64, 128)

SAD_WXH_NEON(128, 64)
SAD_WXH_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_WXH_NEON(4, 16)
SAD_WXH_NEON(8, 32)
SAD_WXH_NEON(16, 4)
SAD_WXH_NEON(16, 64)
SAD_WXH_NEON(32, 8)
SAD_WXH_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_WXH_NEON

#define SAD_SKIP_WXH_NEON(w, h)                                                \
  unsigned int aom_sad_skip_##w##x##h##_neon(                                  \
      const uint8_t *src, int src_stride, const uint8_t *ref,                  \
      int ref_stride) {                                                        \
    return 2 *                                                                 \
           sad##w##xh_neon(src, 2 * src_stride, ref, 2 * ref_stride, (h) / 2); \
  }

SAD_SKIP_WXH_NEON(8, 16)

SAD_SKIP_WXH_NEON(16, 16)
SAD_SKIP_WXH_NEON(16, 32)

SAD_SKIP_WXH_NEON(32, 16)
SAD_SKIP_WXH_NEON(32, 32)
SAD_SKIP_WXH_NEON(32, 64)

SAD_SKIP_WXH_NEON(64, 32)
SAD_SKIP_WXH_NEON(64, 64)
SAD_SKIP_WXH_NEON(64, 128)

SAD_SKIP_WXH_NEON(128, 64)
SAD_SKIP_WXH_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_SKIP_WXH_NEON(4, 16)
SAD_SKIP_WXH_NEON(8, 32)
SAD_SKIP_WXH_NEON(16, 64)
SAD_SKIP_WXH_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_SKIP_WXH_NEON

static inline unsigned int sad128xh_avg_neon(const uint8_t *src_ptr,
                                             int src_stride,
                                             const uint8_t *ref_ptr,
                                             int ref_stride, int h,
                                             const uint8_t *second_pred) {
  // We use 8 accumulators to prevent overflow for large values of 'h', as well
  // as enabling optimal UADALP instruction throughput on CPUs that have either
  // 2 or 4 Neon pipes.
  uint16x8_t sum[8] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0), vdupq_n_u16(0) };

  int i = h;
  do {
    uint8x16_t s0, s1, s2, s3, s4, s5, s6, s7;
    uint8x16_t r0, r1, r2, r3, r4, r5, r6, r7;
    uint8x16_t p0, p1, p2, p3, p4, p5, p6, p7;
    uint8x16_t avg0, avg1, avg2, avg3, avg4, avg5, avg6, avg7;
    uint8x16_t diff0, diff1, diff2, diff3, diff4, diff5, diff6, diff7;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    p0 = vld1q_u8(second_pred);
    avg0 = vrhaddq_u8(r0, p0);
    diff0 = vabdq_u8(s0, avg0);
    sum[0] = vpadalq_u8(sum[0], diff0);

    s1 = vld1q_u8(src_ptr + 16);
    r1 = vld1q_u8(ref_ptr + 16);
    p1 = vld1q_u8(second_pred + 16);
    avg1 = vrhaddq_u8(r1, p1);
    diff1 = vabdq_u8(s1, avg1);
    sum[1] = vpadalq_u8(sum[1], diff1);

    s2 = vld1q_u8(src_ptr + 32);
    r2 = vld1q_u8(ref_ptr + 32);
    p2 = vld1q_u8(second_pred + 32);
    avg2 = vrhaddq_u8(r2, p2);
    diff2 = vabdq_u8(s2, avg2);
    sum[2] = vpadalq_u8(sum[2], diff2);

    s3 = vld1q_u8(src_ptr + 48);
    r3 = vld1q_u8(ref_ptr + 48);
    p3 = vld1q_u8(second_pred + 48);
    avg3 = vrhaddq_u8(r3, p3);
    diff3 = vabdq_u8(s3, avg3);
    sum[3] = vpadalq_u8(sum[3], diff3);

    s4 = vld1q_u8(src_ptr + 64);
    r4 = vld1q_u8(ref_ptr + 64);
    p4 = vld1q_u8(second_pred + 64);
    avg4 = vrhaddq_u8(r4, p4);
    diff4 = vabdq_u8(s4, avg4);
    sum[4] = vpadalq_u8(sum[4], diff4);

    s5 = vld1q_u8(src_ptr + 80);
    r5 = vld1q_u8(ref_ptr + 80);
    p5 = vld1q_u8(second_pred + 80);
    avg5 = vrhaddq_u8(r5, p5);
    diff5 = vabdq_u8(s5, avg5);
    sum[5] = vpadalq_u8(sum[5], diff5);

    s6 = vld1q_u8(src_ptr + 96);
    r6 = vld1q_u8(ref_ptr + 96);
    p6 = vld1q_u8(second_pred + 96);
    avg6 = vrhaddq_u8(r6, p6);
    diff6 = vabdq_u8(s6, avg6);
    sum[6] = vpadalq_u8(sum[6], diff6);

    s7 = vld1q_u8(src_ptr + 112);
    r7 = vld1q_u8(ref_ptr + 112);
    p7 = vld1q_u8(second_pred + 112);
    avg7 = vrhaddq_u8(r7, p7);
    diff7 = vabdq_u8(s7, avg7);
    sum[7] = vpadalq_u8(sum[7], diff7);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 128;
  } while (--i != 0);

  uint32x4_t sum_u32 = vpaddlq_u16(sum[0]);
  sum_u32 = vpadalq_u16(sum_u32, sum[1]);
  sum_u32 = vpadalq_u16(sum_u32, sum[2]);
  sum_u32 = vpadalq_u16(sum_u32, sum[3]);
  sum_u32 = vpadalq_u16(sum_u32, sum[4]);
  sum_u32 = vpadalq_u16(sum_u32, sum[5]);
  sum_u32 = vpadalq_u16(sum_u32, sum[6]);
  sum_u32 = vpadalq_u16(sum_u32, sum[7]);

  return horizontal_add_u32x4(sum_u32);
}

static inline unsigned int sad64xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };

  int i = h;
  do {
    uint8x16_t s0, s1, s2, s3, r0, r1, r2, r3, p0, p1, p2, p3;
    uint8x16_t avg0, avg1, avg2, avg3, diff0, diff1, diff2, diff3;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    p0 = vld1q_u8(second_pred);
    avg0 = vrhaddq_u8(r0, p0);
    diff0 = vabdq_u8(s0, avg0);
    sum[0] = vpadalq_u8(sum[0], diff0);

    s1 = vld1q_u8(src_ptr + 16);
    r1 = vld1q_u8(ref_ptr + 16);
    p1 = vld1q_u8(second_pred + 16);
    avg1 = vrhaddq_u8(r1, p1);
    diff1 = vabdq_u8(s1, avg1);
    sum[1] = vpadalq_u8(sum[1], diff1);

    s2 = vld1q_u8(src_ptr + 32);
    r2 = vld1q_u8(ref_ptr + 32);
    p2 = vld1q_u8(second_pred + 32);
    avg2 = vrhaddq_u8(r2, p2);
    diff2 = vabdq_u8(s2, avg2);
    sum[2] = vpadalq_u8(sum[2], diff2);

    s3 = vld1q_u8(src_ptr + 48);
    r3 = vld1q_u8(ref_ptr + 48);
    p3 = vld1q_u8(second_pred + 48);
    avg3 = vrhaddq_u8(r3, p3);
    diff3 = vabdq_u8(s3, avg3);
    sum[3] = vpadalq_u8(sum[3], diff3);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 64;
  } while (--i != 0);

  uint32x4_t sum_u32 = vpaddlq_u16(sum[0]);
  sum_u32 = vpadalq_u16(sum_u32, sum[1]);
  sum_u32 = vpadalq_u16(sum_u32, sum[2]);
  sum_u32 = vpadalq_u16(sum_u32, sum[3]);

  return horizontal_add_u32x4(sum_u32);
}

static inline unsigned int sad32xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  uint16x8_t sum[2] = { vdupq_n_u16(0), vdupq_n_u16(0) };

  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src_ptr);
    uint8x16_t r0 = vld1q_u8(ref_ptr);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t avg0 = vrhaddq_u8(r0, p0);
    uint8x16_t diff0 = vabdq_u8(s0, avg0);
    sum[0] = vpadalq_u8(sum[0], diff0);

    uint8x16_t s1 = vld1q_u8(src_ptr + 16);
    uint8x16_t r1 = vld1q_u8(ref_ptr + 16);
    uint8x16_t p1 = vld1q_u8(second_pred + 16);
    uint8x16_t avg1 = vrhaddq_u8(r1, p1);
    uint8x16_t diff1 = vabdq_u8(s1, avg1);
    sum[1] = vpadalq_u8(sum[1], diff1);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 32;
  } while (--i != 0);

  return horizontal_add_u16x8(vaddq_u16(sum[0], sum[1]));
}

static inline unsigned int sad16xh_avg_neon(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *ref_ptr,
                                            int ref_stride, int h,
                                            const uint8_t *second_pred) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h;
  do {
    uint8x16_t s = vld1q_u8(src_ptr);
    uint8x16_t r = vld1q_u8(ref_ptr);
    uint8x16_t p = vld1q_u8(second_pred);

    uint8x16_t avg = vrhaddq_u8(r, p);
    uint8x16_t diff = vabdq_u8(s, avg);
    sum = vpadalq_u8(sum, diff);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 16;
  } while (--i != 0);

  return horizontal_add_u16x8(sum);
}

static inline unsigned int sad8xh_avg_neon(const uint8_t *src_ptr,
                                           int src_stride,
                                           const uint8_t *ref_ptr,
                                           int ref_stride, int h,
                                           const uint8_t *second_pred) {
  uint16x8_t sum = vdupq_n_u16(0);

  int i = h;
  do {
    uint8x8_t s = vld1_u8(src_ptr);
    uint8x8_t r = vld1_u8(ref_ptr);
    uint8x8_t p = vld1_u8(second_pred);

    uint8x8_t avg = vrhadd_u8(r, p);
    sum = vabal_u8(sum, s, avg);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 8;
  } while (--i != 0);

  return horizontal_add_u16x8(sum);
}

#define SAD_WXH_AVG_NEON(w, h)                                                 \
  unsigned int aom_sad##w##x##h##_avg_neon(const uint8_t *src, int src_stride, \
                                           const uint8_t *ref, int ref_stride, \
                                           const uint8_t *second_pred) {       \
    return sad##w##xh_avg_neon(src, src_stride, ref, ref_stride, (h),          \
                               second_pred);                                   \
  }

SAD_WXH_AVG_NEON(8, 8)
SAD_WXH_AVG_NEON(8, 16)

SAD_WXH_AVG_NEON(16, 8)
SAD_WXH_AVG_NEON(16, 16)
SAD_WXH_AVG_NEON(16, 32)

SAD_WXH_AVG_NEON(32, 16)
SAD_WXH_AVG_NEON(32, 32)
SAD_WXH_AVG_NEON(32, 64)

SAD_WXH_AVG_NEON(64, 32)
SAD_WXH_AVG_NEON(64, 64)
SAD_WXH_AVG_NEON(64, 128)

SAD_WXH_AVG_NEON(128, 64)
SAD_WXH_AVG_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_WXH_AVG_NEON(8, 32)
SAD_WXH_AVG_NEON(16, 64)
SAD_WXH_AVG_NEON(32, 8)
SAD_WXH_AVG_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_WXH_AVG_NEON
