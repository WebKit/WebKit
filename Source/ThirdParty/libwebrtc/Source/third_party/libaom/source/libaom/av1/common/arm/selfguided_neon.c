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

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/txfm_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/common.h"
#include "av1/common/resize.h"
#include "av1/common/restoration.h"

// Constants used for right shift in final_filter calculation.
#define NB_EVEN 5
#define NB_ODD 4

static INLINE void calc_ab_fast_internal_common(
    uint32x4_t s0, uint32x4_t s1, uint32x4_t s2, uint32x4_t s3, uint32x4_t s4,
    uint32x4_t s5, uint32x4_t s6, uint32x4_t s7, int32x4_t sr4, int32x4_t sr5,
    int32x4_t sr6, int32x4_t sr7, uint32x4_t const_n_val, uint32x4_t s_vec,
    uint32x4_t const_val, uint32x4_t one_by_n_minus_1_vec,
    uint16x4_t sgrproj_sgr, int32_t *src1, uint16_t *dst_A16, int32_t *src2,
    const int buf_stride) {
  uint32x4_t q0, q1, q2, q3;
  uint32x4_t p0, p1, p2, p3;
  uint16x4_t d0, d1, d2, d3;

  s0 = vmulq_u32(s0, const_n_val);
  s1 = vmulq_u32(s1, const_n_val);
  s2 = vmulq_u32(s2, const_n_val);
  s3 = vmulq_u32(s3, const_n_val);

  q0 = vmulq_u32(s4, s4);
  q1 = vmulq_u32(s5, s5);
  q2 = vmulq_u32(s6, s6);
  q3 = vmulq_u32(s7, s7);

  p0 = vcleq_u32(q0, s0);
  p1 = vcleq_u32(q1, s1);
  p2 = vcleq_u32(q2, s2);
  p3 = vcleq_u32(q3, s3);

  q0 = vsubq_u32(s0, q0);
  q1 = vsubq_u32(s1, q1);
  q2 = vsubq_u32(s2, q2);
  q3 = vsubq_u32(s3, q3);

  p0 = vandq_u32(p0, q0);
  p1 = vandq_u32(p1, q1);
  p2 = vandq_u32(p2, q2);
  p3 = vandq_u32(p3, q3);

  p0 = vmulq_u32(p0, s_vec);
  p1 = vmulq_u32(p1, s_vec);
  p2 = vmulq_u32(p2, s_vec);
  p3 = vmulq_u32(p3, s_vec);

  p0 = vrshrq_n_u32(p0, SGRPROJ_MTABLE_BITS);
  p1 = vrshrq_n_u32(p1, SGRPROJ_MTABLE_BITS);
  p2 = vrshrq_n_u32(p2, SGRPROJ_MTABLE_BITS);
  p3 = vrshrq_n_u32(p3, SGRPROJ_MTABLE_BITS);

  p0 = vminq_u32(p0, const_val);
  p1 = vminq_u32(p1, const_val);
  p2 = vminq_u32(p2, const_val);
  p3 = vminq_u32(p3, const_val);

  {
    store_u32_4x4((uint32_t *)src1, buf_stride, p0, p1, p2, p3);

    for (int x = 0; x < 4; x++) {
      for (int y = 0; y < 4; y++) {
        dst_A16[x * buf_stride + y] = av1_x_by_xplus1[src1[x * buf_stride + y]];
      }
    }
    load_u16_4x4(dst_A16, buf_stride, &d0, &d1, &d2, &d3);
  }
  p0 = vsubl_u16(sgrproj_sgr, d0);
  p1 = vsubl_u16(sgrproj_sgr, d1);
  p2 = vsubl_u16(sgrproj_sgr, d2);
  p3 = vsubl_u16(sgrproj_sgr, d3);

  s4 = vmulq_u32(vreinterpretq_u32_s32(sr4), one_by_n_minus_1_vec);
  s5 = vmulq_u32(vreinterpretq_u32_s32(sr5), one_by_n_minus_1_vec);
  s6 = vmulq_u32(vreinterpretq_u32_s32(sr6), one_by_n_minus_1_vec);
  s7 = vmulq_u32(vreinterpretq_u32_s32(sr7), one_by_n_minus_1_vec);

  s4 = vmulq_u32(s4, p0);
  s5 = vmulq_u32(s5, p1);
  s6 = vmulq_u32(s6, p2);
  s7 = vmulq_u32(s7, p3);

  p0 = vrshrq_n_u32(s4, SGRPROJ_RECIP_BITS);
  p1 = vrshrq_n_u32(s5, SGRPROJ_RECIP_BITS);
  p2 = vrshrq_n_u32(s6, SGRPROJ_RECIP_BITS);
  p3 = vrshrq_n_u32(s7, SGRPROJ_RECIP_BITS);

  store_s32_4x4(src2, buf_stride, vreinterpretq_s32_u32(p0),
                vreinterpretq_s32_u32(p1), vreinterpretq_s32_u32(p2),
                vreinterpretq_s32_u32(p3));
}
static INLINE void calc_ab_internal_common(
    uint32x4_t s0, uint32x4_t s1, uint32x4_t s2, uint32x4_t s3, uint32x4_t s4,
    uint32x4_t s5, uint32x4_t s6, uint32x4_t s7, uint16x8_t s16_0,
    uint16x8_t s16_1, uint16x8_t s16_2, uint16x8_t s16_3, uint16x8_t s16_4,
    uint16x8_t s16_5, uint16x8_t s16_6, uint16x8_t s16_7,
    uint32x4_t const_n_val, uint32x4_t s_vec, uint32x4_t const_val,
    uint16x4_t one_by_n_minus_1_vec, uint16x8_t sgrproj_sgr, int32_t *src1,
    uint16_t *dst_A16, int32_t *dst2, const int buf_stride) {
  uint16x4_t d0, d1, d2, d3, d4, d5, d6, d7;
  uint32x4_t q0, q1, q2, q3, q4, q5, q6, q7;
  uint32x4_t p0, p1, p2, p3, p4, p5, p6, p7;

  s0 = vmulq_u32(s0, const_n_val);
  s1 = vmulq_u32(s1, const_n_val);
  s2 = vmulq_u32(s2, const_n_val);
  s3 = vmulq_u32(s3, const_n_val);
  s4 = vmulq_u32(s4, const_n_val);
  s5 = vmulq_u32(s5, const_n_val);
  s6 = vmulq_u32(s6, const_n_val);
  s7 = vmulq_u32(s7, const_n_val);

  d0 = vget_low_u16(s16_4);
  d1 = vget_low_u16(s16_5);
  d2 = vget_low_u16(s16_6);
  d3 = vget_low_u16(s16_7);
  d4 = vget_high_u16(s16_4);
  d5 = vget_high_u16(s16_5);
  d6 = vget_high_u16(s16_6);
  d7 = vget_high_u16(s16_7);

  q0 = vmull_u16(d0, d0);
  q1 = vmull_u16(d1, d1);
  q2 = vmull_u16(d2, d2);
  q3 = vmull_u16(d3, d3);
  q4 = vmull_u16(d4, d4);
  q5 = vmull_u16(d5, d5);
  q6 = vmull_u16(d6, d6);
  q7 = vmull_u16(d7, d7);

  p0 = vcleq_u32(q0, s0);
  p1 = vcleq_u32(q1, s1);
  p2 = vcleq_u32(q2, s2);
  p3 = vcleq_u32(q3, s3);
  p4 = vcleq_u32(q4, s4);
  p5 = vcleq_u32(q5, s5);
  p6 = vcleq_u32(q6, s6);
  p7 = vcleq_u32(q7, s7);

  q0 = vsubq_u32(s0, q0);
  q1 = vsubq_u32(s1, q1);
  q2 = vsubq_u32(s2, q2);
  q3 = vsubq_u32(s3, q3);
  q4 = vsubq_u32(s4, q4);
  q5 = vsubq_u32(s5, q5);
  q6 = vsubq_u32(s6, q6);
  q7 = vsubq_u32(s7, q7);

  p0 = vandq_u32(p0, q0);
  p1 = vandq_u32(p1, q1);
  p2 = vandq_u32(p2, q2);
  p3 = vandq_u32(p3, q3);
  p4 = vandq_u32(p4, q4);
  p5 = vandq_u32(p5, q5);
  p6 = vandq_u32(p6, q6);
  p7 = vandq_u32(p7, q7);

  p0 = vmulq_u32(p0, s_vec);
  p1 = vmulq_u32(p1, s_vec);
  p2 = vmulq_u32(p2, s_vec);
  p3 = vmulq_u32(p3, s_vec);
  p4 = vmulq_u32(p4, s_vec);
  p5 = vmulq_u32(p5, s_vec);
  p6 = vmulq_u32(p6, s_vec);
  p7 = vmulq_u32(p7, s_vec);

  p0 = vrshrq_n_u32(p0, SGRPROJ_MTABLE_BITS);
  p1 = vrshrq_n_u32(p1, SGRPROJ_MTABLE_BITS);
  p2 = vrshrq_n_u32(p2, SGRPROJ_MTABLE_BITS);
  p3 = vrshrq_n_u32(p3, SGRPROJ_MTABLE_BITS);
  p4 = vrshrq_n_u32(p4, SGRPROJ_MTABLE_BITS);
  p5 = vrshrq_n_u32(p5, SGRPROJ_MTABLE_BITS);
  p6 = vrshrq_n_u32(p6, SGRPROJ_MTABLE_BITS);
  p7 = vrshrq_n_u32(p7, SGRPROJ_MTABLE_BITS);

  p0 = vminq_u32(p0, const_val);
  p1 = vminq_u32(p1, const_val);
  p2 = vminq_u32(p2, const_val);
  p3 = vminq_u32(p3, const_val);
  p4 = vminq_u32(p4, const_val);
  p5 = vminq_u32(p5, const_val);
  p6 = vminq_u32(p6, const_val);
  p7 = vminq_u32(p7, const_val);

  {
    store_u32_4x4((uint32_t *)src1, buf_stride, p0, p1, p2, p3);
    store_u32_4x4((uint32_t *)src1 + 4, buf_stride, p4, p5, p6, p7);

    for (int x = 0; x < 4; x++) {
      for (int y = 0; y < 8; y++) {
        dst_A16[x * buf_stride + y] = av1_x_by_xplus1[src1[x * buf_stride + y]];
      }
    }
    load_u16_8x4(dst_A16, buf_stride, &s16_4, &s16_5, &s16_6, &s16_7);
  }

  s16_4 = vsubq_u16(sgrproj_sgr, s16_4);
  s16_5 = vsubq_u16(sgrproj_sgr, s16_5);
  s16_6 = vsubq_u16(sgrproj_sgr, s16_6);
  s16_7 = vsubq_u16(sgrproj_sgr, s16_7);

  s0 = vmull_u16(vget_low_u16(s16_0), one_by_n_minus_1_vec);
  s1 = vmull_u16(vget_low_u16(s16_1), one_by_n_minus_1_vec);
  s2 = vmull_u16(vget_low_u16(s16_2), one_by_n_minus_1_vec);
  s3 = vmull_u16(vget_low_u16(s16_3), one_by_n_minus_1_vec);
  s4 = vmull_u16(vget_high_u16(s16_0), one_by_n_minus_1_vec);
  s5 = vmull_u16(vget_high_u16(s16_1), one_by_n_minus_1_vec);
  s6 = vmull_u16(vget_high_u16(s16_2), one_by_n_minus_1_vec);
  s7 = vmull_u16(vget_high_u16(s16_3), one_by_n_minus_1_vec);

  s0 = vmulq_u32(s0, vmovl_u16(vget_low_u16(s16_4)));
  s1 = vmulq_u32(s1, vmovl_u16(vget_low_u16(s16_5)));
  s2 = vmulq_u32(s2, vmovl_u16(vget_low_u16(s16_6)));
  s3 = vmulq_u32(s3, vmovl_u16(vget_low_u16(s16_7)));
  s4 = vmulq_u32(s4, vmovl_u16(vget_high_u16(s16_4)));
  s5 = vmulq_u32(s5, vmovl_u16(vget_high_u16(s16_5)));
  s6 = vmulq_u32(s6, vmovl_u16(vget_high_u16(s16_6)));
  s7 = vmulq_u32(s7, vmovl_u16(vget_high_u16(s16_7)));

  p0 = vrshrq_n_u32(s0, SGRPROJ_RECIP_BITS);
  p1 = vrshrq_n_u32(s1, SGRPROJ_RECIP_BITS);
  p2 = vrshrq_n_u32(s2, SGRPROJ_RECIP_BITS);
  p3 = vrshrq_n_u32(s3, SGRPROJ_RECIP_BITS);
  p4 = vrshrq_n_u32(s4, SGRPROJ_RECIP_BITS);
  p5 = vrshrq_n_u32(s5, SGRPROJ_RECIP_BITS);
  p6 = vrshrq_n_u32(s6, SGRPROJ_RECIP_BITS);
  p7 = vrshrq_n_u32(s7, SGRPROJ_RECIP_BITS);

  store_s32_4x4(dst2, buf_stride, vreinterpretq_s32_u32(p0),
                vreinterpretq_s32_u32(p1), vreinterpretq_s32_u32(p2),
                vreinterpretq_s32_u32(p3));
  store_s32_4x4(dst2 + 4, buf_stride, vreinterpretq_s32_u32(p4),
                vreinterpretq_s32_u32(p5), vreinterpretq_s32_u32(p6),
                vreinterpretq_s32_u32(p7));
}

static INLINE void boxsum2_square_sum_calc(
    int16x4_t t1, int16x4_t t2, int16x4_t t3, int16x4_t t4, int16x4_t t5,
    int16x4_t t6, int16x4_t t7, int16x4_t t8, int16x4_t t9, int16x4_t t10,
    int16x4_t t11, int32x4_t *r0, int32x4_t *r1, int32x4_t *r2, int32x4_t *r3) {
  int32x4_t d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11;
  int32x4_t r12, r34, r67, r89, r1011;
  int32x4_t r345, r6789, r789;

  d1 = vmull_s16(t1, t1);
  d2 = vmull_s16(t2, t2);
  d3 = vmull_s16(t3, t3);
  d4 = vmull_s16(t4, t4);
  d5 = vmull_s16(t5, t5);
  d6 = vmull_s16(t6, t6);
  d7 = vmull_s16(t7, t7);
  d8 = vmull_s16(t8, t8);
  d9 = vmull_s16(t9, t9);
  d10 = vmull_s16(t10, t10);
  d11 = vmull_s16(t11, t11);

  r12 = vaddq_s32(d1, d2);
  r34 = vaddq_s32(d3, d4);
  r67 = vaddq_s32(d6, d7);
  r89 = vaddq_s32(d8, d9);
  r1011 = vaddq_s32(d10, d11);
  r345 = vaddq_s32(r34, d5);
  r6789 = vaddq_s32(r67, r89);
  r789 = vsubq_s32(r6789, d6);
  *r0 = vaddq_s32(r12, r345);
  *r1 = vaddq_s32(r67, r345);
  *r2 = vaddq_s32(d5, r6789);
  *r3 = vaddq_s32(r789, r1011);
}

static INLINE void boxsum2(int16_t *src, const int src_stride, int16_t *dst16,
                           int32_t *dst32, int32_t *dst2, const int dst_stride,
                           const int width, const int height) {
  assert(width > 2 * SGRPROJ_BORDER_HORZ);
  assert(height > 2 * SGRPROJ_BORDER_VERT);

  int16_t *dst1_16_ptr, *src_ptr;
  int32_t *dst2_ptr;
  int h, w, count = 0;
  const int dst_stride_2 = (dst_stride << 1);
  const int dst_stride_8 = (dst_stride << 3);

  dst1_16_ptr = dst16;
  dst2_ptr = dst2;
  src_ptr = src;
  w = width;
  {
    int16x8_t t1, t2, t3, t4, t5, t6, t7;
    int16x8_t t8, t9, t10, t11, t12;

    int16x8_t q12345, q56789, q34567, q7891011;
    int16x8_t q12, q34, q67, q89, q1011;
    int16x8_t q345, q6789, q789;

    int32x4_t r12345, r56789, r34567, r7891011;

    do {
      h = height;
      dst1_16_ptr = dst16 + (count << 3);
      dst2_ptr = dst2 + (count << 3);
      src_ptr = src + (count << 3);

      dst1_16_ptr += dst_stride_2;
      dst2_ptr += dst_stride_2;
      do {
        load_s16_8x4(src_ptr, src_stride, &t1, &t2, &t3, &t4);
        src_ptr += 4 * src_stride;
        load_s16_8x4(src_ptr, src_stride, &t5, &t6, &t7, &t8);
        src_ptr += 4 * src_stride;
        load_s16_8x4(src_ptr, src_stride, &t9, &t10, &t11, &t12);

        q12 = vaddq_s16(t1, t2);
        q34 = vaddq_s16(t3, t4);
        q67 = vaddq_s16(t6, t7);
        q89 = vaddq_s16(t8, t9);
        q1011 = vaddq_s16(t10, t11);
        q345 = vaddq_s16(q34, t5);
        q6789 = vaddq_s16(q67, q89);
        q789 = vaddq_s16(q89, t7);
        q12345 = vaddq_s16(q12, q345);
        q34567 = vaddq_s16(q67, q345);
        q56789 = vaddq_s16(t5, q6789);
        q7891011 = vaddq_s16(q789, q1011);

        store_s16_8x4(dst1_16_ptr, dst_stride_2, q12345, q34567, q56789,
                      q7891011);
        dst1_16_ptr += dst_stride_8;

        boxsum2_square_sum_calc(
            vget_low_s16(t1), vget_low_s16(t2), vget_low_s16(t3),
            vget_low_s16(t4), vget_low_s16(t5), vget_low_s16(t6),
            vget_low_s16(t7), vget_low_s16(t8), vget_low_s16(t9),
            vget_low_s16(t10), vget_low_s16(t11), &r12345, &r34567, &r56789,
            &r7891011);

        store_s32_4x4(dst2_ptr, dst_stride_2, r12345, r34567, r56789, r7891011);

        boxsum2_square_sum_calc(
            vget_high_s16(t1), vget_high_s16(t2), vget_high_s16(t3),
            vget_high_s16(t4), vget_high_s16(t5), vget_high_s16(t6),
            vget_high_s16(t7), vget_high_s16(t8), vget_high_s16(t9),
            vget_high_s16(t10), vget_high_s16(t11), &r12345, &r34567, &r56789,
            &r7891011);

        store_s32_4x4(dst2_ptr + 4, dst_stride_2, r12345, r34567, r56789,
                      r7891011);
        dst2_ptr += (dst_stride_8);
        h -= 8;
      } while (h > 0);
      w -= 8;
      count++;
    } while (w > 0);

    // memset needed for row pixels as 2nd stage of boxsum filter uses
    // first 2 rows of dst16, dst2 buffer which is not filled in first stage.
    for (int x = 0; x < 2; x++) {
      memset(dst16 + x * dst_stride, 0, (width + 4) * sizeof(*dst16));
      memset(dst2 + x * dst_stride, 0, (width + 4) * sizeof(*dst2));
    }

    // memset needed for extra columns as 2nd stage of boxsum filter uses
    // last 2 columns of dst16, dst2 buffer which is not filled in first stage.
    for (int x = 2; x < height + 2; x++) {
      int dst_offset = x * dst_stride + width + 2;
      memset(dst16 + dst_offset, 0, 3 * sizeof(*dst16));
      memset(dst2 + dst_offset, 0, 3 * sizeof(*dst2));
    }
  }

  {
    int16x4_t s1, s2, s3, s4, s5, s6, s7, s8;
    int32x4_t d1, d2, d3, d4, d5, d6, d7, d8;
    int32x4_t q12345, q34567, q23456, q45678;
    int32x4_t q23, q45, q67;
    int32x4_t q2345, q4567;

    int32x4_t r12345, r34567, r23456, r45678;
    int32x4_t r23, r45, r67;
    int32x4_t r2345, r4567;

    int32_t *src2_ptr, *dst1_32_ptr;
    int16_t *src1_ptr;
    count = 0;
    h = height;
    do {
      dst1_32_ptr = dst32 + count * dst_stride_8 + (dst_stride_2);
      dst2_ptr = dst2 + count * dst_stride_8 + (dst_stride_2);
      src1_ptr = dst16 + count * dst_stride_8 + (dst_stride_2);
      src2_ptr = dst2 + count * dst_stride_8 + (dst_stride_2);
      w = width;

      dst1_32_ptr += 2;
      dst2_ptr += 2;
      load_s16_4x4(src1_ptr, dst_stride_2, &s1, &s2, &s3, &s4);
      transpose_elems_inplace_s16_4x4(&s1, &s2, &s3, &s4);
      load_s32_4x4(src2_ptr, dst_stride_2, &d1, &d2, &d3, &d4);
      transpose_elems_inplace_s32_4x4(&d1, &d2, &d3, &d4);
      do {
        src1_ptr += 4;
        src2_ptr += 4;
        load_s16_4x4(src1_ptr, dst_stride_2, &s5, &s6, &s7, &s8);
        transpose_elems_inplace_s16_4x4(&s5, &s6, &s7, &s8);
        load_s32_4x4(src2_ptr, dst_stride_2, &d5, &d6, &d7, &d8);
        transpose_elems_inplace_s32_4x4(&d5, &d6, &d7, &d8);
        q23 = vaddl_s16(s2, s3);
        q45 = vaddl_s16(s4, s5);
        q67 = vaddl_s16(s6, s7);
        q2345 = vaddq_s32(q23, q45);
        q4567 = vaddq_s32(q45, q67);
        q12345 = vaddq_s32(vmovl_s16(s1), q2345);
        q23456 = vaddq_s32(q2345, vmovl_s16(s6));
        q34567 = vaddq_s32(q4567, vmovl_s16(s3));
        q45678 = vaddq_s32(q4567, vmovl_s16(s8));

        transpose_elems_inplace_s32_4x4(&q12345, &q23456, &q34567, &q45678);
        store_s32_4x4(dst1_32_ptr, dst_stride_2, q12345, q23456, q34567,
                      q45678);
        dst1_32_ptr += 4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;

        r23 = vaddq_s32(d2, d3);
        r45 = vaddq_s32(d4, d5);
        r67 = vaddq_s32(d6, d7);
        r2345 = vaddq_s32(r23, r45);
        r4567 = vaddq_s32(r45, r67);
        r12345 = vaddq_s32(d1, r2345);
        r23456 = vaddq_s32(r2345, d6);
        r34567 = vaddq_s32(r4567, d3);
        r45678 = vaddq_s32(r4567, d8);

        transpose_elems_inplace_s32_4x4(&r12345, &r23456, &r34567, &r45678);
        store_s32_4x4(dst2_ptr, dst_stride_2, r12345, r23456, r34567, r45678);
        dst2_ptr += 4;
        d1 = d5;
        d2 = d6;
        d3 = d7;
        d4 = d8;
        w -= 4;
      } while (w > 0);
      h -= 8;
      count++;
    } while (h > 0);
  }
}

static INLINE void calc_ab_internal_lbd(int32_t *A, uint16_t *A16,
                                        uint16_t *B16, int32_t *B,
                                        const int buf_stride, const int width,
                                        const int height, const int r,
                                        const int s, const int ht_inc) {
  int32_t *src1, *dst2, count = 0;
  uint16_t *dst_A16, *src2;
  const uint32_t n = (2 * r + 1) * (2 * r + 1);
  const uint32x4_t const_n_val = vdupq_n_u32(n);
  const uint16x8_t sgrproj_sgr = vdupq_n_u16(SGRPROJ_SGR);
  const uint16x4_t one_by_n_minus_1_vec = vdup_n_u16(av1_one_by_x[n - 1]);
  const uint32x4_t const_val = vdupq_n_u32(255);

  uint16x8_t s16_0, s16_1, s16_2, s16_3, s16_4, s16_5, s16_6, s16_7;

  uint32x4_t s0, s1, s2, s3, s4, s5, s6, s7;

  const uint32x4_t s_vec = vdupq_n_u32(s);
  int w, h = height;

  do {
    dst_A16 = A16 + (count << 2) * buf_stride;
    src1 = A + (count << 2) * buf_stride;
    src2 = B16 + (count << 2) * buf_stride;
    dst2 = B + (count << 2) * buf_stride;
    w = width;
    do {
      load_u32_4x4((uint32_t *)src1, buf_stride, &s0, &s1, &s2, &s3);
      load_u32_4x4((uint32_t *)src1 + 4, buf_stride, &s4, &s5, &s6, &s7);
      load_u16_8x4(src2, buf_stride, &s16_0, &s16_1, &s16_2, &s16_3);

      s16_4 = s16_0;
      s16_5 = s16_1;
      s16_6 = s16_2;
      s16_7 = s16_3;

      calc_ab_internal_common(
          s0, s1, s2, s3, s4, s5, s6, s7, s16_0, s16_1, s16_2, s16_3, s16_4,
          s16_5, s16_6, s16_7, const_n_val, s_vec, const_val,
          one_by_n_minus_1_vec, sgrproj_sgr, src1, dst_A16, dst2, buf_stride);

      w -= 8;
      dst2 += 8;
      src1 += 8;
      src2 += 8;
      dst_A16 += 8;
    } while (w > 0);
    count++;
    h -= (ht_inc * 4);
  } while (h > 0);
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE void calc_ab_internal_hbd(int32_t *A, uint16_t *A16,
                                        uint16_t *B16, int32_t *B,
                                        const int buf_stride, const int width,
                                        const int height, const int bit_depth,
                                        const int r, const int s,
                                        const int ht_inc) {
  int32_t *src1, *dst2, count = 0;
  uint16_t *dst_A16, *src2;
  const uint32_t n = (2 * r + 1) * (2 * r + 1);
  const int16x8_t bd_min_2_vec = vdupq_n_s16(-(bit_depth - 8));
  const int32x4_t bd_min_1_vec = vdupq_n_s32(-((bit_depth - 8) << 1));
  const uint32x4_t const_n_val = vdupq_n_u32(n);
  const uint16x8_t sgrproj_sgr = vdupq_n_u16(SGRPROJ_SGR);
  const uint16x4_t one_by_n_minus_1_vec = vdup_n_u16(av1_one_by_x[n - 1]);
  const uint32x4_t const_val = vdupq_n_u32(255);

  int32x4_t sr0, sr1, sr2, sr3, sr4, sr5, sr6, sr7;
  uint16x8_t s16_0, s16_1, s16_2, s16_3;
  uint16x8_t s16_4, s16_5, s16_6, s16_7;
  uint32x4_t s0, s1, s2, s3, s4, s5, s6, s7;

  const uint32x4_t s_vec = vdupq_n_u32(s);
  int w, h = height;

  do {
    src1 = A + (count << 2) * buf_stride;
    src2 = B16 + (count << 2) * buf_stride;
    dst2 = B + (count << 2) * buf_stride;
    dst_A16 = A16 + (count << 2) * buf_stride;
    w = width;
    do {
      load_s32_4x4(src1, buf_stride, &sr0, &sr1, &sr2, &sr3);
      load_s32_4x4(src1 + 4, buf_stride, &sr4, &sr5, &sr6, &sr7);
      load_u16_8x4(src2, buf_stride, &s16_0, &s16_1, &s16_2, &s16_3);

      s0 = vrshlq_u32(vreinterpretq_u32_s32(sr0), bd_min_1_vec);
      s1 = vrshlq_u32(vreinterpretq_u32_s32(sr1), bd_min_1_vec);
      s2 = vrshlq_u32(vreinterpretq_u32_s32(sr2), bd_min_1_vec);
      s3 = vrshlq_u32(vreinterpretq_u32_s32(sr3), bd_min_1_vec);
      s4 = vrshlq_u32(vreinterpretq_u32_s32(sr4), bd_min_1_vec);
      s5 = vrshlq_u32(vreinterpretq_u32_s32(sr5), bd_min_1_vec);
      s6 = vrshlq_u32(vreinterpretq_u32_s32(sr6), bd_min_1_vec);
      s7 = vrshlq_u32(vreinterpretq_u32_s32(sr7), bd_min_1_vec);

      s16_4 = vrshlq_u16(s16_0, bd_min_2_vec);
      s16_5 = vrshlq_u16(s16_1, bd_min_2_vec);
      s16_6 = vrshlq_u16(s16_2, bd_min_2_vec);
      s16_7 = vrshlq_u16(s16_3, bd_min_2_vec);

      calc_ab_internal_common(
          s0, s1, s2, s3, s4, s5, s6, s7, s16_0, s16_1, s16_2, s16_3, s16_4,
          s16_5, s16_6, s16_7, const_n_val, s_vec, const_val,
          one_by_n_minus_1_vec, sgrproj_sgr, src1, dst_A16, dst2, buf_stride);

      w -= 8;
      dst2 += 8;
      src1 += 8;
      src2 += 8;
      dst_A16 += 8;
    } while (w > 0);
    count++;
    h -= (ht_inc * 4);
  } while (h > 0);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static INLINE void calc_ab_fast_internal_lbd(int32_t *A, uint16_t *A16,
                                             int32_t *B, const int buf_stride,
                                             const int width, const int height,
                                             const int r, const int s,
                                             const int ht_inc) {
  int32_t *src1, *src2, count = 0;
  uint16_t *dst_A16;
  const uint32_t n = (2 * r + 1) * (2 * r + 1);
  const uint32x4_t const_n_val = vdupq_n_u32(n);
  const uint16x4_t sgrproj_sgr = vdup_n_u16(SGRPROJ_SGR);
  const uint32x4_t one_by_n_minus_1_vec = vdupq_n_u32(av1_one_by_x[n - 1]);
  const uint32x4_t const_val = vdupq_n_u32(255);

  int32x4_t sr0, sr1, sr2, sr3, sr4, sr5, sr6, sr7;
  uint32x4_t s0, s1, s2, s3, s4, s5, s6, s7;

  const uint32x4_t s_vec = vdupq_n_u32(s);
  int w, h = height;

  do {
    src1 = A + (count << 2) * buf_stride;
    src2 = B + (count << 2) * buf_stride;
    dst_A16 = A16 + (count << 2) * buf_stride;
    w = width;
    do {
      load_s32_4x4(src1, buf_stride, &sr0, &sr1, &sr2, &sr3);
      load_s32_4x4(src2, buf_stride, &sr4, &sr5, &sr6, &sr7);

      s0 = vreinterpretq_u32_s32(sr0);
      s1 = vreinterpretq_u32_s32(sr1);
      s2 = vreinterpretq_u32_s32(sr2);
      s3 = vreinterpretq_u32_s32(sr3);
      s4 = vreinterpretq_u32_s32(sr4);
      s5 = vreinterpretq_u32_s32(sr5);
      s6 = vreinterpretq_u32_s32(sr6);
      s7 = vreinterpretq_u32_s32(sr7);

      calc_ab_fast_internal_common(s0, s1, s2, s3, s4, s5, s6, s7, sr4, sr5,
                                   sr6, sr7, const_n_val, s_vec, const_val,
                                   one_by_n_minus_1_vec, sgrproj_sgr, src1,
                                   dst_A16, src2, buf_stride);

      w -= 4;
      src1 += 4;
      src2 += 4;
      dst_A16 += 4;
    } while (w > 0);
    count++;
    h -= (ht_inc * 4);
  } while (h > 0);
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE void calc_ab_fast_internal_hbd(int32_t *A, uint16_t *A16,
                                             int32_t *B, const int buf_stride,
                                             const int width, const int height,
                                             const int bit_depth, const int r,
                                             const int s, const int ht_inc) {
  int32_t *src1, *src2, count = 0;
  uint16_t *dst_A16;
  const uint32_t n = (2 * r + 1) * (2 * r + 1);
  const int32x4_t bd_min_2_vec = vdupq_n_s32(-(bit_depth - 8));
  const int32x4_t bd_min_1_vec = vdupq_n_s32(-((bit_depth - 8) << 1));
  const uint32x4_t const_n_val = vdupq_n_u32(n);
  const uint16x4_t sgrproj_sgr = vdup_n_u16(SGRPROJ_SGR);
  const uint32x4_t one_by_n_minus_1_vec = vdupq_n_u32(av1_one_by_x[n - 1]);
  const uint32x4_t const_val = vdupq_n_u32(255);

  int32x4_t sr0, sr1, sr2, sr3, sr4, sr5, sr6, sr7;
  uint32x4_t s0, s1, s2, s3, s4, s5, s6, s7;

  const uint32x4_t s_vec = vdupq_n_u32(s);
  int w, h = height;

  do {
    src1 = A + (count << 2) * buf_stride;
    src2 = B + (count << 2) * buf_stride;
    dst_A16 = A16 + (count << 2) * buf_stride;
    w = width;
    do {
      load_s32_4x4(src1, buf_stride, &sr0, &sr1, &sr2, &sr3);
      load_s32_4x4(src2, buf_stride, &sr4, &sr5, &sr6, &sr7);

      s0 = vrshlq_u32(vreinterpretq_u32_s32(sr0), bd_min_1_vec);
      s1 = vrshlq_u32(vreinterpretq_u32_s32(sr1), bd_min_1_vec);
      s2 = vrshlq_u32(vreinterpretq_u32_s32(sr2), bd_min_1_vec);
      s3 = vrshlq_u32(vreinterpretq_u32_s32(sr3), bd_min_1_vec);
      s4 = vrshlq_u32(vreinterpretq_u32_s32(sr4), bd_min_2_vec);
      s5 = vrshlq_u32(vreinterpretq_u32_s32(sr5), bd_min_2_vec);
      s6 = vrshlq_u32(vreinterpretq_u32_s32(sr6), bd_min_2_vec);
      s7 = vrshlq_u32(vreinterpretq_u32_s32(sr7), bd_min_2_vec);

      calc_ab_fast_internal_common(s0, s1, s2, s3, s4, s5, s6, s7, sr4, sr5,
                                   sr6, sr7, const_n_val, s_vec, const_val,
                                   one_by_n_minus_1_vec, sgrproj_sgr, src1,
                                   dst_A16, src2, buf_stride);

      w -= 4;
      src1 += 4;
      src2 += 4;
      dst_A16 += 4;
    } while (w > 0);
    count++;
    h -= (ht_inc * 4);
  } while (h > 0);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static INLINE void boxsum1(int16_t *src, const int src_stride, uint16_t *dst1,
                           int32_t *dst2, const int dst_stride, const int width,
                           const int height) {
  assert(width > 2 * SGRPROJ_BORDER_HORZ);
  assert(height > 2 * SGRPROJ_BORDER_VERT);

  int16_t *src_ptr;
  int32_t *dst2_ptr;
  uint16_t *dst1_ptr;
  int h, w, count = 0;

  w = width;
  {
    int16x8_t s1, s2, s3, s4, s5, s6, s7, s8;
    int16x8_t q23, q34, q56, q234, q345, q456, q567;
    int32x4_t r23, r56, r345, r456, r567, r78, r678;
    int32x4_t r4_low, r4_high, r34_low, r34_high, r234_low, r234_high;
    int32x4_t r2, r3, r5, r6, r7, r8;
    int16x8_t q678, q78;

    do {
      dst1_ptr = dst1 + (count << 3);
      dst2_ptr = dst2 + (count << 3);
      src_ptr = src + (count << 3);
      h = height;

      load_s16_8x4(src_ptr, src_stride, &s1, &s2, &s3, &s4);
      src_ptr += 4 * src_stride;

      q23 = vaddq_s16(s2, s3);
      q234 = vaddq_s16(q23, s4);
      q34 = vaddq_s16(s3, s4);
      dst1_ptr += (dst_stride << 1);

      r2 = vmull_s16(vget_low_s16(s2), vget_low_s16(s2));
      r3 = vmull_s16(vget_low_s16(s3), vget_low_s16(s3));
      r4_low = vmull_s16(vget_low_s16(s4), vget_low_s16(s4));
      r23 = vaddq_s32(r2, r3);
      r234_low = vaddq_s32(r23, r4_low);
      r34_low = vaddq_s32(r3, r4_low);

      r2 = vmull_s16(vget_high_s16(s2), vget_high_s16(s2));
      r3 = vmull_s16(vget_high_s16(s3), vget_high_s16(s3));
      r4_high = vmull_s16(vget_high_s16(s4), vget_high_s16(s4));
      r23 = vaddq_s32(r2, r3);
      r234_high = vaddq_s32(r23, r4_high);
      r34_high = vaddq_s32(r3, r4_high);

      dst2_ptr += (dst_stride << 1);

      do {
        load_s16_8x4(src_ptr, src_stride, &s5, &s6, &s7, &s8);
        src_ptr += 4 * src_stride;

        q345 = vaddq_s16(s5, q34);
        q56 = vaddq_s16(s5, s6);
        q456 = vaddq_s16(s4, q56);
        q567 = vaddq_s16(s7, q56);
        q78 = vaddq_s16(s7, s8);
        q678 = vaddq_s16(s6, q78);

        store_s16_8x4((int16_t *)dst1_ptr, dst_stride, q234, q345, q456, q567);
        dst1_ptr += (dst_stride << 2);

        s4 = s8;
        q34 = q78;
        q234 = q678;

        r5 = vmull_s16(vget_low_s16(s5), vget_low_s16(s5));
        r6 = vmull_s16(vget_low_s16(s6), vget_low_s16(s6));
        r7 = vmull_s16(vget_low_s16(s7), vget_low_s16(s7));
        r8 = vmull_s16(vget_low_s16(s8), vget_low_s16(s8));

        r345 = vaddq_s32(r5, r34_low);
        r56 = vaddq_s32(r5, r6);
        r456 = vaddq_s32(r4_low, r56);
        r567 = vaddq_s32(r7, r56);
        r78 = vaddq_s32(r7, r8);
        r678 = vaddq_s32(r6, r78);
        store_s32_4x4(dst2_ptr, dst_stride, r234_low, r345, r456, r567);

        r4_low = r8;
        r34_low = r78;
        r234_low = r678;

        r5 = vmull_s16(vget_high_s16(s5), vget_high_s16(s5));
        r6 = vmull_s16(vget_high_s16(s6), vget_high_s16(s6));
        r7 = vmull_s16(vget_high_s16(s7), vget_high_s16(s7));
        r8 = vmull_s16(vget_high_s16(s8), vget_high_s16(s8));

        r345 = vaddq_s32(r5, r34_high);
        r56 = vaddq_s32(r5, r6);
        r456 = vaddq_s32(r4_high, r56);
        r567 = vaddq_s32(r7, r56);
        r78 = vaddq_s32(r7, r8);
        r678 = vaddq_s32(r6, r78);
        store_s32_4x4((dst2_ptr + 4), dst_stride, r234_high, r345, r456, r567);
        dst2_ptr += (dst_stride << 2);

        r4_high = r8;
        r34_high = r78;
        r234_high = r678;

        h -= 4;
      } while (h > 0);
      w -= 8;
      count++;
    } while (w > 0);

    // memset needed for row pixels as 2nd stage of boxsum filter uses
    // first 2 rows of dst1, dst2 buffer which is not filled in first stage.
    for (int x = 0; x < 2; x++) {
      memset(dst1 + x * dst_stride, 0, (width + 4) * sizeof(*dst1));
      memset(dst2 + x * dst_stride, 0, (width + 4) * sizeof(*dst2));
    }

    // memset needed for extra columns as 2nd stage of boxsum filter uses
    // last 2 columns of dst1, dst2 buffer which is not filled in first stage.
    for (int x = 2; x < height + 2; x++) {
      int dst_offset = x * dst_stride + width + 2;
      memset(dst1 + dst_offset, 0, 3 * sizeof(*dst1));
      memset(dst2 + dst_offset, 0, 3 * sizeof(*dst2));
    }
  }

  {
    int16x4_t d1, d2, d3, d4, d5, d6, d7, d8;
    int16x4_t q23, q34, q56, q234, q345, q456, q567;
    int32x4_t r23, r56, r234, r345, r456, r567, r34, r78, r678;
    int32x4_t r1, r2, r3, r4, r5, r6, r7, r8;
    int16x4_t q678, q78;

    int32_t *src2_ptr;
    uint16_t *src1_ptr;
    count = 0;
    h = height;
    w = width;
    do {
      dst1_ptr = dst1 + (count << 2) * dst_stride;
      dst2_ptr = dst2 + (count << 2) * dst_stride;
      src1_ptr = dst1 + (count << 2) * dst_stride;
      src2_ptr = dst2 + (count << 2) * dst_stride;
      w = width;

      load_s16_4x4((int16_t *)src1_ptr, dst_stride, &d1, &d2, &d3, &d4);
      transpose_elems_inplace_s16_4x4(&d1, &d2, &d3, &d4);
      load_s32_4x4(src2_ptr, dst_stride, &r1, &r2, &r3, &r4);
      transpose_elems_inplace_s32_4x4(&r1, &r2, &r3, &r4);
      src1_ptr += 4;
      src2_ptr += 4;

      q23 = vadd_s16(d2, d3);
      q234 = vadd_s16(q23, d4);
      q34 = vadd_s16(d3, d4);
      dst1_ptr += 2;
      r23 = vaddq_s32(r2, r3);
      r234 = vaddq_s32(r23, r4);
      r34 = vaddq_s32(r3, r4);
      dst2_ptr += 2;

      do {
        load_s16_4x4((int16_t *)src1_ptr, dst_stride, &d5, &d6, &d7, &d8);
        transpose_elems_inplace_s16_4x4(&d5, &d6, &d7, &d8);
        load_s32_4x4(src2_ptr, dst_stride, &r5, &r6, &r7, &r8);
        transpose_elems_inplace_s32_4x4(&r5, &r6, &r7, &r8);
        src1_ptr += 4;
        src2_ptr += 4;

        q345 = vadd_s16(d5, q34);
        q56 = vadd_s16(d5, d6);
        q456 = vadd_s16(d4, q56);
        q567 = vadd_s16(d7, q56);
        q78 = vadd_s16(d7, d8);
        q678 = vadd_s16(d6, q78);
        transpose_elems_inplace_s16_4x4(&q234, &q345, &q456, &q567);
        store_s16_4x4((int16_t *)dst1_ptr, dst_stride, q234, q345, q456, q567);
        dst1_ptr += 4;

        d4 = d8;
        q34 = q78;
        q234 = q678;

        r345 = vaddq_s32(r5, r34);
        r56 = vaddq_s32(r5, r6);
        r456 = vaddq_s32(r4, r56);
        r567 = vaddq_s32(r7, r56);
        r78 = vaddq_s32(r7, r8);
        r678 = vaddq_s32(r6, r78);
        transpose_elems_inplace_s32_4x4(&r234, &r345, &r456, &r567);
        store_s32_4x4(dst2_ptr, dst_stride, r234, r345, r456, r567);
        dst2_ptr += 4;

        r4 = r8;
        r34 = r78;
        r234 = r678;
        w -= 4;
      } while (w > 0);
      h -= 4;
      count++;
    } while (h > 0);
  }
}

static INLINE int32x4_t cross_sum_inp_s32(int32_t *buf, int buf_stride) {
  int32x4_t xtr, xt, xtl, xl, x, xr, xbr, xb, xbl;
  int32x4_t fours, threes, res;

  xtl = vld1q_s32(buf - buf_stride - 1);
  xt = vld1q_s32(buf - buf_stride);
  xtr = vld1q_s32(buf - buf_stride + 1);
  xl = vld1q_s32(buf - 1);
  x = vld1q_s32(buf);
  xr = vld1q_s32(buf + 1);
  xbl = vld1q_s32(buf + buf_stride - 1);
  xb = vld1q_s32(buf + buf_stride);
  xbr = vld1q_s32(buf + buf_stride + 1);

  fours = vaddq_s32(xl, vaddq_s32(xt, vaddq_s32(xr, vaddq_s32(xb, x))));
  threes = vaddq_s32(xtl, vaddq_s32(xtr, vaddq_s32(xbr, xbl)));
  res = vsubq_s32(vshlq_n_s32(vaddq_s32(fours, threes), 2), threes);
  return res;
}

static INLINE void cross_sum_inp_u16(uint16_t *buf, int buf_stride,
                                     int32x4_t *a0, int32x4_t *a1) {
  uint16x8_t xtr, xt, xtl, xl, x, xr, xbr, xb, xbl;
  uint16x8_t r0, r1;

  xtl = vld1q_u16(buf - buf_stride - 1);
  xt = vld1q_u16(buf - buf_stride);
  xtr = vld1q_u16(buf - buf_stride + 1);
  xl = vld1q_u16(buf - 1);
  x = vld1q_u16(buf);
  xr = vld1q_u16(buf + 1);
  xbl = vld1q_u16(buf + buf_stride - 1);
  xb = vld1q_u16(buf + buf_stride);
  xbr = vld1q_u16(buf + buf_stride + 1);

  xb = vaddq_u16(xb, x);
  xt = vaddq_u16(xt, xr);
  xl = vaddq_u16(xl, xb);
  xl = vaddq_u16(xl, xt);

  r0 = vshlq_n_u16(xl, 2);

  xbl = vaddq_u16(xbl, xbr);
  xtl = vaddq_u16(xtl, xtr);
  xtl = vaddq_u16(xtl, xbl);

  r1 = vshlq_n_u16(xtl, 2);
  r1 = vsubq_u16(r1, xtl);

  *a0 = vreinterpretq_s32_u32(
      vaddq_u32(vmovl_u16(vget_low_u16(r0)), vmovl_u16(vget_low_u16(r1))));
  *a1 = vreinterpretq_s32_u32(
      vaddq_u32(vmovl_u16(vget_high_u16(r0)), vmovl_u16(vget_high_u16(r1))));
}

static INLINE int32x4_t cross_sum_fast_even_row(int32_t *buf, int buf_stride) {
  int32x4_t xtr, xt, xtl, xbr, xb, xbl;
  int32x4_t fives, sixes, fives_plus_sixes;

  xtl = vld1q_s32(buf - buf_stride - 1);
  xt = vld1q_s32(buf - buf_stride);
  xtr = vld1q_s32(buf - buf_stride + 1);
  xbl = vld1q_s32(buf + buf_stride - 1);
  xb = vld1q_s32(buf + buf_stride);
  xbr = vld1q_s32(buf + buf_stride + 1);

  fives = vaddq_s32(xtl, vaddq_s32(xtr, vaddq_s32(xbr, xbl)));
  sixes = vaddq_s32(xt, xb);
  fives_plus_sixes = vaddq_s32(fives, sixes);

  return vaddq_s32(
      vaddq_s32(vshlq_n_s32(fives_plus_sixes, 2), fives_plus_sixes), sixes);
}

static INLINE void cross_sum_fast_even_row_inp16(uint16_t *buf, int buf_stride,
                                                 int32x4_t *a0, int32x4_t *a1) {
  uint16x8_t xtr, xt, xtl, xbr, xb, xbl, xb0;

  xtl = vld1q_u16(buf - buf_stride - 1);
  xt = vld1q_u16(buf - buf_stride);
  xtr = vld1q_u16(buf - buf_stride + 1);
  xbl = vld1q_u16(buf + buf_stride - 1);
  xb = vld1q_u16(buf + buf_stride);
  xbr = vld1q_u16(buf + buf_stride + 1);

  xbr = vaddq_u16(xbr, xbl);
  xtr = vaddq_u16(xtr, xtl);
  xbr = vaddq_u16(xbr, xtr);
  xtl = vshlq_n_u16(xbr, 2);
  xbr = vaddq_u16(xtl, xbr);

  xb = vaddq_u16(xb, xt);
  xb0 = vshlq_n_u16(xb, 1);
  xb = vshlq_n_u16(xb, 2);
  xb = vaddq_u16(xb, xb0);

  *a0 = vreinterpretq_s32_u32(
      vaddq_u32(vmovl_u16(vget_low_u16(xbr)), vmovl_u16(vget_low_u16(xb))));
  *a1 = vreinterpretq_s32_u32(
      vaddq_u32(vmovl_u16(vget_high_u16(xbr)), vmovl_u16(vget_high_u16(xb))));
}

static INLINE int32x4_t cross_sum_fast_odd_row(int32_t *buf) {
  int32x4_t xl, x, xr;
  int32x4_t fives, sixes, fives_plus_sixes;

  xl = vld1q_s32(buf - 1);
  x = vld1q_s32(buf);
  xr = vld1q_s32(buf + 1);
  fives = vaddq_s32(xl, xr);
  sixes = x;
  fives_plus_sixes = vaddq_s32(fives, sixes);

  return vaddq_s32(
      vaddq_s32(vshlq_n_s32(fives_plus_sixes, 2), fives_plus_sixes), sixes);
}

static INLINE void cross_sum_fast_odd_row_inp16(uint16_t *buf, int32x4_t *a0,
                                                int32x4_t *a1) {
  uint16x8_t xl, x, xr;
  uint16x8_t x0;

  xl = vld1q_u16(buf - 1);
  x = vld1q_u16(buf);
  xr = vld1q_u16(buf + 1);
  xl = vaddq_u16(xl, xr);
  x0 = vshlq_n_u16(xl, 2);
  xl = vaddq_u16(xl, x0);

  x0 = vshlq_n_u16(x, 1);
  x = vshlq_n_u16(x, 2);
  x = vaddq_u16(x, x0);

  *a0 = vreinterpretq_s32_u32(
      vaddq_u32(vmovl_u16(vget_low_u16(xl)), vmovl_u16(vget_low_u16(x))));
  *a1 = vreinterpretq_s32_u32(
      vaddq_u32(vmovl_u16(vget_high_u16(xl)), vmovl_u16(vget_high_u16(x))));
}

static void final_filter_fast_internal(uint16_t *A, int32_t *B,
                                       const int buf_stride, int16_t *src,
                                       const int src_stride, int32_t *dst,
                                       const int dst_stride, const int width,
                                       const int height) {
  int16x8_t s0;
  int32_t *B_tmp, *dst_ptr;
  uint16_t *A_tmp;
  int16_t *src_ptr;
  int32x4_t a_res0, a_res1, b_res0, b_res1;
  int w, h, count = 0;
  assert(SGRPROJ_SGR_BITS == 8);
  assert(SGRPROJ_RST_BITS == 4);

  A_tmp = A;
  B_tmp = B;
  src_ptr = src;
  dst_ptr = dst;
  h = height;
  do {
    A_tmp = (A + count * buf_stride);
    B_tmp = (B + count * buf_stride);
    src_ptr = (src + count * src_stride);
    dst_ptr = (dst + count * dst_stride);
    w = width;
    if (!(count & 1)) {
      do {
        s0 = vld1q_s16(src_ptr);
        cross_sum_fast_even_row_inp16(A_tmp, buf_stride, &a_res0, &a_res1);
        a_res0 = vmulq_s32(vmovl_s16(vget_low_s16(s0)), a_res0);
        a_res1 = vmulq_s32(vmovl_s16(vget_high_s16(s0)), a_res1);

        b_res0 = cross_sum_fast_even_row(B_tmp, buf_stride);
        b_res1 = cross_sum_fast_even_row(B_tmp + 4, buf_stride);
        a_res0 = vaddq_s32(a_res0, b_res0);
        a_res1 = vaddq_s32(a_res1, b_res1);

        a_res0 =
            vrshrq_n_s32(a_res0, SGRPROJ_SGR_BITS + NB_EVEN - SGRPROJ_RST_BITS);
        a_res1 =
            vrshrq_n_s32(a_res1, SGRPROJ_SGR_BITS + NB_EVEN - SGRPROJ_RST_BITS);

        vst1q_s32(dst_ptr, a_res0);
        vst1q_s32(dst_ptr + 4, a_res1);

        A_tmp += 8;
        B_tmp += 8;
        src_ptr += 8;
        dst_ptr += 8;
        w -= 8;
      } while (w > 0);
    } else {
      do {
        s0 = vld1q_s16(src_ptr);
        cross_sum_fast_odd_row_inp16(A_tmp, &a_res0, &a_res1);
        a_res0 = vmulq_s32(vmovl_s16(vget_low_s16(s0)), a_res0);
        a_res1 = vmulq_s32(vmovl_s16(vget_high_s16(s0)), a_res1);

        b_res0 = cross_sum_fast_odd_row(B_tmp);
        b_res1 = cross_sum_fast_odd_row(B_tmp + 4);
        a_res0 = vaddq_s32(a_res0, b_res0);
        a_res1 = vaddq_s32(a_res1, b_res1);

        a_res0 =
            vrshrq_n_s32(a_res0, SGRPROJ_SGR_BITS + NB_ODD - SGRPROJ_RST_BITS);
        a_res1 =
            vrshrq_n_s32(a_res1, SGRPROJ_SGR_BITS + NB_ODD - SGRPROJ_RST_BITS);

        vst1q_s32(dst_ptr, a_res0);
        vst1q_s32(dst_ptr + 4, a_res1);

        A_tmp += 8;
        B_tmp += 8;
        src_ptr += 8;
        dst_ptr += 8;
        w -= 8;
      } while (w > 0);
    }
    count++;
    h -= 1;
  } while (h > 0);
}

void final_filter_internal(uint16_t *A, int32_t *B, const int buf_stride,
                           int16_t *src, const int src_stride, int32_t *dst,
                           const int dst_stride, const int width,
                           const int height) {
  int16x8_t s0;
  int32_t *B_tmp, *dst_ptr;
  uint16_t *A_tmp;
  int16_t *src_ptr;
  int32x4_t a_res0, a_res1, b_res0, b_res1;
  int w, h, count = 0;

  assert(SGRPROJ_SGR_BITS == 8);
  assert(SGRPROJ_RST_BITS == 4);
  h = height;

  do {
    A_tmp = (A + count * buf_stride);
    B_tmp = (B + count * buf_stride);
    src_ptr = (src + count * src_stride);
    dst_ptr = (dst + count * dst_stride);
    w = width;
    do {
      s0 = vld1q_s16(src_ptr);
      cross_sum_inp_u16(A_tmp, buf_stride, &a_res0, &a_res1);
      a_res0 = vmulq_s32(vmovl_s16(vget_low_s16(s0)), a_res0);
      a_res1 = vmulq_s32(vmovl_s16(vget_high_s16(s0)), a_res1);

      b_res0 = cross_sum_inp_s32(B_tmp, buf_stride);
      b_res1 = cross_sum_inp_s32(B_tmp + 4, buf_stride);
      a_res0 = vaddq_s32(a_res0, b_res0);
      a_res1 = vaddq_s32(a_res1, b_res1);

      a_res0 =
          vrshrq_n_s32(a_res0, SGRPROJ_SGR_BITS + NB_EVEN - SGRPROJ_RST_BITS);
      a_res1 =
          vrshrq_n_s32(a_res1, SGRPROJ_SGR_BITS + NB_EVEN - SGRPROJ_RST_BITS);
      vst1q_s32(dst_ptr, a_res0);
      vst1q_s32(dst_ptr + 4, a_res1);

      A_tmp += 8;
      B_tmp += 8;
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
    count++;
    h -= 1;
  } while (h > 0);
}

static INLINE void restoration_fast_internal(uint16_t *dgd16, int width,
                                             int height, int dgd_stride,
                                             int32_t *dst, int dst_stride,
                                             int bit_depth, int sgr_params_idx,
                                             int radius_idx) {
  const sgr_params_type *const params = &av1_sgr_params[sgr_params_idx];
  const int r = params->r[radius_idx];
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;

  const int buf_stride = ((width_ext + 3) & ~3) + 16;
  int32_t A_[RESTORATION_PROC_UNIT_PELS];
  uint16_t A16_[RESTORATION_PROC_UNIT_PELS];
  int32_t B_[RESTORATION_PROC_UNIT_PELS];
  int32_t *square_sum_buf = A_;
  int32_t *sum_buf = B_;
  uint16_t *tmp16_buf = A16_;

  assert(r <= MAX_RADIUS && "Need MAX_RADIUS >= r");
  assert(r <= SGRPROJ_BORDER_VERT - 1 && r <= SGRPROJ_BORDER_HORZ - 1 &&
         "Need SGRPROJ_BORDER_* >= r+1");

  assert(radius_idx == 0);
  assert(r == 2);

  // input(dgd16) is 16bit.
  // sum of pixels 1st stage output will be in 16bit(tmp16_buf). End output is
  // kept in 32bit [sum_buf]. sum of squares output is kept in 32bit
  // buffer(square_sum_buf).
  boxsum2((int16_t *)(dgd16 - dgd_stride * SGRPROJ_BORDER_VERT -
                      SGRPROJ_BORDER_HORZ),
          dgd_stride, (int16_t *)tmp16_buf, sum_buf, square_sum_buf, buf_stride,
          width_ext, height_ext);

  square_sum_buf += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  sum_buf += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  tmp16_buf += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;

  // Calculation of a, b. a output is in 16bit tmp_buf which is in range of
  // [1, 256] for all bit depths. b output is kept in 32bit buffer.

#if CONFIG_AV1_HIGHBITDEPTH
  if (bit_depth > 8) {
    calc_ab_fast_internal_hbd(
        (square_sum_buf - buf_stride - 1), (tmp16_buf - buf_stride - 1),
        (sum_buf - buf_stride - 1), buf_stride * 2, width + 2, height + 2,
        bit_depth, r, params->s[radius_idx], 2);
  } else {
    calc_ab_fast_internal_lbd(
        (square_sum_buf - buf_stride - 1), (tmp16_buf - buf_stride - 1),
        (sum_buf - buf_stride - 1), buf_stride * 2, width + 2, height + 2, r,
        params->s[radius_idx], 2);
  }
#else
  (void)bit_depth;
  calc_ab_fast_internal_lbd((square_sum_buf - buf_stride - 1),
                            (tmp16_buf - buf_stride - 1),
                            (sum_buf - buf_stride - 1), buf_stride * 2,
                            width + 2, height + 2, r, params->s[radius_idx], 2);
#endif
  final_filter_fast_internal(tmp16_buf, sum_buf, buf_stride, (int16_t *)dgd16,
                             dgd_stride, dst, dst_stride, width, height);
}

static INLINE void restoration_internal(uint16_t *dgd16, int width, int height,
                                        int dgd_stride, int32_t *dst,
                                        int dst_stride, int bit_depth,
                                        int sgr_params_idx, int radius_idx) {
  const sgr_params_type *const params = &av1_sgr_params[sgr_params_idx];
  const int r = params->r[radius_idx];
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;

  int buf_stride = ((width_ext + 3) & ~3) + 16;
  int32_t A_[RESTORATION_PROC_UNIT_PELS];
  uint16_t A16_[RESTORATION_PROC_UNIT_PELS];
  uint16_t B16_[RESTORATION_PROC_UNIT_PELS];
  int32_t B_[RESTORATION_PROC_UNIT_PELS];
  int32_t *square_sum_buf = A_;
  uint16_t *sum_buf = B16_;
  uint16_t *A16 = A16_;
  int32_t *B = B_;

  assert(r <= MAX_RADIUS && "Need MAX_RADIUS >= r");
  assert(r <= SGRPROJ_BORDER_VERT - 1 && r <= SGRPROJ_BORDER_HORZ - 1 &&
         "Need SGRPROJ_BORDER_* >= r+1");

  assert(radius_idx == 1);
  assert(r == 1);

  // input(dgd16) is 16bit.
  // sum of pixels output will be in 16bit(sum_buf).
  // sum of squares output is kept in 32bit buffer(square_sum_buf).
  boxsum1((int16_t *)(dgd16 - dgd_stride * SGRPROJ_BORDER_VERT -
                      SGRPROJ_BORDER_HORZ),
          dgd_stride, sum_buf, square_sum_buf, buf_stride, width_ext,
          height_ext);

  square_sum_buf += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  B += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  A16 += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  sum_buf += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;

#if CONFIG_AV1_HIGHBITDEPTH
  // Calculation of a, b. a output is in 16bit tmp_buf which is in range of
  // [1, 256] for all bit depths. b output is kept in 32bit buffer.
  if (bit_depth > 8) {
    calc_ab_internal_hbd((square_sum_buf - buf_stride - 1),
                         (A16 - buf_stride - 1), (sum_buf - buf_stride - 1),
                         (B - buf_stride - 1), buf_stride, width + 2,
                         height + 2, bit_depth, r, params->s[radius_idx], 1);
  } else {
    calc_ab_internal_lbd((square_sum_buf - buf_stride - 1),
                         (A16 - buf_stride - 1), (sum_buf - buf_stride - 1),
                         (B - buf_stride - 1), buf_stride, width + 2,
                         height + 2, r, params->s[radius_idx], 1);
  }
#else
  (void)bit_depth;
  calc_ab_internal_lbd((square_sum_buf - buf_stride - 1),
                       (A16 - buf_stride - 1), (sum_buf - buf_stride - 1),
                       (B - buf_stride - 1), buf_stride, width + 2, height + 2,
                       r, params->s[radius_idx], 1);
#endif
  final_filter_internal(A16, B, buf_stride, (int16_t *)dgd16, dgd_stride, dst,
                        dst_stride, width, height);
}

static INLINE void src_convert_u8_to_u16(const uint8_t *src,
                                         const int src_stride, uint16_t *dst,
                                         const int dst_stride, const int width,
                                         const int height) {
  const uint8_t *src_ptr;
  uint16_t *dst_ptr;
  int h, w, count = 0;

  uint8x8_t t1, t2, t3, t4;
  uint16x8_t s1, s2, s3, s4;
  h = height;
  do {
    src_ptr = src + (count << 2) * src_stride;
    dst_ptr = dst + (count << 2) * dst_stride;
    w = width;
    if (w >= 7) {
      do {
        load_u8_8x4(src_ptr, src_stride, &t1, &t2, &t3, &t4);
        s1 = vmovl_u8(t1);
        s2 = vmovl_u8(t2);
        s3 = vmovl_u8(t3);
        s4 = vmovl_u8(t4);
        store_u16_8x4(dst_ptr, dst_stride, s1, s2, s3, s4);

        src_ptr += 8;
        dst_ptr += 8;
        w -= 8;
      } while (w > 7);
    }

    for (int y = 0; y < w; y++) {
      dst_ptr[y] = src_ptr[y];
      dst_ptr[y + 1 * dst_stride] = src_ptr[y + 1 * src_stride];
      dst_ptr[y + 2 * dst_stride] = src_ptr[y + 2 * src_stride];
      dst_ptr[y + 3 * dst_stride] = src_ptr[y + 3 * src_stride];
    }
    count++;
    h -= 4;
  } while (h > 3);

  src_ptr = src + (count << 2) * src_stride;
  dst_ptr = dst + (count << 2) * dst_stride;
  for (int x = 0; x < h; x++) {
    for (int y = 0; y < width; y++) {
      dst_ptr[y + x * dst_stride] = src_ptr[y + x * src_stride];
    }
  }

  // memset uninitialized rows of src buffer as they are needed for the
  // boxsum filter calculation.
  for (int x = height; x < height + 5; x++)
    memset(dst + x * dst_stride, 0, (width + 2) * sizeof(*dst));
}

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE void src_convert_hbd_copy(const uint16_t *src, int src_stride,
                                        uint16_t *dst, const int dst_stride,
                                        int width, int height) {
  const uint16_t *src_ptr;
  uint16_t *dst_ptr;
  int h, w, count = 0;
  uint16x8_t s1, s2, s3, s4;

  h = height;
  do {
    src_ptr = src + (count << 2) * src_stride;
    dst_ptr = dst + (count << 2) * dst_stride;
    w = width;
    do {
      load_u16_8x4(src_ptr, src_stride, &s1, &s2, &s3, &s4);
      store_u16_8x4(dst_ptr, dst_stride, s1, s2, s3, s4);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 7);

    for (int y = 0; y < w; y++) {
      dst_ptr[y] = src_ptr[y];
      dst_ptr[y + 1 * dst_stride] = src_ptr[y + 1 * src_stride];
      dst_ptr[y + 2 * dst_stride] = src_ptr[y + 2 * src_stride];
      dst_ptr[y + 3 * dst_stride] = src_ptr[y + 3 * src_stride];
    }
    count++;
    h -= 4;
  } while (h > 3);

  src_ptr = src + (count << 2) * src_stride;
  dst_ptr = dst + (count << 2) * dst_stride;

  for (int x = 0; x < h; x++) {
    memcpy((dst_ptr + x * dst_stride), (src_ptr + x * src_stride),
           sizeof(uint16_t) * width);
  }
  // memset uninitialized rows of src buffer as they are needed for the
  // boxsum filter calculation.
  for (int x = height; x < height + 5; x++)
    memset(dst + x * dst_stride, 0, (width + 2) * sizeof(*dst));
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

int av1_selfguided_restoration_neon(const uint8_t *dat8, int width, int height,
                                    int stride, int32_t *flt0, int32_t *flt1,
                                    int flt_stride, int sgr_params_idx,
                                    int bit_depth, int highbd) {
  const sgr_params_type *const params = &av1_sgr_params[sgr_params_idx];
  assert(!(params->r[0] == 0 && params->r[1] == 0));

  uint16_t dgd16_[RESTORATION_PROC_UNIT_PELS];
  const int dgd16_stride = width + 2 * SGRPROJ_BORDER_HORZ;
  uint16_t *dgd16 =
      dgd16_ + dgd16_stride * SGRPROJ_BORDER_VERT + SGRPROJ_BORDER_HORZ;
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;
  const int dgd_stride = stride;

#if CONFIG_AV1_HIGHBITDEPTH
  if (highbd) {
    const uint16_t *dgd16_tmp = CONVERT_TO_SHORTPTR(dat8);
    src_convert_hbd_copy(
        dgd16_tmp - SGRPROJ_BORDER_VERT * dgd_stride - SGRPROJ_BORDER_HORZ,
        dgd_stride,
        dgd16 - SGRPROJ_BORDER_VERT * dgd16_stride - SGRPROJ_BORDER_HORZ,
        dgd16_stride, width_ext, height_ext);
  } else {
    src_convert_u8_to_u16(
        dat8 - SGRPROJ_BORDER_VERT * dgd_stride - SGRPROJ_BORDER_HORZ,
        dgd_stride,
        dgd16 - SGRPROJ_BORDER_VERT * dgd16_stride - SGRPROJ_BORDER_HORZ,
        dgd16_stride, width_ext, height_ext);
  }
#else
  (void)highbd;
  src_convert_u8_to_u16(
      dat8 - SGRPROJ_BORDER_VERT * dgd_stride - SGRPROJ_BORDER_HORZ, dgd_stride,
      dgd16 - SGRPROJ_BORDER_VERT * dgd16_stride - SGRPROJ_BORDER_HORZ,
      dgd16_stride, width_ext, height_ext);
#endif

  if (params->r[0] > 0)
    restoration_fast_internal(dgd16, width, height, dgd16_stride, flt0,
                              flt_stride, bit_depth, sgr_params_idx, 0);
  if (params->r[1] > 0)
    restoration_internal(dgd16, width, height, dgd16_stride, flt1, flt_stride,
                         bit_depth, sgr_params_idx, 1);
  return 0;
}

void av1_apply_selfguided_restoration_neon(const uint8_t *dat8, int width,
                                           int height, int stride, int eps,
                                           const int *xqd, uint8_t *dst8,
                                           int dst_stride, int32_t *tmpbuf,
                                           int bit_depth, int highbd) {
  int32_t *flt0 = tmpbuf;
  int32_t *flt1 = flt0 + RESTORATION_UNITPELS_MAX;
  assert(width * height <= RESTORATION_UNITPELS_MAX);
  uint16_t dgd16_[RESTORATION_PROC_UNIT_PELS];
  const int dgd16_stride = width + 2 * SGRPROJ_BORDER_HORZ;
  uint16_t *dgd16 =
      dgd16_ + dgd16_stride * SGRPROJ_BORDER_VERT + SGRPROJ_BORDER_HORZ;
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;
  const int dgd_stride = stride;
  const sgr_params_type *const params = &av1_sgr_params[eps];
  int xq[2];

  assert(!(params->r[0] == 0 && params->r[1] == 0));

#if CONFIG_AV1_HIGHBITDEPTH
  if (highbd) {
    const uint16_t *dgd16_tmp = CONVERT_TO_SHORTPTR(dat8);
    src_convert_hbd_copy(
        dgd16_tmp - SGRPROJ_BORDER_VERT * dgd_stride - SGRPROJ_BORDER_HORZ,
        dgd_stride,
        dgd16 - SGRPROJ_BORDER_VERT * dgd16_stride - SGRPROJ_BORDER_HORZ,
        dgd16_stride, width_ext, height_ext);
  } else {
    src_convert_u8_to_u16(
        dat8 - SGRPROJ_BORDER_VERT * dgd_stride - SGRPROJ_BORDER_HORZ,
        dgd_stride,
        dgd16 - SGRPROJ_BORDER_VERT * dgd16_stride - SGRPROJ_BORDER_HORZ,
        dgd16_stride, width_ext, height_ext);
  }
#else
  (void)highbd;
  src_convert_u8_to_u16(
      dat8 - SGRPROJ_BORDER_VERT * dgd_stride - SGRPROJ_BORDER_HORZ, dgd_stride,
      dgd16 - SGRPROJ_BORDER_VERT * dgd16_stride - SGRPROJ_BORDER_HORZ,
      dgd16_stride, width_ext, height_ext);
#endif
  if (params->r[0] > 0)
    restoration_fast_internal(dgd16, width, height, dgd16_stride, flt0, width,
                              bit_depth, eps, 0);
  if (params->r[1] > 0)
    restoration_internal(dgd16, width, height, dgd16_stride, flt1, width,
                         bit_depth, eps, 1);

  av1_decode_xq(xqd, xq, params);

  {
    int16_t *src_ptr;
    uint8_t *dst_ptr;
#if CONFIG_AV1_HIGHBITDEPTH
    uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst8);
    uint16_t *dst16_ptr;
#endif
    int16x4_t d0, d4;
    int16x8_t r0, s0;
    uint16x8_t r4;
    int32x4_t u0, u4, v0, v4, f00, f10;
    uint8x8_t t0;
    int count = 0, w = width, h = height, rc = 0;

    const int32x4_t xq0_vec = vdupq_n_s32(xq[0]);
    const int32x4_t xq1_vec = vdupq_n_s32(xq[1]);
    const int16x8_t zero = vdupq_n_s16(0);
    const uint16x8_t max = vdupq_n_u16((1 << bit_depth) - 1);
    src_ptr = (int16_t *)dgd16;
    do {
      w = width;
      count = 0;
      dst_ptr = dst8 + rc * dst_stride;
#if CONFIG_AV1_HIGHBITDEPTH
      dst16_ptr = dst16 + rc * dst_stride;
#endif
      do {
        s0 = vld1q_s16(src_ptr + count);

        u0 = vshll_n_s16(vget_low_s16(s0), SGRPROJ_RST_BITS);
        u4 = vshll_n_s16(vget_high_s16(s0), SGRPROJ_RST_BITS);

        v0 = vshlq_n_s32(u0, SGRPROJ_PRJ_BITS);
        v4 = vshlq_n_s32(u4, SGRPROJ_PRJ_BITS);

        if (params->r[0] > 0) {
          f00 = vld1q_s32(flt0 + count);
          f10 = vld1q_s32(flt0 + count + 4);

          f00 = vsubq_s32(f00, u0);
          f10 = vsubq_s32(f10, u4);

          v0 = vmlaq_s32(v0, xq0_vec, f00);
          v4 = vmlaq_s32(v4, xq0_vec, f10);
        }

        if (params->r[1] > 0) {
          f00 = vld1q_s32(flt1 + count);
          f10 = vld1q_s32(flt1 + count + 4);

          f00 = vsubq_s32(f00, u0);
          f10 = vsubq_s32(f10, u4);

          v0 = vmlaq_s32(v0, xq1_vec, f00);
          v4 = vmlaq_s32(v4, xq1_vec, f10);
        }

        d0 = vqrshrn_n_s32(v0, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
        d4 = vqrshrn_n_s32(v4, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);

        r0 = vcombine_s16(d0, d4);

        r4 = vreinterpretq_u16_s16(vmaxq_s16(r0, zero));

#if CONFIG_AV1_HIGHBITDEPTH
        if (highbd) {
          r4 = vminq_u16(r4, max);
          vst1q_u16(dst16_ptr, r4);
          dst16_ptr += 8;
        } else {
          t0 = vqmovn_u16(r4);
          vst1_u8(dst_ptr, t0);
          dst_ptr += 8;
        }
#else
        (void)max;
        t0 = vqmovn_u16(r4);
        vst1_u8(dst_ptr, t0);
        dst_ptr += 8;
#endif
        w -= 8;
        count += 8;
      } while (w > 0);

      src_ptr += dgd16_stride;
      flt1 += width;
      flt0 += width;
      rc++;
      h--;
    } while (h > 0);
  }
}
