/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <assert.h>
#include <immintrin.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/idct.h"
#include "av1/common/x86/av1_inv_txfm_ssse3.h"
#include "av1/common/x86/highbd_txfm_utility_sse4.h"
#include "aom_dsp/x86/txfm_common_avx2.h"

// Note:
//  Total 32x4 registers to represent 32x32 block coefficients.
//  For high bit depth, each coefficient is 4-byte.
//  Each __m256i register holds 8 coefficients.
//  So each "row" we needs 4 register. Totally 32 rows
//  Register layout:
//   v0,   v1,   v2,   v3,
//   v4,   v5,   v6,   v7,
//   ... ...
//   v124, v125, v126, v127

static inline __m256i highbd_clamp_epi16_avx2(__m256i u, int bd) {
  const __m256i zero = _mm256_setzero_si256();
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i max = _mm256_sub_epi16(_mm256_slli_epi16(one, bd), one);
  __m256i clamped, mask;

  mask = _mm256_cmpgt_epi16(u, max);
  clamped = _mm256_andnot_si256(mask, u);
  mask = _mm256_and_si256(mask, max);
  clamped = _mm256_or_si256(mask, clamped);
  mask = _mm256_cmpgt_epi16(clamped, zero);
  clamped = _mm256_and_si256(clamped, mask);

  return clamped;
}

static inline void round_shift_4x4_avx2(__m256i *in, int shift) {
  if (shift != 0) {
    __m256i rnding = _mm256_set1_epi32(1 << (shift - 1));
    in[0] = _mm256_add_epi32(in[0], rnding);
    in[1] = _mm256_add_epi32(in[1], rnding);
    in[2] = _mm256_add_epi32(in[2], rnding);
    in[3] = _mm256_add_epi32(in[3], rnding);

    in[0] = _mm256_srai_epi32(in[0], shift);
    in[1] = _mm256_srai_epi32(in[1], shift);
    in[2] = _mm256_srai_epi32(in[2], shift);
    in[3] = _mm256_srai_epi32(in[3], shift);
  }
}

static inline void round_shift_8x8_avx2(__m256i *in, int shift) {
  round_shift_4x4_avx2(in, shift);
  round_shift_4x4_avx2(in + 4, shift);
  round_shift_4x4_avx2(in + 8, shift);
  round_shift_4x4_avx2(in + 12, shift);
}

static void highbd_clamp_epi32_avx2(__m256i *in, __m256i *out,
                                    const __m256i *clamp_lo,
                                    const __m256i *clamp_hi, int size) {
  __m256i a0, a1;
  for (int i = 0; i < size; i += 4) {
    a0 = _mm256_max_epi32(in[i], *clamp_lo);
    out[i] = _mm256_min_epi32(a0, *clamp_hi);

    a1 = _mm256_max_epi32(in[i + 1], *clamp_lo);
    out[i + 1] = _mm256_min_epi32(a1, *clamp_hi);

    a0 = _mm256_max_epi32(in[i + 2], *clamp_lo);
    out[i + 2] = _mm256_min_epi32(a0, *clamp_hi);

    a1 = _mm256_max_epi32(in[i + 3], *clamp_lo);
    out[i + 3] = _mm256_min_epi32(a1, *clamp_hi);
  }
}

static inline __m256i highbd_get_recon_16x8_avx2(const __m256i pred,
                                                 __m256i res0, __m256i res1,
                                                 const int bd) {
  __m256i x0 = _mm256_cvtepi16_epi32(_mm256_castsi256_si128(pred));
  __m256i x1 = _mm256_cvtepi16_epi32(_mm256_extractf128_si256(pred, 1));

  x0 = _mm256_add_epi32(res0, x0);
  x1 = _mm256_add_epi32(res1, x1);
  x0 = _mm256_packus_epi32(x0, x1);
  x0 = _mm256_permute4x64_epi64(x0, 0xd8);
  x0 = highbd_clamp_epi16_avx2(x0, bd);
  return x0;
}

static inline void highbd_write_buffer_16xn_avx2(__m256i *in, uint16_t *output,
                                                 int stride, int flipud,
                                                 int height, const int bd) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    __m256i v = _mm256_loadu_si256((__m256i const *)(output + i * stride));
    __m256i u = highbd_get_recon_16x8_avx2(v, in[j], in[j + height], bd);

    _mm256_storeu_si256((__m256i *)(output + i * stride), u);
  }
}
static inline __m256i highbd_get_recon_8x8_avx2(const __m256i pred, __m256i res,
                                                const int bd) {
  __m256i x0 = pred;
  x0 = _mm256_add_epi32(res, x0);
  x0 = _mm256_packus_epi32(x0, x0);
  x0 = _mm256_permute4x64_epi64(x0, 0xd8);
  x0 = highbd_clamp_epi16_avx2(x0, bd);
  return x0;
}

static inline void highbd_write_buffer_8xn_avx2(__m256i *in, uint16_t *output,
                                                int stride, int flipud,
                                                int height, const int bd) {
  int j = flipud ? (height - 1) : 0;
  __m128i temp;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    temp = _mm_loadu_si128((__m128i const *)(output + i * stride));
    __m256i v = _mm256_cvtepi16_epi32(temp);
    __m256i u = highbd_get_recon_8x8_avx2(v, in[j], bd);
    __m128i u1 = _mm256_castsi256_si128(u);
    _mm_storeu_si128((__m128i *)(output + i * stride), u1);
  }
}
static void neg_shift_avx2(const __m256i in0, const __m256i in1, __m256i *out0,
                           __m256i *out1, const __m256i *clamp_lo,
                           const __m256i *clamp_hi, int shift) {
  __m256i offset = _mm256_set1_epi32((1 << shift) >> 1);
  __m256i a0 = _mm256_add_epi32(offset, in0);
  __m256i a1 = _mm256_sub_epi32(offset, in1);

  a0 = _mm256_sra_epi32(a0, _mm_cvtsi32_si128(shift));
  a1 = _mm256_sra_epi32(a1, _mm_cvtsi32_si128(shift));

  a0 = _mm256_max_epi32(a0, *clamp_lo);
  a0 = _mm256_min_epi32(a0, *clamp_hi);
  a1 = _mm256_max_epi32(a1, *clamp_lo);
  a1 = _mm256_min_epi32(a1, *clamp_hi);

  *out0 = a0;
  *out1 = a1;
}

static void transpose_8x8_avx2(const __m256i *in, __m256i *out) {
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i x0, x1;

  u0 = _mm256_unpacklo_epi32(in[0], in[1]);
  u1 = _mm256_unpackhi_epi32(in[0], in[1]);

  u2 = _mm256_unpacklo_epi32(in[2], in[3]);
  u3 = _mm256_unpackhi_epi32(in[2], in[3]);

  u4 = _mm256_unpacklo_epi32(in[4], in[5]);
  u5 = _mm256_unpackhi_epi32(in[4], in[5]);

  u6 = _mm256_unpacklo_epi32(in[6], in[7]);
  u7 = _mm256_unpackhi_epi32(in[6], in[7]);

  x0 = _mm256_unpacklo_epi64(u0, u2);
  x1 = _mm256_unpacklo_epi64(u4, u6);
  out[0] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[4] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u0, u2);
  x1 = _mm256_unpackhi_epi64(u4, u6);
  out[1] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[5] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpacklo_epi64(u1, u3);
  x1 = _mm256_unpacklo_epi64(u5, u7);
  out[2] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[6] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u1, u3);
  x1 = _mm256_unpackhi_epi64(u5, u7);
  out[3] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[7] = _mm256_permute2f128_si256(x0, x1, 0x31);
}

static void transpose_8x8_flip_avx2(const __m256i *in, __m256i *out) {
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i x0, x1;

  u0 = _mm256_unpacklo_epi32(in[7], in[6]);
  u1 = _mm256_unpackhi_epi32(in[7], in[6]);

  u2 = _mm256_unpacklo_epi32(in[5], in[4]);
  u3 = _mm256_unpackhi_epi32(in[5], in[4]);

  u4 = _mm256_unpacklo_epi32(in[3], in[2]);
  u5 = _mm256_unpackhi_epi32(in[3], in[2]);

  u6 = _mm256_unpacklo_epi32(in[1], in[0]);
  u7 = _mm256_unpackhi_epi32(in[1], in[0]);

  x0 = _mm256_unpacklo_epi64(u0, u2);
  x1 = _mm256_unpacklo_epi64(u4, u6);
  out[0] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[4] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u0, u2);
  x1 = _mm256_unpackhi_epi64(u4, u6);
  out[1] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[5] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpacklo_epi64(u1, u3);
  x1 = _mm256_unpacklo_epi64(u5, u7);
  out[2] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[6] = _mm256_permute2f128_si256(x0, x1, 0x31);

  x0 = _mm256_unpackhi_epi64(u1, u3);
  x1 = _mm256_unpackhi_epi64(u5, u7);
  out[3] = _mm256_permute2f128_si256(x0, x1, 0x20);
  out[7] = _mm256_permute2f128_si256(x0, x1, 0x31);
}

static inline void load_buffer_32bit_input(const int32_t *in, int stride,
                                           __m256i *out, int out_size) {
  for (int i = 0; i < out_size; ++i) {
    out[i] = _mm256_loadu_si256((const __m256i *)(in + i * stride));
  }
}

static inline __m256i half_btf_0_avx2(const __m256i *w0, const __m256i *n0,
                                      const __m256i *rounding, int bit) {
  __m256i x;
  x = _mm256_mullo_epi32(*w0, *n0);
  x = _mm256_add_epi32(x, *rounding);
  x = _mm256_srai_epi32(x, bit);
  return x;
}

static inline __m256i half_btf_avx2(const __m256i *w0, const __m256i *n0,
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

static void addsub_avx2(const __m256i in0, const __m256i in1, __m256i *out0,
                        __m256i *out1, const __m256i *clamp_lo,
                        const __m256i *clamp_hi) {
  __m256i a0 = _mm256_add_epi32(in0, in1);
  __m256i a1 = _mm256_sub_epi32(in0, in1);

  a0 = _mm256_max_epi32(a0, *clamp_lo);
  a0 = _mm256_min_epi32(a0, *clamp_hi);
  a1 = _mm256_max_epi32(a1, *clamp_lo);
  a1 = _mm256_min_epi32(a1, *clamp_hi);

  *out0 = a0;
  *out1 = a1;
}

static inline void idct32_stage4_avx2(
    __m256i *bf1, const __m256i *cospim8, const __m256i *cospi56,
    const __m256i *cospi8, const __m256i *cospim56, const __m256i *cospim40,
    const __m256i *cospi24, const __m256i *cospi40, const __m256i *cospim24,
    const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  temp1 = half_btf_avx2(cospim8, &bf1[17], cospi56, &bf1[30], rounding, bit);
  bf1[30] = half_btf_avx2(cospi56, &bf1[17], cospi8, &bf1[30], rounding, bit);
  bf1[17] = temp1;

  temp2 = half_btf_avx2(cospim56, &bf1[18], cospim8, &bf1[29], rounding, bit);
  bf1[29] = half_btf_avx2(cospim8, &bf1[18], cospi56, &bf1[29], rounding, bit);
  bf1[18] = temp2;

  temp1 = half_btf_avx2(cospim40, &bf1[21], cospi24, &bf1[26], rounding, bit);
  bf1[26] = half_btf_avx2(cospi24, &bf1[21], cospi40, &bf1[26], rounding, bit);
  bf1[21] = temp1;

  temp2 = half_btf_avx2(cospim24, &bf1[22], cospim40, &bf1[25], rounding, bit);
  bf1[25] = half_btf_avx2(cospim40, &bf1[22], cospi24, &bf1[25], rounding, bit);
  bf1[22] = temp2;
}

static inline void idct32_stage5_avx2(
    __m256i *bf1, const __m256i *cospim16, const __m256i *cospi48,
    const __m256i *cospi16, const __m256i *cospim48, const __m256i *clamp_lo,
    const __m256i *clamp_hi, const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  temp1 = half_btf_avx2(cospim16, &bf1[9], cospi48, &bf1[14], rounding, bit);
  bf1[14] = half_btf_avx2(cospi48, &bf1[9], cospi16, &bf1[14], rounding, bit);
  bf1[9] = temp1;

  temp2 = half_btf_avx2(cospim48, &bf1[10], cospim16, &bf1[13], rounding, bit);
  bf1[13] = half_btf_avx2(cospim16, &bf1[10], cospi48, &bf1[13], rounding, bit);
  bf1[10] = temp2;

  addsub_avx2(bf1[16], bf1[19], bf1 + 16, bf1 + 19, clamp_lo, clamp_hi);
  addsub_avx2(bf1[17], bf1[18], bf1 + 17, bf1 + 18, clamp_lo, clamp_hi);
  addsub_avx2(bf1[23], bf1[20], bf1 + 23, bf1 + 20, clamp_lo, clamp_hi);
  addsub_avx2(bf1[22], bf1[21], bf1 + 22, bf1 + 21, clamp_lo, clamp_hi);
  addsub_avx2(bf1[24], bf1[27], bf1 + 24, bf1 + 27, clamp_lo, clamp_hi);
  addsub_avx2(bf1[25], bf1[26], bf1 + 25, bf1 + 26, clamp_lo, clamp_hi);
  addsub_avx2(bf1[31], bf1[28], bf1 + 31, bf1 + 28, clamp_lo, clamp_hi);
  addsub_avx2(bf1[30], bf1[29], bf1 + 30, bf1 + 29, clamp_lo, clamp_hi);
}

static inline void idct32_stage6_avx2(
    __m256i *bf1, const __m256i *cospim32, const __m256i *cospi32,
    const __m256i *cospim16, const __m256i *cospi48, const __m256i *cospi16,
    const __m256i *cospim48, const __m256i *clamp_lo, const __m256i *clamp_hi,
    const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  temp1 = half_btf_avx2(cospim32, &bf1[5], cospi32, &bf1[6], rounding, bit);
  bf1[6] = half_btf_avx2(cospi32, &bf1[5], cospi32, &bf1[6], rounding, bit);
  bf1[5] = temp1;

  addsub_avx2(bf1[8], bf1[11], bf1 + 8, bf1 + 11, clamp_lo, clamp_hi);
  addsub_avx2(bf1[9], bf1[10], bf1 + 9, bf1 + 10, clamp_lo, clamp_hi);
  addsub_avx2(bf1[15], bf1[12], bf1 + 15, bf1 + 12, clamp_lo, clamp_hi);
  addsub_avx2(bf1[14], bf1[13], bf1 + 14, bf1 + 13, clamp_lo, clamp_hi);

  temp1 = half_btf_avx2(cospim16, &bf1[18], cospi48, &bf1[29], rounding, bit);
  bf1[29] = half_btf_avx2(cospi48, &bf1[18], cospi16, &bf1[29], rounding, bit);
  bf1[18] = temp1;
  temp2 = half_btf_avx2(cospim16, &bf1[19], cospi48, &bf1[28], rounding, bit);
  bf1[28] = half_btf_avx2(cospi48, &bf1[19], cospi16, &bf1[28], rounding, bit);
  bf1[19] = temp2;
  temp1 = half_btf_avx2(cospim48, &bf1[20], cospim16, &bf1[27], rounding, bit);
  bf1[27] = half_btf_avx2(cospim16, &bf1[20], cospi48, &bf1[27], rounding, bit);
  bf1[20] = temp1;
  temp2 = half_btf_avx2(cospim48, &bf1[21], cospim16, &bf1[26], rounding, bit);
  bf1[26] = half_btf_avx2(cospim16, &bf1[21], cospi48, &bf1[26], rounding, bit);
  bf1[21] = temp2;
}

static inline void idct32_stage7_avx2(__m256i *bf1, const __m256i *cospim32,
                                      const __m256i *cospi32,
                                      const __m256i *clamp_lo,
                                      const __m256i *clamp_hi,
                                      const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  addsub_avx2(bf1[0], bf1[7], bf1 + 0, bf1 + 7, clamp_lo, clamp_hi);
  addsub_avx2(bf1[1], bf1[6], bf1 + 1, bf1 + 6, clamp_lo, clamp_hi);
  addsub_avx2(bf1[2], bf1[5], bf1 + 2, bf1 + 5, clamp_lo, clamp_hi);
  addsub_avx2(bf1[3], bf1[4], bf1 + 3, bf1 + 4, clamp_lo, clamp_hi);

  temp1 = half_btf_avx2(cospim32, &bf1[10], cospi32, &bf1[13], rounding, bit);
  bf1[13] = half_btf_avx2(cospi32, &bf1[10], cospi32, &bf1[13], rounding, bit);
  bf1[10] = temp1;
  temp2 = half_btf_avx2(cospim32, &bf1[11], cospi32, &bf1[12], rounding, bit);
  bf1[12] = half_btf_avx2(cospi32, &bf1[11], cospi32, &bf1[12], rounding, bit);
  bf1[11] = temp2;

  addsub_avx2(bf1[16], bf1[23], bf1 + 16, bf1 + 23, clamp_lo, clamp_hi);
  addsub_avx2(bf1[17], bf1[22], bf1 + 17, bf1 + 22, clamp_lo, clamp_hi);
  addsub_avx2(bf1[18], bf1[21], bf1 + 18, bf1 + 21, clamp_lo, clamp_hi);
  addsub_avx2(bf1[19], bf1[20], bf1 + 19, bf1 + 20, clamp_lo, clamp_hi);
  addsub_avx2(bf1[31], bf1[24], bf1 + 31, bf1 + 24, clamp_lo, clamp_hi);
  addsub_avx2(bf1[30], bf1[25], bf1 + 30, bf1 + 25, clamp_lo, clamp_hi);
  addsub_avx2(bf1[29], bf1[26], bf1 + 29, bf1 + 26, clamp_lo, clamp_hi);
  addsub_avx2(bf1[28], bf1[27], bf1 + 28, bf1 + 27, clamp_lo, clamp_hi);
}

static inline void idct32_stage8_avx2(__m256i *bf1, const __m256i *cospim32,
                                      const __m256i *cospi32,
                                      const __m256i *clamp_lo,
                                      const __m256i *clamp_hi,
                                      const __m256i *rounding, int bit) {
  __m256i temp1, temp2;
  addsub_avx2(bf1[0], bf1[15], bf1 + 0, bf1 + 15, clamp_lo, clamp_hi);
  addsub_avx2(bf1[1], bf1[14], bf1 + 1, bf1 + 14, clamp_lo, clamp_hi);
  addsub_avx2(bf1[2], bf1[13], bf1 + 2, bf1 + 13, clamp_lo, clamp_hi);
  addsub_avx2(bf1[3], bf1[12], bf1 + 3, bf1 + 12, clamp_lo, clamp_hi);
  addsub_avx2(bf1[4], bf1[11], bf1 + 4, bf1 + 11, clamp_lo, clamp_hi);
  addsub_avx2(bf1[5], bf1[10], bf1 + 5, bf1 + 10, clamp_lo, clamp_hi);
  addsub_avx2(bf1[6], bf1[9], bf1 + 6, bf1 + 9, clamp_lo, clamp_hi);
  addsub_avx2(bf1[7], bf1[8], bf1 + 7, bf1 + 8, clamp_lo, clamp_hi);

  temp1 = half_btf_avx2(cospim32, &bf1[20], cospi32, &bf1[27], rounding, bit);
  bf1[27] = half_btf_avx2(cospi32, &bf1[20], cospi32, &bf1[27], rounding, bit);
  bf1[20] = temp1;
  temp2 = half_btf_avx2(cospim32, &bf1[21], cospi32, &bf1[26], rounding, bit);
  bf1[26] = half_btf_avx2(cospi32, &bf1[21], cospi32, &bf1[26], rounding, bit);
  bf1[21] = temp2;
  temp1 = half_btf_avx2(cospim32, &bf1[22], cospi32, &bf1[25], rounding, bit);
  bf1[25] = half_btf_avx2(cospi32, &bf1[22], cospi32, &bf1[25], rounding, bit);
  bf1[22] = temp1;
  temp2 = half_btf_avx2(cospim32, &bf1[23], cospi32, &bf1[24], rounding, bit);
  bf1[24] = half_btf_avx2(cospi32, &bf1[23], cospi32, &bf1[24], rounding, bit);
  bf1[23] = temp2;
}

static inline void idct32_stage9_avx2(__m256i *bf1, __m256i *out,
                                      const int do_cols, const int bd,
                                      const int out_shift,
                                      const __m256i *clamp_lo,
                                      const __m256i *clamp_hi) {
  addsub_avx2(bf1[0], bf1[31], out + 0, out + 31, clamp_lo, clamp_hi);
  addsub_avx2(bf1[1], bf1[30], out + 1, out + 30, clamp_lo, clamp_hi);
  addsub_avx2(bf1[2], bf1[29], out + 2, out + 29, clamp_lo, clamp_hi);
  addsub_avx2(bf1[3], bf1[28], out + 3, out + 28, clamp_lo, clamp_hi);
  addsub_avx2(bf1[4], bf1[27], out + 4, out + 27, clamp_lo, clamp_hi);
  addsub_avx2(bf1[5], bf1[26], out + 5, out + 26, clamp_lo, clamp_hi);
  addsub_avx2(bf1[6], bf1[25], out + 6, out + 25, clamp_lo, clamp_hi);
  addsub_avx2(bf1[7], bf1[24], out + 7, out + 24, clamp_lo, clamp_hi);
  addsub_avx2(bf1[8], bf1[23], out + 8, out + 23, clamp_lo, clamp_hi);
  addsub_avx2(bf1[9], bf1[22], out + 9, out + 22, clamp_lo, clamp_hi);
  addsub_avx2(bf1[10], bf1[21], out + 10, out + 21, clamp_lo, clamp_hi);
  addsub_avx2(bf1[11], bf1[20], out + 11, out + 20, clamp_lo, clamp_hi);
  addsub_avx2(bf1[12], bf1[19], out + 12, out + 19, clamp_lo, clamp_hi);
  addsub_avx2(bf1[13], bf1[18], out + 13, out + 18, clamp_lo, clamp_hi);
  addsub_avx2(bf1[14], bf1[17], out + 14, out + 17, clamp_lo, clamp_hi);
  addsub_avx2(bf1[15], bf1[16], out + 15, out + 16, clamp_lo, clamp_hi);
  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const __m256i clamp_lo_out = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
    const __m256i clamp_hi_out =
        _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);
    round_shift_8x8_avx2(out, out_shift);
    round_shift_8x8_avx2(out + 16, out_shift);
    highbd_clamp_epi32_avx2(out, out, &clamp_lo_out, &clamp_hi_out, 32);
  }
}

static void idct32_low1_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                             int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rounding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i x;
  // stage 0
  // stage 1
  // stage 2
  // stage 3
  // stage 4
  // stage 5
  x = _mm256_mullo_epi32(in[0], cospi32);
  x = _mm256_add_epi32(x, rounding);
  x = _mm256_srai_epi32(x, bit);

  // stage 6
  // stage 7
  // stage 8
  // stage 9
  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    __m256i offset = _mm256_set1_epi32((1 << out_shift) >> 1);
    clamp_lo = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
    clamp_hi = _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);
    x = _mm256_add_epi32(offset, x);
    x = _mm256_sra_epi32(x, _mm_cvtsi32_si128(out_shift));
  }
  x = _mm256_max_epi32(x, clamp_lo);
  x = _mm256_min_epi32(x, clamp_hi);
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
}

static void idct32_low8_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                             int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i rounding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i bf1[32];

  {
    // stage 0
    // stage 1
    bf1[0] = in[0];
    bf1[4] = in[4];
    bf1[8] = in[2];
    bf1[12] = in[6];
    bf1[16] = in[1];
    bf1[20] = in[5];
    bf1[24] = in[3];
    bf1[28] = in[7];

    // stage 2
    bf1[31] = half_btf_0_avx2(&cospi2, &bf1[16], &rounding, bit);
    bf1[16] = half_btf_0_avx2(&cospi62, &bf1[16], &rounding, bit);
    bf1[19] = half_btf_0_avx2(&cospim50, &bf1[28], &rounding, bit);
    bf1[28] = half_btf_0_avx2(&cospi14, &bf1[28], &rounding, bit);
    bf1[27] = half_btf_0_avx2(&cospi10, &bf1[20], &rounding, bit);
    bf1[20] = half_btf_0_avx2(&cospi54, &bf1[20], &rounding, bit);
    bf1[23] = half_btf_0_avx2(&cospim58, &bf1[24], &rounding, bit);
    bf1[24] = half_btf_0_avx2(&cospi6, &bf1[24], &rounding, bit);

    // stage 3
    bf1[15] = half_btf_0_avx2(&cospi4, &bf1[8], &rounding, bit);
    bf1[8] = half_btf_0_avx2(&cospi60, &bf1[8], &rounding, bit);

    bf1[11] = half_btf_0_avx2(&cospim52, &bf1[12], &rounding, bit);
    bf1[12] = half_btf_0_avx2(&cospi12, &bf1[12], &rounding, bit);
    bf1[17] = bf1[16];
    bf1[18] = bf1[19];
    bf1[21] = bf1[20];
    bf1[22] = bf1[23];
    bf1[25] = bf1[24];
    bf1[26] = bf1[27];
    bf1[29] = bf1[28];
    bf1[30] = bf1[31];

    // stage 4
    bf1[7] = half_btf_0_avx2(&cospi8, &bf1[4], &rounding, bit);
    bf1[4] = half_btf_0_avx2(&cospi56, &bf1[4], &rounding, bit);

    bf1[9] = bf1[8];
    bf1[10] = bf1[11];
    bf1[13] = bf1[12];
    bf1[14] = bf1[15];

    idct32_stage4_avx2(bf1, &cospim8, &cospi56, &cospi8, &cospim56, &cospim40,
                       &cospi24, &cospi40, &cospim24, &rounding, bit);

    // stage 5
    bf1[0] = half_btf_0_avx2(&cospi32, &bf1[0], &rounding, bit);
    bf1[1] = bf1[0];
    bf1[5] = bf1[4];
    bf1[6] = bf1[7];

    idct32_stage5_avx2(bf1, &cospim16, &cospi48, &cospi16, &cospim48, &clamp_lo,
                       &clamp_hi, &rounding, bit);

    // stage 6
    bf1[3] = bf1[0];
    bf1[2] = bf1[1];

    idct32_stage6_avx2(bf1, &cospim32, &cospi32, &cospim16, &cospi48, &cospi16,
                       &cospim48, &clamp_lo, &clamp_hi, &rounding, bit);

    // stage 7
    idct32_stage7_avx2(bf1, &cospim32, &cospi32, &clamp_lo, &clamp_hi,
                       &rounding, bit);

    // stage 8
    idct32_stage8_avx2(bf1, &cospim32, &cospi32, &clamp_lo, &clamp_hi,
                       &rounding, bit);

    // stage 9
    idct32_stage9_avx2(bf1, out, do_cols, bd, out_shift, &clamp_lo, &clamp_hi);
  }
}

static void idct32_low16_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                              int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
  const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
  const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospim42 = _mm256_set1_epi32(-cospi[42]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospim34 = _mm256_set1_epi32(-cospi[34]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i rounding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i bf1[32];

  {
    // stage 0
    // stage 1
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
    bf1[31] = half_btf_0_avx2(&cospi2, &bf1[16], &rounding, bit);
    bf1[16] = half_btf_0_avx2(&cospi62, &bf1[16], &rounding, bit);
    bf1[17] = half_btf_0_avx2(&cospim34, &bf1[30], &rounding, bit);
    bf1[30] = half_btf_0_avx2(&cospi30, &bf1[30], &rounding, bit);
    bf1[29] = half_btf_0_avx2(&cospi18, &bf1[18], &rounding, bit);
    bf1[18] = half_btf_0_avx2(&cospi46, &bf1[18], &rounding, bit);
    bf1[19] = half_btf_0_avx2(&cospim50, &bf1[28], &rounding, bit);
    bf1[28] = half_btf_0_avx2(&cospi14, &bf1[28], &rounding, bit);
    bf1[27] = half_btf_0_avx2(&cospi10, &bf1[20], &rounding, bit);
    bf1[20] = half_btf_0_avx2(&cospi54, &bf1[20], &rounding, bit);
    bf1[21] = half_btf_0_avx2(&cospim42, &bf1[26], &rounding, bit);
    bf1[26] = half_btf_0_avx2(&cospi22, &bf1[26], &rounding, bit);
    bf1[25] = half_btf_0_avx2(&cospi26, &bf1[22], &rounding, bit);
    bf1[22] = half_btf_0_avx2(&cospi38, &bf1[22], &rounding, bit);
    bf1[23] = half_btf_0_avx2(&cospim58, &bf1[24], &rounding, bit);
    bf1[24] = half_btf_0_avx2(&cospi6, &bf1[24], &rounding, bit);

    // stage 3
    bf1[15] = half_btf_0_avx2(&cospi4, &bf1[8], &rounding, bit);
    bf1[8] = half_btf_0_avx2(&cospi60, &bf1[8], &rounding, bit);
    bf1[9] = half_btf_0_avx2(&cospim36, &bf1[14], &rounding, bit);
    bf1[14] = half_btf_0_avx2(&cospi28, &bf1[14], &rounding, bit);
    bf1[13] = half_btf_0_avx2(&cospi20, &bf1[10], &rounding, bit);
    bf1[10] = half_btf_0_avx2(&cospi44, &bf1[10], &rounding, bit);
    bf1[11] = half_btf_0_avx2(&cospim52, &bf1[12], &rounding, bit);
    bf1[12] = half_btf_0_avx2(&cospi12, &bf1[12], &rounding, bit);

    addsub_avx2(bf1[16], bf1[17], bf1 + 16, bf1 + 17, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[19], bf1[18], bf1 + 19, bf1 + 18, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[20], bf1[21], bf1 + 20, bf1 + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[23], bf1[22], bf1 + 23, bf1 + 22, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[24], bf1[25], bf1 + 24, bf1 + 25, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[27], bf1[26], bf1 + 27, bf1 + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[28], bf1[29], bf1 + 28, bf1 + 29, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[31], bf1[30], bf1 + 31, bf1 + 30, &clamp_lo, &clamp_hi);

    // stage 4
    bf1[7] = half_btf_0_avx2(&cospi8, &bf1[4], &rounding, bit);
    bf1[4] = half_btf_0_avx2(&cospi56, &bf1[4], &rounding, bit);
    bf1[5] = half_btf_0_avx2(&cospim40, &bf1[6], &rounding, bit);
    bf1[6] = half_btf_0_avx2(&cospi24, &bf1[6], &rounding, bit);

    addsub_avx2(bf1[8], bf1[9], bf1 + 8, bf1 + 9, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[11], bf1[10], bf1 + 11, bf1 + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[12], bf1[13], bf1 + 12, bf1 + 13, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[15], bf1[14], bf1 + 15, bf1 + 14, &clamp_lo, &clamp_hi);

    idct32_stage4_avx2(bf1, &cospim8, &cospi56, &cospi8, &cospim56, &cospim40,
                       &cospi24, &cospi40, &cospim24, &rounding, bit);

    // stage 5
    bf1[0] = half_btf_0_avx2(&cospi32, &bf1[0], &rounding, bit);
    bf1[1] = bf1[0];
    bf1[3] = half_btf_0_avx2(&cospi16, &bf1[2], &rounding, bit);
    bf1[2] = half_btf_0_avx2(&cospi48, &bf1[2], &rounding, bit);

    addsub_avx2(bf1[4], bf1[5], bf1 + 4, bf1 + 5, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[7], bf1[6], bf1 + 7, bf1 + 6, &clamp_lo, &clamp_hi);

    idct32_stage5_avx2(bf1, &cospim16, &cospi48, &cospi16, &cospim48, &clamp_lo,
                       &clamp_hi, &rounding, bit);

    // stage 6
    addsub_avx2(bf1[0], bf1[3], bf1 + 0, bf1 + 3, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[1], bf1[2], bf1 + 1, bf1 + 2, &clamp_lo, &clamp_hi);

    idct32_stage6_avx2(bf1, &cospim32, &cospi32, &cospim16, &cospi48, &cospi16,
                       &cospim48, &clamp_lo, &clamp_hi, &rounding, bit);

    // stage 7
    idct32_stage7_avx2(bf1, &cospim32, &cospi32, &clamp_lo, &clamp_hi,
                       &rounding, bit);

    // stage 8
    idct32_stage8_avx2(bf1, &cospim32, &cospi32, &clamp_lo, &clamp_hi,
                       &rounding, bit);

    // stage 9
    idct32_stage9_avx2(bf1, out, do_cols, bd, out_shift, &clamp_lo, &clamp_hi);
  }
}

static void idct32_avx2(__m256i *in, __m256i *out, int bit, int do_cols, int bd,
                        int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
  const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
  const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi58 = _mm256_set1_epi32(cospi[58]);
  const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
  const __m256i cospi42 = _mm256_set1_epi32(cospi[42]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi50 = _mm256_set1_epi32(cospi[50]);
  const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
  const __m256i cospi34 = _mm256_set1_epi32(cospi[34]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospim26 = _mm256_set1_epi32(-cospi[26]);
  const __m256i cospim42 = _mm256_set1_epi32(-cospi[42]);
  const __m256i cospim10 = _mm256_set1_epi32(-cospi[10]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospim18 = _mm256_set1_epi32(-cospi[18]);
  const __m256i cospim34 = _mm256_set1_epi32(-cospi[34]);
  const __m256i cospim2 = _mm256_set1_epi32(-cospi[2]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospim20 = _mm256_set1_epi32(-cospi[20]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospim4 = _mm256_set1_epi32(-cospi[4]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i rounding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i bf1[32], bf0[32];

  {
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
    bf0[0] = bf1[0];
    bf0[1] = bf1[1];
    bf0[2] = bf1[2];
    bf0[3] = bf1[3];
    bf0[4] = bf1[4];
    bf0[5] = bf1[5];
    bf0[6] = bf1[6];
    bf0[7] = bf1[7];
    bf0[8] = bf1[8];
    bf0[9] = bf1[9];
    bf0[10] = bf1[10];
    bf0[11] = bf1[11];
    bf0[12] = bf1[12];
    bf0[13] = bf1[13];
    bf0[14] = bf1[14];
    bf0[15] = bf1[15];
    bf0[16] =
        half_btf_avx2(&cospi62, &bf1[16], &cospim2, &bf1[31], &rounding, bit);
    bf0[17] =
        half_btf_avx2(&cospi30, &bf1[17], &cospim34, &bf1[30], &rounding, bit);
    bf0[18] =
        half_btf_avx2(&cospi46, &bf1[18], &cospim18, &bf1[29], &rounding, bit);
    bf0[19] =
        half_btf_avx2(&cospi14, &bf1[19], &cospim50, &bf1[28], &rounding, bit);
    bf0[20] =
        half_btf_avx2(&cospi54, &bf1[20], &cospim10, &bf1[27], &rounding, bit);
    bf0[21] =
        half_btf_avx2(&cospi22, &bf1[21], &cospim42, &bf1[26], &rounding, bit);
    bf0[22] =
        half_btf_avx2(&cospi38, &bf1[22], &cospim26, &bf1[25], &rounding, bit);
    bf0[23] =
        half_btf_avx2(&cospi6, &bf1[23], &cospim58, &bf1[24], &rounding, bit);
    bf0[24] =
        half_btf_avx2(&cospi58, &bf1[23], &cospi6, &bf1[24], &rounding, bit);
    bf0[25] =
        half_btf_avx2(&cospi26, &bf1[22], &cospi38, &bf1[25], &rounding, bit);
    bf0[26] =
        half_btf_avx2(&cospi42, &bf1[21], &cospi22, &bf1[26], &rounding, bit);
    bf0[27] =
        half_btf_avx2(&cospi10, &bf1[20], &cospi54, &bf1[27], &rounding, bit);
    bf0[28] =
        half_btf_avx2(&cospi50, &bf1[19], &cospi14, &bf1[28], &rounding, bit);
    bf0[29] =
        half_btf_avx2(&cospi18, &bf1[18], &cospi46, &bf1[29], &rounding, bit);
    bf0[30] =
        half_btf_avx2(&cospi34, &bf1[17], &cospi30, &bf1[30], &rounding, bit);
    bf0[31] =
        half_btf_avx2(&cospi2, &bf1[16], &cospi62, &bf1[31], &rounding, bit);

    // stage 3
    bf1[0] = bf0[0];
    bf1[1] = bf0[1];
    bf1[2] = bf0[2];
    bf1[3] = bf0[3];
    bf1[4] = bf0[4];
    bf1[5] = bf0[5];
    bf1[6] = bf0[6];
    bf1[7] = bf0[7];
    bf1[8] =
        half_btf_avx2(&cospi60, &bf0[8], &cospim4, &bf0[15], &rounding, bit);
    bf1[9] =
        half_btf_avx2(&cospi28, &bf0[9], &cospim36, &bf0[14], &rounding, bit);
    bf1[10] =
        half_btf_avx2(&cospi44, &bf0[10], &cospim20, &bf0[13], &rounding, bit);
    bf1[11] =
        half_btf_avx2(&cospi12, &bf0[11], &cospim52, &bf0[12], &rounding, bit);
    bf1[12] =
        half_btf_avx2(&cospi52, &bf0[11], &cospi12, &bf0[12], &rounding, bit);
    bf1[13] =
        half_btf_avx2(&cospi20, &bf0[10], &cospi44, &bf0[13], &rounding, bit);
    bf1[14] =
        half_btf_avx2(&cospi36, &bf0[9], &cospi28, &bf0[14], &rounding, bit);
    bf1[15] =
        half_btf_avx2(&cospi4, &bf0[8], &cospi60, &bf0[15], &rounding, bit);

    addsub_avx2(bf0[16], bf0[17], bf1 + 16, bf1 + 17, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[19], bf0[18], bf1 + 19, bf1 + 18, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[20], bf0[21], bf1 + 20, bf1 + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[23], bf0[22], bf1 + 23, bf1 + 22, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[24], bf0[25], bf1 + 24, bf1 + 25, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[27], bf0[26], bf1 + 27, bf1 + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[28], bf0[29], bf1 + 28, bf1 + 29, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[31], bf0[30], bf1 + 31, bf1 + 30, &clamp_lo, &clamp_hi);

    // stage 4
    bf0[0] = bf1[0];
    bf0[1] = bf1[1];
    bf0[2] = bf1[2];
    bf0[3] = bf1[3];
    bf0[4] =
        half_btf_avx2(&cospi56, &bf1[4], &cospim8, &bf1[7], &rounding, bit);
    bf0[5] =
        half_btf_avx2(&cospi24, &bf1[5], &cospim40, &bf1[6], &rounding, bit);
    bf0[6] =
        half_btf_avx2(&cospi40, &bf1[5], &cospi24, &bf1[6], &rounding, bit);
    bf0[7] = half_btf_avx2(&cospi8, &bf1[4], &cospi56, &bf1[7], &rounding, bit);

    addsub_avx2(bf1[8], bf1[9], bf0 + 8, bf0 + 9, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[11], bf1[10], bf0 + 11, bf0 + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[12], bf1[13], bf0 + 12, bf0 + 13, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[15], bf1[14], bf0 + 15, bf0 + 14, &clamp_lo, &clamp_hi);

    bf0[16] = bf1[16];
    bf0[17] =
        half_btf_avx2(&cospim8, &bf1[17], &cospi56, &bf1[30], &rounding, bit);
    bf0[18] =
        half_btf_avx2(&cospim56, &bf1[18], &cospim8, &bf1[29], &rounding, bit);
    bf0[19] = bf1[19];
    bf0[20] = bf1[20];
    bf0[21] =
        half_btf_avx2(&cospim40, &bf1[21], &cospi24, &bf1[26], &rounding, bit);
    bf0[22] =
        half_btf_avx2(&cospim24, &bf1[22], &cospim40, &bf1[25], &rounding, bit);
    bf0[23] = bf1[23];
    bf0[24] = bf1[24];
    bf0[25] =
        half_btf_avx2(&cospim40, &bf1[22], &cospi24, &bf1[25], &rounding, bit);
    bf0[26] =
        half_btf_avx2(&cospi24, &bf1[21], &cospi40, &bf1[26], &rounding, bit);
    bf0[27] = bf1[27];
    bf0[28] = bf1[28];
    bf0[29] =
        half_btf_avx2(&cospim8, &bf1[18], &cospi56, &bf1[29], &rounding, bit);
    bf0[30] =
        half_btf_avx2(&cospi56, &bf1[17], &cospi8, &bf1[30], &rounding, bit);
    bf0[31] = bf1[31];

    // stage 5
    bf1[0] =
        half_btf_avx2(&cospi32, &bf0[0], &cospi32, &bf0[1], &rounding, bit);
    bf1[1] =
        half_btf_avx2(&cospi32, &bf0[0], &cospim32, &bf0[1], &rounding, bit);
    bf1[2] =
        half_btf_avx2(&cospi48, &bf0[2], &cospim16, &bf0[3], &rounding, bit);
    bf1[3] =
        half_btf_avx2(&cospi16, &bf0[2], &cospi48, &bf0[3], &rounding, bit);
    addsub_avx2(bf0[4], bf0[5], bf1 + 4, bf1 + 5, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[7], bf0[6], bf1 + 7, bf1 + 6, &clamp_lo, &clamp_hi);
    bf1[8] = bf0[8];
    bf1[9] =
        half_btf_avx2(&cospim16, &bf0[9], &cospi48, &bf0[14], &rounding, bit);
    bf1[10] =
        half_btf_avx2(&cospim48, &bf0[10], &cospim16, &bf0[13], &rounding, bit);
    bf1[11] = bf0[11];
    bf1[12] = bf0[12];
    bf1[13] =
        half_btf_avx2(&cospim16, &bf0[10], &cospi48, &bf0[13], &rounding, bit);
    bf1[14] =
        half_btf_avx2(&cospi48, &bf0[9], &cospi16, &bf0[14], &rounding, bit);
    bf1[15] = bf0[15];
    addsub_avx2(bf0[16], bf0[19], bf1 + 16, bf1 + 19, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[17], bf0[18], bf1 + 17, bf1 + 18, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[23], bf0[20], bf1 + 23, bf1 + 20, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[22], bf0[21], bf1 + 22, bf1 + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[24], bf0[27], bf1 + 24, bf1 + 27, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[25], bf0[26], bf1 + 25, bf1 + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[31], bf0[28], bf1 + 31, bf1 + 28, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[30], bf0[29], bf1 + 30, bf1 + 29, &clamp_lo, &clamp_hi);

    // stage 6
    addsub_avx2(bf1[0], bf1[3], bf0 + 0, bf0 + 3, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[1], bf1[2], bf0 + 1, bf0 + 2, &clamp_lo, &clamp_hi);
    bf0[4] = bf1[4];
    bf0[5] =
        half_btf_avx2(&cospim32, &bf1[5], &cospi32, &bf1[6], &rounding, bit);
    bf0[6] =
        half_btf_avx2(&cospi32, &bf1[5], &cospi32, &bf1[6], &rounding, bit);
    bf0[7] = bf1[7];
    addsub_avx2(bf1[8], bf1[11], bf0 + 8, bf0 + 11, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[9], bf1[10], bf0 + 9, bf0 + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[15], bf1[12], bf0 + 15, bf0 + 12, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[14], bf1[13], bf0 + 14, bf0 + 13, &clamp_lo, &clamp_hi);
    bf0[16] = bf1[16];
    bf0[17] = bf1[17];
    bf0[18] =
        half_btf_avx2(&cospim16, &bf1[18], &cospi48, &bf1[29], &rounding, bit);
    bf0[19] =
        half_btf_avx2(&cospim16, &bf1[19], &cospi48, &bf1[28], &rounding, bit);
    bf0[20] =
        half_btf_avx2(&cospim48, &bf1[20], &cospim16, &bf1[27], &rounding, bit);
    bf0[21] =
        half_btf_avx2(&cospim48, &bf1[21], &cospim16, &bf1[26], &rounding, bit);
    bf0[22] = bf1[22];
    bf0[23] = bf1[23];
    bf0[24] = bf1[24];
    bf0[25] = bf1[25];
    bf0[26] =
        half_btf_avx2(&cospim16, &bf1[21], &cospi48, &bf1[26], &rounding, bit);
    bf0[27] =
        half_btf_avx2(&cospim16, &bf1[20], &cospi48, &bf1[27], &rounding, bit);
    bf0[28] =
        half_btf_avx2(&cospi48, &bf1[19], &cospi16, &bf1[28], &rounding, bit);
    bf0[29] =
        half_btf_avx2(&cospi48, &bf1[18], &cospi16, &bf1[29], &rounding, bit);
    bf0[30] = bf1[30];
    bf0[31] = bf1[31];

    // stage 7
    addsub_avx2(bf0[0], bf0[7], bf1 + 0, bf1 + 7, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[1], bf0[6], bf1 + 1, bf1 + 6, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[2], bf0[5], bf1 + 2, bf1 + 5, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[3], bf0[4], bf1 + 3, bf1 + 4, &clamp_lo, &clamp_hi);
    bf1[8] = bf0[8];
    bf1[9] = bf0[9];
    bf1[10] =
        half_btf_avx2(&cospim32, &bf0[10], &cospi32, &bf0[13], &rounding, bit);
    bf1[11] =
        half_btf_avx2(&cospim32, &bf0[11], &cospi32, &bf0[12], &rounding, bit);
    bf1[12] =
        half_btf_avx2(&cospi32, &bf0[11], &cospi32, &bf0[12], &rounding, bit);
    bf1[13] =
        half_btf_avx2(&cospi32, &bf0[10], &cospi32, &bf0[13], &rounding, bit);
    bf1[14] = bf0[14];
    bf1[15] = bf0[15];
    addsub_avx2(bf0[16], bf0[23], bf1 + 16, bf1 + 23, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[17], bf0[22], bf1 + 17, bf1 + 22, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[18], bf0[21], bf1 + 18, bf1 + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[19], bf0[20], bf1 + 19, bf1 + 20, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[31], bf0[24], bf1 + 31, bf1 + 24, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[30], bf0[25], bf1 + 30, bf1 + 25, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[29], bf0[26], bf1 + 29, bf1 + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[28], bf0[27], bf1 + 28, bf1 + 27, &clamp_lo, &clamp_hi);

    // stage 8
    addsub_avx2(bf1[0], bf1[15], bf0 + 0, bf0 + 15, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[1], bf1[14], bf0 + 1, bf0 + 14, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[2], bf1[13], bf0 + 2, bf0 + 13, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[3], bf1[12], bf0 + 3, bf0 + 12, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[4], bf1[11], bf0 + 4, bf0 + 11, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[5], bf1[10], bf0 + 5, bf0 + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[6], bf1[9], bf0 + 6, bf0 + 9, &clamp_lo, &clamp_hi);
    addsub_avx2(bf1[7], bf1[8], bf0 + 7, bf0 + 8, &clamp_lo, &clamp_hi);
    bf0[16] = bf1[16];
    bf0[17] = bf1[17];
    bf0[18] = bf1[18];
    bf0[19] = bf1[19];
    bf0[20] =
        half_btf_avx2(&cospim32, &bf1[20], &cospi32, &bf1[27], &rounding, bit);
    bf0[21] =
        half_btf_avx2(&cospim32, &bf1[21], &cospi32, &bf1[26], &rounding, bit);
    bf0[22] =
        half_btf_avx2(&cospim32, &bf1[22], &cospi32, &bf1[25], &rounding, bit);
    bf0[23] =
        half_btf_avx2(&cospim32, &bf1[23], &cospi32, &bf1[24], &rounding, bit);
    bf0[24] =
        half_btf_avx2(&cospi32, &bf1[23], &cospi32, &bf1[24], &rounding, bit);
    bf0[25] =
        half_btf_avx2(&cospi32, &bf1[22], &cospi32, &bf1[25], &rounding, bit);
    bf0[26] =
        half_btf_avx2(&cospi32, &bf1[21], &cospi32, &bf1[26], &rounding, bit);
    bf0[27] =
        half_btf_avx2(&cospi32, &bf1[20], &cospi32, &bf1[27], &rounding, bit);
    bf0[28] = bf1[28];
    bf0[29] = bf1[29];
    bf0[30] = bf1[30];
    bf0[31] = bf1[31];

    // stage 9
    addsub_avx2(bf0[0], bf0[31], out + 0, out + 31, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[1], bf0[30], out + 1, out + 30, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[2], bf0[29], out + 2, out + 29, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[3], bf0[28], out + 3, out + 28, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[4], bf0[27], out + 4, out + 27, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[5], bf0[26], out + 5, out + 26, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[6], bf0[25], out + 6, out + 25, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[7], bf0[24], out + 7, out + 24, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[8], bf0[23], out + 8, out + 23, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[9], bf0[22], out + 9, out + 22, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[10], bf0[21], out + 10, out + 21, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[11], bf0[20], out + 11, out + 20, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[12], bf0[19], out + 12, out + 19, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[13], bf0[18], out + 13, out + 18, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[14], bf0[17], out + 14, out + 17, &clamp_lo, &clamp_hi);
    addsub_avx2(bf0[15], bf0[16], out + 15, out + 16, &clamp_lo, &clamp_hi);
    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      const __m256i clamp_lo_out =
          _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      const __m256i clamp_hi_out =
          _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);
      round_shift_8x8_avx2(out, out_shift);
      round_shift_8x8_avx2(out + 16, out_shift);
      highbd_clamp_epi32_avx2(out, out, &clamp_lo_out, &clamp_hi_out, 32);
    }
  }
}
static void idct16_low1_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                             int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);

  {
    // stage 0
    // stage 1
    // stage 2
    // stage 3
    // stage 4
    in[0] = _mm256_mullo_epi32(in[0], cospi32);
    in[0] = _mm256_add_epi32(in[0], rnding);
    in[0] = _mm256_srai_epi32(in[0], bit);

    // stage 5
    // stage 6
    // stage 7
    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      clamp_lo = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      clamp_hi = _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);
      __m256i offset = _mm256_set1_epi32((1 << out_shift) >> 1);
      in[0] = _mm256_add_epi32(in[0], offset);
      in[0] = _mm256_sra_epi32(in[0], _mm_cvtsi32_si128(out_shift));
    }
    in[0] = _mm256_max_epi32(in[0], clamp_lo);
    in[0] = _mm256_min_epi32(in[0], clamp_hi);
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
}

static void idct16_low8_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                             int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i u[16], x, y;

  {
    // stage 0
    // stage 1
    u[0] = in[0];
    u[2] = in[4];
    u[4] = in[2];
    u[6] = in[6];
    u[8] = in[1];
    u[10] = in[5];
    u[12] = in[3];
    u[14] = in[7];

    // stage 2
    u[15] = half_btf_0_avx2(&cospi4, &u[8], &rnding, bit);
    u[8] = half_btf_0_avx2(&cospi60, &u[8], &rnding, bit);

    u[9] = half_btf_0_avx2(&cospim36, &u[14], &rnding, bit);
    u[14] = half_btf_0_avx2(&cospi28, &u[14], &rnding, bit);

    u[13] = half_btf_0_avx2(&cospi20, &u[10], &rnding, bit);
    u[10] = half_btf_0_avx2(&cospi44, &u[10], &rnding, bit);

    u[11] = half_btf_0_avx2(&cospim52, &u[12], &rnding, bit);
    u[12] = half_btf_0_avx2(&cospi12, &u[12], &rnding, bit);

    // stage 3
    u[7] = half_btf_0_avx2(&cospi8, &u[4], &rnding, bit);
    u[4] = half_btf_0_avx2(&cospi56, &u[4], &rnding, bit);
    u[5] = half_btf_0_avx2(&cospim40, &u[6], &rnding, bit);
    u[6] = half_btf_0_avx2(&cospi24, &u[6], &rnding, bit);

    addsub_avx2(u[8], u[9], &u[8], &u[9], &clamp_lo, &clamp_hi);
    addsub_avx2(u[11], u[10], &u[11], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(u[12], u[13], &u[12], &u[13], &clamp_lo, &clamp_hi);
    addsub_avx2(u[15], u[14], &u[15], &u[14], &clamp_lo, &clamp_hi);

    // stage 4
    x = _mm256_mullo_epi32(u[0], cospi32);
    u[0] = _mm256_add_epi32(x, rnding);
    u[0] = _mm256_srai_epi32(u[0], bit);
    u[1] = u[0];

    u[3] = half_btf_0_avx2(&cospi16, &u[2], &rnding, bit);
    u[2] = half_btf_0_avx2(&cospi48, &u[2], &rnding, bit);

    addsub_avx2(u[4], u[5], &u[4], &u[5], &clamp_lo, &clamp_hi);
    addsub_avx2(u[7], u[6], &u[7], &u[6], &clamp_lo, &clamp_hi);

    x = half_btf_avx2(&cospim16, &u[9], &cospi48, &u[14], &rnding, bit);
    u[14] = half_btf_avx2(&cospi48, &u[9], &cospi16, &u[14], &rnding, bit);
    u[9] = x;
    y = half_btf_avx2(&cospim48, &u[10], &cospim16, &u[13], &rnding, bit);
    u[13] = half_btf_avx2(&cospim16, &u[10], &cospi48, &u[13], &rnding, bit);
    u[10] = y;

    // stage 5
    addsub_avx2(u[0], u[3], &u[0], &u[3], &clamp_lo, &clamp_hi);
    addsub_avx2(u[1], u[2], &u[1], &u[2], &clamp_lo, &clamp_hi);

    x = _mm256_mullo_epi32(u[5], cospi32);
    y = _mm256_mullo_epi32(u[6], cospi32);
    u[5] = _mm256_sub_epi32(y, x);
    u[5] = _mm256_add_epi32(u[5], rnding);
    u[5] = _mm256_srai_epi32(u[5], bit);

    u[6] = _mm256_add_epi32(y, x);
    u[6] = _mm256_add_epi32(u[6], rnding);
    u[6] = _mm256_srai_epi32(u[6], bit);

    addsub_avx2(u[8], u[11], &u[8], &u[11], &clamp_lo, &clamp_hi);
    addsub_avx2(u[9], u[10], &u[9], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(u[15], u[12], &u[15], &u[12], &clamp_lo, &clamp_hi);
    addsub_avx2(u[14], u[13], &u[14], &u[13], &clamp_lo, &clamp_hi);

    // stage 6
    addsub_avx2(u[0], u[7], &u[0], &u[7], &clamp_lo, &clamp_hi);
    addsub_avx2(u[1], u[6], &u[1], &u[6], &clamp_lo, &clamp_hi);
    addsub_avx2(u[2], u[5], &u[2], &u[5], &clamp_lo, &clamp_hi);
    addsub_avx2(u[3], u[4], &u[3], &u[4], &clamp_lo, &clamp_hi);

    x = _mm256_mullo_epi32(u[10], cospi32);
    y = _mm256_mullo_epi32(u[13], cospi32);
    u[10] = _mm256_sub_epi32(y, x);
    u[10] = _mm256_add_epi32(u[10], rnding);
    u[10] = _mm256_srai_epi32(u[10], bit);

    u[13] = _mm256_add_epi32(x, y);
    u[13] = _mm256_add_epi32(u[13], rnding);
    u[13] = _mm256_srai_epi32(u[13], bit);

    x = _mm256_mullo_epi32(u[11], cospi32);
    y = _mm256_mullo_epi32(u[12], cospi32);
    u[11] = _mm256_sub_epi32(y, x);
    u[11] = _mm256_add_epi32(u[11], rnding);
    u[11] = _mm256_srai_epi32(u[11], bit);

    u[12] = _mm256_add_epi32(x, y);
    u[12] = _mm256_add_epi32(u[12], rnding);
    u[12] = _mm256_srai_epi32(u[12], bit);
    // stage 7
    addsub_avx2(u[0], u[15], out + 0, out + 15, &clamp_lo, &clamp_hi);
    addsub_avx2(u[1], u[14], out + 1, out + 14, &clamp_lo, &clamp_hi);
    addsub_avx2(u[2], u[13], out + 2, out + 13, &clamp_lo, &clamp_hi);
    addsub_avx2(u[3], u[12], out + 3, out + 12, &clamp_lo, &clamp_hi);
    addsub_avx2(u[4], u[11], out + 4, out + 11, &clamp_lo, &clamp_hi);
    addsub_avx2(u[5], u[10], out + 5, out + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(u[6], u[9], out + 6, out + 9, &clamp_lo, &clamp_hi);
    addsub_avx2(u[7], u[8], out + 7, out + 8, &clamp_lo, &clamp_hi);

    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      const __m256i clamp_lo_out =
          _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      const __m256i clamp_hi_out =
          _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);
      round_shift_8x8_avx2(out, out_shift);
      highbd_clamp_epi32_avx2(out, out, &clamp_lo_out, &clamp_hi_out, 16);
    }
  }
}

static void idct16_avx2(__m256i *in, __m256i *out, int bit, int do_cols, int bd,
                        int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospim4 = _mm256_set1_epi32(-cospi[4]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospim20 = _mm256_set1_epi32(-cospi[20]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
  const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i u[16], v[16], x, y;

  {
    // stage 0
    // stage 1
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

    v[8] = half_btf_avx2(&cospi60, &u[8], &cospim4, &u[15], &rnding, bit);
    v[9] = half_btf_avx2(&cospi28, &u[9], &cospim36, &u[14], &rnding, bit);
    v[10] = half_btf_avx2(&cospi44, &u[10], &cospim20, &u[13], &rnding, bit);
    v[11] = half_btf_avx2(&cospi12, &u[11], &cospim52, &u[12], &rnding, bit);
    v[12] = half_btf_avx2(&cospi52, &u[11], &cospi12, &u[12], &rnding, bit);
    v[13] = half_btf_avx2(&cospi20, &u[10], &cospi44, &u[13], &rnding, bit);
    v[14] = half_btf_avx2(&cospi36, &u[9], &cospi28, &u[14], &rnding, bit);
    v[15] = half_btf_avx2(&cospi4, &u[8], &cospi60, &u[15], &rnding, bit);

    // stage 3
    u[0] = v[0];
    u[1] = v[1];
    u[2] = v[2];
    u[3] = v[3];
    u[4] = half_btf_avx2(&cospi56, &v[4], &cospim8, &v[7], &rnding, bit);
    u[5] = half_btf_avx2(&cospi24, &v[5], &cospim40, &v[6], &rnding, bit);
    u[6] = half_btf_avx2(&cospi40, &v[5], &cospi24, &v[6], &rnding, bit);
    u[7] = half_btf_avx2(&cospi8, &v[4], &cospi56, &v[7], &rnding, bit);
    addsub_avx2(v[8], v[9], &u[8], &u[9], &clamp_lo, &clamp_hi);
    addsub_avx2(v[11], v[10], &u[11], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(v[12], v[13], &u[12], &u[13], &clamp_lo, &clamp_hi);
    addsub_avx2(v[15], v[14], &u[15], &u[14], &clamp_lo, &clamp_hi);

    // stage 4
    x = _mm256_mullo_epi32(u[0], cospi32);
    y = _mm256_mullo_epi32(u[1], cospi32);
    v[0] = _mm256_add_epi32(x, y);
    v[0] = _mm256_add_epi32(v[0], rnding);
    v[0] = _mm256_srai_epi32(v[0], bit);

    v[1] = _mm256_sub_epi32(x, y);
    v[1] = _mm256_add_epi32(v[1], rnding);
    v[1] = _mm256_srai_epi32(v[1], bit);

    v[2] = half_btf_avx2(&cospi48, &u[2], &cospim16, &u[3], &rnding, bit);
    v[3] = half_btf_avx2(&cospi16, &u[2], &cospi48, &u[3], &rnding, bit);
    addsub_avx2(u[4], u[5], &v[4], &v[5], &clamp_lo, &clamp_hi);
    addsub_avx2(u[7], u[6], &v[7], &v[6], &clamp_lo, &clamp_hi);
    v[8] = u[8];
    v[9] = half_btf_avx2(&cospim16, &u[9], &cospi48, &u[14], &rnding, bit);
    v[10] = half_btf_avx2(&cospim48, &u[10], &cospim16, &u[13], &rnding, bit);
    v[11] = u[11];
    v[12] = u[12];
    v[13] = half_btf_avx2(&cospim16, &u[10], &cospi48, &u[13], &rnding, bit);
    v[14] = half_btf_avx2(&cospi48, &u[9], &cospi16, &u[14], &rnding, bit);
    v[15] = u[15];

    // stage 5
    addsub_avx2(v[0], v[3], &u[0], &u[3], &clamp_lo, &clamp_hi);
    addsub_avx2(v[1], v[2], &u[1], &u[2], &clamp_lo, &clamp_hi);
    u[4] = v[4];

    x = _mm256_mullo_epi32(v[5], cospi32);
    y = _mm256_mullo_epi32(v[6], cospi32);
    u[5] = _mm256_sub_epi32(y, x);
    u[5] = _mm256_add_epi32(u[5], rnding);
    u[5] = _mm256_srai_epi32(u[5], bit);

    u[6] = _mm256_add_epi32(y, x);
    u[6] = _mm256_add_epi32(u[6], rnding);
    u[6] = _mm256_srai_epi32(u[6], bit);

    u[7] = v[7];
    addsub_avx2(v[8], v[11], &u[8], &u[11], &clamp_lo, &clamp_hi);
    addsub_avx2(v[9], v[10], &u[9], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(v[15], v[12], &u[15], &u[12], &clamp_lo, &clamp_hi);
    addsub_avx2(v[14], v[13], &u[14], &u[13], &clamp_lo, &clamp_hi);

    // stage 6
    addsub_avx2(u[0], u[7], &v[0], &v[7], &clamp_lo, &clamp_hi);
    addsub_avx2(u[1], u[6], &v[1], &v[6], &clamp_lo, &clamp_hi);
    addsub_avx2(u[2], u[5], &v[2], &v[5], &clamp_lo, &clamp_hi);
    addsub_avx2(u[3], u[4], &v[3], &v[4], &clamp_lo, &clamp_hi);
    v[8] = u[8];
    v[9] = u[9];

    x = _mm256_mullo_epi32(u[10], cospi32);
    y = _mm256_mullo_epi32(u[13], cospi32);
    v[10] = _mm256_sub_epi32(y, x);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[13] = _mm256_add_epi32(x, y);
    v[13] = _mm256_add_epi32(v[13], rnding);
    v[13] = _mm256_srai_epi32(v[13], bit);

    x = _mm256_mullo_epi32(u[11], cospi32);
    y = _mm256_mullo_epi32(u[12], cospi32);
    v[11] = _mm256_sub_epi32(y, x);
    v[11] = _mm256_add_epi32(v[11], rnding);
    v[11] = _mm256_srai_epi32(v[11], bit);

    v[12] = _mm256_add_epi32(x, y);
    v[12] = _mm256_add_epi32(v[12], rnding);
    v[12] = _mm256_srai_epi32(v[12], bit);

    v[14] = u[14];
    v[15] = u[15];

    // stage 7
    addsub_avx2(v[0], v[15], out + 0, out + 15, &clamp_lo, &clamp_hi);
    addsub_avx2(v[1], v[14], out + 1, out + 14, &clamp_lo, &clamp_hi);
    addsub_avx2(v[2], v[13], out + 2, out + 13, &clamp_lo, &clamp_hi);
    addsub_avx2(v[3], v[12], out + 3, out + 12, &clamp_lo, &clamp_hi);
    addsub_avx2(v[4], v[11], out + 4, out + 11, &clamp_lo, &clamp_hi);
    addsub_avx2(v[5], v[10], out + 5, out + 10, &clamp_lo, &clamp_hi);
    addsub_avx2(v[6], v[9], out + 6, out + 9, &clamp_lo, &clamp_hi);
    addsub_avx2(v[7], v[8], out + 7, out + 8, &clamp_lo, &clamp_hi);

    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      const __m256i clamp_lo_out =
          _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      const __m256i clamp_hi_out =
          _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);
      round_shift_8x8_avx2(out, out_shift);
      highbd_clamp_epi32_avx2(out, out, &clamp_lo_out, &clamp_hi_out, 16);
    }
  }
}

static void iadst16_low1_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                              int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const __m256i zero = _mm256_setzero_si256();
  __m256i v[16], x, y, temp1, temp2;

  // Calculate the column 0, 1, 2, 3
  {
    // stage 0
    // stage 1
    // stage 2
    x = _mm256_mullo_epi32(in[0], cospi62);
    v[0] = _mm256_add_epi32(x, rnding);
    v[0] = _mm256_srai_epi32(v[0], bit);

    x = _mm256_mullo_epi32(in[0], cospi2);
    v[1] = _mm256_sub_epi32(zero, x);
    v[1] = _mm256_add_epi32(v[1], rnding);
    v[1] = _mm256_srai_epi32(v[1], bit);

    // stage 3
    v[8] = v[0];
    v[9] = v[1];

    // stage 4
    temp1 = _mm256_mullo_epi32(v[8], cospi8);
    x = _mm256_mullo_epi32(v[9], cospi56);
    temp1 = _mm256_add_epi32(temp1, x);
    temp1 = _mm256_add_epi32(temp1, rnding);
    temp1 = _mm256_srai_epi32(temp1, bit);

    temp2 = _mm256_mullo_epi32(v[8], cospi56);
    x = _mm256_mullo_epi32(v[9], cospi8);
    temp2 = _mm256_sub_epi32(temp2, x);
    temp2 = _mm256_add_epi32(temp2, rnding);
    temp2 = _mm256_srai_epi32(temp2, bit);
    v[8] = temp1;
    v[9] = temp2;

    // stage 5
    v[4] = v[0];
    v[5] = v[1];
    v[12] = v[8];
    v[13] = v[9];

    // stage 6
    temp1 = _mm256_mullo_epi32(v[4], cospi16);
    x = _mm256_mullo_epi32(v[5], cospi48);
    temp1 = _mm256_add_epi32(temp1, x);
    temp1 = _mm256_add_epi32(temp1, rnding);
    temp1 = _mm256_srai_epi32(temp1, bit);

    temp2 = _mm256_mullo_epi32(v[4], cospi48);
    x = _mm256_mullo_epi32(v[5], cospi16);
    temp2 = _mm256_sub_epi32(temp2, x);
    temp2 = _mm256_add_epi32(temp2, rnding);
    temp2 = _mm256_srai_epi32(temp2, bit);
    v[4] = temp1;
    v[5] = temp2;

    temp1 = _mm256_mullo_epi32(v[12], cospi16);
    x = _mm256_mullo_epi32(v[13], cospi48);
    temp1 = _mm256_add_epi32(temp1, x);
    temp1 = _mm256_add_epi32(temp1, rnding);
    temp1 = _mm256_srai_epi32(temp1, bit);

    temp2 = _mm256_mullo_epi32(v[12], cospi48);
    x = _mm256_mullo_epi32(v[13], cospi16);
    temp2 = _mm256_sub_epi32(temp2, x);
    temp2 = _mm256_add_epi32(temp2, rnding);
    temp2 = _mm256_srai_epi32(temp2, bit);
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
    y = _mm256_mullo_epi32(v[2], cospi32);
    x = _mm256_mullo_epi32(v[3], cospi32);
    v[2] = _mm256_add_epi32(y, x);
    v[2] = _mm256_add_epi32(v[2], rnding);
    v[2] = _mm256_srai_epi32(v[2], bit);

    v[3] = _mm256_sub_epi32(y, x);
    v[3] = _mm256_add_epi32(v[3], rnding);
    v[3] = _mm256_srai_epi32(v[3], bit);

    y = _mm256_mullo_epi32(v[6], cospi32);
    x = _mm256_mullo_epi32(v[7], cospi32);
    v[6] = _mm256_add_epi32(y, x);
    v[6] = _mm256_add_epi32(v[6], rnding);
    v[6] = _mm256_srai_epi32(v[6], bit);

    v[7] = _mm256_sub_epi32(y, x);
    v[7] = _mm256_add_epi32(v[7], rnding);
    v[7] = _mm256_srai_epi32(v[7], bit);

    y = _mm256_mullo_epi32(v[10], cospi32);
    x = _mm256_mullo_epi32(v[11], cospi32);
    v[10] = _mm256_add_epi32(y, x);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[11] = _mm256_sub_epi32(y, x);
    v[11] = _mm256_add_epi32(v[11], rnding);
    v[11] = _mm256_srai_epi32(v[11], bit);

    y = _mm256_mullo_epi32(v[14], cospi32);
    x = _mm256_mullo_epi32(v[15], cospi32);
    v[14] = _mm256_add_epi32(y, x);
    v[14] = _mm256_add_epi32(v[14], rnding);
    v[14] = _mm256_srai_epi32(v[14], bit);

    v[15] = _mm256_sub_epi32(y, x);
    v[15] = _mm256_add_epi32(v[15], rnding);
    v[15] = _mm256_srai_epi32(v[15], bit);

    // stage 9
    if (do_cols) {
      out[0] = v[0];
      out[1] = _mm256_sub_epi32(_mm256_setzero_si256(), v[8]);
      out[2] = v[12];
      out[3] = _mm256_sub_epi32(_mm256_setzero_si256(), v[4]);
      out[4] = v[6];
      out[5] = _mm256_sub_epi32(_mm256_setzero_si256(), v[14]);
      out[6] = v[10];
      out[7] = _mm256_sub_epi32(_mm256_setzero_si256(), v[2]);
      out[8] = v[3];
      out[9] = _mm256_sub_epi32(_mm256_setzero_si256(), v[11]);
      out[10] = v[15];
      out[11] = _mm256_sub_epi32(_mm256_setzero_si256(), v[7]);
      out[12] = v[5];
      out[13] = _mm256_sub_epi32(_mm256_setzero_si256(), v[13]);
      out[14] = v[9];
      out[15] = _mm256_sub_epi32(_mm256_setzero_si256(), v[1]);
    } else {
      const int log_range_out = AOMMAX(16, bd + 6);
      const __m256i clamp_lo_out =
          _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      const __m256i clamp_hi_out =
          _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);

      neg_shift_avx2(v[0], v[8], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                     out_shift);
      neg_shift_avx2(v[12], v[4], out + 2, out + 3, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[6], v[14], out + 4, out + 5, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[10], v[2], out + 6, out + 7, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[3], v[11], out + 8, out + 9, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[15], v[7], out + 10, out + 11, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[5], v[13], out + 12, out + 13, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[9], v[1], out + 14, out + 15, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
    }
  }
}

static void iadst16_low8_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                              int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
  const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
  const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
  const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
  const __m256i cospi34 = _mm256_set1_epi32(cospi[34]);
  const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
  const __m256i cospi42 = _mm256_set1_epi32(cospi[42]);
  const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
  const __m256i cospi50 = _mm256_set1_epi32(cospi[50]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi58 = _mm256_set1_epi32(cospi[58]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i u[16], x, y;

  {
    // stage 0
    // stage 1
    // stage 2
    __m256i zero = _mm256_setzero_si256();
    x = _mm256_mullo_epi32(in[0], cospi62);
    u[0] = _mm256_add_epi32(x, rnding);
    u[0] = _mm256_srai_epi32(u[0], bit);

    x = _mm256_mullo_epi32(in[0], cospi2);
    u[1] = _mm256_sub_epi32(zero, x);
    u[1] = _mm256_add_epi32(u[1], rnding);
    u[1] = _mm256_srai_epi32(u[1], bit);

    x = _mm256_mullo_epi32(in[2], cospi54);
    u[2] = _mm256_add_epi32(x, rnding);
    u[2] = _mm256_srai_epi32(u[2], bit);

    x = _mm256_mullo_epi32(in[2], cospi10);
    u[3] = _mm256_sub_epi32(zero, x);
    u[3] = _mm256_add_epi32(u[3], rnding);
    u[3] = _mm256_srai_epi32(u[3], bit);

    x = _mm256_mullo_epi32(in[4], cospi46);
    u[4] = _mm256_add_epi32(x, rnding);
    u[4] = _mm256_srai_epi32(u[4], bit);

    x = _mm256_mullo_epi32(in[4], cospi18);
    u[5] = _mm256_sub_epi32(zero, x);
    u[5] = _mm256_add_epi32(u[5], rnding);
    u[5] = _mm256_srai_epi32(u[5], bit);

    x = _mm256_mullo_epi32(in[6], cospi38);
    u[6] = _mm256_add_epi32(x, rnding);
    u[6] = _mm256_srai_epi32(u[6], bit);

    x = _mm256_mullo_epi32(in[6], cospi26);
    u[7] = _mm256_sub_epi32(zero, x);
    u[7] = _mm256_add_epi32(u[7], rnding);
    u[7] = _mm256_srai_epi32(u[7], bit);

    u[8] = _mm256_mullo_epi32(in[7], cospi34);
    u[8] = _mm256_add_epi32(u[8], rnding);
    u[8] = _mm256_srai_epi32(u[8], bit);

    u[9] = _mm256_mullo_epi32(in[7], cospi30);
    u[9] = _mm256_add_epi32(u[9], rnding);
    u[9] = _mm256_srai_epi32(u[9], bit);

    u[10] = _mm256_mullo_epi32(in[5], cospi42);
    u[10] = _mm256_add_epi32(u[10], rnding);
    u[10] = _mm256_srai_epi32(u[10], bit);

    u[11] = _mm256_mullo_epi32(in[5], cospi22);
    u[11] = _mm256_add_epi32(u[11], rnding);
    u[11] = _mm256_srai_epi32(u[11], bit);

    u[12] = _mm256_mullo_epi32(in[3], cospi50);
    u[12] = _mm256_add_epi32(u[12], rnding);
    u[12] = _mm256_srai_epi32(u[12], bit);

    u[13] = _mm256_mullo_epi32(in[3], cospi14);
    u[13] = _mm256_add_epi32(u[13], rnding);
    u[13] = _mm256_srai_epi32(u[13], bit);

    u[14] = _mm256_mullo_epi32(in[1], cospi58);
    u[14] = _mm256_add_epi32(u[14], rnding);
    u[14] = _mm256_srai_epi32(u[14], bit);

    u[15] = _mm256_mullo_epi32(in[1], cospi6);
    u[15] = _mm256_add_epi32(u[15], rnding);
    u[15] = _mm256_srai_epi32(u[15], bit);

    // stage 3
    addsub_avx2(u[0], u[8], &u[0], &u[8], &clamp_lo, &clamp_hi);
    addsub_avx2(u[1], u[9], &u[1], &u[9], &clamp_lo, &clamp_hi);
    addsub_avx2(u[2], u[10], &u[2], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(u[3], u[11], &u[3], &u[11], &clamp_lo, &clamp_hi);
    addsub_avx2(u[4], u[12], &u[4], &u[12], &clamp_lo, &clamp_hi);
    addsub_avx2(u[5], u[13], &u[5], &u[13], &clamp_lo, &clamp_hi);
    addsub_avx2(u[6], u[14], &u[6], &u[14], &clamp_lo, &clamp_hi);
    addsub_avx2(u[7], u[15], &u[7], &u[15], &clamp_lo, &clamp_hi);

    // stage 4
    y = _mm256_mullo_epi32(u[8], cospi56);
    x = _mm256_mullo_epi32(u[9], cospi56);
    u[8] = _mm256_mullo_epi32(u[8], cospi8);
    u[8] = _mm256_add_epi32(u[8], x);
    u[8] = _mm256_add_epi32(u[8], rnding);
    u[8] = _mm256_srai_epi32(u[8], bit);

    x = _mm256_mullo_epi32(u[9], cospi8);
    u[9] = _mm256_sub_epi32(y, x);
    u[9] = _mm256_add_epi32(u[9], rnding);
    u[9] = _mm256_srai_epi32(u[9], bit);

    x = _mm256_mullo_epi32(u[11], cospi24);
    y = _mm256_mullo_epi32(u[10], cospi24);
    u[10] = _mm256_mullo_epi32(u[10], cospi40);
    u[10] = _mm256_add_epi32(u[10], x);
    u[10] = _mm256_add_epi32(u[10], rnding);
    u[10] = _mm256_srai_epi32(u[10], bit);

    x = _mm256_mullo_epi32(u[11], cospi40);
    u[11] = _mm256_sub_epi32(y, x);
    u[11] = _mm256_add_epi32(u[11], rnding);
    u[11] = _mm256_srai_epi32(u[11], bit);

    x = _mm256_mullo_epi32(u[13], cospi8);
    y = _mm256_mullo_epi32(u[12], cospi8);
    u[12] = _mm256_mullo_epi32(u[12], cospim56);
    u[12] = _mm256_add_epi32(u[12], x);
    u[12] = _mm256_add_epi32(u[12], rnding);
    u[12] = _mm256_srai_epi32(u[12], bit);

    x = _mm256_mullo_epi32(u[13], cospim56);
    u[13] = _mm256_sub_epi32(y, x);
    u[13] = _mm256_add_epi32(u[13], rnding);
    u[13] = _mm256_srai_epi32(u[13], bit);

    x = _mm256_mullo_epi32(u[15], cospi40);
    y = _mm256_mullo_epi32(u[14], cospi40);
    u[14] = _mm256_mullo_epi32(u[14], cospim24);
    u[14] = _mm256_add_epi32(u[14], x);
    u[14] = _mm256_add_epi32(u[14], rnding);
    u[14] = _mm256_srai_epi32(u[14], bit);

    x = _mm256_mullo_epi32(u[15], cospim24);
    u[15] = _mm256_sub_epi32(y, x);
    u[15] = _mm256_add_epi32(u[15], rnding);
    u[15] = _mm256_srai_epi32(u[15], bit);

    // stage 5
    addsub_avx2(u[0], u[4], &u[0], &u[4], &clamp_lo, &clamp_hi);
    addsub_avx2(u[1], u[5], &u[1], &u[5], &clamp_lo, &clamp_hi);
    addsub_avx2(u[2], u[6], &u[2], &u[6], &clamp_lo, &clamp_hi);
    addsub_avx2(u[3], u[7], &u[3], &u[7], &clamp_lo, &clamp_hi);
    addsub_avx2(u[8], u[12], &u[8], &u[12], &clamp_lo, &clamp_hi);
    addsub_avx2(u[9], u[13], &u[9], &u[13], &clamp_lo, &clamp_hi);
    addsub_avx2(u[10], u[14], &u[10], &u[14], &clamp_lo, &clamp_hi);
    addsub_avx2(u[11], u[15], &u[11], &u[15], &clamp_lo, &clamp_hi);

    // stage 6
    x = _mm256_mullo_epi32(u[5], cospi48);
    y = _mm256_mullo_epi32(u[4], cospi48);
    u[4] = _mm256_mullo_epi32(u[4], cospi16);
    u[4] = _mm256_add_epi32(u[4], x);
    u[4] = _mm256_add_epi32(u[4], rnding);
    u[4] = _mm256_srai_epi32(u[4], bit);

    x = _mm256_mullo_epi32(u[5], cospi16);
    u[5] = _mm256_sub_epi32(y, x);
    u[5] = _mm256_add_epi32(u[5], rnding);
    u[5] = _mm256_srai_epi32(u[5], bit);

    x = _mm256_mullo_epi32(u[7], cospi16);
    y = _mm256_mullo_epi32(u[6], cospi16);
    u[6] = _mm256_mullo_epi32(u[6], cospim48);
    u[6] = _mm256_add_epi32(u[6], x);
    u[6] = _mm256_add_epi32(u[6], rnding);
    u[6] = _mm256_srai_epi32(u[6], bit);

    x = _mm256_mullo_epi32(u[7], cospim48);
    u[7] = _mm256_sub_epi32(y, x);
    u[7] = _mm256_add_epi32(u[7], rnding);
    u[7] = _mm256_srai_epi32(u[7], bit);

    x = _mm256_mullo_epi32(u[13], cospi48);
    y = _mm256_mullo_epi32(u[12], cospi48);
    u[12] = _mm256_mullo_epi32(u[12], cospi16);
    u[12] = _mm256_add_epi32(u[12], x);
    u[12] = _mm256_add_epi32(u[12], rnding);
    u[12] = _mm256_srai_epi32(u[12], bit);

    x = _mm256_mullo_epi32(u[13], cospi16);
    u[13] = _mm256_sub_epi32(y, x);
    u[13] = _mm256_add_epi32(u[13], rnding);
    u[13] = _mm256_srai_epi32(u[13], bit);

    x = _mm256_mullo_epi32(u[15], cospi16);
    y = _mm256_mullo_epi32(u[14], cospi16);
    u[14] = _mm256_mullo_epi32(u[14], cospim48);
    u[14] = _mm256_add_epi32(u[14], x);
    u[14] = _mm256_add_epi32(u[14], rnding);
    u[14] = _mm256_srai_epi32(u[14], bit);

    x = _mm256_mullo_epi32(u[15], cospim48);
    u[15] = _mm256_sub_epi32(y, x);
    u[15] = _mm256_add_epi32(u[15], rnding);
    u[15] = _mm256_srai_epi32(u[15], bit);

    // stage 7
    addsub_avx2(u[0], u[2], &u[0], &u[2], &clamp_lo, &clamp_hi);
    addsub_avx2(u[1], u[3], &u[1], &u[3], &clamp_lo, &clamp_hi);
    addsub_avx2(u[4], u[6], &u[4], &u[6], &clamp_lo, &clamp_hi);
    addsub_avx2(u[5], u[7], &u[5], &u[7], &clamp_lo, &clamp_hi);
    addsub_avx2(u[8], u[10], &u[8], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(u[9], u[11], &u[9], &u[11], &clamp_lo, &clamp_hi);
    addsub_avx2(u[12], u[14], &u[12], &u[14], &clamp_lo, &clamp_hi);
    addsub_avx2(u[13], u[15], &u[13], &u[15], &clamp_lo, &clamp_hi);

    // stage 8
    y = _mm256_mullo_epi32(u[2], cospi32);
    x = _mm256_mullo_epi32(u[3], cospi32);
    u[2] = _mm256_add_epi32(y, x);
    u[2] = _mm256_add_epi32(u[2], rnding);
    u[2] = _mm256_srai_epi32(u[2], bit);

    u[3] = _mm256_sub_epi32(y, x);
    u[3] = _mm256_add_epi32(u[3], rnding);
    u[3] = _mm256_srai_epi32(u[3], bit);
    y = _mm256_mullo_epi32(u[6], cospi32);
    x = _mm256_mullo_epi32(u[7], cospi32);
    u[6] = _mm256_add_epi32(y, x);
    u[6] = _mm256_add_epi32(u[6], rnding);
    u[6] = _mm256_srai_epi32(u[6], bit);

    u[7] = _mm256_sub_epi32(y, x);
    u[7] = _mm256_add_epi32(u[7], rnding);
    u[7] = _mm256_srai_epi32(u[7], bit);

    y = _mm256_mullo_epi32(u[10], cospi32);
    x = _mm256_mullo_epi32(u[11], cospi32);
    u[10] = _mm256_add_epi32(y, x);
    u[10] = _mm256_add_epi32(u[10], rnding);
    u[10] = _mm256_srai_epi32(u[10], bit);

    u[11] = _mm256_sub_epi32(y, x);
    u[11] = _mm256_add_epi32(u[11], rnding);
    u[11] = _mm256_srai_epi32(u[11], bit);

    y = _mm256_mullo_epi32(u[14], cospi32);
    x = _mm256_mullo_epi32(u[15], cospi32);
    u[14] = _mm256_add_epi32(y, x);
    u[14] = _mm256_add_epi32(u[14], rnding);
    u[14] = _mm256_srai_epi32(u[14], bit);

    u[15] = _mm256_sub_epi32(y, x);
    u[15] = _mm256_add_epi32(u[15], rnding);
    u[15] = _mm256_srai_epi32(u[15], bit);

    // stage 9
    if (do_cols) {
      out[0] = u[0];
      out[1] = _mm256_sub_epi32(_mm256_setzero_si256(), u[8]);
      out[2] = u[12];
      out[3] = _mm256_sub_epi32(_mm256_setzero_si256(), u[4]);
      out[4] = u[6];
      out[5] = _mm256_sub_epi32(_mm256_setzero_si256(), u[14]);
      out[6] = u[10];
      out[7] = _mm256_sub_epi32(_mm256_setzero_si256(), u[2]);
      out[8] = u[3];
      out[9] = _mm256_sub_epi32(_mm256_setzero_si256(), u[11]);
      out[10] = u[15];
      out[11] = _mm256_sub_epi32(_mm256_setzero_si256(), u[7]);
      out[12] = u[5];
      out[13] = _mm256_sub_epi32(_mm256_setzero_si256(), u[13]);
      out[14] = u[9];
      out[15] = _mm256_sub_epi32(_mm256_setzero_si256(), u[1]);
    } else {
      const int log_range_out = AOMMAX(16, bd + 6);
      const __m256i clamp_lo_out =
          _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      const __m256i clamp_hi_out =
          _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);

      neg_shift_avx2(u[0], u[8], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                     out_shift);
      neg_shift_avx2(u[12], u[4], out + 2, out + 3, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(u[6], u[14], out + 4, out + 5, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(u[10], u[2], out + 6, out + 7, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(u[3], u[11], out + 8, out + 9, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(u[15], u[7], out + 10, out + 11, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(u[5], u[13], out + 12, out + 13, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(u[9], u[1], out + 14, out + 15, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
    }
  }
}

static void iadst16_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                         int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
  const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
  const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
  const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
  const __m256i cospi34 = _mm256_set1_epi32(cospi[34]);
  const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
  const __m256i cospi42 = _mm256_set1_epi32(cospi[42]);
  const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
  const __m256i cospi50 = _mm256_set1_epi32(cospi[50]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi58 = _mm256_set1_epi32(cospi[58]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i u[16], v[16], x, y;

  {
    // stage 0
    // stage 1
    // stage 2
    v[0] = _mm256_mullo_epi32(in[15], cospi2);
    x = _mm256_mullo_epi32(in[0], cospi62);
    v[0] = _mm256_add_epi32(v[0], x);
    v[0] = _mm256_add_epi32(v[0], rnding);
    v[0] = _mm256_srai_epi32(v[0], bit);

    v[1] = _mm256_mullo_epi32(in[15], cospi62);
    x = _mm256_mullo_epi32(in[0], cospi2);
    v[1] = _mm256_sub_epi32(v[1], x);
    v[1] = _mm256_add_epi32(v[1], rnding);
    v[1] = _mm256_srai_epi32(v[1], bit);

    v[2] = _mm256_mullo_epi32(in[13], cospi10);
    x = _mm256_mullo_epi32(in[2], cospi54);
    v[2] = _mm256_add_epi32(v[2], x);
    v[2] = _mm256_add_epi32(v[2], rnding);
    v[2] = _mm256_srai_epi32(v[2], bit);

    v[3] = _mm256_mullo_epi32(in[13], cospi54);
    x = _mm256_mullo_epi32(in[2], cospi10);
    v[3] = _mm256_sub_epi32(v[3], x);
    v[3] = _mm256_add_epi32(v[3], rnding);
    v[3] = _mm256_srai_epi32(v[3], bit);

    v[4] = _mm256_mullo_epi32(in[11], cospi18);
    x = _mm256_mullo_epi32(in[4], cospi46);
    v[4] = _mm256_add_epi32(v[4], x);
    v[4] = _mm256_add_epi32(v[4], rnding);
    v[4] = _mm256_srai_epi32(v[4], bit);

    v[5] = _mm256_mullo_epi32(in[11], cospi46);
    x = _mm256_mullo_epi32(in[4], cospi18);
    v[5] = _mm256_sub_epi32(v[5], x);
    v[5] = _mm256_add_epi32(v[5], rnding);
    v[5] = _mm256_srai_epi32(v[5], bit);

    v[6] = _mm256_mullo_epi32(in[9], cospi26);
    x = _mm256_mullo_epi32(in[6], cospi38);
    v[6] = _mm256_add_epi32(v[6], x);
    v[6] = _mm256_add_epi32(v[6], rnding);
    v[6] = _mm256_srai_epi32(v[6], bit);

    v[7] = _mm256_mullo_epi32(in[9], cospi38);
    x = _mm256_mullo_epi32(in[6], cospi26);
    v[7] = _mm256_sub_epi32(v[7], x);
    v[7] = _mm256_add_epi32(v[7], rnding);
    v[7] = _mm256_srai_epi32(v[7], bit);

    v[8] = _mm256_mullo_epi32(in[7], cospi34);
    x = _mm256_mullo_epi32(in[8], cospi30);
    v[8] = _mm256_add_epi32(v[8], x);
    v[8] = _mm256_add_epi32(v[8], rnding);
    v[8] = _mm256_srai_epi32(v[8], bit);

    v[9] = _mm256_mullo_epi32(in[7], cospi30);
    x = _mm256_mullo_epi32(in[8], cospi34);
    v[9] = _mm256_sub_epi32(v[9], x);
    v[9] = _mm256_add_epi32(v[9], rnding);
    v[9] = _mm256_srai_epi32(v[9], bit);

    v[10] = _mm256_mullo_epi32(in[5], cospi42);
    x = _mm256_mullo_epi32(in[10], cospi22);
    v[10] = _mm256_add_epi32(v[10], x);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[11] = _mm256_mullo_epi32(in[5], cospi22);
    x = _mm256_mullo_epi32(in[10], cospi42);
    v[11] = _mm256_sub_epi32(v[11], x);
    v[11] = _mm256_add_epi32(v[11], rnding);
    v[11] = _mm256_srai_epi32(v[11], bit);

    v[12] = _mm256_mullo_epi32(in[3], cospi50);
    x = _mm256_mullo_epi32(in[12], cospi14);
    v[12] = _mm256_add_epi32(v[12], x);
    v[12] = _mm256_add_epi32(v[12], rnding);
    v[12] = _mm256_srai_epi32(v[12], bit);

    v[13] = _mm256_mullo_epi32(in[3], cospi14);
    x = _mm256_mullo_epi32(in[12], cospi50);
    v[13] = _mm256_sub_epi32(v[13], x);
    v[13] = _mm256_add_epi32(v[13], rnding);
    v[13] = _mm256_srai_epi32(v[13], bit);

    v[14] = _mm256_mullo_epi32(in[1], cospi58);
    x = _mm256_mullo_epi32(in[14], cospi6);
    v[14] = _mm256_add_epi32(v[14], x);
    v[14] = _mm256_add_epi32(v[14], rnding);
    v[14] = _mm256_srai_epi32(v[14], bit);

    v[15] = _mm256_mullo_epi32(in[1], cospi6);
    x = _mm256_mullo_epi32(in[14], cospi58);
    v[15] = _mm256_sub_epi32(v[15], x);
    v[15] = _mm256_add_epi32(v[15], rnding);
    v[15] = _mm256_srai_epi32(v[15], bit);

    // stage 3
    addsub_avx2(v[0], v[8], &u[0], &u[8], &clamp_lo, &clamp_hi);
    addsub_avx2(v[1], v[9], &u[1], &u[9], &clamp_lo, &clamp_hi);
    addsub_avx2(v[2], v[10], &u[2], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(v[3], v[11], &u[3], &u[11], &clamp_lo, &clamp_hi);
    addsub_avx2(v[4], v[12], &u[4], &u[12], &clamp_lo, &clamp_hi);
    addsub_avx2(v[5], v[13], &u[5], &u[13], &clamp_lo, &clamp_hi);
    addsub_avx2(v[6], v[14], &u[6], &u[14], &clamp_lo, &clamp_hi);
    addsub_avx2(v[7], v[15], &u[7], &u[15], &clamp_lo, &clamp_hi);

    // stage 4
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];
    v[4] = u[4];
    v[5] = u[5];
    v[6] = u[6];
    v[7] = u[7];

    v[8] = _mm256_mullo_epi32(u[8], cospi8);
    x = _mm256_mullo_epi32(u[9], cospi56);
    v[8] = _mm256_add_epi32(v[8], x);
    v[8] = _mm256_add_epi32(v[8], rnding);
    v[8] = _mm256_srai_epi32(v[8], bit);

    v[9] = _mm256_mullo_epi32(u[8], cospi56);
    x = _mm256_mullo_epi32(u[9], cospi8);
    v[9] = _mm256_sub_epi32(v[9], x);
    v[9] = _mm256_add_epi32(v[9], rnding);
    v[9] = _mm256_srai_epi32(v[9], bit);

    v[10] = _mm256_mullo_epi32(u[10], cospi40);
    x = _mm256_mullo_epi32(u[11], cospi24);
    v[10] = _mm256_add_epi32(v[10], x);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[11] = _mm256_mullo_epi32(u[10], cospi24);
    x = _mm256_mullo_epi32(u[11], cospi40);
    v[11] = _mm256_sub_epi32(v[11], x);
    v[11] = _mm256_add_epi32(v[11], rnding);
    v[11] = _mm256_srai_epi32(v[11], bit);

    v[12] = _mm256_mullo_epi32(u[12], cospim56);
    x = _mm256_mullo_epi32(u[13], cospi8);
    v[12] = _mm256_add_epi32(v[12], x);
    v[12] = _mm256_add_epi32(v[12], rnding);
    v[12] = _mm256_srai_epi32(v[12], bit);

    v[13] = _mm256_mullo_epi32(u[12], cospi8);
    x = _mm256_mullo_epi32(u[13], cospim56);
    v[13] = _mm256_sub_epi32(v[13], x);
    v[13] = _mm256_add_epi32(v[13], rnding);
    v[13] = _mm256_srai_epi32(v[13], bit);

    v[14] = _mm256_mullo_epi32(u[14], cospim24);
    x = _mm256_mullo_epi32(u[15], cospi40);
    v[14] = _mm256_add_epi32(v[14], x);
    v[14] = _mm256_add_epi32(v[14], rnding);
    v[14] = _mm256_srai_epi32(v[14], bit);

    v[15] = _mm256_mullo_epi32(u[14], cospi40);
    x = _mm256_mullo_epi32(u[15], cospim24);
    v[15] = _mm256_sub_epi32(v[15], x);
    v[15] = _mm256_add_epi32(v[15], rnding);
    v[15] = _mm256_srai_epi32(v[15], bit);

    // stage 5
    addsub_avx2(v[0], v[4], &u[0], &u[4], &clamp_lo, &clamp_hi);
    addsub_avx2(v[1], v[5], &u[1], &u[5], &clamp_lo, &clamp_hi);
    addsub_avx2(v[2], v[6], &u[2], &u[6], &clamp_lo, &clamp_hi);
    addsub_avx2(v[3], v[7], &u[3], &u[7], &clamp_lo, &clamp_hi);
    addsub_avx2(v[8], v[12], &u[8], &u[12], &clamp_lo, &clamp_hi);
    addsub_avx2(v[9], v[13], &u[9], &u[13], &clamp_lo, &clamp_hi);
    addsub_avx2(v[10], v[14], &u[10], &u[14], &clamp_lo, &clamp_hi);
    addsub_avx2(v[11], v[15], &u[11], &u[15], &clamp_lo, &clamp_hi);

    // stage 6
    v[0] = u[0];
    v[1] = u[1];
    v[2] = u[2];
    v[3] = u[3];

    v[4] = _mm256_mullo_epi32(u[4], cospi16);
    x = _mm256_mullo_epi32(u[5], cospi48);
    v[4] = _mm256_add_epi32(v[4], x);
    v[4] = _mm256_add_epi32(v[4], rnding);
    v[4] = _mm256_srai_epi32(v[4], bit);

    v[5] = _mm256_mullo_epi32(u[4], cospi48);
    x = _mm256_mullo_epi32(u[5], cospi16);
    v[5] = _mm256_sub_epi32(v[5], x);
    v[5] = _mm256_add_epi32(v[5], rnding);
    v[5] = _mm256_srai_epi32(v[5], bit);

    v[6] = _mm256_mullo_epi32(u[6], cospim48);
    x = _mm256_mullo_epi32(u[7], cospi16);
    v[6] = _mm256_add_epi32(v[6], x);
    v[6] = _mm256_add_epi32(v[6], rnding);
    v[6] = _mm256_srai_epi32(v[6], bit);

    v[7] = _mm256_mullo_epi32(u[6], cospi16);
    x = _mm256_mullo_epi32(u[7], cospim48);
    v[7] = _mm256_sub_epi32(v[7], x);
    v[7] = _mm256_add_epi32(v[7], rnding);
    v[7] = _mm256_srai_epi32(v[7], bit);

    v[8] = u[8];
    v[9] = u[9];
    v[10] = u[10];
    v[11] = u[11];

    v[12] = _mm256_mullo_epi32(u[12], cospi16);
    x = _mm256_mullo_epi32(u[13], cospi48);
    v[12] = _mm256_add_epi32(v[12], x);
    v[12] = _mm256_add_epi32(v[12], rnding);
    v[12] = _mm256_srai_epi32(v[12], bit);

    v[13] = _mm256_mullo_epi32(u[12], cospi48);
    x = _mm256_mullo_epi32(u[13], cospi16);
    v[13] = _mm256_sub_epi32(v[13], x);
    v[13] = _mm256_add_epi32(v[13], rnding);
    v[13] = _mm256_srai_epi32(v[13], bit);

    v[14] = _mm256_mullo_epi32(u[14], cospim48);
    x = _mm256_mullo_epi32(u[15], cospi16);
    v[14] = _mm256_add_epi32(v[14], x);
    v[14] = _mm256_add_epi32(v[14], rnding);
    v[14] = _mm256_srai_epi32(v[14], bit);

    v[15] = _mm256_mullo_epi32(u[14], cospi16);
    x = _mm256_mullo_epi32(u[15], cospim48);
    v[15] = _mm256_sub_epi32(v[15], x);
    v[15] = _mm256_add_epi32(v[15], rnding);
    v[15] = _mm256_srai_epi32(v[15], bit);

    // stage 7
    addsub_avx2(v[0], v[2], &u[0], &u[2], &clamp_lo, &clamp_hi);
    addsub_avx2(v[1], v[3], &u[1], &u[3], &clamp_lo, &clamp_hi);
    addsub_avx2(v[4], v[6], &u[4], &u[6], &clamp_lo, &clamp_hi);
    addsub_avx2(v[5], v[7], &u[5], &u[7], &clamp_lo, &clamp_hi);
    addsub_avx2(v[8], v[10], &u[8], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(v[9], v[11], &u[9], &u[11], &clamp_lo, &clamp_hi);
    addsub_avx2(v[12], v[14], &u[12], &u[14], &clamp_lo, &clamp_hi);
    addsub_avx2(v[13], v[15], &u[13], &u[15], &clamp_lo, &clamp_hi);

    // stage 8
    v[0] = u[0];
    v[1] = u[1];

    y = _mm256_mullo_epi32(u[2], cospi32);
    x = _mm256_mullo_epi32(u[3], cospi32);
    v[2] = _mm256_add_epi32(y, x);
    v[2] = _mm256_add_epi32(v[2], rnding);
    v[2] = _mm256_srai_epi32(v[2], bit);

    v[3] = _mm256_sub_epi32(y, x);
    v[3] = _mm256_add_epi32(v[3], rnding);
    v[3] = _mm256_srai_epi32(v[3], bit);

    v[4] = u[4];
    v[5] = u[5];

    y = _mm256_mullo_epi32(u[6], cospi32);
    x = _mm256_mullo_epi32(u[7], cospi32);
    v[6] = _mm256_add_epi32(y, x);
    v[6] = _mm256_add_epi32(v[6], rnding);
    v[6] = _mm256_srai_epi32(v[6], bit);

    v[7] = _mm256_sub_epi32(y, x);
    v[7] = _mm256_add_epi32(v[7], rnding);
    v[7] = _mm256_srai_epi32(v[7], bit);

    v[8] = u[8];
    v[9] = u[9];

    y = _mm256_mullo_epi32(u[10], cospi32);
    x = _mm256_mullo_epi32(u[11], cospi32);
    v[10] = _mm256_add_epi32(y, x);
    v[10] = _mm256_add_epi32(v[10], rnding);
    v[10] = _mm256_srai_epi32(v[10], bit);

    v[11] = _mm256_sub_epi32(y, x);
    v[11] = _mm256_add_epi32(v[11], rnding);
    v[11] = _mm256_srai_epi32(v[11], bit);

    v[12] = u[12];
    v[13] = u[13];

    y = _mm256_mullo_epi32(u[14], cospi32);
    x = _mm256_mullo_epi32(u[15], cospi32);
    v[14] = _mm256_add_epi32(y, x);
    v[14] = _mm256_add_epi32(v[14], rnding);
    v[14] = _mm256_srai_epi32(v[14], bit);

    v[15] = _mm256_sub_epi32(y, x);
    v[15] = _mm256_add_epi32(v[15], rnding);
    v[15] = _mm256_srai_epi32(v[15], bit);

    // stage 9
    if (do_cols) {
      out[0] = v[0];
      out[1] = _mm256_sub_epi32(_mm256_setzero_si256(), v[8]);
      out[2] = v[12];
      out[3] = _mm256_sub_epi32(_mm256_setzero_si256(), v[4]);
      out[4] = v[6];
      out[5] = _mm256_sub_epi32(_mm256_setzero_si256(), v[14]);
      out[6] = v[10];
      out[7] = _mm256_sub_epi32(_mm256_setzero_si256(), v[2]);
      out[8] = v[3];
      out[9] = _mm256_sub_epi32(_mm256_setzero_si256(), v[11]);
      out[10] = v[15];
      out[11] = _mm256_sub_epi32(_mm256_setzero_si256(), v[7]);
      out[12] = v[5];
      out[13] = _mm256_sub_epi32(_mm256_setzero_si256(), v[13]);
      out[14] = v[9];
      out[15] = _mm256_sub_epi32(_mm256_setzero_si256(), v[1]);
    } else {
      const int log_range_out = AOMMAX(16, bd + 6);
      const __m256i clamp_lo_out =
          _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      const __m256i clamp_hi_out =
          _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);

      neg_shift_avx2(v[0], v[8], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                     out_shift);
      neg_shift_avx2(v[12], v[4], out + 2, out + 3, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[6], v[14], out + 4, out + 5, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[10], v[2], out + 6, out + 7, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[3], v[11], out + 8, out + 9, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[15], v[7], out + 10, out + 11, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[5], v[13], out + 12, out + 13, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
      neg_shift_avx2(v[9], v[1], out + 14, out + 15, &clamp_lo_out,
                     &clamp_hi_out, out_shift);
    }
  }
}
static void idct8x8_low1_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                              int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i x;

  // stage 0
  // stage 1
  // stage 2
  // stage 3
  x = _mm256_mullo_epi32(in[0], cospi32);
  x = _mm256_add_epi32(x, rnding);
  x = _mm256_srai_epi32(x, bit);

  // stage 4
  // stage 5
  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    __m256i offset = _mm256_set1_epi32((1 << out_shift) >> 1);
    clamp_lo = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
    clamp_hi = _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);
    x = _mm256_add_epi32(x, offset);
    x = _mm256_sra_epi32(x, _mm_cvtsi32_si128(out_shift));
  }
  x = _mm256_max_epi32(x, clamp_lo);
  x = _mm256_min_epi32(x, clamp_hi);
  out[0] = x;
  out[1] = x;
  out[2] = x;
  out[3] = x;
  out[4] = x;
  out[5] = x;
  out[6] = x;
  out[7] = x;
}
static void idct8x8_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                         int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i u0, u1, u2, u3, u4, u5, u6, u7;
  __m256i v0, v1, v2, v3, v4, v5, v6, v7;
  __m256i x, y;

  // stage 0
  // stage 1
  // stage 2
  u0 = in[0];
  u1 = in[4];
  u2 = in[2];
  u3 = in[6];

  x = _mm256_mullo_epi32(in[1], cospi56);
  y = _mm256_mullo_epi32(in[7], cospim8);
  u4 = _mm256_add_epi32(x, y);
  u4 = _mm256_add_epi32(u4, rnding);
  u4 = _mm256_srai_epi32(u4, bit);

  x = _mm256_mullo_epi32(in[1], cospi8);
  y = _mm256_mullo_epi32(in[7], cospi56);
  u7 = _mm256_add_epi32(x, y);
  u7 = _mm256_add_epi32(u7, rnding);
  u7 = _mm256_srai_epi32(u7, bit);

  x = _mm256_mullo_epi32(in[5], cospi24);
  y = _mm256_mullo_epi32(in[3], cospim40);
  u5 = _mm256_add_epi32(x, y);
  u5 = _mm256_add_epi32(u5, rnding);
  u5 = _mm256_srai_epi32(u5, bit);

  x = _mm256_mullo_epi32(in[5], cospi40);
  y = _mm256_mullo_epi32(in[3], cospi24);
  u6 = _mm256_add_epi32(x, y);
  u6 = _mm256_add_epi32(u6, rnding);
  u6 = _mm256_srai_epi32(u6, bit);

  // stage 3
  x = _mm256_mullo_epi32(u0, cospi32);
  y = _mm256_mullo_epi32(u1, cospi32);
  v0 = _mm256_add_epi32(x, y);
  v0 = _mm256_add_epi32(v0, rnding);
  v0 = _mm256_srai_epi32(v0, bit);

  v1 = _mm256_sub_epi32(x, y);
  v1 = _mm256_add_epi32(v1, rnding);
  v1 = _mm256_srai_epi32(v1, bit);

  x = _mm256_mullo_epi32(u2, cospi48);
  y = _mm256_mullo_epi32(u3, cospim16);
  v2 = _mm256_add_epi32(x, y);
  v2 = _mm256_add_epi32(v2, rnding);
  v2 = _mm256_srai_epi32(v2, bit);

  x = _mm256_mullo_epi32(u2, cospi16);
  y = _mm256_mullo_epi32(u3, cospi48);
  v3 = _mm256_add_epi32(x, y);
  v3 = _mm256_add_epi32(v3, rnding);
  v3 = _mm256_srai_epi32(v3, bit);

  addsub_avx2(u4, u5, &v4, &v5, &clamp_lo, &clamp_hi);
  addsub_avx2(u7, u6, &v7, &v6, &clamp_lo, &clamp_hi);

  // stage 4
  addsub_avx2(v0, v3, &u0, &u3, &clamp_lo, &clamp_hi);
  addsub_avx2(v1, v2, &u1, &u2, &clamp_lo, &clamp_hi);
  u4 = v4;
  u7 = v7;

  x = _mm256_mullo_epi32(v5, cospi32);
  y = _mm256_mullo_epi32(v6, cospi32);
  u6 = _mm256_add_epi32(y, x);
  u6 = _mm256_add_epi32(u6, rnding);
  u6 = _mm256_srai_epi32(u6, bit);

  u5 = _mm256_sub_epi32(y, x);
  u5 = _mm256_add_epi32(u5, rnding);
  u5 = _mm256_srai_epi32(u5, bit);

  addsub_avx2(u0, u7, out + 0, out + 7, &clamp_lo, &clamp_hi);
  addsub_avx2(u1, u6, out + 1, out + 6, &clamp_lo, &clamp_hi);
  addsub_avx2(u2, u5, out + 2, out + 5, &clamp_lo, &clamp_hi);
  addsub_avx2(u3, u4, out + 3, out + 4, &clamp_lo, &clamp_hi);
  // stage 5
  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const __m256i clamp_lo_out = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
    const __m256i clamp_hi_out =
        _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);

    round_shift_4x4_avx2(out, out_shift);
    round_shift_4x4_avx2(out + 4, out_shift);
    highbd_clamp_epi32_avx2(out, out, &clamp_lo_out, &clamp_hi_out, 8);
  }
}
static void iadst8x8_low1_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                               int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const __m256i kZero = _mm256_setzero_si256();
  __m256i u[8], x;

  // stage 0
  // stage 1
  // stage 2

  x = _mm256_mullo_epi32(in[0], cospi60);
  u[0] = _mm256_add_epi32(x, rnding);
  u[0] = _mm256_srai_epi32(u[0], bit);

  x = _mm256_mullo_epi32(in[0], cospi4);
  u[1] = _mm256_sub_epi32(kZero, x);
  u[1] = _mm256_add_epi32(u[1], rnding);
  u[1] = _mm256_srai_epi32(u[1], bit);

  // stage 3
  // stage 4
  __m256i temp1, temp2;
  temp1 = _mm256_mullo_epi32(u[0], cospi16);
  x = _mm256_mullo_epi32(u[1], cospi48);
  temp1 = _mm256_add_epi32(temp1, x);
  temp1 = _mm256_add_epi32(temp1, rnding);
  temp1 = _mm256_srai_epi32(temp1, bit);
  u[4] = temp1;

  temp2 = _mm256_mullo_epi32(u[0], cospi48);
  x = _mm256_mullo_epi32(u[1], cospi16);
  u[5] = _mm256_sub_epi32(temp2, x);
  u[5] = _mm256_add_epi32(u[5], rnding);
  u[5] = _mm256_srai_epi32(u[5], bit);

  // stage 5
  // stage 6
  temp1 = _mm256_mullo_epi32(u[0], cospi32);
  x = _mm256_mullo_epi32(u[1], cospi32);
  u[2] = _mm256_add_epi32(temp1, x);
  u[2] = _mm256_add_epi32(u[2], rnding);
  u[2] = _mm256_srai_epi32(u[2], bit);

  u[3] = _mm256_sub_epi32(temp1, x);
  u[3] = _mm256_add_epi32(u[3], rnding);
  u[3] = _mm256_srai_epi32(u[3], bit);

  temp1 = _mm256_mullo_epi32(u[4], cospi32);
  x = _mm256_mullo_epi32(u[5], cospi32);
  u[6] = _mm256_add_epi32(temp1, x);
  u[6] = _mm256_add_epi32(u[6], rnding);
  u[6] = _mm256_srai_epi32(u[6], bit);

  u[7] = _mm256_sub_epi32(temp1, x);
  u[7] = _mm256_add_epi32(u[7], rnding);
  u[7] = _mm256_srai_epi32(u[7], bit);

  // stage 7
  if (do_cols) {
    out[0] = u[0];
    out[1] = _mm256_sub_epi32(kZero, u[4]);
    out[2] = u[6];
    out[3] = _mm256_sub_epi32(kZero, u[2]);
    out[4] = u[3];
    out[5] = _mm256_sub_epi32(kZero, u[7]);
    out[6] = u[5];
    out[7] = _mm256_sub_epi32(kZero, u[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const __m256i clamp_lo_out = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
    const __m256i clamp_hi_out =
        _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);

    neg_shift_avx2(u[0], u[4], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                   out_shift);
    neg_shift_avx2(u[6], u[2], out + 2, out + 3, &clamp_lo_out, &clamp_hi_out,
                   out_shift);
    neg_shift_avx2(u[3], u[7], out + 4, out + 5, &clamp_lo_out, &clamp_hi_out,
                   out_shift);
    neg_shift_avx2(u[5], u[1], out + 6, out + 7, &clamp_lo_out, &clamp_hi_out,
                   out_shift);
  }
}

static void iadst8x8_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                          int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const __m256i kZero = _mm256_setzero_si256();
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);
  __m256i u[8], v[8], x;

  // stage 0
  // stage 1
  // stage 2

  u[0] = _mm256_mullo_epi32(in[7], cospi4);
  x = _mm256_mullo_epi32(in[0], cospi60);
  u[0] = _mm256_add_epi32(u[0], x);
  u[0] = _mm256_add_epi32(u[0], rnding);
  u[0] = _mm256_srai_epi32(u[0], bit);

  u[1] = _mm256_mullo_epi32(in[7], cospi60);
  x = _mm256_mullo_epi32(in[0], cospi4);
  u[1] = _mm256_sub_epi32(u[1], x);
  u[1] = _mm256_add_epi32(u[1], rnding);
  u[1] = _mm256_srai_epi32(u[1], bit);

  u[2] = _mm256_mullo_epi32(in[5], cospi20);
  x = _mm256_mullo_epi32(in[2], cospi44);
  u[2] = _mm256_add_epi32(u[2], x);
  u[2] = _mm256_add_epi32(u[2], rnding);
  u[2] = _mm256_srai_epi32(u[2], bit);

  u[3] = _mm256_mullo_epi32(in[5], cospi44);
  x = _mm256_mullo_epi32(in[2], cospi20);
  u[3] = _mm256_sub_epi32(u[3], x);
  u[3] = _mm256_add_epi32(u[3], rnding);
  u[3] = _mm256_srai_epi32(u[3], bit);

  u[4] = _mm256_mullo_epi32(in[3], cospi36);
  x = _mm256_mullo_epi32(in[4], cospi28);
  u[4] = _mm256_add_epi32(u[4], x);
  u[4] = _mm256_add_epi32(u[4], rnding);
  u[4] = _mm256_srai_epi32(u[4], bit);

  u[5] = _mm256_mullo_epi32(in[3], cospi28);
  x = _mm256_mullo_epi32(in[4], cospi36);
  u[5] = _mm256_sub_epi32(u[5], x);
  u[5] = _mm256_add_epi32(u[5], rnding);
  u[5] = _mm256_srai_epi32(u[5], bit);

  u[6] = _mm256_mullo_epi32(in[1], cospi52);
  x = _mm256_mullo_epi32(in[6], cospi12);
  u[6] = _mm256_add_epi32(u[6], x);
  u[6] = _mm256_add_epi32(u[6], rnding);
  u[6] = _mm256_srai_epi32(u[6], bit);

  u[7] = _mm256_mullo_epi32(in[1], cospi12);
  x = _mm256_mullo_epi32(in[6], cospi52);
  u[7] = _mm256_sub_epi32(u[7], x);
  u[7] = _mm256_add_epi32(u[7], rnding);
  u[7] = _mm256_srai_epi32(u[7], bit);

  // stage 3
  addsub_avx2(u[0], u[4], &v[0], &v[4], &clamp_lo, &clamp_hi);
  addsub_avx2(u[1], u[5], &v[1], &v[5], &clamp_lo, &clamp_hi);
  addsub_avx2(u[2], u[6], &v[2], &v[6], &clamp_lo, &clamp_hi);
  addsub_avx2(u[3], u[7], &v[3], &v[7], &clamp_lo, &clamp_hi);

  // stage 4
  u[0] = v[0];
  u[1] = v[1];
  u[2] = v[2];
  u[3] = v[3];

  u[4] = _mm256_mullo_epi32(v[4], cospi16);
  x = _mm256_mullo_epi32(v[5], cospi48);
  u[4] = _mm256_add_epi32(u[4], x);
  u[4] = _mm256_add_epi32(u[4], rnding);
  u[4] = _mm256_srai_epi32(u[4], bit);

  u[5] = _mm256_mullo_epi32(v[4], cospi48);
  x = _mm256_mullo_epi32(v[5], cospi16);
  u[5] = _mm256_sub_epi32(u[5], x);
  u[5] = _mm256_add_epi32(u[5], rnding);
  u[5] = _mm256_srai_epi32(u[5], bit);

  u[6] = _mm256_mullo_epi32(v[6], cospim48);
  x = _mm256_mullo_epi32(v[7], cospi16);
  u[6] = _mm256_add_epi32(u[6], x);
  u[6] = _mm256_add_epi32(u[6], rnding);
  u[6] = _mm256_srai_epi32(u[6], bit);

  u[7] = _mm256_mullo_epi32(v[6], cospi16);
  x = _mm256_mullo_epi32(v[7], cospim48);
  u[7] = _mm256_sub_epi32(u[7], x);
  u[7] = _mm256_add_epi32(u[7], rnding);
  u[7] = _mm256_srai_epi32(u[7], bit);

  // stage 5
  addsub_avx2(u[0], u[2], &v[0], &v[2], &clamp_lo, &clamp_hi);
  addsub_avx2(u[1], u[3], &v[1], &v[3], &clamp_lo, &clamp_hi);
  addsub_avx2(u[4], u[6], &v[4], &v[6], &clamp_lo, &clamp_hi);
  addsub_avx2(u[5], u[7], &v[5], &v[7], &clamp_lo, &clamp_hi);

  // stage 6
  u[0] = v[0];
  u[1] = v[1];
  u[4] = v[4];
  u[5] = v[5];

  v[0] = _mm256_mullo_epi32(v[2], cospi32);
  x = _mm256_mullo_epi32(v[3], cospi32);
  u[2] = _mm256_add_epi32(v[0], x);
  u[2] = _mm256_add_epi32(u[2], rnding);
  u[2] = _mm256_srai_epi32(u[2], bit);

  u[3] = _mm256_sub_epi32(v[0], x);
  u[3] = _mm256_add_epi32(u[3], rnding);
  u[3] = _mm256_srai_epi32(u[3], bit);

  v[0] = _mm256_mullo_epi32(v[6], cospi32);
  x = _mm256_mullo_epi32(v[7], cospi32);
  u[6] = _mm256_add_epi32(v[0], x);
  u[6] = _mm256_add_epi32(u[6], rnding);
  u[6] = _mm256_srai_epi32(u[6], bit);

  u[7] = _mm256_sub_epi32(v[0], x);
  u[7] = _mm256_add_epi32(u[7], rnding);
  u[7] = _mm256_srai_epi32(u[7], bit);

  // stage 7
  if (do_cols) {
    out[0] = u[0];
    out[1] = _mm256_sub_epi32(kZero, u[4]);
    out[2] = u[6];
    out[3] = _mm256_sub_epi32(kZero, u[2]);
    out[4] = u[3];
    out[5] = _mm256_sub_epi32(kZero, u[7]);
    out[6] = u[5];
    out[7] = _mm256_sub_epi32(kZero, u[1]);
  } else {
    const int log_range_out = AOMMAX(16, bd + 6);
    const __m256i clamp_lo_out = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
    const __m256i clamp_hi_out =
        _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);

    neg_shift_avx2(u[0], u[4], out + 0, out + 1, &clamp_lo_out, &clamp_hi_out,
                   out_shift);
    neg_shift_avx2(u[6], u[2], out + 2, out + 3, &clamp_lo_out, &clamp_hi_out,
                   out_shift);
    neg_shift_avx2(u[3], u[7], out + 4, out + 5, &clamp_lo_out, &clamp_hi_out,
                   out_shift);
    neg_shift_avx2(u[5], u[1], out + 6, out + 7, &clamp_lo_out, &clamp_hi_out,
                   out_shift);
  }
}
static inline void idct64_stage8_avx2(
    __m256i *u, const __m256i *cospim32, const __m256i *cospi32,
    const __m256i *cospim16, const __m256i *cospi48, const __m256i *cospi16,
    const __m256i *cospim48, const __m256i *clamp_lo, const __m256i *clamp_hi,
    const __m256i *rnding, int bit) {
  int i;
  __m256i temp1, temp2, temp3, temp4;
  temp1 = half_btf_avx2(cospim32, &u[10], cospi32, &u[13], rnding, bit);
  u[13] = half_btf_avx2(cospi32, &u[10], cospi32, &u[13], rnding, bit);
  u[10] = temp1;
  temp2 = half_btf_avx2(cospim32, &u[11], cospi32, &u[12], rnding, bit);
  u[12] = half_btf_avx2(cospi32, &u[11], cospi32, &u[12], rnding, bit);
  u[11] = temp2;

  for (i = 16; i < 20; ++i) {
    addsub_avx2(u[i], u[i ^ 7], &u[i], &u[i ^ 7], clamp_lo, clamp_hi);
    addsub_avx2(u[i ^ 15], u[i ^ 8], &u[i ^ 15], &u[i ^ 8], clamp_lo, clamp_hi);
  }

  temp1 = half_btf_avx2(cospim16, &u[36], cospi48, &u[59], rnding, bit);
  temp2 = half_btf_avx2(cospim16, &u[37], cospi48, &u[58], rnding, bit);
  temp3 = half_btf_avx2(cospim16, &u[38], cospi48, &u[57], rnding, bit);
  temp4 = half_btf_avx2(cospim16, &u[39], cospi48, &u[56], rnding, bit);
  u[56] = half_btf_avx2(cospi48, &u[39], cospi16, &u[56], rnding, bit);
  u[57] = half_btf_avx2(cospi48, &u[38], cospi16, &u[57], rnding, bit);
  u[58] = half_btf_avx2(cospi48, &u[37], cospi16, &u[58], rnding, bit);
  u[59] = half_btf_avx2(cospi48, &u[36], cospi16, &u[59], rnding, bit);
  u[36] = temp1;
  u[37] = temp2;
  u[38] = temp3;
  u[39] = temp4;

  temp1 = half_btf_avx2(cospim48, &u[40], cospim16, &u[55], rnding, bit);
  temp2 = half_btf_avx2(cospim48, &u[41], cospim16, &u[54], rnding, bit);
  temp3 = half_btf_avx2(cospim48, &u[42], cospim16, &u[53], rnding, bit);
  temp4 = half_btf_avx2(cospim48, &u[43], cospim16, &u[52], rnding, bit);
  u[52] = half_btf_avx2(cospim16, &u[43], cospi48, &u[52], rnding, bit);
  u[53] = half_btf_avx2(cospim16, &u[42], cospi48, &u[53], rnding, bit);
  u[54] = half_btf_avx2(cospim16, &u[41], cospi48, &u[54], rnding, bit);
  u[55] = half_btf_avx2(cospim16, &u[40], cospi48, &u[55], rnding, bit);
  u[40] = temp1;
  u[41] = temp2;
  u[42] = temp3;
  u[43] = temp4;
}

static inline void idct64_stage9_avx2(__m256i *u, const __m256i *cospim32,
                                      const __m256i *cospi32,
                                      const __m256i *clamp_lo,
                                      const __m256i *clamp_hi,
                                      const __m256i *rnding, int bit) {
  int i;
  __m256i temp1, temp2, temp3, temp4;
  for (i = 0; i < 8; ++i) {
    addsub_avx2(u[i], u[15 - i], &u[i], &u[15 - i], clamp_lo, clamp_hi);
  }

  temp1 = half_btf_avx2(cospim32, &u[20], cospi32, &u[27], rnding, bit);
  temp2 = half_btf_avx2(cospim32, &u[21], cospi32, &u[26], rnding, bit);
  temp3 = half_btf_avx2(cospim32, &u[22], cospi32, &u[25], rnding, bit);
  temp4 = half_btf_avx2(cospim32, &u[23], cospi32, &u[24], rnding, bit);
  u[24] = half_btf_avx2(cospi32, &u[23], cospi32, &u[24], rnding, bit);
  u[25] = half_btf_avx2(cospi32, &u[22], cospi32, &u[25], rnding, bit);
  u[26] = half_btf_avx2(cospi32, &u[21], cospi32, &u[26], rnding, bit);
  u[27] = half_btf_avx2(cospi32, &u[20], cospi32, &u[27], rnding, bit);
  u[20] = temp1;
  u[21] = temp2;
  u[22] = temp3;
  u[23] = temp4;
  for (i = 32; i < 40; i++) {
    addsub_avx2(u[i], u[i ^ 15], &u[i], &u[i ^ 15], clamp_lo, clamp_hi);
  }

  for (i = 48; i < 56; i++) {
    addsub_avx2(u[i ^ 15], u[i], &u[i ^ 15], &u[i], clamp_lo, clamp_hi);
  }
}

static inline void idct64_stage10_avx2(__m256i *u, const __m256i *cospim32,
                                       const __m256i *cospi32,
                                       const __m256i *clamp_lo,
                                       const __m256i *clamp_hi,
                                       const __m256i *rnding, int bit) {
  __m256i temp1, temp2, temp3, temp4;
  for (int i = 0; i < 16; i++) {
    addsub_avx2(u[i], u[31 - i], &u[i], &u[31 - i], clamp_lo, clamp_hi);
  }

  temp1 = half_btf_avx2(cospim32, &u[40], cospi32, &u[55], rnding, bit);
  temp2 = half_btf_avx2(cospim32, &u[41], cospi32, &u[54], rnding, bit);
  temp3 = half_btf_avx2(cospim32, &u[42], cospi32, &u[53], rnding, bit);
  temp4 = half_btf_avx2(cospim32, &u[43], cospi32, &u[52], rnding, bit);
  u[52] = half_btf_avx2(cospi32, &u[43], cospi32, &u[52], rnding, bit);
  u[53] = half_btf_avx2(cospi32, &u[42], cospi32, &u[53], rnding, bit);
  u[54] = half_btf_avx2(cospi32, &u[41], cospi32, &u[54], rnding, bit);
  u[55] = half_btf_avx2(cospi32, &u[40], cospi32, &u[55], rnding, bit);
  u[40] = temp1;
  u[41] = temp2;
  u[42] = temp3;
  u[43] = temp4;

  temp1 = half_btf_avx2(cospim32, &u[44], cospi32, &u[51], rnding, bit);
  temp2 = half_btf_avx2(cospim32, &u[45], cospi32, &u[50], rnding, bit);
  temp3 = half_btf_avx2(cospim32, &u[46], cospi32, &u[49], rnding, bit);
  temp4 = half_btf_avx2(cospim32, &u[47], cospi32, &u[48], rnding, bit);
  u[48] = half_btf_avx2(cospi32, &u[47], cospi32, &u[48], rnding, bit);
  u[49] = half_btf_avx2(cospi32, &u[46], cospi32, &u[49], rnding, bit);
  u[50] = half_btf_avx2(cospi32, &u[45], cospi32, &u[50], rnding, bit);
  u[51] = half_btf_avx2(cospi32, &u[44], cospi32, &u[51], rnding, bit);
  u[44] = temp1;
  u[45] = temp2;
  u[46] = temp3;
  u[47] = temp4;
}

static inline void idct64_stage11_avx2(__m256i *u, __m256i *out, int do_cols,
                                       int bd, int out_shift,
                                       const __m256i *clamp_lo,
                                       const __m256i *clamp_hi) {
  for (int i = 0; i < 32; i++) {
    addsub_avx2(u[i], u[63 - i], &out[(i)], &out[(63 - i)], clamp_lo, clamp_hi);
  }

  if (!do_cols) {
    const int log_range_out = AOMMAX(16, bd + 6);
    const __m256i clamp_lo_out = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
    const __m256i clamp_hi_out =
        _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);

    round_shift_8x8_avx2(out, out_shift);
    round_shift_8x8_avx2(out + 16, out_shift);
    round_shift_8x8_avx2(out + 32, out_shift);
    round_shift_8x8_avx2(out + 48, out_shift);
    highbd_clamp_epi32_avx2(out, out, &clamp_lo_out, &clamp_hi_out, 64);
  }
}

static void idct64_low1_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                             int bd, int out_shift) {
  const int32_t *cospi = cospi_arr(bit);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);

  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);

  {
    __m256i x;

    // stage 1
    // stage 2
    // stage 3
    // stage 4
    // stage 5
    // stage 6
    x = half_btf_0_avx2(&cospi32, &in[0], &rnding, bit);

    // stage 8
    // stage 9
    // stage 10
    // stage 11
    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      clamp_lo = _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      clamp_hi = _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);
      if (out_shift != 0) {
        __m256i offset = _mm256_set1_epi32((1 << out_shift) >> 1);
        x = _mm256_add_epi32(x, offset);
        x = _mm256_sra_epi32(x, _mm_cvtsi32_si128(out_shift));
      }
    }
    x = _mm256_max_epi32(x, clamp_lo);
    x = _mm256_min_epi32(x, clamp_hi);
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
static void idct64_low8_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                             int bd, int out_shift) {
  int i, j;
  const int32_t *cospi = cospi_arr(bit);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);

  const __m256i cospi1 = _mm256_set1_epi32(cospi[1]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospi3 = _mm256_set1_epi32(cospi[3]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospim4 = _mm256_set1_epi32(-cospi[4]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospim12 = _mm256_set1_epi32(-cospi[12]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospim20 = _mm256_set1_epi32(-cospi[20]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospim28 = _mm256_set1_epi32(-cospi[28]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospi63 = _mm256_set1_epi32(cospi[63]);
  const __m256i cospim57 = _mm256_set1_epi32(-cospi[57]);
  const __m256i cospi7 = _mm256_set1_epi32(cospi[7]);
  const __m256i cospi5 = _mm256_set1_epi32(cospi[5]);
  const __m256i cospi59 = _mm256_set1_epi32(cospi[59]);
  const __m256i cospim61 = _mm256_set1_epi32(-cospi[61]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);

  {
    __m256i u[64];

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
    u[63] = half_btf_0_avx2(&cospi1, &u[32], &rnding, bit);
    u[32] = half_btf_0_avx2(&cospi63, &u[32], &rnding, bit);
    u[39] = half_btf_0_avx2(&cospim57, &u[56], &rnding, bit);
    u[56] = half_btf_0_avx2(&cospi7, &u[56], &rnding, bit);
    u[55] = half_btf_0_avx2(&cospi5, &u[40], &rnding, bit);
    u[40] = half_btf_0_avx2(&cospi59, &u[40], &rnding, bit);
    u[47] = half_btf_0_avx2(&cospim61, &u[48], &rnding, bit);
    u[48] = half_btf_0_avx2(&cospi3, &u[48], &rnding, bit);

    // stage 3
    u[31] = half_btf_0_avx2(&cospi2, &u[16], &rnding, bit);
    u[16] = half_btf_0_avx2(&cospi62, &u[16], &rnding, bit);
    u[23] = half_btf_0_avx2(&cospim58, &u[24], &rnding, bit);
    u[24] = half_btf_0_avx2(&cospi6, &u[24], &rnding, bit);
    u[33] = u[32];
    u[38] = u[39];
    u[41] = u[40];
    u[46] = u[47];
    u[49] = u[48];
    u[54] = u[55];
    u[57] = u[56];
    u[62] = u[63];

    // stage 4
    __m256i temp1, temp2;
    u[15] = half_btf_0_avx2(&cospi4, &u[8], &rnding, bit);
    u[8] = half_btf_0_avx2(&cospi60, &u[8], &rnding, bit);
    u[17] = u[16];
    u[22] = u[23];
    u[25] = u[24];
    u[30] = u[31];

    temp1 = half_btf_avx2(&cospim4, &u[33], &cospi60, &u[62], &rnding, bit);
    u[62] = half_btf_avx2(&cospi60, &u[33], &cospi4, &u[62], &rnding, bit);
    u[33] = temp1;

    temp2 = half_btf_avx2(&cospim36, &u[38], &cospi28, &u[57], &rnding, bit);
    u[38] = half_btf_avx2(&cospim28, &u[38], &cospim36, &u[57], &rnding, bit);
    u[57] = temp2;

    temp1 = half_btf_avx2(&cospim20, &u[41], &cospi44, &u[54], &rnding, bit);
    u[54] = half_btf_avx2(&cospi44, &u[41], &cospi20, &u[54], &rnding, bit);
    u[41] = temp1;

    temp2 = half_btf_avx2(&cospim12, &u[46], &cospim52, &u[49], &rnding, bit);
    u[49] = half_btf_avx2(&cospim52, &u[46], &cospi12, &u[49], &rnding, bit);
    u[46] = temp2;

    // stage 5
    u[9] = u[8];
    u[14] = u[15];

    temp1 = half_btf_avx2(&cospim8, &u[17], &cospi56, &u[30], &rnding, bit);
    u[30] = half_btf_avx2(&cospi56, &u[17], &cospi8, &u[30], &rnding, bit);
    u[17] = temp1;

    temp2 = half_btf_avx2(&cospim24, &u[22], &cospim40, &u[25], &rnding, bit);
    u[25] = half_btf_avx2(&cospim40, &u[22], &cospi24, &u[25], &rnding, bit);
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
    temp1 = half_btf_0_avx2(&cospi32, &u[0], &rnding, bit);
    u[1] = half_btf_0_avx2(&cospi32, &u[0], &rnding, bit);
    u[0] = temp1;

    temp2 = half_btf_avx2(&cospim16, &u[9], &cospi48, &u[14], &rnding, bit);
    u[14] = half_btf_avx2(&cospi48, &u[9], &cospi16, &u[14], &rnding, bit);
    u[9] = temp2;
    u[19] = u[16];
    u[18] = u[17];
    u[20] = u[23];
    u[21] = u[22];
    u[27] = u[24];
    u[26] = u[25];
    u[28] = u[31];
    u[29] = u[30];

    temp1 = half_btf_avx2(&cospim8, &u[34], &cospi56, &u[61], &rnding, bit);
    u[61] = half_btf_avx2(&cospi56, &u[34], &cospi8, &u[61], &rnding, bit);
    u[34] = temp1;
    temp2 = half_btf_avx2(&cospim8, &u[35], &cospi56, &u[60], &rnding, bit);
    u[60] = half_btf_avx2(&cospi56, &u[35], &cospi8, &u[60], &rnding, bit);
    u[35] = temp2;
    temp1 = half_btf_avx2(&cospim56, &u[36], &cospim8, &u[59], &rnding, bit);
    u[59] = half_btf_avx2(&cospim8, &u[36], &cospi56, &u[59], &rnding, bit);
    u[36] = temp1;
    temp2 = half_btf_avx2(&cospim56, &u[37], &cospim8, &u[58], &rnding, bit);
    u[58] = half_btf_avx2(&cospim8, &u[37], &cospi56, &u[58], &rnding, bit);
    u[37] = temp2;
    temp1 = half_btf_avx2(&cospim40, &u[42], &cospi24, &u[53], &rnding, bit);
    u[53] = half_btf_avx2(&cospi24, &u[42], &cospi40, &u[53], &rnding, bit);
    u[42] = temp1;
    temp2 = half_btf_avx2(&cospim40, &u[43], &cospi24, &u[52], &rnding, bit);
    u[52] = half_btf_avx2(&cospi24, &u[43], &cospi40, &u[52], &rnding, bit);
    u[43] = temp2;
    temp1 = half_btf_avx2(&cospim24, &u[44], &cospim40, &u[51], &rnding, bit);
    u[51] = half_btf_avx2(&cospim40, &u[44], &cospi24, &u[51], &rnding, bit);
    u[44] = temp1;
    temp2 = half_btf_avx2(&cospim24, &u[45], &cospim40, &u[50], &rnding, bit);
    u[50] = half_btf_avx2(&cospim40, &u[45], &cospi24, &u[50], &rnding, bit);
    u[45] = temp2;

    // stage 7
    u[3] = u[0];
    u[2] = u[1];
    u[11] = u[8];
    u[10] = u[9];
    u[12] = u[15];
    u[13] = u[14];

    temp1 = half_btf_avx2(&cospim16, &u[18], &cospi48, &u[29], &rnding, bit);
    u[29] = half_btf_avx2(&cospi48, &u[18], &cospi16, &u[29], &rnding, bit);
    u[18] = temp1;
    temp2 = half_btf_avx2(&cospim16, &u[19], &cospi48, &u[28], &rnding, bit);
    u[28] = half_btf_avx2(&cospi48, &u[19], &cospi16, &u[28], &rnding, bit);
    u[19] = temp2;
    temp1 = half_btf_avx2(&cospim48, &u[20], &cospim16, &u[27], &rnding, bit);
    u[27] = half_btf_avx2(&cospim16, &u[20], &cospi48, &u[27], &rnding, bit);
    u[20] = temp1;
    temp2 = half_btf_avx2(&cospim48, &u[21], &cospim16, &u[26], &rnding, bit);
    u[26] = half_btf_avx2(&cospim16, &u[21], &cospi48, &u[26], &rnding, bit);
    u[21] = temp2;
    for (i = 32; i < 64; i += 16) {
      for (j = i; j < i + 4; j++) {
        addsub_avx2(u[j], u[j ^ 7], &u[j], &u[j ^ 7], &clamp_lo, &clamp_hi);
        addsub_avx2(u[j ^ 15], u[j ^ 8], &u[j ^ 15], &u[j ^ 8], &clamp_lo,
                    &clamp_hi);
      }
    }

    // stage 8
    u[7] = u[0];
    u[6] = u[1];
    u[5] = u[2];
    u[4] = u[3];

    idct64_stage8_avx2(u, &cospim32, &cospi32, &cospim16, &cospi48, &cospi16,
                       &cospim48, &clamp_lo, &clamp_hi, &rnding, bit);

    // stage 9
    idct64_stage9_avx2(u, &cospim32, &cospi32, &clamp_lo, &clamp_hi, &rnding,
                       bit);

    // stage 10
    idct64_stage10_avx2(u, &cospim32, &cospi32, &clamp_lo, &clamp_hi, &rnding,
                        bit);

    // stage 11
    idct64_stage11_avx2(u, out, do_cols, bd, out_shift, &clamp_lo, &clamp_hi);
  }
}
static void idct64_low16_avx2(__m256i *in, __m256i *out, int bit, int do_cols,
                              int bd, int out_shift) {
  int i, j;
  const int32_t *cospi = cospi_arr(bit);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);

  const __m256i cospi1 = _mm256_set1_epi32(cospi[1]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospi3 = _mm256_set1_epi32(cospi[3]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospi5 = _mm256_set1_epi32(cospi[5]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi7 = _mm256_set1_epi32(cospi[7]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi9 = _mm256_set1_epi32(cospi[9]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi11 = _mm256_set1_epi32(cospi[11]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi13 = _mm256_set1_epi32(cospi[13]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi15 = _mm256_set1_epi32(cospi[15]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi51 = _mm256_set1_epi32(cospi[51]);
  const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi55 = _mm256_set1_epi32(cospi[55]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi59 = _mm256_set1_epi32(cospi[59]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi63 = _mm256_set1_epi32(cospi[63]);

  const __m256i cospim4 = _mm256_set1_epi32(-cospi[4]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospim12 = _mm256_set1_epi32(-cospi[12]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospim20 = _mm256_set1_epi32(-cospi[20]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospim28 = _mm256_set1_epi32(-cospi[28]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospim44 = _mm256_set1_epi32(-cospi[44]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospim49 = _mm256_set1_epi32(-cospi[49]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospim53 = _mm256_set1_epi32(-cospi[53]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim57 = _mm256_set1_epi32(-cospi[57]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospim60 = _mm256_set1_epi32(-cospi[60]);
  const __m256i cospim61 = _mm256_set1_epi32(-cospi[61]);

  {
    __m256i u[64];
    __m256i tmp1, tmp2, tmp3, tmp4;
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
    u[63] = half_btf_0_avx2(&cospi1, &u[32], &rnding, bit);
    u[32] = half_btf_0_avx2(&cospi63, &u[32], &rnding, bit);
    u[35] = half_btf_0_avx2(&cospim49, &u[60], &rnding, bit);
    u[60] = half_btf_0_avx2(&cospi15, &u[60], &rnding, bit);
    u[59] = half_btf_0_avx2(&cospi9, &u[36], &rnding, bit);
    u[36] = half_btf_0_avx2(&cospi55, &u[36], &rnding, bit);
    u[39] = half_btf_0_avx2(&cospim57, &u[56], &rnding, bit);
    u[56] = half_btf_0_avx2(&cospi7, &u[56], &rnding, bit);
    u[55] = half_btf_0_avx2(&cospi5, &u[40], &rnding, bit);
    u[40] = half_btf_0_avx2(&cospi59, &u[40], &rnding, bit);
    u[43] = half_btf_0_avx2(&cospim53, &u[52], &rnding, bit);
    u[52] = half_btf_0_avx2(&cospi11, &u[52], &rnding, bit);
    u[47] = half_btf_0_avx2(&cospim61, &u[48], &rnding, bit);
    u[48] = half_btf_0_avx2(&cospi3, &u[48], &rnding, bit);
    u[51] = half_btf_0_avx2(&cospi13, &u[44], &rnding, bit);
    u[44] = half_btf_0_avx2(&cospi51, &u[44], &rnding, bit);

    // stage 3
    u[31] = half_btf_0_avx2(&cospi2, &u[16], &rnding, bit);
    u[16] = half_btf_0_avx2(&cospi62, &u[16], &rnding, bit);
    u[19] = half_btf_0_avx2(&cospim50, &u[28], &rnding, bit);
    u[28] = half_btf_0_avx2(&cospi14, &u[28], &rnding, bit);
    u[27] = half_btf_0_avx2(&cospi10, &u[20], &rnding, bit);
    u[20] = half_btf_0_avx2(&cospi54, &u[20], &rnding, bit);
    u[23] = half_btf_0_avx2(&cospim58, &u[24], &rnding, bit);
    u[24] = half_btf_0_avx2(&cospi6, &u[24], &rnding, bit);
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
    u[15] = half_btf_0_avx2(&cospi4, &u[8], &rnding, bit);
    u[8] = half_btf_0_avx2(&cospi60, &u[8], &rnding, bit);
    u[11] = half_btf_0_avx2(&cospim52, &u[12], &rnding, bit);
    u[12] = half_btf_0_avx2(&cospi12, &u[12], &rnding, bit);

    u[17] = u[16];
    u[18] = u[19];
    u[21] = u[20];
    u[22] = u[23];
    u[25] = u[24];
    u[26] = u[27];
    u[29] = u[28];
    u[30] = u[31];

    tmp1 = half_btf_avx2(&cospim4, &u[33], &cospi60, &u[62], &rnding, bit);
    tmp2 = half_btf_avx2(&cospim60, &u[34], &cospim4, &u[61], &rnding, bit);
    tmp3 = half_btf_avx2(&cospim36, &u[37], &cospi28, &u[58], &rnding, bit);
    tmp4 = half_btf_avx2(&cospim28, &u[38], &cospim36, &u[57], &rnding, bit);
    u[57] = half_btf_avx2(&cospim36, &u[38], &cospi28, &u[57], &rnding, bit);
    u[58] = half_btf_avx2(&cospi28, &u[37], &cospi36, &u[58], &rnding, bit);
    u[61] = half_btf_avx2(&cospim4, &u[34], &cospi60, &u[61], &rnding, bit);
    u[62] = half_btf_avx2(&cospi60, &u[33], &cospi4, &u[62], &rnding, bit);
    u[33] = tmp1;
    u[34] = tmp2;
    u[37] = tmp3;
    u[38] = tmp4;

    tmp1 = half_btf_avx2(&cospim20, &u[41], &cospi44, &u[54], &rnding, bit);
    tmp2 = half_btf_avx2(&cospim44, &u[42], &cospim20, &u[53], &rnding, bit);
    tmp3 = half_btf_avx2(&cospim52, &u[45], &cospi12, &u[50], &rnding, bit);
    tmp4 = half_btf_avx2(&cospim12, &u[46], &cospim52, &u[49], &rnding, bit);
    u[49] = half_btf_avx2(&cospim52, &u[46], &cospi12, &u[49], &rnding, bit);
    u[50] = half_btf_avx2(&cospi12, &u[45], &cospi52, &u[50], &rnding, bit);
    u[53] = half_btf_avx2(&cospim20, &u[42], &cospi44, &u[53], &rnding, bit);
    u[54] = half_btf_avx2(&cospi44, &u[41], &cospi20, &u[54], &rnding, bit);
    u[41] = tmp1;
    u[42] = tmp2;
    u[45] = tmp3;
    u[46] = tmp4;

    // stage 5
    u[7] = half_btf_0_avx2(&cospi8, &u[4], &rnding, bit);
    u[4] = half_btf_0_avx2(&cospi56, &u[4], &rnding, bit);

    u[9] = u[8];
    u[10] = u[11];
    u[13] = u[12];
    u[14] = u[15];

    tmp1 = half_btf_avx2(&cospim8, &u[17], &cospi56, &u[30], &rnding, bit);
    tmp2 = half_btf_avx2(&cospim56, &u[18], &cospim8, &u[29], &rnding, bit);
    tmp3 = half_btf_avx2(&cospim40, &u[21], &cospi24, &u[26], &rnding, bit);
    tmp4 = half_btf_avx2(&cospim24, &u[22], &cospim40, &u[25], &rnding, bit);
    u[25] = half_btf_avx2(&cospim40, &u[22], &cospi24, &u[25], &rnding, bit);
    u[26] = half_btf_avx2(&cospi24, &u[21], &cospi40, &u[26], &rnding, bit);
    u[29] = half_btf_avx2(&cospim8, &u[18], &cospi56, &u[29], &rnding, bit);
    u[30] = half_btf_avx2(&cospi56, &u[17], &cospi8, &u[30], &rnding, bit);
    u[17] = tmp1;
    u[18] = tmp2;
    u[21] = tmp3;
    u[22] = tmp4;

    for (i = 32; i < 64; i += 8) {
      addsub_avx2(u[i + 0], u[i + 3], &u[i + 0], &u[i + 3], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(u[i + 1], u[i + 2], &u[i + 1], &u[i + 2], &clamp_lo,
                  &clamp_hi);

      addsub_avx2(u[i + 7], u[i + 4], &u[i + 7], &u[i + 4], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(u[i + 6], u[i + 5], &u[i + 6], &u[i + 5], &clamp_lo,
                  &clamp_hi);
    }

    // stage 6
    tmp1 = half_btf_0_avx2(&cospi32, &u[0], &rnding, bit);
    u[1] = half_btf_0_avx2(&cospi32, &u[0], &rnding, bit);
    u[0] = tmp1;
    u[5] = u[4];
    u[6] = u[7];

    tmp1 = half_btf_avx2(&cospim16, &u[9], &cospi48, &u[14], &rnding, bit);
    u[14] = half_btf_avx2(&cospi48, &u[9], &cospi16, &u[14], &rnding, bit);
    u[9] = tmp1;
    tmp2 = half_btf_avx2(&cospim48, &u[10], &cospim16, &u[13], &rnding, bit);
    u[13] = half_btf_avx2(&cospim16, &u[10], &cospi48, &u[13], &rnding, bit);
    u[10] = tmp2;

    for (i = 16; i < 32; i += 8) {
      addsub_avx2(u[i + 0], u[i + 3], &u[i + 0], &u[i + 3], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(u[i + 1], u[i + 2], &u[i + 1], &u[i + 2], &clamp_lo,
                  &clamp_hi);

      addsub_avx2(u[i + 7], u[i + 4], &u[i + 7], &u[i + 4], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(u[i + 6], u[i + 5], &u[i + 6], &u[i + 5], &clamp_lo,
                  &clamp_hi);
    }

    tmp1 = half_btf_avx2(&cospim8, &u[34], &cospi56, &u[61], &rnding, bit);
    tmp2 = half_btf_avx2(&cospim8, &u[35], &cospi56, &u[60], &rnding, bit);
    tmp3 = half_btf_avx2(&cospim56, &u[36], &cospim8, &u[59], &rnding, bit);
    tmp4 = half_btf_avx2(&cospim56, &u[37], &cospim8, &u[58], &rnding, bit);
    u[58] = half_btf_avx2(&cospim8, &u[37], &cospi56, &u[58], &rnding, bit);
    u[59] = half_btf_avx2(&cospim8, &u[36], &cospi56, &u[59], &rnding, bit);
    u[60] = half_btf_avx2(&cospi56, &u[35], &cospi8, &u[60], &rnding, bit);
    u[61] = half_btf_avx2(&cospi56, &u[34], &cospi8, &u[61], &rnding, bit);
    u[34] = tmp1;
    u[35] = tmp2;
    u[36] = tmp3;
    u[37] = tmp4;

    tmp1 = half_btf_avx2(&cospim40, &u[42], &cospi24, &u[53], &rnding, bit);
    tmp2 = half_btf_avx2(&cospim40, &u[43], &cospi24, &u[52], &rnding, bit);
    tmp3 = half_btf_avx2(&cospim24, &u[44], &cospim40, &u[51], &rnding, bit);
    tmp4 = half_btf_avx2(&cospim24, &u[45], &cospim40, &u[50], &rnding, bit);
    u[50] = half_btf_avx2(&cospim40, &u[45], &cospi24, &u[50], &rnding, bit);
    u[51] = half_btf_avx2(&cospim40, &u[44], &cospi24, &u[51], &rnding, bit);
    u[52] = half_btf_avx2(&cospi24, &u[43], &cospi40, &u[52], &rnding, bit);
    u[53] = half_btf_avx2(&cospi24, &u[42], &cospi40, &u[53], &rnding, bit);
    u[42] = tmp1;
    u[43] = tmp2;
    u[44] = tmp3;
    u[45] = tmp4;

    // stage 7
    u[3] = u[0];
    u[2] = u[1];
    tmp1 = half_btf_avx2(&cospim32, &u[5], &cospi32, &u[6], &rnding, bit);
    u[6] = half_btf_avx2(&cospi32, &u[5], &cospi32, &u[6], &rnding, bit);
    u[5] = tmp1;
    addsub_avx2(u[8], u[11], &u[8], &u[11], &clamp_lo, &clamp_hi);
    addsub_avx2(u[9], u[10], &u[9], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(u[15], u[12], &u[15], &u[12], &clamp_lo, &clamp_hi);
    addsub_avx2(u[14], u[13], &u[14], &u[13], &clamp_lo, &clamp_hi);

    tmp1 = half_btf_avx2(&cospim16, &u[18], &cospi48, &u[29], &rnding, bit);
    tmp2 = half_btf_avx2(&cospim16, &u[19], &cospi48, &u[28], &rnding, bit);
    tmp3 = half_btf_avx2(&cospim48, &u[20], &cospim16, &u[27], &rnding, bit);
    tmp4 = half_btf_avx2(&cospim48, &u[21], &cospim16, &u[26], &rnding, bit);
    u[26] = half_btf_avx2(&cospim16, &u[21], &cospi48, &u[26], &rnding, bit);
    u[27] = half_btf_avx2(&cospim16, &u[20], &cospi48, &u[27], &rnding, bit);
    u[28] = half_btf_avx2(&cospi48, &u[19], &cospi16, &u[28], &rnding, bit);
    u[29] = half_btf_avx2(&cospi48, &u[18], &cospi16, &u[29], &rnding, bit);
    u[18] = tmp1;
    u[19] = tmp2;
    u[20] = tmp3;
    u[21] = tmp4;

    for (i = 32; i < 64; i += 16) {
      for (j = i; j < i + 4; j++) {
        addsub_avx2(u[j], u[j ^ 7], &u[j], &u[j ^ 7], &clamp_lo, &clamp_hi);
        addsub_avx2(u[j ^ 15], u[j ^ 8], &u[j ^ 15], &u[j ^ 8], &clamp_lo,
                    &clamp_hi);
      }
    }

    // stage 8
    for (i = 0; i < 4; ++i) {
      addsub_avx2(u[i], u[7 - i], &u[i], &u[7 - i], &clamp_lo, &clamp_hi);
    }

    idct64_stage8_avx2(u, &cospim32, &cospi32, &cospim16, &cospi48, &cospi16,
                       &cospim48, &clamp_lo, &clamp_hi, &rnding, bit);

    // stage 9
    idct64_stage9_avx2(u, &cospim32, &cospi32, &clamp_lo, &clamp_hi, &rnding,
                       bit);

    // stage 10
    idct64_stage10_avx2(u, &cospim32, &cospi32, &clamp_lo, &clamp_hi, &rnding,
                        bit);

    // stage 11
    idct64_stage11_avx2(u, out, do_cols, bd, out_shift, &clamp_lo, &clamp_hi);
  }
}
static void idct64_avx2(__m256i *in, __m256i *out, int bit, int do_cols, int bd,
                        int out_shift) {
  int i, j;
  const int32_t *cospi = cospi_arr(bit);
  const __m256i rnding = _mm256_set1_epi32(1 << (bit - 1));
  const int log_range = AOMMAX(16, bd + (do_cols ? 6 : 8));
  const __m256i clamp_lo = _mm256_set1_epi32(-(1 << (log_range - 1)));
  const __m256i clamp_hi = _mm256_set1_epi32((1 << (log_range - 1)) - 1);

  const __m256i cospi1 = _mm256_set1_epi32(cospi[1]);
  const __m256i cospi2 = _mm256_set1_epi32(cospi[2]);
  const __m256i cospi3 = _mm256_set1_epi32(cospi[3]);
  const __m256i cospi4 = _mm256_set1_epi32(cospi[4]);
  const __m256i cospi5 = _mm256_set1_epi32(cospi[5]);
  const __m256i cospi6 = _mm256_set1_epi32(cospi[6]);
  const __m256i cospi7 = _mm256_set1_epi32(cospi[7]);
  const __m256i cospi8 = _mm256_set1_epi32(cospi[8]);
  const __m256i cospi9 = _mm256_set1_epi32(cospi[9]);
  const __m256i cospi10 = _mm256_set1_epi32(cospi[10]);
  const __m256i cospi11 = _mm256_set1_epi32(cospi[11]);
  const __m256i cospi12 = _mm256_set1_epi32(cospi[12]);
  const __m256i cospi13 = _mm256_set1_epi32(cospi[13]);
  const __m256i cospi14 = _mm256_set1_epi32(cospi[14]);
  const __m256i cospi15 = _mm256_set1_epi32(cospi[15]);
  const __m256i cospi16 = _mm256_set1_epi32(cospi[16]);
  const __m256i cospi17 = _mm256_set1_epi32(cospi[17]);
  const __m256i cospi18 = _mm256_set1_epi32(cospi[18]);
  const __m256i cospi19 = _mm256_set1_epi32(cospi[19]);
  const __m256i cospi20 = _mm256_set1_epi32(cospi[20]);
  const __m256i cospi21 = _mm256_set1_epi32(cospi[21]);
  const __m256i cospi22 = _mm256_set1_epi32(cospi[22]);
  const __m256i cospi23 = _mm256_set1_epi32(cospi[23]);
  const __m256i cospi24 = _mm256_set1_epi32(cospi[24]);
  const __m256i cospi25 = _mm256_set1_epi32(cospi[25]);
  const __m256i cospi26 = _mm256_set1_epi32(cospi[26]);
  const __m256i cospi27 = _mm256_set1_epi32(cospi[27]);
  const __m256i cospi28 = _mm256_set1_epi32(cospi[28]);
  const __m256i cospi29 = _mm256_set1_epi32(cospi[29]);
  const __m256i cospi30 = _mm256_set1_epi32(cospi[30]);
  const __m256i cospi31 = _mm256_set1_epi32(cospi[31]);
  const __m256i cospi32 = _mm256_set1_epi32(cospi[32]);
  const __m256i cospi35 = _mm256_set1_epi32(cospi[35]);
  const __m256i cospi36 = _mm256_set1_epi32(cospi[36]);
  const __m256i cospi38 = _mm256_set1_epi32(cospi[38]);
  const __m256i cospi39 = _mm256_set1_epi32(cospi[39]);
  const __m256i cospi40 = _mm256_set1_epi32(cospi[40]);
  const __m256i cospi43 = _mm256_set1_epi32(cospi[43]);
  const __m256i cospi44 = _mm256_set1_epi32(cospi[44]);
  const __m256i cospi46 = _mm256_set1_epi32(cospi[46]);
  const __m256i cospi47 = _mm256_set1_epi32(cospi[47]);
  const __m256i cospi48 = _mm256_set1_epi32(cospi[48]);
  const __m256i cospi51 = _mm256_set1_epi32(cospi[51]);
  const __m256i cospi52 = _mm256_set1_epi32(cospi[52]);
  const __m256i cospi54 = _mm256_set1_epi32(cospi[54]);
  const __m256i cospi55 = _mm256_set1_epi32(cospi[55]);
  const __m256i cospi56 = _mm256_set1_epi32(cospi[56]);
  const __m256i cospi59 = _mm256_set1_epi32(cospi[59]);
  const __m256i cospi60 = _mm256_set1_epi32(cospi[60]);
  const __m256i cospi62 = _mm256_set1_epi32(cospi[62]);
  const __m256i cospi63 = _mm256_set1_epi32(cospi[63]);

  const __m256i cospim4 = _mm256_set1_epi32(-cospi[4]);
  const __m256i cospim8 = _mm256_set1_epi32(-cospi[8]);
  const __m256i cospim12 = _mm256_set1_epi32(-cospi[12]);
  const __m256i cospim16 = _mm256_set1_epi32(-cospi[16]);
  const __m256i cospim20 = _mm256_set1_epi32(-cospi[20]);
  const __m256i cospim24 = _mm256_set1_epi32(-cospi[24]);
  const __m256i cospim28 = _mm256_set1_epi32(-cospi[28]);
  const __m256i cospim32 = _mm256_set1_epi32(-cospi[32]);
  const __m256i cospim33 = _mm256_set1_epi32(-cospi[33]);
  const __m256i cospim34 = _mm256_set1_epi32(-cospi[34]);
  const __m256i cospim36 = _mm256_set1_epi32(-cospi[36]);
  const __m256i cospim37 = _mm256_set1_epi32(-cospi[37]);
  const __m256i cospim40 = _mm256_set1_epi32(-cospi[40]);
  const __m256i cospim41 = _mm256_set1_epi32(-cospi[41]);
  const __m256i cospim42 = _mm256_set1_epi32(-cospi[42]);
  const __m256i cospim44 = _mm256_set1_epi32(-cospi[44]);
  const __m256i cospim45 = _mm256_set1_epi32(-cospi[45]);
  const __m256i cospim48 = _mm256_set1_epi32(-cospi[48]);
  const __m256i cospim49 = _mm256_set1_epi32(-cospi[49]);
  const __m256i cospim50 = _mm256_set1_epi32(-cospi[50]);
  const __m256i cospim52 = _mm256_set1_epi32(-cospi[52]);
  const __m256i cospim53 = _mm256_set1_epi32(-cospi[53]);
  const __m256i cospim56 = _mm256_set1_epi32(-cospi[56]);
  const __m256i cospim57 = _mm256_set1_epi32(-cospi[57]);
  const __m256i cospim58 = _mm256_set1_epi32(-cospi[58]);
  const __m256i cospim60 = _mm256_set1_epi32(-cospi[60]);
  const __m256i cospim61 = _mm256_set1_epi32(-cospi[61]);

  {
    __m256i u[64], v[64];

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
    v[32] = half_btf_0_avx2(&cospi63, &u[32], &rnding, bit);
    v[33] = half_btf_0_avx2(&cospim33, &u[62], &rnding, bit);
    v[34] = half_btf_0_avx2(&cospi47, &u[34], &rnding, bit);
    v[35] = half_btf_0_avx2(&cospim49, &u[60], &rnding, bit);
    v[36] = half_btf_0_avx2(&cospi55, &u[36], &rnding, bit);
    v[37] = half_btf_0_avx2(&cospim41, &u[58], &rnding, bit);
    v[38] = half_btf_0_avx2(&cospi39, &u[38], &rnding, bit);
    v[39] = half_btf_0_avx2(&cospim57, &u[56], &rnding, bit);
    v[40] = half_btf_0_avx2(&cospi59, &u[40], &rnding, bit);
    v[41] = half_btf_0_avx2(&cospim37, &u[54], &rnding, bit);
    v[42] = half_btf_0_avx2(&cospi43, &u[42], &rnding, bit);
    v[43] = half_btf_0_avx2(&cospim53, &u[52], &rnding, bit);
    v[44] = half_btf_0_avx2(&cospi51, &u[44], &rnding, bit);
    v[45] = half_btf_0_avx2(&cospim45, &u[50], &rnding, bit);
    v[46] = half_btf_0_avx2(&cospi35, &u[46], &rnding, bit);
    v[47] = half_btf_0_avx2(&cospim61, &u[48], &rnding, bit);
    v[48] = half_btf_0_avx2(&cospi3, &u[48], &rnding, bit);
    v[49] = half_btf_0_avx2(&cospi29, &u[46], &rnding, bit);
    v[50] = half_btf_0_avx2(&cospi19, &u[50], &rnding, bit);
    v[51] = half_btf_0_avx2(&cospi13, &u[44], &rnding, bit);
    v[52] = half_btf_0_avx2(&cospi11, &u[52], &rnding, bit);
    v[53] = half_btf_0_avx2(&cospi21, &u[42], &rnding, bit);
    v[54] = half_btf_0_avx2(&cospi27, &u[54], &rnding, bit);
    v[55] = half_btf_0_avx2(&cospi5, &u[40], &rnding, bit);
    v[56] = half_btf_0_avx2(&cospi7, &u[56], &rnding, bit);
    v[57] = half_btf_0_avx2(&cospi25, &u[38], &rnding, bit);
    v[58] = half_btf_0_avx2(&cospi23, &u[58], &rnding, bit);
    v[59] = half_btf_0_avx2(&cospi9, &u[36], &rnding, bit);
    v[60] = half_btf_0_avx2(&cospi15, &u[60], &rnding, bit);
    v[61] = half_btf_0_avx2(&cospi17, &u[34], &rnding, bit);
    v[62] = half_btf_0_avx2(&cospi31, &u[62], &rnding, bit);
    v[63] = half_btf_0_avx2(&cospi1, &u[32], &rnding, bit);

    // stage 3
    u[16] = half_btf_0_avx2(&cospi62, &v[16], &rnding, bit);
    u[17] = half_btf_0_avx2(&cospim34, &v[30], &rnding, bit);
    u[18] = half_btf_0_avx2(&cospi46, &v[18], &rnding, bit);
    u[19] = half_btf_0_avx2(&cospim50, &v[28], &rnding, bit);
    u[20] = half_btf_0_avx2(&cospi54, &v[20], &rnding, bit);
    u[21] = half_btf_0_avx2(&cospim42, &v[26], &rnding, bit);
    u[22] = half_btf_0_avx2(&cospi38, &v[22], &rnding, bit);
    u[23] = half_btf_0_avx2(&cospim58, &v[24], &rnding, bit);
    u[24] = half_btf_0_avx2(&cospi6, &v[24], &rnding, bit);
    u[25] = half_btf_0_avx2(&cospi26, &v[22], &rnding, bit);
    u[26] = half_btf_0_avx2(&cospi22, &v[26], &rnding, bit);
    u[27] = half_btf_0_avx2(&cospi10, &v[20], &rnding, bit);
    u[28] = half_btf_0_avx2(&cospi14, &v[28], &rnding, bit);
    u[29] = half_btf_0_avx2(&cospi18, &v[18], &rnding, bit);
    u[30] = half_btf_0_avx2(&cospi30, &v[30], &rnding, bit);
    u[31] = half_btf_0_avx2(&cospi2, &v[16], &rnding, bit);

    for (i = 32; i < 64; i += 4) {
      addsub_avx2(v[i + 0], v[i + 1], &u[i + 0], &u[i + 1], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(v[i + 3], v[i + 2], &u[i + 3], &u[i + 2], &clamp_lo,
                  &clamp_hi);
    }

    // stage 4
    v[8] = half_btf_0_avx2(&cospi60, &u[8], &rnding, bit);
    v[9] = half_btf_0_avx2(&cospim36, &u[14], &rnding, bit);
    v[10] = half_btf_0_avx2(&cospi44, &u[10], &rnding, bit);
    v[11] = half_btf_0_avx2(&cospim52, &u[12], &rnding, bit);
    v[12] = half_btf_0_avx2(&cospi12, &u[12], &rnding, bit);
    v[13] = half_btf_0_avx2(&cospi20, &u[10], &rnding, bit);
    v[14] = half_btf_0_avx2(&cospi28, &u[14], &rnding, bit);
    v[15] = half_btf_0_avx2(&cospi4, &u[8], &rnding, bit);

    for (i = 16; i < 32; i += 4) {
      addsub_avx2(u[i + 0], u[i + 1], &v[i + 0], &v[i + 1], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(u[i + 3], u[i + 2], &v[i + 3], &v[i + 2], &clamp_lo,
                  &clamp_hi);
    }

    for (i = 32; i < 64; i += 4) {
      v[i + 0] = u[i + 0];
      v[i + 3] = u[i + 3];
    }

    v[33] = half_btf_avx2(&cospim4, &u[33], &cospi60, &u[62], &rnding, bit);
    v[34] = half_btf_avx2(&cospim60, &u[34], &cospim4, &u[61], &rnding, bit);
    v[37] = half_btf_avx2(&cospim36, &u[37], &cospi28, &u[58], &rnding, bit);
    v[38] = half_btf_avx2(&cospim28, &u[38], &cospim36, &u[57], &rnding, bit);
    v[41] = half_btf_avx2(&cospim20, &u[41], &cospi44, &u[54], &rnding, bit);
    v[42] = half_btf_avx2(&cospim44, &u[42], &cospim20, &u[53], &rnding, bit);
    v[45] = half_btf_avx2(&cospim52, &u[45], &cospi12, &u[50], &rnding, bit);
    v[46] = half_btf_avx2(&cospim12, &u[46], &cospim52, &u[49], &rnding, bit);
    v[49] = half_btf_avx2(&cospim52, &u[46], &cospi12, &u[49], &rnding, bit);
    v[50] = half_btf_avx2(&cospi12, &u[45], &cospi52, &u[50], &rnding, bit);
    v[53] = half_btf_avx2(&cospim20, &u[42], &cospi44, &u[53], &rnding, bit);
    v[54] = half_btf_avx2(&cospi44, &u[41], &cospi20, &u[54], &rnding, bit);
    v[57] = half_btf_avx2(&cospim36, &u[38], &cospi28, &u[57], &rnding, bit);
    v[58] = half_btf_avx2(&cospi28, &u[37], &cospi36, &u[58], &rnding, bit);
    v[61] = half_btf_avx2(&cospim4, &u[34], &cospi60, &u[61], &rnding, bit);
    v[62] = half_btf_avx2(&cospi60, &u[33], &cospi4, &u[62], &rnding, bit);

    // stage 5
    u[4] = half_btf_0_avx2(&cospi56, &v[4], &rnding, bit);
    u[5] = half_btf_0_avx2(&cospim40, &v[6], &rnding, bit);
    u[6] = half_btf_0_avx2(&cospi24, &v[6], &rnding, bit);
    u[7] = half_btf_0_avx2(&cospi8, &v[4], &rnding, bit);

    for (i = 8; i < 16; i += 4) {
      addsub_avx2(v[i + 0], v[i + 1], &u[i + 0], &u[i + 1], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(v[i + 3], v[i + 2], &u[i + 3], &u[i + 2], &clamp_lo,
                  &clamp_hi);
    }

    for (i = 16; i < 32; i += 4) {
      u[i + 0] = v[i + 0];
      u[i + 3] = v[i + 3];
    }

    u[17] = half_btf_avx2(&cospim8, &v[17], &cospi56, &v[30], &rnding, bit);
    u[18] = half_btf_avx2(&cospim56, &v[18], &cospim8, &v[29], &rnding, bit);
    u[21] = half_btf_avx2(&cospim40, &v[21], &cospi24, &v[26], &rnding, bit);
    u[22] = half_btf_avx2(&cospim24, &v[22], &cospim40, &v[25], &rnding, bit);
    u[25] = half_btf_avx2(&cospim40, &v[22], &cospi24, &v[25], &rnding, bit);
    u[26] = half_btf_avx2(&cospi24, &v[21], &cospi40, &v[26], &rnding, bit);
    u[29] = half_btf_avx2(&cospim8, &v[18], &cospi56, &v[29], &rnding, bit);
    u[30] = half_btf_avx2(&cospi56, &v[17], &cospi8, &v[30], &rnding, bit);

    for (i = 32; i < 64; i += 8) {
      addsub_avx2(v[i + 0], v[i + 3], &u[i + 0], &u[i + 3], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(v[i + 1], v[i + 2], &u[i + 1], &u[i + 2], &clamp_lo,
                  &clamp_hi);

      addsub_avx2(v[i + 7], v[i + 4], &u[i + 7], &u[i + 4], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(v[i + 6], v[i + 5], &u[i + 6], &u[i + 5], &clamp_lo,
                  &clamp_hi);
    }

    // stage 6
    v[0] = half_btf_0_avx2(&cospi32, &u[0], &rnding, bit);
    v[1] = half_btf_0_avx2(&cospi32, &u[0], &rnding, bit);
    v[2] = half_btf_0_avx2(&cospi48, &u[2], &rnding, bit);
    v[3] = half_btf_0_avx2(&cospi16, &u[2], &rnding, bit);

    addsub_avx2(u[4], u[5], &v[4], &v[5], &clamp_lo, &clamp_hi);
    addsub_avx2(u[7], u[6], &v[7], &v[6], &clamp_lo, &clamp_hi);

    for (i = 8; i < 16; i += 4) {
      v[i + 0] = u[i + 0];
      v[i + 3] = u[i + 3];
    }

    v[9] = half_btf_avx2(&cospim16, &u[9], &cospi48, &u[14], &rnding, bit);
    v[10] = half_btf_avx2(&cospim48, &u[10], &cospim16, &u[13], &rnding, bit);
    v[13] = half_btf_avx2(&cospim16, &u[10], &cospi48, &u[13], &rnding, bit);
    v[14] = half_btf_avx2(&cospi48, &u[9], &cospi16, &u[14], &rnding, bit);

    for (i = 16; i < 32; i += 8) {
      addsub_avx2(u[i + 0], u[i + 3], &v[i + 0], &v[i + 3], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(u[i + 1], u[i + 2], &v[i + 1], &v[i + 2], &clamp_lo,
                  &clamp_hi);

      addsub_avx2(u[i + 7], u[i + 4], &v[i + 7], &v[i + 4], &clamp_lo,
                  &clamp_hi);
      addsub_avx2(u[i + 6], u[i + 5], &v[i + 6], &v[i + 5], &clamp_lo,
                  &clamp_hi);
    }

    for (i = 32; i < 64; i += 8) {
      v[i + 0] = u[i + 0];
      v[i + 1] = u[i + 1];
      v[i + 6] = u[i + 6];
      v[i + 7] = u[i + 7];
    }

    v[34] = half_btf_avx2(&cospim8, &u[34], &cospi56, &u[61], &rnding, bit);
    v[35] = half_btf_avx2(&cospim8, &u[35], &cospi56, &u[60], &rnding, bit);
    v[36] = half_btf_avx2(&cospim56, &u[36], &cospim8, &u[59], &rnding, bit);
    v[37] = half_btf_avx2(&cospim56, &u[37], &cospim8, &u[58], &rnding, bit);
    v[42] = half_btf_avx2(&cospim40, &u[42], &cospi24, &u[53], &rnding, bit);
    v[43] = half_btf_avx2(&cospim40, &u[43], &cospi24, &u[52], &rnding, bit);
    v[44] = half_btf_avx2(&cospim24, &u[44], &cospim40, &u[51], &rnding, bit);
    v[45] = half_btf_avx2(&cospim24, &u[45], &cospim40, &u[50], &rnding, bit);
    v[50] = half_btf_avx2(&cospim40, &u[45], &cospi24, &u[50], &rnding, bit);
    v[51] = half_btf_avx2(&cospim40, &u[44], &cospi24, &u[51], &rnding, bit);
    v[52] = half_btf_avx2(&cospi24, &u[43], &cospi40, &u[52], &rnding, bit);
    v[53] = half_btf_avx2(&cospi24, &u[42], &cospi40, &u[53], &rnding, bit);
    v[58] = half_btf_avx2(&cospim8, &u[37], &cospi56, &u[58], &rnding, bit);
    v[59] = half_btf_avx2(&cospim8, &u[36], &cospi56, &u[59], &rnding, bit);
    v[60] = half_btf_avx2(&cospi56, &u[35], &cospi8, &u[60], &rnding, bit);
    v[61] = half_btf_avx2(&cospi56, &u[34], &cospi8, &u[61], &rnding, bit);

    // stage 7
    addsub_avx2(v[0], v[3], &u[0], &u[3], &clamp_lo, &clamp_hi);
    addsub_avx2(v[1], v[2], &u[1], &u[2], &clamp_lo, &clamp_hi);

    u[4] = v[4];
    u[7] = v[7];
    u[5] = half_btf_avx2(&cospim32, &v[5], &cospi32, &v[6], &rnding, bit);
    u[6] = half_btf_avx2(&cospi32, &v[5], &cospi32, &v[6], &rnding, bit);

    addsub_avx2(v[8], v[11], &u[8], &u[11], &clamp_lo, &clamp_hi);
    addsub_avx2(v[9], v[10], &u[9], &u[10], &clamp_lo, &clamp_hi);
    addsub_avx2(v[15], v[12], &u[15], &u[12], &clamp_lo, &clamp_hi);
    addsub_avx2(v[14], v[13], &u[14], &u[13], &clamp_lo, &clamp_hi);

    for (i = 16; i < 32; i += 8) {
      u[i + 0] = v[i + 0];
      u[i + 1] = v[i + 1];
      u[i + 6] = v[i + 6];
      u[i + 7] = v[i + 7];
    }

    u[18] = half_btf_avx2(&cospim16, &v[18], &cospi48, &v[29], &rnding, bit);
    u[19] = half_btf_avx2(&cospim16, &v[19], &cospi48, &v[28], &rnding, bit);
    u[20] = half_btf_avx2(&cospim48, &v[20], &cospim16, &v[27], &rnding, bit);
    u[21] = half_btf_avx2(&cospim48, &v[21], &cospim16, &v[26], &rnding, bit);
    u[26] = half_btf_avx2(&cospim16, &v[21], &cospi48, &v[26], &rnding, bit);
    u[27] = half_btf_avx2(&cospim16, &v[20], &cospi48, &v[27], &rnding, bit);
    u[28] = half_btf_avx2(&cospi48, &v[19], &cospi16, &v[28], &rnding, bit);
    u[29] = half_btf_avx2(&cospi48, &v[18], &cospi16, &v[29], &rnding, bit);

    for (i = 32; i < 64; i += 16) {
      for (j = i; j < i + 4; j++) {
        addsub_avx2(v[j], v[j ^ 7], &u[j], &u[j ^ 7], &clamp_lo, &clamp_hi);
        addsub_avx2(v[j ^ 15], v[j ^ 8], &u[j ^ 15], &u[j ^ 8], &clamp_lo,
                    &clamp_hi);
      }
    }

    // stage 8
    for (i = 0; i < 4; ++i) {
      addsub_avx2(u[i], u[7 - i], &v[i], &v[7 - i], &clamp_lo, &clamp_hi);
    }

    v[8] = u[8];
    v[9] = u[9];
    v[14] = u[14];
    v[15] = u[15];

    v[10] = half_btf_avx2(&cospim32, &u[10], &cospi32, &u[13], &rnding, bit);
    v[11] = half_btf_avx2(&cospim32, &u[11], &cospi32, &u[12], &rnding, bit);
    v[12] = half_btf_avx2(&cospi32, &u[11], &cospi32, &u[12], &rnding, bit);
    v[13] = half_btf_avx2(&cospi32, &u[10], &cospi32, &u[13], &rnding, bit);

    for (i = 16; i < 20; ++i) {
      addsub_avx2(u[i], u[i ^ 7], &v[i], &v[i ^ 7], &clamp_lo, &clamp_hi);
      addsub_avx2(u[i ^ 15], u[i ^ 8], &v[i ^ 15], &v[i ^ 8], &clamp_lo,
                  &clamp_hi);
    }

    for (i = 32; i < 36; ++i) {
      v[i] = u[i];
      v[i + 12] = u[i + 12];
      v[i + 16] = u[i + 16];
      v[i + 28] = u[i + 28];
    }

    v[36] = half_btf_avx2(&cospim16, &u[36], &cospi48, &u[59], &rnding, bit);
    v[37] = half_btf_avx2(&cospim16, &u[37], &cospi48, &u[58], &rnding, bit);
    v[38] = half_btf_avx2(&cospim16, &u[38], &cospi48, &u[57], &rnding, bit);
    v[39] = half_btf_avx2(&cospim16, &u[39], &cospi48, &u[56], &rnding, bit);
    v[40] = half_btf_avx2(&cospim48, &u[40], &cospim16, &u[55], &rnding, bit);
    v[41] = half_btf_avx2(&cospim48, &u[41], &cospim16, &u[54], &rnding, bit);
    v[42] = half_btf_avx2(&cospim48, &u[42], &cospim16, &u[53], &rnding, bit);
    v[43] = half_btf_avx2(&cospim48, &u[43], &cospim16, &u[52], &rnding, bit);
    v[52] = half_btf_avx2(&cospim16, &u[43], &cospi48, &u[52], &rnding, bit);
    v[53] = half_btf_avx2(&cospim16, &u[42], &cospi48, &u[53], &rnding, bit);
    v[54] = half_btf_avx2(&cospim16, &u[41], &cospi48, &u[54], &rnding, bit);
    v[55] = half_btf_avx2(&cospim16, &u[40], &cospi48, &u[55], &rnding, bit);
    v[56] = half_btf_avx2(&cospi48, &u[39], &cospi16, &u[56], &rnding, bit);
    v[57] = half_btf_avx2(&cospi48, &u[38], &cospi16, &u[57], &rnding, bit);
    v[58] = half_btf_avx2(&cospi48, &u[37], &cospi16, &u[58], &rnding, bit);
    v[59] = half_btf_avx2(&cospi48, &u[36], &cospi16, &u[59], &rnding, bit);

    // stage 9
    for (i = 0; i < 8; ++i) {
      addsub_avx2(v[i], v[15 - i], &u[i], &u[15 - i], &clamp_lo, &clamp_hi);
    }

    for (i = 16; i < 20; ++i) {
      u[i] = v[i];
      u[i + 12] = v[i + 12];
    }

    u[20] = half_btf_avx2(&cospim32, &v[20], &cospi32, &v[27], &rnding, bit);
    u[21] = half_btf_avx2(&cospim32, &v[21], &cospi32, &v[26], &rnding, bit);
    u[22] = half_btf_avx2(&cospim32, &v[22], &cospi32, &v[25], &rnding, bit);
    u[23] = half_btf_avx2(&cospim32, &v[23], &cospi32, &v[24], &rnding, bit);
    u[24] = half_btf_avx2(&cospi32, &v[23], &cospi32, &v[24], &rnding, bit);
    u[25] = half_btf_avx2(&cospi32, &v[22], &cospi32, &v[25], &rnding, bit);
    u[26] = half_btf_avx2(&cospi32, &v[21], &cospi32, &v[26], &rnding, bit);
    u[27] = half_btf_avx2(&cospi32, &v[20], &cospi32, &v[27], &rnding, bit);

    for (i = 32; i < 40; i++) {
      addsub_avx2(v[i], v[i ^ 15], &u[i], &u[i ^ 15], &clamp_lo, &clamp_hi);
    }

    for (i = 48; i < 56; i++) {
      addsub_avx2(v[i ^ 15], v[i], &u[i ^ 15], &u[i], &clamp_lo, &clamp_hi);
    }

    // stage 10
    for (i = 0; i < 16; i++) {
      addsub_avx2(u[i], u[31 - i], &v[i], &v[31 - i], &clamp_lo, &clamp_hi);
    }

    for (i = 32; i < 40; i++) v[i] = u[i];

    v[40] = half_btf_avx2(&cospim32, &u[40], &cospi32, &u[55], &rnding, bit);
    v[41] = half_btf_avx2(&cospim32, &u[41], &cospi32, &u[54], &rnding, bit);
    v[42] = half_btf_avx2(&cospim32, &u[42], &cospi32, &u[53], &rnding, bit);
    v[43] = half_btf_avx2(&cospim32, &u[43], &cospi32, &u[52], &rnding, bit);
    v[44] = half_btf_avx2(&cospim32, &u[44], &cospi32, &u[51], &rnding, bit);
    v[45] = half_btf_avx2(&cospim32, &u[45], &cospi32, &u[50], &rnding, bit);
    v[46] = half_btf_avx2(&cospim32, &u[46], &cospi32, &u[49], &rnding, bit);
    v[47] = half_btf_avx2(&cospim32, &u[47], &cospi32, &u[48], &rnding, bit);
    v[48] = half_btf_avx2(&cospi32, &u[47], &cospi32, &u[48], &rnding, bit);
    v[49] = half_btf_avx2(&cospi32, &u[46], &cospi32, &u[49], &rnding, bit);
    v[50] = half_btf_avx2(&cospi32, &u[45], &cospi32, &u[50], &rnding, bit);
    v[51] = half_btf_avx2(&cospi32, &u[44], &cospi32, &u[51], &rnding, bit);
    v[52] = half_btf_avx2(&cospi32, &u[43], &cospi32, &u[52], &rnding, bit);
    v[53] = half_btf_avx2(&cospi32, &u[42], &cospi32, &u[53], &rnding, bit);
    v[54] = half_btf_avx2(&cospi32, &u[41], &cospi32, &u[54], &rnding, bit);
    v[55] = half_btf_avx2(&cospi32, &u[40], &cospi32, &u[55], &rnding, bit);

    for (i = 56; i < 64; i++) v[i] = u[i];

    // stage 11
    for (i = 0; i < 32; i++) {
      addsub_avx2(v[i], v[63 - i], &out[(i)], &out[(63 - i)], &clamp_lo,
                  &clamp_hi);
    }
    if (!do_cols) {
      const int log_range_out = AOMMAX(16, bd + 6);
      const __m256i clamp_lo_out =
          _mm256_set1_epi32(-(1 << (log_range_out - 1)));
      const __m256i clamp_hi_out =
          _mm256_set1_epi32((1 << (log_range_out - 1)) - 1);

      round_shift_8x8_avx2(out, out_shift);
      round_shift_8x8_avx2(out + 16, out_shift);
      round_shift_8x8_avx2(out + 32, out_shift);
      round_shift_8x8_avx2(out + 48, out_shift);
      highbd_clamp_epi32_avx2(out, out, &clamp_lo_out, &clamp_hi_out, 64);
    }
  }
}
typedef void (*transform_1d_avx2)(__m256i *in, __m256i *out, int bit,
                                  int do_cols, int bd, int out_shift);

static const transform_1d_avx2
    highbd_txfm_all_1d_zeros_w8_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
      },
      {
          { idct8x8_low1_avx2, idct8x8_avx2, NULL, NULL },
          { iadst8x8_low1_avx2, iadst8x8_avx2, NULL, NULL },
          { NULL, NULL, NULL, NULL },
      },
      {
          { idct16_low1_avx2, idct16_low8_avx2, idct16_avx2, NULL },
          { iadst16_low1_avx2, iadst16_low8_avx2, iadst16_avx2, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { idct32_low1_avx2, idct32_low8_avx2, idct32_low16_avx2, idct32_avx2 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },

      { { idct64_low1_avx2, idct64_low8_avx2, idct64_low16_avx2, idct64_avx2 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };

static void highbd_inv_txfm2d_add_no_identity_avx2(const int32_t *input,
                                                   uint16_t *output, int stride,
                                                   TX_TYPE tx_type,
                                                   TX_SIZE tx_size, int eob,
                                                   const int bd) {
  __m256i buf1[64 * 8];
  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_nonzero_w = (eobx + 8) >> 3 << 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int input_stride = AOMMIN(32, txfm_size_row);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_avx2 row_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_avx2 col_txfm =
      highbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  // 1st stage: column transform
  for (int i = 0; i < buf_size_nonzero_h_div8; i++) {
    __m256i buf0[64];
    load_buffer_32bit_input(input + i * 8, input_stride, buf0,
                            buf_size_nonzero_w);
    if (rect_type == 1 || rect_type == -1) {
      round_shift_rect_array_32_avx2(buf0, buf0, buf_size_nonzero_w, 0,
                                     NewInvSqrt2);
    }
    row_txfm(buf0, buf0, INV_COS_BIT, 0, bd, -shift[0]);

    __m256i *_buf1 = buf1 + i * 8;
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        transpose_8x8_flip_avx2(
            &buf0[j * 8], &_buf1[(buf_size_w_div8 - 1 - j) * txfm_size_row]);
      }
    } else {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        transpose_8x8_avx2(&buf0[j * 8], &_buf1[j * txfm_size_row]);
      }
    }
  }
  // 2nd stage: column transform
  for (int i = 0; i < buf_size_w_div8; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row, INV_COS_BIT, 1,
             bd, 0);

    round_shift_array_32_avx2(buf1 + i * txfm_size_row,
                              buf1 + i * txfm_size_row, txfm_size_row,
                              -shift[1]);
  }

  // write to buffer
  if (txfm_size_col >= 16) {
    for (int i = 0; i < (txfm_size_col >> 4); i++) {
      highbd_write_buffer_16xn_avx2(buf1 + i * txfm_size_row * 2,
                                    output + 16 * i, stride, ud_flip,
                                    txfm_size_row, bd);
    }
  } else if (txfm_size_col == 8) {
    highbd_write_buffer_8xn_avx2(buf1, output, stride, ud_flip, txfm_size_row,
                                 bd);
  }
}

static void av1_highbd_inv_txfm2d_add_universe_avx2(const int32_t *input,
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
      highbd_inv_txfm2d_add_no_identity_avx2(input, CONVERT_TO_SHORTPTR(output),
                                             stride, tx_type, tx_size, eob, bd);
      break;
    case IDTX:
    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      av1_highbd_inv_txfm2d_add_universe_sse4_1(input, output, stride, tx_type,
                                                tx_size, eob, bd);
      break;
    default: assert(0); break;
  }
}
void av1_highbd_inv_txfm_add_avx2(const tran_low_t *input, uint8_t *dest,
                                  int stride, const TxfmParam *txfm_param) {
  assert(av1_ext_tx_used[txfm_param->tx_set_type][txfm_param->tx_type]);
  const TX_SIZE tx_size = txfm_param->tx_size;
  switch (tx_size) {
    case TX_4X8:
    case TX_8X4:
    case TX_4X4:
    case TX_16X4:
    case TX_4X16:
      av1_highbd_inv_txfm_add_sse4_1(input, dest, stride, txfm_param);
      break;
    default:
      av1_highbd_inv_txfm2d_add_universe_avx2(
          input, dest, stride, txfm_param->tx_type, txfm_param->tx_size,
          txfm_param->eob, txfm_param->bd);
      break;
  }
}
