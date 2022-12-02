/*
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
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
#include "aom_ports/mem.h"

#if !defined(__aarch64__)
static INLINE uint32x2_t horizontal_add_u16x8_v(const uint16x8_t a) {
  const uint32x4_t b = vpaddlq_u16(a);
  const uint64x2_t c = vpaddlq_u32(b);
  return vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)),
                  vreinterpret_u32_u64(vget_high_u64(c)));
}
#endif

unsigned int aom_avg_4x4_neon(const uint8_t *a, int a_stride) {
  const uint8x16_t b = load_unaligned_u8q(a, a_stride);
  const uint16x8_t c = vaddl_u8(vget_low_u8(b), vget_high_u8(b));
#if defined(__aarch64__)
  const uint32_t d = vaddlvq_u16(c);
  return (d + 8) >> 4;
#else
  const uint32x2_t d = horizontal_add_u16x8_v(c);
  return vget_lane_u32(vrshr_n_u32(d, 4), 0);
#endif
}

unsigned int aom_avg_8x8_neon(const uint8_t *a, int a_stride) {
  uint16x8_t sum;
  uint8x8_t b = vld1_u8(a);
  a += a_stride;
  uint8x8_t c = vld1_u8(a);
  a += a_stride;
  sum = vaddl_u8(b, c);

  for (int i = 0; i < 6; ++i) {
    const uint8x8_t e = vld1_u8(a);
    a += a_stride;
    sum = vaddw_u8(sum, e);
  }

#if defined(__aarch64__)
  const uint32_t d = vaddlvq_u16(sum);
  return (d + 32) >> 6;
#else
  const uint32x2_t d = horizontal_add_u16x8_v(sum);
  return vget_lane_u32(vrshr_n_u32(d, 6), 0);
#endif
}

void aom_avg_8x8_quad_neon(const uint8_t *s, int p, int x16_idx, int y16_idx,
                           int *avg) {
  for (int k = 0; k < 4; k++) {
    const int x8_idx = x16_idx + ((k & 1) << 3);
    const int y8_idx = y16_idx + ((k >> 1) << 3);
    const uint8_t *s_tmp = s + y8_idx * p + x8_idx;
    avg[k] = aom_avg_8x8_neon(s_tmp, p);
  }
}

int aom_satd_lp_neon(const int16_t *coeff, int length) {
  const int16x4_t zero = vdup_n_s16(0);
  int32x4_t accum = vdupq_n_s32(0);

  do {
    const int16x8_t src0 = vld1q_s16(coeff);
    const int16x8_t src8 = vld1q_s16(coeff + 8);
    accum = vabal_s16(accum, vget_low_s16(src0), zero);
    accum = vabal_s16(accum, vget_high_s16(src0), zero);
    accum = vabal_s16(accum, vget_low_s16(src8), zero);
    accum = vabal_s16(accum, vget_high_s16(src8), zero);
    length -= 16;
    coeff += 16;
  } while (length != 0);

  return horizontal_add_s32x4(accum);
}

void aom_int_pro_row_neon(int16_t hbuf[16], const uint8_t *ref,
                          const int ref_stride, const int height) {
  int i;
  const uint8_t *idx = ref;
  uint16x8_t vec0 = vdupq_n_u16(0);
  uint16x8_t vec1 = vec0;
  uint8x16_t tmp;

  for (i = 0; i < height; ++i) {
    tmp = vld1q_u8(idx);
    idx += ref_stride;
    vec0 = vaddw_u8(vec0, vget_low_u8(tmp));
    vec1 = vaddw_u8(vec1, vget_high_u8(tmp));
  }

  if (128 == height) {
    vec0 = vshrq_n_u16(vec0, 6);
    vec1 = vshrq_n_u16(vec1, 6);
  } else if (64 == height) {
    vec0 = vshrq_n_u16(vec0, 5);
    vec1 = vshrq_n_u16(vec1, 5);
  } else if (32 == height) {
    vec0 = vshrq_n_u16(vec0, 4);
    vec1 = vshrq_n_u16(vec1, 4);
  } else if (16 == height) {
    vec0 = vshrq_n_u16(vec0, 3);
    vec1 = vshrq_n_u16(vec1, 3);
  }

  vst1q_s16(hbuf, vreinterpretq_s16_u16(vec0));
  hbuf += 8;
  vst1q_s16(hbuf, vreinterpretq_s16_u16(vec1));
}

int16_t aom_int_pro_col_neon(const uint8_t *ref, const int width) {
  const uint8_t *idx;
  uint16x8_t sum = vdupq_n_u16(0);

  for (idx = ref; idx < (ref + width); idx += 16) {
    uint8x16_t vec = vld1q_u8(idx);
    sum = vaddq_u16(sum, vpaddlq_u8(vec));
  }

#if defined(__aarch64__)
  return (int16_t)vaddvq_u16(sum);
#else
  const uint32x4_t a = vpaddlq_u16(sum);
  const uint64x2_t b = vpaddlq_u32(a);
  const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                vreinterpret_u32_u64(vget_high_u64(b)));
  return (int16_t)vget_lane_u32(c, 0);
#endif
}

// coeff: 16 bits, dynamic range [-32640, 32640].
// length: value range {16, 64, 256, 1024}.
int aom_satd_neon(const tran_low_t *coeff, int length) {
  const int32x4_t zero = vdupq_n_s32(0);
  int32x4_t accum = zero;
  do {
    const int32x4_t src0 = vld1q_s32(&coeff[0]);
    const int32x4_t src8 = vld1q_s32(&coeff[4]);
    const int32x4_t src16 = vld1q_s32(&coeff[8]);
    const int32x4_t src24 = vld1q_s32(&coeff[12]);
    accum = vabaq_s32(accum, src0, zero);
    accum = vabaq_s32(accum, src8, zero);
    accum = vabaq_s32(accum, src16, zero);
    accum = vabaq_s32(accum, src24, zero);
    length -= 16;
    coeff += 16;
  } while (length != 0);

  // satd: 26 bits, dynamic range [-32640 * 1024, 32640 * 1024]
  return horizontal_add_s32x4(accum);
}

int aom_vector_var_neon(const int16_t *ref, const int16_t *src, const int bwl) {
  int32x4_t v_mean = vdupq_n_s32(0);
  int32x4_t v_sse = v_mean;
  int16x8_t v_ref, v_src;
  int16x4_t v_low;

  int i, width = 4 << bwl;
  for (i = 0; i < width; i += 8) {
    v_ref = vld1q_s16(&ref[i]);
    v_src = vld1q_s16(&src[i]);
    const int16x8_t diff = vsubq_s16(v_ref, v_src);
    // diff: dynamic range [-510, 510], 10 bits.
    v_mean = vpadalq_s16(v_mean, diff);
    v_low = vget_low_s16(diff);
    v_sse = vmlal_s16(v_sse, v_low, v_low);
#if defined(__aarch64__)
    v_sse = vmlal_high_s16(v_sse, diff, diff);
#else
    const int16x4_t v_high = vget_high_s16(diff);
    v_sse = vmlal_s16(v_sse, v_high, v_high);
#endif
  }
  int mean = horizontal_add_s32x4(v_mean);
  int sse = horizontal_add_s32x4(v_sse);
  // (mean * mean): dynamic range 31 bits.
  int var = sse - ((mean * mean) >> (bwl + 2));
  return var;
}

#if CONFIG_AV1_HIGHBITDEPTH
unsigned int aom_highbd_avg_4x4_neon(const uint8_t *s, int p) {
  const uint16_t *src = CONVERT_TO_SHORTPTR(s);
  const uint16x4_t r0 = vld1_u16(src);
  src += p;
  uint16x4_t r1, r2, r3;
  r1 = vld1_u16(src);
  src += p;
  r2 = vld1_u16(src);
  src += p;
  r3 = vld1_u16(src);
  const uint16x4_t s1 = vadd_u16(r0, r1);
  const uint16x4_t s2 = vadd_u16(r2, r3);
  const uint16x4_t s3 = vadd_u16(s1, s2);
#if defined(__aarch64__)
  return (vaddv_u16(s3) + 8) >> 4;
#else
  const uint16x4_t h1 = vpadd_u16(s3, s3);
  const uint16x4_t h2 = vpadd_u16(h1, h1);
  const uint16x4_t res = vrshr_n_u16(h2, 4);
  return vget_lane_u16(res, 0);
#endif
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
