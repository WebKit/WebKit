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
#include "aom_dsp/arm/sum_neon.h"
#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

// w * h must be less than 2048 or local variable v_sum may overflow.
static void variance_neon_w8(const uint8_t *a, int a_stride, const uint8_t *b,
                             int b_stride, int w, int h, uint32_t *sse,
                             int *sum) {
  int i, j;
  int16x8_t v_sum = vdupq_n_s16(0);
  int32x4_t v_sse_lo = vdupq_n_s32(0);
  int32x4_t v_sse_hi = vdupq_n_s32(0);

  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 8) {
      const uint8x8_t v_a = vld1_u8(&a[j]);
      const uint8x8_t v_b = vld1_u8(&b[j]);
      const uint16x8_t v_diff = vsubl_u8(v_a, v_b);
      const int16x8_t sv_diff = vreinterpretq_s16_u16(v_diff);
      v_sum = vaddq_s16(v_sum, sv_diff);
      v_sse_lo =
          vmlal_s16(v_sse_lo, vget_low_s16(sv_diff), vget_low_s16(sv_diff));
      v_sse_hi =
          vmlal_s16(v_sse_hi, vget_high_s16(sv_diff), vget_high_s16(sv_diff));
    }
    a += a_stride;
    b += b_stride;
  }

  *sum = horizontal_add_s16x8(v_sum);
  *sse = (unsigned int)horizontal_add_s32x4(vaddq_s32(v_sse_lo, v_sse_hi));
}

void aom_get8x8var_neon(const uint8_t *a, int a_stride, const uint8_t *b,
                        int b_stride, unsigned int *sse, int *sum) {
  variance_neon_w8(a, a_stride, b, b_stride, 8, 8, sse, sum);
}

void aom_get16x16var_neon(const uint8_t *a, int a_stride, const uint8_t *b,
                          int b_stride, unsigned int *sse, int *sum) {
  variance_neon_w8(a, a_stride, b, b_stride, 16, 16, sse, sum);
}

// TODO(yunqingwang): Perform variance of two/four 8x8 blocks similar to that of
// AVX2.
void aom_get_sse_sum_8x8_quad_neon(const uint8_t *a, int a_stride,
                                   const uint8_t *b, int b_stride,
                                   unsigned int *sse, int *sum) {
  // Loop over 4 8x8 blocks. Process one 8x32 block.
  for (int k = 0; k < 4; k++) {
    variance_neon_w8(a + (k * 8), a_stride, b + (k * 8), b_stride, 8, 8,
                     &sse[k], &sum[k]);
  }
}

unsigned int aom_variance8x8_neon(const uint8_t *a, int a_stride,
                                  const uint8_t *b, int b_stride,
                                  unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, 8, 8, sse, &sum);
  return *sse - ((sum * sum) >> 6);
}

unsigned int aom_variance16x16_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, 16, 16, sse, &sum);
  return *sse - (((unsigned int)((int64_t)sum * sum)) >> 8);
}

unsigned int aom_variance32x32_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum;
  variance_neon_w8(a, a_stride, b, b_stride, 32, 32, sse, &sum);
  return *sse - (unsigned int)(((int64_t)sum * sum) >> 10);
}

unsigned int aom_variance32x64_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;
  variance_neon_w8(a, a_stride, b, b_stride, 32, 32, &sse1, &sum1);
  variance_neon_w8(a + (32 * a_stride), a_stride, b + (32 * b_stride), b_stride,
                   32, 32, &sse2, &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (unsigned int)(((int64_t)sum1 * sum1) >> 11);
}

unsigned int aom_variance64x32_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;
  variance_neon_w8(a, a_stride, b, b_stride, 64, 16, &sse1, &sum1);
  variance_neon_w8(a + (16 * a_stride), a_stride, b + (16 * b_stride), b_stride,
                   64, 16, &sse2, &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (unsigned int)(((int64_t)sum1 * sum1) >> 11);
}

unsigned int aom_variance64x64_neon(const uint8_t *a, int a_stride,
                                    const uint8_t *b, int b_stride,
                                    unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;

  variance_neon_w8(a, a_stride, b, b_stride, 64, 16, &sse1, &sum1);
  variance_neon_w8(a + (16 * a_stride), a_stride, b + (16 * b_stride), b_stride,
                   64, 16, &sse2, &sum2);
  sse1 += sse2;
  sum1 += sum2;

  variance_neon_w8(a + (16 * 2 * a_stride), a_stride, b + (16 * 2 * b_stride),
                   b_stride, 64, 16, &sse2, &sum2);
  sse1 += sse2;
  sum1 += sum2;

  variance_neon_w8(a + (16 * 3 * a_stride), a_stride, b + (16 * 3 * b_stride),
                   b_stride, 64, 16, &sse2, &sum2);
  *sse = sse1 + sse2;
  sum1 += sum2;
  return *sse - (unsigned int)(((int64_t)sum1 * sum1) >> 12);
}

unsigned int aom_variance128x128_neon(const uint8_t *a, int a_stride,
                                      const uint8_t *b, int b_stride,
                                      unsigned int *sse) {
  int sum1, sum2;
  uint32_t sse1, sse2;
  sum1 = sse1 = 0;
  for (int i = 0; i < 16; i++) {
    variance_neon_w8(a + (8 * i * a_stride), a_stride, b + (8 * i * b_stride),
                     b_stride, 128, 8, &sse2, &sum2);
    sse1 += sse2;
    sum1 += sum2;
  }

  *sse = sse1;

  return *sse - (unsigned int)(((int64_t)sum1 * sum1) >> 14);
}

unsigned int aom_variance16x8_neon(const unsigned char *src_ptr,
                                   int source_stride,
                                   const unsigned char *ref_ptr,
                                   int recon_stride, unsigned int *sse) {
  int i;
  int16x4_t d22s16, d23s16, d24s16, d25s16, d26s16, d27s16, d28s16, d29s16;
  uint32x2_t d0u32, d10u32;
  int64x1_t d0s64, d1s64;
  uint8x16_t q0u8, q1u8, q2u8, q3u8;
  uint16x8_t q11u16, q12u16, q13u16, q14u16;
  int32x4_t q8s32, q9s32, q10s32;
  int64x2_t q0s64, q1s64, q5s64;

  q8s32 = vdupq_n_s32(0);
  q9s32 = vdupq_n_s32(0);
  q10s32 = vdupq_n_s32(0);

  for (i = 0; i < 4; i++) {
    q0u8 = vld1q_u8(src_ptr);
    src_ptr += source_stride;
    q1u8 = vld1q_u8(src_ptr);
    src_ptr += source_stride;
    __builtin_prefetch(src_ptr);

    q2u8 = vld1q_u8(ref_ptr);
    ref_ptr += recon_stride;
    q3u8 = vld1q_u8(ref_ptr);
    ref_ptr += recon_stride;
    __builtin_prefetch(ref_ptr);

    q11u16 = vsubl_u8(vget_low_u8(q0u8), vget_low_u8(q2u8));
    q12u16 = vsubl_u8(vget_high_u8(q0u8), vget_high_u8(q2u8));
    q13u16 = vsubl_u8(vget_low_u8(q1u8), vget_low_u8(q3u8));
    q14u16 = vsubl_u8(vget_high_u8(q1u8), vget_high_u8(q3u8));

    d22s16 = vreinterpret_s16_u16(vget_low_u16(q11u16));
    d23s16 = vreinterpret_s16_u16(vget_high_u16(q11u16));
    q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q11u16));
    q9s32 = vmlal_s16(q9s32, d22s16, d22s16);
    q10s32 = vmlal_s16(q10s32, d23s16, d23s16);

    d24s16 = vreinterpret_s16_u16(vget_low_u16(q12u16));
    d25s16 = vreinterpret_s16_u16(vget_high_u16(q12u16));
    q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q12u16));
    q9s32 = vmlal_s16(q9s32, d24s16, d24s16);
    q10s32 = vmlal_s16(q10s32, d25s16, d25s16);

    d26s16 = vreinterpret_s16_u16(vget_low_u16(q13u16));
    d27s16 = vreinterpret_s16_u16(vget_high_u16(q13u16));
    q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q13u16));
    q9s32 = vmlal_s16(q9s32, d26s16, d26s16);
    q10s32 = vmlal_s16(q10s32, d27s16, d27s16);

    d28s16 = vreinterpret_s16_u16(vget_low_u16(q14u16));
    d29s16 = vreinterpret_s16_u16(vget_high_u16(q14u16));
    q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q14u16));
    q9s32 = vmlal_s16(q9s32, d28s16, d28s16);
    q10s32 = vmlal_s16(q10s32, d29s16, d29s16);
  }

  q10s32 = vaddq_s32(q10s32, q9s32);
  q0s64 = vpaddlq_s32(q8s32);
  q1s64 = vpaddlq_s32(q10s32);

  d0s64 = vadd_s64(vget_low_s64(q0s64), vget_high_s64(q0s64));
  d1s64 = vadd_s64(vget_low_s64(q1s64), vget_high_s64(q1s64));

  q5s64 = vmull_s32(vreinterpret_s32_s64(d0s64), vreinterpret_s32_s64(d0s64));
  vst1_lane_u32((uint32_t *)sse, vreinterpret_u32_s64(d1s64), 0);

  d10u32 = vshr_n_u32(vreinterpret_u32_s64(vget_low_s64(q5s64)), 7);
  d0u32 = vsub_u32(vreinterpret_u32_s64(d1s64), d10u32);

  return vget_lane_u32(d0u32, 0);
}

unsigned int aom_variance8x16_neon(const unsigned char *src_ptr,
                                   int source_stride,
                                   const unsigned char *ref_ptr,
                                   int recon_stride, unsigned int *sse) {
  int i;
  uint8x8_t d0u8, d2u8, d4u8, d6u8;
  int16x4_t d22s16, d23s16, d24s16, d25s16;
  uint32x2_t d0u32, d10u32;
  int64x1_t d0s64, d1s64;
  uint16x8_t q11u16, q12u16;
  int32x4_t q8s32, q9s32, q10s32;
  int64x2_t q0s64, q1s64, q5s64;

  q8s32 = vdupq_n_s32(0);
  q9s32 = vdupq_n_s32(0);
  q10s32 = vdupq_n_s32(0);

  for (i = 0; i < 8; i++) {
    d0u8 = vld1_u8(src_ptr);
    src_ptr += source_stride;
    d2u8 = vld1_u8(src_ptr);
    src_ptr += source_stride;
    __builtin_prefetch(src_ptr);

    d4u8 = vld1_u8(ref_ptr);
    ref_ptr += recon_stride;
    d6u8 = vld1_u8(ref_ptr);
    ref_ptr += recon_stride;
    __builtin_prefetch(ref_ptr);

    q11u16 = vsubl_u8(d0u8, d4u8);
    q12u16 = vsubl_u8(d2u8, d6u8);

    d22s16 = vreinterpret_s16_u16(vget_low_u16(q11u16));
    d23s16 = vreinterpret_s16_u16(vget_high_u16(q11u16));
    q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q11u16));
    q9s32 = vmlal_s16(q9s32, d22s16, d22s16);
    q10s32 = vmlal_s16(q10s32, d23s16, d23s16);

    d24s16 = vreinterpret_s16_u16(vget_low_u16(q12u16));
    d25s16 = vreinterpret_s16_u16(vget_high_u16(q12u16));
    q8s32 = vpadalq_s16(q8s32, vreinterpretq_s16_u16(q12u16));
    q9s32 = vmlal_s16(q9s32, d24s16, d24s16);
    q10s32 = vmlal_s16(q10s32, d25s16, d25s16);
  }

  q10s32 = vaddq_s32(q10s32, q9s32);
  q0s64 = vpaddlq_s32(q8s32);
  q1s64 = vpaddlq_s32(q10s32);

  d0s64 = vadd_s64(vget_low_s64(q0s64), vget_high_s64(q0s64));
  d1s64 = vadd_s64(vget_low_s64(q1s64), vget_high_s64(q1s64));

  q5s64 = vmull_s32(vreinterpret_s32_s64(d0s64), vreinterpret_s32_s64(d0s64));
  vst1_lane_u32((uint32_t *)sse, vreinterpret_u32_s64(d1s64), 0);

  d10u32 = vshr_n_u32(vreinterpret_u32_s64(vget_low_s64(q5s64)), 7);
  d0u32 = vsub_u32(vreinterpret_u32_s64(d1s64), d10u32);

  return vget_lane_u32(d0u32, 0);
}

unsigned int aom_mse16x16_neon(const unsigned char *src_ptr, int source_stride,
                               const unsigned char *ref_ptr, int recon_stride,
                               unsigned int *sse) {
  int i;
  int16x4_t d22s16, d23s16, d24s16, d25s16, d26s16, d27s16, d28s16, d29s16;
  int64x1_t d0s64;
  uint8x16_t q0u8, q1u8, q2u8, q3u8;
  int32x4_t q7s32, q8s32, q9s32, q10s32;
  uint16x8_t q11u16, q12u16, q13u16, q14u16;
  int64x2_t q1s64;

  q7s32 = vdupq_n_s32(0);
  q8s32 = vdupq_n_s32(0);
  q9s32 = vdupq_n_s32(0);
  q10s32 = vdupq_n_s32(0);

  for (i = 0; i < 8; i++) {  // mse16x16_neon_loop
    q0u8 = vld1q_u8(src_ptr);
    src_ptr += source_stride;
    q1u8 = vld1q_u8(src_ptr);
    src_ptr += source_stride;
    q2u8 = vld1q_u8(ref_ptr);
    ref_ptr += recon_stride;
    q3u8 = vld1q_u8(ref_ptr);
    ref_ptr += recon_stride;

    q11u16 = vsubl_u8(vget_low_u8(q0u8), vget_low_u8(q2u8));
    q12u16 = vsubl_u8(vget_high_u8(q0u8), vget_high_u8(q2u8));
    q13u16 = vsubl_u8(vget_low_u8(q1u8), vget_low_u8(q3u8));
    q14u16 = vsubl_u8(vget_high_u8(q1u8), vget_high_u8(q3u8));

    d22s16 = vreinterpret_s16_u16(vget_low_u16(q11u16));
    d23s16 = vreinterpret_s16_u16(vget_high_u16(q11u16));
    q7s32 = vmlal_s16(q7s32, d22s16, d22s16);
    q8s32 = vmlal_s16(q8s32, d23s16, d23s16);

    d24s16 = vreinterpret_s16_u16(vget_low_u16(q12u16));
    d25s16 = vreinterpret_s16_u16(vget_high_u16(q12u16));
    q9s32 = vmlal_s16(q9s32, d24s16, d24s16);
    q10s32 = vmlal_s16(q10s32, d25s16, d25s16);

    d26s16 = vreinterpret_s16_u16(vget_low_u16(q13u16));
    d27s16 = vreinterpret_s16_u16(vget_high_u16(q13u16));
    q7s32 = vmlal_s16(q7s32, d26s16, d26s16);
    q8s32 = vmlal_s16(q8s32, d27s16, d27s16);

    d28s16 = vreinterpret_s16_u16(vget_low_u16(q14u16));
    d29s16 = vreinterpret_s16_u16(vget_high_u16(q14u16));
    q9s32 = vmlal_s16(q9s32, d28s16, d28s16);
    q10s32 = vmlal_s16(q10s32, d29s16, d29s16);
  }

  q7s32 = vaddq_s32(q7s32, q8s32);
  q9s32 = vaddq_s32(q9s32, q10s32);
  q10s32 = vaddq_s32(q7s32, q9s32);

  q1s64 = vpaddlq_s32(q10s32);
  d0s64 = vadd_s64(vget_low_s64(q1s64), vget_high_s64(q1s64));

  vst1_lane_u32((uint32_t *)sse, vreinterpret_u32_s64(d0s64), 0);
  return vget_lane_u32(vreinterpret_u32_s64(d0s64), 0);
}

unsigned int aom_get4x4sse_cs_neon(const unsigned char *src_ptr,
                                   int source_stride,
                                   const unsigned char *ref_ptr,
                                   int recon_stride) {
  int16x4_t d22s16, d24s16, d26s16, d28s16;
  int64x1_t d0s64;
  uint8x8_t d0u8, d1u8, d2u8, d3u8, d4u8, d5u8, d6u8, d7u8;
  int32x4_t q7s32, q8s32, q9s32, q10s32;
  uint16x8_t q11u16, q12u16, q13u16, q14u16;
  int64x2_t q1s64;

  d0u8 = vld1_u8(src_ptr);
  src_ptr += source_stride;
  d4u8 = vld1_u8(ref_ptr);
  ref_ptr += recon_stride;
  d1u8 = vld1_u8(src_ptr);
  src_ptr += source_stride;
  d5u8 = vld1_u8(ref_ptr);
  ref_ptr += recon_stride;
  d2u8 = vld1_u8(src_ptr);
  src_ptr += source_stride;
  d6u8 = vld1_u8(ref_ptr);
  ref_ptr += recon_stride;
  d3u8 = vld1_u8(src_ptr);
  d7u8 = vld1_u8(ref_ptr);

  q11u16 = vsubl_u8(d0u8, d4u8);
  q12u16 = vsubl_u8(d1u8, d5u8);
  q13u16 = vsubl_u8(d2u8, d6u8);
  q14u16 = vsubl_u8(d3u8, d7u8);

  d22s16 = vget_low_s16(vreinterpretq_s16_u16(q11u16));
  d24s16 = vget_low_s16(vreinterpretq_s16_u16(q12u16));
  d26s16 = vget_low_s16(vreinterpretq_s16_u16(q13u16));
  d28s16 = vget_low_s16(vreinterpretq_s16_u16(q14u16));

  q7s32 = vmull_s16(d22s16, d22s16);
  q8s32 = vmull_s16(d24s16, d24s16);
  q9s32 = vmull_s16(d26s16, d26s16);
  q10s32 = vmull_s16(d28s16, d28s16);

  q7s32 = vaddq_s32(q7s32, q8s32);
  q9s32 = vaddq_s32(q9s32, q10s32);
  q9s32 = vaddq_s32(q7s32, q9s32);

  q1s64 = vpaddlq_s32(q9s32);
  d0s64 = vadd_s64(vget_low_s64(q1s64), vget_high_s64(q1s64));

  return vget_lane_u32(vreinterpret_u32_s64(d0s64), 0);
}

// Load 4 sets of 4 bytes when alignment is not guaranteed.
static INLINE uint8x16_t load_unaligned_u8q(const uint8_t *buf, int stride) {
  uint32_t a;
  uint32x4_t a_u32 = vdupq_n_u32(0);
  if (stride == 4) return vld1q_u8(buf);
  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vld1q_lane_u32(&a, a_u32, 0);
  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vld1q_lane_u32(&a, a_u32, 1);
  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vld1q_lane_u32(&a, a_u32, 2);
  memcpy(&a, buf, 4);
  buf += stride;
  a_u32 = vld1q_lane_u32(&a, a_u32, 3);
  return vreinterpretq_u8_u32(a_u32);
}

// The variance helper functions use int16_t for sum. 8 values are accumulated
// and then added (at which point they expand up to int32_t). To avoid overflow,
// there can be no more than 32767 / 255 ~= 128 values accumulated in each
// column. For a 32x32 buffer, this results in 32 / 8 = 4 values per row * 32
// rows = 128. Asserts have been added to each function to warn against reaching
// this limit.

// Process a block of width 4 four rows at a time.
static void variance_neon_w4x4(const uint8_t *a, int a_stride, const uint8_t *b,
                               int b_stride, int h, uint32_t *sse, int *sum) {
  const int32x4_t zero = vdupq_n_s32(0);
  int16x8_t sum_s16 = vreinterpretq_s16_s32(zero);
  int32x4_t sse_s32 = zero;

  // Since width is only 4, sum_s16 only loads a half row per loop.
  assert(h <= 256);

  int i;
  for (i = 0; i < h; i += 4) {
    const uint8x16_t a_u8 = load_unaligned_u8q(a, a_stride);
    const uint8x16_t b_u8 = load_unaligned_u8q(b, b_stride);
    const int16x8_t diff_lo_s16 =
        vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(a_u8), vget_low_u8(b_u8)));
    const int16x8_t diff_hi_s16 =
        vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(a_u8), vget_high_u8(b_u8)));

    sum_s16 = vaddq_s16(sum_s16, diff_lo_s16);
    sum_s16 = vaddq_s16(sum_s16, diff_hi_s16);

    sse_s32 = vmlal_s16(sse_s32, vget_low_s16(diff_lo_s16),
                        vget_low_s16(diff_lo_s16));
    sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff_lo_s16),
                        vget_high_s16(diff_lo_s16));

    sse_s32 = vmlal_s16(sse_s32, vget_low_s16(diff_hi_s16),
                        vget_low_s16(diff_hi_s16));
    sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff_hi_s16),
                        vget_high_s16(diff_hi_s16));

    a += 4 * a_stride;
    b += 4 * b_stride;
  }

  *sum = horizontal_add_s16x8(sum_s16);
  *sse = (uint32_t)horizontal_add_s32x4(sse_s32);
}

// Process a block of any size where the width is divisible by 16.
static void variance_neon_w16(const uint8_t *a, int a_stride, const uint8_t *b,
                              int b_stride, int w, int h, uint32_t *sse,
                              int *sum) {
  const int32x4_t zero = vdupq_n_s32(0);
  int16x8_t sum_s16 = vreinterpretq_s16_s32(zero);
  int32x4_t sse_s32 = zero;

  // The loop loads 16 values at a time but doubles them up when accumulating
  // into sum_s16.
  assert(w / 8 * h <= 128);

  int i, j;
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 16) {
      const uint8x16_t a_u8 = vld1q_u8(a + j);
      const uint8x16_t b_u8 = vld1q_u8(b + j);

      const int16x8_t diff_lo_s16 =
          vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(a_u8), vget_low_u8(b_u8)));
      const int16x8_t diff_hi_s16 = vreinterpretq_s16_u16(
          vsubl_u8(vget_high_u8(a_u8), vget_high_u8(b_u8)));

      sum_s16 = vaddq_s16(sum_s16, diff_lo_s16);
      sum_s16 = vaddq_s16(sum_s16, diff_hi_s16);

      sse_s32 = vmlal_s16(sse_s32, vget_low_s16(diff_lo_s16),
                          vget_low_s16(diff_lo_s16));
      sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff_lo_s16),
                          vget_high_s16(diff_lo_s16));

      sse_s32 = vmlal_s16(sse_s32, vget_low_s16(diff_hi_s16),
                          vget_low_s16(diff_hi_s16));
      sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff_hi_s16),
                          vget_high_s16(diff_hi_s16));
    }
    a += a_stride;
    b += b_stride;
  }

  *sum = horizontal_add_s16x8(sum_s16);
  *sse = (uint32_t)horizontal_add_s32x4(sse_s32);
}

// Process a block of width 8 two rows at a time.
static void variance_neon_w8x2(const uint8_t *a, int a_stride, const uint8_t *b,
                               int b_stride, int h, uint32_t *sse, int *sum) {
  const int32x4_t zero = vdupq_n_s32(0);
  int16x8_t sum_s16 = vreinterpretq_s16_s32(zero);
  int32x4_t sse_s32 = zero;

  // Each column has it's own accumulator entry in sum_s16.
  assert(h <= 128);

  int i = 0;
  do {
    const uint8x8_t a_0_u8 = vld1_u8(a);
    const uint8x8_t a_1_u8 = vld1_u8(a + a_stride);
    const uint8x8_t b_0_u8 = vld1_u8(b);
    const uint8x8_t b_1_u8 = vld1_u8(b + b_stride);
    const int16x8_t diff_0_s16 =
        vreinterpretq_s16_u16(vsubl_u8(a_0_u8, b_0_u8));
    const int16x8_t diff_1_s16 =
        vreinterpretq_s16_u16(vsubl_u8(a_1_u8, b_1_u8));
    sum_s16 = vaddq_s16(sum_s16, diff_0_s16);
    sum_s16 = vaddq_s16(sum_s16, diff_1_s16);
    sse_s32 =
        vmlal_s16(sse_s32, vget_low_s16(diff_0_s16), vget_low_s16(diff_0_s16));
    sse_s32 =
        vmlal_s16(sse_s32, vget_low_s16(diff_1_s16), vget_low_s16(diff_1_s16));
    sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff_0_s16),
                        vget_high_s16(diff_0_s16));
    sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff_1_s16),
                        vget_high_s16(diff_1_s16));
    a += a_stride + a_stride;
    b += b_stride + b_stride;
    i += 2;
  } while (i < h);

  *sum = horizontal_add_s16x8(sum_s16);
  *sse = (uint32_t)horizontal_add_s32x4(sse_s32);
}

#define VARIANCE_NXM(n, m, shift)                                           \
  unsigned int aom_variance##n##x##m##_neon(const uint8_t *a, int a_stride, \
                                            const uint8_t *b, int b_stride, \
                                            unsigned int *sse) {            \
    int sum;                                                                \
    if (n == 4)                                                             \
      variance_neon_w4x4(a, a_stride, b, b_stride, m, sse, &sum);           \
    else if (n == 8)                                                        \
      variance_neon_w8x2(a, a_stride, b, b_stride, m, sse, &sum);           \
    else                                                                    \
      variance_neon_w16(a, a_stride, b, b_stride, n, m, sse, &sum);         \
    if (n * m < 16 * 16)                                                    \
      return *sse - ((sum * sum) >> shift);                                 \
    else                                                                    \
      return *sse - (uint32_t)(((int64_t)sum * sum) >> shift);              \
  }

static void variance_neon_wide_block(const uint8_t *a, int a_stride,
                                     const uint8_t *b, int b_stride, int w,
                                     int h, uint32_t *sse, int *sum) {
  const int32x4_t zero = vdupq_n_s32(0);
  int32x4_t v_diff = zero;
  int64x2_t v_sse = vreinterpretq_s64_s32(zero);

  int s, i, j;
  for (s = 0; s < 16; s++) {
    int32x4_t sse_s32 = zero;
    int16x8_t sum_s16 = vreinterpretq_s16_s32(zero);
    for (i = (s * h) >> 4; i < (((s + 1) * h) >> 4); ++i) {
      for (j = 0; j < w; j += 16) {
        const uint8x16_t a_u8 = vld1q_u8(a + j);
        const uint8x16_t b_u8 = vld1q_u8(b + j);

        const int16x8_t diff_lo_s16 = vreinterpretq_s16_u16(
            vsubl_u8(vget_low_u8(a_u8), vget_low_u8(b_u8)));
        const int16x8_t diff_hi_s16 = vreinterpretq_s16_u16(
            vsubl_u8(vget_high_u8(a_u8), vget_high_u8(b_u8)));

        sum_s16 = vaddq_s16(sum_s16, diff_lo_s16);
        sum_s16 = vaddq_s16(sum_s16, diff_hi_s16);

        sse_s32 = vmlal_s16(sse_s32, vget_low_s16(diff_lo_s16),
                            vget_low_s16(diff_lo_s16));
        sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff_lo_s16),
                            vget_high_s16(diff_lo_s16));
        sse_s32 = vmlal_s16(sse_s32, vget_low_s16(diff_hi_s16),
                            vget_low_s16(diff_hi_s16));
        sse_s32 = vmlal_s16(sse_s32, vget_high_s16(diff_hi_s16),
                            vget_high_s16(diff_hi_s16));
      }

      a += a_stride;
      b += b_stride;
    }

    v_diff = vpadalq_s16(v_diff, sum_s16);
    v_sse = vpadalq_s32(v_sse, sse_s32);
  }
  int diff = horizontal_add_s32x4(v_diff);
#if defined(__aarch64__)
  uint32_t sq = (uint32_t)vaddvq_u64(vreinterpretq_u64_s64(v_sse));
#else
  uint32_t sq = vget_lane_u32(
      vreinterpret_u32_s64(vadd_s64(vget_low_s64(v_sse), vget_high_s64(v_sse))),
      0);
#endif

  *sum = diff;
  *sse = sq;
}

#define VARIANCE_NXM_WIDE(W, H)                                             \
  unsigned int aom_variance##W##x##H##_neon(const uint8_t *a, int a_stride, \
                                            const uint8_t *b, int b_stride, \
                                            uint32_t *sse) {                \
    int sum;                                                                \
    variance_neon_wide_block(a, a_stride, b, b_stride, W, H, sse, &sum);    \
    return *sse - (uint32_t)(((int64_t)sum * sum) / (W * H));               \
  }

VARIANCE_NXM(4, 4, 4)
VARIANCE_NXM(4, 8, 5)
VARIANCE_NXM(8, 4, 5)
VARIANCE_NXM(16, 32, 9)
VARIANCE_NXM(32, 16, 9)
VARIANCE_NXM_WIDE(128, 64)
VARIANCE_NXM_WIDE(64, 128)
