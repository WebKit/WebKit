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

#include "av1/common/av1_txfm.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"
#include "config/av1_rtcd.h"
#include "config/aom_config.h"

static INLINE void store_output_w4(int32_t *const out,
                                   const int32x4_t *const in, const int stride,
                                   const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    vst1q_s32(out + i * stride, in[i]);
  }
}

static INLINE int32x4_t half_btf_neon(const int32_t *w0, const int32x4_t *n0,
                                      const int32_t *w1, const int32x4_t *n1,
                                      const int32x4_t v_bit) {
  int32x4_t x;
  x = vmulq_n_s32(*n0, *w0);
  x = vmlaq_n_s32(x, *n1, *w1);
  x = vrshlq_s32(x, v_bit);
  return x;
}

static INLINE int32x4_t half_btf_neon_m(const int32_t *w0, const int32x4_t *n0,
                                        const int32_t *w1, const int32x4_t *n1,
                                        const int32x4_t v_bit) {
  int32x4_t x;
  x = vmulq_n_s32(*n0, *w0);
  x = vmlsq_n_s32(x, *n1, *w1);
  x = vrshlq_s32(x, v_bit);
  return x;
}

#if defined(__aarch64__)
#define TRANSPOSE_4X4(x0, x1, x2, x3, y0, y1, y2, y3)         \
  do {                                                        \
    int32x4x2_t swap_low = vtrnq_s32(x0, x1);                 \
    int32x4x2_t swap_high = vtrnq_s32(x2, x3);                \
    y0 = vreinterpretq_s32_s64(                               \
        vzip1q_s64(vreinterpretq_s64_s32(swap_low.val[0]),    \
                   vreinterpretq_s64_s32(swap_high.val[0]))); \
    y1 = vreinterpretq_s32_s64(                               \
        vzip1q_s64(vreinterpretq_s64_s32(swap_low.val[1]),    \
                   vreinterpretq_s64_s32(swap_high.val[1]))); \
    y2 = vreinterpretq_s32_s64(                               \
        vzip2q_s64(vreinterpretq_s64_s32(swap_low.val[0]),    \
                   vreinterpretq_s64_s32(swap_high.val[0]))); \
    y3 = vreinterpretq_s32_s64(                               \
        vzip2q_s64(vreinterpretq_s64_s32(swap_low.val[1]),    \
                   vreinterpretq_s64_s32(swap_high.val[1]))); \
  } while (0)
#else
#define TRANSPOSE_4X4(x0, x1, x2, x3, y0, y1, y2, y3)                    \
  do {                                                                   \
    int32x4x2_t swap_low = vtrnq_s32(x0, x1);                            \
    int32x4x2_t swap_high = vtrnq_s32(x2, x3);                           \
    y0 = vextq_s32(vextq_s32(swap_low.val[0], swap_low.val[0], 2),       \
                   swap_high.val[0], 2);                                 \
    y1 = vextq_s32(vextq_s32(swap_low.val[1], swap_low.val[1], 2),       \
                   swap_high.val[1], 2);                                 \
    y2 = vextq_s32(swap_low.val[0],                                      \
                   vextq_s32(swap_high.val[0], swap_high.val[0], 2), 2); \
    y3 = vextq_s32(swap_low.val[1],                                      \
                   vextq_s32(swap_high.val[1], swap_high.val[1], 2), 2); \
  } while (0)
#endif  // (__aarch64__)

static INLINE void transpose_4x4(const int32x4_t *in, int32x4_t *out) {
  TRANSPOSE_4X4(in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]);
}

static INLINE void transpose_8x8(const int32x4_t *in, int32x4_t *out) {
  TRANSPOSE_4X4(in[0], in[2], in[4], in[6], out[0], out[2], out[4], out[6]);
  TRANSPOSE_4X4(in[1], in[3], in[5], in[7], out[8], out[10], out[12], out[14]);
  TRANSPOSE_4X4(in[8], in[10], in[12], in[14], out[1], out[3], out[5], out[7]);
  TRANSPOSE_4X4(in[9], in[11], in[13], in[15], out[9], out[11], out[13],
                out[15]);
}

static INLINE void transpose_16x16(const int32x4_t *in, int32x4_t *out) {
  // Upper left 8x8
  TRANSPOSE_4X4(in[0], in[4], in[8], in[12], out[0], out[4], out[8], out[12]);
  TRANSPOSE_4X4(in[1], in[5], in[9], in[13], out[16], out[20], out[24],
                out[28]);
  TRANSPOSE_4X4(in[16], in[20], in[24], in[28], out[1], out[5], out[9],
                out[13]);
  TRANSPOSE_4X4(in[17], in[21], in[25], in[29], out[17], out[21], out[25],
                out[29]);

  // Upper right 8x8
  TRANSPOSE_4X4(in[2], in[6], in[10], in[14], out[32], out[36], out[40],
                out[44]);
  TRANSPOSE_4X4(in[3], in[7], in[11], in[15], out[48], out[52], out[56],
                out[60]);
  TRANSPOSE_4X4(in[18], in[22], in[26], in[30], out[33], out[37], out[41],
                out[45]);
  TRANSPOSE_4X4(in[19], in[23], in[27], in[31], out[49], out[53], out[57],
                out[61]);

  // Lower left 8x8
  TRANSPOSE_4X4(in[32], in[36], in[40], in[44], out[2], out[6], out[10],
                out[14]);
  TRANSPOSE_4X4(in[33], in[37], in[41], in[45], out[18], out[22], out[26],
                out[30]);
  TRANSPOSE_4X4(in[48], in[52], in[56], in[60], out[3], out[7], out[11],
                out[15]);
  TRANSPOSE_4X4(in[49], in[53], in[57], in[61], out[19], out[23], out[27],
                out[31]);
  // Lower right 8x8
  TRANSPOSE_4X4(in[34], in[38], in[42], in[46], out[34], out[38], out[42],
                out[46]);
  TRANSPOSE_4X4(in[35], in[39], in[43], in[47], out[50], out[54], out[58],
                out[62]);
  TRANSPOSE_4X4(in[50], in[54], in[58], in[62], out[35], out[39], out[43],
                out[47]);
  TRANSPOSE_4X4(in[51], in[55], in[59], in[63], out[51], out[55], out[59],
                out[63]);
}

static INLINE void av1_round_shift_rect_array_32_neon(int32x4_t *input,
                                                      int32x4_t *output,
                                                      const int size,
                                                      const int bit,
                                                      const int val) {
  const int32x4_t sqrt2 = vdupq_n_s32(val);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  int i;
  for (i = 0; i < size; i++) {
    const int32x4_t r0 = vrshlq_s32(input[i], v_bit);
    const int32x4_t r1 = vmulq_s32(sqrt2, r0);
    output[i] = vrshrq_n_s32(r1, NewSqrt2Bits);
  }
}

#define btf_32_neon_type0(w0, w1, in0, in1, out0, out1, v_cos_bit) \
  do {                                                             \
    out0 = vmulq_n_s32(in0, w0);                                   \
    out0 = vmlaq_n_s32(out0, in1, w1);                             \
    out0 = vrshlq_s32(out0, v_cos_bit);                            \
    out1 = vmulq_n_s32(in0, w1);                                   \
    out1 = vmlsq_n_s32(out1, in1, w0);                             \
    out1 = vrshlq_s32(out1, v_cos_bit);                            \
  } while (0)

#define btf_32_neon_type1(w0, w1, in0, in1, out0, out1, bit) \
  do {                                                       \
    btf_32_neon_type0(w1, w0, in1, in0, out0, out1, bit);    \
  } while (0)

static INLINE void load_buffer_4x4(const int16_t *input, int32x4_t *in,
                                   int stride, int flipud, int fliplr,
                                   const int32x4_t *v_shift) {
  int16x4_t v0, v1, v2, v3;

  if (!flipud) {
    v0 = vld1_s16(input + 0 * stride);
    v1 = vld1_s16(input + 1 * stride);
    v2 = vld1_s16(input + 2 * stride);
    v3 = vld1_s16(input + 3 * stride);
  } else {
    v0 = vld1_s16(input + 3 * stride);
    v1 = vld1_s16(input + 2 * stride);
    v2 = vld1_s16(input + 1 * stride);
    v3 = vld1_s16(input + 0 * stride);
  }

  if (fliplr) {
    v0 = vrev64_s16(v0);
    v1 = vrev64_s16(v1);
    v2 = vrev64_s16(v2);
    v3 = vrev64_s16(v3);
  }
  in[0] = vshlq_s32(vmovl_s16(v0), *v_shift);
  in[1] = vshlq_s32(vmovl_s16(v1), *v_shift);
  in[2] = vshlq_s32(vmovl_s16(v2), *v_shift);
  in[3] = vshlq_s32(vmovl_s16(v3), *v_shift);
}

static void fdct4x4_neon(int32x4_t *in, int32x4_t *out, int bit,
                         const int num_col) {
  const int32_t *cospi = cospi_arr(bit);
  const int32x4_t cospi32 = vdupq_n_s32(cospi[32]);
  const int32x4_t cospi48 = vdupq_n_s32(cospi[48]);
  const int32x4_t cospi16 = vdupq_n_s32(cospi[16]);
  int32x4_t s0, s1, s2, s3;
  int32x4_t u0, u1, u2, u3;
  int32x4_t v0, v2;

  int endidx = 3 * num_col;
  s0 = vaddq_s32(in[0], in[endidx]);
  s3 = vsubq_s32(in[0], in[endidx]);
  endidx -= num_col;
  s1 = vaddq_s32(in[num_col], in[endidx]);
  s2 = vsubq_s32(in[num_col], in[endidx]);

  u0 = vmulq_s32(s0, cospi32);
  u1 = vmulq_s32(s1, cospi32);
  u2 = vaddq_s32(u0, u1);
  v0 = vsubq_s32(u0, u1);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  u0 = vrshlq_s32(u2, v_bit);
  u2 = vrshlq_s32(v0, v_bit);

  v0 = vmulq_s32(s2, cospi48);
  v2 = vmlaq_s32(v0, s3, cospi16);

  u1 = vrshlq_s32(v2, v_bit);

  v0 = vmulq_s32(s3, cospi48);
  v2 = vmlsq_s32(v0, s2, cospi16);

  u3 = vrshlq_s32(v2, v_bit);

  out[0] = u0;
  out[1] = u1;
  out[2] = u2;
  out[3] = u3;
}

static INLINE void write_buffer_4x4(int32x4_t *res, int32_t *output) {
  vst1q_s32((output + 0 * 4), res[0]);
  vst1q_s32((output + 1 * 4), res[1]);
  vst1q_s32((output + 2 * 4), res[2]);
  vst1q_s32((output + 3 * 4), res[3]);
}

static void fadst4x4_neon(int32x4_t *in, int32x4_t *out, int bit,
                          const int num_col) {
  const int32_t *sinpi = sinpi_arr(bit);
  const int32x4_t sinpi4x = vld1q_s32(&sinpi[1]);

  const int32x4_t sinpi1 = vdupq_lane_s32(vget_low_s32(sinpi4x), 0);
  const int32x4_t sinpi2 = vdupq_lane_s32(vget_low_s32(sinpi4x), 1);
  const int32x4_t sinpi3 = vdupq_lane_s32(vget_high_s32(sinpi4x), 0);
  const int32x4_t sinpi4 = vdupq_lane_s32(vget_high_s32(sinpi4x), 1);
  int32x4_t t;
  int32x4_t s0, s1, s2, s3, s7;
  int32x4_t x0, x1, x2, x3;

  int idx = 0 * num_col;
  s0 = vmulq_s32(in[idx], sinpi1);
  s1 = vmulq_s32(in[idx], sinpi4);
  t = vaddq_s32(in[idx], in[idx + num_col]);
  idx += 2 * num_col;
  x3 = vmulq_s32(in[idx], sinpi3);
  idx += num_col;
  s7 = vsubq_s32(t, in[idx]);

  t = vmlaq_s32(s0, in[idx - 2 * num_col], sinpi2);
  x0 = vmlaq_s32(t, in[idx], sinpi4);
  x1 = vmulq_s32(s7, sinpi3);
  t = vmlsq_s32(s1, in[idx - 2 * num_col], sinpi1);
  x2 = vmlaq_s32(t, in[idx], sinpi2);

  s0 = vaddq_s32(x0, x3);
  s1 = x1;
  s2 = vsubq_s32(x2, x3);
  t = vsubq_s32(x2, x0);
  s3 = vaddq_s32(t, x3);

  const int32x4_t v_bit = vdupq_n_s32(-bit);
  out[0] = vrshlq_s32(s0, v_bit);
  out[1] = vrshlq_s32(s1, v_bit);
  out[2] = vrshlq_s32(s2, v_bit);
  out[3] = vrshlq_s32(s3, v_bit);
}
static void idtx4x4_neon(int32x4_t *in, int32x4_t *out, int bit, int col_num) {
  (void)bit;
  int32x4_t fact = vdupq_n_s32(NewSqrt2);
  int32x4_t a_low;

  int i;
  for (i = 0; i < 4; i++) {
    a_low = vmulq_s32(in[i * col_num], fact);
    out[i] = vrshrq_n_s32(a_low, NewSqrt2Bits);
  }
}
void av1_fwd_txfm2d_4x4_neon(const int16_t *input, int32_t *coeff,
                             int input_stride, TX_TYPE tx_type, int bd) {
  int32x4_t in[4];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X4];
  const int txw_idx = get_txw_idx(TX_4X4);
  const int txh_idx = get_txh_idx(TX_4X4);
  int32x4_t v_shift0 = vdupq_n_s32(shift[0]);
  switch (tx_type) {
    case DCT_DCT:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      fdct4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fdct4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case ADST_DCT:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fdct4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case DCT_ADST:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      fdct4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case ADST_ADST:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case FLIPADST_DCT:
      load_buffer_4x4(input, in, input_stride, 1, 0, &v_shift0);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fdct4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case DCT_FLIPADST:
      load_buffer_4x4(input, in, input_stride, 0, 1, &v_shift0);
      fdct4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_4x4(input, in, input_stride, 1, 1, &v_shift0);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case ADST_FLIPADST:
      load_buffer_4x4(input, in, input_stride, 0, 1, &v_shift0);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case FLIPADST_ADST:
      load_buffer_4x4(input, in, input_stride, 1, 0, &v_shift0);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case IDTX:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      idtx4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      idtx4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case V_DCT:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      fdct4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      idtx4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case H_DCT:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      idtx4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fdct4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case V_ADST:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      idtx4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case H_ADST:
      load_buffer_4x4(input, in, input_stride, 0, 0, &v_shift0);
      idtx4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case V_FLIPADST:
      load_buffer_4x4(input, in, input_stride, 1, 0, &v_shift0);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      idtx4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    case H_FLIPADST:
      load_buffer_4x4(input, in, input_stride, 0, 1, &v_shift0);
      idtx4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      transpose_4x4(in, in);
      fadst4x4_neon(in, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], 1);
      write_buffer_4x4(in, coeff);
      break;
    default: assert(0);
  }
  (void)bd;
}

static INLINE void load_buffer_8x8(const int16_t *input, int32x4_t *in,
                                   int stride, int flipud, int fliplr,
                                   const int shift) {
  if (!flipud) {
    in[0] = vreinterpretq_s32_s16(vld1q_s16((input + 0 * stride)));
    in[1] = vreinterpretq_s32_s16(vld1q_s16((input + 1 * stride)));
    in[2] = vreinterpretq_s32_s16(vld1q_s16((input + 2 * stride)));
    in[3] = vreinterpretq_s32_s16(vld1q_s16((input + 3 * stride)));
    in[4] = vreinterpretq_s32_s16(vld1q_s16((input + 4 * stride)));
    in[5] = vreinterpretq_s32_s16(vld1q_s16((input + 5 * stride)));
    in[6] = vreinterpretq_s32_s16(vld1q_s16((input + 6 * stride)));
    in[7] = vreinterpretq_s32_s16(vld1q_s16((input + 7 * stride)));
  } else {
    in[0] = vreinterpretq_s32_s16(vld1q_s16((input + 7 * stride)));
    in[1] = vreinterpretq_s32_s16(vld1q_s16((input + 6 * stride)));
    in[2] = vreinterpretq_s32_s16(vld1q_s16((input + 5 * stride)));
    in[3] = vreinterpretq_s32_s16(vld1q_s16((input + 4 * stride)));
    in[4] = vreinterpretq_s32_s16(vld1q_s16((input + 3 * stride)));
    in[5] = vreinterpretq_s32_s16(vld1q_s16((input + 2 * stride)));
    in[6] = vreinterpretq_s32_s16(vld1q_s16((input + 1 * stride)));
    in[7] = vreinterpretq_s32_s16(vld1q_s16((input + 0 * stride)));
  }

  if (fliplr) {
    in[0] = vreinterpretq_s32_s16(vrev64q_s16(vreinterpretq_s16_s32(in[0])));
    in[0] = vextq_s32(in[0], in[0], 2);
    in[1] = vreinterpretq_s32_s16(vrev64q_s16(vreinterpretq_s16_s32(in[1])));
    in[1] = vextq_s32(in[1], in[1], 2);
    in[2] = vreinterpretq_s32_s16(vrev64q_s16(vreinterpretq_s16_s32(in[2])));
    in[2] = vextq_s32(in[2], in[2], 2);
    in[3] = vreinterpretq_s32_s16(vrev64q_s16(vreinterpretq_s16_s32(in[3])));
    in[3] = vextq_s32(in[3], in[3], 2);
    in[4] = vreinterpretq_s32_s16(vrev64q_s16(vreinterpretq_s16_s32(in[4])));
    in[4] = vextq_s32(in[4], in[4], 2);
    in[5] = vreinterpretq_s32_s16(vrev64q_s16(vreinterpretq_s16_s32(in[5])));
    in[5] = vextq_s32(in[5], in[5], 2);
    in[6] = vreinterpretq_s32_s16(vrev64q_s16(vreinterpretq_s16_s32(in[6])));
    in[6] = vextq_s32(in[6], in[6], 2);
    in[7] = vreinterpretq_s32_s16(vrev64q_s16(vreinterpretq_s16_s32(in[7])));
    in[7] = vextq_s32(in[7], in[7], 2);
  }

  int16x4_t u = vget_high_s16(vreinterpretq_s16_s32(in[4]));
  in[8] = vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(in[4])));
  in[9] = vmovl_s16(u);

  u = vget_high_s16(vreinterpretq_s16_s32(in[5]));
  in[10] = vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(in[5])));
  in[11] = vmovl_s16(u);

  u = vget_high_s16(vreinterpretq_s16_s32(in[6]));
  in[12] = vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(in[6])));
  in[13] = vmovl_s16(u);

  u = vget_high_s16(vreinterpretq_s16_s32(in[7]));
  in[14] = vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(in[7])));
  in[15] = vmovl_s16(u);

  u = vget_high_s16(vreinterpretq_s16_s32(in[3]));
  in[6] = vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(in[3])));
  in[7] = vmovl_s16(u);

  u = vget_high_s16(vreinterpretq_s16_s32(in[2]));
  in[4] = vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(in[2])));
  in[5] = vmovl_s16(u);

  u = vget_high_s16(vreinterpretq_s16_s32(in[1]));
  in[2] = vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(in[1])));
  in[3] = vmovl_s16(u);

  u = vget_high_s16(vreinterpretq_s16_s32(in[0]));
  in[0] = vmovl_s16(vget_low_s16(vreinterpretq_s16_s32(in[0])));
  in[1] = vmovl_s16(u);

  const int32x4_t v_shift = vdupq_n_s32(shift);

  in[0] = vshlq_s32(in[0], v_shift);
  in[1] = vshlq_s32(in[1], v_shift);
  in[2] = vshlq_s32(in[2], v_shift);
  in[3] = vshlq_s32(in[3], v_shift);
  in[4] = vshlq_s32(in[4], v_shift);
  in[5] = vshlq_s32(in[5], v_shift);
  in[6] = vshlq_s32(in[6], v_shift);
  in[7] = vshlq_s32(in[7], v_shift);

  in[8] = vshlq_s32(in[8], v_shift);
  in[9] = vshlq_s32(in[9], v_shift);
  in[10] = vshlq_s32(in[10], v_shift);
  in[11] = vshlq_s32(in[11], v_shift);
  in[12] = vshlq_s32(in[12], v_shift);
  in[13] = vshlq_s32(in[13], v_shift);
  in[14] = vshlq_s32(in[14], v_shift);
  in[15] = vshlq_s32(in[15], v_shift);
}

static INLINE void col_txfm_8x8_rounding(int32x4_t *in,
                                         const int32x4_t *v_shift) {
  in[0] = vrshlq_s32(in[0], *v_shift);
  in[1] = vrshlq_s32(in[1], *v_shift);
  in[2] = vrshlq_s32(in[2], *v_shift);
  in[3] = vrshlq_s32(in[3], *v_shift);
  in[4] = vrshlq_s32(in[4], *v_shift);
  in[5] = vrshlq_s32(in[5], *v_shift);
  in[6] = vrshlq_s32(in[6], *v_shift);
  in[7] = vrshlq_s32(in[7], *v_shift);
  in[8] = vrshlq_s32(in[8], *v_shift);
  in[9] = vrshlq_s32(in[9], *v_shift);
  in[10] = vrshlq_s32(in[10], *v_shift);
  in[11] = vrshlq_s32(in[11], *v_shift);
  in[12] = vrshlq_s32(in[12], *v_shift);
  in[13] = vrshlq_s32(in[13], *v_shift);
  in[14] = vrshlq_s32(in[14], *v_shift);
  in[15] = vrshlq_s32(in[15], *v_shift);
}

static INLINE void col_txfm_4x8_rounding(int32x4_t *in,
                                         const int32x4_t *v_shift) {
  in[0] = vrshlq_s32(in[0], *v_shift);
  in[1] = vrshlq_s32(in[1], *v_shift);
  in[2] = vrshlq_s32(in[2], *v_shift);
  in[3] = vrshlq_s32(in[3], *v_shift);
  in[4] = vrshlq_s32(in[4], *v_shift);
  in[5] = vrshlq_s32(in[5], *v_shift);
  in[6] = vrshlq_s32(in[6], *v_shift);
  in[7] = vrshlq_s32(in[7], *v_shift);
}

static INLINE void write_buffer_8x8(const int32x4_t *res, int32_t *output) {
  vst1q_s32(output + 0 * 4, res[0]);
  vst1q_s32(output + 1 * 4, res[1]);
  vst1q_s32(output + 2 * 4, res[2]);
  vst1q_s32(output + 3 * 4, res[3]);

  vst1q_s32(output + 4 * 4, res[4]);
  vst1q_s32(output + 5 * 4, res[5]);
  vst1q_s32(output + 6 * 4, res[6]);
  vst1q_s32(output + 7 * 4, res[7]);

  vst1q_s32(output + 8 * 4, res[8]);
  vst1q_s32(output + 9 * 4, res[9]);
  vst1q_s32(output + 10 * 4, res[10]);
  vst1q_s32(output + 11 * 4, res[11]);

  vst1q_s32(output + 12 * 4, res[12]);
  vst1q_s32(output + 13 * 4, res[13]);
  vst1q_s32(output + 14 * 4, res[14]);
  vst1q_s32(output + 15 * 4, res[15]);
}

static INLINE void write_buffer_16x8(const int32x4_t *res, int32_t *output,
                                     const int stride) {
  vst1q_s32(output, res[0]);
  vst1q_s32(output + 4, res[1]);
  vst1q_s32(output + stride, res[2]);
  vst1q_s32(output + stride + 4, res[3]);

  vst1q_s32(output + (stride * 2), res[4]);
  vst1q_s32(output + (stride * 2) + 4, res[5]);
  vst1q_s32(output + (stride * 3), res[6]);
  vst1q_s32(output + (stride * 3) + 4, res[7]);

  vst1q_s32(output + (stride * 4), res[8]);
  vst1q_s32(output + (stride * 4) + 4, res[9]);
  vst1q_s32(output + (stride * 5), res[10]);
  vst1q_s32(output + (stride * 5) + 4, res[11]);

  vst1q_s32(output + (stride * 6), res[12]);
  vst1q_s32(output + (stride * 6) + 4, res[13]);
  vst1q_s32(output + (stride * 7), res[14]);
  vst1q_s32(output + (stride * 7) + 4, res[15]);
}

static void fdct4x8_neon(int32x4_t *in, int32x4_t *out, int bit,
                         const int col_num) {
  const int32_t *cospi = cospi_arr(bit);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  int32x4_t u[8], v[8];

  int startidx = 0 * col_num;
  int endidx = 7 * col_num;
  // stage 0-1
  u[0] = vaddq_s32(in[startidx], in[endidx]);
  v[7] = vsubq_s32(in[startidx], in[endidx]);
  startidx += col_num;
  endidx -= col_num;
  u[1] = vaddq_s32(in[startidx], in[endidx]);
  u[6] = vsubq_s32(in[startidx], in[endidx]);
  startidx += col_num;
  endidx -= col_num;
  u[2] = vaddq_s32(in[startidx], in[endidx]);
  u[5] = vsubq_s32(in[startidx], in[endidx]);
  startidx += col_num;
  endidx -= col_num;
  u[3] = vaddq_s32(in[startidx], in[endidx]);
  v[4] = vsubq_s32(in[startidx], in[endidx]);

  // stage 2
  v[0] = vaddq_s32(u[0], u[3]);
  v[3] = vsubq_s32(u[0], u[3]);
  v[1] = vaddq_s32(u[1], u[2]);
  v[2] = vsubq_s32(u[1], u[2]);

  v[5] = vmulq_n_s32(u[6], cospi[32]);
  v[5] = vmlsq_n_s32(v[5], u[5], cospi[32]);
  v[5] = vrshlq_s32(v[5], v_bit);

  u[0] = vmulq_n_s32(u[5], cospi[32]);
  v[6] = vmlaq_n_s32(u[0], u[6], cospi[32]);
  v[6] = vrshlq_s32(v[6], v_bit);

  // stage 3
  // type 0
  v[0] = vmulq_n_s32(v[0], cospi[32]);
  v[1] = vmulq_n_s32(v[1], cospi[32]);
  u[0] = vaddq_s32(v[0], v[1]);
  u[0] = vrshlq_s32(u[0], v_bit);

  u[1] = vsubq_s32(v[0], v[1]);
  u[1] = vrshlq_s32(u[1], v_bit);

  // type 1
  v[0] = vmulq_n_s32(v[2], cospi[48]);
  u[2] = vmlaq_n_s32(v[0], v[3], cospi[16]);
  u[2] = vrshlq_s32(u[2], v_bit);

  v[1] = vmulq_n_s32(v[3], cospi[48]);
  u[3] = vmlsq_n_s32(v[1], v[2], cospi[16]);
  u[3] = vrshlq_s32(u[3], v_bit);

  u[4] = vaddq_s32(v[4], v[5]);
  u[5] = vsubq_s32(v[4], v[5]);
  u[6] = vsubq_s32(v[7], v[6]);
  u[7] = vaddq_s32(v[7], v[6]);

  // stage 4-5
  v[0] = vmulq_n_s32(u[4], cospi[56]);
  v[0] = vmlaq_n_s32(v[0], u[7], cospi[8]);
  out[1 * col_num] = vrshlq_s32(v[0], v_bit);

  v[1] = vmulq_n_s32(u[7], cospi[56]);
  v[0] = vmlsq_n_s32(v[1], u[4], cospi[8]);
  out[7 * col_num] = vrshlq_s32(v[0], v_bit);

  v[0] = vmulq_n_s32(u[5], cospi[24]);
  v[0] = vmlaq_n_s32(v[0], u[6], cospi[40]);
  out[5 * col_num] = vrshlq_s32(v[0], v_bit);

  v[1] = vmulq_n_s32(u[6], cospi[24]);
  v[0] = vmlsq_n_s32(v[1], u[5], cospi[40]);
  out[3 * col_num] = vrshlq_s32(v[0], v_bit);

  out[0 * col_num] = u[0];
  out[4 * col_num] = u[1];
  out[2 * col_num] = u[2];
  out[6 * col_num] = u[3];
}

static void fdct8x8_neon(int32x4_t *in, int32x4_t *out, int bit,
                         const int col_num) {
  fdct4x8_neon(in, out, bit, col_num);
  fdct4x8_neon(in + 1, out + 1, bit, col_num);
}

static void fadst8x8_neon(int32x4_t *in, int32x4_t *out, int bit,
                          const int col_num) {
  const int32_t *cospi = cospi_arr(bit);

  const int32x4_t v_bit = vdupq_n_s32(-bit);
  int32x4_t u0, u1, u2, u3, u4, u5, u6, u7;
  int32x4_t v0, v1, v2, v3, v4, v5, v6, v7;
  int32x4_t x, y;
  int col;

  for (col = 0; col < col_num; ++col) {
    // stage 0-1
    u0 = in[col_num * 0 + col];
    u1 = vnegq_s32(in[col_num * 7 + col]);
    u2 = vnegq_s32(in[col_num * 3 + col]);
    u3 = in[col_num * 4 + col];
    u4 = vnegq_s32(in[col_num * 1 + col]);
    u5 = in[col_num * 6 + col];
    u6 = in[col_num * 2 + col];
    u7 = vnegq_s32(in[col_num * 5 + col]);

    // stage 2
    v0 = u0;
    v1 = u1;

    x = vmulq_n_s32(u2, cospi[32]);
    y = vmulq_n_s32(u3, cospi[32]);
    v2 = vaddq_s32(x, y);
    v2 = vrshlq_s32(v2, v_bit);

    v3 = vsubq_s32(x, y);
    v3 = vrshlq_s32(v3, v_bit);

    v4 = u4;
    v5 = u5;

    x = vmulq_n_s32(u6, cospi[32]);
    y = vmulq_n_s32(u7, cospi[32]);
    v6 = vaddq_s32(x, y);
    v6 = vrshlq_s32(v6, v_bit);

    v7 = vsubq_s32(x, y);
    v7 = vrshlq_s32(v7, v_bit);

    // stage 3
    u0 = vaddq_s32(v0, v2);
    u1 = vaddq_s32(v1, v3);
    u2 = vsubq_s32(v0, v2);
    u3 = vsubq_s32(v1, v3);
    u4 = vaddq_s32(v4, v6);
    u5 = vaddq_s32(v5, v7);
    u6 = vsubq_s32(v4, v6);
    u7 = vsubq_s32(v5, v7);

    // stage 4
    v0 = u0;
    v1 = u1;
    v2 = u2;
    v3 = u3;

    v4 = vmulq_n_s32(u4, cospi[16]);
    v4 = vmlaq_n_s32(v4, u5, cospi[48]);
    v4 = vrshlq_s32(v4, v_bit);

    v5 = vmulq_n_s32(u4, cospi[48]);
    v5 = vmlsq_n_s32(v5, u5, cospi[16]);
    v5 = vrshlq_s32(v5, v_bit);

    v6 = vmulq_n_s32(u7, cospi[16]);
    v6 = vmlsq_n_s32(v6, u6, cospi[48]);
    v6 = vrshlq_s32(v6, v_bit);

    v7 = vmulq_n_s32(u6, cospi[16]);
    v7 = vmlaq_n_s32(v7, u7, cospi[48]);
    v7 = vrshlq_s32(v7, v_bit);

    // stage 5
    u0 = vaddq_s32(v0, v4);
    u1 = vaddq_s32(v1, v5);
    u2 = vaddq_s32(v2, v6);
    u3 = vaddq_s32(v3, v7);
    u4 = vsubq_s32(v0, v4);
    u5 = vsubq_s32(v1, v5);
    u6 = vsubq_s32(v2, v6);
    u7 = vsubq_s32(v3, v7);

    // stage 6
    v0 = vmulq_n_s32(u0, cospi[4]);
    v0 = vmlaq_n_s32(v0, u1, cospi[60]);
    v0 = vrshlq_s32(v0, v_bit);

    v1 = vmulq_n_s32(u0, cospi[60]);
    v1 = vmlsq_n_s32(v1, u1, cospi[4]);
    v1 = vrshlq_s32(v1, v_bit);

    v2 = vmulq_n_s32(u2, cospi[20]);
    v2 = vmlaq_n_s32(v2, u3, cospi[44]);
    v2 = vrshlq_s32(v2, v_bit);

    v3 = vmulq_n_s32(u2, cospi[44]);
    v3 = vmlsq_n_s32(v3, u3, cospi[20]);
    v3 = vrshlq_s32(v3, v_bit);

    v4 = vmulq_n_s32(u4, cospi[36]);
    v4 = vmlaq_n_s32(v4, u5, cospi[28]);
    v4 = vrshlq_s32(v4, v_bit);

    v5 = vmulq_n_s32(u4, cospi[28]);
    v5 = vmlsq_n_s32(v5, u5, cospi[36]);
    v5 = vrshlq_s32(v5, v_bit);

    x = vmulq_n_s32(u6, cospi[52]);
    v6 = vmlaq_n_s32(x, u7, cospi[12]);
    v6 = vrshlq_s32(v6, v_bit);

    v7 = vmulq_n_s32(u6, cospi[12]);
    v7 = vmlsq_n_s32(v7, u7, cospi[52]);
    v7 = vrshlq_s32(v7, v_bit);

    // stage 7
    out[col_num * 0 + col] = v1;
    out[col_num * 1 + col] = v6;
    out[col_num * 2 + col] = v3;
    out[col_num * 3 + col] = v4;
    out[col_num * 4 + col] = v5;
    out[col_num * 5 + col] = v2;
    out[col_num * 6 + col] = v7;
    out[col_num * 7 + col] = v0;
  }
}
static void idtx8x8_neon(int32x4_t *in, int32x4_t *out, int bit, int col_num) {
  (void)bit;

  for (int i = 0; i < col_num; i += 1) {
    out[0 + 8 * i] = vshlq_n_s32(in[0 + 8 * i], 1);
    out[1 + 8 * i] = vshlq_n_s32(in[1 + 8 * i], 1);
    out[2 + 8 * i] = vshlq_n_s32(in[2 + 8 * i], 1);
    out[3 + 8 * i] = vshlq_n_s32(in[3 + 8 * i], 1);
    out[4 + 8 * i] = vshlq_n_s32(in[4 + 8 * i], 1);
    out[5 + 8 * i] = vshlq_n_s32(in[5 + 8 * i], 1);
    out[6 + 8 * i] = vshlq_n_s32(in[6 + 8 * i], 1);
    out[7 + 8 * i] = vshlq_n_s32(in[7 + 8 * i], 1);
  }
}
#if !CONFIG_REALTIME_ONLY
static void idtx32x8_neon(int32x4_t *in, int32x4_t *out, int bit, int col_num) {
  (void)bit;
  (void)col_num;
  for (int j = 0; j < 2; j++) {
    out[j + 8 * 0] = vshlq_n_s32(in[j + 8 * 0], 1);
    out[j + 8 * 1] = vshlq_n_s32(in[j + 8 * 1], 1);
    out[j + 8 * 2] = vshlq_n_s32(in[j + 8 * 2], 1);
    out[j + 8 * 3] = vshlq_n_s32(in[j + 8 * 3], 1);
    out[j + 8 * 4] = vshlq_n_s32(in[j + 8 * 4], 1);
    out[j + 8 * 5] = vshlq_n_s32(in[j + 8 * 5], 1);
    out[j + 8 * 6] = vshlq_n_s32(in[j + 8 * 6], 1);
    out[j + 8 * 7] = vshlq_n_s32(in[j + 8 * 7], 1);
  }
}
#endif
void av1_fwd_txfm2d_8x8_neon(const int16_t *input, int32_t *coeff, int stride,
                             TX_TYPE tx_type, int bd) {
  int32x4_t in[16], out[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X8];
  const int txw_idx = get_txw_idx(TX_8X8);
  const int txh_idx = get_txh_idx(TX_8X8);
  const int32x4_t v_shift1 = vdupq_n_s32(shift[1]);
  switch (tx_type) {
    case DCT_DCT:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fdct8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fdct8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case ADST_DCT:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fdct8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case DCT_ADST:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fdct8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case ADST_ADST:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case FLIPADST_DCT:
      load_buffer_8x8(input, in, stride, 1, 0, shift[0]);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fdct8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case DCT_FLIPADST:
      load_buffer_8x8(input, in, stride, 0, 1, shift[0]);
      fdct8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_8x8(input, in, stride, 1, 1, shift[0]);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case ADST_FLIPADST:
      load_buffer_8x8(input, in, stride, 0, 1, shift[0]);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case FLIPADST_ADST:
      load_buffer_8x8(input, in, stride, 1, 0, shift[0]);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case IDTX:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      idtx8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      idtx8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case V_DCT:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fdct8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      idtx8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case H_DCT:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      idtx8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fdct8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case V_ADST:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      idtx8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case H_ADST:
      load_buffer_8x8(input, in, stride, 0, 0, shift[0]);
      idtx8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case V_FLIPADST:
      load_buffer_8x8(input, in, stride, 1, 0, shift[0]);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      idtx8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    case H_FLIPADST:
      load_buffer_8x8(input, in, stride, 0, 1, shift[0]);
      idtx8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      col_txfm_8x8_rounding(out, &v_shift1);
      transpose_8x8(out, in);
      fadst8x8_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], 2);
      write_buffer_8x8(out, coeff);
      break;
    default: assert(0);
  }
  (void)bd;
}

// Hybrid Transform 16x16

static INLINE void convert_8x8_to_16x16(const int32x4_t *in, int32x4_t *out) {
  int row_index = 0;
  int dst_index = 0;
  int src_index = 0;

  // row 0, 1, .., 7
  do {
    out[dst_index] = in[src_index];
    out[dst_index + 1] = in[src_index + 1];
    out[dst_index + 2] = in[src_index + 16];
    out[dst_index + 3] = in[src_index + 17];
    dst_index += 4;
    src_index += 2;
    row_index += 1;
  } while (row_index < 8);

  // row 8, 9, ..., 15
  src_index += 16;
  do {
    out[dst_index] = in[src_index];
    out[dst_index + 1] = in[src_index + 1];
    out[dst_index + 2] = in[src_index + 16];
    out[dst_index + 3] = in[src_index + 17];
    dst_index += 4;
    src_index += 2;
    row_index += 1;
  } while (row_index < 16);
}

static INLINE void load_buffer_16x16(const int16_t *input, int32x4_t *out,
                                     int stride, int flipud, int fliplr,
                                     int shift) {
  int32x4_t in[64];
  // Load 4 8x8 blocks
  const int16_t *topL = input;
  const int16_t *topR = input + 8;
  const int16_t *botL = input + 8 * stride;
  const int16_t *botR = input + 8 * stride + 8;

  const int16_t *tmp;

  if (flipud) {
    // Swap left columns
    tmp = topL;
    topL = botL;
    botL = tmp;
    // Swap right columns
    tmp = topR;
    topR = botR;
    botR = tmp;
  }

  if (fliplr) {
    // Swap top rows
    tmp = topL;
    topL = topR;
    topR = tmp;
    // Swap bottom rows
    tmp = botL;
    botL = botR;
    botR = tmp;
  }

  // load first 8 columns
  load_buffer_8x8(topL, &in[0], stride, flipud, fliplr, shift);
  load_buffer_8x8(botL, &in[32], stride, flipud, fliplr, shift);

  // load second 8 columns
  load_buffer_8x8(topR, &in[16], stride, flipud, fliplr, shift);
  load_buffer_8x8(botR, &in[48], stride, flipud, fliplr, shift);

  convert_8x8_to_16x16(in, out);
}

static INLINE void load_buffer_8x16(const int16_t *input, int32x4_t *out,
                                    int stride, int flipud, int fliplr,
                                    int shift) {
  const int16_t *topL = input;
  const int16_t *botL = input + 8 * stride;

  const int16_t *tmp;

  if (flipud) {
    tmp = topL;
    topL = botL;
    botL = tmp;
  }

  load_buffer_8x8(topL, out, stride, flipud, fliplr, shift);
  load_buffer_8x8(botL, out + 16, stride, flipud, fliplr, shift);
}

static INLINE void load_buffer_8x4(const int16_t *input, int32x4_t *out,
                                   int stride, int flipud, int fliplr,
                                   const int32x4_t *v_shift) {
  const int16_t *topL = input;
  const int16_t *topR = input + 4;

  const int16_t *tmp;

  if (fliplr) {
    tmp = topL;
    topL = topR;
    topR = tmp;
  }
  load_buffer_4x4(topL, out, stride, flipud, fliplr, v_shift);
  load_buffer_4x4(topR, out + 4, stride, flipud, fliplr, v_shift);
}

static INLINE void load_buffer_16x4(const int16_t *input, int32x4_t *out,
                                    int stride, int flipud, int fliplr,
                                    const int32x4_t *v_shift) {
  const int16_t *topL = input;
  const int16_t *topR = input + 8;

  const int16_t *tmp;

  if (fliplr) {
    tmp = topL;
    topL = topR;
    topR = tmp;
  }

  load_buffer_8x4(topL, out, stride, flipud, fliplr, v_shift);
  load_buffer_8x4(topR, out + 8, stride, flipud, fliplr, v_shift);
}

static INLINE void load_buffer_4x8(const int16_t *input, int32x4_t *out,
                                   int stride, int flipud, int fliplr,
                                   const int32x4_t *v_shift) {
  const int16_t *topL = input;
  const int16_t *botL = input + 4 * stride;

  const int16_t *tmp;

  if (flipud) {
    tmp = topL;
    topL = botL;
    botL = tmp;
  }

  load_buffer_4x4(topL, out, stride, flipud, fliplr, v_shift);
  load_buffer_4x4(botL, out + 4, stride, flipud, fliplr, v_shift);
}

#if !CONFIG_REALTIME_ONLY
static INLINE void load_buffer_4x16(const int16_t *input, int32x4_t *out,
                                    const int stride, const int flipud,
                                    const int fliplr,
                                    const int32x4_t *v_shift) {
  const int16_t *topL = input;
  const int16_t *botL = input + 8 * stride;

  const int16_t *tmp;

  if (flipud) {
    tmp = topL;
    topL = botL;
    botL = tmp;
  }
  load_buffer_4x8(topL, out, stride, flipud, fliplr, v_shift);
  load_buffer_4x8(botL, out + 8, stride, flipud, fliplr, v_shift);
}
#endif

static INLINE void load_buffer_32x8n(const int16_t *input, int32x4_t *out,
                                     int stride, int flipud, int fliplr,
                                     int shift, const int height) {
  const int16_t *in = input;
  int32x4_t *output = out;
  for (int col = 0; col < height; col++) {
    in = input + col * stride;
    output = out + col * 8;
    int32x4_t v_shift = vdupq_n_s32(shift);
    load_buffer_4x4(in, output, 4, flipud, fliplr, &v_shift);
    load_buffer_4x4((in + 16), (output + 4), 4, flipud, fliplr, &v_shift);
  }
}

static void fdct16x16_neon(int32x4_t *in, int32x4_t *out, int bit,
                           const int col_num) {
  const int32_t *cospi = cospi_arr(bit);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  int32x4_t u[16], v[16];
  int col;

  // Calculate the column 0, 1, 2, 3
  for (col = 0; col < col_num; ++col) {
    // stage 0
    // stage 1
    u[0] = vaddq_s32(in[0 * col_num + col], in[15 * col_num + col]);
    u[15] = vsubq_s32(in[0 * col_num + col], in[15 * col_num + col]);
    u[1] = vaddq_s32(in[1 * col_num + col], in[14 * col_num + col]);
    u[14] = vsubq_s32(in[1 * col_num + col], in[14 * col_num + col]);
    u[2] = vaddq_s32(in[2 * col_num + col], in[13 * col_num + col]);
    u[13] = vsubq_s32(in[2 * col_num + col], in[13 * col_num + col]);
    u[3] = vaddq_s32(in[3 * col_num + col], in[12 * col_num + col]);
    u[12] = vsubq_s32(in[3 * col_num + col], in[12 * col_num + col]);
    u[4] = vaddq_s32(in[4 * col_num + col], in[11 * col_num + col]);
    u[11] = vsubq_s32(in[4 * col_num + col], in[11 * col_num + col]);
    u[5] = vaddq_s32(in[5 * col_num + col], in[10 * col_num + col]);
    u[10] = vsubq_s32(in[5 * col_num + col], in[10 * col_num + col]);
    u[6] = vaddq_s32(in[6 * col_num + col], in[9 * col_num + col]);
    u[9] = vsubq_s32(in[6 * col_num + col], in[9 * col_num + col]);
    u[7] = vaddq_s32(in[7 * col_num + col], in[8 * col_num + col]);
    u[8] = vsubq_s32(in[7 * col_num + col], in[8 * col_num + col]);

    // stage 2
    v[0] = vaddq_s32(u[0], u[7]);
    v[7] = vsubq_s32(u[0], u[7]);
    v[1] = vaddq_s32(u[1], u[6]);
    v[6] = vsubq_s32(u[1], u[6]);
    v[2] = vaddq_s32(u[2], u[5]);
    v[5] = vsubq_s32(u[2], u[5]);
    v[3] = vaddq_s32(u[3], u[4]);
    v[4] = vsubq_s32(u[3], u[4]);
    v[8] = u[8];
    v[9] = u[9];

    v[10] = vmulq_n_s32(u[13], cospi[32]);
    v[10] = vmlsq_n_s32(v[10], u[10], cospi[32]);
    v[10] = vrshlq_s32(v[10], v_bit);

    v[13] = vmulq_n_s32(u[10], cospi[32]);
    v[13] = vmlaq_n_s32(v[13], u[13], cospi[32]);
    v[13] = vrshlq_s32(v[13], v_bit);

    v[11] = vmulq_n_s32(u[12], cospi[32]);
    v[11] = vmlsq_n_s32(v[11], u[11], cospi[32]);
    v[11] = vrshlq_s32(v[11], v_bit);

    v[12] = vmulq_n_s32(u[11], cospi[32]);
    v[12] = vmlaq_n_s32(v[12], u[12], cospi[32]);
    v[12] = vrshlq_s32(v[12], v_bit);
    v[14] = u[14];
    v[15] = u[15];

    // stage 3
    u[0] = vaddq_s32(v[0], v[3]);
    u[3] = vsubq_s32(v[0], v[3]);
    u[1] = vaddq_s32(v[1], v[2]);
    u[2] = vsubq_s32(v[1], v[2]);
    u[4] = v[4];

    u[5] = vmulq_n_s32(v[6], cospi[32]);
    u[5] = vmlsq_n_s32(u[5], v[5], cospi[32]);
    u[5] = vrshlq_s32(u[5], v_bit);

    u[6] = vmulq_n_s32(v[5], cospi[32]);
    u[6] = vmlaq_n_s32(u[6], v[6], cospi[32]);
    u[6] = vrshlq_s32(u[6], v_bit);

    u[7] = v[7];
    u[8] = vaddq_s32(v[8], v[11]);
    u[11] = vsubq_s32(v[8], v[11]);
    u[9] = vaddq_s32(v[9], v[10]);
    u[10] = vsubq_s32(v[9], v[10]);
    u[12] = vsubq_s32(v[15], v[12]);
    u[15] = vaddq_s32(v[15], v[12]);
    u[13] = vsubq_s32(v[14], v[13]);
    u[14] = vaddq_s32(v[14], v[13]);

    // stage 4
    u[0] = vmulq_n_s32(u[0], cospi[32]);
    u[1] = vmulq_n_s32(u[1], cospi[32]);
    v[0] = vaddq_s32(u[0], u[1]);
    v[0] = vrshlq_s32(v[0], v_bit);

    v[1] = vsubq_s32(u[0], u[1]);
    v[1] = vrshlq_s32(v[1], v_bit);

    v[2] = vmulq_n_s32(u[2], cospi[48]);
    v[2] = vmlaq_n_s32(v[2], u[3], cospi[16]);
    v[2] = vrshlq_s32(v[2], v_bit);

    v[3] = vmulq_n_s32(u[3], cospi[48]);
    v[3] = vmlsq_n_s32(v[3], u[2], cospi[16]);
    v[3] = vrshlq_s32(v[3], v_bit);

    v[4] = vaddq_s32(u[4], u[5]);
    v[5] = vsubq_s32(u[4], u[5]);
    v[6] = vsubq_s32(u[7], u[6]);
    v[7] = vaddq_s32(u[7], u[6]);
    v[8] = u[8];

    v[9] = vmulq_n_s32(u[14], cospi[48]);
    v[9] = vmlsq_n_s32(v[9], u[9], cospi[16]);
    v[9] = vrshlq_s32(v[9], v_bit);

    v[14] = vmulq_n_s32(u[9], cospi[48]);
    v[14] = vmlaq_n_s32(v[14], u[14], cospi[16]);
    v[14] = vrshlq_s32(v[14], v_bit);

    v[10] = vmulq_n_s32(u[13], -cospi[16]);
    v[10] = vmlsq_n_s32(v[10], u[10], cospi[48]);
    v[10] = vrshlq_s32(v[10], v_bit);

    v[13] = vmulq_n_s32(u[10], -cospi[16]);
    v[13] = vmlaq_n_s32(v[13], u[13], cospi[48]);
    v[13] = vrshlq_s32(v[13], v_bit);

    v[11] = u[11];
    v[12] = u[12];
    v[15] = u[15];

    // stage 5
    u[0] = v[0];
    u[1] = v[1];
    u[2] = v[2];
    u[3] = v[3];

    u[4] = vmulq_n_s32(v[4], cospi[56]);
    u[4] = vmlaq_n_s32(u[4], v[7], cospi[8]);
    u[4] = vrshlq_s32(u[4], v_bit);

    u[7] = vmulq_n_s32(v[7], cospi[56]);
    u[7] = vmlsq_n_s32(u[7], v[4], cospi[8]);
    u[7] = vrshlq_s32(u[7], v_bit);

    u[5] = vmulq_n_s32(v[5], cospi[24]);
    u[5] = vmlaq_n_s32(u[5], v[6], cospi[40]);
    u[5] = vrshlq_s32(u[5], v_bit);

    u[6] = vmulq_n_s32(v[6], cospi[24]);
    u[6] = vmlsq_n_s32(u[6], v[5], cospi[40]);
    u[6] = vrshlq_s32(u[6], v_bit);

    u[8] = vaddq_s32(v[8], v[9]);
    u[9] = vsubq_s32(v[8], v[9]);
    u[10] = vsubq_s32(v[11], v[10]);
    u[11] = vaddq_s32(v[11], v[10]);
    u[12] = vaddq_s32(v[12], v[13]);
    u[13] = vsubq_s32(v[12], v[13]);
    u[14] = vsubq_s32(v[15], v[14]);
    u[15] = vaddq_s32(v[15], v[14]);

    // stage 6
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = u[4];
    v[5] = u[5];
    v[6] = u[6];
    v[7] = u[7];

    v[8] = vmulq_n_s32(u[8], cospi[60]);
    v[8] = vmlaq_n_s32(v[8], u[15], cospi[4]);
    v[8] = vrshlq_s32(v[8], v_bit);

    v[15] = vmulq_n_s32(u[15], cospi[60]);
    v[15] = vmlsq_n_s32(v[15], u[8], cospi[4]);
    v[15] = vrshlq_s32(v[15], v_bit);

    v[9] = vmulq_n_s32(u[9], cospi[28]);
    v[9] = vmlaq_n_s32(v[9], u[14], cospi[36]);
    v[9] = vrshlq_s32(v[9], v_bit);

    v[14] = vmulq_n_s32(u[14], cospi[28]);
    v[14] = vmlsq_n_s32(v[14], u[9], cospi[36]);
    v[14] = vrshlq_s32(v[14], v_bit);

    v[10] = vmulq_n_s32(u[10], cospi[44]);
    v[10] = vmlaq_n_s32(v[10], u[13], cospi[20]);
    v[10] = vrshlq_s32(v[10], v_bit);

    v[13] = vmulq_n_s32(u[13], cospi[44]);
    v[13] = vmlsq_n_s32(v[13], u[10], cospi[20]);
    v[13] = vrshlq_s32(v[13], v_bit);

    v[11] = vmulq_n_s32(u[11], cospi[12]);
    v[11] = vmlaq_n_s32(v[11], u[12], cospi[52]);
    v[11] = vrshlq_s32(v[11], v_bit);

    v[12] = vmulq_n_s32(u[12], cospi[12]);
    v[12] = vmlsq_n_s32(v[12], u[11], cospi[52]);
    v[12] = vrshlq_s32(v[12], v_bit);

    out[0 * col_num + col] = v[0];
    out[1 * col_num + col] = v[8];
    out[2 * col_num + col] = v[4];
    out[3 * col_num + col] = v[12];
    out[4 * col_num + col] = v[2];
    out[5 * col_num + col] = v[10];
    out[6 * col_num + col] = v[6];
    out[7 * col_num + col] = v[14];
    out[8 * col_num + col] = v[1];
    out[9 * col_num + col] = v[9];
    out[10 * col_num + col] = v[5];
    out[11 * col_num + col] = v[13];
    out[12 * col_num + col] = v[3];
    out[13 * col_num + col] = v[11];
    out[14 * col_num + col] = v[7];
    out[15 * col_num + col] = v[15];
  }
}

static void fadst16x16_neon(int32x4_t *in, int32x4_t *out, int bit,
                            const int num_cols) {
  const int32_t *cospi = cospi_arr(bit);

  const int32x4_t v_bit = vdupq_n_s32(-bit);

  int32x4_t u[16], v[16], x, y;
  int col;

  for (col = 0; col < num_cols; ++col) {
    // stage 0-1
    u[0] = in[0 * num_cols + col];
    u[1] = vnegq_s32(in[15 * num_cols + col]);
    u[2] = vnegq_s32(in[7 * num_cols + col]);
    u[3] = in[8 * num_cols + col];
    u[4] = vnegq_s32(in[3 * num_cols + col]);
    u[5] = in[12 * num_cols + col];
    u[6] = in[4 * num_cols + col];
    u[7] = vnegq_s32(in[11 * num_cols + col]);
    u[8] = vnegq_s32(in[1 * num_cols + col]);
    u[9] = in[14 * num_cols + col];
    u[10] = in[6 * num_cols + col];
    u[11] = vnegq_s32(in[9 * num_cols + col]);
    u[12] = in[2 * num_cols + col];
    u[13] = vnegq_s32(in[13 * num_cols + col]);
    u[14] = vnegq_s32(in[5 * num_cols + col]);
    u[15] = in[10 * num_cols + col];

    // stage 2
    v[0] = u[0];
    v[1] = u[1];

    x = vmulq_n_s32(u[2], cospi[32]);
    y = vmulq_n_s32(u[3], cospi[32]);
    v[2] = vaddq_s32(x, y);
    v[2] = vrshlq_s32(v[2], v_bit);

    v[3] = vsubq_s32(x, y);
    v[3] = vrshlq_s32(v[3], v_bit);

    v[4] = u[4];
    v[5] = u[5];

    x = vmulq_n_s32(u[6], cospi[32]);
    y = vmulq_n_s32(u[7], cospi[32]);
    v[6] = vaddq_s32(x, y);
    v[6] = vrshlq_s32(v[6], v_bit);

    v[7] = vsubq_s32(x, y);
    v[7] = vrshlq_s32(v[7], v_bit);

    v[8] = u[8];
    v[9] = u[9];

    x = vmulq_n_s32(u[10], cospi[32]);
    y = vmulq_n_s32(u[11], cospi[32]);
    v[10] = vaddq_s32(x, y);
    v[10] = vrshlq_s32(v[10], v_bit);

    v[11] = vsubq_s32(x, y);
    v[11] = vrshlq_s32(v[11], v_bit);

    v[12] = u[12];
    v[13] = u[13];

    x = vmulq_n_s32(u[14], cospi[32]);
    y = vmulq_n_s32(u[15], cospi[32]);
    v[14] = vaddq_s32(x, y);
    v[14] = vrshlq_s32(v[14], v_bit);

    v[15] = vsubq_s32(x, y);
    v[15] = vrshlq_s32(v[15], v_bit);

    // stage 3
    u[0] = vaddq_s32(v[0], v[2]);
    u[1] = vaddq_s32(v[1], v[3]);
    u[2] = vsubq_s32(v[0], v[2]);
    u[3] = vsubq_s32(v[1], v[3]);
    u[4] = vaddq_s32(v[4], v[6]);
    u[5] = vaddq_s32(v[5], v[7]);
    u[6] = vsubq_s32(v[4], v[6]);
    u[7] = vsubq_s32(v[5], v[7]);
    u[8] = vaddq_s32(v[8], v[10]);
    u[9] = vaddq_s32(v[9], v[11]);
    u[10] = vsubq_s32(v[8], v[10]);
    u[11] = vsubq_s32(v[9], v[11]);
    u[12] = vaddq_s32(v[12], v[14]);
    u[13] = vaddq_s32(v[13], v[15]);
    u[14] = vsubq_s32(v[12], v[14]);
    u[15] = vsubq_s32(v[13], v[15]);

    // stage 4
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = half_btf_neon(&cospi[16], &u[4], &cospi[48], &u[5], v_bit);
    v[7] = half_btf_neon(&cospi[16], &u[6], &cospi[48], &u[7], v_bit);
    v[5] = half_btf_neon_m(&cospi[48], &u[4], &cospi[16], &u[5], v_bit);
    v[6] = half_btf_neon_m(&cospi[16], &u[7], &cospi[48], &u[6], v_bit);

    v[8] = u[8];
    v[9] = u[9];
    v[10] = u[10];
    v[11] = u[11];

    v[12] = half_btf_neon(&cospi[16], &u[12], &cospi[48], &u[13], v_bit);
    v[15] = half_btf_neon(&cospi[16], &u[14], &cospi[48], &u[15], v_bit);
    v[13] = half_btf_neon_m(&cospi[48], &u[12], &cospi[16], &u[13], v_bit);
    v[14] = half_btf_neon_m(&cospi[16], &u[15], &cospi[48], &u[14], v_bit);

    // stage 5
    u[0] = vaddq_s32(v[0], v[4]);
    u[1] = vaddq_s32(v[1], v[5]);
    u[2] = vaddq_s32(v[2], v[6]);
    u[3] = vaddq_s32(v[3], v[7]);
    u[4] = vsubq_s32(v[0], v[4]);
    u[5] = vsubq_s32(v[1], v[5]);
    u[6] = vsubq_s32(v[2], v[6]);
    u[7] = vsubq_s32(v[3], v[7]);
    u[8] = vaddq_s32(v[8], v[12]);
    u[9] = vaddq_s32(v[9], v[13]);
    u[10] = vaddq_s32(v[10], v[14]);
    u[11] = vaddq_s32(v[11], v[15]);
    u[12] = vsubq_s32(v[8], v[12]);
    u[13] = vsubq_s32(v[9], v[13]);
    u[14] = vsubq_s32(v[10], v[14]);
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

    v[8] = half_btf_neon(&cospi[8], &u[8], &cospi[56], &u[9], v_bit);
    v[13] = half_btf_neon(&cospi[8], &u[12], &cospi[56], &u[13], v_bit);
    v[9] = half_btf_neon_m(&cospi[56], &u[8], &cospi[8], &u[9], v_bit);
    v[12] = half_btf_neon_m(&cospi[8], &u[13], &cospi[56], &u[12], v_bit);

    v[10] = half_btf_neon(&cospi[40], &u[10], &cospi[24], &u[11], v_bit);
    v[15] = half_btf_neon(&cospi[40], &u[14], &cospi[24], &u[15], v_bit);
    v[11] = half_btf_neon_m(&cospi[24], &u[10], &cospi[40], &u[11], v_bit);
    v[14] = half_btf_neon_m(&cospi[40], &u[15], &cospi[24], &u[14], v_bit);

    // stage 7
    u[0] = vaddq_s32(v[0], v[8]);
    u[1] = vaddq_s32(v[1], v[9]);
    u[2] = vaddq_s32(v[2], v[10]);
    u[3] = vaddq_s32(v[3], v[11]);
    u[4] = vaddq_s32(v[4], v[12]);
    u[5] = vaddq_s32(v[5], v[13]);
    u[6] = vaddq_s32(v[6], v[14]);
    u[7] = vaddq_s32(v[7], v[15]);
    u[8] = vsubq_s32(v[0], v[8]);
    u[9] = vsubq_s32(v[1], v[9]);
    u[10] = vsubq_s32(v[2], v[10]);
    u[11] = vsubq_s32(v[3], v[11]);
    u[12] = vsubq_s32(v[4], v[12]);
    u[13] = vsubq_s32(v[5], v[13]);
    u[14] = vsubq_s32(v[6], v[14]);
    u[15] = vsubq_s32(v[7], v[15]);

    // stage 8
    v[0] = half_btf_neon(&cospi[2], &u[0], &cospi[62], &u[1], v_bit);
    v[1] = half_btf_neon_m(&cospi[62], &u[0], &cospi[2], &u[1], v_bit);
    v[2] = half_btf_neon(&cospi[10], &u[2], &cospi[54], &u[3], v_bit);
    v[3] = half_btf_neon_m(&cospi[54], &u[2], &cospi[10], &u[3], v_bit);
    v[4] = half_btf_neon(&cospi[18], &u[4], &cospi[46], &u[5], v_bit);
    v[5] = half_btf_neon_m(&cospi[46], &u[4], &cospi[18], &u[5], v_bit);
    v[6] = half_btf_neon(&cospi[26], &u[6], &cospi[38], &u[7], v_bit);
    v[7] = half_btf_neon_m(&cospi[38], &u[6], &cospi[26], &u[7], v_bit);
    v[8] = half_btf_neon(&cospi[34], &u[8], &cospi[30], &u[9], v_bit);
    v[9] = half_btf_neon_m(&cospi[30], &u[8], &cospi[34], &u[9], v_bit);
    v[10] = half_btf_neon(&cospi[42], &u[10], &cospi[22], &u[11], v_bit);
    v[11] = half_btf_neon_m(&cospi[22], &u[10], &cospi[42], &u[11], v_bit);
    v[12] = half_btf_neon(&cospi[50], &u[12], &cospi[14], &u[13], v_bit);
    v[13] = half_btf_neon_m(&cospi[14], &u[12], &cospi[50], &u[13], v_bit);
    v[14] = half_btf_neon(&cospi[58], &u[14], &cospi[6], &u[15], v_bit);
    v[15] = half_btf_neon_m(&cospi[6], &u[14], &cospi[58], &u[15], v_bit);

    // stage 9
    out[0 * num_cols + col] = v[1];
    out[1 * num_cols + col] = v[14];
    out[2 * num_cols + col] = v[3];
    out[3 * num_cols + col] = v[12];
    out[4 * num_cols + col] = v[5];
    out[5 * num_cols + col] = v[10];
    out[6 * num_cols + col] = v[7];
    out[7 * num_cols + col] = v[8];
    out[8 * num_cols + col] = v[9];
    out[9 * num_cols + col] = v[6];
    out[10 * num_cols + col] = v[11];
    out[11 * num_cols + col] = v[4];
    out[12 * num_cols + col] = v[13];
    out[13 * num_cols + col] = v[2];
    out[14 * num_cols + col] = v[15];
    out[15 * num_cols + col] = v[0];
  }
}

static void col_txfm_16x16_rounding(int32x4_t *in, const int32x4_t *v_shift) {
  // Note:
  //  We split 16x16 rounding into 4 sections of 8x8 rounding,
  //  instead of 4 columns
  col_txfm_8x8_rounding(&in[0], v_shift);
  col_txfm_8x8_rounding(&in[16], v_shift);
  col_txfm_8x8_rounding(&in[32], v_shift);
  col_txfm_8x8_rounding(&in[48], v_shift);
}

static void col_txfm_8x16_rounding(int32x4_t *in, const int32x4_t *v_shift) {
  col_txfm_8x8_rounding(&in[0], v_shift);
  col_txfm_8x8_rounding(&in[16], v_shift);
}

static void write_buffer_16x16(const int32x4_t *in, int32_t *output) {
  const int size_8x8 = 16 * 4;
  write_buffer_8x8(&in[0], output);
  output += size_8x8;
  write_buffer_8x8(&in[16], output);
  output += size_8x8;
  write_buffer_8x8(&in[32], output);
  output += size_8x8;
  write_buffer_8x8(&in[48], output);
}
static void idtx16x16_neon(int32x4_t *in, int32x4_t *out, int bit,
                           int col_num) {
  (void)bit;
  int32x4_t fact = vdupq_n_s32(2 * NewSqrt2);
  int32x4_t offset = vdupq_n_s32(1 << (NewSqrt2Bits - 1));
  int32x4_t a_low;

  int num_iters = 16 * col_num;
  for (int i = 0; i < num_iters; i++) {
    a_low = vmulq_s32(in[i], fact);
    a_low = vaddq_s32(a_low, offset);
    out[i] = vshrq_n_s32(a_low, NewSqrt2Bits);
  }
}
void av1_fwd_txfm2d_16x16_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  int32x4_t in[64], out[64];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X16];
  const int txw_idx = get_txw_idx(TX_16X16);
  const int txh_idx = get_txh_idx(TX_16X16);
  const int col_num = 4;
  const int32x4_t v_shift = vdupq_n_s32(shift[1]);
  switch (tx_type) {
    case DCT_DCT:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fdct16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fdct16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case ADST_DCT:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fdct16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case DCT_ADST:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fdct16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case ADST_ADST:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case FLIPADST_DCT:
      load_buffer_16x16(input, in, stride, 1, 0, shift[0]);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fdct16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case DCT_FLIPADST:
      load_buffer_16x16(input, in, stride, 0, 1, shift[0]);
      fdct16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_16x16(input, in, stride, 1, 1, shift[0]);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case ADST_FLIPADST:
      load_buffer_16x16(input, in, stride, 0, 1, shift[0]);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case FLIPADST_ADST:
      load_buffer_16x16(input, in, stride, 1, 0, shift[0]);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case IDTX:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      idtx16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      idtx16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case V_DCT:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fdct16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      idtx16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case H_DCT:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      idtx16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fdct16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case V_ADST:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      idtx16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case H_ADST:
      load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
      idtx16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case V_FLIPADST:
      load_buffer_16x16(input, in, stride, 1, 0, shift[0]);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      idtx16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    case H_FLIPADST:
      load_buffer_16x16(input, in, stride, 0, 1, shift[0]);
      idtx16x16_neon(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], col_num);
      col_txfm_16x16_rounding(out, &v_shift);
      transpose_16x16(out, in);
      fadst16x16_neon(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], col_num);
      write_buffer_16x16(out, coeff);
      break;
    default: assert(0);
  }
  (void)bd;
}

static INLINE void flip_buf_neon(int32x4_t *in, int32x4_t *out, int size) {
  for (int i = 0; i < size; i += 2) in[30 - i] = out[i];
  for (int i = 1; i < size; i += 2) in[size - i] = out[i];
}

typedef void (*fwd_transform_1d_neon)(int32x4_t *in, int32x4_t *out, int bit,
                                      const int num_cols);

static const fwd_transform_1d_neon col_highbd_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_neon,   // DCT_DCT
  fadst8x8_neon,  // ADST_DCT
  fdct8x8_neon,   // DCT_ADST
  fadst8x8_neon,  // ADST_ADST
  fadst8x8_neon,  // FLIPADST_DCT
  fdct8x8_neon,   // DCT_FLIPADST
  fadst8x8_neon,  // FLIPADST_FLIPADST
  fadst8x8_neon,  // ADST_FLIPADST
  fadst8x8_neon,  // FLIPADST_ADST
  idtx8x8_neon,   // IDTX
  fdct8x8_neon,   // V_DCT
  idtx8x8_neon,   // H_DCT
  fadst8x8_neon,  // V_ADST
  idtx8x8_neon,   // H_ADST
  fadst8x8_neon,  // V_FLIPADST
  idtx8x8_neon    // H_FLIPADST
};
#if !CONFIG_REALTIME_ONLY
static const fwd_transform_1d_neon row_highbd_txfm32x8_arr[TX_TYPES] = {
  fdct8x8_neon,   // DCT_DCT
  NULL,           // ADST_DCT
  NULL,           // DCT_ADST
  NULL,           // ADST_ADST
  NULL,           // FLIPADST_DCT
  NULL,           // DCT_FLIPADST
  NULL,           // FLIPADST_FLIPADST
  NULL,           // ADST_FLIPADST
  NULL,           // FLIPADST-ADST
  idtx32x8_neon,  // IDTX
  NULL,           // V_DCT
  NULL,           // H_DCT
  NULL,           // V_ADST
  NULL,           // H_ADST
  NULL,           // V_FLIPADST
  NULL,           // H_FLIPADST
};
#endif
static const fwd_transform_1d_neon col_highbd_txfm4x8_arr[TX_TYPES] = {
  fdct4x8_neon,   // DCT_DCT
  fadst8x8_neon,  // ADST_DCT
  fdct4x8_neon,   // DCT_ADST
  fadst8x8_neon,  // ADST_ADST
  fadst8x8_neon,  // FLIPADST_DCT
  fdct4x8_neon,   // DCT_FLIPADST
  fadst8x8_neon,  // FLIPADST_FLIPADST
  fadst8x8_neon,  // ADST_FLIPADST
  fadst8x8_neon,  // FLIPADST_ADST
  idtx8x8_neon,   // IDTX
  fdct4x8_neon,   // V_DCT
  idtx8x8_neon,   // H_DCT
  fadst8x8_neon,  // V_ADST
  idtx8x8_neon,   // H_ADST
  fadst8x8_neon,  // V_FLIPADST
  idtx8x8_neon    // H_FLIPADST
};

static const fwd_transform_1d_neon row_highbd_txfm8x16_arr[TX_TYPES] = {
  fdct16x16_neon,   // DCT_DCT
  fdct16x16_neon,   // ADST_DCT
  fadst16x16_neon,  // DCT_ADST
  fadst16x16_neon,  // ADST_ADST
  fdct16x16_neon,   // FLIPADST_DCT
  fadst16x16_neon,  // DCT_FLIPADST
  fadst16x16_neon,  // FLIPADST_FLIPADST
  fadst16x16_neon,  // ADST_FLIPADST
  fadst16x16_neon,  // FLIPADST_ADST
  idtx16x16_neon,   // IDTX
  idtx16x16_neon,   // V_DCT
  fdct16x16_neon,   // H_DCT
  idtx16x16_neon,   // V_ADST
  fadst16x16_neon,  // H_ADST
  idtx16x16_neon,   // V_FLIPADST
  fadst16x16_neon   // H_FLIPADST
};

static const fwd_transform_1d_neon col_highbd_txfm8x16_arr[TX_TYPES] = {
  fdct16x16_neon,   // DCT_DCT
  fadst16x16_neon,  // ADST_DCT
  fdct16x16_neon,   // DCT_ADST
  fadst16x16_neon,  // ADST_ADST
  fadst16x16_neon,  // FLIPADST_DCT
  fdct16x16_neon,   // DCT_FLIPADST
  fadst16x16_neon,  // FLIPADST_FLIPADST
  fadst16x16_neon,  // ADST_FLIPADST
  fadst16x16_neon,  // FLIPADST_ADST
  idtx16x16_neon,   // IDTX
  fdct16x16_neon,   // V_DCT
  idtx16x16_neon,   // H_DCT
  fadst16x16_neon,  // V_ADST
  idtx16x16_neon,   // H_ADST
  fadst16x16_neon,  // V_FLIPADST
  idtx16x16_neon    // H_FLIPADST
};
static const fwd_transform_1d_neon row_highbd_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_neon,   // DCT_DCT
  fdct8x8_neon,   // ADST_DCT
  fadst8x8_neon,  // DCT_ADST
  fadst8x8_neon,  // ADST_ADST
  fdct8x8_neon,   // FLIPADST_DCT
  fadst8x8_neon,  // DCT_FLIPADST
  fadst8x8_neon,  // FLIPADST_FLIPADST
  fadst8x8_neon,  // ADST_FLIPADST
  fadst8x8_neon,  // FLIPADST_ADST
  idtx8x8_neon,   // IDTX
  idtx8x8_neon,   // V_DCT
  fdct8x8_neon,   // H_DCT
  idtx8x8_neon,   // V_ADST
  fadst8x8_neon,  // H_ADST
  idtx8x8_neon,   // V_FLIPADST
  fadst8x8_neon   // H_FLIPADST
};

static const fwd_transform_1d_neon row_highbd_txfm4x8_arr[TX_TYPES] = {
  fdct4x8_neon,   // DCT_DCT
  fdct4x8_neon,   // ADST_DCT
  fadst8x8_neon,  // DCT_ADST
  fadst8x8_neon,  // ADST_ADST
  fdct4x8_neon,   // FLIPADST_DCT
  fadst8x8_neon,  // DCT_FLIPADST
  fadst8x8_neon,  // FLIPADST_FLIPADST
  fadst8x8_neon,  // ADST_FLIPADST
  fadst8x8_neon,  // FLIPADST_ADST
  idtx8x8_neon,   // IDTX
  idtx8x8_neon,   // V_DCT
  fdct4x8_neon,   // H_DCT
  idtx8x8_neon,   // V_ADST
  fadst8x8_neon,  // H_ADST
  idtx8x8_neon,   // V_FLIPADST
  fadst8x8_neon   // H_FLIPADST
};

static const fwd_transform_1d_neon row_highbd_txfm4x4_arr[TX_TYPES] = {
  fdct4x4_neon,   // DCT_DCT
  fdct4x4_neon,   // ADST_DCT
  fadst4x4_neon,  // DCT_ADST
  fadst4x4_neon,  // ADST_ADST
  fdct4x4_neon,   // FLIPADST_DCT
  fadst4x4_neon,  // DCT_FLIPADST
  fadst4x4_neon,  // FLIPADST_FLIPADST
  fadst4x4_neon,  // ADST_FLIPADST
  fadst4x4_neon,  // FLIPADST_ADST
  idtx4x4_neon,   // IDTX
  idtx4x4_neon,   // V_DCT
  fdct4x4_neon,   // H_DCT
  idtx4x4_neon,   // V_ADST
  fadst4x4_neon,  // H_ADST
  idtx4x4_neon,   // V_FLIPADST
  fadst4x4_neon   // H_FLIPADST
};

static const fwd_transform_1d_neon col_highbd_txfm4x4_arr[TX_TYPES] = {
  fdct4x4_neon,   // DCT_DCT
  fadst4x4_neon,  // ADST_DCT
  fdct4x4_neon,   // DCT_ADST
  fadst4x4_neon,  // ADST_ADST
  fadst4x4_neon,  // FLIPADST_DCT
  fdct4x4_neon,   // DCT_FLIPADST
  fadst4x4_neon,  // FLIPADST_FLIPADST
  fadst4x4_neon,  // ADST_FLIPADST
  fadst4x4_neon,  // FLIPADST_ADST
  idtx4x4_neon,   // IDTX
  fdct4x4_neon,   // V_DCT
  idtx4x4_neon,   // H_DCT
  fadst4x4_neon,  // V_ADST
  idtx4x4_neon,   // H_ADST
  fadst4x4_neon,  // V_FLIPADST
  idtx4x4_neon    // H_FLIPADST
};

void av1_fdct32_new_neon(int32x4_t *input, int32x4_t *output, int cos_bit,
                         const int stride) {
  int32x4_t buf0[32];
  int32x4_t buf1[32];
  const int32_t *cospi;
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  int startidx = 0 * stride;
  int endidx = 31 * stride;
  // stage 0
  // stage 1
  buf1[0] = vaddq_s32(input[startidx], input[endidx]);
  buf1[31] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[1] = vaddq_s32(input[startidx], input[endidx]);
  buf1[30] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[2] = vaddq_s32(input[startidx], input[endidx]);
  buf1[29] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[3] = vaddq_s32(input[startidx], input[endidx]);
  buf1[28] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[4] = vaddq_s32(input[startidx], input[endidx]);
  buf1[27] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[5] = vaddq_s32(input[startidx], input[endidx]);
  buf1[26] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[6] = vaddq_s32(input[startidx], input[endidx]);
  buf1[25] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[7] = vaddq_s32(input[startidx], input[endidx]);
  buf1[24] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[8] = vaddq_s32(input[startidx], input[endidx]);
  buf1[23] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[9] = vaddq_s32(input[startidx], input[endidx]);
  buf1[22] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[10] = vaddq_s32(input[startidx], input[endidx]);
  buf1[21] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[11] = vaddq_s32(input[startidx], input[endidx]);
  buf1[20] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[12] = vaddq_s32(input[startidx], input[endidx]);
  buf1[19] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[13] = vaddq_s32(input[startidx], input[endidx]);
  buf1[18] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[14] = vaddq_s32(input[startidx], input[endidx]);
  buf1[17] = vsubq_s32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[15] = vaddq_s32(input[startidx], input[endidx]);
  buf1[16] = vsubq_s32(input[startidx], input[endidx]);

  // stage 2
  cospi = cospi_arr(cos_bit);
  buf0[0] = vaddq_s32(buf1[0], buf1[15]);
  buf0[15] = vsubq_s32(buf1[0], buf1[15]);
  buf0[1] = vaddq_s32(buf1[1], buf1[14]);
  buf0[14] = vsubq_s32(buf1[1], buf1[14]);
  buf0[2] = vaddq_s32(buf1[2], buf1[13]);
  buf0[13] = vsubq_s32(buf1[2], buf1[13]);
  buf0[3] = vaddq_s32(buf1[3], buf1[12]);
  buf0[12] = vsubq_s32(buf1[3], buf1[12]);
  buf0[4] = vaddq_s32(buf1[4], buf1[11]);
  buf0[11] = vsubq_s32(buf1[4], buf1[11]);
  buf0[5] = vaddq_s32(buf1[5], buf1[10]);
  buf0[10] = vsubq_s32(buf1[5], buf1[10]);
  buf0[6] = vaddq_s32(buf1[6], buf1[9]);
  buf0[9] = vsubq_s32(buf1[6], buf1[9]);
  buf0[7] = vaddq_s32(buf1[7], buf1[8]);
  buf0[8] = vsubq_s32(buf1[7], buf1[8]);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  buf0[18] = buf1[18];
  buf0[19] = buf1[19];
  btf_32_neon_type0(-cospi[32], cospi[32], buf1[20], buf1[27], buf0[20],
                    buf0[27], v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], buf1[21], buf1[26], buf0[21],
                    buf0[26], v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], buf1[22], buf1[25], buf0[22],
                    buf0[25], v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], buf1[23], buf1[24], buf0[23],
                    buf0[24], v_cos_bit);
  buf0[28] = buf1[28];
  buf0[29] = buf1[29];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 3
  cospi = cospi_arr(cos_bit);
  buf1[0] = vaddq_s32(buf0[0], buf0[7]);
  buf1[7] = vsubq_s32(buf0[0], buf0[7]);
  buf1[1] = vaddq_s32(buf0[1], buf0[6]);
  buf1[6] = vsubq_s32(buf0[1], buf0[6]);
  buf1[2] = vaddq_s32(buf0[2], buf0[5]);
  buf1[5] = vsubq_s32(buf0[2], buf0[5]);
  buf1[3] = vaddq_s32(buf0[3], buf0[4]);
  buf1[4] = vsubq_s32(buf0[3], buf0[4]);
  buf1[8] = buf0[8];
  buf1[9] = buf0[9];
  btf_32_neon_type0(-cospi[32], cospi[32], buf0[10], buf0[13], buf1[10],
                    buf1[13], v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], buf0[11], buf0[12], buf1[11],
                    buf1[12], v_cos_bit);
  buf1[14] = buf0[14];
  buf1[15] = buf0[15];
  buf1[16] = vaddq_s32(buf0[16], buf0[23]);
  buf1[23] = vsubq_s32(buf0[16], buf0[23]);
  buf1[17] = vaddq_s32(buf0[17], buf0[22]);
  buf1[22] = vsubq_s32(buf0[17], buf0[22]);
  buf1[18] = vaddq_s32(buf0[18], buf0[21]);
  buf1[21] = vsubq_s32(buf0[18], buf0[21]);
  buf1[19] = vaddq_s32(buf0[19], buf0[20]);
  buf1[20] = vsubq_s32(buf0[19], buf0[20]);
  buf1[24] = vsubq_s32(buf0[31], buf0[24]);
  buf1[31] = vaddq_s32(buf0[31], buf0[24]);
  buf1[25] = vsubq_s32(buf0[30], buf0[25]);
  buf1[30] = vaddq_s32(buf0[30], buf0[25]);
  buf1[26] = vsubq_s32(buf0[29], buf0[26]);
  buf1[29] = vaddq_s32(buf0[29], buf0[26]);
  buf1[27] = vsubq_s32(buf0[28], buf0[27]);
  buf1[28] = vaddq_s32(buf0[28], buf0[27]);

  // stage 4
  cospi = cospi_arr(cos_bit);
  buf0[0] = vaddq_s32(buf1[0], buf1[3]);
  buf0[3] = vsubq_s32(buf1[0], buf1[3]);
  buf0[1] = vaddq_s32(buf1[1], buf1[2]);
  buf0[2] = vsubq_s32(buf1[1], buf1[2]);
  buf0[4] = buf1[4];
  btf_32_neon_type0(-cospi[32], cospi[32], buf1[5], buf1[6], buf0[5], buf0[6],
                    v_cos_bit);
  buf0[7] = buf1[7];
  buf0[8] = vaddq_s32(buf1[8], buf1[11]);
  buf0[11] = vsubq_s32(buf1[8], buf1[11]);
  buf0[9] = vaddq_s32(buf1[9], buf1[10]);
  buf0[10] = vsubq_s32(buf1[9], buf1[10]);
  buf0[12] = vsubq_s32(buf1[15], buf1[12]);
  buf0[15] = vaddq_s32(buf1[15], buf1[12]);
  buf0[13] = vsubq_s32(buf1[14], buf1[13]);
  buf0[14] = vaddq_s32(buf1[14], buf1[13]);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];

  btf_32_neon_type0(-cospi[16], cospi[48], buf1[18], buf1[29], buf0[18],
                    buf0[29], v_cos_bit);
  btf_32_neon_type0(-cospi[16], cospi[48], buf1[19], buf1[28], buf0[19],
                    buf0[28], v_cos_bit);

  btf_32_neon_type0(-cospi[48], -cospi[16], buf1[20], buf1[27], buf0[20],
                    buf0[27], v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], buf1[21], buf1[26], buf0[21],
                    buf0[26], v_cos_bit);

  buf0[22] = buf1[22];
  buf0[23] = buf1[23];
  buf0[24] = buf1[24];
  buf0[25] = buf1[25];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 5
  btf_32_neon_type0(cospi[32], cospi[32], buf0[0], buf0[1], buf1[0], buf1[1],
                    v_cos_bit);

  btf_32_neon_type1(cospi[48], cospi[16], buf0[2], buf0[3], buf1[2], buf1[3],
                    v_cos_bit);
  buf1[4] = vaddq_s32(buf0[4], buf0[5]);
  buf1[5] = vsubq_s32(buf0[4], buf0[5]);
  buf1[6] = vsubq_s32(buf0[7], buf0[6]);
  buf1[7] = vaddq_s32(buf0[7], buf0[6]);
  buf1[8] = buf0[8];
  btf_32_neon_type0(-cospi[16], cospi[48], buf0[9], buf0[14], buf1[9], buf1[14],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], buf0[10], buf0[13], buf1[10],
                    buf1[13], v_cos_bit);
  buf1[11] = buf0[11];
  buf1[12] = buf0[12];
  buf1[15] = buf0[15];
  buf1[16] = vaddq_s32(buf0[16], buf0[19]);
  buf1[19] = vsubq_s32(buf0[16], buf0[19]);
  buf1[17] = vaddq_s32(buf0[17], buf0[18]);
  buf1[18] = vsubq_s32(buf0[17], buf0[18]);
  buf1[20] = vsubq_s32(buf0[23], buf0[20]);
  buf1[23] = vaddq_s32(buf0[23], buf0[20]);
  buf1[21] = vsubq_s32(buf0[22], buf0[21]);
  buf1[22] = vaddq_s32(buf0[22], buf0[21]);
  buf1[24] = vaddq_s32(buf0[24], buf0[27]);
  buf1[27] = vsubq_s32(buf0[24], buf0[27]);
  buf1[25] = vaddq_s32(buf0[25], buf0[26]);
  buf1[26] = vsubq_s32(buf0[25], buf0[26]);
  buf1[28] = vsubq_s32(buf0[31], buf0[28]);
  buf1[31] = vaddq_s32(buf0[31], buf0[28]);
  buf1[29] = vsubq_s32(buf0[30], buf0[29]);
  buf1[30] = vaddq_s32(buf0[30], buf0[29]);

  // stage 6
  cospi = cospi_arr(cos_bit);
  buf0[0] = buf1[0];
  buf0[1] = buf1[1];
  buf0[2] = buf1[2];
  buf0[3] = buf1[3];

  btf_32_neon_type1(cospi[56], cospi[8], buf1[4], buf1[7], buf0[4], buf0[7],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[8], cospi[56], buf1[17], buf1[30], buf0[17],
                    buf0[30], v_cos_bit);
  btf_32_neon_type0(-cospi[56], -cospi[8], buf1[18], buf1[29], buf0[18],
                    buf0[29], v_cos_bit);

  buf0[8] = vaddq_s32(buf1[8], buf1[9]);
  buf0[9] = vsubq_s32(buf1[8], buf1[9]);
  buf0[10] = vsubq_s32(buf1[11], buf1[10]);
  buf0[11] = vaddq_s32(buf1[11], buf1[10]);
  buf0[12] = vaddq_s32(buf1[12], buf1[13]);
  buf0[13] = vsubq_s32(buf1[12], buf1[13]);
  buf0[14] = vsubq_s32(buf1[15], buf1[14]);
  buf0[15] = vaddq_s32(buf1[15], buf1[14]);
  buf0[16] = buf1[16];
  buf0[19] = buf1[19];
  buf0[20] = buf1[20];

  btf_32_neon_type1(cospi[24], cospi[40], buf1[5], buf1[6], buf0[5], buf0[6],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[40], cospi[24], buf1[21], buf1[26], buf0[21],
                    buf0[26], v_cos_bit);
  btf_32_neon_type0(-cospi[24], -cospi[40], buf1[22], buf1[25], buf0[22],
                    buf0[25], v_cos_bit);

  buf0[23] = buf1[23];
  buf0[24] = buf1[24];
  buf0[27] = buf1[27];
  buf0[28] = buf1[28];
  buf0[31] = buf1[31];

  // stage 7
  cospi = cospi_arr(cos_bit);
  buf1[0] = buf0[0];
  buf1[1] = buf0[1];
  buf1[2] = buf0[2];
  buf1[3] = buf0[3];
  buf1[4] = buf0[4];
  buf1[5] = buf0[5];
  buf1[6] = buf0[6];
  buf1[7] = buf0[7];
  btf_32_neon_type1(cospi[60], cospi[4], buf0[8], buf0[15], buf1[8], buf1[15],
                    v_cos_bit);
  btf_32_neon_type1(cospi[28], cospi[36], buf0[9], buf0[14], buf1[9], buf1[14],
                    v_cos_bit);
  btf_32_neon_type1(cospi[44], cospi[20], buf0[10], buf0[13], buf1[10],
                    buf1[13], v_cos_bit);
  btf_32_neon_type1(cospi[12], cospi[52], buf0[11], buf0[12], buf1[11],
                    buf1[12], v_cos_bit);
  buf1[16] = vaddq_s32(buf0[16], buf0[17]);
  buf1[17] = vsubq_s32(buf0[16], buf0[17]);
  buf1[18] = vsubq_s32(buf0[19], buf0[18]);
  buf1[19] = vaddq_s32(buf0[19], buf0[18]);
  buf1[20] = vaddq_s32(buf0[20], buf0[21]);
  buf1[21] = vsubq_s32(buf0[20], buf0[21]);
  buf1[22] = vsubq_s32(buf0[23], buf0[22]);
  buf1[23] = vaddq_s32(buf0[23], buf0[22]);
  buf1[24] = vaddq_s32(buf0[24], buf0[25]);
  buf1[25] = vsubq_s32(buf0[24], buf0[25]);
  buf1[26] = vsubq_s32(buf0[27], buf0[26]);
  buf1[27] = vaddq_s32(buf0[27], buf0[26]);
  buf1[28] = vaddq_s32(buf0[28], buf0[29]);
  buf1[29] = vsubq_s32(buf0[28], buf0[29]);
  buf1[30] = vsubq_s32(buf0[31], buf0[30]);
  buf1[31] = vaddq_s32(buf0[31], buf0[30]);

  // stage 8
  cospi = cospi_arr(cos_bit);
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
  btf_32_neon_type1(cospi[62], cospi[2], buf1[16], buf1[31], buf0[16], buf0[31],
                    v_cos_bit);
  btf_32_neon_type1(cospi[30], cospi[34], buf1[17], buf1[30], buf0[17],
                    buf0[30], v_cos_bit);
  btf_32_neon_type1(cospi[46], cospi[18], buf1[18], buf1[29], buf0[18],
                    buf0[29], v_cos_bit);
  btf_32_neon_type1(cospi[14], cospi[50], buf1[19], buf1[28], buf0[19],
                    buf0[28], v_cos_bit);
  btf_32_neon_type1(cospi[54], cospi[10], buf1[20], buf1[27], buf0[20],
                    buf0[27], v_cos_bit);
  btf_32_neon_type1(cospi[22], cospi[42], buf1[21], buf1[26], buf0[21],
                    buf0[26], v_cos_bit);
  btf_32_neon_type1(cospi[38], cospi[26], buf1[22], buf1[25], buf0[22],
                    buf0[25], v_cos_bit);
  btf_32_neon_type1(cospi[6], cospi[58], buf1[23], buf1[24], buf0[23], buf0[24],
                    v_cos_bit);

  startidx = 0 * stride;
  endidx = 31 * stride;
  // stage 9
  output[startidx] = buf0[0];
  output[endidx] = buf0[31];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[16];
  output[endidx] = buf0[15];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[8];
  output[endidx] = buf0[23];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[24];
  output[endidx] = buf0[7];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[4];
  output[endidx] = buf0[27];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[20];
  output[endidx] = buf0[11];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[12];
  output[endidx] = buf0[19];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[28];
  output[endidx] = buf0[3];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[2];
  output[endidx] = buf0[29];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[18];
  output[endidx] = buf0[13];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[10];
  output[endidx] = buf0[21];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[26];
  output[endidx] = buf0[5];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[6];
  output[endidx] = buf0[25];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[22];
  output[endidx] = buf0[9];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[14];
  output[endidx] = buf0[17];
  startidx += stride;
  endidx -= stride;
  output[startidx] = buf0[30];
  output[endidx] = buf0[1];
}

void av1_fadst4_new_neon(const int32x4_t *input, int32x4_t *output,
                         const int8_t cos_bit, const int8_t *stage_range) {
  const int txfm_size = 4;
  const int num_per_128 = 4;
  const int32_t *cospi;
  int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);
  int32x4_t buf0[4];
  int32x4_t buf1[4];
  int col_num = txfm_size / num_per_128;
  int col;
  (void)stage_range;
  cospi = cospi_arr(cos_bit);
  for (col = 0; col < col_num; col++) {
    // stage 0;
    int j;
    for (j = 0; j < 4; ++j) {
      buf0[j] = input[j * col_num + col];
    }

    // stage 1
    buf1[0] = buf0[3];
    buf1[1] = buf0[0];
    buf1[2] = buf0[1];
    buf1[3] = buf0[2];

    // stage 2
    btf_32_neon_type0(cospi[8], cospi[56], buf1[0], buf1[1], buf0[0], buf0[1],
                      v_cos_bit);
    btf_32_neon_type0(cospi[40], cospi[24], buf1[2], buf1[3], buf0[2], buf0[3],
                      v_cos_bit);

    // stage 3
    buf1[0] = vaddq_s32(buf0[0], buf0[2]);
    buf1[2] = vsubq_s32(buf0[0], buf0[2]);
    buf1[1] = vaddq_s32(buf0[1], buf0[3]);
    buf1[3] = vsubq_s32(buf0[1], buf0[3]);

    // stage 4
    cospi = cospi_arr(cos_bit);
    buf0[0] = buf1[0];
    buf0[1] = buf1[1];

    btf_32_neon_type0(cospi[32], cospi[32], buf1[2], buf1[3], buf0[2], buf0[3],
                      v_cos_bit);

    // stage 5
    buf1[0] = buf0[0];
    buf1[1] = vnegq_s32(buf0[2]);
    buf1[2] = buf0[3];
    buf1[3] = vnegq_s32(buf0[1]);

    for (j = 0; j < 4; ++j) {
      output[j * col_num + col] = buf1[j];
    }
  }
}

static void av1_fdct64_new_stage12345_neon(int32x4_t *input, const int instride,
                                           int32x4_t *x5, const int32_t *cospi,
                                           const int32x4_t *v_cos_bit,
                                           int *startidx, int *endidx) {
  int32x4_t x1[64];
  x1[0] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[63] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[1] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[62] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[2] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[61] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[3] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[60] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[4] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[59] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[5] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[58] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[6] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[57] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[7] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[56] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[8] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[55] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[9] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[54] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[10] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[53] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[11] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[52] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[12] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[51] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[13] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[50] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[14] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[49] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[15] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[48] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[16] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[47] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[17] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[46] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[18] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[45] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[19] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[44] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[20] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[43] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[21] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[42] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[22] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[41] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[23] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[40] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[24] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[39] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[25] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[38] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[26] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[37] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[27] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[36] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[28] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[35] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[29] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[34] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[30] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[33] = vsubq_s32(input[*startidx], input[*endidx]);
  *startidx += instride;
  *endidx -= instride;
  x1[31] = vaddq_s32(input[*startidx], input[*endidx]);
  x1[32] = vsubq_s32(input[*startidx], input[*endidx]);

  // stage 2
  int32x4_t x2[64];
  x2[0] = vaddq_s32(x1[0], x1[31]);
  x2[31] = vsubq_s32(x1[0], x1[31]);
  x2[1] = vaddq_s32(x1[1], x1[30]);
  x2[30] = vsubq_s32(x1[1], x1[30]);
  x2[2] = vaddq_s32(x1[2], x1[29]);
  x2[29] = vsubq_s32(x1[2], x1[29]);
  x2[3] = vaddq_s32(x1[3], x1[28]);
  x2[28] = vsubq_s32(x1[3], x1[28]);
  x2[4] = vaddq_s32(x1[4], x1[27]);
  x2[27] = vsubq_s32(x1[4], x1[27]);
  x2[5] = vaddq_s32(x1[5], x1[26]);
  x2[26] = vsubq_s32(x1[5], x1[26]);
  x2[6] = vaddq_s32(x1[6], x1[25]);
  x2[25] = vsubq_s32(x1[6], x1[25]);
  x2[7] = vaddq_s32(x1[7], x1[24]);
  x2[24] = vsubq_s32(x1[7], x1[24]);
  x2[8] = vaddq_s32(x1[8], x1[23]);
  x2[23] = vsubq_s32(x1[8], x1[23]);
  x2[9] = vaddq_s32(x1[9], x1[22]);
  x2[22] = vsubq_s32(x1[9], x1[22]);
  x2[10] = vaddq_s32(x1[10], x1[21]);
  x2[21] = vsubq_s32(x1[10], x1[21]);
  x2[11] = vaddq_s32(x1[11], x1[20]);
  x2[20] = vsubq_s32(x1[11], x1[20]);
  x2[12] = vaddq_s32(x1[12], x1[19]);
  x2[19] = vsubq_s32(x1[12], x1[19]);
  x2[13] = vaddq_s32(x1[13], x1[18]);
  x2[18] = vsubq_s32(x1[13], x1[18]);
  x2[14] = vaddq_s32(x1[14], x1[17]);
  x2[17] = vsubq_s32(x1[14], x1[17]);
  x2[15] = vaddq_s32(x1[15], x1[16]);
  x2[16] = vsubq_s32(x1[15], x1[16]);
  x2[32] = x1[32];
  x2[33] = x1[33];
  x2[34] = x1[34];
  x2[35] = x1[35];
  x2[36] = x1[36];
  x2[37] = x1[37];
  x2[38] = x1[38];
  x2[39] = x1[39];

  btf_32_neon_type0(-cospi[32], cospi[32], x1[40], x1[55], x2[40], x2[55],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x1[41], x1[54], x2[41], x2[54],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x1[42], x1[53], x2[42], x2[53],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x1[43], x1[52], x2[43], x2[52],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x1[44], x1[51], x2[44], x2[51],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x1[45], x1[50], x2[45], x2[50],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x1[46], x1[49], x2[46], x2[49],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x1[47], x1[48], x2[47], x2[48],
                    *v_cos_bit);
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
  x3[0] = vaddq_s32(x2[0], x2[15]);
  x3[15] = vsubq_s32(x2[0], x2[15]);
  x3[1] = vaddq_s32(x2[1], x2[14]);
  x3[14] = vsubq_s32(x2[1], x2[14]);
  x3[2] = vaddq_s32(x2[2], x2[13]);
  x3[13] = vsubq_s32(x2[2], x2[13]);
  x3[3] = vaddq_s32(x2[3], x2[12]);
  x3[12] = vsubq_s32(x2[3], x2[12]);
  x3[4] = vaddq_s32(x2[4], x2[11]);
  x3[11] = vsubq_s32(x2[4], x2[11]);
  x3[5] = vaddq_s32(x2[5], x2[10]);
  x3[10] = vsubq_s32(x2[5], x2[10]);
  x3[6] = vaddq_s32(x2[6], x2[9]);
  x3[9] = vsubq_s32(x2[6], x2[9]);
  x3[7] = vaddq_s32(x2[7], x2[8]);
  x3[8] = vsubq_s32(x2[7], x2[8]);
  x3[16] = x2[16];
  x3[17] = x2[17];
  x3[18] = x2[18];
  x3[19] = x2[19];
  btf_32_neon_type0(-cospi[32], cospi[32], x2[20], x2[27], x3[20], x3[27],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x2[21], x2[26], x3[21], x3[26],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x2[22], x2[25], x3[22], x3[25],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x2[23], x2[24], x3[23], x3[24],
                    *v_cos_bit);
  x3[28] = x2[28];
  x3[29] = x2[29];
  x3[30] = x2[30];
  x3[31] = x2[31];
  x3[32] = vaddq_s32(x2[32], x2[47]);
  x3[47] = vsubq_s32(x2[32], x2[47]);
  x3[33] = vaddq_s32(x2[33], x2[46]);
  x3[46] = vsubq_s32(x2[33], x2[46]);
  x3[34] = vaddq_s32(x2[34], x2[45]);
  x3[45] = vsubq_s32(x2[34], x2[45]);
  x3[35] = vaddq_s32(x2[35], x2[44]);
  x3[44] = vsubq_s32(x2[35], x2[44]);
  x3[36] = vaddq_s32(x2[36], x2[43]);
  x3[43] = vsubq_s32(x2[36], x2[43]);
  x3[37] = vaddq_s32(x2[37], x2[42]);
  x3[42] = vsubq_s32(x2[37], x2[42]);
  x3[38] = vaddq_s32(x2[38], x2[41]);
  x3[41] = vsubq_s32(x2[38], x2[41]);
  x3[39] = vaddq_s32(x2[39], x2[40]);
  x3[40] = vsubq_s32(x2[39], x2[40]);
  x3[48] = vsubq_s32(x2[63], x2[48]);
  x3[63] = vaddq_s32(x2[63], x2[48]);
  x3[49] = vsubq_s32(x2[62], x2[49]);
  x3[62] = vaddq_s32(x2[62], x2[49]);
  x3[50] = vsubq_s32(x2[61], x2[50]);
  x3[61] = vaddq_s32(x2[61], x2[50]);
  x3[51] = vsubq_s32(x2[60], x2[51]);
  x3[60] = vaddq_s32(x2[60], x2[51]);
  x3[52] = vsubq_s32(x2[59], x2[52]);
  x3[59] = vaddq_s32(x2[59], x2[52]);
  x3[53] = vsubq_s32(x2[58], x2[53]);
  x3[58] = vaddq_s32(x2[58], x2[53]);
  x3[54] = vsubq_s32(x2[57], x2[54]);
  x3[57] = vaddq_s32(x2[57], x2[54]);
  x3[55] = vsubq_s32(x2[56], x2[55]);
  x3[56] = vaddq_s32(x2[56], x2[55]);

  // stage 4
  int32x4_t x4[64];
  x4[0] = vaddq_s32(x3[0], x3[7]);
  x4[7] = vsubq_s32(x3[0], x3[7]);
  x4[1] = vaddq_s32(x3[1], x3[6]);
  x4[6] = vsubq_s32(x3[1], x3[6]);
  x4[2] = vaddq_s32(x3[2], x3[5]);
  x4[5] = vsubq_s32(x3[2], x3[5]);
  x4[3] = vaddq_s32(x3[3], x3[4]);
  x4[4] = vsubq_s32(x3[3], x3[4]);
  x4[8] = x3[8];
  x4[9] = x3[9];
  btf_32_neon_type0(-cospi[32], cospi[32], x3[10], x3[13], x4[10], x4[13],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[32], cospi[32], x3[11], x3[12], x4[11], x4[12],
                    *v_cos_bit);
  x4[14] = x3[14];
  x4[15] = x3[15];
  x4[16] = vaddq_s32(x3[16], x3[23]);
  x4[23] = vsubq_s32(x3[16], x3[23]);
  x4[17] = vaddq_s32(x3[17], x3[22]);
  x4[22] = vsubq_s32(x3[17], x3[22]);
  x4[18] = vaddq_s32(x3[18], x3[21]);
  x4[21] = vsubq_s32(x3[18], x3[21]);
  x4[19] = vaddq_s32(x3[19], x3[20]);
  x4[20] = vsubq_s32(x3[19], x3[20]);
  x4[24] = vsubq_s32(x3[31], x3[24]);
  x4[31] = vaddq_s32(x3[31], x3[24]);
  x4[25] = vsubq_s32(x3[30], x3[25]);
  x4[30] = vaddq_s32(x3[30], x3[25]);
  x4[26] = vsubq_s32(x3[29], x3[26]);
  x4[29] = vaddq_s32(x3[29], x3[26]);
  x4[27] = vsubq_s32(x3[28], x3[27]);
  x4[28] = vaddq_s32(x3[28], x3[27]);
  x4[32] = x3[32];
  x4[33] = x3[33];
  x4[34] = x3[34];
  x4[35] = x3[35];

  btf_32_neon_type0(-cospi[16], cospi[48], x3[36], x3[59], x4[36], x4[59],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[16], cospi[48], x3[37], x3[58], x4[37], x4[58],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[16], cospi[48], x3[38], x3[57], x4[38], x4[57],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[16], cospi[48], x3[39], x3[56], x4[39], x4[56],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], x3[40], x3[55], x4[40], x4[55],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], x3[41], x3[54], x4[41], x4[54],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], x3[42], x3[53], x4[42], x4[53],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], x3[43], x3[52], x4[43], x4[52],
                    *v_cos_bit);
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
  x5[0] = vaddq_s32(x4[0], x4[3]);
  x5[3] = vsubq_s32(x4[0], x4[3]);
  x5[1] = vaddq_s32(x4[1], x4[2]);
  x5[2] = vsubq_s32(x4[1], x4[2]);
  x5[4] = x4[4];

  btf_32_neon_type0(-cospi[32], cospi[32], x4[5], x4[6], x5[5], x5[6],
                    *v_cos_bit);
  x5[7] = x4[7];
  x5[8] = vaddq_s32(x4[8], x4[11]);
  x5[11] = vsubq_s32(x4[8], x4[11]);
  x5[9] = vaddq_s32(x4[9], x4[10]);
  x5[10] = vsubq_s32(x4[9], x4[10]);
  x5[12] = vsubq_s32(x4[15], x4[12]);
  x5[15] = vaddq_s32(x4[15], x4[12]);
  x5[13] = vsubq_s32(x4[14], x4[13]);
  x5[14] = vaddq_s32(x4[14], x4[13]);
  x5[16] = x4[16];
  x5[17] = x4[17];

  btf_32_neon_type0(-cospi[16], cospi[48], x4[18], x4[29], x5[18], x5[29],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[16], cospi[48], x4[19], x4[28], x5[19], x5[28],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], x4[20], x4[27], x5[20], x5[27],
                    *v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], x4[21], x4[26], x5[21], x5[26],
                    *v_cos_bit);
  x5[22] = x4[22];
  x5[23] = x4[23];
  x5[24] = x4[24];
  x5[25] = x4[25];
  x5[30] = x4[30];
  x5[31] = x4[31];
  x5[32] = vaddq_s32(x4[32], x4[39]);
  x5[39] = vsubq_s32(x4[32], x4[39]);
  x5[33] = vaddq_s32(x4[33], x4[38]);
  x5[38] = vsubq_s32(x4[33], x4[38]);
  x5[34] = vaddq_s32(x4[34], x4[37]);
  x5[37] = vsubq_s32(x4[34], x4[37]);
  x5[35] = vaddq_s32(x4[35], x4[36]);
  x5[36] = vsubq_s32(x4[35], x4[36]);
  x5[40] = vsubq_s32(x4[47], x4[40]);
  x5[47] = vaddq_s32(x4[47], x4[40]);
  x5[41] = vsubq_s32(x4[46], x4[41]);
  x5[46] = vaddq_s32(x4[46], x4[41]);
  x5[42] = vsubq_s32(x4[45], x4[42]);
  x5[45] = vaddq_s32(x4[45], x4[42]);
  x5[43] = vsubq_s32(x4[44], x4[43]);
  x5[44] = vaddq_s32(x4[44], x4[43]);
  x5[48] = vaddq_s32(x4[48], x4[55]);
  x5[55] = vsubq_s32(x4[48], x4[55]);
  x5[49] = vaddq_s32(x4[49], x4[54]);
  x5[54] = vsubq_s32(x4[49], x4[54]);
  x5[50] = vaddq_s32(x4[50], x4[53]);
  x5[53] = vsubq_s32(x4[50], x4[53]);
  x5[51] = vaddq_s32(x4[51], x4[52]);
  x5[52] = vsubq_s32(x4[51], x4[52]);
  x5[56] = vsubq_s32(x4[63], x4[56]);
  x5[63] = vaddq_s32(x4[63], x4[56]);
  x5[57] = vsubq_s32(x4[62], x4[57]);
  x5[62] = vaddq_s32(x4[62], x4[57]);
  x5[58] = vsubq_s32(x4[61], x4[58]);
  x5[61] = vaddq_s32(x4[61], x4[58]);
  x5[59] = vsubq_s32(x4[60], x4[59]);
  x5[60] = vaddq_s32(x4[60], x4[59]);
}

static void av1_fdct64_new_neon(int32x4_t *input, int32x4_t *output,
                                int8_t cos_bit, const int instride,
                                const int outstride) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  int startidx = 0 * instride;
  int endidx = 63 * instride;

  // stage 1-2-3-4-5
  int32x4_t x5[64];
  av1_fdct64_new_stage12345_neon(input, instride, x5, cospi, &v_cos_bit,
                                 &startidx, &endidx);

  // stage 6
  int32x4_t x6[64];
  btf_32_neon_type0(cospi[32], cospi[32], x5[0], x5[1], x6[0], x6[1],
                    v_cos_bit);
  btf_32_neon_type1(cospi[48], cospi[16], x5[2], x5[3], x6[2], x6[3],
                    v_cos_bit);
  x6[4] = vaddq_s32(x5[4], x5[5]);
  x6[5] = vsubq_s32(x5[4], x5[5]);
  x6[6] = vsubq_s32(x5[7], x5[6]);
  x6[7] = vaddq_s32(x5[7], x5[6]);
  x6[8] = x5[8];
  btf_32_neon_type0(-cospi[16], cospi[48], x5[9], x5[14], x6[9], x6[14],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[48], -cospi[16], x5[10], x5[13], x6[10], x6[13],
                    v_cos_bit);
  x6[11] = x5[11];
  x6[12] = x5[12];
  x6[15] = x5[15];
  x6[16] = vaddq_s32(x5[16], x5[19]);
  x6[19] = vsubq_s32(x5[16], x5[19]);
  x6[17] = vaddq_s32(x5[17], x5[18]);
  x6[18] = vsubq_s32(x5[17], x5[18]);
  x6[20] = vsubq_s32(x5[23], x5[20]);
  x6[23] = vaddq_s32(x5[23], x5[20]);
  x6[21] = vsubq_s32(x5[22], x5[21]);
  x6[22] = vaddq_s32(x5[22], x5[21]);
  x6[24] = vaddq_s32(x5[24], x5[27]);
  x6[27] = vsubq_s32(x5[24], x5[27]);
  x6[25] = vaddq_s32(x5[25], x5[26]);
  x6[26] = vsubq_s32(x5[25], x5[26]);
  x6[28] = vsubq_s32(x5[31], x5[28]);
  x6[31] = vaddq_s32(x5[31], x5[28]);
  x6[29] = vsubq_s32(x5[30], x5[29]);
  x6[30] = vaddq_s32(x5[30], x5[29]);
  x6[32] = x5[32];
  x6[33] = x5[33];

  btf_32_neon_type0(-cospi[40], cospi[24], x5[42], x5[53], x6[42], x6[53],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[40], cospi[24], x5[43], x5[52], x6[43], x6[52],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[24], -cospi[40], x5[44], x5[51], x6[44], x6[51],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[24], -cospi[40], x5[45], x5[50], x6[45], x6[50],
                    v_cos_bit);

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
  btf_32_neon_type1(cospi[24], cospi[40], x6[5], x6[6], x7[5], x7[6],
                    v_cos_bit);

  x7[8] = vaddq_s32(x6[8], x6[9]);
  x7[9] = vsubq_s32(x6[8], x6[9]);
  x7[10] = vsubq_s32(x6[11], x6[10]);
  x7[11] = vaddq_s32(x6[11], x6[10]);
  x7[12] = vaddq_s32(x6[12], x6[13]);
  x7[13] = vsubq_s32(x6[12], x6[13]);
  x7[14] = vsubq_s32(x6[15], x6[14]);
  x7[15] = vaddq_s32(x6[15], x6[14]);
  x7[16] = x6[16];

  btf_32_neon_type0(-cospi[40], cospi[24], x6[21], x6[26], x7[21], x7[26],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[24], -cospi[40], x6[22], x6[25], x7[22], x7[25],
                    v_cos_bit);
  x7[23] = x6[23];
  x7[24] = x6[24];
  x7[27] = x6[27];
  x7[28] = x6[28];
  x7[31] = x6[31];

  btf_32_neon_type0(-cospi[8], cospi[56], x5[34], x5[61], x6[34], x6[61],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[8], cospi[56], x5[35], x5[60], x6[35], x6[60],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[56], -cospi[8], x5[36], x5[59], x6[36], x6[59],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[56], -cospi[8], x5[37], x5[58], x6[37], x6[58],
                    v_cos_bit);
  x6[38] = x5[38];
  x6[39] = x5[39];
  x6[40] = x5[40];
  x6[41] = x5[41];

  btf_32_neon_type1(cospi[56], cospi[8], x6[4], x6[7], x7[4], x7[7], v_cos_bit);
  btf_32_neon_type0(-cospi[8], cospi[56], x6[17], x6[30], x7[17], x7[30],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[56], -cospi[8], x6[18], x6[29], x7[18], x7[29],
                    v_cos_bit);
  x7[19] = x6[19];
  x7[20] = x6[20];

  x7[32] = vaddq_s32(x6[32], x6[35]);
  x7[35] = vsubq_s32(x6[32], x6[35]);
  x7[33] = vaddq_s32(x6[33], x6[34]);
  x7[34] = vsubq_s32(x6[33], x6[34]);
  x7[36] = vsubq_s32(x6[39], x6[36]);
  x7[39] = vaddq_s32(x6[39], x6[36]);
  x7[37] = vsubq_s32(x6[38], x6[37]);
  x7[38] = vaddq_s32(x6[38], x6[37]);
  x7[40] = vaddq_s32(x6[40], x6[43]);
  x7[43] = vsubq_s32(x6[40], x6[43]);
  x7[41] = vaddq_s32(x6[41], x6[42]);
  x7[42] = vsubq_s32(x6[41], x6[42]);
  x7[44] = vsubq_s32(x6[47], x6[44]);
  x7[47] = vaddq_s32(x6[47], x6[44]);
  x7[45] = vsubq_s32(x6[46], x6[45]);
  x7[46] = vaddq_s32(x6[46], x6[45]);
  x7[48] = vaddq_s32(x6[48], x6[51]);
  x7[51] = vsubq_s32(x6[48], x6[51]);
  x7[49] = vaddq_s32(x6[49], x6[50]);
  x7[50] = vsubq_s32(x6[49], x6[50]);
  x7[52] = vsubq_s32(x6[55], x6[52]);
  x7[55] = vaddq_s32(x6[55], x6[52]);
  x7[53] = vsubq_s32(x6[54], x6[53]);
  x7[54] = vaddq_s32(x6[54], x6[53]);
  x7[56] = vaddq_s32(x6[56], x6[59]);
  x7[59] = vsubq_s32(x6[56], x6[59]);
  x7[57] = vaddq_s32(x6[57], x6[58]);
  x7[58] = vsubq_s32(x6[57], x6[58]);
  x7[60] = vsubq_s32(x6[63], x6[60]);
  x7[63] = vaddq_s32(x6[63], x6[60]);
  x7[61] = vsubq_s32(x6[62], x6[61]);
  x7[62] = vaddq_s32(x6[62], x6[61]);

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

  btf_32_neon_type1(cospi[60], cospi[4], x7[8], x7[15], x8[8], x8[15],
                    v_cos_bit);
  btf_32_neon_type1(cospi[28], cospi[36], x7[9], x7[14], x8[9], x8[14],
                    v_cos_bit);
  btf_32_neon_type1(cospi[44], cospi[20], x7[10], x7[13], x8[10], x8[13],
                    v_cos_bit);
  btf_32_neon_type1(cospi[12], cospi[52], x7[11], x7[12], x8[11], x8[12],
                    v_cos_bit);
  x8[16] = vaddq_s32(x7[16], x7[17]);
  x8[17] = vsubq_s32(x7[16], x7[17]);
  x8[18] = vsubq_s32(x7[19], x7[18]);
  x8[19] = vaddq_s32(x7[19], x7[18]);
  x8[20] = vaddq_s32(x7[20], x7[21]);
  x8[21] = vsubq_s32(x7[20], x7[21]);
  x8[22] = vsubq_s32(x7[23], x7[22]);
  x8[23] = vaddq_s32(x7[23], x7[22]);
  x8[24] = vaddq_s32(x7[24], x7[25]);
  x8[25] = vsubq_s32(x7[24], x7[25]);
  x8[26] = vsubq_s32(x7[27], x7[26]);
  x8[27] = vaddq_s32(x7[27], x7[26]);
  x8[28] = vaddq_s32(x7[28], x7[29]);
  x8[29] = vsubq_s32(x7[28], x7[29]);
  x8[30] = vsubq_s32(x7[31], x7[30]);
  x8[31] = vaddq_s32(x7[31], x7[30]);
  x8[32] = x7[32];

  btf_32_neon_type0(-cospi[4], cospi[60], x7[33], x7[62], x8[33], x8[62],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[60], -cospi[4], x7[34], x7[61], x8[34], x8[61],
                    v_cos_bit);
  x8[35] = x7[35];
  x8[36] = x7[36];
  btf_32_neon_type0(-cospi[36], cospi[28], x7[37], x7[58], x8[37], x8[58],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[28], -cospi[36], x7[38], x7[57], x8[38], x8[57],
                    v_cos_bit);
  x8[39] = x7[39];
  x8[40] = x7[40];
  btf_32_neon_type0(-cospi[20], cospi[44], x7[41], x7[54], x8[41], x8[54],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[44], -cospi[20], x7[42], x7[53], x8[42], x8[53],
                    v_cos_bit);
  x8[43] = x7[43];
  x8[44] = x7[44];
  btf_32_neon_type0(-cospi[52], cospi[12], x7[45], x7[50], x8[45], x8[50],
                    v_cos_bit);
  btf_32_neon_type0(-cospi[12], -cospi[52], x7[46], x7[49], x8[46], x8[49],
                    v_cos_bit);
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

  btf_32_neon_type1(cospi[62], cospi[2], x8[16], x8[31], x9[16], x9[31],
                    v_cos_bit);
  btf_32_neon_type1(cospi[30], cospi[34], x8[17], x8[30], x9[17], x9[30],
                    v_cos_bit);
  btf_32_neon_type1(cospi[46], cospi[18], x8[18], x8[29], x9[18], x9[29],
                    v_cos_bit);
  btf_32_neon_type1(cospi[14], cospi[50], x8[19], x8[28], x9[19], x9[28],
                    v_cos_bit);
  btf_32_neon_type1(cospi[54], cospi[10], x8[20], x8[27], x9[20], x9[27],
                    v_cos_bit);
  btf_32_neon_type1(cospi[22], cospi[42], x8[21], x8[26], x9[21], x9[26],
                    v_cos_bit);
  btf_32_neon_type1(cospi[38], cospi[26], x8[22], x8[25], x9[22], x9[25],
                    v_cos_bit);
  btf_32_neon_type1(cospi[6], cospi[58], x8[23], x8[24], x9[23], x9[24],
                    v_cos_bit);

  x9[32] = vaddq_s32(x8[32], x8[33]);
  x9[33] = vsubq_s32(x8[32], x8[33]);
  x9[34] = vsubq_s32(x8[35], x8[34]);
  x9[35] = vaddq_s32(x8[35], x8[34]);
  x9[36] = vaddq_s32(x8[36], x8[37]);
  x9[37] = vsubq_s32(x8[36], x8[37]);
  x9[38] = vsubq_s32(x8[39], x8[38]);
  x9[39] = vaddq_s32(x8[39], x8[38]);
  x9[40] = vaddq_s32(x8[40], x8[41]);
  x9[41] = vsubq_s32(x8[40], x8[41]);
  x9[42] = vsubq_s32(x8[43], x8[42]);
  x9[43] = vaddq_s32(x8[43], x8[42]);
  x9[44] = vaddq_s32(x8[44], x8[45]);
  x9[45] = vsubq_s32(x8[44], x8[45]);
  x9[46] = vsubq_s32(x8[47], x8[46]);
  x9[47] = vaddq_s32(x8[47], x8[46]);
  x9[48] = vaddq_s32(x8[48], x8[49]);
  x9[49] = vsubq_s32(x8[48], x8[49]);
  x9[50] = vsubq_s32(x8[51], x8[50]);
  x9[51] = vaddq_s32(x8[51], x8[50]);
  x9[52] = vaddq_s32(x8[52], x8[53]);
  x9[53] = vsubq_s32(x8[52], x8[53]);
  x9[54] = vsubq_s32(x8[55], x8[54]);
  x9[55] = vaddq_s32(x8[55], x8[54]);
  x9[56] = vaddq_s32(x8[56], x8[57]);
  x9[57] = vsubq_s32(x8[56], x8[57]);
  x9[58] = vsubq_s32(x8[59], x8[58]);
  x9[59] = vaddq_s32(x8[59], x8[58]);
  x9[60] = vaddq_s32(x8[60], x8[61]);
  x9[61] = vsubq_s32(x8[60], x8[61]);
  x9[62] = vsubq_s32(x8[63], x8[62]);
  x9[63] = vaddq_s32(x8[63], x8[62]);

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
  btf_32_neon_type1(cospi[63], cospi[1], x9[32], x9[63], x10[32], x10[63],
                    v_cos_bit);
  btf_32_neon_type1(cospi[31], cospi[33], x9[33], x9[62], x10[33], x10[62],
                    v_cos_bit);
  btf_32_neon_type1(cospi[47], cospi[17], x9[34], x9[61], x10[34], x10[61],
                    v_cos_bit);
  btf_32_neon_type1(cospi[15], cospi[49], x9[35], x9[60], x10[35], x10[60],
                    v_cos_bit);
  btf_32_neon_type1(cospi[55], cospi[9], x9[36], x9[59], x10[36], x10[59],
                    v_cos_bit);
  btf_32_neon_type1(cospi[23], cospi[41], x9[37], x9[58], x10[37], x10[58],
                    v_cos_bit);
  btf_32_neon_type1(cospi[39], cospi[25], x9[38], x9[57], x10[38], x10[57],
                    v_cos_bit);
  btf_32_neon_type1(cospi[7], cospi[57], x9[39], x9[56], x10[39], x10[56],
                    v_cos_bit);
  btf_32_neon_type1(cospi[59], cospi[5], x9[40], x9[55], x10[40], x10[55],
                    v_cos_bit);
  btf_32_neon_type1(cospi[27], cospi[37], x9[41], x9[54], x10[41], x10[54],
                    v_cos_bit);
  btf_32_neon_type1(cospi[43], cospi[21], x9[42], x9[53], x10[42], x10[53],
                    v_cos_bit);
  btf_32_neon_type1(cospi[11], cospi[53], x9[43], x9[52], x10[43], x10[52],
                    v_cos_bit);
  btf_32_neon_type1(cospi[51], cospi[13], x9[44], x9[51], x10[44], x10[51],
                    v_cos_bit);
  btf_32_neon_type1(cospi[19], cospi[45], x9[45], x9[50], x10[45], x10[50],
                    v_cos_bit);
  btf_32_neon_type1(cospi[35], cospi[29], x9[46], x9[49], x10[46], x10[49],
                    v_cos_bit);
  btf_32_neon_type1(cospi[3], cospi[61], x9[47], x9[48], x10[47], x10[48],
                    v_cos_bit);

  startidx = 0 * outstride;
  endidx = 63 * outstride;
  // stage 11
  output[startidx] = x10[0];
  output[endidx] = x10[63];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[32];
  output[endidx] = x10[31];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[16];
  output[endidx] = x10[47];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[48];
  output[endidx] = x10[15];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[8];
  output[endidx] = x10[55];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[40];
  output[endidx] = x10[23];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[24];
  output[endidx] = x10[39];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[56];
  output[endidx] = x10[7];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[4];
  output[endidx] = x10[59];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[36];
  output[endidx] = x10[27];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[20];
  output[endidx] = x10[43];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[52];
  output[endidx] = x10[11];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[12];
  output[endidx] = x10[51];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[44];
  output[endidx] = x10[19];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[28];
  output[endidx] = x10[35];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[60];
  output[endidx] = x10[3];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[2];
  output[endidx] = x10[61];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[34];
  output[endidx] = x10[29];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[18];
  output[endidx] = x10[45];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[50];
  output[endidx] = x10[13];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[10];
  output[endidx] = x10[53];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[42];
  output[endidx] = x10[21];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[26];
  output[endidx] = x10[37];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[58];
  output[endidx] = x10[5];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[6];
  output[endidx] = x10[57];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[38];
  output[endidx] = x10[25];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[22];
  output[endidx] = x10[41];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[54];
  output[endidx] = x10[9];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[14];
  output[endidx] = x10[49];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[46];
  output[endidx] = x10[17];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[30];
  output[endidx] = x10[33];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[62];
  output[endidx] = x10[1];
}

void av1_idtx32_new_neon(int32x4_t *input, int32x4_t *output, int cos_bit,
                         const int col_num) {
  (void)cos_bit;
  for (int i = 0; i < 32; i++) {
    output[i * col_num] = vshlq_n_s32(input[i * col_num], 2);
  }
}

static const fwd_transform_1d_neon col_highbd_txfm8x32_arr[TX_TYPES] = {
  av1_fdct32_new_neon,  // DCT_DCT
  NULL,                 // ADST_DCT
  NULL,                 // DCT_ADST
  NULL,                 // ADST_ADST
  NULL,                 // FLIPADST_DCT
  NULL,                 // DCT_FLIPADST
  NULL,                 // FLIPADST_FLIPADST
  NULL,                 // ADST_FLIPADST
  NULL,                 // FLIPADST_ADST
  av1_idtx32_new_neon,  // IDTX
  NULL,                 // V_DCT
  NULL,                 // H_DCT
  NULL,                 // V_ADST
  NULL,                 // H_ADST
  NULL,                 // V_FLIPADST
  NULL                  // H_FLIPADST
};

static const fwd_transform_1d_neon row_highbd_txfm8x32_arr[TX_TYPES] = {
  fdct16x16_neon,  // DCT_DCT
  NULL,            // ADST_DCT
  NULL,            // DCT_ADST
  NULL,            // ADST_ADST
  NULL,            // FLIPADST_DCT
  NULL,            // DCT_FLIPADST
  NULL,            // FLIPADST_FLIPADST
  NULL,            // ADST_FLIPADST
  NULL,            // FLIPADST_ADST
  idtx16x16_neon,  // IDTX
  NULL,            // V_DCT
  NULL,            // H_DCT
  NULL,            // V_ADST
  NULL,            // H_ADST
  NULL,            // V_FLIPADST
  NULL             // H_FLIPADST
};

void av1_fwd_txfm2d_16x8_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;
  int32x4_t in[32], out[32];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X8];
  const int txw_idx = get_txw_idx(TX_16X8);
  const int txh_idx = get_txh_idx(TX_16X8);
  const fwd_transform_1d_neon col_txfm = col_highbd_txfm8x8_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = row_highbd_txfm8x16_arr[tx_type];
  int bit = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int32x4_t v_shift1 = vdupq_n_s32(shift[1]);
  for (int i = 0; i < 2; i++) {
    load_buffer_8x8(input + i * 8, in, stride, ud_flip, 0, shift[0]);
    col_txfm(in, in, bit, 2);
    col_txfm_8x8_rounding(in, &v_shift1);
    transpose_8x8(in, out + i * 16);
  }

  if (lr_flip) {
    flip_buf_neon(in, out, 32);
    row_txfm(in, out, bit, 2);
  } else {
    row_txfm(out, out, bit, 2);
  }

  for (int i = 0; i < 2; i++) {
    av1_round_shift_rect_array_32_neon(out + i * 16, in, 16, -shift[2],
                                       NewSqrt2);
    write_buffer_8x8(in, coeff + i * 64);
  }
}

void av1_fwd_txfm2d_8x16_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;
  int32x4_t in[32], out[32];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X16];
  const int txw_idx = get_txw_idx(TX_8X16);
  const int txh_idx = get_txh_idx(TX_8X16);
  const fwd_transform_1d_neon col_txfm = col_highbd_txfm8x16_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = row_highbd_txfm8x8_arr[tx_type];
  int bit = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  load_buffer_8x16(input, in, stride, ud_flip, lr_flip, shift[0]);
  col_txfm(in, in, bit, 2);
  const int32x4_t v_shift1 = vdupq_n_s32(shift[1]);
  col_txfm_8x16_rounding(in, &v_shift1);
  transpose_8x8(in, out);
  transpose_8x8(in + 16, out + 16);

  for (int i = 0; i < 2; i++) {
    row_txfm(out + i * 16, out, bit, 2);
    av1_round_shift_rect_array_32_neon(out, out, 16, -shift[2], NewSqrt2);
    write_buffer_16x8(out, coeff + i * 8, 16);
  }
}

static INLINE void transpose_8nx8n(const int32x4_t *input, int32x4_t *output,
                                   const int width, const int height) {
  const int numcol = height >> 2;
  const int numrow = width >> 2;
  for (int j = 0; j < numrow; j++) {
    for (int i = 0; i < numcol; i++) {
      TRANSPOSE_4X4(input[i * width + j + (numrow * 0)],
                    input[i * width + j + (numrow * 1)],
                    input[i * width + j + (numrow * 2)],
                    input[i * width + j + (numrow * 3)],
                    output[j * height + i + (numcol * 0)],
                    output[j * height + i + (numcol * 1)],
                    output[j * height + i + (numcol * 2)],
                    output[j * height + i + (numcol * 3)]);
    }
  }
}

#if !CONFIG_REALTIME_ONLY
void av1_fwd_txfm2d_4x16_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;

  int32x4_t in[16];
  int32x4_t *outcoeff128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X16];
  const int txw_idx = get_txw_idx(TX_4X16);
  const int txh_idx = get_txh_idx(TX_4X16);
  const int txfm_size_col = tx_size_wide[TX_4X16];
  const int txfm_size_row = tx_size_high[TX_4X16];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const fwd_transform_1d_neon col_txfm = col_highbd_txfm8x16_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = row_highbd_txfm4x4_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  // col transform
  int32x4_t v_shift0 = vdupq_n_s32(shift[0]);
  load_buffer_4x16(input, in, stride, ud_flip, lr_flip, &v_shift0);
  col_txfm(in, outcoeff128, bitcol, 1);
  const int32x4_t v_shift1 = vdupq_n_s32(shift[1]);
  col_txfm_8x8_rounding(outcoeff128, &v_shift1);
  transpose_8nx8n(outcoeff128, in, txfm_size_col, txfm_size_row);

  // row transform
  for (int i = 0; i < txfm_size_col; i++) {
    int32x4_t tmp[4];
    row_txfm(in + i, tmp, bitrow, txfm_size_row >> 2);
    store_output_w4(coeff + i * 4, tmp, txfm_size_row, txfm_size_col);
  }
}
#endif

void av1_fwd_txfm2d_16x4_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  (void)bd;

  int32x4_t in[16];
  int32x4_t *outcoeff128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X4];
  const int txw_idx = get_txw_idx(TX_16X4);
  const int txh_idx = get_txh_idx(TX_16X4);
  const int txfm_size_col = tx_size_wide[TX_16X4];
  const int txfm_size_row = tx_size_high[TX_16X4];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const fwd_transform_1d_neon col_txfm = col_highbd_txfm4x4_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = row_highbd_txfm8x16_arr[tx_type];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // col transform
  const int32x4_t v_shift0 = vdupq_n_s32(shift[0]);
  load_buffer_16x4(input, in, stride, ud_flip, lr_flip, &v_shift0);

  for (int i = 0; i < (txfm_size_col >> 2); i++) {
    int32x4_t *cur_in = &in[i * txfm_size_row];
    col_txfm(cur_in, cur_in, bitcol, 1);
    transpose_4x4(cur_in, cur_in);
  }
  const int32x4_t v_shift1 = vdupq_n_s32(shift[1]);
  col_txfm_8x8_rounding(in, &v_shift1);

  // row transform
  row_txfm(in, outcoeff128, bitrow, 1);
}

void av1_fwd_txfm2d_16x32_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)bd;

  int32x4_t in[128];
  int32x4_t *outcoef128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X32];
  const int txw_idx = get_txw_idx(TX_16X32);
  const int txh_idx = get_txh_idx(TX_16X32);
  const fwd_transform_1d_neon col_txfm = col_highbd_txfm8x32_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = row_highbd_txfm8x32_arr[tx_type];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];

  // column transform
  load_buffer_16x16(input, in, stride, 0, 0, shift[0]);
  load_buffer_16x16(input + 16 * stride, in + 64, stride, 0, 0, shift[0]);

  for (int i = 0; i < 4; i++) {
    col_txfm((in + i), (in + i), bitcol, 4);
  }

  const int32x4_t v_shift = vdupq_n_s32(shift[1]);
  col_txfm_16x16_rounding(&in[0], &v_shift);
  col_txfm_16x16_rounding(&in[64], &v_shift);
  transpose_8nx8n(in, outcoef128, 16, 32);

  // row transform
  row_txfm(outcoef128, in, bitrow, 8);
  av1_round_shift_rect_array_32_neon(in, outcoef128, 128, -shift[2], NewSqrt2);
}

void av1_fwd_txfm2d_32x64_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)tx_type;
  (void)bd;

  int32x4_t in[512];
  int32x4_t *outcoef128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X64];
  const int txw_idx = get_txw_idx(TX_32X64);
  const int txh_idx = get_txh_idx(TX_32X64);
  const int txfm_size_col = tx_size_wide[TX_32X64];
  const int txfm_size_row = tx_size_high[TX_32X64];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int num_row = txfm_size_row >> 2;
  const int num_col = txfm_size_col >> 2;

  // column transform
  load_buffer_32x8n(input, in, stride, 0, 0, shift[0], txfm_size_row);
  for (int i = 0; i < num_col; i++) {
    av1_fdct64_new_neon((in + i), (in + i), bitcol, num_col, num_col);
  }

  const int32x4_t v_shift = vdupq_n_s32(shift[1]);
  for (int i = 0; i < num_col; i++) {
    col_txfm_16x16_rounding((in + i * txfm_size_row), &v_shift);
  }
  transpose_8nx8n(in, outcoef128, txfm_size_col, txfm_size_row);

  // row transform
  for (int i = 0; i < num_row; i++) {
    av1_fdct32_new_neon((outcoef128 + i), (in + i), bitrow, num_row);
  }
  for (int i = 0; i < txfm_size_col; i++) {
    av1_round_shift_rect_array_32_neon(in + i * 16, outcoef128 + i * 8, 8,
                                       -shift[2], NewSqrt2);
  }
}

void av1_fwd_txfm2d_64x32_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  (void)tx_type;
  int32x4_t in[512];
  int32x4_t *outcoef128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_64X32];
  const int txw_idx = get_txw_idx(TX_64X32);
  const int txh_idx = get_txh_idx(TX_64X32);
  const int txfm_size_col = tx_size_wide[TX_64X32];
  const int txfm_size_row = tx_size_high[TX_64X32];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int num_row = txfm_size_row >> 2;
  const int num_col = txfm_size_col >> 2;

  // column transform
  const int32x4_t v_shift0 = vdupq_n_s32(shift[0]);
  for (int i = 0; i < 32; i++) {
    load_buffer_4x4(input + 0 + i * stride, in + 0 + i * 16, 4, 0, 0,
                    &v_shift0);
    load_buffer_4x4(input + 16 + i * stride, in + 4 + i * 16, 4, 0, 0,
                    &v_shift0);
    load_buffer_4x4(input + 32 + i * stride, in + 8 + i * 16, 4, 0, 0,
                    &v_shift0);
    load_buffer_4x4(input + 48 + i * stride, in + 12 + i * 16, 4, 0, 0,
                    &v_shift0);
  }

  for (int i = 0; i < num_col; i++) {
    av1_fdct32_new_neon((in + i), (in + i), bitcol, num_col);
  }

  const int32x4_t v_shift1 = vdupq_n_s32(shift[1]);
  for (int i = 0; i < num_row; i++) {
    col_txfm_16x16_rounding((in + i * txfm_size_col), &v_shift1);
  }
  transpose_8nx8n(in, outcoef128, txfm_size_col, txfm_size_row);

  // row transform
  for (int i = 0; i < num_row; i++) {
    av1_fdct64_new_neon((outcoef128 + i), (in + i), bitrow, num_row, num_row);
  }
  av1_round_shift_rect_array_32_neon(in, outcoef128, 512, -shift[2], NewSqrt2);
  (void)bd;
}

void av1_fwd_txfm2d_32x16_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  int32x4_t in[128];
  int32x4_t *outcoef128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X16];
  const int txw_idx = get_txw_idx(TX_32X16);
  const int txh_idx = get_txh_idx(TX_32X16);
  const fwd_transform_1d_neon col_txfm = row_highbd_txfm8x32_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = col_highbd_txfm8x32_arr[tx_type];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];

  // column transform
  load_buffer_32x8n(input, in, stride, 0, 0, shift[0], 16);
  col_txfm(in, in, bitcol, 8);
  const int32x4_t v_shift = vdupq_n_s32(shift[1]);
  col_txfm_16x16_rounding(&in[0], &v_shift);
  col_txfm_16x16_rounding(&in[64], &v_shift);
  transpose_8nx8n(in, outcoef128, 32, 16);

  // row transform
  for (int i = 0; i < 4; i++) {
    row_txfm((outcoef128 + i), (in + i), bitrow, 4);
  }
  av1_round_shift_rect_array_32_neon(in, outcoef128, 128, -shift[2], NewSqrt2);
  (void)bd;
}

#if !CONFIG_REALTIME_ONLY
void av1_fwd_txfm2d_8x32_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  int32x4_t in[64];
  int32x4_t *outcoef128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X32];
  const int txw_idx = get_txw_idx(TX_8X32);
  const int txh_idx = get_txh_idx(TX_8X32);
  const fwd_transform_1d_neon col_txfm = col_highbd_txfm8x32_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = row_highbd_txfm32x8_arr[tx_type];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];

  const int txfm_size_col = tx_size_wide[TX_8X32];
  const int txfm_size_row = tx_size_high[TX_8X32];
  const int num_col = txfm_size_col >> 2;

  // column transform
  load_buffer_8x16(input, in, stride, 0, 0, shift[0]);
  load_buffer_8x16(input + (txfm_size_row >> 1) * stride, in + txfm_size_row,
                   stride, 0, 0, shift[0]);

  for (int i = 0; i < num_col; i++) {
    col_txfm((in + i), (in + i), bitcol, num_col);
  }

  const int32x4_t v_shift = vdupq_n_s32(shift[1]);
  col_txfm_16x16_rounding(in, &v_shift);
  transpose_8nx8n(in, outcoef128, txfm_size_col, txfm_size_row);

  // row transform
  for (int i = 0; i < txfm_size_col; i += 2) {
    row_txfm((outcoef128 + i), (outcoef128 + i), bitrow, txfm_size_col);
  }
  (void)bd;
}

void av1_fwd_txfm2d_32x8_neon(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  int32x4_t in[64];
  int32x4_t *outcoef128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X8];
  const int txw_idx = get_txw_idx(TX_32X8);
  const int txh_idx = get_txh_idx(TX_32X8);
  const fwd_transform_1d_neon col_txfm = row_highbd_txfm32x8_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = col_highbd_txfm8x32_arr[tx_type];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];

  const int txfm_size_col = tx_size_wide[TX_32X8];
  const int txfm_size_row = tx_size_high[TX_32X8];
  const int num_col = txfm_size_row >> 2;

  // column transform
  load_buffer_32x8n(input, in, stride, 0, 0, shift[0], 8);
  for (int i = 0; i < txfm_size_row; i += 2) {
    col_txfm((in + i), (in + i), bitcol, txfm_size_row);
  }

  const int32x4_t v_shift = vdupq_n_s32(shift[1]);
  col_txfm_16x16_rounding(&in[0], &v_shift);
  transpose_8nx8n(in, outcoef128, txfm_size_col, txfm_size_row);

  // row transform
  for (int i = 0; i < num_col; i++) {
    row_txfm((outcoef128 + i), (outcoef128 + i), bitrow, num_col);
  }
  (void)bd;
}
#endif

void av1_fwd_txfm2d_4x8_neon(const int16_t *input, int32_t *coeff, int stride,
                             TX_TYPE tx_type, int bd) {
  int32x4_t in[8];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X8];
  const int txw_idx = get_txw_idx(TX_4X8);
  const int txh_idx = get_txh_idx(TX_4X8);
  const int txfm_size_col = tx_size_wide[TX_4X8];
  const int txfm_size_row = tx_size_high[TX_4X8];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const fwd_transform_1d_neon col_txfm = col_highbd_txfm4x8_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = row_highbd_txfm4x4_arr[tx_type];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int32x4_t v_shift0 = vdupq_n_s32(shift[0]);
  load_buffer_4x8(input, in, stride, ud_flip, lr_flip, &v_shift0);
  col_txfm(in, in, bitcol, 1);
  int32x4_t v_shift1 = vdupq_n_s32(shift[1]);
  col_txfm_4x8_rounding(in, &v_shift1);

  for (int i = 0; i < 2; i++) {
    int32x4_t *cur_in = &in[i * 4];
    transpose_4x4(cur_in, cur_in);
    row_txfm(cur_in, cur_in, bitrow, 1);
    av1_round_shift_rect_array_32_neon(cur_in, cur_in, txfm_size_col, -shift[2],
                                       NewSqrt2);
    store_output_w4(coeff + i * 4, cur_in, txfm_size_row, 4);
  }
  (void)bd;
}

void av1_fwd_txfm2d_8x4_neon(const int16_t *input, int32_t *coeff, int stride,
                             TX_TYPE tx_type, int bd) {
  int32x4_t in[8];
  int32x4_t *outcoeff128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X4];
  const int txw_idx = get_txw_idx(TX_8X4);
  const int txh_idx = get_txh_idx(TX_8X4);
  const int txfm_size_col = tx_size_wide[TX_8X4];
  const int txfm_size_row = tx_size_high[TX_8X4];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const fwd_transform_1d_neon col_txfm = col_highbd_txfm4x4_arr[tx_type];
  const fwd_transform_1d_neon row_txfm = row_highbd_txfm4x8_arr[tx_type];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  // col tranform
  int32x4_t v_shift0 = vdupq_n_s32(shift[0]);
  load_buffer_8x4(input, in, stride, ud_flip, lr_flip, &v_shift0);
  for (int i = 0; i < 2; i++) {
    int32x4_t *cur_in = &in[i * txfm_size_row];
    col_txfm(cur_in, cur_in, bitcol, 1);
    transpose_4x4(cur_in, cur_in);
  }
  int32x4_t v_shift1 = vdupq_n_s32(shift[1]);
  col_txfm_4x8_rounding(in, &v_shift1);

  // row tranform
  row_txfm(in, outcoeff128, bitrow, 1);
  av1_round_shift_rect_array_32_neon(outcoeff128, outcoeff128, txfm_size_col,
                                     -shift[2], NewSqrt2);
  (void)bd;
}

#if !CONFIG_REALTIME_ONLY
void av1_fwd_txfm2d_16x64_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  int32x4_t in[256];
  int32x4_t *outcoeff128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X64];
  const int txw_idx = get_txw_idx(TX_16X64);
  const int txh_idx = get_txh_idx(TX_16X64);
  const int txfm_size_col = tx_size_wide[TX_16X64];
  const int txfm_size_row = tx_size_high[TX_16X64];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int num_col = txfm_size_col >> 2;
  // col tranform
  const int32x4_t v_shift0 = vdupq_n_s32(shift[0]);
  for (int i = 0; i < txfm_size_row; i += num_col) {
    load_buffer_4x4(input + (i + 0) * stride, in + (i + 0) * num_col, num_col,
                    ud_flip, lr_flip, &v_shift0);
    load_buffer_4x4(input + (i + 1) * stride, in + (i + 1) * num_col, num_col,
                    ud_flip, lr_flip, &v_shift0);
    load_buffer_4x4(input + (i + 2) * stride, in + (i + 2) * num_col, num_col,
                    ud_flip, lr_flip, &v_shift0);
    load_buffer_4x4(input + (i + 3) * stride, in + (i + 3) * num_col, num_col,
                    ud_flip, lr_flip, &v_shift0);
  }

  for (int i = 0; i < num_col; i++) {
    av1_fdct64_new_neon(in + i, outcoeff128 + i, bitcol, num_col, num_col);
  }

  const int32x4_t v_shift = vdupq_n_s32(shift[1]);
  col_txfm_16x16_rounding(outcoeff128, &v_shift);
  col_txfm_16x16_rounding(outcoeff128 + 64, &v_shift);
  col_txfm_16x16_rounding(outcoeff128 + 128, &v_shift);
  col_txfm_16x16_rounding(outcoeff128 + 192, &v_shift);

  transpose_8nx8n(outcoeff128, in, txfm_size_col, 32);
  fdct16x16_neon(in, outcoeff128, bitrow, 8);
  (void)bd;
}

void av1_fwd_txfm2d_64x16_neon(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  int32x4_t in[256];
  int32x4_t *outcoeff128 = (int32x4_t *)coeff;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_64X16];
  const int txw_idx = get_txw_idx(TX_64X16);
  const int txh_idx = get_txh_idx(TX_64X16);
  const int txfm_size_col = tx_size_wide[TX_64X16];
  const int txfm_size_row = tx_size_high[TX_64X16];
  int bitcol = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int bitrow = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  // col tranform
  const int32x4_t v_shift0 = vdupq_n_s32(shift[0]);
  for (int i = 0; i < txfm_size_row; i++) {
    load_buffer_4x4(input + 0 + i * stride, in + 0 + i * txfm_size_row, 4,
                    ud_flip, lr_flip, &v_shift0);
    load_buffer_4x4(input + 16 + i * stride, in + 4 + i * txfm_size_row, 4,
                    ud_flip, lr_flip, &v_shift0);
    load_buffer_4x4(input + 32 + i * stride, in + 8 + i * txfm_size_row, 4,
                    ud_flip, lr_flip, &v_shift0);
    load_buffer_4x4(input + 48 + i * stride, in + 12 + i * txfm_size_row, 4,
                    ud_flip, lr_flip, &v_shift0);
  }

  fdct16x16_neon(in, outcoeff128, bitcol, txfm_size_row);
  const int32x4_t v_shift = vdupq_n_s32(shift[1]);
  col_txfm_16x16_rounding(outcoeff128, &v_shift);
  col_txfm_16x16_rounding(outcoeff128 + 64, &v_shift);
  col_txfm_16x16_rounding(outcoeff128 + 128, &v_shift);
  col_txfm_16x16_rounding(outcoeff128 + 192, &v_shift);

  transpose_8nx8n(outcoeff128, in, txfm_size_col, txfm_size_row);
  for (int i = 0; i < 4; i++) {
    av1_fdct64_new_neon(in + i, outcoeff128 + i, bitrow, 4, 4);
  }
  memset(coeff + txfm_size_row * 32, 0, txfm_size_row * 32 * sizeof(*coeff));
  (void)bd;
}
#endif

static void fdct64_new_neon(int32x4_t *input, int32x4_t *output,
                            const int8_t cos_bit, const int8_t *stage_range) {
  const int txfm_size = 64;
  const int num_per_128 = 4;
  int col_num = txfm_size / num_per_128;
  (void)stage_range;
  for (int col = 0; col < col_num; col++) {
    av1_fdct64_new_neon((input + col), (output + col), cos_bit, col_num,
                        col_num);
  }
}

static void fdct32_new_neon(int32x4_t *input, int32x4_t *output,
                            const int8_t cos_bit, const int8_t *stage_range) {
  const int txfm_size = 32;
  const int num_per_128 = 4;
  int col_num = txfm_size / num_per_128;
  int col;
  (void)stage_range;
  for (col = 0; col < col_num; col++) {
    av1_fdct32_new_neon((input + col), (output + col), cos_bit, col_num);
  }
}

static void idtx32x32_neon(int32x4_t *input, int32x4_t *output,
                           const int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;

  for (int i = 0; i < 8; i++) {
    av1_idtx32_new_neon(&input[i * 32], &output[i * 32], cos_bit, 1);
  }
}

typedef void (*TxfmFuncNEON)(int32x4_t *input, int32x4_t *output,
                             const int8_t cos_bit, const int8_t *stage_range);

static INLINE TxfmFuncNEON fwd_txfm_type_to_func(TXFM_TYPE txfm_type) {
  switch (txfm_type) {
    case TXFM_TYPE_DCT32: return fdct32_new_neon; break;
    case TXFM_TYPE_DCT64: return fdct64_new_neon; break;
    case TXFM_TYPE_IDENTITY32: return idtx32x32_neon; break;
    default: assert(0);
  }
  return NULL;
}

static INLINE void int16_array_with_stride_to_int32_array_without_stride(
    const int16_t *input, int stride, int32_t *output, int txfm1d_size) {
  int r, c;
  for (r = 0; r < txfm1d_size; r++) {
    for (c = 0; c < txfm1d_size; c++) {
      output[r * txfm1d_size + c] = (int32_t)input[r * stride + c];
    }
  }
}

static INLINE void av1_round_shift_array_32_neon(int32x4_t *input,
                                                 int32x4_t *output,
                                                 const int size,
                                                 const int bit) {
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  for (int i = 0; i < size; i++) output[i] = vrshlq_s32(input[i], v_bit);
}

static INLINE void transpose_32_4x4(int stride, const int32x4_t *input,
                                    int32x4_t *output) {
  int32x4x2_t temp01 = vzipq_s32(input[0 * stride], input[2 * stride]);
  int32x4x2_t temp23 = vzipq_s32(input[1 * stride], input[3 * stride]);

  const int32x4x2_t output01 = vzipq_s32(temp01.val[0], temp23.val[0]);
  const int32x4x2_t output23 = vzipq_s32(temp01.val[1], temp23.val[1]);

  output[0 * stride] = output01.val[0];
  output[1 * stride] = output01.val[1];
  output[2 * stride] = output23.val[0];
  output[3 * stride] = output23.val[1];
}

static INLINE void transpose_32(int txfm_size, const int32x4_t *input,
                                int32x4_t *output) {
  const int num_per_128 = 4;
  const int row_size = txfm_size;
  const int col_size = txfm_size / num_per_128;
  int r, c;

  // transpose each 4x4 block internally
  for (r = 0; r < row_size; r += 4) {
    for (c = 0; c < col_size; c++) {
      transpose_32_4x4(col_size, &input[r * col_size + c],
                       &output[c * 4 * col_size + r / 4]);
    }
  }
}

static INLINE void fwd_txfm2d_64x64_neon(const int16_t *input, int32_t *output,
                                         const int stride,
                                         const TXFM_2D_FLIP_CFG *cfg,
                                         int32_t *txfm_buf) {
  assert(cfg->tx_size < TX_SIZES);
  const int txfm_size = tx_size_wide[cfg->tx_size];
  const int8_t *shift = cfg->shift;
  const int8_t *stage_range_col = cfg->stage_range_col;
  const int8_t cos_bit_col = cfg->cos_bit_col;
  const int8_t cos_bit_row = cfg->cos_bit_row;
  const TxfmFuncNEON txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
  int32x4_t *buf_128 = (int32x4_t *)txfm_buf;
  int32x4_t *out_128 = (int32x4_t *)output;

  const int num_per_128 = 4;
  int txfm2d_size_128 = txfm_size * txfm_size / num_per_128;
  int col_num = txfm_size / num_per_128;

  int16_array_with_stride_to_int32_array_without_stride(input, stride, output,
                                                        txfm_size);
  /*col wise transform*/
  txfm_func_col(out_128, buf_128, cos_bit_col, stage_range_col);
  av1_round_shift_array_32_neon(buf_128, out_128, txfm2d_size_128, -shift[1]);
  transpose_32(txfm_size, out_128, buf_128);

  /*row wise transform*/
  for (int col = 0; col < (col_num >> 1); col++) {
    av1_fdct64_new_neon((buf_128 + col), (out_128 + col), cos_bit_row, col_num,
                        (col_num >> 1));
  }

  txfm2d_size_128 = (col_num >> 1) * (txfm_size >> 1);
  av1_round_shift_array_32_neon(out_128, out_128, txfm2d_size_128, -shift[2]);
}

static INLINE void fwd_txfm2d_neon(const int16_t *input, int32_t *output,
                                   const int stride,
                                   const TXFM_2D_FLIP_CFG *cfg,
                                   int32_t *txfm_buf) {
  assert(cfg->tx_size < TX_SIZES);
  const int txfm_size = tx_size_wide[cfg->tx_size];
  const int8_t *shift = cfg->shift;
  const int8_t *stage_range_col = cfg->stage_range_col;
  const int8_t *stage_range_row = cfg->stage_range_row;
  const int8_t cos_bit_col = cfg->cos_bit_col;
  const int8_t cos_bit_row = cfg->cos_bit_row;
  const TxfmFuncNEON txfm_func_col = fwd_txfm_type_to_func(cfg->txfm_type_col);
  const TxfmFuncNEON txfm_func_row = fwd_txfm_type_to_func(cfg->txfm_type_row);

  int32x4_t *buf_128 = (int32x4_t *)txfm_buf;
  int32x4_t *out_128 = (int32x4_t *)output;
  int num_per_128 = 4;
  int txfm2d_size_128 = txfm_size * txfm_size / num_per_128;

  int16_array_with_stride_to_int32_array_without_stride(input, stride, txfm_buf,
                                                        txfm_size);
  av1_round_shift_array_32_neon(buf_128, out_128, txfm2d_size_128, -shift[0]);
  txfm_func_col(out_128, buf_128, cos_bit_col, stage_range_col);
  av1_round_shift_array_32_neon(buf_128, out_128, txfm2d_size_128, -shift[1]);
  transpose_32(txfm_size, out_128, buf_128);
  txfm_func_row(buf_128, out_128, cos_bit_row, stage_range_row);
  av1_round_shift_array_32_neon(out_128, out_128, txfm2d_size_128, -shift[2]);
}

void av1_fwd_txfm2d_32x32_neon(const int16_t *input, int32_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(16, int32_t, txfm_buf[1024]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_32X32, &cfg);
  (void)bd;
  fwd_txfm2d_neon(input, output, stride, &cfg, txfm_buf);
}

void av1_fwd_txfm2d_64x64_neon(const int16_t *input, int32_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  DECLARE_ALIGNED(16, int32_t, txfm_buf[4096]);
  TXFM_2D_FLIP_CFG cfg;
  av1_get_fwd_txfm_cfg(tx_type, TX_64X64, &cfg);
  (void)bd;
  fwd_txfm2d_64x64_neon(input, output, stride, &cfg, txfm_buf);
}
