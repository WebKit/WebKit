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
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

static INLINE void sad16_neon(uint8x16_t src, uint8x16_t ref,
                              uint32x4_t *const sad_sum) {
  uint8x16_t abs_diff = vabdq_u8(src, ref);
  *sad_sum = vdotq_u32(*sad_sum, abs_diff, vdupq_n_u8(1));
}

static INLINE void sadwxhx3d_large_neon_dotprod(const uint8_t *src,
                                                int src_stride,
                                                const uint8_t *const ref[4],
                                                int ref_stride, uint32_t res[4],
                                                int w, int h) {
  uint32x4_t sum_lo[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };
  uint32x4_t sum_hi[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };

  int ref_offset = 0;
  int i = h;
  do {
    int j = 0;
    do {
      const uint8x16_t s0 = vld1q_u8(src + j);
      sad16_neon(s0, vld1q_u8(ref[0] + ref_offset + j), &sum_lo[0]);
      sad16_neon(s0, vld1q_u8(ref[1] + ref_offset + j), &sum_lo[1]);
      sad16_neon(s0, vld1q_u8(ref[2] + ref_offset + j), &sum_lo[2]);

      const uint8x16_t s1 = vld1q_u8(src + j + 16);
      sad16_neon(s1, vld1q_u8(ref[0] + ref_offset + j + 16), &sum_hi[0]);
      sad16_neon(s1, vld1q_u8(ref[1] + ref_offset + j + 16), &sum_hi[1]);
      sad16_neon(s1, vld1q_u8(ref[2] + ref_offset + j + 16), &sum_hi[2]);

      j += 32;
    } while (j < w);

    src += src_stride;
    ref_offset += ref_stride;
  } while (--i != 0);

  res[0] = horizontal_add_u32x4(vaddq_u32(sum_lo[0], sum_hi[0]));
  res[1] = horizontal_add_u32x4(vaddq_u32(sum_lo[1], sum_hi[1]));
  res[2] = horizontal_add_u32x4(vaddq_u32(sum_lo[2], sum_hi[2]));
}

static INLINE void sad128xhx3d_neon_dotprod(const uint8_t *src, int src_stride,
                                            const uint8_t *const ref[4],
                                            int ref_stride, uint32_t res[4],
                                            int h) {
  sadwxhx3d_large_neon_dotprod(src, src_stride, ref, ref_stride, res, 128, h);
}

static INLINE void sad64xhx3d_neon_dotprod(const uint8_t *src, int src_stride,
                                           const uint8_t *const ref[4],
                                           int ref_stride, uint32_t res[4],
                                           int h) {
  sadwxhx3d_large_neon_dotprod(src, src_stride, ref, ref_stride, res, 64, h);
}

static INLINE void sad32xhx3d_neon_dotprod(const uint8_t *src, int src_stride,
                                           const uint8_t *const ref[4],
                                           int ref_stride, uint32_t res[4],
                                           int h) {
  sadwxhx3d_large_neon_dotprod(src, src_stride, ref, ref_stride, res, 32, h);
}

static INLINE void sad16xhx3d_neon_dotprod(const uint8_t *src, int src_stride,
                                           const uint8_t *const ref[4],
                                           int ref_stride, uint32_t res[4],
                                           int h) {
  uint32x4_t sum[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };

  int ref_offset = 0;
  int i = h;
  do {
    const uint8x16_t s = vld1q_u8(src);
    sad16_neon(s, vld1q_u8(ref[0] + ref_offset), &sum[0]);
    sad16_neon(s, vld1q_u8(ref[1] + ref_offset), &sum[1]);
    sad16_neon(s, vld1q_u8(ref[2] + ref_offset), &sum[2]);

    src += src_stride;
    ref_offset += ref_stride;
  } while (--i != 0);

  res[0] = horizontal_add_u32x4(sum[0]);
  res[1] = horizontal_add_u32x4(sum[1]);
  res[2] = horizontal_add_u32x4(sum[2]);
}

#define SAD_WXH_3D_NEON_DOTPROD(w, h)                                         \
  void aom_sad##w##x##h##x3d_neon_dotprod(const uint8_t *src, int src_stride, \
                                          const uint8_t *const ref[4],        \
                                          int ref_stride, uint32_t res[4]) {  \
    sad##w##xhx3d_neon_dotprod(src, src_stride, ref, ref_stride, res, (h));   \
  }

SAD_WXH_3D_NEON_DOTPROD(16, 8)
SAD_WXH_3D_NEON_DOTPROD(16, 16)
SAD_WXH_3D_NEON_DOTPROD(16, 32)

SAD_WXH_3D_NEON_DOTPROD(32, 16)
SAD_WXH_3D_NEON_DOTPROD(32, 32)
SAD_WXH_3D_NEON_DOTPROD(32, 64)

SAD_WXH_3D_NEON_DOTPROD(64, 32)
SAD_WXH_3D_NEON_DOTPROD(64, 64)
SAD_WXH_3D_NEON_DOTPROD(64, 128)

SAD_WXH_3D_NEON_DOTPROD(128, 64)
SAD_WXH_3D_NEON_DOTPROD(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_WXH_3D_NEON_DOTPROD(16, 4)
SAD_WXH_3D_NEON_DOTPROD(16, 64)
SAD_WXH_3D_NEON_DOTPROD(32, 8)
SAD_WXH_3D_NEON_DOTPROD(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_WXH_3D_NEON_DOTPROD

static INLINE void sadwxhx4d_large_neon_dotprod(const uint8_t *src,
                                                int src_stride,
                                                const uint8_t *const ref[4],
                                                int ref_stride, uint32_t res[4],
                                                int w, int h) {
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

static INLINE void sad128xhx4d_neon_dotprod(const uint8_t *src, int src_stride,
                                            const uint8_t *const ref[4],
                                            int ref_stride, uint32_t res[4],
                                            int h) {
  sadwxhx4d_large_neon_dotprod(src, src_stride, ref, ref_stride, res, 128, h);
}

static INLINE void sad64xhx4d_neon_dotprod(const uint8_t *src, int src_stride,
                                           const uint8_t *const ref[4],
                                           int ref_stride, uint32_t res[4],
                                           int h) {
  sadwxhx4d_large_neon_dotprod(src, src_stride, ref, ref_stride, res, 64, h);
}

static INLINE void sad32xhx4d_neon_dotprod(const uint8_t *src, int src_stride,
                                           const uint8_t *const ref[4],
                                           int ref_stride, uint32_t res[4],
                                           int h) {
  sadwxhx4d_large_neon_dotprod(src, src_stride, ref, ref_stride, res, 32, h);
}

static INLINE void sad16xhx4d_neon_dotprod(const uint8_t *src, int src_stride,
                                           const uint8_t *const ref[4],
                                           int ref_stride, uint32_t res[4],
                                           int h) {
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

#define SAD_WXH_4D_NEON_DOTPROD(w, h)                                         \
  void aom_sad##w##x##h##x4d_neon_dotprod(const uint8_t *src, int src_stride, \
                                          const uint8_t *const ref[4],        \
                                          int ref_stride, uint32_t res[4]) {  \
    sad##w##xhx4d_neon_dotprod(src, src_stride, ref, ref_stride, res, (h));   \
  }

SAD_WXH_4D_NEON_DOTPROD(16, 8)
SAD_WXH_4D_NEON_DOTPROD(16, 16)
SAD_WXH_4D_NEON_DOTPROD(16, 32)

SAD_WXH_4D_NEON_DOTPROD(32, 16)
SAD_WXH_4D_NEON_DOTPROD(32, 32)
SAD_WXH_4D_NEON_DOTPROD(32, 64)

SAD_WXH_4D_NEON_DOTPROD(64, 32)
SAD_WXH_4D_NEON_DOTPROD(64, 64)
SAD_WXH_4D_NEON_DOTPROD(64, 128)

SAD_WXH_4D_NEON_DOTPROD(128, 64)
SAD_WXH_4D_NEON_DOTPROD(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_WXH_4D_NEON_DOTPROD(16, 4)
SAD_WXH_4D_NEON_DOTPROD(16, 64)
SAD_WXH_4D_NEON_DOTPROD(32, 8)
SAD_WXH_4D_NEON_DOTPROD(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_WXH_4D_NEON_DOTPROD

#define SAD_SKIP_WXH_4D_NEON_DOTPROD(w, h)                                    \
  void aom_sad_skip_##w##x##h##x4d_neon_dotprod(                              \
      const uint8_t *src, int src_stride, const uint8_t *const ref[4],        \
      int ref_stride, uint32_t res[4]) {                                      \
    sad##w##xhx4d_neon_dotprod(src, 2 * src_stride, ref, 2 * ref_stride, res, \
                               ((h) >> 1));                                   \
    res[0] <<= 1;                                                             \
    res[1] <<= 1;                                                             \
    res[2] <<= 1;                                                             \
    res[3] <<= 1;                                                             \
  }

SAD_SKIP_WXH_4D_NEON_DOTPROD(16, 8)
SAD_SKIP_WXH_4D_NEON_DOTPROD(16, 16)
SAD_SKIP_WXH_4D_NEON_DOTPROD(16, 32)

SAD_SKIP_WXH_4D_NEON_DOTPROD(32, 16)
SAD_SKIP_WXH_4D_NEON_DOTPROD(32, 32)
SAD_SKIP_WXH_4D_NEON_DOTPROD(32, 64)

SAD_SKIP_WXH_4D_NEON_DOTPROD(64, 32)
SAD_SKIP_WXH_4D_NEON_DOTPROD(64, 64)
SAD_SKIP_WXH_4D_NEON_DOTPROD(64, 128)

SAD_SKIP_WXH_4D_NEON_DOTPROD(128, 64)
SAD_SKIP_WXH_4D_NEON_DOTPROD(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_SKIP_WXH_4D_NEON_DOTPROD(16, 4)
SAD_SKIP_WXH_4D_NEON_DOTPROD(16, 64)
SAD_SKIP_WXH_4D_NEON_DOTPROD(32, 8)
SAD_SKIP_WXH_4D_NEON_DOTPROD(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#undef SAD_SKIP_WXH_4D_NEON_DOTPROD
