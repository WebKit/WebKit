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

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_ports/mem.h"
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

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
  // Loop over four 8x8 blocks. Process one 8x32 block.
  for (int k = 0; k < 4; k++) {
    variance_8xh_neon(src + (k * 8), src_stride, ref + (k * 8), ref_stride, 8,
                      &sse8x8[k], &sum8x8[k]);
  }

  *tot_sse += sse8x8[0] + sse8x8[1] + sse8x8[2] + sse8x8[3];
  *tot_sum += sum8x8[0] + sum8x8[1] + sum8x8[2] + sum8x8[3];
  for (int i = 0; i < 4; i++) {
    var8x8[i] = sse8x8[i] - (uint32_t)(((int64_t)sum8x8[i] * sum8x8[i]) >> 6);
  }
}

void aom_get_var_sse_sum_16x16_dual_neon(const uint8_t *src, int src_stride,
                                         const uint8_t *ref, int ref_stride,
                                         uint32_t *sse16x16,
                                         unsigned int *tot_sse, int *tot_sum,
                                         uint32_t *var16x16) {
  int sum16x16[2] = { 0 };
  // Loop over two 16x16 blocks. Process one 16x32 block.
  for (int k = 0; k < 2; k++) {
    variance_16xh_neon(src + (k * 16), src_stride, ref + (k * 16), ref_stride,
                       16, &sse16x16[k], &sum16x16[k]);
  }

  *tot_sse += sse16x16[0] + sse16x16[1];
  *tot_sum += sum16x16[0] + sum16x16[1];
  for (int i = 0; i < 2; i++) {
    var16x16[i] =
        sse16x16[i] - (uint32_t)(((int64_t)sum16x16[i] * sum16x16[i]) >> 8);
  }
}

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

static INLINE uint64x2_t mse_accumulate_u16_u8_8x2(uint64x2_t sum,
                                                   uint16x8_t s0, uint16x8_t s1,
                                                   uint8x8_t d0, uint8x8_t d1) {
  int16x8_t e0 = vreinterpretq_s16_u16(vsubw_u8(s0, d0));
  int16x8_t e1 = vreinterpretq_s16_u16(vsubw_u8(s1, d1));

  int32x4_t mse = vmull_s16(vget_low_s16(e0), vget_low_s16(e0));
  mse = vmlal_s16(mse, vget_high_s16(e0), vget_high_s16(e0));
  mse = vmlal_s16(mse, vget_low_s16(e1), vget_low_s16(e1));
  mse = vmlal_s16(mse, vget_high_s16(e1), vget_high_s16(e1));

  return vpadalq_u32(sum, vreinterpretq_u32_s32(mse));
}

static uint64x2_t mse_wxh_16bit(uint8_t *dst, int dstride, const uint16_t *src,
                                int sstride, int w, int h) {
  assert((w == 8 || w == 4) && (h == 8 || h == 4));

  uint64x2_t sum = vdupq_n_u64(0);

  if (w == 8) {
    do {
      uint8x8_t d0 = vld1_u8(dst + 0 * dstride);
      uint8x8_t d1 = vld1_u8(dst + 1 * dstride);
      uint16x8_t s0 = vld1q_u16(src + 0 * sstride);
      uint16x8_t s1 = vld1q_u16(src + 1 * sstride);

      sum = mse_accumulate_u16_u8_8x2(sum, s0, s1, d0, d1);

      dst += 2 * dstride;
      src += 2 * sstride;
      h -= 2;
    } while (h != 0);
  } else {
    do {
      uint8x8_t d0 = load_unaligned_u8_4x2(dst + 0 * dstride, dstride);
      uint8x8_t d1 = load_unaligned_u8_4x2(dst + 2 * dstride, dstride);
      uint16x8_t s0 = load_unaligned_u16_4x2(src + 0 * sstride, sstride);
      uint16x8_t s1 = load_unaligned_u16_4x2(src + 2 * sstride, sstride);

      sum = mse_accumulate_u16_u8_8x2(sum, s0, s1, d0, d1);

      dst += 4 * dstride;
      src += 4 * sstride;
      h -= 4;
    } while (h != 0);
  }

  return sum;
}

// Computes mse for a given block size. This function gets called for specific
// block sizes, which are 8x8, 8x4, 4x8 and 4x4.
uint64_t aom_mse_wxh_16bit_neon(uint8_t *dst, int dstride, uint16_t *src,
                                int sstride, int w, int h) {
  return horizontal_add_u64x2(mse_wxh_16bit(dst, dstride, src, sstride, w, h));
}

uint32_t aom_get_mb_ss_neon(const int16_t *a) {
  int32x4_t sse[2] = { vdupq_n_s32(0), vdupq_n_s32(0) };

  for (int i = 0; i < 256; i = i + 8) {
    int16x8_t a_s16 = vld1q_s16(a + i);

    sse[0] = vmlal_s16(sse[0], vget_low_s16(a_s16), vget_low_s16(a_s16));
    sse[1] = vmlal_s16(sse[1], vget_high_s16(a_s16), vget_high_s16(a_s16));
  }

  return horizontal_add_s32x4(vaddq_s32(sse[0], sse[1]));
}

uint64_t aom_mse_16xh_16bit_neon(uint8_t *dst, int dstride, uint16_t *src,
                                 int w, int h) {
  uint64x2_t sum = vdupq_n_u64(0);

  int num_blks = 16 / w;
  do {
    sum = vaddq_u64(sum, mse_wxh_16bit(dst, dstride, src, w, w, h));
    dst += w;
    src += w * h;
  } while (--num_blks != 0);

  return horizontal_add_u64x2(sum);
}
