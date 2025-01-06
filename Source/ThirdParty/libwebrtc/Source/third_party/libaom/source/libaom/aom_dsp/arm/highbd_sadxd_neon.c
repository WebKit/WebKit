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

static inline void highbd_sad4xhx4d_small_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *const ref_ptr[4],
                                               int ref_stride, uint32_t res[4],
                                               int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);
  const uint16_t *ref16_ptr3 = CONVERT_TO_SHORTPTR(ref_ptr[3]);

  uint32x4_t sum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0) };

  int i = 0;
  do {
    uint16x4_t s = vld1_u16(src16_ptr + i * src_stride);
    uint16x4_t r0 = vld1_u16(ref16_ptr0 + i * ref_stride);
    uint16x4_t r1 = vld1_u16(ref16_ptr1 + i * ref_stride);
    uint16x4_t r2 = vld1_u16(ref16_ptr2 + i * ref_stride);
    uint16x4_t r3 = vld1_u16(ref16_ptr3 + i * ref_stride);

    sum[0] = vabal_u16(sum[0], s, r0);
    sum[1] = vabal_u16(sum[1], s, r1);
    sum[2] = vabal_u16(sum[2], s, r2);
    sum[3] = vabal_u16(sum[3], s, r3);

  } while (++i < h);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}

static inline void highbd_sad8xhx4d_small_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *const ref_ptr[4],
                                               int ref_stride, uint32_t res[4],
                                               int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);
  const uint16_t *ref16_ptr3 = CONVERT_TO_SHORTPTR(ref_ptr[3]);

  uint16x8_t sum[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0),
                        vdupq_n_u16(0) };
  uint32x4_t sum_u32[4];

  int i = 0;
  do {
    uint16x8_t s = vld1q_u16(src16_ptr + i * src_stride);

    sum[0] = vabaq_u16(sum[0], s, vld1q_u16(ref16_ptr0 + i * ref_stride));
    sum[1] = vabaq_u16(sum[1], s, vld1q_u16(ref16_ptr1 + i * ref_stride));
    sum[2] = vabaq_u16(sum[2], s, vld1q_u16(ref16_ptr2 + i * ref_stride));
    sum[3] = vabaq_u16(sum[3], s, vld1q_u16(ref16_ptr3 + i * ref_stride));

  } while (++i < h);

  sum_u32[0] = vpaddlq_u16(sum[0]);
  sum_u32[1] = vpaddlq_u16(sum[1]);
  sum_u32[2] = vpaddlq_u16(sum[2]);
  sum_u32[3] = vpaddlq_u16(sum[3]);
  vst1q_u32(res, horizontal_add_4d_u32x4(sum_u32));
}

static inline void sad8_neon(uint16x8_t src, uint16x8_t ref,
                             uint32x4_t *const sad_sum) {
  uint16x8_t abs_diff = vabdq_u16(src, ref);
  *sad_sum = vpadalq_u16(*sad_sum, abs_diff);
}

#if !CONFIG_REALTIME_ONLY
static inline void highbd_sad8xhx4d_large_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *const ref_ptr[4],
                                               int ref_stride, uint32_t res[4],
                                               int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);
  const uint16_t *ref16_ptr3 = CONVERT_TO_SHORTPTR(ref_ptr[3]);

  uint32x4_t sum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0) };

  int i = 0;
  do {
    uint16x8_t s = vld1q_u16(src16_ptr + i * src_stride);
    sad8_neon(s, vld1q_u16(ref16_ptr0 + i * ref_stride), &sum[0]);
    sad8_neon(s, vld1q_u16(ref16_ptr1 + i * ref_stride), &sum[1]);
    sad8_neon(s, vld1q_u16(ref16_ptr2 + i * ref_stride), &sum[2]);
    sad8_neon(s, vld1q_u16(ref16_ptr3 + i * ref_stride), &sum[3]);

  } while (++i < h);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}
#endif  // !CONFIG_REALTIME_ONLY

static inline void highbd_sad16xhx4d_large_neon(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *const ref_ptr[4],
                                                int ref_stride, uint32_t res[4],
                                                int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);
  const uint16_t *ref16_ptr3 = CONVERT_TO_SHORTPTR(ref_ptr[3]);

  uint32x4_t sum_lo[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                           vdupq_n_u32(0) };
  uint32x4_t sum_hi[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                           vdupq_n_u32(0) };
  uint32x4_t sum[4];

  int i = 0;
  do {
    uint16x8_t s0 = vld1q_u16(src16_ptr + i * src_stride);
    sad8_neon(s0, vld1q_u16(ref16_ptr0 + i * ref_stride), &sum_lo[0]);
    sad8_neon(s0, vld1q_u16(ref16_ptr1 + i * ref_stride), &sum_lo[1]);
    sad8_neon(s0, vld1q_u16(ref16_ptr2 + i * ref_stride), &sum_lo[2]);
    sad8_neon(s0, vld1q_u16(ref16_ptr3 + i * ref_stride), &sum_lo[3]);

    uint16x8_t s1 = vld1q_u16(src16_ptr + i * src_stride + 8);
    sad8_neon(s1, vld1q_u16(ref16_ptr0 + i * ref_stride + 8), &sum_hi[0]);
    sad8_neon(s1, vld1q_u16(ref16_ptr1 + i * ref_stride + 8), &sum_hi[1]);
    sad8_neon(s1, vld1q_u16(ref16_ptr2 + i * ref_stride + 8), &sum_hi[2]);
    sad8_neon(s1, vld1q_u16(ref16_ptr3 + i * ref_stride + 8), &sum_hi[3]);

  } while (++i < h);

  sum[0] = vaddq_u32(sum_lo[0], sum_hi[0]);
  sum[1] = vaddq_u32(sum_lo[1], sum_hi[1]);
  sum[2] = vaddq_u32(sum_lo[2], sum_hi[2]);
  sum[3] = vaddq_u32(sum_lo[3], sum_hi[3]);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}

static inline void highbd_sadwxhx4d_large_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *const ref_ptr[4],
                                               int ref_stride, uint32_t res[4],
                                               int w, int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);
  const uint16_t *ref16_ptr3 = CONVERT_TO_SHORTPTR(ref_ptr[3]);

  uint32x4_t sum_lo[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                           vdupq_n_u32(0) };
  uint32x4_t sum_hi[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                           vdupq_n_u32(0) };
  uint32x4_t sum[4];

  int i = 0;
  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src16_ptr + i * src_stride + j);
      sad8_neon(s0, vld1q_u16(ref16_ptr0 + i * ref_stride + j), &sum_lo[0]);
      sad8_neon(s0, vld1q_u16(ref16_ptr1 + i * ref_stride + j), &sum_lo[1]);
      sad8_neon(s0, vld1q_u16(ref16_ptr2 + i * ref_stride + j), &sum_lo[2]);
      sad8_neon(s0, vld1q_u16(ref16_ptr3 + i * ref_stride + j), &sum_lo[3]);

      uint16x8_t s1 = vld1q_u16(src16_ptr + i * src_stride + j + 8);
      sad8_neon(s1, vld1q_u16(ref16_ptr0 + i * ref_stride + j + 8), &sum_hi[0]);
      sad8_neon(s1, vld1q_u16(ref16_ptr1 + i * ref_stride + j + 8), &sum_hi[1]);
      sad8_neon(s1, vld1q_u16(ref16_ptr2 + i * ref_stride + j + 8), &sum_hi[2]);
      sad8_neon(s1, vld1q_u16(ref16_ptr3 + i * ref_stride + j + 8), &sum_hi[3]);

      uint16x8_t s2 = vld1q_u16(src16_ptr + i * src_stride + j + 16);
      sad8_neon(s2, vld1q_u16(ref16_ptr0 + i * ref_stride + j + 16),
                &sum_lo[0]);
      sad8_neon(s2, vld1q_u16(ref16_ptr1 + i * ref_stride + j + 16),
                &sum_lo[1]);
      sad8_neon(s2, vld1q_u16(ref16_ptr2 + i * ref_stride + j + 16),
                &sum_lo[2]);
      sad8_neon(s2, vld1q_u16(ref16_ptr3 + i * ref_stride + j + 16),
                &sum_lo[3]);

      uint16x8_t s3 = vld1q_u16(src16_ptr + i * src_stride + j + 24);
      sad8_neon(s3, vld1q_u16(ref16_ptr0 + i * ref_stride + j + 24),
                &sum_hi[0]);
      sad8_neon(s3, vld1q_u16(ref16_ptr1 + i * ref_stride + j + 24),
                &sum_hi[1]);
      sad8_neon(s3, vld1q_u16(ref16_ptr2 + i * ref_stride + j + 24),
                &sum_hi[2]);
      sad8_neon(s3, vld1q_u16(ref16_ptr3 + i * ref_stride + j + 24),
                &sum_hi[3]);

      j += 32;
    } while (j < w);

  } while (++i < h);

  sum[0] = vaddq_u32(sum_lo[0], sum_hi[0]);
  sum[1] = vaddq_u32(sum_lo[1], sum_hi[1]);
  sum[2] = vaddq_u32(sum_lo[2], sum_hi[2]);
  sum[3] = vaddq_u32(sum_lo[3], sum_hi[3]);

  vst1q_u32(res, horizontal_add_4d_u32x4(sum));
}

static inline void highbd_sad128xhx4d_large_neon(
    const uint8_t *src_ptr, int src_stride, const uint8_t *const ref_ptr[4],
    int ref_stride, uint32_t res[4], int h) {
  highbd_sadwxhx4d_large_neon(src_ptr, src_stride, ref_ptr, ref_stride, res,
                              128, h);
}

static inline void highbd_sad64xhx4d_large_neon(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *const ref_ptr[4],
                                                int ref_stride, uint32_t res[4],
                                                int h) {
  highbd_sadwxhx4d_large_neon(src_ptr, src_stride, ref_ptr, ref_stride, res, 64,
                              h);
}

static inline void highbd_sad32xhx4d_large_neon(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *const ref_ptr[4],
                                                int ref_stride, uint32_t res[4],
                                                int h) {
  highbd_sadwxhx4d_large_neon(src_ptr, src_stride, ref_ptr, ref_stride, res, 32,
                              h);
}

#define HBD_SAD_WXH_4D_SMALL_NEON(w, h)                                      \
  void aom_highbd_sad##w##x##h##x4d_neon(                                    \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4], \
      int ref_stride, uint32_t sad_array[4]) {                               \
    highbd_sad##w##xhx4d_small_neon(src, src_stride, ref_array, ref_stride,  \
                                    sad_array, (h));                         \
  }

#define HBD_SAD_WXH_4D_LARGE_NEON(w, h)                                      \
  void aom_highbd_sad##w##x##h##x4d_neon(                                    \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4], \
      int ref_stride, uint32_t sad_array[4]) {                               \
    highbd_sad##w##xhx4d_large_neon(src, src_stride, ref_array, ref_stride,  \
                                    sad_array, (h));                         \
  }

HBD_SAD_WXH_4D_SMALL_NEON(4, 4)
HBD_SAD_WXH_4D_SMALL_NEON(4, 8)

HBD_SAD_WXH_4D_SMALL_NEON(8, 4)
HBD_SAD_WXH_4D_SMALL_NEON(8, 8)
HBD_SAD_WXH_4D_SMALL_NEON(8, 16)

HBD_SAD_WXH_4D_LARGE_NEON(16, 8)
HBD_SAD_WXH_4D_LARGE_NEON(16, 16)
HBD_SAD_WXH_4D_LARGE_NEON(16, 32)

HBD_SAD_WXH_4D_LARGE_NEON(32, 16)
HBD_SAD_WXH_4D_LARGE_NEON(32, 32)
HBD_SAD_WXH_4D_LARGE_NEON(32, 64)

HBD_SAD_WXH_4D_LARGE_NEON(64, 32)
HBD_SAD_WXH_4D_LARGE_NEON(64, 64)
HBD_SAD_WXH_4D_LARGE_NEON(64, 128)

HBD_SAD_WXH_4D_LARGE_NEON(128, 64)
HBD_SAD_WXH_4D_LARGE_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SAD_WXH_4D_SMALL_NEON(4, 16)

HBD_SAD_WXH_4D_LARGE_NEON(8, 32)

HBD_SAD_WXH_4D_LARGE_NEON(16, 4)
HBD_SAD_WXH_4D_LARGE_NEON(16, 64)

HBD_SAD_WXH_4D_LARGE_NEON(32, 8)

HBD_SAD_WXH_4D_LARGE_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#define HBD_SAD_SKIP_WXH_4D_SMALL_NEON(w, h)                                 \
  void aom_highbd_sad_skip_##w##x##h##x4d_neon(                              \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4], \
      int ref_stride, uint32_t sad_array[4]) {                               \
    highbd_sad##w##xhx4d_small_neon(src, 2 * src_stride, ref_array,          \
                                    2 * ref_stride, sad_array, ((h) >> 1));  \
    sad_array[0] <<= 1;                                                      \
    sad_array[1] <<= 1;                                                      \
    sad_array[2] <<= 1;                                                      \
    sad_array[3] <<= 1;                                                      \
  }

#define HBD_SAD_SKIP_WXH_4D_LARGE_NEON(w, h)                                 \
  void aom_highbd_sad_skip_##w##x##h##x4d_neon(                              \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4], \
      int ref_stride, uint32_t sad_array[4]) {                               \
    highbd_sad##w##xhx4d_large_neon(src, 2 * src_stride, ref_array,          \
                                    2 * ref_stride, sad_array, ((h) >> 1));  \
    sad_array[0] <<= 1;                                                      \
    sad_array[1] <<= 1;                                                      \
    sad_array[2] <<= 1;                                                      \
    sad_array[3] <<= 1;                                                      \
  }

HBD_SAD_SKIP_WXH_4D_SMALL_NEON(8, 16)

HBD_SAD_SKIP_WXH_4D_LARGE_NEON(16, 16)
HBD_SAD_SKIP_WXH_4D_LARGE_NEON(16, 32)

HBD_SAD_SKIP_WXH_4D_LARGE_NEON(32, 16)
HBD_SAD_SKIP_WXH_4D_LARGE_NEON(32, 32)
HBD_SAD_SKIP_WXH_4D_LARGE_NEON(32, 64)

HBD_SAD_SKIP_WXH_4D_LARGE_NEON(64, 32)
HBD_SAD_SKIP_WXH_4D_LARGE_NEON(64, 64)
HBD_SAD_SKIP_WXH_4D_LARGE_NEON(64, 128)

HBD_SAD_SKIP_WXH_4D_LARGE_NEON(128, 64)
HBD_SAD_SKIP_WXH_4D_LARGE_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SAD_SKIP_WXH_4D_SMALL_NEON(4, 16)

HBD_SAD_SKIP_WXH_4D_SMALL_NEON(8, 32)

HBD_SAD_SKIP_WXH_4D_LARGE_NEON(16, 64)

HBD_SAD_SKIP_WXH_4D_LARGE_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

static inline void highbd_sad4xhx3d_small_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *const ref_ptr[4],
                                               int ref_stride, uint32_t res[4],
                                               int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);

  uint32x4_t sum[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = 0;
  do {
    uint16x4_t s = vld1_u16(src16_ptr + i * src_stride);
    uint16x4_t r0 = vld1_u16(ref16_ptr0 + i * ref_stride);
    uint16x4_t r1 = vld1_u16(ref16_ptr1 + i * ref_stride);
    uint16x4_t r2 = vld1_u16(ref16_ptr2 + i * ref_stride);

    sum[0] = vabal_u16(sum[0], s, r0);
    sum[1] = vabal_u16(sum[1], s, r1);
    sum[2] = vabal_u16(sum[2], s, r2);

  } while (++i < h);

  res[0] = horizontal_add_u32x4(sum[0]);
  res[1] = horizontal_add_u32x4(sum[1]);
  res[2] = horizontal_add_u32x4(sum[2]);
}

static inline void highbd_sad8xhx3d_small_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *const ref_ptr[4],
                                               int ref_stride, uint32_t res[4],
                                               int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);

  uint16x8_t sum[3] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0) };

  int i = 0;
  do {
    uint16x8_t s = vld1q_u16(src16_ptr + i * src_stride);

    sum[0] = vabaq_u16(sum[0], s, vld1q_u16(ref16_ptr0 + i * ref_stride));
    sum[1] = vabaq_u16(sum[1], s, vld1q_u16(ref16_ptr1 + i * ref_stride));
    sum[2] = vabaq_u16(sum[2], s, vld1q_u16(ref16_ptr2 + i * ref_stride));

  } while (++i < h);

  res[0] = horizontal_add_u32x4(vpaddlq_u16(sum[0]));
  res[1] = horizontal_add_u32x4(vpaddlq_u16(sum[1]));
  res[2] = horizontal_add_u32x4(vpaddlq_u16(sum[2]));
}

#if !CONFIG_REALTIME_ONLY
static inline void highbd_sad8xhx3d_large_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *const ref_ptr[4],
                                               int ref_stride, uint32_t res[4],
                                               int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);

  uint32x4_t sum[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = 0;
  do {
    uint16x8_t s = vld1q_u16(src16_ptr + i * src_stride);
    uint16x8_t r0 = vld1q_u16(ref16_ptr0 + i * ref_stride);
    uint16x8_t r1 = vld1q_u16(ref16_ptr1 + i * ref_stride);
    uint16x8_t r2 = vld1q_u16(ref16_ptr2 + i * ref_stride);

    sad8_neon(s, r0, &sum[0]);
    sad8_neon(s, r1, &sum[1]);
    sad8_neon(s, r2, &sum[2]);

  } while (++i < h);

  res[0] = horizontal_add_u32x4(sum[0]);
  res[1] = horizontal_add_u32x4(sum[1]);
  res[2] = horizontal_add_u32x4(sum[2]);
}
#endif  // !CONFIG_REALTIME_ONLY

static inline void highbd_sad16xhx3d_large_neon(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *const ref_ptr[4],
                                                int ref_stride, uint32_t res[4],
                                                int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);

  uint32x4_t sum_lo[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };
  uint32x4_t sum_hi[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = 0;
  do {
    uint16x8_t s0 = vld1q_u16(src16_ptr + i * src_stride);
    sad8_neon(s0, vld1q_u16(ref16_ptr0 + i * ref_stride), &sum_lo[0]);
    sad8_neon(s0, vld1q_u16(ref16_ptr1 + i * ref_stride), &sum_lo[1]);
    sad8_neon(s0, vld1q_u16(ref16_ptr2 + i * ref_stride), &sum_lo[2]);

    uint16x8_t s1 = vld1q_u16(src16_ptr + i * src_stride + 8);
    sad8_neon(s1, vld1q_u16(ref16_ptr0 + i * ref_stride + 8), &sum_hi[0]);
    sad8_neon(s1, vld1q_u16(ref16_ptr1 + i * ref_stride + 8), &sum_hi[1]);
    sad8_neon(s1, vld1q_u16(ref16_ptr2 + i * ref_stride + 8), &sum_hi[2]);

  } while (++i < h);

  res[0] = horizontal_add_u32x4(vaddq_u32(sum_lo[0], sum_hi[0]));
  res[1] = horizontal_add_u32x4(vaddq_u32(sum_lo[1], sum_hi[1]));
  res[2] = horizontal_add_u32x4(vaddq_u32(sum_lo[2], sum_hi[2]));
}

static inline void highbd_sadwxhx3d_large_neon(const uint8_t *src_ptr,
                                               int src_stride,
                                               const uint8_t *const ref_ptr[4],
                                               int ref_stride, uint32_t res[4],
                                               int w, int h) {
  const uint16_t *src16_ptr = CONVERT_TO_SHORTPTR(src_ptr);
  const uint16_t *ref16_ptr0 = CONVERT_TO_SHORTPTR(ref_ptr[0]);
  const uint16_t *ref16_ptr1 = CONVERT_TO_SHORTPTR(ref_ptr[1]);
  const uint16_t *ref16_ptr2 = CONVERT_TO_SHORTPTR(ref_ptr[2]);

  uint32x4_t sum_lo[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };
  uint32x4_t sum_hi[3] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };
  uint32x4_t sum[3];

  int i = 0;
  do {
    int j = 0;
    do {
      uint16x8_t s0 = vld1q_u16(src16_ptr + i * src_stride + j);
      sad8_neon(s0, vld1q_u16(ref16_ptr0 + i * ref_stride + j), &sum_lo[0]);
      sad8_neon(s0, vld1q_u16(ref16_ptr1 + i * ref_stride + j), &sum_lo[1]);
      sad8_neon(s0, vld1q_u16(ref16_ptr2 + i * ref_stride + j), &sum_lo[2]);

      uint16x8_t s1 = vld1q_u16(src16_ptr + i * src_stride + j + 8);
      sad8_neon(s1, vld1q_u16(ref16_ptr0 + i * ref_stride + j + 8), &sum_hi[0]);
      sad8_neon(s1, vld1q_u16(ref16_ptr1 + i * ref_stride + j + 8), &sum_hi[1]);
      sad8_neon(s1, vld1q_u16(ref16_ptr2 + i * ref_stride + j + 8), &sum_hi[2]);

      uint16x8_t s2 = vld1q_u16(src16_ptr + i * src_stride + j + 16);
      sad8_neon(s2, vld1q_u16(ref16_ptr0 + i * ref_stride + j + 16),
                &sum_lo[0]);
      sad8_neon(s2, vld1q_u16(ref16_ptr1 + i * ref_stride + j + 16),
                &sum_lo[1]);
      sad8_neon(s2, vld1q_u16(ref16_ptr2 + i * ref_stride + j + 16),
                &sum_lo[2]);

      uint16x8_t s3 = vld1q_u16(src16_ptr + i * src_stride + j + 24);
      sad8_neon(s3, vld1q_u16(ref16_ptr0 + i * ref_stride + j + 24),
                &sum_hi[0]);
      sad8_neon(s3, vld1q_u16(ref16_ptr1 + i * ref_stride + j + 24),
                &sum_hi[1]);
      sad8_neon(s3, vld1q_u16(ref16_ptr2 + i * ref_stride + j + 24),
                &sum_hi[2]);

      j += 32;
    } while (j < w);

  } while (++i < h);

  sum[0] = vaddq_u32(sum_lo[0], sum_hi[0]);
  sum[1] = vaddq_u32(sum_lo[1], sum_hi[1]);
  sum[2] = vaddq_u32(sum_lo[2], sum_hi[2]);

  res[0] = horizontal_add_u32x4(sum[0]);
  res[1] = horizontal_add_u32x4(sum[1]);
  res[2] = horizontal_add_u32x4(sum[2]);
}

static inline void highbd_sad128xhx3d_large_neon(
    const uint8_t *src_ptr, int src_stride, const uint8_t *const ref_ptr[4],
    int ref_stride, uint32_t res[4], int h) {
  highbd_sadwxhx3d_large_neon(src_ptr, src_stride, ref_ptr, ref_stride, res,
                              128, h);
}

static inline void highbd_sad64xhx3d_large_neon(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *const ref_ptr[4],
                                                int ref_stride, uint32_t res[4],
                                                int h) {
  highbd_sadwxhx3d_large_neon(src_ptr, src_stride, ref_ptr, ref_stride, res, 64,
                              h);
}

static inline void highbd_sad32xhx3d_large_neon(const uint8_t *src_ptr,
                                                int src_stride,
                                                const uint8_t *const ref_ptr[4],
                                                int ref_stride, uint32_t res[4],
                                                int h) {
  highbd_sadwxhx3d_large_neon(src_ptr, src_stride, ref_ptr, ref_stride, res, 32,
                              h);
}

#define HBD_SAD_WXH_3D_SMALL_NEON(w, h)                                      \
  void aom_highbd_sad##w##x##h##x3d_neon(                                    \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4], \
      int ref_stride, uint32_t sad_array[4]) {                               \
    highbd_sad##w##xhx3d_small_neon(src, src_stride, ref_array, ref_stride,  \
                                    sad_array, (h));                         \
  }

#define HBD_SAD_WXH_3D_LARGE_NEON(w, h)                                      \
  void aom_highbd_sad##w##x##h##x3d_neon(                                    \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[4], \
      int ref_stride, uint32_t sad_array[4]) {                               \
    highbd_sad##w##xhx3d_large_neon(src, src_stride, ref_array, ref_stride,  \
                                    sad_array, (h));                         \
  }

HBD_SAD_WXH_3D_SMALL_NEON(4, 4)
HBD_SAD_WXH_3D_SMALL_NEON(4, 8)

HBD_SAD_WXH_3D_SMALL_NEON(8, 4)
HBD_SAD_WXH_3D_SMALL_NEON(8, 8)
HBD_SAD_WXH_3D_SMALL_NEON(8, 16)

HBD_SAD_WXH_3D_LARGE_NEON(16, 8)
HBD_SAD_WXH_3D_LARGE_NEON(16, 16)
HBD_SAD_WXH_3D_LARGE_NEON(16, 32)

HBD_SAD_WXH_3D_LARGE_NEON(32, 16)
HBD_SAD_WXH_3D_LARGE_NEON(32, 32)
HBD_SAD_WXH_3D_LARGE_NEON(32, 64)

HBD_SAD_WXH_3D_LARGE_NEON(64, 32)
HBD_SAD_WXH_3D_LARGE_NEON(64, 64)
HBD_SAD_WXH_3D_LARGE_NEON(64, 128)

HBD_SAD_WXH_3D_LARGE_NEON(128, 64)
HBD_SAD_WXH_3D_LARGE_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HBD_SAD_WXH_3D_SMALL_NEON(4, 16)

HBD_SAD_WXH_3D_LARGE_NEON(8, 32)

HBD_SAD_WXH_3D_LARGE_NEON(16, 4)
HBD_SAD_WXH_3D_LARGE_NEON(16, 64)

HBD_SAD_WXH_3D_LARGE_NEON(32, 8)

HBD_SAD_WXH_3D_LARGE_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY
