/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
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

#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_txfm.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "shift_neon.h"
#include "txfm_neon.h"

#define TXFM_COS_BIT_MAX 13

// A note on butterfly helper naming:
//
// butterfly_[input_ty]_[acc_ty]_[input_num]_[weight_num]_[weight_neg]_neon
// e.g. butterfly_s32_s32_x4_0231_neon
//                |   |   |  ^ Weights are applied as indices 0, 2, 3, 1
//                |   |   |    (see more detail below)
//                |   |   ^ (int32)x4 input/output parameters
//                |   ^ 32-bit accumulators internally
//                ^ 32-bit input/output parameters
//
// Weights are stored as 4-tuples in Q2.13 format as (w0, 1-w0, -w0, w0-1) to
// avoid needing separate negation instructions. This is represented in the
// helper naming by referring to the lane index in the loaded tuple that each
// multiply is performed with:
//
//        in0  in1
//      /----------
// out0 |  w0   w1   ==>  out0 = in0 * w0 + in1 * w1
// out1 |  w2   w3   ==>  out1 = in0 * w2 + in1 * w3
//
// So for indices 0331 from the earlier example, we end up with:
//
//          in0       in1
//      /------------------
// out0 | (lane 0) (lane 2)   ==>  out0 = in0 *   w0   + in1 *  -w0
// out1 | (lane 3) (lane 1)   ==>  out1 = in0 * (w0-1) + in1 * (1-w0)

static AOM_FORCE_INLINE void butterfly_s32_s32_x4_0112_neon(
    const int16x4_t w0101_s16, const int32x4_t in0, const int32x4_t in1,
    int32x4_t *out0, int32x4_t *out1) {
  int32x4_t w0101 = vmovl_s16(w0101_s16);
  int32x4_t o0 = vmulq_lane_s32(in0, vget_low_s32(w0101), 0);
  o0 = vmlaq_lane_s32(o0, in1, vget_low_s32(w0101), 1);
  int32x4_t o1 = vmulq_lane_s32(in0, vget_low_s32(w0101), 1);
  o1 = vmlaq_lane_s32(o1, in1, vget_high_s32(w0101), 0);
  *out0 = vrshrq_n_s32(o0, TXFM_COS_BIT_MAX);
  *out1 = vrshrq_n_s32(o1, TXFM_COS_BIT_MAX);
}

static AOM_FORCE_INLINE void butterfly_s32_s32_x4_0332_neon(
    const int16x4_t w0101_s16, const int32x4_t in0, const int32x4_t in1,
    int32x4_t *out0, int32x4_t *out1) {
  int32x4_t w0101 = vmovl_s16(w0101_s16);
  int32x4_t o0 = vmulq_lane_s32(in0, vget_low_s32(w0101), 0);
  o0 = vmlaq_lane_s32(o0, in1, vget_high_s32(w0101), 1);
  int32x4_t o1 = vmulq_lane_s32(in0, vget_high_s32(w0101), 1);
  o1 = vmlaq_lane_s32(o1, in1, vget_high_s32(w0101), 0);
  *out0 = vrshrq_n_s32(o0, TXFM_COS_BIT_MAX);
  *out1 = vrshrq_n_s32(o1, TXFM_COS_BIT_MAX);
}

static AOM_FORCE_INLINE void butterfly_s32_s32_x4_1003_neon(
    const int16x4_t w0101_s16, const int32x4_t in0, const int32x4_t in1,
    int32x4_t *out0, int32x4_t *out1) {
  int32x4_t w0101 = vmovl_s16(w0101_s16);
  int32x4_t o0 = vmulq_lane_s32(in0, vget_low_s32(w0101), 1);
  o0 = vmlaq_lane_s32(o0, in1, vget_low_s32(w0101), 0);
  int32x4_t o1 = vmulq_lane_s32(in0, vget_low_s32(w0101), 0);
  o1 = vmlaq_lane_s32(o1, in1, vget_high_s32(w0101), 1);
  *out0 = vrshrq_n_s32(o0, TXFM_COS_BIT_MAX);
  *out1 = vrshrq_n_s32(o1, TXFM_COS_BIT_MAX);
}

static AOM_FORCE_INLINE void butterfly_s32_s32_x4_1223_neon(
    const int16x4_t w0101_s16, const int32x4_t in0, const int32x4_t in1,
    int32x4_t *out0, int32x4_t *out1) {
  int32x4_t w0101 = vmovl_s16(w0101_s16);
  int32x4_t o0 = vmulq_lane_s32(in0, vget_low_s32(w0101), 1);
  o0 = vmlaq_lane_s32(o0, in1, vget_high_s32(w0101), 0);
  int32x4_t o1 = vmulq_lane_s32(in0, vget_high_s32(w0101), 0);
  o1 = vmlaq_lane_s32(o1, in1, vget_high_s32(w0101), 1);
  *out0 = vrshrq_n_s32(o0, TXFM_COS_BIT_MAX);
  *out1 = vrshrq_n_s32(o1, TXFM_COS_BIT_MAX);
}

#define butterfly_s16_s32_x4_neon(wvec, lane0, lane1, lane2, lane3, in0, in1, \
                                  out0, out1)                                 \
  do {                                                                        \
    int32x4_t u0 = vmull_lane_s16(in0, wvec, lane0);                          \
    u0 = vmlal_lane_s16(u0, in1, wvec, lane1);                                \
    int32x4_t v0 = vmull_lane_s16(in0, wvec, lane2);                          \
    v0 = vmlal_lane_s16(v0, in1, wvec, lane3);                                \
    *out0 = vqrshrn_n_s32(u0, TXFM_COS_BIT_MAX);                              \
    *out1 = vqrshrn_n_s32(v0, TXFM_COS_BIT_MAX);                              \
  } while (0)

static AOM_FORCE_INLINE void butterfly_s16_s32_x4_0112_neon(
    const int16x4_t w0101, const int16x4_t in0, const int16x4_t in1,
    int16x4_t *out0, int16x4_t *out1) {
  butterfly_s16_s32_x4_neon(w0101, 0, 1, 1, 2, in0, in1, out0, out1);
}

static AOM_FORCE_INLINE void butterfly_s16_s32_x4_0332_neon(
    const int16x4_t w0101, const int16x4_t in0, const int16x4_t in1,
    int16x4_t *out0, int16x4_t *out1) {
  butterfly_s16_s32_x4_neon(w0101, 0, 3, 3, 2, in0, in1, out0, out1);
}

static AOM_FORCE_INLINE void butterfly_s16_s32_x4_1003_neon(
    const int16x4_t w0101, const int16x4_t in0, const int16x4_t in1,
    int16x4_t *out0, int16x4_t *out1) {
  butterfly_s16_s32_x4_neon(w0101, 1, 0, 0, 3, in0, in1, out0, out1);
}

static AOM_FORCE_INLINE void butterfly_s16_s32_x4_1223_neon(
    const int16x4_t w0101, const int16x4_t in0, const int16x4_t in1,
    int16x4_t *out0, int16x4_t *out1) {
  butterfly_s16_s32_x4_neon(w0101, 1, 2, 2, 3, in0, in1, out0, out1);
}

#define butterfly_s16_s32_x8_neon(wvec, lane0, lane1, lane2, lane3, in0, in1, \
                                  out0, out1)                                 \
  do {                                                                        \
    int32x4_t u0 = vmull_lane_s16(vget_low_s16(in0), wvec, lane0);            \
    u0 = vmlal_lane_s16(u0, vget_low_s16(in1), wvec, lane1);                  \
    int32x4_t u1 = vmull_lane_s16(vget_high_s16(in0), wvec, lane0);           \
    u1 = vmlal_lane_s16(u1, vget_high_s16(in1), wvec, lane1);                 \
    int32x4_t v0 = vmull_lane_s16(vget_low_s16(in0), wvec, lane2);            \
    v0 = vmlal_lane_s16(v0, vget_low_s16(in1), wvec, lane3);                  \
    int32x4_t v1 = vmull_lane_s16(vget_high_s16(in0), wvec, lane2);           \
    v1 = vmlal_lane_s16(v1, vget_high_s16(in1), wvec, lane3);                 \
    const int16x4_t c0 = vrshrn_n_s32(u0, TXFM_COS_BIT_MAX);                  \
    const int16x4_t c1 = vrshrn_n_s32(u1, TXFM_COS_BIT_MAX);                  \
    const int16x4_t d0 = vrshrn_n_s32(v0, TXFM_COS_BIT_MAX);                  \
    const int16x4_t d1 = vrshrn_n_s32(v1, TXFM_COS_BIT_MAX);                  \
    *out0 = vcombine_s16(c0, c1);                                             \
    *out1 = vcombine_s16(d0, d1);                                             \
  } while (0)

static AOM_FORCE_INLINE void butterfly_s16_s32_x8_0112_neon(
    const int16x4_t w0101, const int16x8_t in0, const int16x8_t in1,
    int16x8_t *out0, int16x8_t *out1) {
  butterfly_s16_s32_x8_neon(w0101, 0, 1, 1, 2, in0, in1, out0, out1);
}

static AOM_FORCE_INLINE void butterfly_s16_s32_x8_0332_neon(
    const int16x4_t w0101, const int16x8_t in0, const int16x8_t in1,
    int16x8_t *out0, int16x8_t *out1) {
  butterfly_s16_s32_x8_neon(w0101, 0, 3, 3, 2, in0, in1, out0, out1);
}

static AOM_FORCE_INLINE void butterfly_s16_s32_x8_1003_neon(
    const int16x4_t w0101, const int16x8_t in0, const int16x8_t in1,
    int16x8_t *out0, int16x8_t *out1) {
  butterfly_s16_s32_x8_neon(w0101, 1, 0, 0, 3, in0, in1, out0, out1);
}

static AOM_FORCE_INLINE void butterfly_s16_s32_x8_1223_neon(
    const int16x4_t w0101, const int16x8_t in0, const int16x8_t in1,
    int16x8_t *out0, int16x8_t *out1) {
  butterfly_s16_s32_x8_neon(w0101, 1, 2, 2, 3, in0, in1, out0, out1);
}

static AOM_FORCE_INLINE void flip_buf_4_neon(int16x4_t *in, int16x4_t *out,
                                             int size) {
  for (int i = 0; i < size; ++i) {
    out[size - i - 1] = in[i];
  }
}

static AOM_FORCE_INLINE void flip_buf_8_neon(int16x8_t *in, int16x8_t *out,
                                             int size) {
  for (int i = 0; i < size; ++i) {
    out[size - i - 1] = in[i];
  }
}

static AOM_FORCE_INLINE void store_buffer_interleaved_s32_x8(
    int32_t *const out, const int32x4_t *const in1, const int32x4_t *const in2,
    const int stride, const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    vst1q_s32(out + stride * i, in1[i]);
    vst1q_s32(out + stride * i + 4, in2[i]);
  }
}

static AOM_FORCE_INLINE void load_buffer_s16_x4(const int16_t *in,
                                                const int stride,
                                                int16x4_t *const out,
                                                const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[i] = vld1_s16(in);
    in += stride;
  }
}

static AOM_FORCE_INLINE void load_buffer_s16_x8(const int16_t *in, int stride,
                                                int16x8_t *out, int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[i] = vld1q_s16(in + i * stride);
  }
}

static AOM_FORCE_INLINE void store_buffer_s16_x4(const int16x4_t *const in,
                                                 int32_t *const out,
                                                 const int stride,
                                                 const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    vst1q_s32(out + i * stride, vmovl_s16(in[i]));
  }
}

static AOM_FORCE_INLINE void store_buffer_s16_x8(const int16x8_t *const in,
                                                 int32_t *const out,
                                                 const int stride,
                                                 const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    vst1q_s32(out + i * stride + 0, vmovl_s16(vget_low_s16(in[i])));
    vst1q_s32(out + i * stride + 4, vmovl_s16(vget_high_s16(in[i])));
  }
}

// A note on naming:
//   round_shift_[sqrt2]_s16_s32_4x1_neon(...)
//                |      |   |     ^ 1 => a single vector
//                |      |   |       n => an array of vectors
//                |      |   |   ^ input/output vector element count
//                |      |   ^ output type
//                |      ^ input type
//                ^ multiplicand and shift identifier

static AOM_FORCE_INLINE int16x4_t
round_shift_sqrt2_s16_s16_4x1_neon(int16x4_t a) {
  return vqrshrn_n_s32(vmull_n_s16(a, NewSqrt2), NewSqrt2Bits);
}

static AOM_FORCE_INLINE int16x8_t
round_shift_sqrt2_s16_s16_8x1_neon(int16x8_t a) {
  return vcombine_s16(round_shift_sqrt2_s16_s16_4x1_neon(vget_low_s16(a)),
                      round_shift_sqrt2_s16_s16_4x1_neon(vget_high_s16(a)));
}

static AOM_FORCE_INLINE int16x4_t
round_shift_2sqrt2_s16_s16_4x1_neon(int16x4_t a) {
  return vqrshrn_n_s32(vmull_n_s16(a, 2 * NewSqrt2), NewSqrt2Bits);
}

static AOM_FORCE_INLINE int16x8_t
round_shift_2sqrt2_s16_s16_8x1_neon(int16x8_t a) {
  return vcombine_s16(round_shift_2sqrt2_s16_s16_4x1_neon(vget_low_s16(a)),
                      round_shift_2sqrt2_s16_s16_4x1_neon(vget_high_s16(a)));
}

static AOM_FORCE_INLINE int32x4_t
round_shift_sqrt2_s16_s32_4x1_neon(int16x4_t a) {
  return vrshrq_n_s32(vmull_n_s16(a, NewSqrt2), NewSqrt2Bits);
}

static AOM_FORCE_INLINE int32x4_t
round_shift_sqrt2_s32_s32_4x1_neon(int32x4_t a) {
  return vrshrq_n_s32(vmulq_n_s32(a, NewSqrt2), NewSqrt2Bits);
}

#define ROUND_SHIFT_SQRT_LOOP_HELPER(name, type0, type1, fn)                 \
  static AOM_FORCE_INLINE void name(const type0 *in, type1 *out, int size) { \
    for (int i = 0; i < size; ++i) {                                         \
      out[i] = fn(in[i]);                                                    \
    }                                                                        \
  }

ROUND_SHIFT_SQRT_LOOP_HELPER(round_shift_sqrt2_s32_s32_4xn_neon, int32x4_t,
                             int32x4_t, round_shift_sqrt2_s32_s32_4x1_neon)
ROUND_SHIFT_SQRT_LOOP_HELPER(round_shift_sqrt2_s16_s16_4xn_neon, int16x4_t,
                             int16x4_t, round_shift_sqrt2_s16_s16_4x1_neon)
ROUND_SHIFT_SQRT_LOOP_HELPER(round_shift_sqrt2_s16_s16_8xn_neon, int16x8_t,
                             int16x8_t, round_shift_sqrt2_s16_s16_8x1_neon)
ROUND_SHIFT_SQRT_LOOP_HELPER(round_shift_2sqrt2_s16_s16_4xn_neon, int16x4_t,
                             int16x4_t, round_shift_2sqrt2_s16_s16_4x1_neon)
ROUND_SHIFT_SQRT_LOOP_HELPER(round_shift_2sqrt2_s16_s16_8xn_neon, int16x8_t,
                             int16x8_t, round_shift_2sqrt2_s16_s16_8x1_neon)

static AOM_FORCE_INLINE void store_rect_buffer_s16_x4(const int16x4_t *const in,
                                                      int32_t *const out,
                                                      const int stride,
                                                      const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    vst1q_s32(out + i * stride, round_shift_sqrt2_s16_s32_4x1_neon(in[i]));
  }
}

static AOM_FORCE_INLINE void store_rect_buffer_s16_x8(const int16x8_t *const in,
                                                      int32_t *const out,
                                                      const int stride,
                                                      const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    vst1q_s32(out + i * stride + 0,
              round_shift_sqrt2_s16_s32_4x1_neon(vget_low_s16(in[i])));
    vst1q_s32(out + i * stride + 4,
              round_shift_sqrt2_s16_s32_4x1_neon(vget_high_s16(in[i])));
  }
}

static AOM_FORCE_INLINE void fadst4x4_neon(const int16x4_t *input,
                                           int16x4_t *output, int cos_bit) {
  int32x4_t u[6], v[6];
  const int16x4_t sinpi = vld1_s16(sinpi_arr_q13(cos_bit));
  const int16x4_t u01 = vqadd_s16(input[0], input[1]);

  v[5] = vmull_lane_s16(input[2], sinpi, 2);
  v[0] = vmull_lane_s16(input[1], sinpi, 1);
  v[0] = vmlal_lane_s16(v[0], input[0], sinpi, 0);
  v[1] = vmlal_lane_s16(v[5], input[3], sinpi, 3);
  v[2] = vmull_lane_s16(u01, sinpi, 2);
  v[3] = vmull_lane_s16(input[0], sinpi, 3);
  v[3] = vmlsl_lane_s16(v[3], input[1], sinpi, 0);
  v[4] = vmlsl_lane_s16(v[5], input[3], sinpi, 1);

  u[0] = vaddq_s32(v[0], v[1]);
  u[1] = vmlsl_lane_s16(v[2], input[3], sinpi, 2);
  u[2] = vsubq_s32(v[3], v[4]);
  u[3] = vsubq_s32(u[2], u[0]);
  u[3] = vmlaq_n_s32(u[3], v[5], 3);

  output[0] = vrshrn_n_s32(u[0], TXFM_COS_BIT_MAX);
  output[1] = vrshrn_n_s32(u[1], TXFM_COS_BIT_MAX);
  output[2] = vrshrn_n_s32(u[2], TXFM_COS_BIT_MAX);
  output[3] = vrshrn_n_s32(u[3], TXFM_COS_BIT_MAX);
}

static AOM_FORCE_INLINE void fadst4x8_neon(const int16x4_t *input,
                                           int16x4_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi4_12 = vld1q_s16(&cospi[4 * 4]);
  const int16x8_t cospi20_28 = vld1q_s16(&cospi[4 * 6]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi4 = vget_low_s16(cospi4_12);
  const int16x4_t cospi12 = vget_high_s16(cospi4_12);
  const int16x4_t cospi20 = vget_low_s16(cospi20_28);
  const int16x4_t cospi28 = vget_high_s16(cospi20_28);

  // stage 1-2
  int16x4_t x2[8];
  butterfly_s16_s32_x4_0332_neon(cospi32, input[4], input[3], &x2[2], &x2[3]);
  butterfly_s16_s32_x4_0112_neon(cospi32, input[2], input[5], &x2[7], &x2[6]);

  // stage 3
  int16x4_t x3[8];
  x3[0] = vqadd_s16(input[0], x2[2]);
  x3[1] = vqsub_s16(x2[3], input[7]);
  x3[2] = vqsub_s16(input[0], x2[2]);
  x3[3] = vqadd_s16(input[7], x2[3]);
  x3[4] = vqsub_s16(x2[6], input[1]);
  x3[5] = vqadd_s16(input[6], x2[7]);
  x3[6] = vqadd_s16(input[1], x2[6]);
  x3[7] = vqsub_s16(input[6], x2[7]);

  // stage 4
  int16x4_t x4[8];
  butterfly_s16_s32_x4_0112_neon(cospi16, x3[4], x3[5], &x4[4], &x4[5]);
  butterfly_s16_s32_x4_0112_neon(cospi16, x3[7], x3[6], &x4[6], &x4[7]);

  // stage 5
  int16x4_t x5[8];
  x5[0] = vqadd_s16(x3[0], x4[4]);
  x5[1] = vqadd_s16(x3[1], x4[5]);
  x5[2] = vqadd_s16(x3[2], x4[6]);
  x5[3] = vqsub_s16(x4[7], x3[3]);
  x5[4] = vqsub_s16(x3[0], x4[4]);
  x5[5] = vqsub_s16(x3[1], x4[5]);
  x5[6] = vqsub_s16(x3[2], x4[6]);
  x5[7] = vqadd_s16(x3[3], x4[7]);

  // stage 6-7
  butterfly_s16_s32_x4_0112_neon(cospi4, x5[0], x5[1], &output[7], &output[0]);
  butterfly_s16_s32_x4_0112_neon(cospi20, x5[2], x5[3], &output[5], &output[2]);
  butterfly_s16_s32_x4_1003_neon(cospi28, x5[4], x5[5], &output[3], &output[4]);
  butterfly_s16_s32_x4_0112_neon(cospi12, x5[6], x5[7], &output[6], &output[1]);
}

static AOM_FORCE_INLINE void fadst8x4_neon(const int16x8_t *input,
                                           int16x8_t *output, int cos_bit) {
  int32x4_t u_lo[4], u_hi[4];
  const int16x4_t sinpi = vld1_s16(sinpi_arr_q13(cos_bit));
  const int16x8_t u01 = vqaddq_s16(input[0], input[1]);

  u_lo[0] = vmull_lane_s16(vget_low_s16(input[1]), sinpi, 1);
  u_hi[0] = vmull_lane_s16(vget_high_s16(input[1]), sinpi, 1);

  u_lo[0] = vmlal_lane_s16(u_lo[0], vget_low_s16(input[0]), sinpi, 0);
  u_hi[0] = vmlal_lane_s16(u_hi[0], vget_high_s16(input[0]), sinpi, 0);

  u_lo[0] = vmlal_lane_s16(u_lo[0], vget_low_s16(input[3]), sinpi, 3);
  u_hi[0] = vmlal_lane_s16(u_hi[0], vget_high_s16(input[3]), sinpi, 3);

  u_lo[0] = vmlal_lane_s16(u_lo[0], vget_low_s16(input[2]), sinpi, 2);
  u_hi[0] = vmlal_lane_s16(u_hi[0], vget_high_s16(input[2]), sinpi, 2);

  u_lo[1] = vmull_lane_s16(vget_low_s16(u01), sinpi, 2);
  u_hi[1] = vmull_lane_s16(vget_high_s16(u01), sinpi, 2);

  u_lo[2] = vmull_lane_s16(vget_low_s16(input[0]), sinpi, 3);
  u_hi[2] = vmull_lane_s16(vget_high_s16(input[0]), sinpi, 3);

  u_lo[2] = vmlsl_lane_s16(u_lo[2], vget_low_s16(input[1]), sinpi, 0);
  u_hi[2] = vmlsl_lane_s16(u_hi[2], vget_high_s16(input[1]), sinpi, 0);

  u_lo[2] = vmlal_lane_s16(u_lo[2], vget_low_s16(input[3]), sinpi, 1);
  u_hi[2] = vmlal_lane_s16(u_hi[2], vget_high_s16(input[3]), sinpi, 1);

  u_lo[2] = vmlsl_lane_s16(u_lo[2], vget_low_s16(input[2]), sinpi, 2);
  u_hi[2] = vmlsl_lane_s16(u_hi[2], vget_high_s16(input[2]), sinpi, 2);

  u_lo[1] = vmlsl_lane_s16(u_lo[1], vget_low_s16(input[3]), sinpi, 2);
  u_hi[1] = vmlsl_lane_s16(u_hi[1], vget_high_s16(input[3]), sinpi, 2);

  u_lo[3] = vsubq_s32(u_lo[2], u_lo[0]);
  u_hi[3] = vsubq_s32(u_hi[2], u_hi[0]);

  const int16x4_t sinpix3 = vmul_n_s16(sinpi, 3);
  u_lo[3] = vmlal_lane_s16(u_lo[3], vget_low_s16(input[2]), sinpix3, 2);
  u_hi[3] = vmlal_lane_s16(u_hi[3], vget_high_s16(input[2]), sinpix3, 2);

  output[0] = vcombine_s16(vrshrn_n_s32(u_lo[0], TXFM_COS_BIT_MAX),
                           vrshrn_n_s32(u_hi[0], TXFM_COS_BIT_MAX));
  output[1] = vcombine_s16(vrshrn_n_s32(u_lo[1], TXFM_COS_BIT_MAX),
                           vrshrn_n_s32(u_hi[1], TXFM_COS_BIT_MAX));
  output[2] = vcombine_s16(vrshrn_n_s32(u_lo[2], TXFM_COS_BIT_MAX),
                           vrshrn_n_s32(u_hi[2], TXFM_COS_BIT_MAX));
  output[3] = vcombine_s16(vrshrn_n_s32(u_lo[3], TXFM_COS_BIT_MAX),
                           vrshrn_n_s32(u_hi[3], TXFM_COS_BIT_MAX));
}

static AOM_FORCE_INLINE void fdct4x4_neon(const int16x4_t *input,
                                          int16x4_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);
  const int16x4_t cospi16 = vld1_s16(&cospi[4 * 1]);

  int16x4_t in12a = vadd_s16(input[1], input[2]);
  int16x4_t in12s = vsub_s16(input[1], input[2]);
  int16x4_t in03a = vadd_s16(input[0], input[3]);
  int16x4_t in03s = vsub_s16(input[0], input[3]);

  int32x4_t u0ad1 = vmull_n_s16(in12a, cospi[4 * 0]);
  int32x4_t u0ad2 = vmull_n_s16(in03a, cospi[4 * 0]);

  int32x4_t u[4];
  u[0] = vaddq_s32(u0ad1, u0ad2);
  u[1] = vsubq_s32(u0ad2, u0ad1);
  u[2] = vmull_lane_s16(in12s, cospi16, 1);
  u[2] = vmlal_lane_s16(u[2], in03s, cospi16, 0);
  u[3] = vmull_lane_s16(in03s, cospi16, 1);
  u[3] = vmlsl_lane_s16(u[3], in12s, cospi16, 0);

  output[0] = vrshrn_n_s32(u[0], TXFM_COS_BIT_MAX);
  output[1] = vrshrn_n_s32(u[2], TXFM_COS_BIT_MAX);
  output[2] = vrshrn_n_s32(u[1], TXFM_COS_BIT_MAX);
  output[3] = vrshrn_n_s32(u[3], TXFM_COS_BIT_MAX);
}

// Butterfly pre-processing:
// e.g. n=4:
//   out[0] = in[0] + in[3]
//   out[1] = in[1] + in[2]
//   out[2] = in[1] - in[2]
//   out[3] = in[0] - in[3]

static AOM_FORCE_INLINE void butterfly_dct_pre_s16_x4(const int16x4_t *input,
                                                      int16x4_t *output,
                                                      int n) {
  for (int i = 0; i < n / 2; ++i) {
    output[i] = vqadd_s16(input[i], input[n - i - 1]);
  }
  for (int i = 0; i < n / 2; ++i) {
    output[n / 2 + i] = vqsub_s16(input[n / 2 - i - 1], input[n / 2 + i]);
  }
}

static AOM_FORCE_INLINE void butterfly_dct_pre_s16_x8(const int16x8_t *input,
                                                      int16x8_t *output,
                                                      int n) {
  for (int i = 0; i < n / 2; ++i) {
    output[i] = vqaddq_s16(input[i], input[n - i - 1]);
  }
  for (int i = 0; i < n / 2; ++i) {
    output[n / 2 + i] = vqsubq_s16(input[n / 2 - i - 1], input[n / 2 + i]);
  }
}

static AOM_FORCE_INLINE void butterfly_dct_pre_s32_x4(const int32x4_t *input,
                                                      int32x4_t *output,
                                                      int n) {
  for (int i = 0; i < n / 2; ++i) {
    output[i] = vqaddq_s32(input[i], input[n - i - 1]);
  }
  for (int i = 0; i < n / 2; ++i) {
    output[n / 2 + i] = vqsubq_s32(input[n / 2 - i - 1], input[n / 2 + i]);
  }
}

// Butterfly post-processing:
// e.g. n=8:
//   out[0] = in0[0] + in1[3];
//   out[1] = in0[1] + in1[2];
//   out[2] = in0[1] - in1[2];
//   out[3] = in0[0] - in1[3];
//   out[4] = in0[7] - in1[4];
//   out[5] = in0[6] - in1[5];
//   out[6] = in0[6] + in1[5];
//   out[7] = in0[7] + in1[4];

static AOM_FORCE_INLINE void butterfly_dct_post_s16_x4(const int16x4_t *in0,
                                                       const int16x4_t *in1,
                                                       int16x4_t *output,
                                                       int n) {
  for (int i = 0; i < n / 4; ++i) {
    output[i] = vqadd_s16(in0[i], in1[n / 2 - i - 1]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[n / 4 + i] = vqsub_s16(in0[n / 4 - i - 1], in1[n / 4 + i]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[n / 2 + i] = vqsub_s16(in0[n - i - 1], in1[n / 2 + i]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[(3 * n) / 4 + i] =
        vqadd_s16(in0[(3 * n) / 4 + i], in1[(3 * n) / 4 - i - 1]);
  }
}

static AOM_FORCE_INLINE void butterfly_dct_post_s16_x8(const int16x8_t *in0,
                                                       const int16x8_t *in1,
                                                       int16x8_t *output,
                                                       int n) {
  for (int i = 0; i < n / 4; ++i) {
    output[i] = vqaddq_s16(in0[i], in1[n / 2 - i - 1]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[n / 4 + i] = vqsubq_s16(in0[n / 4 - i - 1], in1[n / 4 + i]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[n / 2 + i] = vqsubq_s16(in0[n - i - 1], in1[n / 2 + i]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[(3 * n) / 4 + i] =
        vqaddq_s16(in0[(3 * n) / 4 + i], in1[(3 * n) / 4 - i - 1]);
  }
}

static AOM_FORCE_INLINE void butterfly_dct_post_s32_x4(const int32x4_t *in0,
                                                       const int32x4_t *in1,
                                                       int32x4_t *output,
                                                       int n) {
  for (int i = 0; i < n / 4; ++i) {
    output[i] = vqaddq_s32(in0[i], in1[n / 2 - i - 1]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[n / 4 + i] = vqsubq_s32(in0[n / 4 - i - 1], in1[n / 4 + i]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[n / 2 + i] = vqsubq_s32(in0[n - i - 1], in1[n / 2 + i]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[(3 * n) / 4 + i] =
        vqaddq_s32(in0[(3 * n) / 4 + i], in1[(3 * n) / 4 - i - 1]);
  }
}

static AOM_FORCE_INLINE void fdct8x4_neon(const int16x8_t *input,
                                          int16x8_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);

  // stage 1
  int16x8_t x1[4];
  butterfly_dct_pre_s16_x8(input, x1, 4);

  // stage 2
  int16x8_t x2[4];
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[0], x1[1], &x2[0], &x2[1]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x1[3], x1[2], &x2[2], &x2[3]);

  // stage 3
  output[0] = x2[0];
  output[1] = x2[2];
  output[2] = x2[1];
  output[3] = x2[3];
}

static AOM_FORCE_INLINE void fdct4x8_neon(const int16x4_t *input,
                                          int16x4_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);

  // stage 1
  int16x4_t x1[8];
  butterfly_dct_pre_s16_x4(input, x1, 8);

  // stage 2
  int16x4_t x2[8];
  butterfly_dct_pre_s16_x4(x1, x2, 4);
  butterfly_s16_s32_x4_0112_neon(cospi32, x1[6], x1[5], &x2[6], &x2[5]);

  // stage 3
  int16x4_t x3[8];
  butterfly_s16_s32_x4_0112_neon(cospi32, x2[0], x2[1], &output[0], &output[4]);
  butterfly_s16_s32_x4_0112_neon(cospi16, x2[3], x2[2], &output[2], &output[6]);
  butterfly_dct_post_s16_x4(x1 + 4, x2 + 4, x3 + 4, 4);

  // stage 4-5
  butterfly_s16_s32_x4_0112_neon(cospi8, x3[7], x3[4], &output[1], &output[7]);
  butterfly_s16_s32_x4_1003_neon(cospi24, x3[6], x3[5], &output[5], &output[3]);
}

static AOM_FORCE_INLINE void fdct8x8_neon(const int16x8_t *input,
                                          int16x8_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);

  // stage 1
  int16x8_t x1[8];
  butterfly_dct_pre_s16_x8(input, x1, 8);

  // stage 2
  int16x8_t x2[8];
  butterfly_dct_pre_s16_x8(x1, x2, 4);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[6], x1[5], &x2[6], &x2[5]);

  // stage 3
  int16x8_t x3[8];
  butterfly_s16_s32_x8_0112_neon(cospi32, x2[0], x2[1], &output[0], &output[4]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x2[3], x2[2], &output[2], &output[6]);
  butterfly_dct_post_s16_x8(x1 + 4, x2 + 4, x3 + 4, 4);

  // stage 4-5
  butterfly_s16_s32_x8_0112_neon(cospi8, x3[7], x3[4], &output[1], &output[7]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x3[6], x3[5], &output[5], &output[3]);
}

static AOM_FORCE_INLINE void fdct4x16_neon(const int16x4_t *input,
                                           int16x4_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);
  const int16x8_t cospi4_12 = vld1q_s16(&cospi[4 * 4]);
  const int16x8_t cospi20_28 = vld1q_s16(&cospi[4 * 6]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);
  const int16x4_t cospi4 = vget_low_s16(cospi4_12);
  const int16x4_t cospi12 = vget_high_s16(cospi4_12);
  const int16x4_t cospi20 = vget_low_s16(cospi20_28);
  const int16x4_t cospi28 = vget_high_s16(cospi20_28);

  // stage 1
  int16x4_t x1[16];
  butterfly_dct_pre_s16_x4(input, x1, 16);

  // stage 2
  int16x4_t x2[16];
  butterfly_dct_pre_s16_x4(x1, x2, 8);
  butterfly_s16_s32_x4_0112_neon(cospi32, x1[13], x1[10], &x2[13], &x2[10]);
  butterfly_s16_s32_x4_0112_neon(cospi32, x1[12], x1[11], &x2[12], &x2[11]);

  // stage 3
  int16x4_t x3[16];
  butterfly_dct_pre_s16_x4(x2, x3, 4);
  butterfly_s16_s32_x4_0112_neon(cospi32, x2[6], x2[5], &x3[6], &x3[5]);
  butterfly_dct_post_s16_x4(x1 + 8, x2 + 8, x3 + 8, 8);

  // stage 4
  int16x4_t x4[16];
  butterfly_s16_s32_x4_0112_neon(cospi32, x3[0], x3[1], &output[0], &output[8]);
  butterfly_s16_s32_x4_0112_neon(cospi16, x3[3], x3[2], &output[4],
                                 &output[12]);
  butterfly_dct_post_s16_x4(x2 + 4, x3 + 4, x4 + 4, 4);
  butterfly_s16_s32_x4_0112_neon(cospi16, x3[14], x3[9], &x4[14], &x4[9]);
  butterfly_s16_s32_x4_1223_neon(cospi16, x3[13], x3[10], &x4[13], &x4[10]);

  // stage 5
  int16x4_t x5[16];
  butterfly_s16_s32_x4_0112_neon(cospi8, x4[7], x4[4], &output[2], &output[14]);
  butterfly_s16_s32_x4_1003_neon(cospi24, x4[6], x4[5], &output[10],
                                 &output[6]);
  butterfly_dct_post_s16_x4(x3 + 8, x4 + 8, x5 + 8, 4);
  butterfly_dct_post_s16_x4(x3 + 12, x4 + 12, x5 + 12, 4);

  // stage 6-7
  butterfly_s16_s32_x4_0112_neon(cospi4, x5[15], x5[8], &output[1],
                                 &output[15]);
  butterfly_s16_s32_x4_1003_neon(cospi28, x5[14], x5[9], &output[9],
                                 &output[7]);
  butterfly_s16_s32_x4_0112_neon(cospi20, x5[13], x5[10], &output[5],
                                 &output[11]);
  butterfly_s16_s32_x4_1003_neon(cospi12, x5[12], x5[11], &output[13],
                                 &output[3]);
}

static AOM_FORCE_INLINE void fdct8x16_neon(const int16x8_t *input,
                                           int16x8_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);
  const int16x8_t cospi4_12 = vld1q_s16(&cospi[4 * 4]);
  const int16x8_t cospi20_28 = vld1q_s16(&cospi[4 * 6]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);
  const int16x4_t cospi4 = vget_low_s16(cospi4_12);
  const int16x4_t cospi12 = vget_high_s16(cospi4_12);
  const int16x4_t cospi20 = vget_low_s16(cospi20_28);
  const int16x4_t cospi28 = vget_high_s16(cospi20_28);

  // stage 1
  int16x8_t x1[16];
  butterfly_dct_pre_s16_x8(input, x1, 16);

  // stage 2
  int16x8_t x2[16];
  butterfly_dct_pre_s16_x8(x1, x2, 8);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[13], x1[10], &x2[13], &x2[10]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[12], x1[11], &x2[12], &x2[11]);

  // stage 3
  int16x8_t x3[16];
  butterfly_dct_pre_s16_x8(x2, x3, 4);
  butterfly_s16_s32_x8_0112_neon(cospi32, x2[6], x2[5], &x3[6], &x3[5]);
  butterfly_dct_post_s16_x8(x1 + 8, x2 + 8, x3 + 8, 8);

  // stage 4
  int16x8_t x4[16];
  butterfly_s16_s32_x8_0112_neon(cospi32, x3[0], x3[1], &output[0], &output[8]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[3], x3[2], &output[4],
                                 &output[12]);
  butterfly_dct_post_s16_x8(x2 + 4, x3 + 4, x4 + 4, 4);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[14], x3[9], &x4[14], &x4[9]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x3[13], x3[10], &x4[13], &x4[10]);

  // stage 5
  int16x8_t x5[16];
  butterfly_s16_s32_x8_0112_neon(cospi8, x4[7], x4[4], &output[2], &output[14]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x4[6], x4[5], &output[10],
                                 &output[6]);
  butterfly_dct_post_s16_x8(x3 + 8, x4 + 8, x5 + 8, 4);
  butterfly_dct_post_s16_x8(x3 + 12, x4 + 12, x5 + 12, 4);

  // stage 6-7
  butterfly_s16_s32_x8_0112_neon(cospi4, x5[15], x5[8], &output[1],
                                 &output[15]);
  butterfly_s16_s32_x8_1003_neon(cospi28, x5[14], x5[9], &output[9],
                                 &output[7]);
  butterfly_s16_s32_x8_0112_neon(cospi20, x5[13], x5[10], &output[5],
                                 &output[11]);
  butterfly_s16_s32_x8_1003_neon(cospi12, x5[12], x5[11], &output[13],
                                 &output[3]);
}

static AOM_FORCE_INLINE void fdct8x32_neon(const int16x8_t *input,
                                           int16x8_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);
  const int16x8_t cospi4_12 = vld1q_s16(&cospi[4 * 4]);
  const int16x8_t cospi20_28 = vld1q_s16(&cospi[4 * 6]);
  const int16x8_t cospi2_6 = vld1q_s16(&cospi[4 * 8]);
  const int16x8_t cospi10_14 = vld1q_s16(&cospi[4 * 10]);
  const int16x8_t cospi18_22 = vld1q_s16(&cospi[4 * 12]);
  const int16x8_t cospi26_30 = vld1q_s16(&cospi[4 * 14]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);
  const int16x4_t cospi4 = vget_low_s16(cospi4_12);
  const int16x4_t cospi12 = vget_high_s16(cospi4_12);
  const int16x4_t cospi20 = vget_low_s16(cospi20_28);
  const int16x4_t cospi28 = vget_high_s16(cospi20_28);
  const int16x4_t cospi2 = vget_low_s16(cospi2_6);
  const int16x4_t cospi6 = vget_high_s16(cospi2_6);
  const int16x4_t cospi10 = vget_low_s16(cospi10_14);
  const int16x4_t cospi14 = vget_high_s16(cospi10_14);
  const int16x4_t cospi18 = vget_low_s16(cospi18_22);
  const int16x4_t cospi22 = vget_high_s16(cospi18_22);
  const int16x4_t cospi26 = vget_low_s16(cospi26_30);
  const int16x4_t cospi30 = vget_high_s16(cospi26_30);

  // stage 1
  int16x8_t x1[32];
  butterfly_dct_pre_s16_x8(input, x1, 32);

  // stage 2
  int16x8_t x2[32];
  butterfly_dct_pre_s16_x8(x1, x2, 16);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[27], x1[20], &x2[27], &x2[20]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[26], x1[21], &x2[26], &x2[21]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[25], x1[22], &x2[25], &x2[22]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[24], x1[23], &x2[24], &x2[23]);

  // stage 3
  int16x8_t x3[32];
  butterfly_dct_pre_s16_x8(x2, x3, 8);
  butterfly_s16_s32_x8_0112_neon(cospi32, x2[13], x2[10], &x3[13], &x3[10]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x2[12], x2[11], &x3[12], &x3[11]);
  butterfly_dct_post_s16_x8(x1 + 16, x2 + 16, x3 + 16, 16);

  // stage 4
  int16x8_t x4[32];
  butterfly_dct_pre_s16_x8(x3, x4, 4);
  butterfly_s16_s32_x8_0112_neon(cospi32, x3[6], x3[5], &x4[6], &x4[5]);
  butterfly_dct_post_s16_x8(x2 + 8, x3 + 8, x4 + 8, 8);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[29], x3[18], &x4[29], &x4[18]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[28], x3[19], &x4[28], &x4[19]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x3[27], x3[20], &x4[27], &x4[20]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x3[26], x3[21], &x4[26], &x4[21]);

  // stage 5
  int16x8_t x5[32];
  butterfly_s16_s32_x8_0112_neon(cospi32, x4[0], x4[1], &output[0],
                                 &output[16]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x4[3], x4[2], &output[8],
                                 &output[24]);
  butterfly_dct_post_s16_x8(x3 + 4, x4 + 4, x5 + 4, 4);
  butterfly_s16_s32_x8_0112_neon(cospi16, x4[14], x4[9], &x5[14], &x5[9]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x4[13], x4[10], &x5[13], &x5[10]);
  butterfly_dct_post_s16_x8(x3 + 16, x4 + 16, x5 + 16, 8);
  butterfly_dct_post_s16_x8(x3 + 24, x4 + 24, x5 + 24, 8);

  // stage 6
  int16x8_t x6[32];
  butterfly_s16_s32_x8_0112_neon(cospi8, x5[7], x5[4], &output[4], &output[28]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x5[6], x5[5], &output[20],
                                 &output[12]);
  butterfly_dct_post_s16_x8(x4 + 8, x5 + 8, x6 + 8, 4);
  butterfly_dct_post_s16_x8(x4 + 12, x5 + 12, x6 + 12, 4);
  butterfly_s16_s32_x8_0112_neon(cospi8, x5[30], x5[17], &x6[30], &x6[17]);
  butterfly_s16_s32_x8_1223_neon(cospi8, x5[29], x5[18], &x6[29], &x6[18]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x5[26], x5[21], &x6[26], &x6[21]);
  butterfly_s16_s32_x8_0332_neon(cospi24, x5[25], x5[22], &x6[25], &x6[22]);

  // stage 7
  int16x8_t x7[32];
  butterfly_s16_s32_x8_0112_neon(cospi4, x6[15], x6[8], &output[2],
                                 &output[30]);
  butterfly_s16_s32_x8_1003_neon(cospi28, x6[14], x6[9], &output[18],
                                 &output[14]);
  butterfly_s16_s32_x8_0112_neon(cospi20, x6[13], x6[10], &output[10],
                                 &output[22]);
  butterfly_s16_s32_x8_1003_neon(cospi12, x6[12], x6[11], &output[26],
                                 &output[6]);
  butterfly_dct_post_s16_x8(x5 + 16, x6 + 16, x7 + 16, 4);
  butterfly_dct_post_s16_x8(x5 + 20, x6 + 20, x7 + 20, 4);
  butterfly_dct_post_s16_x8(x5 + 24, x6 + 24, x7 + 24, 4);
  butterfly_dct_post_s16_x8(x5 + 28, x6 + 28, x7 + 28, 4);

  butterfly_s16_s32_x8_0112_neon(cospi2, x7[31], x7[16], &output[1],
                                 &output[31]);
  butterfly_s16_s32_x8_1003_neon(cospi30, x7[30], x7[17], &output[17],
                                 &output[15]);
  butterfly_s16_s32_x8_0112_neon(cospi18, x7[29], x7[18], &output[9],
                                 &output[23]);
  butterfly_s16_s32_x8_1003_neon(cospi14, x7[28], x7[19], &output[25],
                                 &output[7]);
  butterfly_s16_s32_x8_0112_neon(cospi10, x7[27], x7[20], &output[5],
                                 &output[27]);
  butterfly_s16_s32_x8_1003_neon(cospi22, x7[26], x7[21], &output[21],
                                 &output[11]);
  butterfly_s16_s32_x8_0112_neon(cospi26, x7[25], x7[22], &output[13],
                                 &output[19]);
  butterfly_s16_s32_x8_1003_neon(cospi6, x7[24], x7[23], &output[29],
                                 &output[3]);
}

static AOM_FORCE_INLINE void fdct8x64_neon(const int16x8_t *input,
                                           int16x8_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);
  const int16x8_t cospi4_12 = vld1q_s16(&cospi[4 * 4]);
  const int16x8_t cospi20_28 = vld1q_s16(&cospi[4 * 6]);
  const int16x8_t cospi2_6 = vld1q_s16(&cospi[4 * 8]);
  const int16x8_t cospi10_14 = vld1q_s16(&cospi[4 * 10]);
  const int16x8_t cospi18_22 = vld1q_s16(&cospi[4 * 12]);
  const int16x8_t cospi26_30 = vld1q_s16(&cospi[4 * 14]);
  const int16x8_t cospi1_3 = vld1q_s16(&cospi[4 * 16]);
  const int16x8_t cospi5_7 = vld1q_s16(&cospi[4 * 18]);
  const int16x8_t cospi9_11 = vld1q_s16(&cospi[4 * 20]);
  const int16x8_t cospi13_15 = vld1q_s16(&cospi[4 * 22]);
  const int16x8_t cospi17_19 = vld1q_s16(&cospi[4 * 24]);
  const int16x8_t cospi21_23 = vld1q_s16(&cospi[4 * 26]);
  const int16x8_t cospi25_27 = vld1q_s16(&cospi[4 * 28]);
  const int16x8_t cospi29_31 = vld1q_s16(&cospi[4 * 30]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);
  const int16x4_t cospi4 = vget_low_s16(cospi4_12);
  const int16x4_t cospi12 = vget_high_s16(cospi4_12);
  const int16x4_t cospi20 = vget_low_s16(cospi20_28);
  const int16x4_t cospi28 = vget_high_s16(cospi20_28);
  const int16x4_t cospi2 = vget_low_s16(cospi2_6);
  const int16x4_t cospi6 = vget_high_s16(cospi2_6);
  const int16x4_t cospi10 = vget_low_s16(cospi10_14);
  const int16x4_t cospi14 = vget_high_s16(cospi10_14);
  const int16x4_t cospi18 = vget_low_s16(cospi18_22);
  const int16x4_t cospi22 = vget_high_s16(cospi18_22);
  const int16x4_t cospi26 = vget_low_s16(cospi26_30);
  const int16x4_t cospi30 = vget_high_s16(cospi26_30);
  const int16x4_t cospi1 = vget_low_s16(cospi1_3);
  const int16x4_t cospi3 = vget_high_s16(cospi1_3);
  const int16x4_t cospi5 = vget_low_s16(cospi5_7);
  const int16x4_t cospi7 = vget_high_s16(cospi5_7);
  const int16x4_t cospi9 = vget_low_s16(cospi9_11);
  const int16x4_t cospi11 = vget_high_s16(cospi9_11);
  const int16x4_t cospi13 = vget_low_s16(cospi13_15);
  const int16x4_t cospi15 = vget_high_s16(cospi13_15);
  const int16x4_t cospi17 = vget_low_s16(cospi17_19);
  const int16x4_t cospi19 = vget_high_s16(cospi17_19);
  const int16x4_t cospi21 = vget_low_s16(cospi21_23);
  const int16x4_t cospi23 = vget_high_s16(cospi21_23);
  const int16x4_t cospi25 = vget_low_s16(cospi25_27);
  const int16x4_t cospi27 = vget_high_s16(cospi25_27);
  const int16x4_t cospi29 = vget_low_s16(cospi29_31);
  const int16x4_t cospi31 = vget_high_s16(cospi29_31);

  // stage 1
  int16x8_t x1[64];
  butterfly_dct_pre_s16_x8(input, x1, 64);

  // stage 2
  int16x8_t x2[64];
  butterfly_dct_pre_s16_x8(x1, x2, 32);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[55], x1[40], &x2[55], &x2[40]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[54], x1[41], &x2[54], &x2[41]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[53], x1[42], &x2[53], &x2[42]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[52], x1[43], &x2[52], &x2[43]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[51], x1[44], &x2[51], &x2[44]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[50], x1[45], &x2[50], &x2[45]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[49], x1[46], &x2[49], &x2[46]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x1[48], x1[47], &x2[48], &x2[47]);

  // stage 3
  int16x8_t x3[64];
  butterfly_dct_pre_s16_x8(x2, x3, 16);
  x3[16] = x2[16];
  x3[17] = x2[17];
  x3[18] = x2[18];
  x3[19] = x2[19];
  butterfly_s16_s32_x8_0112_neon(cospi32, x2[27], x2[20], &x3[27], &x3[20]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x2[26], x2[21], &x3[26], &x3[21]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x2[25], x2[22], &x3[25], &x3[22]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x2[24], x2[23], &x3[24], &x3[23]);
  x3[28] = x2[28];
  x3[29] = x2[29];
  x3[30] = x2[30];
  x3[31] = x2[31];
  butterfly_dct_post_s16_x8(x1 + 32, x2 + 32, x3 + 32, 32);

  // stage 4
  int16x8_t x4[64];
  butterfly_dct_pre_s16_x8(x3, x4, 8);
  butterfly_s16_s32_x8_0112_neon(cospi32, x3[13], x3[10], &x4[13], &x4[10]);
  butterfly_s16_s32_x8_0112_neon(cospi32, x3[12], x3[11], &x4[12], &x4[11]);
  butterfly_dct_post_s16_x8(x3 + 16, x3 + 16, x4 + 16, 16);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[59], x3[36], &x4[59], &x4[36]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[58], x3[37], &x4[58], &x4[37]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[57], x3[38], &x4[57], &x4[38]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[56], x3[39], &x4[56], &x4[39]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x3[55], x3[40], &x4[55], &x4[40]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x3[54], x3[41], &x4[54], &x4[41]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x3[53], x3[42], &x4[53], &x4[42]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x3[52], x3[43], &x4[52], &x4[43]);

  // stage 5
  int16x8_t x5[64];
  butterfly_dct_pre_s16_x8(x4, x5, 4);
  butterfly_s16_s32_x8_0112_neon(cospi32, x4[6], x4[5], &x5[6], &x5[5]);
  butterfly_dct_post_s16_x8(x3 + 8, x4 + 8, x5 + 8, 8);
  butterfly_s16_s32_x8_0112_neon(cospi16, x4[29], x4[18], &x5[29], &x5[18]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x4[28], x4[19], &x5[28], &x5[19]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x4[27], x4[20], &x5[27], &x5[20]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x4[26], x4[21], &x5[26], &x5[21]);
  butterfly_dct_post_s16_x8(x3 + 32, x4 + 32, x5 + 32, 16);
  butterfly_dct_post_s16_x8(x3 + 48, x4 + 48, x5 + 48, 16);

  // stage 6
  int16x8_t x6[64];
  butterfly_s16_s32_x8_0112_neon(cospi32, x5[1], x5[0], &x6[0], &x6[1]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x5[3], x5[2], &x6[2], &x6[3]);
  butterfly_dct_post_s16_x8(x4 + 4, x5 + 4, x6 + 4, 4);
  butterfly_s16_s32_x8_0112_neon(cospi16, x5[14], x5[9], &x6[14], &x6[9]);
  butterfly_s16_s32_x8_1223_neon(cospi16, x5[13], x5[10], &x6[13], &x6[10]);
  butterfly_dct_post_s16_x8(x4 + 16, x5 + 16, x6 + 16, 8);
  butterfly_dct_post_s16_x8(x4 + 24, x5 + 24, x6 + 24, 8);
  butterfly_s16_s32_x8_0112_neon(cospi8, x5[61], x5[34], &x6[61], &x6[34]);
  butterfly_s16_s32_x8_0112_neon(cospi8, x5[60], x5[35], &x6[60], &x6[35]);
  butterfly_s16_s32_x8_1223_neon(cospi8, x5[59], x5[36], &x6[59], &x6[36]);
  butterfly_s16_s32_x8_1223_neon(cospi8, x5[58], x5[37], &x6[58], &x6[37]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x5[53], x5[42], &x6[53], &x6[42]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x5[52], x5[43], &x6[52], &x6[43]);
  butterfly_s16_s32_x8_0332_neon(cospi24, x5[51], x5[44], &x6[51], &x6[44]);
  butterfly_s16_s32_x8_0332_neon(cospi24, x5[50], x5[45], &x6[50], &x6[45]);

  // stage 7
  int16x8_t x7[64];
  butterfly_s16_s32_x8_0112_neon(cospi8, x6[7], x6[4], &x7[4], &x7[7]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x6[6], x6[5], &x7[5], &x7[6]);
  butterfly_dct_post_s16_x8(x5 + 8, x6 + 8, x7 + 8, 4);
  butterfly_dct_post_s16_x8(x5 + 12, x6 + 12, x7 + 12, 4);
  butterfly_s16_s32_x8_0112_neon(cospi8, x6[30], x6[17], &x7[30], &x7[17]);
  butterfly_s16_s32_x8_1223_neon(cospi8, x6[29], x6[18], &x7[29], &x7[18]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x6[26], x6[21], &x7[26], &x7[21]);
  butterfly_s16_s32_x8_0332_neon(cospi24, x6[25], x6[22], &x7[25], &x7[22]);
  butterfly_dct_post_s16_x8(x5 + 32, x6 + 32, x7 + 32, 8);
  butterfly_dct_post_s16_x8(x5 + 40, x6 + 40, x7 + 40, 8);
  butterfly_dct_post_s16_x8(x5 + 48, x6 + 48, x7 + 48, 8);
  butterfly_dct_post_s16_x8(x5 + 56, x6 + 56, x7 + 56, 8);

  // stage 8
  int16x8_t x8[64];
  butterfly_s16_s32_x8_0112_neon(cospi4, x7[15], x7[8], &x8[8], &x8[15]);
  butterfly_s16_s32_x8_1003_neon(cospi28, x7[14], x7[9], &x8[9], &x8[14]);
  butterfly_s16_s32_x8_0112_neon(cospi20, x7[13], x7[10], &x8[10], &x8[13]);
  butterfly_s16_s32_x8_1003_neon(cospi12, x7[12], x7[11], &x8[11], &x8[12]);
  butterfly_dct_post_s16_x8(x6 + 16, x7 + 16, x8 + 16, 4);
  butterfly_dct_post_s16_x8(x6 + 20, x7 + 20, x8 + 20, 4);
  butterfly_dct_post_s16_x8(x6 + 24, x7 + 24, x8 + 24, 4);
  butterfly_dct_post_s16_x8(x6 + 28, x7 + 28, x8 + 28, 4);
  butterfly_s16_s32_x8_0112_neon(cospi4, x7[62], x7[33], &x8[62], &x8[33]);
  butterfly_s16_s32_x8_1223_neon(cospi4, x7[61], x7[34], &x8[61], &x8[34]);
  butterfly_s16_s32_x8_1003_neon(cospi28, x7[58], x7[37], &x8[58], &x8[37]);
  butterfly_s16_s32_x8_0332_neon(cospi28, x7[57], x7[38], &x8[57], &x8[38]);
  butterfly_s16_s32_x8_0112_neon(cospi20, x7[54], x7[41], &x8[54], &x8[41]);
  butterfly_s16_s32_x8_1223_neon(cospi20, x7[53], x7[42], &x8[53], &x8[42]);
  butterfly_s16_s32_x8_1003_neon(cospi12, x7[50], x7[45], &x8[50], &x8[45]);
  butterfly_s16_s32_x8_0332_neon(cospi12, x7[49], x7[46], &x8[49], &x8[46]);

  // stage 9
  int16x8_t x9[64];
  butterfly_s16_s32_x8_0112_neon(cospi2, x8[31], x8[16], &x9[16], &x9[31]);
  butterfly_s16_s32_x8_1003_neon(cospi30, x8[30], x8[17], &x9[17], &x9[30]);
  butterfly_s16_s32_x8_0112_neon(cospi18, x8[29], x8[18], &x9[18], &x9[29]);
  butterfly_s16_s32_x8_1003_neon(cospi14, x8[28], x8[19], &x9[19], &x9[28]);
  butterfly_s16_s32_x8_0112_neon(cospi10, x8[27], x8[20], &x9[20], &x9[27]);
  butterfly_s16_s32_x8_1003_neon(cospi22, x8[26], x8[21], &x9[21], &x9[26]);
  butterfly_s16_s32_x8_0112_neon(cospi26, x8[25], x8[22], &x9[22], &x9[25]);
  butterfly_s16_s32_x8_1003_neon(cospi6, x8[24], x8[23], &x9[23], &x9[24]);
  butterfly_dct_post_s16_x8(x7 + 32, x8 + 32, x9 + 32, 4);
  butterfly_dct_post_s16_x8(x7 + 36, x8 + 36, x9 + 36, 4);
  butterfly_dct_post_s16_x8(x7 + 40, x8 + 40, x9 + 40, 4);
  butterfly_dct_post_s16_x8(x7 + 44, x8 + 44, x9 + 44, 4);
  butterfly_dct_post_s16_x8(x7 + 48, x8 + 48, x9 + 48, 4);
  butterfly_dct_post_s16_x8(x7 + 52, x8 + 52, x9 + 52, 4);
  butterfly_dct_post_s16_x8(x7 + 56, x8 + 56, x9 + 56, 4);
  butterfly_dct_post_s16_x8(x7 + 60, x8 + 60, x9 + 60, 4);

  // stage 10
  butterfly_s16_s32_x8_0112_neon(cospi1, x9[63], x9[32], &output[1],
                                 &output[63]);
  butterfly_s16_s32_x8_1003_neon(cospi31, x9[62], x9[33], &output[33],
                                 &output[31]);
  butterfly_s16_s32_x8_0112_neon(cospi17, x9[61], x9[34], &output[17],
                                 &output[47]);
  butterfly_s16_s32_x8_1003_neon(cospi15, x9[60], x9[35], &output[49],
                                 &output[15]);
  butterfly_s16_s32_x8_0112_neon(cospi9, x9[59], x9[36], &output[9],
                                 &output[55]);
  butterfly_s16_s32_x8_1003_neon(cospi23, x9[58], x9[37], &output[41],
                                 &output[23]);
  butterfly_s16_s32_x8_0112_neon(cospi25, x9[57], x9[38], &output[25],
                                 &output[39]);
  butterfly_s16_s32_x8_1003_neon(cospi7, x9[56], x9[39], &output[57],
                                 &output[7]);
  butterfly_s16_s32_x8_0112_neon(cospi5, x9[55], x9[40], &output[5],
                                 &output[59]);
  butterfly_s16_s32_x8_1003_neon(cospi27, x9[54], x9[41], &output[37],
                                 &output[27]);
  butterfly_s16_s32_x8_0112_neon(cospi21, x9[53], x9[42], &output[21],
                                 &output[43]);
  butterfly_s16_s32_x8_1003_neon(cospi11, x9[52], x9[43], &output[53],
                                 &output[11]);
  butterfly_s16_s32_x8_0112_neon(cospi13, x9[51], x9[44], &output[13],
                                 &output[51]);
  butterfly_s16_s32_x8_1003_neon(cospi19, x9[50], x9[45], &output[45],
                                 &output[19]);
  butterfly_s16_s32_x8_0112_neon(cospi29, x9[49], x9[46], &output[29],
                                 &output[35]);
  butterfly_s16_s32_x8_1003_neon(cospi3, x9[48], x9[47], &output[61],
                                 &output[3]);

  // stage 11
  output[0] = x6[0];
  output[2] = x9[16];
  output[4] = x8[8];
  output[6] = x9[24];
  output[8] = x7[4];
  output[10] = x9[20];
  output[12] = x8[12];
  output[14] = x9[28];
  output[16] = x6[2];
  output[18] = x9[18];
  output[20] = x8[10];
  output[22] = x9[26];
  output[24] = x7[6];
  output[26] = x9[22];
  output[28] = x8[14];
  output[30] = x9[30];
  output[32] = x6[1];
  output[34] = x9[17];
  output[36] = x8[9];
  output[38] = x9[25];
  output[40] = x7[5];
  output[42] = x9[21];
  output[44] = x8[13];
  output[46] = x9[29];
  output[48] = x6[3];
  output[52] = x8[11];
  output[54] = x9[27];
  output[56] = x7[7];
  output[58] = x9[23];
  output[60] = x8[15];
  output[62] = x9[31];
}

static AOM_FORCE_INLINE void fadst8x8_neon(const int16x8_t *input,
                                           int16x8_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi4_12 = vld1q_s16(&cospi[4 * 4]);
  const int16x8_t cospi20_28 = vld1q_s16(&cospi[4 * 6]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi4 = vget_low_s16(cospi4_12);
  const int16x4_t cospi12 = vget_high_s16(cospi4_12);
  const int16x4_t cospi20 = vget_low_s16(cospi20_28);
  const int16x4_t cospi28 = vget_high_s16(cospi20_28);

  // stage 2
  int16x8_t x2[8];
  butterfly_s16_s32_x8_0332_neon(cospi32, input[4], input[3], &x2[2], &x2[3]);
  butterfly_s16_s32_x8_0112_neon(cospi32, input[2], input[5], &x2[7], &x2[6]);

  // stage 3
  int16x8_t x3[8];
  x3[0] = vqaddq_s16(input[0], x2[2]);
  x3[1] = vqsubq_s16(x2[3], input[7]);
  x3[2] = vqsubq_s16(input[0], x2[2]);
  x3[3] = vqaddq_s16(input[7], x2[3]);
  x3[4] = vqsubq_s16(x2[6], input[1]);
  x3[5] = vqaddq_s16(input[6], x2[7]);
  x3[6] = vqaddq_s16(input[1], x2[6]);
  x3[7] = vqsubq_s16(input[6], x2[7]);

  // stage 4
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[4], x3[5], &x3[4], &x3[5]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[7], x3[6], &x3[6], &x3[7]);

  // stage 5
  int16x8_t x5[8];
  x5[0] = vqaddq_s16(x3[0], x3[4]);
  x5[1] = vqaddq_s16(x3[1], x3[5]);
  x5[2] = vqaddq_s16(x3[2], x3[6]);
  x5[3] = vqsubq_s16(x3[7], x3[3]);
  x5[4] = vqsubq_s16(x3[0], x3[4]);
  x5[5] = vqsubq_s16(x3[1], x3[5]);
  x5[6] = vqsubq_s16(x3[2], x3[6]);
  x5[7] = vqaddq_s16(x3[3], x3[7]);

  // stage 6
  butterfly_s16_s32_x8_0112_neon(cospi4, x5[0], x5[1], &output[7], &output[0]);
  butterfly_s16_s32_x8_0112_neon(cospi20, x5[2], x5[3], &output[5], &output[2]);
  butterfly_s16_s32_x8_1003_neon(cospi28, x5[4], x5[5], &output[3], &output[4]);
  butterfly_s16_s32_x8_0112_neon(cospi12, x5[6], x5[7], &output[6], &output[1]);
}

static AOM_FORCE_INLINE void fadst4x16_neon(const int16x4_t *input,
                                            int16x4_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);
  const int16x8_t cospi2_6 = vld1q_s16(&cospi[4 * 8]);
  const int16x8_t cospi10_14 = vld1q_s16(&cospi[4 * 10]);
  const int16x8_t cospi18_22 = vld1q_s16(&cospi[4 * 12]);
  const int16x8_t cospi26_30 = vld1q_s16(&cospi[4 * 14]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);
  const int16x4_t cospi2 = vget_low_s16(cospi2_6);
  const int16x4_t cospi6 = vget_high_s16(cospi2_6);
  const int16x4_t cospi10 = vget_low_s16(cospi10_14);
  const int16x4_t cospi14 = vget_high_s16(cospi10_14);
  const int16x4_t cospi18 = vget_low_s16(cospi18_22);
  const int16x4_t cospi22 = vget_high_s16(cospi18_22);
  const int16x4_t cospi26 = vget_low_s16(cospi26_30);
  const int16x4_t cospi30 = vget_high_s16(cospi26_30);

  // stage 2
  int16x4_t x2[8];
  butterfly_s16_s32_x4_0332_neon(cospi32, input[8], input[7], &x2[0], &x2[1]);
  butterfly_s16_s32_x4_0112_neon(cospi32, input[4], input[11], &x2[3], &x2[2]);
  butterfly_s16_s32_x4_0112_neon(cospi32, input[6], input[9], &x2[5], &x2[4]);
  butterfly_s16_s32_x4_0332_neon(cospi32, input[10], input[5], &x2[6], &x2[7]);

  // stage 3
  int16x4_t x3[16];
  x3[0] = vqadd_s16(input[0], x2[0]);
  x3[1] = vqsub_s16(x2[1], input[15]);
  x3[2] = vqsub_s16(input[0], x2[0]);
  x3[3] = vqadd_s16(input[15], x2[1]);
  x3[4] = vqsub_s16(x2[2], input[3]);
  x3[5] = vqadd_s16(input[12], x2[3]);
  x3[6] = vqadd_s16(input[3], x2[2]);
  x3[7] = vqsub_s16(input[12], x2[3]);
  x3[8] = vqsub_s16(x2[4], input[1]);
  x3[9] = vqadd_s16(input[14], x2[5]);
  x3[10] = vqadd_s16(input[1], x2[4]);
  x3[11] = vqsub_s16(input[14], x2[5]);
  x3[12] = vqadd_s16(input[2], x2[6]);
  x3[13] = vqsub_s16(x2[7], input[13]);
  x3[14] = vqsub_s16(input[2], x2[6]);
  x3[15] = vqadd_s16(input[13], x2[7]);

  // stage 4
  butterfly_s16_s32_x4_0112_neon(cospi16, x3[4], x3[5], &x3[4], &x3[5]);
  butterfly_s16_s32_x4_0112_neon(cospi16, x3[7], x3[6], &x3[6], &x3[7]);
  butterfly_s16_s32_x4_0112_neon(cospi16, x3[12], x3[13], &x3[12], &x3[13]);
  butterfly_s16_s32_x4_0332_neon(cospi16, x3[14], x3[15], &x3[15], &x3[14]);

  // stage 5
  int16x4_t x5[16];
  x5[0] = vqadd_s16(x3[0], x3[4]);
  x5[1] = vqadd_s16(x3[1], x3[5]);
  x5[2] = vqadd_s16(x3[2], x3[6]);
  x5[3] = vqsub_s16(x3[7], x3[3]);
  x5[4] = vqsub_s16(x3[0], x3[4]);
  x5[5] = vqsub_s16(x3[1], x3[5]);
  x5[6] = vqsub_s16(x3[2], x3[6]);
  x5[7] = vqadd_s16(x3[3], x3[7]);
  x5[8] = vqadd_s16(x3[8], x3[12]);
  x5[9] = vqadd_s16(x3[9], x3[13]);
  x5[10] = vqsub_s16(x3[14], x3[10]);
  x5[11] = vqadd_s16(x3[11], x3[15]);
  x5[12] = vqsub_s16(x3[8], x3[12]);
  x5[13] = vqsub_s16(x3[9], x3[13]);
  x5[14] = vqadd_s16(x3[10], x3[14]);
  x5[15] = vqsub_s16(x3[11], x3[15]);

  // stage 6
  butterfly_s16_s32_x4_0112_neon(cospi8, x5[8], x5[9], &x5[8], &x5[9]);
  butterfly_s16_s32_x4_1003_neon(cospi24, x5[10], x5[11], &x5[10], &x5[11]);
  butterfly_s16_s32_x4_1003_neon(cospi8, x5[13], x5[12], &x5[13], &x5[12]);
  butterfly_s16_s32_x4_1003_neon(cospi24, x5[15], x5[14], &x5[14], &x5[15]);

  // stage 7
  int16x4_t x7[16];
  x7[0] = vqadd_s16(x5[0], x5[8]);
  x7[1] = vqadd_s16(x5[1], x5[9]);
  x7[2] = vqadd_s16(x5[2], x5[10]);
  x7[3] = vqadd_s16(x5[3], x5[11]);
  x7[4] = vqadd_s16(x5[4], x5[12]);
  x7[5] = vqadd_s16(x5[5], x5[13]);
  x7[6] = vqadd_s16(x5[6], x5[14]);
  x7[7] = vqsub_s16(x5[15], x5[7]);
  x7[8] = vqsub_s16(x5[0], x5[8]);
  x7[9] = vqsub_s16(x5[1], x5[9]);
  x7[10] = vqsub_s16(x5[2], x5[10]);
  x7[11] = vqsub_s16(x5[3], x5[11]);
  x7[12] = vqsub_s16(x5[4], x5[12]);
  x7[13] = vqsub_s16(x5[5], x5[13]);
  x7[14] = vqsub_s16(x5[6], x5[14]);
  x7[15] = vqadd_s16(x5[7], x5[15]);

  // stage 8
  butterfly_s16_s32_x4_0112_neon(cospi2, x7[0], x7[1], &output[15], &output[0]);
  butterfly_s16_s32_x4_0112_neon(cospi10, x7[2], x7[3], &output[13],
                                 &output[2]);
  butterfly_s16_s32_x4_0112_neon(cospi18, x7[4], x7[5], &output[11],
                                 &output[4]);
  butterfly_s16_s32_x4_0112_neon(cospi26, x7[6], x7[7], &output[9], &output[6]);
  butterfly_s16_s32_x4_1003_neon(cospi30, x7[8], x7[9], &output[7], &output[8]);
  butterfly_s16_s32_x4_1003_neon(cospi22, x7[10], x7[11], &output[5],
                                 &output[10]);
  butterfly_s16_s32_x4_1003_neon(cospi14, x7[12], x7[13], &output[3],
                                 &output[12]);
  butterfly_s16_s32_x4_0112_neon(cospi6, x7[14], x7[15], &output[14],
                                 &output[1]);
}

static AOM_FORCE_INLINE void fadst8x16_neon(const int16x8_t *input,
                                            int16x8_t *output, int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);
  const int16x8_t cospi2_6 = vld1q_s16(&cospi[4 * 8]);
  const int16x8_t cospi10_14 = vld1q_s16(&cospi[4 * 10]);
  const int16x8_t cospi18_22 = vld1q_s16(&cospi[4 * 12]);
  const int16x8_t cospi26_30 = vld1q_s16(&cospi[4 * 14]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);
  const int16x4_t cospi2 = vget_low_s16(cospi2_6);
  const int16x4_t cospi6 = vget_high_s16(cospi2_6);
  const int16x4_t cospi10 = vget_low_s16(cospi10_14);
  const int16x4_t cospi14 = vget_high_s16(cospi10_14);
  const int16x4_t cospi18 = vget_low_s16(cospi18_22);
  const int16x4_t cospi22 = vget_high_s16(cospi18_22);
  const int16x4_t cospi26 = vget_low_s16(cospi26_30);
  const int16x4_t cospi30 = vget_high_s16(cospi26_30);

  // stage 2
  int16x8_t x2[8];
  butterfly_s16_s32_x8_0332_neon(cospi32, input[8], input[7], &x2[0], &x2[1]);
  butterfly_s16_s32_x8_0112_neon(cospi32, input[4], input[11], &x2[3], &x2[2]);
  butterfly_s16_s32_x8_0112_neon(cospi32, input[6], input[9], &x2[5], &x2[4]);
  butterfly_s16_s32_x8_0332_neon(cospi32, input[10], input[5], &x2[6], &x2[7]);

  // stage 3
  int16x8_t x3[16];
  x3[0] = vqaddq_s16(input[0], x2[0]);
  x3[1] = vqsubq_s16(x2[1], input[15]);
  x3[2] = vqsubq_s16(input[0], x2[0]);
  x3[3] = vqaddq_s16(input[15], x2[1]);
  x3[4] = vqsubq_s16(x2[2], input[3]);
  x3[5] = vqaddq_s16(input[12], x2[3]);
  x3[6] = vqaddq_s16(input[3], x2[2]);
  x3[7] = vqsubq_s16(input[12], x2[3]);
  x3[8] = vqsubq_s16(x2[4], input[1]);
  x3[9] = vqaddq_s16(input[14], x2[5]);
  x3[10] = vqaddq_s16(input[1], x2[4]);
  x3[11] = vqsubq_s16(input[14], x2[5]);
  x3[12] = vqaddq_s16(input[2], x2[6]);
  x3[13] = vqsubq_s16(x2[7], input[13]);
  x3[14] = vqsubq_s16(input[2], x2[6]);
  x3[15] = vqaddq_s16(input[13], x2[7]);

  // stage 4
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[4], x3[5], &x3[4], &x3[5]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[7], x3[6], &x3[6], &x3[7]);
  butterfly_s16_s32_x8_0112_neon(cospi16, x3[12], x3[13], &x3[12], &x3[13]);
  butterfly_s16_s32_x8_0332_neon(cospi16, x3[14], x3[15], &x3[15], &x3[14]);

  // stage 5
  int16x8_t x5[16];
  x5[0] = vqaddq_s16(x3[0], x3[4]);
  x5[1] = vqaddq_s16(x3[1], x3[5]);
  x5[2] = vqaddq_s16(x3[2], x3[6]);
  x5[3] = vqsubq_s16(x3[7], x3[3]);
  x5[4] = vqsubq_s16(x3[0], x3[4]);
  x5[5] = vqsubq_s16(x3[1], x3[5]);
  x5[6] = vqsubq_s16(x3[2], x3[6]);
  x5[7] = vqaddq_s16(x3[3], x3[7]);
  x5[8] = vqaddq_s16(x3[8], x3[12]);
  x5[9] = vqaddq_s16(x3[9], x3[13]);
  x5[10] = vqsubq_s16(x3[14], x3[10]);
  x5[11] = vqaddq_s16(x3[11], x3[15]);
  x5[12] = vqsubq_s16(x3[8], x3[12]);
  x5[13] = vqsubq_s16(x3[9], x3[13]);
  x5[14] = vqaddq_s16(x3[10], x3[14]);
  x5[15] = vqsubq_s16(x3[11], x3[15]);

  // stage 6
  butterfly_s16_s32_x8_0112_neon(cospi8, x5[8], x5[9], &x5[8], &x5[9]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x5[10], x5[11], &x5[10], &x5[11]);
  butterfly_s16_s32_x8_1003_neon(cospi8, x5[13], x5[12], &x5[13], &x5[12]);
  butterfly_s16_s32_x8_1003_neon(cospi24, x5[15], x5[14], &x5[14], &x5[15]);

  // stage 7
  int16x8_t x7[16];
  x7[0] = vqaddq_s16(x5[0], x5[8]);
  x7[1] = vqaddq_s16(x5[1], x5[9]);
  x7[2] = vqaddq_s16(x5[2], x5[10]);
  x7[3] = vqaddq_s16(x5[3], x5[11]);
  x7[4] = vqaddq_s16(x5[4], x5[12]);
  x7[5] = vqaddq_s16(x5[5], x5[13]);
  x7[6] = vqaddq_s16(x5[6], x5[14]);
  x7[7] = vqsubq_s16(x5[15], x5[7]);
  x7[8] = vqsubq_s16(x5[0], x5[8]);
  x7[9] = vqsubq_s16(x5[1], x5[9]);
  x7[10] = vqsubq_s16(x5[2], x5[10]);
  x7[11] = vqsubq_s16(x5[3], x5[11]);
  x7[12] = vqsubq_s16(x5[4], x5[12]);
  x7[13] = vqsubq_s16(x5[5], x5[13]);
  x7[14] = vqsubq_s16(x5[6], x5[14]);
  x7[15] = vqaddq_s16(x5[7], x5[15]);

  // stage 8
  butterfly_s16_s32_x8_0112_neon(cospi2, x7[0], x7[1], &output[15], &output[0]);
  butterfly_s16_s32_x8_0112_neon(cospi10, x7[2], x7[3], &output[13],
                                 &output[2]);
  butterfly_s16_s32_x8_0112_neon(cospi18, x7[4], x7[5], &output[11],
                                 &output[4]);
  butterfly_s16_s32_x8_0112_neon(cospi26, x7[6], x7[7], &output[9], &output[6]);
  butterfly_s16_s32_x8_1003_neon(cospi30, x7[8], x7[9], &output[7], &output[8]);
  butterfly_s16_s32_x8_1003_neon(cospi22, x7[10], x7[11], &output[5],
                                 &output[10]);
  butterfly_s16_s32_x8_1003_neon(cospi14, x7[12], x7[13], &output[3],
                                 &output[12]);
  butterfly_s16_s32_x8_0112_neon(cospi6, x7[14], x7[15], &output[14],
                                 &output[1]);
}

static AOM_FORCE_INLINE void fidentity4x4_neon(const int16x4_t *const input,
                                               int16x4_t *const output,
                                               const int cos_bit) {
  (void)cos_bit;
  round_shift_sqrt2_s16_s16_4xn_neon(input, output, 4);
}

static AOM_FORCE_INLINE void fidentity8x4_neon(const int16x8_t *const input,
                                               int16x8_t *const output,
                                               const int cos_bit) {
  (void)cos_bit;
  round_shift_sqrt2_s16_s16_8xn_neon(input, output, 4);
}

static AOM_FORCE_INLINE void fidentity4x8_neon(const int16x4_t *input,
                                               int16x4_t *output, int cos_bit) {
  (void)cos_bit;
  shift_left_1_s16_x4(input, output, 8);
}

static AOM_FORCE_INLINE void fidentity8x8_neon(const int16x8_t *input,
                                               int16x8_t *output, int cos_bit) {
  (void)cos_bit;
  shift_left_1_s16_x8(input, output, 8);
}

static AOM_FORCE_INLINE void fidentity4x16_neon(const int16x4_t *input,
                                                int16x4_t *output,
                                                int cos_bit) {
  (void)cos_bit;
  round_shift_2sqrt2_s16_s16_4xn_neon(input, output, 16);
}

static AOM_FORCE_INLINE void fidentity8x16_neon(const int16x8_t *input,
                                                int16x8_t *output,
                                                int cos_bit) {
  (void)cos_bit;
  round_shift_2sqrt2_s16_s16_8xn_neon(input, output, 16);
}

static AOM_FORCE_INLINE void fidentity8x32_neon(const int16x8_t *input,
                                                int16x8_t *output,
                                                int cos_bit) {
  (void)cos_bit;
  shift_left_2_s16_x8(input, output, 32);
}

#define TRANSFORM_COL(name, tw, n)                                          \
  static void name##_col_neon(const int16_t *input, int16x##tw##_t *output, \
                              int stride, int cos_bit) {                    \
    int16x##tw##_t buf0[n];                                                 \
    load_buffer_s16_x##tw(input, stride, buf0, n);                          \
    shift_left_2_s16_x##tw(buf0, buf0, n);                                  \
    name##_neon(buf0, output, cos_bit);                                     \
  }

TRANSFORM_COL(fadst4x4, 4, 4)
TRANSFORM_COL(fadst4x8, 4, 8)
TRANSFORM_COL(fadst4x16, 4, 16)
TRANSFORM_COL(fadst8x4, 8, 4)
TRANSFORM_COL(fadst8x8, 8, 8)
TRANSFORM_COL(fadst8x16, 8, 16)
TRANSFORM_COL(fdct4x4, 4, 4)
TRANSFORM_COL(fdct4x8, 4, 8)
TRANSFORM_COL(fdct4x16, 4, 16)
TRANSFORM_COL(fdct8x4, 8, 4)
TRANSFORM_COL(fdct8x8, 8, 8)
TRANSFORM_COL(fdct8x16, 8, 16)
TRANSFORM_COL(fdct8x32, 8, 32)
TRANSFORM_COL(fidentity4x4, 4, 4)
TRANSFORM_COL(fidentity4x8, 4, 8)
TRANSFORM_COL(fidentity4x16, 4, 16)
TRANSFORM_COL(fidentity8x4, 8, 4)
TRANSFORM_COL(fidentity8x8, 8, 8)
TRANSFORM_COL(fidentity8x16, 8, 16)
TRANSFORM_COL(fidentity8x32, 8, 32)

#define TRANSFORM_ROW(name, tw, n)                                          \
  static void name##_row_neon(const int16x##tw##_t *input, int32_t *output, \
                              int stride, int cos_bit) {                    \
    int16x##tw##_t buf0[n];                                                 \
    name##_neon(input, buf0, cos_bit);                                      \
    store_buffer_s16_x##tw(buf0, output, stride, n);                        \
  }

#define TRANSFORM_ROW_RECT(name, tw, n)                                        \
  static void name##_row_rect_neon(const int16x##tw##_t *input,                \
                                   int32_t *output, int stride, int cos_bit) { \
    int16x##tw##_t buf0[n];                                                    \
    name##_neon(input, buf0, cos_bit);                                         \
    store_rect_buffer_s16_x##tw(buf0, output, stride, n);                      \
  }

TRANSFORM_ROW(fadst4x4, 4, 4)
TRANSFORM_ROW(fadst4x16, 4, 16)
TRANSFORM_ROW(fadst8x4, 8, 4)
TRANSFORM_ROW(fadst8x8, 8, 8)
TRANSFORM_ROW(fadst8x16, 8, 16)
TRANSFORM_ROW(fdct4x4, 4, 4)
TRANSFORM_ROW(fdct4x16, 4, 16)
TRANSFORM_ROW(fdct8x4, 8, 4)
TRANSFORM_ROW(fdct8x8, 8, 8)
TRANSFORM_ROW(fdct8x16, 8, 16)
TRANSFORM_ROW(fdct8x32, 8, 32)
TRANSFORM_ROW(fidentity4x4, 4, 4)
TRANSFORM_ROW(fidentity4x16, 4, 16)
TRANSFORM_ROW(fidentity8x4, 8, 4)
TRANSFORM_ROW(fidentity8x8, 8, 8)
TRANSFORM_ROW(fidentity8x16, 8, 16)
TRANSFORM_ROW(fidentity8x32, 8, 32)

TRANSFORM_ROW_RECT(fadst4x8, 4, 8)
TRANSFORM_ROW_RECT(fadst8x4, 8, 4)
TRANSFORM_ROW_RECT(fadst8x8, 8, 8)
TRANSFORM_ROW_RECT(fadst8x16, 8, 16)
TRANSFORM_ROW_RECT(fdct4x8, 4, 8)
TRANSFORM_ROW_RECT(fdct8x4, 8, 4)
TRANSFORM_ROW_RECT(fdct8x8, 8, 8)
TRANSFORM_ROW_RECT(fdct8x16, 8, 16)
TRANSFORM_ROW_RECT(fdct8x32, 8, 32)
TRANSFORM_ROW_RECT(fidentity4x8, 4, 8)
TRANSFORM_ROW_RECT(fidentity8x4, 8, 4)
TRANSFORM_ROW_RECT(fidentity8x8, 8, 8)
TRANSFORM_ROW_RECT(fidentity8x16, 8, 16)
TRANSFORM_ROW_RECT(fidentity8x32, 8, 32)

typedef void (*transform_1d_lbd_4_neon)(const int16x4_t *input,
                                        int16x4_t *output, int cos_bit);
typedef void (*transform_1d_lbd_8_neon)(const int16x8_t *input,
                                        int16x8_t *output, int cos_bit);

typedef void (*col_transform_1d_lbd_4_neon)(const int16_t *input,
                                            int16x4_t *output, int stride,
                                            int cos_bit);
typedef void (*col_transform_1d_lbd_8_neon)(const int16_t *input,
                                            int16x8_t *output, int stride,
                                            int cos_bit);

typedef void (*row_transform_1d_lbd_4_neon)(const int16x4_t *input,
                                            int32_t *output, int stride,
                                            int cos_bit);
typedef void (*row_transform_1d_lbd_8_neon)(const int16x8_t *input,
                                            int32_t *output, int stride,
                                            int cos_bit);

static const col_transform_1d_lbd_4_neon col_txfm4x8_arr[TX_TYPES] = {
  fdct4x8_col_neon,       // DCT_DCT
  fadst4x8_col_neon,      // ADST_DCT
  fdct4x8_col_neon,       // DCT_ADST
  fadst4x8_col_neon,      // ADST_ADST
  fadst4x8_col_neon,      // FLIPADST_DCT
  fdct4x8_col_neon,       // DCT_FLIPADST
  fadst4x8_col_neon,      // FLIPADST_FLIPADST
  fadst4x8_col_neon,      // ADST_FLIPADST
  fadst4x8_col_neon,      // FLIPADST_ADST
  fidentity4x8_col_neon,  // IDTX
  fdct4x8_col_neon,       // V_DCT
  fidentity4x8_col_neon,  // H_DCT
  fadst4x8_col_neon,      // V_ADST
  fidentity4x8_col_neon,  // H_ADST
  fadst4x8_col_neon,      // V_FLIPADST
  fidentity4x8_col_neon   // H_FLIPADST
};

static const row_transform_1d_lbd_8_neon row_txfm8x4_arr[TX_TYPES] = {
  fdct8x4_row_neon,       // DCT_DCT
  fdct8x4_row_neon,       // ADST_DCT
  fadst8x4_row_neon,      // DCT_ADST
  fadst8x4_row_neon,      // ADST_ADST
  fdct8x4_row_neon,       // FLIPADST_DCT
  fadst8x4_row_neon,      // DCT_FLIPADST
  fadst8x4_row_neon,      // FLIPADST_FLIPADST
  fadst8x4_row_neon,      // ADST_FLIPADST
  fadst8x4_row_neon,      // FLIPADST_ADST
  fidentity8x4_row_neon,  // IDTX
  fidentity8x4_row_neon,  // V_DCT
  fdct8x4_row_neon,       // H_DCT
  fidentity8x4_row_neon,  // V_ADST
  fadst8x4_row_neon,      // H_ADST
  fidentity8x4_row_neon,  // V_FLIPADST
  fadst8x4_row_neon       // H_FLIPADST
};

static const row_transform_1d_lbd_8_neon row_rect_txfm8x4_arr[TX_TYPES] = {
  fdct8x4_row_rect_neon,       // DCT_DCT
  fdct8x4_row_rect_neon,       // ADST_DCT
  fadst8x4_row_rect_neon,      // DCT_ADST
  fadst8x4_row_rect_neon,      // ADST_ADST
  fdct8x4_row_rect_neon,       // FLIPADST_DCT
  fadst8x4_row_rect_neon,      // DCT_FLIPADST
  fadst8x4_row_rect_neon,      // FLIPADST_FLIPADST
  fadst8x4_row_rect_neon,      // ADST_FLIPADST
  fadst8x4_row_rect_neon,      // FLIPADST_ADST
  fidentity8x4_row_rect_neon,  // IDTX
  fidentity8x4_row_rect_neon,  // V_DCT
  fdct8x4_row_rect_neon,       // H_DCT
  fidentity8x4_row_rect_neon,  // V_ADST
  fadst8x4_row_rect_neon,      // H_ADST
  fidentity8x4_row_rect_neon,  // V_FLIPADST
  fadst8x4_row_rect_neon       // H_FLIPADST
};

static const col_transform_1d_lbd_8_neon col_txfm8x4_arr[TX_TYPES] = {
  fdct8x4_col_neon,       // DCT_DCT
  fadst8x4_col_neon,      // ADST_DCT
  fdct8x4_col_neon,       // DCT_ADST
  fadst8x4_col_neon,      // ADST_ADST
  fadst8x4_col_neon,      // FLIPADST_DCT
  fdct8x4_col_neon,       // DCT_FLIPADST
  fadst8x4_col_neon,      // FLIPADST_FLIPADST
  fadst8x4_col_neon,      // ADST_FLIPADST
  fadst8x4_col_neon,      // FLIPADST_ADST
  fidentity8x4_col_neon,  // IDTX
  fdct8x4_col_neon,       // V_DCT
  fidentity8x4_col_neon,  // H_DCT
  fadst8x4_col_neon,      // V_ADST
  fidentity8x4_col_neon,  // H_ADST
  fadst8x4_col_neon,      // V_FLIPADST
  fidentity8x4_col_neon   // H_FLIPADST
};

static const row_transform_1d_lbd_4_neon row_rect_txfm4x8_arr[TX_TYPES] = {
  fdct4x8_row_rect_neon,       // DCT_DCT
  fdct4x8_row_rect_neon,       // ADST_DCT
  fadst4x8_row_rect_neon,      // DCT_ADST
  fadst4x8_row_rect_neon,      // ADST_ADST
  fdct4x8_row_rect_neon,       // FLIPADST_DCT
  fadst4x8_row_rect_neon,      // DCT_FLIPADST
  fadst4x8_row_rect_neon,      // FLIPADST_FLIPADST
  fadst4x8_row_rect_neon,      // ADST_FLIPADST
  fadst4x8_row_rect_neon,      // FLIPADST_ADST
  fidentity4x8_row_rect_neon,  // IDTX
  fidentity4x8_row_rect_neon,  // V_DCT
  fdct4x8_row_rect_neon,       // H_DCT
  fidentity4x8_row_rect_neon,  // V_ADST
  fadst4x8_row_rect_neon,      // H_ADST
  fidentity4x8_row_rect_neon,  // V_FLIPADST
  fadst4x8_row_rect_neon       // H_FLIPADST
};

static const col_transform_1d_lbd_8_neon col_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_col_neon,       // DCT_DCT
  fadst8x8_col_neon,      // ADST_DCT
  fdct8x8_col_neon,       // DCT_ADST
  fadst8x8_col_neon,      // ADST_ADST
  fadst8x8_col_neon,      // FLIPADST_DCT
  fdct8x8_col_neon,       // DCT_FLIPADST
  fadst8x8_col_neon,      // FLIPADST_FLIPADST
  fadst8x8_col_neon,      // ADST_FLIPADST
  fadst8x8_col_neon,      // FLIPADST_ADST
  fidentity8x8_col_neon,  // IDTX
  fdct8x8_col_neon,       // V_DCT
  fidentity8x8_col_neon,  // H_DCT
  fadst8x8_col_neon,      // V_ADST
  fidentity8x8_col_neon,  // H_ADST
  fadst8x8_col_neon,      // V_FLIPADST
  fidentity8x8_col_neon,  // H_FLIPADST
};

static const row_transform_1d_lbd_8_neon row_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_row_neon,       // DCT_DCT
  fdct8x8_row_neon,       // ADST_DCT
  fadst8x8_row_neon,      // DCT_ADST
  fadst8x8_row_neon,      // ADST_ADST
  fdct8x8_row_neon,       // FLIPADST_DCT
  fadst8x8_row_neon,      // DCT_FLIPADST
  fadst8x8_row_neon,      // FLIPADST_FLIPADST
  fadst8x8_row_neon,      // ADST_FLIPADST
  fadst8x8_row_neon,      // FLIPADST_ADST
  fidentity8x8_row_neon,  // IDTX
  fidentity8x8_row_neon,  // V_DCT
  fdct8x8_row_neon,       // H_DCT
  fidentity8x8_row_neon,  // V_ADST
  fadst8x8_row_neon,      // H_ADST
  fidentity8x8_row_neon,  // V_FLIPADST
  fadst8x8_row_neon       // H_FLIPADST
};

static const row_transform_1d_lbd_8_neon row_rect_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_row_rect_neon,       // DCT_DCT
  fdct8x8_row_rect_neon,       // ADST_DCT
  fadst8x8_row_rect_neon,      // DCT_ADST
  fadst8x8_row_rect_neon,      // ADST_ADST
  fdct8x8_row_rect_neon,       // FLIPADST_DCT
  fadst8x8_row_rect_neon,      // DCT_FLIPADST
  fadst8x8_row_rect_neon,      // FLIPADST_FLIPADST
  fadst8x8_row_rect_neon,      // ADST_FLIPADST
  fadst8x8_row_rect_neon,      // FLIPADST_ADST
  fidentity8x8_row_rect_neon,  // IDTX
  fidentity8x8_row_rect_neon,  // V_DCT
  fdct8x8_row_rect_neon,       // H_DCT
  fidentity8x8_row_rect_neon,  // V_ADST
  fadst8x8_row_rect_neon,      // H_ADST
  fidentity8x8_row_rect_neon,  // V_FLIPADST
  fadst8x8_row_rect_neon       // H_FLIPADST
};

static const col_transform_1d_lbd_4_neon col_txfm4x16_arr[TX_TYPES] = {
  fdct4x16_col_neon,       // DCT_DCT
  fadst4x16_col_neon,      // ADST_DCT
  fdct4x16_col_neon,       // DCT_ADST
  fadst4x16_col_neon,      // ADST_ADST
  fadst4x16_col_neon,      // FLIPADST_DCT
  fdct4x16_col_neon,       // DCT_FLIPADST
  fadst4x16_col_neon,      // FLIPADST_FLIPADST
  fadst4x16_col_neon,      // ADST_FLIPADST
  fadst4x16_col_neon,      // FLIPADST_ADST
  fidentity4x16_col_neon,  // IDTX
  fdct4x16_col_neon,       // V_DCT
  fidentity4x16_col_neon,  // H_DCT
  fadst4x16_col_neon,      // V_ADST
  fidentity4x16_col_neon,  // H_ADST
  fadst4x16_col_neon,      // V_FLIPADST
  fidentity4x16_col_neon   // H_FLIPADST
};

static const row_transform_1d_lbd_4_neon row_txfm4x16_arr[TX_TYPES] = {
  fdct4x16_row_neon,       // DCT_DCT
  fdct4x16_row_neon,       // ADST_DCT
  fadst4x16_row_neon,      // DCT_ADST
  fadst4x16_row_neon,      // ADST_ADST
  fdct4x16_row_neon,       // FLIPADST_DCT
  fadst4x16_row_neon,      // DCT_FLIPADST
  fadst4x16_row_neon,      // FLIPADST_FLIPADST
  fadst4x16_row_neon,      // ADST_FLIPADST
  fadst4x16_row_neon,      // FLIPADST_ADST
  fidentity4x16_row_neon,  // IDTX
  fidentity4x16_row_neon,  // V_DCT
  fdct4x16_row_neon,       // H_DCT
  fidentity4x16_row_neon,  // V_ADST
  fadst4x16_row_neon,      // H_ADST
  fidentity4x16_row_neon,  // V_FLIPADST
  fadst4x16_row_neon       // H_FLIPADST
};

static const col_transform_1d_lbd_8_neon col_txfm8x16_arr[TX_TYPES] = {
  fdct8x16_col_neon,       // DCT_DCT
  fadst8x16_col_neon,      // ADST_DCT
  fdct8x16_col_neon,       // DCT_ADST
  fadst8x16_col_neon,      // ADST_ADST
  fadst8x16_col_neon,      // FLIPADST_DCT
  fdct8x16_col_neon,       // DCT_FLIPADST
  fadst8x16_col_neon,      // FLIPADST_FLIPADST
  fadst8x16_col_neon,      // ADST_FLIPADST
  fadst8x16_col_neon,      // FLIPADST_ADST
  fidentity8x16_col_neon,  // IDTX
  fdct8x16_col_neon,       // V_DCT
  fidentity8x16_col_neon,  // H_DCT
  fadst8x16_col_neon,      // V_ADST
  fidentity8x16_col_neon,  // H_ADST
  fadst8x16_col_neon,      // V_FLIPADST
  fidentity8x16_col_neon   // H_FLIPADST
};

static const row_transform_1d_lbd_8_neon row_txfm8x16_arr[TX_TYPES] = {
  fdct8x16_row_neon,       // DCT_DCT
  fdct8x16_row_neon,       // ADST_DCT
  fadst8x16_row_neon,      // DCT_ADST
  fadst8x16_row_neon,      // ADST_ADST
  fdct8x16_row_neon,       // FLIPADST_DCT
  fadst8x16_row_neon,      // DCT_FLIPADST
  fadst8x16_row_neon,      // FLIPADST_FLIPADST
  fadst8x16_row_neon,      // ADST_FLIPADST
  fadst8x16_row_neon,      // FLIPADST_ADST
  fidentity8x16_row_neon,  // IDTX
  fidentity8x16_row_neon,  // V_DCT
  fdct8x16_row_neon,       // H_DCT
  fidentity8x16_row_neon,  // V_ADST
  fadst8x16_row_neon,      // H_ADST
  fidentity8x16_row_neon,  // V_FLIPADST
  fadst8x16_row_neon       // H_FLIPADST
};

static const row_transform_1d_lbd_8_neon row_rect_txfm8x16_arr[TX_TYPES] = {
  fdct8x16_row_rect_neon,       // DCT_DCT
  fdct8x16_row_rect_neon,       // ADST_DCT
  fadst8x16_row_rect_neon,      // DCT_ADST
  fadst8x16_row_rect_neon,      // ADST_ADST
  fdct8x16_row_rect_neon,       // FLIPADST_DCT
  fadst8x16_row_rect_neon,      // DCT_FLIPADST
  fadst8x16_row_rect_neon,      // FLIPADST_FLIPADST
  fadst8x16_row_rect_neon,      // ADST_FLIPADST
  fadst8x16_row_rect_neon,      // FLIPADST_ADST
  fidentity8x16_row_rect_neon,  // IDTX
  fidentity8x16_row_rect_neon,  // V_DCT
  fdct8x16_row_rect_neon,       // H_DCT
  fidentity8x16_row_rect_neon,  // V_ADST
  fadst8x16_row_rect_neon,      // H_ADST
  fidentity8x16_row_rect_neon,  // V_FLIPADST
  fadst8x16_row_rect_neon       // H_FLIPADST
};

static const row_transform_1d_lbd_8_neon row_txfm8x32_arr[TX_TYPES] = {
  fdct8x32_row_neon,       // DCT_DCT
  NULL,                    // ADST_DCT
  NULL,                    // DCT_ADST
  NULL,                    // ADST_ADST
  NULL,                    // FLIPADST_DCT
  NULL,                    // DCT_FLIPADST
  NULL,                    // FLIPADST_FLIPADST
  NULL,                    // ADST_FLIPADST
  NULL,                    // FLIPADST_ADST
  fidentity8x32_row_neon,  // IDTX
  fidentity8x32_row_neon,  // V_DCT
  fdct8x32_row_neon,       // H_DCT
  NULL,                    // V_ADST
  NULL,                    // H_ADST
  NULL,                    // V_FLIPADST
  NULL                     // H_FLIPADST
};

static const row_transform_1d_lbd_8_neon row_rect_txfm8x32_arr[TX_TYPES] = {
  fdct8x32_row_rect_neon,       // DCT_DCT
  NULL,                         // ADST_DCT
  NULL,                         // DCT_ADST
  NULL,                         // ADST_ADST
  NULL,                         // FLIPADST_DCT
  NULL,                         // DCT_FLIPADST
  NULL,                         // FLIPADST_FLIPADST
  NULL,                         // ADST_FLIPADST
  NULL,                         // FLIPADST_ADST
  fidentity8x32_row_rect_neon,  // IDTX
  fidentity8x32_row_rect_neon,  // V_DCT
  fdct8x32_row_rect_neon,       // H_DCT
  NULL,                         // V_ADST
  NULL,                         // H_ADST
  NULL,                         // V_FLIPADST
  NULL                          // H_FLIPADST
};

static const col_transform_1d_lbd_8_neon col_txfm8x32_arr[TX_TYPES] = {
  fdct8x32_col_neon,       // DCT_DCT
  NULL,                    // ADST_DCT
  NULL,                    // DCT_ADST
  NULL,                    // ADST_ADST
  NULL,                    // FLIPADST_DCT
  NULL,                    // DCT_FLIPADST
  NULL,                    // FLIPADST_FLIPADST
  NULL,                    // ADST_FLIPADST
  NULL,                    // FLIPADST_ADST
  fidentity8x32_col_neon,  // IDTX
  fdct8x32_col_neon,       // V_DCT
  fidentity8x32_col_neon,  // H_DCT
  NULL,                    // V_ADST
  NULL,                    // H_ADST
  NULL,                    // V_FLIPADST
  NULL                     // H_FLIPADST
};

static void lowbd_fwd_txfm2d_4x4_neon(const int16_t *input, int32_t *output,
                                      int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 4);

  int16x4_t buf0[4], buf1[4];
  switch (tx_type) {
    case DCT_DCT:
      fdct4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fdct4x4_row_neon(buf1, output, 4, 13);
      break;
    case ADST_DCT:
      fadst4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fdct4x4_row_neon(buf1, output, 4, 13);
      break;
    case DCT_ADST:
      fdct4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fadst4x4_row_neon(buf1, output, 4, 13);
      break;
    case ADST_ADST:
      fadst4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fadst4x4_row_neon(buf1, output, 4, 13);
      break;
    case FLIPADST_DCT:
      fadst4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fdct4x4_row_neon(buf1, output, 4, 13);
      break;
    case DCT_FLIPADST:
      fdct4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      flip_buf_4_neon(buf1, buf0, 4);
      fadst4x4_row_neon(buf0, output, 4, 13);
      break;
    case FLIPADST_FLIPADST:
      fadst4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      flip_buf_4_neon(buf1, buf0, 4);
      fadst4x4_row_neon(buf0, output, 4, 13);
      break;
    case ADST_FLIPADST:
      fadst4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      flip_buf_4_neon(buf1, buf0, 4);
      fadst4x4_row_neon(buf0, output, 4, 13);
      break;
    case FLIPADST_ADST:
      fadst4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fadst4x4_row_neon(buf1, output, 4, 13);
      break;
    case IDTX:
      fidentity4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fidentity4x4_row_neon(buf1, output, 4, 13);
      break;
    case V_DCT:
      fdct4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fidentity4x4_row_neon(buf1, output, 4, 13);
      break;
    case H_DCT:
      fidentity4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fdct4x4_row_neon(buf1, output, 4, 13);
      break;
    case V_ADST:
      fadst4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fidentity4x4_row_neon(buf1, output, 4, 13);
      break;
    case H_ADST:
      fidentity4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fadst4x4_row_neon(buf1, output, 4, 13);
      break;
    case V_FLIPADST:
      fadst4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      fidentity4x4_row_neon(buf1, output, 4, 13);
      break;
    case H_FLIPADST:
      fidentity4x4_col_neon(input, buf0, stride, 13);
      transpose_arrays_s16_4x4(buf0, buf1);
      flip_buf_4_neon(buf1, buf0, 4);
      fadst4x4_row_neon(buf0, output, 4, 13);
      break;
  }
}

static void lowbd_fwd_txfm2d_4x8_neon(const int16_t *input, int32_t *output,
                                      int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x4_t buf0[8];
  int16x8_t buf1[8];
  const col_transform_1d_lbd_4_neon col_txfm = col_txfm4x8_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_rect_txfm8x4_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 8);
  col_txfm(input, buf0, stride, 13);
  shift_right_1_round_s16_x4(buf0, buf0, 8);
  transpose_arrays_s16_4x8(buf0, buf1);

  if (lr_flip) {
    int16x8_t buf2[8];
    flip_buf_8_neon(buf1, buf2, 4);
    row_txfm(buf2, output, 8, 13);
  } else {
    row_txfm(buf1, output, 8, 13);
  }
}

static void lowbd_fwd_txfm2d_4x16_neon(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x4_t buf0[16];
  int16x8_t buf1[16];
  const col_transform_1d_lbd_4_neon col_txfm = col_txfm4x16_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_txfm8x4_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 16);
  col_txfm(input, buf0, stride, 13);
  shift_right_1_round_s16_x4(buf0, buf0, 16);
  transpose_arrays_s16_4x8(buf0, buf1);
  transpose_arrays_s16_4x8(buf0 + 8, buf1 + 8);

  for (int i = 0; i < 2; i++) {
    if (lr_flip) {
      int16x8_t buf2[16];
      flip_buf_8_neon(buf1 + 8 * i, buf2, 4);
      row_txfm(buf2, output + 8 * i, 16, 12);
    } else {
      int16x8_t *buf = buf1 + 8 * i;
      row_txfm(buf, output + 8 * i, 16, 12);
    }
  }
}

static void lowbd_fwd_txfm2d_8x4_neon(const int16_t *input, int32_t *output,
                                      int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[8];
  int16x4_t buf1[8];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x4_arr[tx_type];
  const row_transform_1d_lbd_4_neon row_txfm = row_rect_txfm4x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 4);
  col_txfm(input, buf0, stride, 13);
  shift_right_1_round_s16_x8(buf0, buf0, 4);
  transpose_arrays_s16_8x4(buf0, buf1);

  if (lr_flip) {
    int16x4_t buf2[8];
    flip_buf_4_neon(buf1, buf2, 8);
    row_txfm(buf2, output, 4, 13);
  } else {
    row_txfm(buf1, output, 4, 13);
  }
}

static void lowbd_fwd_txfm2d_8x8_neon(const int16_t *input, int32_t *output,
                                      int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 8);

  int16x8_t buf0[8], buf1[8];

  switch (tx_type) {
    case DCT_DCT:
      fdct8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fdct8x8_row_neon(buf1, output, 8, 13);
      break;
    case ADST_DCT:
      fadst8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fdct8x8_row_neon(buf1, output, 8, 13);
      break;
    case DCT_ADST:
      fdct8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fadst8x8_row_neon(buf1, output, 8, 13);
      break;
    case ADST_ADST:
      fadst8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fadst8x8_row_neon(buf1, output, 8, 13);
      break;
    case FLIPADST_DCT:
      fadst8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fdct8x8_row_neon(buf1, output, 8, 13);
      break;
    case DCT_FLIPADST:
      fdct8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      flip_buf_8_neon(buf1, buf0, 8);
      fadst8x8_row_neon(buf0, output, 8, 13);
      break;
    case FLIPADST_FLIPADST:
      fadst8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      flip_buf_8_neon(buf1, buf0, 8);
      fadst8x8_row_neon(buf0, output, 8, 13);
      break;
    case ADST_FLIPADST:
      fadst8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      flip_buf_8_neon(buf1, buf0, 8);
      fadst8x8_row_neon(buf0, output, 8, 13);
      break;
    case FLIPADST_ADST:
      fadst8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fadst8x8_row_neon(buf1, output, 8, 13);
      break;
    case IDTX:
      fidentity8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fidentity8x8_row_neon(buf1, output, 8, 13);
      break;
    case V_DCT:
      fdct8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fidentity8x8_row_neon(buf1, output, 8, 13);
      break;
    case H_DCT:
      fidentity8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fdct8x8_row_neon(buf1, output, 8, 13);
      break;
    case V_ADST:
      fadst8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fidentity8x8_row_neon(buf1, output, 8, 13);
      break;
    case H_ADST:
      fidentity8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fadst8x8_row_neon(buf1, output, 8, 13);
      break;
    case V_FLIPADST:
      fadst8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      fidentity8x8_row_neon(buf1, output, 8, 13);
      break;
    case H_FLIPADST:
      fidentity8x8_col_neon(input, buf0, stride, 13);
      shift_right_1_round_s16_x8(buf0, buf0, 8);
      transpose_arrays_s16_8x8(buf0, buf1);
      flip_buf_8_neon(buf1, buf0, 8);
      fadst8x8_row_neon(buf0, output, 8, 13);
      break;
  }
}

static void lowbd_fwd_txfm2d_8x16_neon(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16], buf1[16];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x16_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_rect_txfm8x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 16);
  col_txfm(input, buf0, stride, 13);
  shift_right_2_round_s16_x8(buf0, buf0, 16);
  transpose_arrays_s16_8x8(buf0, buf1);
  transpose_arrays_s16_8x8(buf0 + 8, buf1 + 8);

  for (int i = 0; i < 2; i++) {
    if (lr_flip) {
      flip_buf_8_neon(buf1 + 8 * i, buf0, 8);
      row_txfm(buf0, output + 8 * i, 16, 13);
    } else {
      int16x8_t *buf = buf1 + 8 * i;
      row_txfm(buf, output + 8 * i, 16, 13);
    }
  }
}

static void lowbd_fwd_txfm2d_8x32_neon(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[32];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x32_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_txfm8x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 32);
  col_txfm(input, buf0, stride, 12);
  shift_right_2_round_s16_x8(buf0, buf0, 32);
  transpose_arrays_s16_8x8(buf0, buf1);
  transpose_arrays_s16_8x8(buf0 + 8, buf1 + 8);
  transpose_arrays_s16_8x8(buf0 + 16, buf1 + 16);
  transpose_arrays_s16_8x8(buf0 + 24, buf1 + 24);

  for (int i = 0; i < 4; i++) {
    if (lr_flip) {
      flip_buf_8_neon(buf1 + 8 * i, buf0, 8);
      row_txfm(buf0, output + 8 * i, 32, 12);
    } else {
      int16x8_t *buf = buf1 + 8 * i;
      row_txfm(buf, output + 8 * i, 32, 12);
    }
  }
}

static void lowbd_fwd_txfm2d_16x4_neon(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16];
  int16x4_t buf1[16];
  int16x4_t buf2[16];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x4_arr[tx_type];
  const row_transform_1d_lbd_4_neon row_txfm = row_txfm4x16_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 4);
  for (int i = 0; i < 2; i++) {
    col_txfm(input + 8 * i, buf0, stride, 13);
    shift_right_1_round_s16_x8(buf0, buf0, 4);
    transpose_arrays_s16_8x4(buf0, buf1 + 8 * i);
  }

  if (lr_flip) {
    flip_buf_4_neon(buf1, buf2, 16);
    row_txfm(buf2, output, 4, 13);
  } else {
    row_txfm(buf1, output, 4, 13);
  }
}

static void lowbd_fwd_txfm2d_16x8_neon(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16], buf1[16];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x8_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_rect_txfm8x16_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 8);
  for (int i = 0; i < 2; i++) {
    col_txfm(input + 8 * i, buf0, stride, 13);
    shift_right_2_round_s16_x8(buf0, buf0, 8);
    transpose_arrays_s16_8x8(buf0, buf1 + 8 * i);
  }

  if (lr_flip) {
    flip_buf_8_neon(buf1, buf0, 16);
    row_txfm(buf0, output, 8, 13);
  } else {
    row_txfm(buf1, output, 8, 13);
  }
}

static void lowbd_fwd_txfm2d_16x16_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16], buf1[32];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x16_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_txfm8x16_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 16);
  for (int i = 0; i < 2; i++) {
    col_txfm(input + 8 * i, buf0, stride, 13);
    shift_right_2_round_s16_x8(buf0, buf0, 16);
    transpose_arrays_s16_8x8(buf0, buf1 + 0 * 16 + 8 * i);
    transpose_arrays_s16_8x8(buf0 + 8, buf1 + 1 * 16 + 8 * i);
  }

  for (int i = 0; i < 2; i++) {
    if (lr_flip) {
      flip_buf_8_neon(buf1 + 16 * i, buf0, 16);
      row_txfm(buf0, output + 8 * i, 16, 12);
    } else {
      int16x8_t *buf = buf1 + 16 * i;
      row_txfm(buf, output + 8 * i, 16, 12);
    }
  }
}

static void lowbd_fwd_txfm2d_16x32_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[64];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x32_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_rect_txfm8x16_arr[tx_type];

  if (col_txfm == NULL || row_txfm == NULL) {
    av1_fwd_txfm2d_16x32_c(input, output, stride, tx_type, bd);
    return;
  }

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 32);
  for (int i = 0; i < 2; i++) {
    col_txfm(input + 8 * i, buf0, stride, 12);
    shift_right_4_round_s16_x8(buf0, buf0, 32);
    transpose_arrays_s16_8x8(buf0 + 0 * 8, buf1 + 0 * 16 + 8 * i);
    transpose_arrays_s16_8x8(buf0 + 1 * 8, buf1 + 1 * 16 + 8 * i);
    transpose_arrays_s16_8x8(buf0 + 2 * 8, buf1 + 2 * 16 + 8 * i);
    transpose_arrays_s16_8x8(buf0 + 3 * 8, buf1 + 3 * 16 + 8 * i);
  }

  for (int i = 0; i < 4; i++) {
    if (lr_flip) {
      flip_buf_8_neon(buf1 + 16 * i, buf0, 16);
      row_txfm(buf0, output + 8 * i, 32, 13);
    } else {
      int16x8_t *buf = buf1 + 16 * i;
      row_txfm(buf, output + 8 * i, 32, 13);
    }
  }
}

static void lowbd_fwd_txfm2d_32x8_neon(const int16_t *input, int32_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[32];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x8_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_txfm8x32_arr[tx_type];

  if (col_txfm == NULL || row_txfm == NULL) {
    av1_fwd_txfm2d_32x16_c(input, output, stride, tx_type, bd);
    return;
  }

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 8);
  for (int i = 0; i < 4; i++) {
    col_txfm(input + 8 * i, buf0, stride, 13);
    shift_right_2_round_s16_x8(buf0, buf0, 8);
    transpose_arrays_s16_8x8(buf0, buf1 + 0 * 32 + 8 * i);
  }

  if (lr_flip) {
    flip_buf_8_neon(buf1, buf0, 32);
    row_txfm(buf0, output, 8, 12);
  } else {
    row_txfm(buf1, output, 8, 12);
  }
}

static void lowbd_fwd_txfm2d_32x16_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[64];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x16_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_rect_txfm8x32_arr[tx_type];

  if (col_txfm == NULL || row_txfm == NULL) {
    av1_fwd_txfm2d_32x16_c(input, output, stride, tx_type, bd);
    return;
  }

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 16);
  for (int i = 0; i < 4; i++) {
    col_txfm(input + 8 * i, buf0, stride, 13);
    shift_right_4_round_s16_x8(buf0, buf0, 16);
    transpose_arrays_s16_8x8(buf0, buf1 + 0 * 32 + 8 * i);
    transpose_arrays_s16_8x8(buf0 + 8, buf1 + 1 * 32 + 8 * i);
  }

  for (int i = 0; i < 2; i++) {
    if (lr_flip) {
      flip_buf_8_neon(buf1 + 32 * i, buf0, 32);
      row_txfm(buf0, output + 8 * i, 16, 13);
    } else {
      int16x8_t *buf = buf1 + 32 * i;
      row_txfm(buf, output + 8 * i, 16, 13);
    }
  }
}

static void lowbd_fwd_txfm2d_32x32_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[128];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x32_arr[tx_type];
  const row_transform_1d_lbd_8_neon row_txfm = row_txfm8x32_arr[tx_type];

  if (col_txfm == NULL || row_txfm == NULL) {
    av1_fwd_txfm2d_32x32_c(input, output, stride, tx_type, bd);
    return;
  }

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 32);
  for (int i = 0; i < 4; i++) {
    col_txfm(input + 8 * i, buf0, stride, 12);
    shift_right_4_round_s16_x8(buf0, buf0, 32);
    transpose_arrays_s16_8x8(buf0 + 0 * 8, buf1 + 0 * 32 + 8 * i);
    transpose_arrays_s16_8x8(buf0 + 1 * 8, buf1 + 1 * 32 + 8 * i);
    transpose_arrays_s16_8x8(buf0 + 2 * 8, buf1 + 2 * 32 + 8 * i);
    transpose_arrays_s16_8x8(buf0 + 3 * 8, buf1 + 3 * 32 + 8 * i);
  }

  for (int i = 0; i < 4; i++) {
    if (lr_flip) {
      flip_buf_8_neon(buf1 + 32 * i, buf0, 32);
      row_txfm(buf0, output + 8 * i, 32, 12);
    } else {
      int16x8_t *buf = buf1 + 32 * i;
      row_txfm(buf, output + 8 * i, 32, 12);
    }
  }
}

static void lowbd_fwd_txfm2d_64x16_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  int16x8_t buf0[64], buf1[128];
  const transform_1d_lbd_8_neon col_txfm = fdct8x16_neon;
  const transform_1d_lbd_8_neon row_txfm = fdct8x64_neon;

  for (int i = 0; i < 8; i++) {
    load_buffer_s16_x8(input + 8 * i, stride, buf0, 16);
    shift_left_2_s16_x8(buf0, buf0, 16);
    col_txfm(buf0, buf0, 13);
    shift_right_4_round_s16_x8(buf0, buf0, 16);
    for (int j = 0; j < 2; ++j) {
      transpose_arrays_s16_8x8(buf0 + j * 8, buf1 + j * 64 + 8 * i);
    }
  }

  for (int i = 0; i < 2; i++) {
    int16x8_t *buf = buf1 + 64 * i;
    row_txfm(buf, buf, 12);
    store_buffer_s16_x8(buf, output + 8 * i, 16, 32);
  }
  // Zero out the bottom 16x32 area.
  memset(output + 16 * 32, 0, 16 * 32 * sizeof(*output));
}

static void lowbd_fwd_txfm2d_16x64_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  int16x8_t buf0[64], buf1[128];
  const transform_1d_lbd_8_neon col_txfm = fdct8x64_neon;
  const transform_1d_lbd_8_neon row_txfm = fdct8x16_neon;

  for (int i = 0; i < 2; i++) {
    load_buffer_s16_x8(input + 8 * i, stride, buf0, 64);
    col_txfm(buf0, buf0, 13);
    shift_right_2_round_s16_x8(buf0, buf0, 64);
    for (int j = 0; j < 8; ++j) {
      transpose_arrays_s16_8x8(buf0 + j * 8, buf1 + j * 16 + 8 * i);
    }
  }

  for (int i = 0; i < 4; i++) {
    int16x8_t *buf = buf1 + 16 * i;
    row_txfm(buf, buf, 12);
    store_buffer_s16_x8(buf, output + 8 * i, 32, 16);
  }
}

static void fdct32_neon(const int32x4_t *input, int32x4_t *output,
                        int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);
  const int16x8_t cospi4_12 = vld1q_s16(&cospi[4 * 4]);
  const int16x8_t cospi20_28 = vld1q_s16(&cospi[4 * 6]);
  const int16x8_t cospi2_6 = vld1q_s16(&cospi[4 * 8]);
  const int16x8_t cospi10_14 = vld1q_s16(&cospi[4 * 10]);
  const int16x8_t cospi18_22 = vld1q_s16(&cospi[4 * 12]);
  const int16x8_t cospi26_30 = vld1q_s16(&cospi[4 * 14]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);
  const int16x4_t cospi4 = vget_low_s16(cospi4_12);
  const int16x4_t cospi12 = vget_high_s16(cospi4_12);
  const int16x4_t cospi20 = vget_low_s16(cospi20_28);
  const int16x4_t cospi28 = vget_high_s16(cospi20_28);
  const int16x4_t cospi2 = vget_low_s16(cospi2_6);
  const int16x4_t cospi6 = vget_high_s16(cospi2_6);
  const int16x4_t cospi10 = vget_low_s16(cospi10_14);
  const int16x4_t cospi14 = vget_high_s16(cospi10_14);
  const int16x4_t cospi18 = vget_low_s16(cospi18_22);
  const int16x4_t cospi22 = vget_high_s16(cospi18_22);
  const int16x4_t cospi26 = vget_low_s16(cospi26_30);
  const int16x4_t cospi30 = vget_high_s16(cospi26_30);

  int32x4_t buf0[32];
  int32x4_t buf1[32];

  // stage 1
  butterfly_dct_pre_s32_x4(input, buf1, 32);

  // stage 2
  butterfly_dct_pre_s32_x4(buf1, buf0, 16);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  buf0[18] = buf1[18];
  buf0[19] = buf1[19];
  butterfly_s32_s32_x4_0112_neon(cospi32, buf1[27], buf1[20], &buf0[27],
                                 &buf0[20]);
  butterfly_s32_s32_x4_0112_neon(cospi32, buf1[26], buf1[21], &buf0[26],
                                 &buf0[21]);
  butterfly_s32_s32_x4_0112_neon(cospi32, buf1[25], buf1[22], &buf0[25],
                                 &buf0[22]);
  butterfly_s32_s32_x4_0112_neon(cospi32, buf1[24], buf1[23], &buf0[24],
                                 &buf0[23]);
  buf0[28] = buf1[28];
  buf0[29] = buf1[29];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 3
  butterfly_dct_pre_s32_x4(buf0, buf1, 8);
  buf1[8] = buf0[8];
  buf1[9] = buf0[9];
  butterfly_s32_s32_x4_0112_neon(cospi32, buf0[13], buf0[10], &buf1[13],
                                 &buf1[10]);
  butterfly_s32_s32_x4_0112_neon(cospi32, buf0[12], buf0[11], &buf1[12],
                                 &buf1[11]);
  buf1[14] = buf0[14];
  buf1[15] = buf0[15];
  butterfly_dct_post_s32_x4(buf0 + 16, buf0 + 16, buf1 + 16, 16);

  // stage 4
  butterfly_dct_pre_s32_x4(buf1, buf0, 4);
  buf0[4] = buf1[4];
  butterfly_s32_s32_x4_0112_neon(cospi32, buf1[6], buf1[5], &buf0[6], &buf0[5]);
  buf0[7] = buf1[7];
  butterfly_dct_post_s32_x4(buf1 + 8, buf1 + 8, buf0 + 8, 8);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  butterfly_s32_s32_x4_0112_neon(cospi16, buf1[29], buf1[18], &buf0[29],
                                 &buf0[18]);
  butterfly_s32_s32_x4_0112_neon(cospi16, buf1[28], buf1[19], &buf0[28],
                                 &buf0[19]);
  butterfly_s32_s32_x4_1223_neon(cospi16, buf1[27], buf1[20], &buf0[27],
                                 &buf0[20]);
  butterfly_s32_s32_x4_1223_neon(cospi16, buf1[26], buf1[21], &buf0[26],
                                 &buf0[21]);
  buf0[22] = buf1[22];
  buf0[23] = buf1[23];
  buf0[24] = buf1[24];
  buf0[25] = buf1[25];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 5
  butterfly_s32_s32_x4_0112_neon(cospi32, buf0[0], buf0[1], &buf1[0], &buf1[1]);
  butterfly_s32_s32_x4_0112_neon(cospi16, buf0[3], buf0[2], &buf1[2], &buf1[3]);
  butterfly_dct_post_s32_x4(buf0 + 4, buf0 + 4, buf1 + 4, 4);
  buf1[8] = buf0[8];
  butterfly_s32_s32_x4_0112_neon(cospi16, buf0[14], buf0[9], &buf1[14],
                                 &buf1[9]);
  butterfly_s32_s32_x4_1223_neon(cospi16, buf0[13], buf0[10], &buf1[13],
                                 &buf1[10]);
  buf1[11] = buf0[11];
  buf1[12] = buf0[12];
  buf1[15] = buf0[15];
  butterfly_dct_post_s32_x4(buf0 + 16, buf0 + 16, buf1 + 16, 8);
  butterfly_dct_post_s32_x4(buf0 + 24, buf0 + 24, buf1 + 24, 8);

  // stage 6
  buf0[0] = buf1[0];
  buf0[1] = buf1[1];
  buf0[2] = buf1[2];
  buf0[3] = buf1[3];
  butterfly_s32_s32_x4_0112_neon(cospi8, buf1[7], buf1[4], &buf0[4], &buf0[7]);
  butterfly_s32_s32_x4_1003_neon(cospi24, buf1[6], buf1[5], &buf0[5], &buf0[6]);
  butterfly_dct_post_s32_x4(buf1 + 8, buf1 + 8, buf0 + 8, 4);
  butterfly_dct_post_s32_x4(buf1 + 12, buf1 + 12, buf0 + 12, 4);
  buf0[16] = buf1[16];
  butterfly_s32_s32_x4_0112_neon(cospi8, buf1[30], buf1[17], &buf0[30],
                                 &buf0[17]);
  butterfly_s32_s32_x4_1223_neon(cospi8, buf1[29], buf1[18], &buf0[29],
                                 &buf0[18]);
  buf0[19] = buf1[19];
  buf0[20] = buf1[20];
  butterfly_s32_s32_x4_1003_neon(cospi24, buf1[26], buf1[21], &buf0[26],
                                 &buf0[21]);
  butterfly_s32_s32_x4_0332_neon(cospi24, buf1[25], buf1[22], &buf0[25],
                                 &buf0[22]);
  buf0[23] = buf1[23];
  buf0[24] = buf1[24];
  buf0[27] = buf1[27];
  buf0[28] = buf1[28];
  buf0[31] = buf1[31];

  // stage 7
  buf1[0] = buf0[0];
  buf1[1] = buf0[1];
  buf1[2] = buf0[2];
  buf1[3] = buf0[3];
  buf1[4] = buf0[4];
  buf1[5] = buf0[5];
  buf1[6] = buf0[6];
  buf1[7] = buf0[7];
  butterfly_s32_s32_x4_0112_neon(cospi4, buf0[15], buf0[8], &buf1[8],
                                 &buf1[15]);
  butterfly_s32_s32_x4_1003_neon(cospi28, buf0[14], buf0[9], &buf1[9],
                                 &buf1[14]);
  butterfly_s32_s32_x4_0112_neon(cospi20, buf0[13], buf0[10], &buf1[10],
                                 &buf1[13]);
  butterfly_s32_s32_x4_1003_neon(cospi12, buf0[12], buf0[11], &buf1[11],
                                 &buf1[12]);
  butterfly_dct_post_s32_x4(buf0 + 16, buf0 + 16, buf1 + 16, 4);
  butterfly_dct_post_s32_x4(buf0 + 20, buf0 + 20, buf1 + 20, 4);
  butterfly_dct_post_s32_x4(buf0 + 24, buf0 + 24, buf1 + 24, 4);
  butterfly_dct_post_s32_x4(buf0 + 28, buf0 + 28, buf1 + 28, 4);

  // stage 8
  buf0[0] = buf1[0];
  buf0[1] = buf1[1];
  buf0[2] = buf1[2];
  buf0[3] = buf1[3];
  buf0[4] = buf1[4];
  buf0[5] = buf1[5];
  buf0[6] = buf1[6];
  buf0[7] = buf1[7];
  buf0[8] = buf1[8];
  buf0[9] = buf1[9];
  buf0[10] = buf1[10];
  buf0[11] = buf1[11];
  buf0[12] = buf1[12];
  buf0[13] = buf1[13];
  buf0[14] = buf1[14];
  buf0[15] = buf1[15];
  butterfly_s32_s32_x4_0112_neon(cospi2, buf1[31], buf1[16], &buf0[16],
                                 &buf0[31]);
  butterfly_s32_s32_x4_1003_neon(cospi30, buf1[30], buf1[17], &buf0[17],
                                 &buf0[30]);
  butterfly_s32_s32_x4_0112_neon(cospi18, buf1[29], buf1[18], &buf0[18],
                                 &buf0[29]);
  butterfly_s32_s32_x4_1003_neon(cospi14, buf1[28], buf1[19], &buf0[19],
                                 &buf0[28]);
  butterfly_s32_s32_x4_0112_neon(cospi10, buf1[27], buf1[20], &buf0[20],
                                 &buf0[27]);
  butterfly_s32_s32_x4_1003_neon(cospi22, buf1[26], buf1[21], &buf0[21],
                                 &buf0[26]);
  butterfly_s32_s32_x4_0112_neon(cospi26, buf1[25], buf1[22], &buf0[22],
                                 &buf0[25]);
  butterfly_s32_s32_x4_1003_neon(cospi6, buf1[24], buf1[23], &buf0[23],
                                 &buf0[24]);

  // stage 9
  output[0] = buf0[0];
  output[1] = buf0[16];
  output[2] = buf0[8];
  output[3] = buf0[24];
  output[4] = buf0[4];
  output[5] = buf0[20];
  output[6] = buf0[12];
  output[7] = buf0[28];
  output[8] = buf0[2];
  output[9] = buf0[18];
  output[10] = buf0[10];
  output[11] = buf0[26];
  output[12] = buf0[6];
  output[13] = buf0[22];
  output[14] = buf0[14];
  output[15] = buf0[30];
  output[16] = buf0[1];
  output[17] = buf0[17];
  output[18] = buf0[9];
  output[19] = buf0[25];
  output[20] = buf0[5];
  output[21] = buf0[21];
  output[22] = buf0[13];
  output[23] = buf0[29];
  output[24] = buf0[3];
  output[25] = buf0[19];
  output[26] = buf0[11];
  output[27] = buf0[27];
  output[28] = buf0[7];
  output[29] = buf0[23];
  output[30] = buf0[15];
  output[31] = buf0[31];
}

static void fdct64_neon(const int32x4_t *input, int32x4_t *output,
                        int cos_bit) {
  const int16_t *cospi = cospi_arr_q13(cos_bit);

  const int16x8_t cospi32_16 = vld1q_s16(&cospi[4 * 0]);
  const int16x8_t cospi8_24 = vld1q_s16(&cospi[4 * 2]);
  const int16x8_t cospi4_12 = vld1q_s16(&cospi[4 * 4]);
  const int16x8_t cospi20_28 = vld1q_s16(&cospi[4 * 6]);
  const int16x8_t cospi2_6 = vld1q_s16(&cospi[4 * 8]);
  const int16x8_t cospi10_14 = vld1q_s16(&cospi[4 * 10]);
  const int16x8_t cospi18_22 = vld1q_s16(&cospi[4 * 12]);
  const int16x8_t cospi26_30 = vld1q_s16(&cospi[4 * 14]);
  const int16x8_t cospi1_3 = vld1q_s16(&cospi[4 * 16]);
  const int16x8_t cospi5_7 = vld1q_s16(&cospi[4 * 18]);
  const int16x8_t cospi9_11 = vld1q_s16(&cospi[4 * 20]);
  const int16x8_t cospi13_15 = vld1q_s16(&cospi[4 * 22]);
  const int16x8_t cospi17_19 = vld1q_s16(&cospi[4 * 24]);
  const int16x8_t cospi21_23 = vld1q_s16(&cospi[4 * 26]);
  const int16x8_t cospi25_27 = vld1q_s16(&cospi[4 * 28]);
  const int16x8_t cospi29_31 = vld1q_s16(&cospi[4 * 30]);

  const int16x4_t cospi32 = vget_low_s16(cospi32_16);
  const int16x4_t cospi16 = vget_high_s16(cospi32_16);
  const int16x4_t cospi8 = vget_low_s16(cospi8_24);
  const int16x4_t cospi24 = vget_high_s16(cospi8_24);
  const int16x4_t cospi4 = vget_low_s16(cospi4_12);
  const int16x4_t cospi12 = vget_high_s16(cospi4_12);
  const int16x4_t cospi20 = vget_low_s16(cospi20_28);
  const int16x4_t cospi28 = vget_high_s16(cospi20_28);
  const int16x4_t cospi2 = vget_low_s16(cospi2_6);
  const int16x4_t cospi6 = vget_high_s16(cospi2_6);
  const int16x4_t cospi10 = vget_low_s16(cospi10_14);
  const int16x4_t cospi14 = vget_high_s16(cospi10_14);
  const int16x4_t cospi18 = vget_low_s16(cospi18_22);
  const int16x4_t cospi22 = vget_high_s16(cospi18_22);
  const int16x4_t cospi26 = vget_low_s16(cospi26_30);
  const int16x4_t cospi30 = vget_high_s16(cospi26_30);
  const int16x4_t cospi1 = vget_low_s16(cospi1_3);
  const int16x4_t cospi3 = vget_high_s16(cospi1_3);
  const int16x4_t cospi5 = vget_low_s16(cospi5_7);
  const int16x4_t cospi7 = vget_high_s16(cospi5_7);
  const int16x4_t cospi9 = vget_low_s16(cospi9_11);
  const int16x4_t cospi11 = vget_high_s16(cospi9_11);
  const int16x4_t cospi13 = vget_low_s16(cospi13_15);
  const int16x4_t cospi15 = vget_high_s16(cospi13_15);
  const int16x4_t cospi17 = vget_low_s16(cospi17_19);
  const int16x4_t cospi19 = vget_high_s16(cospi17_19);
  const int16x4_t cospi21 = vget_low_s16(cospi21_23);
  const int16x4_t cospi23 = vget_high_s16(cospi21_23);
  const int16x4_t cospi25 = vget_low_s16(cospi25_27);
  const int16x4_t cospi27 = vget_high_s16(cospi25_27);
  const int16x4_t cospi29 = vget_low_s16(cospi29_31);
  const int16x4_t cospi31 = vget_high_s16(cospi29_31);

  // stage 1
  int32x4_t x1[64];
  butterfly_dct_pre_s32_x4(input, x1, 64);

  // stage 2
  int32x4_t x2[64];
  butterfly_dct_pre_s32_x4(x1, x2, 32);
  butterfly_s32_s32_x4_0112_neon(cospi32, x1[55], x1[40], &x2[55], &x2[40]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x1[54], x1[41], &x2[54], &x2[41]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x1[53], x1[42], &x2[53], &x2[42]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x1[52], x1[43], &x2[52], &x2[43]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x1[51], x1[44], &x2[51], &x2[44]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x1[50], x1[45], &x2[50], &x2[45]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x1[49], x1[46], &x2[49], &x2[46]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x1[48], x1[47], &x2[48], &x2[47]);

  // stage 3
  int32x4_t x3[64];
  butterfly_dct_pre_s32_x4(x2, x3, 16);
  butterfly_s32_s32_x4_0112_neon(cospi32, x2[27], x2[20], &x3[27], &x3[20]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x2[26], x2[21], &x3[26], &x3[21]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x2[25], x2[22], &x3[25], &x3[22]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x2[24], x2[23], &x3[24], &x3[23]);
  butterfly_dct_post_s32_x4(x1 + 32, x2 + 32, x3 + 32, 32);

  // stage 4
  int32x4_t x4[64];
  butterfly_dct_pre_s32_x4(x3, x4, 8);
  butterfly_s32_s32_x4_0112_neon(cospi32, x3[13], x3[10], &x4[13], &x4[10]);
  butterfly_s32_s32_x4_0112_neon(cospi32, x3[12], x3[11], &x4[12], &x4[11]);
  butterfly_dct_post_s32_x4(x2 + 16, x3 + 16, x4 + 16, 16);
  butterfly_s32_s32_x4_0112_neon(cospi16, x3[59], x3[36], &x4[59], &x4[36]);
  butterfly_s32_s32_x4_0112_neon(cospi16, x3[58], x3[37], &x4[58], &x4[37]);
  butterfly_s32_s32_x4_0112_neon(cospi16, x3[57], x3[38], &x4[57], &x4[38]);
  butterfly_s32_s32_x4_0112_neon(cospi16, x3[56], x3[39], &x4[56], &x4[39]);
  butterfly_s32_s32_x4_1223_neon(cospi16, x3[55], x3[40], &x4[55], &x4[40]);
  butterfly_s32_s32_x4_1223_neon(cospi16, x3[54], x3[41], &x4[54], &x4[41]);
  butterfly_s32_s32_x4_1223_neon(cospi16, x3[53], x3[42], &x4[53], &x4[42]);
  butterfly_s32_s32_x4_1223_neon(cospi16, x3[52], x3[43], &x4[52], &x4[43]);

  // stage 5
  int32x4_t x5[64];
  butterfly_dct_pre_s32_x4(x4, x5, 4);
  butterfly_s32_s32_x4_0112_neon(cospi32, x4[6], x4[5], &x5[6], &x5[5]);
  butterfly_dct_post_s32_x4(x3 + 8, x4 + 8, x5 + 8, 8);
  butterfly_s32_s32_x4_0112_neon(cospi16, x4[29], x4[18], &x5[29], &x5[18]);
  butterfly_s32_s32_x4_0112_neon(cospi16, x4[28], x4[19], &x5[28], &x5[19]);
  butterfly_s32_s32_x4_1223_neon(cospi16, x4[27], x4[20], &x5[27], &x5[20]);
  butterfly_s32_s32_x4_1223_neon(cospi16, x4[26], x4[21], &x5[26], &x5[21]);
  butterfly_dct_post_s32_x4(x3 + 32, x4 + 32, x5 + 32, 16);
  butterfly_dct_post_s32_x4(x3 + 48, x4 + 48, x5 + 48, 16);

  // stage 6
  int32x4_t x6[64];
  butterfly_s32_s32_x4_0112_neon(cospi32, x5[0], x5[1], &x6[0], &x6[1]);
  butterfly_s32_s32_x4_0112_neon(cospi16, x5[3], x5[2], &x6[2], &x6[3]);
  butterfly_dct_post_s32_x4(x4 + 4, x5 + 4, x6 + 4, 4);
  butterfly_s32_s32_x4_0112_neon(cospi16, x5[14], x5[9], &x6[14], &x6[9]);
  butterfly_s32_s32_x4_1223_neon(cospi16, x5[13], x5[10], &x6[13], &x6[10]);
  butterfly_dct_post_s32_x4(x4 + 16, x5 + 16, x6 + 16, 8);
  butterfly_dct_post_s32_x4(x4 + 24, x5 + 24, x6 + 24, 8);
  butterfly_s32_s32_x4_0112_neon(cospi8, x5[61], x5[34], &x6[61], &x6[34]);
  butterfly_s32_s32_x4_0112_neon(cospi8, x5[60], x5[35], &x6[60], &x6[35]);
  butterfly_s32_s32_x4_1223_neon(cospi8, x5[59], x5[36], &x6[59], &x6[36]);
  butterfly_s32_s32_x4_1223_neon(cospi8, x5[58], x5[37], &x6[58], &x6[37]);
  butterfly_s32_s32_x4_1003_neon(cospi24, x5[53], x5[42], &x6[53], &x6[42]);
  butterfly_s32_s32_x4_1003_neon(cospi24, x5[52], x5[43], &x6[52], &x6[43]);
  butterfly_s32_s32_x4_0332_neon(cospi24, x5[51], x5[44], &x6[51], &x6[44]);
  butterfly_s32_s32_x4_0332_neon(cospi24, x5[50], x5[45], &x6[50], &x6[45]);

  // stage 7
  int32x4_t x7[64];
  butterfly_s32_s32_x4_0112_neon(cospi8, x6[7], x6[4], &x7[4], &x7[7]);
  butterfly_s32_s32_x4_1003_neon(cospi24, x6[6], x6[5], &x7[5], &x7[6]);
  butterfly_dct_post_s32_x4(x5 + 8, x6 + 8, x7 + 8, 4);
  butterfly_dct_post_s32_x4(x5 + 12, x6 + 12, x7 + 12, 4);
  butterfly_s32_s32_x4_0112_neon(cospi8, x6[30], x6[17], &x7[30], &x7[17]);
  butterfly_s32_s32_x4_1223_neon(cospi8, x6[29], x6[18], &x7[29], &x7[18]);
  butterfly_s32_s32_x4_1003_neon(cospi24, x6[26], x6[21], &x7[26], &x7[21]);
  butterfly_s32_s32_x4_0332_neon(cospi24, x6[25], x6[22], &x7[25], &x7[22]);
  butterfly_dct_post_s32_x4(x5 + 32, x6 + 32, x7 + 32, 8);
  butterfly_dct_post_s32_x4(x5 + 40, x6 + 40, x7 + 40, 8);
  butterfly_dct_post_s32_x4(x5 + 48, x6 + 48, x7 + 48, 8);
  butterfly_dct_post_s32_x4(x5 + 56, x6 + 56, x7 + 56, 8);

  // stage 8
  int32x4_t x8[64];
  butterfly_s32_s32_x4_0112_neon(cospi4, x7[15], x7[8], &x8[8], &x8[15]);
  butterfly_s32_s32_x4_1003_neon(cospi28, x7[14], x7[9], &x8[9], &x8[14]);
  butterfly_s32_s32_x4_0112_neon(cospi20, x7[13], x7[10], &x8[10], &x8[13]);
  butterfly_s32_s32_x4_1003_neon(cospi12, x7[12], x7[11], &x8[11], &x8[12]);
  butterfly_dct_post_s32_x4(x6 + 16, x7 + 16, x8 + 16, 4);
  butterfly_dct_post_s32_x4(x6 + 20, x7 + 20, x8 + 20, 4);
  butterfly_dct_post_s32_x4(x6 + 24, x7 + 24, x8 + 24, 4);
  butterfly_dct_post_s32_x4(x6 + 28, x7 + 28, x8 + 28, 4);
  butterfly_s32_s32_x4_0112_neon(cospi4, x7[62], x7[33], &x8[62], &x8[33]);
  butterfly_s32_s32_x4_1223_neon(cospi4, x7[61], x7[34], &x8[61], &x8[34]);
  butterfly_s32_s32_x4_1003_neon(cospi28, x7[58], x7[37], &x8[58], &x8[37]);
  butterfly_s32_s32_x4_0332_neon(cospi28, x7[57], x7[38], &x8[57], &x8[38]);
  butterfly_s32_s32_x4_0112_neon(cospi20, x7[54], x7[41], &x8[54], &x8[41]);
  butterfly_s32_s32_x4_1223_neon(cospi20, x7[53], x7[42], &x8[53], &x8[42]);
  butterfly_s32_s32_x4_1003_neon(cospi12, x7[50], x7[45], &x8[50], &x8[45]);
  butterfly_s32_s32_x4_0332_neon(cospi12, x7[49], x7[46], &x8[49], &x8[46]);

  // stage 9
  int32x4_t x9[64];
  butterfly_s32_s32_x4_0112_neon(cospi2, x8[31], x8[16], &x9[16], &x9[31]);
  butterfly_s32_s32_x4_1003_neon(cospi30, x8[30], x8[17], &x9[17], &x9[30]);
  butterfly_s32_s32_x4_0112_neon(cospi18, x8[29], x8[18], &x9[18], &x9[29]);
  butterfly_s32_s32_x4_1003_neon(cospi14, x8[28], x8[19], &x9[19], &x9[28]);
  butterfly_s32_s32_x4_0112_neon(cospi10, x8[27], x8[20], &x9[20], &x9[27]);
  butterfly_s32_s32_x4_1003_neon(cospi22, x8[26], x8[21], &x9[21], &x9[26]);
  butterfly_s32_s32_x4_0112_neon(cospi26, x8[25], x8[22], &x9[22], &x9[25]);
  butterfly_s32_s32_x4_1003_neon(cospi6, x8[24], x8[23], &x9[23], &x9[24]);
  butterfly_dct_post_s32_x4(x7 + 32, x8 + 32, x9 + 32, 4);
  butterfly_dct_post_s32_x4(x7 + 36, x8 + 36, x9 + 36, 4);
  butterfly_dct_post_s32_x4(x7 + 40, x8 + 40, x9 + 40, 4);
  butterfly_dct_post_s32_x4(x7 + 44, x8 + 44, x9 + 44, 4);
  butterfly_dct_post_s32_x4(x7 + 48, x8 + 48, x9 + 48, 4);
  butterfly_dct_post_s32_x4(x7 + 52, x8 + 52, x9 + 52, 4);
  butterfly_dct_post_s32_x4(x7 + 56, x8 + 56, x9 + 56, 4);
  butterfly_dct_post_s32_x4(x7 + 60, x8 + 60, x9 + 60, 4);

  // stage 10
  int32x4_t x10[64];
  butterfly_s32_s32_x4_0112_neon(cospi1, x9[63], x9[32], &x10[32], &x10[63]);
  butterfly_s32_s32_x4_1003_neon(cospi31, x9[62], x9[33], &x10[33], &x10[62]);
  butterfly_s32_s32_x4_0112_neon(cospi17, x9[61], x9[34], &x10[34], &x10[61]);
  butterfly_s32_s32_x4_1003_neon(cospi15, x9[60], x9[35], &x10[35], &x10[60]);
  butterfly_s32_s32_x4_0112_neon(cospi9, x9[59], x9[36], &x10[36], &x10[59]);
  butterfly_s32_s32_x4_1003_neon(cospi23, x9[58], x9[37], &x10[37], &x10[58]);
  butterfly_s32_s32_x4_0112_neon(cospi25, x9[57], x9[38], &x10[38], &x10[57]);
  butterfly_s32_s32_x4_1003_neon(cospi7, x9[56], x9[39], &x10[39], &x10[56]);
  butterfly_s32_s32_x4_0112_neon(cospi5, x9[55], x9[40], &x10[40], &x10[55]);
  butterfly_s32_s32_x4_1003_neon(cospi27, x9[54], x9[41], &x10[41], &x10[54]);
  butterfly_s32_s32_x4_0112_neon(cospi21, x9[53], x9[42], &x10[42], &x10[53]);
  butterfly_s32_s32_x4_1003_neon(cospi11, x9[52], x9[43], &x10[43], &x10[52]);
  butterfly_s32_s32_x4_0112_neon(cospi13, x9[51], x9[44], &x10[44], &x10[51]);
  butterfly_s32_s32_x4_1003_neon(cospi19, x9[50], x9[45], &x10[45], &x10[50]);
  butterfly_s32_s32_x4_0112_neon(cospi29, x9[49], x9[46], &x10[46], &x10[49]);
  butterfly_s32_s32_x4_1003_neon(cospi3, x9[48], x9[47], &x10[47], &x10[48]);

  // stage 11, only store into the low 32 output indices.
  output[0] = x6[0];
  output[1] = x10[32];
  output[2] = x9[16];
  output[3] = x10[48];
  output[4] = x8[8];
  output[5] = x10[40];
  output[6] = x9[24];
  output[7] = x10[56];
  output[8] = x7[4];
  output[9] = x10[36];
  output[10] = x9[20];
  output[11] = x10[52];
  output[12] = x8[12];
  output[13] = x10[44];
  output[14] = x9[28];
  output[15] = x10[60];
  output[16] = x6[2];
  output[17] = x10[34];
  output[18] = x9[18];
  output[19] = x10[50];
  output[20] = x8[10];
  output[21] = x10[42];
  output[22] = x9[26];
  output[23] = x10[58];
  output[24] = x7[6];
  output[25] = x10[38];
  output[26] = x9[22];
  output[27] = x10[54];
  output[28] = x8[14];
  output[29] = x10[46];
  output[30] = x9[30];
  output[31] = x10[62];
}

static void lowbd_fwd_txfm2d_64x64_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  int16x8_t buf0[64], buf1[512];
  const transform_1d_lbd_8_neon col_txfm = fdct8x64_neon;

  for (int i = 0; i < 8; i++) {
    load_buffer_s16_x8(input + 8 * i, stride, buf0, 64);
    col_txfm(buf0, buf0, 13);
    shift_right_2_round_s16_x8(buf0, buf0, 64);
    for (int j = 0; j < 4; ++j) {
      transpose_arrays_s16_8x8(buf0 + j * 8, buf1 + j * 64 + 8 * i);
    }
  }
  for (int i = 0; i < 4; i++) {
    int32x4_t bufA[64];
    int32x4_t bufB[64];
    int16x8_t *buf = buf1 + 64 * i;
    for (int j = 0; j < 64; ++j) {
      bufA[j] = vmovl_s16(vget_low_s16(buf[j]));
      bufB[j] = vmovl_s16(vget_high_s16(buf[j]));
    }
    fdct64_neon(bufA, bufA, 10);
    fdct64_neon(bufB, bufB, 10);
    shift_right_2_round_s32_x4(bufA, bufA, 32);
    shift_right_2_round_s32_x4(bufB, bufB, 32);
    store_buffer_interleaved_s32_x8(output + i * 8, bufA, bufB, 32, 32);
  }
}

static void lowbd_fwd_txfm2d_64x32_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[64], buf1[256];
  const col_transform_1d_lbd_8_neon col_txfm = col_txfm8x32_arr[tx_type];

  for (int i = 0; i < 8; i++) {
    col_txfm(input + 8 * i, buf0, stride, 12);
    shift_right_4_round_s16_x8(buf0, buf0, 32);
    for (int j = 0; j < 4; ++j) {
      transpose_arrays_s16_8x8(buf0 + j * 8, buf1 + j * 64 + 8 * i);
    }
  }
  assert(tx_type == DCT_DCT);
  for (int i = 0; i < 4; i++) {
    int32x4_t bufA[64];
    int32x4_t bufB[64];
    int16x8_t *buf = buf1 + 64 * i;
    for (int j = 0; j < 64; ++j) {
      bufA[j] = vmovl_s16(vget_low_s16(buf[j]));
      bufB[j] = vmovl_s16(vget_high_s16(buf[j]));
    }
    fdct64_neon(bufA, bufA, 11);
    fdct64_neon(bufB, bufB, 11);
    shift_right_2_round_s32_x4(bufA, bufA, 32);
    shift_right_2_round_s32_x4(bufB, bufB, 32);
    round_shift_sqrt2_s32_s32_4xn_neon(bufA, bufA, 32);
    round_shift_sqrt2_s32_s32_4xn_neon(bufB, bufB, 32);
    store_buffer_interleaved_s32_x8(output + i * 8, bufA, bufB, 32, 32);
  }
}

static void lowbd_fwd_txfm2d_32x64_neon(const int16_t *input, int32_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  int16x8_t buf0[64], buf1[256];
  const transform_1d_lbd_8_neon col_txfm = fdct8x64_neon;

  for (int i = 0; i < 4; i++) {
    load_buffer_s16_x8(input + 8 * i, stride, buf0, 64);
    col_txfm(buf0, buf0, 13);
    shift_right_2_round_s16_x8(buf0, buf0, 64);
    for (int j = 0; j < 4; ++j) {
      transpose_arrays_s16_8x8(buf0 + j * 8, buf1 + j * 32 + 8 * i);
    }
  }

  for (int i = 0; i < 4; i++) {
    int32x4_t bufA[32];
    int32x4_t bufB[32];
    int16x8_t *buf = buf1 + 32 * i;
    for (int j = 0; j < 32; ++j) {
      bufA[j] = vmovl_s16(vget_low_s16(buf[j]));
      bufB[j] = vmovl_s16(vget_high_s16(buf[j]));
    }
    fdct32_neon(bufA, bufA, 11);
    fdct32_neon(bufB, bufB, 11);
    shift_right_2_round_s32_x4(bufA, bufA, 32);
    shift_right_2_round_s32_x4(bufB, bufB, 32);
    round_shift_sqrt2_s32_s32_4xn_neon(bufA, bufA, 32);
    round_shift_sqrt2_s32_s32_4xn_neon(bufB, bufB, 32);
    store_buffer_interleaved_s32_x8(output + i * 8, bufA, bufB, 32, 32);
  }
}

static FwdTxfm2dFunc lowbd_fwd_txfm_func_ls[TX_SIZES_ALL] = {
  lowbd_fwd_txfm2d_4x4_neon,    // 4x4 transform
  lowbd_fwd_txfm2d_8x8_neon,    // 8x8 transform
  lowbd_fwd_txfm2d_16x16_neon,  // 16x16 transform
  lowbd_fwd_txfm2d_32x32_neon,  // 32x32 transform
  lowbd_fwd_txfm2d_64x64_neon,  // 64x64 transform
  lowbd_fwd_txfm2d_4x8_neon,    // 4x8 transform
  lowbd_fwd_txfm2d_8x4_neon,    // 8x4 transform
  lowbd_fwd_txfm2d_8x16_neon,   // 8x16 transform
  lowbd_fwd_txfm2d_16x8_neon,   // 16x8 transform
  lowbd_fwd_txfm2d_16x32_neon,  // 16x32 transform
  lowbd_fwd_txfm2d_32x16_neon,  // 32x16 transform
  lowbd_fwd_txfm2d_32x64_neon,  // 32x64 transform
  lowbd_fwd_txfm2d_64x32_neon,  // 64x32 transform
  lowbd_fwd_txfm2d_4x16_neon,   // 4x16 transform
  lowbd_fwd_txfm2d_16x4_neon,   // 16x4 transform
  lowbd_fwd_txfm2d_8x32_neon,   // 8x32 transform
  lowbd_fwd_txfm2d_32x8_neon,   // 32x8 transform
  lowbd_fwd_txfm2d_16x64_neon,  // 16x64 transform
  lowbd_fwd_txfm2d_64x16_neon,  // 64x16 transform
};

void av1_lowbd_fwd_txfm_neon(const int16_t *src_diff, tran_low_t *coeff,
                             int diff_stride, TxfmParam *txfm_param) {
  FwdTxfm2dFunc fwd_txfm2d_func = lowbd_fwd_txfm_func_ls[txfm_param->tx_size];
  if (txfm_param->lossless && txfm_param->tx_size == TX_4X4) {
    av1_lowbd_fwd_txfm_c(src_diff, coeff, diff_stride, txfm_param);
  } else {
    fwd_txfm2d_func(src_diff, coeff, diff_stride, txfm_param->tx_type,
                    txfm_param->bd);
  }
}
