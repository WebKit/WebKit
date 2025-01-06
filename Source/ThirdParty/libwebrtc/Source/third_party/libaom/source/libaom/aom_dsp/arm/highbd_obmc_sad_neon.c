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
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"

static inline void highbd_obmc_sad_8x1_s16_neon(uint16x8_t ref,
                                                const int32_t *mask,
                                                const int32_t *wsrc,
                                                uint32x4_t *sum) {
  int16x8_t ref_s16 = vreinterpretq_s16_u16(ref);

  int32x4_t wsrc_lo = vld1q_s32(wsrc);
  int32x4_t wsrc_hi = vld1q_s32(wsrc + 4);

  int32x4_t mask_lo = vld1q_s32(mask);
  int32x4_t mask_hi = vld1q_s32(mask + 4);

  int16x8_t mask_s16 = vcombine_s16(vmovn_s32(mask_lo), vmovn_s32(mask_hi));

  int32x4_t pre_lo = vmull_s16(vget_low_s16(ref_s16), vget_low_s16(mask_s16));
  int32x4_t pre_hi = vmull_s16(vget_high_s16(ref_s16), vget_high_s16(mask_s16));

  uint32x4_t abs_lo = vreinterpretq_u32_s32(vabdq_s32(wsrc_lo, pre_lo));
  uint32x4_t abs_hi = vreinterpretq_u32_s32(vabdq_s32(wsrc_hi, pre_hi));

  *sum = vrsraq_n_u32(*sum, abs_lo, 12);
  *sum = vrsraq_n_u32(*sum, abs_hi, 12);
}

static inline unsigned int highbd_obmc_sad_4xh_neon(const uint8_t *ref,
                                                    int ref_stride,
                                                    const int32_t *wsrc,
                                                    const int32_t *mask,
                                                    int height) {
  const uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(ref);
  uint32x4_t sum = vdupq_n_u32(0);

  int h = height / 2;
  do {
    uint16x8_t r = load_unaligned_u16_4x2(ref_ptr, ref_stride);

    highbd_obmc_sad_8x1_s16_neon(r, mask, wsrc, &sum);

    ref_ptr += 2 * ref_stride;
    wsrc += 8;
    mask += 8;
  } while (--h != 0);

  return horizontal_add_u32x4(sum);
}

static inline unsigned int highbd_obmc_sad_8xh_neon(const uint8_t *ref,
                                                    int ref_stride,
                                                    const int32_t *wsrc,
                                                    const int32_t *mask,
                                                    int height) {
  const uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(ref);
  uint32x4_t sum = vdupq_n_u32(0);

  do {
    uint16x8_t r = vld1q_u16(ref_ptr);

    highbd_obmc_sad_8x1_s16_neon(r, mask, wsrc, &sum);

    ref_ptr += ref_stride;
    wsrc += 8;
    mask += 8;
  } while (--height != 0);

  return horizontal_add_u32x4(sum);
}

static inline unsigned int highbd_obmc_sad_large_neon(const uint8_t *ref,
                                                      int ref_stride,
                                                      const int32_t *wsrc,
                                                      const int32_t *mask,
                                                      int width, int height) {
  const uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(ref);
  uint32x4_t sum[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  do {
    int i = 0;
    do {
      uint16x8_t r0 = vld1q_u16(ref_ptr + i);
      highbd_obmc_sad_8x1_s16_neon(r0, mask, wsrc, &sum[0]);

      uint16x8_t r1 = vld1q_u16(ref_ptr + i + 8);
      highbd_obmc_sad_8x1_s16_neon(r1, mask + 8, wsrc + 8, &sum[1]);

      wsrc += 16;
      mask += 16;
      i += 16;
    } while (i < width);

    ref_ptr += ref_stride;
  } while (--height != 0);

  return horizontal_add_u32x4(vaddq_u32(sum[0], sum[1]));
}

static inline unsigned int highbd_obmc_sad_16xh_neon(const uint8_t *ref,
                                                     int ref_stride,
                                                     const int32_t *wsrc,
                                                     const int32_t *mask,
                                                     int h) {
  return highbd_obmc_sad_large_neon(ref, ref_stride, wsrc, mask, 16, h);
}

static inline unsigned int highbd_obmc_sad_32xh_neon(const uint8_t *ref,
                                                     int ref_stride,
                                                     const int32_t *wsrc,
                                                     const int32_t *mask,
                                                     int height) {
  uint32x4_t sum[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0),
                        vdupq_n_u32(0) };
  const uint16_t *ref_ptr = CONVERT_TO_SHORTPTR(ref);

  do {
    uint16x8_t r0 = vld1q_u16(ref_ptr);
    uint16x8_t r1 = vld1q_u16(ref_ptr + 8);
    uint16x8_t r2 = vld1q_u16(ref_ptr + 16);
    uint16x8_t r3 = vld1q_u16(ref_ptr + 24);

    highbd_obmc_sad_8x1_s16_neon(r0, mask, wsrc, &sum[0]);
    highbd_obmc_sad_8x1_s16_neon(r1, mask + 8, wsrc + 8, &sum[1]);
    highbd_obmc_sad_8x1_s16_neon(r2, mask + 16, wsrc + 16, &sum[2]);
    highbd_obmc_sad_8x1_s16_neon(r3, mask + 24, wsrc + 24, &sum[3]);

    wsrc += 32;
    mask += 32;
    ref_ptr += ref_stride;
  } while (--height != 0);

  sum[0] = vaddq_u32(sum[0], sum[1]);
  sum[2] = vaddq_u32(sum[2], sum[3]);

  return horizontal_add_u32x4(vaddq_u32(sum[0], sum[2]));
}

static inline unsigned int highbd_obmc_sad_64xh_neon(const uint8_t *ref,
                                                     int ref_stride,
                                                     const int32_t *wsrc,
                                                     const int32_t *mask,
                                                     int h) {
  return highbd_obmc_sad_large_neon(ref, ref_stride, wsrc, mask, 64, h);
}

static inline unsigned int highbd_obmc_sad_128xh_neon(const uint8_t *ref,
                                                      int ref_stride,
                                                      const int32_t *wsrc,
                                                      const int32_t *mask,
                                                      int h) {
  return highbd_obmc_sad_large_neon(ref, ref_stride, wsrc, mask, 128, h);
}

#define HIGHBD_OBMC_SAD_WXH_NEON(w, h)                                   \
  unsigned int aom_highbd_obmc_sad##w##x##h##_neon(                      \
      const uint8_t *ref, int ref_stride, const int32_t *wsrc,           \
      const int32_t *mask) {                                             \
    return highbd_obmc_sad_##w##xh_neon(ref, ref_stride, wsrc, mask, h); \
  }

HIGHBD_OBMC_SAD_WXH_NEON(4, 4)
HIGHBD_OBMC_SAD_WXH_NEON(4, 8)

HIGHBD_OBMC_SAD_WXH_NEON(8, 4)
HIGHBD_OBMC_SAD_WXH_NEON(8, 8)
HIGHBD_OBMC_SAD_WXH_NEON(8, 16)

HIGHBD_OBMC_SAD_WXH_NEON(16, 8)
HIGHBD_OBMC_SAD_WXH_NEON(16, 16)
HIGHBD_OBMC_SAD_WXH_NEON(16, 32)

HIGHBD_OBMC_SAD_WXH_NEON(32, 16)
HIGHBD_OBMC_SAD_WXH_NEON(32, 32)
HIGHBD_OBMC_SAD_WXH_NEON(32, 64)

HIGHBD_OBMC_SAD_WXH_NEON(64, 32)
HIGHBD_OBMC_SAD_WXH_NEON(64, 64)
HIGHBD_OBMC_SAD_WXH_NEON(64, 128)

HIGHBD_OBMC_SAD_WXH_NEON(128, 64)
HIGHBD_OBMC_SAD_WXH_NEON(128, 128)

#if !CONFIG_REALTIME_ONLY
HIGHBD_OBMC_SAD_WXH_NEON(4, 16)

HIGHBD_OBMC_SAD_WXH_NEON(8, 32)

HIGHBD_OBMC_SAD_WXH_NEON(16, 4)
HIGHBD_OBMC_SAD_WXH_NEON(16, 64)

HIGHBD_OBMC_SAD_WXH_NEON(32, 8)

HIGHBD_OBMC_SAD_WXH_NEON(64, 16)
#endif  // !CONFIG_REALTIME_ONLY
