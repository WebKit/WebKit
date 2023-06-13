/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

#if defined(__ARM_FEATURE_DOTPROD)

static INLINE void variance_4xh_neon(const uint8_t *src, int src_stride,
                                     const uint8_t *ref, int ref_stride, int h,
                                     uint32_t *sse, int *sum) {
  uint32x4_t src_sum = vdupq_n_u32(0);
  uint32x4_t ref_sum = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s = load_unaligned_u8q(src, src_stride);
    uint8x16_t r = load_unaligned_u8q(ref, ref_stride);

    src_sum = vdotq_u32(src_sum, s, vdupq_n_u8(1));
    ref_sum = vdotq_u32(ref_sum, r, vdupq_n_u8(1));

    uint8x16_t abs_diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

    src += 4 * src_stride;
    ref += 4 * ref_stride;
    i -= 4;
  } while (i != 0);

  int32x4_t sum_diff =
      vsubq_s32(vreinterpretq_s32_u32(src_sum), vreinterpretq_s32_u32(ref_sum));
  *sum = horizontal_add_s32x4(sum_diff);
  *sse = horizontal_add_u32x4(sse_u32);
}

static INLINE void variance_8xh_neon(const uint8_t *src, int src_stride,
                                     const uint8_t *ref, int ref_stride, int h,
                                     uint32_t *sse, int *sum) {
  uint32x4_t src_sum = vdupq_n_u32(0);
  uint32x4_t ref_sum = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s = vcombine_u8(vld1_u8(src), vld1_u8(src + src_stride));
    uint8x16_t r = vcombine_u8(vld1_u8(ref), vld1_u8(ref + ref_stride));

    src_sum = vdotq_u32(src_sum, s, vdupq_n_u8(1));
    ref_sum = vdotq_u32(ref_sum, r, vdupq_n_u8(1));

    uint8x16_t abs_diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    i -= 2;
  } while (i != 0);

  int32x4_t sum_diff =
      vsubq_s32(vreinterpretq_s32_u32(src_sum), vreinterpretq_s32_u32(ref_sum));
  *sum = horizontal_add_s32x4(sum_diff);
  *sse = horizontal_add_u32x4(sse_u32);
}

static INLINE void variance_16xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h,
                                      uint32_t *sse, int *sum) {
  uint32x4_t src_sum = vdupq_n_u32(0);
  uint32x4_t ref_sum = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s = vld1q_u8(src);
    uint8x16_t r = vld1q_u8(ref);

    src_sum = vdotq_u32(src_sum, s, vdupq_n_u8(1));
    ref_sum = vdotq_u32(ref_sum, r, vdupq_n_u8(1));

    uint8x16_t abs_diff = vabdq_u8(s, r);
    sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  int32x4_t sum_diff =
      vsubq_s32(vreinterpretq_s32_u32(src_sum), vreinterpretq_s32_u32(ref_sum));
  *sum = horizontal_add_s32x4(sum_diff);
  *sse = horizontal_add_u32x4(sse_u32);
}

static INLINE void variance_large_neon(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       int w, int h, uint32_t *sse, int *sum) {
  uint32x4_t src_sum = vdupq_n_u32(0);
  uint32x4_t ref_sum = vdupq_n_u32(0);
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    int j = 0;
    do {
      uint8x16_t s = vld1q_u8(src + j);
      uint8x16_t r = vld1q_u8(ref + j);

      src_sum = vdotq_u32(src_sum, s, vdupq_n_u8(1));
      ref_sum = vdotq_u32(ref_sum, r, vdupq_n_u8(1));

      uint8x16_t abs_diff = vabdq_u8(s, r);
      sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

      j += 16;
    } while (j < w);

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  int32x4_t sum_diff =
      vsubq_s32(vreinterpretq_s32_u32(src_sum), vreinterpretq_s32_u32(ref_sum));
  *sum = horizontal_add_s32x4(sum_diff);
  *sse = horizontal_add_u32x4(sse_u32);
}

static INLINE void variance_32xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h,
                                      uint32_t *sse, int *sum) {
  variance_large_neon(src, src_stride, ref, ref_stride, 32, h, sse, sum);
}

static INLINE void variance_64xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h,
                                      uint32_t *sse, int *sum) {
  variance_large_neon(src, src_stride, ref, ref_stride, 64, h, sse, sum);
}

static INLINE void variance_128xh_neon(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       int h, uint32_t *sse, int *sum) {
  variance_large_neon(src, src_stride, ref, ref_stride, 128, h, sse, sum);
}

#else  // !defined(__ARM_FEATURE_DOTPROD)

static INLINE void variance_4xh_neon(const uint8_t *src, int src_stride,
                                     const uint8_t *ref, int ref_stride, int h,
                                     uint32_t *sse, int *sum) {
  int16x8_t sum_s16 = vdupq_n_s16(0);
  int32x4_t sse_s32 = vdupq_n_s32(0);

  // Number of rows we can process before 'sum_s16' overflows:
  // 32767 / 255 ~= 128, but we use an 8-wide accumulator; so 256 4-wide rows.
  assert(h <= 256);

  int i = h;
  do {
    uint8x8_t s = load_unaligned_u8(src, src_stride);
    uint8x8_t r = load_unaligned_u8(ref, ref_stride);
    int16x8_t diff = vreinterpretq_s16_u16(vsubl_u8(s, r));

    sum_s16 = vaddq_s16(sum_s16, diff);

    sse_s32 = vmlal_s16(sse_s32, vget_low_s16(diff), vget_low_s16(diff));
    sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff), vget_high_s16(diff));

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    i -= 2;
  } while (i != 0);

  *sum = horizontal_add_s16x8(sum_s16);
  *sse = (uint32_t)horizontal_add_s32x4(sse_s32);
}

static INLINE void variance_8xh_neon(const uint8_t *src, int src_stride,
                                     const uint8_t *ref, int ref_stride, int h,
                                     uint32_t *sse, int *sum) {
  int16x8_t sum_s16 = vdupq_n_s16(0);
  int32x4_t sse_s32[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  // Number of rows we can process before 'sum_s16' overflows:
  // 32767 / 255 ~= 128
  assert(h <= 128);

  int i = h;
  do {
    uint8x8_t s = vld1_u8(src);
    uint8x8_t r = vld1_u8(ref);
    int16x8_t diff = vreinterpretq_s16_u16(vsubl_u8(s, r));

    sum_s16 = vaddq_s16(sum_s16, diff);

    sse_s32[0] = vmlal_s16(sse_s32[0], vget_low_s16(diff), vget_low_s16(diff));
    sse_s32[1] =
        vmlal_s16(sse_s32[1], vget_high_s16(diff), vget_high_s16(diff));

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  *sum = horizontal_add_s16x8(sum_s16);
  *sse = (uint32_t)horizontal_add_s32x4(vaddq_s32(sse_s32[0], sse_s32[1]));
}

static INLINE void variance_16xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h,
                                      uint32_t *sse, int *sum) {
  int16x8_t sum_s16[2] = { vdupq_n_s16(0), vdupq_n_s16(0) };
  int32x4_t sse_s32[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  // Number of rows we can process before 'sum_s16' accumulators overflow:
  // 32767 / 255 ~= 128, so 128 16-wide rows.
  assert(h <= 128);

  int i = h;
  do {
    uint8x16_t s = vld1q_u8(src);
    uint8x16_t r = vld1q_u8(ref);

    int16x8_t diff_l =
        vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(s), vget_low_u8(r)));
    int16x8_t diff_h =
        vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(s), vget_high_u8(r)));

    sum_s16[0] = vaddq_s16(sum_s16[0], diff_l);
    sum_s16[1] = vaddq_s16(sum_s16[1], diff_h);

    sse_s32[0] =
        vmlal_s16(sse_s32[0], vget_low_s16(diff_l), vget_low_s16(diff_l));
    sse_s32[1] =
        vmlal_s16(sse_s32[1], vget_high_s16(diff_l), vget_high_s16(diff_l));
    sse_s32[0] =
        vmlal_s16(sse_s32[0], vget_low_s16(diff_h), vget_low_s16(diff_h));
    sse_s32[1] =
        vmlal_s16(sse_s32[1], vget_high_s16(diff_h), vget_high_s16(diff_h));

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  *sum = horizontal_add_s16x8(vaddq_s16(sum_s16[0], sum_s16[1]));
  *sse = (uint32_t)horizontal_add_s32x4(vaddq_s32(sse_s32[0], sse_s32[1]));
}

static INLINE void variance_large_neon(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       int w, int h, int h_limit, uint32_t *sse,
                                       int *sum) {
  int32x4_t sum_s32 = vdupq_n_s32(0);
  int32x4_t sse_s32[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  // 'h_limit' is the number of 'w'-width rows we can process before our 16-bit
  // accumulator overflows. After hitting this limit we accumulate into 32-bit
  // elements.
  int h_tmp = h > h_limit ? h_limit : h;

  int i = 0;
  do {
    int16x8_t sum_s16[2] = { vdupq_n_s16(0), vdupq_n_s16(0) };
    do {
      int j = 0;
      do {
        uint8x16_t s = vld1q_u8(src + j);
        uint8x16_t r = vld1q_u8(ref + j);

        int16x8_t diff_l =
            vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(s), vget_low_u8(r)));
        int16x8_t diff_h =
            vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(s), vget_high_u8(r)));

        sum_s16[0] = vaddq_s16(sum_s16[0], diff_l);
        sum_s16[1] = vaddq_s16(sum_s16[1], diff_h);

        sse_s32[0] =
            vmlal_s16(sse_s32[0], vget_low_s16(diff_l), vget_low_s16(diff_l));
        sse_s32[1] =
            vmlal_s16(sse_s32[1], vget_high_s16(diff_l), vget_high_s16(diff_l));
        sse_s32[0] =
            vmlal_s16(sse_s32[0], vget_low_s16(diff_h), vget_low_s16(diff_h));
        sse_s32[1] =
            vmlal_s16(sse_s32[1], vget_high_s16(diff_h), vget_high_s16(diff_h));

        j += 16;
      } while (j < w);

      src += src_stride;
      ref += ref_stride;
      i++;
    } while (i < h_tmp);

    sum_s32 = vpadalq_s16(sum_s32, sum_s16[0]);
    sum_s32 = vpadalq_s16(sum_s32, sum_s16[1]);

    h_tmp += h_limit;
  } while (i < h);

  *sum = horizontal_add_s32x4(sum_s32);
  *sse = (uint32_t)horizontal_add_s32x4(vaddq_s32(sse_s32[0], sse_s32[1]));
}

static INLINE void variance_32xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h,
                                      uint32_t *sse, int *sum) {
  variance_large_neon(src, src_stride, ref, ref_stride, 32, h, 64, sse, sum);
}

static INLINE void variance_64xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride, int h,
                                      uint32_t *sse, int *sum) {
  variance_large_neon(src, src_stride, ref, ref_stride, 64, h, 32, sse, sum);
}

static INLINE void variance_128xh_neon(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       int h, uint32_t *sse, int *sum) {
  variance_large_neon(src, src_stride, ref, ref_stride, 128, h, 16, sse, sum);
}

#endif  // defined(__ARM_FEATURE_DOTPROD)

#define VARIANCE_WXH_NEON(w, h, shift)                                        \
  unsigned int aom_variance##w##x##h##_neon(                                  \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      unsigned int *sse) {                                                    \
    int sum;                                                                  \
    variance_##w##xh_neon(src, src_stride, ref, ref_stride, h, sse, &sum);    \
    return *sse - (uint32_t)(((int64_t)sum * sum) >> shift);                  \
  }

VARIANCE_WXH_NEON(4, 4, 4)
VARIANCE_WXH_NEON(4, 8, 5)
VARIANCE_WXH_NEON(4, 16, 6)

VARIANCE_WXH_NEON(8, 4, 5)
VARIANCE_WXH_NEON(8, 8, 6)
VARIANCE_WXH_NEON(8, 16, 7)
VARIANCE_WXH_NEON(8, 32, 8)

VARIANCE_WXH_NEON(16, 4, 6)
VARIANCE_WXH_NEON(16, 8, 7)
VARIANCE_WXH_NEON(16, 16, 8)
VARIANCE_WXH_NEON(16, 32, 9)
VARIANCE_WXH_NEON(16, 64, 10)

VARIANCE_WXH_NEON(32, 8, 8)
VARIANCE_WXH_NEON(32, 16, 9)
VARIANCE_WXH_NEON(32, 32, 10)
VARIANCE_WXH_NEON(32, 64, 11)

VARIANCE_WXH_NEON(64, 16, 10)
VARIANCE_WXH_NEON(64, 32, 11)
VARIANCE_WXH_NEON(64, 64, 12)
VARIANCE_WXH_NEON(64, 128, 13)

VARIANCE_WXH_NEON(128, 64, 13)
VARIANCE_WXH_NEON(128, 128, 14)

#undef VARIANCE_WXH_NEON

// TODO(yunqingwang): Perform variance of two/four 8x8 blocks similar to that of
// AVX2. Also, implement the NEON for variance computation present in this
// function.
void aom_get_var_sse_sum_8x8_quad_neon(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       uint32_t *sse8x8, int *sum8x8,
                                       unsigned int *tot_sse, int *tot_sum,
                                       uint32_t *var8x8) {
  // Loop over 4 8x8 blocks. Process one 8x32 block.
  for (int k = 0; k < 4; k++) {
    variance_8xh_neon(src + (k * 8), src_stride, ref + (k * 8), ref_stride, 8,
                      &sse8x8[k], &sum8x8[k]);
  }

  *tot_sse += sse8x8[0] + sse8x8[1] + sse8x8[2] + sse8x8[3];
  *tot_sum += sum8x8[0] + sum8x8[1] + sum8x8[2] + sum8x8[3];
  for (int i = 0; i < 4; i++)
    var8x8[i] = sse8x8[i] - (uint32_t)(((int64_t)sum8x8[i] * sum8x8[i]) >> 6);
}

void aom_get_var_sse_sum_16x16_dual_neon(const uint8_t *src, int src_stride,
                                         const uint8_t *ref, int ref_stride,
                                         uint32_t *sse16x16,
                                         unsigned int *tot_sse, int *tot_sum,
                                         uint32_t *var16x16) {
  int sum16x16[2] = { 0 };
  // Loop over 2 16x16 blocks. Process one 16x32 block.
  for (int k = 0; k < 2; k++) {
    variance_16xh_neon(src + (k * 16), src_stride, ref + (k * 16), ref_stride,
                       16, &sse16x16[k], &sum16x16[k]);
  }

  *tot_sse += sse16x16[0] + sse16x16[1];
  *tot_sum += sum16x16[0] + sum16x16[1];
  for (int i = 0; i < 2; i++)
    var16x16[i] =
        sse16x16[i] - (uint32_t)(((int64_t)sum16x16[i] * sum16x16[i]) >> 8);
}

#if defined(__ARM_FEATURE_DOTPROD)

static INLINE unsigned int mse8xh_neon(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       unsigned int *sse, int h) {
  uint32x4_t sse_u32 = vdupq_n_u32(0);

  int i = h;
  do {
    uint8x16_t s = vcombine_u8(vld1_u8(src), vld1_u8(src + src_stride));
    uint8x16_t r = vcombine_u8(vld1_u8(ref), vld1_u8(ref + ref_stride));

    uint8x16_t abs_diff = vabdq_u8(s, r);

    sse_u32 = vdotq_u32(sse_u32, abs_diff, abs_diff);

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    i -= 2;
  } while (i != 0);

  *sse = horizontal_add_u32x4(sse_u32);
  return horizontal_add_u32x4(sse_u32);
}

static INLINE unsigned int mse16xh_neon(const uint8_t *src, int src_stride,
                                        const uint8_t *ref, int ref_stride,
                                        unsigned int *sse, int h) {
  uint32x4_t sse_u32[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = h;
  do {
    uint8x16_t s0 = vld1q_u8(src);
    uint8x16_t s1 = vld1q_u8(src + src_stride);
    uint8x16_t r0 = vld1q_u8(ref);
    uint8x16_t r1 = vld1q_u8(ref + ref_stride);

    uint8x16_t abs_diff0 = vabdq_u8(s0, r0);
    uint8x16_t abs_diff1 = vabdq_u8(s1, r1);

    sse_u32[0] = vdotq_u32(sse_u32[0], abs_diff0, abs_diff0);
    sse_u32[1] = vdotq_u32(sse_u32[1], abs_diff1, abs_diff1);

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    i -= 2;
  } while (i != 0);

  *sse = horizontal_add_u32x4(vaddq_u32(sse_u32[0], sse_u32[1]));
  return horizontal_add_u32x4(vaddq_u32(sse_u32[0], sse_u32[1]));
}

#else  // !defined(__ARM_FEATURE_DOTPROD)

static INLINE unsigned int mse8xh_neon(const uint8_t *src, int src_stride,
                                       const uint8_t *ref, int ref_stride,
                                       unsigned int *sse, int h) {
  uint8x8_t s[2], r[2];
  int16x4_t diff_lo[2], diff_hi[2];
  uint16x8_t diff[2];
  int32x4_t sse_s32[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  int i = h;
  do {
    s[0] = vld1_u8(src);
    src += src_stride;
    s[1] = vld1_u8(src);
    src += src_stride;
    r[0] = vld1_u8(ref);
    ref += ref_stride;
    r[1] = vld1_u8(ref);
    ref += ref_stride;

    diff[0] = vsubl_u8(s[0], r[0]);
    diff[1] = vsubl_u8(s[1], r[1]);

    diff_lo[0] = vreinterpret_s16_u16(vget_low_u16(diff[0]));
    diff_lo[1] = vreinterpret_s16_u16(vget_low_u16(diff[1]));
    sse_s32[0] = vmlal_s16(sse_s32[0], diff_lo[0], diff_lo[0]);
    sse_s32[1] = vmlal_s16(sse_s32[1], diff_lo[1], diff_lo[1]);

    diff_hi[0] = vreinterpret_s16_u16(vget_high_u16(diff[0]));
    diff_hi[1] = vreinterpret_s16_u16(vget_high_u16(diff[1]));
    sse_s32[0] = vmlal_s16(sse_s32[0], diff_hi[0], diff_hi[0]);
    sse_s32[1] = vmlal_s16(sse_s32[1], diff_hi[1], diff_hi[1]);

    i -= 2;
  } while (i != 0);

  sse_s32[0] = vaddq_s32(sse_s32[0], sse_s32[1]);

  *sse = horizontal_add_u32x4(vreinterpretq_u32_s32(sse_s32[0]));
  return horizontal_add_u32x4(vreinterpretq_u32_s32(sse_s32[0]));
}

static INLINE unsigned int mse16xh_neon(const uint8_t *src, int src_stride,
                                        const uint8_t *ref, int ref_stride,
                                        unsigned int *sse, int h) {
  uint8x16_t s[2], r[2];
  int16x4_t diff_lo[4], diff_hi[4];
  uint16x8_t diff[4];
  int32x4_t sse_s32[4] = { vdupq_n_s32(0), vdupq_n_s32(0), vdupq_n_s32(0),
                           vdupq_n_s32(0) };

  int i = h;
  do {
    s[0] = vld1q_u8(src);
    src += src_stride;
    s[1] = vld1q_u8(src);
    src += src_stride;
    r[0] = vld1q_u8(ref);
    ref += ref_stride;
    r[1] = vld1q_u8(ref);
    ref += ref_stride;

    diff[0] = vsubl_u8(vget_low_u8(s[0]), vget_low_u8(r[0]));
    diff[1] = vsubl_u8(vget_high_u8(s[0]), vget_high_u8(r[0]));
    diff[2] = vsubl_u8(vget_low_u8(s[1]), vget_low_u8(r[1]));
    diff[3] = vsubl_u8(vget_high_u8(s[1]), vget_high_u8(r[1]));

    diff_lo[0] = vreinterpret_s16_u16(vget_low_u16(diff[0]));
    diff_lo[1] = vreinterpret_s16_u16(vget_low_u16(diff[1]));
    sse_s32[0] = vmlal_s16(sse_s32[0], diff_lo[0], diff_lo[0]);
    sse_s32[1] = vmlal_s16(sse_s32[1], diff_lo[1], diff_lo[1]);

    diff_lo[2] = vreinterpret_s16_u16(vget_low_u16(diff[2]));
    diff_lo[3] = vreinterpret_s16_u16(vget_low_u16(diff[3]));
    sse_s32[2] = vmlal_s16(sse_s32[2], diff_lo[2], diff_lo[2]);
    sse_s32[3] = vmlal_s16(sse_s32[3], diff_lo[3], diff_lo[3]);

    diff_hi[0] = vreinterpret_s16_u16(vget_high_u16(diff[0]));
    diff_hi[1] = vreinterpret_s16_u16(vget_high_u16(diff[1]));
    sse_s32[0] = vmlal_s16(sse_s32[0], diff_hi[0], diff_hi[0]);
    sse_s32[1] = vmlal_s16(sse_s32[1], diff_hi[1], diff_hi[1]);

    diff_hi[2] = vreinterpret_s16_u16(vget_high_u16(diff[2]));
    diff_hi[3] = vreinterpret_s16_u16(vget_high_u16(diff[3]));
    sse_s32[2] = vmlal_s16(sse_s32[2], diff_hi[2], diff_hi[2]);
    sse_s32[3] = vmlal_s16(sse_s32[3], diff_hi[3], diff_hi[3]);

    i -= 2;
  } while (i != 0);

  sse_s32[0] = vaddq_s32(sse_s32[0], sse_s32[1]);
  sse_s32[2] = vaddq_s32(sse_s32[2], sse_s32[3]);
  sse_s32[0] = vaddq_s32(sse_s32[0], sse_s32[2]);

  *sse = horizontal_add_u32x4(vreinterpretq_u32_s32(sse_s32[0]));
  return horizontal_add_u32x4(vreinterpretq_u32_s32(sse_s32[0]));
}

#endif  // defined(__ARM_FEATURE_DOTPROD)

#define MSE_WXH_NEON(w, h)                                                 \
  unsigned int aom_mse##w##x##h##_neon(const uint8_t *src, int src_stride, \
                                       const uint8_t *ref, int ref_stride, \
                                       unsigned int *sse) {                \
    return mse##w##xh_neon(src, src_stride, ref, ref_stride, sse, h);      \
  }

MSE_WXH_NEON(8, 8)
MSE_WXH_NEON(8, 16)

MSE_WXH_NEON(16, 8)
MSE_WXH_NEON(16, 16)

#undef MSE_WXH_NEON

#define COMPUTE_MSE_16BIT(src_16x8, dst_16x8)                           \
  /* r7 r6 r5 r4 r3 r2 r1 r0 - 16 bit */                                \
  const uint16x8_t diff = vabdq_u16(src_16x8, dst_16x8);                \
  /*r3 r2 r1 r0 - 16 bit */                                             \
  const uint16x4_t res0_low_16x4 = vget_low_u16(diff);                  \
  /*r7 r6 r5 r4 - 16 bit */                                             \
  const uint16x4_t res0_high_16x4 = vget_high_u16(diff);                \
  /* (r3*r3)= b3 (r2*r2)= b2 (r1*r1)= b1 (r0*r0)= b0 - 32 bit */        \
  const uint32x4_t res0_32x4 = vmull_u16(res0_low_16x4, res0_low_16x4); \
  /* (r7*r7)= b7 (r6*r6)= b6 (r5*r5)= b5 (r4*r4)= b4 - 32 bit*/         \
  /* b3+b7 b2+b6 b1+b5 b0+b4 - 32 bit*/                                 \
  const uint32x4_t res_32x4 =                                           \
      vmlal_u16(res0_32x4, res0_high_16x4, res0_high_16x4);             \
                                                                        \
  /*a1 a0 - 64 bit*/                                                    \
  const uint64x2_t vl = vpaddlq_u32(res_32x4);                          \
  /*a1+a2= f1 a3+a0= f0*/                                               \
  square_result = vaddq_u64(square_result, vl);

static AOM_INLINE uint64_t mse_4xh_16bit_neon(uint8_t *dst, int dstride,
                                              uint16_t *src, int sstride,
                                              int h) {
  uint64x2_t square_result = vdupq_n_u64(0);
  uint32_t d0, d1;
  int i = h;
  uint8_t *dst_ptr = dst;
  uint16_t *src_ptr = src;
  do {
    // d03 d02 d01 d00 - 8 bit
    memcpy(&d0, dst_ptr, 4);
    dst_ptr += dstride;
    // d13 d12 d11 d10 - 8 bit
    memcpy(&d1, dst_ptr, 4);
    dst_ptr += dstride;
    // duplication
    uint8x8_t tmp0_8x8 = vreinterpret_u8_u32(vdup_n_u32(d0));
    // d03 d02 d01 d00 - 16 bit
    const uint16x4_t dst0_16x4 = vget_low_u16(vmovl_u8(tmp0_8x8));
    // duplication
    tmp0_8x8 = vreinterpret_u8_u32(vdup_n_u32(d1));
    // d13 d12 d11 d10 - 16 bit
    const uint16x4_t dst1_16x4 = vget_low_u16(vmovl_u8(tmp0_8x8));
    // d13 d12 d11 d10 d03 d02 d01 d00 - 16 bit
    const uint16x8_t dst_16x8 = vcombine_u16(dst0_16x4, dst1_16x4);

    // b1r0 - s03 s02 s01 s00 - 16 bit
    const uint16x4_t src0_16x4 = vld1_u16(src_ptr);
    src_ptr += sstride;
    // b1r1 - s13 s12 s11 s10 - 16 bit
    const uint16x4_t src1_16x4 = vld1_u16(src_ptr);
    src_ptr += sstride;
    // s13 s12 s11 s10 s03 s02 s01 s00 - 16 bit
    const uint16x8_t src_16x8 = vcombine_u16(src0_16x4, src1_16x4);

    COMPUTE_MSE_16BIT(src_16x8, dst_16x8)
    i -= 2;
  } while (i != 0);
  uint64x1_t sum =
      vadd_u64(vget_high_u64(square_result), vget_low_u64(square_result));
  return vget_lane_u64(sum, 0);
}

static AOM_INLINE uint64_t mse_8xh_16bit_neon(uint8_t *dst, int dstride,
                                              uint16_t *src, int sstride,
                                              int h) {
  uint64x2_t square_result = vdupq_n_u64(0);
  int i = h;
  do {
    // d7 d6 d5 d4 d3 d2 d1 d0 - 8 bit
    const uint16x8_t dst_16x8 = vmovl_u8(vld1_u8(dst));
    // s7 s6 s5 s4 s3 s2 s1 s0 - 16 bit
    const uint16x8_t src_16x8 = vld1q_u16(src);

    COMPUTE_MSE_16BIT(src_16x8, dst_16x8)

    dst += dstride;
    src += sstride;
  } while (--i != 0);
  uint64x1_t sum =
      vadd_u64(vget_high_u64(square_result), vget_low_u64(square_result));
  return vget_lane_u64(sum, 0);
}

// Computes mse for a given block size. This function gets called for specific
// block sizes, which are 8x8, 8x4, 4x8 and 4x4.
uint64_t aom_mse_wxh_16bit_neon(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int w, int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4) &&
         "w=8/4 and h=8/4 must satisfy");
  switch (w) {
    case 4: return mse_4xh_16bit_neon(dst, dstride, src, sstride, h);
    case 8: return mse_8xh_16bit_neon(dst, dstride, src, sstride, h);
    default: assert(0 && "unsupported width"); return -1;
  }
}
