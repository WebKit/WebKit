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
#include <assert.h>
#include <immintrin.h> /*AVX2*/

#include "config/aom_config.h"
#include "config/av1_rtcd.h"
#include "av1/common/av1_txfm.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "aom_dsp/txfm_common.h"
#include "aom_ports/mem.h"
#include "aom_dsp/x86/txfm_common_sse2.h"
#include "aom_dsp/x86/txfm_common_avx2.h"

static INLINE void load_buffer_8x8_avx2(const int16_t *input, __m256i *out,
                                        int stride, int flipud, int fliplr,
                                        int shift) {
  __m128i out1[8];
  if (!flipud) {
    out1[0] = _mm_load_si128((const __m128i *)(input + 0 * stride));
    out1[1] = _mm_load_si128((const __m128i *)(input + 1 * stride));
    out1[2] = _mm_load_si128((const __m128i *)(input + 2 * stride));
    out1[3] = _mm_load_si128((const __m128i *)(input + 3 * stride));
    out1[4] = _mm_load_si128((const __m128i *)(input + 4 * stride));
    out1[5] = _mm_load_si128((const __m128i *)(input + 5 * stride));
    out1[6] = _mm_load_si128((const __m128i *)(input + 6 * stride));
    out1[7] = _mm_load_si128((const __m128i *)(input + 7 * stride));

  } else {
    out1[7] = _mm_load_si128((const __m128i *)(input + 0 * stride));
    out1[6] = _mm_load_si128((const __m128i *)(input + 1 * stride));
    out1[5] = _mm_load_si128((const __m128i *)(input + 2 * stride));
    out1[4] = _mm_load_si128((const __m128i *)(input + 3 * stride));
    out1[3] = _mm_load_si128((const __m128i *)(input + 4 * stride));
    out1[2] = _mm_load_si128((const __m128i *)(input + 5 * stride));
    out1[1] = _mm_load_si128((const __m128i *)(input + 6 * stride));
    out1[0] = _mm_load_si128((const __m128i *)(input + 7 * stride));
  }
  if (!fliplr) {
    out[0] = _mm256_cvtepi16_epi32(out1[0]);
    out[1] = _mm256_cvtepi16_epi32(out1[1]);
    out[2] = _mm256_cvtepi16_epi32(out1[2]);
    out[3] = _mm256_cvtepi16_epi32(out1[3]);
    out[4] = _mm256_cvtepi16_epi32(out1[4]);
    out[5] = _mm256_cvtepi16_epi32(out1[5]);
    out[6] = _mm256_cvtepi16_epi32(out1[6]);
    out[7] = _mm256_cvtepi16_epi32(out1[7]);

  } else {
    out[0] = _mm256_cvtepi16_epi32(mm_reverse_epi16(out1[0]));
    out[1] = _mm256_cvtepi16_epi32(mm_reverse_epi16(out1[1]));
    out[2] = _mm256_cvtepi16_epi32(mm_reverse_epi16(out1[2]));
    out[3] = _mm256_cvtepi16_epi32(mm_reverse_epi16(out1[3]));
    out[4] = _mm256_cvtepi16_epi32(mm_reverse_epi16(out1[4]));
    out[5] = _mm256_cvtepi16_epi32(mm_reverse_epi16(out1[5]));
    out[6] = _mm256_cvtepi16_epi32(mm_reverse_epi16(out1[6]));
    out[7] = _mm256_cvtepi16_epi32(mm_reverse_epi16(out1[7]));
  }
  out[0] = _mm256_slli_epi32(out[0], shift);
  out[1] = _mm256_slli_epi32(out[1], shift);
  out[2] = _mm256_slli_epi32(out[2], shift);
  out[3] = _mm256_slli_epi32(out[3], shift);
  out[4] = _mm256_slli_epi32(out[4], shift);
  out[5] = _mm256_slli_epi32(out[5], shift);
  out[6] = _mm256_slli_epi32(out[6], shift);
  out[7] = _mm256_slli_epi32(out[7], shift);
}
static INLINE void col_txfm_8x8_rounding(__m256i *in, int shift) {
  const __m256i rounding = _mm256_set1_epi32(1 << (shift - 1));

  in[0] = _mm256_add_epi32(in[0], rounding);
  in[1] = _mm256_add_epi32(in[1], rounding);
  in[2] = _mm256_add_epi32(in[2], rounding);
  in[3] = _mm256_add_epi32(in[3], rounding);
  in[4] = _mm256_add_epi32(in[4], rounding);
  in[5] = _mm256_add_epi32(in[5], rounding);
  in[6] = _mm256_add_epi32(in[6], rounding);
  in[7] = _mm256_add_epi32(in[7], rounding);

  in[0] = _mm256_srai_epi32(in[0], shift);
  in[1] = _mm256_srai_epi32(in[1], shift);
  in[2] = _mm256_srai_epi32(in[2], shift);
  in[3] = _mm256_srai_epi32(in[3], shift);
  in[4] = _mm256_srai_epi32(in[4], shift);
  in[5] = _mm256_srai_epi32(in[5], shift);
  in[6] = _mm256_srai_epi32(in[6], shift);
  in[7] = _mm256_srai_epi32(in[7], shift);
}
static INLINE void load_buffer_8x16_avx2(const int16_t *input, __m256i *out,
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
  load_buffer_8x8_avx2(topL, out, stride, flipud, fliplr, shift);
  load_buffer_8x8_avx2(botL, out + 8, stride, flipud, fliplr, shift);
}
static INLINE void load_buffer_16xn_avx2(const int16_t *input, __m256i *out,
                                         int stride, int height, int outstride,
                                         int flipud, int fliplr) {
  __m256i out1[64];
  if (!flipud) {
    for (int i = 0; i < height; i++) {
      out1[i] = _mm256_loadu_si256((const __m256i *)(input + i * stride));
    }
  } else {
    for (int i = 0; i < height; i++) {
      out1[(height - 1) - i] =
          _mm256_loadu_si256((const __m256i *)(input + i * stride));
    }
  }
  if (!fliplr) {
    for (int i = 0; i < height; i++) {
      out[i * outstride] =
          _mm256_cvtepi16_epi32(_mm256_castsi256_si128(out1[i]));
      out[i * outstride + 1] =
          _mm256_cvtepi16_epi32(_mm256_extractf128_si256(out1[i], 1));
    }
  } else {
    for (int i = 0; i < height; i++) {
      out[i * outstride + 1] = _mm256_cvtepi16_epi32(
          mm_reverse_epi16(_mm256_castsi256_si128(out1[i])));
      out[i * outstride + 0] = _mm256_cvtepi16_epi32(
          mm_reverse_epi16(_mm256_extractf128_si256(out1[i], 1)));
    }
  }
}

static void fwd_txfm_transpose_8x8_avx2(const __m256i *in, __m256i *out,
                                        const int instride,
                                        const int outstride) {
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i x0, x1;

  u0 = _mm256_unpacklo_epi32(in[0 * instride], in[1 * instride]);
  u1 = _mm256_unpackhi_epi32(in[0 * instride], in[1 * instride]);

  u2 = _mm256_unpacklo_epi32(in[2 * instride], in[3 * instride]);
  u3 = _mm256_unpackhi_epi32(in[2 * instride], in[3 * instride]);

  u4 = _mm256_unpacklo_epi32(in[4 * instride], in[5 * instride]);
  u5 = _mm256_unpackhi_epi32(in[4 * instride], in[5 * instride]);

  u6 = _mm256_unpacklo_epi32(in[6 * instride], in[7 * instride]);
  u7 = _mm256_unpackhi_epi32(in[6 * instride], in[7 * instride]);

  x0 = _mm256_unpacklo_epi64(u0, u2);
  x1 = _mm256_unpacklo_epi64(u4, u6);
  out[0 * outstride] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[4 * outstride] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u0, u2);
  x1 = _mm256_unpackhi_epi64(u4, u6);
  out[1 * outstride] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[5 * outstride] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpacklo_epi64(u1, u3);
  x1 = _mm256_unpacklo_epi64(u5, u7);
  out[2 * outstride] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[6 * outstride] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u1, u3);
  x1 = _mm256_unpackhi_epi64(u5, u7);
  out[3 * outstride] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[7 * outstride] = _mm256_permute2f128_si256(x0, x1, 0x31);
}
static INLINE void round_shift_32_8xn_avx2(__m256i *in, int size, int bit,
                                           int stride) {
  if (bit < 0) {
    bit = -bit;
    __m256i round = _mm256_set1_epi32(1 << (bit - 1));
    for (int i = 0; i < size; ++i) {
      in[stride * i] = _mm256_add_epi32(in[stride * i], round);
      in[stride * i] = _mm256_srai_epi32(in[stride * i], bit);
    }
  } else if (bit > 0) {
    for (int i = 0; i < size; ++i) {
      in[stride * i] = _mm256_slli_epi32(in[stride * i], bit);
    }
  }
}
static INLINE void store_buffer_avx2(const __m256i *const in, int32_t *out,
                                     const int stride, const int out_size) {
  for (int i = 0; i < out_size; ++i) {
    _mm256_store_si256((__m256i *)(out), in[i]);
    out += stride;
  }
}
static INLINE void fwd_txfm_transpose_16x16_avx2(const __m256i *in,
                                                 __m256i *out) {
  fwd_txfm_transpose_8x8_avx2(&in[0], &out[0], 2, 2);
  fwd_txfm_transpose_8x8_avx2(&in[1], &out[16], 2, 2);
  fwd_txfm_transpose_8x8_avx2(&in[16], &out[1], 2, 2);
  fwd_txfm_transpose_8x8_avx2(&in[17], &out[17], 2, 2);
}

static INLINE __m256i av1_half_btf_avx2(const __m256i *w0, const __m256i *n0,
                                        const __m256i *w1, const __m256i *n1,
                                        const __m256i *rounding, int bit) {
  __m256i x, y;

  x = _mm256_mullo_epi32(*w0, *n0);
  y = _mm256_mullo_epi32(*w1, *n1);
  x = _mm256_add_epi32(x, y);
  x = _mm256_add_epi32(x, *rounding);
  x = _mm256_srai_epi32(x, bit);
  return x;
}
#define btf_32_avx2_type0(w0, w1, in0, in1, out0, out1, bit) \
  do {                                                       \
    const __m256i ww0 = _mm256_set1_epi32(w0);               \
    const __m256i ww1 = _mm256_set1_epi32(w1);               \
    const __m256i in0_w0 = _mm256_mullo_epi32(in0, ww0);     \
    const __m256i in1_w1 = _mm256_mullo_epi32(in1, ww1);     \
    out0 = _mm256_add_epi32(in0_w0, in1_w1);                 \
    round_shift_32_8xn_avx2(&out0, 1, -bit, 1);              \
    const __m256i in0_w1 = _mm256_mullo_epi32(in0, ww1);     \
    const __m256i in1_w0 = _mm256_mullo_epi32(in1, ww0);     \
    out1 = _mm256_sub_epi32(in0_w1, in1_w0);                 \
    round_shift_32_8xn_avx2(&out1, 1, -bit, 1);              \
  } while (0)

#define btf_32_type0_avx2_new(ww0, ww1, in0, in1, out0, out1, r, bit) \
  do {                                                                \
    const __m256i in0_w0 = _mm256_mullo_epi32(in0, ww0);              \
    const __m256i in1_w1 = _mm256_mullo_epi32(in1, ww1);              \
    out0 = _mm256_add_epi32(in0_w0, in1_w1);                          \
    out0 = _mm256_add_epi32(out0, r);                                 \
    out0 = _mm256_srai_epi32(out0, bit);                              \
    const __m256i in0_w1 = _mm256_mullo_epi32(in0, ww1);              \
    const __m256i in1_w0 = _mm256_mullo_epi32(in1, ww0);              \
    out1 = _mm256_sub_epi32(in0_w1, in1_w0);                          \
    out1 = _mm256_add_epi32(out1, r);                                 \
    out1 = _mm256_srai_epi32(out1, bit);                              \
  } while (0)

typedef void (*transform_1d_avx2)(__m256i *in, __m256i *out,
                                  const int8_t cos_bit, int instride,
                                  int outstride);
static void fdct8_avx2(__m256i *in, __m256i *out, const int8_t bit,
                       const int col_num, const int outstride) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  __m256i u[8], v[8];
  for (int col = 0; col < col_num; ++col) {
    u[0] = _mm256_add_epi32(in[0 * col_num + col], in[7 * col_num + col]);
    v[7] = _mm256_sub_epi32(in[0 * col_num + col], in[7 * col_num + col]);
    u[1] = _mm256_add_epi32(in[1 * col_num + col], in[6 * col_num + col]);
    u[6] = _mm256_sub_epi32(in[1 * col_num + col], in[6 * col_num + col]);
    u[2] = _mm256_add_epi32(in[2 * col_num + col], in[5 * col_num + col]);
    u[5] = _mm256_sub_epi32(in[2 * col_num + col], in[5 * col_num + col]);
    u[3] = _mm256_add_epi32(in[3 * col_num + col], in[4 * col_num + col]);
    v[4] = _mm256_sub_epi32(in[3 * col_num + col], in[4 * col_num + col]);
    v[0] = _mm256_add_epi32(u[0], u[3]);
    v[3] = _mm256_sub_epi32(u[0], u[3]);
    v[1] = _mm256_add_epi32(u[1], u[2]);
    v[2] = _mm256_sub_epi32(u[1], u[2]);

    v[5] = _mm256_mullo_epi32(u[5], cospim32);
    v[6] = _mm256_mullo_epi32(u[6], cospi32);
    v[5] = _mm256_add_epi32(v[5], v[6]);
    v[5] = _mm256_add_epi32(v[5], rnding);
    v[5] = _mm256_srai_epi32(v[5], bit);

    u[0] = _mm256_mullo_epi32(u[5], cospi32);
    v[6] = _mm256_mullo_epi32(u[6], cospim32);
    v[6] = _mm256_sub_epi32(u[0], v[6]);
    v[6] = _mm256_add_epi32(v[6], rnding);
    v[6] = _mm256_srai_epi32(v[6], bit);

    // stage 3
    // type 0
    v[0] = _mm256_mullo_epi32(v[0], cospi32);
    v[1] = _mm256_mullo_epi32(v[1], cospi32);
    u[0] = _mm256_add_epi32(v[0], v[1]);
    u[0] = _mm256_add_epi32(u[0], rnding);
    u[0] = _mm256_srai_epi32(u[0], bit);

    u[1] = _mm256_sub_epi32(v[0], v[1]);
    u[1] = _mm256_add_epi32(u[1], rnding);
    u[1] = _mm256_srai_epi32(u[1], bit);

    // type 1
    v[0] = _mm256_mullo_epi32(v[2], cospi48);
    v[1] = _mm256_mullo_epi32(v[3], cospi16);
    u[2] = _mm256_add_epi32(v[0], v[1]);
    u[2] = _mm256_add_epi32(u[2], rnding);
    u[2] = _mm256_srai_epi32(u[2], bit);

    v[0] = _mm256_mullo_epi32(v[2], cospi16);
    v[1] = _mm256_mullo_epi32(v[3], cospi48);
    u[3] = _mm256_sub_epi32(v[1], v[0]);
    u[3] = _mm256_add_epi32(u[3], rnding);
    u[3] = _mm256_srai_epi32(u[3], bit);

    u[4] = _mm256_add_epi32(v[4], v[5]);
    u[5] = _mm256_sub_epi32(v[4], v[5]);
    u[6] = _mm256_sub_epi32(v[7], v[6]);
    u[7] = _mm256_add_epi32(v[7], v[6]);

    // stage 4
    // stage 5
    v[0] = _mm256_mullo_epi32(u[4], cospi56);
    v[1] = _mm256_mullo_epi32(u[7], cospi8);
    v[0] = _mm256_add_epi32(v[0], v[1]);
    v[0] = _mm256_add_epi32(v[0], rnding);
    out[1 * outstride + col] = _mm256_srai_epi32(v[0], bit);  // buf0[4]

    v[0] = _mm256_mullo_epi32(u[4], cospi8);
    v[1] = _mm256_mullo_epi32(u[7], cospi56);
    v[0] = _mm256_sub_epi32(v[1], v[0]);
    v[0] = _mm256_add_epi32(v[0], rnding);
    out[7 * outstride + col] = _mm256_srai_epi32(v[0], bit);  // buf0[7]

    v[0] = _mm256_mullo_epi32(u[5], cospi24);
    v[1] = _mm256_mullo_epi32(u[6], cospi40);
    v[0] = _mm256_add_epi32(v[0], v[1]);
    v[0] = _mm256_add_epi32(v[0], rnding);
    out[5 * outstride + col] = _mm256_srai_epi32(v[0], bit);  // buf0[5]

    v[0] = _mm256_mullo_epi32(u[5], cospi40);
    v[1] = _mm256_mullo_epi32(u[6], cospi24);
    v[0] = _mm256_sub_epi32(v[1], v[0]);
    v[0] = _mm256_add_epi32(v[0], rnding);
    out[3 * outstride + col] = _mm256_srai_epi32(v[0], bit);  // buf0[6]

    out[0 * outstride + col] = u[0];  // buf0[0]
    out[4 * outstride + col] = u[1];  // buf0[1]
    out[2 * outstride + col] = u[2];  // buf0[2]
    out[6 * outstride + col] = u[3];  // buf0[3]
  }
}
static void fadst8_avx2(__m256i *in, __m256i *out, const int8_t bit,
                        const int col_num, const int outstirde) {
  (void)col_num;
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospim4 = _mm256_set1_epi32(-cospi[4]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospim20 = _mm256_set1_epi32(-cospi[20]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const __m256i zero = _mm256_setzero_si256();
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i v0, v1, v2, v3, v4, v5, v6, v7;
  __m256i x, y;
  for (int col = 0; col < col_num; ++col) {
    u0 = in[0 * col_num + col];
    u1 = _mm256_sub_epi32(zero, in[7 * col_num + col]);
    u2 = _mm256_sub_epi32(zero, in[3 * col_num + col]);
    u3 = in[4 * col_num + col];
    u4 = _mm256_sub_epi32(zero, in[1 * col_num + col]);
    u5 = in[6 * col_num + col];
    u6 = in[2 * col_num + col];
    u7 = _mm256_sub_epi32(zero, in[5 * col_num + col]);

    // stage 2
    v0 = u0;
    v1 = u1;

    x = _mm256_mullo_epi32(u2, cospi32);
    y = _mm256_mullo_epi32(u3, cospi32);
    v2 = _mm256_add_epi32(x, y);
    v2 = _mm256_add_epi32(v2, rnding);
    v2 = _mm256_srai_epi32(v2, bit);

    v3 = _mm256_sub_epi32(x, y);
    v3 = _mm256_add_epi32(v3, rnding);
    v3 = _mm256_srai_epi32(v3, bit);

    v4 = u4;
    v5 = u5;

    x = _mm256_mullo_epi32(u6, cospi32);
    y = _mm256_mullo_epi32(u7, cospi32);
    v6 = _mm256_add_epi32(x, y);
    v6 = _mm256_add_epi32(v6, rnding);
    v6 = _mm256_srai_epi32(v6, bit);

    v7 = _mm256_sub_epi32(x, y);
    v7 = _mm256_add_epi32(v7, rnding);
    v7 = _mm256_srai_epi32(v7, bit);

    // stage 3
    u0 = _mm256_add_epi32(v0, v2);
    u1 = _mm256_add_epi32(v1, v3);
    u2 = _mm256_sub_epi32(v0, v2);
    u3 = _mm256_sub_epi32(v1, v3);
    u4 = _mm256_add_epi32(v4, v6);
    u5 = _mm256_add_epi32(v5, v7);
    u6 = _mm256_sub_epi32(v4, v6);
    u7 = _mm256_sub_epi32(v5, v7);

    // stage 4
    v0 = u0;
    v1 = u1;
    v2 = u2;
    v3 = u3;

    x = _mm256_mullo_epi32(u4, cospi16);
    y = _mm256_mullo_epi32(u5, cospi48);
    v4 = _mm256_add_epi32(x, y);
    v4 = _mm256_add_epi32(v4, rnding);
    v4 = _mm256_srai_epi32(v4, bit);

    x = _mm256_mullo_epi32(u4, cospi48);
    y = _mm256_mullo_epi32(u5, cospim16);
    v5 = _mm256_add_epi32(x, y);
    v5 = _mm256_add_epi32(v5, rnding);
    v5 = _mm256_srai_epi32(v5, bit);

    x = _mm256_mullo_epi32(u6, cospim48);
    y = _mm256_mullo_epi32(u7, cospi16);
    v6 = _mm256_add_epi32(x, y);
    v6 = _mm256_add_epi32(v6, rnding);
    v6 = _mm256_srai_epi32(v6, bit);

    x = _mm256_mullo_epi32(u6, cospi16);
    y = _mm256_mullo_epi32(u7, cospi48);
    v7 = _mm256_add_epi32(x, y);
    v7 = _mm256_add_epi32(v7, rnding);
    v7 = _mm256_srai_epi32(v7, bit);

    // stage 5
    u0 = _mm256_add_epi32(v0, v4);
    u1 = _mm256_add_epi32(v1, v5);
    u2 = _mm256_add_epi32(v2, v6);
    u3 = _mm256_add_epi32(v3, v7);
    u4 = _mm256_sub_epi32(v0, v4);
    u5 = _mm256_sub_epi32(v1, v5);
    u6 = _mm256_sub_epi32(v2, v6);
    u7 = _mm256_sub_epi32(v3, v7);

    // stage 6
    x = _mm256_mullo_epi32(u0, cospi4);
    y = _mm256_mullo_epi32(u1, cospi60);
    v0 = _mm256_add_epi32(x, y);
    v0 = _mm256_add_epi32(v0, rnding);
    v0 = _mm256_srai_epi32(v0, bit);

    x = _mm256_mullo_epi32(u0, cospi60);
    y = _mm256_mullo_epi32(u1, cospim4);
    v1 = _mm256_add_epi32(x, y);
    v1 = _mm256_add_epi32(v1, rnding);
    v1 = _mm256_srai_epi32(v1, bit);

    x = _mm256_mullo_epi32(u2, cospi20);
    y = _mm256_mullo_epi32(u3, cospi44);
    v2 = _mm256_add_epi32(x, y);
    v2 = _mm256_add_epi32(v2, rnding);
    v2 = _mm256_srai_epi32(v2, bit);

    x = _mm256_mullo_epi32(u2, cospi44);
    y = _mm256_mullo_epi32(u3, cospim20);
    v3 = _mm256_add_epi32(x, y);
    v3 = _mm256_add_epi32(v3, rnding);
    v3 = _mm256_srai_epi32(v3, bit);

    x = _mm256_mullo_epi32(u4, cospi36);
    y = _mm256_mullo_epi32(u5, cospi28);
    v4 = _mm256_add_epi32(x, y);
    v4 = _mm256_add_epi32(v4, rnding);
    v4 = _mm256_srai_epi32(v4, bit);

    x = _mm256_mullo_epi32(u4, cospi28);
    y = _mm256_mullo_epi32(u5, cospim36);
    v5 = _mm256_add_epi32(x, y);
    v5 = _mm256_add_epi32(v5, rnding);
    v5 = _mm256_srai_epi32(v5, bit);

    x = _mm256_mullo_epi32(u6, cospi52);
    y = _mm256_mullo_epi32(u7, cospi12);
    v6 = _mm256_add_epi32(x, y);
    v6 = _mm256_add_epi32(v6, rnding);
    v6 = _mm256_srai_epi32(v6, bit);

    x = _mm256_mullo_epi32(u6, cospi12);
    y = _mm256_mullo_epi32(u7, cospim52);
    v7 = _mm256_add_epi32(x, y);
    v7 = _mm256_add_epi32(v7, rnding);
    v7 = _mm256_srai_epi32(v7, bit);

    // stage 7
    out[0 * outstirde + col] = v1;
    out[1 * outstirde + col] = v6;
    out[2 * outstirde + col] = v3;
    out[3 * outstirde + col] = v4;
    out[4 * outstirde + col] = v5;
    out[5 * outstirde + col] = v2;
    out[6 * outstirde + col] = v7;
    out[7 * outstirde + col] = v0;
  }
}
static void idtx8_avx2(__m256i *in, __m256i *out, const int8_t bit, int col_num,
                       int outstride) {
  (void)bit;
  (void)outstride;
  int num_iters = 8 * col_num;
  for (int i = 0; i < num_iters; i += 8) {
    out[i] = _mm256_add_epi32(in[i], in[i]);
    out[i + 1] = _mm256_add_epi32(in[i + 1], in[i + 1]);
    out[i + 2] = _mm256_add_epi32(in[i + 2], in[i + 2]);
    out[i + 3] = _mm256_add_epi32(in[i + 3], in[i + 3]);
    out[i + 4] = _mm256_add_epi32(in[i + 4], in[i + 4]);
    out[i + 5] = _mm256_add_epi32(in[i + 5], in[i + 5]);
    out[i + 6] = _mm256_add_epi32(in[i + 6], in[i + 6]);
    out[i + 7] = _mm256_add_epi32(in[i + 7], in[i + 7]);
  }
}
void av1_fwd_txfm2d_8x8_avx2(const int16_t *input, int32_t *coeff, int stride,
                             TX_TYPE tx_type, int bd) {
  __m256i in[8], out[8];
  const TX_SIZE tx_size = TX_8X8;
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int width = tx_size_wide[tx_size];
  const int width_div8 = (width >> 3);

  switch (tx_type) {
    case DCT_DCT:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      fdct8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fdct8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                 width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case ADST_DCT:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fdct8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                 width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case DCT_ADST:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      fdct8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fadst8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case ADST_ADST:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fadst8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case FLIPADST_DCT:
      load_buffer_8x8_avx2(input, in, stride, 1, 0, shift[0]);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fdct8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                 width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case DCT_FLIPADST:
      load_buffer_8x8_avx2(input, in, stride, 0, 1, shift[0]);
      fdct8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fadst8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_8x8_avx2(input, in, stride, 1, 1, shift[0]);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fadst8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case ADST_FLIPADST:
      load_buffer_8x8_avx2(input, in, stride, 0, 1, shift[0]);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fadst8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case FLIPADST_ADST:
      load_buffer_8x8_avx2(input, in, stride, 1, 0, shift[0]);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fadst8_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case IDTX:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      idtx8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      idtx8_avx2(out, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case V_DCT:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      fdct8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      idtx8_avx2(out, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case H_DCT:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      idtx8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fdct8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case V_ADST:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      idtx8_avx2(out, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case H_ADST:
      load_buffer_8x8_avx2(input, in, stride, 0, 0, shift[0]);
      idtx8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case V_FLIPADST:
      load_buffer_8x8_avx2(input, in, stride, 1, 0, shift[0]);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      idtx8_avx2(out, in, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    case H_FLIPADST:
      load_buffer_8x8_avx2(input, in, stride, 0, 1, shift[0]);
      idtx8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                 width_div8);
      col_txfm_8x8_rounding(out, -shift[1]);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      fadst8_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_8x8_avx2(out, in, width_div8, width_div8);
      store_buffer_avx2(in, coeff, 8, 8);
      break;
    default: assert(0);
  }
  (void)bd;
}

static void fdct16_avx2(__m256i *in, __m256i *out, const int8_t bit,
                        const int col_num, const int outstride) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  __m256i u[16], v[16], x;
  int col;

  // Calculate the column 0, 1, 2, 3
  for (col = 0; col < col_num; ++col) {
    // stage 0
    // stage 1
    u[0] = _mm256_add_epi32(in[0 * col_num + col], in[15 * col_num + col]);
    u[15] = _mm256_sub_epi32(in[0 * col_num + col], in[15 * col_num + col]);
    u[1] = _mm256_add_epi32(in[1 * col_num + col], in[14 * col_num + col]);
    u[14] = _mm256_sub_epi32(in[1 * col_num + col], in[14 * col_num + col]);
    u[2] = _mm256_add_epi32(in[2 * col_num + col], in[13 * col_num + col]);
    u[13] = _mm256_sub_epi32(in[2 * col_num + col], in[13 * col_num + col]);
    u[3] = _mm256_add_epi32(in[3 * col_num + col], in[12 * col_num + col]);
    u[12] = _mm256_sub_epi32(in[3 * col_num + col], in[12 * col_num + col]);
    u[4] = _mm256_add_epi32(in[4 * col_num + col], in[11 * col_num + col]);
    u[11] = _mm256_sub_epi32(in[4 * col_num + col], in[11 * col_num + col]);
    u[5] = _mm256_add_epi32(in[5 * col_num + col], in[10 * col_num + col]);
    u[10] = _mm256_sub_epi32(in[5 * col_num + col], in[10 * col_num + col]);
    u[6] = _mm256_add_epi32(in[6 * col_num + col], in[9 * col_num + col]);
    u[9] = _mm256_sub_epi32(in[6 * col_num + col], in[9 * col_num + col]);
    u[7] = _mm256_add_epi32(in[7 * col_num + col], in[8 * col_num + col]);
    u[8] = _mm256_sub_epi32(in[7 * col_num + col], in[8 * col_num + col]);

    // stage 2
    v[0] = _mm256_add_epi32(u[0], u[7]);
    v[7] = _mm256_sub_epi32(u[0], u[7]);
    v[1] = _mm256_add_epi32(u[1], u[6]);
    v[6] = _mm256_sub_epi32(u[1], u[6]);
    v[2] = _mm256_add_epi32(u[2], u[5]);
    v[5] = _mm256_sub_epi32(u[2], u[5]);
    v[3] = _mm256_add_epi32(u[3], u[4]);
    v[4] = _mm256_sub_epi32(u[3], u[4]);
    v[8] = u[8];
    v[9] = u[9];

    v[10] = _mm256_mullo_epi32(u[10], cospim32);
    x = _mm256_mullo_epi32(u[13], cospi32);
    v[10] = _mm256_add_epi32(v[10], x);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[13] = _mm256_mullo_epi32(u[10], cospi32);
    x = _mm256_mullo_epi32(u[13], cospim32);
    v[13] = _mm256_sub_epi32(v[13], x);
    v[13] = _mm256_add_epi32(v[13], rnding);
    v[13] = _mm256_srai_epi32(v[13], bit);

    v[11] = _mm256_mullo_epi32(u[11], cospim32);
    x = _mm256_mullo_epi32(u[12], cospi32);
    v[11] = _mm256_add_epi32(v[11], x);
    v[11] = _mm256_add_epi32(v[11], rnding);
    v[11] = _mm256_srai_epi32(v[11], bit);

    v[12] = _mm256_mullo_epi32(u[11], cospi32);
    x = _mm256_mullo_epi32(u[12], cospim32);
    v[12] = _mm256_sub_epi32(v[12], x);
    v[12] = _mm256_add_epi32(v[12], rnding);
    v[12] = _mm256_srai_epi32(v[12], bit);
    v[14] = u[14];
    v[15] = u[15];

    // stage 3
    u[0] = _mm256_add_epi32(v[0], v[3]);
    u[3] = _mm256_sub_epi32(v[0], v[3]);
    u[1] = _mm256_add_epi32(v[1], v[2]);
    u[2] = _mm256_sub_epi32(v[1], v[2]);
    u[4] = v[4];

    u[5] = _mm256_mullo_epi32(v[5], cospim32);
    x = _mm256_mullo_epi32(v[6], cospi32);
    u[5] = _mm256_add_epi32(u[5], x);
    u[5] = _mm256_add_epi32(u[5], rnding);
    u[5] = _mm256_srai_epi32(u[5], bit);

    u[6] = _mm256_mullo_epi32(v[5], cospi32);
    x = _mm256_mullo_epi32(v[6], cospim32);
    u[6] = _mm256_sub_epi32(u[6], x);
    u[6] = _mm256_add_epi32(u[6], rnding);
    u[6] = _mm256_srai_epi32(u[6], bit);

    u[7] = v[7];
    u[8] = _mm256_add_epi32(v[8], v[11]);
    u[11] = _mm256_sub_epi32(v[8], v[11]);
    u[9] = _mm256_add_epi32(v[9], v[10]);
    u[10] = _mm256_sub_epi32(v[9], v[10]);
    u[12] = _mm256_sub_epi32(v[15], v[12]);
    u[15] = _mm256_add_epi32(v[15], v[12]);
    u[13] = _mm256_sub_epi32(v[14], v[13]);
    u[14] = _mm256_add_epi32(v[14], v[13]);

    // stage 4
    u[0] = _mm256_mullo_epi32(u[0], cospi32);
    u[1] = _mm256_mullo_epi32(u[1], cospi32);
    v[0] = _mm256_add_epi32(u[0], u[1]);
    v[0] = _mm256_add_epi32(v[0], rnding);
    v[0] = _mm256_srai_epi32(v[0], bit);

    v[1] = _mm256_sub_epi32(u[0], u[1]);
    v[1] = _mm256_add_epi32(v[1], rnding);
    v[1] = _mm256_srai_epi32(v[1], bit);

    v[2] = _mm256_mullo_epi32(u[2], cospi48);
    x = _mm256_mullo_epi32(u[3], cospi16);
    v[2] = _mm256_add_epi32(v[2], x);
    v[2] = _mm256_add_epi32(v[2], rnding);
    v[2] = _mm256_srai_epi32(v[2], bit);

    v[3] = _mm256_mullo_epi32(u[2], cospi16);
    x = _mm256_mullo_epi32(u[3], cospi48);
    v[3] = _mm256_sub_epi32(x, v[3]);
    v[3] = _mm256_add_epi32(v[3], rnding);
    v[3] = _mm256_srai_epi32(v[3], bit);

    v[4] = _mm256_add_epi32(u[4], u[5]);
    v[5] = _mm256_sub_epi32(u[4], u[5]);
    v[6] = _mm256_sub_epi32(u[7], u[6]);
    v[7] = _mm256_add_epi32(u[7], u[6]);
    v[8] = u[8];

    v[9] = _mm256_mullo_epi32(u[9], cospim16);
    x = _mm256_mullo_epi32(u[14], cospi48);
    v[9] = _mm256_add_epi32(v[9], x);
    v[9] = _mm256_add_epi32(v[9], rnding);
    v[9] = _mm256_srai_epi32(v[9], bit);

    v[14] = _mm256_mullo_epi32(u[9], cospi48);
    x = _mm256_mullo_epi32(u[14], cospim16);
    v[14] = _mm256_sub_epi32(v[14], x);
    v[14] = _mm256_add_epi32(v[14], rnding);
    v[14] = _mm256_srai_epi32(v[14], bit);

    v[10] = _mm256_mullo_epi32(u[10], cospim48);
    x = _mm256_mullo_epi32(u[13], cospim16);
    v[10] = _mm256_add_epi32(v[10], x);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[13] = _mm256_mullo_epi32(u[10], cospim16);
    x = _mm256_mullo_epi32(u[13], cospim48);
    v[13] = _mm256_sub_epi32(v[13], x);
    v[13] = _mm256_add_epi32(v[13], rnding);
    v[13] = _mm256_srai_epi32(v[13], bit);

    v[11] = u[11];
    v[12] = u[12];
    v[15] = u[15];

    // stage 5
    u[0] = v[0];
    u[1] = v[1];
    u[2] = v[2];
    u[3] = v[3];

    u[4] = _mm256_mullo_epi32(v[4], cospi56);
    x = _mm256_mullo_epi32(v[7], cospi8);
    u[4] = _mm256_add_epi32(u[4], x);
    u[4] = _mm256_add_epi32(u[4], rnding);
    u[4] = _mm256_srai_epi32(u[4], bit);

    u[7] = _mm256_mullo_epi32(v[4], cospi8);
    x = _mm256_mullo_epi32(v[7], cospi56);
    u[7] = _mm256_sub_epi32(x, u[7]);
    u[7] = _mm256_add_epi32(u[7], rnding);
    u[7] = _mm256_srai_epi32(u[7], bit);

    u[5] = _mm256_mullo_epi32(v[5], cospi24);
    x = _mm256_mullo_epi32(v[6], cospi40);
    u[5] = _mm256_add_epi32(u[5], x);
    u[5] = _mm256_add_epi32(u[5], rnding);
    u[5] = _mm256_srai_epi32(u[5], bit);

    u[6] = _mm256_mullo_epi32(v[5], cospi40);
    x = _mm256_mullo_epi32(v[6], cospi24);
    u[6] = _mm256_sub_epi32(x, u[6]);
    u[6] = _mm256_add_epi32(u[6], rnding);
    u[6] = _mm256_srai_epi32(u[6], bit);

    u[8] = _mm256_add_epi32(v[8], v[9]);
    u[9] = _mm256_sub_epi32(v[8], v[9]);
    u[10] = _mm256_sub_epi32(v[11], v[10]);
    u[11] = _mm256_add_epi32(v[11], v[10]);
    u[12] = _mm256_add_epi32(v[12], v[13]);
    u[13] = _mm256_sub_epi32(v[12], v[13]);
    u[14] = _mm256_sub_epi32(v[15], v[14]);
    u[15] = _mm256_add_epi32(v[15], v[14]);

    // stage 6
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = u[4];
    v[5] = u[5];
    v[6] = u[6];
    v[7] = u[7];

    v[8] = _mm256_mullo_epi32(u[8], cospi60);
    x = _mm256_mullo_epi32(u[15], cospi4);
    v[8] = _mm256_add_epi32(v[8], x);
    v[8] = _mm256_add_epi32(v[8], rnding);
    v[8] = _mm256_srai_epi32(v[8], bit);

    v[15] = _mm256_mullo_epi32(u[8], cospi4);
    x = _mm256_mullo_epi32(u[15], cospi60);
    v[15] = _mm256_sub_epi32(x, v[15]);
    v[15] = _mm256_add_epi32(v[15], rnding);
    v[15] = _mm256_srai_epi32(v[15], bit);

    v[9] = _mm256_mullo_epi32(u[9], cospi28);
    x = _mm256_mullo_epi32(u[14], cospi36);
    v[9] = _mm256_add_epi32(v[9], x);
    v[9] = _mm256_add_epi32(v[9], rnding);
    v[9] = _mm256_srai_epi32(v[9], bit);

    v[14] = _mm256_mullo_epi32(u[9], cospi36);
    x = _mm256_mullo_epi32(u[14], cospi28);
    v[14] = _mm256_sub_epi32(x, v[14]);
    v[14] = _mm256_add_epi32(v[14], rnding);
    v[14] = _mm256_srai_epi32(v[14], bit);

    v[10] = _mm256_mullo_epi32(u[10], cospi44);
    x = _mm256_mullo_epi32(u[13], cospi20);
    v[10] = _mm256_add_epi32(v[10], x);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[13] = _mm256_mullo_epi32(u[10], cospi20);
    x = _mm256_mullo_epi32(u[13], cospi44);
    v[13] = _mm256_sub_epi32(x, v[13]);
    v[13] = _mm256_add_epi32(v[13], rnding);
    v[13] = _mm256_srai_epi32(v[13], bit);

    v[11] = _mm256_mullo_epi32(u[11], cospi12);
    x = _mm256_mullo_epi32(u[12], cospi52);
    v[11] = _mm256_add_epi32(v[11], x);
    v[11] = _mm256_add_epi32(v[11], rnding);
    v[11] = _mm256_srai_epi32(v[11], bit);

    v[12] = _mm256_mullo_epi32(u[11], cospi52);
    x = _mm256_mullo_epi32(u[12], cospi12);
    v[12] = _mm256_sub_epi32(x, v[12]);
    v[12] = _mm256_add_epi32(v[12], rnding);
    v[12] = _mm256_srai_epi32(v[12], bit);

    out[0 * outstride + col] = v[0];
    out[1 * outstride + col] = v[8];
    out[2 * outstride + col] = v[4];
    out[3 * outstride + col] = v[12];
    out[4 * outstride + col] = v[2];
    out[5 * outstride + col] = v[10];
    out[6 * outstride + col] = v[6];
    out[7 * outstride + col] = v[14];
    out[8 * outstride + col] = v[1];
    out[9 * outstride + col] = v[9];
    out[10 * outstride + col] = v[5];
    out[11 * outstride + col] = v[13];
    out[12 * outstride + col] = v[3];
    out[13 * outstride + col] = v[11];
    out[14 * outstride + col] = v[7];
    out[15 * outstride + col] = v[15];
  }
}
static void fadst16_avx2(__m256i *in, __m256i *out, const int8_t bit,
                         const int num_cols, const int outstride) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospim2 = _mm256_set1_epi32(-cospi[2]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospim10 = _mm256_set1_epi32(-cospi[10]);
  const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
  const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
  const __m256i cospim18 = _mm256_set1_epi32(-cospi[18]);
  const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
  const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
  const __m256i cospim26 = _mm256_set1_epi32(-cospi[26]);
  const __m256i cospi34 = _mm256_set1_epi32(cospi[34]);
  const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
  const __m256i cospim34 = _mm256_set1_epi32(-cospi[34]);
  const __m256i cospi42 = _mm256_set1_epi32(cospi[42]);
  const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
  const __m256i cospim42 = _mm256_set1_epi32(-cospi[42]);
  const __m256i cospi50 = _mm256_set1_epi32(cospi[50]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospi58 = _mm256_set1_epi32(cospi[58]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const __m256i zero = _mm256_setzero_si256();

  __m256i u[16], v[16], x, y;
  int col;

  for (col = 0; col < num_cols; ++col) {
    // stage 0
    // stage 1
    u[0] = in[0 * num_cols + col];
    u[1] = _mm256_sub_epi32(zero, in[15 * num_cols + col]);
    u[2] = _mm256_sub_epi32(zero, in[7 * num_cols + col]);
    u[3] = in[8 * num_cols + col];
    u[4] = _mm256_sub_epi32(zero, in[3 * num_cols + col]);
    u[5] = in[12 * num_cols + col];
    u[6] = in[4 * num_cols + col];
    u[7] = _mm256_sub_epi32(zero, in[11 * num_cols + col]);
    u[8] = _mm256_sub_epi32(zero, in[1 * num_cols + col]);
    u[9] = in[14 * num_cols + col];
    u[10] = in[6 * num_cols + col];
    u[11] = _mm256_sub_epi32(zero, in[9 * num_cols + col]);
    u[12] = in[2 * num_cols + col];
    u[13] = _mm256_sub_epi32(zero, in[13 * num_cols + col]);
    u[14] = _mm256_sub_epi32(zero, in[5 * num_cols + col]);
    u[15] = in[10 * num_cols + col];

    // stage 2
    v[0] = u[0];
    v[1] = u[1];

    x = _mm256_mullo_epi32(u[2], cospi32);
    y = _mm256_mullo_epi32(u[3], cospi32);
    v[2] = _mm256_add_epi32(x, y);
    v[2] = _mm256_add_epi32(v[2], rnding);
    v[2] = _mm256_srai_epi32(v[2], bit);

    v[3] = _mm256_sub_epi32(x, y);
    v[3] = _mm256_add_epi32(v[3], rnding);
    v[3] = _mm256_srai_epi32(v[3], bit);

    v[4] = u[4];
    v[5] = u[5];

    x = _mm256_mullo_epi32(u[6], cospi32);
    y = _mm256_mullo_epi32(u[7], cospi32);
    v[6] = _mm256_add_epi32(x, y);
    v[6] = _mm256_add_epi32(v[6], rnding);
    v[6] = _mm256_srai_epi32(v[6], bit);

    v[7] = _mm256_sub_epi32(x, y);
    v[7] = _mm256_add_epi32(v[7], rnding);
    v[7] = _mm256_srai_epi32(v[7], bit);

    v[8] = u[8];
    v[9] = u[9];

    x = _mm256_mullo_epi32(u[10], cospi32);
    y = _mm256_mullo_epi32(u[11], cospi32);
    v[10] = _mm256_add_epi32(x, y);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[11] = _mm256_sub_epi32(x, y);
    v[11] = _mm256_add_epi32(v[11], rnding);
    v[11] = _mm256_srai_epi32(v[11], bit);

    v[12] = u[12];
    v[13] = u[13];

    x = _mm256_mullo_epi32(u[14], cospi32);
    y = _mm256_mullo_epi32(u[15], cospi32);
    v[14] = _mm256_add_epi32(x, y);
    v[14] = _mm256_add_epi32(v[14], rnding);
    v[14] = _mm256_srai_epi32(v[14], bit);

    v[15] = _mm256_sub_epi32(x, y);
    v[15] = _mm256_add_epi32(v[15], rnding);
    v[15] = _mm256_srai_epi32(v[15], bit);

    // stage 3
    u[0] = _mm256_add_epi32(v[0], v[2]);
    u[1] = _mm256_add_epi32(v[1], v[3]);
    u[2] = _mm256_sub_epi32(v[0], v[2]);
    u[3] = _mm256_sub_epi32(v[1], v[3]);
    u[4] = _mm256_add_epi32(v[4], v[6]);
    u[5] = _mm256_add_epi32(v[5], v[7]);
    u[6] = _mm256_sub_epi32(v[4], v[6]);
    u[7] = _mm256_sub_epi32(v[5], v[7]);
    u[8] = _mm256_add_epi32(v[8], v[10]);
    u[9] = _mm256_add_epi32(v[9], v[11]);
    u[10] = _mm256_sub_epi32(v[8], v[10]);
    u[11] = _mm256_sub_epi32(v[9], v[11]);
    u[12] = _mm256_add_epi32(v[12], v[14]);
    u[13] = _mm256_add_epi32(v[13], v[15]);
    u[14] = _mm256_sub_epi32(v[12], v[14]);
    u[15] = _mm256_sub_epi32(v[13], v[15]);

    // stage 4
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = av1_half_btf_avx2(&cospi16, &u[4], &cospi48, &u[5], &rnding, bit);
    v[5] = av1_half_btf_avx2(&cospi48, &u[4], &cospim16, &u[5], &rnding, bit);
    v[6] = av1_half_btf_avx2(&cospim48, &u[6], &cospi16, &u[7], &rnding, bit);
    v[7] = av1_half_btf_avx2(&cospi16, &u[6], &cospi48, &u[7], &rnding, bit);
    v[8] = u[8];
    v[9] = u[9];
    v[10] = u[10];
    v[11] = u[11];
    v[12] = av1_half_btf_avx2(&cospi16, &u[12], &cospi48, &u[13], &rnding, bit);
    v[13] =
        av1_half_btf_avx2(&cospi48, &u[12], &cospim16, &u[13], &rnding, bit);
    v[14] =
        av1_half_btf_avx2(&cospim48, &u[14], &cospi16, &u[15], &rnding, bit);
    v[15] = av1_half_btf_avx2(&cospi16, &u[14], &cospi48, &u[15], &rnding, bit);

    // stage 5
    u[0] = _mm256_add_epi32(v[0], v[4]);
    u[1] = _mm256_add_epi32(v[1], v[5]);
    u[2] = _mm256_add_epi32(v[2], v[6]);
    u[3] = _mm256_add_epi32(v[3], v[7]);
    u[4] = _mm256_sub_epi32(v[0], v[4]);
    u[5] = _mm256_sub_epi32(v[1], v[5]);
    u[6] = _mm256_sub_epi32(v[2], v[6]);
    u[7] = _mm256_sub_epi32(v[3], v[7]);
    u[8] = _mm256_add_epi32(v[8], v[12]);
    u[9] = _mm256_add_epi32(v[9], v[13]);
    u[10] = _mm256_add_epi32(v[10], v[14]);
    u[11] = _mm256_add_epi32(v[11], v[15]);
    u[12] = _mm256_sub_epi32(v[8], v[12]);
    u[13] = _mm256_sub_epi32(v[9], v[13]);
    u[14] = _mm256_sub_epi32(v[10], v[14]);
    u[15] = _mm256_sub_epi32(v[11], v[15]);

    // stage 6
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = u[4];
    v[5] = u[5];
    v[6] = u[6];
    v[7] = u[7];
    v[8] = av1_half_btf_avx2(&cospi8, &u[8], &cospi56, &u[9], &rnding, bit);
    v[9] = av1_half_btf_avx2(&cospi56, &u[8], &cospim8, &u[9], &rnding, bit);
    v[10] = av1_half_btf_avx2(&cospi40, &u[10], &cospi24, &u[11], &rnding, bit);
    v[11] =
        av1_half_btf_avx2(&cospi24, &u[10], &cospim40, &u[11], &rnding, bit);
    v[12] = av1_half_btf_avx2(&cospim56, &u[12], &cospi8, &u[13], &rnding, bit);
    v[13] = av1_half_btf_avx2(&cospi8, &u[12], &cospi56, &u[13], &rnding, bit);
    v[14] =
        av1_half_btf_avx2(&cospim24, &u[14], &cospi40, &u[15], &rnding, bit);
    v[15] = av1_half_btf_avx2(&cospi40, &u[14], &cospi24, &u[15], &rnding, bit);

    // stage 7
    u[0] = _mm256_add_epi32(v[0], v[8]);
    u[1] = _mm256_add_epi32(v[1], v[9]);
    u[2] = _mm256_add_epi32(v[2], v[10]);
    u[3] = _mm256_add_epi32(v[3], v[11]);
    u[4] = _mm256_add_epi32(v[4], v[12]);
    u[5] = _mm256_add_epi32(v[5], v[13]);
    u[6] = _mm256_add_epi32(v[6], v[14]);
    u[7] = _mm256_add_epi32(v[7], v[15]);
    u[8] = _mm256_sub_epi32(v[0], v[8]);
    u[9] = _mm256_sub_epi32(v[1], v[9]);
    u[10] = _mm256_sub_epi32(v[2], v[10]);
    u[11] = _mm256_sub_epi32(v[3], v[11]);
    u[12] = _mm256_sub_epi32(v[4], v[12]);
    u[13] = _mm256_sub_epi32(v[5], v[13]);
    u[14] = _mm256_sub_epi32(v[6], v[14]);
    u[15] = _mm256_sub_epi32(v[7], v[15]);

    // stage 8
    v[0] = av1_half_btf_avx2(&cospi2, &u[0], &cospi62, &u[1], &rnding, bit);
    v[1] = av1_half_btf_avx2(&cospi62, &u[0], &cospim2, &u[1], &rnding, bit);
    v[2] = av1_half_btf_avx2(&cospi10, &u[2], &cospi54, &u[3], &rnding, bit);
    v[3] = av1_half_btf_avx2(&cospi54, &u[2], &cospim10, &u[3], &rnding, bit);
    v[4] = av1_half_btf_avx2(&cospi18, &u[4], &cospi46, &u[5], &rnding, bit);
    v[5] = av1_half_btf_avx2(&cospi46, &u[4], &cospim18, &u[5], &rnding, bit);
    v[6] = av1_half_btf_avx2(&cospi26, &u[6], &cospi38, &u[7], &rnding, bit);
    v[7] = av1_half_btf_avx2(&cospi38, &u[6], &cospim26, &u[7], &rnding, bit);
    v[8] = av1_half_btf_avx2(&cospi34, &u[8], &cospi30, &u[9], &rnding, bit);
    v[9] = av1_half_btf_avx2(&cospi30, &u[8], &cospim34, &u[9], &rnding, bit);
    v[10] = av1_half_btf_avx2(&cospi42, &u[10], &cospi22, &u[11], &rnding, bit);
    v[11] =
        av1_half_btf_avx2(&cospi22, &u[10], &cospim42, &u[11], &rnding, bit);
    v[12] = av1_half_btf_avx2(&cospi50, &u[12], &cospi14, &u[13], &rnding, bit);
    v[13] =
        av1_half_btf_avx2(&cospi14, &u[12], &cospim50, &u[13], &rnding, bit);
    v[14] = av1_half_btf_avx2(&cospi58, &u[14], &cospi6, &u[15], &rnding, bit);
    v[15] = av1_half_btf_avx2(&cospi6, &u[14], &cospim58, &u[15], &rnding, bit);

    // stage 9
    out[0 * outstride + col] = v[1];
    out[1 * outstride + col] = v[14];
    out[2 * outstride + col] = v[3];
    out[3 * outstride + col] = v[12];
    out[4 * outstride + col] = v[5];
    out[5 * outstride + col] = v[10];
    out[6 * outstride + col] = v[7];
    out[7 * outstride + col] = v[8];
    out[8 * outstride + col] = v[9];
    out[9 * outstride + col] = v[6];
    out[10 * outstride + col] = v[11];
    out[11 * outstride + col] = v[4];
    out[12 * outstride + col] = v[13];
    out[13 * outstride + col] = v[2];
    out[14 * outstride + col] = v[15];
    out[15 * outstride + col] = v[0];
  }
}
static void idtx16_avx2(__m256i *in, __m256i *out, const int8_t bit,
                        int col_num, const int outstride) {
  (void)bit;
  (void)outstride;
  __m256i fact = _mm256_set1_epi32(2 * NewSqrt2);
  __m256i offset = _mm256_set1_epi32(1 << (NewSqrt2Bits - 1));
  __m256i a_low;

  int num_iters = 16 * col_num;
  for (int i = 0; i < num_iters; i++) {
    a_low = _mm256_mullo_epi32(in[i], fact);
    a_low = _mm256_add_epi32(a_low, offset);
    out[i] = _mm256_srai_epi32(a_low, NewSqrt2Bits);
  }
}
static const transform_1d_avx2 col_highbd_txfm8x16_arr[TX_TYPES] = {
  fdct16_avx2,   // DCT_DCT
  fadst16_avx2,  // ADST_DCT
  fdct16_avx2,   // DCT_ADST
  fadst16_avx2,  // ADST_ADST
  fadst16_avx2,  // FLIPADST_DCT
  fdct16_avx2,   // DCT_FLIPADST
  fadst16_avx2,  // FLIPADST_FLIPADST
  fadst16_avx2,  // ADST_FLIPADST
  fadst16_avx2,  // FLIPADST_ADST
  idtx16_avx2,   // IDTX
  fdct16_avx2,   // V_DCT
  idtx16_avx2,   // H_DCT
  fadst16_avx2,  // V_ADST
  idtx16_avx2,   // H_ADST
  fadst16_avx2,  // V_FLIPADST
  idtx16_avx2    // H_FLIPADST
};
static const transform_1d_avx2 row_highbd_txfm8x8_arr[TX_TYPES] = {
  fdct8_avx2,   // DCT_DCT
  fdct8_avx2,   // ADST_DCT
  fadst8_avx2,  // DCT_ADST
  fadst8_avx2,  // ADST_ADST
  fdct8_avx2,   // FLIPADST_DCT
  fadst8_avx2,  // DCT_FLIPADST
  fadst8_avx2,  // FLIPADST_FLIPADST
  fadst8_avx2,  // ADST_FLIPADST
  fadst8_avx2,  // FLIPADST_ADST
  idtx8_avx2,   // IDTX
  idtx8_avx2,   // V_DCT
  fdct8_avx2,   // H_DCT
  idtx8_avx2,   // V_ADST
  fadst8_avx2,  // H_ADST
  idtx8_avx2,   // V_FLIPADST
  fadst8_avx2   // H_FLIPADST
};
void av1_fwd_txfm2d_8x16_avx2(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  __m256i in[16], out[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X16];
  const int txw_idx = get_txw_idx(TX_8X16);
  const int txh_idx = get_txh_idx(TX_8X16);
  const transform_1d_avx2 col_txfm = col_highbd_txfm8x16_arr[tx_type];
  const transform_1d_avx2 row_txfm = row_highbd_txfm8x8_arr[tx_type];
  const int8_t bit = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  load_buffer_8x16_avx2(input, in, stride, ud_flip, lr_flip, shift[0]);
  col_txfm(in, out, bit, 1, 1);
  col_txfm_8x8_rounding(out, -shift[1]);
  col_txfm_8x8_rounding(&out[8], -shift[1]);
  fwd_txfm_transpose_8x8_avx2(out, in, 1, 2);
  fwd_txfm_transpose_8x8_avx2(&out[8], &in[1], 1, 2);
  row_txfm(in, out, bit, 2, 2);
  fwd_txfm_transpose_8x8_avx2(out, in, 2, 1);
  fwd_txfm_transpose_8x8_avx2(&out[1], &in[8], 2, 1);
  round_shift_rect_array_32_avx2(in, in, 16, -shift[2], NewSqrt2);
  store_buffer_avx2(in, coeff, 8, 16);
  (void)bd;
}
static const transform_1d_avx2 col_highbd_txfm8x8_arr[TX_TYPES] = {
  fdct8_avx2,   // DCT_DCT
  fadst8_avx2,  // ADST_DCT
  fdct8_avx2,   // DCT_ADST
  fadst8_avx2,  // ADST_ADST
  fadst8_avx2,  // FLIPADST_DCT
  fdct8_avx2,   // DCT_FLIPADST
  fadst8_avx2,  // FLIPADST_FLIPADST
  fadst8_avx2,  // ADST_FLIPADST
  fadst8_avx2,  // FLIPADST_ADST
  idtx8_avx2,   // IDTX
  fdct8_avx2,   // V_DCT
  idtx8_avx2,   // H_DCT
  fadst8_avx2,  // V_ADST
  idtx8_avx2,   // H_ADST
  fadst8_avx2,  // V_FLIPADST
  idtx8_avx2    // H_FLIPADST
};
static const transform_1d_avx2 row_highbd_txfm8x16_arr[TX_TYPES] = {
  fdct16_avx2,   // DCT_DCT
  fdct16_avx2,   // ADST_DCT
  fadst16_avx2,  // DCT_ADST
  fadst16_avx2,  // ADST_ADST
  fdct16_avx2,   // FLIPADST_DCT
  fadst16_avx2,  // DCT_FLIPADST
  fadst16_avx2,  // FLIPADST_FLIPADST
  fadst16_avx2,  // ADST_FLIPADST
  fadst16_avx2,  // FLIPADST_ADST
  idtx16_avx2,   // IDTX
  idtx16_avx2,   // V_DCT
  fdct16_avx2,   // H_DCT
  idtx16_avx2,   // V_ADST
  fadst16_avx2,  // H_ADST
  idtx16_avx2,   // V_FLIPADST
  fadst16_avx2   // H_FLIPADST
};
void av1_fwd_txfm2d_16x8_avx2(const int16_t *input, int32_t *coeff, int stride,
                              TX_TYPE tx_type, int bd) {
  __m256i in[16], out[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X8];
  const int txw_idx = get_txw_idx(TX_16X8);
  const int txh_idx = get_txh_idx(TX_16X8);
  const transform_1d_avx2 col_txfm = col_highbd_txfm8x8_arr[tx_type];
  const transform_1d_avx2 row_txfm = row_highbd_txfm8x16_arr[tx_type];
  const int8_t bit = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  load_buffer_16xn_avx2(input, in, stride, 8, 2, ud_flip, lr_flip);
  round_shift_32_8xn_avx2(in, 16, shift[0], 1);
  col_txfm(in, out, bit, 2, 2);
  round_shift_32_8xn_avx2(out, 16, shift[1], 1);
  fwd_txfm_transpose_8x8_avx2(out, in, 2, 1);
  fwd_txfm_transpose_8x8_avx2(&out[1], &in[8], 2, 1);
  row_txfm(in, out, bit, 1, 1);
  fwd_txfm_transpose_8x8_avx2(out, in, 1, 2);
  fwd_txfm_transpose_8x8_avx2(&out[8], &in[1], 1, 2);
  round_shift_rect_array_32_avx2(in, in, 16, -shift[2], NewSqrt2);
  store_buffer_avx2(in, coeff, 8, 16);
  (void)bd;
}
void av1_fwd_txfm2d_16x16_avx2(const int16_t *input, int32_t *coeff, int stride,
                               TX_TYPE tx_type, int bd) {
  __m256i in[32], out[32];
  const TX_SIZE tx_size = TX_16X16;
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const int width_div8 = (width >> 3);
  const int width_div16 = (width >> 4);
  const int size = (height << 1);
  switch (tx_type) {
    case DCT_DCT:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fdct16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fdct16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case ADST_DCT:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fadst16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                   width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fdct16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case DCT_ADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fdct16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fadst16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                   width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case ADST_ADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fadst16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                   width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fadst16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                   width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case FLIPADST_DCT:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 1, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fadst16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                   width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fdct16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case DCT_FLIPADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 1);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fdct16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fadst16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                   width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case FLIPADST_FLIPADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 1, 1);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fadst16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                   width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fadst16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                   width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case ADST_FLIPADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 1);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fadst16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                   width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fadst16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                   width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case FLIPADST_ADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 1, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fadst16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                   width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fadst16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                   width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case IDTX:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      idtx16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      idtx16_avx2(out, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case V_DCT:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fdct16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      idtx16_avx2(out, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case H_DCT:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      idtx16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fdct16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case V_ADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fadst16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                   width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      idtx16_avx2(out, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case H_ADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      idtx16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fadst16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                   width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case V_FLIPADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 1, 0);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      fadst16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                   width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      idtx16_avx2(out, in, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                  width_div8);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    case H_FLIPADST:
      load_buffer_16xn_avx2(input, in, stride, height, width_div8, 0, 1);
      round_shift_32_8xn_avx2(in, size, shift[0], width_div16);
      idtx16_avx2(in, out, av1_fwd_cos_bit_col[txw_idx][txh_idx], width_div8,
                  width_div8);
      round_shift_32_8xn_avx2(out, size, shift[1], width_div16);
      fwd_txfm_transpose_16x16_avx2(out, in);
      fadst16_avx2(in, out, av1_fwd_cos_bit_row[txw_idx][txh_idx], width_div8,
                   width_div8);
      fwd_txfm_transpose_16x16_avx2(out, in);
      store_buffer_avx2(in, coeff, 8, 32);
      break;
    default: assert(0);
  }
  (void)bd;
}
static INLINE void fdct32_avx2(__m256i *input, __m256i *output,
                               const int8_t cos_bit, const int instride,
                               const int outstride) {
  __m256i buf0[32];
  __m256i buf1[32];
  const int32_t *cospi;
  int startidx = 0 * instride;
  int endidx = 31 * instride;
  // stage 0
  // stage 1
  buf1[0] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[31] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[1] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[30] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[2] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[29] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[3] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[28] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[4] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[27] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[5] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[26] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[6] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[25] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[7] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[24] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[8] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[23] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[9] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[22] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[10] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[21] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[11] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[20] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[12] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[19] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[13] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[18] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[14] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[17] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  buf1[15] = _mm256_add_epi32(input[startidx], input[endidx]);
  buf1[16] = _mm256_sub_epi32(input[startidx], input[endidx]);

  // stage 2
  cospi = cospi_arr(cos_bit);
  buf0[0] = _mm256_add_epi32(buf1[0], buf1[15]);
  buf0[15] = _mm256_sub_epi32(buf1[0], buf1[15]);
  buf0[1] = _mm256_add_epi32(buf1[1], buf1[14]);
  buf0[14] = _mm256_sub_epi32(buf1[1], buf1[14]);
  buf0[2] = _mm256_add_epi32(buf1[2], buf1[13]);
  buf0[13] = _mm256_sub_epi32(buf1[2], buf1[13]);
  buf0[3] = _mm256_add_epi32(buf1[3], buf1[12]);
  buf0[12] = _mm256_sub_epi32(buf1[3], buf1[12]);
  buf0[4] = _mm256_add_epi32(buf1[4], buf1[11]);
  buf0[11] = _mm256_sub_epi32(buf1[4], buf1[11]);
  buf0[5] = _mm256_add_epi32(buf1[5], buf1[10]);
  buf0[10] = _mm256_sub_epi32(buf1[5], buf1[10]);
  buf0[6] = _mm256_add_epi32(buf1[6], buf1[9]);
  buf0[9] = _mm256_sub_epi32(buf1[6], buf1[9]);
  buf0[7] = _mm256_add_epi32(buf1[7], buf1[8]);
  buf0[8] = _mm256_sub_epi32(buf1[7], buf1[8]);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  buf0[18] = buf1[18];
  buf0[19] = buf1[19];
  btf_32_avx2_type0(-cospi[32], cospi[32], buf1[20], buf1[27], buf0[20],
                    buf0[27], cos_bit);
  btf_32_avx2_type0(-cospi[32], cospi[32], buf1[21], buf1[26], buf0[21],
                    buf0[26], cos_bit);
  btf_32_avx2_type0(-cospi[32], cospi[32], buf1[22], buf1[25], buf0[22],
                    buf0[25], cos_bit);
  btf_32_avx2_type0(-cospi[32], cospi[32], buf1[23], buf1[24], buf0[23],
                    buf0[24], cos_bit);
  buf0[28] = buf1[28];
  buf0[29] = buf1[29];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 3
  cospi = cospi_arr(cos_bit);
  buf1[0] = _mm256_add_epi32(buf0[0], buf0[7]);
  buf1[7] = _mm256_sub_epi32(buf0[0], buf0[7]);
  buf1[1] = _mm256_add_epi32(buf0[1], buf0[6]);
  buf1[6] = _mm256_sub_epi32(buf0[1], buf0[6]);
  buf1[2] = _mm256_add_epi32(buf0[2], buf0[5]);
  buf1[5] = _mm256_sub_epi32(buf0[2], buf0[5]);
  buf1[3] = _mm256_add_epi32(buf0[3], buf0[4]);
  buf1[4] = _mm256_sub_epi32(buf0[3], buf0[4]);
  buf1[8] = buf0[8];
  buf1[9] = buf0[9];
  btf_32_avx2_type0(-cospi[32], cospi[32], buf0[10], buf0[13], buf1[10],
                    buf1[13], cos_bit);
  btf_32_avx2_type0(-cospi[32], cospi[32], buf0[11], buf0[12], buf1[11],
                    buf1[12], cos_bit);
  buf1[14] = buf0[14];
  buf1[15] = buf0[15];
  buf1[16] = _mm256_add_epi32(buf0[16], buf0[23]);
  buf1[23] = _mm256_sub_epi32(buf0[16], buf0[23]);
  buf1[17] = _mm256_add_epi32(buf0[17], buf0[22]);
  buf1[22] = _mm256_sub_epi32(buf0[17], buf0[22]);
  buf1[18] = _mm256_add_epi32(buf0[18], buf0[21]);
  buf1[21] = _mm256_sub_epi32(buf0[18], buf0[21]);
  buf1[19] = _mm256_add_epi32(buf0[19], buf0[20]);
  buf1[20] = _mm256_sub_epi32(buf0[19], buf0[20]);
  buf1[24] = _mm256_sub_epi32(buf0[31], buf0[24]);
  buf1[31] = _mm256_add_epi32(buf0[31], buf0[24]);
  buf1[25] = _mm256_sub_epi32(buf0[30], buf0[25]);
  buf1[30] = _mm256_add_epi32(buf0[30], buf0[25]);
  buf1[26] = _mm256_sub_epi32(buf0[29], buf0[26]);
  buf1[29] = _mm256_add_epi32(buf0[29], buf0[26]);
  buf1[27] = _mm256_sub_epi32(buf0[28], buf0[27]);
  buf1[28] = _mm256_add_epi32(buf0[28], buf0[27]);

  // stage 4
  cospi = cospi_arr(cos_bit);
  buf0[0] = _mm256_add_epi32(buf1[0], buf1[3]);
  buf0[3] = _mm256_sub_epi32(buf1[0], buf1[3]);
  buf0[1] = _mm256_add_epi32(buf1[1], buf1[2]);
  buf0[2] = _mm256_sub_epi32(buf1[1], buf1[2]);
  buf0[4] = buf1[4];
  btf_32_avx2_type0(-cospi[32], cospi[32], buf1[5], buf1[6], buf0[5], buf0[6],
                    cos_bit);
  buf0[7] = buf1[7];
  buf0[8] = _mm256_add_epi32(buf1[8], buf1[11]);
  buf0[11] = _mm256_sub_epi32(buf1[8], buf1[11]);
  buf0[9] = _mm256_add_epi32(buf1[9], buf1[10]);
  buf0[10] = _mm256_sub_epi32(buf1[9], buf1[10]);
  buf0[12] = _mm256_sub_epi32(buf1[15], buf1[12]);
  buf0[15] = _mm256_add_epi32(buf1[15], buf1[12]);
  buf0[13] = _mm256_sub_epi32(buf1[14], buf1[13]);
  buf0[14] = _mm256_add_epi32(buf1[14], buf1[13]);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  btf_32_avx2_type0(-cospi[16], cospi[48], buf1[18], buf1[29], buf0[18],
                    buf0[29], cos_bit);
  btf_32_avx2_type0(-cospi[16], cospi[48], buf1[19], buf1[28], buf0[19],
                    buf0[28], cos_bit);
  btf_32_avx2_type0(-cospi[48], -cospi[16], buf1[20], buf1[27], buf0[20],
                    buf0[27], cos_bit);
  btf_32_avx2_type0(-cospi[48], -cospi[16], buf1[21], buf1[26], buf0[21],
                    buf0[26], cos_bit);
  buf0[22] = buf1[22];
  buf0[23] = buf1[23];
  buf0[24] = buf1[24];
  buf0[25] = buf1[25];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 5
  cospi = cospi_arr(cos_bit);
  btf_32_avx2_type0(cospi[32], cospi[32], buf0[0], buf0[1], buf1[0], buf1[1],
                    cos_bit);
  btf_32_avx2_type0(cospi[16], cospi[48], buf0[3], buf0[2], buf1[2], buf1[3],
                    cos_bit);
  buf1[4] = _mm256_add_epi32(buf0[4], buf0[5]);
  buf1[5] = _mm256_sub_epi32(buf0[4], buf0[5]);
  buf1[6] = _mm256_sub_epi32(buf0[7], buf0[6]);
  buf1[7] = _mm256_add_epi32(buf0[7], buf0[6]);
  buf1[8] = buf0[8];
  btf_32_avx2_type0(-cospi[16], cospi[48], buf0[9], buf0[14], buf1[9], buf1[14],
                    cos_bit);
  btf_32_avx2_type0(-cospi[48], -cospi[16], buf0[10], buf0[13], buf1[10],
                    buf1[13], cos_bit);
  buf1[11] = buf0[11];
  buf1[12] = buf0[12];
  buf1[15] = buf0[15];
  buf1[16] = _mm256_add_epi32(buf0[16], buf0[19]);
  buf1[19] = _mm256_sub_epi32(buf0[16], buf0[19]);
  buf1[17] = _mm256_add_epi32(buf0[17], buf0[18]);
  buf1[18] = _mm256_sub_epi32(buf0[17], buf0[18]);
  buf1[20] = _mm256_sub_epi32(buf0[23], buf0[20]);
  buf1[23] = _mm256_add_epi32(buf0[23], buf0[20]);
  buf1[21] = _mm256_sub_epi32(buf0[22], buf0[21]);
  buf1[22] = _mm256_add_epi32(buf0[22], buf0[21]);
  buf1[24] = _mm256_add_epi32(buf0[24], buf0[27]);
  buf1[27] = _mm256_sub_epi32(buf0[24], buf0[27]);
  buf1[25] = _mm256_add_epi32(buf0[25], buf0[26]);
  buf1[26] = _mm256_sub_epi32(buf0[25], buf0[26]);
  buf1[28] = _mm256_sub_epi32(buf0[31], buf0[28]);
  buf1[31] = _mm256_add_epi32(buf0[31], buf0[28]);
  buf1[29] = _mm256_sub_epi32(buf0[30], buf0[29]);
  buf1[30] = _mm256_add_epi32(buf0[30], buf0[29]);

  // stage 6
  cospi = cospi_arr(cos_bit);
  buf0[0] = buf1[0];
  buf0[1] = buf1[1];
  buf0[2] = buf1[2];
  buf0[3] = buf1[3];
  btf_32_avx2_type0(cospi[8], cospi[56], buf1[7], buf1[4], buf0[4], buf0[7],
                    cos_bit);
  btf_32_avx2_type0(cospi[40], cospi[24], buf1[6], buf1[5], buf0[5], buf0[6],
                    cos_bit);
  buf0[8] = _mm256_add_epi32(buf1[8], buf1[9]);
  buf0[9] = _mm256_sub_epi32(buf1[8], buf1[9]);
  buf0[10] = _mm256_sub_epi32(buf1[11], buf1[10]);
  buf0[11] = _mm256_add_epi32(buf1[11], buf1[10]);
  buf0[12] = _mm256_add_epi32(buf1[12], buf1[13]);
  buf0[13] = _mm256_sub_epi32(buf1[12], buf1[13]);
  buf0[14] = _mm256_sub_epi32(buf1[15], buf1[14]);
  buf0[15] = _mm256_add_epi32(buf1[15], buf1[14]);
  buf0[16] = buf1[16];
  btf_32_avx2_type0(-cospi[8], cospi[56], buf1[17], buf1[30], buf0[17],
                    buf0[30], cos_bit);
  btf_32_avx2_type0(-cospi[56], -cospi[8], buf1[18], buf1[29], buf0[18],
                    buf0[29], cos_bit);
  buf0[19] = buf1[19];
  buf0[20] = buf1[20];
  btf_32_avx2_type0(-cospi[40], cospi[24], buf1[21], buf1[26], buf0[21],
                    buf0[26], cos_bit);
  btf_32_avx2_type0(-cospi[24], -cospi[40], buf1[22], buf1[25], buf0[22],
                    buf0[25], cos_bit);
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
  btf_32_avx2_type0(cospi[4], cospi[60], buf0[15], buf0[8], buf1[8], buf1[15],
                    cos_bit);
  btf_32_avx2_type0(cospi[36], cospi[28], buf0[14], buf0[9], buf1[9], buf1[14],
                    cos_bit);
  btf_32_avx2_type0(cospi[20], cospi[44], buf0[13], buf0[10], buf1[10],
                    buf1[13], cos_bit);
  btf_32_avx2_type0(cospi[52], cospi[12], buf0[12], buf0[11], buf1[11],
                    buf1[12], cos_bit);
  buf1[16] = _mm256_add_epi32(buf0[16], buf0[17]);
  buf1[17] = _mm256_sub_epi32(buf0[16], buf0[17]);
  buf1[18] = _mm256_sub_epi32(buf0[19], buf0[18]);
  buf1[19] = _mm256_add_epi32(buf0[19], buf0[18]);
  buf1[20] = _mm256_add_epi32(buf0[20], buf0[21]);
  buf1[21] = _mm256_sub_epi32(buf0[20], buf0[21]);
  buf1[22] = _mm256_sub_epi32(buf0[23], buf0[22]);
  buf1[23] = _mm256_add_epi32(buf0[23], buf0[22]);
  buf1[24] = _mm256_add_epi32(buf0[24], buf0[25]);
  buf1[25] = _mm256_sub_epi32(buf0[24], buf0[25]);
  buf1[26] = _mm256_sub_epi32(buf0[27], buf0[26]);
  buf1[27] = _mm256_add_epi32(buf0[27], buf0[26]);
  buf1[28] = _mm256_add_epi32(buf0[28], buf0[29]);
  buf1[29] = _mm256_sub_epi32(buf0[28], buf0[29]);
  buf1[30] = _mm256_sub_epi32(buf0[31], buf0[30]);
  buf1[31] = _mm256_add_epi32(buf0[31], buf0[30]);

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
  btf_32_avx2_type0(cospi[2], cospi[62], buf1[31], buf1[16], buf0[16], buf0[31],
                    cos_bit);
  btf_32_avx2_type0(cospi[34], cospi[30], buf1[30], buf1[17], buf0[17],
                    buf0[30], cos_bit);
  btf_32_avx2_type0(cospi[18], cospi[46], buf1[29], buf1[18], buf0[18],
                    buf0[29], cos_bit);
  btf_32_avx2_type0(cospi[50], cospi[14], buf1[28], buf1[19], buf0[19],
                    buf0[28], cos_bit);
  btf_32_avx2_type0(cospi[10], cospi[54], buf1[27], buf1[20], buf0[20],
                    buf0[27], cos_bit);
  btf_32_avx2_type0(cospi[42], cospi[22], buf1[26], buf1[21], buf0[21],
                    buf0[26], cos_bit);
  btf_32_avx2_type0(cospi[26], cospi[38], buf1[25], buf1[22], buf0[22],
                    buf0[25], cos_bit);
  btf_32_avx2_type0(cospi[58], cospi[6], buf1[24], buf1[23], buf0[23], buf0[24],
                    cos_bit);

  startidx = 0 * outstride;
  endidx = 31 * outstride;
  // stage 9
  output[startidx] = buf0[0];
  output[endidx] = buf0[31];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[16];
  output[endidx] = buf0[15];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[8];
  output[endidx] = buf0[23];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[24];
  output[endidx] = buf0[7];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[4];
  output[endidx] = buf0[27];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[20];
  output[endidx] = buf0[11];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[12];
  output[endidx] = buf0[19];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[28];
  output[endidx] = buf0[3];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[2];
  output[endidx] = buf0[29];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[18];
  output[endidx] = buf0[13];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[10];
  output[endidx] = buf0[21];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[26];
  output[endidx] = buf0[5];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[6];
  output[endidx] = buf0[25];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[22];
  output[endidx] = buf0[9];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[14];
  output[endidx] = buf0[17];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = buf0[30];
  output[endidx] = buf0[1];
}
static INLINE void idtx32x32_avx2(__m256i *input, __m256i *output,
                                  const int8_t cos_bit, int instride,
                                  int outstride) {
  (void)cos_bit;
  for (int i = 0; i < 32; i += 8) {
    output[i * outstride] = _mm256_slli_epi32(input[i * instride], 2);
    output[(i + 1) * outstride] =
        _mm256_slli_epi32(input[(i + 1) * instride], 2);
    output[(i + 2) * outstride] =
        _mm256_slli_epi32(input[(i + 2) * instride], 2);
    output[(i + 3) * outstride] =
        _mm256_slli_epi32(input[(i + 3) * instride], 2);
    output[(i + 4) * outstride] =
        _mm256_slli_epi32(input[(i + 4) * instride], 2);
    output[(i + 5) * outstride] =
        _mm256_slli_epi32(input[(i + 5) * instride], 2);
    output[(i + 6) * outstride] =
        _mm256_slli_epi32(input[(i + 6) * instride], 2);
    output[(i + 7) * outstride] =
        _mm256_slli_epi32(input[(i + 7) * instride], 2);
  }
}
static const transform_1d_avx2 col_txfm8x32_arr[TX_TYPES] = {
  fdct32_avx2,     // DCT_DCT
  NULL,            // ADST_DCT
  NULL,            // DCT_ADST
  NULL,            // ADST_ADST
  NULL,            // FLIPADST_DCT
  NULL,            // DCT_FLIPADST
  NULL,            // FLIPADST_FLIPADST
  NULL,            // ADST_FLIPADST
  NULL,            // FLIPADST_ADST
  idtx32x32_avx2,  // IDTX
  NULL,            // V_DCT
  NULL,            // H_DCT
  NULL,            // V_ADST
  NULL,            // H_ADST
  NULL,            // V_FLIPADST
  NULL             // H_FLIPADST
};
static const transform_1d_avx2 row_txfm8x32_arr[TX_TYPES] = {
  fdct32_avx2,     // DCT_DCT
  NULL,            // ADST_DCT
  NULL,            // DCT_ADST
  NULL,            // ADST_ADST
  NULL,            // FLIPADST_DCT
  NULL,            // DCT_FLIPADST
  NULL,            // FLIPADST_FLIPADST
  NULL,            // ADST_FLIPADST
  NULL,            // FLIPADST_ADST
  idtx32x32_avx2,  // IDTX
  NULL,            // V_DCT
  NULL,            // H_DCT
  NULL,            // V_ADST
  NULL,            // H_ADST
  NULL,            // V_FLIPADST
  NULL             // H_FLIPADST
};
void av1_fwd_txfm2d_32x32_avx2(const int16_t *input, int32_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m256i buf0[128], buf1[128];
  const int tx_size = TX_32X32;
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_avx2 col_txfm = col_txfm8x32_arr[tx_type];
  const transform_1d_avx2 row_txfm = row_txfm8x32_arr[tx_type];
  int r, c;
  const int width_div16 = (width >> 4);
  const int width_div8 = (width >> 3);

  for (int i = 0; i < width_div16; i++) {
    load_buffer_16xn_avx2(input + (i << 4), &buf0[(i << 1)], stride, height,
                          width_div8, 0, 0);
    round_shift_32_8xn_avx2(&buf0[(i << 1)], height, shift[0], width_div8);
    round_shift_32_8xn_avx2(&buf0[(i << 1) + 1], height, shift[0], width_div8);
    col_txfm(&buf0[(i << 1)], &buf0[(i << 1)], cos_bit_col, width_div8,
             width_div8);
    col_txfm(&buf0[(i << 1) + 1], &buf0[(i << 1) + 1], cos_bit_col, width_div8,
             width_div8);
    round_shift_32_8xn_avx2(&buf0[(i << 1)], height, shift[1], width_div8);
    round_shift_32_8xn_avx2(&buf0[(i << 1) + 1], height, shift[1], width_div8);
  }

  for (r = 0; r < height; r += 8) {
    for (c = 0; c < width_div8; c++) {
      fwd_txfm_transpose_8x8_avx2(&buf0[r * width_div8 + c],
                                  &buf1[c * 8 * width_div8 + (r >> 3)],
                                  width_div8, width_div8);
    }
  }

  for (int i = 0; i < width_div16; i++) {
    row_txfm(&buf1[(i << 1)], &buf1[(i << 1)], cos_bit_row, width_div8,
             width_div8);
    row_txfm(&buf1[(i << 1) + 1], &buf1[(i << 1) + 1], cos_bit_row, width_div8,
             width_div8);
    round_shift_32_8xn_avx2(&buf1[(i << 1)], height, shift[2], width_div8);
    round_shift_32_8xn_avx2(&buf1[(i << 1) + 1], height, shift[2], width_div8);
  }

  for (r = 0; r < height; r += 8) {
    for (c = 0; c < width_div8; c++) {
      fwd_txfm_transpose_8x8_avx2(&buf1[r * width_div8 + c],
                                  &buf0[c * 8 * width_div8 + (r >> 3)],
                                  width_div8, width_div8);
    }
  }

  store_buffer_avx2(buf0, output, 8, 128);
}
static INLINE void fdct64_stage2_avx2(__m256i *x1, __m256i *x2,
                                      __m256i *cospi_m32, __m256i *cospi_p32,
                                      const __m256i *__rounding,
                                      int8_t cos_bit) {
  x2[0] = _mm256_add_epi32(x1[0], x1[31]);
  x2[31] = _mm256_sub_epi32(x1[0], x1[31]);
  x2[1] = _mm256_add_epi32(x1[1], x1[30]);
  x2[30] = _mm256_sub_epi32(x1[1], x1[30]);
  x2[2] = _mm256_add_epi32(x1[2], x1[29]);
  x2[29] = _mm256_sub_epi32(x1[2], x1[29]);
  x2[3] = _mm256_add_epi32(x1[3], x1[28]);
  x2[28] = _mm256_sub_epi32(x1[3], x1[28]);
  x2[4] = _mm256_add_epi32(x1[4], x1[27]);
  x2[27] = _mm256_sub_epi32(x1[4], x1[27]);
  x2[5] = _mm256_add_epi32(x1[5], x1[26]);
  x2[26] = _mm256_sub_epi32(x1[5], x1[26]);
  x2[6] = _mm256_add_epi32(x1[6], x1[25]);
  x2[25] = _mm256_sub_epi32(x1[6], x1[25]);
  x2[7] = _mm256_add_epi32(x1[7], x1[24]);
  x2[24] = _mm256_sub_epi32(x1[7], x1[24]);
  x2[8] = _mm256_add_epi32(x1[8], x1[23]);
  x2[23] = _mm256_sub_epi32(x1[8], x1[23]);
  x2[9] = _mm256_add_epi32(x1[9], x1[22]);
  x2[22] = _mm256_sub_epi32(x1[9], x1[22]);
  x2[10] = _mm256_add_epi32(x1[10], x1[21]);
  x2[21] = _mm256_sub_epi32(x1[10], x1[21]);
  x2[11] = _mm256_add_epi32(x1[11], x1[20]);
  x2[20] = _mm256_sub_epi32(x1[11], x1[20]);
  x2[12] = _mm256_add_epi32(x1[12], x1[19]);
  x2[19] = _mm256_sub_epi32(x1[12], x1[19]);
  x2[13] = _mm256_add_epi32(x1[13], x1[18]);
  x2[18] = _mm256_sub_epi32(x1[13], x1[18]);
  x2[14] = _mm256_add_epi32(x1[14], x1[17]);
  x2[17] = _mm256_sub_epi32(x1[14], x1[17]);
  x2[15] = _mm256_add_epi32(x1[15], x1[16]);
  x2[16] = _mm256_sub_epi32(x1[15], x1[16]);
  x2[32] = x1[32];
  x2[33] = x1[33];
  x2[34] = x1[34];
  x2[35] = x1[35];
  x2[36] = x1[36];
  x2[37] = x1[37];
  x2[38] = x1[38];
  x2[39] = x1[39];
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x1[40], x1[55], x2[40], x2[55],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x1[41], x1[54], x2[41], x2[54],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x1[42], x1[53], x2[42], x2[53],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x1[43], x1[52], x2[43], x2[52],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x1[44], x1[51], x2[44], x2[51],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x1[45], x1[50], x2[45], x2[50],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x1[46], x1[49], x2[46], x2[49],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x1[47], x1[48], x2[47], x2[48],
                        *__rounding, cos_bit);
  x2[56] = x1[56];
  x2[57] = x1[57];
  x2[58] = x1[58];
  x2[59] = x1[59];
  x2[60] = x1[60];
  x2[61] = x1[61];
  x2[62] = x1[62];
  x2[63] = x1[63];
}
static INLINE void fdct64_stage3_avx2(__m256i *x2, __m256i *x3,
                                      __m256i *cospi_m32, __m256i *cospi_p32,
                                      const __m256i *__rounding,
                                      int8_t cos_bit) {
  x3[0] = _mm256_add_epi32(x2[0], x2[15]);
  x3[15] = _mm256_sub_epi32(x2[0], x2[15]);
  x3[1] = _mm256_add_epi32(x2[1], x2[14]);
  x3[14] = _mm256_sub_epi32(x2[1], x2[14]);
  x3[2] = _mm256_add_epi32(x2[2], x2[13]);
  x3[13] = _mm256_sub_epi32(x2[2], x2[13]);
  x3[3] = _mm256_add_epi32(x2[3], x2[12]);
  x3[12] = _mm256_sub_epi32(x2[3], x2[12]);
  x3[4] = _mm256_add_epi32(x2[4], x2[11]);
  x3[11] = _mm256_sub_epi32(x2[4], x2[11]);
  x3[5] = _mm256_add_epi32(x2[5], x2[10]);
  x3[10] = _mm256_sub_epi32(x2[5], x2[10]);
  x3[6] = _mm256_add_epi32(x2[6], x2[9]);
  x3[9] = _mm256_sub_epi32(x2[6], x2[9]);
  x3[7] = _mm256_add_epi32(x2[7], x2[8]);
  x3[8] = _mm256_sub_epi32(x2[7], x2[8]);
  x3[16] = x2[16];
  x3[17] = x2[17];
  x3[18] = x2[18];
  x3[19] = x2[19];
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x2[20], x2[27], x3[20], x3[27],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x2[21], x2[26], x3[21], x3[26],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x2[22], x2[25], x3[22], x3[25],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x2[23], x2[24], x3[23], x3[24],
                        *__rounding, cos_bit);
  x3[28] = x2[28];
  x3[29] = x2[29];
  x3[30] = x2[30];
  x3[31] = x2[31];
  x3[32] = _mm256_add_epi32(x2[32], x2[47]);
  x3[47] = _mm256_sub_epi32(x2[32], x2[47]);
  x3[33] = _mm256_add_epi32(x2[33], x2[46]);
  x3[46] = _mm256_sub_epi32(x2[33], x2[46]);
  x3[34] = _mm256_add_epi32(x2[34], x2[45]);
  x3[45] = _mm256_sub_epi32(x2[34], x2[45]);
  x3[35] = _mm256_add_epi32(x2[35], x2[44]);
  x3[44] = _mm256_sub_epi32(x2[35], x2[44]);
  x3[36] = _mm256_add_epi32(x2[36], x2[43]);
  x3[43] = _mm256_sub_epi32(x2[36], x2[43]);
  x3[37] = _mm256_add_epi32(x2[37], x2[42]);
  x3[42] = _mm256_sub_epi32(x2[37], x2[42]);
  x3[38] = _mm256_add_epi32(x2[38], x2[41]);
  x3[41] = _mm256_sub_epi32(x2[38], x2[41]);
  x3[39] = _mm256_add_epi32(x2[39], x2[40]);
  x3[40] = _mm256_sub_epi32(x2[39], x2[40]);
  x3[48] = _mm256_sub_epi32(x2[63], x2[48]);
  x3[63] = _mm256_add_epi32(x2[63], x2[48]);
  x3[49] = _mm256_sub_epi32(x2[62], x2[49]);
  x3[62] = _mm256_add_epi32(x2[62], x2[49]);
  x3[50] = _mm256_sub_epi32(x2[61], x2[50]);
  x3[61] = _mm256_add_epi32(x2[61], x2[50]);
  x3[51] = _mm256_sub_epi32(x2[60], x2[51]);
  x3[60] = _mm256_add_epi32(x2[60], x2[51]);
  x3[52] = _mm256_sub_epi32(x2[59], x2[52]);
  x3[59] = _mm256_add_epi32(x2[59], x2[52]);
  x3[53] = _mm256_sub_epi32(x2[58], x2[53]);
  x3[58] = _mm256_add_epi32(x2[58], x2[53]);
  x3[54] = _mm256_sub_epi32(x2[57], x2[54]);
  x3[57] = _mm256_add_epi32(x2[57], x2[54]);
  x3[55] = _mm256_sub_epi32(x2[56], x2[55]);
  x3[56] = _mm256_add_epi32(x2[56], x2[55]);
}
static INLINE void fdct64_stage4_avx2(__m256i *x3, __m256i *x4,
                                      __m256i *cospi_m32, __m256i *cospi_p32,
                                      __m256i *cospi_m16, __m256i *cospi_p48,
                                      __m256i *cospi_m48,
                                      const __m256i *__rounding,
                                      int8_t cos_bit) {
  x4[0] = _mm256_add_epi32(x3[0], x3[7]);
  x4[7] = _mm256_sub_epi32(x3[0], x3[7]);
  x4[1] = _mm256_add_epi32(x3[1], x3[6]);
  x4[6] = _mm256_sub_epi32(x3[1], x3[6]);
  x4[2] = _mm256_add_epi32(x3[2], x3[5]);
  x4[5] = _mm256_sub_epi32(x3[2], x3[5]);
  x4[3] = _mm256_add_epi32(x3[3], x3[4]);
  x4[4] = _mm256_sub_epi32(x3[3], x3[4]);
  x4[8] = x3[8];
  x4[9] = x3[9];
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x3[10], x3[13], x4[10], x4[13],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x3[11], x3[12], x4[11], x4[12],
                        *__rounding, cos_bit);
  x4[14] = x3[14];
  x4[15] = x3[15];
  x4[16] = _mm256_add_epi32(x3[16], x3[23]);
  x4[23] = _mm256_sub_epi32(x3[16], x3[23]);
  x4[17] = _mm256_add_epi32(x3[17], x3[22]);
  x4[22] = _mm256_sub_epi32(x3[17], x3[22]);
  x4[18] = _mm256_add_epi32(x3[18], x3[21]);
  x4[21] = _mm256_sub_epi32(x3[18], x3[21]);
  x4[19] = _mm256_add_epi32(x3[19], x3[20]);
  x4[20] = _mm256_sub_epi32(x3[19], x3[20]);
  x4[24] = _mm256_sub_epi32(x3[31], x3[24]);
  x4[31] = _mm256_add_epi32(x3[31], x3[24]);
  x4[25] = _mm256_sub_epi32(x3[30], x3[25]);
  x4[30] = _mm256_add_epi32(x3[30], x3[25]);
  x4[26] = _mm256_sub_epi32(x3[29], x3[26]);
  x4[29] = _mm256_add_epi32(x3[29], x3[26]);
  x4[27] = _mm256_sub_epi32(x3[28], x3[27]);
  x4[28] = _mm256_add_epi32(x3[28], x3[27]);
  x4[32] = x3[32];
  x4[33] = x3[33];
  x4[34] = x3[34];
  x4[35] = x3[35];
  btf_32_type0_avx2_new(*cospi_m16, *cospi_p48, x3[36], x3[59], x4[36], x4[59],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m16, *cospi_p48, x3[37], x3[58], x4[37], x4[58],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m16, *cospi_p48, x3[38], x3[57], x4[38], x4[57],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m16, *cospi_p48, x3[39], x3[56], x4[39], x4[56],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m48, *cospi_m16, x3[40], x3[55], x4[40], x4[55],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m48, *cospi_m16, x3[41], x3[54], x4[41], x4[54],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m48, *cospi_m16, x3[42], x3[53], x4[42], x4[53],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m48, *cospi_m16, x3[43], x3[52], x4[43], x4[52],
                        *__rounding, cos_bit);
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
}
static INLINE void fdct64_stage5_avx2(__m256i *x4, __m256i *x5,
                                      __m256i *cospi_m32, __m256i *cospi_p32,
                                      __m256i *cospi_m16, __m256i *cospi_p48,
                                      __m256i *cospi_m48,
                                      const __m256i *__rounding,
                                      int8_t cos_bit) {
  x5[0] = _mm256_add_epi32(x4[0], x4[3]);
  x5[3] = _mm256_sub_epi32(x4[0], x4[3]);
  x5[1] = _mm256_add_epi32(x4[1], x4[2]);
  x5[2] = _mm256_sub_epi32(x4[1], x4[2]);
  x5[4] = x4[4];
  btf_32_type0_avx2_new(*cospi_m32, *cospi_p32, x4[5], x4[6], x5[5], x5[6],
                        *__rounding, cos_bit);
  x5[7] = x4[7];
  x5[8] = _mm256_add_epi32(x4[8], x4[11]);
  x5[11] = _mm256_sub_epi32(x4[8], x4[11]);
  x5[9] = _mm256_add_epi32(x4[9], x4[10]);
  x5[10] = _mm256_sub_epi32(x4[9], x4[10]);
  x5[12] = _mm256_sub_epi32(x4[15], x4[12]);
  x5[15] = _mm256_add_epi32(x4[15], x4[12]);
  x5[13] = _mm256_sub_epi32(x4[14], x4[13]);
  x5[14] = _mm256_add_epi32(x4[14], x4[13]);
  x5[16] = x4[16];
  x5[17] = x4[17];
  btf_32_type0_avx2_new(*cospi_m16, *cospi_p48, x4[18], x4[29], x5[18], x5[29],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m16, *cospi_p48, x4[19], x4[28], x5[19], x5[28],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m48, *cospi_m16, x4[20], x4[27], x5[20], x5[27],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m48, *cospi_m16, x4[21], x4[26], x5[21], x5[26],
                        *__rounding, cos_bit);
  x5[22] = x4[22];
  x5[23] = x4[23];
  x5[24] = x4[24];
  x5[25] = x4[25];
  x5[30] = x4[30];
  x5[31] = x4[31];
  x5[32] = _mm256_add_epi32(x4[32], x4[39]);
  x5[39] = _mm256_sub_epi32(x4[32], x4[39]);
  x5[33] = _mm256_add_epi32(x4[33], x4[38]);
  x5[38] = _mm256_sub_epi32(x4[33], x4[38]);
  x5[34] = _mm256_add_epi32(x4[34], x4[37]);
  x5[37] = _mm256_sub_epi32(x4[34], x4[37]);
  x5[35] = _mm256_add_epi32(x4[35], x4[36]);
  x5[36] = _mm256_sub_epi32(x4[35], x4[36]);
  x5[40] = _mm256_sub_epi32(x4[47], x4[40]);
  x5[47] = _mm256_add_epi32(x4[47], x4[40]);
  x5[41] = _mm256_sub_epi32(x4[46], x4[41]);
  x5[46] = _mm256_add_epi32(x4[46], x4[41]);
  x5[42] = _mm256_sub_epi32(x4[45], x4[42]);
  x5[45] = _mm256_add_epi32(x4[45], x4[42]);
  x5[43] = _mm256_sub_epi32(x4[44], x4[43]);
  x5[44] = _mm256_add_epi32(x4[44], x4[43]);
  x5[48] = _mm256_add_epi32(x4[48], x4[55]);
  x5[55] = _mm256_sub_epi32(x4[48], x4[55]);
  x5[49] = _mm256_add_epi32(x4[49], x4[54]);
  x5[54] = _mm256_sub_epi32(x4[49], x4[54]);
  x5[50] = _mm256_add_epi32(x4[50], x4[53]);
  x5[53] = _mm256_sub_epi32(x4[50], x4[53]);
  x5[51] = _mm256_add_epi32(x4[51], x4[52]);
  x5[52] = _mm256_sub_epi32(x4[51], x4[52]);
  x5[56] = _mm256_sub_epi32(x4[63], x4[56]);
  x5[63] = _mm256_add_epi32(x4[63], x4[56]);
  x5[57] = _mm256_sub_epi32(x4[62], x4[57]);
  x5[62] = _mm256_add_epi32(x4[62], x4[57]);
  x5[58] = _mm256_sub_epi32(x4[61], x4[58]);
  x5[61] = _mm256_add_epi32(x4[61], x4[58]);
  x5[59] = _mm256_sub_epi32(x4[60], x4[59]);
  x5[60] = _mm256_add_epi32(x4[60], x4[59]);
}
static INLINE void fdct64_stage6_avx2(
    __m256i *x5, __m256i *x6, __m256i *cospi_p16, __m256i *cospi_p32,
    __m256i *cospi_m16, __m256i *cospi_p48, __m256i *cospi_m48,
    __m256i *cospi_m08, __m256i *cospi_p56, __m256i *cospi_m56,
    __m256i *cospi_m40, __m256i *cospi_p24, __m256i *cospi_m24,
    const __m256i *__rounding, int8_t cos_bit) {
  btf_32_type0_avx2_new(*cospi_p32, *cospi_p32, x5[0], x5[1], x6[0], x6[1],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_p16, *cospi_p48, x5[3], x5[2], x6[2], x6[3],
                        *__rounding, cos_bit);
  x6[4] = _mm256_add_epi32(x5[4], x5[5]);
  x6[5] = _mm256_sub_epi32(x5[4], x5[5]);
  x6[6] = _mm256_sub_epi32(x5[7], x5[6]);
  x6[7] = _mm256_add_epi32(x5[7], x5[6]);
  x6[8] = x5[8];
  btf_32_type0_avx2_new(*cospi_m16, *cospi_p48, x5[9], x5[14], x6[9], x6[14],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m48, *cospi_m16, x5[10], x5[13], x6[10], x6[13],
                        *__rounding, cos_bit);
  x6[11] = x5[11];
  x6[12] = x5[12];
  x6[15] = x5[15];
  x6[16] = _mm256_add_epi32(x5[16], x5[19]);
  x6[19] = _mm256_sub_epi32(x5[16], x5[19]);
  x6[17] = _mm256_add_epi32(x5[17], x5[18]);
  x6[18] = _mm256_sub_epi32(x5[17], x5[18]);
  x6[20] = _mm256_sub_epi32(x5[23], x5[20]);
  x6[23] = _mm256_add_epi32(x5[23], x5[20]);
  x6[21] = _mm256_sub_epi32(x5[22], x5[21]);
  x6[22] = _mm256_add_epi32(x5[22], x5[21]);
  x6[24] = _mm256_add_epi32(x5[24], x5[27]);
  x6[27] = _mm256_sub_epi32(x5[24], x5[27]);
  x6[25] = _mm256_add_epi32(x5[25], x5[26]);
  x6[26] = _mm256_sub_epi32(x5[25], x5[26]);
  x6[28] = _mm256_sub_epi32(x5[31], x5[28]);
  x6[31] = _mm256_add_epi32(x5[31], x5[28]);
  x6[29] = _mm256_sub_epi32(x5[30], x5[29]);
  x6[30] = _mm256_add_epi32(x5[30], x5[29]);
  x6[32] = x5[32];
  x6[33] = x5[33];
  btf_32_type0_avx2_new(*cospi_m08, *cospi_p56, x5[34], x5[61], x6[34], x6[61],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m08, *cospi_p56, x5[35], x5[60], x6[35], x6[60],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m56, *cospi_m08, x5[36], x5[59], x6[36], x6[59],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m56, *cospi_m08, x5[37], x5[58], x6[37], x6[58],
                        *__rounding, cos_bit);
  x6[38] = x5[38];
  x6[39] = x5[39];
  x6[40] = x5[40];
  x6[41] = x5[41];
  btf_32_type0_avx2_new(*cospi_m40, *cospi_p24, x5[42], x5[53], x6[42], x6[53],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m40, *cospi_p24, x5[43], x5[52], x6[43], x6[52],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m24, *cospi_m40, x5[44], x5[51], x6[44], x6[51],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m24, *cospi_m40, x5[45], x5[50], x6[45], x6[50],
                        *__rounding, cos_bit);
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
}
static INLINE void fdct64_stage7_avx2(__m256i *x6, __m256i *x7,
                                      __m256i *cospi_p08, __m256i *cospi_p56,
                                      __m256i *cospi_p40, __m256i *cospi_p24,
                                      __m256i *cospi_m08, __m256i *cospi_m56,
                                      __m256i *cospi_m40, __m256i *cospi_m24,
                                      const __m256i *__rounding,
                                      int8_t cos_bit) {
  x7[0] = x6[0];
  x7[1] = x6[1];
  x7[2] = x6[2];
  x7[3] = x6[3];
  btf_32_type0_avx2_new(*cospi_p08, *cospi_p56, x6[7], x6[4], x7[4], x7[7],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_p40, *cospi_p24, x6[6], x6[5], x7[5], x7[6],
                        *__rounding, cos_bit);
  x7[8] = _mm256_add_epi32(x6[8], x6[9]);
  x7[9] = _mm256_sub_epi32(x6[8], x6[9]);
  x7[10] = _mm256_sub_epi32(x6[11], x6[10]);
  x7[11] = _mm256_add_epi32(x6[11], x6[10]);
  x7[12] = _mm256_add_epi32(x6[12], x6[13]);
  x7[13] = _mm256_sub_epi32(x6[12], x6[13]);
  x7[14] = _mm256_sub_epi32(x6[15], x6[14]);
  x7[15] = _mm256_add_epi32(x6[15], x6[14]);
  x7[16] = x6[16];
  btf_32_type0_avx2_new(*cospi_m08, *cospi_p56, x6[17], x6[30], x7[17], x7[30],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m56, *cospi_m08, x6[18], x6[29], x7[18], x7[29],
                        *__rounding, cos_bit);
  x7[19] = x6[19];
  x7[20] = x6[20];
  btf_32_type0_avx2_new(*cospi_m40, *cospi_p24, x6[21], x6[26], x7[21], x7[26],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(*cospi_m24, *cospi_m40, x6[22], x6[25], x7[22], x7[25],
                        *__rounding, cos_bit);
  x7[23] = x6[23];
  x7[24] = x6[24];
  x7[27] = x6[27];
  x7[28] = x6[28];
  x7[31] = x6[31];
  x7[32] = _mm256_add_epi32(x6[32], x6[35]);
  x7[35] = _mm256_sub_epi32(x6[32], x6[35]);
  x7[33] = _mm256_add_epi32(x6[33], x6[34]);
  x7[34] = _mm256_sub_epi32(x6[33], x6[34]);
  x7[36] = _mm256_sub_epi32(x6[39], x6[36]);
  x7[39] = _mm256_add_epi32(x6[39], x6[36]);
  x7[37] = _mm256_sub_epi32(x6[38], x6[37]);
  x7[38] = _mm256_add_epi32(x6[38], x6[37]);
  x7[40] = _mm256_add_epi32(x6[40], x6[43]);
  x7[43] = _mm256_sub_epi32(x6[40], x6[43]);
  x7[41] = _mm256_add_epi32(x6[41], x6[42]);
  x7[42] = _mm256_sub_epi32(x6[41], x6[42]);
  x7[44] = _mm256_sub_epi32(x6[47], x6[44]);
  x7[47] = _mm256_add_epi32(x6[47], x6[44]);
  x7[45] = _mm256_sub_epi32(x6[46], x6[45]);
  x7[46] = _mm256_add_epi32(x6[46], x6[45]);
  x7[48] = _mm256_add_epi32(x6[48], x6[51]);
  x7[51] = _mm256_sub_epi32(x6[48], x6[51]);
  x7[49] = _mm256_add_epi32(x6[49], x6[50]);
  x7[50] = _mm256_sub_epi32(x6[49], x6[50]);
  x7[52] = _mm256_sub_epi32(x6[55], x6[52]);
  x7[55] = _mm256_add_epi32(x6[55], x6[52]);
  x7[53] = _mm256_sub_epi32(x6[54], x6[53]);
  x7[54] = _mm256_add_epi32(x6[54], x6[53]);
  x7[56] = _mm256_add_epi32(x6[56], x6[59]);
  x7[59] = _mm256_sub_epi32(x6[56], x6[59]);
  x7[57] = _mm256_add_epi32(x6[57], x6[58]);
  x7[58] = _mm256_sub_epi32(x6[57], x6[58]);
  x7[60] = _mm256_sub_epi32(x6[63], x6[60]);
  x7[63] = _mm256_add_epi32(x6[63], x6[60]);
  x7[61] = _mm256_sub_epi32(x6[62], x6[61]);
  x7[62] = _mm256_add_epi32(x6[62], x6[61]);
}
static INLINE void fdct64_stage8_avx2(__m256i *x7, __m256i *x8,
                                      const int32_t *cospi,
                                      const __m256i *__rounding,
                                      int8_t cos_bit) {
  __m256i cospi_p60 = _mm256_set1_epi32(cospi[60]);
  __m256i cospi_p04 = _mm256_set1_epi32(cospi[4]);
  __m256i cospi_p28 = _mm256_set1_epi32(cospi[28]);
  __m256i cospi_p36 = _mm256_set1_epi32(cospi[36]);
  __m256i cospi_p44 = _mm256_set1_epi32(cospi[44]);
  __m256i cospi_p20 = _mm256_set1_epi32(cospi[20]);
  __m256i cospi_p12 = _mm256_set1_epi32(cospi[12]);
  __m256i cospi_p52 = _mm256_set1_epi32(cospi[52]);
  __m256i cospi_m04 = _mm256_set1_epi32(-cospi[4]);
  __m256i cospi_m60 = _mm256_set1_epi32(-cospi[60]);
  __m256i cospi_m36 = _mm256_set1_epi32(-cospi[36]);
  __m256i cospi_m28 = _mm256_set1_epi32(-cospi[28]);
  __m256i cospi_m20 = _mm256_set1_epi32(-cospi[20]);
  __m256i cospi_m44 = _mm256_set1_epi32(-cospi[44]);
  __m256i cospi_m52 = _mm256_set1_epi32(-cospi[52]);
  __m256i cospi_m12 = _mm256_set1_epi32(-cospi[12]);

  x8[0] = x7[0];
  x8[1] = x7[1];
  x8[2] = x7[2];
  x8[3] = x7[3];
  x8[4] = x7[4];
  x8[5] = x7[5];
  x8[6] = x7[6];
  x8[7] = x7[7];

  btf_32_type0_avx2_new(cospi_p04, cospi_p60, x7[15], x7[8], x8[8], x8[15],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p36, cospi_p28, x7[14], x7[9], x8[9], x8[14],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p20, cospi_p44, x7[13], x7[10], x8[10], x8[13],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p52, cospi_p12, x7[12], x7[11], x8[11], x8[12],
                        *__rounding, cos_bit);
  x8[16] = _mm256_add_epi32(x7[16], x7[17]);
  x8[17] = _mm256_sub_epi32(x7[16], x7[17]);
  x8[18] = _mm256_sub_epi32(x7[19], x7[18]);
  x8[19] = _mm256_add_epi32(x7[19], x7[18]);
  x8[20] = _mm256_add_epi32(x7[20], x7[21]);
  x8[21] = _mm256_sub_epi32(x7[20], x7[21]);
  x8[22] = _mm256_sub_epi32(x7[23], x7[22]);
  x8[23] = _mm256_add_epi32(x7[23], x7[22]);
  x8[24] = _mm256_add_epi32(x7[24], x7[25]);
  x8[25] = _mm256_sub_epi32(x7[24], x7[25]);
  x8[26] = _mm256_sub_epi32(x7[27], x7[26]);
  x8[27] = _mm256_add_epi32(x7[27], x7[26]);
  x8[28] = _mm256_add_epi32(x7[28], x7[29]);
  x8[29] = _mm256_sub_epi32(x7[28], x7[29]);
  x8[30] = _mm256_sub_epi32(x7[31], x7[30]);
  x8[31] = _mm256_add_epi32(x7[31], x7[30]);
  x8[32] = x7[32];
  btf_32_type0_avx2_new(cospi_m04, cospi_p60, x7[33], x7[62], x8[33], x8[62],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_m60, cospi_m04, x7[34], x7[61], x8[34], x8[61],
                        *__rounding, cos_bit);
  x8[35] = x7[35];
  x8[36] = x7[36];
  btf_32_type0_avx2_new(cospi_m36, cospi_p28, x7[37], x7[58], x8[37], x8[58],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_m28, cospi_m36, x7[38], x7[57], x8[38], x8[57],
                        *__rounding, cos_bit);
  x8[39] = x7[39];
  x8[40] = x7[40];
  btf_32_type0_avx2_new(cospi_m20, cospi_p44, x7[41], x7[54], x8[41], x8[54],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_m44, cospi_m20, x7[42], x7[53], x8[42], x8[53],
                        *__rounding, cos_bit);
  x8[43] = x7[43];
  x8[44] = x7[44];
  btf_32_type0_avx2_new(cospi_m52, cospi_p12, x7[45], x7[50], x8[45], x8[50],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_m12, cospi_m52, x7[46], x7[49], x8[46], x8[49],
                        *__rounding, cos_bit);
  x8[47] = x7[47];
  x8[48] = x7[48];
  x8[51] = x7[51];
  x8[52] = x7[52];
  x8[55] = x7[55];
  x8[56] = x7[56];
  x8[59] = x7[59];
  x8[60] = x7[60];
  x8[63] = x7[63];
}
static INLINE void fdct64_stage9_avx2(__m256i *x8, __m256i *x9,
                                      const int32_t *cospi,
                                      const __m256i *__rounding,
                                      int8_t cos_bit) {
  __m256i cospi_p62 = _mm256_set1_epi32(cospi[62]);
  __m256i cospi_p02 = _mm256_set1_epi32(cospi[2]);
  __m256i cospi_p30 = _mm256_set1_epi32(cospi[30]);
  __m256i cospi_p34 = _mm256_set1_epi32(cospi[34]);
  __m256i cospi_p46 = _mm256_set1_epi32(cospi[46]);
  __m256i cospi_p18 = _mm256_set1_epi32(cospi[18]);
  __m256i cospi_p14 = _mm256_set1_epi32(cospi[14]);
  __m256i cospi_p50 = _mm256_set1_epi32(cospi[50]);
  __m256i cospi_p54 = _mm256_set1_epi32(cospi[54]);
  __m256i cospi_p10 = _mm256_set1_epi32(cospi[10]);
  __m256i cospi_p22 = _mm256_set1_epi32(cospi[22]);
  __m256i cospi_p42 = _mm256_set1_epi32(cospi[42]);
  __m256i cospi_p38 = _mm256_set1_epi32(cospi[38]);
  __m256i cospi_p26 = _mm256_set1_epi32(cospi[26]);
  __m256i cospi_p06 = _mm256_set1_epi32(cospi[6]);
  __m256i cospi_p58 = _mm256_set1_epi32(cospi[58]);

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
  btf_32_type0_avx2_new(cospi_p02, cospi_p62, x8[31], x8[16], x9[16], x9[31],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p34, cospi_p30, x8[30], x8[17], x9[17], x9[30],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p18, cospi_p46, x8[29], x8[18], x9[18], x9[29],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p50, cospi_p14, x8[28], x8[19], x9[19], x9[28],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p10, cospi_p54, x8[27], x8[20], x9[20], x9[27],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p42, cospi_p22, x8[26], x8[21], x9[21], x9[26],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p26, cospi_p38, x8[25], x8[22], x9[22], x9[25],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p58, cospi_p06, x8[24], x8[23], x9[23], x9[24],
                        *__rounding, cos_bit);
  x9[32] = _mm256_add_epi32(x8[32], x8[33]);
  x9[33] = _mm256_sub_epi32(x8[32], x8[33]);
  x9[34] = _mm256_sub_epi32(x8[35], x8[34]);
  x9[35] = _mm256_add_epi32(x8[35], x8[34]);
  x9[36] = _mm256_add_epi32(x8[36], x8[37]);
  x9[37] = _mm256_sub_epi32(x8[36], x8[37]);
  x9[38] = _mm256_sub_epi32(x8[39], x8[38]);
  x9[39] = _mm256_add_epi32(x8[39], x8[38]);
  x9[40] = _mm256_add_epi32(x8[40], x8[41]);
  x9[41] = _mm256_sub_epi32(x8[40], x8[41]);
  x9[42] = _mm256_sub_epi32(x8[43], x8[42]);
  x9[43] = _mm256_add_epi32(x8[43], x8[42]);
  x9[44] = _mm256_add_epi32(x8[44], x8[45]);
  x9[45] = _mm256_sub_epi32(x8[44], x8[45]);
  x9[46] = _mm256_sub_epi32(x8[47], x8[46]);
  x9[47] = _mm256_add_epi32(x8[47], x8[46]);
  x9[48] = _mm256_add_epi32(x8[48], x8[49]);
  x9[49] = _mm256_sub_epi32(x8[48], x8[49]);
  x9[50] = _mm256_sub_epi32(x8[51], x8[50]);
  x9[51] = _mm256_add_epi32(x8[51], x8[50]);
  x9[52] = _mm256_add_epi32(x8[52], x8[53]);
  x9[53] = _mm256_sub_epi32(x8[52], x8[53]);
  x9[54] = _mm256_sub_epi32(x8[55], x8[54]);
  x9[55] = _mm256_add_epi32(x8[55], x8[54]);
  x9[56] = _mm256_add_epi32(x8[56], x8[57]);
  x9[57] = _mm256_sub_epi32(x8[56], x8[57]);
  x9[58] = _mm256_sub_epi32(x8[59], x8[58]);
  x9[59] = _mm256_add_epi32(x8[59], x8[58]);
  x9[60] = _mm256_add_epi32(x8[60], x8[61]);
  x9[61] = _mm256_sub_epi32(x8[60], x8[61]);
  x9[62] = _mm256_sub_epi32(x8[63], x8[62]);
  x9[63] = _mm256_add_epi32(x8[63], x8[62]);
}
static INLINE void fdct64_stage10_avx2(__m256i *x9, __m256i *x10,
                                       const int32_t *cospi,
                                       const __m256i *__rounding,
                                       int8_t cos_bit) {
  __m256i cospi_p63 = _mm256_set1_epi32(cospi[63]);
  __m256i cospi_p01 = _mm256_set1_epi32(cospi[1]);
  __m256i cospi_p31 = _mm256_set1_epi32(cospi[31]);
  __m256i cospi_p33 = _mm256_set1_epi32(cospi[33]);
  __m256i cospi_p47 = _mm256_set1_epi32(cospi[47]);
  __m256i cospi_p17 = _mm256_set1_epi32(cospi[17]);
  __m256i cospi_p15 = _mm256_set1_epi32(cospi[15]);
  __m256i cospi_p49 = _mm256_set1_epi32(cospi[49]);
  __m256i cospi_p55 = _mm256_set1_epi32(cospi[55]);
  __m256i cospi_p09 = _mm256_set1_epi32(cospi[9]);
  __m256i cospi_p23 = _mm256_set1_epi32(cospi[23]);
  __m256i cospi_p41 = _mm256_set1_epi32(cospi[41]);
  __m256i cospi_p39 = _mm256_set1_epi32(cospi[39]);
  __m256i cospi_p25 = _mm256_set1_epi32(cospi[25]);
  __m256i cospi_p07 = _mm256_set1_epi32(cospi[7]);
  __m256i cospi_p57 = _mm256_set1_epi32(cospi[57]);
  __m256i cospi_p59 = _mm256_set1_epi32(cospi[59]);
  __m256i cospi_p05 = _mm256_set1_epi32(cospi[5]);
  __m256i cospi_p27 = _mm256_set1_epi32(cospi[27]);
  __m256i cospi_p37 = _mm256_set1_epi32(cospi[37]);
  __m256i cospi_p43 = _mm256_set1_epi32(cospi[43]);
  __m256i cospi_p21 = _mm256_set1_epi32(cospi[21]);
  __m256i cospi_p11 = _mm256_set1_epi32(cospi[11]);
  __m256i cospi_p53 = _mm256_set1_epi32(cospi[53]);
  __m256i cospi_p51 = _mm256_set1_epi32(cospi[51]);
  __m256i cospi_p13 = _mm256_set1_epi32(cospi[13]);
  __m256i cospi_p19 = _mm256_set1_epi32(cospi[19]);
  __m256i cospi_p45 = _mm256_set1_epi32(cospi[45]);
  __m256i cospi_p35 = _mm256_set1_epi32(cospi[35]);
  __m256i cospi_p29 = _mm256_set1_epi32(cospi[29]);
  __m256i cospi_p03 = _mm256_set1_epi32(cospi[3]);
  __m256i cospi_p61 = _mm256_set1_epi32(cospi[61]);

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
  btf_32_type0_avx2_new(cospi_p01, cospi_p63, x9[63], x9[32], x10[32], x10[63],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p33, cospi_p31, x9[62], x9[33], x10[33], x10[62],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p17, cospi_p47, x9[61], x9[34], x10[34], x10[61],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p49, cospi_p15, x9[60], x9[35], x10[35], x10[60],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p09, cospi_p55, x9[59], x9[36], x10[36], x10[59],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p41, cospi_p23, x9[58], x9[37], x10[37], x10[58],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p25, cospi_p39, x9[57], x9[38], x10[38], x10[57],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p57, cospi_p07, x9[56], x9[39], x10[39], x10[56],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p05, cospi_p59, x9[55], x9[40], x10[40], x10[55],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p37, cospi_p27, x9[54], x9[41], x10[41], x10[54],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p21, cospi_p43, x9[53], x9[42], x10[42], x10[53],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p53, cospi_p11, x9[52], x9[43], x10[43], x10[52],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p13, cospi_p51, x9[51], x9[44], x10[44], x10[51],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p45, cospi_p19, x9[50], x9[45], x10[45], x10[50],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p29, cospi_p35, x9[49], x9[46], x10[46], x10[49],
                        *__rounding, cos_bit);
  btf_32_type0_avx2_new(cospi_p61, cospi_p03, x9[48], x9[47], x10[47], x10[48],
                        *__rounding, cos_bit);
}
static void fdct64_avx2(__m256i *input, __m256i *output, int8_t cos_bit,
                        const int instride, const int outstride) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m256i __rounding = _mm256_set1_epi32(1 << (cos_bit - 1));
  __m256i cospi_m32 = _mm256_set1_epi32(-cospi[32]);
  __m256i cospi_p32 = _mm256_set1_epi32(cospi[32]);
  __m256i cospi_m16 = _mm256_set1_epi32(-cospi[16]);
  __m256i cospi_p48 = _mm256_set1_epi32(cospi[48]);
  __m256i cospi_m48 = _mm256_set1_epi32(-cospi[48]);
  __m256i cospi_p16 = _mm256_set1_epi32(cospi[16]);
  __m256i cospi_m08 = _mm256_set1_epi32(-cospi[8]);
  __m256i cospi_p56 = _mm256_set1_epi32(cospi[56]);
  __m256i cospi_m56 = _mm256_set1_epi32(-cospi[56]);
  __m256i cospi_m40 = _mm256_set1_epi32(-cospi[40]);
  __m256i cospi_p24 = _mm256_set1_epi32(cospi[24]);
  __m256i cospi_m24 = _mm256_set1_epi32(-cospi[24]);
  __m256i cospi_p08 = _mm256_set1_epi32(cospi[8]);
  __m256i cospi_p40 = _mm256_set1_epi32(cospi[40]);

  int startidx = 0 * instride;
  int endidx = 63 * instride;
  // stage 1
  __m256i x1[64];
  x1[0] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[63] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[1] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[62] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[2] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[61] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[3] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[60] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[4] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[59] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[5] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[58] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[6] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[57] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[7] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[56] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[8] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[55] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[9] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[54] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[10] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[53] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[11] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[52] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[12] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[51] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[13] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[50] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[14] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[49] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[15] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[48] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[16] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[47] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[17] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[46] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[18] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[45] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[19] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[44] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[20] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[43] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[21] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[42] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[22] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[41] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[23] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[40] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[24] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[39] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[25] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[38] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[26] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[37] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[27] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[36] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[28] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[35] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[29] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[34] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[30] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[33] = _mm256_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[31] = _mm256_add_epi32(input[startidx], input[endidx]);
  x1[32] = _mm256_sub_epi32(input[startidx], input[endidx]);

  // stage 2
  __m256i x2[64];
  fdct64_stage2_avx2(x1, x2, &cospi_m32, &cospi_p32, &__rounding, cos_bit);
  // stage 3
  fdct64_stage3_avx2(x2, x1, &cospi_m32, &cospi_p32, &__rounding, cos_bit);
  // stage 4
  fdct64_stage4_avx2(x1, x2, &cospi_m32, &cospi_p32, &cospi_m16, &cospi_p48,
                     &cospi_m48, &__rounding, cos_bit);
  // stage 5
  fdct64_stage5_avx2(x2, x1, &cospi_m32, &cospi_p32, &cospi_m16, &cospi_p48,
                     &cospi_m48, &__rounding, cos_bit);
  // stage 6
  fdct64_stage6_avx2(x1, x2, &cospi_p16, &cospi_p32, &cospi_m16, &cospi_p48,
                     &cospi_m48, &cospi_m08, &cospi_p56, &cospi_m56, &cospi_m40,
                     &cospi_p24, &cospi_m24, &__rounding, cos_bit);
  // stage 7
  fdct64_stage7_avx2(x2, x1, &cospi_p08, &cospi_p56, &cospi_p40, &cospi_p24,
                     &cospi_m08, &cospi_m56, &cospi_m40, &cospi_m24,
                     &__rounding, cos_bit);
  // stage 8
  fdct64_stage8_avx2(x1, x2, cospi, &__rounding, cos_bit);
  // stage 9
  fdct64_stage9_avx2(x2, x1, cospi, &__rounding, cos_bit);
  // stage 10
  fdct64_stage10_avx2(x1, x2, cospi, &__rounding, cos_bit);

  startidx = 0 * outstride;
  endidx = 63 * outstride;

  // stage 11
  output[startidx] = x2[0];
  output[endidx] = x2[63];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[32];
  output[endidx] = x2[31];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[16];
  output[endidx] = x2[47];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[48];
  output[endidx] = x2[15];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[8];
  output[endidx] = x2[55];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[40];
  output[endidx] = x2[23];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[24];
  output[endidx] = x2[39];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[56];
  output[endidx] = x2[7];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[4];
  output[endidx] = x2[59];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[36];
  output[endidx] = x2[27];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[20];
  output[endidx] = x2[43];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[52];
  output[endidx] = x2[11];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[12];
  output[endidx] = x2[51];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[44];
  output[endidx] = x2[19];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[28];
  output[endidx] = x2[35];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[60];
  output[endidx] = x2[3];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[2];
  output[endidx] = x2[61];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[34];
  output[endidx] = x2[29];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[18];
  output[endidx] = x2[45];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[50];
  output[endidx] = x2[13];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[10];
  output[endidx] = x2[53];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[42];
  output[endidx] = x2[21];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[26];
  output[endidx] = x2[37];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[58];
  output[endidx] = x2[5];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[6];
  output[endidx] = x2[57];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[38];
  output[endidx] = x2[25];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[22];
  output[endidx] = x2[41];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[54];
  output[endidx] = x2[9];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[14];
  output[endidx] = x2[49];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[46];
  output[endidx] = x2[17];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[30];
  output[endidx] = x2[33];
  startidx += outstride;
  endidx -= outstride;
  output[startidx] = x2[62];
  output[endidx] = x2[1];
}
void av1_fwd_txfm2d_64x64_avx2(const int16_t *input, int32_t *output,
                               int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_64X64;
  __m256i buf0[512], buf1[512];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_avx2 col_txfm = fdct64_avx2;
  const transform_1d_avx2 row_txfm = fdct64_avx2;
  const int width_div16 = (width >> 4);
  const int width_div8 = (width >> 3);
  int r, c;
  for (int i = 0; i < width_div16; i++) {
    load_buffer_16xn_avx2(input + (i << 4), &buf0[i << 1], stride, height,
                          width_div8, 0, 0);
    round_shift_32_8xn_avx2(&buf0[i << 1], height, shift[0], width_div8);
    round_shift_32_8xn_avx2(&buf0[(i << 1) + 1], height, shift[0], width_div8);
    col_txfm(&buf0[i << 1], &buf0[i << 1], cos_bit_col, width_div8, width_div8);
    col_txfm(&buf0[(i << 1) + 1], &buf0[(i << 1) + 1], cos_bit_col, width_div8,
             width_div8);
    round_shift_32_8xn_avx2(&buf0[i << 1], height, shift[1], width_div8);
    round_shift_32_8xn_avx2(&buf0[(i << 1) + 1], height, shift[1], width_div8);
  }

  for (r = 0; r < height; r += 8) {
    for (c = 0; c < width_div8; c++) {
      fwd_txfm_transpose_8x8_avx2(&buf0[r * width_div8 + c],
                                  &buf1[c * 8 * width_div8 + (r >> 3)],
                                  width_div8, width_div8);
    }
  }

  for (int i = 0; i < 2; i++) {
    row_txfm(&buf1[i << 1], &buf0[i << 1], cos_bit_row, width_div8,
             width_div16);
    row_txfm(&buf1[(i << 1) + 1], &buf0[(i << 1) + 1], cos_bit_row, width_div8,
             width_div16);
    round_shift_32_8xn_avx2(&buf0[i << 1], (height >> 1), shift[2],
                            width_div16);
    round_shift_32_8xn_avx2(&buf0[(i << 1) + 1], (height >> 1), shift[2],
                            width_div16);
  }

  for (r = 0; r < (height >> 1); r += 8) {
    for (c = 0; c < width_div16; c++) {
      fwd_txfm_transpose_8x8_avx2(&buf0[r * width_div16 + c],
                                  &buf1[c * 8 * width_div16 + (r >> 3)],
                                  width_div16, width_div16);
    }
  }
  store_buffer_avx2(buf1, output, 8, 128);
}
