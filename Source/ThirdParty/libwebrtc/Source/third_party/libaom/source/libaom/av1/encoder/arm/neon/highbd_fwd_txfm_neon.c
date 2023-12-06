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

#include "aom_dsp/arm/transpose_neon.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_txfm.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "shift_neon.h"
#include "txfm_neon.h"

static AOM_FORCE_INLINE void transpose_arrays_s32_64x64(const int32x4_t *in,
                                                        int32x4_t *out) {
  // This is not quite the same as the other transposes defined in
  // transpose_neon.h: We only write the low 64x32 sub-matrix since the rest is
  // unused by the following row transform.
  for (int j = 0; j < 8; ++j) {
    for (int i = 0; i < 16; ++i) {
      transpose_arrays_s32_4x4(in + 64 * i + 4 * j, out + 64 * j + 4 * i);
    }
  }
}

// A note on butterfly helper naming:
//
// butterfly_[weight_indices]_neon
// e.g. butterfly_0312_neon
//                ^ Weights are applied as indices 0, 3, 2, 1
//                  (see more detail below)
//
// Weight indices are treated as an index into the 4-tuple of the weight
// itself, plus related and negated constants: w=(w0, 1-w0, -w0, w0-1).
// This is then represented in the helper naming by referring to the lane index
// in the loaded tuple that each multiply is performed with:
//
//         in0   in1
//      /------------
// out0 |  w[0]  w[1]   ==>  out0 = in0 * w[0] + in1 * w[1]
// out1 |  w[2]  w[3]   ==>  out1 = in0 * w[2] + in1 * w[3]
//
// So for indices 0321 from the earlier example, we end up with:
//
//          in0       in1
//      /------------------
// out0 | (lane 0) (lane 3)   ==>  out0 = in0 *  w0 + in1 * (w0-1)
// out1 | (lane 2) (lane 1)   ==>  out1 = in0 * -w0 + in1 * (1-w0)

#define butterfly_half_neon(wvec, lane0, lane1, in0, in1, out, v_bit)   \
  do {                                                                  \
    int32x2x2_t wvecs = { { wvec, vneg_s32(wvec) } };                   \
    int32x4_t x = vmulq_lane_s32(in0, wvecs.val[lane0 / 2], lane0 % 2); \
    x = vmlaq_lane_s32(x, in1, wvecs.val[lane1 / 2], lane1 % 2);        \
    *out = vrshlq_s32(x, v_bit);                                        \
  } while (false)

static AOM_FORCE_INLINE void butterfly_0112_neon(
    const int32_t *cospi, const int widx0, const int32x4_t n0,
    const int32x4_t n1, int32x4_t *out0, int32x4_t *out1,
    const int32x4_t v_bit) {
  int32x2_t w01 = vld1_s32(cospi + 2 * widx0);
  butterfly_half_neon(w01, 0, 1, n0, n1, out0, v_bit);
  butterfly_half_neon(w01, 1, 2, n0, n1, out1, v_bit);
}

static AOM_FORCE_INLINE void butterfly_2312_neon(
    const int32_t *cospi, const int widx0, const int32x4_t n0,
    const int32x4_t n1, int32x4_t *out0, int32x4_t *out1,
    const int32x4_t v_bit) {
  int32x2_t w01 = vld1_s32(cospi + 2 * widx0);
  butterfly_half_neon(w01, 2, 3, n0, n1, out0, v_bit);
  butterfly_half_neon(w01, 1, 2, n0, n1, out1, v_bit);
}

static AOM_FORCE_INLINE void butterfly_0332_neon(
    const int32_t *cospi, const int widx0, const int32x4_t n0,
    const int32x4_t n1, int32x4_t *out0, int32x4_t *out1,
    const int32x4_t v_bit) {
  int32x2_t w01 = vld1_s32(cospi + 2 * widx0);
  butterfly_half_neon(w01, 0, 3, n0, n1, out0, v_bit);
  butterfly_half_neon(w01, 3, 2, n0, n1, out1, v_bit);
}

static AOM_FORCE_INLINE void butterfly_0130_neon(
    const int32_t *cospi, const int widx0, const int32x4_t n0,
    const int32x4_t n1, int32x4_t *out0, int32x4_t *out1,
    const int32x4_t v_bit) {
  int32x2_t w01 = vld1_s32(cospi + 2 * widx0);
  butterfly_half_neon(w01, 0, 1, n0, n1, out0, v_bit);
  butterfly_half_neon(w01, 3, 0, n0, n1, out1, v_bit);
}

static AOM_FORCE_INLINE void butterfly_cospi32_0002_neon(
    const int32_t *cospi, const int32x4_t n0, const int32x4_t n1,
    int32x4_t *out0, int32x4_t *out1, const int32x4_t v_bit) {
  int32x2_t w01 = vld1_s32(cospi + 2 * 32);
  butterfly_half_neon(w01, 0, 0, n0, n1, out0, v_bit);
  butterfly_half_neon(w01, 0, 2, n0, n1, out1, v_bit);
}

static AOM_FORCE_INLINE void butterfly_cospi32_0222_neon(
    const int32_t *cospi, const int32x4_t n0, const int32x4_t n1,
    int32x4_t *out0, int32x4_t *out1, const int32x4_t v_bit) {
  int32x2_t w01 = vld1_s32(cospi + 2 * 32);
  butterfly_half_neon(w01, 0, 2, n0, n1, out0, v_bit);
  butterfly_half_neon(w01, 2, 2, n0, n1, out1, v_bit);
}

static AOM_FORCE_INLINE void round_rect_array_s32_neon(const int32x4_t *input,
                                                       int32x4_t *output,
                                                       const int size) {
  const int32x4_t sqrt2 = vdupq_n_s32(NewSqrt2);
  int i = 0;
  do {
    const int32x4_t r1 = vmulq_s32(input[i], sqrt2);
    output[i] = vrshrq_n_s32(r1, NewSqrt2Bits);
  } while (++i < size);
}

static AOM_FORCE_INLINE void round_shift2_rect_array_s32_neon(
    const int32x4_t *input, int32x4_t *output, const int size) {
  const int32x4_t sqrt2 = vdupq_n_s32(NewSqrt2);
  int i = 0;
  do {
    const int32x4_t r0 = vrshrq_n_s32(input[i], 2);
    const int32x4_t r1 = vmulq_s32(r0, sqrt2);
    output[i] = vrshrq_n_s32(r1, NewSqrt2Bits);
  } while (++i < size);
}

#define LOAD_BUFFER_4XH(h)                                           \
  static AOM_FORCE_INLINE void load_buffer_4x##h(                    \
      const int16_t *input, int32x4_t *in, int stride, int fliplr) { \
    if (fliplr) {                                                    \
      for (int i = 0; i < (h); ++i) {                                \
        int16x4_t a = vld1_s16(input + i * stride);                  \
        a = vrev64_s16(a);                                           \
        in[i] = vshll_n_s16(a, 2);                                   \
      }                                                              \
    } else {                                                         \
      for (int i = 0; i < (h); ++i) {                                \
        int16x4_t a = vld1_s16(input + i * stride);                  \
        in[i] = vshll_n_s16(a, 2);                                   \
      }                                                              \
    }                                                                \
  }

// AArch32 does not permit the argument to vshll_n_s16 to be zero, so need to
// avoid the expression even though the compiler can prove that the code path
// is never taken if `shift == 0`.
#define shift_left_long_s16(a, shift) \
  ((shift) == 0 ? vmovl_s16(a) : vshll_n_s16((a), (shift) == 0 ? 1 : (shift)))

#define LOAD_BUFFER_WXH(w, h, shift)                                 \
  static AOM_FORCE_INLINE void load_buffer_##w##x##h(                \
      const int16_t *input, int32x4_t *in, int stride, int fliplr) { \
    assert(w >= 8);                                                  \
    if (fliplr) {                                                    \
      for (int i = 0; i < (h); ++i) {                                \
        for (int j = 0; j < (w) / 8; ++j) {                          \
          int16x8_t a = vld1q_s16(input + i * stride + j * 8);       \
          a = vrev64q_s16(a);                                        \
          int j2 = (w) / 8 - j - 1;                                  \
          in[i + (h) * (2 * j2 + 0)] =                               \
              shift_left_long_s16(vget_high_s16(a), (shift));        \
          in[i + (h) * (2 * j2 + 1)] =                               \
              shift_left_long_s16(vget_low_s16(a), (shift));         \
        }                                                            \
      }                                                              \
    } else {                                                         \
      for (int i = 0; i < (h); ++i) {                                \
        for (int j = 0; j < (w) / 8; ++j) {                          \
          int16x8_t a = vld1q_s16(input + i * stride + j * 8);       \
          in[i + (h) * (2 * j + 0)] =                                \
              shift_left_long_s16(vget_low_s16(a), (shift));         \
          in[i + (h) * (2 * j + 1)] =                                \
              shift_left_long_s16(vget_high_s16(a), (shift));        \
        }                                                            \
      }                                                              \
    }                                                                \
  }

LOAD_BUFFER_4XH(4)
LOAD_BUFFER_4XH(8)
LOAD_BUFFER_4XH(16)
LOAD_BUFFER_4XH(32)
LOAD_BUFFER_WXH(8, 8, 2)
LOAD_BUFFER_WXH(16, 16, 2)
LOAD_BUFFER_WXH(32, 64, 0)
LOAD_BUFFER_WXH(64, 32, 2)
LOAD_BUFFER_WXH(64, 64, 0)

#if !CONFIG_REALTIME_ONLY
LOAD_BUFFER_WXH(16, 64, 0)
LOAD_BUFFER_WXH(64, 16, 2)
#endif  // !CONFIG_REALTIME_ONLY

#define STORE_BUFFER_WXH(w, h)                                \
  static AOM_FORCE_INLINE void store_buffer_##w##x##h(        \
      const int32x4_t *in, int32_t *out, int stride) {        \
    for (int i = 0; i < (w); ++i) {                           \
      for (int j = 0; j < (h) / 4; ++j) {                     \
        vst1q_s32(&out[i * stride + j * 4], in[i + j * (w)]); \
      }                                                       \
    }                                                         \
  }

STORE_BUFFER_WXH(4, 4)
STORE_BUFFER_WXH(8, 4)
STORE_BUFFER_WXH(8, 8)
STORE_BUFFER_WXH(16, 4)
STORE_BUFFER_WXH(16, 16)
STORE_BUFFER_WXH(32, 4)
STORE_BUFFER_WXH(32, 32)
STORE_BUFFER_WXH(64, 32)

#if !CONFIG_REALTIME_ONLY
STORE_BUFFER_WXH(16, 32)
STORE_BUFFER_WXH(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

static AOM_FORCE_INLINE void highbd_fdct4_x4_neon(const int32x4_t *in,
                                                  int32x4_t *out, int bit) {
  const int32_t *const cospi = cospi_arr_s32(bit);
  const int32x4_t cospi32 = vdupq_n_s32(cospi[2 * 32]);
  const int32x2_t cospi16_48 = vld1_s32(&cospi[2 * 16]);

  const int32x4_t a0 = vaddq_s32(in[0], in[3]);
  const int32x4_t a1 = vsubq_s32(in[0], in[3]);
  const int32x4_t a2 = vaddq_s32(in[1], in[2]);
  const int32x4_t a3 = vsubq_s32(in[1], in[2]);

  const int32x4_t b0 = vmulq_s32(a0, cospi32);
  const int32x4_t b1 = vmulq_lane_s32(a1, cospi16_48, 1);
  const int32x4_t b2 = vmulq_s32(a2, cospi32);
  const int32x4_t b3 = vmulq_lane_s32(a3, cospi16_48, 1);

  const int32x4_t c0 = vaddq_s32(b0, b2);
  const int32x4_t c1 = vsubq_s32(b0, b2);
  const int32x4_t c2 = vmlaq_lane_s32(b3, a1, cospi16_48, 0);
  const int32x4_t c3 = vmlsq_lane_s32(b1, a3, cospi16_48, 0);

  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t d0 = vrshlq_s32(c0, v_bit);
  const int32x4_t d1 = vrshlq_s32(c1, v_bit);
  const int32x4_t d2 = vrshlq_s32(c2, v_bit);
  const int32x4_t d3 = vrshlq_s32(c3, v_bit);

  out[0] = d0;
  out[1] = d2;
  out[2] = d1;
  out[3] = d3;
}

static AOM_FORCE_INLINE void highbd_fadst4_x4_neon(const int32x4_t *in,
                                                   int32x4_t *out, int bit) {
  const int32x4_t sinpi = vld1q_s32(sinpi_arr(bit) + 1);

  const int32x4_t a0 = vaddq_s32(in[0], in[1]);
  const int32x4_t a1 = vmulq_lane_s32(in[0], vget_low_s32(sinpi), 0);
  const int32x4_t a2 = vmulq_lane_s32(in[0], vget_high_s32(sinpi), 1);
  const int32x4_t a3 = vmulq_lane_s32(in[2], vget_high_s32(sinpi), 0);

  const int32x4_t b0 = vmlaq_lane_s32(a1, in[1], vget_low_s32(sinpi), 1);
  const int32x4_t b1 = vmlsq_lane_s32(a2, in[1], vget_low_s32(sinpi), 0);
  const int32x4_t b2 = vsubq_s32(a0, in[3]);

  const int32x4_t c0 = vmlaq_lane_s32(b0, in[3], vget_high_s32(sinpi), 1);
  const int32x4_t c1 = vmlaq_lane_s32(b1, in[3], vget_low_s32(sinpi), 1);
  const int32x4_t c2 = vmulq_lane_s32(b2, vget_high_s32(sinpi), 0);

  const int32x4_t d0 = vaddq_s32(c0, a3);
  const int32x4_t d1 = vsubq_s32(c1, a3);
  const int32x4_t d2 = vsubq_s32(c1, c0);

  const int32x4_t e0 = vaddq_s32(d2, a3);

  const int32x4_t v_bit = vdupq_n_s32(-bit);
  out[0] = vrshlq_s32(d0, v_bit);
  out[1] = vrshlq_s32(c2, v_bit);
  out[2] = vrshlq_s32(d1, v_bit);
  out[3] = vrshlq_s32(e0, v_bit);
}

static AOM_FORCE_INLINE void highbd_fidentity4_x4_neon(const int32x4_t *in,
                                                       int32x4_t *out,
                                                       int bit) {
  (void)bit;
  int32x4_t fact = vdupq_n_s32(NewSqrt2);

  for (int i = 0; i < 4; i++) {
    const int32x4_t a_low = vmulq_s32(in[i], fact);
    out[i] = vrshrq_n_s32(a_low, NewSqrt2Bits);
  }
}

void av1_fwd_txfm2d_4x4_neon(const int16_t *input, int32_t *coeff,
                             int input_stride, TX_TYPE tx_type, int bd) {
  (void)bd;

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &input_stride, 4);

  // Workspace for column/row-wise transforms.
  int32x4_t buf[4];

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fdct4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fdct4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case ADST_DCT:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fdct4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case DCT_ADST:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fdct4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case ADST_ADST:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case FLIPADST_DCT:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fdct4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case DCT_FLIPADST:
      load_buffer_4x4(input, buf, input_stride, 1);
      highbd_fdct4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_4x4(input, buf, input_stride, 1);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case ADST_FLIPADST:
      load_buffer_4x4(input, buf, input_stride, 1);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case FLIPADST_ADST:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case IDTX:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fidentity4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fidentity4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case V_DCT:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fdct4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fidentity4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case H_DCT:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fidentity4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fdct4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case V_ADST:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fidentity4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case H_ADST:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fidentity4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_col[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case V_FLIPADST:
      load_buffer_4x4(input, buf, input_stride, 0);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fidentity4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    case H_FLIPADST:
      load_buffer_4x4(input, buf, input_stride, 1);
      highbd_fidentity4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      transpose_arrays_s32_4x4(buf, buf);
      highbd_fadst4_x4_neon(buf, buf, av1_fwd_cos_bit_row[0][0]);
      store_buffer_4x4(buf, coeff, /*stride=*/4);
      break;
    default: assert(0);
  }
}

// Butterfly pre-processing:
// e.g. n=4:
//   out[0] = in[0] + in[3]
//   out[1] = in[1] + in[2]
//   out[2] = in[1] - in[2]
//   out[3] = in[0] - in[3]

static AOM_FORCE_INLINE void butterfly_dct_pre(const int32x4_t *input,
                                               int32x4_t *output, int n) {
  for (int i = 0; i < n / 2; ++i) {
    output[i] = vaddq_s32(input[i], input[n - i - 1]);
  }
  for (int i = 0; i < n / 2; ++i) {
    output[n / 2 + i] = vsubq_s32(input[n / 2 - i - 1], input[n / 2 + i]);
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

static AOM_FORCE_INLINE void butterfly_dct_post(const int32x4_t *in0,
                                                const int32x4_t *in1,
                                                int32x4_t *output, int n) {
  for (int i = 0; i < n / 4; ++i) {
    output[i] = vaddq_s32(in0[i], in1[n / 2 - i - 1]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[n / 4 + i] = vsubq_s32(in0[n / 4 - i - 1], in1[n / 4 + i]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[n / 2 + i] = vsubq_s32(in0[n - i - 1], in1[n / 2 + i]);
  }
  for (int i = 0; i < n / 4; ++i) {
    output[(3 * n) / 4 + i] =
        vaddq_s32(in0[(3 * n) / 4 + i], in1[(3 * n) / 4 - i - 1]);
  }
}

static AOM_FORCE_INLINE void highbd_fdct8_x4_neon(const int32x4_t *in,
                                                  int32x4_t *out, int bit) {
  const int32_t *const cospi = cospi_arr_s32(bit);
  const int32x4_t v_bit = vdupq_n_s32(-bit);

  // stage 1
  int32x4_t a[8];
  butterfly_dct_pre(in, a, 8);

  // stage 2
  int32x4_t b[8];
  butterfly_dct_pre(a, b, 4);
  butterfly_0130_neon(cospi, 32, a[5], a[6], &b[6], &b[5], v_bit);

  // stage 3
  int32x4_t c[8];
  butterfly_0130_neon(cospi, 32, b[1], b[0], &c[0], &c[1], v_bit);
  butterfly_0112_neon(cospi, 16, b[3], b[2], &c[2], &c[3], v_bit);
  butterfly_dct_post(a + 4, b + 4, c + 4, 4);

  // stage 4-5
  butterfly_0112_neon(cospi, 8, c[7], c[4], &out[1], &out[7], v_bit);
  butterfly_0130_neon(cospi, 24, c[5], c[6], &out[5], &out[3], v_bit);

  out[0] = c[0];
  out[2] = c[2];
  out[4] = c[1];
  out[6] = c[3];
}

static AOM_FORCE_INLINE void highbd_fadst8_x4_neon(const int32x4_t *in,
                                                   int32x4_t *out, int bit) {
  const int32_t *const cospi = cospi_arr_s32(bit);
  const int32x4_t v_bit = vdupq_n_s32(-bit);

  int32x4_t u0, u1, u2, u3, u4, u5, u6, u7;
  int32x4_t v0, v1, v2, v3, v4, v5, v6, v7;

  // stage 0-1
  u0 = in[0];
  u1 = in[7];
  u2 = in[3];
  u3 = in[4];
  u4 = in[1];
  u5 = in[6];
  u6 = in[2];
  u7 = in[5];

  // stage 2
  v0 = u0;
  v1 = u1;
  butterfly_cospi32_0222_neon(cospi, u3, u2, &v2, &v3, v_bit);
  v4 = u4;
  v5 = u5;
  butterfly_cospi32_0002_neon(cospi, u6, u7, &v7, &v6, v_bit);

  // stage 3
  u0 = vaddq_s32(v0, v2);
  u1 = vsubq_s32(v3, v1);
  u2 = vsubq_s32(v0, v2);
  u3 = vaddq_s32(v1, v3);
  u4 = vsubq_s32(v6, v4);
  u5 = vaddq_s32(v5, v7);
  u6 = vaddq_s32(v4, v6);
  u7 = vsubq_s32(v5, v7);

  // stage 4
  v0 = u0;
  v1 = u1;
  v2 = u2;
  v3 = u3;

  butterfly_0112_neon(cospi, 16, u4, u5, &v4, &v5, v_bit);
  butterfly_0112_neon(cospi, 16, u7, u6, &v6, &v7, v_bit);

  // stage 5
  u0 = vaddq_s32(v0, v4);
  u1 = vaddq_s32(v1, v5);
  u2 = vaddq_s32(v2, v6);
  u3 = vsubq_s32(v7, v3);
  u4 = vsubq_s32(v0, v4);
  u5 = vsubq_s32(v1, v5);
  u6 = vsubq_s32(v2, v6);
  u7 = vaddq_s32(v3, v7);

  // stage 6
  butterfly_0112_neon(cospi, 4, u0, u1, &v0, &v1, v_bit);
  butterfly_0112_neon(cospi, 20, u2, u3, &v2, &v3, v_bit);
  butterfly_0130_neon(cospi, 28, u5, u4, &v4, &v5, v_bit);
  butterfly_0112_neon(cospi, 12, u6, u7, &v7, &v6, v_bit);

  // stage 7
  out[0] = v1;
  out[1] = v6;
  out[2] = v3;
  out[3] = v4;
  out[4] = v5;
  out[5] = v2;
  out[6] = v7;
  out[7] = v0;
}

static AOM_FORCE_INLINE void highbd_fidentity8_x4_neon(const int32x4_t *in,
                                                       int32x4_t *out,
                                                       int bit) {
  (void)bit;
  out[0] = vshlq_n_s32(in[0], 1);
  out[1] = vshlq_n_s32(in[1], 1);
  out[2] = vshlq_n_s32(in[2], 1);
  out[3] = vshlq_n_s32(in[3], 1);
  out[4] = vshlq_n_s32(in[4], 1);
  out[5] = vshlq_n_s32(in[5], 1);
  out[6] = vshlq_n_s32(in[6], 1);
  out[7] = vshlq_n_s32(in[7], 1);
}

static AOM_FORCE_INLINE void highbd_fdct8_xn_neon(const int32x4_t *in,
                                                  int32x4_t *out, int bit,
                                                  int howmany) {
  const int stride = 8;
  int i = 0;
  do {
    highbd_fdct8_x4_neon(in + i * stride, out + i * stride, bit);
  } while (++i < howmany);
}

static AOM_FORCE_INLINE void highbd_fadst8_xn_neon(const int32x4_t *in,
                                                   int32x4_t *out, int bit,
                                                   int howmany) {
  const int stride = 8;
  int i = 0;
  do {
    highbd_fadst8_x4_neon(in + i * stride, out + i * stride, bit);
  } while (++i < howmany);
}

static AOM_FORCE_INLINE void highbd_fidentity8_xn_neon(const int32x4_t *in,
                                                       int32x4_t *out, int bit,
                                                       int howmany) {
  (void)bit;
  const int stride = 8;
  int i = 0;
  do {
    highbd_fidentity8_x4_neon(in + i * stride, out + i * stride, bit);
  } while (++i < howmany);
}

void av1_fwd_txfm2d_8x8_neon(const int16_t *input, int32_t *coeff, int stride,
                             TX_TYPE tx_type, int bd) {
  (void)bd;

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 8);

  // Workspaces for column/row-wise transforms.
  int32x4_t buf0[16], buf1[16];

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fdct8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fdct8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case ADST_DCT:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fadst8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fdct8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case DCT_ADST:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fdct8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fadst8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case ADST_ADST:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fadst8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fadst8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case FLIPADST_DCT:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fadst8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fdct8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case DCT_FLIPADST:
      load_buffer_8x8(input, buf0, stride, 1);
      highbd_fdct8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fadst8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_8x8(input, buf0, stride, 1);
      highbd_fadst8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fadst8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case ADST_FLIPADST:
      load_buffer_8x8(input, buf0, stride, 1);
      highbd_fadst8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fadst8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case FLIPADST_ADST:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fadst8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fadst8_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case IDTX:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fidentity8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fidentity8_xn_neon(buf1, buf1, av1_fwd_cos_bit_col[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case V_DCT:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fdct8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fidentity8_xn_neon(buf1, buf1, av1_fwd_cos_bit_col[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case H_DCT:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fidentity8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fdct8_xn_neon(buf1, buf1, av1_fwd_cos_bit_col[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case V_ADST:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fadst8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fidentity8_xn_neon(buf1, buf1, av1_fwd_cos_bit_col[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case H_ADST:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fidentity8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fadst8_xn_neon(buf1, buf1, av1_fwd_cos_bit_col[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case V_FLIPADST:
      load_buffer_8x8(input, buf0, stride, 0);
      highbd_fadst8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fidentity8_xn_neon(buf1, buf1, av1_fwd_cos_bit_col[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    case H_FLIPADST:
      load_buffer_8x8(input, buf0, stride, 1);
      highbd_fidentity8_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[1][1], 2);
      shift_right_1_round_s32_x4(buf0, buf0, 16);
      transpose_arrays_s32_8x8(buf0, buf1);
      highbd_fadst8_xn_neon(buf1, buf1, av1_fwd_cos_bit_col[1][1], 2);
      store_buffer_8x8(buf1, coeff, /*stride=*/8);
      break;
    default: assert(0);
  }
}

static void highbd_fdct16_x4_neon(const int32x4_t *in, int32x4_t *out,
                                  int bit) {
  const int32_t *const cospi = cospi_arr_s32(bit);
  const int32x4_t v_bit = vdupq_n_s32(-bit);

  int32x4_t u[16], v[16];

  // stage 1
  butterfly_dct_pre(in, u, 16);

  // stage 2
  butterfly_dct_pre(u, v, 8);
  v[8] = u[8];
  v[9] = u[9];
  butterfly_cospi32_0002_neon(cospi, u[13], u[10], &v[13], &v[10], v_bit);
  butterfly_cospi32_0002_neon(cospi, u[12], u[11], &v[12], &v[11], v_bit);
  v[14] = u[14];
  v[15] = u[15];

  // stage 3
  butterfly_dct_pre(v, u, 4);
  u[4] = v[4];
  butterfly_cospi32_0002_neon(cospi, v[6], v[5], &u[6], &u[5], v_bit);
  u[7] = v[7];
  butterfly_dct_post(v + 8, v + 8, u + 8, 8);

  // stage 4
  butterfly_cospi32_0002_neon(cospi, u[0], u[1], &v[0], &v[1], v_bit);
  butterfly_0112_neon(cospi, 16, u[3], u[2], &v[2], &v[3], v_bit);
  butterfly_dct_post(u + 4, u + 4, v + 4, 4);
  v[8] = u[8];
  butterfly_0112_neon(cospi, 16, u[14], u[9], &v[14], &v[9], v_bit);
  butterfly_2312_neon(cospi, 16, u[13], u[10], &v[10], &v[13], v_bit);
  v[11] = u[11];
  v[12] = u[12];
  v[15] = u[15];

  // stage 5
  u[0] = v[0];
  u[1] = v[1];
  u[2] = v[2];
  u[3] = v[3];
  butterfly_0112_neon(cospi, 8, v[7], v[4], &u[4], &u[7], v_bit);
  butterfly_0130_neon(cospi, 24, v[5], v[6], &u[5], &u[6], v_bit);
  butterfly_dct_post(v + 8, v + 8, u + 8, 4);
  butterfly_dct_post(v + 12, v + 12, u + 12, 4);

  // stage 6
  v[0] = u[0];
  v[1] = u[1];
  v[2] = u[2];
  v[3] = u[3];
  v[4] = u[4];
  v[5] = u[5];
  v[6] = u[6];
  v[7] = u[7];
  butterfly_0112_neon(cospi, 4, u[15], u[8], &v[8], &v[15], v_bit);
  butterfly_0130_neon(cospi, 28, u[9], u[14], &v[9], &v[14], v_bit);
  butterfly_0112_neon(cospi, 20, u[13], u[10], &v[10], &v[13], v_bit);
  butterfly_0130_neon(cospi, 12, u[11], u[12], &v[11], &v[12], v_bit);

  out[0] = v[0];
  out[1] = v[8];
  out[2] = v[4];
  out[3] = v[12];
  out[4] = v[2];
  out[5] = v[10];
  out[6] = v[6];
  out[7] = v[14];
  out[8] = v[1];
  out[9] = v[9];
  out[10] = v[5];
  out[11] = v[13];
  out[12] = v[3];
  out[13] = v[11];
  out[14] = v[7];
  out[15] = v[15];
}

static void highbd_fadst16_x4_neon(const int32x4_t *in, int32x4_t *out,
                                   int bit) {
  const int32_t *const cospi = cospi_arr_s32(bit);
  const int32x4_t v_bit = vdupq_n_s32(-bit);

  int32x4_t u[16], v[16];

  // stage 0-1
  u[0] = in[0];
  u[1] = in[15];
  u[2] = in[7];
  u[3] = in[8];
  u[4] = in[3];
  u[5] = in[12];
  u[6] = in[4];
  u[7] = in[11];
  u[8] = in[1];
  u[9] = in[14];
  u[10] = in[6];
  u[11] = in[9];
  u[12] = in[2];
  u[13] = in[13];
  u[14] = in[5];
  u[15] = in[10];

  // stage 2
  v[0] = u[0];
  v[1] = u[1];
  butterfly_cospi32_0222_neon(cospi, u[3], u[2], &v[2], &v[3], v_bit);
  v[4] = u[4];
  v[5] = u[5];
  butterfly_cospi32_0002_neon(cospi, u[6], u[7], &v[7], &v[6], v_bit);
  v[8] = u[8];
  v[9] = u[9];
  butterfly_cospi32_0002_neon(cospi, u[10], u[11], &v[11], &v[10], v_bit);
  v[12] = u[12];
  v[13] = u[13];
  butterfly_cospi32_0222_neon(cospi, u[15], u[14], &v[14], &v[15], v_bit);

  // stage 3
  u[0] = vaddq_s32(v[0], v[2]);
  u[1] = vsubq_s32(v[3], v[1]);
  u[2] = vsubq_s32(v[0], v[2]);
  u[3] = vaddq_s32(v[1], v[3]);
  u[4] = vsubq_s32(v[6], v[4]);
  u[5] = vaddq_s32(v[5], v[7]);
  u[6] = vaddq_s32(v[4], v[6]);
  u[7] = vsubq_s32(v[5], v[7]);
  u[8] = vsubq_s32(v[10], v[8]);
  u[9] = vaddq_s32(v[9], v[11]);
  u[10] = vaddq_s32(v[8], v[10]);
  u[11] = vsubq_s32(v[9], v[11]);
  u[12] = vaddq_s32(v[12], v[14]);
  u[13] = vsubq_s32(v[15], v[13]);
  u[14] = vsubq_s32(v[12], v[14]);
  u[15] = vaddq_s32(v[13], v[15]);

  // stage 4
  v[0] = u[0];
  v[1] = u[1];
  v[2] = u[2];
  v[3] = u[3];
  butterfly_0112_neon(cospi, 16, u[4], u[5], &v[4], &v[5], v_bit);
  butterfly_0112_neon(cospi, 16, u[7], u[6], &v[6], &v[7], v_bit);

  v[8] = u[8];
  v[9] = u[9];
  v[10] = u[10];
  v[11] = u[11];

  butterfly_0112_neon(cospi, 16, u[12], u[13], &v[12], &v[13], v_bit);
  butterfly_0332_neon(cospi, 16, u[14], u[15], &v[15], &v[14], v_bit);

  // stage 5
  u[0] = vaddq_s32(v[0], v[4]);
  u[1] = vaddq_s32(v[1], v[5]);
  u[2] = vaddq_s32(v[2], v[6]);
  u[3] = vsubq_s32(v[7], v[3]);
  u[4] = vsubq_s32(v[0], v[4]);
  u[5] = vsubq_s32(v[1], v[5]);
  u[6] = vsubq_s32(v[2], v[6]);
  u[7] = vaddq_s32(v[3], v[7]);
  u[8] = vaddq_s32(v[8], v[12]);
  u[9] = vaddq_s32(v[9], v[13]);
  u[10] = vsubq_s32(v[14], v[10]);
  u[11] = vaddq_s32(v[11], v[15]);
  u[12] = vsubq_s32(v[8], v[12]);
  u[13] = vsubq_s32(v[9], v[13]);
  u[14] = vaddq_s32(v[10], v[14]);
  u[15] = vsubq_s32(v[11], v[15]);

  // stage 6
  v[0] = u[0];
  v[1] = u[1];
  v[2] = u[2];
  v[3] = u[3];
  v[4] = u[4];
  v[5] = u[5];
  v[6] = u[6];
  v[7] = u[7];

  butterfly_0112_neon(cospi, 8, u[8], u[9], &v[8], &v[9], v_bit);
  butterfly_0130_neon(cospi, 8, u[12], u[13], &v[13], &v[12], v_bit);
  butterfly_0130_neon(cospi, 24, u[11], u[10], &v[10], &v[11], v_bit);
  butterfly_0130_neon(cospi, 24, u[14], u[15], &v[14], &v[15], v_bit);

  // stage 7
  u[0] = vaddq_s32(v[0], v[8]);
  u[1] = vaddq_s32(v[1], v[9]);
  u[2] = vaddq_s32(v[2], v[10]);
  u[3] = vaddq_s32(v[3], v[11]);
  u[4] = vaddq_s32(v[4], v[12]);
  u[5] = vaddq_s32(v[5], v[13]);
  u[6] = vaddq_s32(v[6], v[14]);
  u[7] = vsubq_s32(v[15], v[7]);
  u[8] = vsubq_s32(v[0], v[8]);
  u[9] = vsubq_s32(v[1], v[9]);
  u[10] = vsubq_s32(v[2], v[10]);
  u[11] = vsubq_s32(v[3], v[11]);
  u[12] = vsubq_s32(v[4], v[12]);
  u[13] = vsubq_s32(v[5], v[13]);
  u[14] = vsubq_s32(v[6], v[14]);
  u[15] = vaddq_s32(v[7], v[15]);

  // stage 8
  butterfly_0112_neon(cospi, 2, u[0], u[1], &v[0], &v[1], v_bit);
  butterfly_0112_neon(cospi, 10, u[2], u[3], &v[2], &v[3], v_bit);
  butterfly_0112_neon(cospi, 18, u[4], u[5], &v[4], &v[5], v_bit);
  butterfly_0112_neon(cospi, 26, u[6], u[7], &v[6], &v[7], v_bit);
  butterfly_0130_neon(cospi, 30, u[9], u[8], &v[8], &v[9], v_bit);
  butterfly_0130_neon(cospi, 22, u[11], u[10], &v[10], &v[11], v_bit);
  butterfly_0130_neon(cospi, 14, u[13], u[12], &v[12], &v[13], v_bit);
  butterfly_0112_neon(cospi, 6, u[14], u[15], &v[15], &v[14], v_bit);

  // stage 9
  out[0] = v[1];
  out[1] = v[14];
  out[2] = v[3];
  out[3] = v[12];
  out[4] = v[5];
  out[5] = v[10];
  out[6] = v[7];
  out[7] = v[8];
  out[8] = v[9];
  out[9] = v[6];
  out[10] = v[11];
  out[11] = v[4];
  out[12] = v[13];
  out[13] = v[2];
  out[14] = v[15];
  out[15] = v[0];
}

static void highbd_fidentity16_x4_neon(const int32x4_t *in, int32x4_t *out,
                                       int bit) {
  (void)bit;
  const int32x4_t fact = vdupq_n_s32(2 * NewSqrt2);
  const int32x4_t offset = vdupq_n_s32(1 << (NewSqrt2Bits - 1));

  for (int i = 0; i < 16; i++) {
    int32x4_t a = vmulq_s32(in[i], fact);
    a = vaddq_s32(a, offset);
    out[i] = vshrq_n_s32(a, NewSqrt2Bits);
  }
}

static void highbd_fdct16_xn_neon(const int32x4_t *in, int32x4_t *out, int bit,
                                  const int howmany) {
  const int stride = 16;
  int i = 0;
  do {
    highbd_fdct16_x4_neon(in + i * stride, out + i * stride, bit);
  } while (++i < howmany);
}

static void highbd_fadst16_xn_neon(const int32x4_t *in, int32x4_t *out, int bit,
                                   int howmany) {
  const int stride = 16;
  int i = 0;
  do {
    highbd_fadst16_x4_neon(in + i * stride, out + i * stride, bit);
  } while (++i < howmany);
}

static void highbd_fidentity16_xn_neon(const int32x4_t *in, int32x4_t *out,
                                       int bit, int howmany) {
  const int stride = 16;
  int i = 0;
  do {
    highbd_fidentity16_x4_neon(in + i * stride, out + i * stride, bit);
  } while (++i < howmany);
}

void av1_fwd_txfm2d_16x16_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)bd;
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 16);

  // Workspaces for column/row-wise transforms.
  int32x4_t buf0[64], buf1[64];

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fdct16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fdct16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case ADST_DCT:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fadst16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fdct16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case DCT_ADST:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fdct16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fadst16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case ADST_ADST:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fadst16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fadst16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case FLIPADST_DCT:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fadst16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fdct16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case DCT_FLIPADST:
      load_buffer_16x16(input, buf0, stride, 1);
      highbd_fdct16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fadst16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_16x16(input, buf0, stride, 1);
      highbd_fadst16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fadst16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case ADST_FLIPADST:
      load_buffer_16x16(input, buf0, stride, 1);
      highbd_fadst16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fadst16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case FLIPADST_ADST:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fadst16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fadst16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case IDTX:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fidentity16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fidentity16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case V_DCT:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fdct16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fidentity16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case H_DCT:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fidentity16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fdct16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case V_ADST:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fadst16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fidentity16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case H_ADST:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fidentity16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fadst16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case V_FLIPADST:
      load_buffer_16x16(input, buf0, stride, 0);
      highbd_fadst16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fidentity16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    case H_FLIPADST:
      load_buffer_16x16(input, buf0, stride, 1);
      highbd_fidentity16_xn_neon(buf0, buf0, av1_fwd_cos_bit_col[2][2], 4);
      shift_right_2_round_s32_x4(buf0, buf0, 64);
      transpose_arrays_s32_16x16(buf0, buf1);
      highbd_fadst16_xn_neon(buf1, buf1, av1_fwd_cos_bit_row[2][2], 4);
      store_buffer_16x16(buf1, coeff, /*stride=*/16);
      break;
    default: assert(0);
  }
}

typedef void (*fwd_transform_1d_col_neon)(const int16_t *in, int32x4_t *out,
                                          int stride, int bit, int lr_flip);
typedef void (*fwd_transform_1d_col_many_neon)(const int16_t *in,
                                               int32x4_t *out, int stride,
                                               int bit, int lr_flip,
                                               int howmany, int hm_stride);

typedef void (*fwd_transform_1d_row_neon)(const int32x4_t *in, int32_t *out,
                                          int bit, int stride);
typedef void (*fwd_transform_1d_row_many_neon)(const int32x4_t *in,
                                               int32_t *out, int bit,
                                               int howmany, int hm_stride,
                                               int stride);

// Construct component kernels that include the load_buffer and store_buffer
// stages to avoid the need to spill loaded data to the stack between these and
// the txfm kernel calls.
// The TRANSFORM_*_ONE cases are only ever called in situations where the
// howmany parameter would be one, so no need for the loop at all in these
// cases.

#define TRANSFORM_COL_ONE(name, n)                                    \
  static void highbd_##name##_col_neon(const int16_t *input,          \
                                       int32x4_t *output, int stride, \
                                       int cos_bit, int lr_flip) {    \
    int32x4_t buf0[n];                                                \
    load_buffer_4x##n(input, buf0, stride, lr_flip);                  \
    highbd_##name##_x4_neon(buf0, output, cos_bit);                   \
  }

#define TRANSFORM_COL_MANY(name, n)                                     \
  static void highbd_##name##_col_many_neon(                            \
      const int16_t *input, int32x4_t *output, int stride, int cos_bit, \
      int lr_flip, int howmany, int hm_stride) {                        \
    int i = 0;                                                          \
    do {                                                                \
      int32x4_t buf0[n];                                                \
      load_buffer_4x##n(input + 4 * i, buf0, stride, lr_flip);          \
      highbd_##name##_x4_neon(buf0, output + i * hm_stride, cos_bit);   \
    } while (++i < howmany);                                            \
  }

#define TRANSFORM_ROW_ONE(name, n)                                        \
  static void highbd_##name##_row_neon(                                   \
      const int32x4_t *input, int32_t *output, int cos_bit, int stride) { \
    int32x4_t buf0[n];                                                    \
    highbd_##name##_x4_neon(input, buf0, cos_bit);                        \
    store_buffer_##n##x4(buf0, output, stride);                           \
  }

#define TRANSFORM_ROW_RECT_ONE(name, n)                                   \
  static void highbd_##name##_row_rect_neon(                              \
      const int32x4_t *input, int32_t *output, int cos_bit, int stride) { \
    int32x4_t buf0[n];                                                    \
    highbd_##name##_x4_neon(input, buf0, cos_bit);                        \
    round_rect_array_s32_neon(buf0, buf0, (n));                           \
    store_buffer_##n##x4(buf0, output, stride);                           \
  }

#define TRANSFORM_ROW_MANY(name, n)                                      \
  static void highbd_##name##_row_many_neon(                             \
      const int32x4_t *input, int32_t *output, int cos_bit, int howmany, \
      int hm_stride, int stride) {                                       \
    int i = 0;                                                           \
    do {                                                                 \
      int32x4_t buf0[n];                                                 \
      highbd_##name##_x4_neon(input + hm_stride * i, buf0, cos_bit);     \
      store_buffer_##n##x4(buf0, output + 4 * i, stride);                \
    } while (++i < howmany);                                             \
  }

#define TRANSFORM_ROW_RECT_MANY(name, n)                                 \
  static void highbd_##name##_row_rect_many_neon(                        \
      const int32x4_t *input, int32_t *output, int cos_bit, int howmany, \
      int hm_stride, int stride) {                                       \
    int i = 0;                                                           \
    do {                                                                 \
      int32x4_t buf0[n];                                                 \
      highbd_##name##_x4_neon(input + hm_stride * i, buf0, cos_bit);     \
      round_rect_array_s32_neon(buf0, buf0, (n));                        \
      store_buffer_##n##x4(buf0, output + 4 * i, stride);                \
    } while (++i < howmany);                                             \
  }

TRANSFORM_COL_ONE(fdct8, 8)
TRANSFORM_COL_ONE(fadst8, 8)
TRANSFORM_COL_ONE(fidentity8, 8)

TRANSFORM_COL_MANY(fdct4, 4)
TRANSFORM_COL_MANY(fdct8, 8)
TRANSFORM_COL_MANY(fdct16, 16)
TRANSFORM_COL_MANY(fadst4, 4)
TRANSFORM_COL_MANY(fadst8, 8)
TRANSFORM_COL_MANY(fadst16, 16)
TRANSFORM_COL_MANY(fidentity4, 4)
TRANSFORM_COL_MANY(fidentity8, 8)
TRANSFORM_COL_MANY(fidentity16, 16)

TRANSFORM_ROW_ONE(fdct16, 16)
TRANSFORM_ROW_ONE(fadst16, 16)
TRANSFORM_ROW_ONE(fidentity16, 16)

TRANSFORM_ROW_RECT_ONE(fdct8, 8)
TRANSFORM_ROW_RECT_ONE(fadst8, 8)
TRANSFORM_ROW_RECT_ONE(fidentity8, 8)

#if !CONFIG_REALTIME_ONLY
TRANSFORM_ROW_MANY(fdct4, 4)
TRANSFORM_ROW_MANY(fdct8, 8)
TRANSFORM_ROW_MANY(fadst4, 4)
TRANSFORM_ROW_MANY(fadst8, 8)
TRANSFORM_ROW_MANY(fidentity4, 4)
TRANSFORM_ROW_MANY(fidentity8, 8)
#endif

TRANSFORM_ROW_RECT_MANY(fdct4, 4)
TRANSFORM_ROW_RECT_MANY(fdct8, 8)
TRANSFORM_ROW_RECT_MANY(fdct16, 16)
TRANSFORM_ROW_RECT_MANY(fadst4, 4)
TRANSFORM_ROW_RECT_MANY(fadst8, 8)
TRANSFORM_ROW_RECT_MANY(fadst16, 16)
TRANSFORM_ROW_RECT_MANY(fidentity4, 4)
TRANSFORM_ROW_RECT_MANY(fidentity8, 8)
TRANSFORM_ROW_RECT_MANY(fidentity16, 16)

static const fwd_transform_1d_col_many_neon
    col_highbd_txfm8_xn_arr[TX_TYPES] = {
      highbd_fdct8_col_many_neon,       // DCT_DCT
      highbd_fadst8_col_many_neon,      // ADST_DCT
      highbd_fdct8_col_many_neon,       // DCT_ADST
      highbd_fadst8_col_many_neon,      // ADST_ADST
      highbd_fadst8_col_many_neon,      // FLIPADST_DCT
      highbd_fdct8_col_many_neon,       // DCT_FLIPADST
      highbd_fadst8_col_many_neon,      // FLIPADST_FLIPADST
      highbd_fadst8_col_many_neon,      // ADST_FLIPADST
      highbd_fadst8_col_many_neon,      // FLIPADST_ADST
      highbd_fidentity8_col_many_neon,  // IDTX
      highbd_fdct8_col_many_neon,       // V_DCT
      highbd_fidentity8_col_many_neon,  // H_DCT
      highbd_fadst8_col_many_neon,      // V_ADST
      highbd_fidentity8_col_many_neon,  // H_ADST
      highbd_fadst8_col_many_neon,      // V_FLIPADST
      highbd_fidentity8_col_many_neon   // H_FLIPADST
    };

static const fwd_transform_1d_col_neon col_highbd_txfm8_x4_arr[TX_TYPES] = {
  highbd_fdct8_col_neon,       // DCT_DCT
  highbd_fadst8_col_neon,      // ADST_DCT
  highbd_fdct8_col_neon,       // DCT_ADST
  highbd_fadst8_col_neon,      // ADST_ADST
  highbd_fadst8_col_neon,      // FLIPADST_DCT
  highbd_fdct8_col_neon,       // DCT_FLIPADST
  highbd_fadst8_col_neon,      // FLIPADST_FLIPADST
  highbd_fadst8_col_neon,      // ADST_FLIPADST
  highbd_fadst8_col_neon,      // FLIPADST_ADST
  highbd_fidentity8_col_neon,  // IDTX
  highbd_fdct8_col_neon,       // V_DCT
  highbd_fidentity8_col_neon,  // H_DCT
  highbd_fadst8_col_neon,      // V_ADST
  highbd_fidentity8_col_neon,  // H_ADST
  highbd_fadst8_col_neon,      // V_FLIPADST
  highbd_fidentity8_col_neon   // H_FLIPADST
};

static const fwd_transform_1d_col_many_neon
    col_highbd_txfm16_xn_arr[TX_TYPES] = {
      highbd_fdct16_col_many_neon,       // DCT_DCT
      highbd_fadst16_col_many_neon,      // ADST_DCT
      highbd_fdct16_col_many_neon,       // DCT_ADST
      highbd_fadst16_col_many_neon,      // ADST_ADST
      highbd_fadst16_col_many_neon,      // FLIPADST_DCT
      highbd_fdct16_col_many_neon,       // DCT_FLIPADST
      highbd_fadst16_col_many_neon,      // FLIPADST_FLIPADST
      highbd_fadst16_col_many_neon,      // ADST_FLIPADST
      highbd_fadst16_col_many_neon,      // FLIPADST_ADST
      highbd_fidentity16_col_many_neon,  // IDTX
      highbd_fdct16_col_many_neon,       // V_DCT
      highbd_fidentity16_col_many_neon,  // H_DCT
      highbd_fadst16_col_many_neon,      // V_ADST
      highbd_fidentity16_col_many_neon,  // H_ADST
      highbd_fadst16_col_many_neon,      // V_FLIPADST
      highbd_fidentity16_col_many_neon   // H_FLIPADST
    };

static const fwd_transform_1d_col_many_neon
    col_highbd_txfm4_xn_arr[TX_TYPES] = {
      highbd_fdct4_col_many_neon,       // DCT_DCT
      highbd_fadst4_col_many_neon,      // ADST_DCT
      highbd_fdct4_col_many_neon,       // DCT_ADST
      highbd_fadst4_col_many_neon,      // ADST_ADST
      highbd_fadst4_col_many_neon,      // FLIPADST_DCT
      highbd_fdct4_col_many_neon,       // DCT_FLIPADST
      highbd_fadst4_col_many_neon,      // FLIPADST_FLIPADST
      highbd_fadst4_col_many_neon,      // ADST_FLIPADST
      highbd_fadst4_col_many_neon,      // FLIPADST_ADST
      highbd_fidentity4_col_many_neon,  // IDTX
      highbd_fdct4_col_many_neon,       // V_DCT
      highbd_fidentity4_col_many_neon,  // H_DCT
      highbd_fadst4_col_many_neon,      // V_ADST
      highbd_fidentity4_col_many_neon,  // H_ADST
      highbd_fadst4_col_many_neon,      // V_FLIPADST
      highbd_fidentity4_col_many_neon   // H_FLIPADST
    };

static const fwd_transform_1d_row_neon row_highbd_txfm16_xn_arr[TX_TYPES] = {
  highbd_fdct16_row_neon,       // DCT_DCT
  highbd_fdct16_row_neon,       // ADST_DCT
  highbd_fadst16_row_neon,      // DCT_ADST
  highbd_fadst16_row_neon,      // ADST_ADST
  highbd_fdct16_row_neon,       // FLIPADST_DCT
  highbd_fadst16_row_neon,      // DCT_FLIPADST
  highbd_fadst16_row_neon,      // FLIPADST_FLIPADST
  highbd_fadst16_row_neon,      // ADST_FLIPADST
  highbd_fadst16_row_neon,      // FLIPADST_ADST
  highbd_fidentity16_row_neon,  // IDTX
  highbd_fidentity16_row_neon,  // V_DCT
  highbd_fdct16_row_neon,       // H_DCT
  highbd_fidentity16_row_neon,  // V_ADST
  highbd_fadst16_row_neon,      // H_ADST
  highbd_fidentity16_row_neon,  // V_FLIPADST
  highbd_fadst16_row_neon       // H_FLIPADST
};

static const fwd_transform_1d_row_many_neon
    row_rect_highbd_txfm16_xn_arr[TX_TYPES] = {
      highbd_fdct16_row_rect_many_neon,       // DCT_DCT
      highbd_fdct16_row_rect_many_neon,       // ADST_DCT
      highbd_fadst16_row_rect_many_neon,      // DCT_ADST
      highbd_fadst16_row_rect_many_neon,      // ADST_ADST
      highbd_fdct16_row_rect_many_neon,       // FLIPADST_DCT
      highbd_fadst16_row_rect_many_neon,      // DCT_FLIPADST
      highbd_fadst16_row_rect_many_neon,      // FLIPADST_FLIPADST
      highbd_fadst16_row_rect_many_neon,      // ADST_FLIPADST
      highbd_fadst16_row_rect_many_neon,      // FLIPADST_ADST
      highbd_fidentity16_row_rect_many_neon,  // IDTX
      highbd_fidentity16_row_rect_many_neon,  // V_DCT
      highbd_fdct16_row_rect_many_neon,       // H_DCT
      highbd_fidentity16_row_rect_many_neon,  // V_ADST
      highbd_fadst16_row_rect_many_neon,      // H_ADST
      highbd_fidentity16_row_rect_many_neon,  // V_FLIPADST
      highbd_fadst16_row_rect_many_neon       // H_FLIPADST
    };

#if !CONFIG_REALTIME_ONLY
static const fwd_transform_1d_row_many_neon
    row_highbd_txfm8_xn_arr[TX_TYPES] = {
      highbd_fdct8_row_many_neon,       // DCT_DCT
      highbd_fdct8_row_many_neon,       // ADST_DCT
      highbd_fadst8_row_many_neon,      // DCT_ADST
      highbd_fadst8_row_many_neon,      // ADST_ADST
      highbd_fdct8_row_many_neon,       // FLIPADST_DCT
      highbd_fadst8_row_many_neon,      // DCT_FLIPADST
      highbd_fadst8_row_many_neon,      // FLIPADST_FLIPADST
      highbd_fadst8_row_many_neon,      // ADST_FLIPADST
      highbd_fadst8_row_many_neon,      // FLIPADST_ADST
      highbd_fidentity8_row_many_neon,  // IDTX
      highbd_fidentity8_row_many_neon,  // V_DCT
      highbd_fdct8_row_many_neon,       // H_DCT
      highbd_fidentity8_row_many_neon,  // V_ADST
      highbd_fadst8_row_many_neon,      // H_ADST
      highbd_fidentity8_row_many_neon,  // V_FLIPADST
      highbd_fadst8_row_many_neon       // H_FLIPADST
    };
#endif

static const fwd_transform_1d_row_many_neon
    row_rect_highbd_txfm8_xn_arr[TX_TYPES] = {
      highbd_fdct8_row_rect_many_neon,       // DCT_DCT
      highbd_fdct8_row_rect_many_neon,       // ADST_DCT
      highbd_fadst8_row_rect_many_neon,      // DCT_ADST
      highbd_fadst8_row_rect_many_neon,      // ADST_ADST
      highbd_fdct8_row_rect_many_neon,       // FLIPADST_DCT
      highbd_fadst8_row_rect_many_neon,      // DCT_FLIPADST
      highbd_fadst8_row_rect_many_neon,      // FLIPADST_FLIPADST
      highbd_fadst8_row_rect_many_neon,      // ADST_FLIPADST
      highbd_fadst8_row_rect_many_neon,      // FLIPADST_ADST
      highbd_fidentity8_row_rect_many_neon,  // IDTX
      highbd_fidentity8_row_rect_many_neon,  // V_DCT
      highbd_fdct8_row_rect_many_neon,       // H_DCT
      highbd_fidentity8_row_rect_many_neon,  // V_ADST
      highbd_fadst8_row_rect_many_neon,      // H_ADST
      highbd_fidentity8_row_rect_many_neon,  // V_FLIPADST
      highbd_fadst8_row_rect_many_neon       // H_FLIPADST
    };

static const fwd_transform_1d_row_neon row_highbd_txfm8_x4_arr[TX_TYPES] = {
  highbd_fdct8_row_rect_neon,       // DCT_DCT
  highbd_fdct8_row_rect_neon,       // ADST_DCT
  highbd_fadst8_row_rect_neon,      // DCT_ADST
  highbd_fadst8_row_rect_neon,      // ADST_ADST
  highbd_fdct8_row_rect_neon,       // FLIPADST_DCT
  highbd_fadst8_row_rect_neon,      // DCT_FLIPADST
  highbd_fadst8_row_rect_neon,      // FLIPADST_FLIPADST
  highbd_fadst8_row_rect_neon,      // ADST_FLIPADST
  highbd_fadst8_row_rect_neon,      // FLIPADST_ADST
  highbd_fidentity8_row_rect_neon,  // IDTX
  highbd_fidentity8_row_rect_neon,  // V_DCT
  highbd_fdct8_row_rect_neon,       // H_DCT
  highbd_fidentity8_row_rect_neon,  // V_ADST
  highbd_fadst8_row_rect_neon,      // H_ADST
  highbd_fidentity8_row_rect_neon,  // V_FLIPADST
  highbd_fadst8_row_rect_neon       // H_FLIPADST
};

#if !CONFIG_REALTIME_ONLY
static const fwd_transform_1d_row_many_neon
    row_highbd_txfm4_xn_arr[TX_TYPES] = {
      highbd_fdct4_row_many_neon,       // DCT_DCT
      highbd_fdct4_row_many_neon,       // ADST_DCT
      highbd_fadst4_row_many_neon,      // DCT_ADST
      highbd_fadst4_row_many_neon,      // ADST_ADST
      highbd_fdct4_row_many_neon,       // FLIPADST_DCT
      highbd_fadst4_row_many_neon,      // DCT_FLIPADST
      highbd_fadst4_row_many_neon,      // FLIPADST_FLIPADST
      highbd_fadst4_row_many_neon,      // ADST_FLIPADST
      highbd_fadst4_row_many_neon,      // FLIPADST_ADST
      highbd_fidentity4_row_many_neon,  // IDTX
      highbd_fidentity4_row_many_neon,  // V_DCT
      highbd_fdct4_row_many_neon,       // H_DCT
      highbd_fidentity4_row_many_neon,  // V_ADST
      highbd_fadst4_row_many_neon,      // H_ADST
      highbd_fidentity4_row_many_neon,  // V_FLIPADST
      highbd_fadst4_row_many_neon       // H_FLIPADST
    };
#endif

static const fwd_transform_1d_row_many_neon
    row_rect_highbd_txfm4_xn_arr[TX_TYPES] = {
      highbd_fdct4_row_rect_many_neon,       // DCT_DCT
      highbd_fdct4_row_rect_many_neon,       // ADST_DCT
      highbd_fadst4_row_rect_many_neon,      // DCT_ADST
      highbd_fadst4_row_rect_many_neon,      // ADST_ADST
      highbd_fdct4_row_rect_many_neon,       // FLIPADST_DCT
      highbd_fadst4_row_rect_many_neon,      // DCT_FLIPADST
      highbd_fadst4_row_rect_many_neon,      // FLIPADST_FLIPADST
      highbd_fadst4_row_rect_many_neon,      // ADST_FLIPADST
      highbd_fadst4_row_rect_many_neon,      // FLIPADST_ADST
      highbd_fidentity4_row_rect_many_neon,  // IDTX
      highbd_fidentity4_row_rect_many_neon,  // V_DCT
      highbd_fdct4_row_rect_many_neon,       // H_DCT
      highbd_fidentity4_row_rect_many_neon,  // V_ADST
      highbd_fadst4_row_rect_many_neon,      // H_ADST
      highbd_fidentity4_row_rect_many_neon,  // V_FLIPADST
      highbd_fadst4_row_rect_many_neon       // H_FLIPADST
    };

static void highbd_fdct32_x4_neon(const int32x4_t *input, int32x4_t *output,
                                  int cos_bit) {
  const int32_t *const cospi = cospi_arr_s32(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // Workspaces for intermediate transform steps.
  int32x4_t buf0[32];
  int32x4_t buf1[32];

  // stage 1
  butterfly_dct_pre(input, buf1, 32);

  // stage 2
  butterfly_dct_pre(buf1, buf0, 16);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  buf0[18] = buf1[18];
  buf0[19] = buf1[19];
  butterfly_0112_neon(cospi, 32, buf1[27], buf1[20], &buf0[27], &buf0[20],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 32, buf1[26], buf1[21], &buf0[26], &buf0[21],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 32, buf1[25], buf1[22], &buf0[25], &buf0[22],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 32, buf1[24], buf1[23], &buf0[24], &buf0[23],
                      v_cos_bit);
  buf0[28] = buf1[28];
  buf0[29] = buf1[29];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 3
  butterfly_dct_pre(buf0, buf1, 8);
  buf1[8] = buf0[8];
  buf1[9] = buf0[9];
  butterfly_0112_neon(cospi, 32, buf0[13], buf0[10], &buf1[13], &buf1[10],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 32, buf0[12], buf0[11], &buf1[12], &buf1[11],
                      v_cos_bit);
  buf1[14] = buf0[14];
  buf1[15] = buf0[15];
  butterfly_dct_post(buf0 + 16, buf0 + 16, buf1 + 16, 16);

  // stage 4
  butterfly_dct_pre(buf1, buf0, 4);
  buf0[4] = buf1[4];
  butterfly_0112_neon(cospi, 32, buf1[6], buf1[5], &buf0[6], &buf0[5],
                      v_cos_bit);
  buf0[7] = buf1[7];
  butterfly_dct_post(buf1 + 8, buf1 + 8, buf0 + 8, 8);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  butterfly_0112_neon(cospi, 16, buf1[29], buf1[18], &buf0[29], &buf0[18],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 16, buf1[28], buf1[19], &buf0[28], &buf0[19],
                      v_cos_bit);
  butterfly_2312_neon(cospi, 16, buf1[27], buf1[20], &buf0[20], &buf0[27],
                      v_cos_bit);
  butterfly_2312_neon(cospi, 16, buf1[26], buf1[21], &buf0[21], &buf0[26],
                      v_cos_bit);
  buf0[22] = buf1[22];
  buf0[23] = buf1[23];
  buf0[24] = buf1[24];
  buf0[25] = buf1[25];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 5
  butterfly_0112_neon(cospi, 32, buf0[0], buf0[1], &buf1[0], &buf1[1],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 16, buf0[3], buf0[2], &buf1[2], &buf1[3],
                      v_cos_bit);
  butterfly_dct_post(buf0 + 4, buf0 + 4, buf1 + 4, 4);
  buf1[8] = buf0[8];
  butterfly_0112_neon(cospi, 16, buf0[14], buf0[9], &buf1[14], &buf1[9],
                      v_cos_bit);
  butterfly_2312_neon(cospi, 16, buf0[13], buf0[10], &buf1[10], &buf1[13],
                      v_cos_bit);
  buf1[11] = buf0[11];
  buf1[12] = buf0[12];
  buf1[15] = buf0[15];
  butterfly_dct_post(buf0 + 16, buf0 + 16, buf1 + 16, 8);
  butterfly_dct_post(buf0 + 24, buf0 + 24, buf1 + 24, 8);

  // stage 6
  buf0[0] = buf1[0];
  buf0[1] = buf1[1];
  buf0[2] = buf1[2];
  buf0[3] = buf1[3];

  butterfly_0112_neon(cospi, 8, buf1[7], buf1[4], &buf0[4], &buf0[7],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 8, buf1[30], buf1[17], &buf0[30], &buf0[17],
                      v_cos_bit);
  butterfly_2312_neon(cospi, 8, buf1[29], buf1[18], &buf0[18], &buf0[29],
                      v_cos_bit);
  butterfly_dct_post(buf1 + 8, buf1 + 8, buf0 + 8, 4);
  butterfly_dct_post(buf1 + 12, buf1 + 12, buf0 + 12, 4);
  buf0[16] = buf1[16];
  buf0[19] = buf1[19];
  buf0[20] = buf1[20];

  butterfly_0130_neon(cospi, 24, buf1[5], buf1[6], &buf0[5], &buf0[6],
                      v_cos_bit);
  butterfly_0130_neon(cospi, 24, buf1[21], buf1[26], &buf0[26], &buf0[21],
                      v_cos_bit);
  butterfly_0332_neon(cospi, 24, buf1[25], buf1[22], &buf0[25], &buf0[22],
                      v_cos_bit);

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
  butterfly_0112_neon(cospi, 4, buf0[15], buf0[8], &buf1[8], &buf1[15],
                      v_cos_bit);
  butterfly_0130_neon(cospi, 28, buf0[9], buf0[14], &buf1[9], &buf1[14],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 20, buf0[13], buf0[10], &buf1[10], &buf1[13],
                      v_cos_bit);
  butterfly_0130_neon(cospi, 12, buf0[11], buf0[12], &buf1[11], &buf1[12],
                      v_cos_bit);
  butterfly_dct_post(buf0 + 16, buf0 + 16, buf1 + 16, 4);
  butterfly_dct_post(buf0 + 20, buf0 + 20, buf1 + 20, 4);
  butterfly_dct_post(buf0 + 24, buf0 + 24, buf1 + 24, 4);
  butterfly_dct_post(buf0 + 28, buf0 + 28, buf1 + 28, 4);

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
  butterfly_0112_neon(cospi, 2, buf1[31], buf1[16], &buf0[16], &buf0[31],
                      v_cos_bit);
  butterfly_0130_neon(cospi, 30, buf1[17], buf1[30], &buf0[17], &buf0[30],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 18, buf1[29], buf1[18], &buf0[18], &buf0[29],
                      v_cos_bit);
  butterfly_0130_neon(cospi, 14, buf1[19], buf1[28], &buf0[19], &buf0[28],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 10, buf1[27], buf1[20], &buf0[20], &buf0[27],
                      v_cos_bit);
  butterfly_0130_neon(cospi, 22, buf1[21], buf1[26], &buf0[21], &buf0[26],
                      v_cos_bit);
  butterfly_0112_neon(cospi, 26, buf1[25], buf1[22], &buf0[22], &buf0[25],
                      v_cos_bit);
  butterfly_0130_neon(cospi, 6, buf1[23], buf1[24], &buf0[23], &buf0[24],
                      v_cos_bit);

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

static void highbd_fdct64_x4_neon(const int32x4_t *input, int32x4_t *output,
                                  int8_t cos_bit) {
  const int32_t *const cospi = cospi_arr_s32(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1
  int32x4_t x1[64];
  butterfly_dct_pre(input, x1, 64);

  // stage 2
  int32x4_t x2[64];
  butterfly_dct_pre(x1, x2, 32);
  x2[32] = x1[32];
  x2[33] = x1[33];
  x2[34] = x1[34];
  x2[35] = x1[35];
  x2[36] = x1[36];
  x2[37] = x1[37];
  x2[38] = x1[38];
  x2[39] = x1[39];
  butterfly_0112_neon(cospi, 32, x1[55], x1[40], &x2[55], &x2[40], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x1[54], x1[41], &x2[54], &x2[41], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x1[53], x1[42], &x2[53], &x2[42], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x1[52], x1[43], &x2[52], &x2[43], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x1[51], x1[44], &x2[51], &x2[44], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x1[50], x1[45], &x2[50], &x2[45], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x1[49], x1[46], &x2[49], &x2[46], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x1[48], x1[47], &x2[48], &x2[47], v_cos_bit);
  x2[56] = x1[56];
  x2[57] = x1[57];
  x2[58] = x1[58];
  x2[59] = x1[59];
  x2[60] = x1[60];
  x2[61] = x1[61];
  x2[62] = x1[62];
  x2[63] = x1[63];

  // stage 3
  int32x4_t x3[64];
  butterfly_dct_pre(x2, x3, 16);
  x3[16] = x2[16];
  x3[17] = x2[17];
  x3[18] = x2[18];
  x3[19] = x2[19];
  butterfly_0112_neon(cospi, 32, x2[27], x2[20], &x3[27], &x3[20], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x2[26], x2[21], &x3[26], &x3[21], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x2[25], x2[22], &x3[25], &x3[22], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x2[24], x2[23], &x3[24], &x3[23], v_cos_bit);
  x3[28] = x2[28];
  x3[29] = x2[29];
  x3[30] = x2[30];
  x3[31] = x2[31];
  butterfly_dct_post(x2 + 32, x2 + 32, x3 + 32, 32);

  // stage 4
  int32x4_t x4[64];
  butterfly_dct_pre(x3, x4, 8);
  x4[8] = x3[8];
  x4[9] = x3[9];
  butterfly_0112_neon(cospi, 32, x3[13], x3[10], &x4[13], &x4[10], v_cos_bit);
  butterfly_0112_neon(cospi, 32, x3[12], x3[11], &x4[12], &x4[11], v_cos_bit);
  x4[14] = x3[14];
  x4[15] = x3[15];
  butterfly_dct_post(x3 + 16, x3 + 16, x4 + 16, 16);
  x4[32] = x3[32];
  x4[33] = x3[33];
  x4[34] = x3[34];
  x4[35] = x3[35];
  butterfly_0112_neon(cospi, 16, x3[59], x3[36], &x4[59], &x4[36], v_cos_bit);
  butterfly_0112_neon(cospi, 16, x3[58], x3[37], &x4[58], &x4[37], v_cos_bit);
  butterfly_0112_neon(cospi, 16, x3[57], x3[38], &x4[57], &x4[38], v_cos_bit);
  butterfly_0112_neon(cospi, 16, x3[56], x3[39], &x4[56], &x4[39], v_cos_bit);
  butterfly_2312_neon(cospi, 16, x3[55], x3[40], &x4[40], &x4[55], v_cos_bit);
  butterfly_2312_neon(cospi, 16, x3[54], x3[41], &x4[41], &x4[54], v_cos_bit);
  butterfly_2312_neon(cospi, 16, x3[53], x3[42], &x4[42], &x4[53], v_cos_bit);
  butterfly_2312_neon(cospi, 16, x3[52], x3[43], &x4[43], &x4[52], v_cos_bit);
  x4[44] = x3[44];
  x4[45] = x3[45];
  x4[46] = x3[46];
  x4[47] = x3[47];
  x4[48] = x3[48];
  x4[49] = x3[49];
  x4[50] = x3[50];
  x4[51] = x3[51];
  x4[60] = x3[60];
  x4[61] = x3[61];
  x4[62] = x3[62];
  x4[63] = x3[63];

  // stage 5
  int32x4_t x5[64];
  butterfly_dct_pre(x4, x5, 4);
  x5[4] = x4[4];
  butterfly_0112_neon(cospi, 32, x4[6], x4[5], &x5[6], &x5[5], v_cos_bit);
  x5[7] = x4[7];
  butterfly_dct_post(x4 + 8, x4 + 8, x5 + 8, 8);
  x5[16] = x4[16];
  x5[17] = x4[17];
  butterfly_0112_neon(cospi, 16, x4[29], x4[18], &x5[29], &x5[18], v_cos_bit);
  butterfly_0112_neon(cospi, 16, x4[28], x4[19], &x5[28], &x5[19], v_cos_bit);
  butterfly_2312_neon(cospi, 16, x4[27], x4[20], &x5[20], &x5[27], v_cos_bit);
  butterfly_2312_neon(cospi, 16, x4[26], x4[21], &x5[21], &x5[26], v_cos_bit);
  x5[22] = x4[22];
  x5[23] = x4[23];
  x5[24] = x4[24];
  x5[25] = x4[25];
  x5[30] = x4[30];
  x5[31] = x4[31];
  butterfly_dct_post(x4 + 32, x4 + 32, x5 + 32, 16);
  butterfly_dct_post(x4 + 48, x4 + 48, x5 + 48, 16);

  // stage 6
  int32x4_t x6[64];
  butterfly_0112_neon(cospi, 32, x5[0], x5[1], &x6[0], &x6[1], v_cos_bit);
  butterfly_0112_neon(cospi, 16, x5[3], x5[2], &x6[2], &x6[3], v_cos_bit);
  butterfly_dct_post(x5 + 4, x5 + 4, x6 + 4, 4);
  x6[8] = x5[8];
  butterfly_0112_neon(cospi, 16, x5[14], x5[9], &x6[14], &x6[9], v_cos_bit);
  butterfly_2312_neon(cospi, 16, x5[13], x5[10], &x6[10], &x6[13], v_cos_bit);
  x6[11] = x5[11];
  x6[12] = x5[12];
  x6[15] = x5[15];
  butterfly_dct_post(x5 + 16, x5 + 16, x6 + 16, 8);
  butterfly_dct_post(x5 + 24, x5 + 24, x6 + 24, 8);
  x6[32] = x5[32];
  x6[33] = x5[33];
  butterfly_0112_neon(cospi, 8, x5[61], x5[34], &x6[61], &x6[34], v_cos_bit);
  butterfly_0112_neon(cospi, 8, x5[60], x5[35], &x6[60], &x6[35], v_cos_bit);
  butterfly_2312_neon(cospi, 8, x5[59], x5[36], &x6[36], &x6[59], v_cos_bit);
  butterfly_2312_neon(cospi, 8, x5[58], x5[37], &x6[37], &x6[58], v_cos_bit);
  x6[38] = x5[38];
  x6[39] = x5[39];
  x6[40] = x5[40];
  x6[41] = x5[41];
  butterfly_0130_neon(cospi, 24, x5[42], x5[53], &x6[53], &x6[42], v_cos_bit);
  butterfly_0130_neon(cospi, 24, x5[43], x5[52], &x6[52], &x6[43], v_cos_bit);
  butterfly_0332_neon(cospi, 24, x5[51], x5[44], &x6[51], &x6[44], v_cos_bit);
  butterfly_0332_neon(cospi, 24, x5[50], x5[45], &x6[50], &x6[45], v_cos_bit);
  x6[46] = x5[46];
  x6[47] = x5[47];
  x6[48] = x5[48];
  x6[49] = x5[49];
  x6[54] = x5[54];
  x6[55] = x5[55];
  x6[56] = x5[56];
  x6[57] = x5[57];
  x6[62] = x5[62];
  x6[63] = x5[63];

  // stage 7
  int32x4_t x7[64];
  x7[0] = x6[0];
  x7[1] = x6[1];
  x7[2] = x6[2];
  x7[3] = x6[3];
  butterfly_0112_neon(cospi, 8, x6[7], x6[4], &x7[4], &x7[7], v_cos_bit);
  butterfly_0130_neon(cospi, 24, x6[5], x6[6], &x7[5], &x7[6], v_cos_bit);
  butterfly_dct_post(x6 + 8, x6 + 8, x7 + 8, 4);
  butterfly_dct_post(x6 + 12, x6 + 12, x7 + 12, 4);
  x7[16] = x6[16];
  butterfly_0112_neon(cospi, 8, x6[30], x6[17], &x7[30], &x7[17], v_cos_bit);
  butterfly_2312_neon(cospi, 8, x6[29], x6[18], &x7[18], &x7[29], v_cos_bit);
  x7[19] = x6[19];
  x7[20] = x6[20];
  butterfly_0130_neon(cospi, 24, x6[21], x6[26], &x7[26], &x7[21], v_cos_bit);
  butterfly_0332_neon(cospi, 24, x6[25], x6[22], &x7[25], &x7[22], v_cos_bit);
  x7[23] = x6[23];
  x7[24] = x6[24];
  x7[27] = x6[27];
  x7[28] = x6[28];
  x7[31] = x6[31];
  butterfly_dct_post(x6 + 32, x6 + 32, x7 + 32, 8);
  butterfly_dct_post(x6 + 40, x6 + 40, x7 + 40, 8);
  butterfly_dct_post(x6 + 48, x6 + 48, x7 + 48, 8);
  butterfly_dct_post(x6 + 56, x6 + 56, x7 + 56, 8);

  // stage 8
  int32x4_t x8[64];
  x8[0] = x7[0];
  x8[1] = x7[1];
  x8[2] = x7[2];
  x8[3] = x7[3];
  x8[4] = x7[4];
  x8[5] = x7[5];
  x8[6] = x7[6];
  x8[7] = x7[7];

  butterfly_0112_neon(cospi, 4, x7[15], x7[8], &x8[8], &x8[15], v_cos_bit);
  butterfly_0130_neon(cospi, 28, x7[9], x7[14], &x8[9], &x8[14], v_cos_bit);
  butterfly_0112_neon(cospi, 20, x7[13], x7[10], &x8[10], &x8[13], v_cos_bit);
  butterfly_0130_neon(cospi, 12, x7[11], x7[12], &x8[11], &x8[12], v_cos_bit);
  butterfly_dct_post(x7 + 16, x7 + 16, x8 + 16, 4);
  butterfly_dct_post(x7 + 20, x7 + 20, x8 + 20, 4);
  butterfly_dct_post(x7 + 24, x7 + 24, x8 + 24, 4);
  butterfly_dct_post(x7 + 28, x7 + 28, x8 + 28, 4);
  x8[32] = x7[32];
  butterfly_0112_neon(cospi, 4, x7[62], x7[33], &x8[62], &x8[33], v_cos_bit);
  butterfly_2312_neon(cospi, 4, x7[61], x7[34], &x8[34], &x8[61], v_cos_bit);
  x8[35] = x7[35];
  x8[36] = x7[36];
  butterfly_0130_neon(cospi, 28, x7[37], x7[58], &x8[58], &x8[37], v_cos_bit);
  butterfly_0332_neon(cospi, 28, x7[57], x7[38], &x8[57], &x8[38], v_cos_bit);
  x8[39] = x7[39];
  x8[40] = x7[40];
  butterfly_0112_neon(cospi, 20, x7[54], x7[41], &x8[54], &x8[41], v_cos_bit);
  butterfly_2312_neon(cospi, 20, x7[53], x7[42], &x8[42], &x8[53], v_cos_bit);
  x8[43] = x7[43];
  x8[44] = x7[44];
  butterfly_0130_neon(cospi, 12, x7[45], x7[50], &x8[50], &x8[45], v_cos_bit);
  butterfly_0332_neon(cospi, 12, x7[49], x7[46], &x8[49], &x8[46], v_cos_bit);
  x8[47] = x7[47];
  x8[48] = x7[48];
  x8[51] = x7[51];
  x8[52] = x7[52];
  x8[55] = x7[55];
  x8[56] = x7[56];
  x8[59] = x7[59];
  x8[60] = x7[60];
  x8[63] = x7[63];

  // stage 9
  int32x4_t x9[64];
  x9[0] = x8[0];
  x9[1] = x8[1];
  x9[2] = x8[2];
  x9[3] = x8[3];
  x9[4] = x8[4];
  x9[5] = x8[5];
  x9[6] = x8[6];
  x9[7] = x8[7];
  x9[8] = x8[8];
  x9[9] = x8[9];
  x9[10] = x8[10];
  x9[11] = x8[11];
  x9[12] = x8[12];
  x9[13] = x8[13];
  x9[14] = x8[14];
  x9[15] = x8[15];
  butterfly_0112_neon(cospi, 2, x8[31], x8[16], &x9[16], &x9[31], v_cos_bit);
  butterfly_0130_neon(cospi, 30, x8[17], x8[30], &x9[17], &x9[30], v_cos_bit);
  butterfly_0112_neon(cospi, 18, x8[29], x8[18], &x9[18], &x9[29], v_cos_bit);
  butterfly_0130_neon(cospi, 14, x8[19], x8[28], &x9[19], &x9[28], v_cos_bit);
  butterfly_0112_neon(cospi, 10, x8[27], x8[20], &x9[20], &x9[27], v_cos_bit);
  butterfly_0130_neon(cospi, 22, x8[21], x8[26], &x9[21], &x9[26], v_cos_bit);
  butterfly_0112_neon(cospi, 26, x8[25], x8[22], &x9[22], &x9[25], v_cos_bit);
  butterfly_0130_neon(cospi, 6, x8[23], x8[24], &x9[23], &x9[24], v_cos_bit);
  butterfly_dct_post(x8 + 32, x8 + 32, x9 + 32, 4);
  butterfly_dct_post(x8 + 36, x8 + 36, x9 + 36, 4);
  butterfly_dct_post(x8 + 40, x8 + 40, x9 + 40, 4);
  butterfly_dct_post(x8 + 44, x8 + 44, x9 + 44, 4);
  butterfly_dct_post(x8 + 48, x8 + 48, x9 + 48, 4);
  butterfly_dct_post(x8 + 52, x8 + 52, x9 + 52, 4);
  butterfly_dct_post(x8 + 56, x8 + 56, x9 + 56, 4);
  butterfly_dct_post(x8 + 60, x8 + 60, x9 + 60, 4);

  // stage 10
  int32x4_t x10[64];
  x10[0] = x9[0];
  x10[1] = x9[1];
  x10[2] = x9[2];
  x10[3] = x9[3];
  x10[4] = x9[4];
  x10[5] = x9[5];
  x10[6] = x9[6];
  x10[7] = x9[7];
  x10[8] = x9[8];
  x10[9] = x9[9];
  x10[10] = x9[10];
  x10[11] = x9[11];
  x10[12] = x9[12];
  x10[13] = x9[13];
  x10[14] = x9[14];
  x10[15] = x9[15];
  x10[16] = x9[16];
  x10[17] = x9[17];
  x10[18] = x9[18];
  x10[19] = x9[19];
  x10[20] = x9[20];
  x10[21] = x9[21];
  x10[22] = x9[22];
  x10[23] = x9[23];
  x10[24] = x9[24];
  x10[25] = x9[25];
  x10[26] = x9[26];
  x10[27] = x9[27];
  x10[28] = x9[28];
  x10[29] = x9[29];
  x10[30] = x9[30];
  x10[31] = x9[31];
  butterfly_0112_neon(cospi, 1, x9[63], x9[32], &x10[32], &x10[63], v_cos_bit);
  butterfly_0130_neon(cospi, 31, x9[33], x9[62], &x10[33], &x10[62], v_cos_bit);
  butterfly_0112_neon(cospi, 17, x9[61], x9[34], &x10[34], &x10[61], v_cos_bit);
  butterfly_0130_neon(cospi, 15, x9[35], x9[60], &x10[35], &x10[60], v_cos_bit);
  butterfly_0112_neon(cospi, 9, x9[59], x9[36], &x10[36], &x10[59], v_cos_bit);
  butterfly_0130_neon(cospi, 23, x9[37], x9[58], &x10[37], &x10[58], v_cos_bit);
  butterfly_0112_neon(cospi, 25, x9[57], x9[38], &x10[38], &x10[57], v_cos_bit);
  butterfly_0130_neon(cospi, 7, x9[39], x9[56], &x10[39], &x10[56], v_cos_bit);
  butterfly_0112_neon(cospi, 5, x9[55], x9[40], &x10[40], &x10[55], v_cos_bit);
  butterfly_0130_neon(cospi, 27, x9[41], x9[54], &x10[41], &x10[54], v_cos_bit);
  butterfly_0112_neon(cospi, 21, x9[53], x9[42], &x10[42], &x10[53], v_cos_bit);
  butterfly_0130_neon(cospi, 11, x9[43], x9[52], &x10[43], &x10[52], v_cos_bit);
  butterfly_0112_neon(cospi, 13, x9[51], x9[44], &x10[44], &x10[51], v_cos_bit);
  butterfly_0130_neon(cospi, 19, x9[45], x9[50], &x10[45], &x10[50], v_cos_bit);
  butterfly_0112_neon(cospi, 29, x9[49], x9[46], &x10[46], &x10[49], v_cos_bit);
  butterfly_0130_neon(cospi, 3, x9[47], x9[48], &x10[47], &x10[48], v_cos_bit);

  // stage 11
  output[0] = x10[0];
  output[1] = x10[32];
  output[2] = x10[16];
  output[3] = x10[48];
  output[4] = x10[8];
  output[5] = x10[40];
  output[6] = x10[24];
  output[7] = x10[56];
  output[8] = x10[4];
  output[9] = x10[36];
  output[10] = x10[20];
  output[11] = x10[52];
  output[12] = x10[12];
  output[13] = x10[44];
  output[14] = x10[28];
  output[15] = x10[60];
  output[16] = x10[2];
  output[17] = x10[34];
  output[18] = x10[18];
  output[19] = x10[50];
  output[20] = x10[10];
  output[21] = x10[42];
  output[22] = x10[26];
  output[23] = x10[58];
  output[24] = x10[6];
  output[25] = x10[38];
  output[26] = x10[22];
  output[27] = x10[54];
  output[28] = x10[14];
  output[29] = x10[46];
  output[30] = x10[30];
  output[31] = x10[62];
  output[32] = x10[1];
  output[33] = x10[33];
  output[34] = x10[17];
  output[35] = x10[49];
  output[36] = x10[9];
  output[37] = x10[41];
  output[38] = x10[25];
  output[39] = x10[57];
  output[40] = x10[5];
  output[41] = x10[37];
  output[42] = x10[21];
  output[43] = x10[53];
  output[44] = x10[13];
  output[45] = x10[45];
  output[46] = x10[29];
  output[47] = x10[61];
  output[48] = x10[3];
  output[49] = x10[35];
  output[50] = x10[19];
  output[51] = x10[51];
  output[52] = x10[11];
  output[53] = x10[43];
  output[54] = x10[27];
  output[55] = x10[59];
  output[56] = x10[7];
  output[57] = x10[39];
  output[58] = x10[23];
  output[59] = x10[55];
  output[60] = x10[15];
  output[61] = x10[47];
  output[62] = x10[31];
  output[63] = x10[63];
}

static void highbd_fidentity32_x4_neon(const int32x4_t *input,
                                       int32x4_t *output, int cos_bit) {
  (void)cos_bit;
  for (int i = 0; i < 32; i++) {
    output[i] = vshlq_n_s32(input[i], 2);
  }
}

TRANSFORM_COL_MANY(fdct32, 32)
TRANSFORM_COL_MANY(fidentity32, 32)

static const fwd_transform_1d_col_many_neon
    col_highbd_txfm32_x4_arr[TX_TYPES] = {
      highbd_fdct32_col_many_neon,       // DCT_DCT
      NULL,                              // ADST_DCT
      NULL,                              // DCT_ADST
      NULL,                              // ADST_ADST
      NULL,                              // FLIPADST_DCT
      NULL,                              // DCT_FLIPADST
      NULL,                              // FLIPADST_FLIPADST
      NULL,                              // ADST_FLIPADST
      NULL,                              // FLIPADST_ADST
      highbd_fidentity32_col_many_neon,  // IDTX
      NULL,                              // V_DCT
      NULL,                              // H_DCT
      NULL,                              // V_ADST
      NULL,                              // H_ADST
      NULL,                              // V_FLIPADST
      NULL                               // H_FLIPADST
    };

TRANSFORM_ROW_MANY(fdct32, 32)
TRANSFORM_ROW_MANY(fidentity32, 32)

static const fwd_transform_1d_row_many_neon
    row_highbd_txfm32_x4_arr[TX_TYPES] = {
      highbd_fdct32_row_many_neon,       // DCT_DCT
      NULL,                              // ADST_DCT
      NULL,                              // DCT_ADST
      NULL,                              // ADST_ADST
      NULL,                              // FLIPADST_DCT
      NULL,                              // DCT_FLIPADST
      NULL,                              // FLIPADST_FLIPADST
      NULL,                              // ADST_FLIPADST
      NULL,                              // FLIPADST_ADST
      highbd_fidentity32_row_many_neon,  // IDTX
      NULL,                              // V_DCT
      NULL,                              // H_DCT
      NULL,                              // V_ADST
      NULL,                              // H_ADST
      NULL,                              // V_FLIPADST
      NULL                               // H_FLIPADST
    };

TRANSFORM_ROW_RECT_MANY(fdct32, 32)
TRANSFORM_ROW_RECT_MANY(fidentity32, 32)

static const fwd_transform_1d_row_many_neon
    row_rect_highbd_txfm32_x4_arr[TX_TYPES] = {
      highbd_fdct32_row_rect_many_neon,       // DCT_DCT
      NULL,                                   // ADST_DCT
      NULL,                                   // DCT_ADST
      NULL,                                   // ADST_ADST
      NULL,                                   // FLIPADST_DCT
      NULL,                                   // DCT_FLIPADST
      NULL,                                   // FLIPADST_FLIPADST
      NULL,                                   // ADST_FLIPADST
      NULL,                                   // FLIPADST_ADST
      highbd_fidentity32_row_rect_many_neon,  // IDTX
      NULL,                                   // V_DCT
      NULL,                                   // H_DCT
      NULL,                                   // V_ADST
      NULL,                                   // H_ADST
      NULL,                                   // V_FLIPADST
      NULL                                    // H_FLIPADST
    };

void av1_fwd_txfm2d_16x8_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm8_xn_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_rect_highbd_txfm16_xn_arr[tx_type];
  int bit = av1_fwd_cos_bit_col[2][1];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 8);

  // Column-wise transform.
  int32x4_t buf0[32];
  if (lr_flip) {
    col_txfm(input, buf0 + 3 * 8, stride, bit, /*lr_flip=*/1, /*howmany=*/4,
             /*hm_stride=*/-8);
  } else {
    col_txfm(input, buf0, stride, bit, /*lr_flip=*/0, /*howmany=*/4,
             /*hm_stride=*/8);
  }
  shift_right_2_round_s32_x4(buf0, buf0, 32);

  int32x4_t buf1[32];
  transpose_arrays_s32_16x8(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bit, /*howmany=*/2, /*hm_stride=*/16, /*stride=*/8);
}

void av1_fwd_txfm2d_8x16_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm16_xn_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_rect_highbd_txfm8_xn_arr[tx_type];
  int bit = av1_fwd_cos_bit_col[1][2];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 16);

  // Column-wise transform.
  int32x4_t buf0[32];
  if (lr_flip) {
    col_txfm(input, buf0 + 16, stride, bit, /*lr_flip=*/1, /*howmany=*/2,
             /*hm_stride=*/-16);
  } else {
    col_txfm(input, buf0, stride, bit, /*lr_flip=*/0, /*howmany=*/2,
             /*hm_stride=*/16);
  }
  shift_right_2_round_s32_x4(buf0, buf0, 32);

  int32x4_t buf1[32];
  transpose_arrays_s32_8x16(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bit, /*howmany=*/4, /*hm_stride=*/8, /*stride=*/16);
}

#if !CONFIG_REALTIME_ONLY
void av1_fwd_txfm2d_4x16_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;
  int bitcol = av1_fwd_cos_bit_col[0][2];
  int bitrow = av1_fwd_cos_bit_row[0][2];
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm16_xn_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_highbd_txfm4_xn_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 16);

  // Column-wise transform.
  int32x4_t buf0[16];
  if (lr_flip) {
    col_txfm(input, buf0, stride, bitcol, /*lr_flip=*/1, /*howmany=*/1,
             /*hm_stride=*/0);
  } else {
    col_txfm(input, buf0, stride, bitcol, /*lr_flip=*/0, /*howmany=*/1,
             /*hm_stride=*/0);
  }
  shift_right_1_round_s32_x4(buf0, buf0, 16);

  int32x4_t buf1[16];
  transpose_arrays_s32_4x16(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bitrow, /*howmany=*/4, /*hm_stride=*/4, /*stride=*/16);
}
#endif

void av1_fwd_txfm2d_16x4_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;
  int bitcol = av1_fwd_cos_bit_col[2][0];
  int bitrow = av1_fwd_cos_bit_row[2][0];
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm4_xn_arr[tx_type];
  const fwd_transform_1d_row_neon row_txfm = row_highbd_txfm16_xn_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 4);

  // Column-wise transform.
  int32x4_t buf0[16];
  if (lr_flip) {
    col_txfm(input, buf0 + 3 * 4, stride, bitcol, /*lr_flip=*/1, /*howmany=*/4,
             /*hm_stride=*/-4);
  } else {
    col_txfm(input, buf0, stride, bitcol, /*lr_flip=*/0, /*howmany=*/4,
             /*hm_stride=*/4);
  }

  shift_right_1_round_s32_x4(buf0, buf0, 16);
  transpose_arrays_s32_4x16(buf0, buf0);

  // Row-wise transform.
  row_txfm(buf0, coeff, bitrow, /*stride=*/4);
}

void av1_fwd_txfm2d_16x32_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)bd;
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm32_x4_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_rect_highbd_txfm16_xn_arr[tx_type];
  int bitcol = av1_fwd_cos_bit_col[2][3];
  int bitrow = av1_fwd_cos_bit_row[2][3];

  // Column-wise transform.
  int32x4_t buf0[128];
  col_txfm(input, buf0, stride, bitcol, /*lr_flip=*/0, /*howmany=*/4,
           /*hm_stride=*/32);
  shift_right_4_round_s32_x4(buf0, buf0, 128);

  int32x4_t buf1[128];
  transpose_arrays_s32_16x32(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bitrow, /*howmany=*/8, /*hm_stride=*/16, /*stride=*/32);
}

void av1_fwd_txfm2d_32x64_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  int bitcol = av1_fwd_cos_bit_col[3][4];
  int bitrow = av1_fwd_cos_bit_row[3][4];

  // Column-wise transform.
  int32x4_t buf0[512];
  load_buffer_32x64(input, buf0, stride, 0);
  for (int i = 0; i < 8; i++) {
    highbd_fdct64_x4_neon(buf0 + i * 64, buf0 + i * 64, bitcol);
  }
  shift_right_2_round_s32_x4(buf0, buf0, 512);

  int32x4_t buf1[512];
  transpose_arrays_s32_32x64(buf0, buf1);

  // Row-wise transform.
  for (int i = 0; i < 16; i++) {
    highbd_fdct32_x4_neon(buf1 + i * 32, buf1 + i * 32, bitrow);
  }
  round_shift2_rect_array_s32_neon(buf1, buf1, 512);
  store_buffer_32x32(buf1, coeff, /*stride=*/32);
}

void av1_fwd_txfm2d_64x32_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  int bitcol = av1_fwd_cos_bit_col[4][3];
  int bitrow = av1_fwd_cos_bit_row[4][3];

  // Column-wise transform.
  int32x4_t buf0[512];
  load_buffer_64x32(input, buf0, stride, 0);
  for (int i = 0; i < 16; i++) {
    highbd_fdct32_x4_neon(buf0 + i * 32, buf0 + i * 32, bitcol);
  }
  shift_right_4_round_s32_x4(buf0, buf0, 512);

  int32x4_t buf1[512];
  transpose_arrays_s32_64x32(buf0, buf1);

  // Row-wise transform.
  for (int i = 0; i < 8; i++) {
    highbd_fdct64_x4_neon(buf1 + i * 64, buf1 + i * 64, bitrow);
  }
  round_shift2_rect_array_s32_neon(buf1, buf1, 512);
  store_buffer_64x32(buf1, coeff, /*stride=*/32);
}

void av1_fwd_txfm2d_32x16_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)bd;
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm16_xn_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_rect_highbd_txfm32_x4_arr[tx_type];
  int bitcol = av1_fwd_cos_bit_col[3][2];
  int bitrow = av1_fwd_cos_bit_row[3][2];

  // Column-wise transform.
  int32x4_t buf0[128];
  col_txfm(input, buf0, stride, bitcol, /*lr_flip=*/0, /*howmany=*/8,
           /*hm_stride=*/16);
  shift_right_4_round_s32_x4(buf0, buf0, 128);

  int32x4_t buf1[128];
  transpose_arrays_s32_32x16(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bitrow, /*howmany=*/4, /*hm_stride=*/32, /*stride=*/16);
}

#if !CONFIG_REALTIME_ONLY
void av1_fwd_txfm2d_8x32_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm32_x4_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_highbd_txfm8_xn_arr[tx_type];
  int bitcol = av1_fwd_cos_bit_col[1][3];
  int bitrow = av1_fwd_cos_bit_row[1][3];

  // Column-wise transform.
  int32x4_t buf0[64];
  col_txfm(input, buf0, stride, bitcol, /*lr_flip=*/0, /*howmany=*/2,
           /*hm_stride=*/32);
  shift_right_2_round_s32_x4(buf0, buf0, 64);

  int32x4_t buf1[64];
  transpose_arrays_s32_8x32(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bitrow, /*howmany=*/8, /*hm_stride=*/8, /*stride=*/32);
}

void av1_fwd_txfm2d_32x8_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm8_xn_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_highbd_txfm32_x4_arr[tx_type];
  int bitcol = av1_fwd_cos_bit_col[3][1];
  int bitrow = av1_fwd_cos_bit_row[3][1];

  // Column-wise transform.
  int32x4_t buf0[64];
  col_txfm(input, buf0, stride, bitcol, /*lr_flip=*/0, /*howmany=*/8,
           /*hm_stride=*/8);
  shift_right_2_round_s32_x4(buf0, buf0, 64);

  int32x4_t buf1[64];
  transpose_arrays_s32_32x8(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bitrow, /*howmany=*/2, /*hm_stride=*/32, /*stride=*/8);
}
#endif

void av1_fwd_txfm2d_4x8_neon(const int16_t *input, int32_t *coeff, int stride,
                             TX_TYPE tx_type, int bd) {
  (void)bd;
  int bitcol = av1_fwd_cos_bit_col[0][1];
  int bitrow = av1_fwd_cos_bit_row[0][1];
  const fwd_transform_1d_col_neon col_txfm = col_highbd_txfm8_x4_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_rect_highbd_txfm4_xn_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 8);

  // Column-wise transform.
  int32x4_t buf0[8];
  col_txfm(input, buf0, stride, bitcol, lr_flip);
  shift_right_1_round_s32_x4(buf0, buf0, 8);

  int32x4_t buf1[8];
  transpose_arrays_s32_4x8(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bitrow, /*howmany=*/2, /*hm_stride=*/4, /*stride=*/8);
}

void av1_fwd_txfm2d_8x4_neon(const int16_t *input, int32_t *coeff, int stride,
                             TX_TYPE tx_type, int bd) {
  (void)bd;
  const int bitcol = av1_fwd_cos_bit_col[1][0];
  const int bitrow = av1_fwd_cos_bit_row[1][0];
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm4_xn_arr[tx_type];
  const fwd_transform_1d_row_neon row_txfm = row_highbd_txfm8_x4_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 4);

  // Column-wise transform.
  int32x4_t buf0[8];
  if (lr_flip) {
    col_txfm(input, buf0 + 4, stride, bitcol, /*lr_flip=*/1, /*howmany=*/2,
             /*hm_stride=*/-4);
  } else {
    col_txfm(input, buf0, stride, bitcol, /*lr_flip=*/0, /*howmany=*/2,
             /*hm_stride=*/4);
  }

  shift_right_1_round_s32_x4(buf0, buf0, 8);

  int32x4_t buf1[8];
  transpose_arrays_s32_8x4(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, coeff, bitrow, /*stride=*/4);
}

#if !CONFIG_REALTIME_ONLY
void av1_fwd_txfm2d_16x64_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)bd;
  const int bitcol = av1_fwd_cos_bit_col[2][4];
  const int bitrow = av1_fwd_cos_bit_row[2][4];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 64);

  // Column-wise transform.
  int32x4_t buf0[256];
  load_buffer_16x64(input, buf0, stride, lr_flip);
  for (int i = 0; i < 4; i++) {
    highbd_fdct64_x4_neon(buf0 + i * 64, buf0 + i * 64, bitcol);
  }
  shift_right_2_round_s32_x4(buf0, buf0, 256);

  int32x4_t buf1[256];
  transpose_arrays_s32_16x64(buf0, buf1);

  // Row-wise transform.
  highbd_fdct16_xn_neon(buf1, buf1, bitrow, 8);
  store_buffer_16x32(buf1, coeff, /*stride=*/32);
}

void av1_fwd_txfm2d_64x16_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)bd;
  const int bitcol = av1_fwd_cos_bit_col[4][2];
  const int bitrow = av1_fwd_cos_bit_row[4][2];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  ud_adjust_input_and_stride(ud_flip, &input, &stride, 16);

  // Column-wise transform.
  int32x4_t buf0[256];
  load_buffer_64x16(input, buf0, stride, lr_flip);
  highbd_fdct16_xn_neon(buf0, buf0, bitcol, 16);
  shift_right_4_round_s32_x4(buf0, buf0, 256);

  int32x4_t buf1[256];
  transpose_arrays_s32_64x16(buf0, buf1);

  // Row-wise transform.
  for (int i = 0; i < 4; i++) {
    highbd_fdct64_x4_neon(buf1 + i * 64, buf1 + i * 64, bitrow);
  }
  store_buffer_64x16(buf1, coeff, /*stride=*/16);
  memset(coeff + 16 * 32, 0, 16 * 32 * sizeof(*coeff));
}
#endif

void av1_fwd_txfm2d_32x32_neon(const int16_t *input, int32_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  const fwd_transform_1d_col_many_neon col_txfm =
      col_highbd_txfm32_x4_arr[tx_type];
  const fwd_transform_1d_row_many_neon row_txfm =
      row_highbd_txfm32_x4_arr[tx_type];

  // Column-wise transform.
  int32x4_t buf0[256];
  col_txfm(input, buf0, stride, /*cos_bit=*/12, /*lr_flip=*/0, /*howmany=*/8,
           /*hm_stride=*/32);
  shift_right_4_round_s32_x4(buf0, buf0, 256);

  int32x4_t buf1[256];
  transpose_arrays_s32_32x32(buf0, buf1);

  // Row-wise transform.
  row_txfm(buf1, output, /*cos_bit=*/12, /*howmany=*/8, /*hm_stride=*/32,
           /*stride=*/32);
}

void av1_fwd_txfm2d_64x64_neon(const int16_t *input, int32_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;

  // Column-wise transform.
  int32x4_t buf0[1024];
  load_buffer_64x64(input, buf0, stride, 0);
  for (int col = 0; col < 16; col++) {
    highbd_fdct64_x4_neon(buf0 + col * 64, buf0 + col * 64, 13);
  }
  shift_right_2_round_s32_x4(buf0, buf0, 1024);

  int32x4_t buf1[1024];
  transpose_arrays_s32_64x64(buf0, buf1);

  // Row-wise transform.
  for (int col = 0; col < 8; col++) {
    highbd_fdct64_x4_neon(buf1 + col * 64, buf1 + col * 64, 10);
  }
  shift_right_2_round_s32_x4(buf1, buf1, 512);
  store_buffer_64x32(buf1, output, /*stride=*/32);
}
