/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <arm_neon.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/convolve_neon.h"

static INLINE int16x4_t convolve8_4x4(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x4_t s6, const int16x4_t s7,
                                      const int16_t *filter) {
  int16x4_t sum;

  sum = vmul_n_s16(s0, filter[0]);
  sum = vmla_n_s16(sum, s1, filter[1]);
  sum = vmla_n_s16(sum, s2, filter[2]);
  sum = vmla_n_s16(sum, s5, filter[5]);
  sum = vmla_n_s16(sum, s6, filter[6]);
  sum = vmla_n_s16(sum, s7, filter[7]);
  /* filter[3] can take a max value of 128. So the max value of the result :
   * 128*255 + sum > 16 bits
   */
  sum = vqadd_s16(sum, vmul_n_s16(s3, filter[3]));
  sum = vqadd_s16(sum, vmul_n_s16(s4, filter[4]));

  return sum;
}

static INLINE uint8x8_t convolve8_horiz_8x8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16_t *filter,
    const int16x8_t shift_round_0, const int16x8_t shift_by_bits) {
  int16x8_t sum;

  sum = vmulq_n_s16(s0, filter[0]);
  sum = vmlaq_n_s16(sum, s1, filter[1]);
  sum = vmlaq_n_s16(sum, s2, filter[2]);
  sum = vmlaq_n_s16(sum, s5, filter[5]);
  sum = vmlaq_n_s16(sum, s6, filter[6]);
  sum = vmlaq_n_s16(sum, s7, filter[7]);
  /* filter[3] can take a max value of 128. So the max value of the result :
   * 128*255 + sum > 16 bits
   */
  sum = vqaddq_s16(sum, vmulq_n_s16(s3, filter[3]));
  sum = vqaddq_s16(sum, vmulq_n_s16(s4, filter[4]));

  sum = vqrshlq_s16(sum, shift_round_0);
  sum = vqrshlq_s16(sum, shift_by_bits);

  return vqmovun_s16(sum);
}

#if !defined(__aarch64__)
static INLINE uint8x8_t convolve8_horiz_4x1(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16_t *filter,
    const int16x4_t shift_round_0, const int16x4_t shift_by_bits) {
  int16x4_t sum;

  sum = vmul_n_s16(s0, filter[0]);
  sum = vmla_n_s16(sum, s1, filter[1]);
  sum = vmla_n_s16(sum, s2, filter[2]);
  sum = vmla_n_s16(sum, s5, filter[5]);
  sum = vmla_n_s16(sum, s6, filter[6]);
  sum = vmla_n_s16(sum, s7, filter[7]);
  /* filter[3] can take a max value of 128. So the max value of the result :
   * 128*255 + sum > 16 bits
   */
  sum = vqadd_s16(sum, vmul_n_s16(s3, filter[3]));
  sum = vqadd_s16(sum, vmul_n_s16(s4, filter[4]));

  sum = vqrshl_s16(sum, shift_round_0);
  sum = vqrshl_s16(sum, shift_by_bits);

  return vqmovun_s16(vcombine_s16(sum, sum));
}
#endif  // !defined(__arch64__)

static INLINE uint8x8_t convolve8_vert_8x4(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16_t *filter) {
  int16x8_t sum;

  sum = vmulq_n_s16(s0, filter[0]);
  sum = vmlaq_n_s16(sum, s1, filter[1]);
  sum = vmlaq_n_s16(sum, s2, filter[2]);
  sum = vmlaq_n_s16(sum, s5, filter[5]);
  sum = vmlaq_n_s16(sum, s6, filter[6]);
  sum = vmlaq_n_s16(sum, s7, filter[7]);
  /* filter[3] can take a max value of 128. So the max value of the result :
   * 128*255 + sum > 16 bits
   */
  sum = vqaddq_s16(sum, vmulq_n_s16(s3, filter[3]));
  sum = vqaddq_s16(sum, vmulq_n_s16(s4, filter[4]));

  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE uint16x4_t convolve8_vert_4x4_s32(
    const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
    const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
    const int16x4_t s6, const int16x4_t s7, const int16_t *y_filter,
    const int32x4_t round_shift_vec, const int32x4_t offset_const,
    const int32x4_t sub_const_vec) {
  int32x4_t sum0;
  uint16x4_t res;
  const int32x4_t zero = vdupq_n_s32(0);

  sum0 = vmull_n_s16(s0, y_filter[0]);
  sum0 = vmlal_n_s16(sum0, s1, y_filter[1]);
  sum0 = vmlal_n_s16(sum0, s2, y_filter[2]);
  sum0 = vmlal_n_s16(sum0, s3, y_filter[3]);
  sum0 = vmlal_n_s16(sum0, s4, y_filter[4]);
  sum0 = vmlal_n_s16(sum0, s5, y_filter[5]);
  sum0 = vmlal_n_s16(sum0, s6, y_filter[6]);
  sum0 = vmlal_n_s16(sum0, s7, y_filter[7]);

  sum0 = vaddq_s32(sum0, offset_const);
  sum0 = vqrshlq_s32(sum0, round_shift_vec);
  sum0 = vsubq_s32(sum0, sub_const_vec);
  sum0 = vmaxq_s32(sum0, zero);

  res = vmovn_u32(vreinterpretq_u32_s32(sum0));

  return res;
}

static INLINE uint8x8_t convolve8_vert_8x4_s32(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, const int16x8_t s7, const int16_t *y_filter,
    const int32x4_t round_shift_vec, const int32x4_t offset_const,
    const int32x4_t sub_const_vec, const int16x8_t vec_round_bits) {
  int32x4_t sum0, sum1;
  uint16x8_t res;
  const int32x4_t zero = vdupq_n_s32(0);

  sum0 = vmull_n_s16(vget_low_s16(s0), y_filter[0]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(s1), y_filter[1]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(s2), y_filter[2]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(s3), y_filter[3]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(s4), y_filter[4]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(s5), y_filter[5]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(s6), y_filter[6]);
  sum0 = vmlal_n_s16(sum0, vget_low_s16(s7), y_filter[7]);

  sum1 = vmull_n_s16(vget_high_s16(s0), y_filter[0]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(s1), y_filter[1]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(s2), y_filter[2]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(s3), y_filter[3]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(s4), y_filter[4]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(s5), y_filter[5]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(s6), y_filter[6]);
  sum1 = vmlal_n_s16(sum1, vget_high_s16(s7), y_filter[7]);

  sum0 = vaddq_s32(sum0, offset_const);
  sum1 = vaddq_s32(sum1, offset_const);
  sum0 = vqrshlq_s32(sum0, round_shift_vec);
  sum1 = vqrshlq_s32(sum1, round_shift_vec);
  sum0 = vsubq_s32(sum0, sub_const_vec);
  sum1 = vsubq_s32(sum1, sub_const_vec);
  sum0 = vmaxq_s32(sum0, zero);
  sum1 = vmaxq_s32(sum1, zero);
  res = vcombine_u16(vqmovn_u32(vreinterpretq_u32_s32(sum0)),
                     vqmovn_u32(vreinterpretq_u32_s32(sum1)));

  res = vqrshlq_u16(res, vec_round_bits);

  return vqmovn_u16(res);
}

void av1_convolve_x_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_x,
                            const int subpel_x_qn,
                            ConvolveParams *conv_params) {
  if (filter_params_x->taps > 8) {
    av1_convolve_x_sr_c(src, src_stride, dst, dst_stride, w, h, filter_params_x,
                        subpel_x_qn, conv_params);
    return;
  }
  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;
  const int8_t bits = FILTER_BITS - conv_params->round_0;

  uint8x8_t t0;
#if defined(__aarch64__)
  uint8x8_t t1, t2, t3;
#endif

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));

  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  const int16x8_t shift_round_0 = vdupq_n_s16(-conv_params->round_0);
  const int16x8_t shift_by_bits = vdupq_n_s16(-bits);

  src -= horiz_offset;
#if defined(__aarch64__)
  if (h == 4) {
    uint8x8_t d01, d23;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    int16x8_t d01_temp, d23_temp;

    __builtin_prefetch(src + 0 * src_stride);
    __builtin_prefetch(src + 1 * src_stride);
    __builtin_prefetch(src + 2 * src_stride);
    __builtin_prefetch(src + 3 * src_stride);

    load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    __builtin_prefetch(dst + 0 * dst_stride);
    __builtin_prefetch(dst + 1 * dst_stride);
    __builtin_prefetch(dst + 2 * dst_stride);
    __builtin_prefetch(dst + 3 * dst_stride);
    src += 7;

    do {
      load_u8_8x4(src, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, x_filter);

      d1 = convolve8_4x4(s1, s2, s3, s4, s5, s6, s7, s8, x_filter);

      d2 = convolve8_4x4(s2, s3, s4, s5, s6, s7, s8, s9, x_filter);

      d3 = convolve8_4x4(s3, s4, s5, s6, s7, s8, s9, s10, x_filter);

      d01_temp = vqrshlq_s16(vcombine_s16(d0, d1), shift_round_0);
      d23_temp = vqrshlq_s16(vcombine_s16(d2, d3), shift_round_0);

      d01_temp = vqrshlq_s16(d01_temp, shift_by_bits);
      d23_temp = vqrshlq_s16(d23_temp, shift_by_bits);

      d01 = vqmovun_s16(d01_temp);
      d23 = vqmovun_s16(d23_temp);

      transpose_u8_4x4(&d01, &d23);

      if (w != 2) {
        vst1_lane_u32((uint32_t *)(dst + 0 * dst_stride),  // 00 01 02 03
                      vreinterpret_u32_u8(d01), 0);
        vst1_lane_u32((uint32_t *)(dst + 1 * dst_stride),  // 10 11 12 13
                      vreinterpret_u32_u8(d23), 0);
        vst1_lane_u32((uint32_t *)(dst + 2 * dst_stride),  // 20 21 22 23
                      vreinterpret_u32_u8(d01), 1);
        vst1_lane_u32((uint32_t *)(dst + 3 * dst_stride),  // 30 31 32 33
                      vreinterpret_u32_u8(d23), 1);
      } else {
        vst1_lane_u16((uint16_t *)(dst + 0 * dst_stride),  // 00 01
                      vreinterpret_u16_u8(d01), 0);
        vst1_lane_u16((uint16_t *)(dst + 1 * dst_stride),  // 10 11
                      vreinterpret_u16_u8(d23), 0);
        vst1_lane_u16((uint16_t *)(dst + 2 * dst_stride),  // 20 21
                      vreinterpret_u16_u8(d01), 2);
        vst1_lane_u16((uint16_t *)(dst + 3 * dst_stride),  // 30 31
                      vreinterpret_u16_u8(d23), 2);
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src += 4;
      dst += 4;
      w -= 4;
    } while (w > 0);
  } else {
#endif
    int width;
    const uint8_t *s;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;

#if defined(__aarch64__)
    int16x8_t s8, s9, s10;
    uint8x8_t t4, t5, t6, t7;
#endif

    if (w <= 4) {
#if defined(__aarch64__)
      do {
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        load_u8_8x8(src + 7, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6,
                    &t7);
        src += 8 * src_stride;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        transpose_u8_4x8(&t0, &t1, &t2, &t3, t4, t5, t6, t7);

        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));

        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        t0 = convolve8_horiz_8x8(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                 shift_round_0, shift_by_bits);
        t1 = convolve8_horiz_8x8(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                 shift_round_0, shift_by_bits);
        t2 = convolve8_horiz_8x8(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                 shift_round_0, shift_by_bits);
        t3 = convolve8_horiz_8x8(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                 shift_round_0, shift_by_bits);

        transpose_u8_8x4(&t0, &t1, &t2, &t3);

        if ((w == 4) && (h > 4)) {
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0),
                        0);  // 00 01 02 03
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1),
                        0);  // 10 11 12 13
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t2),
                        0);  // 20 21 22 23
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t3),
                        0);  // 30 31 32 33
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0),
                        1);  // 40 41 42 43
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1),
                        1);  // 50 51 52 53
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t2),
                        1);  // 60 61 62 63
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t3),
                        1);  // 70 71 72 73
          dst += dst_stride;
        } else if ((w == 4) && (h == 2)) {
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0),
                        0);  // 00 01 02 03
          dst += dst_stride;
          vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t1),
                        0);  // 10 11 12 13
          dst += dst_stride;
        } else if ((w == 2) && (h > 4)) {
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0),
                        0);  // 00 01
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1),
                        0);  // 10 11
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t2),
                        0);  // 20 21
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t3),
                        0);  // 30 31
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0),
                        2);  // 40 41
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1),
                        2);  // 50 51
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t2),
                        2);  // 60 61
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t3),
                        2);  // 70 71
          dst += dst_stride;
        } else if ((w == 2) && (h == 2)) {
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0),
                        0);  // 00 01
          dst += dst_stride;
          vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t1),
                        0);  // 10 11
          dst += dst_stride;
        }
        h -= 8;
      } while (h > 0);
#else
    int16x8_t tt0;
    int16x4_t x0, x1, x2, x3, x4, x5, x6, x7;
    const int16x4_t shift_round_0_low = vget_low_s16(shift_round_0);
    const int16x4_t shift_by_bits_low = vget_low_s16(shift_by_bits);
    do {
      t0 = vld1_u8(src);  // a0 a1 a2 a3 a4 a5 a6 a7
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      x0 = vget_low_s16(tt0);   // a0 a1 a2 a3
      x4 = vget_high_s16(tt0);  // a4 a5 a6 a7

      t0 = vld1_u8(src + 8);  // a8 a9 a10 a11 a12 a13 a14 a15
      tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      x7 = vget_low_s16(tt0);  // a8 a9 a10 a11

      x1 = vext_s16(x0, x4, 1);  // a1 a2 a3 a4
      x2 = vext_s16(x0, x4, 2);  // a2 a3 a4 a5
      x3 = vext_s16(x0, x4, 3);  // a3 a4 a5 a6
      x5 = vext_s16(x4, x7, 1);  // a5 a6 a7 a8
      x6 = vext_s16(x4, x7, 2);  // a6 a7 a8 a9
      x7 = vext_s16(x4, x7, 3);  // a7 a8 a9 a10

      src += src_stride;

      t0 = convolve8_horiz_4x1(x0, x1, x2, x3, x4, x5, x6, x7, x_filter,
                               shift_round_0_low, shift_by_bits_low);

      if (w == 4) {
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(t0),
                      0);  // 00 01 02 03
        dst += dst_stride;
      } else if (w == 2) {
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(t0), 0);  // 00 01
        dst += dst_stride;
      }
      h -= 1;
    } while (h > 0);
#endif
    } else {
      uint8_t *d;
      int16x8_t s11;
#if defined(__aarch64__)
      int16x8_t s12, s13, s14;
      do {
        __builtin_prefetch(src + 0 * src_stride);
        __builtin_prefetch(src + 1 * src_stride);
        __builtin_prefetch(src + 2 * src_stride);
        __builtin_prefetch(src + 3 * src_stride);
        __builtin_prefetch(src + 4 * src_stride);
        __builtin_prefetch(src + 5 * src_stride);
        __builtin_prefetch(src + 6 * src_stride);
        __builtin_prefetch(src + 7 * src_stride);
        load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
        s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

        width = w;
        s = src + 7;
        d = dst;
        __builtin_prefetch(dst + 0 * dst_stride);
        __builtin_prefetch(dst + 1 * dst_stride);
        __builtin_prefetch(dst + 2 * dst_stride);
        __builtin_prefetch(dst + 3 * dst_stride);
        __builtin_prefetch(dst + 4 * dst_stride);
        __builtin_prefetch(dst + 5 * dst_stride);
        __builtin_prefetch(dst + 6 * dst_stride);
        __builtin_prefetch(dst + 7 * dst_stride);

        do {
          load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
          s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
          s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
          s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
          s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
          s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
          s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
          s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

          t0 = convolve8_horiz_8x8(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                   shift_round_0, shift_by_bits);

          t1 = convolve8_horiz_8x8(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                   shift_round_0, shift_by_bits);

          t2 = convolve8_horiz_8x8(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                   shift_round_0, shift_by_bits);

          t3 = convolve8_horiz_8x8(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                   shift_round_0, shift_by_bits);

          t4 = convolve8_horiz_8x8(s4, s5, s6, s7, s8, s9, s10, s11, x_filter,
                                   shift_round_0, shift_by_bits);

          t5 = convolve8_horiz_8x8(s5, s6, s7, s8, s9, s10, s11, s12, x_filter,
                                   shift_round_0, shift_by_bits);

          t6 = convolve8_horiz_8x8(s6, s7, s8, s9, s10, s11, s12, s13, x_filter,
                                   shift_round_0, shift_by_bits);

          t7 = convolve8_horiz_8x8(s7, s8, s9, s10, s11, s12, s13, s14,
                                   x_filter, shift_round_0, shift_by_bits);

          transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
          if (h != 2) {
            store_u8_8x8(d, dst_stride, t0, t1, t2, t3, t4, t5, t6, t7);
          } else {
            store_row2_u8_8x8(d, dst_stride, t0, t1);
          }
          s0 = s8;
          s1 = s9;
          s2 = s10;
          s3 = s11;
          s4 = s12;
          s5 = s13;
          s6 = s14;
          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src += 8 * src_stride;
        dst += 8 * dst_stride;
        h -= 8;
      } while (h > 0);
#else
    do {
      t0 = vld1_u8(src);  // a0 a1 a2 a3 a4 a5 a6 a7
      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));

      width = w;
      s = src + 8;
      d = dst;
      __builtin_prefetch(dst);

      do {
        t0 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s11 = s0;
        s0 = s7;

        s1 = vextq_s16(s11, s7, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
        s2 = vextq_s16(s11, s7, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
        s3 = vextq_s16(s11, s7, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
        s4 = vextq_s16(s11, s7, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
        s5 = vextq_s16(s11, s7, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
        s6 = vextq_s16(s11, s7, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
        s7 = vextq_s16(s11, s7, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

        t0 = convolve8_horiz_8x8(s11, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                 shift_round_0, shift_by_bits);
        vst1_u8(d, t0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src += src_stride;
      dst += dst_stride;
      h -= 1;
    } while (h > 0);
#endif
    }
#if defined(__aarch64__)
  }
#endif
}

void av1_convolve_y_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_y,
                            const int subpel_y_qn) {
  if (filter_params_y->taps > 8) {
    av1_convolve_y_sr_c(src, src_stride, dst, dst_stride, w, h, filter_params_y,
                        subpel_y_qn);
    return;
  }
  const int vert_offset = filter_params_y->taps / 2 - 1;

  src -= vert_offset * src_stride;

  const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (w <= 4) {
    uint8x8_t d01;
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, d0;
#if defined(__aarch64__)
    uint8x8_t d23;
    int16x4_t s8, s9, s10, d1, d2, d3;
#endif
    s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;
    s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
    src += src_stride;

    do {
      s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
#if defined(__aarch64__)
      s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;
      s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(vld1_u8(src))));
      src += src_stride;

      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(dst + 1 * dst_stride);
      __builtin_prefetch(dst + 2 * dst_stride);
      __builtin_prefetch(dst + 3 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      d0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      d1 = convolve8_4x4(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
      d2 = convolve8_4x4(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
      d3 = convolve8_4x4(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);
      if ((w == 4) && (h != 2)) {
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01),
                      0);  // 00 01 02 03
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01),
                      1);  // 10 11 12 13
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d23),
                      0);  // 20 21 22 23
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d23),
                      1);  // 30 31 32 33
        dst += dst_stride;
      } else if ((w == 4) && (h == 2)) {
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01),
                      0);  // 00 01 02 03
        dst += dst_stride;
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01),
                      1);  // 10 11 12 13
        dst += dst_stride;
      } else if ((w == 2) && (h != 2)) {
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 0);  // 00 01
        dst += dst_stride;
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 2);  // 10 11
        dst += dst_stride;
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d23), 0);  // 20 21
        dst += dst_stride;
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d23), 2);  // 30 31
        dst += dst_stride;
      } else if ((w == 2) && (h == 2)) {
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 0);  // 00 01
        dst += dst_stride;
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 2);  // 10 11
        dst += dst_stride;
      }
      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      h -= 4;
#else
      __builtin_prefetch(dst + 0 * dst_stride);
      __builtin_prefetch(src + 0 * src_stride);

      d0 = convolve8_4x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);

      d01 = vqrshrun_n_s16(vcombine_s16(d0, d0), FILTER_BITS);

      if (w == 4) {
        vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d01), 0);
        dst += dst_stride;
      } else if (w == 2) {
        vst1_lane_u16((uint16_t *)dst, vreinterpret_u16_u8(d01), 0);
        dst += dst_stride;
      }
      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s5 = s6;
      s6 = s7;
      h -= 1;
#endif
    } while (h > 0);
  } else {
    int height;
    const uint8_t *s;
    uint8_t *d;
    uint8x8_t t0;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
#if defined(__aarch64__)
    uint8x8_t t1, t2, t3;
    int16x8_t s8, s9, s10;
#endif
    do {
      __builtin_prefetch(src + 0 * src_stride);
      __builtin_prefetch(src + 1 * src_stride);
      __builtin_prefetch(src + 2 * src_stride);
      __builtin_prefetch(src + 3 * src_stride);
      __builtin_prefetch(src + 4 * src_stride);
      __builtin_prefetch(src + 5 * src_stride);
      __builtin_prefetch(src + 6 * src_stride);
      s = src;
      s0 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s2 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s3 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s4 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      s6 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
      s += src_stride;
      d = dst;
      height = h;

      do {
        s7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
#if defined(__aarch64__)
        s8 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s9 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;
        s10 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));
        s += src_stride;

        __builtin_prefetch(d + 0 * dst_stride);
        __builtin_prefetch(d + 1 * dst_stride);
        __builtin_prefetch(d + 2 * dst_stride);
        __builtin_prefetch(d + 3 * dst_stride);
        __builtin_prefetch(s + 0 * src_stride);
        __builtin_prefetch(s + 1 * src_stride);
        __builtin_prefetch(s + 2 * src_stride);
        __builtin_prefetch(s + 3 * src_stride);
        t0 = convolve8_vert_8x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
        t1 = convolve8_vert_8x4(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
        t2 = convolve8_vert_8x4(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
        t3 = convolve8_vert_8x4(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);
        if (h != 2) {
          vst1_u8(d, t0);
          d += dst_stride;
          vst1_u8(d, t1);
          d += dst_stride;
          vst1_u8(d, t2);
          d += dst_stride;
          vst1_u8(d, t3);
          d += dst_stride;
        } else {
          vst1_u8(d, t0);
          d += dst_stride;
          vst1_u8(d, t1);
          d += dst_stride;
        }
        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        height -= 4;
#else
        __builtin_prefetch(d);
        __builtin_prefetch(s);

        t0 = convolve8_vert_8x4(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);

        vst1_u8(d, t0);
        d += dst_stride;

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s5 = s6;
        s6 = s7;
        height -= 1;
#endif
      } while (height > 0);
      src += 8;
      dst += 8;
      w -= 8;
    } while (w > 0);
  }
}

// Horizontal filtering for convolve_2d_sr for width multiple of 8
// Processes one row at a time
static INLINE void horiz_filter_w8_single_row(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int width, int height, const int16_t *x_filter,
    const int16x8_t horiz_const, const int16x8_t shift_round_0) {
  int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
  do {
    uint8x8_t t0 = vld1_u8(src_ptr);
    s0 = vreinterpretq_s16_u16(vmovl_u8(t0));  // a0 a1 a2 a3 a4 a5 a6 a7

    int width_tmp = width;
    const uint8_t *s = src_ptr + 8;
    int16_t *dst_tmp = dst_ptr;

    __builtin_prefetch(dst_ptr);

    do {
      t0 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
      s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t sum = s0;
      s0 = s7;

      s1 = vextq_s16(sum, s7, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
      s2 = vextq_s16(sum, s7, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
      s3 = vextq_s16(sum, s7, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
      s4 = vextq_s16(sum, s7, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
      s5 = vextq_s16(sum, s7, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
      s6 = vextq_s16(sum, s7, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
      s7 = vextq_s16(sum, s7, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

      int16x8_t res0 = convolve8_8x8_s16(sum, s1, s2, s3, s4, s5, s6, s7,
                                         x_filter, horiz_const, shift_round_0);

      vst1q_s16(dst_tmp, res0);

      s += 8;
      dst_tmp += 8;
      width_tmp -= 8;
    } while (width_tmp > 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
    height--;
  } while (height > 0);
}

// Horizontal filtering for convolve_2d_sr for width <= 4
// Processes one row at a time
static INLINE void horiz_filter_w4_single_row(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int width, int height, const int16_t *x_filter,
    const int16x4_t horiz_const, const int16x4_t shift_round_0) {
  int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
  do {
    const uint8_t *s = src_ptr;

    __builtin_prefetch(s);

    uint8x8_t t0 = vld1_u8(s);  // a0 a1 a2 a3 a4 a5 a6 a7
    int16x8_t tt0 = vreinterpretq_s16_u16(vmovl_u8(t0));
    s0 = vget_low_s16(tt0);
    s4 = vget_high_s16(tt0);

    __builtin_prefetch(dst_ptr);
    s += 8;

    t0 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
    s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));

    s1 = vext_s16(s0, s4, 1);  // a1 a2 a3 a4
    s2 = vext_s16(s0, s4, 2);  // a2 a3 a4 a5
    s3 = vext_s16(s0, s4, 3);  // a3 a4 a5 a6
    s5 = vext_s16(s4, s7, 1);  // a5 a6 a7 a8
    s6 = vext_s16(s4, s7, 2);  // a6 a7 a8 a9
    s7 = vext_s16(s4, s7, 3);  // a7 a8 a9 a10

    int16x4_t d0 = convolve8_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                     horiz_const, shift_round_0);

    if (width == 4) {
      vst1_s16(dst_ptr, d0);
      dst_ptr += dst_stride;
    } else if (width == 2) {
      vst1_lane_u32((uint32_t *)dst_ptr, vreinterpret_u32_s16(d0), 0);
      dst_ptr += dst_stride;
    }

    src_ptr += src_stride;
    height--;
  } while (height > 0);
}

void av1_convolve_2d_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int subpel_y_qn,
                             ConvolveParams *conv_params) {
  if (filter_params_x->taps > 8) {
    av1_convolve_2d_sr_c(src, src_stride, dst, dst_stride, w, h,
                         filter_params_x, filter_params_y, subpel_x_qn,
                         subpel_y_qn, conv_params);
    return;
  }
  int im_dst_stride;
  int width, height;
#if defined(__aarch64__)
  uint8x8_t t0;
  uint8x8_t t1, t2, t3, t4, t5, t6, t7;
  const uint8_t *s;
#endif

  DECLARE_ALIGNED(16, int16_t,
                  im_block[(MAX_SB_SIZE + HORIZ_EXTRA_ROWS) * MAX_SB_SIZE]);

  const int bd = 8;
  const int im_h = h + filter_params_y->taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = filter_params_y->taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;

  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  int16_t *dst_ptr;

  dst_ptr = im_block;
  im_dst_stride = im_stride;
  height = im_h;
  width = w;

  const int16_t round_bits =
      FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
  const int16x8_t vec_round_bits = vdupq_n_s16(-round_bits);
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  int16_t x_filter_tmp[8];
  int16x8_t filter_x_coef = vld1q_s16(x_filter);

  // filter coeffs are even, so downshifting by 1 to reduce intermediate
  // precision requirements.
  filter_x_coef = vshrq_n_s16(filter_x_coef, 1);
  vst1q_s16(&x_filter_tmp[0], filter_x_coef);

  assert(conv_params->round_0 > 0);

  if (w <= 4) {
    const int16x4_t horiz_const = vdup_n_s16((1 << (bd + FILTER_BITS - 2)));
    const int16x4_t shift_round_0 = vdup_n_s16(-(conv_params->round_0 - 1));

#if defined(__aarch64__)
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, d0, d1, d2, d3;
    do {
      assert(height >= 4);
      s = src_ptr;
      __builtin_prefetch(s + 0 * src_stride);
      __builtin_prefetch(s + 1 * src_stride);
      __builtin_prefetch(s + 2 * src_stride);
      __builtin_prefetch(s + 3 * src_stride);

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
      s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

      __builtin_prefetch(dst_ptr + 0 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 1 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 2 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 3 * im_dst_stride);
      s += 7;

      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      d0 = convolve8_4x4_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_tmp,
                             horiz_const, shift_round_0);
      d1 = convolve8_4x4_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter_tmp,
                             horiz_const, shift_round_0);
      d2 = convolve8_4x4_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter_tmp,
                             horiz_const, shift_round_0);
      d3 = convolve8_4x4_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter_tmp,
                             horiz_const, shift_round_0);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);
      if (w == 4) {
        vst1_s16((dst_ptr + 0 * im_dst_stride), d0);
        vst1_s16((dst_ptr + 1 * im_dst_stride), d1);
        vst1_s16((dst_ptr + 2 * im_dst_stride), d2);
        vst1_s16((dst_ptr + 3 * im_dst_stride), d3);
      } else if (w == 2) {
        vst1_lane_u32((uint32_t *)(dst_ptr + 0 * im_dst_stride),
                      vreinterpret_u32_s16(d0), 0);
        vst1_lane_u32((uint32_t *)(dst_ptr + 1 * im_dst_stride),
                      vreinterpret_u32_s16(d1), 0);
        vst1_lane_u32((uint32_t *)(dst_ptr + 2 * im_dst_stride),
                      vreinterpret_u32_s16(d2), 0);
        vst1_lane_u32((uint32_t *)(dst_ptr + 3 * im_dst_stride),
                      vreinterpret_u32_s16(d3), 0);
      }
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * im_dst_stride;
      height -= 4;
    } while (height >= 4);

    if (height) {
      assert(height < 4);
      horiz_filter_w4_single_row(src_ptr, src_stride, dst_ptr, im_dst_stride, w,
                                 height, x_filter_tmp, horiz_const,
                                 shift_round_0);
    }
#else
    horiz_filter_w4_single_row(src_ptr, src_stride, dst_ptr, im_dst_stride, w,
                               height, x_filter_tmp, horiz_const,
                               shift_round_0);
#endif

  } else {
    const int16x8_t horiz_const = vdupq_n_s16((1 << (bd + FILTER_BITS - 2)));
    const int16x8_t shift_round_0 = vdupq_n_s16(-(conv_params->round_0 - 1));

#if defined(__aarch64__)
    int16_t *d_tmp;
    int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14;
    int16x8_t res0, res1, res2, res3, res4, res5, res6, res7;
    do {
      assert(height >= 8);
      __builtin_prefetch(src_ptr + 0 * src_stride);
      __builtin_prefetch(src_ptr + 1 * src_stride);
      __builtin_prefetch(src_ptr + 2 * src_stride);
      __builtin_prefetch(src_ptr + 3 * src_stride);
      __builtin_prefetch(src_ptr + 4 * src_stride);
      __builtin_prefetch(src_ptr + 5 * src_stride);
      __builtin_prefetch(src_ptr + 6 * src_stride);
      __builtin_prefetch(src_ptr + 7 * src_stride);

      load_u8_8x8(src_ptr, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      width = w;
      s = src_ptr + 7;
      d_tmp = dst_ptr;

      __builtin_prefetch(dst_ptr + 0 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 1 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 2 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 3 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 4 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 5 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 6 * im_dst_stride);
      __builtin_prefetch(dst_ptr + 7 * im_dst_stride);

      do {
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

        s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
        s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
        s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
        s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
        s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

        res0 = convolve8_8x8_s16(s0, s1, s2, s3, s4, s5, s6, s7, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res1 = convolve8_8x8_s16(s1, s2, s3, s4, s5, s6, s7, s8, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res2 = convolve8_8x8_s16(s2, s3, s4, s5, s6, s7, s8, s9, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res3 = convolve8_8x8_s16(s3, s4, s5, s6, s7, s8, s9, s10, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res4 = convolve8_8x8_s16(s4, s5, s6, s7, s8, s9, s10, s11, x_filter_tmp,
                                 horiz_const, shift_round_0);
        res5 = convolve8_8x8_s16(s5, s6, s7, s8, s9, s10, s11, s12,
                                 x_filter_tmp, horiz_const, shift_round_0);
        res6 = convolve8_8x8_s16(s6, s7, s8, s9, s10, s11, s12, s13,
                                 x_filter_tmp, horiz_const, shift_round_0);
        res7 = convolve8_8x8_s16(s7, s8, s9, s10, s11, s12, s13, s14,
                                 x_filter_tmp, horiz_const, shift_round_0);

        transpose_s16_8x8(&res0, &res1, &res2, &res3, &res4, &res5, &res6,
                          &res7);

        store_s16_8x8(d_tmp, im_dst_stride, res0, res1, res2, res3, res4, res5,
                      res6, res7);

        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += 8 * src_stride;
      dst_ptr += 8 * im_dst_stride;
      height -= 8;
    } while (height >= 8);

    if (height >= 4) {
      assert(height < 8);
      int16x4_t reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8, reg9,
          reg10, reg11, reg12, reg13, reg14;
      int16x4_t d0, d1, d2, d3, d4, d5, d6, d7;
      int16x8_t out0, out1, out2, out3;

      __builtin_prefetch(src_ptr + 0 * src_stride);
      __builtin_prefetch(src_ptr + 1 * src_stride);
      __builtin_prefetch(src_ptr + 2 * src_stride);
      __builtin_prefetch(src_ptr + 3 * src_stride);

      load_u8_8x4(src_ptr, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      reg0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      reg1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      reg2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      reg3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
      reg4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      reg5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      reg6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);

      s = src_ptr + 7;
      d_tmp = dst_ptr;
      width = w;

      do {
        load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
        transpose_u8_8x4(&t0, &t1, &t2, &t3);

        reg7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
        reg8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
        reg9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
        reg10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
        reg11 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
        reg12 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
        reg13 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
        reg14 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

        d0 = convolve8_4x4(reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7,
                           x_filter_tmp);

        d1 = convolve8_4x4(reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8,
                           x_filter_tmp);

        d2 = convolve8_4x4(reg2, reg3, reg4, reg5, reg6, reg7, reg8, reg9,
                           x_filter_tmp);

        d3 = convolve8_4x4(reg3, reg4, reg5, reg6, reg7, reg8, reg9, reg10,
                           x_filter_tmp);

        d4 = convolve8_4x4(reg4, reg5, reg6, reg7, reg8, reg9, reg10, reg11,
                           x_filter_tmp);

        d5 = convolve8_4x4(reg5, reg6, reg7, reg8, reg9, reg10, reg11, reg12,
                           x_filter_tmp);

        d6 = convolve8_4x4(reg6, reg7, reg8, reg9, reg10, reg11, reg12, reg13,
                           x_filter_tmp);

        d7 = convolve8_4x4(reg7, reg8, reg9, reg10, reg11, reg12, reg13, reg14,
                           x_filter_tmp);

        transpose_s16_4x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7, &out0, &out1,
                          &out2, &out3);

        out0 = vaddq_s16(out0, horiz_const);
        out0 = vqrshlq_s16(out0, shift_round_0);

        out1 = vaddq_s16(out1, horiz_const);
        out1 = vqrshlq_s16(out1, shift_round_0);

        out2 = vaddq_s16(out2, horiz_const);
        out2 = vqrshlq_s16(out2, shift_round_0);

        out3 = vaddq_s16(out3, horiz_const);
        out3 = vqrshlq_s16(out3, shift_round_0);

        store_s16_8x4(d_tmp, im_dst_stride, out0, out1, out2, out3);

        reg0 = reg8;
        reg1 = reg9;
        reg2 = reg10;
        reg3 = reg11;
        reg4 = reg12;
        reg5 = reg13;
        reg6 = reg14;
        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * im_dst_stride;
      height -= 4;
    }

    if (height) {
      assert(height < 4);
      horiz_filter_w8_single_row(src_ptr, src_stride, dst_ptr, im_stride, w,
                                 height, x_filter_tmp, horiz_const,
                                 shift_round_0);
    }
#else

    horiz_filter_w8_single_row(src_ptr, src_stride, dst_ptr, im_stride, w,
                               height, x_filter_tmp, horiz_const,
                               shift_round_0);
#endif
  }

  // vertical
  {
    uint8_t *dst_u8_ptr, *d_u8;
    int16_t *v_src_ptr, *v_s;

    const int32_t sub_const = (1 << (offset_bits - conv_params->round_1)) +
                              (1 << (offset_bits - conv_params->round_1 - 1));
    const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(
        filter_params_y, subpel_y_qn & SUBPEL_MASK);

    const int32x4_t round_shift_vec = vdupq_n_s32(-(conv_params->round_1));
    const int32x4_t offset_const = vdupq_n_s32(1 << offset_bits);
    const int32x4_t sub_const_vec = vdupq_n_s32(sub_const);

    src_stride = im_stride;
    v_src_ptr = im_block;
    dst_u8_ptr = dst;

    height = h;
    width = w;

    if (width <= 4) {
      int16x4_t s0, s1, s2, s3, s4, s5, s6, s7;
      uint16x4_t d0;
      uint16x8_t dd0;
      uint8x8_t d01;

#if defined(__aarch64__)
      int16x4_t s8, s9, s10;
      uint16x4_t d1, d2, d3;
      uint16x8_t dd1;
      uint8x8_t d23;
#endif

      d_u8 = dst_u8_ptr;
      v_s = v_src_ptr;

      __builtin_prefetch(v_s + 0 * im_stride);
      __builtin_prefetch(v_s + 1 * im_stride);
      __builtin_prefetch(v_s + 2 * im_stride);
      __builtin_prefetch(v_s + 3 * im_stride);
      __builtin_prefetch(v_s + 4 * im_stride);
      __builtin_prefetch(v_s + 5 * im_stride);
      __builtin_prefetch(v_s + 6 * im_stride);
      __builtin_prefetch(v_s + 7 * im_stride);

      load_s16_4x8(v_s, im_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);
      v_s += (7 * im_stride);

      do {
#if defined(__aarch64__)
        load_s16_4x4(v_s, im_stride, &s7, &s8, &s9, &s10);
        v_s += (im_stride << 2);

        __builtin_prefetch(d_u8 + 0 * dst_stride);
        __builtin_prefetch(d_u8 + 1 * dst_stride);
        __builtin_prefetch(d_u8 + 2 * dst_stride);
        __builtin_prefetch(d_u8 + 3 * dst_stride);

        d0 = convolve8_vert_4x4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                    round_shift_vec, offset_const,
                                    sub_const_vec);
        d1 = convolve8_vert_4x4_s32(s1, s2, s3, s4, s5, s6, s7, s8, y_filter,
                                    round_shift_vec, offset_const,
                                    sub_const_vec);
        d2 = convolve8_vert_4x4_s32(s2, s3, s4, s5, s6, s7, s8, s9, y_filter,
                                    round_shift_vec, offset_const,
                                    sub_const_vec);
        d3 = convolve8_vert_4x4_s32(s3, s4, s5, s6, s7, s8, s9, s10, y_filter,
                                    round_shift_vec, offset_const,
                                    sub_const_vec);

        dd0 = vqrshlq_u16(vcombine_u16(d0, d1), vec_round_bits);
        dd1 = vqrshlq_u16(vcombine_u16(d2, d3), vec_round_bits);

        d01 = vqmovn_u16(dd0);
        d23 = vqmovn_u16(dd1);

        if ((w == 4) && (h != 2)) {
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(d01),
                        0);  // 00 01 02 03
          d_u8 += dst_stride;
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(d01),
                        1);  // 10 11 12 13
          d_u8 += dst_stride;
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(d23),
                        0);  // 20 21 22 23
          d_u8 += dst_stride;
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(d23),
                        1);  // 30 31 32 33
          d_u8 += dst_stride;
        } else if ((w == 2) && (h != 2)) {
          vst1_lane_u16((uint16_t *)d_u8, vreinterpret_u16_u8(d01),
                        0);  // 00 01
          d_u8 += dst_stride;
          vst1_lane_u16((uint16_t *)d_u8, vreinterpret_u16_u8(d01),
                        2);  // 10 11
          d_u8 += dst_stride;
          vst1_lane_u16((uint16_t *)d_u8, vreinterpret_u16_u8(d23),
                        0);  // 20 21
          d_u8 += dst_stride;
          vst1_lane_u16((uint16_t *)d_u8, vreinterpret_u16_u8(d23),
                        2);  // 30 31
          d_u8 += dst_stride;
        } else if ((w == 4) && (h == 2)) {
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(d01),
                        0);  // 00 01 02 03
          d_u8 += dst_stride;
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(d01),
                        1);  // 10 11 12 13
          d_u8 += dst_stride;
        } else if ((w == 2) && (h == 2)) {
          vst1_lane_u16((uint16_t *)d_u8, vreinterpret_u16_u8(d01),
                        0);  // 00 01
          d_u8 += dst_stride;
          vst1_lane_u16((uint16_t *)d_u8, vreinterpret_u16_u8(d01),
                        2);  // 10 11
          d_u8 += dst_stride;
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        height -= 4;
#else
        s7 = vld1_s16(v_s);
        v_s += im_stride;

        __builtin_prefetch(d_u8 + 0 * dst_stride);

        d0 = convolve8_vert_4x4_s32(s0, s1, s2, s3, s4, s5, s6, s7, y_filter,
                                    round_shift_vec, offset_const,
                                    sub_const_vec);

        dd0 = vqrshlq_u16(vcombine_u16(d0, d0), vec_round_bits);
        d01 = vqmovn_u16(dd0);

        if (w == 4) {
          vst1_lane_u32((uint32_t *)d_u8, vreinterpret_u32_u8(d01),
                        0);  // 00 01 02 03
          d_u8 += dst_stride;

        } else if (w == 2) {
          vst1_lane_u16((uint16_t *)d_u8, vreinterpret_u16_u8(d01),
                        0);  // 00 01
          d_u8 += dst_stride;
        }

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s5 = s6;
        s6 = s7;
        height -= 1;
#endif
      } while (height > 0);
    } else {
      // if width is a multiple of 8 & height is a multiple of 4
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
      uint8x8_t res0;
#if defined(__aarch64__)
      int16x8_t s8, s9, s10;
      uint8x8_t res1, res2, res3;
#endif

      do {
        __builtin_prefetch(v_src_ptr + 0 * im_stride);
        __builtin_prefetch(v_src_ptr + 1 * im_stride);
        __builtin_prefetch(v_src_ptr + 2 * im_stride);
        __builtin_prefetch(v_src_ptr + 3 * im_stride);
        __builtin_prefetch(v_src_ptr + 4 * im_stride);
        __builtin_prefetch(v_src_ptr + 5 * im_stride);
        __builtin_prefetch(v_src_ptr + 6 * im_stride);
        __builtin_prefetch(v_src_ptr + 7 * im_stride);

        v_s = v_src_ptr;
        load_s16_8x8(v_s, im_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7);
        v_s += (7 * im_stride);

        d_u8 = dst_u8_ptr;
        height = h;

        do {
#if defined(__aarch64__)
          load_s16_8x4(v_s, im_stride, &s7, &s8, &s9, &s10);
          v_s += (im_stride << 2);

          __builtin_prefetch(d_u8 + 4 * dst_stride);
          __builtin_prefetch(d_u8 + 5 * dst_stride);
          __builtin_prefetch(d_u8 + 6 * dst_stride);
          __builtin_prefetch(d_u8 + 7 * dst_stride);

          res0 = convolve8_vert_8x4_s32(s0, s1, s2, s3, s4, s5, s6, s7,
                                        y_filter, round_shift_vec, offset_const,
                                        sub_const_vec, vec_round_bits);
          res1 = convolve8_vert_8x4_s32(s1, s2, s3, s4, s5, s6, s7, s8,
                                        y_filter, round_shift_vec, offset_const,
                                        sub_const_vec, vec_round_bits);
          res2 = convolve8_vert_8x4_s32(s2, s3, s4, s5, s6, s7, s8, s9,
                                        y_filter, round_shift_vec, offset_const,
                                        sub_const_vec, vec_round_bits);
          res3 = convolve8_vert_8x4_s32(s3, s4, s5, s6, s7, s8, s9, s10,
                                        y_filter, round_shift_vec, offset_const,
                                        sub_const_vec, vec_round_bits);

          if (h != 2) {
            vst1_u8(d_u8, res0);
            d_u8 += dst_stride;
            vst1_u8(d_u8, res1);
            d_u8 += dst_stride;
            vst1_u8(d_u8, res2);
            d_u8 += dst_stride;
            vst1_u8(d_u8, res3);
            d_u8 += dst_stride;
          } else {
            vst1_u8(d_u8, res0);
            d_u8 += dst_stride;
            vst1_u8(d_u8, res1);
            d_u8 += dst_stride;
          }
          s0 = s4;
          s1 = s5;
          s2 = s6;
          s3 = s7;
          s4 = s8;
          s5 = s9;
          s6 = s10;
          height -= 4;
#else
          s7 = vld1q_s16(v_s);
          v_s += im_stride;

          __builtin_prefetch(d_u8 + 0 * dst_stride);

          res0 = convolve8_vert_8x4_s32(s0, s1, s2, s3, s4, s5, s6, s7,
                                        y_filter, round_shift_vec, offset_const,
                                        sub_const_vec, vec_round_bits);

          vst1_u8(d_u8, res0);
          d_u8 += dst_stride;

          s0 = s1;
          s1 = s2;
          s2 = s3;
          s3 = s4;
          s4 = s5;
          s5 = s6;
          s6 = s7;
          height -= 1;
#endif
        } while (height > 0);
        v_src_ptr += 8;
        dst_u8_ptr += 8;
        w -= 8;
      } while (w > 0);
    }
  }
}

static INLINE void scaledconvolve_horiz_w4(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const x_filters,
    const int x0_q4, const int x_step_q4, const int w, const int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[4 * 4]);
  int x, y, z;

  src -= SUBPEL_TAPS / 2 - 1;

  y = h;
  do {
    int x_q4 = x0_q4;
    x = 0;
    do {
      // process 4 src_x steps
      for (z = 0; z < 4; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
        if (x_q4 & SUBPEL_MASK) {
          const int16x8_t filters = vld1q_s16(x_filters[x_q4 & SUBPEL_MASK]);
          const int16x4_t filter3 = vdup_lane_s16(vget_low_s16(filters), 3);
          const int16x4_t filter4 = vdup_lane_s16(vget_high_s16(filters), 0);
          uint8x8_t s[8], d;
          int16x8_t ss[4];
          int16x4_t t[8], tt;

          load_u8_8x4(src_x, src_stride, &s[0], &s[1], &s[2], &s[3]);
          transpose_u8_8x4(&s[0], &s[1], &s[2], &s[3]);

          ss[0] = vreinterpretq_s16_u16(vmovl_u8(s[0]));
          ss[1] = vreinterpretq_s16_u16(vmovl_u8(s[1]));
          ss[2] = vreinterpretq_s16_u16(vmovl_u8(s[2]));
          ss[3] = vreinterpretq_s16_u16(vmovl_u8(s[3]));
          t[0] = vget_low_s16(ss[0]);
          t[1] = vget_low_s16(ss[1]);
          t[2] = vget_low_s16(ss[2]);
          t[3] = vget_low_s16(ss[3]);
          t[4] = vget_high_s16(ss[0]);
          t[5] = vget_high_s16(ss[1]);
          t[6] = vget_high_s16(ss[2]);
          t[7] = vget_high_s16(ss[3]);

          tt = convolve8_4(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7],
                           filters, filter3, filter4);
          d = vqrshrun_n_s16(vcombine_s16(tt, tt), 7);
          vst1_lane_u32((uint32_t *)&temp[4 * z], vreinterpret_u32_u8(d), 0);
        } else {
          int i;
          for (i = 0; i < 4; ++i) {
            temp[z * 4 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      // transpose the 4x4 filters values back to dst
      {
        const uint8x8x4_t d4 = vld4_u8(temp);
        vst1_lane_u32((uint32_t *)&dst[x + 0 * dst_stride],
                      vreinterpret_u32_u8(d4.val[0]), 0);
        vst1_lane_u32((uint32_t *)&dst[x + 1 * dst_stride],
                      vreinterpret_u32_u8(d4.val[1]), 0);
        vst1_lane_u32((uint32_t *)&dst[x + 2 * dst_stride],
                      vreinterpret_u32_u8(d4.val[2]), 0);
        vst1_lane_u32((uint32_t *)&dst[x + 3 * dst_stride],
                      vreinterpret_u32_u8(d4.val[3]), 0);
      }
      x += 4;
    } while (x < w);

    src += src_stride * 4;
    dst += dst_stride * 4;
    y -= 4;
  } while (y > 0);
}

static INLINE void scaledconvolve_horiz_w8(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const x_filters,
    const int x0_q4, const int x_step_q4, const int w, const int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[8 * 8]);
  int x, y, z;
  src -= SUBPEL_TAPS / 2 - 1;

  // This function processes 8x8 areas. The intermediate height is not always
  // a multiple of 8, so force it to be a multiple of 8 here.
  y = (h + 7) & ~7;

  do {
    int x_q4 = x0_q4;
    x = 0;
    do {
      uint8x8_t d[8];
      // process 8 src_x steps
      for (z = 0; z < 8; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];

        if (x_q4 & SUBPEL_MASK) {
          const int16x8_t filters = vld1q_s16(x_filters[x_q4 & SUBPEL_MASK]);
          uint8x8_t s[8];
          load_u8_8x8(src_x, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4],
                      &s[5], &s[6], &s[7]);
          transpose_u8_8x8(&s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                           &s[7]);
          d[0] = scale_filter_8(s, filters);
          vst1_u8(&temp[8 * z], d[0]);
        } else {
          int i;
          for (i = 0; i < 8; ++i) {
            temp[z * 8 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      // transpose the 8x8 filters values back to dst
      load_u8_8x8(temp, 8, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6],
                  &d[7]);
      transpose_u8_8x8(&d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]);
      vst1_u8(&dst[x + 0 * dst_stride], d[0]);
      vst1_u8(&dst[x + 1 * dst_stride], d[1]);
      vst1_u8(&dst[x + 2 * dst_stride], d[2]);
      vst1_u8(&dst[x + 3 * dst_stride], d[3]);
      vst1_u8(&dst[x + 4 * dst_stride], d[4]);
      vst1_u8(&dst[x + 5 * dst_stride], d[5]);
      vst1_u8(&dst[x + 6 * dst_stride], d[6]);
      vst1_u8(&dst[x + 7 * dst_stride], d[7]);
      x += 8;
    } while (x < w);

    src += src_stride * 8;
    dst += dst_stride * 8;
  } while (y -= 8);
}

static INLINE void scaledconvolve_vert_w4(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];

    if (y_q4 & SUBPEL_MASK) {
      const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
      const int16x4_t filter3 = vdup_lane_s16(vget_low_s16(filters), 3);
      const int16x4_t filter4 = vdup_lane_s16(vget_high_s16(filters), 0);
      uint8x8_t s[8], d;
      int16x4_t t[8], tt;

      load_u8_8x8(src_y, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                  &s[6], &s[7]);
      t[0] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[0])));
      t[1] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[1])));
      t[2] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[2])));
      t[3] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[3])));
      t[4] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[4])));
      t[5] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[5])));
      t[6] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[6])));
      t[7] = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(s[7])));

      tt = convolve8_4(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], filters,
                       filter3, filter4);
      d = vqrshrun_n_s16(vcombine_s16(tt, tt), 7);
      vst1_lane_u32((uint32_t *)dst, vreinterpret_u32_u8(d), 0);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }

    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

static INLINE void scaledconvolve_vert_w8(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    if (y_q4 & SUBPEL_MASK) {
      const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
      uint8x8_t s[8], d;
      load_u8_8x8(src_y, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                  &s[6], &s[7]);
      d = scale_filter_8(s, filters);
      vst1_u8(dst, d);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }
    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

static INLINE void scaledconvolve_vert_w16(
    const uint8_t *src, const ptrdiff_t src_stride, uint8_t *dst,
    const ptrdiff_t dst_stride, const InterpKernel *const y_filters,
    const int y0_q4, const int y_step_q4, const int w, const int h) {
  int x, y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  y = h;
  do {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    if (y_q4 & SUBPEL_MASK) {
      x = 0;
      do {
        const int16x8_t filters = vld1q_s16(y_filters[y_q4 & SUBPEL_MASK]);
        uint8x16_t ss[8];
        uint8x8_t s[8], d[2];
        load_u8_16x8(src_y, src_stride, &ss[0], &ss[1], &ss[2], &ss[3], &ss[4],
                     &ss[5], &ss[6], &ss[7]);
        s[0] = vget_low_u8(ss[0]);
        s[1] = vget_low_u8(ss[1]);
        s[2] = vget_low_u8(ss[2]);
        s[3] = vget_low_u8(ss[3]);
        s[4] = vget_low_u8(ss[4]);
        s[5] = vget_low_u8(ss[5]);
        s[6] = vget_low_u8(ss[6]);
        s[7] = vget_low_u8(ss[7]);
        d[0] = scale_filter_8(s, filters);

        s[0] = vget_high_u8(ss[0]);
        s[1] = vget_high_u8(ss[1]);
        s[2] = vget_high_u8(ss[2]);
        s[3] = vget_high_u8(ss[3]);
        s[4] = vget_high_u8(ss[4]);
        s[5] = vget_high_u8(ss[5]);
        s[6] = vget_high_u8(ss[6]);
        s[7] = vget_high_u8(ss[7]);
        d[1] = scale_filter_8(s, filters);
        vst1q_u8(&dst[x], vcombine_u8(d[0], d[1]));
        src_y += 16;
        x += 16;
      } while (x < w);
    } else {
      memcpy(dst, &src_y[3 * src_stride], w);
    }
    dst += dst_stride;
    y_q4 += y_step_q4;
  } while (--y);
}

void aom_scaled_2d_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst,
                        ptrdiff_t dst_stride, const InterpKernel *filter,
                        int x0_q4, int x_step_q4, int y0_q4, int y_step_q4,
                        int w, int h) {
  // Note: Fixed size intermediate buffer, temp, places limits on parameters.
  // 2d filtering proceeds in 2 steps:
  //   (1) Interpolate horizontally into an intermediate buffer, temp.
  //   (2) Interpolate temp vertically to derive the sub-pixel result.
  // Deriving the maximum number of rows in the temp buffer (135):
  // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
  // --Largest block size is 64x64 pixels.
  // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
  //   original frame (in 1/16th pixel units).
  // --Must round-up because block may be located at sub-pixel position.
  // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
  // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
  // --Require an additional 8 rows for the horiz_w8 transpose tail.
  // When calling in frame scaling function, the smallest scaling factor is x1/4
  // ==> y_step_q4 = 64. Since w and h are at most 16, the temp buffer is still
  // big enough.
  DECLARE_ALIGNED(16, uint8_t, temp[(135 + 8) * 64]);
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 32 || (y_step_q4 <= 64 && h <= 32));
  assert(x_step_q4 <= 64);

  if (w >= 8) {
    scaledconvolve_horiz_w8(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                            src_stride, temp, 64, filter, x0_q4, x_step_q4, w,
                            intermediate_height);
  } else {
    scaledconvolve_horiz_w4(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                            src_stride, temp, 64, filter, x0_q4, x_step_q4, w,
                            intermediate_height);
  }

  if (w >= 16) {
    scaledconvolve_vert_w16(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                            dst_stride, filter, y0_q4, y_step_q4, w, h);
  } else if (w == 8) {
    scaledconvolve_vert_w8(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                           dst_stride, filter, y0_q4, y_step_q4, w, h);
  } else {
    scaledconvolve_vert_w4(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                           dst_stride, filter, y0_q4, y_step_q4, w, h);
  }
}
