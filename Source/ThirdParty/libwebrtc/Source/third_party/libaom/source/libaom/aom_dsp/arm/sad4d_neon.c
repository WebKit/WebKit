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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

#if defined(__ARM_FEATURE_DOTPROD)

static INLINE void sad16_neon(uint8x16_t src, uint8x16_t ref,
                              uint32x4_t *const sad_sum) {
  uint8x16_t abs_diff = vabdq_u8(src, ref);
  *sad_sum = vdotq_u32(*sad_sum, abs_diff, vdupq_n_u8(1));
}

static INLINE void sadwxhx4d_large_neon(const uint8_t *src, int src_stride,
                                        const uint8_t *const ref[4],
                                        int ref_stride, uint32_t res[4], int w,
                                        int h) {
  uint32x4_t sum_lo[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                           vdupq_n_u32(0) };
  uint32x4_t sum_hi[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                           vdupq_n_u32(0) };
  uint32x4_t sum[4];

  int ref_offset = 0;
  int i = h;
  do {
    int j = 0;
    do {
      const uint8x16_t s0 = vld1q_u8(src + j);
      sad16_neon(s0, vld1q_u8(ref[0] + ref_offset + j), &sum_lo[0]);
      sad16_neon(s0, vld1q_u8(ref[1] + ref_offset + j), &sum_lo[1]);
      sad16_neon(s0, vld1q_u8(ref[2] + ref_offset + j), &sum_lo[2]);
      sad16_neon(s0, vld1q_u8(ref[3] + ref_offset + j), &sum_lo[3]);

      const uint8x16_t s1 = vld1q_u8(src + j + 16);
      sad16_neon(s1, vld1q_u8(ref[0] + ref_offset + j + 16), &sum_hi[0]);
      sad16_neon(s1, vld1q_u8(ref[1] + ref_offset + j + 16), &sum_hi[1]);
      sad16_neon(s1, vld1q_u8(ref[2] + ref_offset + j + 16), &sum_hi[2]);
      sad16_neon(s1, vld1q_u8(ref[3] + ref_offset + j + 16), &sum_hi[3]);

      j += 32;
    } while (j < w);

    src += src_stride;
    ref_offset += ref_stride;
  } while (--i != 0);

  sum[0] = vaddq_u32(sum_lo[0], sum_hi[0]);
  sum[1] = vaddq_u32(sum_lo[1], sum_hi[1]);
  sum[2] = vaddq_u32(sum_lo[2], sum_hi[2]);
  sum[3] = vaddq_u32(sum_lo[3], sum_hi[3]);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}

static INLINE void sad128xhx4d_neon(const uint8_t *src, int src_stride,
                                    const uint8_t *const ref[4], int ref_stride,
                                    uint32_t res[4], int h) {
  sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, res, 128, h);
}

static INLINE void sad64xhx4d_neon(const uint8_t *src, int src_stride,
                                   const uint8_t *const ref[4], int ref_stride,
                                   uint32_t res[4], int h) {
  sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, res, 64, h);
}

static INLINE void sad32xhx4d_neon(const uint8_t *src, int src_stride,
                                   const uint8_t *const ref[4], int ref_stride,
                                   uint32_t res[4], int h) {
  sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, res, 32, h);
}

static INLINE void sad16xhx4d_neon(const uint8_t *src, int src_stride,
                                   const uint8_t *const ref[4], int ref_stride,
                                   uint32_t res[4], int h) {
  uint32x4_t sum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0) };

  int ref_offset = 0;
  int i = h;
  do {
    const uint8x16_t s = vld1q_u8(src);
    sad16_neon(s, vld1q_u8(ref[0] + ref_offset), &sum[0]);
    sad16_neon(s, vld1q_u8(ref[1] + ref_offset), &sum[1]);
    sad16_neon(s, vld1q_u8(ref[2] + ref_offset), &sum[2]);
    sad16_neon(s, vld1q_u8(ref[3] + ref_offset), &sum[3]);

    src += src_stride;
    ref_offset += ref_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}

#else  // !(defined(__ARM_FEATURE_DOTPROD))

static INLINE void sad16_neon(uint8x16_t src, uint8x16_t ref,
                              uint16x8_t *const sad_sum) {
  uint8x16_t abs_diff = vabdq_u8(src, ref);
  *sad_sum = vpadalq_u8(*sad_sum, abs_diff);
}

static INLINE void sadwxhx4d_large_neon(const uint8_t *src, int src_stride,
                                        const uint8_t *const ref[4],
                                        int ref_stride, uint32_t res[4], int w,
                                        int h, int h_overflow) {
  uint32x4_t sum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0) };
  int h_limit = h > h_overflow ? h_overflow : h;

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
        const uint8x16_t s0 = vld1q_u8(src + j);
        sad16_neon(s0, vld1q_u8(ref[0] + ref_offset + j), &sum_lo[0]);
        sad16_neon(s0, vld1q_u8(ref[1] + ref_offset + j), &sum_lo[1]);
        sad16_neon(s0, vld1q_u8(ref[2] + ref_offset + j), &sum_lo[2]);
        sad16_neon(s0, vld1q_u8(ref[3] + ref_offset + j), &sum_lo[3]);

        const uint8x16_t s1 = vld1q_u8(src + j + 16);
        sad16_neon(s1, vld1q_u8(ref[0] + ref_offset + j + 16), &sum_hi[0]);
        sad16_neon(s1, vld1q_u8(ref[1] + ref_offset + j + 16), &sum_hi[1]);
        sad16_neon(s1, vld1q_u8(ref[2] + ref_offset + j + 16), &sum_hi[2]);
        sad16_neon(s1, vld1q_u8(ref[3] + ref_offset + j + 16), &sum_hi[3]);

        j += 32;
      } while (j < w);

      src += src_stride;
      ref_offset += ref_stride;
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
  } while (i < h);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}

static INLINE void sad128xhx4d_neon(const uint8_t *src, int src_stride,
                                    const uint8_t *const ref[4], int ref_stride,
                                    uint32_t res[4], int h) {
  sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, res, 128, h, 32);
}

static INLINE void sad64xhx4d_neon(const uint8_t *src, int src_stride,
                                   const uint8_t *const ref[4], int ref_stride,
                                   uint32_t res[4], int h) {
  sadwxhx4d_large_neon(src, src_stride, ref, ref_stride, res, 64, h, 64);
}

static INLINE void sad32xhx4d_neon(const uint8_t *src, int src_stride,
                                   const uint8_t *const ref[4], int ref_stride,
                                   uint32_t res[4], int h) {
  uint16x8_t sum_lo[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                           vdupq_n_u16(0) };
  uint16x8_t sum_hi[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                           vdupq_n_u16(0) };

  int ref_offset = 0;
  int i = h;
  do {
    const uint8x16_t s0 = vld1q_u8(src);
    sad16_neon(s0, vld1q_u8(ref[0] + ref_offset), &sum_lo[0]);
    sad16_neon(s0, vld1q_u8(ref[1] + ref_offset), &sum_lo[1]);
    sad16_neon(s0, vld1q_u8(ref[2] + ref_offset), &sum_lo[2]);
    sad16_neon(s0, vld1q_u8(ref[3] + ref_offset), &sum_lo[3]);

    const uint8x16_t s1 = vld1q_u8(src + 16);
    sad16_neon(s1, vld1q_u8(ref[0] + ref_offset + 16), &sum_hi[0]);
    sad16_neon(s1, vld1q_u8(ref[1] + ref_offset + 16), &sum_hi[1]);
    sad16_neon(s1, vld1q_u8(ref[2] + ref_offset + 16), &sum_hi[2]);
    sad16_neon(s1, vld1q_u8(ref[3] + ref_offset + 16), &sum_hi[3]);

    src += src_stride;
    ref_offset += ref_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_long_add_4d_u16x8(sum_lo, sum_hi));
}

static INLINE void sad16xhx4d_neon(const uint8_t *src, int src_stride,
                                   const uint8_t *const ref[4], int ref_stride,
                                   uint32_t res[4], int h) {
  uint16x8_t sum_u16[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                            vdupq_n_u16(0) };
  uint32x4_t sum_u32[4];

  int ref_offset = 0;
  int i = h;
  do {
    const uint8x16_t s = vld1q_u8(src);
    sad16_neon(s, vld1q_u8(ref[0] + ref_offset), &sum_u16[0]);
    sad16_neon(s, vld1q_u8(ref[1] + ref_offset), &sum_u16[1]);
    sad16_neon(s, vld1q_u8(ref[2] + ref_offset), &sum_u16[2]);
    sad16_neon(s, vld1q_u8(ref[3] + ref_offset), &sum_u16[3]);

    src += src_stride;
    ref_offset += ref_stride;
  } while (--i != 0);

  sum_u32[0] = vpaddlq_u16(sum_u16[0]);
  sum_u32[1] = vpaddlq_u16(sum_u16[1]);
  sum_u32[2] = vpaddlq_u16(sum_u16[2]);
  sum_u32[3] = vpaddlq_u16(sum_u16[3]);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum_u32));
}

#endif  // defined(__ARM_FEATURE_DOTPROD)

static INLINE void sad8xhx4d_neon(const uint8_t *src, int src_stride,
                                  const uint8_t *const ref[4], int ref_stride,
                                  uint32_t res[4], int h) {
  uint16x8_t sum[4];

  uint8x8_t s = vld1_u8(src);
  sum[0] = vabdl_u8(s, vld1_u8(ref[0]));
  sum[1] = vabdl_u8(s, vld1_u8(ref[1]));
  sum[2] = vabdl_u8(s, vld1_u8(ref[2]));
  sum[3] = vabdl_u8(s, vld1_u8(ref[3]));

  src += src_stride;
  int ref_offset = ref_stride;
  int i = h - 1;
  do {
    s = vld1_u8(src);
    sum[0] = vabal_u8(sum[0], s, vld1_u8(ref[0] + ref_offset));
    sum[1] = vabal_u8(sum[1], s, vld1_u8(ref[1] + ref_offset));
    sum[2] = vabal_u8(sum[2], s, vld1_u8(ref[2] + ref_offset));
    sum[3] = vabal_u8(sum[3], s, vld1_u8(ref[3] + ref_offset));

    src += src_stride;
    ref_offset += ref_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_add_4d_u16x8(sum));
}

static INLINE void sad4xhx4d_neon(const uint8_t *src, int src_stride,
                                  const uint8_t *const ref[4], int ref_stride,
                                  uint32_t res[4], int h) {
  uint16x8_t sum[4];

  uint8x8_t s = load_unaligned_u8(src, src_stride);
  uint8x8_t r0 = load_unaligned_u8(ref[0], ref_stride);
  uint8x8_t r1 = load_unaligned_u8(ref[1], ref_stride);
  uint8x8_t r2 = load_unaligned_u8(ref[2], ref_stride);
  uint8x8_t r3 = load_unaligned_u8(ref[3], ref_stride);

  sum[0] = vabdl_u8(s, r0);
  sum[1] = vabdl_u8(s, r1);
  sum[2] = vabdl_u8(s, r2);
  sum[3] = vabdl_u8(s, r3);

  src += 2 * src_stride;
  int ref_offset = 2 * ref_stride;
  int i = (h - 1) / 2;
  do {
    s = load_unaligned_u8(src, src_stride);
    r0 = load_unaligned_u8(ref[0] + ref_offset, ref_stride);
    r1 = load_unaligned_u8(ref[1] + ref_offset, ref_stride);
    r2 = load_unaligned_u8(ref[2] + ref_offset, ref_stride);
    r3 = load_unaligned_u8(ref[3] + ref_offset, ref_stride);

    sum[0] = vabal_u8(sum[0], s, r0);
    sum[1] = vabal_u8(sum[1], s, r1);
    sum[2] = vabal_u8(sum[2], s, r2);
    sum[3] = vabal_u8(sum[3], s, r3);

    src += 2 * src_stride;
    ref_offset += 2 * ref_stride;
  } while (--i != 0);

  vst1q_u32(res, horizontal_add_4d_u16x8(sum));
}

#define SAD_WXH_4D_NEON(w, h)                                                  \
  void aom_sad##w##x##h##x4d_neon(const uint8_t *src, int src_stride,          \
                                  const uint8_t *const ref[4], int ref_stride, \
                                  uint32_t res[4]) {                           \
    sad##w##xhx4d_neon(src, src_stride, ref, ref_stride, res, (h));            \
  }

SAD_WXH_4D_NEON(4, 4)
SAD_WXH_4D_NEON(4, 8)
SAD_WXH_4D_NEON(4, 16)
SAD_WXH_4D_NEON(4, 32)

SAD_WXH_4D_NEON(8, 4)
SAD_WXH_4D_NEON(8, 8)
SAD_WXH_4D_NEON(8, 16)
SAD_WXH_4D_NEON(8, 32)

SAD_WXH_4D_NEON(16, 4)
SAD_WXH_4D_NEON(16, 8)
SAD_WXH_4D_NEON(16, 16)
SAD_WXH_4D_NEON(16, 32)
SAD_WXH_4D_NEON(16, 64)

SAD_WXH_4D_NEON(32, 8)
SAD_WXH_4D_NEON(32, 16)
SAD_WXH_4D_NEON(32, 32)
SAD_WXH_4D_NEON(32, 64)

SAD_WXH_4D_NEON(64, 16)
SAD_WXH_4D_NEON(64, 32)
SAD_WXH_4D_NEON(64, 64)
SAD_WXH_4D_NEON(64, 128)

SAD_WXH_4D_NEON(128, 64)
SAD_WXH_4D_NEON(128, 128)

#undef SAD_WXH_4D_NEON

#define SAD_SKIP_WXH_4D_NEON(w, h)                                          \
  void aom_sad_skip_##w##x##h##x4d_neon(const uint8_t *src, int src_stride, \
                                        const uint8_t *const ref[4],        \
                                        int ref_stride, uint32_t res[4]) {  \
    sad##w##xhx4d_neon(src, 2 * src_stride, ref, 2 * ref_stride, res,       \
                       ((h) >> 1));                                         \
    res[0] <<= 1;                                                           \
    res[1] <<= 1;                                                           \
    res[2] <<= 1;                                                           \
    res[3] <<= 1;                                                           \
  }

SAD_SKIP_WXH_4D_NEON(4, 8)
SAD_SKIP_WXH_4D_NEON(4, 16)
SAD_SKIP_WXH_4D_NEON(4, 32)

SAD_SKIP_WXH_4D_NEON(8, 8)
SAD_SKIP_WXH_4D_NEON(8, 16)
SAD_SKIP_WXH_4D_NEON(8, 32)

SAD_SKIP_WXH_4D_NEON(16, 8)
SAD_SKIP_WXH_4D_NEON(16, 16)
SAD_SKIP_WXH_4D_NEON(16, 32)
SAD_SKIP_WXH_4D_NEON(16, 64)

SAD_SKIP_WXH_4D_NEON(32, 8)
SAD_SKIP_WXH_4D_NEON(32, 16)
SAD_SKIP_WXH_4D_NEON(32, 32)
SAD_SKIP_WXH_4D_NEON(32, 64)

SAD_SKIP_WXH_4D_NEON(64, 16)
SAD_SKIP_WXH_4D_NEON(64, 32)
SAD_SKIP_WXH_4D_NEON(64, 64)
SAD_SKIP_WXH_4D_NEON(64, 128)

SAD_SKIP_WXH_4D_NEON(128, 64)
SAD_SKIP_WXH_4D_NEON(128, 128)

#undef SAD_SKIP_WXH_4D_NEON
