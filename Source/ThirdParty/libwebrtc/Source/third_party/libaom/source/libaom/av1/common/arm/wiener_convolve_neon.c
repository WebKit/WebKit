/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/txfm_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/common.h"

static INLINE uint8x8_t wiener_convolve8_vert_4x8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
    const int16x8_t s6, int16_t *filter_y, const int bd,
    const int round1_bits) {
  int16x8_t ss0, ss1, ss2;
  int32x4_t sum0, sum1;
  int16x8_t tmp;
  uint8x8_t res;

  const int32_t round_const = (1 << (bd + round1_bits - 1));
  const int32x4_t round_bits = vdupq_n_s32(-round1_bits);
  const int32x4_t round_vec = vdupq_n_s32(round_const);
  const int16x4_t filter = vld1_s16(filter_y);

  ss0 = vaddq_s16(s0, s6);
  ss1 = vaddq_s16(s1, s5);
  ss2 = vaddq_s16(s2, s4);

  sum0 = vmull_lane_s16(vget_low_s16(ss0), filter, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(ss1), filter, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(ss2), filter, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), filter, 3);

  sum1 = vmull_lane_s16(vget_high_s16(ss0), filter, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(ss1), filter, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(ss2), filter, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), filter, 3);

  sum0 = vsubq_s32(sum0, round_vec);
  sum1 = vsubq_s32(sum1, round_vec);

  /* right shift & rounding */
  sum0 = vrshlq_s32(sum0, round_bits);
  sum1 = vrshlq_s32(sum1, round_bits);

  /* from int32x4_t to uint8x8_t */
  tmp = vcombine_s16(vmovn_s32(sum0), vmovn_s32(sum1));
  res = vqmovun_s16(tmp);

  return res;
}

static INLINE uint16x8_t wiener_convolve8_horiz_8x8(
    const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
    const int16x8_t s3, int16_t *filter_x, const int bd,
    const int round0_bits) {
  int16x8_t sum;
  uint16x8_t res;
  int32x4_t sum_0, sum_1;
  int32x4_t s3_0, s3_1;
  const int32_t round_const_0 = (1 << (bd + FILTER_BITS - 1));
  const int32_t round_const_1 = (1 << (bd + 1 + FILTER_BITS - round0_bits)) - 1;

  /* for the purpose of right shift by { conv_params->round_0 } */
  const int32x4_t round_bits = vdupq_n_s32(-round0_bits);

  const int32x4_t round_vec_0 = vdupq_n_s32(round_const_0);
  const int32x4_t round_vec_1 = vdupq_n_s32(round_const_1);
  const int16x4_t filter = vld1_s16(filter_x);

  sum = vmulq_lane_s16(s0, filter, 0);
  sum = vmlaq_lane_s16(sum, s1, filter, 1);
  sum = vmlaq_lane_s16(sum, s2, filter, 2);

  /* sum from 16x8 to 2 32x4 registers */
  sum_0 = vmovl_s16(vget_low_s16(sum));
  sum_1 = vmovl_s16(vget_high_s16(sum));

  /* s[3]*128 -- and filter coef max can be 128
   *  then max value possible = 128*128*255 exceeding 16 bit
   */

  s3_0 = vmull_lane_s16(vget_low_s16(s3), filter, 3);
  s3_1 = vmull_lane_s16(vget_high_s16(s3), filter, 3);
  sum_0 = vaddq_s32(sum_0, s3_0);
  sum_1 = vaddq_s32(sum_1, s3_1);

  /* Add the constant value */
  sum_0 = vaddq_s32(sum_0, round_vec_0);
  sum_1 = vaddq_s32(sum_1, round_vec_0);

  /* right shift & rounding & saturating */
  sum_0 = vqrshlq_s32(sum_0, round_bits);
  sum_1 = vqrshlq_s32(sum_1, round_bits);

  /* Clipping to max value */
  sum_0 = vminq_s32(sum_0, round_vec_1);
  sum_1 = vminq_s32(sum_1, round_vec_1);

  res = vcombine_u16(vqmovun_s32(sum_0), vqmovun_s32(sum_1));
  return res;
}

#define HORZ_FILTERING_CORE(t0, t1, t2, t3, t4, t5, t6, res)                 \
  res0 = vreinterpretq_s16_u16(vaddl_u8(t0, t1));                            \
  res1 = vreinterpretq_s16_u16(vaddl_u8(t2, t3));                            \
  res2 = vreinterpretq_s16_u16(vaddl_u8(t4, t5));                            \
  res3 = vreinterpretq_s16_u16(vmovl_u8(t6));                                \
  res = wiener_convolve8_horiz_8x8(res0, res1, res2, res3, filter_x_tmp, bd, \
                                   conv_params->round_0);

#define PROCESS_ROW_FOR_VERTICAL_FILTER                                      \
  __builtin_prefetch(dst_tmp_ptr + 0 * dst_stride);                          \
                                                                             \
  do {                                                                       \
    s7 = vld1q_s16(s);                                                       \
    s += src_stride;                                                         \
                                                                             \
    t0 = wiener_convolve8_vert_4x8(s0, s1, s2, s3, s4, s5, s6, filter_y_tmp, \
                                   bd, conv_params->round_1);                \
    vst1_u8(d, t0);                                                          \
    d += dst_stride;                                                         \
                                                                             \
    s0 = s1;                                                                 \
    s1 = s2;                                                                 \
    s2 = s3;                                                                 \
    s3 = s4;                                                                 \
    s4 = s5;                                                                 \
    s5 = s6;                                                                 \
    s6 = s7;                                                                 \
    height--;                                                                \
  } while (height > 0);

static INLINE void process_row_for_horz_filtering(
    uint16_t *dst_ptr, int16_t *filter_x, const uint8_t *src_ptr,
    ptrdiff_t src_stride, ptrdiff_t dst_stride, int round0_bits, int w,
    int height, int bd) {
  do {
    __builtin_prefetch(src_ptr);

    uint8x8_t tt0 = vld1_u8(src_ptr);  // a0 a1 a2 a3 a4 a5 a6 a7

    __builtin_prefetch(dst_ptr);

    const uint8_t *ss = src_ptr + 8;
    uint16_t *d_tmp = dst_ptr;
    int width = w;

    do {
      uint8x8_t tt7 = vld1_u8(ss);  // a8 a9 a10 a11 a12 a13 a14 a15
      uint8x8_t ttemp_0 = tt0;
      tt0 = tt7;

      uint8x8_t tt1 = vext_u8(ttemp_0, tt7, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
      uint8x8_t tt2 = vext_u8(ttemp_0, tt7, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
      uint8x8_t tt3 = vext_u8(ttemp_0, tt7, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
      uint8x8_t tt4 = vext_u8(ttemp_0, tt7, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
      uint8x8_t tt5 = vext_u8(ttemp_0, tt7, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
      uint8x8_t tt6 = vext_u8(ttemp_0, tt7, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
      tt7 = vext_u8(ttemp_0, tt7, 7);            // a7 a8 a9 a10 a11 a12 a13 a14

      int16x8_t ttt0 = vreinterpretq_s16_u16(vaddl_u8(ttemp_0, tt6));
      int16x8_t ttt1 = vreinterpretq_s16_u16(vaddl_u8(tt1, tt5));
      int16x8_t ttt2 = vreinterpretq_s16_u16(vaddl_u8(tt2, tt4));
      int16x8_t ttt3 = vreinterpretq_s16_u16(vmovl_u8(tt3));
      uint16x8_t dd0 = wiener_convolve8_horiz_8x8(ttt0, ttt1, ttt2, ttt3,
                                                  filter_x, bd, round0_bits);

      vst1q_u16(d_tmp, dd0);

      ss += 8;
      d_tmp += 8;
      width -= 8;
    } while (width > 0);

    src_ptr += src_stride;
    dst_ptr += dst_stride;
    height--;
  } while (height > 0);
}

/* Wiener filter 2D
   Apply horizontal filter and store in a temporary buffer. When applying
   vertical filter, overwrite the original pixel values.
*/
void av1_wiener_convolve_add_src_neon(const uint8_t *src, ptrdiff_t src_stride,
                                      uint8_t *dst, ptrdiff_t dst_stride,
                                      const int16_t *filter_x, int x_step_q4,
                                      const int16_t *filter_y, int y_step_q4,
                                      int w, int h,
                                      const ConvolveParams *conv_params) {
  uint8_t *d;
  const uint8_t *src_ptr, *s_tmp;
  uint16_t *dst_ptr;
  (void)x_step_q4;
  (void)y_step_q4;

  int height;
  const int bd = 8;
  // Indicates the height needs to be processed during horizontal filtering.
  const int intermediate_height = h + SUBPEL_TAPS - 1;
  const int center_tap = ((SUBPEL_TAPS - 1) / 2);
  int16_t filter_x_tmp[7], filter_y_tmp[7];

  DECLARE_ALIGNED(16, uint16_t,
                  temp[(MAX_SB_SIZE + SUBPEL_TAPS - 1) * MAX_SB_SIZE]);

  assert(x_step_q4 == 16 && y_step_q4 == 16);
  assert(!(w % 8));

  assert(w <= MAX_SB_SIZE);
  assert(h <= MAX_SB_SIZE);

  assert(filter_x[7] == 0);
  assert(filter_y[7] == 0);

  /* assumption of horizontal filtering output will not exceed 15 bit.
     ((bd) + 1 + FILTER_BITS - conv_params->round_0) <= 15
     16 - conv_params->round_0 <= 15 -- (conv_params->round_0) >= 1
   */
  assert((conv_params->round_0) >= 1);

  memcpy(&filter_x_tmp[0], filter_x, sizeof(*filter_x) * FILTER_BITS);
  memcpy(&filter_y_tmp[0], filter_y, sizeof(*filter_y) * FILTER_BITS);

  filter_x_tmp[3] += (1 << FILTER_BITS);
  filter_y_tmp[3] += (1 << FILTER_BITS);

  s_tmp = src - center_tap * src_stride - center_tap;
  dst_ptr = temp;
  src_ptr = s_tmp;
  height = intermediate_height;

  // For aarch_64.
#if AOM_ARCH_AARCH64
  int processed_height = 0;
  uint16_t *d_tmp;
  int width, remaining_height;
  // Start of horizontal filtering.
  if (intermediate_height > 7) {
    uint16x8_t res4, res5, res6, res7, res8, res9, res10, res11;
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
    do {
      const uint8_t *s;

      __builtin_prefetch(src_ptr + 0 * src_stride);
      __builtin_prefetch(src_ptr + 1 * src_stride);
      __builtin_prefetch(src_ptr + 2 * src_stride);
      __builtin_prefetch(src_ptr + 3 * src_stride);
      __builtin_prefetch(src_ptr + 4 * src_stride);
      __builtin_prefetch(src_ptr + 5 * src_stride);
      __builtin_prefetch(src_ptr + 6 * src_stride);
      __builtin_prefetch(src_ptr + 7 * src_stride);

      load_u8_8x8(src_ptr, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      transpose_elems_inplace_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      s = src_ptr + 7;
      d_tmp = dst_ptr;
      width = w;

      __builtin_prefetch(dst_ptr + 0 * dst_stride);
      __builtin_prefetch(dst_ptr + 1 * dst_stride);
      __builtin_prefetch(dst_ptr + 2 * dst_stride);
      __builtin_prefetch(dst_ptr + 3 * dst_stride);
      __builtin_prefetch(dst_ptr + 4 * dst_stride);
      __builtin_prefetch(dst_ptr + 5 * dst_stride);
      __builtin_prefetch(dst_ptr + 6 * dst_stride);
      __builtin_prefetch(dst_ptr + 7 * dst_stride);

      do {
        int16x8_t res0, res1, res2, res3;
        uint8x8_t t8, t9, t10, t11, t12, t13, t14;
        load_u8_8x8(s, src_stride, &t7, &t8, &t9, &t10, &t11, &t12, &t13, &t14);
        transpose_elems_inplace_u8_8x8(&t7, &t8, &t9, &t10, &t11, &t12, &t13,
                                       &t14);

        HORZ_FILTERING_CORE(t0, t6, t1, t5, t2, t4, t3, res4)
        HORZ_FILTERING_CORE(t1, t7, t2, t6, t3, t5, t4, res5)
        HORZ_FILTERING_CORE(t2, t8, t3, t7, t4, t6, t5, res6)
        HORZ_FILTERING_CORE(t3, t9, t4, t8, t5, t7, t6, res7)
        HORZ_FILTERING_CORE(t4, t10, t5, t9, t6, t8, t7, res8)
        HORZ_FILTERING_CORE(t5, t11, t6, t10, t7, t9, t8, res9)
        HORZ_FILTERING_CORE(t6, t12, t7, t11, t8, t10, t9, res10)
        HORZ_FILTERING_CORE(t7, t13, t8, t12, t9, t11, t10, res11)

        transpose_elems_inplace_u16_8x8(&res4, &res5, &res6, &res7, &res8,
                                        &res9, &res10, &res11);
        store_u16_8x8(d_tmp, MAX_SB_SIZE, res4, res5, res6, res7, res8, res9,
                      res10, res11);

        t0 = t8;
        t1 = t9;
        t2 = t10;
        t3 = t11;
        t4 = t12;
        t5 = t13;
        t6 = t14;
        s += 8;
        d_tmp += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += 8 * src_stride;
      dst_ptr += 8 * MAX_SB_SIZE;
      height -= 8;
      processed_height += 8;
    } while (height > 7);
  }

  // Process the remaining rows for horizontal filtering.
  remaining_height = intermediate_height - processed_height;
  if (remaining_height)
    process_row_for_horz_filtering(dst_ptr, filter_x_tmp, src_ptr, src_stride,
                                   MAX_SB_SIZE, conv_params->round_0, w, height,
                                   bd);

  // Start of vertical filtering.
  {
    int16_t *src_tmp_ptr, *s;
    uint8_t *dst_tmp_ptr;
    height = h;
    width = w;
    src_tmp_ptr = (int16_t *)temp;
    dst_tmp_ptr = dst;
    src_stride = MAX_SB_SIZE;

    do {
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
      uint8x8_t t0;
      s = src_tmp_ptr;
      d = dst_tmp_ptr;

      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      height = h;

      do {
        int16x8_t s8, s9, s10;
        uint8x8_t t1, t2, t3;
        __builtin_prefetch(dst_tmp_ptr + 0 * dst_stride);
        __builtin_prefetch(dst_tmp_ptr + 1 * dst_stride);
        __builtin_prefetch(dst_tmp_ptr + 2 * dst_stride);
        __builtin_prefetch(dst_tmp_ptr + 3 * dst_stride);

        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        t0 = wiener_convolve8_vert_4x8(s0, s1, s2, s3, s4, s5, s6, filter_y_tmp,
                                       bd, conv_params->round_1);
        t1 = wiener_convolve8_vert_4x8(s1, s2, s3, s4, s5, s6, s7, filter_y_tmp,
                                       bd, conv_params->round_1);
        t2 = wiener_convolve8_vert_4x8(s2, s3, s4, s5, s6, s7, s8, filter_y_tmp,
                                       bd, conv_params->round_1);
        t3 = wiener_convolve8_vert_4x8(s3, s4, s5, s6, s7, s8, s9, filter_y_tmp,
                                       bd, conv_params->round_1);

        store_u8_8x4(d, dst_stride, t0, t1, t2, t3);

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 3);

      if (height) {
        PROCESS_ROW_FOR_VERTICAL_FILTER
      }
      src_tmp_ptr += 8;
      dst_tmp_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
#else
  // Start of horizontal filtering.
  process_row_for_horz_filtering(dst_ptr, filter_x_tmp, src_ptr, src_stride,
                                 MAX_SB_SIZE, conv_params->round_0, w, height,
                                 bd);

  // Start of vertical filtering.
  {
    int16_t *src_tmp_ptr, *s;
    uint8_t *dst_tmp_ptr;
    src_tmp_ptr = (int16_t *)temp;
    dst_tmp_ptr = dst;
    src_stride = MAX_SB_SIZE;

    do {
      uint8x8_t t0;
      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7;
      s = src_tmp_ptr;
      d = dst_tmp_ptr;

      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      height = h;
      PROCESS_ROW_FOR_VERTICAL_FILTER

      src_tmp_ptr += 8;
      dst_tmp_ptr += 8;

      w -= 8;
    } while (w > 0);
  }
#endif
}
