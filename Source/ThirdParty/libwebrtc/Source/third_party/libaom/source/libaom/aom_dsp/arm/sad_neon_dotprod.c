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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/dist_wtd_avg_neon.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

static inline unsigned int sadwxh_neon_dotprod(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *ref_ptr,
                                               int ref_stride, int w, int h) {
  // Only two accumulators are required for optimal instruction throughput of
  // the ABD, UDOT sequence on CPUs with either 2 or 4 Neon pipes.
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h;
  do {
    int j = 0;
    do {
      uint8x16_t s0, s1, r0, r1, diff0, diff1;

      s0 = vld1q_u8(src_ptr + j);
      r0 = vld1q_u8(ref_ptr + j);
      diff0 = vabdq_u8(s0, r0);
      sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

      s1 = vld1q_u8(src_ptr + j + 16);
      r1 = vld1q_u8(ref_ptr + j + 16);
      diff1 = vabdq_u8(s1, r1);
      sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

      j += 32;
    } while (j < w);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_u32x4(vaddq_u32(sum[0], sum[1]));
}

static inline unsigned int sad128xh_neon_dotprod(const uint8_t *src_ptr,
                                                 int src_stride,
                                                 const uint8_t *ref_ptr,
                                                 int ref_stride, int h) {
  return sadwxh_neon_dotprod(src_ptr, src_stride, ref_ptr, ref_stride, 128, h);
}

static inline unsigned int sad64xh_neon_dotprod(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *ref_ptr,
                                                int ref_stride, int h) {
  return sadwxh_neon_dotprod(src_ptr, src_stride, ref_ptr, ref_stride, 64, h);
}

static inline unsigned int sad32xh_neon_dotprod(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *ref_ptr,
                                                int ref_stride, int h) {
  return sadwxh_neon_dotprod(src_ptr, src_stride, ref_ptr, ref_stride, 32, h);
}

static inline unsigned int sad16xh_neon_dotprod(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *ref_ptr,
                                                int ref_stride, int h) {
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h / 2;
  do {
    uint8x16_t s0, s1, r0, r1, diff0, diff1;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    diff0 = vabdq_u8(s0, r0);
    sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;

    s1 = vld1q_u8(src_ptr);
    r1 = vld1q_u8(ref_ptr);
    diff1 = vabdq_u8(s1, r1);
    sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_u32x4(vaddq_u32(sum[0], sum[1]));
}

#define SAD_WXH_NEON_DOTPROD(w, h)                                         \
  unsigned int aom_sad##w##x##h##_neon_dotprod(                            \
      const uint8_t *src, int src_stride, const uint8_t *ref,              \
      int ref_stride) {                                                    \
    return sad##w##xh_neon_dotprod(src, src_stride, ref, ref_stride, (h)); \
  }

SAD_WXH_NEON_DOTPROD(16, 8)
SAD_WXH_NEON_DOTPROD(16, 16)
SAD_WXH_NEON_DOTPROD(16, 32)

SAD_WXH_NEON_DOTPROD(32, 16)
SAD_WXH_NEON_DOTPROD(32, 32)
SAD_WXH_NEON_DOTPROD(32, 64)

SAD_WXH_NEON_DOTPROD(64, 32)
SAD_WXH_NEON_DOTPROD(64, 64)
SAD_WXH_NEON_DOTPROD(64, 128)

SAD_WXH_NEON_DOTPROD(128, 64)
SAD_WXH_NEON_DOTPROD(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_WXH_NEON_DOTPROD(16, 4)
SAD_WXH_NEON_DOTPROD(16, 64)
SAD_WXH_NEON_DOTPROD(32, 8)
SAD_WXH_NEON_DOTPROD(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_WXH_NEON_DOTPROD

#define SAD_SKIP_WXH_NEON_DOTPROD(w, h)                          \
  unsigned int aom_sad_skip_##w##x##h##_neon_dotprod(            \
      const uint8_t *src, int src_stride, const uint8_t *ref,    \
      int ref_stride) {                                          \
    return 2 * sad##w##xh_neon_dotprod(src, 2 * src_stride, ref, \
                                       2 * ref_stride, (h) / 2); \
  }

SAD_SKIP_WXH_NEON_DOTPROD(16, 16)
SAD_SKIP_WXH_NEON_DOTPROD(16, 32)

SAD_SKIP_WXH_NEON_DOTPROD(32, 16)
SAD_SKIP_WXH_NEON_DOTPROD(32, 32)
SAD_SKIP_WXH_NEON_DOTPROD(32, 64)

SAD_SKIP_WXH_NEON_DOTPROD(64, 32)
SAD_SKIP_WXH_NEON_DOTPROD(64, 64)
SAD_SKIP_WXH_NEON_DOTPROD(64, 128)

SAD_SKIP_WXH_NEON_DOTPROD(128, 64)
SAD_SKIP_WXH_NEON_DOTPROD(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_SKIP_WXH_NEON_DOTPROD(16, 64)
SAD_SKIP_WXH_NEON_DOTPROD(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_SKIP_WXH_NEON_DOTPROD

static inline unsigned int sadwxh_avg_neon_dotprod(const uint8_t *src_ptr,
                                                   int src_stride,
                                                   const uint8_t *ref_ptr,
                                                   int ref_stride, int w, int h,
                                                   const uint8_t *second_pred) {
  // Only two accumulators are required for optimal instruction throughput of
  // the ABD, UDOT sequence on CPUs with either 2 or 4 Neon pipes.
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h;
  do {
    int j = 0;
    do {
      uint8x16_t s0, s1, r0, r1, p0, p1, avg0, avg1, diff0, diff1;

      s0 = vld1q_u8(src_ptr + j);
      r0 = vld1q_u8(ref_ptr + j);
      p0 = vld1q_u8(second_pred);
      avg0 = vrhaddq_u8(r0, p0);
      diff0 = vabdq_u8(s0, avg0);
      sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

      s1 = vld1q_u8(src_ptr + j + 16);
      r1 = vld1q_u8(ref_ptr + j + 16);
      p1 = vld1q_u8(second_pred + 16);
      avg1 = vrhaddq_u8(r1, p1);
      diff1 = vabdq_u8(s1, avg1);
      sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

      j += 32;
      second_pred += 32;
    } while (j < w);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  } while (--i != 0);

  return horizontal_add_u32x4(vaddq_u32(sum[0], sum[1]));
}

static inline unsigned int sad128xh_avg_neon_dotprod(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred) {
  return sadwxh_avg_neon_dotprod(src_ptr, src_stride, ref_ptr, ref_stride, 128,
                                 h, second_pred);
}

static inline unsigned int sad64xh_avg_neon_dotprod(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred) {
  return sadwxh_avg_neon_dotprod(src_ptr, src_stride, ref_ptr, ref_stride, 64,
                                 h, second_pred);
}

static inline unsigned int sad32xh_avg_neon_dotprod(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred) {
  return sadwxh_avg_neon_dotprod(src_ptr, src_stride, ref_ptr, ref_stride, 32,
                                 h, second_pred);
}

static inline unsigned int sad16xh_avg_neon_dotprod(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred) {
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h / 2;
  do {
    uint8x16_t s0, s1, r0, r1, p0, p1, avg0, avg1, diff0, diff1;

    s0 = vld1q_u8(src_ptr);
    r0 = vld1q_u8(ref_ptr);
    p0 = vld1q_u8(second_pred);
    avg0 = vrhaddq_u8(r0, p0);
    diff0 = vabdq_u8(s0, avg0);
    sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 16;

    s1 = vld1q_u8(src_ptr);
    r1 = vld1q_u8(ref_ptr);
    p1 = vld1q_u8(second_pred);
    avg1 = vrhaddq_u8(r1, p1);
    diff1 = vabdq_u8(s1, avg1);
    sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 16;
  } while (--i != 0);

  return horizontal_add_u32x4(vaddq_u32(sum[0], sum[1]));
}

#define SAD_WXH_AVG_NEON_DOTPROD(w, h)                                        \
  unsigned int aom_sad##w##x##h##_avg_neon_dotprod(                           \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred) {                                           \
    return sad##w##xh_avg_neon_dotprod(src, src_stride, ref, ref_stride, (h), \
                                       second_pred);                          \
  }

SAD_WXH_AVG_NEON_DOTPROD(16, 8)
SAD_WXH_AVG_NEON_DOTPROD(16, 16)
SAD_WXH_AVG_NEON_DOTPROD(16, 32)

SAD_WXH_AVG_NEON_DOTPROD(32, 16)
SAD_WXH_AVG_NEON_DOTPROD(32, 32)
SAD_WXH_AVG_NEON_DOTPROD(32, 64)

SAD_WXH_AVG_NEON_DOTPROD(64, 32)
SAD_WXH_AVG_NEON_DOTPROD(64, 64)
SAD_WXH_AVG_NEON_DOTPROD(64, 128)

SAD_WXH_AVG_NEON_DOTPROD(128, 64)
SAD_WXH_AVG_NEON_DOTPROD(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_WXH_AVG_NEON_DOTPROD(16, 4)
SAD_WXH_AVG_NEON_DOTPROD(16, 64)
SAD_WXH_AVG_NEON_DOTPROD(32, 8)
SAD_WXH_AVG_NEON_DOTPROD(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_WXH_AVG_NEON_DOTPROD

static inline unsigned int dist_wtd_sad128xh_avg_neon_dotprod(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint8x16_t fwd_offset = vdupq_n_u8(jcp_param->fwd_offset);
  const uint8x16_t bck_offset = vdupq_n_u8(jcp_param->bck_offset);
  // We use 8 accumulators to minimize the accumulation and loop carried
  // dependencies for better instruction throughput.
  uint32x4_t sum[8] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0), vdupq_n_u32(0) };

  do {
    uint8x16_t s0 = vld1q_u8(src_ptr);
    uint8x16_t r0 = vld1q_u8(ref_ptr);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t wtd_avg0 = dist_wtd_avg_u8x16(p0, r0, bck_offset, fwd_offset);
    uint8x16_t diff0 = vabdq_u8(s0, wtd_avg0);
    sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

    uint8x16_t s1 = vld1q_u8(src_ptr + 16);
    uint8x16_t r1 = vld1q_u8(ref_ptr + 16);
    uint8x16_t p1 = vld1q_u8(second_pred + 16);
    uint8x16_t wtd_avg1 = dist_wtd_avg_u8x16(p1, r1, bck_offset, fwd_offset);
    uint8x16_t diff1 = vabdq_u8(s1, wtd_avg1);
    sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

    uint8x16_t s2 = vld1q_u8(src_ptr + 32);
    uint8x16_t r2 = vld1q_u8(ref_ptr + 32);
    uint8x16_t p2 = vld1q_u8(second_pred + 32);
    uint8x16_t wtd_avg2 = dist_wtd_avg_u8x16(p2, r2, bck_offset, fwd_offset);
    uint8x16_t diff2 = vabdq_u8(s2, wtd_avg2);
    sum[2] = vdotq_u32(sum[2], diff2, vdupq_n_u8(1));

    uint8x16_t s3 = vld1q_u8(src_ptr + 48);
    uint8x16_t r3 = vld1q_u8(ref_ptr + 48);
    uint8x16_t p3 = vld1q_u8(second_pred + 48);
    uint8x16_t wtd_avg3 = dist_wtd_avg_u8x16(p3, r3, bck_offset, fwd_offset);
    uint8x16_t diff3 = vabdq_u8(s3, wtd_avg3);
    sum[3] = vdotq_u32(sum[3], diff3, vdupq_n_u8(1));

    uint8x16_t s4 = vld1q_u8(src_ptr + 64);
    uint8x16_t r4 = vld1q_u8(ref_ptr + 64);
    uint8x16_t p4 = vld1q_u8(second_pred + 64);
    uint8x16_t wtd_avg4 = dist_wtd_avg_u8x16(p4, r4, bck_offset, fwd_offset);
    uint8x16_t diff4 = vabdq_u8(s4, wtd_avg4);
    sum[4] = vdotq_u32(sum[4], diff4, vdupq_n_u8(1));

    uint8x16_t s5 = vld1q_u8(src_ptr + 80);
    uint8x16_t r5 = vld1q_u8(ref_ptr + 80);
    uint8x16_t p5 = vld1q_u8(second_pred + 80);
    uint8x16_t wtd_avg5 = dist_wtd_avg_u8x16(p5, r5, bck_offset, fwd_offset);
    uint8x16_t diff5 = vabdq_u8(s5, wtd_avg5);
    sum[5] = vdotq_u32(sum[5], diff5, vdupq_n_u8(1));

    uint8x16_t s6 = vld1q_u8(src_ptr + 96);
    uint8x16_t r6 = vld1q_u8(ref_ptr + 96);
    uint8x16_t p6 = vld1q_u8(second_pred + 96);
    uint8x16_t wtd_avg6 = dist_wtd_avg_u8x16(p6, r6, bck_offset, fwd_offset);
    uint8x16_t diff6 = vabdq_u8(s6, wtd_avg6);
    sum[6] = vdotq_u32(sum[6], diff6, vdupq_n_u8(1));

    uint8x16_t s7 = vld1q_u8(src_ptr + 112);
    uint8x16_t r7 = vld1q_u8(ref_ptr + 112);
    uint8x16_t p7 = vld1q_u8(second_pred + 112);
    uint8x16_t wtd_avg7 = dist_wtd_avg_u8x16(p7, r7, bck_offset, fwd_offset);
    uint8x16_t diff7 = vabdq_u8(s7, wtd_avg7);
    sum[7] = vdotq_u32(sum[7], diff7, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 128;
  } while (--h != 0);

  sum[0] = vaddq_u32(sum[0], sum[1]);
  sum[2] = vaddq_u32(sum[2], sum[3]);
  sum[4] = vaddq_u32(sum[4], sum[5]);
  sum[6] = vaddq_u32(sum[6], sum[7]);
  sum[0] = vaddq_u32(sum[0], sum[2]);
  sum[4] = vaddq_u32(sum[4], sum[6]);
  sum[0] = vaddq_u32(sum[0], sum[4]);
  return horizontal_add_u32x4(sum[0]);
}

static inline unsigned int dist_wtd_sad64xh_avg_neon_dotprod(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint8x16_t fwd_offset = vdupq_n_u8(jcp_param->fwd_offset);
  const uint8x16_t bck_offset = vdupq_n_u8(jcp_param->bck_offset);
  uint32x4_t sum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0) };

  do {
    uint8x16_t s0 = vld1q_u8(src_ptr);
    uint8x16_t r0 = vld1q_u8(ref_ptr);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t wtd_avg0 = dist_wtd_avg_u8x16(p0, r0, bck_offset, fwd_offset);
    uint8x16_t diff0 = vabdq_u8(s0, wtd_avg0);
    sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

    uint8x16_t s1 = vld1q_u8(src_ptr + 16);
    uint8x16_t r1 = vld1q_u8(ref_ptr + 16);
    uint8x16_t p1 = vld1q_u8(second_pred + 16);
    uint8x16_t wtd_avg1 = dist_wtd_avg_u8x16(p1, r1, bck_offset, fwd_offset);
    uint8x16_t diff1 = vabdq_u8(s1, wtd_avg1);
    sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

    uint8x16_t s2 = vld1q_u8(src_ptr + 32);
    uint8x16_t r2 = vld1q_u8(ref_ptr + 32);
    uint8x16_t p2 = vld1q_u8(second_pred + 32);
    uint8x16_t wtd_avg2 = dist_wtd_avg_u8x16(p2, r2, bck_offset, fwd_offset);
    uint8x16_t diff2 = vabdq_u8(s2, wtd_avg2);
    sum[2] = vdotq_u32(sum[2], diff2, vdupq_n_u8(1));

    uint8x16_t s3 = vld1q_u8(src_ptr + 48);
    uint8x16_t r3 = vld1q_u8(ref_ptr + 48);
    uint8x16_t p3 = vld1q_u8(second_pred + 48);
    uint8x16_t wtd_avg3 = dist_wtd_avg_u8x16(p3, r3, bck_offset, fwd_offset);
    uint8x16_t diff3 = vabdq_u8(s3, wtd_avg3);
    sum[3] = vdotq_u32(sum[3], diff3, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 64;
  } while (--h != 0);

  sum[0] = vaddq_u32(sum[0], sum[1]);
  sum[2] = vaddq_u32(sum[2], sum[3]);
  sum[0] = vaddq_u32(sum[0], sum[2]);
  return horizontal_add_u32x4(sum[0]);
}

static inline unsigned int dist_wtd_sad32xh_avg_neon_dotprod(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint8x16_t fwd_offset = vdupq_n_u8(jcp_param->fwd_offset);
  const uint8x16_t bck_offset = vdupq_n_u8(jcp_param->bck_offset);
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  do {
    uint8x16_t s0 = vld1q_u8(src_ptr);
    uint8x16_t r0 = vld1q_u8(ref_ptr);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t wtd_avg0 = dist_wtd_avg_u8x16(p0, r0, bck_offset, fwd_offset);
    uint8x16_t diff0 = vabdq_u8(s0, wtd_avg0);
    sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

    uint8x16_t s1 = vld1q_u8(src_ptr + 16);
    uint8x16_t r1 = vld1q_u8(ref_ptr + 16);
    uint8x16_t p1 = vld1q_u8(second_pred + 16);
    uint8x16_t wtd_avg1 = dist_wtd_avg_u8x16(p1, r1, bck_offset, fwd_offset);
    uint8x16_t diff1 = vabdq_u8(s1, wtd_avg1);
    sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 32;
  } while (--h != 0);

  sum[0] = vaddq_u32(sum[0], sum[1]);
  return horizontal_add_u32x4(sum[0]);
}

static inline unsigned int dist_wtd_sad16xh_avg_neon_dotprod(
    const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,
    int ref_stride, int h, const uint8_t *second_pred,
    const DIST_WTD_COMP_PARAMS *jcp_param) {
  const uint8x16_t fwd_offset = vdupq_n_u8(jcp_param->fwd_offset);
  const uint8x16_t bck_offset = vdupq_n_u8(jcp_param->bck_offset);
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h / 2;
  do {
    uint8x16_t s0 = vld1q_u8(src_ptr);
    uint8x16_t r0 = vld1q_u8(ref_ptr);
    uint8x16_t p0 = vld1q_u8(second_pred);
    uint8x16_t wtd_avg0 = dist_wtd_avg_u8x16(p0, r0, bck_offset, fwd_offset);
    uint8x16_t diff0 = vabdq_u8(s0, wtd_avg0);
    sum[0] = vdotq_u32(sum[0], diff0, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 16;

    uint8x16_t s1 = vld1q_u8(src_ptr);
    uint8x16_t r1 = vld1q_u8(ref_ptr);
    uint8x16_t p1 = vld1q_u8(second_pred);
    uint8x16_t wtd_avg1 = dist_wtd_avg_u8x16(p1, r1, bck_offset, fwd_offset);
    uint8x16_t diff1 = vabdq_u8(s1, wtd_avg1);
    sum[1] = vdotq_u32(sum[1], diff1, vdupq_n_u8(1));

    src_ptr += src_stride;
    ref_ptr += ref_stride;
    second_pred += 16;
  } while (--i != 0);

  sum[0] = vaddq_u32(sum[0], sum[1]);
  return horizontal_add_u32x4(sum[0]);
}

#define DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(w, h)                               \
  unsigned int aom_dist_wtd_sad##w##x##h##_avg_neon_dotprod(                  \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    return dist_wtd_sad##w##xh_avg_neon_dotprod(                              \
        src, src_stride, ref, ref_stride, (h), second_pred, jcp_param);       \
  }

DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(16, 8)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(16, 16)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(16, 32)

DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(32, 16)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(32, 32)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(32, 64)

DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(64, 32)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(64, 64)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(64, 128)

DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(128, 64)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(128, 128)

#if !CONFIG_REALTIME_ONLY
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(16, 4)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(16, 64)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(32, 8)
DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef DIST_WTD_SAD_WXH_AVG_NEON_DOTPROD
