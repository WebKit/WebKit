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

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/arm/sum_neon.h"

static inline void highbd_sse_8x1_init_neon(const uint16_t *src,
                                            const uint16_t *ref,
                                            uint32x4_t *sse_acc0,
                                            uint32x4_t *sse_acc1) {
  uint16x8_t s = vld1q_u16(src);
  uint16x8_t r = vld1q_u16(ref);

  uint16x8_t abs_diff = vabdq_u16(s, r);
  uint16x4_t abs_diff_lo = vget_low_u16(abs_diff);
  uint16x4_t abs_diff_hi = vget_high_u16(abs_diff);

  *sse_acc0 = vmull_u16(abs_diff_lo, abs_diff_lo);
  *sse_acc1 = vmull_u16(abs_diff_hi, abs_diff_hi);
}

static inline void highbd_sse_8x1_neon(const uint16_t *src, const uint16_t *ref,
                                       uint32x4_t *sse_acc0,
                                       uint32x4_t *sse_acc1) {
  uint16x8_t s = vld1q_u16(src);
  uint16x8_t r = vld1q_u16(ref);

  uint16x8_t abs_diff = vabdq_u16(s, r);
  uint16x4_t abs_diff_lo = vget_low_u16(abs_diff);
  uint16x4_t abs_diff_hi = vget_high_u16(abs_diff);

  *sse_acc0 = vmlal_u16(*sse_acc0, abs_diff_lo, abs_diff_lo);
  *sse_acc1 = vmlal_u16(*sse_acc1, abs_diff_hi, abs_diff_hi);
}

static inline int64_t highbd_sse_128xh_neon(const uint16_t *src, int src_stride,
                                            const uint16_t *ref, int ref_stride,
                                            int height) {
  uint32x4_t sse[16];
  highbd_sse_8x1_init_neon(src + 0 * 8, ref + 0 * 8, &sse[0], &sse[1]);
  highbd_sse_8x1_init_neon(src + 1 * 8, ref + 1 * 8, &sse[2], &sse[3]);
  highbd_sse_8x1_init_neon(src + 2 * 8, ref + 2 * 8, &sse[4], &sse[5]);
  highbd_sse_8x1_init_neon(src + 3 * 8, ref + 3 * 8, &sse[6], &sse[7]);
  highbd_sse_8x1_init_neon(src + 4 * 8, ref + 4 * 8, &sse[8], &sse[9]);
  highbd_sse_8x1_init_neon(src + 5 * 8, ref + 5 * 8, &sse[10], &sse[11]);
  highbd_sse_8x1_init_neon(src + 6 * 8, ref + 6 * 8, &sse[12], &sse[13]);
  highbd_sse_8x1_init_neon(src + 7 * 8, ref + 7 * 8, &sse[14], &sse[15]);
  highbd_sse_8x1_neon(src + 8 * 8, ref + 8 * 8, &sse[0], &sse[1]);
  highbd_sse_8x1_neon(src + 9 * 8, ref + 9 * 8, &sse[2], &sse[3]);
  highbd_sse_8x1_neon(src + 10 * 8, ref + 10 * 8, &sse[4], &sse[5]);
  highbd_sse_8x1_neon(src + 11 * 8, ref + 11 * 8, &sse[6], &sse[7]);
  highbd_sse_8x1_neon(src + 12 * 8, ref + 12 * 8, &sse[8], &sse[9]);
  highbd_sse_8x1_neon(src + 13 * 8, ref + 13 * 8, &sse[10], &sse[11]);
  highbd_sse_8x1_neon(src + 14 * 8, ref + 14 * 8, &sse[12], &sse[13]);
  highbd_sse_8x1_neon(src + 15 * 8, ref + 15 * 8, &sse[14], &sse[15]);

  src += src_stride;
  ref += ref_stride;

  while (--height != 0) {
    highbd_sse_8x1_neon(src + 0 * 8, ref + 0 * 8, &sse[0], &sse[1]);
    highbd_sse_8x1_neon(src + 1 * 8, ref + 1 * 8, &sse[2], &sse[3]);
    highbd_sse_8x1_neon(src + 2 * 8, ref + 2 * 8, &sse[4], &sse[5]);
    highbd_sse_8x1_neon(src + 3 * 8, ref + 3 * 8, &sse[6], &sse[7]);
    highbd_sse_8x1_neon(src + 4 * 8, ref + 4 * 8, &sse[8], &sse[9]);
    highbd_sse_8x1_neon(src + 5 * 8, ref + 5 * 8, &sse[10], &sse[11]);
    highbd_sse_8x1_neon(src + 6 * 8, ref + 6 * 8, &sse[12], &sse[13]);
    highbd_sse_8x1_neon(src + 7 * 8, ref + 7 * 8, &sse[14], &sse[15]);
    highbd_sse_8x1_neon(src + 8 * 8, ref + 8 * 8, &sse[0], &sse[1]);
    highbd_sse_8x1_neon(src + 9 * 8, ref + 9 * 8, &sse[2], &sse[3]);
    highbd_sse_8x1_neon(src + 10 * 8, ref + 10 * 8, &sse[4], &sse[5]);
    highbd_sse_8x1_neon(src + 11 * 8, ref + 11 * 8, &sse[6], &sse[7]);
    highbd_sse_8x1_neon(src + 12 * 8, ref + 12 * 8, &sse[8], &sse[9]);
    highbd_sse_8x1_neon(src + 13 * 8, ref + 13 * 8, &sse[10], &sse[11]);
    highbd_sse_8x1_neon(src + 14 * 8, ref + 14 * 8, &sse[12], &sse[13]);
    highbd_sse_8x1_neon(src + 15 * 8, ref + 15 * 8, &sse[14], &sse[15]);

    src += src_stride;
    ref += ref_stride;
  }

  return horizontal_long_add_u32x4_x16(sse);
}

static inline int64_t highbd_sse_64xh_neon(const uint16_t *src, int src_stride,
                                           const uint16_t *ref, int ref_stride,
                                           int height) {
  uint32x4_t sse[8];
  highbd_sse_8x1_init_neon(src + 0 * 8, ref + 0 * 8, &sse[0], &sse[1]);
  highbd_sse_8x1_init_neon(src + 1 * 8, ref + 1 * 8, &sse[2], &sse[3]);
  highbd_sse_8x1_init_neon(src + 2 * 8, ref + 2 * 8, &sse[4], &sse[5]);
  highbd_sse_8x1_init_neon(src + 3 * 8, ref + 3 * 8, &sse[6], &sse[7]);
  highbd_sse_8x1_neon(src + 4 * 8, ref + 4 * 8, &sse[0], &sse[1]);
  highbd_sse_8x1_neon(src + 5 * 8, ref + 5 * 8, &sse[2], &sse[3]);
  highbd_sse_8x1_neon(src + 6 * 8, ref + 6 * 8, &sse[4], &sse[5]);
  highbd_sse_8x1_neon(src + 7 * 8, ref + 7 * 8, &sse[6], &sse[7]);

  src += src_stride;
  ref += ref_stride;

  while (--height != 0) {
    highbd_sse_8x1_neon(src + 0 * 8, ref + 0 * 8, &sse[0], &sse[1]);
    highbd_sse_8x1_neon(src + 1 * 8, ref + 1 * 8, &sse[2], &sse[3]);
    highbd_sse_8x1_neon(src + 2 * 8, ref + 2 * 8, &sse[4], &sse[5]);
    highbd_sse_8x1_neon(src + 3 * 8, ref + 3 * 8, &sse[6], &sse[7]);
    highbd_sse_8x1_neon(src + 4 * 8, ref + 4 * 8, &sse[0], &sse[1]);
    highbd_sse_8x1_neon(src + 5 * 8, ref + 5 * 8, &sse[2], &sse[3]);
    highbd_sse_8x1_neon(src + 6 * 8, ref + 6 * 8, &sse[4], &sse[5]);
    highbd_sse_8x1_neon(src + 7 * 8, ref + 7 * 8, &sse[6], &sse[7]);

    src += src_stride;
    ref += ref_stride;
  }

  return horizontal_long_add_u32x4_x8(sse);
}

static inline int64_t highbd_sse_32xh_neon(const uint16_t *src, int src_stride,
                                           const uint16_t *ref, int ref_stride,
                                           int height) {
  uint32x4_t sse[8];
  highbd_sse_8x1_init_neon(src + 0 * 8, ref + 0 * 8, &sse[0], &sse[1]);
  highbd_sse_8x1_init_neon(src + 1 * 8, ref + 1 * 8, &sse[2], &sse[3]);
  highbd_sse_8x1_init_neon(src + 2 * 8, ref + 2 * 8, &sse[4], &sse[5]);
  highbd_sse_8x1_init_neon(src + 3 * 8, ref + 3 * 8, &sse[6], &sse[7]);

  src += src_stride;
  ref += ref_stride;

  while (--height != 0) {
    highbd_sse_8x1_neon(src + 0 * 8, ref + 0 * 8, &sse[0], &sse[1]);
    highbd_sse_8x1_neon(src + 1 * 8, ref + 1 * 8, &sse[2], &sse[3]);
    highbd_sse_8x1_neon(src + 2 * 8, ref + 2 * 8, &sse[4], &sse[5]);
    highbd_sse_8x1_neon(src + 3 * 8, ref + 3 * 8, &sse[6], &sse[7]);

    src += src_stride;
    ref += ref_stride;
  }

  return horizontal_long_add_u32x4_x8(sse);
}

static inline int64_t highbd_sse_16xh_neon(const uint16_t *src, int src_stride,
                                           const uint16_t *ref, int ref_stride,
                                           int height) {
  uint32x4_t sse[4];
  highbd_sse_8x1_init_neon(src + 0 * 8, ref + 0 * 8, &sse[0], &sse[1]);
  highbd_sse_8x1_init_neon(src + 1 * 8, ref + 1 * 8, &sse[2], &sse[3]);

  src += src_stride;
  ref += ref_stride;

  while (--height != 0) {
    highbd_sse_8x1_neon(src + 0 * 8, ref + 0 * 8, &sse[0], &sse[1]);
    highbd_sse_8x1_neon(src + 1 * 8, ref + 1 * 8, &sse[2], &sse[3]);

    src += src_stride;
    ref += ref_stride;
  }

  return horizontal_long_add_u32x4_x4(sse);
}

static inline int64_t highbd_sse_8xh_neon(const uint16_t *src, int src_stride,
                                          const uint16_t *ref, int ref_stride,
                                          int height) {
  uint32x4_t sse[2];
  highbd_sse_8x1_init_neon(src, ref, &sse[0], &sse[1]);

  src += src_stride;
  ref += ref_stride;

  while (--height != 0) {
    highbd_sse_8x1_neon(src, ref, &sse[0], &sse[1]);

    src += src_stride;
    ref += ref_stride;
  }

  return horizontal_long_add_u32x4_x2(sse);
}

static inline int64_t highbd_sse_4xh_neon(const uint16_t *src, int src_stride,
                                          const uint16_t *ref, int ref_stride,
                                          int height) {
  // Peel the first loop iteration.
  uint16x4_t s = vld1_u16(src);
  uint16x4_t r = vld1_u16(ref);

  uint16x4_t abs_diff = vabd_u16(s, r);
  uint32x4_t sse = vmull_u16(abs_diff, abs_diff);

  src += src_stride;
  ref += ref_stride;

  while (--height != 0) {
    s = vld1_u16(src);
    r = vld1_u16(ref);

    abs_diff = vabd_u16(s, r);
    sse = vmlal_u16(sse, abs_diff, abs_diff);

    src += src_stride;
    ref += ref_stride;
  }

  return horizontal_long_add_u32x4(sse);
}

static inline int64_t highbd_sse_wxh_neon(const uint16_t *src, int src_stride,
                                          const uint16_t *ref, int ref_stride,
                                          int width, int height) {
  // { 0, 1, 2, 3, 4, 5, 6, 7 }
  uint16x8_t k01234567 = vmovl_u8(vcreate_u8(0x0706050403020100));
  uint16x8_t remainder_mask = vcltq_u16(k01234567, vdupq_n_u16(width & 7));
  uint64_t sse = 0;

  do {
    int w = width;
    int offset = 0;

    do {
      uint16x8_t s = vld1q_u16(src + offset);
      uint16x8_t r = vld1q_u16(ref + offset);

      if (w < 8) {
        // Mask out-of-range elements.
        s = vandq_u16(s, remainder_mask);
        r = vandq_u16(r, remainder_mask);
      }

      uint16x8_t abs_diff = vabdq_u16(s, r);
      uint16x4_t abs_diff_lo = vget_low_u16(abs_diff);
      uint16x4_t abs_diff_hi = vget_high_u16(abs_diff);

      uint32x4_t sse_u32 = vmull_u16(abs_diff_lo, abs_diff_lo);
      sse_u32 = vmlal_u16(sse_u32, abs_diff_hi, abs_diff_hi);

      sse += horizontal_long_add_u32x4(sse_u32);

      offset += 8;
      w -= 8;
    } while (w > 0);

    src += src_stride;
    ref += ref_stride;
  } while (--height != 0);

  return sse;
}

int64_t aom_highbd_sse_neon(const uint8_t *src8, int src_stride,
                            const uint8_t *ref8, int ref_stride, int width,
                            int height) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);

  switch (width) {
    case 4:
      return highbd_sse_4xh_neon(src, src_stride, ref, ref_stride, height);
    case 8:
      return highbd_sse_8xh_neon(src, src_stride, ref, ref_stride, height);
    case 16:
      return highbd_sse_16xh_neon(src, src_stride, ref, ref_stride, height);
    case 32:
      return highbd_sse_32xh_neon(src, src_stride, ref, ref_stride, height);
    case 64:
      return highbd_sse_64xh_neon(src, src_stride, ref, ref_stride, height);
    case 128:
      return highbd_sse_128xh_neon(src, src_stride, ref, ref_stride, height);
    default:
      return highbd_sse_wxh_neon(src, src_stride, ref, ref_stride, width,
                                 height);
  }
}
