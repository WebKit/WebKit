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

#include "aom_dsp/txfm_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"

static void aom_fdct4x4_helper(const int16_t *input, int stride,
                               int16x4_t *input_0, int16x4_t *input_1,
                               int16x4_t *input_2, int16x4_t *input_3) {
  *input_0 = vshl_n_s16(vld1_s16(input + 0 * stride), 4);
  *input_1 = vshl_n_s16(vld1_s16(input + 1 * stride), 4);
  *input_2 = vshl_n_s16(vld1_s16(input + 2 * stride), 4);
  *input_3 = vshl_n_s16(vld1_s16(input + 3 * stride), 4);
  // If the very first value != 0, then add 1.
  if (input[0] != 0) {
    const int16x4_t one = vreinterpret_s16_s64(vdup_n_s64(1));
    *input_0 = vadd_s16(*input_0, one);
  }

  for (int i = 0; i < 2; ++i) {
    const int16x8_t input_01 = vcombine_s16(*input_0, *input_1);
    const int16x8_t input_32 = vcombine_s16(*input_3, *input_2);

    // in_0 +/- in_3, in_1 +/- in_2
    const int16x8_t s_01 = vaddq_s16(input_01, input_32);
    const int16x8_t s_32 = vsubq_s16(input_01, input_32);

    // step_0 +/- step_1, step_2 +/- step_3
    const int16x4_t s_0 = vget_low_s16(s_01);
    const int16x4_t s_1 = vget_high_s16(s_01);
    const int16x4_t s_2 = vget_high_s16(s_32);
    const int16x4_t s_3 = vget_low_s16(s_32);

    // (s_0 +/- s_1) * cospi_16_64
    // Must expand all elements to s32. See 'needs32' comment in fwd_txfm.c.
    const int32x4_t s_0_p_s_1 = vaddl_s16(s_0, s_1);
    const int32x4_t s_0_m_s_1 = vsubl_s16(s_0, s_1);
    const int32x4_t temp1 = vmulq_n_s32(s_0_p_s_1, cospi_16_64);
    const int32x4_t temp2 = vmulq_n_s32(s_0_m_s_1, cospi_16_64);

    // fdct_round_shift
    int16x4_t out_0 = vrshrn_n_s32(temp1, DCT_CONST_BITS);
    int16x4_t out_2 = vrshrn_n_s32(temp2, DCT_CONST_BITS);

    // s_3 * cospi_8_64 + s_2 * cospi_24_64
    // s_3 * cospi_24_64 - s_2 * cospi_8_64
    const int32x4_t s_3_cospi_8_64 = vmull_n_s16(s_3, cospi_8_64);
    const int32x4_t s_3_cospi_24_64 = vmull_n_s16(s_3, cospi_24_64);

    const int32x4_t temp3 = vmlal_n_s16(s_3_cospi_8_64, s_2, cospi_24_64);
    const int32x4_t temp4 = vmlsl_n_s16(s_3_cospi_24_64, s_2, cospi_8_64);

    // fdct_round_shift
    int16x4_t out_1 = vrshrn_n_s32(temp3, DCT_CONST_BITS);
    int16x4_t out_3 = vrshrn_n_s32(temp4, DCT_CONST_BITS);

    transpose_s16_4x4d(&out_0, &out_1, &out_2, &out_3);

    *input_0 = out_0;
    *input_1 = out_1;
    *input_2 = out_2;
    *input_3 = out_3;
  }
}

void aom_fdct4x4_neon(const int16_t *input, tran_low_t *final_output,
                      int stride) {
  // input[M * stride] * 16
  int16x4_t input_0, input_1, input_2, input_3;

  aom_fdct4x4_helper(input, stride, &input_0, &input_1, &input_2, &input_3);

  // Not quite a rounding shift. Only add 1 despite shifting by 2.
  const int16x8_t one = vdupq_n_s16(1);
  int16x8_t out_01 = vcombine_s16(input_0, input_1);
  int16x8_t out_23 = vcombine_s16(input_2, input_3);
  out_01 = vshrq_n_s16(vaddq_s16(out_01, one), 2);
  out_23 = vshrq_n_s16(vaddq_s16(out_23, one), 2);
  store_s16q_to_tran_low(final_output + 0 * 8, out_01);
  store_s16q_to_tran_low(final_output + 1 * 8, out_23);
}

void aom_fdct4x4_lp_neon(const int16_t *input, int16_t *final_output,
                         int stride) {
  // input[M * stride] * 16
  int16x4_t input_0, input_1, input_2, input_3;

  aom_fdct4x4_helper(input, stride, &input_0, &input_1, &input_2, &input_3);

  // Not quite a rounding shift. Only add 1 despite shifting by 2.
  const int16x8_t one = vdupq_n_s16(1);
  int16x8_t out_01 = vcombine_s16(input_0, input_1);
  int16x8_t out_23 = vcombine_s16(input_2, input_3);
  out_01 = vshrq_n_s16(vaddq_s16(out_01, one), 2);
  out_23 = vshrq_n_s16(vaddq_s16(out_23, one), 2);
  vst1q_s16(final_output + 0 * 8, out_01);
  vst1q_s16(final_output + 1 * 8, out_23);
}

void aom_fdct8x8_neon(const int16_t *input, int16_t *final_output, int stride) {
  // stage 1
  int16x8_t input_0 = vshlq_n_s16(vld1q_s16(&input[0 * stride]), 2);
  int16x8_t input_1 = vshlq_n_s16(vld1q_s16(&input[1 * stride]), 2);
  int16x8_t input_2 = vshlq_n_s16(vld1q_s16(&input[2 * stride]), 2);
  int16x8_t input_3 = vshlq_n_s16(vld1q_s16(&input[3 * stride]), 2);
  int16x8_t input_4 = vshlq_n_s16(vld1q_s16(&input[4 * stride]), 2);
  int16x8_t input_5 = vshlq_n_s16(vld1q_s16(&input[5 * stride]), 2);
  int16x8_t input_6 = vshlq_n_s16(vld1q_s16(&input[6 * stride]), 2);
  int16x8_t input_7 = vshlq_n_s16(vld1q_s16(&input[7 * stride]), 2);
  for (int i = 0; i < 2; ++i) {
    int16x8_t out_0, out_1, out_2, out_3, out_4, out_5, out_6, out_7;
    const int16x8_t v_s0 = vaddq_s16(input_0, input_7);
    const int16x8_t v_s1 = vaddq_s16(input_1, input_6);
    const int16x8_t v_s2 = vaddq_s16(input_2, input_5);
    const int16x8_t v_s3 = vaddq_s16(input_3, input_4);
    const int16x8_t v_s4 = vsubq_s16(input_3, input_4);
    const int16x8_t v_s5 = vsubq_s16(input_2, input_5);
    const int16x8_t v_s6 = vsubq_s16(input_1, input_6);
    const int16x8_t v_s7 = vsubq_s16(input_0, input_7);
    // fdct4(step, step);
    int16x8_t v_x0 = vaddq_s16(v_s0, v_s3);
    int16x8_t v_x1 = vaddq_s16(v_s1, v_s2);
    int16x8_t v_x2 = vsubq_s16(v_s1, v_s2);
    int16x8_t v_x3 = vsubq_s16(v_s0, v_s3);
    // fdct4(step, step);
    int32x4_t v_t0_lo = vaddl_s16(vget_low_s16(v_x0), vget_low_s16(v_x1));
    int32x4_t v_t0_hi = vaddl_s16(vget_high_s16(v_x0), vget_high_s16(v_x1));
    int32x4_t v_t1_lo = vsubl_s16(vget_low_s16(v_x0), vget_low_s16(v_x1));
    int32x4_t v_t1_hi = vsubl_s16(vget_high_s16(v_x0), vget_high_s16(v_x1));
    int32x4_t v_t2_lo = vmull_n_s16(vget_low_s16(v_x2), (int16_t)cospi_24_64);
    int32x4_t v_t2_hi = vmull_n_s16(vget_high_s16(v_x2), (int16_t)cospi_24_64);
    int32x4_t v_t3_lo = vmull_n_s16(vget_low_s16(v_x3), (int16_t)cospi_24_64);
    int32x4_t v_t3_hi = vmull_n_s16(vget_high_s16(v_x3), (int16_t)cospi_24_64);
    v_t2_lo = vmlal_n_s16(v_t2_lo, vget_low_s16(v_x3), (int16_t)cospi_8_64);
    v_t2_hi = vmlal_n_s16(v_t2_hi, vget_high_s16(v_x3), (int16_t)cospi_8_64);
    v_t3_lo = vmlsl_n_s16(v_t3_lo, vget_low_s16(v_x2), (int16_t)cospi_8_64);
    v_t3_hi = vmlsl_n_s16(v_t3_hi, vget_high_s16(v_x2), (int16_t)cospi_8_64);
    v_t0_lo = vmulq_n_s32(v_t0_lo, (int32_t)cospi_16_64);
    v_t0_hi = vmulq_n_s32(v_t0_hi, (int32_t)cospi_16_64);
    v_t1_lo = vmulq_n_s32(v_t1_lo, (int32_t)cospi_16_64);
    v_t1_hi = vmulq_n_s32(v_t1_hi, (int32_t)cospi_16_64);
    {
      const int16x4_t a = vrshrn_n_s32(v_t0_lo, DCT_CONST_BITS);
      const int16x4_t b = vrshrn_n_s32(v_t0_hi, DCT_CONST_BITS);
      const int16x4_t c = vrshrn_n_s32(v_t1_lo, DCT_CONST_BITS);
      const int16x4_t d = vrshrn_n_s32(v_t1_hi, DCT_CONST_BITS);
      const int16x4_t e = vrshrn_n_s32(v_t2_lo, DCT_CONST_BITS);
      const int16x4_t f = vrshrn_n_s32(v_t2_hi, DCT_CONST_BITS);
      const int16x4_t g = vrshrn_n_s32(v_t3_lo, DCT_CONST_BITS);
      const int16x4_t h = vrshrn_n_s32(v_t3_hi, DCT_CONST_BITS);
      out_0 = vcombine_s16(a, c);  // 00 01 02 03 40 41 42 43
      out_2 = vcombine_s16(e, g);  // 20 21 22 23 60 61 62 63
      out_4 = vcombine_s16(b, d);  // 04 05 06 07 44 45 46 47
      out_6 = vcombine_s16(f, h);  // 24 25 26 27 64 65 66 67
    }
    // Stage 2
    v_x0 = vsubq_s16(v_s6, v_s5);
    v_x1 = vaddq_s16(v_s6, v_s5);
    v_t0_lo = vmull_n_s16(vget_low_s16(v_x0), (int16_t)cospi_16_64);
    v_t0_hi = vmull_n_s16(vget_high_s16(v_x0), (int16_t)cospi_16_64);
    v_t1_lo = vmull_n_s16(vget_low_s16(v_x1), (int16_t)cospi_16_64);
    v_t1_hi = vmull_n_s16(vget_high_s16(v_x1), (int16_t)cospi_16_64);
    {
      const int16x4_t a = vrshrn_n_s32(v_t0_lo, DCT_CONST_BITS);
      const int16x4_t b = vrshrn_n_s32(v_t0_hi, DCT_CONST_BITS);
      const int16x4_t c = vrshrn_n_s32(v_t1_lo, DCT_CONST_BITS);
      const int16x4_t d = vrshrn_n_s32(v_t1_hi, DCT_CONST_BITS);
      const int16x8_t ab = vcombine_s16(a, b);
      const int16x8_t cd = vcombine_s16(c, d);
      // Stage 3
      v_x0 = vaddq_s16(v_s4, ab);
      v_x1 = vsubq_s16(v_s4, ab);
      v_x2 = vsubq_s16(v_s7, cd);
      v_x3 = vaddq_s16(v_s7, cd);
    }
    // Stage 4
    v_t0_lo = vmull_n_s16(vget_low_s16(v_x3), (int16_t)cospi_4_64);
    v_t0_hi = vmull_n_s16(vget_high_s16(v_x3), (int16_t)cospi_4_64);
    v_t0_lo = vmlal_n_s16(v_t0_lo, vget_low_s16(v_x0), (int16_t)cospi_28_64);
    v_t0_hi = vmlal_n_s16(v_t0_hi, vget_high_s16(v_x0), (int16_t)cospi_28_64);
    v_t1_lo = vmull_n_s16(vget_low_s16(v_x1), (int16_t)cospi_12_64);
    v_t1_hi = vmull_n_s16(vget_high_s16(v_x1), (int16_t)cospi_12_64);
    v_t1_lo = vmlal_n_s16(v_t1_lo, vget_low_s16(v_x2), (int16_t)cospi_20_64);
    v_t1_hi = vmlal_n_s16(v_t1_hi, vget_high_s16(v_x2), (int16_t)cospi_20_64);
    v_t2_lo = vmull_n_s16(vget_low_s16(v_x2), (int16_t)cospi_12_64);
    v_t2_hi = vmull_n_s16(vget_high_s16(v_x2), (int16_t)cospi_12_64);
    v_t2_lo = vmlsl_n_s16(v_t2_lo, vget_low_s16(v_x1), (int16_t)cospi_20_64);
    v_t2_hi = vmlsl_n_s16(v_t2_hi, vget_high_s16(v_x1), (int16_t)cospi_20_64);
    v_t3_lo = vmull_n_s16(vget_low_s16(v_x3), (int16_t)cospi_28_64);
    v_t3_hi = vmull_n_s16(vget_high_s16(v_x3), (int16_t)cospi_28_64);
    v_t3_lo = vmlsl_n_s16(v_t3_lo, vget_low_s16(v_x0), (int16_t)cospi_4_64);
    v_t3_hi = vmlsl_n_s16(v_t3_hi, vget_high_s16(v_x0), (int16_t)cospi_4_64);
    {
      const int16x4_t a = vrshrn_n_s32(v_t0_lo, DCT_CONST_BITS);
      const int16x4_t b = vrshrn_n_s32(v_t0_hi, DCT_CONST_BITS);
      const int16x4_t c = vrshrn_n_s32(v_t1_lo, DCT_CONST_BITS);
      const int16x4_t d = vrshrn_n_s32(v_t1_hi, DCT_CONST_BITS);
      const int16x4_t e = vrshrn_n_s32(v_t2_lo, DCT_CONST_BITS);
      const int16x4_t f = vrshrn_n_s32(v_t2_hi, DCT_CONST_BITS);
      const int16x4_t g = vrshrn_n_s32(v_t3_lo, DCT_CONST_BITS);
      const int16x4_t h = vrshrn_n_s32(v_t3_hi, DCT_CONST_BITS);
      out_1 = vcombine_s16(a, c);  // 10 11 12 13 50 51 52 53
      out_3 = vcombine_s16(e, g);  // 30 31 32 33 70 71 72 73
      out_5 = vcombine_s16(b, d);  // 14 15 16 17 54 55 56 57
      out_7 = vcombine_s16(f, h);  // 34 35 36 37 74 75 76 77
    }
    // transpose 8x8
    {
      // 00 01 02 03 40 41 42 43
      // 10 11 12 13 50 51 52 53
      // 20 21 22 23 60 61 62 63
      // 30 31 32 33 70 71 72 73
      // 04 05 06 07 44 45 46 47
      // 14 15 16 17 54 55 56 57
      // 24 25 26 27 64 65 66 67
      // 34 35 36 37 74 75 76 77
      const int32x4x2_t r02_s32 =
          vtrnq_s32(vreinterpretq_s32_s16(out_0), vreinterpretq_s32_s16(out_2));
      const int32x4x2_t r13_s32 =
          vtrnq_s32(vreinterpretq_s32_s16(out_1), vreinterpretq_s32_s16(out_3));
      const int32x4x2_t r46_s32 =
          vtrnq_s32(vreinterpretq_s32_s16(out_4), vreinterpretq_s32_s16(out_6));
      const int32x4x2_t r57_s32 =
          vtrnq_s32(vreinterpretq_s32_s16(out_5), vreinterpretq_s32_s16(out_7));
      const int16x8x2_t r01_s16 =
          vtrnq_s16(vreinterpretq_s16_s32(r02_s32.val[0]),
                    vreinterpretq_s16_s32(r13_s32.val[0]));
      const int16x8x2_t r23_s16 =
          vtrnq_s16(vreinterpretq_s16_s32(r02_s32.val[1]),
                    vreinterpretq_s16_s32(r13_s32.val[1]));
      const int16x8x2_t r45_s16 =
          vtrnq_s16(vreinterpretq_s16_s32(r46_s32.val[0]),
                    vreinterpretq_s16_s32(r57_s32.val[0]));
      const int16x8x2_t r67_s16 =
          vtrnq_s16(vreinterpretq_s16_s32(r46_s32.val[1]),
                    vreinterpretq_s16_s32(r57_s32.val[1]));
      input_0 = r01_s16.val[0];
      input_1 = r01_s16.val[1];
      input_2 = r23_s16.val[0];
      input_3 = r23_s16.val[1];
      input_4 = r45_s16.val[0];
      input_5 = r45_s16.val[1];
      input_6 = r67_s16.val[0];
      input_7 = r67_s16.val[1];
      // 00 10 20 30 40 50 60 70
      // 01 11 21 31 41 51 61 71
      // 02 12 22 32 42 52 62 72
      // 03 13 23 33 43 53 63 73
      // 04 14 24 34 44 54 64 74
      // 05 15 25 35 45 55 65 75
      // 06 16 26 36 46 56 66 76
      // 07 17 27 37 47 57 67 77
    }
  }  // for
  {
    // from aom_dct_sse2.c
    // Post-condition (division by two)
    //    division of two 16 bits signed numbers using shifts
    //    n / 2 = (n - (n >> 15)) >> 1
    const int16x8_t sign_in0 = vshrq_n_s16(input_0, 15);
    const int16x8_t sign_in1 = vshrq_n_s16(input_1, 15);
    const int16x8_t sign_in2 = vshrq_n_s16(input_2, 15);
    const int16x8_t sign_in3 = vshrq_n_s16(input_3, 15);
    const int16x8_t sign_in4 = vshrq_n_s16(input_4, 15);
    const int16x8_t sign_in5 = vshrq_n_s16(input_5, 15);
    const int16x8_t sign_in6 = vshrq_n_s16(input_6, 15);
    const int16x8_t sign_in7 = vshrq_n_s16(input_7, 15);
    input_0 = vhsubq_s16(input_0, sign_in0);
    input_1 = vhsubq_s16(input_1, sign_in1);
    input_2 = vhsubq_s16(input_2, sign_in2);
    input_3 = vhsubq_s16(input_3, sign_in3);
    input_4 = vhsubq_s16(input_4, sign_in4);
    input_5 = vhsubq_s16(input_5, sign_in5);
    input_6 = vhsubq_s16(input_6, sign_in6);
    input_7 = vhsubq_s16(input_7, sign_in7);
    // store results
    vst1q_s16(&final_output[0 * 8], input_0);
    vst1q_s16(&final_output[1 * 8], input_1);
    vst1q_s16(&final_output[2 * 8], input_2);
    vst1q_s16(&final_output[3 * 8], input_3);
    vst1q_s16(&final_output[4 * 8], input_4);
    vst1q_s16(&final_output[5 * 8], input_5);
    vst1q_s16(&final_output[6 * 8], input_6);
    vst1q_s16(&final_output[7 * 8], input_7);
  }
}

void aom_fdct8x8_1_neon(const int16_t *input, int16_t *output, int stride) {
  int r;
  int16x8_t sum = vld1q_s16(&input[0]);
  for (r = 1; r < 8; ++r) {
    const int16x8_t input_00 = vld1q_s16(&input[r * stride]);
    sum = vaddq_s16(sum, input_00);
  }
  {
    const int32x4_t a = vpaddlq_s16(sum);
    const int64x2_t b = vpaddlq_s32(a);
    const int32x2_t c = vadd_s32(vreinterpret_s32_s64(vget_low_s64(b)),
                                 vreinterpret_s32_s64(vget_high_s64(b)));
    output[0] = vget_lane_s16(vreinterpret_s16_s32(c), 0);
    output[1] = 0;
  }
}
