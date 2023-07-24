/*
 *  Copyright (c) 2020, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "config/aom_dsp_rtcd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/sum_neon.h"
#include "aom_dsp/arm/transpose_neon.h"

#if defined(__ARM_FEATURE_DOTPROD)

static INLINE void sse_16x1_neon(const uint8_t *src, const uint8_t *ref,
                                 uint32x4_t *sse) {
  uint8x16_t s = vld1q_u8(src);
  uint8x16_t r = vld1q_u8(ref);

  uint8x16_t abs_diff = vabdq_u8(s, r);

  *sse = vdotq_u32(*sse, abs_diff, abs_diff);
}

static INLINE void sse_8x1_neon(const uint8_t *src, const uint8_t *ref,
                                uint32x2_t *sse) {
  uint8x8_t s = vld1_u8(src);
  uint8x8_t r = vld1_u8(ref);

  uint8x8_t abs_diff = vabd_u8(s, r);

  *sse = vdot_u32(*sse, abs_diff, abs_diff);
}

static INLINE void sse_4x2_neon(const uint8_t *src, int src_stride,
                                const uint8_t *ref, int ref_stride,
                                uint32x2_t *sse) {
  uint8x8_t s = load_unaligned_u8(src, src_stride);
  uint8x8_t r = load_unaligned_u8(ref, ref_stride);

  uint8x8_t abs_diff = vabd_u8(s, r);

  *sse = vdot_u32(*sse, abs_diff, abs_diff);
}

static INLINE uint32_t sse_8xh_neon(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    int height) {
  uint32x2_t sse[2] = { vdup_n_u32(0), vdup_n_u32(0) };

  int i = height;
  do {
    sse_8x1_neon(src, ref, &sse[0]);
    src += src_stride;
    ref += ref_stride;
    sse_8x1_neon(src, ref, &sse[1]);
    src += src_stride;
    ref += ref_stride;
    i -= 2;
  } while (i != 0);

  return horizontal_add_u32x4(vcombine_u32(sse[0], sse[1]));
}

static INLINE uint32_t sse_4xh_neon(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    int height) {
  uint32x2_t sse = vdup_n_u32(0);

  int i = height;
  do {
    sse_4x2_neon(src, src_stride, ref, ref_stride, &sse);

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    i -= 2;
  } while (i != 0);

  return horizontal_add_u32x2(sse);
}

static INLINE uint32_t sse_wxh_neon(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    int width, int height) {
  uint32x2_t sse[2] = { vdup_n_u32(0), vdup_n_u32(0) };

  if ((width & 0x07) && ((width & 0x07) < 5)) {
    int i = height;
    do {
      int j = 0;
      do {
        sse_8x1_neon(src + j, ref + j, &sse[0]);
        sse_8x1_neon(src + j + src_stride, ref + j + ref_stride, &sse[1]);
        j += 8;
      } while (j + 4 < width);

      sse_4x2_neon(src + j, src_stride, ref + j, ref_stride, &sse[0]);
      src += 2 * src_stride;
      ref += 2 * ref_stride;
      i -= 2;
    } while (i != 0);
  } else {
    int i = height;
    do {
      int j = 0;
      do {
        sse_8x1_neon(src + j, ref + j, &sse[0]);
        sse_8x1_neon(src + j + src_stride, ref + j + ref_stride, &sse[1]);
        j += 8;
      } while (j < width);

      src += 2 * src_stride;
      ref += 2 * ref_stride;
      i -= 2;
    } while (i != 0);
  }
  return horizontal_add_u32x4(vcombine_u32(sse[0], sse[1]));
}

#else  // !defined(__ARM_FEATURE_DOTPROD)

static INLINE void sse_16x1_neon(const uint8_t *src, const uint8_t *ref,
                                 uint32x4_t *sse) {
  uint8x16_t s = vld1q_u8(src);
  uint8x16_t r = vld1q_u8(ref);

  uint8x16_t abs_diff = vabdq_u8(s, r);
  uint8x8_t abs_diff_lo = vget_low_u8(abs_diff);
  uint8x8_t abs_diff_hi = vget_high_u8(abs_diff);

  *sse = vpadalq_u16(*sse, vmull_u8(abs_diff_lo, abs_diff_lo));
  *sse = vpadalq_u16(*sse, vmull_u8(abs_diff_hi, abs_diff_hi));
}

static INLINE void sse_8x1_neon(const uint8_t *src, const uint8_t *ref,
                                uint32x4_t *sse) {
  uint8x8_t s = vld1_u8(src);
  uint8x8_t r = vld1_u8(ref);

  uint8x8_t abs_diff = vabd_u8(s, r);

  *sse = vpadalq_u16(*sse, vmull_u8(abs_diff, abs_diff));
}

static INLINE void sse_4x2_neon(const uint8_t *src, int src_stride,
                                const uint8_t *ref, int ref_stride,
                                uint32x4_t *sse) {
  uint8x8_t s = load_unaligned_u8(src, src_stride);
  uint8x8_t r = load_unaligned_u8(ref, ref_stride);

  uint8x8_t abs_diff = vabd_u8(s, r);

  *sse = vpadalq_u16(*sse, vmull_u8(abs_diff, abs_diff));
}

static INLINE uint32_t sse_8xh_neon(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    int height) {
  uint32x4_t sse = vdupq_n_u32(0);

  int i = height;
  do {
    sse_8x1_neon(src, ref, &sse);

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  return horizontal_add_u32x4(sse);
}

static INLINE uint32_t sse_4xh_neon(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    int height) {
  uint32x4_t sse = vdupq_n_u32(0);

  int i = height;
  do {
    sse_4x2_neon(src, src_stride, ref, ref_stride, &sse);

    src += 2 * src_stride;
    ref += 2 * ref_stride;
    i -= 2;
  } while (i != 0);

  return horizontal_add_u32x4(sse);
}

static INLINE uint32_t sse_wxh_neon(const uint8_t *src, int src_stride,
                                    const uint8_t *ref, int ref_stride,
                                    int width, int height) {
  uint32x4_t sse = vdupq_n_u32(0);

  if ((width & 0x07) && ((width & 0x07) < 5)) {
    int i = height;
    do {
      int j = 0;
      do {
        sse_8x1_neon(src + j, ref + j, &sse);
        sse_8x1_neon(src + j + src_stride, ref + j + ref_stride, &sse);
        j += 8;
      } while (j + 4 < width);

      sse_4x2_neon(src + j, src_stride, ref + j, ref_stride, &sse);
      src += 2 * src_stride;
      ref += 2 * ref_stride;
      i -= 2;
    } while (i != 0);
  } else {
    int i = height;
    do {
      int j = 0;
      do {
        sse_8x1_neon(src + j, ref + j, &sse);
        j += 8;
      } while (j < width);

      src += src_stride;
      ref += ref_stride;
    } while (--i != 0);
  }
  return horizontal_add_u32x4(sse);
}

#endif  // defined(__ARM_FEATURE_DOTPROD)

static INLINE uint32_t sse_128xh_neon(const uint8_t *src, int src_stride,
                                      const uint8_t *ref, int ref_stride,
                                      int height) {
  uint32x4_t sse[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = height;
  do {
    sse_16x1_neon(src, ref, &sse[0]);
    sse_16x1_neon(src + 16, ref + 16, &sse[1]);
    sse_16x1_neon(src + 32, ref + 32, &sse[0]);
    sse_16x1_neon(src + 48, ref + 48, &sse[1]);
    sse_16x1_neon(src + 64, ref + 64, &sse[0]);
    sse_16x1_neon(src + 80, ref + 80, &sse[1]);
    sse_16x1_neon(src + 96, ref + 96, &sse[0]);
    sse_16x1_neon(src + 112, ref + 112, &sse[1]);

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  return horizontal_add_u32x4(vaddq_u32(sse[0], sse[1]));
}

static INLINE uint32_t sse_64xh_neon(const uint8_t *src, int src_stride,
                                     const uint8_t *ref, int ref_stride,
                                     int height) {
  uint32x4_t sse[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = height;
  do {
    sse_16x1_neon(src, ref, &sse[0]);
    sse_16x1_neon(src + 16, ref + 16, &sse[1]);
    sse_16x1_neon(src + 32, ref + 32, &sse[0]);
    sse_16x1_neon(src + 48, ref + 48, &sse[1]);

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  return horizontal_add_u32x4(vaddq_u32(sse[0], sse[1]));
}

static INLINE uint32_t sse_32xh_neon(const uint8_t *src, int src_stride,
                                     const uint8_t *ref, int ref_stride,
                                     int height) {
  uint32x4_t sse[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = height;
  do {
    sse_16x1_neon(src, ref, &sse[0]);
    sse_16x1_neon(src + 16, ref + 16, &sse[1]);

    src += src_stride;
    ref += ref_stride;
  } while (--i != 0);

  return horizontal_add_u32x4(vaddq_u32(sse[0], sse[1]));
}

static INLINE uint32_t sse_16xh_neon(const uint8_t *src, int src_stride,
                                     const uint8_t *ref, int ref_stride,
                                     int height) {
  uint32x4_t sse[2] = { vdupq_n_u32(0), vdupq_n_u32(0) };

  int i = height;
  do {
    sse_16x1_neon(src, ref, &sse[0]);
    src += src_stride;
    ref += ref_stride;
    sse_16x1_neon(src, ref, &sse[1]);
    src += src_stride;
    ref += ref_stride;
    i -= 2;
  } while (i != 0);

  return horizontal_add_u32x4(vaddq_u32(sse[0], sse[1]));
}

int64_t aom_sse_neon(const uint8_t *src, int src_stride, const uint8_t *ref,
                     int ref_stride, int width, int height) {
  switch (width) {
    case 4: return sse_4xh_neon(src, src_stride, ref, ref_stride, height);
    case 8: return sse_8xh_neon(src, src_stride, ref, ref_stride, height);
    case 16: return sse_16xh_neon(src, src_stride, ref, ref_stride, height);
    case 32: return sse_32xh_neon(src, src_stride, ref, ref_stride, height);
    case 64: return sse_64xh_neon(src, src_stride, ref, ref_stride, height);
    case 128: return sse_128xh_neon(src, src_stride, ref, ref_stride, height);
    default:
      return sse_wxh_neon(src, src_stride, ref, ref_stride, width, height);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE uint32_t highbd_sse_W8x1_neon(uint16x8_t q2, uint16x8_t q3) {
  uint32_t sse;
  const uint32_t sse1 = 0;
  const uint32x4_t q1 = vld1q_dup_u32(&sse1);

  uint16x8_t q4 = vabdq_u16(q2, q3);  // diff = abs(a[x] - b[x])
  uint16x4_t d0 = vget_low_u16(q4);
  uint16x4_t d1 = vget_high_u16(q4);

  uint32x4_t q6 = vmlal_u16(q1, d0, d0);
  uint32x4_t q7 = vmlal_u16(q1, d1, d1);

  uint32x2_t d4 = vadd_u32(vget_low_u32(q6), vget_high_u32(q6));
  uint32x2_t d5 = vadd_u32(vget_low_u32(q7), vget_high_u32(q7));

  uint32x2_t d6 = vadd_u32(d4, d5);

  sse = vget_lane_u32(d6, 0);
  sse += vget_lane_u32(d6, 1);

  return sse;
}

int64_t aom_highbd_sse_neon(const uint8_t *a8, int a_stride, const uint8_t *b8,
                            int b_stride, int width, int height) {
  static const uint16_t k01234567[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  const uint16x8_t q0 = vld1q_u16(k01234567);
  int64_t sse = 0;
  uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  int x, y;
  int addinc;
  uint16x4_t d0, d1, d2, d3;
  uint16_t dx;
  uint16x8_t q2, q3, q4, q5;

  switch (width) {
    case 4:
      for (y = 0; y < height; y += 2) {
        d0 = vld1_u16(a);  // load 4 data
        a += a_stride;
        d1 = vld1_u16(a);
        a += a_stride;

        d2 = vld1_u16(b);
        b += b_stride;
        d3 = vld1_u16(b);
        b += b_stride;
        q2 = vcombine_u16(d0, d1);  // make a 8 data vector
        q3 = vcombine_u16(d2, d3);

        sse += highbd_sse_W8x1_neon(q2, q3);
      }
      break;
    case 8:
      for (y = 0; y < height; y++) {
        q2 = vld1q_u16(a);
        q3 = vld1q_u16(b);

        sse += highbd_sse_W8x1_neon(q2, q3);

        a += a_stride;
        b += b_stride;
      }
      break;
    case 16:
      for (y = 0; y < height; y++) {
        q2 = vld1q_u16(a);
        q3 = vld1q_u16(b);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 8);
        q3 = vld1q_u16(b + 8);

        sse += highbd_sse_W8x1_neon(q2, q3);

        a += a_stride;
        b += b_stride;
      }
      break;
    case 32:
      for (y = 0; y < height; y++) {
        q2 = vld1q_u16(a);
        q3 = vld1q_u16(b);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 8);
        q3 = vld1q_u16(b + 8);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 16);
        q3 = vld1q_u16(b + 16);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 24);
        q3 = vld1q_u16(b + 24);

        sse += highbd_sse_W8x1_neon(q2, q3);

        a += a_stride;
        b += b_stride;
      }
      break;
    case 64:
      for (y = 0; y < height; y++) {
        q2 = vld1q_u16(a);
        q3 = vld1q_u16(b);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 8);
        q3 = vld1q_u16(b + 8);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 16);
        q3 = vld1q_u16(b + 16);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 24);
        q3 = vld1q_u16(b + 24);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 32);
        q3 = vld1q_u16(b + 32);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 40);
        q3 = vld1q_u16(b + 40);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 48);
        q3 = vld1q_u16(b + 48);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 56);
        q3 = vld1q_u16(b + 56);

        sse += highbd_sse_W8x1_neon(q2, q3);

        a += a_stride;
        b += b_stride;
      }
      break;
    case 128:
      for (y = 0; y < height; y++) {
        q2 = vld1q_u16(a);
        q3 = vld1q_u16(b);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 8);
        q3 = vld1q_u16(b + 8);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 16);
        q3 = vld1q_u16(b + 16);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 24);
        q3 = vld1q_u16(b + 24);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 32);
        q3 = vld1q_u16(b + 32);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 40);
        q3 = vld1q_u16(b + 40);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 48);
        q3 = vld1q_u16(b + 48);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 56);
        q3 = vld1q_u16(b + 56);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 64);
        q3 = vld1q_u16(b + 64);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 72);
        q3 = vld1q_u16(b + 72);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 80);
        q3 = vld1q_u16(b + 80);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 88);
        q3 = vld1q_u16(b + 88);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 96);
        q3 = vld1q_u16(b + 96);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 104);
        q3 = vld1q_u16(b + 104);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 112);
        q3 = vld1q_u16(b + 112);

        sse += highbd_sse_W8x1_neon(q2, q3);

        q2 = vld1q_u16(a + 120);
        q3 = vld1q_u16(b + 120);

        sse += highbd_sse_W8x1_neon(q2, q3);
        a += a_stride;
        b += b_stride;
      }
      break;
    default:

      for (y = 0; y < height; y++) {
        x = width;
        while (x > 0) {
          addinc = width - x;
          q2 = vld1q_u16(a + addinc);
          q3 = vld1q_u16(b + addinc);
          if (x < 8) {
            dx = x;
            q4 = vld1q_dup_u16(&dx);
            q5 = vcltq_u16(q0, q4);
            q2 = vandq_u16(q2, q5);
            q3 = vandq_u16(q3, q5);
          }
          sse += highbd_sse_W8x1_neon(q2, q3);
          x -= 8;
        }
        a += a_stride;
        b += b_stride;
      }
  }
  return (int64_t)sse;
}
#endif
