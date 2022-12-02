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
#include "aom_dsp/arm/sum_neon.h"

unsigned int aom_sad8x16_neon(const uint8_t *src_ptr, int src_stride,
                              const uint8_t *ref_ptr, int ref_stride) {
  uint8x8_t d0, d8;
  uint16x8_t q12;
  uint32x4_t q1;
  uint64x2_t q3;
  uint32x2_t d5;
  int i;

  d0 = vld1_u8(src_ptr);
  src_ptr += src_stride;
  d8 = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  q12 = vabdl_u8(d0, d8);

  for (i = 0; i < 15; i++) {
    d0 = vld1_u8(src_ptr);
    src_ptr += src_stride;
    d8 = vld1_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabal_u8(q12, d0, d8);
  }

  q1 = vpaddlq_u16(q12);
  q3 = vpaddlq_u32(q1);
  d5 = vadd_u32(vreinterpret_u32_u64(vget_low_u64(q3)),
                vreinterpret_u32_u64(vget_high_u64(q3)));

  return vget_lane_u32(d5, 0);
}

unsigned int aom_sad4x4_neon(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *ref_ptr, int ref_stride) {
  uint8x8_t d0, d8;
  uint16x8_t q12;
  uint32x2_t d1;
  uint64x1_t d3;
  int i;

  d0 = vld1_u8(src_ptr);
  src_ptr += src_stride;
  d8 = vld1_u8(ref_ptr);
  ref_ptr += ref_stride;
  q12 = vabdl_u8(d0, d8);

  for (i = 0; i < 3; i++) {
    d0 = vld1_u8(src_ptr);
    src_ptr += src_stride;
    d8 = vld1_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabal_u8(q12, d0, d8);
  }

  d1 = vpaddl_u16(vget_low_u16(q12));
  d3 = vpaddl_u32(d1);

  return vget_lane_u32(vreinterpret_u32_u64(d3), 0);
}

unsigned int aom_sad16x8_neon(const uint8_t *src_ptr, int src_stride,
                              const uint8_t *ref_ptr, int ref_stride) {
  uint8x16_t q0, q4;
  uint16x8_t q12, q13;
  uint32x4_t q1;
  uint64x2_t q3;
  uint32x2_t d5;
  int i;

  q0 = vld1q_u8(src_ptr);
  src_ptr += src_stride;
  q4 = vld1q_u8(ref_ptr);
  ref_ptr += ref_stride;
  q12 = vabdl_u8(vget_low_u8(q0), vget_low_u8(q4));
  q13 = vabdl_u8(vget_high_u8(q0), vget_high_u8(q4));

  for (i = 0; i < 7; i++) {
    q0 = vld1q_u8(src_ptr);
    src_ptr += src_stride;
    q4 = vld1q_u8(ref_ptr);
    ref_ptr += ref_stride;
    q12 = vabal_u8(q12, vget_low_u8(q0), vget_low_u8(q4));
    q13 = vabal_u8(q13, vget_high_u8(q0), vget_high_u8(q4));
  }

  q12 = vaddq_u16(q12, q13);
  q1 = vpaddlq_u16(q12);
  q3 = vpaddlq_u32(q1);
  d5 = vadd_u32(vreinterpret_u32_u64(vget_low_u64(q3)),
                vreinterpret_u32_u64(vget_high_u64(q3)));

  return vget_lane_u32(d5, 0);
}

unsigned int aom_sad64x64_neon(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum_lo = vdupq_n_u16(0);
  uint16x8_t vec_accum_hi = vdupq_n_u16(0);
  for (i = 0; i < 64; ++i) {
    const uint8x16_t vec_src_00 = vld1q_u8(src);
    const uint8x16_t vec_src_16 = vld1q_u8(src + 16);
    const uint8x16_t vec_src_32 = vld1q_u8(src + 32);
    const uint8x16_t vec_src_48 = vld1q_u8(src + 48);
    const uint8x16_t vec_ref_00 = vld1q_u8(ref);
    const uint8x16_t vec_ref_16 = vld1q_u8(ref + 16);
    const uint8x16_t vec_ref_32 = vld1q_u8(ref + 32);
    const uint8x16_t vec_ref_48 = vld1q_u8(ref + 48);
    src += src_stride;
    ref += ref_stride;
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_00),
                            vget_low_u8(vec_ref_00));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_00),
                            vget_high_u8(vec_ref_00));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_16),
                            vget_low_u8(vec_ref_16));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_16),
                            vget_high_u8(vec_ref_16));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_32),
                            vget_low_u8(vec_ref_32));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_32),
                            vget_high_u8(vec_ref_32));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_48),
                            vget_low_u8(vec_ref_48));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_48),
                            vget_high_u8(vec_ref_48));
  }
  return horizontal_long_add_u16x8(vec_accum_lo, vec_accum_hi);
}

unsigned int aom_sad128x128_neon(const uint8_t *src, int src_stride,
                                 const uint8_t *ref, int ref_stride) {
  uint16x8_t vec_accum_lo, vec_accum_hi;
  uint32x4_t vec_accum_32lo = vdupq_n_u32(0);
  uint32x4_t vec_accum_32hi = vdupq_n_u32(0);
  uint16x8_t tmp;
  for (int i = 0; i < 128; ++i) {
    const uint8x16_t vec_src_00 = vld1q_u8(src);
    const uint8x16_t vec_src_16 = vld1q_u8(src + 16);
    const uint8x16_t vec_src_32 = vld1q_u8(src + 32);
    const uint8x16_t vec_src_48 = vld1q_u8(src + 48);
    const uint8x16_t vec_src_64 = vld1q_u8(src + 64);
    const uint8x16_t vec_src_80 = vld1q_u8(src + 80);
    const uint8x16_t vec_src_96 = vld1q_u8(src + 96);
    const uint8x16_t vec_src_112 = vld1q_u8(src + 112);
    const uint8x16_t vec_ref_00 = vld1q_u8(ref);
    const uint8x16_t vec_ref_16 = vld1q_u8(ref + 16);
    const uint8x16_t vec_ref_32 = vld1q_u8(ref + 32);
    const uint8x16_t vec_ref_48 = vld1q_u8(ref + 48);
    const uint8x16_t vec_ref_64 = vld1q_u8(ref + 64);
    const uint8x16_t vec_ref_80 = vld1q_u8(ref + 80);
    const uint8x16_t vec_ref_96 = vld1q_u8(ref + 96);
    const uint8x16_t vec_ref_112 = vld1q_u8(ref + 112);
    src += src_stride;
    ref += ref_stride;
    vec_accum_lo = vdupq_n_u16(0);
    vec_accum_hi = vdupq_n_u16(0);
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_00),
                            vget_low_u8(vec_ref_00));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_00),
                            vget_high_u8(vec_ref_00));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_16),
                            vget_low_u8(vec_ref_16));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_16),
                            vget_high_u8(vec_ref_16));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_32),
                            vget_low_u8(vec_ref_32));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_32),
                            vget_high_u8(vec_ref_32));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_48),
                            vget_low_u8(vec_ref_48));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_48),
                            vget_high_u8(vec_ref_48));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_64),
                            vget_low_u8(vec_ref_64));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_64),
                            vget_high_u8(vec_ref_64));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_80),
                            vget_low_u8(vec_ref_80));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_80),
                            vget_high_u8(vec_ref_80));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_96),
                            vget_low_u8(vec_ref_96));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_96),
                            vget_high_u8(vec_ref_96));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_112),
                            vget_low_u8(vec_ref_112));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_112),
                            vget_high_u8(vec_ref_112));

    tmp = vaddq_u16(vec_accum_lo, vec_accum_hi);
    vec_accum_32lo = vaddw_u16(vec_accum_32lo, vget_low_u16(tmp));
    vec_accum_32hi = vaddw_u16(vec_accum_32hi, vget_high_u16(tmp));
  }
  const uint32x4_t a = vaddq_u32(vec_accum_32lo, vec_accum_32hi);
  const uint64x2_t b = vpaddlq_u32(a);
  const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                vreinterpret_u32_u64(vget_high_u64(b)));
  return vget_lane_u32(c, 0);
}

unsigned int aom_sad32x32_neon(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum_lo = vdupq_n_u16(0);
  uint16x8_t vec_accum_hi = vdupq_n_u16(0);

  for (i = 0; i < 32; ++i) {
    const uint8x16_t vec_src_00 = vld1q_u8(src);
    const uint8x16_t vec_src_16 = vld1q_u8(src + 16);
    const uint8x16_t vec_ref_00 = vld1q_u8(ref);
    const uint8x16_t vec_ref_16 = vld1q_u8(ref + 16);
    src += src_stride;
    ref += ref_stride;
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_00),
                            vget_low_u8(vec_ref_00));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_00),
                            vget_high_u8(vec_ref_00));
    vec_accum_lo = vabal_u8(vec_accum_lo, vget_low_u8(vec_src_16),
                            vget_low_u8(vec_ref_16));
    vec_accum_hi = vabal_u8(vec_accum_hi, vget_high_u8(vec_src_16),
                            vget_high_u8(vec_ref_16));
  }
  return horizontal_add_u16x8(vaddq_u16(vec_accum_lo, vec_accum_hi));
}

unsigned int aom_sad16x16_neon(const uint8_t *src, int src_stride,
                               const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum_lo = vdupq_n_u16(0);
  uint16x8_t vec_accum_hi = vdupq_n_u16(0);

  for (i = 0; i < 16; ++i) {
    const uint8x16_t vec_src = vld1q_u8(src);
    const uint8x16_t vec_ref = vld1q_u8(ref);
    src += src_stride;
    ref += ref_stride;
    vec_accum_lo =
        vabal_u8(vec_accum_lo, vget_low_u8(vec_src), vget_low_u8(vec_ref));
    vec_accum_hi =
        vabal_u8(vec_accum_hi, vget_high_u8(vec_src), vget_high_u8(vec_ref));
  }
  return horizontal_add_u16x8(vaddq_u16(vec_accum_lo, vec_accum_hi));
}

unsigned int aom_sad8x8_neon(const uint8_t *src, int src_stride,
                             const uint8_t *ref, int ref_stride) {
  int i;
  uint16x8_t vec_accum = vdupq_n_u16(0);

  for (i = 0; i < 8; ++i) {
    const uint8x8_t vec_src = vld1_u8(src);
    const uint8x8_t vec_ref = vld1_u8(ref);
    src += src_stride;
    ref += ref_stride;
    vec_accum = vabal_u8(vec_accum, vec_src, vec_ref);
  }
  return horizontal_add_u16x8(vec_accum);
}

static INLINE unsigned int sad128xh_neon(const uint8_t *src_ptr, int src_stride,
                                         const uint8_t *ref_ptr, int ref_stride,
                                         int h) {
  int sum = 0;
  for (int i = 0; i < h; i++) {
    uint16x8_t q3 = vdupq_n_u16(0);

    uint8x16_t q0 = vld1q_u8(src_ptr);
    uint8x16_t q1 = vld1q_u8(ref_ptr);
    uint8x16_t q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 16);
    q1 = vld1q_u8(ref_ptr + 16);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 32);
    q1 = vld1q_u8(ref_ptr + 32);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 48);
    q1 = vld1q_u8(ref_ptr + 48);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 64);
    q1 = vld1q_u8(ref_ptr + 64);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 80);
    q1 = vld1q_u8(ref_ptr + 80);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 96);
    q1 = vld1q_u8(ref_ptr + 96);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 112);
    q1 = vld1q_u8(ref_ptr + 112);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    src_ptr += src_stride;
    ref_ptr += ref_stride;

    sum += horizontal_add_u16x8(q3);
  }

  return sum;
}

static INLINE unsigned int sad64xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  int sum = 0;
  for (int i = 0; i < h; i++) {
    uint16x8_t q3 = vdupq_n_u16(0);

    uint8x16_t q0 = vld1q_u8(src_ptr);
    uint8x16_t q1 = vld1q_u8(ref_ptr);
    uint8x16_t q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 16);
    q1 = vld1q_u8(ref_ptr + 16);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 32);
    q1 = vld1q_u8(ref_ptr + 32);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 48);
    q1 = vld1q_u8(ref_ptr + 48);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    src_ptr += src_stride;
    ref_ptr += ref_stride;

    sum += horizontal_add_u16x8(q3);
  }

  return sum;
}

static INLINE unsigned int sad32xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  int sum = 0;
  for (int i = 0; i < h; i++) {
    uint16x8_t q3 = vdupq_n_u16(0);

    uint8x16_t q0 = vld1q_u8(src_ptr);
    uint8x16_t q1 = vld1q_u8(ref_ptr);
    uint8x16_t q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    q0 = vld1q_u8(src_ptr + 16);
    q1 = vld1q_u8(ref_ptr + 16);
    q2 = vabdq_u8(q0, q1);
    q3 = vpadalq_u8(q3, q2);

    sum += horizontal_add_u16x8(q3);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  }

  return sum;
}

static INLINE unsigned int sad16xh_neon(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  int sum = 0;
  for (int i = 0; i < h; i++) {
    uint8x8_t q0 = vld1_u8(src_ptr);
    uint8x8_t q1 = vld1_u8(ref_ptr);
    sum += vget_lane_u16(vpaddl_u8(vabd_u8(q0, q1)), 0);
    sum += vget_lane_u16(vpaddl_u8(vabd_u8(q0, q1)), 1);
    sum += vget_lane_u16(vpaddl_u8(vabd_u8(q0, q1)), 2);
    sum += vget_lane_u16(vpaddl_u8(vabd_u8(q0, q1)), 3);
    q0 = vld1_u8(src_ptr + 8);
    q1 = vld1_u8(ref_ptr + 8);
    sum += vget_lane_u16(vpaddl_u8(vabd_u8(q0, q1)), 0);
    sum += vget_lane_u16(vpaddl_u8(vabd_u8(q0, q1)), 1);
    sum += vget_lane_u16(vpaddl_u8(vabd_u8(q0, q1)), 2);
    sum += vget_lane_u16(vpaddl_u8(vabd_u8(q0, q1)), 3);

    src_ptr += src_stride;
    ref_ptr += ref_stride;
  }

  return sum;
}

static INLINE unsigned int sad8xh_neon(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       int h) {
  uint16x8_t q3 = vdupq_n_u16(0);
  for (int y = 0; y < h; y++) {
    uint8x8_t q0 = vld1_u8(src_ptr);
    uint8x8_t q1 = vld1_u8(ref_ptr);
    src_ptr += src_stride;
    ref_ptr += ref_stride;
    q3 = vabal_u8(q3, q0, q1);
  }
  return horizontal_add_u16x8(q3);
}

static INLINE unsigned int sad4xh_neon(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride,
                                       int h) {
  uint16x8_t q3 = vdupq_n_u16(0);
  uint32x2_t q0 = vdup_n_u32(0);
  uint32x2_t q1 = vdup_n_u32(0);
  uint32_t src4, ref4;
  for (int y = 0; y < h / 2; y++) {
    memcpy(&src4, src_ptr, 4);
    memcpy(&ref4, ref_ptr, 4);
    src_ptr += src_stride;
    ref_ptr += ref_stride;
    q0 = vset_lane_u32(src4, q0, 0);
    q1 = vset_lane_u32(ref4, q1, 0);

    memcpy(&src4, src_ptr, 4);
    memcpy(&ref4, ref_ptr, 4);
    src_ptr += src_stride;
    ref_ptr += ref_stride;
    q0 = vset_lane_u32(src4, q0, 1);
    q1 = vset_lane_u32(ref4, q1, 1);

    q3 = vabal_u8(q3, vreinterpret_u8_u32(q0), vreinterpret_u8_u32(q1));
  }
  return horizontal_add_u16x8(q3);
}

#define FSADS128_H(h)                                                    \
  unsigned int aom_sad_skip_128x##h##_neon(                              \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,    \
      int ref_stride) {                                                  \
    const uint32_t sum = sad128xh_neon(src_ptr, 2 * src_stride, ref_ptr, \
                                       2 * ref_stride, h / 2);           \
    return 2 * sum;                                                      \
  }

FSADS128_H(128)
FSADS128_H(64)

#undef FSADS128_H

#define FSADS64_H(h)                                                          \
  unsigned int aom_sad_skip_64x##h##_neon(                                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,         \
      int ref_stride) {                                                       \
    return 2 * sad64xh_neon(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2, \
                            h / 2);                                           \
  }

FSADS64_H(128)
FSADS64_H(64)
FSADS64_H(32)
FSADS64_H(16)

#undef FSADS64_H

#define FSADS32_H(h)                                                          \
  unsigned int aom_sad_skip_32x##h##_neon(                                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,         \
      int ref_stride) {                                                       \
    return 2 * sad32xh_neon(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2, \
                            h / 2);                                           \
  }

FSADS32_H(64)
FSADS32_H(32)
FSADS32_H(16)
FSADS32_H(8)

#undef FSADS32_H

#define FSADS16_H(h)                                                          \
  unsigned int aom_sad_skip_16x##h##_neon(                                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,         \
      int ref_stride) {                                                       \
    return 2 * sad16xh_neon(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2, \
                            h / 2);                                           \
  }

FSADS16_H(64)
FSADS16_H(32)
FSADS16_H(16)
FSADS16_H(8)

#undef FSADS16_H

#define FSADS8_H(h)                                                          \
  unsigned int aom_sad_skip_8x##h##_neon(                                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,        \
      int ref_stride) {                                                      \
    return 2 * sad8xh_neon(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2, \
                           h / 2);                                           \
  }

FSADS8_H(32)
FSADS8_H(16)
FSADS8_H(8)

#undef FSADS8_H

#define FSADS4_H(h)                                                          \
  unsigned int aom_sad_skip_4x##h##_neon(                                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,        \
      int ref_stride) {                                                      \
    return 2 * sad4xh_neon(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2, \
                           h / 2);                                           \
  }

FSADS4_H(16)
FSADS4_H(8)

#undef FSADS4_H
