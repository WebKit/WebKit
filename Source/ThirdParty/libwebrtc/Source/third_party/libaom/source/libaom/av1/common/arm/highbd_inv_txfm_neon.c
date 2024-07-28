/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you canzip
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/idct.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#if AOM_ARCH_AARCH64
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
#endif  // AOM_ARCH_AARCH64

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

static INLINE void round_shift_array_32_neon(int32x4_t *input,
                                             int32x4_t *output, const int size,
                                             const int bit) {
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  for (int i = 0; i < size; i++) {
    output[i] = vrshlq_s32(input[i], v_bit);
  }
}

static INLINE void round_shift_rect_array_32_neon(int32x4_t *input,
                                                  int32x4_t *output,
                                                  const int size) {
  for (int i = 0; i < size; i++) {
    const int32x4_t r0 = vmulq_n_s32(input[i], NewInvSqrt2);
    output[i] = vrshrq_n_s32(r0, NewSqrt2Bits);
  }
}

static INLINE int32x4_t half_btf_neon_r(const int32_t *n0, const int32x4_t *w0,
                                        const int32_t *n1, const int32x4_t *w1,
                                        const int32x4_t *v_bit,
                                        const int32x4_t *rnding) {
  int32x4_t x;
  x = vmlaq_n_s32(*rnding, *w0, *n0);
  x = vmlaq_n_s32(x, *w1, *n1);
  x = vshlq_s32(x, *v_bit);
  return x;
}

static INLINE int32x4_t half_btf_neon_mode11_r(
    const int32_t *n0, const int32x4_t *w0, const int32_t *n1,
    const int32x4_t *w1, const int32x4_t *v_bit, const int32x4_t *rnding) {
  int32x4_t x;
  x = vmlaq_n_s32(*rnding, *w0, -*n0);
  x = vmlaq_n_s32(x, *w1, -*n1);
  x = vshlq_s32(x, *v_bit);
  return x;
}

static INLINE int32x4_t half_btf_neon_mode01_r(
    const int32_t *n0, const int32x4_t *w0, const int32_t *n1,
    const int32x4_t *w1, const int32x4_t *v_bit, const int32x4_t *rnding) {
  int32x4_t x;
  x = vmlaq_n_s32(*rnding, *w0, *n0);
  x = vmlsq_n_s32(x, *w1, *n1);
  x = vshlq_s32(x, *v_bit);
  return x;
}

static INLINE int32x4_t half_btf_neon_mode10_r(
    const int32_t *n0, const int32x4_t *w0, const int32_t *n1,
    const int32x4_t *w1, const int32x4_t *v_bit, const int32x4_t *rnding) {
  int32x4_t x;
  x = vmlaq_n_s32(*rnding, *w1, *n1);
  x = vmlsq_n_s32(x, *w0, *n0);
  x = vshlq_s32(x, *v_bit);
  return x;
}

static INLINE int32x4_t half_btf_0_neon_r(const int32_t *n0,
                                          const int32x4_t *w0,
                                          const int32x4_t *v_bit,
                                          const int32x4_t *rnding) {
  int32x4_t x;
  x = vmlaq_n_s32(*rnding, *w0, *n0);
  x = vshlq_s32(x, *v_bit);
  return x;
}

static INLINE int32x4_t half_btf_0_m_neon_r(const int32_t *n0,
                                            const int32x4_t *w0,
                                            const int32x4_t *v_bit,
                                            const int32x4_t *rnding) {
  int32x4_t x;
  x = vmlaq_n_s32(*rnding, *w0, -*n0);
  x = vshlq_s32(x, *v_bit);
  return x;
}

static INLINE void flip_buf_neon(int32x4_t *in, int32x4_t *out, int size) {
  for (int i = 0; i < size; ++i) {
    out[size - i - 1] = in[i];
  }
}

typedef void (*fwd_transform_1d_neon)(int32x4_t *in, int32x4_t *out, int bit,
                                      const int num_cols);

typedef void (*transform_1d_neon)(int32x4_t *in, int32x4_t *out, int32_t bit,
                                  int32_t do_cols, int32_t bd,
                                  int32_t out_shift);

static INLINE uint16x8_t highbd_clamp_u16(uint16x8_t *u, const uint16x8_t *min,
                                          const uint16x8_t *max) {
  int16x8_t clamped;
  clamped = vminq_s16(vreinterpretq_s16_u16(*u), vreinterpretq_s16_u16(*max));
  clamped = vmaxq_s16(clamped, vreinterpretq_s16_u16(*min));
  return vreinterpretq_u16_s16(clamped);
}

static INLINE void round_shift_4x4(int32x4_t *in, int shift) {
  if (shift != 0) {
    const int32x4_t v_shift = vdupq_n_s32(-shift);
    in[0] = vrshlq_s32(in[0], v_shift);
    in[1] = vrshlq_s32(in[1], v_shift);
    in[2] = vrshlq_s32(in[2], v_shift);
    in[3] = vrshlq_s32(in[3], v_shift);
  }
}

static void round_shift_8x8(int32x4_t *in, int shift) {
  assert(shift != 0);
  const int32x4_t v_shift = vdupq_n_s32(-shift);
  in[0] = vrshlq_s32(in[0], v_shift);
  in[1] = vrshlq_s32(in[1], v_shift);
  in[2] = vrshlq_s32(in[2], v_shift);
  in[3] = vrshlq_s32(in[3], v_shift);
  in[4] = vrshlq_s32(in[4], v_shift);
  in[5] = vrshlq_s32(in[5], v_shift);
  in[6] = vrshlq_s32(in[6], v_shift);
  in[7] = vrshlq_s32(in[7], v_shift);
  in[8] = vrshlq_s32(in[8], v_shift);
  in[9] = vrshlq_s32(in[9], v_shift);
  in[10] = vrshlq_s32(in[10], v_shift);
  in[11] = vrshlq_s32(in[11], v_shift);
  in[12] = vrshlq_s32(in[12], v_shift);
  in[13] = vrshlq_s32(in[13], v_shift);
  in[14] = vrshlq_s32(in[14], v_shift);
  in[15] = vrshlq_s32(in[15], v_shift);
}

static void highbd_clamp_s32_neon(int32x4_t *in, int32x4_t *out,
                                  const int32x4_t *clamp_lo,
                                  const int32x4_t *clamp_hi, int size) {
  int32x4_t a0, a1;
  for (int i = 0; i < size; i += 4) {
    a0 = vmaxq_s32(in[i], *clamp_lo);
    out[i] = vminq_s32(a0, *clamp_hi);

    a1 = vmaxq_s32(in[i + 1], *clamp_lo);
    out[i + 1] = vminq_s32(a1, *clamp_hi);

    a0 = vmaxq_s32(in[i + 2], *clamp_lo);
    out[i + 2] = vminq_s32(a0, *clamp_hi);

    a1 = vmaxq_s32(in[i + 3], *clamp_lo);
    out[i + 3] = vminq_s32(a1, *clamp_hi);
  }
}

static INLINE uint16x8_t highbd_get_recon_8x8_neon(const uint16x8_t pred,
                                                   int32x4_t res0,
                                                   int32x4_t res1,
                                                   const int bd) {
  const uint16x8_t v_zero = vdupq_n_u16(0);
  int32x4_t min_clip_val = vreinterpretq_s32_u16(v_zero);
  int32x4_t max_clip_val = vdupq_n_s32((1 << bd) - 1);
  uint16x8x2_t x;
  x.val[0] = vreinterpretq_u16_s32(
      vaddw_s16(res0, vreinterpret_s16_u16(vget_low_u16(pred))));
  x.val[1] = vreinterpretq_u16_s32(
      vaddw_s16(res1, vreinterpret_s16_u16(vget_high_u16(pred))));
  x.val[0] = vreinterpretq_u16_s32(
      vmaxq_s32(vreinterpretq_s32_u16(x.val[0]), min_clip_val));
  x.val[0] = vreinterpretq_u16_s32(
      vminq_s32(vreinterpretq_s32_u16(x.val[0]), max_clip_val));
  x.val[1] = vreinterpretq_u16_s32(
      vmaxq_s32(vreinterpretq_s32_u16(x.val[1]), min_clip_val));
  x.val[1] = vreinterpretq_u16_s32(
      vminq_s32(vreinterpretq_s32_u16(x.val[1]), max_clip_val));
  uint16x8_t res = vcombine_u16(vqmovn_u32(vreinterpretq_u32_u16(x.val[0])),
                                vqmovn_u32(vreinterpretq_u32_u16(x.val[1])));
  return res;
}

static INLINE uint16x4_t highbd_get_recon_4xn_neon(uint16x4_t pred,
                                                   int32x4_t res0,
                                                   const int bd) {
  uint16x4_t x0_ = vreinterpret_u16_s16(
      vmovn_s32(vaddw_s16(res0, vreinterpret_s16_u16(pred))));
  uint16x8_t x0 = vcombine_u16(x0_, x0_);
  const uint16x8_t vmin = vdupq_n_u16(0);
  const uint16x8_t vmax = vdupq_n_u16((1 << bd) - 1);
  x0 = highbd_clamp_u16(&x0, &vmin, &vmax);
  return vget_low_u16(x0);
}

static INLINE void highbd_write_buffer_4xn_neon(int32x4_t *in, uint16_t *output,
                                                int stride, int flipud,
                                                int height, const int bd) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    uint16x4_t v = vld1_u16(output + i * stride);
    uint16x4_t u = highbd_get_recon_4xn_neon(v, in[j], bd);

    vst1_u16(output + i * stride, u);
  }
}

static INLINE void highbd_write_buffer_8xn_neon(int32x4_t *in, uint16_t *output,
                                                int stride, int flipud,
                                                int height, const int bd) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    uint16x8_t v = vld1q_u16(output + i * stride);
    uint16x8_t u = highbd_get_recon_8x8_neon(v, in[j], in[j + height], bd);

    vst1q_u16(output + i * stride, u);
  }
}

static INLINE void load_buffer_32bit_input(const int32_t *in, int stride,
                                           int32x4_t *out, int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[i] = vld1q_s32(in + i * stride);
  }
}

static INLINE void load_buffer_4x4(const int32_t *coeff, int32x4_t *in) {
  in[0] = vld1q_s32(coeff + 0);
  in[1] = vld1q_s32(coeff + 4);
  in[2] = vld1q_s32(coeff + 8);
  in[3] = vld1q_s32(coeff + 12);
}

static void addsub_neon(const int32x4_t in0, const int32x4_t in1,
                        int32x4_t *out0, int32x4_t *out1,
                        const int32x4_t *clamp_lo, const int32x4_t *clamp_hi) {
  int32x4_t a0 = vaddq_s32(in0, in1);
  int32x4_t a1 = vsubq_s32(in0, in1);

  a0 = vmaxq_s32(a0, *clamp_lo);
  a0 = vminq_s32(a0, *clamp_hi);
  a1 = vmaxq_s32(a1, *clamp_lo);
  a1 = vminq_s32(a1, *clamp_hi);

  *out0 = a0;
  *out1 = a1;
}

static void shift_and_clamp_neon(int32x4_t *in0, int32x4_t *in1,
                                 const int32x4_t *clamp_lo,
                                 const int32x4_t *clamp_hi,
                                 const int32x4_t *v_shift) {
  int32x4_t in0_w_offset = vrshlq_s32(*in0, *v_shift);
  int32x4_t in1_w_offset = vrshlq_s32(*in1, *v_shift);

  in0_w_offset = vmaxq_s32(in0_w_offset, *clamp_lo);
  in0_w_offset = vminq_s32(in0_w_offset, *clamp_hi);
  in1_w_offset = vmaxq_s32(in1_w_offset, *clamp_lo);
  in1_w_offset = vminq_s32(in1_w_offset, *clamp_hi);

  *in0 = in0_w_offset;
  *in1 = in1_w_offset;
}

static INLINE void idct32_stage4_neon(int32x4_t *bf1, const int32_t *cospi,
                                      const int32x4_t *v_bit,
                                      const int32x4_t *rnding) {
  int32x4_t temp1, temp2;
  temp1 = half_btf_neon_mode10_r(&cospi[8], &bf1[17], &cospi[56], &bf1[30],
                                 v_bit, rnding);
  bf1[30] =
      half_btf_neon_r(&cospi[56], &bf1[17], &cospi[8], &bf1[30], v_bit, rnding);
  bf1[17] = temp1;

  temp2 = half_btf_neon_mode11_r(&cospi[56], &bf1[18], &cospi[8], &bf1[29],
                                 v_bit, rnding);
  bf1[29] = half_btf_neon_mode10_r(&cospi[8], &bf1[18], &cospi[56], &bf1[29],
                                   v_bit, rnding);
  bf1[18] = temp2;

  temp1 = half_btf_neon_mode10_r(&cospi[40], &bf1[21], &cospi[24], &bf1[26],
                                 v_bit, rnding);
  bf1[26] = half_btf_neon_r(&cospi[24], &bf1[21], &cospi[40], &bf1[26], v_bit,
                            rnding);
  bf1[21] = temp1;

  temp2 = half_btf_neon_mode11_r(&cospi[24], &bf1[22], &cospi[40], &bf1[25],
                                 v_bit, rnding);
  bf1[25] = half_btf_neon_mode10_r(&cospi[40], &bf1[22], &cospi[24], &bf1[25],
                                   v_bit, rnding);
  bf1[22] = temp2;
}

static INLINE void idct32_stage5_neon(int32x4_t *bf1, const int32_t *cospi,
                                      const int32x4_t *clamp_lo,
                                      const int32x4_t *clamp_hi,
                                      const int32x4_t *v_bit,
                                      const int32x4_t *rnding) {
  int32x4_t temp1, temp2;
  temp1 = half_btf_neon_mode10_r(&cospi[16], &bf1[9], &cospi[48], &bf1[14],
                                 v_bit, rnding);
  bf1[14] =
      half_btf_neon_r(&cospi[48], &bf1[9], &cospi[16], &bf1[14], v_bit, rnding);
  bf1[9] = temp1;

  temp2 = half_btf_neon_mode11_r(&cospi[48], &bf1[10], &cospi[16], &bf1[13],
                                 v_bit, rnding);
  bf1[13] = half_btf_neon_mode10_r(&cospi[16], &bf1[10], &cospi[48], &bf1[13],
                                   v_bit, rnding);
  bf1[10] = temp2;

  addsub_neon(bf1[16], bf1[19], bf1 + 16, bf1 + 19, clamp_lo, clamp_hi);
  addsub_neon(bf1[17], bf1[18], bf1 + 17, bf1 + 18, clamp_lo, clamp_hi);
  addsub_neon(bf1[23], bf1[20], bf1 + 23, bf1 + 20, clamp_lo, clamp_hi);
  addsub_neon(bf1[22], bf1[21], bf1 + 22, bf1 + 21, clamp_lo, clamp_hi);
  addsub_neon(bf1[24], bf1[27], bf1 + 24, bf1 + 27, clamp_lo, clamp_hi);
  addsub_neon(bf1[25], bf1[26], bf1 + 25, bf1 + 26, clamp_lo, clamp_hi);
  addsub_neon(bf1[31], bf1[28], bf1 + 31, bf1 + 28, clamp_lo, clamp_hi);
  addsub_neon(bf1[30], bf1[29], bf1 + 30, bf1 + 29, clamp_lo, clamp_hi);
}

static INLINE void idct32_stage6_neon(int32x4_t *bf1, const int32_t *cospi,
                                      const int32x4_t *clamp_lo,
                                      const int32x4_t *clamp_hi,
                                      const int32x4_t *v_bit,
                                      const int32x4_t *rnding) {
  int32x4_t temp1, temp2;
  temp1 = half_btf_neon_mode10_r(&cospi[32], &bf1[5], &cospi[32], &bf1[6],
                                 v_bit, rnding);
  bf1[6] =
      half_btf_neon_r(&cospi[32], &bf1[5], &cospi[32], &bf1[6], v_bit, rnding);
  bf1[5] = temp1;

  addsub_neon(bf1[8], bf1[11], bf1 + 8, bf1 + 11, clamp_lo, clamp_hi);
  addsub_neon(bf1[9], bf1[10], bf1 + 9, bf1 + 10, clamp_lo, clamp_hi);
  addsub_neon(bf1[15], bf1[12], bf1 + 15, bf1 + 12, clamp_lo, clamp_hi);
  addsub_neon(bf1[14], bf1[13], bf1 + 14, bf1 + 13, clamp_lo, clamp_hi);

  temp1 = half_btf_neon_mode10_r(&cospi[16], &bf1[18], &cospi[48], &bf1[29],
                                 v_bit, rnding);
  bf1[29] = half_btf_neon_r(&cospi[48], &bf1[18], &cospi[16], &bf1[29], v_bit,
                            rnding);
  bf1[18] = temp1;
  temp2 = half_btf_neon_mode10_r(&cospi[16], &bf1[19], &cospi[48], &bf1[28],
                                 v_bit, rnding);
  bf1[28] = half_btf_neon_r(&cospi[48], &bf1[19], &cospi[16], &bf1[28], v_bit,
                            rnding);
  bf1[19] = temp2;
  temp1 = half_btf_neon_mode11_r(&cospi[48], &bf1[20], &cospi[16], &bf1[27],
                                 v_bit, rnding);
  bf1[27] = half_btf_neon_mode10_r(&cospi[16], &bf1[20], &cospi[48], &bf1[27],
                                   v_bit, rnding);
  bf1[20] = temp1;
  temp2 = half_btf_neon_mode11_r(&cospi[48], &bf1[21], &cospi[16], &bf1[26],
                                 v_bit, rnding);
  bf1[26] = half_btf_neon_mode10_r(&cospi[16], &bf1[21], &cospi[48], &bf1[26],
                                   v_bit, rnding);
  bf1[21] = temp2;
}

static INLINE void idct32_stage7_neon(int32x4_t *bf1, const int32_t *cospi,
                                      const int32x4_t *clamp_lo,
                                      const int32x4_t *clamp_hi,
                                      const int32x4_t *v_bit,
                                      const int32x4_t *rnding) {
  int32x4_t temp1, temp2;
  addsub_neon(bf1[0], bf1[7], bf1 + 0, bf1 + 7, clamp_lo, clamp_hi);
  addsub_neon(bf1[1], bf1[6], bf1 + 1, bf1 + 6, clamp_lo, clamp_hi);
  addsub_neon(bf1[2], bf1[5], bf1 + 2, bf1 + 5, clamp_lo, clamp_hi);
  addsub_neon(bf1[3], bf1[4], bf1 + 3, bf1 + 4, clamp_lo, clamp_hi);
  temp1 = half_btf_neon_mode10_r(&cospi[32], &bf1[10], &cospi[32], &bf1[13],
                                 v_bit, rnding);
  bf1[13] = half_btf_neon_r(&cospi[32], &bf1[10], &cospi[32], &bf1[13], v_bit,
                            rnding);
  bf1[10] = temp1;
  temp2 = half_btf_neon_mode10_r(&cospi[32], &bf1[11], &cospi[32], &bf1[12],
                                 v_bit, rnding);
  bf1[12] = half_btf_neon_r(&cospi[32], &bf1[11], &cospi[32], &bf1[12], v_bit,
                            rnding);
  bf1[11] = temp2;

  addsub_neon(bf1[16], bf1[23], bf1 + 16, bf1 + 23, clamp_lo, clamp_hi);
  addsub_neon(bf1[17], bf1[22], bf1 + 17, bf1 + 22, clamp_lo, clamp_hi);
  addsub_neon(bf1[18], bf1[21], bf1 + 18, bf1 + 21, clamp_lo, clamp_hi);
  addsub_neon(bf1[19], bf1[20], bf1 + 19, bf1 + 20, clamp_lo, clamp_hi);
  addsub_neon(bf1[31], bf1[24], bf1 + 31, bf1 + 24, clamp_lo, clamp_hi);
  addsub_neon(bf1[30], bf1[25], bf1 + 30, bf1 + 25, clamp_lo, clamp_hi);
  addsub_neon(bf1[29], bf1[26], bf1 + 29, bf1 + 26, clamp_lo, clamp_hi);
  addsub_neon(bf1[28], bf1[27], bf1 + 28, bf1 + 27, clamp_lo, clamp_hi);
}

static INLINE void idct32_stage8_neon(int32x4_t *bf1, const int32_t *cospi,
                                      const int32x4_t *clamp_lo,
                                      const int32x4_t *clamp_hi,
                                      const int32x4_t *v_bit,
                                      const int32x4_t *rnding) {
  int32x4_t temp1, temp2;
  addsub_neon(bf1[0], bf1[15], bf1 + 0, bf1 + 15, clamp_lo, clamp_hi);
  addsub_neon(bf1[1], bf1[14], bf1 + 1, bf1 + 14, clamp_lo, clamp_hi);
  addsub_neon(bf1[2], bf1[13], bf1 + 2, bf1 + 13, clamp_lo, clamp_hi);
  addsub_neon(bf1[3], bf1[12], bf1 + 3, bf1 + 12, clamp_lo, clamp_hi);
  addsub_neon(bf1[4], bf1[11], bf1 + 4, bf1 + 11, clamp_lo, clamp_hi);
  addsub_neon(bf1[5], bf1[10], bf1 + 5, bf1 + 10, clamp_lo, clamp_hi);
  addsub_neon(bf1[6], bf1[9], bf1 + 6, bf1 + 9, clamp_lo, clamp_hi);
  addsub_neon(bf1[7], bf1[8], bf1 + 7, bf1 + 8, clamp_lo, clamp_hi);
  temp1 = half_btf_neon_mode10_r(&cospi[32], &bf1[20], &cospi[32], &bf1[27],
                                 v_bit, rnding);
  bf1[27] = half_btf_neon_r(&cospi[32], &bf1[20], &cospi[32], &bf1[27], v_bit,
                            rnding);
  bf1[20] = temp1;
  temp2 = half_btf_neon_mode10_r(&cospi[32], &bf1[21], &cospi[32], &bf1[26],
                                 v_bit, rnding);
  bf1[26] = half_btf_neon_r(&cospi[32], &bf1[21], &cospi[32], &bf1[26], v_bit,
                            rnding);
  bf1[21] = temp2;
  temp1 = half_btf_neon_mode10_r(&cospi[32], &bf1[22], &cospi[32], &bf1[25],
                                 v_bit, rnding);
  bf1[25] = half_btf_neon_r(&cospi[32], &bf1[22], &cospi[32], &bf1[25], v_bit,
                            rnding);
  bf1[22] = temp1;
  temp2 = half_btf_neon_mode10_r(&cospi[32], &bf1[23], &cospi[32], &bf1[24],
                                 v_bit, rnding);
  bf1[24] = half_btf_neon_r(&cospi[32], &bf1[23], &cospi[32], &bf1[24], v_bit,
                            rnding);
  bf1[23] = temp2;
}

static INLINE void idct32_stage9_neon(int32x4_t *bf1, int32x4_t *out,
                                      const int do_cols, const int bd,
                                      const int out_shift,
                                      const int32x4_t *clamp_lo,
                                      const int32x4_t *clamp_hi) {
  addsub_neon(bf1[0], bf1[31], out + 0, out + 31, clamp_lo, clamp_hi);
  addsub_neon(bf1[1], bf1[30], out + 1, out + 30, clamp_lo, clamp_hi);
  addsub_neon(bf1[2], bf1[29], out + 2, out + 29, clamp_lo, clamp_hi);
  addsub_neon(bf1[3], bf1[28], out + 3, out + 28, clamp_lo, clamp_hi);
  addsub_neon(bf1[4], bf1[27], out + 4, out + 27, clamp_lo, clamp_hi);
  addsub_neon(bf1[5], bf1[26], out + 5, out + 26, clamp_lo, clamp_hi);
  addsub_neon(bf1[6], bf1[25], out + 6, out + 25, clamp_lo, clamp_hi);
  addsub_neon(bf1[7], bf1[24], out + 7, out + 24, clamp_lo, clamp_hi);
  addsub_neon(bf1[8], bf1[23], out + 8, out + 23, clamp_lo, clamp_hi);
  addsub_neon(bf1[9], bf1[22], out + 9, out + 22, clamp_lo, clamp_hi);
  addsub_neon(bf1[10], bf1[21], out + 10, out + 21, clamp_lo, clamp_hi);
  addsub_neon(bf1[11], bf1[20], out + 11, out + 20, clamp_lo, clamp_hi);
  addsub_neon(bf1[12], bf1[19], out + 12, out + 19, clamp_lo, clamp_hi);
  addsub_neon(bf1[13], bf1[18], out + 13, out + 18, clamp_lo, clamp_hi);
  addsub_neon(bf1[14], bf1[17], out + 14, out + 17, clamp_lo, clamp_hi);
  addsub_neon(bf1[15], bf1[16], out + 15, out + 16, clamp_lo, clamp_hi);

  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    for (int i = 0; i < 32; i += 8) {
      round_shift_4x4(out + i, out_shift);
      round_shift_4x4(out + i + 4, out_shift);
    }
    highbd_clamp_s32_neon(out, out, &clamp_lo_out, &clamp_hi_out, 32);
  }
}

static void neg_shift_neon(const int32x4_t *in0, const int32x4_t *in1,
                           int32x4_t *out0, int32x4_t *out1,
                           const int32x4_t *clamp_lo, const int32x4_t *clamp_hi,
                           const int32x4_t *v_shift, int32x4_t *offset) {
  int32x4_t a0 = vaddq_s32(*offset, *in0);
  int32x4_t a1 = vsubq_s32(*offset, *in1);

  a0 = vshlq_s32(a0, *v_shift);
  a1 = vshlq_s32(a1, *v_shift);

  a0 = vmaxq_s32(a0, *clamp_lo);
  a0 = vminq_s32(a0, *clamp_hi);
  a1 = vmaxq_s32(a1, *clamp_lo);
  a1 = vminq_s32(a1, *clamp_hi);

  *out0 = a0;
  *out1 = a1;
}

static void idct4x4_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                         int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));

  int32x4_t u0, u1, u2, u3;
  int32x4_t v0, v1, v2, v3, x, y;

  // Stage 0-1-2

  u0 = in[0];
  u1 = in[1];
  u2 = in[2];
  u3 = in[3];

  const int32x4_t v_bit = vdupq_n_s32(-bit);

  x = vmlaq_n_s32(rnding, u0, cospi[32]);
  y = vmulq_n_s32(u2, cospi[32]);
  v0 = vaddq_s32(x, y);
  v0 = vshlq_s32(v0, v_bit);

  v1 = vsubq_s32(x, y);
  v1 = vshlq_s32(v1, v_bit);

  x = vmlaq_n_s32(rnding, u1, cospi[48]);
  v2 = vmlsq_n_s32(x, u3, cospi[16]);
  v2 = vshlq_s32(v2, v_bit);

  x = vmlaq_n_s32(rnding, u1, cospi[16]);
  v3 = vmlaq_n_s32(x, u3, cospi[48]);
  v3 = vshlq_s32(v3, v_bit);
  // Stage 3
  addsub_neon(v0, v3, out + 0, out + 3, &clamp_lo, &clamp_hi);
  addsub_neon(v1, v2, out + 1, out + 2, &clamp_lo, &clamp_hi);

  if (!do_cols) {
    log_range = AOMMAX(16, bd + 6);
    clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
    clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
    const int32x4_t v_shift = vdupq_n_s32(-out_shift);
    shift_and_clamp_neon(out + 0, out + 3, &clamp_lo, &clamp_hi, &v_shift);
    shift_and_clamp_neon(out + 1, out + 2, &clamp_lo, &clamp_hi, &v_shift);
  }
}

static void iadst4x4_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                          int bd, int out_shift) {
  const int32_t *sinpi = sinpi_arr(bit);
  const int32x4_t zero = vdupq_n_s32(0);
  int64x2_t rnding = vdupq_n_s64(1ll << (bit + 4 - 1));
  const int32x2_t mul = vdup_n_s32(1 << 4);
  int32x4_t t;
  int32x4_t s0, s1, s2, s3, s4, s5, s6, s7;
  int32x4_t x0, x1, x2, x3;
  int32x4_t u0, u1, u2, u3;

  x0 = in[0];
  x1 = in[1];
  x2 = in[2];
  x3 = in[3];

  s0 = vmulq_n_s32(x0, sinpi[1]);
  s1 = vmulq_n_s32(x0, sinpi[2]);
  s2 = vmulq_n_s32(x1, sinpi[3]);
  s3 = vmulq_n_s32(x2, sinpi[4]);
  s4 = vmulq_n_s32(x2, sinpi[1]);
  s5 = vmulq_n_s32(x3, sinpi[2]);
  s6 = vmulq_n_s32(x3, sinpi[4]);
  t = vsubq_s32(x0, x2);
  s7 = vaddq_s32(t, x3);

  t = vaddq_s32(s0, s3);
  s0 = vaddq_s32(t, s5);
  t = vsubq_s32(s1, s4);
  s1 = vsubq_s32(t, s6);
  s3 = s2;
  s2 = vmulq_n_s32(s7, sinpi[3]);

  u0 = vaddq_s32(s0, s3);
  u1 = vaddq_s32(s1, s3);
  u2 = s2;
  t = vaddq_s32(s0, s1);
  u3 = vsubq_s32(t, s3);

  // u0
  int32x4x2_t u0x;
  u0x.val[0] = vreinterpretq_s32_s64(
      vmull_s32(vmovn_s64(vreinterpretq_s64_s32(u0)), mul));
  u0x.val[0] = vreinterpretq_s32_s64(
      vaddq_s64(vreinterpretq_s64_s32(u0x.val[0]), rnding));

  u0 = vextq_s32(u0, zero, 1);
  u0x.val[1] = vreinterpretq_s32_s64(
      vmull_s32(vmovn_s64(vreinterpretq_s64_s32(u0)), mul));
  u0x.val[1] = vreinterpretq_s32_s64(
      vaddq_s64(vreinterpretq_s64_s32(u0x.val[1]), rnding));

  u0x.val[0] = vreinterpretq_s32_s16(vextq_s16(
      vreinterpretq_s16_s32(u0x.val[0]), vreinterpretq_s16_s32(zero), 1));
  u0x.val[1] = vreinterpretq_s32_s16(vextq_s16(
      vreinterpretq_s16_s32(u0x.val[1]), vreinterpretq_s16_s32(zero), 1));

  u0x = vzipq_s32(u0x.val[0], u0x.val[1]);
#if AOM_ARCH_AARCH64
  u0 = vreinterpretq_s32_s64(vzip1q_s64(vreinterpretq_s64_s32(u0x.val[0]),
                                        vreinterpretq_s64_s32(u0x.val[1])));
#else
  u0 = vcombine_s32(vget_low_s32(u0x.val[0]), vget_low_s32(u0x.val[1]));
#endif  // AOM_ARCH_AARCH64
  // u1
  int32x4x2_t u1x;
  u1x.val[0] = vreinterpretq_s32_s64(
      vmull_s32(vmovn_s64(vreinterpretq_s64_s32(u1)), mul));
  u1x.val[0] = vreinterpretq_s32_s64(
      vaddq_s64(vreinterpretq_s64_s32(u1x.val[0]), rnding));

  u1 = vextq_s32(u1, zero, 1);
  u1x.val[1] = vreinterpretq_s32_s64(
      vmull_s32(vmovn_s64(vreinterpretq_s64_s32(u1)), mul));
  u1x.val[1] = vreinterpretq_s32_s64(
      vaddq_s64(vreinterpretq_s64_s32(u1x.val[1]), rnding));

  u1x.val[0] = vreinterpretq_s32_s16(vextq_s16(
      vreinterpretq_s16_s32(u1x.val[0]), vreinterpretq_s16_s32(zero), 1));
  u1x.val[1] = vreinterpretq_s32_s16(vextq_s16(
      vreinterpretq_s16_s32(u1x.val[1]), vreinterpretq_s16_s32(zero), 1));

  u1x = vzipq_s32(u1x.val[0], u1x.val[1]);
#if AOM_ARCH_AARCH64
  u1 = vreinterpretq_s32_s64(vzip1q_s64(vreinterpretq_s64_s32(u1x.val[0]),
                                        vreinterpretq_s64_s32(u1x.val[1])));
#else
  u1 = vcombine_s32(vget_low_s32(u1x.val[0]), vget_low_s32(u1x.val[1]));
#endif  // AOM_ARCH_AARCH64

  // u2
  int32x4x2_t u2x;
  u2x.val[0] = vreinterpretq_s32_s64(
      vmull_s32(vmovn_s64(vreinterpretq_s64_s32(u2)), mul));
  u2x.val[0] = vreinterpretq_s32_s64(
      vaddq_s64(vreinterpretq_s64_s32(u2x.val[0]), rnding));

  u2 = vextq_s32(u2, zero, 1);
  u2x.val[1] = vreinterpretq_s32_s64(
      vmull_s32(vmovn_s64(vreinterpretq_s64_s32(u2)), mul));
  u2x.val[1] = vreinterpretq_s32_s64(
      vaddq_s64(vreinterpretq_s64_s32(u2x.val[1]), rnding));

  u2x.val[0] = vreinterpretq_s32_s16(vextq_s16(
      vreinterpretq_s16_s32(u2x.val[0]), vreinterpretq_s16_s32(zero), 1));
  u2x.val[1] = vreinterpretq_s32_s16(vextq_s16(
      vreinterpretq_s16_s32(u2x.val[1]), vreinterpretq_s16_s32(zero), 1));

  u2x = vzipq_s32(u2x.val[0], u2x.val[1]);
#if AOM_ARCH_AARCH64
  u2 = vreinterpretq_s32_s64(vzip1q_s64(vreinterpretq_s64_s32(u2x.val[0]),
                                        vreinterpretq_s64_s32(u2x.val[1])));
#else
  u2 = vcombine_s32(vget_low_s32(u2x.val[0]), vget_low_s32(u2x.val[1]));
#endif  // AOM_ARCH_AARCH64

  // u3
  int32x4x2_t u3x;
  u3x.val[0] = vreinterpretq_s32_s64(
      vmull_s32(vmovn_s64(vreinterpretq_s64_s32(u3)), mul));
  u3x.val[0] = vreinterpretq_s32_s64(
      vaddq_s64(vreinterpretq_s64_s32(u3x.val[0]), rnding));

  u3 = vextq_s32(u3, zero, 1);
  u3x.val[1] = vreinterpretq_s32_s64(
      vmull_s32(vmovn_s64(vreinterpretq_s64_s32(u3)), mul));
  u3x.val[1] = vreinterpretq_s32_s64(
      vaddq_s64(vreinterpretq_s64_s32(u3x.val[1]), rnding));

  u3x.val[0] = vreinterpretq_s32_s16(vextq_s16(
      vreinterpretq_s16_s32(u3x.val[0]), vreinterpretq_s16_s32(zero), 1));
  u3x.val[1] = vreinterpretq_s32_s16(vextq_s16(
      vreinterpretq_s16_s32(u3x.val[1]), vreinterpretq_s16_s32(zero), 1));

  u3x = vzipq_s32(u3x.val[0], u3x.val[1]);
#if AOM_ARCH_AARCH64
  u3 = vreinterpretq_s32_s64(vzip1q_s64(vreinterpretq_s64_s32(u3x.val[0]),
                                        vreinterpretq_s64_s32(u3x.val[1])));
#else
  u3 = vcombine_s32(vget_low_s32(u3x.val[0]), vget_low_s32(u3x.val[1]));
#endif  // AOM_ARCH_AARCH64

  out[0] = u0;
  out[1] = u1;
  out[2] = u2;
  out[3] = u3;

  if (!do_cols) {
    const int log_range = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
    const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
    round_shift_4x4(out, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo, &clamp_hi, 4);
  }
}

static void write_buffer_4x4(int32x4_t *in, uint16_t *output, int stride,
                             int fliplr, int flipud, int shift, int bd) {
  uint32x4_t u0, u1, u2, u3;
  uint16x4_t v0, v1, v2, v3;
  round_shift_4x4(in, shift);

  v0 = vld1_u16(output + 0 * stride);
  v1 = vld1_u16(output + 1 * stride);
  v2 = vld1_u16(output + 2 * stride);
  v3 = vld1_u16(output + 3 * stride);

  if (fliplr) {
    u0 = vrev64q_u32(vreinterpretq_u32_s32(in[0]));
    in[0] = vreinterpretq_s32_u32(vextq_u32(u0, u0, 2));
    u0 = vrev64q_u32(vreinterpretq_u32_s32(in[1]));
    in[1] = vreinterpretq_s32_u32(vextq_u32(u0, u0, 2));
    u0 = vrev64q_u32(vreinterpretq_u32_s32(in[2]));
    in[2] = vreinterpretq_s32_u32(vextq_u32(u0, u0, 2));
    u0 = vrev64q_u32(vreinterpretq_u32_s32(in[3]));
    in[3] = vreinterpretq_s32_u32(vextq_u32(u0, u0, 2));
  }

  if (flipud) {
    u0 = vaddw_u16(vreinterpretq_u32_s32(in[3]), v0);
    u1 = vaddw_u16(vreinterpretq_u32_s32(in[2]), v1);
    u2 = vaddw_u16(vreinterpretq_u32_s32(in[1]), v2);
    u3 = vaddw_u16(vreinterpretq_u32_s32(in[0]), v3);
  } else {
    u0 = vaddw_u16(vreinterpretq_u32_s32(in[0]), v0);
    u1 = vaddw_u16(vreinterpretq_u32_s32(in[1]), v1);
    u2 = vaddw_u16(vreinterpretq_u32_s32(in[2]), v2);
    u3 = vaddw_u16(vreinterpretq_u32_s32(in[3]), v3);
  }

  uint16x8_t u4 = vcombine_u16(vqmovn_u32(u0), vqmovn_u32(u1));
  uint16x8_t u5 = vcombine_u16(vqmovn_u32(u2), vqmovn_u32(u3));
  const uint16x8_t vmin = vdupq_n_u16(0);
  const uint16x8_t vmax = vdupq_n_u16((1 << bd) - 1);
  u4 = highbd_clamp_u16(&u4, &vmin, &vmax);
  u5 = highbd_clamp_u16(&u5, &vmin, &vmax);

  vst1_u16(output + 0 * stride, vget_low_u16(u4));
  vst1_u16(output + 1 * stride, vget_high_u16(u4));
  vst1_u16(output + 2 * stride, vget_low_u16(u5));
  vst1_u16(output + 3 * stride, vget_high_u16(u5));
}

static void iidentity4_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                            int bd, int out_shift) {
  (void)bit;
  int32x4_t zero = vdupq_n_s32(0);
  int32x2_t fact = vdup_n_s32(NewSqrt2);
  int32x4x2_t a0;
  const int64x2_t rnding = vdupq_n_s64(1 << (NewSqrt2Bits - 1));

  for (int i = 0; i < 4; i++) {
    a0.val[0] = vreinterpretq_s32_s64(
        vmlal_s32(rnding, vmovn_s64(vreinterpretq_s64_s32(in[i])), fact));
    a0.val[0] = vreinterpretq_s32_s64(
        vshrq_n_s64(vreinterpretq_s64_s32(a0.val[0]), NewSqrt2Bits));
    a0.val[1] = vextq_s32(in[i], zero, 1);
    a0.val[1] = vreinterpretq_s32_s64(
        vmlal_s32(rnding, vmovn_s64(vreinterpretq_s64_s32(a0.val[1])), fact));
    a0.val[1] = vreinterpretq_s32_s64(
        vshrq_n_s64(vreinterpretq_s64_s32(a0.val[1]), NewSqrt2Bits));

    a0 = vzipq_s32(a0.val[0], a0.val[1]);
#if AOM_ARCH_AARCH64
    out[i] = vreinterpretq_s32_s64(vzip1q_s64(
        vreinterpretq_s64_s32(a0.val[0]), vreinterpretq_s64_s32(a0.val[1])));
#else
    out[i] = vextq_s32(vextq_s32(a0.val[0], a0.val[0], 2), a0.val[1], 2);
#endif
  }
  if (!do_cols) {
    const int log_range = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
    const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
    round_shift_4x4(out, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo, &clamp_hi, 4);
  }
}

void av1_inv_txfm2d_add_4x4_neon(const int32_t *input, uint16_t *output,
                                 int stride, TX_TYPE tx_type, int bd) {
  int32x4_t in[4];

  const int8_t *shift = av1_inv_txfm_shift_ls[TX_4X4];

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_4x4(input, in);
      idct4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      idct4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case ADST_DCT:
      load_buffer_4x4(input, in);
      idct4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case DCT_ADST:
      load_buffer_4x4(input, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      idct4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case ADST_ADST:
      load_buffer_4x4(input, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case FLIPADST_DCT:
      load_buffer_4x4(input, in);
      idct4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 1, -shift[1], bd);
      break;
    case DCT_FLIPADST:
      load_buffer_4x4(input, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      idct4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 1, 0, -shift[1], bd);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_4x4(input, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 1, 1, -shift[1], bd);
      break;
    case ADST_FLIPADST:
      load_buffer_4x4(input, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 1, 0, -shift[1], bd);
      break;
    case FLIPADST_ADST:
      load_buffer_4x4(input, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 1, -shift[1], bd);
      break;
    case IDTX:
      load_buffer_4x4(input, in);
      iidentity4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iidentity4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case V_DCT:
      load_buffer_4x4(input, in);
      iidentity4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      idct4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case H_DCT:
      load_buffer_4x4(input, in);
      idct4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iidentity4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case V_ADST:
      load_buffer_4x4(input, in);
      iidentity4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case H_ADST:
      load_buffer_4x4(input, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iidentity4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 0, -shift[1], bd);
      break;
    case V_FLIPADST:
      load_buffer_4x4(input, in);
      iidentity4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 0, 1, -shift[1], bd);
      break;
    case H_FLIPADST:
      load_buffer_4x4(input, in);
      iadst4x4_neon(in, in, INV_COS_BIT, 0, bd, 0);
      transpose_4x4(in, in);
      iidentity4_neon(in, in, INV_COS_BIT, 1, bd, 0);
      write_buffer_4x4(in, output, stride, 1, 0, -shift[1], bd);
      break;
    default: assert(0);
  }
}

// 8x8
static void load_buffer_8x8(const int32_t *coeff, int32x4_t *in) {
  in[0] = vld1q_s32(coeff + 0);
  in[1] = vld1q_s32(coeff + 4);
  in[2] = vld1q_s32(coeff + 8);
  in[3] = vld1q_s32(coeff + 12);
  in[4] = vld1q_s32(coeff + 16);
  in[5] = vld1q_s32(coeff + 20);
  in[6] = vld1q_s32(coeff + 24);
  in[7] = vld1q_s32(coeff + 28);
  in[8] = vld1q_s32(coeff + 32);
  in[9] = vld1q_s32(coeff + 36);
  in[10] = vld1q_s32(coeff + 40);
  in[11] = vld1q_s32(coeff + 44);
  in[12] = vld1q_s32(coeff + 48);
  in[13] = vld1q_s32(coeff + 52);
  in[14] = vld1q_s32(coeff + 56);
  in[15] = vld1q_s32(coeff + 60);
}

static void idct8x8_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                         int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t u0, u1, u2, u3, u4, u5, u6, u7;
  int32x4_t v0, v1, v2, v3, v4, v5, v6, v7;
  int32x4_t x, y;
  int col;
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  // Note:
  //  Even column: 0, 2, ..., 14
  //  Odd column: 1, 3, ..., 15
  //  one even column plus one odd column constructs one row (8 coeffs)
  //  total we have 8 rows (8x8).
  for (col = 0; col < 2; ++col) {
    // stage 0
    // stage 1
    // stage 2
    u0 = in[0 * 2 + col];
    u1 = in[4 * 2 + col];
    u2 = in[2 * 2 + col];
    u3 = in[6 * 2 + col];

    x = vmulq_n_s32(in[1 * 2 + col], cospi[56]);
    u4 = vmlaq_n_s32(x, in[7 * 2 + col], -cospi[8]);
    u4 = vaddq_s32(u4, rnding);
    u4 = vshlq_s32(u4, v_bit);

    x = vmulq_n_s32(in[1 * 2 + col], cospi[8]);
    u7 = vmlaq_n_s32(x, in[7 * 2 + col], cospi[56]);
    u7 = vaddq_s32(u7, rnding);
    u7 = vshlq_s32(u7, v_bit);

    x = vmulq_n_s32(in[5 * 2 + col], cospi[24]);
    u5 = vmlaq_n_s32(x, in[3 * 2 + col], -cospi[40]);
    u5 = vaddq_s32(u5, rnding);
    u5 = vshlq_s32(u5, v_bit);

    x = vmulq_n_s32(in[5 * 2 + col], cospi[40]);
    u6 = vmlaq_n_s32(x, in[3 * 2 + col], cospi[24]);
    u6 = vaddq_s32(u6, rnding);
    u6 = vshlq_s32(u6, v_bit);

    // stage 3
    x = vmulq_n_s32(u0, cospi[32]);
    y = vmulq_n_s32(u1, cospi[32]);
    v0 = vaddq_s32(x, y);
    v0 = vaddq_s32(v0, rnding);
    v0 = vshlq_s32(v0, v_bit);

    v1 = vsubq_s32(x, y);
    v1 = vaddq_s32(v1, rnding);
    v1 = vshlq_s32(v1, v_bit);

    x = vmulq_n_s32(u2, cospi[48]);
    v2 = vmlaq_n_s32(x, u3, -cospi[16]);
    v2 = vaddq_s32(v2, rnding);
    v2 = vshlq_s32(v2, v_bit);

    x = vmulq_n_s32(u2, cospi[16]);
    v3 = vmlaq_n_s32(x, u3, cospi[48]);
    v3 = vaddq_s32(v3, rnding);
    v3 = vshlq_s32(v3, v_bit);

    addsub_neon(u4, u5, &v4, &v5, &clamp_lo, &clamp_hi);
    addsub_neon(u7, u6, &v7, &v6, &clamp_lo, &clamp_hi);

    // stage 4
    addsub_neon(v0, v3, &u0, &u3, &clamp_lo, &clamp_hi);
    addsub_neon(v1, v2, &u1, &u2, &clamp_lo, &clamp_hi);
    u4 = v4;
    u7 = v7;

    x = vmulq_n_s32(v5, cospi[32]);
    y = vmulq_n_s32(v6, cospi[32]);
    u6 = vaddq_s32(y, x);
    u6 = vaddq_s32(u6, rnding);
    u6 = vshlq_s32(u6, v_bit);

    u5 = vsubq_s32(y, x);
    u5 = vaddq_s32(u5, rnding);
    u5 = vshlq_s32(u5, v_bit);

    // stage 5
    addsub_neon(u0, u7, out + 0 * 2 + col, out + 7 * 2 + col, &clamp_lo,
                &clamp_hi);
    addsub_neon(u1, u6, out + 1 * 2 + col, out + 6 * 2 + col, &clamp_lo,
                &clamp_hi);
    addsub_neon(u2, u5, out + 2 * 2 + col, out + 5 * 2 + col, &clamp_lo,
                &clamp_hi);
    addsub_neon(u3, u4, out + 3 * 2 + col, out + 4 * 2 + col, &clamp_lo,
                &clamp_hi);
  }

  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    round_shift_8x8(out, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo_out, &clamp_hi_out, 16);
  }
}

static void iadst8x8_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                          int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int32x4_t kZero = vdupq_n_s32(0);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t u[8], v[8], x;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0-1-2
  // (1)
  u[0] = vmlaq_n_s32(rnding, in[14], cospi[4]);
  u[0] = vmlaq_n_s32(u[0], in[0], cospi[60]);
  u[0] = vshlq_s32(u[0], v_bit);

  u[1] = vmlaq_n_s32(rnding, in[14], cospi[60]);
  u[1] = vmlsq_n_s32(u[1], in[0], cospi[4]);
  u[1] = vshlq_s32(u[1], v_bit);

  // (2)
  u[2] = vmlaq_n_s32(rnding, in[10], cospi[20]);
  u[2] = vmlaq_n_s32(u[2], in[4], cospi[44]);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vmlaq_n_s32(rnding, in[10], cospi[44]);
  u[3] = vmlsq_n_s32(u[3], in[4], cospi[20]);
  u[3] = vshlq_s32(u[3], v_bit);

  // (3)
  u[4] = vmlaq_n_s32(rnding, in[6], cospi[36]);
  u[4] = vmlaq_n_s32(u[4], in[8], cospi[28]);
  u[4] = vshlq_s32(u[4], v_bit);

  u[5] = vmlaq_n_s32(rnding, in[6], cospi[28]);
  u[5] = vmlsq_n_s32(u[5], in[8], cospi[36]);
  u[5] = vshlq_s32(u[5], v_bit);

  // (4)
  u[6] = vmlaq_n_s32(rnding, in[2], cospi[52]);
  u[6] = vmlaq_n_s32(u[6], in[12], cospi[12]);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vmlaq_n_s32(rnding, in[2], cospi[12]);
  u[7] = vmlsq_n_s32(u[7], in[12], cospi[52]);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 3
  addsub_neon(u[0], u[4], &v[0], &v[4], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[5], &v[1], &v[5], &clamp_lo, &clamp_hi);
  addsub_neon(u[2], u[6], &v[2], &v[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[3], u[7], &v[3], &v[7], &clamp_lo, &clamp_hi);

  // stage 4
  u[0] = v[0];
  u[1] = v[1];
  u[2] = v[2];
  u[3] = v[3];

  u[4] = vmlaq_n_s32(rnding, v[4], cospi[16]);
  u[4] = vmlaq_n_s32(u[4], v[5], cospi[48]);
  u[4] = vshlq_s32(u[4], v_bit);

  u[5] = vmlaq_n_s32(rnding, v[4], cospi[48]);
  u[5] = vmlsq_n_s32(u[5], v[5], cospi[16]);
  u[5] = vshlq_s32(u[5], v_bit);

  u[6] = vmlaq_n_s32(rnding, v[7], cospi[16]);
  u[6] = vmlsq_n_s32(u[6], v[6], cospi[48]);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vmlaq_n_s32(rnding, v[7], cospi[48]);
  u[7] = vmlaq_n_s32(u[7], v[6], cospi[16]);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 5
  addsub_neon(u[0], u[2], &v[0], &v[2], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[3], &v[1], &v[3], &clamp_lo, &clamp_hi);
  addsub_neon(u[4], u[6], &v[4], &v[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[5], u[7], &v[5], &v[7], &clamp_lo, &clamp_hi);

  // stage 6
  u[0] = v[0];
  u[1] = v[1];
  u[4] = v[4];
  u[5] = v[5];

  v[0] = vmlaq_n_s32(rnding, v[2], cospi[32]);
  x = vmulq_n_s32(v[3], cospi[32]);
  u[2] = vaddq_s32(v[0], x);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vsubq_s32(v[0], x);
  u[3] = vshlq_s32(u[3], v_bit);

  v[0] = vmlaq_n_s32(rnding, v[6], cospi[32]);
  x = vmulq_n_s32(v[7], cospi[32]);
  u[6] = vaddq_s32(v[0], x);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vsubq_s32(v[0], x);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 7
  if (do_cols) {
    out[0] = u[0];
    out[2] = vsubq_s32(kZero, u[4]);
    out[4] = u[6];
    out[6] = vsubq_s32(kZero, u[2]);
    out[8] = u[3];
    out[10] = vsubq_s32(kZero, u[7]);
    out[12] = u[5];
    out[14] = vsubq_s32(kZero, u[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    const int32x4_t v_shift = vdupq_n_s32(-out_shift);
    int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
    neg_shift_neon(&u[0], &u[4], out + 0, out + 2, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[6], &u[2], out + 4, out + 6, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[3], &u[7], out + 8, out + 10, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&u[5], &u[1], out + 12, out + 14, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
  }

  // Odd 8 points: 1, 3, ..., 15
  // stage 0
  // stage 1
  // stage 2
  // (1)
  u[0] = vmlaq_n_s32(rnding, in[15], cospi[4]);
  u[0] = vmlaq_n_s32(u[0], in[1], cospi[60]);
  u[0] = vshlq_s32(u[0], v_bit);

  u[1] = vmlaq_n_s32(rnding, in[15], cospi[60]);
  u[1] = vmlsq_n_s32(u[1], in[1], cospi[4]);
  u[1] = vshlq_s32(u[1], v_bit);

  // (2)
  u[2] = vmlaq_n_s32(rnding, in[11], cospi[20]);
  u[2] = vmlaq_n_s32(u[2], in[5], cospi[44]);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vmlaq_n_s32(rnding, in[11], cospi[44]);
  u[3] = vmlsq_n_s32(u[3], in[5], cospi[20]);
  u[3] = vshlq_s32(u[3], v_bit);

  // (3)
  u[4] = vmlaq_n_s32(rnding, in[7], cospi[36]);
  u[4] = vmlaq_n_s32(u[4], in[9], cospi[28]);
  u[4] = vshlq_s32(u[4], v_bit);

  u[5] = vmlaq_n_s32(rnding, in[7], cospi[28]);
  u[5] = vmlsq_n_s32(u[5], in[9], cospi[36]);
  u[5] = vshlq_s32(u[5], v_bit);

  // (4)
  u[6] = vmlaq_n_s32(rnding, in[3], cospi[52]);
  u[6] = vmlaq_n_s32(u[6], in[13], cospi[12]);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vmlaq_n_s32(rnding, in[3], cospi[12]);
  u[7] = vmlsq_n_s32(u[7], in[13], cospi[52]);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 3
  addsub_neon(u[0], u[4], &v[0], &v[4], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[5], &v[1], &v[5], &clamp_lo, &clamp_hi);
  addsub_neon(u[2], u[6], &v[2], &v[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[3], u[7], &v[3], &v[7], &clamp_lo, &clamp_hi);

  // stage 4
  u[0] = v[0];
  u[1] = v[1];
  u[2] = v[2];
  u[3] = v[3];

  u[4] = vmlaq_n_s32(rnding, v[4], cospi[16]);
  u[4] = vmlaq_n_s32(u[4], v[5], cospi[48]);
  u[4] = vshlq_s32(u[4], v_bit);

  u[5] = vmlaq_n_s32(rnding, v[4], cospi[48]);
  u[5] = vmlsq_n_s32(u[5], v[5], cospi[16]);
  u[5] = vshlq_s32(u[5], v_bit);

  u[6] = vmlaq_n_s32(rnding, v[7], cospi[16]);
  u[6] = vmlsq_n_s32(u[6], v[6], cospi[48]);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vmlaq_n_s32(rnding, v[6], cospi[16]);
  u[7] = vmlaq_n_s32(u[7], v[7], cospi[48]);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 5
  addsub_neon(u[0], u[2], &v[0], &v[2], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[3], &v[1], &v[3], &clamp_lo, &clamp_hi);
  addsub_neon(u[4], u[6], &v[4], &v[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[5], u[7], &v[5], &v[7], &clamp_lo, &clamp_hi);

  // stage 6
  u[0] = v[0];
  u[1] = v[1];
  u[4] = v[4];
  u[5] = v[5];

  v[0] = vmlaq_n_s32(rnding, v[2], cospi[32]);
  x = vmulq_n_s32(v[3], cospi[32]);
  u[2] = vaddq_s32(v[0], x);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vsubq_s32(v[0], x);
  u[3] = vshlq_s32(u[3], v_bit);

  v[0] = vmlaq_n_s32(rnding, v[6], cospi[32]);
  x = vmulq_n_s32(v[7], cospi[32]);
  u[6] = vaddq_s32(v[0], x);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vsubq_s32(v[0], x);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 7
  if (do_cols) {
    out[1] = u[0];
    out[3] = vsubq_s32(kZero, u[4]);
    out[5] = u[6];
    out[7] = vsubq_s32(kZero, u[2]);
    out[9] = u[3];
    out[11] = vsubq_s32(kZero, u[7]);
    out[13] = u[5];
    out[15] = vsubq_s32(kZero, u[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    const int32x4_t v_shift = vdupq_n_s32(-out_shift);
    int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
    neg_shift_neon(&u[0], &u[4], out + 1, out + 3, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[6], &u[2], out + 5, out + 7, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[3], &u[7], out + 9, out + 11, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&u[5], &u[1], out + 13, out + 15, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
  }
}

static void iidentity8_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                            int bd, int out_shift) {
  (void)bit;
  out[0] = vaddq_s32(in[0], in[0]);
  out[1] = vaddq_s32(in[1], in[1]);
  out[2] = vaddq_s32(in[2], in[2]);
  out[3] = vaddq_s32(in[3], in[3]);
  out[4] = vaddq_s32(in[4], in[4]);
  out[5] = vaddq_s32(in[5], in[5]);
  out[6] = vaddq_s32(in[6], in[6]);
  out[7] = vaddq_s32(in[7], in[7]);

  if (!do_cols) {
    const int log_range = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
    const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
    round_shift_4x4(out, out_shift);
    round_shift_4x4(out + 4, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo, &clamp_hi, 8);
  }
}

static uint16x8_t get_recon_8x8(const uint16x8_t pred, int32x4_t res_lo,
                                int32x4_t res_hi, int fliplr, int bd) {
  uint16x8x2_t x;

  if (fliplr) {
    res_lo = vrev64q_s32(res_lo);
    res_lo = vextq_s32(res_lo, res_lo, 2);
    res_hi = vrev64q_s32(res_hi);
    res_hi = vextq_s32(res_hi, res_hi, 2);
    x.val[0] = vreinterpretq_u16_s32(
        vaddw_s16(res_hi, vreinterpret_s16_u16(vget_low_u16(pred))));
    x.val[1] = vreinterpretq_u16_s32(
        vaddw_s16(res_lo, vreinterpret_s16_u16(vget_high_u16(pred))));

  } else {
    x.val[0] = vreinterpretq_u16_s32(
        vaddw_s16(res_lo, vreinterpret_s16_u16(vget_low_u16(pred))));
    x.val[1] = vreinterpretq_u16_s32(
        vaddw_s16(res_hi, vreinterpret_s16_u16(vget_high_u16(pred))));
  }

  uint16x8_t x2 = vcombine_u16(vqmovn_u32(vreinterpretq_u32_u16(x.val[0])),
                               vqmovn_u32(vreinterpretq_u32_u16(x.val[1])));
  const uint16x8_t vmin = vdupq_n_u16(0);
  const uint16x8_t vmax = vdupq_n_u16((1 << bd) - 1);
  return highbd_clamp_u16(&x2, &vmin, &vmax);
}

static void write_buffer_8x8(int32x4_t *in, uint16_t *output, int stride,
                             int fliplr, int flipud, int shift, int bd) {
  uint16x8_t u0, u1, u2, u3, u4, u5, u6, u7;
  uint16x8_t v0, v1, v2, v3, v4, v5, v6, v7;
  round_shift_8x8(in, shift);

  v0 = vld1q_u16(output + 0 * stride);
  v1 = vld1q_u16(output + 1 * stride);
  v2 = vld1q_u16(output + 2 * stride);
  v3 = vld1q_u16(output + 3 * stride);
  v4 = vld1q_u16(output + 4 * stride);
  v5 = vld1q_u16(output + 5 * stride);
  v6 = vld1q_u16(output + 6 * stride);
  v7 = vld1q_u16(output + 7 * stride);

  if (flipud) {
    u0 = get_recon_8x8(v0, in[14], in[15], fliplr, bd);
    u1 = get_recon_8x8(v1, in[12], in[13], fliplr, bd);
    u2 = get_recon_8x8(v2, in[10], in[11], fliplr, bd);
    u3 = get_recon_8x8(v3, in[8], in[9], fliplr, bd);
    u4 = get_recon_8x8(v4, in[6], in[7], fliplr, bd);
    u5 = get_recon_8x8(v5, in[4], in[5], fliplr, bd);
    u6 = get_recon_8x8(v6, in[2], in[3], fliplr, bd);
    u7 = get_recon_8x8(v7, in[0], in[1], fliplr, bd);
  } else {
    u0 = get_recon_8x8(v0, in[0], in[1], fliplr, bd);
    u1 = get_recon_8x8(v1, in[2], in[3], fliplr, bd);
    u2 = get_recon_8x8(v2, in[4], in[5], fliplr, bd);
    u3 = get_recon_8x8(v3, in[6], in[7], fliplr, bd);
    u4 = get_recon_8x8(v4, in[8], in[9], fliplr, bd);
    u5 = get_recon_8x8(v5, in[10], in[11], fliplr, bd);
    u6 = get_recon_8x8(v6, in[12], in[13], fliplr, bd);
    u7 = get_recon_8x8(v7, in[14], in[15], fliplr, bd);
  }

  vst1q_u16(output + 0 * stride, u0);
  vst1q_u16(output + 1 * stride, u1);
  vst1q_u16(output + 2 * stride, u2);
  vst1q_u16(output + 3 * stride, u3);
  vst1q_u16(output + 4 * stride, u4);
  vst1q_u16(output + 5 * stride, u5);
  vst1q_u16(output + 6 * stride, u6);
  vst1q_u16(output + 7 * stride, u7);
}

void av1_inv_txfm2d_add_8x8_neon(const int32_t *input, uint16_t *output,
                                 int stride, TX_TYPE tx_type, int bd) {
  int32x4_t in[16], out[16];
  const int8_t *shift = av1_inv_txfm_shift_ls[TX_8X8];

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_8x8(input, in);
      idct8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      idct8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 0, 0, -shift[1], bd);
      break;
    case DCT_ADST:
      load_buffer_8x8(input, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      idct8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 0, 0, -shift[1], bd);
      break;
    case ADST_DCT:
      load_buffer_8x8(input, in);
      idct8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 0, 0, -shift[1], bd);
      break;
    case ADST_ADST:
      load_buffer_8x8(input, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 0, 0, -shift[1], bd);
      break;
    case FLIPADST_DCT:
      load_buffer_8x8(input, in);
      idct8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 0, 1, -shift[1], bd);
      break;
    case DCT_FLIPADST:
      load_buffer_8x8(input, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      idct8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 1, 0, -shift[1], bd);
      break;
    case ADST_FLIPADST:
      load_buffer_8x8(input, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 1, 0, -shift[1], bd);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_8x8(input, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 1, 1, -shift[1], bd);
      break;
    case FLIPADST_ADST:
      load_buffer_8x8(input, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 0, bd, -shift[0]);
      transpose_8x8(out, in);
      iadst8x8_neon(in, out, INV_COS_BIT, 1, bd, 0);
      write_buffer_8x8(out, output, stride, 0, 1, -shift[1], bd);
      break;
    default: assert(0);
  }
}

static void idct8x8_low1_neon(int32x4_t *in, int32x4_t *out, int bit,
                              int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t x;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0-1-2-3
  x = vmulq_n_s32(in[0], cospi[32]);
  x = vaddq_s32(vshlq_s32(x, v_bit), rnding);

  // stage 4-5
  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    clamp_lo = vdupq_n_s32(-(1 << (log_range_out - 1)));
    clamp_hi = vdupq_n_s32((1 << (log_range_out - 1)) - 1);

    int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
    x = vaddq_s32(x, offset);
    x = vshlq_s32(x, vdupq_n_s32(-out_shift));
  }

  x = vmaxq_s32(x, clamp_lo);
  x = vminq_s32(x, clamp_hi);
  out[0] = x;
  out[1] = x;
  out[2] = x;
  out[3] = x;
  out[4] = x;
  out[5] = x;
  out[6] = x;
  out[7] = x;
}

static void idct8x8_new_neon(int32x4_t *in, int32x4_t *out, int bit,
                             int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t u0, u1, u2, u3, u4, u5, u6, u7;
  int32x4_t v0, v1, v2, v3, v4, v5, v6, v7;
  int32x4_t x, y;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));

  // stage 0
  // stage 1
  // stage 2
  u0 = in[0];
  u1 = in[4];
  u2 = in[2];
  u3 = in[6];

  x = vmlaq_n_s32(rnding, in[1], cospi[56]);
  u4 = vmlaq_n_s32(x, in[7], -cospi[8]);
  u4 = vshlq_s32(u4, v_bit);

  x = vmlaq_n_s32(rnding, in[1], cospi[8]);
  u7 = vmlaq_n_s32(x, in[7], cospi[56]);
  u7 = vshlq_s32(u7, v_bit);

  x = vmlaq_n_s32(rnding, in[5], cospi[24]);
  u5 = vmlaq_n_s32(x, in[3], -cospi[40]);
  u5 = vshlq_s32(u5, v_bit);

  x = vmlaq_n_s32(rnding, in[5], cospi[40]);
  u6 = vmlaq_n_s32(x, in[3], cospi[24]);
  u6 = vshlq_s32(u6, v_bit);

  // stage 3
  x = vmlaq_n_s32(rnding, u0, cospi[32]);
  y = vmulq_n_s32(u1, cospi[32]);
  v0 = vaddq_s32(x, y);
  v0 = vshlq_s32(v0, v_bit);

  v1 = vsubq_s32(x, y);
  v1 = vshlq_s32(v1, v_bit);

  x = vmlaq_n_s32(rnding, u2, cospi[48]);
  v2 = vmlaq_n_s32(x, u3, -cospi[16]);
  v2 = vshlq_s32(v2, v_bit);

  x = vmlaq_n_s32(rnding, u2, cospi[16]);
  v3 = vmlaq_n_s32(x, u3, cospi[48]);
  v3 = vshlq_s32(v3, v_bit);

  addsub_neon(u4, u5, &v4, &v5, &clamp_lo, &clamp_hi);
  addsub_neon(u7, u6, &v7, &v6, &clamp_lo, &clamp_hi);

  // stage 4
  addsub_neon(v0, v3, &u0, &u3, &clamp_lo, &clamp_hi);
  addsub_neon(v1, v2, &u1, &u2, &clamp_lo, &clamp_hi);
  u4 = v4;
  u7 = v7;

  x = vmulq_n_s32(v5, cospi[32]);
  y = vmlaq_n_s32(rnding, v6, cospi[32]);
  u6 = vaddq_s32(y, x);
  u6 = vshlq_s32(u6, v_bit);

  u5 = vsubq_s32(y, x);
  u5 = vshlq_s32(u5, v_bit);

  // stage 5
  addsub_neon(u0, u7, out + 0, out + 7, &clamp_lo, &clamp_hi);
  addsub_neon(u1, u6, out + 1, out + 6, &clamp_lo, &clamp_hi);
  addsub_neon(u2, u5, out + 2, out + 5, &clamp_lo, &clamp_hi);
  addsub_neon(u3, u4, out + 3, out + 4, &clamp_lo, &clamp_hi);

  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    round_shift_4x4(out, out_shift);
    round_shift_4x4(out + 4, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo_out, &clamp_hi_out, 8);
  }
}

static void iadst8x8_low1_neon(int32x4_t *in, int32x4_t *out, int bit,
                               int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  int32x4_t u[8], x;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0-2

  u[0] = vmlaq_n_s32(rnding, in[0], cospi[60]);
  u[0] = vshlq_s32(u[0], v_bit);

  u[1] = vmlaq_n_s32(rnding, in[0], cospi[4]);
  u[1] = vshlq_s32(vnegq_s32(u[1]), v_bit);

  // stage 3-4
  int32x4_t temp1, temp2;
  temp1 = vmlaq_n_s32(rnding, u[0], cospi[16]);
  temp1 = vmlaq_n_s32(temp1, u[1], cospi[48]);
  temp1 = vshlq_s32(temp1, v_bit);
  u[4] = temp1;

  temp2 = vmlaq_n_s32(rnding, u[0], cospi[48]);
  u[5] = vmlsq_n_s32(temp2, u[1], cospi[16]);
  u[5] = vshlq_s32(u[5], v_bit);

  // stage 5-6
  temp1 = vmlaq_n_s32(rnding, u[0], cospi[32]);
  x = vmulq_n_s32(u[1], cospi[32]);
  u[2] = vaddq_s32(temp1, x);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vsubq_s32(temp1, x);
  u[3] = vshlq_s32(u[3], v_bit);

  temp1 = vmlaq_n_s32(rnding, u[4], cospi[32]);
  x = vmulq_n_s32(u[5], cospi[32]);
  u[6] = vaddq_s32(temp1, x);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vsubq_s32(temp1, x);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 7
  if (do_cols) {
    out[0] = u[0];
    out[1] = vnegq_s32(u[4]);
    out[2] = u[6];
    out[3] = vnegq_s32(u[2]);
    out[4] = u[3];
    out[5] = vnegq_s32(u[7]);
    out[6] = u[5];
    out[7] = vnegq_s32(u[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    const int32x4_t v_shift = vdupq_n_s32(-out_shift);
    int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
    neg_shift_neon(&u[0], &u[4], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[6], &u[2], out + 2, out + 3, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[3], &u[7], out + 4, out + 5, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[5], &u[1], out + 6, out + 7, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
  }
}

static void iadst8x8_new_neon(int32x4_t *in, int32x4_t *out, int bit,
                              int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  // const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t u[8], v[8], x;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0-2

  u[0] = vmlaq_n_s32(rnding, in[7], cospi[4]);
  u[0] = vmlaq_n_s32(u[0], in[0], cospi[60]);
  u[0] = vshlq_s32(u[0], v_bit);

  u[1] = vmlaq_n_s32(rnding, in[7], cospi[60]);
  u[1] = vmlsq_n_s32(u[1], in[0], cospi[4]);
  u[1] = vshlq_s32(u[1], v_bit);

  // (2)
  u[2] = vmlaq_n_s32(rnding, in[5], cospi[20]);
  u[2] = vmlaq_n_s32(u[2], in[2], cospi[44]);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vmlaq_n_s32(rnding, in[5], cospi[44]);
  u[3] = vmlsq_n_s32(u[3], in[2], cospi[20]);
  u[3] = vshlq_s32(u[3], v_bit);

  // (3)
  u[4] = vmlaq_n_s32(rnding, in[3], cospi[36]);
  u[4] = vmlaq_n_s32(u[4], in[4], cospi[28]);
  u[4] = vshlq_s32(u[4], v_bit);

  u[5] = vmlaq_n_s32(rnding, in[3], cospi[28]);
  u[5] = vmlsq_n_s32(u[5], in[4], cospi[36]);
  u[5] = vshlq_s32(u[5], v_bit);

  // (4)
  u[6] = vmulq_n_s32(in[1], cospi[52]);
  u[6] = vmlaq_n_s32(u[6], in[6], cospi[12]);
  u[6] = vaddq_s32(u[6], rnding);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vmulq_n_s32(in[1], cospi[12]);
  u[7] = vmlsq_n_s32(u[7], in[6], cospi[52]);
  u[7] = vaddq_s32(u[7], rnding);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 3
  addsub_neon(u[0], u[4], &v[0], &v[4], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[5], &v[1], &v[5], &clamp_lo, &clamp_hi);
  addsub_neon(u[2], u[6], &v[2], &v[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[3], u[7], &v[3], &v[7], &clamp_lo, &clamp_hi);

  // stage 4
  u[0] = v[0];
  u[1] = v[1];
  u[2] = v[2];
  u[3] = v[3];

  u[4] = vmlaq_n_s32(rnding, v[4], cospi[16]);
  u[4] = vmlaq_n_s32(u[4], v[5], cospi[48]);
  u[4] = vshlq_s32(u[4], v_bit);

  u[5] = vmlaq_n_s32(rnding, v[4], cospi[48]);
  u[5] = vmlsq_n_s32(u[5], v[5], cospi[16]);
  u[5] = vshlq_s32(u[5], v_bit);

  u[6] = vmlsq_n_s32(rnding, v[6], cospi[48]);
  u[6] = vmlaq_n_s32(u[6], v[7], cospi[16]);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vmlaq_n_s32(rnding, v[6], cospi[16]);
  u[7] = vmlaq_n_s32(u[7], v[7], cospi[48]);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 5
  addsub_neon(u[0], u[2], &v[0], &v[2], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[3], &v[1], &v[3], &clamp_lo, &clamp_hi);
  addsub_neon(u[4], u[6], &v[4], &v[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[5], u[7], &v[5], &v[7], &clamp_lo, &clamp_hi);

  // stage 6
  u[0] = v[0];
  u[1] = v[1];
  u[4] = v[4];
  u[5] = v[5];

  v[0] = vmlaq_n_s32(rnding, v[2], cospi[32]);
  x = vmulq_n_s32(v[3], cospi[32]);
  u[2] = vaddq_s32(v[0], x);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vsubq_s32(v[0], x);
  u[3] = vshlq_s32(u[3], v_bit);

  v[0] = vmlaq_n_s32(rnding, v[6], cospi[32]);
  x = vmulq_n_s32(v[7], cospi[32]);
  u[6] = vaddq_s32(v[0], x);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vsubq_s32(v[0], x);
  u[7] = vshlq_s32(u[7], v_bit);

  // stage 7
  if (do_cols) {
    out[0] = u[0];
    out[1] = vnegq_s32(u[4]);
    out[2] = u[6];
    out[3] = vnegq_s32(u[2]);
    out[4] = u[3];
    out[5] = vnegq_s32(u[7]);
    out[6] = u[5];
    out[7] = vnegq_s32(u[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    const int32x4_t v_shift = vdupq_n_s32(-out_shift);
    int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
    neg_shift_neon(&u[0], &u[4], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[6], &u[2], out + 2, out + 3, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[3], &u[7], out + 4, out + 5, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[5], &u[1], out + 6, out + 7, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
  }
}

static void idct16x16_low1_neon(int32x4_t *in, int32x4_t *out, int bit,
                                int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0-4
  in[0] = vmlaq_n_s32(rnding, in[0], cospi[32]);
  in[0] = vshlq_s32(in[0], v_bit);

  // stage 5-7
  if (!do_cols) {
    log_range = AOMMAX(16, bd + 6);
    clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
    clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
    if (out_shift != 0) {
      int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
      in[0] = vaddq_s32(in[0], offset);
      in[0] = vshlq_s32(in[0], vdupq_n_s32(-out_shift));
    }
  }

  in[0] = vmaxq_s32(in[0], clamp_lo);
  in[0] = vminq_s32(in[0], clamp_hi);
  out[0] = in[0];
  out[1] = in[0];
  out[2] = in[0];
  out[3] = in[0];
  out[4] = in[0];
  out[5] = in[0];
  out[6] = in[0];
  out[7] = in[0];
  out[8] = in[0];
  out[9] = in[0];
  out[10] = in[0];
  out[11] = in[0];
  out[12] = in[0];
  out[13] = in[0];
  out[14] = in[0];
  out[15] = in[0];
}

static void idct16x16_low8_neon(int32x4_t *in, int32x4_t *out, int bit,
                                int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  int32x4_t u[16], x, y;
  // stage 0-1
  u[0] = in[0];
  u[2] = in[4];
  u[4] = in[2];
  u[6] = in[6];
  u[8] = in[1];
  u[10] = in[5];
  u[12] = in[3];
  u[14] = in[7];

  // stage 2
  u[15] = half_btf_0_neon_r(&cospi[4], &u[8], &v_bit, &rnding);
  u[8] = half_btf_0_neon_r(&cospi[60], &u[8], &v_bit, &rnding);

  u[9] = half_btf_0_m_neon_r(&cospi[36], &u[14], &v_bit, &rnding);
  u[14] = half_btf_0_neon_r(&cospi[28], &u[14], &v_bit, &rnding);

  u[13] = half_btf_0_neon_r(&cospi[20], &u[10], &v_bit, &rnding);
  u[10] = half_btf_0_neon_r(&cospi[44], &u[10], &v_bit, &rnding);

  u[11] = half_btf_0_m_neon_r(&cospi[52], &u[12], &v_bit, &rnding);
  u[12] = half_btf_0_neon_r(&cospi[12], &u[12], &v_bit, &rnding);

  // stage 3
  u[7] = half_btf_0_neon_r(&cospi[8], &u[4], &v_bit, &rnding);
  u[4] = half_btf_0_neon_r(&cospi[56], &u[4], &v_bit, &rnding);
  u[5] = half_btf_0_m_neon_r(&cospi[40], &u[6], &v_bit, &rnding);
  u[6] = half_btf_0_neon_r(&cospi[24], &u[6], &v_bit, &rnding);

  addsub_neon(u[8], u[9], &u[8], &u[9], &clamp_lo, &clamp_hi);
  addsub_neon(u[11], u[10], &u[11], &u[10], &clamp_lo, &clamp_hi);
  addsub_neon(u[12], u[13], &u[12], &u[13], &clamp_lo, &clamp_hi);
  addsub_neon(u[15], u[14], &u[15], &u[14], &clamp_lo, &clamp_hi);

  // stage 4
  x = vmlaq_n_s32(rnding, u[0], cospi[32]);
  u[0] = vshlq_s32(x, v_bit);
  u[1] = u[0];

  u[3] = half_btf_0_neon_r(&cospi[16], &u[2], &v_bit, &rnding);
  u[2] = half_btf_0_neon_r(&cospi[48], &u[2], &v_bit, &rnding);

  addsub_neon(u[4], u[5], &u[4], &u[5], &clamp_lo, &clamp_hi);
  addsub_neon(u[7], u[6], &u[7], &u[6], &clamp_lo, &clamp_hi);

  x = half_btf_neon_mode10_r(&cospi[16], &u[9], &cospi[48], &u[14], &v_bit,
                             &rnding);
  u[14] =
      half_btf_neon_r(&cospi[48], &u[9], &cospi[16], &u[14], &v_bit, &rnding);
  u[9] = x;
  y = half_btf_neon_mode11_r(&cospi[48], &u[10], &cospi[16], &u[13], &v_bit,
                             &rnding);
  u[13] = half_btf_neon_mode10_r(&cospi[16], &u[10], &cospi[48], &u[13], &v_bit,
                                 &rnding);
  u[10] = y;

  // stage 5
  addsub_neon(u[0], u[3], &u[0], &u[3], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[2], &u[1], &u[2], &clamp_lo, &clamp_hi);

  x = vmulq_n_s32(u[5], cospi[32]);
  y = vmlaq_n_s32(rnding, u[6], cospi[32]);
  u[5] = vsubq_s32(y, x);
  u[5] = vshlq_s32(u[5], v_bit);

  u[6] = vaddq_s32(y, x);
  u[6] = vshlq_s32(u[6], v_bit);

  addsub_neon(u[8], u[11], &u[8], &u[11], &clamp_lo, &clamp_hi);
  addsub_neon(u[9], u[10], &u[9], &u[10], &clamp_lo, &clamp_hi);
  addsub_neon(u[15], u[12], &u[15], &u[12], &clamp_lo, &clamp_hi);
  addsub_neon(u[14], u[13], &u[14], &u[13], &clamp_lo, &clamp_hi);

  // stage 6
  addsub_neon(u[0], u[7], &u[0], &u[7], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[6], &u[1], &u[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[2], u[5], &u[2], &u[5], &clamp_lo, &clamp_hi);
  addsub_neon(u[3], u[4], &u[3], &u[4], &clamp_lo, &clamp_hi);

  x = vmulq_n_s32(u[10], cospi[32]);
  y = vmlaq_n_s32(rnding, u[13], cospi[32]);
  u[10] = vsubq_s32(y, x);
  u[10] = vshlq_s32(u[10], v_bit);

  u[13] = vaddq_s32(x, y);
  u[13] = vshlq_s32(u[13], v_bit);

  x = vmulq_n_s32(u[11], cospi[32]);
  y = vmlaq_n_s32(rnding, u[12], cospi[32]);
  u[11] = vsubq_s32(y, x);
  u[11] = vshlq_s32(u[11], v_bit);

  u[12] = vaddq_s32(x, y);
  u[12] = vshlq_s32(u[12], v_bit);
  // stage 7
  addsub_neon(u[0], u[15], out + 0, out + 15, &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[14], out + 1, out + 14, &clamp_lo, &clamp_hi);
  addsub_neon(u[2], u[13], out + 2, out + 13, &clamp_lo, &clamp_hi);
  addsub_neon(u[3], u[12], out + 3, out + 12, &clamp_lo, &clamp_hi);
  addsub_neon(u[4], u[11], out + 4, out + 11, &clamp_lo, &clamp_hi);
  addsub_neon(u[5], u[10], out + 5, out + 10, &clamp_lo, &clamp_hi);
  addsub_neon(u[6], u[9], out + 6, out + 9, &clamp_lo, &clamp_hi);
  addsub_neon(u[7], u[8], out + 7, out + 8, &clamp_lo, &clamp_hi);

  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    round_shift_8x8(out, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo_out, &clamp_hi_out, 16);
  }
}

static void iadst16x16_low1_neon(int32x4_t *in, int32x4_t *out, int bit,
                                 int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  int32x4_t v[16], x, y, temp1, temp2;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0
  // stage 1
  // stage 2
  v[0] = vmlaq_n_s32(rnding, in[0], cospi[62]);
  v[0] = vshlq_s32(v[0], v_bit);

  v[1] = vmlsq_n_s32(rnding, in[0], cospi[2]);
  v[1] = vshlq_s32(v[1], v_bit);

  // stage 3
  v[8] = v[0];
  v[9] = v[1];

  // stage 4
  temp1 = vmlaq_n_s32(rnding, v[8], cospi[8]);
  temp1 = vmlaq_n_s32(temp1, v[9], cospi[56]);
  temp1 = vshlq_s32(temp1, v_bit);

  temp2 = vmlaq_n_s32(rnding, v[8], cospi[56]);
  temp2 = vmlsq_n_s32(temp2, v[9], cospi[8]);
  temp2 = vshlq_s32(temp2, v_bit);
  v[8] = temp1;
  v[9] = temp2;

  // stage 5
  v[4] = v[0];
  v[5] = v[1];
  v[12] = v[8];
  v[13] = v[9];

  // stage 6
  temp1 = vmlaq_n_s32(rnding, v[4], cospi[16]);
  temp1 = vmlaq_n_s32(temp1, v[5], cospi[48]);
  temp1 = vshlq_s32(temp1, v_bit);

  temp2 = vmlaq_n_s32(rnding, v[4], cospi[48]);
  temp2 = vmlsq_n_s32(temp2, v[5], cospi[16]);
  temp2 = vshlq_s32(temp2, v_bit);
  v[4] = temp1;
  v[5] = temp2;

  temp1 = vmlaq_n_s32(rnding, v[12], cospi[16]);
  temp1 = vmlaq_n_s32(temp1, v[13], cospi[48]);
  temp1 = vshlq_s32(temp1, v_bit);

  temp2 = vmlaq_n_s32(rnding, v[12], cospi[48]);
  temp2 = vmlsq_n_s32(temp2, v[13], cospi[16]);
  temp2 = vshlq_s32(temp2, v_bit);
  v[12] = temp1;
  v[13] = temp2;

  // stage 7
  v[2] = v[0];
  v[3] = v[1];
  v[6] = v[4];
  v[7] = v[5];
  v[10] = v[8];
  v[11] = v[9];
  v[14] = v[12];
  v[15] = v[13];

  // stage 8
  y = vmlaq_n_s32(rnding, v[2], cospi[32]);
  x = vmulq_n_s32(v[3], cospi[32]);
  v[2] = vaddq_s32(y, x);
  v[2] = vshlq_s32(v[2], v_bit);

  v[3] = vsubq_s32(y, x);
  v[3] = vshlq_s32(v[3], v_bit);

  y = vmlaq_n_s32(rnding, v[6], cospi[32]);
  x = vmulq_n_s32(v[7], cospi[32]);
  v[6] = vaddq_s32(y, x);
  v[6] = vshlq_s32(v[6], v_bit);

  v[7] = vsubq_s32(y, x);
  v[7] = vshlq_s32(v[7], v_bit);

  y = vmlaq_n_s32(rnding, v[10], cospi[32]);
  x = vmulq_n_s32(v[11], cospi[32]);
  v[10] = vaddq_s32(y, x);
  v[10] = vshlq_s32(v[10], v_bit);

  v[11] = vsubq_s32(y, x);
  v[11] = vshlq_s32(v[11], v_bit);

  y = vmlaq_n_s32(rnding, v[14], cospi[32]);
  x = vmulq_n_s32(v[15], cospi[32]);
  v[14] = vaddq_s32(y, x);
  v[14] = vshlq_s32(v[14], v_bit);

  v[15] = vsubq_s32(y, x);
  v[15] = vshlq_s32(v[15], v_bit);

  // stage 9
  if (do_cols) {
    out[0] = v[0];
    out[1] = vnegq_s32(v[8]);
    out[2] = v[12];
    out[3] = vnegq_s32(v[4]);
    out[4] = v[6];
    out[5] = vnegq_s32(v[14]);
    out[6] = v[10];
    out[7] = vnegq_s32(v[2]);
    out[8] = v[3];
    out[9] = vnegq_s32(v[11]);
    out[10] = v[15];
    out[11] = vnegq_s32(v[7]);
    out[12] = v[5];
    out[13] = vnegq_s32(v[13]);
    out[14] = v[9];
    out[15] = vnegq_s32(v[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    const int32x4_t v_shift = vdupq_n_s32(-out_shift);
    int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
    neg_shift_neon(&v[0], &v[8], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&v[12], &v[4], out + 2, out + 3, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[6], &v[14], out + 4, out + 5, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[10], &v[2], out + 6, out + 7, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[3], &v[11], out + 8, out + 9, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[15], &v[7], out + 10, out + 11, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[5], &v[13], out + 12, out + 13, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[9], &v[1], out + 14, out + 15, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
  }
}

static void iadst16x16_low8_neon(int32x4_t *in, int32x4_t *out, int bit,
                                 int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t zero = vdupq_n_s32(0);
  int32x4_t u[16], x, y;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0-2
  u[0] = vmlaq_n_s32(rnding, in[0], cospi[62]);
  u[0] = vshlq_s32(u[0], v_bit);

  u[1] = vmlsq_n_s32(rnding, in[0], cospi[2]);
  u[1] = vshlq_s32(u[1], v_bit);

  u[2] = vmlaq_n_s32(rnding, in[2], cospi[54]);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vmlsq_n_s32(rnding, in[2], cospi[10]);
  u[3] = vshlq_s32(u[3], v_bit);

  u[4] = vmlaq_n_s32(rnding, in[4], cospi[46]);
  u[4] = vshlq_s32(u[4], v_bit);

  u[5] = vmlsq_n_s32(rnding, in[4], cospi[18]);
  u[5] = vshlq_s32(u[5], v_bit);

  u[6] = vmlaq_n_s32(rnding, in[6], cospi[38]);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vmlsq_n_s32(rnding, in[6], cospi[26]);
  u[7] = vshlq_s32(u[7], v_bit);

  u[8] = vmlaq_n_s32(rnding, in[7], cospi[34]);
  u[8] = vshlq_s32(u[8], v_bit);

  u[9] = vmlaq_n_s32(rnding, in[7], cospi[30]);
  u[9] = vshlq_s32(u[9], v_bit);

  u[10] = vmlaq_n_s32(rnding, in[5], cospi[42]);
  u[10] = vshlq_s32(u[10], v_bit);

  u[11] = vmlaq_n_s32(rnding, in[5], cospi[22]);
  u[11] = vshlq_s32(u[11], v_bit);

  u[12] = vmlaq_n_s32(rnding, in[3], cospi[50]);
  u[12] = vshlq_s32(u[12], v_bit);

  u[13] = vmlaq_n_s32(rnding, in[3], cospi[14]);
  u[13] = vshlq_s32(u[13], v_bit);

  u[14] = vmlaq_n_s32(rnding, in[1], cospi[58]);
  u[14] = vshlq_s32(u[14], v_bit);

  u[15] = vmlaq_n_s32(rnding, in[1], cospi[6]);
  u[15] = vshlq_s32(u[15], v_bit);

  // stage 3
  addsub_neon(u[0], u[8], &u[0], &u[8], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[9], &u[1], &u[9], &clamp_lo, &clamp_hi);
  addsub_neon(u[2], u[10], &u[2], &u[10], &clamp_lo, &clamp_hi);
  addsub_neon(u[3], u[11], &u[3], &u[11], &clamp_lo, &clamp_hi);
  addsub_neon(u[4], u[12], &u[4], &u[12], &clamp_lo, &clamp_hi);
  addsub_neon(u[5], u[13], &u[5], &u[13], &clamp_lo, &clamp_hi);
  addsub_neon(u[6], u[14], &u[6], &u[14], &clamp_lo, &clamp_hi);
  addsub_neon(u[7], u[15], &u[7], &u[15], &clamp_lo, &clamp_hi);

  // stage 4
  y = vmlaq_n_s32(rnding, u[8], cospi[56]);
  u[8] = vmlaq_n_s32(rnding, u[8], cospi[8]);
  u[8] = vmlaq_n_s32(u[8], u[9], cospi[56]);
  u[8] = vshlq_s32(u[8], v_bit);

  u[9] = vmlsq_n_s32(y, u[9], cospi[8]);
  u[9] = vshlq_s32(u[9], v_bit);

  y = vmlaq_n_s32(rnding, u[10], cospi[24]);
  u[10] = vmlaq_n_s32(rnding, u[10], cospi[40]);
  u[10] = vmlaq_n_s32(u[10], u[11], cospi[24]);
  u[10] = vshlq_s32(u[10], v_bit);

  u[11] = vmlsq_n_s32(y, u[11], cospi[40]);
  u[11] = vshlq_s32(u[11], v_bit);

  y = vmlaq_n_s32(rnding, u[12], cospi[8]);
  u[12] = vmlsq_n_s32(rnding, u[12], cospi[56]);
  u[12] = vmlaq_n_s32(u[12], u[13], cospi[8]);
  u[12] = vshlq_s32(u[12], v_bit);

  u[13] = vmlaq_n_s32(y, u[13], cospi[56]);
  u[13] = vshlq_s32(u[13], v_bit);

  y = vmlaq_n_s32(rnding, u[14], cospi[40]);
  u[14] = vmlsq_n_s32(rnding, u[14], cospi[24]);
  u[14] = vmlaq_n_s32(u[14], u[15], cospi[40]);
  u[14] = vshlq_s32(u[14], v_bit);

  u[15] = vmlaq_n_s32(y, u[15], cospi[24]);
  u[15] = vshlq_s32(u[15], v_bit);

  // stage 5
  addsub_neon(u[0], u[4], &u[0], &u[4], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[5], &u[1], &u[5], &clamp_lo, &clamp_hi);
  addsub_neon(u[2], u[6], &u[2], &u[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[3], u[7], &u[3], &u[7], &clamp_lo, &clamp_hi);
  addsub_neon(u[8], u[12], &u[8], &u[12], &clamp_lo, &clamp_hi);
  addsub_neon(u[9], u[13], &u[9], &u[13], &clamp_lo, &clamp_hi);
  addsub_neon(u[10], u[14], &u[10], &u[14], &clamp_lo, &clamp_hi);
  addsub_neon(u[11], u[15], &u[11], &u[15], &clamp_lo, &clamp_hi);

  // stage 6
  y = vmlaq_n_s32(rnding, u[4], cospi[48]);
  u[4] = vmlaq_n_s32(rnding, u[4], cospi[16]);
  u[4] = vmlaq_n_s32(u[4], u[5], cospi[48]);
  u[4] = vshlq_s32(u[4], v_bit);

  u[5] = vmlsq_n_s32(y, u[5], cospi[16]);
  u[5] = vshlq_s32(u[5], v_bit);

  y = vmlaq_n_s32(rnding, u[6], cospi[16]);
  u[6] = vmlsq_n_s32(rnding, u[6], cospi[48]);
  u[6] = vmlaq_n_s32(u[6], u[7], cospi[16]);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vmlaq_n_s32(y, u[7], cospi[48]);
  u[7] = vshlq_s32(u[7], v_bit);

  y = vmlaq_n_s32(rnding, u[12], cospi[48]);
  u[12] = vmulq_n_s32(u[12], cospi[16]);
  u[12] = vmlaq_n_s32(u[12], u[13], cospi[48]);
  u[12] = vshlq_s32(u[12], v_bit);

  u[13] = vmlsq_n_s32(y, u[13], cospi[16]);
  u[13] = vshlq_s32(u[13], v_bit);

  y = vmlaq_n_s32(rnding, u[14], cospi[16]);
  u[14] = vmlsq_n_s32(rnding, u[14], cospi[48]);
  u[14] = vmlaq_n_s32(u[14], u[15], cospi[16]);
  u[14] = vshlq_s32(u[14], v_bit);

  u[15] = vmlaq_n_s32(y, u[15], cospi[48]);
  u[15] = vshlq_s32(u[15], v_bit);

  // stage 7
  addsub_neon(u[0], u[2], &u[0], &u[2], &clamp_lo, &clamp_hi);
  addsub_neon(u[1], u[3], &u[1], &u[3], &clamp_lo, &clamp_hi);
  addsub_neon(u[4], u[6], &u[4], &u[6], &clamp_lo, &clamp_hi);
  addsub_neon(u[5], u[7], &u[5], &u[7], &clamp_lo, &clamp_hi);
  addsub_neon(u[8], u[10], &u[8], &u[10], &clamp_lo, &clamp_hi);
  addsub_neon(u[9], u[11], &u[9], &u[11], &clamp_lo, &clamp_hi);
  addsub_neon(u[12], u[14], &u[12], &u[14], &clamp_lo, &clamp_hi);
  addsub_neon(u[13], u[15], &u[13], &u[15], &clamp_lo, &clamp_hi);

  // stage 8
  y = vmlaq_n_s32(rnding, u[2], cospi[32]);
  x = vmulq_n_s32(u[3], cospi[32]);
  u[2] = vaddq_s32(y, x);
  u[2] = vshlq_s32(u[2], v_bit);

  u[3] = vsubq_s32(y, x);
  u[3] = vshlq_s32(u[3], v_bit);
  y = vmlaq_n_s32(rnding, u[6], cospi[32]);
  x = vmulq_n_s32(u[7], cospi[32]);
  u[6] = vaddq_s32(y, x);
  u[6] = vshlq_s32(u[6], v_bit);

  u[7] = vsubq_s32(y, x);
  u[7] = vshlq_s32(u[7], v_bit);

  y = vmlaq_n_s32(rnding, u[10], cospi[32]);
  x = vmulq_n_s32(u[11], cospi[32]);
  u[10] = vaddq_s32(y, x);
  u[10] = vshlq_s32(u[10], v_bit);

  u[11] = vsubq_s32(y, x);
  u[11] = vshlq_s32(u[11], v_bit);

  y = vmlaq_n_s32(rnding, u[14], cospi[32]);
  x = vmulq_n_s32(u[15], cospi[32]);
  u[14] = vaddq_s32(y, x);
  u[14] = vshlq_s32(u[14], v_bit);

  u[15] = vsubq_s32(y, x);
  u[15] = vshlq_s32(u[15], v_bit);

  // stage 9
  if (do_cols) {
    out[0] = u[0];
    out[1] = vsubq_s32(zero, u[8]);
    out[2] = u[12];
    out[3] = vsubq_s32(zero, u[4]);
    out[4] = u[6];
    out[5] = vsubq_s32(zero, u[14]);
    out[6] = u[10];
    out[7] = vsubq_s32(zero, u[2]);
    out[8] = u[3];
    out[9] = vsubq_s32(zero, u[11]);
    out[10] = u[15];
    out[11] = vsubq_s32(zero, u[7]);
    out[12] = u[5];
    out[13] = vsubq_s32(zero, u[13]);
    out[14] = u[9];
    out[15] = vsubq_s32(zero, u[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    const int32x4_t v_shift = vdupq_n_s32(-out_shift);
    int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
    neg_shift_neon(&u[0], &u[8], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&u[12], &u[4], out + 2, out + 3, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&u[6], &u[14], out + 4, out + 5, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&u[10], &u[2], out + 6, out + 7, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&u[3], &u[11], out + 8, out + 9, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&u[15], &u[7], out + 10, out + 11, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&u[5], &u[13], out + 12, out + 13, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&u[9], &u[1], out + 14, out + 15, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
  }
}

static void idct16x16_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                           int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t u[16], v[16], x, y;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));

  {
    // stage 0-1
    u[0] = in[0];
    u[1] = in[8];
    u[2] = in[4];
    u[3] = in[12];
    u[4] = in[2];
    u[5] = in[10];
    u[6] = in[6];
    u[7] = in[14];
    u[8] = in[1];
    u[9] = in[9];
    u[10] = in[5];
    u[11] = in[13];
    u[12] = in[3];
    u[13] = in[11];
    u[14] = in[7];
    u[15] = in[15];

    // stage 2
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = u[4];
    v[5] = u[5];
    v[6] = u[6];
    v[7] = u[7];

    v[8] = half_btf_neon_mode01_r(&cospi[60], &u[8], &cospi[4], &u[15], &v_bit,
                                  &rnding);
    v[9] = half_btf_neon_mode01_r(&cospi[28], &u[9], &cospi[36], &u[14], &v_bit,
                                  &rnding);
    v[10] = half_btf_neon_mode01_r(&cospi[44], &u[10], &cospi[20], &u[13],
                                   &v_bit, &rnding);
    v[11] = half_btf_neon_mode01_r(&cospi[12], &u[11], &cospi[52], &u[12],
                                   &v_bit, &rnding);
    v[12] = half_btf_neon_r(&cospi[52], &u[11], &cospi[12], &u[12], &v_bit,
                            &rnding);
    v[13] = half_btf_neon_r(&cospi[20], &u[10], &cospi[44], &u[13], &v_bit,
                            &rnding);
    v[14] =
        half_btf_neon_r(&cospi[36], &u[9], &cospi[28], &u[14], &v_bit, &rnding);
    v[15] =
        half_btf_neon_r(&cospi[4], &u[8], &cospi[60], &u[15], &v_bit, &rnding);

    // stage 3
    u[0] = v[0];
    u[1] = v[1];
    u[2] = v[2];
    u[3] = v[3];
    u[4] = half_btf_neon_mode01_r(&cospi[56], &v[4], &cospi[8], &v[7], &v_bit,
                                  &rnding);
    u[5] = half_btf_neon_mode01_r(&cospi[24], &v[5], &cospi[40], &v[6], &v_bit,
                                  &rnding);
    u[6] =
        half_btf_neon_r(&cospi[40], &v[5], &cospi[24], &v[6], &v_bit, &rnding);
    u[7] =
        half_btf_neon_r(&cospi[8], &v[4], &cospi[56], &v[7], &v_bit, &rnding);
    addsub_neon(v[8], v[9], &u[8], &u[9], &clamp_lo, &clamp_hi);
    addsub_neon(v[11], v[10], &u[11], &u[10], &clamp_lo, &clamp_hi);
    addsub_neon(v[12], v[13], &u[12], &u[13], &clamp_lo, &clamp_hi);
    addsub_neon(v[15], v[14], &u[15], &u[14], &clamp_lo, &clamp_hi);

    // stage 4
    x = vmlaq_n_s32(rnding, u[0], cospi[32]);
    y = vmulq_n_s32(u[1], cospi[32]);
    v[0] = vaddq_s32(x, y);
    v[0] = vshlq_s32(v[0], v_bit);

    v[1] = vsubq_s32(x, y);
    v[1] = vshlq_s32(v[1], v_bit);

    v[2] = half_btf_neon_mode01_r(&cospi[48], &u[2], &cospi[16], &u[3], &v_bit,
                                  &rnding);
    v[3] =
        half_btf_neon_r(&cospi[16], &u[2], &cospi[48], &u[3], &v_bit, &rnding);
    addsub_neon(u[4], u[5], &v[4], &v[5], &clamp_lo, &clamp_hi);
    addsub_neon(u[7], u[6], &v[7], &v[6], &clamp_lo, &clamp_hi);
    v[8] = u[8];
    v[9] = half_btf_neon_mode10_r(&cospi[16], &u[9], &cospi[48], &u[14], &v_bit,
                                  &rnding);
    v[10] = half_btf_neon_mode11_r(&cospi[48], &u[10], &cospi[16], &u[13],
                                   &v_bit, &rnding);
    v[11] = u[11];
    v[12] = u[12];
    v[13] = half_btf_neon_mode10_r(&cospi[16], &u[10], &cospi[48], &u[13],
                                   &v_bit, &rnding);
    v[14] =
        half_btf_neon_r(&cospi[48], &u[9], &cospi[16], &u[14], &v_bit, &rnding);
    v[15] = u[15];

    // stage 5
    addsub_neon(v[0], v[3], &u[0], &u[3], &clamp_lo, &clamp_hi);
    addsub_neon(v[1], v[2], &u[1], &u[2], &clamp_lo, &clamp_hi);
    u[4] = v[4];

    x = vmulq_n_s32(v[5], cospi[32]);
    y = vmlaq_n_s32(rnding, v[6], cospi[32]);
    u[5] = vsubq_s32(y, x);
    u[5] = vshlq_s32(u[5], v_bit);

    u[6] = vaddq_s32(y, x);
    u[6] = vshlq_s32(u[6], v_bit);

    u[7] = v[7];
    addsub_neon(v[8], v[11], &u[8], &u[11], &clamp_lo, &clamp_hi);
    addsub_neon(v[9], v[10], &u[9], &u[10], &clamp_lo, &clamp_hi);
    addsub_neon(v[15], v[12], &u[15], &u[12], &clamp_lo, &clamp_hi);
    addsub_neon(v[14], v[13], &u[14], &u[13], &clamp_lo, &clamp_hi);

    // stage 6
    addsub_neon(u[0], u[7], &v[0], &v[7], &clamp_lo, &clamp_hi);
    addsub_neon(u[1], u[6], &v[1], &v[6], &clamp_lo, &clamp_hi);
    addsub_neon(u[2], u[5], &v[2], &v[5], &clamp_lo, &clamp_hi);
    addsub_neon(u[3], u[4], &v[3], &v[4], &clamp_lo, &clamp_hi);
    v[8] = u[8];
    v[9] = u[9];

    x = vmulq_n_s32(u[10], cospi[32]);
    y = vmlaq_n_s32(rnding, u[13], cospi[32]);
    v[10] = vsubq_s32(y, x);
    v[10] = vshlq_s32(v[10], v_bit);

    v[13] = vaddq_s32(x, y);
    v[13] = vshlq_s32(v[13], v_bit);

    x = vmulq_n_s32(u[11], cospi[32]);
    y = vmlaq_n_s32(rnding, u[12], cospi[32]);
    v[11] = vsubq_s32(y, x);
    v[11] = vshlq_s32(v[11], v_bit);

    v[12] = vaddq_s32(x, y);
    v[12] = vshlq_s32(v[12], v_bit);

    v[14] = u[14];
    v[15] = u[15];

    // stage 7
    addsub_neon(v[0], v[15], out + 0, out + 15, &clamp_lo, &clamp_hi);
    addsub_neon(v[1], v[14], out + 1, out + 14, &clamp_lo, &clamp_hi);
    addsub_neon(v[2], v[13], out + 2, out + 13, &clamp_lo, &clamp_hi);
    addsub_neon(v[3], v[12], out + 3, out + 12, &clamp_lo, &clamp_hi);
    addsub_neon(v[4], v[11], out + 4, out + 11, &clamp_lo, &clamp_hi);
    addsub_neon(v[5], v[10], out + 5, out + 10, &clamp_lo, &clamp_hi);
    addsub_neon(v[6], v[9], out + 6, out + 9, &clamp_lo, &clamp_hi);
    addsub_neon(v[7], v[8], out + 7, out + 8, &clamp_lo, &clamp_hi);

    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
      const int32x4_t clamp_hi_out =
          vdupq_n_s32((1 << (log_range_out - 1)) - 1);
      round_shift_8x8(out, out_shift);
      highbd_clamp_s32_neon(out, out, &clamp_lo_out, &clamp_hi_out, 16);
    }
  }
}

static void iadst16x16_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                            int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  const int32x4_t zero = vdupq_n_s32(0);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  int32x4_t u[16], v[16], x, y;
  // Calculate the column 0, 1, 2, 3
  // stage 0
  // stage 1
  // stage 2
  v[0] = vmlaq_n_s32(rnding, in[15], cospi[2]);
  v[0] = vmlaq_n_s32(v[0], in[0], cospi[62]);
  v[0] = vshlq_s32(v[0], v_bit);

  v[1] = vmlaq_n_s32(rnding, in[15], cospi[62]);
  v[1] = vmlsq_n_s32(v[1], in[0], cospi[2]);
  v[1] = vshlq_s32(v[1], v_bit);

  v[2] = vmlaq_n_s32(rnding, in[13], cospi[10]);
  v[2] = vmlaq_n_s32(v[2], in[2], cospi[54]);
  v[2] = vshlq_s32(v[2], v_bit);

  v[3] = vmlaq_n_s32(rnding, in[13], cospi[54]);
  v[3] = vmlsq_n_s32(v[3], in[2], cospi[10]);
  v[3] = vshlq_s32(v[3], v_bit);

  v[4] = vmlaq_n_s32(rnding, in[11], cospi[18]);
  v[4] = vmlaq_n_s32(v[4], in[4], cospi[46]);
  v[4] = vshlq_s32(v[4], v_bit);

  v[5] = vmlaq_n_s32(rnding, in[11], cospi[46]);
  v[5] = vmlsq_n_s32(v[5], in[4], cospi[18]);
  v[5] = vshlq_s32(v[5], v_bit);

  v[6] = vmlaq_n_s32(rnding, in[9], cospi[26]);
  v[6] = vmlaq_n_s32(v[6], in[6], cospi[38]);
  v[6] = vshlq_s32(v[6], v_bit);

  v[7] = vmlaq_n_s32(rnding, in[9], cospi[38]);
  v[7] = vmlsq_n_s32(v[7], in[6], cospi[26]);
  v[7] = vshlq_s32(v[7], v_bit);

  v[8] = vmlaq_n_s32(rnding, in[7], cospi[34]);
  v[8] = vmlaq_n_s32(v[8], in[8], cospi[30]);
  v[8] = vshlq_s32(v[8], v_bit);

  v[9] = vmlaq_n_s32(rnding, in[7], cospi[30]);
  v[9] = vmlsq_n_s32(v[9], in[8], cospi[34]);
  v[9] = vshlq_s32(v[9], v_bit);

  v[10] = vmlaq_n_s32(rnding, in[5], cospi[42]);
  v[10] = vmlaq_n_s32(v[10], in[10], cospi[22]);
  v[10] = vshlq_s32(v[10], v_bit);

  v[11] = vmlaq_n_s32(rnding, in[5], cospi[22]);
  v[11] = vmlsq_n_s32(v[11], in[10], cospi[42]);
  v[11] = vshlq_s32(v[11], v_bit);

  v[12] = vmlaq_n_s32(rnding, in[3], cospi[50]);
  v[12] = vmlaq_n_s32(v[12], in[12], cospi[14]);
  v[12] = vshlq_s32(v[12], v_bit);

  v[13] = vmlaq_n_s32(rnding, in[3], cospi[14]);
  v[13] = vmlsq_n_s32(v[13], in[12], cospi[50]);
  v[13] = vshlq_s32(v[13], v_bit);

  v[14] = vmlaq_n_s32(rnding, in[1], cospi[58]);
  v[14] = vmlaq_n_s32(v[14], in[14], cospi[6]);
  v[14] = vshlq_s32(v[14], v_bit);

  v[15] = vmlaq_n_s32(rnding, in[1], cospi[6]);
  v[15] = vmlsq_n_s32(v[15], in[14], cospi[58]);
  v[15] = vshlq_s32(v[15], v_bit);

  // stage 3
  addsub_neon(v[0], v[8], &u[0], &u[8], &clamp_lo, &clamp_hi);
  addsub_neon(v[1], v[9], &u[1], &u[9], &clamp_lo, &clamp_hi);
  addsub_neon(v[2], v[10], &u[2], &u[10], &clamp_lo, &clamp_hi);
  addsub_neon(v[3], v[11], &u[3], &u[11], &clamp_lo, &clamp_hi);
  addsub_neon(v[4], v[12], &u[4], &u[12], &clamp_lo, &clamp_hi);
  addsub_neon(v[5], v[13], &u[5], &u[13], &clamp_lo, &clamp_hi);
  addsub_neon(v[6], v[14], &u[6], &u[14], &clamp_lo, &clamp_hi);
  addsub_neon(v[7], v[15], &u[7], &u[15], &clamp_lo, &clamp_hi);

  // stage 4
  v[0] = u[0];
  v[1] = u[1];
  v[2] = u[2];
  v[3] = u[3];
  v[4] = u[4];
  v[5] = u[5];
  v[6] = u[6];
  v[7] = u[7];

  v[8] = vmlaq_n_s32(rnding, u[8], cospi[8]);
  v[8] = vmlaq_n_s32(v[8], u[9], cospi[56]);
  v[8] = vshlq_s32(v[8], v_bit);

  v[9] = vmlaq_n_s32(rnding, u[8], cospi[56]);
  v[9] = vmlsq_n_s32(v[9], u[9], cospi[8]);
  v[9] = vshlq_s32(v[9], v_bit);

  v[10] = vmlaq_n_s32(rnding, u[10], cospi[40]);
  v[10] = vmlaq_n_s32(v[10], u[11], cospi[24]);
  v[10] = vshlq_s32(v[10], v_bit);

  v[11] = vmlaq_n_s32(rnding, u[10], cospi[24]);
  v[11] = vmlsq_n_s32(v[11], u[11], cospi[40]);
  v[11] = vshlq_s32(v[11], v_bit);

  v[12] = vmlaq_n_s32(rnding, u[12], -cospi[56]);
  v[12] = vmlaq_n_s32(v[12], u[13], cospi[8]);
  v[12] = vshlq_s32(v[12], v_bit);

  v[13] = vmlaq_n_s32(rnding, u[12], cospi[8]);
  v[13] = vmlsq_n_s32(v[13], u[13], -cospi[56]);
  v[13] = vshlq_s32(v[13], v_bit);

  v[14] = vmlaq_n_s32(rnding, u[14], -cospi[24]);
  v[14] = vmlaq_n_s32(v[14], u[15], cospi[40]);
  v[14] = vshlq_s32(v[14], v_bit);

  v[15] = vmlaq_n_s32(rnding, u[14], cospi[40]);
  v[15] = vmlsq_n_s32(v[15], u[15], -cospi[24]);
  v[15] = vshlq_s32(v[15], v_bit);

  // stage 5
  addsub_neon(v[0], v[4], &u[0], &u[4], &clamp_lo, &clamp_hi);
  addsub_neon(v[1], v[5], &u[1], &u[5], &clamp_lo, &clamp_hi);
  addsub_neon(v[2], v[6], &u[2], &u[6], &clamp_lo, &clamp_hi);
  addsub_neon(v[3], v[7], &u[3], &u[7], &clamp_lo, &clamp_hi);
  addsub_neon(v[8], v[12], &u[8], &u[12], &clamp_lo, &clamp_hi);
  addsub_neon(v[9], v[13], &u[9], &u[13], &clamp_lo, &clamp_hi);
  addsub_neon(v[10], v[14], &u[10], &u[14], &clamp_lo, &clamp_hi);
  addsub_neon(v[11], v[15], &u[11], &u[15], &clamp_lo, &clamp_hi);

  // stage 6
  v[0] = u[0];
  v[1] = u[1];
  v[2] = u[2];
  v[3] = u[3];

  v[4] = vmlaq_n_s32(rnding, u[4], cospi[16]);
  v[4] = vmlaq_n_s32(v[4], u[5], cospi[48]);
  v[4] = vshlq_s32(v[4], v_bit);

  v[5] = vmlaq_n_s32(rnding, u[4], cospi[48]);
  v[5] = vmlsq_n_s32(v[5], u[5], cospi[16]);
  v[5] = vshlq_s32(v[5], v_bit);

  v[6] = vmlaq_n_s32(rnding, u[6], -cospi[48]);
  v[6] = vmlaq_n_s32(v[6], u[7], cospi[16]);
  v[6] = vshlq_s32(v[6], v_bit);

  v[7] = vmlaq_n_s32(rnding, u[6], cospi[16]);
  v[7] = vmlsq_n_s32(v[7], u[7], -cospi[48]);
  v[7] = vshlq_s32(v[7], v_bit);

  v[8] = u[8];
  v[9] = u[9];
  v[10] = u[10];
  v[11] = u[11];

  v[12] = vmlaq_n_s32(rnding, u[12], cospi[16]);
  v[12] = vmlaq_n_s32(v[12], u[13], cospi[48]);
  v[12] = vshlq_s32(v[12], v_bit);

  v[13] = vmlaq_n_s32(rnding, u[12], cospi[48]);
  v[13] = vmlsq_n_s32(v[13], u[13], cospi[16]);
  v[13] = vshlq_s32(v[13], v_bit);

  v[14] = vmlaq_n_s32(rnding, u[14], -cospi[48]);
  v[14] = vmlaq_n_s32(v[14], u[15], cospi[16]);
  v[14] = vshlq_s32(v[14], v_bit);

  v[15] = vmlaq_n_s32(rnding, u[14], cospi[16]);
  v[15] = vmlsq_n_s32(v[15], u[15], -cospi[48]);
  v[15] = vshlq_s32(v[15], v_bit);

  // stage 7
  addsub_neon(v[0], v[2], &u[0], &u[2], &clamp_lo, &clamp_hi);
  addsub_neon(v[1], v[3], &u[1], &u[3], &clamp_lo, &clamp_hi);
  addsub_neon(v[4], v[6], &u[4], &u[6], &clamp_lo, &clamp_hi);
  addsub_neon(v[5], v[7], &u[5], &u[7], &clamp_lo, &clamp_hi);
  addsub_neon(v[8], v[10], &u[8], &u[10], &clamp_lo, &clamp_hi);
  addsub_neon(v[9], v[11], &u[9], &u[11], &clamp_lo, &clamp_hi);
  addsub_neon(v[12], v[14], &u[12], &u[14], &clamp_lo, &clamp_hi);
  addsub_neon(v[13], v[15], &u[13], &u[15], &clamp_lo, &clamp_hi);

  // stage 8
  v[0] = u[0];
  v[1] = u[1];

  y = vmlaq_n_s32(rnding, u[2], cospi[32]);
  x = vmulq_n_s32(u[3], cospi[32]);
  v[2] = vaddq_s32(y, x);
  v[2] = vshlq_s32(v[2], v_bit);

  v[3] = vsubq_s32(y, x);
  v[3] = vshlq_s32(v[3], v_bit);

  v[4] = u[4];
  v[5] = u[5];

  y = vmlaq_n_s32(rnding, u[6], cospi[32]);
  x = vmulq_n_s32(u[7], cospi[32]);
  v[6] = vaddq_s32(y, x);
  v[6] = vshlq_s32(v[6], v_bit);

  v[7] = vsubq_s32(y, x);
  v[7] = vshlq_s32(v[7], v_bit);

  v[8] = u[8];
  v[9] = u[9];

  y = vmlaq_n_s32(rnding, u[10], cospi[32]);
  x = vmulq_n_s32(u[11], cospi[32]);
  v[10] = vaddq_s32(y, x);
  v[10] = vshlq_s32(v[10], v_bit);

  v[11] = vsubq_s32(y, x);
  v[11] = vshlq_s32(v[11], v_bit);

  v[12] = u[12];
  v[13] = u[13];

  y = vmlaq_n_s32(rnding, u[14], cospi[32]);
  x = vmulq_n_s32(u[15], cospi[32]);
  v[14] = vaddq_s32(y, x);
  v[14] = vshlq_s32(v[14], v_bit);

  v[15] = vsubq_s32(y, x);
  v[15] = vshlq_s32(v[15], v_bit);

  // stage 9
  if (do_cols) {
    out[0] = v[0];
    out[1] = vsubq_s32(zero, v[8]);
    out[2] = v[12];
    out[3] = vsubq_s32(zero, v[4]);
    out[4] = v[6];
    out[5] = vsubq_s32(zero, v[14]);
    out[6] = v[10];
    out[7] = vsubq_s32(zero, v[2]);
    out[8] = v[3];
    out[9] = vsubq_s32(zero, v[11]);
    out[10] = v[15];
    out[11] = vsubq_s32(zero, v[7]);
    out[12] = v[5];
    out[13] = vsubq_s32(zero, v[13]);
    out[14] = v[9];
    out[15] = vsubq_s32(zero, v[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    const int32x4_t v_shift = vdupq_n_s32(-out_shift);
    int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
    neg_shift_neon(&v[0], &v[8], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                   &v_shift, &offset);
    neg_shift_neon(&v[12], &v[4], out + 2, out + 3, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[6], &v[14], out + 4, out + 5, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[10], &v[2], out + 6, out + 7, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[3], &v[11], out + 8, out + 9, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[15], &v[7], out + 10, out + 11, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[5], &v[13], out + 12, out + 13, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
    neg_shift_neon(&v[9], &v[1], out + 14, out + 15, &clamp_lo_out,
                   &clamp_hi_out, &v_shift, &offset);
  }
}

static void iidentity16_neon(int32x4_t *in, int32x4_t *out, int bit,
                             int do_cols, int bd, int out_shift) {
  (void)bit;
  int32x2_t fact = vdup_n_s32(2 * NewSqrt2);
  int32x4x2_t a0;
  int32x4_t zero = vdupq_n_s32(0);
  const int64x2_t rnding = vdupq_n_s64(1 << (NewSqrt2Bits - 1));
  for (int i = 0; i < 16; i++) {
    a0.val[0] = vreinterpretq_s32_s64(
        vmlal_s32(rnding, vmovn_s64(vreinterpretq_s64_s32(in[i])), fact));
    a0.val[0] = vreinterpretq_s32_s64(
        vshrq_n_s64(vreinterpretq_s64_s32(a0.val[0]), NewSqrt2Bits));
    a0.val[1] = vextq_s32(in[i], zero, 1);
    a0.val[1] = vreinterpretq_s32_s64(
        vmlal_s32(rnding, vmovn_s64(vreinterpretq_s64_s32(a0.val[1])), fact));
    a0.val[1] = vreinterpretq_s32_s64(
        vshrq_n_s64(vreinterpretq_s64_s32(a0.val[1]), NewSqrt2Bits));
    a0 = vzipq_s32(a0.val[0], a0.val[1]);
#if AOM_ARCH_AARCH64
    out[i] = vreinterpretq_s32_s64(vzip1q_s64(
        vreinterpretq_s64_s32(a0.val[0]), vreinterpretq_s64_s32(a0.val[1])));
#else
    out[i] = vextq_s32(vextq_s32(a0.val[0], a0.val[0], 2), a0.val[1], 2);
#endif
  }

  if (!do_cols) {
    const int log_range = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
    const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
    round_shift_8x8(out, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo, &clamp_hi, 16);
  }
}

static INLINE void idct64_stage8_neon(int32x4_t *u, const int32_t *cospi,
                                      const int32x4_t *clamp_lo,
                                      const int32x4_t *clamp_hi,
                                      const int32x4_t *v_bit,
                                      const int32x4_t *rnding) {
  int i;
  int32x4_t temp1, temp2, temp3, temp4;
  temp1 = half_btf_neon_mode10_r(&cospi[32], &u[10], &cospi[32], &u[13], v_bit,
                                 rnding);
  u[13] =
      half_btf_neon_r(&cospi[32], &u[10], &cospi[32], &u[13], v_bit, rnding);
  u[10] = temp1;
  temp2 = half_btf_neon_mode10_r(&cospi[32], &u[11], &cospi[32], &u[12], v_bit,
                                 rnding);
  u[12] =
      half_btf_neon_r(&cospi[32], &u[11], &cospi[32], &u[12], v_bit, rnding);
  u[11] = temp2;

  for (i = 16; i < 20; ++i) {
    addsub_neon(u[i], u[i ^ 7], &u[i], &u[i ^ 7], clamp_lo, clamp_hi);
    addsub_neon(u[i ^ 15], u[i ^ 8], &u[i ^ 15], &u[i ^ 8], clamp_lo, clamp_hi);
  }

  temp1 = half_btf_neon_mode10_r(&cospi[16], &u[36], &cospi[48], &u[59], v_bit,
                                 rnding);
  temp2 = half_btf_neon_mode10_r(&cospi[16], &u[37], &cospi[48], &u[58], v_bit,
                                 rnding);
  temp3 = half_btf_neon_mode10_r(&cospi[16], &u[38], &cospi[48], &u[57], v_bit,
                                 rnding);
  temp4 = half_btf_neon_mode10_r(&cospi[16], &u[39], &cospi[48], &u[56], v_bit,
                                 rnding);
  u[56] =
      half_btf_neon_r(&cospi[48], &u[39], &cospi[16], &u[56], v_bit, rnding);
  u[57] =
      half_btf_neon_r(&cospi[48], &u[38], &cospi[16], &u[57], v_bit, rnding);
  u[58] =
      half_btf_neon_r(&cospi[48], &u[37], &cospi[16], &u[58], v_bit, rnding);
  u[59] =
      half_btf_neon_r(&cospi[48], &u[36], &cospi[16], &u[59], v_bit, rnding);
  u[36] = temp1;
  u[37] = temp2;
  u[38] = temp3;
  u[39] = temp4;

  temp1 = half_btf_neon_mode11_r(&cospi[48], &u[40], &cospi[16], &u[55], v_bit,
                                 rnding);
  temp2 = half_btf_neon_mode11_r(&cospi[48], &u[41], &cospi[16], &u[54], v_bit,
                                 rnding);
  temp3 = half_btf_neon_mode11_r(&cospi[48], &u[42], &cospi[16], &u[53], v_bit,
                                 rnding);
  temp4 = half_btf_neon_mode11_r(&cospi[48], &u[43], &cospi[16], &u[52], v_bit,
                                 rnding);
  u[52] = half_btf_neon_mode10_r(&cospi[16], &u[43], &cospi[48], &u[52], v_bit,
                                 rnding);
  u[53] = half_btf_neon_mode10_r(&cospi[16], &u[42], &cospi[48], &u[53], v_bit,
                                 rnding);
  u[54] = half_btf_neon_mode10_r(&cospi[16], &u[41], &cospi[48], &u[54], v_bit,
                                 rnding);
  u[55] = half_btf_neon_mode10_r(&cospi[16], &u[40], &cospi[48], &u[55], v_bit,
                                 rnding);
  u[40] = temp1;
  u[41] = temp2;
  u[42] = temp3;
  u[43] = temp4;
}

static INLINE void idct64_stage9_neon(int32x4_t *u, const int32_t *cospi,
                                      const int32x4_t *clamp_lo,
                                      const int32x4_t *clamp_hi,
                                      const int32x4_t *v_bit,
                                      const int32x4_t *rnding) {
  int i;
  int32x4_t temp1, temp2, temp3, temp4;
  for (i = 0; i < 8; ++i) {
    addsub_neon(u[i], u[15 - i], &u[i], &u[15 - i], clamp_lo, clamp_hi);
  }
  temp1 = half_btf_neon_mode10_r(&cospi[32], &u[20], &cospi[32], &u[27], v_bit,
                                 rnding);
  temp2 = half_btf_neon_mode10_r(&cospi[32], &u[21], &cospi[32], &u[26], v_bit,
                                 rnding);
  temp3 = half_btf_neon_mode10_r(&cospi[32], &u[22], &cospi[32], &u[25], v_bit,
                                 rnding);
  temp4 = half_btf_neon_mode10_r(&cospi[32], &u[23], &cospi[32], &u[24], v_bit,
                                 rnding);
  u[24] =
      half_btf_neon_r(&cospi[32], &u[23], &cospi[32], &u[24], v_bit, rnding);
  u[25] =
      half_btf_neon_r(&cospi[32], &u[22], &cospi[32], &u[25], v_bit, rnding);
  u[26] =
      half_btf_neon_r(&cospi[32], &u[21], &cospi[32], &u[26], v_bit, rnding);
  u[27] =
      half_btf_neon_r(&cospi[32], &u[20], &cospi[32], &u[27], v_bit, rnding);
  u[20] = temp1;
  u[21] = temp2;
  u[22] = temp3;
  u[23] = temp4;
  for (i = 32; i < 40; i++) {
    addsub_neon(u[i], u[i ^ 15], &u[i], &u[i ^ 15], clamp_lo, clamp_hi);
  }

  for (i = 48; i < 56; i++) {
    addsub_neon(u[i ^ 15], u[i], &u[i ^ 15], &u[i], clamp_lo, clamp_hi);
  }
}

static INLINE void idct64_stage10_neon(int32x4_t *u, const int32_t *cospi,
                                       const int32x4_t *clamp_lo,
                                       const int32x4_t *clamp_hi,
                                       const int32x4_t *v_bit,
                                       const int32x4_t *rnding) {
  int32x4_t temp1, temp2, temp3, temp4;
  for (int i = 0; i < 16; i++) {
    addsub_neon(u[i], u[31 - i], &u[i], &u[31 - i], clamp_lo, clamp_hi);
  }
  temp1 = half_btf_neon_mode10_r(&cospi[32], &u[40], &cospi[32], &u[55], v_bit,
                                 rnding);
  temp2 = half_btf_neon_mode10_r(&cospi[32], &u[41], &cospi[32], &u[54], v_bit,
                                 rnding);
  temp3 = half_btf_neon_mode10_r(&cospi[32], &u[42], &cospi[32], &u[53], v_bit,
                                 rnding);
  temp4 = half_btf_neon_mode10_r(&cospi[32], &u[43], &cospi[32], &u[52], v_bit,
                                 rnding);
  u[52] =
      half_btf_neon_r(&cospi[32], &u[43], &cospi[32], &u[52], v_bit, rnding);
  u[53] =
      half_btf_neon_r(&cospi[32], &u[42], &cospi[32], &u[53], v_bit, rnding);
  u[54] =
      half_btf_neon_r(&cospi[32], &u[41], &cospi[32], &u[54], v_bit, rnding);
  u[55] =
      half_btf_neon_r(&cospi[32], &u[40], &cospi[32], &u[55], v_bit, rnding);
  u[40] = temp1;
  u[41] = temp2;
  u[42] = temp3;
  u[43] = temp4;

  temp1 = half_btf_neon_mode10_r(&cospi[32], &u[44], &cospi[32], &u[51], v_bit,
                                 rnding);
  temp2 = half_btf_neon_mode10_r(&cospi[32], &u[45], &cospi[32], &u[50], v_bit,
                                 rnding);
  temp3 = half_btf_neon_mode10_r(&cospi[32], &u[46], &cospi[32], &u[49], v_bit,
                                 rnding);
  temp4 = half_btf_neon_mode10_r(&cospi[32], &u[47], &cospi[32], &u[48], v_bit,
                                 rnding);
  u[48] =
      half_btf_neon_r(&cospi[32], &u[47], &cospi[32], &u[48], v_bit, rnding);
  u[49] =
      half_btf_neon_r(&cospi[32], &u[46], &cospi[32], &u[49], v_bit, rnding);
  u[50] =
      half_btf_neon_r(&cospi[32], &u[45], &cospi[32], &u[50], v_bit, rnding);
  u[51] =
      half_btf_neon_r(&cospi[32], &u[44], &cospi[32], &u[51], v_bit, rnding);
  u[44] = temp1;
  u[45] = temp2;
  u[46] = temp3;
  u[47] = temp4;
}

static INLINE void idct64_stage11_neon(int32x4_t *u, int32x4_t *out,
                                       int do_cols, int bd, int out_shift,
                                       const int32x4_t *clamp_lo,
                                       const int32x4_t *clamp_hi) {
  for (int i = 0; i < 32; i++) {
    addsub_neon(u[i], u[63 - i], out + i, out + 63 - i, clamp_lo, clamp_hi);
  }

  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    for (int i = 0; i < 64; i += 4) {
      round_shift_4x4(out + i, out_shift);
      highbd_clamp_s32_neon(out + i, out + i, &clamp_lo_out, &clamp_hi_out, 4);
    }
  }
}

static void idct64x64_low1_neon(int32x4_t *in, int32x4_t *out, int bit,
                                int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);

  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  {
    int32x4_t x;

    // stage 1
    // stage 2
    // stage 3
    // stage 4
    // stage 5
    // stage 6
    x = half_btf_0_neon_r(&cospi[32], &in[0], &v_bit, &rnding);

    // stage 8
    // stage 9
    // stage 10
    // stage 11
    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      clamp_lo = vdupq_n_s32(-(1 << (log_range_out - 1)));
      clamp_hi = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
      if (out_shift != 0) {
        int32x4_t offset = vdupq_n_s32((1 << out_shift) >> 1);
        x = vaddq_s32(x, offset);
        x = vshlq_s32(x, vdupq_n_s32(-out_shift));
      }
    }
    x = vmaxq_s32(x, clamp_lo);
    x = vminq_s32(x, clamp_hi);
    out[0] = x;
    out[1] = x;
    out[2] = x;
    out[3] = x;
    out[4] = x;
    out[5] = x;
    out[6] = x;
    out[7] = x;
    out[8] = x;
    out[9] = x;
    out[10] = x;
    out[11] = x;
    out[12] = x;
    out[13] = x;
    out[14] = x;
    out[15] = x;
    out[16] = x;
    out[17] = x;
    out[18] = x;
    out[19] = x;
    out[20] = x;
    out[21] = x;
    out[22] = x;
    out[23] = x;
    out[24] = x;
    out[25] = x;
    out[26] = x;
    out[27] = x;
    out[28] = x;
    out[29] = x;
    out[30] = x;
    out[31] = x;
    out[32] = x;
    out[33] = x;
    out[34] = x;
    out[35] = x;
    out[36] = x;
    out[37] = x;
    out[38] = x;
    out[39] = x;
    out[40] = x;
    out[41] = x;
    out[42] = x;
    out[43] = x;
    out[44] = x;
    out[45] = x;
    out[46] = x;
    out[47] = x;
    out[48] = x;
    out[49] = x;
    out[50] = x;
    out[51] = x;
    out[52] = x;
    out[53] = x;
    out[54] = x;
    out[55] = x;
    out[56] = x;
    out[57] = x;
    out[58] = x;
    out[59] = x;
    out[60] = x;
    out[61] = x;
    out[62] = x;
    out[63] = x;
  }
}

static void idct64x64_low8_neon(int32x4_t *in, int32x4_t *out, int bit,
                                int do_cols, int bd, int out_shift) {
  int i, j;
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  {
    int32x4_t u[64];

    // stage 1
    u[0] = in[0];
    u[8] = in[4];
    u[16] = in[2];
    u[24] = in[6];
    u[32] = in[1];
    u[40] = in[5];
    u[48] = in[3];
    u[56] = in[7];

    // stage 2
    u[63] = half_btf_0_neon_r(&cospi[1], &u[32], &v_bit, &rnding);
    u[32] = half_btf_0_neon_r(&cospi[63], &u[32], &v_bit, &rnding);
    u[39] = half_btf_0_m_neon_r(&cospi[57], &u[56], &v_bit, &rnding);
    u[56] = half_btf_0_neon_r(&cospi[7], &u[56], &v_bit, &rnding);
    u[55] = half_btf_0_neon_r(&cospi[5], &u[40], &v_bit, &rnding);
    u[40] = half_btf_0_neon_r(&cospi[59], &u[40], &v_bit, &rnding);
    u[47] = half_btf_0_m_neon_r(&cospi[61], &u[48], &v_bit, &rnding);
    u[48] = half_btf_0_neon_r(&cospi[3], &u[48], &v_bit, &rnding);

    // stage 3
    u[31] = half_btf_0_neon_r(&cospi[2], &u[16], &v_bit, &rnding);
    u[16] = half_btf_0_neon_r(&cospi[62], &u[16], &v_bit, &rnding);
    u[23] = half_btf_0_m_neon_r(&cospi[58], &u[24], &v_bit, &rnding);
    u[24] = half_btf_0_neon_r(&cospi[6], &u[24], &v_bit, &rnding);
    u[33] = u[32];
    u[38] = u[39];
    u[41] = u[40];
    u[46] = u[47];
    u[49] = u[48];
    u[54] = u[55];
    u[57] = u[56];
    u[62] = u[63];

    // stage 4
    int32x4_t temp1, temp2;
    u[15] = half_btf_0_neon_r(&cospi[4], &u[8], &v_bit, &rnding);
    u[8] = half_btf_0_neon_r(&cospi[60], &u[8], &v_bit, &rnding);
    u[17] = u[16];
    u[22] = u[23];
    u[25] = u[24];
    u[30] = u[31];

    temp1 = half_btf_neon_mode10_r(&cospi[4], &u[33], &cospi[60], &u[62],
                                   &v_bit, &rnding);
    u[62] =
        half_btf_neon_r(&cospi[60], &u[33], &cospi[4], &u[62], &v_bit, &rnding);
    u[33] = temp1;

    temp2 = half_btf_neon_mode10_r(&cospi[36], &u[38], &cospi[28], &u[57],
                                   &v_bit, &rnding);
    u[38] = half_btf_neon_mode11_r(&cospi[28], &u[38], &cospi[36], &u[57],
                                   &v_bit, &rnding);
    u[57] = temp2;

    temp1 = half_btf_neon_mode10_r(&cospi[20], &u[41], &cospi[44], &u[54],
                                   &v_bit, &rnding);
    u[54] = half_btf_neon_r(&cospi[44], &u[41], &cospi[20], &u[54], &v_bit,
                            &rnding);
    u[41] = temp1;

    temp2 = half_btf_neon_mode11_r(&cospi[12], &u[46], &cospi[52], &u[49],
                                   &v_bit, &rnding);
    u[49] = half_btf_neon_mode10_r(&cospi[52], &u[46], &cospi[12], &u[49],
                                   &v_bit, &rnding);
    u[46] = temp2;

    // stage 5
    u[9] = u[8];
    u[14] = u[15];

    temp1 = half_btf_neon_mode10_r(&cospi[8], &u[17], &cospi[56], &u[30],
                                   &v_bit, &rnding);
    u[30] =
        half_btf_neon_r(&cospi[56], &u[17], &cospi[8], &u[30], &v_bit, &rnding);
    u[17] = temp1;

    temp2 = half_btf_neon_mode11_r(&cospi[24], &u[22], &cospi[40], &u[25],
                                   &v_bit, &rnding);
    u[25] = half_btf_neon_mode10_r(&cospi[40], &u[22], &cospi[24], &u[25],
                                   &v_bit, &rnding);
    u[22] = temp2;

    u[35] = u[32];
    u[34] = u[33];
    u[36] = u[39];
    u[37] = u[38];
    u[43] = u[40];
    u[42] = u[41];
    u[44] = u[47];
    u[45] = u[46];
    u[51] = u[48];
    u[50] = u[49];
    u[52] = u[55];
    u[53] = u[54];
    u[59] = u[56];
    u[58] = u[57];
    u[60] = u[63];
    u[61] = u[62];

    // stage 6
    temp1 = half_btf_0_neon_r(&cospi[32], &u[0], &v_bit, &rnding);
    u[1] = half_btf_0_neon_r(&cospi[32], &u[0], &v_bit, &rnding);
    u[0] = temp1;

    temp2 = half_btf_neon_mode10_r(&cospi[16], &u[9], &cospi[48], &u[14],
                                   &v_bit, &rnding);
    u[14] =
        half_btf_neon_r(&cospi[48], &u[9], &cospi[16], &u[14], &v_bit, &rnding);
    u[9] = temp2;
    u[19] = u[16];
    u[18] = u[17];
    u[20] = u[23];
    u[21] = u[22];
    u[27] = u[24];
    u[26] = u[25];
    u[28] = u[31];
    u[29] = u[30];

    temp1 = half_btf_neon_mode10_r(&cospi[8], &u[34], &cospi[56], &u[61],
                                   &v_bit, &rnding);
    u[61] =
        half_btf_neon_r(&cospi[56], &u[34], &cospi[8], &u[61], &v_bit, &rnding);
    u[34] = temp1;
    temp2 = half_btf_neon_mode10_r(&cospi[8], &u[35], &cospi[56], &u[60],
                                   &v_bit, &rnding);
    u[60] =
        half_btf_neon_r(&cospi[56], &u[35], &cospi[8], &u[60], &v_bit, &rnding);
    u[35] = temp2;
    temp1 = half_btf_neon_mode11_r(&cospi[56], &u[36], &cospi[8], &u[59],
                                   &v_bit, &rnding);
    u[59] = half_btf_neon_mode10_r(&cospi[8], &u[36], &cospi[56], &u[59],
                                   &v_bit, &rnding);
    u[36] = temp1;
    temp2 = half_btf_neon_mode11_r(&cospi[56], &u[37], &cospi[8], &u[58],
                                   &v_bit, &rnding);
    u[58] = half_btf_neon_mode10_r(&cospi[8], &u[37], &cospi[56], &u[58],
                                   &v_bit, &rnding);
    u[37] = temp2;
    temp1 = half_btf_neon_mode10_r(&cospi[40], &u[42], &cospi[24], &u[53],
                                   &v_bit, &rnding);
    u[53] = half_btf_neon_r(&cospi[24], &u[42], &cospi[40], &u[53], &v_bit,
                            &rnding);
    u[42] = temp1;
    temp2 = half_btf_neon_mode10_r(&cospi[40], &u[43], &cospi[24], &u[52],
                                   &v_bit, &rnding);
    u[52] = half_btf_neon_r(&cospi[24], &u[43], &cospi[40], &u[52], &v_bit,
                            &rnding);
    u[43] = temp2;
    temp1 = half_btf_neon_mode11_r(&cospi[24], &u[44], &cospi[40], &u[51],
                                   &v_bit, &rnding);
    u[51] = half_btf_neon_mode10_r(&cospi[40], &u[44], &cospi[24], &u[51],
                                   &v_bit, &rnding);
    u[44] = temp1;
    temp2 = half_btf_neon_mode11_r(&cospi[24], &u[45], &cospi[40], &u[50],
                                   &v_bit, &rnding);
    u[50] = half_btf_neon_mode10_r(&cospi[40], &u[45], &cospi[24], &u[50],
                                   &v_bit, &rnding);
    u[45] = temp2;

    // stage 7
    u[3] = u[0];
    u[2] = u[1];
    u[11] = u[8];
    u[10] = u[9];
    u[12] = u[15];
    u[13] = u[14];

    temp1 = half_btf_neon_mode10_r(&cospi[16], &u[18], &cospi[48], &u[29],
                                   &v_bit, &rnding);
    u[29] = half_btf_neon_r(&cospi[48], &u[18], &cospi[16], &u[29], &v_bit,
                            &rnding);
    u[18] = temp1;
    temp2 = half_btf_neon_mode10_r(&cospi[16], &u[19], &cospi[48], &u[28],
                                   &v_bit, &rnding);
    u[28] = half_btf_neon_r(&cospi[48], &u[19], &cospi[16], &u[28], &v_bit,
                            &rnding);
    u[19] = temp2;
    temp1 = half_btf_neon_mode11_r(&cospi[48], &u[20], &cospi[16], &u[27],
                                   &v_bit, &rnding);
    u[27] = half_btf_neon_mode10_r(&cospi[16], &u[20], &cospi[48], &u[27],
                                   &v_bit, &rnding);
    u[20] = temp1;
    temp2 = half_btf_neon_mode11_r(&cospi[48], &u[21], &cospi[16], &u[26],
                                   &v_bit, &rnding);
    u[26] = half_btf_neon_mode10_r(&cospi[16], &u[21], &cospi[48], &u[26],
                                   &v_bit, &rnding);
    u[21] = temp2;
    for (i = 32; i < 64; i += 16) {
      for (j = i; j < i + 4; j++) {
        addsub_neon(u[j], u[j ^ 7], &u[j], &u[j ^ 7], &clamp_lo, &clamp_hi);
        addsub_neon(u[j ^ 15], u[j ^ 8], &u[j ^ 15], &u[j ^ 8], &clamp_lo,
                    &clamp_hi);
      }
    }

    // stage 8
    u[7] = u[0];
    u[6] = u[1];
    u[5] = u[2];
    u[4] = u[3];
    u[9] = u[9];

    idct64_stage8_neon(u, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

    // stage 9
    idct64_stage9_neon(u, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

    // stage 10
    idct64_stage10_neon(u, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

    // stage 11
    idct64_stage11_neon(u, out, do_cols, bd, out_shift, &clamp_lo, &clamp_hi);
  }
}

static void idct64x64_low16_neon(int32x4_t *in, int32x4_t *out, int bit,
                                 int do_cols, int bd, int out_shift) {
  int i, j;
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));

  {
    int32x4_t u[64];
    int32x4_t tmp1, tmp2, tmp3, tmp4;
    // stage 1
    u[0] = in[0];
    u[32] = in[1];
    u[36] = in[9];
    u[40] = in[5];
    u[44] = in[13];
    u[48] = in[3];
    u[52] = in[11];
    u[56] = in[7];
    u[60] = in[15];
    u[16] = in[2];
    u[20] = in[10];
    u[24] = in[6];
    u[28] = in[14];
    u[4] = in[8];
    u[8] = in[4];
    u[12] = in[12];

    // stage 2
    u[63] = half_btf_0_neon_r(&cospi[1], &u[32], &v_bit, &rnding);
    u[32] = half_btf_0_neon_r(&cospi[63], &u[32], &v_bit, &rnding);
    u[35] = half_btf_0_m_neon_r(&cospi[49], &u[60], &v_bit, &rnding);
    u[60] = half_btf_0_neon_r(&cospi[15], &u[60], &v_bit, &rnding);
    u[59] = half_btf_0_neon_r(&cospi[9], &u[36], &v_bit, &rnding);
    u[36] = half_btf_0_neon_r(&cospi[55], &u[36], &v_bit, &rnding);
    u[39] = half_btf_0_m_neon_r(&cospi[57], &u[56], &v_bit, &rnding);
    u[56] = half_btf_0_neon_r(&cospi[7], &u[56], &v_bit, &rnding);
    u[55] = half_btf_0_neon_r(&cospi[5], &u[40], &v_bit, &rnding);
    u[40] = half_btf_0_neon_r(&cospi[59], &u[40], &v_bit, &rnding);
    u[43] = half_btf_0_m_neon_r(&cospi[53], &u[52], &v_bit, &rnding);
    u[52] = half_btf_0_neon_r(&cospi[11], &u[52], &v_bit, &rnding);
    u[47] = half_btf_0_m_neon_r(&cospi[61], &u[48], &v_bit, &rnding);
    u[48] = half_btf_0_neon_r(&cospi[3], &u[48], &v_bit, &rnding);
    u[51] = half_btf_0_neon_r(&cospi[13], &u[44], &v_bit, &rnding);
    u[44] = half_btf_0_neon_r(&cospi[51], &u[44], &v_bit, &rnding);

    // stage 3
    u[31] = half_btf_0_neon_r(&cospi[2], &u[16], &v_bit, &rnding);
    u[16] = half_btf_0_neon_r(&cospi[62], &u[16], &v_bit, &rnding);
    u[19] = half_btf_0_m_neon_r(&cospi[50], &u[28], &v_bit, &rnding);
    u[28] = half_btf_0_neon_r(&cospi[14], &u[28], &v_bit, &rnding);
    u[27] = half_btf_0_neon_r(&cospi[10], &u[20], &v_bit, &rnding);
    u[20] = half_btf_0_neon_r(&cospi[54], &u[20], &v_bit, &rnding);
    u[23] = half_btf_0_m_neon_r(&cospi[58], &u[24], &v_bit, &rnding);
    u[24] = half_btf_0_neon_r(&cospi[6], &u[24], &v_bit, &rnding);
    u[33] = u[32];
    u[34] = u[35];
    u[37] = u[36];
    u[38] = u[39];
    u[41] = u[40];
    u[42] = u[43];
    u[45] = u[44];
    u[46] = u[47];
    u[49] = u[48];
    u[50] = u[51];
    u[53] = u[52];
    u[54] = u[55];
    u[57] = u[56];
    u[58] = u[59];
    u[61] = u[60];
    u[62] = u[63];

    // stage 4
    u[15] = half_btf_0_neon_r(&cospi[4], &u[8], &v_bit, &rnding);
    u[8] = half_btf_0_neon_r(&cospi[60], &u[8], &v_bit, &rnding);
    u[11] = half_btf_0_m_neon_r(&cospi[52], &u[12], &v_bit, &rnding);
    u[12] = half_btf_0_neon_r(&cospi[12], &u[12], &v_bit, &rnding);

    u[17] = u[16];
    u[18] = u[19];
    u[21] = u[20];
    u[22] = u[23];
    u[25] = u[24];
    u[26] = u[27];
    u[29] = u[28];
    u[30] = u[31];

    tmp1 = half_btf_neon_mode10_r(&cospi[4], &u[33], &cospi[60], &u[62], &v_bit,
                                  &rnding);
    tmp2 = half_btf_neon_mode11_r(&cospi[60], &u[34], &cospi[4], &u[61], &v_bit,
                                  &rnding);
    tmp3 = half_btf_neon_mode10_r(&cospi[36], &u[37], &cospi[28], &u[58],
                                  &v_bit, &rnding);
    tmp4 = half_btf_neon_mode11_r(&cospi[28], &u[38], &cospi[36], &u[57],
                                  &v_bit, &rnding);
    u[57] = half_btf_neon_mode10_r(&cospi[36], &u[38], &cospi[28], &u[57],
                                   &v_bit, &rnding);
    u[58] = half_btf_neon_r(&cospi[28], &u[37], &cospi[36], &u[58], &v_bit,
                            &rnding);
    u[61] = half_btf_neon_mode10_r(&cospi[4], &u[34], &cospi[60], &u[61],
                                   &v_bit, &rnding);
    u[62] =
        half_btf_neon_r(&cospi[60], &u[33], &cospi[4], &u[62], &v_bit, &rnding);
    u[33] = tmp1;
    u[34] = tmp2;
    u[37] = tmp3;
    u[38] = tmp4;

    tmp1 = half_btf_neon_mode10_r(&cospi[20], &u[41], &cospi[44], &u[54],
                                  &v_bit, &rnding);
    tmp2 = half_btf_neon_mode11_r(&cospi[44], &u[42], &cospi[20], &u[53],
                                  &v_bit, &rnding);
    tmp3 = half_btf_neon_r(&cospi[52], &u[45], &cospi[12], &u[50], &v_bit,
                           &rnding);
    tmp4 = half_btf_neon_mode11_r(&cospi[12], &u[46], &cospi[52], &u[49],
                                  &v_bit, &rnding);
    u[49] = half_btf_neon_mode10_r(&cospi[52], &u[46], &cospi[12], &u[49],
                                   &v_bit, &rnding);
    u[50] = half_btf_neon_r(&cospi[12], &u[45], &cospi[52], &u[50], &v_bit,
                            &rnding);
    u[53] = half_btf_neon_mode10_r(&cospi[20], &u[42], &cospi[44], &u[53],
                                   &v_bit, &rnding);
    u[54] = half_btf_neon_r(&cospi[44], &u[41], &cospi[20], &u[54], &v_bit,
                            &rnding);
    u[41] = tmp1;
    u[42] = tmp2;
    u[45] = tmp3;
    u[46] = tmp4;

    // stage 5
    u[7] = half_btf_0_neon_r(&cospi[8], &u[4], &v_bit, &rnding);
    u[4] = half_btf_0_neon_r(&cospi[56], &u[4], &v_bit, &rnding);

    u[9] = u[8];
    u[10] = u[11];
    u[13] = u[12];
    u[14] = u[15];

    tmp1 = half_btf_neon_mode10_r(&cospi[8], &u[17], &cospi[56], &u[30], &v_bit,
                                  &rnding);
    tmp2 = half_btf_neon_mode11_r(&cospi[56], &u[18], &cospi[8], &u[29], &v_bit,
                                  &rnding);
    tmp3 = half_btf_neon_mode10_r(&cospi[40], &u[21], &cospi[24], &u[26],
                                  &v_bit, &rnding);
    tmp4 = half_btf_neon_mode11_r(&cospi[24], &u[22], &cospi[40], &u[25],
                                  &v_bit, &rnding);
    u[25] = half_btf_neon_mode10_r(&cospi[40], &u[22], &cospi[24], &u[25],
                                   &v_bit, &rnding);
    u[26] = half_btf_neon_r(&cospi[24], &u[21], &cospi[40], &u[26], &v_bit,
                            &rnding);
    u[29] = half_btf_neon_mode10_r(&cospi[8], &u[18], &cospi[56], &u[29],
                                   &v_bit, &rnding);
    u[30] =
        half_btf_neon_r(&cospi[56], &u[17], &cospi[8], &u[30], &v_bit, &rnding);
    u[17] = tmp1;
    u[18] = tmp2;
    u[21] = tmp3;
    u[22] = tmp4;

    for (i = 32; i < 64; i += 8) {
      addsub_neon(u[i + 0], u[i + 3], &u[i + 0], &u[i + 3], &clamp_lo,
                  &clamp_hi);
      addsub_neon(u[i + 1], u[i + 2], &u[i + 1], &u[i + 2], &clamp_lo,
                  &clamp_hi);

      addsub_neon(u[i + 7], u[i + 4], &u[i + 7], &u[i + 4], &clamp_lo,
                  &clamp_hi);
      addsub_neon(u[i + 6], u[i + 5], &u[i + 6], &u[i + 5], &clamp_lo,
                  &clamp_hi);
    }

    // stage 6
    tmp1 = half_btf_0_neon_r(&cospi[32], &u[0], &v_bit, &rnding);
    u[1] = half_btf_0_neon_r(&cospi[32], &u[0], &v_bit, &rnding);
    u[0] = tmp1;
    u[5] = u[4];
    u[6] = u[7];

    tmp1 = half_btf_neon_mode10_r(&cospi[16], &u[9], &cospi[48], &u[14], &v_bit,
                                  &rnding);
    u[14] =
        half_btf_neon_r(&cospi[48], &u[9], &cospi[16], &u[14], &v_bit, &rnding);
    u[9] = tmp1;
    tmp2 = half_btf_neon_mode01_r(&cospi[48], &u[10], &cospi[16], &u[13],
                                  &v_bit, &rnding);
    u[13] = half_btf_neon_mode10_r(&cospi[16], &u[10], &cospi[48], &u[13],
                                   &v_bit, &rnding);
    u[10] = tmp2;

    for (i = 16; i < 32; i += 8) {
      addsub_neon(u[i + 0], u[i + 3], &u[i + 0], &u[i + 3], &clamp_lo,
                  &clamp_hi);
      addsub_neon(u[i + 1], u[i + 2], &u[i + 1], &u[i + 2], &clamp_lo,
                  &clamp_hi);

      addsub_neon(u[i + 7], u[i + 4], &u[i + 7], &u[i + 4], &clamp_lo,
                  &clamp_hi);
      addsub_neon(u[i + 6], u[i + 5], &u[i + 6], &u[i + 5], &clamp_lo,
                  &clamp_hi);
    }

    tmp1 = half_btf_neon_mode10_r(&cospi[8], &u[34], &cospi[56], &u[61], &v_bit,
                                  &rnding);
    tmp2 = half_btf_neon_mode10_r(&cospi[8], &u[35], &cospi[56], &u[60], &v_bit,
                                  &rnding);
    tmp3 = half_btf_neon_mode11_r(&cospi[56], &u[36], &cospi[8], &u[59], &v_bit,
                                  &rnding);
    tmp4 = half_btf_neon_mode11_r(&cospi[56], &u[37], &cospi[8], &u[58], &v_bit,
                                  &rnding);
    u[58] = half_btf_neon_mode10_r(&cospi[8], &u[37], &cospi[56], &u[58],
                                   &v_bit, &rnding);
    u[59] = half_btf_neon_mode10_r(&cospi[8], &u[36], &cospi[56], &u[59],
                                   &v_bit, &rnding);
    u[60] =
        half_btf_neon_r(&cospi[56], &u[35], &cospi[8], &u[60], &v_bit, &rnding);
    u[61] =
        half_btf_neon_r(&cospi[56], &u[34], &cospi[8], &u[61], &v_bit, &rnding);
    u[34] = tmp1;
    u[35] = tmp2;
    u[36] = tmp3;
    u[37] = tmp4;

    tmp1 = half_btf_neon_mode10_r(&cospi[40], &u[42], &cospi[24], &u[53],
                                  &v_bit, &rnding);
    tmp2 = half_btf_neon_mode10_r(&cospi[40], &u[43], &cospi[24], &u[52],
                                  &v_bit, &rnding);
    tmp3 = half_btf_neon_mode11_r(&cospi[24], &u[44], &cospi[40], &u[51],
                                  &v_bit, &rnding);
    tmp4 = half_btf_neon_mode11_r(&cospi[24], &u[45], &cospi[40], &u[50],
                                  &v_bit, &rnding);
    u[50] = half_btf_neon_mode10_r(&cospi[40], &u[45], &cospi[24], &u[50],
                                   &v_bit, &rnding);
    u[51] = half_btf_neon_mode10_r(&cospi[40], &u[44], &cospi[24], &u[51],
                                   &v_bit, &rnding);
    u[52] = half_btf_neon_r(&cospi[24], &u[43], &cospi[40], &u[52], &v_bit,
                            &rnding);
    u[53] = half_btf_neon_r(&cospi[24], &u[42], &cospi[40], &u[53], &v_bit,
                            &rnding);
    u[42] = tmp1;
    u[43] = tmp2;
    u[44] = tmp3;
    u[45] = tmp4;

    // stage 7
    u[3] = u[0];
    u[2] = u[1];
    tmp1 = half_btf_neon_mode10_r(&cospi[32], &u[5], &cospi[32], &u[6], &v_bit,
                                  &rnding);
    u[6] =
        half_btf_neon_r(&cospi[32], &u[5], &cospi[32], &u[6], &v_bit, &rnding);
    u[5] = tmp1;
    addsub_neon(u[8], u[11], &u[8], &u[11], &clamp_lo, &clamp_hi);
    addsub_neon(u[9], u[10], &u[9], &u[10], &clamp_lo, &clamp_hi);
    addsub_neon(u[15], u[12], &u[15], &u[12], &clamp_lo, &clamp_hi);
    addsub_neon(u[14], u[13], &u[14], &u[13], &clamp_lo, &clamp_hi);

    tmp1 = half_btf_neon_mode10_r(&cospi[16], &u[18], &cospi[48], &u[29],
                                  &v_bit, &rnding);
    tmp2 = half_btf_neon_mode10_r(&cospi[16], &u[19], &cospi[48], &u[28],
                                  &v_bit, &rnding);
    tmp3 = half_btf_neon_mode11_r(&cospi[48], &u[20], &cospi[16], &u[27],
                                  &v_bit, &rnding);
    tmp4 = half_btf_neon_mode11_r(&cospi[48], &u[21], &cospi[16], &u[26],
                                  &v_bit, &rnding);
    u[26] = half_btf_neon_mode10_r(&cospi[16], &u[21], &cospi[48], &u[26],
                                   &v_bit, &rnding);
    u[27] = half_btf_neon_mode10_r(&cospi[16], &u[20], &cospi[48], &u[27],
                                   &v_bit, &rnding);
    u[28] = half_btf_neon_r(&cospi[48], &u[19], &cospi[16], &u[28], &v_bit,
                            &rnding);
    u[29] = half_btf_neon_r(&cospi[48], &u[18], &cospi[16], &u[29], &v_bit,
                            &rnding);
    u[18] = tmp1;
    u[19] = tmp2;
    u[20] = tmp3;
    u[21] = tmp4;

    for (i = 32; i < 64; i += 16) {
      for (j = i; j < i + 4; j++) {
        addsub_neon(u[j], u[j ^ 7], &u[j], &u[j ^ 7], &clamp_lo, &clamp_hi);
        addsub_neon(u[j ^ 15], u[j ^ 8], &u[j ^ 15], &u[j ^ 8], &clamp_lo,
                    &clamp_hi);
      }
    }

    // stage 8
    for (i = 0; i < 4; ++i) {
      addsub_neon(u[i], u[7 - i], &u[i], &u[7 - i], &clamp_lo, &clamp_hi);
    }

    idct64_stage8_neon(u, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

    // stage 9
    idct64_stage9_neon(u, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

    // stage 10
    idct64_stage10_neon(u, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

    // stage 11
    idct64_stage11_neon(u, out, do_cols, bd, out_shift, &clamp_lo, &clamp_hi);
  }
}

static void idct64x64_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                           int bd, int out_shift) {
  int i, j;
  const int32_t *cospi = cospi_arr(bit);
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));

  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);

  {
    int32x4_t u[64], v[64];

    // stage 1
    u[32] = in[1];
    u[34] = in[17];
    u[36] = in[9];
    u[38] = in[25];
    u[40] = in[5];
    u[42] = in[21];
    u[44] = in[13];
    u[46] = in[29];
    u[48] = in[3];
    u[50] = in[19];
    u[52] = in[11];
    u[54] = in[27];
    u[56] = in[7];
    u[58] = in[23];
    u[60] = in[15];
    u[62] = in[31];

    v[16] = in[2];
    v[18] = in[18];
    v[20] = in[10];
    v[22] = in[26];
    v[24] = in[6];
    v[26] = in[22];
    v[28] = in[14];
    v[30] = in[30];

    u[8] = in[4];
    u[10] = in[20];
    u[12] = in[12];
    u[14] = in[28];

    v[4] = in[8];
    v[6] = in[24];

    u[0] = in[0];
    u[2] = in[16];

    // stage 2
    v[32] = half_btf_0_neon_r(&cospi[63], &u[32], &v_bit, &rnding);
    v[33] = half_btf_0_m_neon_r(&cospi[33], &u[62], &v_bit, &rnding);
    v[34] = half_btf_0_neon_r(&cospi[47], &u[34], &v_bit, &rnding);
    v[35] = half_btf_0_m_neon_r(&cospi[49], &u[60], &v_bit, &rnding);
    v[36] = half_btf_0_neon_r(&cospi[55], &u[36], &v_bit, &rnding);
    v[37] = half_btf_0_m_neon_r(&cospi[41], &u[58], &v_bit, &rnding);
    v[38] = half_btf_0_neon_r(&cospi[39], &u[38], &v_bit, &rnding);
    v[39] = half_btf_0_m_neon_r(&cospi[57], &u[56], &v_bit, &rnding);
    v[40] = half_btf_0_neon_r(&cospi[59], &u[40], &v_bit, &rnding);
    v[41] = half_btf_0_m_neon_r(&cospi[37], &u[54], &v_bit, &rnding);
    v[42] = half_btf_0_neon_r(&cospi[43], &u[42], &v_bit, &rnding);
    v[43] = half_btf_0_m_neon_r(&cospi[53], &u[52], &v_bit, &rnding);
    v[44] = half_btf_0_neon_r(&cospi[51], &u[44], &v_bit, &rnding);
    v[45] = half_btf_0_m_neon_r(&cospi[45], &u[50], &v_bit, &rnding);
    v[46] = half_btf_0_neon_r(&cospi[35], &u[46], &v_bit, &rnding);
    v[47] = half_btf_0_m_neon_r(&cospi[61], &u[48], &v_bit, &rnding);
    v[48] = half_btf_0_neon_r(&cospi[3], &u[48], &v_bit, &rnding);
    v[49] = half_btf_0_neon_r(&cospi[29], &u[46], &v_bit, &rnding);
    v[50] = half_btf_0_neon_r(&cospi[19], &u[50], &v_bit, &rnding);
    v[51] = half_btf_0_neon_r(&cospi[13], &u[44], &v_bit, &rnding);
    v[52] = half_btf_0_neon_r(&cospi[11], &u[52], &v_bit, &rnding);
    v[53] = half_btf_0_neon_r(&cospi[21], &u[42], &v_bit, &rnding);
    v[54] = half_btf_0_neon_r(&cospi[27], &u[54], &v_bit, &rnding);
    v[55] = half_btf_0_neon_r(&cospi[5], &u[40], &v_bit, &rnding);
    v[56] = half_btf_0_neon_r(&cospi[7], &u[56], &v_bit, &rnding);
    v[57] = half_btf_0_neon_r(&cospi[25], &u[38], &v_bit, &rnding);
    v[58] = half_btf_0_neon_r(&cospi[23], &u[58], &v_bit, &rnding);
    v[59] = half_btf_0_neon_r(&cospi[9], &u[36], &v_bit, &rnding);
    v[60] = half_btf_0_neon_r(&cospi[15], &u[60], &v_bit, &rnding);
    v[61] = half_btf_0_neon_r(&cospi[17], &u[34], &v_bit, &rnding);
    v[62] = half_btf_0_neon_r(&cospi[31], &u[62], &v_bit, &rnding);
    v[63] = half_btf_0_neon_r(&cospi[1], &u[32], &v_bit, &rnding);

    // stage 3
    u[16] = half_btf_0_neon_r(&cospi[62], &v[16], &v_bit, &rnding);
    u[17] = half_btf_0_m_neon_r(&cospi[34], &v[30], &v_bit, &rnding);
    u[18] = half_btf_0_neon_r(&cospi[46], &v[18], &v_bit, &rnding);
    u[19] = half_btf_0_m_neon_r(&cospi[50], &v[28], &v_bit, &rnding);
    u[20] = half_btf_0_neon_r(&cospi[54], &v[20], &v_bit, &rnding);
    u[21] = half_btf_0_m_neon_r(&cospi[42], &v[26], &v_bit, &rnding);
    u[22] = half_btf_0_neon_r(&cospi[38], &v[22], &v_bit, &rnding);
    u[23] = half_btf_0_m_neon_r(&cospi[58], &v[24], &v_bit, &rnding);
    u[24] = half_btf_0_neon_r(&cospi[6], &v[24], &v_bit, &rnding);
    u[25] = half_btf_0_neon_r(&cospi[26], &v[22], &v_bit, &rnding);
    u[26] = half_btf_0_neon_r(&cospi[22], &v[26], &v_bit, &rnding);
    u[27] = half_btf_0_neon_r(&cospi[10], &v[20], &v_bit, &rnding);
    u[28] = half_btf_0_neon_r(&cospi[14], &v[28], &v_bit, &rnding);
    u[29] = half_btf_0_neon_r(&cospi[18], &v[18], &v_bit, &rnding);
    u[30] = half_btf_0_neon_r(&cospi[30], &v[30], &v_bit, &rnding);
    u[31] = half_btf_0_neon_r(&cospi[2], &v[16], &v_bit, &rnding);

    for (i = 32; i < 64; i += 4) {
      addsub_neon(v[i + 0], v[i + 1], &u[i + 0], &u[i + 1], &clamp_lo,
                  &clamp_hi);
      addsub_neon(v[i + 3], v[i + 2], &u[i + 3], &u[i + 2], &clamp_lo,
                  &clamp_hi);
    }

    // stage 4
    v[8] = half_btf_0_neon_r(&cospi[60], &u[8], &v_bit, &rnding);
    v[9] = half_btf_0_m_neon_r(&cospi[36], &u[14], &v_bit, &rnding);
    v[10] = half_btf_0_neon_r(&cospi[44], &u[10], &v_bit, &rnding);
    v[11] = half_btf_0_m_neon_r(&cospi[52], &u[12], &v_bit, &rnding);
    v[12] = half_btf_0_neon_r(&cospi[12], &u[12], &v_bit, &rnding);
    v[13] = half_btf_0_neon_r(&cospi[20], &u[10], &v_bit, &rnding);
    v[14] = half_btf_0_neon_r(&cospi[28], &u[14], &v_bit, &rnding);
    v[15] = half_btf_0_neon_r(&cospi[4], &u[8], &v_bit, &rnding);

    for (i = 16; i < 32; i += 4) {
      addsub_neon(u[i + 0], u[i + 1], &v[i + 0], &v[i + 1], &clamp_lo,
                  &clamp_hi);
      addsub_neon(u[i + 3], u[i + 2], &v[i + 3], &v[i + 2], &clamp_lo,
                  &clamp_hi);
    }

    for (i = 32; i < 64; i += 4) {
      v[i + 0] = u[i + 0];
      v[i + 3] = u[i + 3];
    }

    v[33] = half_btf_neon_mode10_r(&cospi[4], &u[33], &cospi[60], &u[62],
                                   &v_bit, &rnding);
    v[34] = half_btf_neon_mode11_r(&cospi[60], &u[34], &cospi[4], &u[61],
                                   &v_bit, &rnding);
    v[37] = half_btf_neon_mode10_r(&cospi[36], &u[37], &cospi[28], &u[58],
                                   &v_bit, &rnding);
    v[38] = half_btf_neon_mode11_r(&cospi[28], &u[38], &cospi[36], &u[57],
                                   &v_bit, &rnding);
    v[41] = half_btf_neon_mode10_r(&cospi[20], &u[41], &cospi[44], &u[54],
                                   &v_bit, &rnding);
    v[42] = half_btf_neon_mode11_r(&cospi[44], &u[42], &cospi[20], &u[53],
                                   &v_bit, &rnding);
    v[45] = half_btf_neon_mode10_r(&cospi[52], &u[45], &cospi[12], &u[50],
                                   &v_bit, &rnding);
    v[46] = half_btf_neon_mode11_r(&cospi[12], &u[46], &cospi[52], &u[49],
                                   &v_bit, &rnding);
    v[49] = half_btf_neon_mode10_r(&cospi[52], &u[46], &cospi[12], &u[49],
                                   &v_bit, &rnding);
    v[50] = half_btf_neon_r(&cospi[12], &u[45], &cospi[52], &u[50], &v_bit,
                            &rnding);
    v[53] = half_btf_neon_mode10_r(&cospi[20], &u[42], &cospi[44], &u[53],
                                   &v_bit, &rnding);
    v[54] = half_btf_neon_r(&cospi[44], &u[41], &cospi[20], &u[54], &v_bit,
                            &rnding);
    v[57] = half_btf_neon_mode10_r(&cospi[36], &u[38], &cospi[28], &u[57],
                                   &v_bit, &rnding);
    v[58] = half_btf_neon_r(&cospi[28], &u[37], &cospi[36], &u[58], &v_bit,
                            &rnding);
    v[61] = half_btf_neon_mode10_r(&cospi[4], &u[34], &cospi[60], &u[61],
                                   &v_bit, &rnding);
    v[62] =
        half_btf_neon_r(&cospi[60], &u[33], &cospi[4], &u[62], &v_bit, &rnding);

    // stage 5
    u[4] = half_btf_0_neon_r(&cospi[56], &v[4], &v_bit, &rnding);
    u[5] = half_btf_0_m_neon_r(&cospi[40], &v[6], &v_bit, &rnding);
    u[6] = half_btf_0_neon_r(&cospi[24], &v[6], &v_bit, &rnding);
    u[7] = half_btf_0_neon_r(&cospi[8], &v[4], &v_bit, &rnding);

    for (i = 8; i < 16; i += 4) {
      addsub_neon(v[i + 0], v[i + 1], &u[i + 0], &u[i + 1], &clamp_lo,
                  &clamp_hi);
      addsub_neon(v[i + 3], v[i + 2], &u[i + 3], &u[i + 2], &clamp_lo,
                  &clamp_hi);
    }

    for (i = 16; i < 32; i += 4) {
      u[i + 0] = v[i + 0];
      u[i + 3] = v[i + 3];
    }

    u[17] = half_btf_neon_mode10_r(&cospi[8], &v[17], &cospi[56], &v[30],
                                   &v_bit, &rnding);
    u[18] = half_btf_neon_mode11_r(&cospi[56], &v[18], &cospi[8], &v[29],
                                   &v_bit, &rnding);
    u[21] = half_btf_neon_mode10_r(&cospi[40], &v[21], &cospi[24], &v[26],
                                   &v_bit, &rnding);
    u[22] = half_btf_neon_mode11_r(&cospi[24], &v[22], &cospi[40], &v[25],
                                   &v_bit, &rnding);
    u[25] = half_btf_neon_mode10_r(&cospi[40], &v[22], &cospi[24], &v[25],
                                   &v_bit, &rnding);
    u[26] = half_btf_neon_r(&cospi[24], &v[21], &cospi[40], &v[26], &v_bit,
                            &rnding);
    u[29] = half_btf_neon_mode10_r(&cospi[8], &v[18], &cospi[56], &v[29],
                                   &v_bit, &rnding);
    u[30] =
        half_btf_neon_r(&cospi[56], &v[17], &cospi[8], &v[30], &v_bit, &rnding);

    for (i = 32; i < 64; i += 8) {
      addsub_neon(v[i + 0], v[i + 3], &u[i + 0], &u[i + 3], &clamp_lo,
                  &clamp_hi);
      addsub_neon(v[i + 1], v[i + 2], &u[i + 1], &u[i + 2], &clamp_lo,
                  &clamp_hi);

      addsub_neon(v[i + 7], v[i + 4], &u[i + 7], &u[i + 4], &clamp_lo,
                  &clamp_hi);
      addsub_neon(v[i + 6], v[i + 5], &u[i + 6], &u[i + 5], &clamp_lo,
                  &clamp_hi);
    }

    // stage 6
    v[0] = half_btf_0_neon_r(&cospi[32], &u[0], &v_bit, &rnding);
    v[1] = half_btf_0_neon_r(&cospi[32], &u[0], &v_bit, &rnding);
    v[2] = half_btf_0_neon_r(&cospi[48], &u[2], &v_bit, &rnding);
    v[3] = half_btf_0_neon_r(&cospi[16], &u[2], &v_bit, &rnding);

    addsub_neon(u[4], u[5], &v[4], &v[5], &clamp_lo, &clamp_hi);
    addsub_neon(u[7], u[6], &v[7], &v[6], &clamp_lo, &clamp_hi);

    for (i = 8; i < 16; i += 4) {
      v[i + 0] = u[i + 0];
      v[i + 3] = u[i + 3];
    }

    v[9] = half_btf_neon_mode10_r(&cospi[16], &u[9], &cospi[48], &u[14], &v_bit,
                                  &rnding);
    v[10] = half_btf_neon_mode11_r(&cospi[48], &u[10], &cospi[16], &u[13],
                                   &v_bit, &rnding);
    v[13] = half_btf_neon_mode10_r(&cospi[16], &u[10], &cospi[48], &u[13],
                                   &v_bit, &rnding);
    v[14] =
        half_btf_neon_r(&cospi[48], &u[9], &cospi[16], &u[14], &v_bit, &rnding);

    for (i = 16; i < 32; i += 8) {
      addsub_neon(u[i + 0], u[i + 3], &v[i + 0], &v[i + 3], &clamp_lo,
                  &clamp_hi);
      addsub_neon(u[i + 1], u[i + 2], &v[i + 1], &v[i + 2], &clamp_lo,
                  &clamp_hi);

      addsub_neon(u[i + 7], u[i + 4], &v[i + 7], &v[i + 4], &clamp_lo,
                  &clamp_hi);
      addsub_neon(u[i + 6], u[i + 5], &v[i + 6], &v[i + 5], &clamp_lo,
                  &clamp_hi);
    }

    for (i = 32; i < 64; i += 8) {
      v[i + 0] = u[i + 0];
      v[i + 1] = u[i + 1];
      v[i + 6] = u[i + 6];
      v[i + 7] = u[i + 7];
    }

    v[34] = half_btf_neon_mode10_r(&cospi[8], &u[34], &cospi[56], &u[61],
                                   &v_bit, &rnding);
    v[35] = half_btf_neon_mode10_r(&cospi[8], &u[35], &cospi[56], &u[60],
                                   &v_bit, &rnding);
    v[36] = half_btf_neon_mode11_r(&cospi[56], &u[36], &cospi[8], &u[59],
                                   &v_bit, &rnding);
    v[37] = half_btf_neon_mode11_r(&cospi[56], &u[37], &cospi[8], &u[58],
                                   &v_bit, &rnding);
    v[42] = half_btf_neon_mode10_r(&cospi[40], &u[42], &cospi[24], &u[53],
                                   &v_bit, &rnding);
    v[43] = half_btf_neon_mode10_r(&cospi[40], &u[43], &cospi[24], &u[52],
                                   &v_bit, &rnding);
    v[44] = half_btf_neon_mode11_r(&cospi[24], &u[44], &cospi[40], &u[51],
                                   &v_bit, &rnding);
    v[45] = half_btf_neon_mode11_r(&cospi[24], &u[45], &cospi[40], &u[50],
                                   &v_bit, &rnding);
    v[50] = half_btf_neon_mode10_r(&cospi[40], &u[45], &cospi[24], &u[50],
                                   &v_bit, &rnding);
    v[51] = half_btf_neon_mode10_r(&cospi[40], &u[44], &cospi[24], &u[51],
                                   &v_bit, &rnding);
    v[52] = half_btf_neon_r(&cospi[24], &u[43], &cospi[40], &u[52], &v_bit,
                            &rnding);
    v[53] = half_btf_neon_r(&cospi[24], &u[42], &cospi[40], &u[53], &v_bit,
                            &rnding);
    v[58] = half_btf_neon_mode10_r(&cospi[8], &u[37], &cospi[56], &u[58],
                                   &v_bit, &rnding);
    v[59] = half_btf_neon_mode10_r(&cospi[8], &u[36], &cospi[56], &u[59],
                                   &v_bit, &rnding);
    v[60] =
        half_btf_neon_r(&cospi[56], &u[35], &cospi[8], &u[60], &v_bit, &rnding);
    v[61] =
        half_btf_neon_r(&cospi[56], &u[34], &cospi[8], &u[61], &v_bit, &rnding);

    // stage 7
    addsub_neon(v[0], v[3], &u[0], &u[3], &clamp_lo, &clamp_hi);
    addsub_neon(v[1], v[2], &u[1], &u[2], &clamp_lo, &clamp_hi);

    u[4] = v[4];
    u[7] = v[7];
    u[5] = half_btf_neon_mode10_r(&cospi[32], &v[5], &cospi[32], &v[6], &v_bit,
                                  &rnding);
    u[6] =
        half_btf_neon_r(&cospi[32], &v[5], &cospi[32], &v[6], &v_bit, &rnding);

    addsub_neon(v[8], v[11], &u[8], &u[11], &clamp_lo, &clamp_hi);
    addsub_neon(v[9], v[10], &u[9], &u[10], &clamp_lo, &clamp_hi);
    addsub_neon(v[15], v[12], &u[15], &u[12], &clamp_lo, &clamp_hi);
    addsub_neon(v[14], v[13], &u[14], &u[13], &clamp_lo, &clamp_hi);

    for (i = 16; i < 32; i += 8) {
      u[i + 0] = v[i + 0];
      u[i + 1] = v[i + 1];
      u[i + 6] = v[i + 6];
      u[i + 7] = v[i + 7];
    }

    u[18] = half_btf_neon_mode10_r(&cospi[16], &v[18], &cospi[48], &v[29],
                                   &v_bit, &rnding);
    u[19] = half_btf_neon_mode10_r(&cospi[16], &v[19], &cospi[48], &v[28],
                                   &v_bit, &rnding);
    u[20] = half_btf_neon_mode11_r(&cospi[48], &v[20], &cospi[16], &v[27],
                                   &v_bit, &rnding);
    u[21] = half_btf_neon_mode11_r(&cospi[48], &v[21], &cospi[16], &v[26],
                                   &v_bit, &rnding);
    u[26] = half_btf_neon_mode10_r(&cospi[16], &v[21], &cospi[48], &v[26],
                                   &v_bit, &rnding);
    u[27] = half_btf_neon_mode10_r(&cospi[16], &v[20], &cospi[48], &v[27],
                                   &v_bit, &rnding);
    u[28] = half_btf_neon_r(&cospi[48], &v[19], &cospi[16], &v[28], &v_bit,
                            &rnding);
    u[29] = half_btf_neon_r(&cospi[48], &v[18], &cospi[16], &v[29], &v_bit,
                            &rnding);

    for (i = 32; i < 64; i += 16) {
      for (j = i; j < i + 4; j++) {
        addsub_neon(v[j], v[j ^ 7], &u[j], &u[j ^ 7], &clamp_lo, &clamp_hi);
        addsub_neon(v[j ^ 15], v[j ^ 8], &u[j ^ 15], &u[j ^ 8], &clamp_lo,
                    &clamp_hi);
      }
    }

    // stage 8
    for (i = 0; i < 4; ++i) {
      addsub_neon(u[i], u[7 - i], &v[i], &v[7 - i], &clamp_lo, &clamp_hi);
    }

    v[8] = u[8];
    v[9] = u[9];
    v[14] = u[14];
    v[15] = u[15];

    v[10] = half_btf_neon_mode10_r(&cospi[32], &u[10], &cospi[32], &u[13],
                                   &v_bit, &rnding);
    v[11] = half_btf_neon_mode10_r(&cospi[32], &u[11], &cospi[32], &u[12],
                                   &v_bit, &rnding);
    v[12] = half_btf_neon_r(&cospi[32], &u[11], &cospi[32], &u[12], &v_bit,
                            &rnding);
    v[13] = half_btf_neon_r(&cospi[32], &u[10], &cospi[32], &u[13], &v_bit,
                            &rnding);

    for (i = 16; i < 20; ++i) {
      addsub_neon(u[i], u[i ^ 7], &v[i], &v[i ^ 7], &clamp_lo, &clamp_hi);
      addsub_neon(u[i ^ 15], u[i ^ 8], &v[i ^ 15], &v[i ^ 8], &clamp_lo,
                  &clamp_hi);
    }

    for (i = 32; i < 36; ++i) {
      v[i] = u[i];
      v[i + 12] = u[i + 12];
      v[i + 16] = u[i + 16];
      v[i + 28] = u[i + 28];
    }

    v[36] = half_btf_neon_mode10_r(&cospi[16], &u[36], &cospi[48], &u[59],
                                   &v_bit, &rnding);
    v[37] = half_btf_neon_mode10_r(&cospi[16], &u[37], &cospi[48], &u[58],
                                   &v_bit, &rnding);
    v[38] = half_btf_neon_mode10_r(&cospi[16], &u[38], &cospi[48], &u[57],
                                   &v_bit, &rnding);
    v[39] = half_btf_neon_mode10_r(&cospi[16], &u[39], &cospi[48], &u[56],
                                   &v_bit, &rnding);
    v[40] = half_btf_neon_mode11_r(&cospi[48], &u[40], &cospi[16], &u[55],
                                   &v_bit, &rnding);
    v[41] = half_btf_neon_mode11_r(&cospi[48], &u[41], &cospi[16], &u[54],
                                   &v_bit, &rnding);
    v[42] = half_btf_neon_mode11_r(&cospi[48], &u[42], &cospi[16], &u[53],
                                   &v_bit, &rnding);
    v[43] = half_btf_neon_mode11_r(&cospi[48], &u[43], &cospi[16], &u[52],
                                   &v_bit, &rnding);
    v[52] = half_btf_neon_mode10_r(&cospi[16], &u[43], &cospi[48], &u[52],
                                   &v_bit, &rnding);
    v[53] = half_btf_neon_mode10_r(&cospi[16], &u[42], &cospi[48], &u[53],
                                   &v_bit, &rnding);
    v[54] = half_btf_neon_mode10_r(&cospi[16], &u[41], &cospi[48], &u[54],
                                   &v_bit, &rnding);
    v[55] = half_btf_neon_mode10_r(&cospi[16], &u[40], &cospi[48], &u[55],
                                   &v_bit, &rnding);
    v[56] = half_btf_neon_r(&cospi[48], &u[39], &cospi[16], &u[56], &v_bit,
                            &rnding);
    v[57] = half_btf_neon_r(&cospi[48], &u[38], &cospi[16], &u[57], &v_bit,
                            &rnding);
    v[58] = half_btf_neon_r(&cospi[48], &u[37], &cospi[16], &u[58], &v_bit,
                            &rnding);
    v[59] = half_btf_neon_r(&cospi[48], &u[36], &cospi[16], &u[59], &v_bit,
                            &rnding);

    // stage 9
    for (i = 0; i < 8; ++i) {
      addsub_neon(v[i], v[15 - i], &u[i], &u[15 - i], &clamp_lo, &clamp_hi);
    }

    for (i = 16; i < 20; ++i) {
      u[i] = v[i];
      u[i + 12] = v[i + 12];
    }

    u[20] = half_btf_neon_mode10_r(&cospi[32], &v[20], &cospi[32], &v[27],
                                   &v_bit, &rnding);
    u[21] = half_btf_neon_mode10_r(&cospi[32], &v[21], &cospi[32], &v[26],
                                   &v_bit, &rnding);
    u[22] = half_btf_neon_mode10_r(&cospi[32], &v[22], &cospi[32], &v[25],
                                   &v_bit, &rnding);
    u[23] = half_btf_neon_mode10_r(&cospi[32], &v[23], &cospi[32], &v[24],
                                   &v_bit, &rnding);
    u[24] = half_btf_neon_r(&cospi[32], &v[23], &cospi[32], &v[24], &v_bit,
                            &rnding);
    u[25] = half_btf_neon_r(&cospi[32], &v[22], &cospi[32], &v[25], &v_bit,
                            &rnding);
    u[26] = half_btf_neon_r(&cospi[32], &v[21], &cospi[32], &v[26], &v_bit,
                            &rnding);
    u[27] = half_btf_neon_r(&cospi[32], &v[20], &cospi[32], &v[27], &v_bit,
                            &rnding);

    for (i = 32; i < 40; i++) {
      addsub_neon(v[i], v[i ^ 15], &u[i], &u[i ^ 15], &clamp_lo, &clamp_hi);
    }

    for (i = 48; i < 56; i++) {
      addsub_neon(v[i ^ 15], v[i], &u[i ^ 15], &u[i], &clamp_lo, &clamp_hi);
    }

    // stage 10
    for (i = 0; i < 16; i++) {
      addsub_neon(u[i], u[31 - i], &v[i], &v[31 - i], &clamp_lo, &clamp_hi);
    }

    for (i = 32; i < 40; i++) v[i] = u[i];

    v[40] = half_btf_neon_mode10_r(&cospi[32], &u[40], &cospi[32], &u[55],
                                   &v_bit, &rnding);
    v[41] = half_btf_neon_mode10_r(&cospi[32], &u[41], &cospi[32], &u[54],
                                   &v_bit, &rnding);
    v[42] = half_btf_neon_mode10_r(&cospi[32], &u[42], &cospi[32], &u[53],
                                   &v_bit, &rnding);
    v[43] = half_btf_neon_mode10_r(&cospi[32], &u[43], &cospi[32], &u[52],
                                   &v_bit, &rnding);
    v[44] = half_btf_neon_mode10_r(&cospi[32], &u[44], &cospi[32], &u[51],
                                   &v_bit, &rnding);
    v[45] = half_btf_neon_mode10_r(&cospi[32], &u[45], &cospi[32], &u[50],
                                   &v_bit, &rnding);
    v[46] = half_btf_neon_mode10_r(&cospi[32], &u[46], &cospi[32], &u[49],
                                   &v_bit, &rnding);
    v[47] = half_btf_neon_mode10_r(&cospi[32], &u[47], &cospi[32], &u[48],
                                   &v_bit, &rnding);
    v[48] = half_btf_neon_r(&cospi[32], &u[47], &cospi[32], &u[48], &v_bit,
                            &rnding);
    v[49] = half_btf_neon_r(&cospi[32], &u[46], &cospi[32], &u[49], &v_bit,
                            &rnding);
    v[50] = half_btf_neon_r(&cospi[32], &u[45], &cospi[32], &u[50], &v_bit,
                            &rnding);
    v[51] = half_btf_neon_r(&cospi[32], &u[44], &cospi[32], &u[51], &v_bit,
                            &rnding);
    v[52] = half_btf_neon_r(&cospi[32], &u[43], &cospi[32], &u[52], &v_bit,
                            &rnding);
    v[53] = half_btf_neon_r(&cospi[32], &u[42], &cospi[32], &u[53], &v_bit,
                            &rnding);
    v[54] = half_btf_neon_r(&cospi[32], &u[41], &cospi[32], &u[54], &v_bit,
                            &rnding);
    v[55] = half_btf_neon_r(&cospi[32], &u[40], &cospi[32], &u[55], &v_bit,
                            &rnding);

    for (i = 56; i < 64; i++) v[i] = u[i];

    // stage 11
    for (i = 0; i < 32; i++) {
      addsub_neon(v[i], v[63 - i], &out[(i)], &out[(63 - i)], &clamp_lo,
                  &clamp_hi);
    }

    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
      const int32x4_t clamp_hi_out =
          vdupq_n_s32((1 << (log_range_out - 1)) - 1);
      for (i = 0; i < 64; i += 4) {
        round_shift_4x4(out + i, out_shift);
        highbd_clamp_s32_neon(out + i, out + i, &clamp_lo_out, &clamp_hi_out,
                              4);
      }
    }
  }
}

static void idct32x32_low1_neon(int32x4_t *in, int32x4_t *out, int bit,
                                int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t bf1;
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0-1
  bf1 = in[0];

  // stage 2-5
  bf1 = half_btf_0_neon_r(&cospi[32], &bf1, &v_bit, &rnding);

  // stage 6-9
  if (do_cols) {
    bf1 = vmaxq_s32(bf1, clamp_lo);
    bf1 = vminq_s32(bf1, clamp_hi);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    clamp_lo = vdupq_n_s32(-(1 << (log_range_out - 1)));
    clamp_hi = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    if (out_shift != 0) {
      bf1 = vrshlq_s32(bf1, vdupq_n_s32(-out_shift));
    }
  }

  bf1 = vmaxq_s32(bf1, clamp_lo);
  bf1 = vminq_s32(bf1, clamp_hi);

  for (int i = 0; i < 32; i++) out[i] = bf1;
}

static void idct32x32_low8_neon(int32x4_t *in, int32x4_t *out, int bit,
                                int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t bf1[32];
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));

  // stage 0-1
  bf1[0] = in[0];
  bf1[4] = in[4];
  bf1[8] = in[2];
  bf1[12] = in[6];
  bf1[16] = in[1];
  bf1[20] = in[5];
  bf1[24] = in[3];
  bf1[28] = in[7];

  // stage 2
  bf1[31] = half_btf_0_neon_r(&cospi[2], &bf1[16], &v_bit, &rnding);
  bf1[16] = half_btf_0_neon_r(&cospi[62], &bf1[16], &v_bit, &rnding);
  bf1[19] = half_btf_0_m_neon_r(&cospi[50], &bf1[28], &v_bit, &rnding);
  bf1[28] = half_btf_0_neon_r(&cospi[14], &bf1[28], &v_bit, &rnding);
  bf1[27] = half_btf_0_neon_r(&cospi[10], &bf1[20], &v_bit, &rnding);
  bf1[20] = half_btf_0_neon_r(&cospi[54], &bf1[20], &v_bit, &rnding);
  bf1[23] = half_btf_0_m_neon_r(&cospi[58], &bf1[24], &v_bit, &rnding);
  bf1[24] = half_btf_0_neon_r(&cospi[6], &bf1[24], &v_bit, &rnding);

  // stage 3
  bf1[15] = half_btf_0_neon_r(&cospi[4], &bf1[8], &v_bit, &rnding);
  bf1[8] = half_btf_0_neon_r(&cospi[60], &bf1[8], &v_bit, &rnding);

  bf1[11] = half_btf_0_m_neon_r(&cospi[52], &bf1[12], &v_bit, &rnding);
  bf1[12] = half_btf_0_neon_r(&cospi[12], &bf1[12], &v_bit, &rnding);
  bf1[17] = bf1[16];
  bf1[18] = bf1[19];
  bf1[21] = bf1[20];
  bf1[22] = bf1[23];
  bf1[25] = bf1[24];
  bf1[26] = bf1[27];
  bf1[29] = bf1[28];
  bf1[30] = bf1[31];

  // stage 4 :
  bf1[7] = half_btf_0_neon_r(&cospi[8], &bf1[4], &v_bit, &rnding);
  bf1[4] = half_btf_0_neon_r(&cospi[56], &bf1[4], &v_bit, &rnding);

  bf1[9] = bf1[8];
  bf1[10] = bf1[11];
  bf1[13] = bf1[12];
  bf1[14] = bf1[15];

  idct32_stage4_neon(bf1, cospi, &v_bit, &rnding);

  // stage 5
  bf1[0] = half_btf_0_neon_r(&cospi[32], &bf1[0], &v_bit, &rnding);
  bf1[1] = bf1[0];
  bf1[5] = bf1[4];
  bf1[6] = bf1[7];

  idct32_stage5_neon(bf1, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

  // stage 6
  bf1[3] = bf1[0];
  bf1[2] = bf1[1];

  idct32_stage6_neon(bf1, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

  // stage 7
  idct32_stage7_neon(bf1, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

  // stage 8
  idct32_stage8_neon(bf1, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

  // stage 9
  idct32_stage9_neon(bf1, out, do_cols, bd, out_shift, &clamp_lo, &clamp_hi);
}

static void idct32x32_low16_neon(int32x4_t *in, int32x4_t *out, int bit,
                                 int do_cols, int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t bf1[32];
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));

  // stage 0-1

  bf1[0] = in[0];
  bf1[2] = in[8];
  bf1[4] = in[4];
  bf1[6] = in[12];
  bf1[8] = in[2];
  bf1[10] = in[10];
  bf1[12] = in[6];
  bf1[14] = in[14];
  bf1[16] = in[1];
  bf1[18] = in[9];
  bf1[20] = in[5];
  bf1[22] = in[13];
  bf1[24] = in[3];
  bf1[26] = in[11];
  bf1[28] = in[7];
  bf1[30] = in[15];

  // stage 2
  bf1[31] = half_btf_0_neon_r(&cospi[2], &bf1[16], &v_bit, &rnding);
  bf1[16] = half_btf_0_neon_r(&cospi[62], &bf1[16], &v_bit, &rnding);
  bf1[17] = half_btf_0_m_neon_r(&cospi[34], &bf1[30], &v_bit, &rnding);
  bf1[30] = half_btf_0_neon_r(&cospi[30], &bf1[30], &v_bit, &rnding);
  bf1[29] = half_btf_0_neon_r(&cospi[18], &bf1[18], &v_bit, &rnding);
  bf1[18] = half_btf_0_neon_r(&cospi[46], &bf1[18], &v_bit, &rnding);
  bf1[19] = half_btf_0_m_neon_r(&cospi[50], &bf1[28], &v_bit, &rnding);
  bf1[28] = half_btf_0_neon_r(&cospi[14], &bf1[28], &v_bit, &rnding);
  bf1[27] = half_btf_0_neon_r(&cospi[10], &bf1[20], &v_bit, &rnding);
  bf1[20] = half_btf_0_neon_r(&cospi[54], &bf1[20], &v_bit, &rnding);
  bf1[21] = half_btf_0_m_neon_r(&cospi[42], &bf1[26], &v_bit, &rnding);
  bf1[26] = half_btf_0_neon_r(&cospi[22], &bf1[26], &v_bit, &rnding);
  bf1[25] = half_btf_0_neon_r(&cospi[26], &bf1[22], &v_bit, &rnding);
  bf1[22] = half_btf_0_neon_r(&cospi[38], &bf1[22], &v_bit, &rnding);
  bf1[23] = half_btf_0_m_neon_r(&cospi[58], &bf1[24], &v_bit, &rnding);
  bf1[24] = half_btf_0_neon_r(&cospi[6], &bf1[24], &v_bit, &rnding);

  // stage 3
  bf1[15] = half_btf_0_neon_r(&cospi[4], &bf1[8], &v_bit, &rnding);
  bf1[8] = half_btf_0_neon_r(&cospi[60], &bf1[8], &v_bit, &rnding);
  bf1[9] = half_btf_0_m_neon_r(&cospi[36], &bf1[14], &v_bit, &rnding);
  bf1[14] = half_btf_0_neon_r(&cospi[28], &bf1[14], &v_bit, &rnding);
  bf1[13] = half_btf_0_neon_r(&cospi[20], &bf1[10], &v_bit, &rnding);
  bf1[10] = half_btf_0_neon_r(&cospi[44], &bf1[10], &v_bit, &rnding);
  bf1[11] = half_btf_0_m_neon_r(&cospi[52], &bf1[12], &v_bit, &rnding);
  bf1[12] = half_btf_0_neon_r(&cospi[12], &bf1[12], &v_bit, &rnding);

  addsub_neon(bf1[16], bf1[17], bf1 + 16, bf1 + 17, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[19], bf1[18], bf1 + 19, bf1 + 18, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[20], bf1[21], bf1 + 20, bf1 + 21, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[23], bf1[22], bf1 + 23, bf1 + 22, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[24], bf1[25], bf1 + 24, bf1 + 25, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[27], bf1[26], bf1 + 27, bf1 + 26, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[28], bf1[29], bf1 + 28, bf1 + 29, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[31], bf1[30], bf1 + 31, bf1 + 30, &clamp_lo, &clamp_hi);
  // stage 4
  bf1[7] = half_btf_0_neon_r(&cospi[8], &bf1[4], &v_bit, &rnding);
  bf1[4] = half_btf_0_neon_r(&cospi[56], &bf1[4], &v_bit, &rnding);
  bf1[5] = half_btf_0_m_neon_r(&cospi[40], &bf1[6], &v_bit, &rnding);
  bf1[6] = half_btf_0_neon_r(&cospi[24], &bf1[6], &v_bit, &rnding);

  addsub_neon(bf1[8], bf1[9], bf1 + 8, bf1 + 9, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[11], bf1[10], bf1 + 11, bf1 + 10, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[12], bf1[13], bf1 + 12, bf1 + 13, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[15], bf1[14], bf1 + 15, bf1 + 14, &clamp_lo, &clamp_hi);

  idct32_stage4_neon(bf1, cospi, &v_bit, &rnding);

  // stage 5
  bf1[0] = half_btf_0_neon_r(&cospi[32], &bf1[0], &v_bit, &rnding);
  bf1[1] = bf1[0];
  bf1[3] = half_btf_0_neon_r(&cospi[16], &bf1[2], &v_bit, &rnding);
  bf1[2] = half_btf_0_neon_r(&cospi[48], &bf1[2], &v_bit, &rnding);

  addsub_neon(bf1[4], bf1[5], bf1 + 4, bf1 + 5, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[7], bf1[6], bf1 + 7, bf1 + 6, &clamp_lo, &clamp_hi);

  idct32_stage5_neon(bf1, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

  // stage 6
  addsub_neon(bf1[0], bf1[3], bf1 + 0, bf1 + 3, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[1], bf1[2], bf1 + 1, bf1 + 2, &clamp_lo, &clamp_hi);

  idct32_stage6_neon(bf1, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

  // stage 7
  idct32_stage7_neon(bf1, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);

  // stage 8
  idct32_stage8_neon(bf1, cospi, &clamp_lo, &clamp_hi, &v_bit, &rnding);
  // stage 9
  idct32_stage9_neon(bf1, out, do_cols, bd, out_shift, &clamp_lo, &clamp_hi);
}

static void idct32x32_neon(int32x4_t *in, int32x4_t *out, int bit, int do_cols,
                           int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const int32x4_t clamp_lo = vdupq_n_s32(-(1 << (log_range - 1)));
  const int32x4_t clamp_hi = vdupq_n_s32((1 << (log_range - 1)) - 1);
  int32x4_t bf1[32], bf0[32];
  const int32x4_t v_bit = vdupq_n_s32(-bit);
  const int32x4_t rnding = vdupq_n_s32(1 << (bit - 1));
  // stage 0
  // stage 1
  bf1[0] = in[0];
  bf1[1] = in[16];
  bf1[2] = in[8];
  bf1[3] = in[24];
  bf1[4] = in[4];
  bf1[5] = in[20];
  bf1[6] = in[12];
  bf1[7] = in[28];
  bf1[8] = in[2];
  bf1[9] = in[18];
  bf1[10] = in[10];
  bf1[11] = in[26];
  bf1[12] = in[6];
  bf1[13] = in[22];
  bf1[14] = in[14];
  bf1[15] = in[30];
  bf1[16] = in[1];
  bf1[17] = in[17];
  bf1[18] = in[9];
  bf1[19] = in[25];
  bf1[20] = in[5];
  bf1[21] = in[21];
  bf1[22] = in[13];
  bf1[23] = in[29];
  bf1[24] = in[3];
  bf1[25] = in[19];
  bf1[26] = in[11];
  bf1[27] = in[27];
  bf1[28] = in[7];
  bf1[29] = in[23];
  bf1[30] = in[15];
  bf1[31] = in[31];

  // stage 2
  for (int i = 0; i < 16; i++) bf0[i] = bf1[i];

  bf0[16] = half_btf_neon_mode01_r(&cospi[62], &bf1[16], &cospi[2], &bf1[31],
                                   &v_bit, &rnding);
  bf0[17] = half_btf_neon_mode01_r(&cospi[30], &bf1[17], &cospi[34], &bf1[30],
                                   &v_bit, &rnding);
  bf0[18] = half_btf_neon_mode01_r(&cospi[46], &bf1[18], &cospi[18], &bf1[29],
                                   &v_bit, &rnding);
  bf0[19] = half_btf_neon_mode01_r(&cospi[14], &bf1[19], &cospi[50], &bf1[28],
                                   &v_bit, &rnding);
  bf0[20] = half_btf_neon_mode01_r(&cospi[54], &bf1[20], &cospi[10], &bf1[27],
                                   &v_bit, &rnding);
  bf0[21] = half_btf_neon_mode01_r(&cospi[22], &bf1[21], &cospi[42], &bf1[26],
                                   &v_bit, &rnding);
  bf0[22] = half_btf_neon_mode01_r(&cospi[38], &bf1[22], &cospi[26], &bf1[25],
                                   &v_bit, &rnding);
  bf0[23] = half_btf_neon_mode01_r(&cospi[6], &bf1[23], &cospi[58], &bf1[24],
                                   &v_bit, &rnding);
  bf0[24] = half_btf_neon_r(&cospi[58], &bf1[23], &cospi[6], &bf1[24], &v_bit,
                            &rnding);
  bf0[25] = half_btf_neon_r(&cospi[26], &bf1[22], &cospi[38], &bf1[25], &v_bit,
                            &rnding);
  bf0[26] = half_btf_neon_r(&cospi[42], &bf1[21], &cospi[22], &bf1[26], &v_bit,
                            &rnding);
  bf0[27] = half_btf_neon_r(&cospi[10], &bf1[20], &cospi[54], &bf1[27], &v_bit,
                            &rnding);
  bf0[28] = half_btf_neon_r(&cospi[50], &bf1[19], &cospi[14], &bf1[28], &v_bit,
                            &rnding);
  bf0[29] = half_btf_neon_r(&cospi[18], &bf1[18], &cospi[46], &bf1[29], &v_bit,
                            &rnding);
  bf0[30] = half_btf_neon_r(&cospi[34], &bf1[17], &cospi[30], &bf1[30], &v_bit,
                            &rnding);
  bf0[31] = half_btf_neon_r(&cospi[2], &bf1[16], &cospi[62], &bf1[31], &v_bit,
                            &rnding);

  // stage 3
  for (int i = 0; i < 8; i++) bf1[i] = bf0[i];

  bf1[8] = half_btf_neon_mode01_r(&cospi[60], &bf0[8], &cospi[4], &bf0[15],
                                  &v_bit, &rnding);
  bf1[9] = half_btf_neon_mode01_r(&cospi[28], &bf0[9], &cospi[36], &bf0[14],
                                  &v_bit, &rnding);
  bf1[10] = half_btf_neon_mode01_r(&cospi[44], &bf0[10], &cospi[20], &bf0[13],
                                   &v_bit, &rnding);
  bf1[11] = half_btf_neon_mode01_r(&cospi[12], &bf0[11], &cospi[52], &bf0[12],
                                   &v_bit, &rnding);
  bf1[12] = half_btf_neon_r(&cospi[52], &bf0[11], &cospi[12], &bf0[12], &v_bit,
                            &rnding);
  bf1[13] = half_btf_neon_r(&cospi[20], &bf0[10], &cospi[44], &bf0[13], &v_bit,
                            &rnding);
  bf1[14] = half_btf_neon_r(&cospi[36], &bf0[9], &cospi[28], &bf0[14], &v_bit,
                            &rnding);
  bf1[15] = half_btf_neon_r(&cospi[4], &bf0[8], &cospi[60], &bf0[15], &v_bit,
                            &rnding);

  addsub_neon(bf0[16], bf0[17], bf1 + 16, bf1 + 17, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[19], bf0[18], bf1 + 19, bf1 + 18, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[20], bf0[21], bf1 + 20, bf1 + 21, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[23], bf0[22], bf1 + 23, bf1 + 22, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[24], bf0[25], bf1 + 24, bf1 + 25, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[27], bf0[26], bf1 + 27, bf1 + 26, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[28], bf0[29], bf1 + 28, bf1 + 29, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[31], bf0[30], bf1 + 31, bf1 + 30, &clamp_lo, &clamp_hi);

  // stage 4
  bf0[0] = bf1[0];
  bf0[1] = bf1[1];
  bf0[2] = bf1[2];
  bf0[3] = bf1[3];
  bf0[4] = half_btf_neon_mode01_r(&cospi[56], &bf1[4], &cospi[8], &bf1[7],
                                  &v_bit, &rnding);
  bf0[5] = half_btf_neon_mode01_r(&cospi[24], &bf1[5], &cospi[40], &bf1[6],
                                  &v_bit, &rnding);
  bf0[6] = half_btf_neon_r(&cospi[40], &bf1[5], &cospi[24], &bf1[6], &v_bit,
                           &rnding);
  bf0[7] =
      half_btf_neon_r(&cospi[8], &bf1[4], &cospi[56], &bf1[7], &v_bit, &rnding);

  addsub_neon(bf1[8], bf1[9], bf0 + 8, bf0 + 9, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[11], bf1[10], bf0 + 11, bf0 + 10, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[12], bf1[13], bf0 + 12, bf0 + 13, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[15], bf1[14], bf0 + 15, bf0 + 14, &clamp_lo, &clamp_hi);

  bf0[16] = bf1[16];
  bf0[17] = half_btf_neon_mode10_r(&cospi[8], &bf1[17], &cospi[56], &bf1[30],
                                   &v_bit, &rnding);
  bf0[18] = half_btf_neon_mode11_r(&cospi[56], &bf1[18], &cospi[8], &bf1[29],
                                   &v_bit, &rnding);
  bf0[19] = bf1[19];
  bf0[20] = bf1[20];
  bf0[21] = half_btf_neon_mode10_r(&cospi[40], &bf1[21], &cospi[24], &bf1[26],
                                   &v_bit, &rnding);
  bf0[22] = half_btf_neon_mode11_r(&cospi[24], &bf1[22], &cospi[40], &bf1[25],
                                   &v_bit, &rnding);
  bf0[23] = bf1[23];
  bf0[24] = bf1[24];
  bf0[25] = half_btf_neon_mode10_r(&cospi[40], &bf1[22], &cospi[24], &bf1[25],
                                   &v_bit, &rnding);
  bf0[26] = half_btf_neon_r(&cospi[24], &bf1[21], &cospi[40], &bf1[26], &v_bit,
                            &rnding);
  bf0[27] = bf1[27];
  bf0[28] = bf1[28];
  bf0[29] = half_btf_neon_mode10_r(&cospi[8], &bf1[18], &cospi[56], &bf1[29],
                                   &v_bit, &rnding);
  bf0[30] = half_btf_neon_r(&cospi[56], &bf1[17], &cospi[8], &bf1[30], &v_bit,
                            &rnding);
  bf0[31] = bf1[31];

  // stage 5
  bf1[0] = half_btf_neon_r(&cospi[32], &bf0[0], &cospi[32], &bf0[1], &v_bit,
                           &rnding);
  bf1[1] = half_btf_neon_mode01_r(&cospi[32], &bf0[0], &cospi[32], &bf0[1],
                                  &v_bit, &rnding);
  bf1[2] = half_btf_neon_mode01_r(&cospi[48], &bf0[2], &cospi[16], &bf0[3],
                                  &v_bit, &rnding);
  bf1[3] = half_btf_neon_r(&cospi[16], &bf0[2], &cospi[48], &bf0[3], &v_bit,
                           &rnding);
  addsub_neon(bf0[4], bf0[5], bf1 + 4, bf1 + 5, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[7], bf0[6], bf1 + 7, bf1 + 6, &clamp_lo, &clamp_hi);
  bf1[8] = bf0[8];
  bf1[9] = half_btf_neon_mode10_r(&cospi[16], &bf0[9], &cospi[48], &bf0[14],
                                  &v_bit, &rnding);
  bf1[10] = half_btf_neon_mode11_r(&cospi[48], &bf0[10], &cospi[16], &bf0[13],
                                   &v_bit, &rnding);
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = half_btf_neon_mode10_r(&cospi[16], &bf0[10], &cospi[48], &bf0[13],
                                   &v_bit, &rnding);
  bf1[14] = half_btf_neon_r(&cospi[48], &bf0[9], &cospi[16], &bf0[14], &v_bit,
                            &rnding);
  bf1[15] = bf0[15];
  addsub_neon(bf0[16], bf0[19], bf1 + 16, bf1 + 19, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[17], bf0[18], bf1 + 17, bf1 + 18, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[23], bf0[20], bf1 + 23, bf1 + 20, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[22], bf0[21], bf1 + 22, bf1 + 21, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[24], bf0[27], bf1 + 24, bf1 + 27, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[25], bf0[26], bf1 + 25, bf1 + 26, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[31], bf0[28], bf1 + 31, bf1 + 28, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[30], bf0[29], bf1 + 30, bf1 + 29, &clamp_lo, &clamp_hi);

  // stage 6
  addsub_neon(bf1[0], bf1[3], bf0 + 0, bf0 + 3, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[1], bf1[2], bf0 + 1, bf0 + 2, &clamp_lo, &clamp_hi);
  bf0[4] = bf1[4];
  bf0[5] = half_btf_neon_mode10_r(&cospi[32], &bf1[5], &cospi[32], &bf1[6],
                                  &v_bit, &rnding);
  bf0[6] = half_btf_neon_r(&cospi[32], &bf1[5], &cospi[32], &bf1[6], &v_bit,
                           &rnding);
  bf0[7] = bf1[7];
  addsub_neon(bf1[8], bf1[11], bf0 + 8, bf0 + 11, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[9], bf1[10], bf0 + 9, bf0 + 10, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[15], bf1[12], bf0 + 15, bf0 + 12, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[14], bf1[13], bf0 + 14, bf0 + 13, &clamp_lo, &clamp_hi);
  bf0[16] = bf1[16];
  bf0[17] = bf1[17];
  bf0[18] = half_btf_neon_mode10_r(&cospi[16], &bf1[18], &cospi[48], &bf1[29],
                                   &v_bit, &rnding);
  bf0[19] = half_btf_neon_mode10_r(&cospi[16], &bf1[19], &cospi[48], &bf1[28],
                                   &v_bit, &rnding);
  bf0[20] = half_btf_neon_mode11_r(&cospi[48], &bf1[20], &cospi[16], &bf1[27],
                                   &v_bit, &rnding);
  bf0[21] = half_btf_neon_mode11_r(&cospi[48], &bf1[21], &cospi[16], &bf1[26],
                                   &v_bit, &rnding);
  bf0[22] = bf1[22];
  bf0[23] = bf1[23];
  bf0[24] = bf1[24];
  bf0[25] = bf1[25];
  bf0[26] = half_btf_neon_mode10_r(&cospi[16], &bf1[21], &cospi[48], &bf1[26],
                                   &v_bit, &rnding);
  bf0[27] = half_btf_neon_mode10_r(&cospi[16], &bf1[20], &cospi[48], &bf1[27],
                                   &v_bit, &rnding);
  bf0[28] = half_btf_neon_r(&cospi[48], &bf1[19], &cospi[16], &bf1[28], &v_bit,
                            &rnding);
  bf0[29] = half_btf_neon_r(&cospi[48], &bf1[18], &cospi[16], &bf1[29], &v_bit,
                            &rnding);
  bf0[30] = bf1[30];
  bf0[31] = bf1[31];

  // stage 7
  addsub_neon(bf0[0], bf0[7], bf1 + 0, bf1 + 7, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[1], bf0[6], bf1 + 1, bf1 + 6, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[2], bf0[5], bf1 + 2, bf1 + 5, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[3], bf0[4], bf1 + 3, bf1 + 4, &clamp_lo, &clamp_hi);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf_neon_mode10_r(&cospi[32], &bf0[10], &cospi[32], &bf0[13],
                                   &v_bit, &rnding);
  bf1[11] = half_btf_neon_mode10_r(&cospi[32], &bf0[11], &cospi[32], &bf0[12],
                                   &v_bit, &rnding);
  bf1[12] = half_btf_neon_r(&cospi[32], &bf0[11], &cospi[32], &bf0[12], &v_bit,
                            &rnding);
  bf1[13] = half_btf_neon_r(&cospi[32], &bf0[10], &cospi[32], &bf0[13], &v_bit,
                            &rnding);
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  addsub_neon(bf0[16], bf0[23], bf1 + 16, bf1 + 23, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[17], bf0[22], bf1 + 17, bf1 + 22, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[18], bf0[21], bf1 + 18, bf1 + 21, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[19], bf0[20], bf1 + 19, bf1 + 20, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[31], bf0[24], bf1 + 31, bf1 + 24, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[30], bf0[25], bf1 + 30, bf1 + 25, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[29], bf0[26], bf1 + 29, bf1 + 26, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[28], bf0[27], bf1 + 28, bf1 + 27, &clamp_lo, &clamp_hi);

  // stage 8
  addsub_neon(bf1[0], bf1[15], bf0 + 0, bf0 + 15, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[1], bf1[14], bf0 + 1, bf0 + 14, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[2], bf1[13], bf0 + 2, bf0 + 13, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[3], bf1[12], bf0 + 3, bf0 + 12, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[4], bf1[11], bf0 + 4, bf0 + 11, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[5], bf1[10], bf0 + 5, bf0 + 10, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[6], bf1[9], bf0 + 6, bf0 + 9, &clamp_lo, &clamp_hi);
  addsub_neon(bf1[7], bf1[8], bf0 + 7, bf0 + 8, &clamp_lo, &clamp_hi);
  bf0[16] = bf1[16];
  bf0[17] = bf1[17];
  bf0[18] = bf1[18];
  bf0[19] = bf1[19];
  bf0[20] = half_btf_neon_mode10_r(&cospi[32], &bf1[20], &cospi[32], &bf1[27],
                                   &v_bit, &rnding);
  bf0[21] = half_btf_neon_mode10_r(&cospi[32], &bf1[21], &cospi[32], &bf1[26],
                                   &v_bit, &rnding);
  bf0[22] = half_btf_neon_mode10_r(&cospi[32], &bf1[22], &cospi[32], &bf1[25],
                                   &v_bit, &rnding);
  bf0[23] = half_btf_neon_mode10_r(&cospi[32], &bf1[23], &cospi[32], &bf1[24],
                                   &v_bit, &rnding);
  bf0[24] = half_btf_neon_r(&cospi[32], &bf1[23], &cospi[32], &bf1[24], &v_bit,
                            &rnding);
  bf0[25] = half_btf_neon_r(&cospi[32], &bf1[22], &cospi[32], &bf1[25], &v_bit,
                            &rnding);
  bf0[26] = half_btf_neon_r(&cospi[32], &bf1[21], &cospi[32], &bf1[26], &v_bit,
                            &rnding);
  bf0[27] = half_btf_neon_r(&cospi[32], &bf1[20], &cospi[32], &bf1[27], &v_bit,
                            &rnding);
  bf0[28] = bf1[28];
  bf0[29] = bf1[29];
  bf0[30] = bf1[30];
  bf0[31] = bf1[31];

  // stage 9
  addsub_neon(bf0[0], bf0[31], out + 0, out + 31, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[1], bf0[30], out + 1, out + 30, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[2], bf0[29], out + 2, out + 29, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[3], bf0[28], out + 3, out + 28, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[4], bf0[27], out + 4, out + 27, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[5], bf0[26], out + 5, out + 26, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[6], bf0[25], out + 6, out + 25, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[7], bf0[24], out + 7, out + 24, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[8], bf0[23], out + 8, out + 23, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[9], bf0[22], out + 9, out + 22, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[10], bf0[21], out + 10, out + 21, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[11], bf0[20], out + 11, out + 20, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[12], bf0[19], out + 12, out + 19, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[13], bf0[18], out + 13, out + 18, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[14], bf0[17], out + 14, out + 17, &clamp_lo, &clamp_hi);
  addsub_neon(bf0[15], bf0[16], out + 15, out + 16, &clamp_lo, &clamp_hi);

  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    round_shift_8x8(out, out_shift);
    round_shift_8x8(out + 16, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo_out, &clamp_hi_out, 32);
  }
}

static void iidentity32_neon(int32x4_t *in, int32x4_t *out, int bit,
                             int do_cols, int bd, int out_shift) {
  (void)bit;
  for (int i = 0; i < 32; i += 16) {
    out[i] = vshlq_n_s32(in[i], 2);
    out[i + 1] = vshlq_n_s32(in[i + 1], 2);
    out[i + 2] = vshlq_n_s32(in[i + 2], 2);
    out[i + 3] = vshlq_n_s32(in[i + 3], 2);
    out[i + 4] = vshlq_n_s32(in[i + 4], 2);
    out[i + 5] = vshlq_n_s32(in[i + 5], 2);
    out[i + 6] = vshlq_n_s32(in[i + 6], 2);
    out[i + 7] = vshlq_n_s32(in[i + 7], 2);
    out[i + 8] = vshlq_n_s32(in[i + 8], 2);
    out[i + 9] = vshlq_n_s32(in[i + 9], 2);
    out[i + 10] = vshlq_n_s32(in[i + 10], 2);
    out[i + 11] = vshlq_n_s32(in[i + 11], 2);
    out[i + 12] = vshlq_n_s32(in[i + 12], 2);
    out[i + 13] = vshlq_n_s32(in[i + 13], 2);
    out[i + 14] = vshlq_n_s32(in[i + 14], 2);
    out[i + 15] = vshlq_n_s32(in[i + 15], 2);
  }

  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const int32x4_t clamp_lo_out = vdupq_n_s32(-(1 << (log_range_out - 1)));
    const int32x4_t clamp_hi_out = vdupq_n_s32((1 << (log_range_out - 1)) - 1);
    round_shift_8x8(out, out_shift);
    round_shift_8x8(out + 16, out_shift);
    highbd_clamp_s32_neon(out, out, &clamp_lo_out, &clamp_hi_out, 32);
  }
}

// 1D itx types
typedef enum ATTRIBUTE_PACKED {
  IDCT_1D,
  IADST_1D,
  IFLIPADST_1D = IADST_1D,
  IIDENTITY_1D,
  ITX_TYPES_1D,
} ITX_TYPE_1D;

static const ITX_TYPE_1D vitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IADST_1D,     IDCT_1D,      IADST_1D,
  IFLIPADST_1D, IDCT_1D,      IFLIPADST_1D, IADST_1D,
  IFLIPADST_1D, IIDENTITY_1D, IDCT_1D,      IIDENTITY_1D,
  IADST_1D,     IIDENTITY_1D, IFLIPADST_1D, IIDENTITY_1D,
};
static const ITX_TYPE_1D hitx_1d_tab[TX_TYPES] = {
  IDCT_1D,      IDCT_1D,      IADST_1D,     IADST_1D,
  IDCT_1D,      IFLIPADST_1D, IFLIPADST_1D, IFLIPADST_1D,
  IADST_1D,     IIDENTITY_1D, IIDENTITY_1D, IDCT_1D,
  IIDENTITY_1D, IADST_1D,     IIDENTITY_1D, IFLIPADST_1D,
};

static const transform_1d_neon
    highbd_txfm_all_1d_zeros_w8_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { idct4x4_neon, NULL, NULL, NULL },
          { iadst4x4_neon, NULL, NULL, NULL },
          { iidentity4_neon, iidentity4_neon, iidentity4_neon, NULL },
      },
      { { idct8x8_low1_neon, idct8x8_new_neon, NULL, NULL },
        { iadst8x8_low1_neon, iadst8x8_new_neon, NULL, NULL },
        { iidentity8_neon, iidentity8_neon, NULL, NULL } },
      {
          { idct16x16_low1_neon, idct16x16_low8_neon, idct16x16_neon, NULL },
          { iadst16x16_low1_neon, iadst16x16_low8_neon, iadst16x16_neon, NULL },
          { iidentity16_neon, NULL, iidentity16_neon, NULL },
      },
      { { idct32x32_low1_neon, idct32x32_low8_neon, idct32x32_low16_neon,
          idct32x32_neon },
        { NULL, NULL, NULL, NULL },
        { iidentity32_neon, NULL, NULL, NULL } },
      { { idct64x64_low1_neon, idct64x64_low8_neon, idct64x64_low16_neon,
          idct64x64_neon },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };

void av1_inv_txfm2d_add_4x8_neon(const tran_low_t *input, uint16_t *output,
                                 int stride, TX_TYPE tx_type, const int bd) {
  TX_SIZE tx_size = TX_4X8;
  int32x4_t buf1[32] = { vdupq_n_s32(0) };

  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][0];
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][1];
  const int input_stride = AOMMIN(32, txfm_size_row);

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // 1st stage: column transform
  int32x4_t buf0[8];
  load_buffer_32bit_input(input, input_stride, buf0, txfm_size_col);
  load_buffer_32bit_input(input + 4, input_stride, buf0 + 4, txfm_size_col);
  round_shift_rect_array_32_neon(buf0, buf0, txfm_size_row);
  row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);
  row_txfm(buf0 + 4, buf0 + 4, INV_COS_BIT, 0, bd, -shift[0]);

  if (lr_flip) {
    TRANSPOSE_4X4(buf0[3], buf0[2], buf0[1], buf0[0], buf1[0], buf1[1], buf1[2],
                  buf1[3]);

    TRANSPOSE_4X4(buf0[7], buf0[6], buf0[5], buf0[4], buf1[4], buf1[5], buf1[6],
                  buf1[7]);
  } else {
    TRANSPOSE_4X4(buf0[0], buf0[1], buf0[2], buf0[3], buf1[0], buf1[1], buf1[2],
                  buf1[3]);

    TRANSPOSE_4X4(buf0[4], buf0[5], buf0[6], buf0[7], buf1[4], buf1[5], buf1[6],
                  buf1[7]);
  }

  // 2nd stage: column transform
  col_txfm(buf1, buf1, INV_COS_BIT, 1, bd, 0);

  round_shift_array_32_neon(buf1, buf1, txfm_size_row, -shift[1]);

  // write to buffer
  highbd_write_buffer_4xn_neon(buf1, output, stride, ud_flip, txfm_size_row,
                               bd);
}

void av1_inv_txfm2d_add_8x4_neon(const int32_t *input, uint16_t *output,
                                 int stride, TX_TYPE tx_type, const int bd) {
  TX_SIZE tx_size = TX_8X4;
  int32x4_t buf1[8];
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][1];
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][0];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // 1st stage: column transform
  int32x4_t buf0[8];
  const int32_t *input_row = input;
  load_buffer_32bit_input(input_row, 4, buf0, txfm_size_col);

  round_shift_rect_array_32_neon(buf0, buf0, txfm_size_col);
  row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);

  int32x4_t *buf1_ptr;
  if (lr_flip) {
    flip_buf_neon(buf0, buf1, txfm_size_col);
    buf1_ptr = buf1;
  } else {
    buf1_ptr = buf0;
  }

  // 2nd stage: column transform
  for (int i = 0; i < 2; i++) {
    int32x4_t *buf1_cur = buf1_ptr + i * txfm_size_row;
    transpose_4x4(buf1_cur, buf1_cur);
    col_txfm(buf1_cur, buf1_cur, INV_COS_BIT, 1, bd, 0);
  }
  round_shift_array_32_neon(buf1_ptr, buf1_ptr, txfm_size_col, -shift[1]);
  // write to buffer
  highbd_write_buffer_8xn_neon(buf1_ptr, output, stride, ud_flip, txfm_size_row,
                               bd);
}

void av1_inv_txfm2d_add_4x16_neon(const int32_t *input, uint16_t *output,
                                  int stride, TX_TYPE tx_type, const int bd) {
  TX_SIZE tx_size = TX_4X16;
  int32x4_t buf1[16];
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_h_div8 = txfm_size_row >> 2;
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][0];
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][2];
  const int input_stride = AOMMIN(32, txfm_size_row);

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // 1st stage: column transform
  int32x4_t buf0[16];
  for (int i = 0; i < (txfm_size_row >> 2); i++) {
    const int32_t *input_row = input + i * 4;
    int32x4_t *buf0_cur = buf0 + i * 4;
    load_buffer_32bit_input(input_row, input_stride, buf0_cur, txfm_size_col);
    row_txfm(buf0 + (i << 2), buf0 + (i << 2), INV_COS_BIT, 0, bd, -shift[0]);
  }

  if (lr_flip) {
    for (int j = 0; j < buf_size_h_div8; ++j) {
      TRANSPOSE_4X4(buf0[4 * j + 3], buf0[4 * j + 2], buf0[4 * j + 1],
                    buf0[4 * j], buf1[4 * j], buf1[4 * j + 1], buf1[4 * j + 2],
                    buf1[4 * j + 3]);
    }
  } else {
    for (int j = 0; j < buf_size_h_div8; ++j) {
      TRANSPOSE_4X4(buf0[4 * j], buf0[4 * j + 1], buf0[4 * j + 2],
                    buf0[4 * j + 3], buf1[4 * j], buf1[4 * j + 1],
                    buf1[4 * j + 2], buf1[4 * j + 3]);
    }
  }

  // 2nd stage: column transform
  col_txfm(buf1, buf1, INV_COS_BIT, 1, bd, 0);

  round_shift_array_32_neon(buf1, buf1, txfm_size_row, -shift[1]);

  // write to buffer
  highbd_write_buffer_4xn_neon(buf1, output, stride, ud_flip, txfm_size_row,
                               bd);
}

void av1_inv_txfm2d_add_16x4_neon(const int32_t *input, uint16_t *output,
                                  int stride, TX_TYPE tx_type, const int bd) {
  TX_SIZE tx_size = TX_16X4;
  int32x4_t buf1[16];
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 2;
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][2];
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][0];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // 1st stage: column transform
  int32x4_t buf0[16];
  const int32_t *input_row = input;
  load_buffer_32bit_input(input_row, 4, buf0, txfm_size_col);

  row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);

  int32x4_t *buf1_ptr;
  if (lr_flip) {
    flip_buf_neon(buf0, buf1, txfm_size_col);
    buf1_ptr = buf1;
  } else {
    buf1_ptr = buf0;
  }

  // 2nd stage: column transform
  for (int i = 0; i < buf_size_w_div8; i++) {
    int32x4_t *buf1_cur = buf1_ptr + i * txfm_size_row;
    transpose_4x4(buf1_cur, buf1_cur);
    col_txfm(buf1_cur, buf1_cur, INV_COS_BIT, 1, bd, 0);
  }
  round_shift_array_32_neon(buf1_ptr, buf1_ptr, txfm_size_col, -shift[1]);

  // write to buffer
  for (int i = 0; i < (txfm_size_col >> 3); i++) {
    highbd_write_buffer_8xn_neon(buf1_ptr + i * txfm_size_row * 2,
                                 output + 8 * i, stride, ud_flip, txfm_size_row,
                                 bd);
  }
}

static const int lowbd_txfm_all_1d_zeros_idx[32] = {
  0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

// Transform block width in log2 for eob (size of 64 map to 32)
static const int tx_size_wide_log2_eob[TX_SIZES_ALL] = {
  2, 3, 4, 5, 5, 2, 3, 3, 4, 4, 5, 5, 5, 2, 4, 3, 5, 4, 5,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_8x8_default[8]) = {
  0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0707,
};

DECLARE_ALIGNED(16, static const int16_t,
                av1_eob_to_eobxy_16x16_default[16]) = {
  0x0707, 0x0707, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f,
  0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f,
};

DECLARE_ALIGNED(16, static const int16_t,
                av1_eob_to_eobxy_32x32_default[32]) = {
  0x0707, 0x0f0f, 0x0f0f, 0x0f0f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f,
  0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f,
  0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f,
  0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f, 0x1f1f,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_8x16_default[16]) = {
  0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0f07, 0x0f07, 0x0f07,
  0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_16x8_default[8]) = {
  0x0707, 0x0707, 0x070f, 0x070f, 0x070f, 0x070f, 0x070f, 0x070f,
};

DECLARE_ALIGNED(16, static const int16_t,
                av1_eob_to_eobxy_16x32_default[32]) = {
  0x0707, 0x0707, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f0f,
  0x0f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f,
  0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f,
  0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f, 0x1f0f,
};

DECLARE_ALIGNED(16, static const int16_t,
                av1_eob_to_eobxy_32x16_default[16]) = {
  0x0707, 0x0f0f, 0x0f0f, 0x0f0f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f,
  0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f, 0x0f1f,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_8x32_default[32]) = {
  0x0707, 0x0707, 0x0707, 0x0707, 0x0707, 0x0f07, 0x0f07, 0x0f07,
  0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x0f07, 0x1f07, 0x1f07, 0x1f07,
  0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07,
  0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07, 0x1f07,
};

DECLARE_ALIGNED(16, static const int16_t, av1_eob_to_eobxy_32x8_default[8]) = {
  0x0707, 0x070f, 0x070f, 0x071f, 0x071f, 0x071f, 0x071f, 0x071f,
};

DECLARE_ALIGNED(16, static const int16_t *,
                av1_eob_to_eobxy_default[TX_SIZES_ALL]) = {
  NULL,
  av1_eob_to_eobxy_8x8_default,
  av1_eob_to_eobxy_16x16_default,
  av1_eob_to_eobxy_32x32_default,
  av1_eob_to_eobxy_32x32_default,
  NULL,
  NULL,
  av1_eob_to_eobxy_8x16_default,
  av1_eob_to_eobxy_16x8_default,
  av1_eob_to_eobxy_16x32_default,
  av1_eob_to_eobxy_32x16_default,
  av1_eob_to_eobxy_32x32_default,
  av1_eob_to_eobxy_32x32_default,
  NULL,
  NULL,
  av1_eob_to_eobxy_8x32_default,
  av1_eob_to_eobxy_32x8_default,
  av1_eob_to_eobxy_16x32_default,
  av1_eob_to_eobxy_32x16_default,
};

static INLINE void highbd_get_eobx_eoby_scan_default(int *eobx, int *eoby,
                                                     TX_SIZE tx_size, int eob) {
  if (eob == 1) {
    *eobx = 0;
    *eoby = 0;
    return;
  }

  const int tx_w_log2 = tx_size_wide_log2_eob[tx_size];
  const int eob_row = (eob - 1) >> tx_w_log2;
  const int eobxy = av1_eob_to_eobxy_default[tx_size][eob_row];
  *eobx = eobxy & 0xFF;
  *eoby = eobxy >> 8;
}

static INLINE void get_eobx_eoby_scan_default(int *eobx, int *eoby,
                                              TX_SIZE tx_size) {
  if (tx_size == 2) {
    *eoby = 15, *eobx = 15;
  } else if (tx_size == 3) {
    *eoby = 31, *eobx = 31;
  } else if (tx_size == 4) {
    *eoby = 31, *eobx = 31;
  } else if (tx_size == 7) {
    *eoby = 15, *eobx = 7;
  } else if (tx_size == 8) {
    *eoby = 7, *eobx = 15;
  } else if (tx_size == 9) {
    *eoby = 31, *eobx = 15;
  } else if (tx_size == 10) {
    *eoby = 15, *eobx = 31;
  } else if (tx_size == 11) {
    *eoby = 31, *eobx = 31;
  } else if (tx_size == 12) {
    *eoby = 31, *eobx = 31;
  } else if (tx_size == 15) {
    *eoby = 31, *eobx = 7;
  } else if (tx_size == 16) {
    *eoby = 7, *eobx = 31;
  } else if (tx_size == 17) {
    *eoby = 31, *eobx = 15;
  } else if (tx_size == 18) {
    *eoby = 15, *eobx = 31;
  } else {
    *eoby = 0, *eobx = 0;
  }
}

static INLINE void get_eobx_eoby_scan_v_identity(int *eobx, int *eoby,
                                                 TX_SIZE tx_size) {
  const int txfm_size_row = tx_size_high[tx_size];
  *eoby = AOMMIN(32, txfm_size_row) - 1;
  *eobx = 0;
}

static INLINE void get_eobx_eoby_scan_h_identity(int *eobx, int *eoby,
                                                 TX_SIZE tx_size) {
  const int txfm_size_col = tx_size_wide[tx_size];
  *eobx = AOMMIN(32, txfm_size_col) - 1;
  *eoby = 0;
}

static void inv_txfm2d_add_h_identity_neon(const int32_t *input,
                                           uint16_t *output, int stride,
                                           TX_TYPE tx_type, TX_SIZE tx_size,
                                           const int bd) {
  int32x4_t buf1[64];
  int eobx, eoby;
  get_eobx_eoby_scan_v_identity(&eobx, &eoby, tx_size);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w = AOMMIN(32, txfm_size_col);
  const int buf_size_w_div4 = buf_size_w >> 2;
  const int buf_size_h_div8 = (eoby + 8) >> 3;
  const int row_max = AOMMIN(32, txfm_size_row);
  const int input_stride = row_max;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int fun_idx = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][0];
  assert(row_txfm != NULL);
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx];
  assert(col_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < (buf_size_h_div8 << 1); ++i) {
    int32x4_t buf0[16];
    load_buffer_32bit_input(input + i * 4, input_stride, buf0, buf_size_w);
    if (rect_type == 1 || rect_type == -1) {
      round_shift_rect_array_32_neon(buf0, buf0, buf_size_w);
    }
    row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);

    int32x4_t *_buf1 = buf1 + i * 4;

    for (int j = 0; j < buf_size_w_div4; ++j) {
      int32x4_t *buf0_cur = buf0 + j * 4;
      TRANSPOSE_4X4(buf0_cur[0], buf0_cur[1], buf0_cur[2], buf0_cur[3],
                    buf0_cur[0], buf0_cur[1], buf0_cur[2], buf0_cur[3]);
      _buf1[j * txfm_size_row + 0] = buf0_cur[0];
      _buf1[j * txfm_size_row + 1] = buf0_cur[1];
      _buf1[j * txfm_size_row + 2] = buf0_cur[2];
      _buf1[j * txfm_size_row + 3] = buf0_cur[3];
    }
  }
  for (int i = 0; i < buf_size_w_div4; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row, INV_COS_BIT, 1,
             bd, 0);

    round_shift_array_32_neon(buf1 + i * txfm_size_row,
                              buf1 + i * txfm_size_row, txfm_size_row,
                              -shift[1]);
  }

  // write to buffer
  for (int i = 0; i < (txfm_size_col >> 3); i++) {
    highbd_write_buffer_8xn_neon(buf1 + i * txfm_size_row * 2, output + 8 * i,
                                 stride, ud_flip, txfm_size_row, bd);
  }
}

static void inv_txfm2d_add_v_identity_neon(const int32_t *input,
                                           uint16_t *output, int stride,
                                           TX_TYPE tx_type, TX_SIZE tx_size,
                                           const int bd) {
  int32x4_t buf1[64];
  int eobx, eoby;
  get_eobx_eoby_scan_h_identity(&eobx, &eoby, tx_size);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div4 = AOMMIN(32, txfm_size_col) >> 2;
  const int row_max = AOMMIN(32, txfm_size_row);
  const int input_stride = row_max;
  const int buf_size_nonzero_w_div8 = (eobx + 8) >> 3;
  const int buf_size_nonzero_w = buf_size_nonzero_w_div8 << 3;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int fun_idx = lowbd_txfm_all_1d_zeros_idx[eobx];
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx];
  assert(row_txfm != NULL);
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][0];
  assert(col_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  for (int i = 0; i < (row_max >> 2); ++i) {
    int32x4_t buf0[16];
    load_buffer_32bit_input(input + i * 4, input_stride, buf0,
                            buf_size_nonzero_w);
    if (rect_type == 1 || rect_type == -1) {
      round_shift_rect_array_32_neon(buf0, buf0, buf_size_nonzero_w);
    }
    row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);

    int32x4_t *_buf1 = buf1 + i * 4;
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div4; ++j) {
        TRANSPOSE_4X4(buf0[4 * j + 3], buf0[4 * j + 2], buf0[4 * j + 1],
                      buf0[4 * j],
                      _buf1[txfm_size_row * (buf_size_w_div4 - 1 - j) + 0],
                      _buf1[txfm_size_row * (buf_size_w_div4 - 1 - j) + 1],
                      _buf1[txfm_size_row * (buf_size_w_div4 - 1 - j) + 2],
                      _buf1[txfm_size_row * (buf_size_w_div4 - 1 - j) + 3]);
      }
    } else {
      for (int j = 0; j < buf_size_w_div4; ++j) {
        TRANSPOSE_4X4(
            buf0[j * 4 + 0], buf0[j * 4 + 1], buf0[j * 4 + 2], buf0[j * 4 + 3],
            _buf1[j * txfm_size_row + 0], _buf1[j * txfm_size_row + 1],
            _buf1[j * txfm_size_row + 2], _buf1[j * txfm_size_row + 3]);
      }
    }
  }
  for (int i = 0; i < buf_size_w_div4; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row, INV_COS_BIT, 1,
             bd, 0);

    round_shift_array_32_neon(buf1 + i * txfm_size_row,
                              buf1 + i * txfm_size_row, txfm_size_row,
                              -shift[1]);
  }

  // write to buffer
  {
    for (int i = 0; i < (txfm_size_col >> 3); i++) {
      highbd_write_buffer_8xn_neon(buf1 + i * txfm_size_row * 2, output + 8 * i,
                                   stride, ud_flip, txfm_size_row, bd);
    }
  }
}

static void inv_txfm2d_add_idtx_neon(const int32_t *input, uint16_t *output,
                                     int stride, TX_TYPE tx_type,
                                     TX_SIZE tx_size, const int bd) {
  int32x4_t buf1[64 * 4];
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int row_max = AOMMIN(32, txfm_size_row);
  const int input_stride = row_max;
  const int buf_size_w = AOMMIN(32, txfm_size_col);
  const int buf_size_w_div4 = buf_size_w >> 2;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][0];
  assert(row_txfm != NULL);
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][0];
  assert(col_txfm != NULL);
  for (int i = 0; i < (row_max >> 2); ++i) {
    int32x4_t buf0[32];
    load_buffer_32bit_input(input + i * 4, input_stride, buf0, buf_size_w);
    if (rect_type == 1 || rect_type == -1) {
      round_shift_rect_array_32_neon(buf0, buf0, buf_size_w);
    }
    row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);

    int32x4_t *_buf1 = buf1 + i * 4;
    for (int j = 0; j < buf_size_w_div4; ++j) {
      int32x4_t *buf0_cur = buf0 + j * 4;
      TRANSPOSE_4X4(buf0_cur[0], buf0_cur[1], buf0_cur[2], buf0_cur[3],
                    buf0_cur[0], buf0_cur[1], buf0_cur[2], buf0_cur[3]);
      _buf1[j * txfm_size_row + 0] = buf0_cur[0];
      _buf1[j * txfm_size_row + 1] = buf0_cur[1];
      _buf1[j * txfm_size_row + 2] = buf0_cur[2];
      _buf1[j * txfm_size_row + 3] = buf0_cur[3];
    }
  }
  for (int i = 0; i < buf_size_w_div4; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row, INV_COS_BIT, 1,
             bd, 0);

    round_shift_array_32_neon(buf1 + i * txfm_size_row,
                              buf1 + i * txfm_size_row, txfm_size_row,
                              -shift[1]);
  }

  // write to buffer
  {
    for (int i = 0; i < (txfm_size_col >> 3); i++) {
      highbd_write_buffer_8xn_neon(buf1 + i * txfm_size_row * 2, output + 8 * i,
                                   stride, 0, txfm_size_row, bd);
    }
  }
}

static void inv_txfm2d_add_no_identity_neon(const int32_t *input,
                                            uint16_t *output, int stride,
                                            TX_TYPE tx_type, TX_SIZE tx_size,
                                            const int bd) {
  int32x4_t buf1[64 * 16];
  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div4 = txfm_size_col >> 2;
  const int buf_size_nonzero_w = (eobx + 8) >> 3 << 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int input_stride = AOMMIN(32, txfm_size_row);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  // 1st stage: column transform
  for (int i = 0; i < buf_size_nonzero_h_div8 << 1; i++) {
    int32x4_t buf0[64];
    load_buffer_32bit_input(input + i * 4, input_stride, buf0,
                            buf_size_nonzero_w);
    if (rect_type == 1 || rect_type == -1) {
      round_shift_rect_array_32_neon(buf0, buf0, buf_size_nonzero_w);
    }
    row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);

    int32x4_t *_buf1 = &buf1[i * 4];

    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div4; ++j) {
        TRANSPOSE_4X4(buf0[4 * j + 3], buf0[4 * j + 2], buf0[4 * j + 1],
                      buf0[4 * j],
                      _buf1[txfm_size_row * (buf_size_w_div4 - 1 - j) + 0],
                      _buf1[txfm_size_row * (buf_size_w_div4 - 1 - j) + 1],
                      _buf1[txfm_size_row * (buf_size_w_div4 - 1 - j) + 2],
                      _buf1[txfm_size_row * (buf_size_w_div4 - 1 - j) + 3]);
      }
    } else {
      for (int j = 0; j < buf_size_w_div4; ++j) {
        TRANSPOSE_4X4(
            buf0[j * 4 + 0], buf0[j * 4 + 1], buf0[j * 4 + 2], buf0[j * 4 + 3],
            _buf1[j * txfm_size_row + 0], _buf1[j * txfm_size_row + 1],
            _buf1[j * txfm_size_row + 2], _buf1[j * txfm_size_row + 3]);
      }
    }
  }
  // 2nd stage: column transform
  for (int i = 0; i < buf_size_w_div4; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row, INV_COS_BIT, 1,
             bd, 0);

    round_shift_array_32_neon(buf1 + i * txfm_size_row,
                              buf1 + i * txfm_size_row, txfm_size_row,
                              -shift[1]);
  }

  // write to buffer
  {
    for (int i = 0; i < (txfm_size_col >> 3); i++) {
      highbd_write_buffer_8xn_neon(buf1 + i * txfm_size_row * 2, output + 8 * i,
                                   stride, ud_flip, txfm_size_row, bd);
    }
  }
}

static void highbd_inv_txfm2d_add_no_identity_neon(const int32_t *input,
                                                   uint16_t *output, int stride,
                                                   TX_TYPE tx_type,
                                                   TX_SIZE tx_size, int eob,
                                                   const int bd) {
  int32x4_t buf1[64 * 16];
  int eobx, eoby;
  highbd_get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 2;
  const int buf_size_nonzero_w_div8 = (eobx + 8) >> 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int input_stride = AOMMIN(32, txfm_size_col);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_neon row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_neon col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  // 1st stage: column transform
  for (int i = 0; i < buf_size_nonzero_h_div8 << 1; i++) {
    int32x4_t buf0[64];
    const int32_t *input_row = input + i * input_stride * 4;
    for (int j = 0; j < buf_size_nonzero_w_div8 << 1; ++j) {
      int32x4_t *buf0_cur = &buf0[j * 4];
      load_buffer_32bit_input(input_row + j * 4, input_stride, buf0_cur, 4);

      TRANSPOSE_4X4(buf0_cur[0], buf0_cur[1], buf0_cur[2], buf0_cur[3],
                    buf0_cur[0], buf0_cur[1], buf0_cur[2], buf0_cur[3]);
    }
    if (rect_type == 1 || rect_type == -1) {
      round_shift_rect_array_32_neon(buf0, buf0, buf_size_nonzero_w_div8 << 3);
    }
    row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);

    int32x4_t *_buf1 = &buf1[i * 4];

    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        TRANSPOSE_4X4(buf0[4 * j + 3], buf0[4 * j + 2], buf0[4 * j + 1],
                      buf0[4 * j],
                      _buf1[txfm_size_row * (buf_size_w_div8 - 1 - j) + 0],
                      _buf1[txfm_size_row * (buf_size_w_div8 - 1 - j) + 1],
                      _buf1[txfm_size_row * (buf_size_w_div8 - 1 - j) + 2],
                      _buf1[txfm_size_row * (buf_size_w_div8 - 1 - j) + 3]);
      }
    } else {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        TRANSPOSE_4X4(
            buf0[j * 4 + 0], buf0[j * 4 + 1], buf0[j * 4 + 2], buf0[j * 4 + 3],
            _buf1[j * txfm_size_row + 0], _buf1[j * txfm_size_row + 1],
            _buf1[j * txfm_size_row + 2], _buf1[j * txfm_size_row + 3]);
      }
    }
  }
  // 2nd stage: column transform
  for (int i = 0; i < buf_size_w_div8; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row, INV_COS_BIT, 1,
             bd, 0);

    round_shift_array_32_neon(buf1 + i * txfm_size_row,
                              buf1 + i * txfm_size_row, txfm_size_row,
                              -shift[1]);
  }

  // write to buffer
  {
    for (int i = 0; i < (txfm_size_col >> 3); i++) {
      highbd_write_buffer_8xn_neon(buf1 + i * txfm_size_row * 2, output + 8 * i,
                                   stride, ud_flip, txfm_size_row, bd);
    }
  }
}

static void highbd_inv_txfm2d_add_universe_neon(const int32_t *input,
                                                uint8_t *output, int stride,
                                                TX_TYPE tx_type,
                                                TX_SIZE tx_size, int eob,
                                                const int bd) {
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      highbd_inv_txfm2d_add_no_identity_neon(input, CONVERT_TO_SHORTPTR(output),
                                             stride, tx_type, tx_size, eob, bd);
      break;
    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      inv_txfm2d_add_h_identity_neon(input, CONVERT_TO_SHORTPTR(output), stride,
                                     tx_type, tx_size, bd);
      break;
    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      inv_txfm2d_add_v_identity_neon(input, CONVERT_TO_SHORTPTR(output), stride,
                                     tx_type, tx_size, bd);
      break;
    case IDTX:
      inv_txfm2d_add_idtx_neon(input, CONVERT_TO_SHORTPTR(output), stride,
                               tx_type, tx_size, bd);
      break;
    default: assert(0); break;
  }
}

static void inv_txfm2d_add_universe_neon(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type,
                                         TX_SIZE tx_size, const int bd) {
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      inv_txfm2d_add_no_identity_neon(input, CONVERT_TO_SHORTPTR(output),
                                      stride, tx_type, tx_size, bd);
      break;
    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      inv_txfm2d_add_h_identity_neon(input, CONVERT_TO_SHORTPTR(output), stride,
                                     tx_type, tx_size, bd);
      break;
    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      inv_txfm2d_add_v_identity_neon(input, CONVERT_TO_SHORTPTR(output), stride,
                                     tx_type, tx_size, bd);
      break;
    case IDTX:
      inv_txfm2d_add_idtx_neon(input, CONVERT_TO_SHORTPTR(output), stride,
                               tx_type, tx_size, bd);
      break;
    default: assert(0); break;
  }
}

static void highbd_inv_txfm_add_8x8_neon(const tran_low_t *input, uint8_t *dest,
                                         int stride,
                                         const TxfmParam *txfm_param) {
  int bd = txfm_param->bd;
  const TX_TYPE tx_type = txfm_param->tx_type;
  const int32_t *src = cast_to_int32(input);
  switch (tx_type) {
    case IDTX:
    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      highbd_inv_txfm2d_add_universe_neon(input, dest, stride, tx_type,
                                          txfm_param->tx_size, txfm_param->eob,
                                          bd);
      break;
    default:
      av1_inv_txfm2d_add_8x8_neon(src, CONVERT_TO_SHORTPTR(dest), stride,
                                  tx_type, bd);
      break;
  }
}

static void highbd_inv_txfm_add_4x4_neon(const tran_low_t *input, uint8_t *dest,
                                         int stride,
                                         const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  int eob = txfm_param->eob;
  int bd = txfm_param->bd;
  int lossless = txfm_param->lossless;
  const int32_t *src = cast_to_int32(input);
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (lossless) {
    assert(tx_type == DCT_DCT);
    av1_highbd_iwht4x4_add(input, dest, stride, eob, bd);
    return;
  }
  av1_inv_txfm2d_add_4x4_neon(src, CONVERT_TO_SHORTPTR(dest), stride, tx_type,
                              bd);
}

void av1_inv_txfm2d_add_8x16_neon(const tran_low_t *input, uint16_t *dest,
                                  int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type, TX_8X16,
                               bd);
}

void av1_inv_txfm2d_add_16x8_neon(const tran_low_t *input, uint16_t *dest,
                                  int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type, TX_16X8,
                               bd);
}

void av1_inv_txfm2d_add_16x32_neon(const tran_low_t *input, uint16_t *dest,
                                   int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_16X32, bd);
}

void av1_inv_txfm2d_add_32x16_neon(const tran_low_t *input, uint16_t *dest,
                                   int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_32X16, bd);
}

void av1_inv_txfm2d_add_32x32_neon(const tran_low_t *input, uint16_t *dest,
                                   int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_32X32, bd);
}

void av1_inv_txfm2d_add_64x64_neon(const tran_low_t *input, uint16_t *dest,
                                   int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_64X64, bd);
}

void av1_inv_txfm2d_add_32x64_neon(const tran_low_t *input, uint16_t *dest,
                                   int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_32X64, bd);
}

void av1_inv_txfm2d_add_64x32_neon(const tran_low_t *input, uint16_t *dest,
                                   int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_64X32, bd);
}

void av1_inv_txfm2d_add_64x16_neon(const tran_low_t *input, uint16_t *dest,
                                   int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_64X16, bd);
}

void av1_inv_txfm2d_add_16x64_neon(const tran_low_t *input, uint16_t *dest,
                                   int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_16X64, bd);
}

static void av1_inv_txfm2d_add_16x16_neon(const tran_low_t *input,
                                          uint16_t *dest, int stride,
                                          TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type,
                               TX_16X16, bd);
}

void av1_inv_txfm2d_add_32x8_neon(const tran_low_t *input, uint16_t *dest,
                                  int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type, TX_32X8,
                               bd);
}

void av1_inv_txfm2d_add_8x32_neon(const tran_low_t *input, uint16_t *dest,
                                  int stride, TX_TYPE tx_type, const int bd) {
  inv_txfm2d_add_universe_neon(input, (uint8_t *)dest, stride, tx_type, TX_8X32,
                               bd);
}

void av1_highbd_inv_txfm_add_neon(const tran_low_t *input, uint8_t *dest,
                                  int stride, const TxfmParam *txfm_param) {
  const TX_SIZE tx_size = txfm_param->tx_size;

  TX_TYPE tx_type = txfm_param->tx_type;
  int bd = txfm_param->bd;
  switch (tx_size) {
    case TX_8X8:
      highbd_inv_txfm_add_8x8_neon(input, dest, stride, txfm_param);
      break;
    case TX_4X8:
      av1_inv_txfm2d_add_4x8_neon(input, CONVERT_TO_SHORTPTR(dest), stride,
                                  txfm_param->tx_type, txfm_param->bd);
      break;
    case TX_8X4:
      av1_inv_txfm2d_add_8x4_neon(input, CONVERT_TO_SHORTPTR(dest), stride,
                                  txfm_param->tx_type, txfm_param->bd);
      break;
    case TX_4X4:
      highbd_inv_txfm_add_4x4_neon(input, dest, stride, txfm_param);
      break;
    case TX_16X4:
      av1_inv_txfm2d_add_16x4_neon(input, CONVERT_TO_SHORTPTR(dest), stride,
                                   txfm_param->tx_type, txfm_param->bd);
      break;
    case TX_4X16:
      av1_inv_txfm2d_add_4x16_neon(input, CONVERT_TO_SHORTPTR(dest), stride,
                                   txfm_param->tx_type, txfm_param->bd);
      break;
    case TX_8X16:
      av1_inv_txfm2d_add_8x16_neon(input, (uint16_t *)dest, stride, tx_type,
                                   bd);
      break;
    case TX_16X8:
      av1_inv_txfm2d_add_16x8_neon(input, (uint16_t *)dest, stride, tx_type,
                                   bd);
      break;
    case TX_16X32:
      av1_inv_txfm2d_add_16x32_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_32X16:
      av1_inv_txfm2d_add_32x16_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_16X16:
      av1_inv_txfm2d_add_16x16_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_32X32:
      av1_inv_txfm2d_add_32x32_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_64X64:
      av1_inv_txfm2d_add_64x64_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_32X64:
      av1_inv_txfm2d_add_32x64_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_64X32:
      av1_inv_txfm2d_add_64x32_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_16X64:
      av1_inv_txfm2d_add_16x64_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_64X16:
      av1_inv_txfm2d_add_64x16_neon(input, (uint16_t *)dest, stride, tx_type,
                                    bd);
      break;
    case TX_32X8:
      av1_inv_txfm2d_add_32x8_neon(input, (uint16_t *)dest, stride, tx_type,
                                   bd);
      break;
    case TX_8X32:
      av1_inv_txfm2d_add_8x32_neon(input, (uint16_t *)dest, stride, tx_type,
                                   bd);
      break;
  }
}
