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

#include "aom_dsp/txfm_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/av1_txfm.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#define custom_packs_s32(w0, w1) vcombine_s16(vqmovn_s32(w0), vqmovn_s32(w1))

static INLINE void transpose_16bit_4x4(const int16x8_t *const in,
                                       int16x8_t *const out) {
#if defined(__aarch64__)
  const int16x8_t a0 = vzip1q_s16(in[0], in[1]);
  const int16x8_t a1 = vzip1q_s16(in[2], in[3]);
#else
  int16x4x2_t temp;
  temp = vzip_s16(vget_low_s16(in[0]), vget_low_s16(in[1]));
  const int16x8_t a0 = vcombine_s16(temp.val[0], temp.val[1]);
  temp = vzip_s16(vget_low_s16(in[2]), vget_low_s16(in[3]));
  const int16x8_t a1 = vcombine_s16(temp.val[0], temp.val[1]);
#endif

  int32x4x2_t a01 =
      vzipq_s32(vreinterpretq_s32_s16(a0), vreinterpretq_s32_s16(a1));
  out[0] = vreinterpretq_s16_s32(a01.val[0]);
  out[1] = vextq_s16(vreinterpretq_s16_s32(a01.val[0]), out[1], 4);
  out[2] = vreinterpretq_s16_s32(a01.val[1]);
  out[3] = vextq_s16(vreinterpretq_s16_s32(a01.val[1]), out[3], 4);
}

static INLINE void transpose_16bit_4x8(const int16x8_t *const in,
                                       int16x8_t *const out) {
#if defined(__aarch64__)
  const int16x8_t a0 = vzip1q_s16(in[0], in[1]);
  const int16x8_t a1 = vzip1q_s16(in[2], in[3]);
  const int16x8_t a2 = vzip1q_s16(in[4], in[5]);
  const int16x8_t a3 = vzip1q_s16(in[6], in[7]);
#else
  int16x4x2_t temp;
  temp = vzip_s16(vget_low_s16(in[0]), vget_low_s16(in[1]));
  const int16x8_t a0 = vcombine_s16(temp.val[0], temp.val[1]);
  temp = vzip_s16(vget_low_s16(in[2]), vget_low_s16(in[3]));
  const int16x8_t a1 = vcombine_s16(temp.val[0], temp.val[1]);
  temp = vzip_s16(vget_low_s16(in[4]), vget_low_s16(in[5]));
  const int16x8_t a2 = vcombine_s16(temp.val[0], temp.val[1]);
  temp = vzip_s16(vget_low_s16(in[6]), vget_low_s16(in[7]));
  const int16x8_t a3 = vcombine_s16(temp.val[0], temp.val[1]);
#endif

  const int32x4x2_t b02 =
      vzipq_s32(vreinterpretq_s32_s16(a0), vreinterpretq_s32_s16(a1));
  const int32x4x2_t b13 =
      vzipq_s32(vreinterpretq_s32_s16(a2), vreinterpretq_s32_s16(a3));

#if defined(__aarch64__)
  out[0] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b02.val[0]),
                                            vreinterpretq_s64_s32(b13.val[0])));
  out[1] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b02.val[0]),
                                            vreinterpretq_s64_s32(b13.val[0])));
  out[2] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b02.val[1]),
                                            vreinterpretq_s64_s32(b13.val[1])));
  out[3] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b02.val[1]),
                                            vreinterpretq_s64_s32(b13.val[1])));
#else
  out[0] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b02.val[0], b02.val[0], 2), b13.val[0], 2));
  out[2] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b02.val[1], b02.val[1], 2), b13.val[1], 2));
  out[1] = vreinterpretq_s16_s32(
      vextq_s32(b02.val[0], vextq_s32(b13.val[0], b13.val[0], 2), 2));
  out[3] = vreinterpretq_s16_s32(
      vextq_s32(b02.val[1], vextq_s32(b13.val[1], b13.val[1], 2), 2));
#endif
}

static INLINE void transpose_16bit_8x4(const int16x8_t *const in,
                                       int16x8_t *const out) {
  const int16x8x2_t a04 = vzipq_s16(in[0], in[1]);
  const int16x8x2_t a15 = vzipq_s16(in[2], in[3]);

  const int32x4x2_t b01 = vzipq_s32(vreinterpretq_s32_s16(a04.val[0]),
                                    vreinterpretq_s32_s16(a15.val[0]));
  const int32x4x2_t b45 = vzipq_s32(vreinterpretq_s32_s16(a04.val[1]),
                                    vreinterpretq_s32_s16(a15.val[1]));

  const int32x4_t zeros = vdupq_n_s32(0);

#if defined(__aarch64__)
  out[0] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b01.val[0]),
                                            vreinterpretq_s64_s32(zeros)));
  out[1] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b01.val[0]),
                                            vreinterpretq_s64_s32(zeros)));
  out[2] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b01.val[1]),
                                            vreinterpretq_s64_s32(zeros)));
  out[3] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b01.val[1]),
                                            vreinterpretq_s64_s32(zeros)));
  out[4] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b45.val[0]),
                                            vreinterpretq_s64_s32(zeros)));
  out[5] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b45.val[0]),
                                            vreinterpretq_s64_s32(zeros)));
  out[6] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b45.val[1]),
                                            vreinterpretq_s64_s32(zeros)));
  out[7] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b45.val[1]),
                                            vreinterpretq_s64_s32(zeros)));
#else
  out[0] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b01.val[0], b01.val[0], 2), zeros, 2));
  out[1] = vreinterpretq_s16_s32(vextq_s32(b01.val[0], zeros, 2));
  out[2] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b01.val[1], b01.val[1], 2), zeros, 2));
  out[3] = vreinterpretq_s16_s32(vextq_s32(b01.val[1], zeros, 2));
  out[4] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b45.val[0], b45.val[0], 2), zeros, 2));
  out[5] = vreinterpretq_s16_s32(vextq_s32(b45.val[0], zeros, 2));
  out[6] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b45.val[1], b45.val[1], 2), zeros, 2));
  out[7] = vreinterpretq_s16_s32(vextq_s32(b45.val[1], zeros, 2));
#endif
}

static INLINE void transpose_16bit_8x8(const int16x8_t *const in,
                                       int16x8_t *const out) {
  const int16x8x2_t a04 = vzipq_s16(in[0], in[1]);
  const int16x8x2_t a15 = vzipq_s16(in[2], in[3]);
  const int16x8x2_t a26 = vzipq_s16(in[4], in[5]);
  const int16x8x2_t a37 = vzipq_s16(in[6], in[7]);

  const int32x4x2_t b04 = vzipq_s32(vreinterpretq_s32_s16(a04.val[0]),
                                    vreinterpretq_s32_s16(a15.val[0]));
  const int32x4x2_t b15 = vzipq_s32(vreinterpretq_s32_s16(a26.val[0]),
                                    vreinterpretq_s32_s16(a37.val[0]));
  const int32x4x2_t b26 = vzipq_s32(vreinterpretq_s32_s16(a04.val[1]),
                                    vreinterpretq_s32_s16(a15.val[1]));
  const int32x4x2_t b37 = vzipq_s32(vreinterpretq_s32_s16(a26.val[1]),
                                    vreinterpretq_s32_s16(a37.val[1]));

#if defined(__aarch64__)
  out[0] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b04.val[0]),
                                            vreinterpretq_s64_s32(b15.val[0])));
  out[1] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b04.val[0]),
                                            vreinterpretq_s64_s32(b15.val[0])));
  out[2] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b04.val[1]),
                                            vreinterpretq_s64_s32(b15.val[1])));
  out[3] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b04.val[1]),
                                            vreinterpretq_s64_s32(b15.val[1])));
  out[4] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b26.val[0]),
                                            vreinterpretq_s64_s32(b37.val[0])));
  out[5] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b26.val[0]),
                                            vreinterpretq_s64_s32(b37.val[0])));
  out[6] = vreinterpretq_s16_s64(vzip1q_s64(vreinterpretq_s64_s32(b26.val[1]),
                                            vreinterpretq_s64_s32(b37.val[1])));
  out[7] = vreinterpretq_s16_s64(vzip2q_s64(vreinterpretq_s64_s32(b26.val[1]),
                                            vreinterpretq_s64_s32(b37.val[1])));
#else
  out[0] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b04.val[0], b04.val[0], 2), b15.val[0], 2));
  out[1] = vreinterpretq_s16_s32(
      vextq_s32(b04.val[0], vextq_s32(b15.val[0], b15.val[0], 2), 2));
  out[2] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b04.val[1], b04.val[1], 2), b15.val[1], 2));
  out[3] = vreinterpretq_s16_s32(
      vextq_s32(b04.val[1], vextq_s32(b15.val[1], b15.val[1], 2), 2));
  out[4] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b26.val[0], b26.val[0], 2), b37.val[0], 2));
  out[5] = vreinterpretq_s16_s32(
      vextq_s32(b26.val[0], vextq_s32(b37.val[0], b37.val[0], 2), 2));
  out[6] = vreinterpretq_s16_s32(
      vextq_s32(vextq_s32(b26.val[1], b26.val[1], 2), b37.val[1], 2));
  out[7] = vreinterpretq_s16_s32(
      vextq_s32(b26.val[1], vextq_s32(b37.val[1], b37.val[1], 2), 2));
#endif
}

static INLINE void av1_round_shift_rect_array_32_neon(int32x4_t *input,
                                                      int32x4_t *output,
                                                      const int size) {
  int i;
  for (i = 0; i < size; i++) {
    output[i] = vrshrq_n_s32(vmulq_n_s32(vrshrq_n_s32(input[i], 2), NewSqrt2),
                             NewSqrt2Bits);
  }
}

static INLINE void av1_round_shift_array_32_neon(int32x4_t *input,
                                                 int32x4_t *output,
                                                 const int size) {
  int i;
  for (i = 0; i < size; i++) output[i] = vrshrq_n_s32(input[i], 2);
}

#define btf_32_neon(w0, w1, in0, in1, out0, out1, v_cos_bit) \
  do {                                                       \
    out0 = vmulq_n_s32(in0, w0);                             \
    out0 = vmlaq_n_s32(out0, in1, w1);                       \
    out0 = vrshlq_s32(out0, v_cos_bit);                      \
    out1 = vmulq_n_s32(in0, w1);                             \
    out1 = vmlsq_n_s32(out1, in1, w0);                       \
    out1 = vrshlq_s32(out1, v_cos_bit);                      \
  } while (0)

#define btf_32_type1_neon(w0, w1, in0, in1, out0, out1, v_cos_bit) \
  do {                                                             \
    btf_32_neon(w1, w0, in1, in0, out0, out1, v_cos_bit);          \
  } while (0)

#define btf_32_neon_mode0(w0, w1, in0, in1, out0, out1, v_cos_bit) \
  do {                                                             \
    out0 = vmulq_n_s32(in1, w1);                                   \
    out0 = vmlsq_n_s32(out0, in0, w0);                             \
    out0 = vrshlq_s32(out0, v_cos_bit);                            \
    out1 = vmulq_n_s32(in0, w1);                                   \
    out1 = vmlaq_n_s32(out1, in1, w0);                             \
    out1 = vrshlq_s32(out1, v_cos_bit);                            \
  } while (0)

#define btf_32_neon_mode01(w0, w1, in0, in1, out0, out1, v_cos_bit) \
  do {                                                              \
    out0 = vmulq_n_s32(in1, w1);                                    \
    out0 = vmlaq_n_s32(out0, in0, w0);                              \
    out0 = vrshlq_s32(vnegq_s32(out0), v_cos_bit);                  \
    out1 = vmulq_n_s32(in1, w0);                                    \
    out1 = vmlsq_n_s32(out1, in0, w1);                              \
    out1 = vrshlq_s32(out1, v_cos_bit);                             \
  } while (0)

static INLINE void flip_buf_neon(int16x8_t *in, int16x8_t *out, int size) {
  for (int i = 0; i < size; ++i) {
    out[size - i - 1] = in[i];
  }
}

static INLINE void store_16bit_to_32bit_w4(const int16x8_t a,
                                           int32_t *const b) {
  vst1q_s32(b, vmovl_s16(vget_low_s16(a)));
}

static INLINE void store_16bit_to_32bit(int16x8_t a, int32_t *b) {
  vst1q_s32(b, vmovl_s16(vget_low_s16(a)));
  vst1q_s32((b + 4), vmovl_s16(vget_high_s16(a)));
}

static INLINE void store_output_32bit_w8(int32_t *const out,
                                         const int32x4_t *const in1,
                                         const int32x4_t *const in2,
                                         const int stride, const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    vst1q_s32(out + stride * i, in1[i]);
    vst1q_s32(out + stride * i + 4, in2[i]);
  }
}

static INLINE void store_rect_16bit_to_32bit_w4(
    const int16x8_t a, int32_t *const b, const int16x4_t *v_newsqrt2,
    const int32x4_t *v_newsqrt2bits) {
  const int32x4_t b_lo =
      vrshlq_s32(vmull_s16(vget_low_s16(a), *v_newsqrt2), *v_newsqrt2bits);
  vst1q_s32(b, b_lo);
}

static INLINE void store_rect_16bit_to_32bit(const int16x8_t a,
                                             int32_t *const b,
                                             const int16x4_t *v_newsqrt2,
                                             const int32x4_t *v_newsqrt2bits) {
  const int32x4_t b_lo =
      vrshlq_s32(vmull_s16(vget_low_s16(a), *v_newsqrt2), *v_newsqrt2bits);
  const int32x4_t b_hi =
      vrshlq_s32(vmull_s16(vget_high_s16(a), *v_newsqrt2), *v_newsqrt2bits);
  vst1q_s32(b, b_lo);
  vst1q_s32((b + 4), b_hi);
}

static INLINE void load_buffer_16bit_to_16bit_w4(const int16_t *in,
                                                 const int stride,
                                                 int16x8_t *const out,
                                                 const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    // vld1q_dup_u64 is used rather than vld1q_lane_u64(lane=0) to avoid
    // -Wmaybe-uninitialized warnings with some versions of gcc. This assumes
    // the upper lane is unused or further modified after this call. The
    // latency should be similar between the two.
    out[i] = vreinterpretq_s16_u64(vld1q_dup_u64((uint64_t *)in));
    in += stride;
  }
}

static INLINE void load_buffer_16bit_to_16bit_w4_flip(const int16_t *in,
                                                      const int stride,
                                                      int16x8_t *const out,
                                                      const int out_size) {
  for (int i = out_size - 1; i >= 0; --i) {
    // vld1q_dup_u64 is used rather than vld1q_lane_u64(lane=0) to avoid
    // -Wmaybe-uninitialized warnings with some versions of gcc. This assumes
    // the upper lane is unused or further modified after this call. The
    // latency should be similar between the two.
    out[i] = vreinterpretq_s16_u64(vld1q_dup_u64((uint64_t *)in));
    in += stride;
  }
}

static INLINE void load_buffer_16bit_to_16bit(const int16_t *in, int stride,
                                              int16x8_t *out, int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[i] = vld1q_s16(in + i * stride);
  }
}

static INLINE void load_buffer_16bit_to_16bit_flip(const int16_t *in,
                                                   int stride, int16x8_t *out,
                                                   int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[out_size - i - 1] = vld1q_s16(in + i * stride);
  }
}

static INLINE void store_buffer_16bit_to_32bit_w4(const int16x8_t *const in,
                                                  int32_t *const out,
                                                  const int stride,
                                                  const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    store_16bit_to_32bit_w4(in[i], out + i * stride);
  }
}

static INLINE void store_buffer_16bit_to_32bit_w8(const int16x8_t *const in,
                                                  int32_t *const out,
                                                  const int stride,
                                                  const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    store_16bit_to_32bit(in[i], out + i * stride);
  }
}

static INLINE void store_rect_buffer_16bit_to_32bit_w4(
    const int16x8_t *const in, int32_t *const out, const int stride,
    const int out_size) {
  const int16x4_t v_newsqrt2 = vdup_n_s16(NewSqrt2);
  const int32x4_t v_newsqrt2bits = vdupq_n_s32(-NewSqrt2Bits);
  for (int i = 0; i < out_size; ++i) {
    store_rect_16bit_to_32bit_w4(in[i], out + i * stride, &v_newsqrt2,
                                 &v_newsqrt2bits);
  }
}

static INLINE void store_rect_buffer_16bit_to_32bit_w8(
    const int16x8_t *const in, int32_t *const out, const int stride,
    const int out_size) {
  const int16x4_t v_newsqrt2 = vdup_n_s16(NewSqrt2);
  const int32x4_t v_newsqrt2bits = vdupq_n_s32(-NewSqrt2Bits);
  for (int i = 0; i < out_size; ++i) {
    store_rect_16bit_to_32bit(in[i], out + i * stride, &v_newsqrt2,
                              &v_newsqrt2bits);
  }
}

static INLINE void round_shift_16bit(int16x8_t *in, int size, int bit) {
  const int16x8_t vbit = vdupq_n_s16(bit);
  for (int i = 0; i < size; ++i) {
    in[i] = vrshlq_s16(in[i], vbit);
  }
}

static INLINE void round_shift_16bit_vector(int16x8_t *in, int size,
                                            const int16x8_t *v_bit) {
  for (int i = 0; i < size; ++i) {
    in[i] = vrshlq_s16(in[i], *v_bit);
  }
}

void av1_fadst4x4_neon(const int16x8_t *input, int16x8_t *output,
                       int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *sinpi = sinpi_arr(cos_bit);

  int32x4_t u[6], v[6];

  u[0] = vmovl_s16(vget_low_s16(input[0]));
  u[1] = vmovl_s16(vget_low_s16(input[1]));
  u[2] = vmovl_s16(vget_low_s16(input[2]));
  u[3] = vmovl_s16(vget_low_s16(input[3]));
  u[4] = vaddq_s32(u[0], u[1]);
  v[5] = vmulq_n_s32(u[2], sinpi[3]);
  v[0] = vmulq_n_s32(u[1], sinpi[2]);
  v[0] = vmlaq_n_s32(v[0], u[0], sinpi[1]);
  v[1] = vmlaq_n_s32(v[5], u[3], sinpi[4]);
  v[2] = vmulq_n_s32(u[4], sinpi[3]);
  v[3] = vmulq_n_s32(u[0], sinpi[4]);
  v[3] = vmlsq_n_s32(v[3], u[1], sinpi[1]);
  v[4] = vmlsq_n_s32(v[5], u[3], sinpi[2]);

  u[0] = vaddq_s32(v[0], v[1]);
  u[1] = vmlsq_n_s32(v[2], u[3], sinpi[3]);
  u[2] = vsubq_s32(v[3], v[4]);
  u[3] = vsubq_s32(u[2], u[0]);
  u[5] = vmlaq_n_s32(u[3], v[5], 3);

  int32x4_t vshift = vdupq_n_s32(-cos_bit);
  u[0] = vrshlq_s32(u[0], vshift);
  u[1] = vrshlq_s32(u[1], vshift);
  u[2] = vrshlq_s32(u[2], vshift);
  u[3] = vrshlq_s32(u[5], vshift);

  output[0] = custom_packs_s32(u[0], u[2]);

  output[1] = custom_packs_s32(u[1], u[3]);
  output[2] = vextq_s16(output[0], output[0], 4);
  output[3] = vextq_s16(output[1], output[1], 4);
}

#define btf_16_w4_neon(w0_l, w0_h, w1_l, w1_h, in0, in1, out0, out1, \
                       v_cos_bit)                                    \
  do {                                                               \
    int32x4_t in0_l = vmovl_s16(vget_low_s16(in0));                  \
    int32x4_t in1_l = vmovl_s16(vget_low_s16(in1));                  \
    int32x4_t u0 = vmulq_n_s32(in0_l, w0_l);                         \
    u0 = vmlaq_n_s32(u0, in1_l, w0_h);                               \
    int32x4_t v0 = vmulq_n_s32(in0_l, w1_l);                         \
    v0 = vmlaq_n_s32(v0, in1_l, w1_h);                               \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                        \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                        \
    const int16x4_t c1 = vqmovn_s32(c0);                             \
    const int16x4_t d1 = vqmovn_s32(d0);                             \
    out0 = vcombine_s16(c1, c1);                                     \
    out1 = vcombine_s16(d1, c1);                                     \
  } while (0)

#define btf_16_w4_neon_mode0(w0_l, w0_h, in0, in1, out0, out1, v_cos_bit) \
  do {                                                                    \
    int32x4_t in0_l = vmovl_s16(vget_low_s16(in0));                       \
    int32x4_t in1_l = vmovl_s16(vget_low_s16(in1));                       \
    int32x4_t u0 = vmulq_n_s32(in1_l, w0_h);                              \
    u0 = vmlsq_n_s32(u0, in0_l, w0_l);                                    \
    int32x4_t v0 = vmulq_n_s32(in0_l, w0_h);                              \
    v0 = vmlaq_n_s32(v0, in1_l, w0_l);                                    \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                             \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                             \
    const int16x4_t c1 = vqmovn_s32(c0);                                  \
    const int16x4_t d1 = vqmovn_s32(d0);                                  \
    out0 = vcombine_s16(c1, c1);                                          \
    out1 = vcombine_s16(d1, c1);                                          \
  } while (0)

#define btf_16_w4_neon_mode2(w0_l, w0_h, in0, in1, out0, out1, v_cos_bit) \
  do {                                                                    \
    int32x4_t in0_l = vmovl_s16(vget_low_s16(in0));                       \
    int32x4_t in1_l = vmovl_s16(vget_low_s16(in1));                       \
    int32x4_t u0 = vmulq_n_s32(in0_l, w0_l);                              \
    u0 = vmlaq_n_s32(u0, in1_l, w0_h);                                    \
    int32x4_t v0 = vmulq_n_s32(in1_l, w0_l);                              \
    v0 = vmlsq_n_s32(v0, in0_l, w0_h);                                    \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                             \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                             \
    const int16x4_t c1 = vqmovn_s32(c0);                                  \
    const int16x4_t d1 = vqmovn_s32(d0);                                  \
    out0 = vcombine_s16(c1, c1);                                          \
    out1 = vcombine_s16(d1, c1);                                          \
  } while (0)

#define btf_16_w4_neon_mode3(w0_l, w0_h, in0, in1, out0, out1, v_cos_bit) \
  do {                                                                    \
    int32x4_t in0_l = vmovl_s16(vget_low_s16(in0));                       \
    int32x4_t in1_l = vmovl_s16(vget_low_s16(in1));                       \
    int32x4_t u0 = vmulq_n_s32(in0_l, w0_l);                              \
    u0 = vmlaq_n_s32(u0, in1_l, w0_h);                                    \
    int32x4_t v0 = vmulq_n_s32(in0_l, w0_h);                              \
    v0 = vmlsq_n_s32(v0, in1_l, w0_l);                                    \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                             \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                             \
    const int16x4_t c1 = vqmovn_s32(c0);                                  \
    const int16x4_t d1 = vqmovn_s32(d0);                                  \
    out0 = vcombine_s16(c1, c1);                                          \
    out1 = vcombine_s16(d1, c1);                                          \
  } while (0)

static void fadst4x8_neon(const int16x8_t *input, int16x8_t *output,
                          int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1-2
  int16x8_t x2[8];
  btf_16_w4_neon_mode3(cospi[32], cospi[32], vqnegq_s16(input[3]), input[4],
                       x2[2], x2[3], v_cos_bit);
  btf_16_w4_neon_mode3(cospi[32], cospi[32], input[2], vqnegq_s16(input[5]),
                       x2[6], x2[7], v_cos_bit);

  // stage 3
  int16x8_t x3[8];
  x3[0] = vqaddq_s16(input[0], x2[2]);
  x3[2] = vqsubq_s16(input[0], x2[2]);
  x3[1] = vqsubq_s16(x2[3], input[7]);
  x3[3] = vqsubq_s16(vqnegq_s16(input[7]), x2[3]);
  x3[4] = vqaddq_s16(vqnegq_s16(input[1]), x2[6]);
  x3[6] = vqsubq_s16(vqnegq_s16(input[1]), x2[6]);
  x3[5] = vqaddq_s16(input[6], x2[7]);
  x3[7] = vqsubq_s16(input[6], x2[7]);

  // stage 4
  int16x8_t x4[8];

  btf_16_w4_neon_mode3(cospi[16], cospi[48], x3[4], x3[5], x4[4], x4[5],
                       v_cos_bit);
  btf_16_w4_neon_mode0(cospi[48], cospi[16], x3[6], x3[7], x4[6], x4[7],
                       v_cos_bit);

  // stage 5
  int16x8_t x5[8];
  x5[0] = vqaddq_s16(x3[0], x4[4]);
  x5[4] = vqsubq_s16(x3[0], x4[4]);
  x5[1] = vqaddq_s16(x3[1], x4[5]);
  x5[5] = vqsubq_s16(x3[1], x4[5]);
  x5[2] = vqaddq_s16(x3[2], x4[6]);
  x5[6] = vqsubq_s16(x3[2], x4[6]);
  x5[3] = vqaddq_s16(x3[3], x4[7]);
  x5[7] = vqsubq_s16(x3[3], x4[7]);

  // stage 6-7
  btf_16_w4_neon_mode3(cospi[4], cospi[60], x5[0], x5[1], output[7], output[0],
                       v_cos_bit);
  btf_16_w4_neon_mode3(cospi[20], cospi[44], x5[2], x5[3], output[5], output[2],
                       v_cos_bit);
  btf_16_w4_neon_mode3(cospi[36], cospi[28], x5[4], x5[5], output[3], output[4],
                       v_cos_bit);
  btf_16_w4_neon_mode3(cospi[52], cospi[12], x5[6], x5[7], output[1], output[6],
                       v_cos_bit);
}

static void fadst8x4_neon(const int16x8_t *input, int16x8_t *output,
                          int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *sinpi = sinpi_arr(cos_bit);

  const int16x8_t in7 = vaddq_s16(input[0], input[1]);
  int32x4_t u_lo[8], u_hi[8], v_hi[8];

  int32x4_t in0_l = vmovl_s16(vget_low_s16(input[0]));
  int32x4_t in0_h = vmovl_s16(vget_high_s16(input[0]));
  int32x4_t in1_l = vmovl_s16(vget_low_s16(input[1]));
  int32x4_t in1_h = vmovl_s16(vget_high_s16(input[1]));
  int32x4_t in2_l = vmovl_s16(vget_low_s16(input[2]));
  int32x4_t in2_h = vmovl_s16(vget_high_s16(input[2]));
  int32x4_t in3_l = vmovl_s16(vget_low_s16(input[3]));
  int32x4_t in3_h = vmovl_s16(vget_high_s16(input[3]));
  int32x4_t in7_l = vmovl_s16(vget_low_s16(in7));
  int32x4_t in7_h = vmovl_s16(vget_high_s16(in7));

  u_lo[0] = vmulq_n_s32(in1_l, sinpi[2]);
  u_lo[0] = vmlaq_n_s32(u_lo[0], in0_l, sinpi[1]);

  u_hi[0] = vmulq_n_s32(in1_h, sinpi[2]);
  u_hi[0] = vmlaq_n_s32(u_hi[0], in0_h, sinpi[1]);

  u_lo[0] = vmlaq_n_s32(u_lo[0], in3_l, sinpi[4]);
  u_lo[0] = vmlaq_n_s32(u_lo[0], in2_l, sinpi[3]);

  u_hi[0] = vmlaq_n_s32(u_hi[0], in3_h, sinpi[4]);
  u_hi[0] = vmlaq_n_s32(u_hi[0], in2_h, sinpi[3]);

  u_lo[1] = vmulq_n_s32(in7_l, sinpi[3]);

  v_hi[2] = vmulq_n_s32(in7_h, sinpi[3]);
  u_lo[2] = vmulq_n_s32(in0_l, sinpi[4]);
  u_lo[2] = vmlsq_n_s32(u_lo[2], in1_l, sinpi[1]);

  u_hi[2] = vmulq_n_s32(in0_h, sinpi[4]);
  u_hi[2] = vmlsq_n_s32(u_hi[2], in1_h, sinpi[1]);

  u_lo[2] = vmlaq_n_s32(u_lo[2], in3_l, sinpi[2]);
  u_lo[2] = vmlsq_n_s32(u_lo[2], in2_l, sinpi[3]);

  u_hi[2] = vmlaq_n_s32(u_hi[2], in3_h, sinpi[2]);
  u_hi[2] = vmlsq_n_s32(u_hi[2], in2_h, sinpi[3]);

  u_lo[1] = vmlsq_n_s32(u_lo[1], in3_l, sinpi[3]);

  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  u_hi[1] = vmlsq_n_s32(v_hi[2], in3_h, sinpi[3]);

  u_lo[3] = vsubq_s32(u_lo[2], u_lo[0]);
  u_hi[3] = vsubq_s32(u_hi[2], u_hi[0]);

  u_lo[6] = vmlaq_n_s32(u_lo[3], in2_l, sinpi[3] * 3);
  u_hi[6] = vmlaq_n_s32(u_hi[3], in2_h, sinpi[3] * 3);

  u_lo[0] = vrshlq_s32(u_lo[0], v_cos_bit);
  u_hi[0] = vrshlq_s32(u_hi[0], v_cos_bit);
  u_lo[1] = vrshlq_s32(u_lo[1], v_cos_bit);
  u_hi[1] = vrshlq_s32(u_hi[1], v_cos_bit);
  u_lo[2] = vrshlq_s32(u_lo[2], v_cos_bit);
  u_hi[2] = vrshlq_s32(u_hi[2], v_cos_bit);
  u_lo[3] = vrshlq_s32(u_lo[6], v_cos_bit);
  u_hi[3] = vrshlq_s32(u_hi[6], v_cos_bit);

  output[0] = custom_packs_s32(u_lo[0], u_hi[0]);
  output[1] = custom_packs_s32(u_lo[1], u_hi[1]);
  output[2] = custom_packs_s32(u_lo[2], u_hi[2]);
  output[3] = custom_packs_s32(u_lo[3], u_hi[3]);
}

void av1_fdct4x4_neon(const int16x8_t *input, int16x8_t *output, int8_t cos_bit,
                      const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  int32x4_t u[4];

  int32x4_t in12a = vaddl_s16(vget_low_s16(input[1]), vget_low_s16(input[2]));
  int32x4_t in12s = vsubl_s16(vget_low_s16(input[1]), vget_low_s16(input[2]));
  int32x4_t in03a = vaddl_s16(vget_low_s16(input[0]), vget_low_s16(input[3]));
  int32x4_t in03s = vsubl_s16(vget_low_s16(input[0]), vget_low_s16(input[3]));

  int32x4_t u0ad1 = vmulq_n_s32(in12a, cospi[32]);
  int32x4_t u0ad2 = vmulq_n_s32(in03a, cospi[32]);
  u[0] = vaddq_s32(u0ad1, u0ad2);
  u[1] = vsubq_s32(u0ad2, u0ad1);
  u[2] = vmulq_n_s32(in12s, cospi[48]);
  u[2] = vmlaq_n_s32(u[2], in03s, cospi[16]);

  u[3] = vmulq_n_s32(in03s, cospi[48]);
  u[3] = vmlsq_n_s32(u[3], in12s, cospi[16]);

  u[0] = vrshlq_s32(u[0], v_cos_bit);
  u[1] = vrshlq_s32(u[1], v_cos_bit);
  u[2] = vrshlq_s32(u[2], v_cos_bit);
  u[3] = vrshlq_s32(u[3], v_cos_bit);

  output[0] = custom_packs_s32(u[0], u[1]);
  output[1] = custom_packs_s32(u[2], u[3]);
  output[2] = vextq_s16(output[0], output[0], 4);
  output[3] = vextq_s16(output[1], output[1], 4);
}

#define btf_16_neon(w0_l, w0_h, w1_l, w1_h, in0, in1, out0, out1) \
  do {                                                            \
    int32x4_t in_low0 = vmovl_s16(vget_low_s16(in0));             \
    int32x4_t in_high0 = vmovl_s16(vget_high_s16(in0));           \
    int32x4_t in_low1 = vmovl_s16(vget_low_s16(in1));             \
    int32x4_t in_high1 = vmovl_s16(vget_high_s16(in1));           \
    int32x4_t u0 = vmulq_n_s32(in_low1, w0_h);                    \
    u0 = vmlaq_n_s32(u0, in_low0, w0_l);                          \
    int32x4_t u1 = vmulq_n_s32(in_high1, w0_h);                   \
    u1 = vmlaq_n_s32(u1, in_high0, w0_l);                         \
    int32x4_t v0 = vmulq_n_s32(in_low1, w1_h);                    \
    v0 = vmlaq_n_s32(v0, in_low0, w1_l);                          \
    int32x4_t v1 = vmulq_n_s32(in_high1, w1_h);                   \
    v1 = vmlaq_n_s32(v1, in_high0, w1_l);                         \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                     \
    int32x4_t c1 = vrshlq_s32(u1, v_cos_bit);                     \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                     \
    int32x4_t d1 = vrshlq_s32(v1, v_cos_bit);                     \
    out0 = custom_packs_s32(c0, c1);                              \
    out1 = custom_packs_s32(d0, d1);                              \
  } while (0)

#define btf_16_neon_mode0(w0_l, w0_h, in0, in1, out0, out1, v_cos_bit) \
  do {                                                                 \
    int32x4_t in_low0 = vmovl_s16(vget_low_s16(in0));                  \
    int32x4_t in_high0 = vmovl_s16(vget_high_s16(in0));                \
    int32x4_t in_low1 = vmovl_s16(vget_low_s16(in1));                  \
    int32x4_t in_high1 = vmovl_s16(vget_high_s16(in1));                \
    int32x4_t u0 = vmulq_n_s32(in_low1, w0_h);                         \
    u0 = vmlsq_n_s32(u0, in_low0, w0_l);                               \
    int32x4_t u1 = vmulq_n_s32(in_high1, w0_h);                        \
    u1 = vmlsq_n_s32(u1, in_high0, w0_l);                              \
    int32x4_t v0 = vmulq_n_s32(in_low1, w0_l);                         \
    v0 = vmlaq_n_s32(v0, in_low0, w0_h);                               \
    int32x4_t v1 = vmulq_n_s32(in_high1, w0_l);                        \
    v1 = vmlaq_n_s32(v1, in_high0, w0_h);                              \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                          \
    int32x4_t c1 = vrshlq_s32(u1, v_cos_bit);                          \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                          \
    int32x4_t d1 = vrshlq_s32(v1, v_cos_bit);                          \
    out0 = custom_packs_s32(c0, c1);                                   \
    out1 = custom_packs_s32(d0, d1);                                   \
  } while (0)

#define btf_16_neon_mode1(w0_l, w0_h, in0, in1, out0, out1, v_cos_bit) \
  do {                                                                 \
    int32x4_t in_low0 = vmovl_s16(vget_low_s16(in0));                  \
    int32x4_t in_high0 = vmovl_s16(vget_high_s16(in0));                \
    int32x4_t in_low1 = vmovl_s16(vget_low_s16(in1));                  \
    int32x4_t in_high1 = vmovl_s16(vget_high_s16(in1));                \
    int32x4_t u0 = vmulq_n_s32(in_low0, w0_l);                         \
    u0 = vmlsq_n_s32(u0, in_low1, w0_h);                               \
    int32x4_t u1 = vmulq_n_s32(in_high0, w0_l);                        \
    u1 = vmlsq_n_s32(u1, in_high1, w0_h);                              \
    int32x4_t v0 = vmulq_n_s32(in_low1, w0_l);                         \
    v0 = vmlaq_n_s32(v0, in_low0, w0_h);                               \
    int32x4_t v1 = vmulq_n_s32(in_high1, w0_l);                        \
    v1 = vmlaq_n_s32(v1, in_high0, w0_h);                              \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                          \
    int32x4_t c1 = vrshlq_s32(u1, v_cos_bit);                          \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                          \
    int32x4_t d1 = vrshlq_s32(v1, v_cos_bit);                          \
    out0 = custom_packs_s32(c0, c1);                                   \
    out1 = custom_packs_s32(d0, d1);                                   \
  } while (0)

#define btf_16_neon_mode02(w0_l, w0_h, in0, in1, out0, out1, v_cos_bit) \
  do {                                                                  \
    int32x4_t in_low0 = vmovl_s16(vget_low_s16(in0));                   \
    int32x4_t in_high0 = vmovl_s16(vget_high_s16(in0));                 \
    int32x4_t in_low1 = vmovl_s16(vget_low_s16(in1));                   \
    int32x4_t in_high1 = vmovl_s16(vget_high_s16(in1));                 \
    int32x4_t u0 = vmulq_n_s32(in_low1, -w0_h);                         \
    u0 = vmlsq_n_s32(u0, in_low0, w0_l);                                \
    int32x4_t u1 = vmulq_n_s32(in_high1, -w0_h);                        \
    u1 = vmlsq_n_s32(u1, in_high0, w0_l);                               \
    int32x4_t v0 = vmulq_n_s32(in_low1, w0_l);                          \
    v0 = vmlsq_n_s32(v0, in_low0, w0_h);                                \
    int32x4_t v1 = vmulq_n_s32(in_high1, w0_l);                         \
    v1 = vmlsq_n_s32(v1, in_high0, w0_h);                               \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                           \
    int32x4_t c1 = vrshlq_s32(u1, v_cos_bit);                           \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                           \
    int32x4_t d1 = vrshlq_s32(v1, v_cos_bit);                           \
    out0 = custom_packs_s32(c0, c1);                                    \
    out1 = custom_packs_s32(d0, d1);                                    \
  } while (0)

#define btf_16_neon_mode2(w0_l, w0_h, in0, in1, out0, out1, v_cos_bit) \
  do {                                                                 \
    int32x4_t in_low0 = vmovl_s16(vget_low_s16(in0));                  \
    int32x4_t in_high0 = vmovl_s16(vget_high_s16(in0));                \
    int32x4_t in_low1 = vmovl_s16(vget_low_s16(in1));                  \
    int32x4_t in_high1 = vmovl_s16(vget_high_s16(in1));                \
    int32x4_t u0 = vmulq_n_s32(in_low1, w0_h);                         \
    u0 = vmlaq_n_s32(u0, in_low0, w0_l);                               \
    int32x4_t u1 = vmulq_n_s32(in_high1, w0_h);                        \
    u1 = vmlaq_n_s32(u1, in_high0, w0_l);                              \
    int32x4_t v0 = vmulq_n_s32(in_low1, w0_l);                         \
    v0 = vmlsq_n_s32(v0, in_low0, w0_h);                               \
    int32x4_t v1 = vmulq_n_s32(in_high1, w0_l);                        \
    v1 = vmlsq_n_s32(v1, in_high0, w0_h);                              \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                          \
    int32x4_t c1 = vrshlq_s32(u1, v_cos_bit);                          \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                          \
    int32x4_t d1 = vrshlq_s32(v1, v_cos_bit);                          \
    out0 = custom_packs_s32(c0, c1);                                   \
    out1 = custom_packs_s32(d0, d1);                                   \
  } while (0)

#define btf_16_neon_mode3(w0_l, w0_h, in0, in1, out0, out1, v_cos_bit) \
  do {                                                                 \
    int32x4_t in_low0 = vmovl_s16(vget_low_s16(in0));                  \
    int32x4_t in_high0 = vmovl_s16(vget_high_s16(in0));                \
    int32x4_t in_low1 = vmovl_s16(vget_low_s16(in1));                  \
    int32x4_t in_high1 = vmovl_s16(vget_high_s16(in1));                \
    int32x4_t u0 = vmulq_n_s32(in_low1, w0_h);                         \
    u0 = vmlaq_n_s32(u0, in_low0, w0_l);                               \
    int32x4_t u1 = vmulq_n_s32(in_high1, w0_h);                        \
    u1 = vmlaq_n_s32(u1, in_high0, w0_l);                              \
    int32x4_t v0 = vmulq_n_s32(in_low0, w0_h);                         \
    v0 = vmlsq_n_s32(v0, in_low1, w0_l);                               \
    int32x4_t v1 = vmulq_n_s32(in_high0, w0_h);                        \
    v1 = vmlsq_n_s32(v1, in_high1, w0_l);                              \
    int32x4_t c0 = vrshlq_s32(u0, v_cos_bit);                          \
    int32x4_t c1 = vrshlq_s32(u1, v_cos_bit);                          \
    int32x4_t d0 = vrshlq_s32(v0, v_cos_bit);                          \
    int32x4_t d1 = vrshlq_s32(v1, v_cos_bit);                          \
    out0 = custom_packs_s32(c0, c1);                                   \
    out1 = custom_packs_s32(d0, d1);                                   \
  } while (0)

static void fdct8x4_neon(const int16x8_t *input, int16x8_t *output,
                         int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1
  int16x8_t x1[4];
  x1[0] = vqaddq_s16(input[0], input[3]);
  x1[3] = vqsubq_s16(input[0], input[3]);
  x1[1] = vqaddq_s16(input[1], input[2]);
  x1[2] = vqsubq_s16(input[1], input[2]);

  // stage 2
  int16x8_t x2[4];
  btf_16_neon_mode3(cospi[32], cospi[32], x1[0], x1[1], x2[0], x2[1],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[48], cospi[16], x1[2], x1[3], x2[2], x2[3],
                    v_cos_bit);

  // stage 3
  output[0] = x2[0];
  output[1] = x2[2];
  output[2] = x2[1];
  output[3] = x2[3];
}

static void fdct4x8_neon(const int16x8_t *input, int16x8_t *output,
                         int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1
  int16x8_t x1[8];
  x1[0] = vqaddq_s16(input[0], input[7]);
  x1[7] = vqsubq_s16(input[0], input[7]);
  x1[1] = vqaddq_s16(input[1], input[6]);
  x1[6] = vqsubq_s16(input[1], input[6]);
  x1[2] = vqaddq_s16(input[2], input[5]);
  x1[5] = vqsubq_s16(input[2], input[5]);
  x1[3] = vqaddq_s16(input[3], input[4]);
  x1[4] = vqsubq_s16(input[3], input[4]);

  // stage 2
  int16x8_t x2[8];
  x2[0] = vqaddq_s16(x1[0], x1[3]);
  x2[3] = vqsubq_s16(x1[0], x1[3]);
  x2[1] = vqaddq_s16(x1[1], x1[2]);
  x2[2] = vqsubq_s16(x1[1], x1[2]);

  btf_16_w4_neon_mode0(cospi[32], cospi[32], x1[5], x1[6], x2[5], x2[6],
                       v_cos_bit);

  // stage 3
  int16x8_t x3[8];
  btf_16_w4_neon_mode3(cospi[32], cospi[32], x2[0], x2[1], output[0], output[4],
                       v_cos_bit);

  btf_16_w4_neon_mode2(cospi[48], cospi[16], x2[2], x2[3], output[2], output[6],
                       v_cos_bit);
  x3[4] = vqaddq_s16(x1[4], x2[5]);
  x3[5] = vqsubq_s16(x1[4], x2[5]);
  x3[6] = vqsubq_s16(x1[7], x2[6]);
  x3[7] = vqaddq_s16(x1[7], x2[6]);

  // stage 4-5
  btf_16_w4_neon_mode2(cospi[56], cospi[8], x3[4], x3[7], output[1], output[7],
                       v_cos_bit);
  btf_16_w4_neon_mode2(cospi[24], cospi[40], x3[5], x3[6], output[5], output[3],
                       v_cos_bit);
}

void fdct8x8_neon(const int16x8_t *input, int16x8_t *output, int8_t cos_bit,
                  const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1
  int16x8_t x1[8];
  x1[0] = vqaddq_s16(input[0], input[7]);
  x1[7] = vqsubq_s16(input[0], input[7]);
  x1[1] = vqaddq_s16(input[1], input[6]);
  x1[6] = vqsubq_s16(input[1], input[6]);
  x1[2] = vqaddq_s16(input[2], input[5]);
  x1[5] = vqsubq_s16(input[2], input[5]);
  x1[3] = vqaddq_s16(input[3], input[4]);
  x1[4] = vqsubq_s16(input[3], input[4]);

  // stage 2
  int16x8_t x2[8];
  x2[0] = vqaddq_s16(x1[0], x1[3]);
  x2[3] = vqsubq_s16(x1[0], x1[3]);
  x2[1] = vqaddq_s16(x1[1], x1[2]);
  x2[2] = vqsubq_s16(x1[1], x1[2]);
  btf_16_neon_mode0(cospi[32], cospi[32], x1[5], x1[6], x2[5], x2[6],
                    v_cos_bit);

  // stage 3
  int16x8_t x3[8];
  btf_16_neon_mode3(cospi[32], cospi[32], x2[0], x2[1], output[0], output[4],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[48], cospi[16], x2[2], x2[3], output[2], output[6],
                    v_cos_bit);
  x3[4] = vqaddq_s16(x1[4], x2[5]);
  x3[5] = vqsubq_s16(x1[4], x2[5]);
  x3[6] = vqsubq_s16(x1[7], x2[6]);
  x3[7] = vqaddq_s16(x1[7], x2[6]);

  // stage 4-5
  btf_16_neon_mode2(cospi[56], cospi[8], x3[4], x3[7], output[1], output[7],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[24], cospi[40], x3[5], x3[6], output[5], output[3],
                    v_cos_bit);
}

static void fdct8x16_neon(const int16x8_t *input, int16x8_t *output,
                          int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1
  int16x8_t x1[16];
  x1[0] = vqaddq_s16(input[0], input[15]);
  x1[15] = vqsubq_s16(input[0], input[15]);
  x1[1] = vqaddq_s16(input[1], input[14]);
  x1[14] = vqsubq_s16(input[1], input[14]);
  x1[2] = vqaddq_s16(input[2], input[13]);
  x1[13] = vqsubq_s16(input[2], input[13]);
  x1[3] = vqaddq_s16(input[3], input[12]);
  x1[12] = vqsubq_s16(input[3], input[12]);
  x1[4] = vqaddq_s16(input[4], input[11]);
  x1[11] = vqsubq_s16(input[4], input[11]);
  x1[5] = vqaddq_s16(input[5], input[10]);
  x1[10] = vqsubq_s16(input[5], input[10]);
  x1[6] = vqaddq_s16(input[6], input[9]);
  x1[9] = vqsubq_s16(input[6], input[9]);
  x1[7] = vqaddq_s16(input[7], input[8]);
  x1[8] = vqsubq_s16(input[7], input[8]);

  // stage 2
  int16x8_t x2[16];
  x2[0] = vqaddq_s16(x1[0], x1[7]);
  x2[7] = vqsubq_s16(x1[0], x1[7]);
  x2[1] = vqaddq_s16(x1[1], x1[6]);
  x2[6] = vqsubq_s16(x1[1], x1[6]);
  x2[2] = vqaddq_s16(x1[2], x1[5]);
  x2[5] = vqsubq_s16(x1[2], x1[5]);
  x2[3] = vqaddq_s16(x1[3], x1[4]);
  x2[4] = vqsubq_s16(x1[3], x1[4]);

  btf_16_neon_mode0(cospi[32], cospi[32], x1[10], x1[13], x2[10], x2[13],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[32], cospi[32], x1[11], x1[12], x2[11], x2[12],
                    v_cos_bit);

  // stage 3
  int16x8_t x3[16];
  x3[0] = vqaddq_s16(x2[0], x2[3]);
  x3[3] = vqsubq_s16(x2[0], x2[3]);
  x3[1] = vqaddq_s16(x2[1], x2[2]);
  x3[2] = vqsubq_s16(x2[1], x2[2]);

  btf_16_neon_mode0(cospi[32], cospi[32], x2[5], x2[6], x3[5], x3[6],
                    v_cos_bit);

  x3[8] = vqaddq_s16(x1[8], x2[11]);
  x3[11] = vqsubq_s16(x1[8], x2[11]);
  x3[9] = vqaddq_s16(x1[9], x2[10]);
  x3[10] = vqsubq_s16(x1[9], x2[10]);
  x3[12] = vqsubq_s16(x1[15], x2[12]);
  x3[15] = vqaddq_s16(x1[15], x2[12]);
  x3[13] = vqsubq_s16(x1[14], x2[13]);
  x3[14] = vqaddq_s16(x1[14], x2[13]);

  // stage 4
  int16x8_t x4[16];
  btf_16_neon(cospi[32], cospi[32], cospi[32], -cospi[32], x3[0], x3[1],
              output[0], output[8]);
  btf_16_neon(cospi[48], cospi[16], -cospi[16], cospi[48], x3[2], x3[3],
              output[4], output[12]);
  x4[4] = vqaddq_s16(x2[4], x3[5]);
  x4[5] = vqsubq_s16(x2[4], x3[5]);
  x4[6] = vqsubq_s16(x2[7], x3[6]);
  x4[7] = vqaddq_s16(x2[7], x3[6]);
  btf_16_neon_mode0(cospi[16], cospi[48], x3[9], x3[14], x4[9], x4[14],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x3[10], x3[13], x4[10], x4[13],
                     v_cos_bit);

  // stage 5
  int16x8_t x5[16];

  btf_16_neon_mode2(cospi[56], cospi[8], x4[4], x4[7], output[2], output[14],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[24], cospi[40], x4[5], x4[6], output[10], output[6],
                    v_cos_bit);
  x5[8] = vqaddq_s16(x3[8], x4[9]);
  x5[9] = vqsubq_s16(x3[8], x4[9]);
  x5[10] = vqsubq_s16(x3[11], x4[10]);
  x5[11] = vqaddq_s16(x3[11], x4[10]);
  x5[12] = vqaddq_s16(x3[12], x4[13]);
  x5[13] = vqsubq_s16(x3[12], x4[13]);
  x5[14] = vqsubq_s16(x3[15], x4[14]);
  x5[15] = vqaddq_s16(x3[15], x4[14]);

  // stage 6-7
  btf_16_neon_mode2(cospi[60], cospi[4], x5[8], x5[15], output[1], output[15],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[28], cospi[36], x5[9], x5[14], output[9], output[7],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[44], cospi[20], x5[10], x5[13], output[5], output[11],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[12], cospi[52], x5[11], x5[12], output[13], output[3],
                    v_cos_bit);
}

void av1_fdct8x32_neon(const int16x8_t *input, int16x8_t *output,
                       int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1
  int16x8_t x1[32];
  x1[0] = vqaddq_s16(input[0], input[31]);
  x1[31] = vqsubq_s16(input[0], input[31]);
  x1[1] = vqaddq_s16(input[1], input[30]);
  x1[30] = vqsubq_s16(input[1], input[30]);
  x1[2] = vqaddq_s16(input[2], input[29]);
  x1[29] = vqsubq_s16(input[2], input[29]);
  x1[3] = vqaddq_s16(input[3], input[28]);
  x1[28] = vqsubq_s16(input[3], input[28]);
  x1[4] = vqaddq_s16(input[4], input[27]);
  x1[27] = vqsubq_s16(input[4], input[27]);
  x1[5] = vqaddq_s16(input[5], input[26]);
  x1[26] = vqsubq_s16(input[5], input[26]);
  x1[6] = vqaddq_s16(input[6], input[25]);
  x1[25] = vqsubq_s16(input[6], input[25]);
  x1[7] = vqaddq_s16(input[7], input[24]);
  x1[24] = vqsubq_s16(input[7], input[24]);
  x1[8] = vqaddq_s16(input[8], input[23]);
  x1[23] = vqsubq_s16(input[8], input[23]);
  x1[9] = vqaddq_s16(input[9], input[22]);
  x1[22] = vqsubq_s16(input[9], input[22]);
  x1[10] = vqaddq_s16(input[10], input[21]);
  x1[21] = vqsubq_s16(input[10], input[21]);
  x1[11] = vqaddq_s16(input[11], input[20]);
  x1[20] = vqsubq_s16(input[11], input[20]);
  x1[12] = vqaddq_s16(input[12], input[19]);
  x1[19] = vqsubq_s16(input[12], input[19]);
  x1[13] = vqaddq_s16(input[13], input[18]);
  x1[18] = vqsubq_s16(input[13], input[18]);
  x1[14] = vqaddq_s16(input[14], input[17]);
  x1[17] = vqsubq_s16(input[14], input[17]);
  x1[15] = vqaddq_s16(input[15], input[16]);
  x1[16] = vqsubq_s16(input[15], input[16]);

  // stage 2
  int16x8_t x2[32];
  x2[0] = vqaddq_s16(x1[0], x1[15]);
  x2[15] = vqsubq_s16(x1[0], x1[15]);
  x2[1] = vqaddq_s16(x1[1], x1[14]);
  x2[14] = vqsubq_s16(x1[1], x1[14]);
  x2[2] = vqaddq_s16(x1[2], x1[13]);
  x2[13] = vqsubq_s16(x1[2], x1[13]);
  x2[3] = vqaddq_s16(x1[3], x1[12]);
  x2[12] = vqsubq_s16(x1[3], x1[12]);
  x2[4] = vqaddq_s16(x1[4], x1[11]);
  x2[11] = vqsubq_s16(x1[4], x1[11]);
  x2[5] = vqaddq_s16(x1[5], x1[10]);
  x2[10] = vqsubq_s16(x1[5], x1[10]);
  x2[6] = vqaddq_s16(x1[6], x1[9]);
  x2[9] = vqsubq_s16(x1[6], x1[9]);
  x2[7] = vqaddq_s16(x1[7], x1[8]);
  x2[8] = vqsubq_s16(x1[7], x1[8]);

  btf_16_neon_mode0(cospi[32], cospi[32], x1[20], x1[27], x2[20], x2[27],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[32], cospi[32], x1[21], x1[26], x2[21], x2[26],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[32], cospi[32], x1[22], x1[25], x2[22], x2[25],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[32], cospi[32], x1[23], x1[24], x2[23], x2[24],
                    v_cos_bit);

  // stage 3
  int16x8_t x3[32];
  x3[0] = vqaddq_s16(x2[0], x2[7]);
  x3[7] = vqsubq_s16(x2[0], x2[7]);
  x3[1] = vqaddq_s16(x2[1], x2[6]);
  x3[6] = vqsubq_s16(x2[1], x2[6]);
  x3[2] = vqaddq_s16(x2[2], x2[5]);
  x3[5] = vqsubq_s16(x2[2], x2[5]);
  x3[3] = vqaddq_s16(x2[3], x2[4]);
  x3[4] = vqsubq_s16(x2[3], x2[4]);

  btf_16_neon_mode0(cospi[32], cospi[32], x2[10], x2[13], x3[10], x3[13],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[32], cospi[32], x2[11], x2[12], x3[11], x3[12],
                    v_cos_bit);

  x3[16] = vqaddq_s16(x1[16], x2[23]);
  x3[23] = vqsubq_s16(x1[16], x2[23]);
  x3[17] = vqaddq_s16(x1[17], x2[22]);
  x3[22] = vqsubq_s16(x1[17], x2[22]);
  x3[18] = vqaddq_s16(x1[18], x2[21]);
  x3[21] = vqsubq_s16(x1[18], x2[21]);
  x3[19] = vqaddq_s16(x1[19], x2[20]);
  x3[20] = vqsubq_s16(x1[19], x2[20]);
  x3[24] = vqsubq_s16(x1[31], x2[24]);
  x3[31] = vqaddq_s16(x1[31], x2[24]);
  x3[25] = vqsubq_s16(x1[30], x2[25]);
  x3[30] = vqaddq_s16(x1[30], x2[25]);
  x3[26] = vqsubq_s16(x1[29], x2[26]);
  x3[29] = vqaddq_s16(x1[29], x2[26]);
  x3[27] = vqsubq_s16(x1[28], x2[27]);
  x3[28] = vqaddq_s16(x1[28], x2[27]);

  // stage 4
  int16x8_t x4[32];
  x4[0] = vqaddq_s16(x3[0], x3[3]);
  x4[3] = vqsubq_s16(x3[0], x3[3]);
  x4[1] = vqaddq_s16(x3[1], x3[2]);
  x4[2] = vqsubq_s16(x3[1], x3[2]);
  btf_16_neon_mode0(cospi[32], cospi[32], x3[5], x3[6], x4[5], x4[6],
                    v_cos_bit);
  x4[8] = vqaddq_s16(x2[8], x3[11]);
  x4[11] = vqsubq_s16(x2[8], x3[11]);
  x4[9] = vqaddq_s16(x2[9], x3[10]);
  x4[10] = vqsubq_s16(x2[9], x3[10]);
  x4[12] = vqsubq_s16(x2[15], x3[12]);
  x4[15] = vqaddq_s16(x2[15], x3[12]);
  x4[13] = vqsubq_s16(x2[14], x3[13]);
  x4[14] = vqaddq_s16(x2[14], x3[13]);

  btf_16_neon_mode0(cospi[16], cospi[48], x3[18], x3[29], x4[18], x4[29],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[16], cospi[48], x3[19], x3[28], x4[19], x4[28],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x3[20], x3[27], x4[20], x4[27],
                     v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x3[21], x3[26], x4[21], x4[26],
                     v_cos_bit);

  // stage 5
  int16x8_t x5[32];
  btf_16_neon_mode3(cospi[32], cospi[32], x4[0], x4[1], output[0], output[16],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[48], cospi[16], x4[2], x4[3], output[8], output[24],
                    v_cos_bit);
  x5[4] = vqaddq_s16(x3[4], x4[5]);
  x5[5] = vqsubq_s16(x3[4], x4[5]);
  x5[6] = vqsubq_s16(x3[7], x4[6]);
  x5[7] = vqaddq_s16(x3[7], x4[6]);

  btf_16_neon_mode0(cospi[16], cospi[48], x4[9], x4[14], x5[9], x5[14],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x4[10], x4[13], x5[10], x5[13],
                     v_cos_bit);

  x5[16] = vqaddq_s16(x3[16], x4[19]);
  x5[19] = vqsubq_s16(x3[16], x4[19]);
  x5[17] = vqaddq_s16(x3[17], x4[18]);
  x5[18] = vqsubq_s16(x3[17], x4[18]);
  x5[20] = vqsubq_s16(x3[23], x4[20]);
  x5[23] = vqaddq_s16(x3[23], x4[20]);
  x5[21] = vqsubq_s16(x3[22], x4[21]);
  x5[22] = vqaddq_s16(x3[22], x4[21]);
  x5[24] = vqaddq_s16(x3[24], x4[27]);
  x5[27] = vqsubq_s16(x3[24], x4[27]);
  x5[25] = vqaddq_s16(x3[25], x4[26]);
  x5[26] = vqsubq_s16(x3[25], x4[26]);
  x5[28] = vqsubq_s16(x3[31], x4[28]);
  x5[31] = vqaddq_s16(x3[31], x4[28]);
  x5[29] = vqsubq_s16(x3[30], x4[29]);
  x5[30] = vqaddq_s16(x3[30], x4[29]);

  // stage 6
  int16x8_t x6[32];
  btf_16_neon_mode2(cospi[56], cospi[8], x5[4], x5[7], output[4], output[28],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[24], cospi[40], x5[5], x5[6], output[20], output[12],
                    v_cos_bit);
  x6[8] = vqaddq_s16(x4[8], x5[9]);
  x6[9] = vqsubq_s16(x4[8], x5[9]);
  x6[10] = vqsubq_s16(x4[11], x5[10]);
  x6[11] = vqaddq_s16(x4[11], x5[10]);
  x6[12] = vqaddq_s16(x4[12], x5[13]);
  x6[13] = vqsubq_s16(x4[12], x5[13]);
  x6[14] = vqsubq_s16(x4[15], x5[14]);
  x6[15] = vqaddq_s16(x4[15], x5[14]);
  btf_16_neon_mode0(cospi[8], cospi[56], x5[17], x5[30], x6[17], x6[30],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[56], cospi[8], x5[18], x5[29], x6[18], x6[29],
                     v_cos_bit);
  btf_16_neon_mode0(cospi[40], cospi[24], x5[21], x5[26], x6[21], x6[26],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[24], cospi[40], x5[22], x5[25], x6[22], x6[25],
                     v_cos_bit);

  // stage 7
  int16x8_t x7[32];
  btf_16_neon_mode2(cospi[60], cospi[4], x6[8], x6[15], output[2], output[30],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[28], cospi[36], x6[9], x6[14], output[18], output[14],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[44], cospi[20], x6[10], x6[13], output[10],
                    output[22], v_cos_bit);
  btf_16_neon_mode2(cospi[12], cospi[52], x6[11], x6[12], output[26], output[6],
                    v_cos_bit);
  x7[16] = vqaddq_s16(x5[16], x6[17]);
  x7[17] = vqsubq_s16(x5[16], x6[17]);
  x7[18] = vqsubq_s16(x5[19], x6[18]);
  x7[19] = vqaddq_s16(x5[19], x6[18]);
  x7[20] = vqaddq_s16(x5[20], x6[21]);
  x7[21] = vqsubq_s16(x5[20], x6[21]);
  x7[22] = vqsubq_s16(x5[23], x6[22]);
  x7[23] = vqaddq_s16(x5[23], x6[22]);
  x7[24] = vqaddq_s16(x5[24], x6[25]);
  x7[25] = vqsubq_s16(x5[24], x6[25]);
  x7[26] = vqsubq_s16(x5[27], x6[26]);
  x7[27] = vqaddq_s16(x5[27], x6[26]);
  x7[28] = vqaddq_s16(x5[28], x6[29]);
  x7[29] = vqsubq_s16(x5[28], x6[29]);
  x7[30] = vqsubq_s16(x5[31], x6[30]);
  x7[31] = vqaddq_s16(x5[31], x6[30]);

  btf_16_neon_mode2(cospi[62], cospi[2], x7[16], x7[31], output[1], output[31],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[30], cospi[34], x7[17], x7[30], output[17],
                    output[15], v_cos_bit);
  btf_16_neon_mode2(cospi[46], cospi[18], x7[18], x7[29], output[9], output[23],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[14], cospi[50], x7[19], x7[28], output[25], output[7],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[54], cospi[10], x7[20], x7[27], output[5], output[27],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[22], cospi[42], x7[21], x7[26], output[21],
                    output[11], v_cos_bit);
  btf_16_neon_mode2(cospi[38], cospi[26], x7[22], x7[25], output[13],
                    output[19], v_cos_bit);
  btf_16_neon_mode2(cospi[6], cospi[58], x7[23], x7[24], output[29], output[3],
                    v_cos_bit);
}

void av1_fdct8x64_stage_1234_neon(const int16x8_t *input, int16x8_t *x3,
                                  int16x8_t *x4, const int32_t *cospi32,
                                  const int32x4_t *v_cos_bit) {
  int16x8_t x1[64];
  int16x8_t x2[64];
  x1[0] = vqaddq_s16(input[0], input[63]);
  x1[63] = vqsubq_s16(input[0], input[63]);
  x1[1] = vqaddq_s16(input[1], input[62]);
  x1[62] = vqsubq_s16(input[1], input[62]);
  x1[2] = vqaddq_s16(input[2], input[61]);
  x1[61] = vqsubq_s16(input[2], input[61]);
  x1[3] = vqaddq_s16(input[3], input[60]);
  x1[60] = vqsubq_s16(input[3], input[60]);
  x1[4] = vqaddq_s16(input[4], input[59]);
  x1[59] = vqsubq_s16(input[4], input[59]);
  x1[5] = vqaddq_s16(input[5], input[58]);
  x1[58] = vqsubq_s16(input[5], input[58]);
  x1[6] = vqaddq_s16(input[6], input[57]);
  x1[57] = vqsubq_s16(input[6], input[57]);
  x1[7] = vqaddq_s16(input[7], input[56]);
  x1[56] = vqsubq_s16(input[7], input[56]);
  x1[8] = vqaddq_s16(input[8], input[55]);
  x1[55] = vqsubq_s16(input[8], input[55]);
  x1[9] = vqaddq_s16(input[9], input[54]);
  x1[54] = vqsubq_s16(input[9], input[54]);
  x1[10] = vqaddq_s16(input[10], input[53]);
  x1[53] = vqsubq_s16(input[10], input[53]);
  x1[11] = vqaddq_s16(input[11], input[52]);
  x1[52] = vqsubq_s16(input[11], input[52]);
  x1[12] = vqaddq_s16(input[12], input[51]);
  x1[51] = vqsubq_s16(input[12], input[51]);
  x1[13] = vqaddq_s16(input[13], input[50]);
  x1[50] = vqsubq_s16(input[13], input[50]);
  x1[14] = vqaddq_s16(input[14], input[49]);
  x1[49] = vqsubq_s16(input[14], input[49]);
  x1[15] = vqaddq_s16(input[15], input[48]);
  x1[48] = vqsubq_s16(input[15], input[48]);
  x1[16] = vqaddq_s16(input[16], input[47]);
  x1[47] = vqsubq_s16(input[16], input[47]);
  x1[17] = vqaddq_s16(input[17], input[46]);
  x1[46] = vqsubq_s16(input[17], input[46]);
  x1[18] = vqaddq_s16(input[18], input[45]);
  x1[45] = vqsubq_s16(input[18], input[45]);
  x1[19] = vqaddq_s16(input[19], input[44]);
  x1[44] = vqsubq_s16(input[19], input[44]);
  x1[20] = vqaddq_s16(input[20], input[43]);
  x1[43] = vqsubq_s16(input[20], input[43]);
  x1[21] = vqaddq_s16(input[21], input[42]);
  x1[42] = vqsubq_s16(input[21], input[42]);
  x1[22] = vqaddq_s16(input[22], input[41]);
  x1[41] = vqsubq_s16(input[22], input[41]);
  x1[23] = vqaddq_s16(input[23], input[40]);
  x1[40] = vqsubq_s16(input[23], input[40]);
  x1[24] = vqaddq_s16(input[24], input[39]);
  x1[39] = vqsubq_s16(input[24], input[39]);
  x1[25] = vqaddq_s16(input[25], input[38]);
  x1[38] = vqsubq_s16(input[25], input[38]);
  x1[26] = vqaddq_s16(input[26], input[37]);
  x1[37] = vqsubq_s16(input[26], input[37]);
  x1[27] = vqaddq_s16(input[27], input[36]);
  x1[36] = vqsubq_s16(input[27], input[36]);
  x1[28] = vqaddq_s16(input[28], input[35]);
  x1[35] = vqsubq_s16(input[28], input[35]);
  x1[29] = vqaddq_s16(input[29], input[34]);
  x1[34] = vqsubq_s16(input[29], input[34]);
  x1[30] = vqaddq_s16(input[30], input[33]);
  x1[33] = vqsubq_s16(input[30], input[33]);
  x1[31] = vqaddq_s16(input[31], input[32]);
  x1[32] = vqsubq_s16(input[31], input[32]);

  x2[0] = vqaddq_s16(x1[0], x1[31]);
  x2[31] = vqsubq_s16(x1[0], x1[31]);
  x2[1] = vqaddq_s16(x1[1], x1[30]);
  x2[30] = vqsubq_s16(x1[1], x1[30]);
  x2[2] = vqaddq_s16(x1[2], x1[29]);
  x2[29] = vqsubq_s16(x1[2], x1[29]);
  x2[3] = vqaddq_s16(x1[3], x1[28]);
  x2[28] = vqsubq_s16(x1[3], x1[28]);
  x2[4] = vqaddq_s16(x1[4], x1[27]);
  x2[27] = vqsubq_s16(x1[4], x1[27]);
  x2[5] = vqaddq_s16(x1[5], x1[26]);
  x2[26] = vqsubq_s16(x1[5], x1[26]);
  x2[6] = vqaddq_s16(x1[6], x1[25]);
  x2[25] = vqsubq_s16(x1[6], x1[25]);
  x2[7] = vqaddq_s16(x1[7], x1[24]);
  x2[24] = vqsubq_s16(x1[7], x1[24]);
  x2[8] = vqaddq_s16(x1[8], x1[23]);
  x2[23] = vqsubq_s16(x1[8], x1[23]);
  x2[9] = vqaddq_s16(x1[9], x1[22]);
  x2[22] = vqsubq_s16(x1[9], x1[22]);
  x2[10] = vqaddq_s16(x1[10], x1[21]);
  x2[21] = vqsubq_s16(x1[10], x1[21]);
  x2[11] = vqaddq_s16(x1[11], x1[20]);
  x2[20] = vqsubq_s16(x1[11], x1[20]);
  x2[12] = vqaddq_s16(x1[12], x1[19]);
  x2[19] = vqsubq_s16(x1[12], x1[19]);
  x2[13] = vqaddq_s16(x1[13], x1[18]);
  x2[18] = vqsubq_s16(x1[13], x1[18]);
  x2[14] = vqaddq_s16(x1[14], x1[17]);
  x2[17] = vqsubq_s16(x1[14], x1[17]);
  x2[15] = vqaddq_s16(x1[15], x1[16]);
  x2[16] = vqsubq_s16(x1[15], x1[16]);

  btf_16_neon_mode0(*cospi32, *cospi32, x1[40], x1[55], x2[40], x2[55],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x1[41], x1[54], x2[41], x2[54],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x1[42], x1[53], x2[42], x2[53],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x1[43], x1[52], x2[43], x2[52],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x1[44], x1[51], x2[44], x2[51],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x1[45], x1[50], x2[45], x2[50],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x1[46], x1[49], x2[46], x2[49],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x1[47], x1[48], x2[47], x2[48],
                    *v_cos_bit);

  // stage 3
  x3[0] = vqaddq_s16(x2[0], x2[15]);
  x3[15] = vqsubq_s16(x2[0], x2[15]);
  x3[1] = vqaddq_s16(x2[1], x2[14]);
  x3[14] = vqsubq_s16(x2[1], x2[14]);
  x3[2] = vqaddq_s16(x2[2], x2[13]);
  x3[13] = vqsubq_s16(x2[2], x2[13]);
  x3[3] = vqaddq_s16(x2[3], x2[12]);
  x3[12] = vqsubq_s16(x2[3], x2[12]);
  x3[4] = vqaddq_s16(x2[4], x2[11]);
  x3[11] = vqsubq_s16(x2[4], x2[11]);
  x3[5] = vqaddq_s16(x2[5], x2[10]);
  x3[10] = vqsubq_s16(x2[5], x2[10]);
  x3[6] = vqaddq_s16(x2[6], x2[9]);
  x3[9] = vqsubq_s16(x2[6], x2[9]);
  x3[7] = vqaddq_s16(x2[7], x2[8]);
  x3[8] = vqsubq_s16(x2[7], x2[8]);
  x3[16] = x2[16];
  x3[17] = x2[17];
  x3[18] = x2[18];
  x3[19] = x2[19];
  btf_16_neon_mode0(*cospi32, *cospi32, x2[20], x2[27], x3[20], x3[27],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x2[21], x2[26], x3[21], x3[26],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x2[22], x2[25], x3[22], x3[25],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x2[23], x2[24], x3[23], x3[24],
                    *v_cos_bit);
  x3[28] = x2[28];
  x3[29] = x2[29];
  x3[30] = x2[30];
  x3[31] = x2[31];
  x3[32] = vqaddq_s16(x1[32], x2[47]);
  x3[47] = vqsubq_s16(x1[32], x2[47]);
  x3[33] = vqaddq_s16(x1[33], x2[46]);
  x3[46] = vqsubq_s16(x1[33], x2[46]);
  x3[34] = vqaddq_s16(x1[34], x2[45]);
  x3[45] = vqsubq_s16(x1[34], x2[45]);
  x3[35] = vqaddq_s16(x1[35], x2[44]);
  x3[44] = vqsubq_s16(x1[35], x2[44]);
  x3[36] = vqaddq_s16(x1[36], x2[43]);
  x3[43] = vqsubq_s16(x1[36], x2[43]);
  x3[37] = vqaddq_s16(x1[37], x2[42]);
  x3[42] = vqsubq_s16(x1[37], x2[42]);
  x3[38] = vqaddq_s16(x1[38], x2[41]);
  x3[41] = vqsubq_s16(x1[38], x2[41]);
  x3[39] = vqaddq_s16(x1[39], x2[40]);
  x3[40] = vqsubq_s16(x1[39], x2[40]);
  x3[48] = vqsubq_s16(x1[63], x2[48]);
  x3[63] = vqaddq_s16(x1[63], x2[48]);
  x3[49] = vqsubq_s16(x1[62], x2[49]);
  x3[62] = vqaddq_s16(x1[62], x2[49]);
  x3[50] = vqsubq_s16(x1[61], x2[50]);
  x3[61] = vqaddq_s16(x1[61], x2[50]);
  x3[51] = vqsubq_s16(x1[60], x2[51]);
  x3[60] = vqaddq_s16(x1[60], x2[51]);
  x3[52] = vqsubq_s16(x1[59], x2[52]);
  x3[59] = vqaddq_s16(x1[59], x2[52]);
  x3[53] = vqsubq_s16(x1[58], x2[53]);
  x3[58] = vqaddq_s16(x1[58], x2[53]);
  x3[54] = vqsubq_s16(x1[57], x2[54]);
  x3[57] = vqaddq_s16(x1[57], x2[54]);
  x3[55] = vqsubq_s16(x1[56], x2[55]);
  x3[56] = vqaddq_s16(x1[56], x2[55]);

  // stage 4
  x4[0] = vqaddq_s16(x3[0], x3[7]);
  x4[7] = vqsubq_s16(x3[0], x3[7]);
  x4[1] = vqaddq_s16(x3[1], x3[6]);
  x4[6] = vqsubq_s16(x3[1], x3[6]);
  x4[2] = vqaddq_s16(x3[2], x3[5]);
  x4[5] = vqsubq_s16(x3[2], x3[5]);
  x4[3] = vqaddq_s16(x3[3], x3[4]);
  x4[4] = vqsubq_s16(x3[3], x3[4]);

  btf_16_neon_mode0(*cospi32, *cospi32, x3[10], x3[13], x4[10], x4[13],
                    *v_cos_bit);
  btf_16_neon_mode0(*cospi32, *cospi32, x3[11], x3[12], x4[11], x4[12],
                    *v_cos_bit);

  x4[16] = vqaddq_s16(x3[16], x3[23]);
  x4[23] = vqsubq_s16(x3[16], x3[23]);
  x4[17] = vqaddq_s16(x3[17], x3[22]);
  x4[22] = vqsubq_s16(x3[17], x3[22]);
  x4[18] = vqaddq_s16(x3[18], x3[21]);
  x4[21] = vqsubq_s16(x3[18], x3[21]);
  x4[19] = vqaddq_s16(x3[19], x3[20]);
  x4[20] = vqsubq_s16(x3[19], x3[20]);
  x4[24] = vqsubq_s16(x3[31], x3[24]);
  x4[31] = vqaddq_s16(x3[31], x3[24]);
  x4[25] = vqsubq_s16(x3[30], x3[25]);
  x4[30] = vqaddq_s16(x3[30], x3[25]);
  x4[26] = vqsubq_s16(x3[29], x3[26]);
  x4[29] = vqaddq_s16(x3[29], x3[26]);
  x4[27] = vqsubq_s16(x3[28], x3[27]);
  x4[28] = vqaddq_s16(x3[28], x3[27]);
}

void av1_fdct8x64_neon(const int16x8_t *input, int16x8_t *output,
                       int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  int16x8_t x3[64];
  int16x8_t x4[64];

  av1_fdct8x64_stage_1234_neon(input, x3, x4, &cospi[32], &v_cos_bit);

  btf_16_neon_mode0(cospi[16], cospi[48], x3[36], x3[59], x4[36], x4[59],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[16], cospi[48], x3[37], x3[58], x4[37], x4[58],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[16], cospi[48], x3[38], x3[57], x4[38], x4[57],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[16], cospi[48], x3[39], x3[56], x4[39], x4[56],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x3[40], x3[55], x4[40], x4[55],
                     v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x3[41], x3[54], x4[41], x4[54],
                     v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x3[42], x3[53], x4[42], x4[53],
                     v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x3[43], x3[52], x4[43], x4[52],
                     v_cos_bit);

  // stage 5
  int16x8_t x5[64];
  x5[0] = vqaddq_s16(x4[0], x4[3]);
  x5[3] = vqsubq_s16(x4[0], x4[3]);
  x5[1] = vqaddq_s16(x4[1], x4[2]);
  x5[2] = vqsubq_s16(x4[1], x4[2]);

  btf_16_neon_mode0(cospi[32], cospi[32], x4[5], x4[6], x5[5], x5[6],
                    v_cos_bit);

  x5[8] = vqaddq_s16(x3[8], x4[11]);
  x5[11] = vqsubq_s16(x3[8], x4[11]);
  x5[9] = vqaddq_s16(x3[9], x4[10]);
  x5[10] = vqsubq_s16(x3[9], x4[10]);
  x5[12] = vqsubq_s16(x3[15], x4[12]);
  x5[15] = vqaddq_s16(x3[15], x4[12]);
  x5[13] = vqsubq_s16(x3[14], x4[13]);
  x5[14] = vqaddq_s16(x3[14], x4[13]);

  btf_16_neon_mode0(cospi[16], cospi[48], x4[18], x4[29], x5[18], x5[29],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[16], cospi[48], x4[19], x4[28], x5[19], x5[28],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x4[20], x4[27], x5[20], x5[27],
                     v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x4[21], x4[26], x5[21], x5[26],
                     v_cos_bit);

  x5[32] = vqaddq_s16(x3[32], x4[39]);
  x5[39] = vqsubq_s16(x3[32], x4[39]);
  x5[33] = vqaddq_s16(x3[33], x4[38]);
  x5[38] = vqsubq_s16(x3[33], x4[38]);
  x5[34] = vqaddq_s16(x3[34], x4[37]);
  x5[37] = vqsubq_s16(x3[34], x4[37]);
  x5[35] = vqaddq_s16(x3[35], x4[36]);
  x5[36] = vqsubq_s16(x3[35], x4[36]);
  x5[40] = vqsubq_s16(x3[47], x4[40]);
  x5[47] = vqaddq_s16(x3[47], x4[40]);
  x5[41] = vqsubq_s16(x3[46], x4[41]);
  x5[46] = vqaddq_s16(x3[46], x4[41]);
  x5[42] = vqsubq_s16(x3[45], x4[42]);
  x5[45] = vqaddq_s16(x3[45], x4[42]);
  x5[43] = vqsubq_s16(x3[44], x4[43]);
  x5[44] = vqaddq_s16(x3[44], x4[43]);
  x5[48] = vqaddq_s16(x3[48], x4[55]);
  x5[55] = vqsubq_s16(x3[48], x4[55]);
  x5[49] = vqaddq_s16(x3[49], x4[54]);
  x5[54] = vqsubq_s16(x3[49], x4[54]);
  x5[50] = vqaddq_s16(x3[50], x4[53]);
  x5[53] = vqsubq_s16(x3[50], x4[53]);
  x5[51] = vqaddq_s16(x3[51], x4[52]);
  x5[52] = vqsubq_s16(x3[51], x4[52]);
  x5[56] = vqsubq_s16(x3[63], x4[56]);
  x5[63] = vqaddq_s16(x3[63], x4[56]);
  x5[57] = vqsubq_s16(x3[62], x4[57]);
  x5[62] = vqaddq_s16(x3[62], x4[57]);
  x5[58] = vqsubq_s16(x3[61], x4[58]);
  x5[61] = vqaddq_s16(x3[61], x4[58]);
  x5[59] = vqsubq_s16(x3[60], x4[59]);
  x5[60] = vqaddq_s16(x3[60], x4[59]);

  // stage 6
  int16x8_t x6[64];
  btf_16_neon_mode2(cospi[32], cospi[32], x5[0], x5[1], x6[0], x6[1],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[48], cospi[16], x5[2], x5[3], x6[2], x6[3],
                    v_cos_bit);
  x6[4] = vqaddq_s16(x4[4], x5[5]);
  x6[5] = vqsubq_s16(x4[4], x5[5]);
  x6[6] = vqsubq_s16(x4[7], x5[6]);
  x6[7] = vqaddq_s16(x4[7], x5[6]);

  btf_16_neon_mode0(cospi[16], cospi[48], x5[9], x5[14], x6[9], x6[14],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[48], cospi[16], x5[10], x5[13], x6[10], x6[13],
                     v_cos_bit);

  x6[16] = vqaddq_s16(x4[16], x5[19]);
  x6[19] = vqsubq_s16(x4[16], x5[19]);
  x6[17] = vqaddq_s16(x4[17], x5[18]);
  x6[18] = vqsubq_s16(x4[17], x5[18]);
  x6[20] = vqsubq_s16(x4[23], x5[20]);
  x6[23] = vqaddq_s16(x4[23], x5[20]);
  x6[21] = vqsubq_s16(x4[22], x5[21]);
  x6[22] = vqaddq_s16(x4[22], x5[21]);
  x6[24] = vqaddq_s16(x4[24], x5[27]);
  x6[27] = vqsubq_s16(x4[24], x5[27]);
  x6[25] = vqaddq_s16(x4[25], x5[26]);
  x6[26] = vqsubq_s16(x4[25], x5[26]);
  x6[28] = vqsubq_s16(x4[31], x5[28]);
  x6[31] = vqaddq_s16(x4[31], x5[28]);
  x6[29] = vqsubq_s16(x4[30], x5[29]);
  x6[30] = vqaddq_s16(x4[30], x5[29]);

  btf_16_neon_mode0(cospi[8], cospi[56], x5[34], x5[61], x6[34], x6[61],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[8], cospi[56], x5[35], x5[60], x6[35], x6[60],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[56], cospi[8], x5[36], x5[59], x6[36], x6[59],
                     v_cos_bit);
  btf_16_neon_mode02(cospi[56], cospi[8], x5[37], x5[58], x6[37], x6[58],
                     v_cos_bit);
  btf_16_neon_mode0(cospi[40], cospi[24], x5[42], x5[53], x6[42], x6[53],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[40], cospi[24], x5[43], x5[52], x6[43], x6[52],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[24], cospi[40], x5[44], x5[51], x6[44], x6[51],
                     v_cos_bit);
  btf_16_neon_mode02(cospi[24], cospi[40], x5[45], x5[50], x6[45], x6[50],
                     v_cos_bit);

  // stage 7
  int16x8_t x7[64];

  btf_16_neon_mode2(cospi[56], cospi[8], x6[4], x6[7], x7[4], x7[7], v_cos_bit);
  btf_16_neon_mode2(cospi[24], cospi[40], x6[5], x6[6], x7[5], x7[6],
                    v_cos_bit);
  x7[8] = vqaddq_s16(x5[8], x6[9]);
  x7[9] = vqsubq_s16(x5[8], x6[9]);
  x7[10] = vqsubq_s16(x5[11], x6[10]);
  x7[11] = vqaddq_s16(x5[11], x6[10]);
  x7[12] = vqaddq_s16(x5[12], x6[13]);
  x7[13] = vqsubq_s16(x5[12], x6[13]);
  x7[14] = vqsubq_s16(x5[15], x6[14]);
  x7[15] = vqaddq_s16(x5[15], x6[14]);

  btf_16_neon_mode0(cospi[8], cospi[56], x6[17], x6[30], x7[17], x7[30],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[56], cospi[8], x6[18], x6[29], x7[18], x7[29],
                     v_cos_bit);

  btf_16_neon_mode0(cospi[40], cospi[24], x6[21], x6[26], x7[21], x7[26],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[24], cospi[40], x6[22], x6[25], x7[22], x7[25],
                     v_cos_bit);

  x7[32] = vqaddq_s16(x5[32], x6[35]);
  x7[35] = vqsubq_s16(x5[32], x6[35]);
  x7[33] = vqaddq_s16(x5[33], x6[34]);
  x7[34] = vqsubq_s16(x5[33], x6[34]);
  x7[36] = vqsubq_s16(x5[39], x6[36]);
  x7[39] = vqaddq_s16(x5[39], x6[36]);
  x7[37] = vqsubq_s16(x5[38], x6[37]);
  x7[38] = vqaddq_s16(x5[38], x6[37]);
  x7[40] = vqaddq_s16(x5[40], x6[43]);
  x7[43] = vqsubq_s16(x5[40], x6[43]);
  x7[41] = vqaddq_s16(x5[41], x6[42]);
  x7[42] = vqsubq_s16(x5[41], x6[42]);
  x7[44] = vqsubq_s16(x5[47], x6[44]);
  x7[47] = vqaddq_s16(x5[47], x6[44]);
  x7[45] = vqsubq_s16(x5[46], x6[45]);
  x7[46] = vqaddq_s16(x5[46], x6[45]);
  x7[48] = vqaddq_s16(x5[48], x6[51]);
  x7[51] = vqsubq_s16(x5[48], x6[51]);
  x7[49] = vqaddq_s16(x5[49], x6[50]);
  x7[50] = vqsubq_s16(x5[49], x6[50]);
  x7[52] = vqsubq_s16(x5[55], x6[52]);
  x7[55] = vqaddq_s16(x5[55], x6[52]);
  x7[53] = vqsubq_s16(x5[54], x6[53]);
  x7[54] = vqaddq_s16(x5[54], x6[53]);
  x7[56] = vqaddq_s16(x5[56], x6[59]);
  x7[59] = vqsubq_s16(x5[56], x6[59]);
  x7[57] = vqaddq_s16(x5[57], x6[58]);
  x7[58] = vqsubq_s16(x5[57], x6[58]);
  x7[60] = vqsubq_s16(x5[63], x6[60]);
  x7[63] = vqaddq_s16(x5[63], x6[60]);
  x7[61] = vqsubq_s16(x5[62], x6[61]);
  x7[62] = vqaddq_s16(x5[62], x6[61]);

  // stage 8
  int16x8_t x8[64];

  btf_16_neon_mode2(cospi[60], cospi[4], x7[8], x7[15], x8[8], x8[15],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[28], cospi[36], x7[9], x7[14], x8[9], x8[14],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[44], cospi[20], x7[10], x7[13], x8[10], x8[13],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[12], cospi[52], x7[11], x7[12], x8[11], x8[12],
                    v_cos_bit);
  x8[16] = vqaddq_s16(x6[16], x7[17]);
  x8[17] = vqsubq_s16(x6[16], x7[17]);
  x8[18] = vqsubq_s16(x6[19], x7[18]);
  x8[19] = vqaddq_s16(x6[19], x7[18]);
  x8[20] = vqaddq_s16(x6[20], x7[21]);
  x8[21] = vqsubq_s16(x6[20], x7[21]);
  x8[22] = vqsubq_s16(x6[23], x7[22]);
  x8[23] = vqaddq_s16(x6[23], x7[22]);
  x8[24] = vqaddq_s16(x6[24], x7[25]);
  x8[25] = vqsubq_s16(x6[24], x7[25]);
  x8[26] = vqsubq_s16(x6[27], x7[26]);
  x8[27] = vqaddq_s16(x6[27], x7[26]);
  x8[28] = vqaddq_s16(x6[28], x7[29]);
  x8[29] = vqsubq_s16(x6[28], x7[29]);
  x8[30] = vqsubq_s16(x6[31], x7[30]);
  x8[31] = vqaddq_s16(x6[31], x7[30]);

  btf_16_neon_mode0(cospi[4], cospi[60], x7[33], x7[62], x8[33], x8[62],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[60], cospi[4], x7[34], x7[61], x8[34], x8[61],
                     v_cos_bit);
  btf_16_neon_mode0(cospi[36], cospi[28], x7[37], x7[58], x8[37], x8[58],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[28], cospi[36], x7[38], x7[57], x8[38], x8[57],
                     v_cos_bit);
  btf_16_neon_mode0(cospi[20], cospi[44], x7[41], x7[54], x8[41], x8[54],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[44], cospi[20], x7[42], x7[53], x8[42], x8[53],
                     v_cos_bit);
  btf_16_neon_mode0(cospi[52], cospi[12], x7[45], x7[50], x8[45], x8[50],
                    v_cos_bit);
  btf_16_neon_mode02(cospi[12], cospi[52], x7[46], x7[49], x8[46], x8[49],
                     v_cos_bit);

  // stage 9
  int16x8_t x9[64];

  btf_16_neon_mode2(cospi[62], cospi[2], x8[16], x8[31], x9[16], x9[31],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[30], cospi[34], x8[17], x8[30], x9[17], x9[30],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[46], cospi[18], x8[18], x8[29], x9[18], x9[29],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[14], cospi[50], x8[19], x8[28], x9[19], x9[28],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[54], cospi[10], x8[20], x8[27], x9[20], x9[27],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[22], cospi[42], x8[21], x8[26], x9[21], x9[26],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[38], cospi[26], x8[22], x8[25], x9[22], x9[25],
                    v_cos_bit);
  btf_16_neon_mode2(cospi[6], cospi[58], x8[23], x8[24], x9[23], x9[24],
                    v_cos_bit);
  x9[32] = vqaddq_s16(x7[32], x8[33]);
  x9[33] = vqsubq_s16(x7[32], x8[33]);
  x9[34] = vqsubq_s16(x7[35], x8[34]);
  x9[35] = vqaddq_s16(x7[35], x8[34]);
  x9[36] = vqaddq_s16(x7[36], x8[37]);
  x9[37] = vqsubq_s16(x7[36], x8[37]);
  x9[38] = vqsubq_s16(x7[39], x8[38]);
  x9[39] = vqaddq_s16(x7[39], x8[38]);
  x9[40] = vqaddq_s16(x7[40], x8[41]);
  x9[41] = vqsubq_s16(x7[40], x8[41]);
  x9[42] = vqsubq_s16(x7[43], x8[42]);
  x9[43] = vqaddq_s16(x7[43], x8[42]);
  x9[44] = vqaddq_s16(x7[44], x8[45]);
  x9[45] = vqsubq_s16(x7[44], x8[45]);
  x9[46] = vqsubq_s16(x7[47], x8[46]);
  x9[47] = vqaddq_s16(x7[47], x8[46]);
  x9[48] = vqaddq_s16(x7[48], x8[49]);
  x9[49] = vqsubq_s16(x7[48], x8[49]);
  x9[50] = vqsubq_s16(x7[51], x8[50]);
  x9[51] = vqaddq_s16(x7[51], x8[50]);
  x9[52] = vqaddq_s16(x7[52], x8[53]);
  x9[53] = vqsubq_s16(x7[52], x8[53]);
  x9[54] = vqsubq_s16(x7[55], x8[54]);
  x9[55] = vqaddq_s16(x7[55], x8[54]);
  x9[56] = vqaddq_s16(x7[56], x8[57]);
  x9[57] = vqsubq_s16(x7[56], x8[57]);
  x9[58] = vqsubq_s16(x7[59], x8[58]);
  x9[59] = vqaddq_s16(x7[59], x8[58]);
  x9[60] = vqaddq_s16(x7[60], x8[61]);
  x9[61] = vqsubq_s16(x7[60], x8[61]);
  x9[62] = vqsubq_s16(x7[63], x8[62]);
  x9[63] = vqaddq_s16(x7[63], x8[62]);

  // stage 10
  btf_16_neon_mode2(cospi[63], cospi[1], x9[32], x9[63], output[1], output[63],
                    v_cos_bit);

  btf_16_neon_mode2(cospi[31], cospi[33], x9[33], x9[62], output[33],
                    output[31], v_cos_bit);

  btf_16_neon_mode2(cospi[47], cospi[17], x9[34], x9[61], output[17],
                    output[47], v_cos_bit);

  btf_16_neon_mode2(cospi[15], cospi[49], x9[35], x9[60], output[49],
                    output[15], v_cos_bit);

  btf_16_neon_mode2(cospi[55], cospi[9], x9[36], x9[59], output[9], output[55],
                    v_cos_bit);

  btf_16_neon_mode2(cospi[23], cospi[41], x9[37], x9[58], output[41],
                    output[23], v_cos_bit);

  btf_16_neon_mode2(cospi[39], cospi[25], x9[38], x9[57], output[25],
                    output[39], v_cos_bit);

  btf_16_neon_mode2(cospi[7], cospi[57], x9[39], x9[56], output[57], output[7],
                    v_cos_bit);

  btf_16_neon_mode2(cospi[59], cospi[5], x9[40], x9[55], output[5], output[59],
                    v_cos_bit);

  btf_16_neon_mode2(cospi[27], cospi[37], x9[41], x9[54], output[37],
                    output[27], v_cos_bit);

  btf_16_neon_mode2(cospi[43], cospi[21], x9[42], x9[53], output[21],
                    output[43], v_cos_bit);

  btf_16_neon_mode2(cospi[11], cospi[53], x9[43], x9[52], output[53],
                    output[11], v_cos_bit);

  btf_16_neon_mode2(cospi[51], cospi[13], x9[44], x9[51], output[13],
                    output[51], v_cos_bit);

  btf_16_neon_mode2(cospi[19], cospi[45], x9[45], x9[50], output[45],
                    output[19], v_cos_bit);

  btf_16_neon_mode2(cospi[35], cospi[29], x9[46], x9[49], output[29],
                    output[35], v_cos_bit);

  btf_16_neon_mode2(cospi[3], cospi[61], x9[47], x9[48], output[61], output[3],
                    v_cos_bit);

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

void fadst_8x8_neon(const int16x8_t *input, int16x8_t *output, int8_t cos_bit,
                    const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1
  int16x8_t x1[4];

  x1[0] = vqnegq_s16(input[7]);
  x1[1] = vqnegq_s16(input[3]);
  x1[2] = vqnegq_s16(input[1]);
  x1[3] = vqnegq_s16(input[5]);

  // stage 2
  int16x8_t x2[8];

  btf_16_neon_mode3(cospi[32], cospi[32], x1[1], input[4], x2[2], x2[3],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[32], cospi[32], input[2], x1[3], x2[6], x2[7],
                    v_cos_bit);
  // stage 3
  int16x8_t x3[8];
  x3[0] = vqaddq_s16(input[0], x2[2]);
  x3[2] = vqsubq_s16(input[0], x2[2]);
  x3[1] = vqaddq_s16(x1[0], x2[3]);
  x3[3] = vqsubq_s16(x1[0], x2[3]);
  x3[4] = vqaddq_s16(x1[2], x2[6]);
  x3[6] = vqsubq_s16(x1[2], x2[6]);
  x3[5] = vqaddq_s16(input[6], x2[7]);
  x3[7] = vqsubq_s16(input[6], x2[7]);

  // stage 4
  btf_16_neon_mode3(cospi[16], cospi[48], x3[4], x3[5], x3[4], x3[5],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[48], cospi[16], x3[6], x3[7], x3[6], x3[7],
                    v_cos_bit);

  // stage 5
  int16x8_t x5[8];
  x5[0] = vqaddq_s16(x3[0], x3[4]);
  x5[4] = vqsubq_s16(x3[0], x3[4]);
  x5[1] = vqaddq_s16(x3[1], x3[5]);
  x5[5] = vqsubq_s16(x3[1], x3[5]);
  x5[2] = vqaddq_s16(x3[2], x3[6]);
  x5[6] = vqsubq_s16(x3[2], x3[6]);
  x5[3] = vqaddq_s16(x3[3], x3[7]);
  x5[7] = vqsubq_s16(x3[3], x3[7]);

  // stage 6
  btf_16_neon_mode3(cospi[4], cospi[60], x5[0], x5[1], output[7], output[0],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[20], cospi[44], x5[2], x5[3], output[5], output[2],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[36], cospi[28], x5[4], x5[5], output[3], output[4],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[52], cospi[12], x5[6], x5[7], output[1], output[6],
                    v_cos_bit);
}

static void fadst8x16_neon(const int16x8_t *input, int16x8_t *output,
                           int8_t cos_bit, const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  // stage 1
  int16x8_t x1[12];
  x1[0] = vqnegq_s16(input[15]);
  x1[1] = vqnegq_s16(input[3]);
  x1[2] = vqnegq_s16(input[1]);
  x1[3] = vqnegq_s16(input[13]);

  // stage 2
  btf_16_neon(-cospi[32], cospi[32], -cospi[32], -cospi[32], input[7], input[8],
              x1[4], x1[5]);
  btf_16_neon_mode1(cospi[32], cospi[32], input[4], input[11], x1[6], x1[7],
                    v_cos_bit);
  btf_16_neon_mode1(cospi[32], cospi[32], input[6], input[9], x1[8], x1[9],
                    v_cos_bit);
  btf_16_neon(-cospi[32], cospi[32], -cospi[32], -cospi[32], input[5],
              input[10], x1[10], x1[11]);
  // stage 3
  int16x8_t x3[16];
  x3[0] = vqaddq_s16(input[0], x1[4]);
  x3[2] = vqsubq_s16(input[0], x1[4]);
  x3[1] = vqaddq_s16(x1[0], x1[5]);
  x3[3] = vqsubq_s16(x1[0], x1[5]);
  x3[4] = vqaddq_s16(x1[1], x1[6]);
  x3[6] = vqsubq_s16(x1[1], x1[6]);
  x3[5] = vqaddq_s16(input[12], x1[7]);
  x3[7] = vqsubq_s16(input[12], x1[7]);
  x3[8] = vqaddq_s16(x1[2], x1[8]);
  x3[10] = vqsubq_s16(x1[2], x1[8]);
  x3[9] = vqaddq_s16(input[14], x1[9]);
  x3[11] = vqsubq_s16(input[14], x1[9]);
  x3[12] = vqaddq_s16(input[2], x1[10]);
  x3[14] = vqsubq_s16(input[2], x1[10]);
  x3[13] = vqaddq_s16(x1[3], x1[11]);
  x3[15] = vqsubq_s16(x1[3], x1[11]);

  // stage 4
  btf_16_neon_mode3(cospi[16], cospi[48], x3[4], x3[5], x3[4], x3[5],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[48], cospi[16], x3[6], x3[7], x3[6], x3[7],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[16], cospi[48], x3[12], x3[13], x3[12], x3[13],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[48], cospi[16], x3[14], x3[15], x3[14], x3[15],
                    v_cos_bit);

  // stage 5
  int16x8_t x5[16];
  x5[0] = vqaddq_s16(x3[0], x3[4]);
  x5[4] = vqsubq_s16(x3[0], x3[4]);
  x5[1] = vqaddq_s16(x3[1], x3[5]);
  x5[5] = vqsubq_s16(x3[1], x3[5]);
  x5[2] = vqaddq_s16(x3[2], x3[6]);
  x5[6] = vqsubq_s16(x3[2], x3[6]);
  x5[3] = vqaddq_s16(x3[3], x3[7]);
  x5[7] = vqsubq_s16(x3[3], x3[7]);
  x5[8] = vqaddq_s16(x3[8], x3[12]);
  x5[12] = vqsubq_s16(x3[8], x3[12]);
  x5[9] = vqaddq_s16(x3[9], x3[13]);
  x5[13] = vqsubq_s16(x3[9], x3[13]);
  x5[10] = vqaddq_s16(x3[10], x3[14]);
  x5[14] = vqsubq_s16(x3[10], x3[14]);
  x5[11] = vqaddq_s16(x3[11], x3[15]);
  x5[15] = vqsubq_s16(x3[11], x3[15]);

  // stage 6
  btf_16_neon_mode3(cospi[8], cospi[56], x5[8], x5[9], x5[8], x5[9], v_cos_bit);
  btf_16_neon_mode3(cospi[40], cospi[24], x5[10], x5[11], x5[10], x5[11],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[56], cospi[8], x5[12], x5[13], x5[12], x5[13],
                    v_cos_bit);
  btf_16_neon_mode0(cospi[24], cospi[40], x5[14], x5[15], x5[14], x5[15],
                    v_cos_bit);

  // stage 7
  int16x8_t x7[16];
  x7[0] = vqaddq_s16(x5[0], x5[8]);
  x7[8] = vqsubq_s16(x5[0], x5[8]);
  x7[1] = vqaddq_s16(x5[1], x5[9]);
  x7[9] = vqsubq_s16(x5[1], x5[9]);
  x7[2] = vqaddq_s16(x5[2], x5[10]);
  x7[10] = vqsubq_s16(x5[2], x5[10]);
  x7[3] = vqaddq_s16(x5[3], x5[11]);
  x7[11] = vqsubq_s16(x5[3], x5[11]);
  x7[4] = vqaddq_s16(x5[4], x5[12]);
  x7[12] = vqsubq_s16(x5[4], x5[12]);
  x7[5] = vqaddq_s16(x5[5], x5[13]);
  x7[13] = vqsubq_s16(x5[5], x5[13]);
  x7[6] = vqaddq_s16(x5[6], x5[14]);
  x7[14] = vqsubq_s16(x5[6], x5[14]);
  x7[7] = vqaddq_s16(x5[7], x5[15]);
  x7[15] = vqsubq_s16(x5[7], x5[15]);

  // stage 8
  btf_16_neon_mode3(cospi[2], cospi[62], x7[0], x7[1], output[15], output[0],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[10], cospi[54], x7[2], x7[3], output[13], output[2],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[18], cospi[46], x7[4], x7[5], output[11], output[4],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[26], cospi[38], x7[6], x7[7], output[9], output[6],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[34], cospi[30], x7[8], x7[9], output[7], output[8],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[42], cospi[22], x7[10], x7[11], output[5], output[10],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[50], cospi[14], x7[12], x7[13], output[3], output[12],
                    v_cos_bit);
  btf_16_neon_mode3(cospi[58], cospi[6], x7[14], x7[15], output[1], output[14],
                    v_cos_bit);
}

void av1_fidentity4x4_neon(const int16x8_t *const input,
                           int16x8_t *const output, const int8_t cos_bit,
                           const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  const int16x4_t v_newsqrt2 = vdup_n_s16(NewSqrt2);
  for (int i = 0; i < 4; ++i) {
    const int16x4_t b = vqrshrn_n_s32(
        vmull_s16(vget_low_s16(input[i]), v_newsqrt2), NewSqrt2Bits);
    output[i] = vcombine_s16(b, b);
  }
}

static INLINE void fidentity8x4_neon(const int16x8_t *const input,
                                     int16x8_t *const output,
                                     const int8_t cos_bit,
                                     const int8_t *stage_range) {
  (void)stage_range;
  (void)cos_bit;
  const int16x4_t v_newsqrt2 = vdup_n_s16(NewSqrt2);
  for (int i = 0; i < 4; ++i) {
    const int16x4_t b_lo = vqrshrn_n_s32(
        vmull_s16(vget_low_s16(input[i]), v_newsqrt2), NewSqrt2Bits);
    const int16x4_t b_hi = vqrshrn_n_s32(
        vmull_s16(vget_high_s16(input[i]), v_newsqrt2), NewSqrt2Bits);
    output[i] = vcombine_s16(b_lo, b_hi);
  }
}

void fidentity8x8_neon(const int16x8_t *input, int16x8_t *output,
                       int8_t cos_bit, const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  int16x8_t one = vdupq_n_s16(1);
  output[0] = vqrshlq_s16(input[0], one);
  output[1] = vqrshlq_s16(input[1], one);
  output[2] = vqrshlq_s16(input[2], one);
  output[3] = vqrshlq_s16(input[3], one);
  output[4] = vqrshlq_s16(input[4], one);
  output[5] = vqrshlq_s16(input[5], one);
  output[6] = vqrshlq_s16(input[6], one);
  output[7] = vqrshlq_s16(input[7], one);
}

static INLINE void fidentity8x16_neon(const int16x8_t *input, int16x8_t *output,
                                      int8_t cos_bit,
                                      const int8_t *stage_range) {
  (void)stage_range;
  (void)cos_bit;
  const int16x4_t v_newsqrt2 = vdup_n_s16(NewSqrt2 * 2);
  for (int i = 0; i < 16; ++i) {
    const int16x4_t b_lo = vqrshrn_n_s32(
        vmull_s16(vget_low_s16(input[i]), v_newsqrt2), NewSqrt2Bits);
    const int16x4_t b_hi = vqrshrn_n_s32(
        vmull_s16(vget_high_s16(input[i]), v_newsqrt2), NewSqrt2Bits);
    output[i] = vcombine_s16(b_lo, b_hi);
  }
}

static INLINE void fidentity8x32_neon(const int16x8_t *input, int16x8_t *output,
                                      int8_t cos_bit,
                                      const int8_t *stage_range) {
  (void)stage_range;
  (void)cos_bit;
  for (int i = 0; i < 32; ++i) {
    output[i] = vshlq_n_s16(input[i], 2);
  }
}

typedef void (*transform_1d_lbd_neon)(const int16x8_t *input, int16x8_t *output,
                                      int8_t cos_bit,
                                      const int8_t *stage_range);

static const transform_1d_lbd_neon col_txfm4x4_arr[TX_TYPES] = {
  av1_fdct4x4_neon,       // DCT_DCT
  av1_fadst4x4_neon,      // ADST_DCT
  av1_fdct4x4_neon,       // DCT_ADST
  av1_fadst4x4_neon,      // ADST_ADST
  av1_fadst4x4_neon,      // FLIPADST_DCT
  av1_fdct4x4_neon,       // DCT_FLIPADST
  av1_fadst4x4_neon,      // FLIPADST_FLIPADST
  av1_fadst4x4_neon,      // ADST_FLIPADST
  av1_fadst4x4_neon,      // FLIPADST_ADST
  av1_fidentity4x4_neon,  // IDTX
  av1_fdct4x4_neon,       // V_DCT
  av1_fidentity4x4_neon,  // H_DCT
  av1_fadst4x4_neon,      // V_ADST
  av1_fidentity4x4_neon,  // H_ADST
  av1_fadst4x4_neon,      // V_FLIPADST
  av1_fidentity4x4_neon   // H_FLIPADST
};

static const transform_1d_lbd_neon row_txfm4x4_arr[TX_TYPES] = {
  av1_fdct4x4_neon,       // DCT_DCT
  av1_fdct4x4_neon,       // ADST_DCT
  av1_fadst4x4_neon,      // DCT_ADST
  av1_fadst4x4_neon,      // ADST_ADST
  av1_fdct4x4_neon,       // FLIPADST_DCT
  av1_fadst4x4_neon,      // DCT_FLIPADST
  av1_fadst4x4_neon,      // FLIPADST_FLIPADST
  av1_fadst4x4_neon,      // ADST_FLIPADST
  av1_fadst4x4_neon,      // FLIPADST_ADST
  av1_fidentity4x4_neon,  // IDTX
  av1_fidentity4x4_neon,  // V_DCT
  av1_fdct4x4_neon,       // H_DCT
  av1_fidentity4x4_neon,  // V_ADST
  av1_fadst4x4_neon,      // H_ADST
  av1_fidentity4x4_neon,  // V_FLIPADST
  av1_fadst4x4_neon       // H_FLIPADST
};

static const transform_1d_lbd_neon col_txfm4x8_arr[TX_TYPES] = {
  fdct4x8_neon,       // DCT_DCT
  fadst4x8_neon,      // ADST_DCT
  fdct4x8_neon,       // DCT_ADST
  fadst4x8_neon,      // ADST_ADST
  fadst4x8_neon,      // FLIPADST_DCT
  fdct4x8_neon,       // DCT_FLIPADST
  fadst4x8_neon,      // FLIPADST_FLIPADST
  fadst4x8_neon,      // ADST_FLIPADST
  fadst4x8_neon,      // FLIPADST_ADST
  fidentity8x8_neon,  // IDTX
  fdct4x8_neon,       // V_DCT
  fidentity8x8_neon,  // H_DCT
  fadst4x8_neon,      // V_ADST
  fidentity8x8_neon,  // H_ADST
  fadst4x8_neon,      // V_FLIPADST
  fidentity8x8_neon   // H_FLIPADST
};

static const transform_1d_lbd_neon row_txfm8x4_arr[TX_TYPES] = {
  fdct8x4_neon,       // DCT_DCT
  fdct8x4_neon,       // ADST_DCT
  fadst8x4_neon,      // DCT_ADST
  fadst8x4_neon,      // ADST_ADST
  fdct8x4_neon,       // FLIPADST_DCT
  fadst8x4_neon,      // DCT_FLIPADST
  fadst8x4_neon,      // FLIPADST_FLIPADST
  fadst8x4_neon,      // ADST_FLIPADST
  fadst8x4_neon,      // FLIPADST_ADST
  fidentity8x4_neon,  // IDTX
  fidentity8x4_neon,  // V_DCT
  fdct8x4_neon,       // H_DCT
  fidentity8x4_neon,  // V_ADST
  fadst8x4_neon,      // H_ADST
  fidentity8x4_neon,  // V_FLIPADST
  fadst8x4_neon       // H_FLIPADST
};

static const transform_1d_lbd_neon col_txfm8x4_arr[TX_TYPES] = {
  fdct8x4_neon,       // DCT_DCT
  fadst8x4_neon,      // ADST_DCT
  fdct8x4_neon,       // DCT_ADST
  fadst8x4_neon,      // ADST_ADST
  fadst8x4_neon,      // FLIPADST_DCT
  fdct8x4_neon,       // DCT_FLIPADST
  fadst8x4_neon,      // FLIPADST_FLIPADST
  fadst8x4_neon,      // ADST_FLIPADST
  fadst8x4_neon,      // FLIPADST_ADST
  fidentity8x4_neon,  // IDTX
  fdct8x4_neon,       // V_DCT
  fidentity8x4_neon,  // H_DCT
  fadst8x4_neon,      // V_ADST
  fidentity8x4_neon,  // H_ADST
  fadst8x4_neon,      // V_FLIPADST
  fidentity8x4_neon   // H_FLIPADST
};

static const transform_1d_lbd_neon row_txfm4x8_arr[TX_TYPES] = {
  fdct4x8_neon,       // DCT_DCT
  fdct4x8_neon,       // ADST_DCT
  fadst4x8_neon,      // DCT_ADST
  fadst4x8_neon,      // ADST_ADST
  fdct4x8_neon,       // FLIPADST_DCT
  fadst4x8_neon,      // DCT_FLIPADST
  fadst4x8_neon,      // FLIPADST_FLIPADST
  fadst4x8_neon,      // ADST_FLIPADST
  fadst4x8_neon,      // FLIPADST_ADST
  fidentity8x8_neon,  // IDTX
  fidentity8x8_neon,  // V_DCT
  fdct4x8_neon,       // H_DCT
  fidentity8x8_neon,  // V_ADST
  fadst4x8_neon,      // H_ADST
  fidentity8x8_neon,  // V_FLIPADST
  fadst4x8_neon       // H_FLIPADST
};

static const transform_1d_lbd_neon col_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_neon,       // DCT_DCT
  fadst_8x8_neon,     // ADST_DCT
  fdct8x8_neon,       // DCT_ADST
  fadst_8x8_neon,     // ADST_ADST
  fadst_8x8_neon,     // FLIPADST_DCT
  fdct8x8_neon,       // DCT_FLIPADST
  fadst_8x8_neon,     // FLIPADST_FLIPADST
  fadst_8x8_neon,     // ADST_FLIPADST
  fadst_8x8_neon,     // FLIPADST_ADST
  fidentity8x8_neon,  // IDTX
  fdct8x8_neon,       // V_DCT
  fidentity8x8_neon,  // H_DCT
  fadst_8x8_neon,     // V_ADST
  fidentity8x8_neon,  // H_ADST
  fadst_8x8_neon,     // V_FLIPADST
  fidentity8x8_neon,  // H_FLIPADST
};

static const transform_1d_lbd_neon row_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_neon,       // DCT_DCT
  fdct8x8_neon,       // ADST_DCT
  fadst_8x8_neon,     // DCT_ADST
  fadst_8x8_neon,     // ADST_ADST
  fdct8x8_neon,       // FLIPADST_DCT
  fadst_8x8_neon,     // DCT_FLIPADST
  fadst_8x8_neon,     // FLIPADST_FLIPADST
  fadst_8x8_neon,     // ADST_FLIPADST
  fadst_8x8_neon,     // FLIPADST_ADST
  fidentity8x8_neon,  // IDTX
  fidentity8x8_neon,  // V_DCT
  fdct8x8_neon,       // H_DCT
  fidentity8x8_neon,  // V_ADST
  fadst_8x8_neon,     // H_ADST
  fidentity8x8_neon,  // V_FLIPADST
  fadst_8x8_neon      // H_FLIPADST
};

static const transform_1d_lbd_neon col_txfm8x16_arr[TX_TYPES] = {
  fdct8x16_neon,       // DCT_DCT
  fadst8x16_neon,      // ADST_DCT
  fdct8x16_neon,       // DCT_ADST
  fadst8x16_neon,      // ADST_ADST
  fadst8x16_neon,      // FLIPADST_DCT
  fdct8x16_neon,       // DCT_FLIPADST
  fadst8x16_neon,      // FLIPADST_FLIPADST
  fadst8x16_neon,      // ADST_FLIPADST
  fadst8x16_neon,      // FLIPADST_ADST
  fidentity8x16_neon,  // IDTX
  fdct8x16_neon,       // V_DCT
  fidentity8x16_neon,  // H_DCT
  fadst8x16_neon,      // V_ADST
  fidentity8x16_neon,  // H_ADST
  fadst8x16_neon,      // V_FLIPADST
  fidentity8x16_neon   // H_FLIPADST
};

static const transform_1d_lbd_neon row_txfm8x16_arr[TX_TYPES] = {
  fdct8x16_neon,       // DCT_DCT
  fdct8x16_neon,       // ADST_DCT
  fadst8x16_neon,      // DCT_ADST
  fadst8x16_neon,      // ADST_ADST
  fdct8x16_neon,       // FLIPADST_DCT
  fadst8x16_neon,      // DCT_FLIPADST
  fadst8x16_neon,      // FLIPADST_FLIPADST
  fadst8x16_neon,      // ADST_FLIPADST
  fadst8x16_neon,      // FLIPADST_ADST
  fidentity8x16_neon,  // IDTX
  fidentity8x16_neon,  // V_DCT
  fdct8x16_neon,       // H_DCT
  fidentity8x16_neon,  // V_ADST
  fadst8x16_neon,      // H_ADST
  fidentity8x16_neon,  // V_FLIPADST
  fadst8x16_neon       // H_FLIPADST
};

static const transform_1d_lbd_neon row_txfm8x32_arr[TX_TYPES] = {
  av1_fdct8x32_neon,   // DCT_DCT
  NULL,                // ADST_DCT
  NULL,                // DCT_ADST
  NULL,                // ADST_ADST
  NULL,                // FLIPADST_DCT
  NULL,                // DCT_FLIPADST
  NULL,                // FLIPADST_FLIPADST
  NULL,                // ADST_FLIPADST
  NULL,                // FLIPADST_ADST
  fidentity8x32_neon,  // IDTX
  fidentity8x32_neon,  // V_DCT
  av1_fdct8x32_neon,   // H_DCT
  NULL,                // V_ADST
  NULL,                // H_ADST
  NULL,                // V_FLIPADST
  NULL                 // H_FLIPADST
};

static const transform_1d_lbd_neon col_txfm8x32_arr[TX_TYPES] = {
  av1_fdct8x32_neon,   // DCT_DCT
  NULL,                // ADST_DCT
  NULL,                // DCT_ADST
  NULL,                // ADST_ADST
  NULL,                // FLIPADST_DCT
  NULL,                // DCT_FLIPADST
  NULL,                // FLIPADST_FLIPADST
  NULL,                // ADST_FLIPADST
  NULL,                // FLIPADST_ADST
  fidentity8x32_neon,  // IDTX
  av1_fdct8x32_neon,   // V_DCT
  fidentity8x32_neon,  // H_DCT
  NULL,                // V_ADST
  NULL,                // H_ADST
  NULL,                // V_FLIPADST
  NULL                 // H_FLIPADST
};

void av1_lowbd_fwd_txfm2d_4x4_neon(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[4], buf1[4], *buf;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X4];
  const int txw_idx = get_txw_idx(TX_4X4);
  const int txh_idx = get_txh_idx(TX_4X4);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 4;
  const int height = 4;
  const transform_1d_lbd_neon col_txfm = col_txfm4x4_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm4x4_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_w4_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit_w4(input, stride, buf0, height);
  }
  round_shift_16bit_vector(buf0, height, &v_shift0);
  col_txfm(buf0, buf0, cos_bit_col, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift1);
  transpose_16bit_4x4(buf0, buf1);

  if (lr_flip) {
    buf = buf0;
    flip_buf_neon(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift2);

  store_buffer_16bit_to_32bit_w4(buf, output, height, width);
}

void av1_lowbd_fwd_txfm2d_4x8_neon(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  (void)stride;
  (void)bd;
  int16x8_t buf0[8], buf1[8], *buf;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X8];
  const int txw_idx = get_txw_idx(TX_4X8);
  const int txh_idx = get_txh_idx(TX_4X8);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 4;
  const int height = 8;
  const transform_1d_lbd_neon col_txfm = col_txfm4x8_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x4_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_w4_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit_w4(input, stride, buf0, height);
  }
  round_shift_16bit_vector(buf0, height, &v_shift0);
  col_txfm(buf0, buf0, cos_bit_col, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift1);
  transpose_16bit_4x8(buf0, buf1);

  if (lr_flip) {
    buf = buf0;
    flip_buf_neon(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift2);
  store_rect_buffer_16bit_to_32bit_w8(buf, output, height, width);
}

void av1_lowbd_fwd_txfm2d_4x16_neon(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16], buf1[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X16];
  const int txw_idx = get_txw_idx(TX_4X16);
  const int txh_idx = get_txh_idx(TX_4X16);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 4;
  const int height = 16;
  const transform_1d_lbd_neon col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x4_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_w4_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit_w4(input, stride, buf0, height);
  }
  round_shift_16bit_vector(buf0, height, &v_shift0);
  col_txfm(buf0, buf0, cos_bit_col, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift1);
  transpose_16bit_4x8(buf0, buf1);
  transpose_16bit_4x8(buf0 + 8, buf1 + 8);

  for (int i = 0; i < 2; i++) {
    int16x8_t *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_neon(buf1 + 8 * i, buf, width);
    } else {
      buf = buf1 + 8 * i;
    }
    row_txfm(buf, buf, cos_bit_row, NULL);
    round_shift_16bit_vector(buf0, height, &v_shift2);
    store_buffer_16bit_to_32bit_w8(buf, output + 8 * i, height, width);
  }
}

void av1_lowbd_fwd_txfm2d_8x4_neon(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[8], buf1[8], *buf;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X4];
  const int txw_idx = get_txw_idx(TX_8X4);
  const int txh_idx = get_txh_idx(TX_8X4);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 8;
  const int height = 4;
  const transform_1d_lbd_neon col_txfm = col_txfm8x4_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm4x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  if (ud_flip)
    load_buffer_16bit_to_16bit_flip(input, stride, buf0, height);
  else
    load_buffer_16bit_to_16bit(input, stride, buf0, height);
  round_shift_16bit_vector(buf0, height, &v_shift0);
  col_txfm(buf0, buf0, cos_bit_col, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift1);
  transpose_16bit_8x8(buf0, buf1);

  if (lr_flip) {
    buf = buf0;
    flip_buf_neon(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift2);
  store_rect_buffer_16bit_to_32bit_w4(buf, output, height, width);
}

void av1_lowbd_fwd_txfm2d_8x8_neon(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[8], buf1[8], *buf;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X8];
  const int txw_idx = get_txw_idx(TX_8X8);
  const int txh_idx = get_txh_idx(TX_8X8);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 8;
  const int height = 8;
  const transform_1d_lbd_neon col_txfm = col_txfm8x8_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  if (ud_flip)
    load_buffer_16bit_to_16bit_flip(input, stride, buf0, height);
  else
    load_buffer_16bit_to_16bit(input, stride, buf0, height);
  round_shift_16bit_vector(buf0, height, &v_shift0);
  col_txfm(buf0, buf0, cos_bit_col, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift1);
  transpose_16bit_8x8(buf0, buf1);

  if (lr_flip) {
    buf = buf0;
    flip_buf_neon(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift2);
  store_buffer_16bit_to_32bit_w8(buf, output, height, width);
}

void av1_lowbd_fwd_txfm2d_8x16_neon(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16], buf1[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X16];
  const int txw_idx = get_txw_idx(TX_8X16);
  const int txh_idx = get_txh_idx(TX_8X16);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 8;
  const int height = 16;
  const transform_1d_lbd_neon col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit(input, stride, buf0, height);
  }
  round_shift_16bit_vector(buf0, height, &v_shift0);
  col_txfm(buf0, buf0, cos_bit_col, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift1);
  transpose_16bit_8x8(buf0, buf1);
  transpose_16bit_8x8(buf0 + 8, buf1 + 8);

  for (int i = 0; i < 2; i++) {
    int16x8_t *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_neon(buf1 + width * i, buf, width);
    } else {
      buf = buf1 + width * i;
    }
    row_txfm(buf, buf, cos_bit_row, NULL);
    round_shift_16bit_vector(buf0, height, &v_shift2);
    store_rect_buffer_16bit_to_32bit_w8(buf, output + 8 * i, height, 8);
  }
}

void av1_lowbd_fwd_txfm2d_8x32_neon(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[32];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X32];
  const int txw_idx = get_txw_idx(TX_8X32);
  const int txh_idx = get_txh_idx(TX_8X32);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 8;
  const int height = 32;
  const transform_1d_lbd_neon col_txfm = col_txfm8x32_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit(input, stride, buf0, height);
  }
  round_shift_16bit_vector(buf0, height, &v_shift0);
  col_txfm(buf0, buf0, cos_bit_col, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift1);
  transpose_16bit_8x8(buf0, buf1);
  transpose_16bit_8x8(buf0 + 8, buf1 + 8);
  transpose_16bit_8x8(buf0 + 16, buf1 + 16);
  transpose_16bit_8x8(buf0 + 24, buf1 + 24);

  for (int i = 0; i < 4; i++) {
    int16x8_t *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_neon(buf1 + width * i, buf, width);
    } else {
      buf = buf1 + width * i;
    }
    row_txfm(buf, buf, cos_bit_row, NULL);
    round_shift_16bit_vector(buf0, height, &v_shift2);
    store_buffer_16bit_to_32bit_w8(buf, output + 8 * i, height, width);
  }
}

void av1_lowbd_fwd_txfm2d_16x4_neon(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16], buf1[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X4];
  const int txw_idx = get_txw_idx(TX_16X4);
  const int txh_idx = get_txh_idx(TX_16X4);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 16;
  const int height = 4;
  const transform_1d_lbd_neon col_txfm = col_txfm8x4_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x16_arr[tx_type];
  int16x8_t *buf;
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  for (int i = 0; i < 2; i++) {
    if (ud_flip) {
      load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
    } else {
      load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    }
    round_shift_16bit_vector(buf0, height, &v_shift0);
    col_txfm(buf0, buf0, cos_bit_col, NULL);
    round_shift_16bit_vector(buf0, height, &v_shift1);
    transpose_16bit_8x4(buf0, buf1 + 8 * i);
  }

  if (lr_flip) {
    buf = buf0;
    flip_buf_neon(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift2);
  store_buffer_16bit_to_32bit_w4(buf, output, height, width);
}

void av1_lowbd_fwd_txfm2d_16x8_neon(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16], buf1[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X8];
  const int txw_idx = get_txw_idx(TX_16X8);
  const int txh_idx = get_txh_idx(TX_16X8);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 16;
  const int height = 8;
  const transform_1d_lbd_neon col_txfm = col_txfm8x8_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x16_arr[tx_type];
  int16x8_t *buf;
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
  for (int i = 0; i < 2; i++) {
    if (ud_flip) {
      load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
    } else {
      load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    }
    round_shift_16bit_vector(buf0, height, &v_shift0);
    col_txfm(buf0, buf0, cos_bit_col, NULL);
    round_shift_16bit_vector(buf0, height, &v_shift1);
    transpose_16bit_8x8(buf0, buf1 + 8 * i);
  }

  if (lr_flip) {
    buf = buf0;
    flip_buf_neon(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row, NULL);
  round_shift_16bit_vector(buf0, height, &v_shift2);
  store_rect_buffer_16bit_to_32bit_w8(buf, output, height, width);
}

void av1_lowbd_fwd_txfm2d_16x16_neon(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[16], buf1[32];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X16];
  const int txw_idx = get_txw_idx(TX_16X16);
  const int txh_idx = get_txh_idx(TX_16X16);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 16;
  const int height = 16;
  const transform_1d_lbd_neon col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x16_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
  const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
  const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);

  for (int i = 0; i < 2; i++) {
    if (ud_flip) {
      load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
    } else {
      load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    }
    round_shift_16bit_vector(buf0, height, &v_shift0);
    col_txfm(buf0, buf0, cos_bit_col, NULL);
    round_shift_16bit_vector(buf0, height, &v_shift1);
    transpose_16bit_8x8(buf0, buf1 + 0 * width + 8 * i);
    transpose_16bit_8x8(buf0 + 8, buf1 + 1 * width + 8 * i);
  }

  for (int i = 0; i < 2; i++) {
    int16x8_t *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_neon(buf1 + width * i, buf, width);
    } else {
      buf = buf1 + width * i;
    }
    row_txfm(buf, buf, cos_bit_row, NULL);
    round_shift_16bit_vector(buf0, height, &v_shift2);
    store_buffer_16bit_to_32bit_w8(buf, output + 8 * i, height, width);
  }
}

void av1_lowbd_fwd_txfm2d_16x32_neon(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[64];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X32];
  const int txw_idx = get_txw_idx(TX_16X32);
  const int txh_idx = get_txh_idx(TX_16X32);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 16;
  const int height = 32;
  const transform_1d_lbd_neon col_txfm = col_txfm8x32_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x16_arr[tx_type];

  if (col_txfm != NULL && row_txfm != NULL) {
    int ud_flip, lr_flip;
    get_flip_cfg(tx_type, &ud_flip, &lr_flip);
    const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
    const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
    const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);

    for (int i = 0; i < 2; i++) {
      if (ud_flip) {
        load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
      } else {
        load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
      }
      round_shift_16bit_vector(buf0, height, &v_shift0);
      col_txfm(buf0, buf0, cos_bit_col, NULL);
      round_shift_16bit_vector(buf0, height, &v_shift1);
      transpose_16bit_8x8(buf0 + 0 * 8, buf1 + 0 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 1 * 8, buf1 + 1 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 2 * 8, buf1 + 2 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 3 * 8, buf1 + 3 * width + 8 * i);
    }

    for (int i = 0; i < 4; i++) {
      int16x8_t *buf;
      if (lr_flip) {
        buf = buf0;
        flip_buf_neon(buf1 + width * i, buf, width);
      } else {
        buf = buf1 + width * i;
      }
      row_txfm(buf, buf, cos_bit_row, NULL);
      round_shift_16bit_vector(buf0, height, &v_shift2);
      store_rect_buffer_16bit_to_32bit_w8(buf, output + 8 * i, height, width);
    }
  } else {
    av1_fwd_txfm2d_16x32_c(input, output, stride, tx_type, bd);
  }
}

void av1_lowbd_fwd_txfm2d_32x8_neon(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[32];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X8];
  const int txw_idx = get_txw_idx(TX_32X8);
  const int txh_idx = get_txh_idx(TX_32X8);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 32;
  const int height = 8;
  const transform_1d_lbd_neon col_txfm = col_txfm8x8_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x32_arr[tx_type];

  if (col_txfm != NULL && row_txfm != NULL) {
    int ud_flip, lr_flip;
    get_flip_cfg(tx_type, &ud_flip, &lr_flip);
    const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
    const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
    const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);

    for (int i = 0; i < 4; i++) {
      if (ud_flip) {
        load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
      } else {
        load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
      }
      round_shift_16bit_vector(buf0, height, &v_shift0);
      col_txfm(buf0, buf0, cos_bit_col, NULL);
      round_shift_16bit_vector(buf0, height, &v_shift1);
      transpose_16bit_8x8(buf0, buf1 + 0 * width + 8 * i);
    }

    for (int i = 0; i < 1; i++) {
      int16x8_t *buf;
      if (lr_flip) {
        buf = buf0;
        flip_buf_neon(buf1 + width * i, buf, width);
      } else {
        buf = buf1 + width * i;
      }
      row_txfm(buf, buf, cos_bit_row, NULL);
      round_shift_16bit_vector(buf, width, &v_shift2);
      store_buffer_16bit_to_32bit_w8(buf, output + 8 * i, height, width);
    }
  } else {
    av1_fwd_txfm2d_32x16_c(input, output, stride, tx_type, bd);
  }
}

void av1_lowbd_fwd_txfm2d_32x16_neon(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[64];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X16];
  const int txw_idx = get_txw_idx(TX_32X16);
  const int txh_idx = get_txh_idx(TX_32X16);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 32;
  const int height = 16;
  const transform_1d_lbd_neon col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x32_arr[tx_type];

  if (col_txfm != NULL && row_txfm != NULL) {
    const int16x8_t v_shift0 = vdupq_n_s16(shift[0]);
    const int16x8_t v_shift1 = vdupq_n_s16(shift[1]);
    const int16x8_t v_shift2 = vdupq_n_s16(shift[2]);
    int ud_flip, lr_flip;
    get_flip_cfg(tx_type, &ud_flip, &lr_flip);

    for (int i = 0; i < 4; i++) {
      if (ud_flip) {
        load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
      } else {
        load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
      }
      round_shift_16bit_vector(buf0, height, &v_shift0);
      col_txfm(buf0, buf0, cos_bit_col, NULL);
      round_shift_16bit_vector(buf0, height, &v_shift1);
      transpose_16bit_8x8(buf0, buf1 + 0 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 8, buf1 + 1 * width + 8 * i);
    }

    for (int i = 0; i < 2; i++) {
      int16x8_t *buf;
      if (lr_flip) {
        buf = buf0;
        flip_buf_neon(buf1 + width * i, buf, width);
      } else {
        buf = buf1 + width * i;
      }
      row_txfm(buf, buf, cos_bit_row, NULL);
      round_shift_16bit_vector(buf, width, &v_shift2);
      store_rect_buffer_16bit_to_32bit_w8(buf, output + 8 * i, height, width);
    }
  } else {
    av1_fwd_txfm2d_32x16_c(input, output, stride, tx_type, bd);
  }
}

void av1_lowbd_fwd_txfm2d_32x32_neon(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  int16x8_t buf0[32], buf1[128];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X32];
  const int txw_idx = get_txw_idx(TX_32X32);
  const int txh_idx = get_txh_idx(TX_32X32);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 32;
  const int height = 32;
  const transform_1d_lbd_neon col_txfm = col_txfm8x32_arr[tx_type];
  const transform_1d_lbd_neon row_txfm = row_txfm8x32_arr[tx_type];

  if (col_txfm != NULL && row_txfm != NULL) {
    int ud_flip, lr_flip;
    get_flip_cfg(tx_type, &ud_flip, &lr_flip);

    for (int i = 0; i < 4; i++) {
      if (ud_flip) {
        load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
      } else {
        load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
      }
      round_shift_16bit(buf0, height, shift[0]);
      col_txfm(buf0, buf0, cos_bit_col, NULL);
      round_shift_16bit(buf0, height, shift[1]);
      transpose_16bit_8x8(buf0 + 0 * 8, buf1 + 0 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 1 * 8, buf1 + 1 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 2 * 8, buf1 + 2 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 3 * 8, buf1 + 3 * width + 8 * i);
    }

    for (int i = 0; i < 4; i++) {
      int16x8_t *buf;
      if (lr_flip) {
        buf = buf0;
        flip_buf_neon(buf1 + width * i, buf, width);
      } else {
        buf = buf1 + width * i;
      }
      row_txfm(buf, buf, cos_bit_row, NULL);
      round_shift_16bit(buf, width, shift[2]);
      store_buffer_16bit_to_32bit_w8(buf, output + 8 * i, height, width);
    }
  } else {
    av1_fwd_txfm2d_32x32_c(input, output, stride, tx_type, bd);
  }
}

void av1_lowbd_fwd_txfm2d_64x16_neon(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_64X16;
  int16x8_t buf0[64], buf1[128];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_lbd_neon col_txfm = fdct8x16_neon;
  const transform_1d_lbd_neon row_txfm = av1_fdct8x64_neon;
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col, NULL);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < height_div8; ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }

  for (int i = 0; i < height_div8; i++) {
    int16x8_t *buf = buf1 + width * i;
    row_txfm(buf, buf, cos_bit_row, NULL);
    round_shift_16bit(buf, width, shift[2]);
    store_buffer_16bit_to_32bit_w8(buf, output + 8 * i, 16, 32);
  }
  // Zero out the bottom 16x32 area.
  memset(output + 16 * 32, 0, 16 * 32 * sizeof(*output));
}

void av1_lowbd_fwd_txfm2d_16x64_neon(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_16X64;
  int16x8_t buf0[64], buf1[128];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_lbd_neon col_txfm = av1_fdct8x64_neon;
  const transform_1d_lbd_neon row_txfm = fdct8x16_neon;
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col, NULL);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < height_div8; ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }

  for (int i = 0; i < AOMMIN(4, height_div8); i++) {
    int16x8_t *buf = buf1 + width * i;
    row_txfm(buf, buf, cos_bit_row, NULL);
    round_shift_16bit(buf, width, shift[2]);
    store_buffer_16bit_to_32bit_w8(buf, output + 8 * i, 32, 16);
  }
}

#define TRANSPOSE_4X4_L32(x0, x1, x2, x3, y0, y1, y2, y3)      \
  do {                                                         \
    int32x4x2_t temp01 = vzipq_s32(x0, x1);                    \
    int32x4x2_t temp23 = vzipq_s32(x2, x3);                    \
    int32x4x2_t y01 = vzipq_s32(temp01.val[0], temp23.val[0]); \
    int32x4x2_t y23 = vzipq_s32(temp01.val[1], temp23.val[1]); \
    y0 = y01.val[0];                                           \
    y1 = y01.val[1];                                           \
    y2 = y23.val[0];                                           \
    y3 = y23.val[1];                                           \
  } while (0)

static void av1_fdct32_new_neon(int32x4_t *input, int32x4_t *output,
                                int cos_bit, const int stride,
                                const int8_t *stage_range) {
  (void)stage_range;
  int32x4_t buf0[32];
  int32x4_t buf1[32];
  const int32_t *cospi;
  cospi = cospi_arr(cos_bit);
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
  btf_32_neon_mode0(cospi[32], cospi[32], buf1[20], buf1[27], buf0[20],
                    buf0[27], v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], buf1[21], buf1[26], buf0[21],
                    buf0[26], v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], buf1[22], buf1[25], buf0[22],
                    buf0[25], v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], buf1[23], buf1[24], buf0[23],
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
  btf_32_neon_mode0(cospi[32], cospi[32], buf0[10], buf0[13], buf1[10],
                    buf1[13], v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], buf0[11], buf0[12], buf1[11],
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
  btf_32_neon_mode0(cospi[32], cospi[32], buf1[5], buf1[6], buf0[5], buf0[6],
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
  btf_32_neon_mode0(cospi[16], cospi[48], buf1[18], buf1[29], buf0[18],
                    buf0[29], v_cos_bit);
  btf_32_neon_mode0(cospi[16], cospi[48], buf1[19], buf1[28], buf0[19],
                    buf0[28], v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], buf1[20], buf1[27], buf0[20],
                     buf0[27], v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], buf1[21], buf1[26], buf0[21],
                     buf0[26], v_cos_bit);
  buf0[22] = buf1[22];
  buf0[23] = buf1[23];
  buf0[24] = buf1[24];
  buf0[25] = buf1[25];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 5
  cospi = cospi_arr(cos_bit);
  btf_32_neon(cospi[32], cospi[32], buf0[0], buf0[1], buf1[0], buf1[1],
              v_cos_bit);
  btf_32_type1_neon(cospi[48], cospi[16], buf0[2], buf0[3], buf1[2], buf1[3],
                    v_cos_bit);
  buf1[4] = vaddq_s32(buf0[4], buf0[5]);
  buf1[5] = vsubq_s32(buf0[4], buf0[5]);
  buf1[6] = vsubq_s32(buf0[7], buf0[6]);
  buf1[7] = vaddq_s32(buf0[7], buf0[6]);
  buf1[8] = buf0[8];
  btf_32_neon_mode0(cospi[16], cospi[48], buf0[9], buf0[14], buf1[9], buf1[14],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], buf0[10], buf0[13], buf1[10],
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
  btf_32_type1_neon(cospi[56], cospi[8], buf1[4], buf1[7], buf0[4], buf0[7],
                    v_cos_bit);
  btf_32_type1_neon(cospi[24], cospi[40], buf1[5], buf1[6], buf0[5], buf0[6],
                    v_cos_bit);
  buf0[8] = vaddq_s32(buf1[8], buf1[9]);
  buf0[9] = vsubq_s32(buf1[8], buf1[9]);
  buf0[10] = vsubq_s32(buf1[11], buf1[10]);
  buf0[11] = vaddq_s32(buf1[11], buf1[10]);
  buf0[12] = vaddq_s32(buf1[12], buf1[13]);
  buf0[13] = vsubq_s32(buf1[12], buf1[13]);
  buf0[14] = vsubq_s32(buf1[15], buf1[14]);
  buf0[15] = vaddq_s32(buf1[15], buf1[14]);
  buf0[16] = buf1[16];
  btf_32_neon_mode0(cospi[8], cospi[56], buf1[17], buf1[30], buf0[17], buf0[30],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[56], cospi[8], buf1[18], buf1[29], buf0[18],
                     buf0[29], v_cos_bit);
  buf0[19] = buf1[19];
  buf0[20] = buf1[20];
  btf_32_neon_mode0(cospi[40], cospi[24], buf1[21], buf1[26], buf0[21],
                    buf0[26], v_cos_bit);
  btf_32_neon_mode01(cospi[24], cospi[40], buf1[22], buf1[25], buf0[22],
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

  btf_32_type1_neon(cospi[60], cospi[4], buf0[8], buf0[15], buf1[8], buf1[15],
                    v_cos_bit);
  btf_32_type1_neon(cospi[28], cospi[36], buf0[9], buf0[14], buf1[9], buf1[14],
                    v_cos_bit);
  btf_32_type1_neon(cospi[44], cospi[20], buf0[10], buf0[13], buf1[10],
                    buf1[13], v_cos_bit);
  btf_32_type1_neon(cospi[12], cospi[52], buf0[11], buf0[12], buf1[11],
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

  btf_32_type1_neon(cospi[62], cospi[2], buf1[16], buf1[31], buf0[16], buf0[31],
                    v_cos_bit);
  btf_32_type1_neon(cospi[30], cospi[34], buf1[17], buf1[30], buf0[17],
                    buf0[30], v_cos_bit);
  btf_32_type1_neon(cospi[46], cospi[18], buf1[18], buf1[29], buf0[18],
                    buf0[29], v_cos_bit);
  btf_32_type1_neon(cospi[14], cospi[50], buf1[19], buf1[28], buf0[19],
                    buf0[28], v_cos_bit);
  btf_32_type1_neon(cospi[54], cospi[10], buf1[20], buf1[27], buf0[20],
                    buf0[27], v_cos_bit);
  btf_32_type1_neon(cospi[22], cospi[42], buf1[21], buf1[26], buf0[21],
                    buf0[26], v_cos_bit);
  btf_32_type1_neon(cospi[38], cospi[26], buf1[22], buf1[25], buf0[22],
                    buf0[25], v_cos_bit);
  btf_32_type1_neon(cospi[6], cospi[58], buf1[23], buf1[24], buf0[23], buf0[24],
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

static void av1_fdct64_new_stage1234_neon(int32x4_t *input, const int instride,
                                          int32x4_t *x3, int32x4_t *x4,
                                          const int32_t *cospi,
                                          const int32x4_t *v_cos_bit,
                                          int *startidx, int *endidx) {
  // stage 1
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

  btf_32_neon_mode0(cospi[32], cospi[32], x1[40], x1[55], x2[40], x2[55],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x1[41], x1[54], x2[41], x2[54],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x1[42], x1[53], x2[42], x2[53],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x1[43], x1[52], x2[43], x2[52],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x1[44], x1[51], x2[44], x2[51],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x1[45], x1[50], x2[45], x2[50],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x1[46], x1[49], x2[46], x2[49],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x1[47], x1[48], x2[47], x2[48],
                    *v_cos_bit);

  // stage 3
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

  btf_32_neon_mode0(cospi[32], cospi[32], x2[20], x2[27], x3[20], x3[27],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x2[21], x2[26], x3[21], x3[26],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x2[22], x2[25], x3[22], x3[25],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x2[23], x2[24], x3[23], x3[24],
                    *v_cos_bit);

  x3[32] = vaddq_s32(x1[32], x2[47]);
  x3[47] = vsubq_s32(x1[32], x2[47]);
  x3[33] = vaddq_s32(x1[33], x2[46]);
  x3[46] = vsubq_s32(x1[33], x2[46]);
  x3[34] = vaddq_s32(x1[34], x2[45]);
  x3[45] = vsubq_s32(x1[34], x2[45]);
  x3[35] = vaddq_s32(x1[35], x2[44]);
  x3[44] = vsubq_s32(x1[35], x2[44]);
  x3[36] = vaddq_s32(x1[36], x2[43]);
  x3[43] = vsubq_s32(x1[36], x2[43]);
  x3[37] = vaddq_s32(x1[37], x2[42]);
  x3[42] = vsubq_s32(x1[37], x2[42]);
  x3[38] = vaddq_s32(x1[38], x2[41]);
  x3[41] = vsubq_s32(x1[38], x2[41]);
  x3[39] = vaddq_s32(x1[39], x2[40]);
  x3[40] = vsubq_s32(x1[39], x2[40]);
  x3[48] = vsubq_s32(x1[63], x2[48]);
  x3[63] = vaddq_s32(x1[63], x2[48]);
  x3[49] = vsubq_s32(x1[62], x2[49]);
  x3[62] = vaddq_s32(x1[62], x2[49]);
  x3[50] = vsubq_s32(x1[61], x2[50]);
  x3[61] = vaddq_s32(x1[61], x2[50]);
  x3[51] = vsubq_s32(x1[60], x2[51]);
  x3[60] = vaddq_s32(x1[60], x2[51]);
  x3[52] = vsubq_s32(x1[59], x2[52]);
  x3[59] = vaddq_s32(x1[59], x2[52]);
  x3[53] = vsubq_s32(x1[58], x2[53]);
  x3[58] = vaddq_s32(x1[58], x2[53]);
  x3[54] = vsubq_s32(x1[57], x2[54]);
  x3[57] = vaddq_s32(x1[57], x2[54]);
  x3[55] = vsubq_s32(x1[56], x2[55]);
  x3[56] = vaddq_s32(x1[56], x2[55]);

  // stage 4
  x4[0] = vaddq_s32(x3[0], x3[7]);
  x4[7] = vsubq_s32(x3[0], x3[7]);
  x4[1] = vaddq_s32(x3[1], x3[6]);
  x4[6] = vsubq_s32(x3[1], x3[6]);
  x4[2] = vaddq_s32(x3[2], x3[5]);
  x4[5] = vsubq_s32(x3[2], x3[5]);
  x4[3] = vaddq_s32(x3[3], x3[4]);
  x4[4] = vsubq_s32(x3[3], x3[4]);

  btf_32_neon_mode0(cospi[32], cospi[32], x3[10], x3[13], x4[10], x4[13],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[32], cospi[32], x3[11], x3[12], x4[11], x4[12],
                    *v_cos_bit);

  x4[16] = vaddq_s32(x2[16], x3[23]);
  x4[23] = vsubq_s32(x2[16], x3[23]);
  x4[17] = vaddq_s32(x2[17], x3[22]);
  x4[22] = vsubq_s32(x2[17], x3[22]);
  x4[18] = vaddq_s32(x2[18], x3[21]);
  x4[21] = vsubq_s32(x2[18], x3[21]);
  x4[19] = vaddq_s32(x2[19], x3[20]);
  x4[20] = vsubq_s32(x2[19], x3[20]);
  x4[24] = vsubq_s32(x2[31], x3[24]);
  x4[31] = vaddq_s32(x2[31], x3[24]);
  x4[25] = vsubq_s32(x2[30], x3[25]);
  x4[30] = vaddq_s32(x2[30], x3[25]);
  x4[26] = vsubq_s32(x2[29], x3[26]);
  x4[29] = vaddq_s32(x2[29], x3[26]);
  x4[27] = vsubq_s32(x2[28], x3[27]);
  x4[28] = vaddq_s32(x2[28], x3[27]);

  btf_32_neon_mode0(cospi[16], cospi[48], x3[36], x3[59], x4[36], x4[59],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[16], cospi[48], x3[37], x3[58], x4[37], x4[58],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[16], cospi[48], x3[38], x3[57], x4[38], x4[57],
                    *v_cos_bit);
  btf_32_neon_mode0(cospi[16], cospi[48], x3[39], x3[56], x4[39], x4[56],
                    *v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], x3[40], x3[55], x4[40], x4[55],
                     *v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], x3[41], x3[54], x4[41], x4[54],
                     *v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], x3[42], x3[53], x4[42], x4[53],
                     *v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], x3[43], x3[52], x4[43], x4[52],
                     *v_cos_bit);
}

static void av1_fdct64_new_neon(int32x4_t *input, int32x4_t *output,
                                int8_t cos_bit, const int instride,
                                const int outstride,
                                const int8_t *stage_range) {
  (void)stage_range;
  const int32_t *cospi = cospi_arr(cos_bit);
  const int32x4_t v_cos_bit = vdupq_n_s32(-cos_bit);

  int startidx = 0 * instride;
  int endidx = 63 * instride;

  // stage 1-2-3-4
  int32x4_t x3[64], x4[64];
  av1_fdct64_new_stage1234_neon(input, instride, x3, x4, cospi, &v_cos_bit,
                                &startidx, &endidx);

  // stage 5
  int32x4_t x5[64];
  x5[0] = vaddq_s32(x4[0], x4[3]);
  x5[3] = vsubq_s32(x4[0], x4[3]);
  x5[1] = vaddq_s32(x4[1], x4[2]);
  x5[2] = vsubq_s32(x4[1], x4[2]);

  btf_32_neon_mode0(cospi[32], cospi[32], x4[5], x4[6], x5[5], x5[6],
                    v_cos_bit);

  x5[8] = vaddq_s32(x3[8], x4[11]);
  x5[11] = vsubq_s32(x3[8], x4[11]);
  x5[9] = vaddq_s32(x3[9], x4[10]);
  x5[10] = vsubq_s32(x3[9], x4[10]);
  x5[12] = vsubq_s32(x3[15], x4[12]);
  x5[15] = vaddq_s32(x3[15], x4[12]);
  x5[13] = vsubq_s32(x3[14], x4[13]);
  x5[14] = vaddq_s32(x3[14], x4[13]);

  btf_32_neon_mode0(cospi[16], cospi[48], x4[18], x4[29], x5[18], x5[29],
                    v_cos_bit);
  btf_32_neon_mode0(cospi[16], cospi[48], x4[19], x4[28], x5[19], x5[28],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], x4[20], x4[27], x5[20], x5[27],
                     v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], x4[21], x4[26], x5[21], x5[26],
                     v_cos_bit);

  x5[32] = vaddq_s32(x3[32], x4[39]);
  x5[39] = vsubq_s32(x3[32], x4[39]);
  x5[33] = vaddq_s32(x3[33], x4[38]);
  x5[38] = vsubq_s32(x3[33], x4[38]);
  x5[34] = vaddq_s32(x3[34], x4[37]);
  x5[37] = vsubq_s32(x3[34], x4[37]);
  x5[35] = vaddq_s32(x3[35], x4[36]);
  x5[36] = vsubq_s32(x3[35], x4[36]);
  x5[40] = vsubq_s32(x3[47], x4[40]);
  x5[47] = vaddq_s32(x3[47], x4[40]);
  x5[41] = vsubq_s32(x3[46], x4[41]);
  x5[46] = vaddq_s32(x3[46], x4[41]);
  x5[42] = vsubq_s32(x3[45], x4[42]);
  x5[45] = vaddq_s32(x3[45], x4[42]);
  x5[43] = vsubq_s32(x3[44], x4[43]);
  x5[44] = vaddq_s32(x3[44], x4[43]);
  x5[48] = vaddq_s32(x3[48], x4[55]);
  x5[55] = vsubq_s32(x3[48], x4[55]);
  x5[49] = vaddq_s32(x3[49], x4[54]);
  x5[54] = vsubq_s32(x3[49], x4[54]);
  x5[50] = vaddq_s32(x3[50], x4[53]);
  x5[53] = vsubq_s32(x3[50], x4[53]);
  x5[51] = vaddq_s32(x3[51], x4[52]);
  x5[52] = vsubq_s32(x3[51], x4[52]);
  x5[56] = vsubq_s32(x3[63], x4[56]);
  x5[63] = vaddq_s32(x3[63], x4[56]);
  x5[57] = vsubq_s32(x3[62], x4[57]);
  x5[62] = vaddq_s32(x3[62], x4[57]);
  x5[58] = vsubq_s32(x3[61], x4[58]);
  x5[61] = vaddq_s32(x3[61], x4[58]);
  x5[59] = vsubq_s32(x3[60], x4[59]);
  x5[60] = vaddq_s32(x3[60], x4[59]);

  // stage 6
  int32x4_t x6[64];
  btf_32_neon(cospi[32], cospi[32], x5[0], x5[1], x6[0], x6[1], v_cos_bit);
  btf_32_type1_neon(cospi[48], cospi[16], x5[2], x5[3], x6[2], x6[3],
                    v_cos_bit);
  x6[4] = vaddq_s32(x4[4], x5[5]);
  x6[5] = vsubq_s32(x4[4], x5[5]);
  x6[6] = vsubq_s32(x4[7], x5[6]);
  x6[7] = vaddq_s32(x4[7], x5[6]);
  btf_32_neon_mode0(cospi[16], cospi[48], x5[9], x5[14], x6[9], x6[14],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[48], cospi[16], x5[10], x5[13], x6[10], x6[13],
                     v_cos_bit);

  x6[16] = vaddq_s32(x4[16], x5[19]);
  x6[19] = vsubq_s32(x4[16], x5[19]);
  x6[17] = vaddq_s32(x4[17], x5[18]);
  x6[18] = vsubq_s32(x4[17], x5[18]);
  x6[20] = vsubq_s32(x4[23], x5[20]);
  x6[23] = vaddq_s32(x4[23], x5[20]);
  x6[21] = vsubq_s32(x4[22], x5[21]);
  x6[22] = vaddq_s32(x4[22], x5[21]);
  x6[24] = vaddq_s32(x4[24], x5[27]);
  x6[27] = vsubq_s32(x4[24], x5[27]);
  x6[25] = vaddq_s32(x4[25], x5[26]);
  x6[26] = vsubq_s32(x4[25], x5[26]);
  x6[28] = vsubq_s32(x4[31], x5[28]);
  x6[31] = vaddq_s32(x4[31], x5[28]);
  x6[29] = vsubq_s32(x4[30], x5[29]);
  x6[30] = vaddq_s32(x4[30], x5[29]);

  btf_32_neon_mode0(cospi[8], cospi[56], x5[34], x5[61], x6[34], x6[61],
                    v_cos_bit);
  btf_32_neon_mode0(cospi[8], cospi[56], x5[35], x5[60], x6[35], x6[60],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[56], cospi[8], x5[36], x5[59], x6[36], x6[59],
                     v_cos_bit);
  btf_32_neon_mode01(cospi[56], cospi[8], x5[37], x5[58], x6[37], x6[58],
                     v_cos_bit);
  btf_32_neon_mode0(cospi[40], cospi[24], x5[42], x5[53], x6[42], x6[53],
                    v_cos_bit);
  btf_32_neon_mode0(cospi[40], cospi[24], x5[43], x5[52], x6[43], x6[52],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[24], cospi[40], x5[44], x5[51], x6[44], x6[51],
                     v_cos_bit);
  btf_32_neon_mode01(cospi[24], cospi[40], x5[45], x5[50], x6[45], x6[50],
                     v_cos_bit);

  // stage 7
  int32x4_t x7[64];

  btf_32_type1_neon(cospi[56], cospi[8], x6[4], x6[7], x7[4], x7[7], v_cos_bit);
  btf_32_type1_neon(cospi[24], cospi[40], x6[5], x6[6], x7[5], x7[6],
                    v_cos_bit);
  x7[8] = vaddq_s32(x5[8], x6[9]);
  x7[9] = vsubq_s32(x5[8], x6[9]);
  x7[10] = vsubq_s32(x5[11], x6[10]);
  x7[11] = vaddq_s32(x5[11], x6[10]);
  x7[12] = vaddq_s32(x5[12], x6[13]);
  x7[13] = vsubq_s32(x5[12], x6[13]);
  x7[14] = vsubq_s32(x5[15], x6[14]);
  x7[15] = vaddq_s32(x5[15], x6[14]);

  btf_32_neon_mode0(cospi[8], cospi[56], x6[17], x6[30], x7[17], x7[30],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[56], cospi[8], x6[18], x6[29], x7[18], x7[29],
                     v_cos_bit);

  btf_32_neon_mode0(cospi[40], cospi[24], x6[21], x6[26], x7[21], x7[26],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[24], cospi[40], x6[22], x6[25], x7[22], x7[25],
                     v_cos_bit);

  x7[32] = vaddq_s32(x5[32], x6[35]);
  x7[35] = vsubq_s32(x5[32], x6[35]);
  x7[33] = vaddq_s32(x5[33], x6[34]);
  x7[34] = vsubq_s32(x5[33], x6[34]);
  x7[36] = vsubq_s32(x5[39], x6[36]);
  x7[39] = vaddq_s32(x5[39], x6[36]);
  x7[37] = vsubq_s32(x5[38], x6[37]);
  x7[38] = vaddq_s32(x5[38], x6[37]);
  x7[40] = vaddq_s32(x5[40], x6[43]);
  x7[43] = vsubq_s32(x5[40], x6[43]);
  x7[41] = vaddq_s32(x5[41], x6[42]);
  x7[42] = vsubq_s32(x5[41], x6[42]);
  x7[44] = vsubq_s32(x5[47], x6[44]);
  x7[47] = vaddq_s32(x5[47], x6[44]);
  x7[45] = vsubq_s32(x5[46], x6[45]);
  x7[46] = vaddq_s32(x5[46], x6[45]);
  x7[48] = vaddq_s32(x5[48], x6[51]);
  x7[51] = vsubq_s32(x5[48], x6[51]);
  x7[49] = vaddq_s32(x5[49], x6[50]);
  x7[50] = vsubq_s32(x5[49], x6[50]);
  x7[52] = vsubq_s32(x5[55], x6[52]);
  x7[55] = vaddq_s32(x5[55], x6[52]);
  x7[53] = vsubq_s32(x5[54], x6[53]);
  x7[54] = vaddq_s32(x5[54], x6[53]);
  x7[56] = vaddq_s32(x5[56], x6[59]);
  x7[59] = vsubq_s32(x5[56], x6[59]);
  x7[57] = vaddq_s32(x5[57], x6[58]);
  x7[58] = vsubq_s32(x5[57], x6[58]);
  x7[60] = vsubq_s32(x5[63], x6[60]);
  x7[63] = vaddq_s32(x5[63], x6[60]);
  x7[61] = vsubq_s32(x5[62], x6[61]);
  x7[62] = vaddq_s32(x5[62], x6[61]);

  // stage 8
  int32x4_t x8[64];

  btf_32_type1_neon(cospi[60], cospi[4], x7[8], x7[15], x8[8], x8[15],
                    v_cos_bit);
  btf_32_type1_neon(cospi[28], cospi[36], x7[9], x7[14], x8[9], x8[14],
                    v_cos_bit);
  btf_32_type1_neon(cospi[44], cospi[20], x7[10], x7[13], x8[10], x8[13],
                    v_cos_bit);
  btf_32_type1_neon(cospi[12], cospi[52], x7[11], x7[12], x8[11], x8[12],
                    v_cos_bit);
  x8[16] = vaddq_s32(x6[16], x7[17]);
  x8[17] = vsubq_s32(x6[16], x7[17]);
  x8[18] = vsubq_s32(x6[19], x7[18]);
  x8[19] = vaddq_s32(x6[19], x7[18]);
  x8[20] = vaddq_s32(x6[20], x7[21]);
  x8[21] = vsubq_s32(x6[20], x7[21]);
  x8[22] = vsubq_s32(x6[23], x7[22]);
  x8[23] = vaddq_s32(x6[23], x7[22]);
  x8[24] = vaddq_s32(x6[24], x7[25]);
  x8[25] = vsubq_s32(x6[24], x7[25]);
  x8[26] = vsubq_s32(x6[27], x7[26]);
  x8[27] = vaddq_s32(x6[27], x7[26]);
  x8[28] = vaddq_s32(x6[28], x7[29]);
  x8[29] = vsubq_s32(x6[28], x7[29]);
  x8[30] = vsubq_s32(x6[31], x7[30]);
  x8[31] = vaddq_s32(x6[31], x7[30]);

  btf_32_neon_mode0(cospi[4], cospi[60], x7[33], x7[62], x8[33], x8[62],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[60], cospi[4], x7[34], x7[61], x8[34], x8[61],
                     v_cos_bit);
  btf_32_neon_mode0(cospi[36], cospi[28], x7[37], x7[58], x8[37], x8[58],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[28], cospi[36], x7[38], x7[57], x8[38], x8[57],
                     v_cos_bit);
  btf_32_neon_mode0(cospi[20], cospi[44], x7[41], x7[54], x8[41], x8[54],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[44], cospi[20], x7[42], x7[53], x8[42], x8[53],
                     v_cos_bit);
  btf_32_neon_mode0(cospi[52], cospi[12], x7[45], x7[50], x8[45], x8[50],
                    v_cos_bit);
  btf_32_neon_mode01(cospi[12], cospi[52], x7[46], x7[49], x8[46], x8[49],
                     v_cos_bit);

  // stage 9
  int32x4_t x9[64];

  btf_32_type1_neon(cospi[62], cospi[2], x8[16], x8[31], x9[16], x9[31],
                    v_cos_bit);
  btf_32_type1_neon(cospi[30], cospi[34], x8[17], x8[30], x9[17], x9[30],
                    v_cos_bit);
  btf_32_type1_neon(cospi[46], cospi[18], x8[18], x8[29], x9[18], x9[29],
                    v_cos_bit);
  btf_32_type1_neon(cospi[14], cospi[50], x8[19], x8[28], x9[19], x9[28],
                    v_cos_bit);
  btf_32_type1_neon(cospi[54], cospi[10], x8[20], x8[27], x9[20], x9[27],
                    v_cos_bit);
  btf_32_type1_neon(cospi[22], cospi[42], x8[21], x8[26], x9[21], x9[26],
                    v_cos_bit);
  btf_32_type1_neon(cospi[38], cospi[26], x8[22], x8[25], x9[22], x9[25],
                    v_cos_bit);
  btf_32_type1_neon(cospi[6], cospi[58], x8[23], x8[24], x9[23], x9[24],
                    v_cos_bit);
  x9[32] = vaddq_s32(x7[32], x8[33]);
  x9[33] = vsubq_s32(x7[32], x8[33]);
  x9[34] = vsubq_s32(x7[35], x8[34]);
  x9[35] = vaddq_s32(x7[35], x8[34]);
  x9[36] = vaddq_s32(x7[36], x8[37]);
  x9[37] = vsubq_s32(x7[36], x8[37]);
  x9[38] = vsubq_s32(x7[39], x8[38]);
  x9[39] = vaddq_s32(x7[39], x8[38]);
  x9[40] = vaddq_s32(x7[40], x8[41]);
  x9[41] = vsubq_s32(x7[40], x8[41]);
  x9[42] = vsubq_s32(x7[43], x8[42]);
  x9[43] = vaddq_s32(x7[43], x8[42]);
  x9[44] = vaddq_s32(x7[44], x8[45]);
  x9[45] = vsubq_s32(x7[44], x8[45]);
  x9[46] = vsubq_s32(x7[47], x8[46]);
  x9[47] = vaddq_s32(x7[47], x8[46]);
  x9[48] = vaddq_s32(x7[48], x8[49]);
  x9[49] = vsubq_s32(x7[48], x8[49]);
  x9[50] = vsubq_s32(x7[51], x8[50]);
  x9[51] = vaddq_s32(x7[51], x8[50]);
  x9[52] = vaddq_s32(x7[52], x8[53]);
  x9[53] = vsubq_s32(x7[52], x8[53]);
  x9[54] = vsubq_s32(x7[55], x8[54]);
  x9[55] = vaddq_s32(x7[55], x8[54]);
  x9[56] = vaddq_s32(x7[56], x8[57]);
  x9[57] = vsubq_s32(x7[56], x8[57]);
  x9[58] = vsubq_s32(x7[59], x8[58]);
  x9[59] = vaddq_s32(x7[59], x8[58]);
  x9[60] = vaddq_s32(x7[60], x8[61]);
  x9[61] = vsubq_s32(x7[60], x8[61]);
  x9[62] = vsubq_s32(x7[63], x8[62]);
  x9[63] = vaddq_s32(x7[63], x8[62]);

  // stage 10
  int32x4_t x10[64];

  btf_32_type1_neon(cospi[63], cospi[1], x9[32], x9[63], x10[32], x10[63],
                    v_cos_bit);
  btf_32_type1_neon(cospi[31], cospi[33], x9[33], x9[62], x10[33], x10[62],
                    v_cos_bit);
  btf_32_type1_neon(cospi[47], cospi[17], x9[34], x9[61], x10[34], x10[61],
                    v_cos_bit);
  btf_32_type1_neon(cospi[15], cospi[49], x9[35], x9[60], x10[35], x10[60],
                    v_cos_bit);
  btf_32_type1_neon(cospi[55], cospi[9], x9[36], x9[59], x10[36], x10[59],
                    v_cos_bit);
  btf_32_type1_neon(cospi[23], cospi[41], x9[37], x9[58], x10[37], x10[58],
                    v_cos_bit);
  btf_32_type1_neon(cospi[39], cospi[25], x9[38], x9[57], x10[38], x10[57],
                    v_cos_bit);
  btf_32_type1_neon(cospi[7], cospi[57], x9[39], x9[56], x10[39], x10[56],
                    v_cos_bit);
  btf_32_type1_neon(cospi[59], cospi[5], x9[40], x9[55], x10[40], x10[55],
                    v_cos_bit);
  btf_32_type1_neon(cospi[27], cospi[37], x9[41], x9[54], x10[41], x10[54],
                    v_cos_bit);
  btf_32_type1_neon(cospi[43], cospi[21], x9[42], x9[53], x10[42], x10[53],
                    v_cos_bit);
  btf_32_type1_neon(cospi[11], cospi[53], x9[43], x9[52], x10[43], x10[52],
                    v_cos_bit);
  btf_32_type1_neon(cospi[51], cospi[13], x9[44], x9[51], x10[44], x10[51],
                    v_cos_bit);
  btf_32_type1_neon(cospi[19], cospi[45], x9[45], x9[50], x10[45], x10[50],
                    v_cos_bit);
  btf_32_type1_neon(cospi[35], cospi[29], x9[46], x9[49], x10[46], x10[49],
                    v_cos_bit);
  btf_32_type1_neon(cospi[3], cospi[61], x9[47], x9[48], x10[47], x10[48],
                    v_cos_bit);

  startidx = 0 * outstride;
  endidx = 63 * outstride;
  // stage 11
  output[startidx] = x6[0];
  output[endidx] = x10[63];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[32];
  output[endidx] = x9[31];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x9[16];
  output[endidx] = x10[47];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[48];
  output[endidx] = x8[15];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x8[8];
  output[endidx] = x10[55];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[40];
  output[endidx] = x9[23];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x9[24];
  output[endidx] = x10[39];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[56];
  output[endidx] = x7[7];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x7[4];
  output[endidx] = x10[59];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[36];
  output[endidx] = x9[27];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x9[20];
  output[endidx] = x10[43];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[52];
  output[endidx] = x8[11];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x8[12];
  output[endidx] = x10[51];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[44];
  output[endidx] = x9[19];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x9[28];
  output[endidx] = x10[35];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[60];
  output[endidx] = x6[3];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x6[2];
  output[endidx] = x10[61];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[34];
  output[endidx] = x9[29];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x9[18];
  output[endidx] = x10[45];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[50];
  output[endidx] = x8[13];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x8[10];
  output[endidx] = x10[53];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[42];
  output[endidx] = x9[21];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x9[26];
  output[endidx] = x10[37];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[58];
  output[endidx] = x7[5];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x7[6];
  output[endidx] = x10[57];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[38];
  output[endidx] = x9[25];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x9[22];
  output[endidx] = x10[41];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[54];
  output[endidx] = x8[9];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x8[14];
  output[endidx] = x10[49];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[46];
  output[endidx] = x9[17];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x9[30];
  output[endidx] = x10[33];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x10[62];
  output[endidx] = x6[1];
}

static void av1_lowbd_fwd_txfm2d_64x64_neon(const int16_t *input,
                                            int32_t *output, int stride,
                                            TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_64X64;
  int16x8_t buf0[64], buf1[512];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_lbd_neon col_txfm = av1_fdct8x64_neon;
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col, NULL);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(4, height_div8); ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }
  for (int i = 0; i < AOMMIN(4, height_div8); i++) {
    int32x4_t bufA[64];
    int32x4_t bufB[64];
    int16x8_t *buf = buf1 + width * i;
    for (int j = 0; j < width; ++j) {
      bufA[j] = vmovl_s16(vget_low_s16(buf[j]));
      bufB[j] = vmovl_s16(vget_high_s16(buf[j]));
    }
    av1_fdct64_new_neon(bufA, bufA, cos_bit_row, 1, 1, NULL);
    av1_fdct64_new_neon(bufB, bufB, cos_bit_row, 1, 1, NULL);
    av1_round_shift_array_32_neon(bufA, bufA, 32);
    av1_round_shift_array_32_neon(bufB, bufB, 32);

    store_output_32bit_w8(output + i * 8, bufA, bufB, 32, 32);
  }
}
static void av1_lowbd_fwd_txfm2d_64x32_neon(const int16_t *input,
                                            int32_t *output, int stride,
                                            TX_TYPE tx_type, int bd) {
  (void)bd;
  const TX_SIZE tx_size = TX_64X32;
  int16x8_t buf0[64], buf1[256];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_lbd_neon col_txfm = col_txfm8x32_arr[tx_type];
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col, NULL);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(4, height_div8); ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }
  assert(tx_type == DCT_DCT);
  for (int i = 0; i < AOMMIN(4, height_div8); i++) {
    int32x4_t bufA[64];
    int32x4_t bufB[64];
    int16x8_t *buf = buf1 + width * i;
    for (int j = 0; j < width; ++j) {
      bufA[j] = vmovl_s16(vget_low_s16(buf[j]));
      bufB[j] = vmovl_s16(vget_high_s16(buf[j]));
    }
    av1_fdct64_new_neon(bufA, bufA, cos_bit_row, 1, 1, NULL);
    av1_fdct64_new_neon(bufB, bufB, cos_bit_row, 1, 1, NULL);
    av1_round_shift_rect_array_32_neon(bufA, bufA, 32);
    av1_round_shift_rect_array_32_neon(bufB, bufB, 32);

    store_output_32bit_w8(output + i * 8, bufA, bufB, 32, 32);
  }
}

static void av1_lowbd_fwd_txfm2d_32x64_neon(const int16_t *input,
                                            int32_t *output, int stride,
                                            TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_32X64;
  int16x8_t buf0[64], buf1[256];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_lbd_neon col_txfm = av1_fdct8x64_neon;
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col, NULL);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < AOMMIN(4, height_div8); ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }

  for (int i = 0; i < AOMMIN(4, height_div8); i++) {
    int32x4_t bufA[32];
    int32x4_t bufB[32];
    int16x8_t *buf = buf1 + width * i;
    for (int j = 0; j < width; ++j) {
      bufA[j] = vmovl_s16(vget_low_s16(buf[j]));
      bufB[j] = vmovl_s16(vget_high_s16(buf[j]));
    }
    av1_fdct32_new_neon(bufA, bufA, cos_bit_row, 1, NULL);
    av1_fdct32_new_neon(bufB, bufB, cos_bit_row, 1, NULL);
    av1_round_shift_rect_array_32_neon(bufA, bufA, 32);
    av1_round_shift_rect_array_32_neon(bufB, bufB, 32);

    store_output_32bit_w8(output + i * 8, bufA, bufB, 32, 32);
  }
}

static FwdTxfm2dFunc lowbd_fwd_txfm_func_ls[TX_SIZES_ALL] = {
  av1_lowbd_fwd_txfm2d_4x4_neon,    // 4x4 transform
  av1_lowbd_fwd_txfm2d_8x8_neon,    // 8x8 transform
  av1_lowbd_fwd_txfm2d_16x16_neon,  // 16x16 transform
  av1_lowbd_fwd_txfm2d_32x32_neon,  // 32x32 transform
  av1_lowbd_fwd_txfm2d_64x64_neon,  // 64x64 transform
  av1_lowbd_fwd_txfm2d_4x8_neon,    // 4x8 transform
  av1_lowbd_fwd_txfm2d_8x4_neon,    // 8x4 transform
  av1_lowbd_fwd_txfm2d_8x16_neon,   // 8x16 transform
  av1_lowbd_fwd_txfm2d_16x8_neon,   // 16x8 transform
  av1_lowbd_fwd_txfm2d_16x32_neon,  // 16x32 transform
  av1_lowbd_fwd_txfm2d_32x16_neon,  // 32x16 transform
  av1_lowbd_fwd_txfm2d_32x64_neon,  // 32x64 transform
  av1_lowbd_fwd_txfm2d_64x32_neon,  // 64x32 transform
  av1_lowbd_fwd_txfm2d_4x16_neon,   // 4x16 transform
  av1_lowbd_fwd_txfm2d_16x4_neon,   // 16x4 transform
  av1_lowbd_fwd_txfm2d_8x32_neon,   // 8x32 transform
  av1_lowbd_fwd_txfm2d_32x8_neon,   // 32x8 transform
  av1_lowbd_fwd_txfm2d_16x64_neon,  // 16x64 transform
  av1_lowbd_fwd_txfm2d_64x16_neon,  // 64x16 transform
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
