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

#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/encoder/av1_fwd_txfm1d_cfg.h"
#include "av1/encoder/x86/av1_fwd_txfm_sse2.h"

// TODO(linfengz): refine fdct4x8 and fadst4x8 optimization (if possible).

static void fdct4x4_new_sse2(const __m128i *input, __m128i *output,
                             int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));
  __m128i u[4], v[4];

  u[0] = _mm_unpacklo_epi16(input[0], input[1]);
  u[1] = _mm_unpacklo_epi16(input[3], input[2]);

  v[0] = _mm_add_epi16(u[0], u[1]);
  v[1] = _mm_sub_epi16(u[0], u[1]);

  u[0] = _mm_madd_epi16(v[0], cospi_p32_p32);  // 0
  u[1] = _mm_madd_epi16(v[0], cospi_p32_m32);  // 2
  u[2] = _mm_madd_epi16(v[1], cospi_p16_p48);  // 1
  u[3] = _mm_madd_epi16(v[1], cospi_p48_m16);  // 3

  v[0] = _mm_add_epi32(u[0], __rounding);
  v[1] = _mm_add_epi32(u[1], __rounding);
  v[2] = _mm_add_epi32(u[2], __rounding);
  v[3] = _mm_add_epi32(u[3], __rounding);
  u[0] = _mm_srai_epi32(v[0], cos_bit);
  u[1] = _mm_srai_epi32(v[1], cos_bit);
  u[2] = _mm_srai_epi32(v[2], cos_bit);
  u[3] = _mm_srai_epi32(v[3], cos_bit);

  output[0] = _mm_packs_epi32(u[0], u[1]);
  output[1] = _mm_packs_epi32(u[2], u[3]);
  output[2] = _mm_srli_si128(output[0], 8);
  output[3] = _mm_srli_si128(output[1], 8);
}

static void fdct8x4_new_sse2(const __m128i *input, __m128i *output,
                             int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);

  // stage 1
  __m128i x1[4];
  x1[0] = _mm_adds_epi16(input[0], input[3]);
  x1[3] = _mm_subs_epi16(input[0], input[3]);
  x1[1] = _mm_adds_epi16(input[1], input[2]);
  x1[2] = _mm_subs_epi16(input[1], input[2]);

  // stage 2
  __m128i x2[4];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x1[0], x1[1], x2[0], x2[1]);
  btf_16_sse2(cospi_p48_p16, cospi_m16_p48, x1[2], x1[3], x2[2], x2[3]);

  // stage 3
  output[0] = x2[0];
  output[1] = x2[2];
  output[2] = x2[1];
  output[3] = x2[3];
}

static void fdct4x8_new_sse2(const __m128i *input, __m128i *output,
                             int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);

  // stage 1
  __m128i x1[8];
  x1[0] = _mm_adds_epi16(input[0], input[7]);
  x1[7] = _mm_subs_epi16(input[0], input[7]);
  x1[1] = _mm_adds_epi16(input[1], input[6]);
  x1[6] = _mm_subs_epi16(input[1], input[6]);
  x1[2] = _mm_adds_epi16(input[2], input[5]);
  x1[5] = _mm_subs_epi16(input[2], input[5]);
  x1[3] = _mm_adds_epi16(input[3], input[4]);
  x1[4] = _mm_subs_epi16(input[3], input[4]);

  // stage 2
  __m128i x2[8];
  x2[0] = _mm_adds_epi16(x1[0], x1[3]);
  x2[3] = _mm_subs_epi16(x1[0], x1[3]);
  x2[1] = _mm_adds_epi16(x1[1], x1[2]);
  x2[2] = _mm_subs_epi16(x1[1], x1[2]);
  x2[4] = x1[4];
  btf_16_w4_sse2(&cospi_m32_p32, &cospi_p32_p32, __rounding, cos_bit, &x1[5],
                 &x1[6], &x2[5], &x2[6]);
  x2[7] = x1[7];

  // stage 3
  __m128i x3[8];
  btf_16_w4_sse2(&cospi_p32_p32, &cospi_p32_m32, __rounding, cos_bit, &x2[0],
                 &x2[1], &x3[0], &x3[1]);
  btf_16_w4_sse2(&cospi_p48_p16, &cospi_m16_p48, __rounding, cos_bit, &x2[2],
                 &x2[3], &x3[2], &x3[3]);
  x3[4] = _mm_adds_epi16(x2[4], x2[5]);
  x3[5] = _mm_subs_epi16(x2[4], x2[5]);
  x3[6] = _mm_subs_epi16(x2[7], x2[6]);
  x3[7] = _mm_adds_epi16(x2[7], x2[6]);

  // stage 4
  __m128i x4[8];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_w4_sse2(&cospi_p56_p08, &cospi_m08_p56, __rounding, cos_bit, &x3[4],
                 &x3[7], &x4[4], &x4[7]);
  btf_16_w4_sse2(&cospi_p24_p40, &cospi_m40_p24, __rounding, cos_bit, &x3[5],
                 &x3[6], &x4[5], &x4[6]);

  // stage 5
  output[0] = x4[0];
  output[1] = x4[4];
  output[2] = x4[2];
  output[3] = x4[6];
  output[4] = x4[1];
  output[5] = x4[5];
  output[6] = x4[3];
  output[7] = x4[7];
}

static void fdct8x8_new_sse2(const __m128i *input, __m128i *output,
                             int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);

  // stage 1
  __m128i x1[8];
  x1[0] = _mm_adds_epi16(input[0], input[7]);
  x1[7] = _mm_subs_epi16(input[0], input[7]);
  x1[1] = _mm_adds_epi16(input[1], input[6]);
  x1[6] = _mm_subs_epi16(input[1], input[6]);
  x1[2] = _mm_adds_epi16(input[2], input[5]);
  x1[5] = _mm_subs_epi16(input[2], input[5]);
  x1[3] = _mm_adds_epi16(input[3], input[4]);
  x1[4] = _mm_subs_epi16(input[3], input[4]);

  // stage 2
  __m128i x2[8];
  x2[0] = _mm_adds_epi16(x1[0], x1[3]);
  x2[3] = _mm_subs_epi16(x1[0], x1[3]);
  x2[1] = _mm_adds_epi16(x1[1], x1[2]);
  x2[2] = _mm_subs_epi16(x1[1], x1[2]);
  x2[4] = x1[4];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[5], x1[6], x2[5], x2[6]);
  x2[7] = x1[7];

  // stage 3
  __m128i x3[8];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x2[0], x2[1], x3[0], x3[1]);
  btf_16_sse2(cospi_p48_p16, cospi_m16_p48, x2[2], x2[3], x3[2], x3[3]);
  x3[4] = _mm_adds_epi16(x2[4], x2[5]);
  x3[5] = _mm_subs_epi16(x2[4], x2[5]);
  x3[6] = _mm_subs_epi16(x2[7], x2[6]);
  x3[7] = _mm_adds_epi16(x2[7], x2[6]);

  // stage 4
  __m128i x4[8];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_sse2(cospi_p56_p08, cospi_m08_p56, x3[4], x3[7], x4[4], x4[7]);
  btf_16_sse2(cospi_p24_p40, cospi_m40_p24, x3[5], x3[6], x4[5], x4[6]);

  // stage 5
  output[0] = x4[0];
  output[1] = x4[4];
  output[2] = x4[2];
  output[3] = x4[6];
  output[4] = x4[1];
  output[5] = x4[5];
  output[6] = x4[3];
  output[7] = x4[7];
}

static void fdct8x16_new_sse2(const __m128i *input, __m128i *output,
                              int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  __m128i cospi_p60_p04 = pair_set_epi16(cospi[60], cospi[4]);
  __m128i cospi_m04_p60 = pair_set_epi16(-cospi[4], cospi[60]);
  __m128i cospi_p28_p36 = pair_set_epi16(cospi[28], cospi[36]);
  __m128i cospi_m36_p28 = pair_set_epi16(-cospi[36], cospi[28]);
  __m128i cospi_p44_p20 = pair_set_epi16(cospi[44], cospi[20]);
  __m128i cospi_m20_p44 = pair_set_epi16(-cospi[20], cospi[44]);
  __m128i cospi_p12_p52 = pair_set_epi16(cospi[12], cospi[52]);
  __m128i cospi_m52_p12 = pair_set_epi16(-cospi[52], cospi[12]);

  // stage 1
  __m128i x1[16];
  x1[0] = _mm_adds_epi16(input[0], input[15]);
  x1[15] = _mm_subs_epi16(input[0], input[15]);
  x1[1] = _mm_adds_epi16(input[1], input[14]);
  x1[14] = _mm_subs_epi16(input[1], input[14]);
  x1[2] = _mm_adds_epi16(input[2], input[13]);
  x1[13] = _mm_subs_epi16(input[2], input[13]);
  x1[3] = _mm_adds_epi16(input[3], input[12]);
  x1[12] = _mm_subs_epi16(input[3], input[12]);
  x1[4] = _mm_adds_epi16(input[4], input[11]);
  x1[11] = _mm_subs_epi16(input[4], input[11]);
  x1[5] = _mm_adds_epi16(input[5], input[10]);
  x1[10] = _mm_subs_epi16(input[5], input[10]);
  x1[6] = _mm_adds_epi16(input[6], input[9]);
  x1[9] = _mm_subs_epi16(input[6], input[9]);
  x1[7] = _mm_adds_epi16(input[7], input[8]);
  x1[8] = _mm_subs_epi16(input[7], input[8]);

  // stage 2
  __m128i x2[16];
  x2[0] = _mm_adds_epi16(x1[0], x1[7]);
  x2[7] = _mm_subs_epi16(x1[0], x1[7]);
  x2[1] = _mm_adds_epi16(x1[1], x1[6]);
  x2[6] = _mm_subs_epi16(x1[1], x1[6]);
  x2[2] = _mm_adds_epi16(x1[2], x1[5]);
  x2[5] = _mm_subs_epi16(x1[2], x1[5]);
  x2[3] = _mm_adds_epi16(x1[3], x1[4]);
  x2[4] = _mm_subs_epi16(x1[3], x1[4]);
  x2[8] = x1[8];
  x2[9] = x1[9];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[10], x1[13], x2[10], x2[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[11], x1[12], x2[11], x2[12]);
  x2[14] = x1[14];
  x2[15] = x1[15];

  // stage 3
  __m128i x3[16];
  x3[0] = _mm_adds_epi16(x2[0], x2[3]);
  x3[3] = _mm_subs_epi16(x2[0], x2[3]);
  x3[1] = _mm_adds_epi16(x2[1], x2[2]);
  x3[2] = _mm_subs_epi16(x2[1], x2[2]);
  x3[4] = x2[4];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x2[5], x2[6], x3[5], x3[6]);
  x3[7] = x2[7];
  x3[8] = _mm_adds_epi16(x2[8], x2[11]);
  x3[11] = _mm_subs_epi16(x2[8], x2[11]);
  x3[9] = _mm_adds_epi16(x2[9], x2[10]);
  x3[10] = _mm_subs_epi16(x2[9], x2[10]);
  x3[12] = _mm_subs_epi16(x2[15], x2[12]);
  x3[15] = _mm_adds_epi16(x2[15], x2[12]);
  x3[13] = _mm_subs_epi16(x2[14], x2[13]);
  x3[14] = _mm_adds_epi16(x2[14], x2[13]);

  // stage 4
  __m128i x4[16];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x3[0], x3[1], x4[0], x4[1]);
  btf_16_sse2(cospi_p48_p16, cospi_m16_p48, x3[2], x3[3], x4[2], x4[3]);
  x4[4] = _mm_adds_epi16(x3[4], x3[5]);
  x4[5] = _mm_subs_epi16(x3[4], x3[5]);
  x4[6] = _mm_subs_epi16(x3[7], x3[6]);
  x4[7] = _mm_adds_epi16(x3[7], x3[6]);
  x4[8] = x3[8];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x3[9], x3[14], x4[9], x4[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x3[10], x3[13], x4[10], x4[13]);
  x4[11] = x3[11];
  x4[12] = x3[12];
  x4[15] = x3[15];

  // stage 5
  __m128i x5[16];
  x5[0] = x4[0];
  x5[1] = x4[1];
  x5[2] = x4[2];
  x5[3] = x4[3];
  btf_16_sse2(cospi_p56_p08, cospi_m08_p56, x4[4], x4[7], x5[4], x5[7]);
  btf_16_sse2(cospi_p24_p40, cospi_m40_p24, x4[5], x4[6], x5[5], x5[6]);
  x5[8] = _mm_adds_epi16(x4[8], x4[9]);
  x5[9] = _mm_subs_epi16(x4[8], x4[9]);
  x5[10] = _mm_subs_epi16(x4[11], x4[10]);
  x5[11] = _mm_adds_epi16(x4[11], x4[10]);
  x5[12] = _mm_adds_epi16(x4[12], x4[13]);
  x5[13] = _mm_subs_epi16(x4[12], x4[13]);
  x5[14] = _mm_subs_epi16(x4[15], x4[14]);
  x5[15] = _mm_adds_epi16(x4[15], x4[14]);

  // stage 6
  __m128i x6[16];
  x6[0] = x5[0];
  x6[1] = x5[1];
  x6[2] = x5[2];
  x6[3] = x5[3];
  x6[4] = x5[4];
  x6[5] = x5[5];
  x6[6] = x5[6];
  x6[7] = x5[7];
  btf_16_sse2(cospi_p60_p04, cospi_m04_p60, x5[8], x5[15], x6[8], x6[15]);
  btf_16_sse2(cospi_p28_p36, cospi_m36_p28, x5[9], x5[14], x6[9], x6[14]);
  btf_16_sse2(cospi_p44_p20, cospi_m20_p44, x5[10], x5[13], x6[10], x6[13]);
  btf_16_sse2(cospi_p12_p52, cospi_m52_p12, x5[11], x5[12], x6[11], x6[12]);

  // stage 7
  output[0] = x6[0];
  output[1] = x6[8];
  output[2] = x6[4];
  output[3] = x6[12];
  output[4] = x6[2];
  output[5] = x6[10];
  output[6] = x6[6];
  output[7] = x6[14];
  output[8] = x6[1];
  output[9] = x6[9];
  output[10] = x6[5];
  output[11] = x6[13];
  output[12] = x6[3];
  output[13] = x6[11];
  output[14] = x6[7];
  output[15] = x6[15];
}

void av1_fdct8x32_new_sse2(const __m128i *input, __m128i *output,
                           int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  __m128i cospi_m56_m08 = pair_set_epi16(-cospi[56], -cospi[8]);
  __m128i cospi_m24_m40 = pair_set_epi16(-cospi[24], -cospi[40]);
  __m128i cospi_p60_p04 = pair_set_epi16(cospi[60], cospi[4]);
  __m128i cospi_m04_p60 = pair_set_epi16(-cospi[4], cospi[60]);
  __m128i cospi_p28_p36 = pair_set_epi16(cospi[28], cospi[36]);
  __m128i cospi_m36_p28 = pair_set_epi16(-cospi[36], cospi[28]);
  __m128i cospi_p44_p20 = pair_set_epi16(cospi[44], cospi[20]);
  __m128i cospi_m20_p44 = pair_set_epi16(-cospi[20], cospi[44]);
  __m128i cospi_p12_p52 = pair_set_epi16(cospi[12], cospi[52]);
  __m128i cospi_m52_p12 = pair_set_epi16(-cospi[52], cospi[12]);
  __m128i cospi_p62_p02 = pair_set_epi16(cospi[62], cospi[2]);
  __m128i cospi_m02_p62 = pair_set_epi16(-cospi[2], cospi[62]);
  __m128i cospi_p30_p34 = pair_set_epi16(cospi[30], cospi[34]);
  __m128i cospi_m34_p30 = pair_set_epi16(-cospi[34], cospi[30]);
  __m128i cospi_p46_p18 = pair_set_epi16(cospi[46], cospi[18]);
  __m128i cospi_m18_p46 = pair_set_epi16(-cospi[18], cospi[46]);
  __m128i cospi_p14_p50 = pair_set_epi16(cospi[14], cospi[50]);
  __m128i cospi_m50_p14 = pair_set_epi16(-cospi[50], cospi[14]);
  __m128i cospi_p54_p10 = pair_set_epi16(cospi[54], cospi[10]);
  __m128i cospi_m10_p54 = pair_set_epi16(-cospi[10], cospi[54]);
  __m128i cospi_p22_p42 = pair_set_epi16(cospi[22], cospi[42]);
  __m128i cospi_m42_p22 = pair_set_epi16(-cospi[42], cospi[22]);
  __m128i cospi_p38_p26 = pair_set_epi16(cospi[38], cospi[26]);
  __m128i cospi_m26_p38 = pair_set_epi16(-cospi[26], cospi[38]);
  __m128i cospi_p06_p58 = pair_set_epi16(cospi[6], cospi[58]);
  __m128i cospi_m58_p06 = pair_set_epi16(-cospi[58], cospi[6]);

  // stage 1
  __m128i x1[32];
  x1[0] = _mm_adds_epi16(input[0], input[31]);
  x1[31] = _mm_subs_epi16(input[0], input[31]);
  x1[1] = _mm_adds_epi16(input[1], input[30]);
  x1[30] = _mm_subs_epi16(input[1], input[30]);
  x1[2] = _mm_adds_epi16(input[2], input[29]);
  x1[29] = _mm_subs_epi16(input[2], input[29]);
  x1[3] = _mm_adds_epi16(input[3], input[28]);
  x1[28] = _mm_subs_epi16(input[3], input[28]);
  x1[4] = _mm_adds_epi16(input[4], input[27]);
  x1[27] = _mm_subs_epi16(input[4], input[27]);
  x1[5] = _mm_adds_epi16(input[5], input[26]);
  x1[26] = _mm_subs_epi16(input[5], input[26]);
  x1[6] = _mm_adds_epi16(input[6], input[25]);
  x1[25] = _mm_subs_epi16(input[6], input[25]);
  x1[7] = _mm_adds_epi16(input[7], input[24]);
  x1[24] = _mm_subs_epi16(input[7], input[24]);
  x1[8] = _mm_adds_epi16(input[8], input[23]);
  x1[23] = _mm_subs_epi16(input[8], input[23]);
  x1[9] = _mm_adds_epi16(input[9], input[22]);
  x1[22] = _mm_subs_epi16(input[9], input[22]);
  x1[10] = _mm_adds_epi16(input[10], input[21]);
  x1[21] = _mm_subs_epi16(input[10], input[21]);
  x1[11] = _mm_adds_epi16(input[11], input[20]);
  x1[20] = _mm_subs_epi16(input[11], input[20]);
  x1[12] = _mm_adds_epi16(input[12], input[19]);
  x1[19] = _mm_subs_epi16(input[12], input[19]);
  x1[13] = _mm_adds_epi16(input[13], input[18]);
  x1[18] = _mm_subs_epi16(input[13], input[18]);
  x1[14] = _mm_adds_epi16(input[14], input[17]);
  x1[17] = _mm_subs_epi16(input[14], input[17]);
  x1[15] = _mm_adds_epi16(input[15], input[16]);
  x1[16] = _mm_subs_epi16(input[15], input[16]);

  // stage 2
  __m128i x2[32];
  x2[0] = _mm_adds_epi16(x1[0], x1[15]);
  x2[15] = _mm_subs_epi16(x1[0], x1[15]);
  x2[1] = _mm_adds_epi16(x1[1], x1[14]);
  x2[14] = _mm_subs_epi16(x1[1], x1[14]);
  x2[2] = _mm_adds_epi16(x1[2], x1[13]);
  x2[13] = _mm_subs_epi16(x1[2], x1[13]);
  x2[3] = _mm_adds_epi16(x1[3], x1[12]);
  x2[12] = _mm_subs_epi16(x1[3], x1[12]);
  x2[4] = _mm_adds_epi16(x1[4], x1[11]);
  x2[11] = _mm_subs_epi16(x1[4], x1[11]);
  x2[5] = _mm_adds_epi16(x1[5], x1[10]);
  x2[10] = _mm_subs_epi16(x1[5], x1[10]);
  x2[6] = _mm_adds_epi16(x1[6], x1[9]);
  x2[9] = _mm_subs_epi16(x1[6], x1[9]);
  x2[7] = _mm_adds_epi16(x1[7], x1[8]);
  x2[8] = _mm_subs_epi16(x1[7], x1[8]);
  x2[16] = x1[16];
  x2[17] = x1[17];
  x2[18] = x1[18];
  x2[19] = x1[19];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[20], x1[27], x2[20], x2[27]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[21], x1[26], x2[21], x2[26]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[22], x1[25], x2[22], x2[25]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[23], x1[24], x2[23], x2[24]);
  x2[28] = x1[28];
  x2[29] = x1[29];
  x2[30] = x1[30];
  x2[31] = x1[31];

  // stage 3
  __m128i x3[32];
  x3[0] = _mm_adds_epi16(x2[0], x2[7]);
  x3[7] = _mm_subs_epi16(x2[0], x2[7]);
  x3[1] = _mm_adds_epi16(x2[1], x2[6]);
  x3[6] = _mm_subs_epi16(x2[1], x2[6]);
  x3[2] = _mm_adds_epi16(x2[2], x2[5]);
  x3[5] = _mm_subs_epi16(x2[2], x2[5]);
  x3[3] = _mm_adds_epi16(x2[3], x2[4]);
  x3[4] = _mm_subs_epi16(x2[3], x2[4]);
  x3[8] = x2[8];
  x3[9] = x2[9];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x2[10], x2[13], x3[10], x3[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x2[11], x2[12], x3[11], x3[12]);
  x3[14] = x2[14];
  x3[15] = x2[15];
  x3[16] = _mm_adds_epi16(x2[16], x2[23]);
  x3[23] = _mm_subs_epi16(x2[16], x2[23]);
  x3[17] = _mm_adds_epi16(x2[17], x2[22]);
  x3[22] = _mm_subs_epi16(x2[17], x2[22]);
  x3[18] = _mm_adds_epi16(x2[18], x2[21]);
  x3[21] = _mm_subs_epi16(x2[18], x2[21]);
  x3[19] = _mm_adds_epi16(x2[19], x2[20]);
  x3[20] = _mm_subs_epi16(x2[19], x2[20]);
  x3[24] = _mm_subs_epi16(x2[31], x2[24]);
  x3[31] = _mm_adds_epi16(x2[31], x2[24]);
  x3[25] = _mm_subs_epi16(x2[30], x2[25]);
  x3[30] = _mm_adds_epi16(x2[30], x2[25]);
  x3[26] = _mm_subs_epi16(x2[29], x2[26]);
  x3[29] = _mm_adds_epi16(x2[29], x2[26]);
  x3[27] = _mm_subs_epi16(x2[28], x2[27]);
  x3[28] = _mm_adds_epi16(x2[28], x2[27]);

  // stage 4
  __m128i x4[32];
  x4[0] = _mm_adds_epi16(x3[0], x3[3]);
  x4[3] = _mm_subs_epi16(x3[0], x3[3]);
  x4[1] = _mm_adds_epi16(x3[1], x3[2]);
  x4[2] = _mm_subs_epi16(x3[1], x3[2]);
  x4[4] = x3[4];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x3[5], x3[6], x4[5], x4[6]);
  x4[7] = x3[7];
  x4[8] = _mm_adds_epi16(x3[8], x3[11]);
  x4[11] = _mm_subs_epi16(x3[8], x3[11]);
  x4[9] = _mm_adds_epi16(x3[9], x3[10]);
  x4[10] = _mm_subs_epi16(x3[9], x3[10]);
  x4[12] = _mm_subs_epi16(x3[15], x3[12]);
  x4[15] = _mm_adds_epi16(x3[15], x3[12]);
  x4[13] = _mm_subs_epi16(x3[14], x3[13]);
  x4[14] = _mm_adds_epi16(x3[14], x3[13]);
  x4[16] = x3[16];
  x4[17] = x3[17];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x3[18], x3[29], x4[18], x4[29]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x3[19], x3[28], x4[19], x4[28]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x3[20], x3[27], x4[20], x4[27]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x3[21], x3[26], x4[21], x4[26]);
  x4[22] = x3[22];
  x4[23] = x3[23];
  x4[24] = x3[24];
  x4[25] = x3[25];
  x4[30] = x3[30];
  x4[31] = x3[31];

  // stage 5
  __m128i x5[32];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x4[0], x4[1], x5[0], x5[1]);
  btf_16_sse2(cospi_p48_p16, cospi_m16_p48, x4[2], x4[3], x5[2], x5[3]);
  x5[4] = _mm_adds_epi16(x4[4], x4[5]);
  x5[5] = _mm_subs_epi16(x4[4], x4[5]);
  x5[6] = _mm_subs_epi16(x4[7], x4[6]);
  x5[7] = _mm_adds_epi16(x4[7], x4[6]);
  x5[8] = x4[8];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x4[9], x4[14], x5[9], x5[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x4[10], x4[13], x5[10], x5[13]);
  x5[11] = x4[11];
  x5[12] = x4[12];
  x5[15] = x4[15];
  x5[16] = _mm_adds_epi16(x4[16], x4[19]);
  x5[19] = _mm_subs_epi16(x4[16], x4[19]);
  x5[17] = _mm_adds_epi16(x4[17], x4[18]);
  x5[18] = _mm_subs_epi16(x4[17], x4[18]);
  x5[20] = _mm_subs_epi16(x4[23], x4[20]);
  x5[23] = _mm_adds_epi16(x4[23], x4[20]);
  x5[21] = _mm_subs_epi16(x4[22], x4[21]);
  x5[22] = _mm_adds_epi16(x4[22], x4[21]);
  x5[24] = _mm_adds_epi16(x4[24], x4[27]);
  x5[27] = _mm_subs_epi16(x4[24], x4[27]);
  x5[25] = _mm_adds_epi16(x4[25], x4[26]);
  x5[26] = _mm_subs_epi16(x4[25], x4[26]);
  x5[28] = _mm_subs_epi16(x4[31], x4[28]);
  x5[31] = _mm_adds_epi16(x4[31], x4[28]);
  x5[29] = _mm_subs_epi16(x4[30], x4[29]);
  x5[30] = _mm_adds_epi16(x4[30], x4[29]);

  // stage 6
  __m128i x6[32];
  x6[0] = x5[0];
  x6[1] = x5[1];
  x6[2] = x5[2];
  x6[3] = x5[3];
  btf_16_sse2(cospi_p56_p08, cospi_m08_p56, x5[4], x5[7], x6[4], x6[7]);
  btf_16_sse2(cospi_p24_p40, cospi_m40_p24, x5[5], x5[6], x6[5], x6[6]);
  x6[8] = _mm_adds_epi16(x5[8], x5[9]);
  x6[9] = _mm_subs_epi16(x5[8], x5[9]);
  x6[10] = _mm_subs_epi16(x5[11], x5[10]);
  x6[11] = _mm_adds_epi16(x5[11], x5[10]);
  x6[12] = _mm_adds_epi16(x5[12], x5[13]);
  x6[13] = _mm_subs_epi16(x5[12], x5[13]);
  x6[14] = _mm_subs_epi16(x5[15], x5[14]);
  x6[15] = _mm_adds_epi16(x5[15], x5[14]);
  x6[16] = x5[16];
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x5[17], x5[30], x6[17], x6[30]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x5[18], x5[29], x6[18], x6[29]);
  x6[19] = x5[19];
  x6[20] = x5[20];
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x5[21], x5[26], x6[21], x6[26]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x5[22], x5[25], x6[22], x6[25]);
  x6[23] = x5[23];
  x6[24] = x5[24];
  x6[27] = x5[27];
  x6[28] = x5[28];
  x6[31] = x5[31];

  // stage 7
  __m128i x7[32];
  x7[0] = x6[0];
  x7[1] = x6[1];
  x7[2] = x6[2];
  x7[3] = x6[3];
  x7[4] = x6[4];
  x7[5] = x6[5];
  x7[6] = x6[6];
  x7[7] = x6[7];
  btf_16_sse2(cospi_p60_p04, cospi_m04_p60, x6[8], x6[15], x7[8], x7[15]);
  btf_16_sse2(cospi_p28_p36, cospi_m36_p28, x6[9], x6[14], x7[9], x7[14]);
  btf_16_sse2(cospi_p44_p20, cospi_m20_p44, x6[10], x6[13], x7[10], x7[13]);
  btf_16_sse2(cospi_p12_p52, cospi_m52_p12, x6[11], x6[12], x7[11], x7[12]);
  x7[16] = _mm_adds_epi16(x6[16], x6[17]);
  x7[17] = _mm_subs_epi16(x6[16], x6[17]);
  x7[18] = _mm_subs_epi16(x6[19], x6[18]);
  x7[19] = _mm_adds_epi16(x6[19], x6[18]);
  x7[20] = _mm_adds_epi16(x6[20], x6[21]);
  x7[21] = _mm_subs_epi16(x6[20], x6[21]);
  x7[22] = _mm_subs_epi16(x6[23], x6[22]);
  x7[23] = _mm_adds_epi16(x6[23], x6[22]);
  x7[24] = _mm_adds_epi16(x6[24], x6[25]);
  x7[25] = _mm_subs_epi16(x6[24], x6[25]);
  x7[26] = _mm_subs_epi16(x6[27], x6[26]);
  x7[27] = _mm_adds_epi16(x6[27], x6[26]);
  x7[28] = _mm_adds_epi16(x6[28], x6[29]);
  x7[29] = _mm_subs_epi16(x6[28], x6[29]);
  x7[30] = _mm_subs_epi16(x6[31], x6[30]);
  x7[31] = _mm_adds_epi16(x6[31], x6[30]);

  // stage 8
  __m128i x8[32];
  x8[0] = x7[0];
  x8[1] = x7[1];
  x8[2] = x7[2];
  x8[3] = x7[3];
  x8[4] = x7[4];
  x8[5] = x7[5];
  x8[6] = x7[6];
  x8[7] = x7[7];
  x8[8] = x7[8];
  x8[9] = x7[9];
  x8[10] = x7[10];
  x8[11] = x7[11];
  x8[12] = x7[12];
  x8[13] = x7[13];
  x8[14] = x7[14];
  x8[15] = x7[15];
  btf_16_sse2(cospi_p62_p02, cospi_m02_p62, x7[16], x7[31], x8[16], x8[31]);
  btf_16_sse2(cospi_p30_p34, cospi_m34_p30, x7[17], x7[30], x8[17], x8[30]);
  btf_16_sse2(cospi_p46_p18, cospi_m18_p46, x7[18], x7[29], x8[18], x8[29]);
  btf_16_sse2(cospi_p14_p50, cospi_m50_p14, x7[19], x7[28], x8[19], x8[28]);
  btf_16_sse2(cospi_p54_p10, cospi_m10_p54, x7[20], x7[27], x8[20], x8[27]);
  btf_16_sse2(cospi_p22_p42, cospi_m42_p22, x7[21], x7[26], x8[21], x8[26]);
  btf_16_sse2(cospi_p38_p26, cospi_m26_p38, x7[22], x7[25], x8[22], x8[25]);
  btf_16_sse2(cospi_p06_p58, cospi_m58_p06, x7[23], x7[24], x8[23], x8[24]);

  // stage 9
  output[0] = x8[0];
  output[1] = x8[16];
  output[2] = x8[8];
  output[3] = x8[24];
  output[4] = x8[4];
  output[5] = x8[20];
  output[6] = x8[12];
  output[7] = x8[28];
  output[8] = x8[2];
  output[9] = x8[18];
  output[10] = x8[10];
  output[11] = x8[26];
  output[12] = x8[6];
  output[13] = x8[22];
  output[14] = x8[14];
  output[15] = x8[30];
  output[16] = x8[1];
  output[17] = x8[17];
  output[18] = x8[9];
  output[19] = x8[25];
  output[20] = x8[5];
  output[21] = x8[21];
  output[22] = x8[13];
  output[23] = x8[29];
  output[24] = x8[3];
  output[25] = x8[19];
  output[26] = x8[11];
  output[27] = x8[27];
  output[28] = x8[7];
  output[29] = x8[23];
  output[30] = x8[15];
  output[31] = x8[31];
}

void av1_fdct8x64_new_sse2(const __m128i *input, __m128i *output,
                           int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  __m128i cospi_m56_m08 = pair_set_epi16(-cospi[56], -cospi[8]);
  __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  __m128i cospi_m24_m40 = pair_set_epi16(-cospi[24], -cospi[40]);
  __m128i cospi_p60_p04 = pair_set_epi16(cospi[60], cospi[4]);
  __m128i cospi_m04_p60 = pair_set_epi16(-cospi[4], cospi[60]);
  __m128i cospi_p28_p36 = pair_set_epi16(cospi[28], cospi[36]);
  __m128i cospi_m36_p28 = pair_set_epi16(-cospi[36], cospi[28]);
  __m128i cospi_p44_p20 = pair_set_epi16(cospi[44], cospi[20]);
  __m128i cospi_m20_p44 = pair_set_epi16(-cospi[20], cospi[44]);
  __m128i cospi_p12_p52 = pair_set_epi16(cospi[12], cospi[52]);
  __m128i cospi_m52_p12 = pair_set_epi16(-cospi[52], cospi[12]);
  __m128i cospi_m60_m04 = pair_set_epi16(-cospi[60], -cospi[4]);
  __m128i cospi_m28_m36 = pair_set_epi16(-cospi[28], -cospi[36]);
  __m128i cospi_m44_m20 = pair_set_epi16(-cospi[44], -cospi[20]);
  __m128i cospi_m12_m52 = pair_set_epi16(-cospi[12], -cospi[52]);
  __m128i cospi_p62_p02 = pair_set_epi16(cospi[62], cospi[2]);
  __m128i cospi_m02_p62 = pair_set_epi16(-cospi[2], cospi[62]);
  __m128i cospi_p30_p34 = pair_set_epi16(cospi[30], cospi[34]);
  __m128i cospi_m34_p30 = pair_set_epi16(-cospi[34], cospi[30]);
  __m128i cospi_p46_p18 = pair_set_epi16(cospi[46], cospi[18]);
  __m128i cospi_m18_p46 = pair_set_epi16(-cospi[18], cospi[46]);
  __m128i cospi_p14_p50 = pair_set_epi16(cospi[14], cospi[50]);
  __m128i cospi_m50_p14 = pair_set_epi16(-cospi[50], cospi[14]);
  __m128i cospi_p54_p10 = pair_set_epi16(cospi[54], cospi[10]);
  __m128i cospi_m10_p54 = pair_set_epi16(-cospi[10], cospi[54]);
  __m128i cospi_p22_p42 = pair_set_epi16(cospi[22], cospi[42]);
  __m128i cospi_m42_p22 = pair_set_epi16(-cospi[42], cospi[22]);
  __m128i cospi_p38_p26 = pair_set_epi16(cospi[38], cospi[26]);
  __m128i cospi_m26_p38 = pair_set_epi16(-cospi[26], cospi[38]);
  __m128i cospi_p06_p58 = pair_set_epi16(cospi[6], cospi[58]);
  __m128i cospi_m58_p06 = pair_set_epi16(-cospi[58], cospi[6]);
  __m128i cospi_p63_p01 = pair_set_epi16(cospi[63], cospi[1]);
  __m128i cospi_m01_p63 = pair_set_epi16(-cospi[1], cospi[63]);
  __m128i cospi_p31_p33 = pair_set_epi16(cospi[31], cospi[33]);
  __m128i cospi_m33_p31 = pair_set_epi16(-cospi[33], cospi[31]);
  __m128i cospi_p47_p17 = pair_set_epi16(cospi[47], cospi[17]);
  __m128i cospi_m17_p47 = pair_set_epi16(-cospi[17], cospi[47]);
  __m128i cospi_p15_p49 = pair_set_epi16(cospi[15], cospi[49]);
  __m128i cospi_m49_p15 = pair_set_epi16(-cospi[49], cospi[15]);
  __m128i cospi_p55_p09 = pair_set_epi16(cospi[55], cospi[9]);
  __m128i cospi_m09_p55 = pair_set_epi16(-cospi[9], cospi[55]);
  __m128i cospi_p23_p41 = pair_set_epi16(cospi[23], cospi[41]);
  __m128i cospi_m41_p23 = pair_set_epi16(-cospi[41], cospi[23]);
  __m128i cospi_p39_p25 = pair_set_epi16(cospi[39], cospi[25]);
  __m128i cospi_m25_p39 = pair_set_epi16(-cospi[25], cospi[39]);
  __m128i cospi_p07_p57 = pair_set_epi16(cospi[7], cospi[57]);
  __m128i cospi_m57_p07 = pair_set_epi16(-cospi[57], cospi[7]);
  __m128i cospi_p59_p05 = pair_set_epi16(cospi[59], cospi[5]);
  __m128i cospi_m05_p59 = pair_set_epi16(-cospi[5], cospi[59]);
  __m128i cospi_p27_p37 = pair_set_epi16(cospi[27], cospi[37]);
  __m128i cospi_m37_p27 = pair_set_epi16(-cospi[37], cospi[27]);
  __m128i cospi_p43_p21 = pair_set_epi16(cospi[43], cospi[21]);
  __m128i cospi_m21_p43 = pair_set_epi16(-cospi[21], cospi[43]);
  __m128i cospi_p11_p53 = pair_set_epi16(cospi[11], cospi[53]);
  __m128i cospi_m53_p11 = pair_set_epi16(-cospi[53], cospi[11]);
  __m128i cospi_p51_p13 = pair_set_epi16(cospi[51], cospi[13]);
  __m128i cospi_m13_p51 = pair_set_epi16(-cospi[13], cospi[51]);
  __m128i cospi_p19_p45 = pair_set_epi16(cospi[19], cospi[45]);
  __m128i cospi_m45_p19 = pair_set_epi16(-cospi[45], cospi[19]);
  __m128i cospi_p35_p29 = pair_set_epi16(cospi[35], cospi[29]);
  __m128i cospi_m29_p35 = pair_set_epi16(-cospi[29], cospi[35]);
  __m128i cospi_p03_p61 = pair_set_epi16(cospi[3], cospi[61]);
  __m128i cospi_m61_p03 = pair_set_epi16(-cospi[61], cospi[3]);

  // stage 1
  __m128i x1[64];
  x1[0] = _mm_adds_epi16(input[0], input[63]);
  x1[63] = _mm_subs_epi16(input[0], input[63]);
  x1[1] = _mm_adds_epi16(input[1], input[62]);
  x1[62] = _mm_subs_epi16(input[1], input[62]);
  x1[2] = _mm_adds_epi16(input[2], input[61]);
  x1[61] = _mm_subs_epi16(input[2], input[61]);
  x1[3] = _mm_adds_epi16(input[3], input[60]);
  x1[60] = _mm_subs_epi16(input[3], input[60]);
  x1[4] = _mm_adds_epi16(input[4], input[59]);
  x1[59] = _mm_subs_epi16(input[4], input[59]);
  x1[5] = _mm_adds_epi16(input[5], input[58]);
  x1[58] = _mm_subs_epi16(input[5], input[58]);
  x1[6] = _mm_adds_epi16(input[6], input[57]);
  x1[57] = _mm_subs_epi16(input[6], input[57]);
  x1[7] = _mm_adds_epi16(input[7], input[56]);
  x1[56] = _mm_subs_epi16(input[7], input[56]);
  x1[8] = _mm_adds_epi16(input[8], input[55]);
  x1[55] = _mm_subs_epi16(input[8], input[55]);
  x1[9] = _mm_adds_epi16(input[9], input[54]);
  x1[54] = _mm_subs_epi16(input[9], input[54]);
  x1[10] = _mm_adds_epi16(input[10], input[53]);
  x1[53] = _mm_subs_epi16(input[10], input[53]);
  x1[11] = _mm_adds_epi16(input[11], input[52]);
  x1[52] = _mm_subs_epi16(input[11], input[52]);
  x1[12] = _mm_adds_epi16(input[12], input[51]);
  x1[51] = _mm_subs_epi16(input[12], input[51]);
  x1[13] = _mm_adds_epi16(input[13], input[50]);
  x1[50] = _mm_subs_epi16(input[13], input[50]);
  x1[14] = _mm_adds_epi16(input[14], input[49]);
  x1[49] = _mm_subs_epi16(input[14], input[49]);
  x1[15] = _mm_adds_epi16(input[15], input[48]);
  x1[48] = _mm_subs_epi16(input[15], input[48]);
  x1[16] = _mm_adds_epi16(input[16], input[47]);
  x1[47] = _mm_subs_epi16(input[16], input[47]);
  x1[17] = _mm_adds_epi16(input[17], input[46]);
  x1[46] = _mm_subs_epi16(input[17], input[46]);
  x1[18] = _mm_adds_epi16(input[18], input[45]);
  x1[45] = _mm_subs_epi16(input[18], input[45]);
  x1[19] = _mm_adds_epi16(input[19], input[44]);
  x1[44] = _mm_subs_epi16(input[19], input[44]);
  x1[20] = _mm_adds_epi16(input[20], input[43]);
  x1[43] = _mm_subs_epi16(input[20], input[43]);
  x1[21] = _mm_adds_epi16(input[21], input[42]);
  x1[42] = _mm_subs_epi16(input[21], input[42]);
  x1[22] = _mm_adds_epi16(input[22], input[41]);
  x1[41] = _mm_subs_epi16(input[22], input[41]);
  x1[23] = _mm_adds_epi16(input[23], input[40]);
  x1[40] = _mm_subs_epi16(input[23], input[40]);
  x1[24] = _mm_adds_epi16(input[24], input[39]);
  x1[39] = _mm_subs_epi16(input[24], input[39]);
  x1[25] = _mm_adds_epi16(input[25], input[38]);
  x1[38] = _mm_subs_epi16(input[25], input[38]);
  x1[26] = _mm_adds_epi16(input[26], input[37]);
  x1[37] = _mm_subs_epi16(input[26], input[37]);
  x1[27] = _mm_adds_epi16(input[27], input[36]);
  x1[36] = _mm_subs_epi16(input[27], input[36]);
  x1[28] = _mm_adds_epi16(input[28], input[35]);
  x1[35] = _mm_subs_epi16(input[28], input[35]);
  x1[29] = _mm_adds_epi16(input[29], input[34]);
  x1[34] = _mm_subs_epi16(input[29], input[34]);
  x1[30] = _mm_adds_epi16(input[30], input[33]);
  x1[33] = _mm_subs_epi16(input[30], input[33]);
  x1[31] = _mm_adds_epi16(input[31], input[32]);
  x1[32] = _mm_subs_epi16(input[31], input[32]);

  // stage 2
  __m128i x2[64];
  x2[0] = _mm_adds_epi16(x1[0], x1[31]);
  x2[31] = _mm_subs_epi16(x1[0], x1[31]);
  x2[1] = _mm_adds_epi16(x1[1], x1[30]);
  x2[30] = _mm_subs_epi16(x1[1], x1[30]);
  x2[2] = _mm_adds_epi16(x1[2], x1[29]);
  x2[29] = _mm_subs_epi16(x1[2], x1[29]);
  x2[3] = _mm_adds_epi16(x1[3], x1[28]);
  x2[28] = _mm_subs_epi16(x1[3], x1[28]);
  x2[4] = _mm_adds_epi16(x1[4], x1[27]);
  x2[27] = _mm_subs_epi16(x1[4], x1[27]);
  x2[5] = _mm_adds_epi16(x1[5], x1[26]);
  x2[26] = _mm_subs_epi16(x1[5], x1[26]);
  x2[6] = _mm_adds_epi16(x1[6], x1[25]);
  x2[25] = _mm_subs_epi16(x1[6], x1[25]);
  x2[7] = _mm_adds_epi16(x1[7], x1[24]);
  x2[24] = _mm_subs_epi16(x1[7], x1[24]);
  x2[8] = _mm_adds_epi16(x1[8], x1[23]);
  x2[23] = _mm_subs_epi16(x1[8], x1[23]);
  x2[9] = _mm_adds_epi16(x1[9], x1[22]);
  x2[22] = _mm_subs_epi16(x1[9], x1[22]);
  x2[10] = _mm_adds_epi16(x1[10], x1[21]);
  x2[21] = _mm_subs_epi16(x1[10], x1[21]);
  x2[11] = _mm_adds_epi16(x1[11], x1[20]);
  x2[20] = _mm_subs_epi16(x1[11], x1[20]);
  x2[12] = _mm_adds_epi16(x1[12], x1[19]);
  x2[19] = _mm_subs_epi16(x1[12], x1[19]);
  x2[13] = _mm_adds_epi16(x1[13], x1[18]);
  x2[18] = _mm_subs_epi16(x1[13], x1[18]);
  x2[14] = _mm_adds_epi16(x1[14], x1[17]);
  x2[17] = _mm_subs_epi16(x1[14], x1[17]);
  x2[15] = _mm_adds_epi16(x1[15], x1[16]);
  x2[16] = _mm_subs_epi16(x1[15], x1[16]);
  x2[32] = x1[32];
  x2[33] = x1[33];
  x2[34] = x1[34];
  x2[35] = x1[35];
  x2[36] = x1[36];
  x2[37] = x1[37];
  x2[38] = x1[38];
  x2[39] = x1[39];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[40], x1[55], x2[40], x2[55]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[41], x1[54], x2[41], x2[54]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[42], x1[53], x2[42], x2[53]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[43], x1[52], x2[43], x2[52]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[44], x1[51], x2[44], x2[51]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[45], x1[50], x2[45], x2[50]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[46], x1[49], x2[46], x2[49]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x1[47], x1[48], x2[47], x2[48]);
  x2[56] = x1[56];
  x2[57] = x1[57];
  x2[58] = x1[58];
  x2[59] = x1[59];
  x2[60] = x1[60];
  x2[61] = x1[61];
  x2[62] = x1[62];
  x2[63] = x1[63];

  // stage 3
  __m128i x3[64];
  x3[0] = _mm_adds_epi16(x2[0], x2[15]);
  x3[15] = _mm_subs_epi16(x2[0], x2[15]);
  x3[1] = _mm_adds_epi16(x2[1], x2[14]);
  x3[14] = _mm_subs_epi16(x2[1], x2[14]);
  x3[2] = _mm_adds_epi16(x2[2], x2[13]);
  x3[13] = _mm_subs_epi16(x2[2], x2[13]);
  x3[3] = _mm_adds_epi16(x2[3], x2[12]);
  x3[12] = _mm_subs_epi16(x2[3], x2[12]);
  x3[4] = _mm_adds_epi16(x2[4], x2[11]);
  x3[11] = _mm_subs_epi16(x2[4], x2[11]);
  x3[5] = _mm_adds_epi16(x2[5], x2[10]);
  x3[10] = _mm_subs_epi16(x2[5], x2[10]);
  x3[6] = _mm_adds_epi16(x2[6], x2[9]);
  x3[9] = _mm_subs_epi16(x2[6], x2[9]);
  x3[7] = _mm_adds_epi16(x2[7], x2[8]);
  x3[8] = _mm_subs_epi16(x2[7], x2[8]);
  x3[16] = x2[16];
  x3[17] = x2[17];
  x3[18] = x2[18];
  x3[19] = x2[19];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x2[20], x2[27], x3[20], x3[27]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x2[21], x2[26], x3[21], x3[26]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x2[22], x2[25], x3[22], x3[25]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x2[23], x2[24], x3[23], x3[24]);
  x3[28] = x2[28];
  x3[29] = x2[29];
  x3[30] = x2[30];
  x3[31] = x2[31];
  x3[32] = _mm_adds_epi16(x2[32], x2[47]);
  x3[47] = _mm_subs_epi16(x2[32], x2[47]);
  x3[33] = _mm_adds_epi16(x2[33], x2[46]);
  x3[46] = _mm_subs_epi16(x2[33], x2[46]);
  x3[34] = _mm_adds_epi16(x2[34], x2[45]);
  x3[45] = _mm_subs_epi16(x2[34], x2[45]);
  x3[35] = _mm_adds_epi16(x2[35], x2[44]);
  x3[44] = _mm_subs_epi16(x2[35], x2[44]);
  x3[36] = _mm_adds_epi16(x2[36], x2[43]);
  x3[43] = _mm_subs_epi16(x2[36], x2[43]);
  x3[37] = _mm_adds_epi16(x2[37], x2[42]);
  x3[42] = _mm_subs_epi16(x2[37], x2[42]);
  x3[38] = _mm_adds_epi16(x2[38], x2[41]);
  x3[41] = _mm_subs_epi16(x2[38], x2[41]);
  x3[39] = _mm_adds_epi16(x2[39], x2[40]);
  x3[40] = _mm_subs_epi16(x2[39], x2[40]);
  x3[48] = _mm_subs_epi16(x2[63], x2[48]);
  x3[63] = _mm_adds_epi16(x2[63], x2[48]);
  x3[49] = _mm_subs_epi16(x2[62], x2[49]);
  x3[62] = _mm_adds_epi16(x2[62], x2[49]);
  x3[50] = _mm_subs_epi16(x2[61], x2[50]);
  x3[61] = _mm_adds_epi16(x2[61], x2[50]);
  x3[51] = _mm_subs_epi16(x2[60], x2[51]);
  x3[60] = _mm_adds_epi16(x2[60], x2[51]);
  x3[52] = _mm_subs_epi16(x2[59], x2[52]);
  x3[59] = _mm_adds_epi16(x2[59], x2[52]);
  x3[53] = _mm_subs_epi16(x2[58], x2[53]);
  x3[58] = _mm_adds_epi16(x2[58], x2[53]);
  x3[54] = _mm_subs_epi16(x2[57], x2[54]);
  x3[57] = _mm_adds_epi16(x2[57], x2[54]);
  x3[55] = _mm_subs_epi16(x2[56], x2[55]);
  x3[56] = _mm_adds_epi16(x2[56], x2[55]);

  // stage 4
  __m128i x4[64];
  x4[0] = _mm_adds_epi16(x3[0], x3[7]);
  x4[7] = _mm_subs_epi16(x3[0], x3[7]);
  x4[1] = _mm_adds_epi16(x3[1], x3[6]);
  x4[6] = _mm_subs_epi16(x3[1], x3[6]);
  x4[2] = _mm_adds_epi16(x3[2], x3[5]);
  x4[5] = _mm_subs_epi16(x3[2], x3[5]);
  x4[3] = _mm_adds_epi16(x3[3], x3[4]);
  x4[4] = _mm_subs_epi16(x3[3], x3[4]);
  x4[8] = x3[8];
  x4[9] = x3[9];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x3[10], x3[13], x4[10], x4[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x3[11], x3[12], x4[11], x4[12]);
  x4[14] = x3[14];
  x4[15] = x3[15];
  x4[16] = _mm_adds_epi16(x3[16], x3[23]);
  x4[23] = _mm_subs_epi16(x3[16], x3[23]);
  x4[17] = _mm_adds_epi16(x3[17], x3[22]);
  x4[22] = _mm_subs_epi16(x3[17], x3[22]);
  x4[18] = _mm_adds_epi16(x3[18], x3[21]);
  x4[21] = _mm_subs_epi16(x3[18], x3[21]);
  x4[19] = _mm_adds_epi16(x3[19], x3[20]);
  x4[20] = _mm_subs_epi16(x3[19], x3[20]);
  x4[24] = _mm_subs_epi16(x3[31], x3[24]);
  x4[31] = _mm_adds_epi16(x3[31], x3[24]);
  x4[25] = _mm_subs_epi16(x3[30], x3[25]);
  x4[30] = _mm_adds_epi16(x3[30], x3[25]);
  x4[26] = _mm_subs_epi16(x3[29], x3[26]);
  x4[29] = _mm_adds_epi16(x3[29], x3[26]);
  x4[27] = _mm_subs_epi16(x3[28], x3[27]);
  x4[28] = _mm_adds_epi16(x3[28], x3[27]);
  x4[32] = x3[32];
  x4[33] = x3[33];
  x4[34] = x3[34];
  x4[35] = x3[35];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x3[36], x3[59], x4[36], x4[59]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x3[37], x3[58], x4[37], x4[58]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x3[38], x3[57], x4[38], x4[57]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x3[39], x3[56], x4[39], x4[56]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x3[40], x3[55], x4[40], x4[55]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x3[41], x3[54], x4[41], x4[54]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x3[42], x3[53], x4[42], x4[53]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x3[43], x3[52], x4[43], x4[52]);
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
  __m128i x5[64];
  x5[0] = _mm_adds_epi16(x4[0], x4[3]);
  x5[3] = _mm_subs_epi16(x4[0], x4[3]);
  x5[1] = _mm_adds_epi16(x4[1], x4[2]);
  x5[2] = _mm_subs_epi16(x4[1], x4[2]);
  x5[4] = x4[4];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x4[5], x4[6], x5[5], x5[6]);
  x5[7] = x4[7];
  x5[8] = _mm_adds_epi16(x4[8], x4[11]);
  x5[11] = _mm_subs_epi16(x4[8], x4[11]);
  x5[9] = _mm_adds_epi16(x4[9], x4[10]);
  x5[10] = _mm_subs_epi16(x4[9], x4[10]);
  x5[12] = _mm_subs_epi16(x4[15], x4[12]);
  x5[15] = _mm_adds_epi16(x4[15], x4[12]);
  x5[13] = _mm_subs_epi16(x4[14], x4[13]);
  x5[14] = _mm_adds_epi16(x4[14], x4[13]);
  x5[16] = x4[16];
  x5[17] = x4[17];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x4[18], x4[29], x5[18], x5[29]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x4[19], x4[28], x5[19], x5[28]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x4[20], x4[27], x5[20], x5[27]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x4[21], x4[26], x5[21], x5[26]);
  x5[22] = x4[22];
  x5[23] = x4[23];
  x5[24] = x4[24];
  x5[25] = x4[25];
  x5[30] = x4[30];
  x5[31] = x4[31];
  x5[32] = _mm_adds_epi16(x4[32], x4[39]);
  x5[39] = _mm_subs_epi16(x4[32], x4[39]);
  x5[33] = _mm_adds_epi16(x4[33], x4[38]);
  x5[38] = _mm_subs_epi16(x4[33], x4[38]);
  x5[34] = _mm_adds_epi16(x4[34], x4[37]);
  x5[37] = _mm_subs_epi16(x4[34], x4[37]);
  x5[35] = _mm_adds_epi16(x4[35], x4[36]);
  x5[36] = _mm_subs_epi16(x4[35], x4[36]);
  x5[40] = _mm_subs_epi16(x4[47], x4[40]);
  x5[47] = _mm_adds_epi16(x4[47], x4[40]);
  x5[41] = _mm_subs_epi16(x4[46], x4[41]);
  x5[46] = _mm_adds_epi16(x4[46], x4[41]);
  x5[42] = _mm_subs_epi16(x4[45], x4[42]);
  x5[45] = _mm_adds_epi16(x4[45], x4[42]);
  x5[43] = _mm_subs_epi16(x4[44], x4[43]);
  x5[44] = _mm_adds_epi16(x4[44], x4[43]);
  x5[48] = _mm_adds_epi16(x4[48], x4[55]);
  x5[55] = _mm_subs_epi16(x4[48], x4[55]);
  x5[49] = _mm_adds_epi16(x4[49], x4[54]);
  x5[54] = _mm_subs_epi16(x4[49], x4[54]);
  x5[50] = _mm_adds_epi16(x4[50], x4[53]);
  x5[53] = _mm_subs_epi16(x4[50], x4[53]);
  x5[51] = _mm_adds_epi16(x4[51], x4[52]);
  x5[52] = _mm_subs_epi16(x4[51], x4[52]);
  x5[56] = _mm_subs_epi16(x4[63], x4[56]);
  x5[63] = _mm_adds_epi16(x4[63], x4[56]);
  x5[57] = _mm_subs_epi16(x4[62], x4[57]);
  x5[62] = _mm_adds_epi16(x4[62], x4[57]);
  x5[58] = _mm_subs_epi16(x4[61], x4[58]);
  x5[61] = _mm_adds_epi16(x4[61], x4[58]);
  x5[59] = _mm_subs_epi16(x4[60], x4[59]);
  x5[60] = _mm_adds_epi16(x4[60], x4[59]);

  // stage 6
  __m128i x6[64];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x5[0], x5[1], x6[0], x6[1]);
  btf_16_sse2(cospi_p48_p16, cospi_m16_p48, x5[2], x5[3], x6[2], x6[3]);
  x6[4] = _mm_adds_epi16(x5[4], x5[5]);
  x6[5] = _mm_subs_epi16(x5[4], x5[5]);
  x6[6] = _mm_subs_epi16(x5[7], x5[6]);
  x6[7] = _mm_adds_epi16(x5[7], x5[6]);
  x6[8] = x5[8];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x5[9], x5[14], x6[9], x6[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x5[10], x5[13], x6[10], x6[13]);
  x6[11] = x5[11];
  x6[12] = x5[12];
  x6[15] = x5[15];
  x6[16] = _mm_adds_epi16(x5[16], x5[19]);
  x6[19] = _mm_subs_epi16(x5[16], x5[19]);
  x6[17] = _mm_adds_epi16(x5[17], x5[18]);
  x6[18] = _mm_subs_epi16(x5[17], x5[18]);
  x6[20] = _mm_subs_epi16(x5[23], x5[20]);
  x6[23] = _mm_adds_epi16(x5[23], x5[20]);
  x6[21] = _mm_subs_epi16(x5[22], x5[21]);
  x6[22] = _mm_adds_epi16(x5[22], x5[21]);
  x6[24] = _mm_adds_epi16(x5[24], x5[27]);
  x6[27] = _mm_subs_epi16(x5[24], x5[27]);
  x6[25] = _mm_adds_epi16(x5[25], x5[26]);
  x6[26] = _mm_subs_epi16(x5[25], x5[26]);
  x6[28] = _mm_subs_epi16(x5[31], x5[28]);
  x6[31] = _mm_adds_epi16(x5[31], x5[28]);
  x6[29] = _mm_subs_epi16(x5[30], x5[29]);
  x6[30] = _mm_adds_epi16(x5[30], x5[29]);
  x6[32] = x5[32];
  x6[33] = x5[33];
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x5[34], x5[61], x6[34], x6[61]);
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x5[35], x5[60], x6[35], x6[60]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x5[36], x5[59], x6[36], x6[59]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x5[37], x5[58], x6[37], x6[58]);
  x6[38] = x5[38];
  x6[39] = x5[39];
  x6[40] = x5[40];
  x6[41] = x5[41];
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x5[42], x5[53], x6[42], x6[53]);
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x5[43], x5[52], x6[43], x6[52]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x5[44], x5[51], x6[44], x6[51]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x5[45], x5[50], x6[45], x6[50]);
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
  __m128i x7[64];
  x7[0] = x6[0];
  x7[1] = x6[1];
  x7[2] = x6[2];
  x7[3] = x6[3];
  btf_16_sse2(cospi_p56_p08, cospi_m08_p56, x6[4], x6[7], x7[4], x7[7]);
  btf_16_sse2(cospi_p24_p40, cospi_m40_p24, x6[5], x6[6], x7[5], x7[6]);
  x7[8] = _mm_adds_epi16(x6[8], x6[9]);
  x7[9] = _mm_subs_epi16(x6[8], x6[9]);
  x7[10] = _mm_subs_epi16(x6[11], x6[10]);
  x7[11] = _mm_adds_epi16(x6[11], x6[10]);
  x7[12] = _mm_adds_epi16(x6[12], x6[13]);
  x7[13] = _mm_subs_epi16(x6[12], x6[13]);
  x7[14] = _mm_subs_epi16(x6[15], x6[14]);
  x7[15] = _mm_adds_epi16(x6[15], x6[14]);
  x7[16] = x6[16];
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x6[17], x6[30], x7[17], x7[30]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x6[18], x6[29], x7[18], x7[29]);
  x7[19] = x6[19];
  x7[20] = x6[20];
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x6[21], x6[26], x7[21], x7[26]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x6[22], x6[25], x7[22], x7[25]);
  x7[23] = x6[23];
  x7[24] = x6[24];
  x7[27] = x6[27];
  x7[28] = x6[28];
  x7[31] = x6[31];
  x7[32] = _mm_adds_epi16(x6[32], x6[35]);
  x7[35] = _mm_subs_epi16(x6[32], x6[35]);
  x7[33] = _mm_adds_epi16(x6[33], x6[34]);
  x7[34] = _mm_subs_epi16(x6[33], x6[34]);
  x7[36] = _mm_subs_epi16(x6[39], x6[36]);
  x7[39] = _mm_adds_epi16(x6[39], x6[36]);
  x7[37] = _mm_subs_epi16(x6[38], x6[37]);
  x7[38] = _mm_adds_epi16(x6[38], x6[37]);
  x7[40] = _mm_adds_epi16(x6[40], x6[43]);
  x7[43] = _mm_subs_epi16(x6[40], x6[43]);
  x7[41] = _mm_adds_epi16(x6[41], x6[42]);
  x7[42] = _mm_subs_epi16(x6[41], x6[42]);
  x7[44] = _mm_subs_epi16(x6[47], x6[44]);
  x7[47] = _mm_adds_epi16(x6[47], x6[44]);
  x7[45] = _mm_subs_epi16(x6[46], x6[45]);
  x7[46] = _mm_adds_epi16(x6[46], x6[45]);
  x7[48] = _mm_adds_epi16(x6[48], x6[51]);
  x7[51] = _mm_subs_epi16(x6[48], x6[51]);
  x7[49] = _mm_adds_epi16(x6[49], x6[50]);
  x7[50] = _mm_subs_epi16(x6[49], x6[50]);
  x7[52] = _mm_subs_epi16(x6[55], x6[52]);
  x7[55] = _mm_adds_epi16(x6[55], x6[52]);
  x7[53] = _mm_subs_epi16(x6[54], x6[53]);
  x7[54] = _mm_adds_epi16(x6[54], x6[53]);
  x7[56] = _mm_adds_epi16(x6[56], x6[59]);
  x7[59] = _mm_subs_epi16(x6[56], x6[59]);
  x7[57] = _mm_adds_epi16(x6[57], x6[58]);
  x7[58] = _mm_subs_epi16(x6[57], x6[58]);
  x7[60] = _mm_subs_epi16(x6[63], x6[60]);
  x7[63] = _mm_adds_epi16(x6[63], x6[60]);
  x7[61] = _mm_subs_epi16(x6[62], x6[61]);
  x7[62] = _mm_adds_epi16(x6[62], x6[61]);

  // stage 8
  __m128i x8[64];
  x8[0] = x7[0];
  x8[1] = x7[1];
  x8[2] = x7[2];
  x8[3] = x7[3];
  x8[4] = x7[4];
  x8[5] = x7[5];
  x8[6] = x7[6];
  x8[7] = x7[7];
  btf_16_sse2(cospi_p60_p04, cospi_m04_p60, x7[8], x7[15], x8[8], x8[15]);
  btf_16_sse2(cospi_p28_p36, cospi_m36_p28, x7[9], x7[14], x8[9], x8[14]);
  btf_16_sse2(cospi_p44_p20, cospi_m20_p44, x7[10], x7[13], x8[10], x8[13]);
  btf_16_sse2(cospi_p12_p52, cospi_m52_p12, x7[11], x7[12], x8[11], x8[12]);
  x8[16] = _mm_adds_epi16(x7[16], x7[17]);
  x8[17] = _mm_subs_epi16(x7[16], x7[17]);
  x8[18] = _mm_subs_epi16(x7[19], x7[18]);
  x8[19] = _mm_adds_epi16(x7[19], x7[18]);
  x8[20] = _mm_adds_epi16(x7[20], x7[21]);
  x8[21] = _mm_subs_epi16(x7[20], x7[21]);
  x8[22] = _mm_subs_epi16(x7[23], x7[22]);
  x8[23] = _mm_adds_epi16(x7[23], x7[22]);
  x8[24] = _mm_adds_epi16(x7[24], x7[25]);
  x8[25] = _mm_subs_epi16(x7[24], x7[25]);
  x8[26] = _mm_subs_epi16(x7[27], x7[26]);
  x8[27] = _mm_adds_epi16(x7[27], x7[26]);
  x8[28] = _mm_adds_epi16(x7[28], x7[29]);
  x8[29] = _mm_subs_epi16(x7[28], x7[29]);
  x8[30] = _mm_subs_epi16(x7[31], x7[30]);
  x8[31] = _mm_adds_epi16(x7[31], x7[30]);
  x8[32] = x7[32];
  btf_16_sse2(cospi_m04_p60, cospi_p60_p04, x7[33], x7[62], x8[33], x8[62]);
  btf_16_sse2(cospi_m60_m04, cospi_m04_p60, x7[34], x7[61], x8[34], x8[61]);
  x8[35] = x7[35];
  x8[36] = x7[36];
  btf_16_sse2(cospi_m36_p28, cospi_p28_p36, x7[37], x7[58], x8[37], x8[58]);
  btf_16_sse2(cospi_m28_m36, cospi_m36_p28, x7[38], x7[57], x8[38], x8[57]);
  x8[39] = x7[39];
  x8[40] = x7[40];
  btf_16_sse2(cospi_m20_p44, cospi_p44_p20, x7[41], x7[54], x8[41], x8[54]);
  btf_16_sse2(cospi_m44_m20, cospi_m20_p44, x7[42], x7[53], x8[42], x8[53]);
  x8[43] = x7[43];
  x8[44] = x7[44];
  btf_16_sse2(cospi_m52_p12, cospi_p12_p52, x7[45], x7[50], x8[45], x8[50]);
  btf_16_sse2(cospi_m12_m52, cospi_m52_p12, x7[46], x7[49], x8[46], x8[49]);
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
  __m128i x9[64];
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
  btf_16_sse2(cospi_p62_p02, cospi_m02_p62, x8[16], x8[31], x9[16], x9[31]);
  btf_16_sse2(cospi_p30_p34, cospi_m34_p30, x8[17], x8[30], x9[17], x9[30]);
  btf_16_sse2(cospi_p46_p18, cospi_m18_p46, x8[18], x8[29], x9[18], x9[29]);
  btf_16_sse2(cospi_p14_p50, cospi_m50_p14, x8[19], x8[28], x9[19], x9[28]);
  btf_16_sse2(cospi_p54_p10, cospi_m10_p54, x8[20], x8[27], x9[20], x9[27]);
  btf_16_sse2(cospi_p22_p42, cospi_m42_p22, x8[21], x8[26], x9[21], x9[26]);
  btf_16_sse2(cospi_p38_p26, cospi_m26_p38, x8[22], x8[25], x9[22], x9[25]);
  btf_16_sse2(cospi_p06_p58, cospi_m58_p06, x8[23], x8[24], x9[23], x9[24]);
  x9[32] = _mm_adds_epi16(x8[32], x8[33]);
  x9[33] = _mm_subs_epi16(x8[32], x8[33]);
  x9[34] = _mm_subs_epi16(x8[35], x8[34]);
  x9[35] = _mm_adds_epi16(x8[35], x8[34]);
  x9[36] = _mm_adds_epi16(x8[36], x8[37]);
  x9[37] = _mm_subs_epi16(x8[36], x8[37]);
  x9[38] = _mm_subs_epi16(x8[39], x8[38]);
  x9[39] = _mm_adds_epi16(x8[39], x8[38]);
  x9[40] = _mm_adds_epi16(x8[40], x8[41]);
  x9[41] = _mm_subs_epi16(x8[40], x8[41]);
  x9[42] = _mm_subs_epi16(x8[43], x8[42]);
  x9[43] = _mm_adds_epi16(x8[43], x8[42]);
  x9[44] = _mm_adds_epi16(x8[44], x8[45]);
  x9[45] = _mm_subs_epi16(x8[44], x8[45]);
  x9[46] = _mm_subs_epi16(x8[47], x8[46]);
  x9[47] = _mm_adds_epi16(x8[47], x8[46]);
  x9[48] = _mm_adds_epi16(x8[48], x8[49]);
  x9[49] = _mm_subs_epi16(x8[48], x8[49]);
  x9[50] = _mm_subs_epi16(x8[51], x8[50]);
  x9[51] = _mm_adds_epi16(x8[51], x8[50]);
  x9[52] = _mm_adds_epi16(x8[52], x8[53]);
  x9[53] = _mm_subs_epi16(x8[52], x8[53]);
  x9[54] = _mm_subs_epi16(x8[55], x8[54]);
  x9[55] = _mm_adds_epi16(x8[55], x8[54]);
  x9[56] = _mm_adds_epi16(x8[56], x8[57]);
  x9[57] = _mm_subs_epi16(x8[56], x8[57]);
  x9[58] = _mm_subs_epi16(x8[59], x8[58]);
  x9[59] = _mm_adds_epi16(x8[59], x8[58]);
  x9[60] = _mm_adds_epi16(x8[60], x8[61]);
  x9[61] = _mm_subs_epi16(x8[60], x8[61]);
  x9[62] = _mm_subs_epi16(x8[63], x8[62]);
  x9[63] = _mm_adds_epi16(x8[63], x8[62]);

  // stage 10
  __m128i x10[64];
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
  btf_16_sse2(cospi_p63_p01, cospi_m01_p63, x9[32], x9[63], x10[32], x10[63]);
  btf_16_sse2(cospi_p31_p33, cospi_m33_p31, x9[33], x9[62], x10[33], x10[62]);
  btf_16_sse2(cospi_p47_p17, cospi_m17_p47, x9[34], x9[61], x10[34], x10[61]);
  btf_16_sse2(cospi_p15_p49, cospi_m49_p15, x9[35], x9[60], x10[35], x10[60]);
  btf_16_sse2(cospi_p55_p09, cospi_m09_p55, x9[36], x9[59], x10[36], x10[59]);
  btf_16_sse2(cospi_p23_p41, cospi_m41_p23, x9[37], x9[58], x10[37], x10[58]);
  btf_16_sse2(cospi_p39_p25, cospi_m25_p39, x9[38], x9[57], x10[38], x10[57]);
  btf_16_sse2(cospi_p07_p57, cospi_m57_p07, x9[39], x9[56], x10[39], x10[56]);
  btf_16_sse2(cospi_p59_p05, cospi_m05_p59, x9[40], x9[55], x10[40], x10[55]);
  btf_16_sse2(cospi_p27_p37, cospi_m37_p27, x9[41], x9[54], x10[41], x10[54]);
  btf_16_sse2(cospi_p43_p21, cospi_m21_p43, x9[42], x9[53], x10[42], x10[53]);
  btf_16_sse2(cospi_p11_p53, cospi_m53_p11, x9[43], x9[52], x10[43], x10[52]);
  btf_16_sse2(cospi_p51_p13, cospi_m13_p51, x9[44], x9[51], x10[44], x10[51]);
  btf_16_sse2(cospi_p19_p45, cospi_m45_p19, x9[45], x9[50], x10[45], x10[50]);
  btf_16_sse2(cospi_p35_p29, cospi_m29_p35, x9[46], x9[49], x10[46], x10[49]);
  btf_16_sse2(cospi_p03_p61, cospi_m61_p03, x9[47], x9[48], x10[47], x10[48]);

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

static void fadst4x4_new_sse2(const __m128i *input, __m128i *output,
                              int8_t cos_bit) {
  const int32_t *sinpi = sinpi_arr(cos_bit);
  const __m128i sinpi_p01_p02 = pair_set_epi16(sinpi[1], sinpi[2]);
  const __m128i sinpi_p04_m01 = pair_set_epi16(sinpi[4], -sinpi[1]);
  const __m128i sinpi_p03_p04 = pair_set_epi16(sinpi[3], sinpi[4]);
  const __m128i sinpi_m03_p02 = pair_set_epi16(-sinpi[3], sinpi[2]);
  const __m128i sinpi_p03_p03 = _mm_set1_epi16((int16_t)sinpi[3]);
  const __m128i __zero = _mm_set1_epi16(0);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));
  const __m128i in7 = _mm_add_epi16(input[0], input[1]);
  __m128i u[8], v[8];

  u[0] = _mm_unpacklo_epi16(input[0], input[1]);
  u[1] = _mm_unpacklo_epi16(input[2], input[3]);
  u[2] = _mm_unpacklo_epi16(in7, __zero);
  u[3] = _mm_unpacklo_epi16(input[2], __zero);
  u[4] = _mm_unpacklo_epi16(input[3], __zero);

  v[0] = _mm_madd_epi16(u[0], sinpi_p01_p02);  // s0 + s2
  v[1] = _mm_madd_epi16(u[1], sinpi_p03_p04);  // s4 + s5
  v[2] = _mm_madd_epi16(u[2], sinpi_p03_p03);  // x1
  v[3] = _mm_madd_epi16(u[0], sinpi_p04_m01);  // s1 - s3
  v[4] = _mm_madd_epi16(u[1], sinpi_m03_p02);  // -s4 + s6
  v[5] = _mm_madd_epi16(u[3], sinpi_p03_p03);  // s4
  v[6] = _mm_madd_epi16(u[4], sinpi_p03_p03);

  u[0] = _mm_add_epi32(v[0], v[1]);
  u[1] = _mm_sub_epi32(v[2], v[6]);
  u[2] = _mm_add_epi32(v[3], v[4]);
  u[3] = _mm_sub_epi32(u[2], u[0]);
  u[4] = _mm_slli_epi32(v[5], 2);
  u[5] = _mm_sub_epi32(u[4], v[5]);
  u[6] = _mm_add_epi32(u[3], u[5]);

  v[0] = _mm_add_epi32(u[0], __rounding);
  v[1] = _mm_add_epi32(u[1], __rounding);
  v[2] = _mm_add_epi32(u[2], __rounding);
  v[3] = _mm_add_epi32(u[6], __rounding);

  u[0] = _mm_srai_epi32(v[0], cos_bit);
  u[1] = _mm_srai_epi32(v[1], cos_bit);
  u[2] = _mm_srai_epi32(v[2], cos_bit);
  u[3] = _mm_srai_epi32(v[3], cos_bit);

  output[0] = _mm_packs_epi32(u[0], u[2]);
  output[1] = _mm_packs_epi32(u[1], u[3]);
  output[2] = _mm_srli_si128(output[0], 8);
  output[3] = _mm_srli_si128(output[1], 8);
}

static void fadst4x8_new_sse2(const __m128i *input, __m128i *output,
                              int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __zero = _mm_setzero_si128();
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);

  // stage 1
  __m128i x1[8];
  x1[0] = input[0];
  x1[1] = _mm_subs_epi16(__zero, input[7]);
  x1[2] = _mm_subs_epi16(__zero, input[3]);
  x1[3] = input[4];
  x1[4] = _mm_subs_epi16(__zero, input[1]);
  x1[5] = input[6];
  x1[6] = input[2];
  x1[7] = _mm_subs_epi16(__zero, input[5]);

  // stage 2
  __m128i x2[8];
  x2[0] = x1[0];
  x2[1] = x1[1];
  btf_16_w4_sse2(&cospi_p32_p32, &cospi_p32_m32, __rounding, cos_bit, &x1[2],
                 &x1[3], &x2[2], &x2[3]);
  x2[4] = x1[4];
  x2[5] = x1[5];
  btf_16_w4_sse2(&cospi_p32_p32, &cospi_p32_m32, __rounding, cos_bit, &x1[6],
                 &x1[7], &x2[6], &x2[7]);

  // stage 3
  __m128i x3[8];
  x3[0] = _mm_adds_epi16(x2[0], x2[2]);
  x3[2] = _mm_subs_epi16(x2[0], x2[2]);
  x3[1] = _mm_adds_epi16(x2[1], x2[3]);
  x3[3] = _mm_subs_epi16(x2[1], x2[3]);
  x3[4] = _mm_adds_epi16(x2[4], x2[6]);
  x3[6] = _mm_subs_epi16(x2[4], x2[6]);
  x3[5] = _mm_adds_epi16(x2[5], x2[7]);
  x3[7] = _mm_subs_epi16(x2[5], x2[7]);

  // stage 4
  __m128i x4[8];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_w4_sse2(&cospi_p16_p48, &cospi_p48_m16, __rounding, cos_bit, &x3[4],
                 &x3[5], &x4[4], &x4[5]);
  btf_16_w4_sse2(&cospi_m48_p16, &cospi_p16_p48, __rounding, cos_bit, &x3[6],
                 &x3[7], &x4[6], &x4[7]);

  // stage 5
  __m128i x5[8];
  x5[0] = _mm_adds_epi16(x4[0], x4[4]);
  x5[4] = _mm_subs_epi16(x4[0], x4[4]);
  x5[1] = _mm_adds_epi16(x4[1], x4[5]);
  x5[5] = _mm_subs_epi16(x4[1], x4[5]);
  x5[2] = _mm_adds_epi16(x4[2], x4[6]);
  x5[6] = _mm_subs_epi16(x4[2], x4[6]);
  x5[3] = _mm_adds_epi16(x4[3], x4[7]);
  x5[7] = _mm_subs_epi16(x4[3], x4[7]);

  // stage 6
  __m128i x6[8];
  btf_16_w4_sse2(&cospi_p04_p60, &cospi_p60_m04, __rounding, cos_bit, &x5[0],
                 &x5[1], &x6[0], &x6[1]);
  btf_16_w4_sse2(&cospi_p20_p44, &cospi_p44_m20, __rounding, cos_bit, &x5[2],
                 &x5[3], &x6[2], &x6[3]);
  btf_16_w4_sse2(&cospi_p36_p28, &cospi_p28_m36, __rounding, cos_bit, &x5[4],
                 &x5[5], &x6[4], &x6[5]);
  btf_16_w4_sse2(&cospi_p52_p12, &cospi_p12_m52, __rounding, cos_bit, &x5[6],
                 &x5[7], &x6[6], &x6[7]);

  // stage 7
  output[0] = x6[1];
  output[1] = x6[6];
  output[2] = x6[3];
  output[3] = x6[4];
  output[4] = x6[5];
  output[5] = x6[2];
  output[6] = x6[7];
  output[7] = x6[0];
}

static void fadst8x4_new_sse2(const __m128i *input, __m128i *output,
                              int8_t cos_bit) {
  const int32_t *sinpi = sinpi_arr(cos_bit);
  const __m128i sinpi_p01_p02 = pair_set_epi16(sinpi[1], sinpi[2]);
  const __m128i sinpi_p04_m01 = pair_set_epi16(sinpi[4], -sinpi[1]);
  const __m128i sinpi_p03_p04 = pair_set_epi16(sinpi[3], sinpi[4]);
  const __m128i sinpi_m03_p02 = pair_set_epi16(-sinpi[3], sinpi[2]);
  const __m128i sinpi_p03_p03 = _mm_set1_epi16((int16_t)sinpi[3]);
  const __m128i __zero = _mm_set1_epi16(0);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));
  const __m128i in7 = _mm_add_epi16(input[0], input[1]);
  __m128i u_lo[8], u_hi[8], v_lo[8], v_hi[8];

  u_lo[0] = _mm_unpacklo_epi16(input[0], input[1]);
  u_hi[0] = _mm_unpackhi_epi16(input[0], input[1]);
  u_lo[1] = _mm_unpacklo_epi16(input[2], input[3]);
  u_hi[1] = _mm_unpackhi_epi16(input[2], input[3]);
  u_lo[2] = _mm_unpacklo_epi16(in7, __zero);
  u_hi[2] = _mm_unpackhi_epi16(in7, __zero);
  u_lo[3] = _mm_unpacklo_epi16(input[2], __zero);
  u_hi[3] = _mm_unpackhi_epi16(input[2], __zero);
  u_lo[4] = _mm_unpacklo_epi16(input[3], __zero);
  u_hi[4] = _mm_unpackhi_epi16(input[3], __zero);

  v_lo[0] = _mm_madd_epi16(u_lo[0], sinpi_p01_p02);  // s0 + s2
  v_hi[0] = _mm_madd_epi16(u_hi[0], sinpi_p01_p02);  // s0 + s2
  v_lo[1] = _mm_madd_epi16(u_lo[1], sinpi_p03_p04);  // s4 + s5
  v_hi[1] = _mm_madd_epi16(u_hi[1], sinpi_p03_p04);  // s4 + s5
  v_lo[2] = _mm_madd_epi16(u_lo[2], sinpi_p03_p03);  // x1
  v_hi[2] = _mm_madd_epi16(u_hi[2], sinpi_p03_p03);  // x1
  v_lo[3] = _mm_madd_epi16(u_lo[0], sinpi_p04_m01);  // s1 - s3
  v_hi[3] = _mm_madd_epi16(u_hi[0], sinpi_p04_m01);  // s1 - s3
  v_lo[4] = _mm_madd_epi16(u_lo[1], sinpi_m03_p02);  // -s4 + s6
  v_hi[4] = _mm_madd_epi16(u_hi[1], sinpi_m03_p02);  // -s4 + s6
  v_lo[5] = _mm_madd_epi16(u_lo[3], sinpi_p03_p03);  // s4
  v_hi[5] = _mm_madd_epi16(u_hi[3], sinpi_p03_p03);  // s4
  v_lo[6] = _mm_madd_epi16(u_lo[4], sinpi_p03_p03);
  v_hi[6] = _mm_madd_epi16(u_hi[4], sinpi_p03_p03);

  u_lo[0] = _mm_add_epi32(v_lo[0], v_lo[1]);
  u_hi[0] = _mm_add_epi32(v_hi[0], v_hi[1]);
  u_lo[1] = _mm_sub_epi32(v_lo[2], v_lo[6]);
  u_hi[1] = _mm_sub_epi32(v_hi[2], v_hi[6]);
  u_lo[2] = _mm_add_epi32(v_lo[3], v_lo[4]);
  u_hi[2] = _mm_add_epi32(v_hi[3], v_hi[4]);
  u_lo[3] = _mm_sub_epi32(u_lo[2], u_lo[0]);
  u_hi[3] = _mm_sub_epi32(u_hi[2], u_hi[0]);
  u_lo[4] = _mm_slli_epi32(v_lo[5], 2);
  u_hi[4] = _mm_slli_epi32(v_hi[5], 2);
  u_lo[5] = _mm_sub_epi32(u_lo[4], v_lo[5]);
  u_hi[5] = _mm_sub_epi32(u_hi[4], v_hi[5]);
  u_lo[6] = _mm_add_epi32(u_lo[3], u_lo[5]);
  u_hi[6] = _mm_add_epi32(u_hi[3], u_hi[5]);

  v_lo[0] = _mm_add_epi32(u_lo[0], __rounding);
  v_hi[0] = _mm_add_epi32(u_hi[0], __rounding);
  v_lo[1] = _mm_add_epi32(u_lo[1], __rounding);
  v_hi[1] = _mm_add_epi32(u_hi[1], __rounding);
  v_lo[2] = _mm_add_epi32(u_lo[2], __rounding);
  v_hi[2] = _mm_add_epi32(u_hi[2], __rounding);
  v_lo[3] = _mm_add_epi32(u_lo[6], __rounding);
  v_hi[3] = _mm_add_epi32(u_hi[6], __rounding);

  u_lo[0] = _mm_srai_epi32(v_lo[0], cos_bit);
  u_hi[0] = _mm_srai_epi32(v_hi[0], cos_bit);
  u_lo[1] = _mm_srai_epi32(v_lo[1], cos_bit);
  u_hi[1] = _mm_srai_epi32(v_hi[1], cos_bit);
  u_lo[2] = _mm_srai_epi32(v_lo[2], cos_bit);
  u_hi[2] = _mm_srai_epi32(v_hi[2], cos_bit);
  u_lo[3] = _mm_srai_epi32(v_lo[3], cos_bit);
  u_hi[3] = _mm_srai_epi32(v_hi[3], cos_bit);

  output[0] = _mm_packs_epi32(u_lo[0], u_hi[0]);
  output[1] = _mm_packs_epi32(u_lo[1], u_hi[1]);
  output[2] = _mm_packs_epi32(u_lo[2], u_hi[2]);
  output[3] = _mm_packs_epi32(u_lo[3], u_hi[3]);
}

static void fadst8x8_new_sse2(const __m128i *input, __m128i *output,
                              int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __zero = _mm_setzero_si128();
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);

  // stage 1
  __m128i x1[8];
  x1[0] = input[0];
  x1[1] = _mm_subs_epi16(__zero, input[7]);
  x1[2] = _mm_subs_epi16(__zero, input[3]);
  x1[3] = input[4];
  x1[4] = _mm_subs_epi16(__zero, input[1]);
  x1[5] = input[6];
  x1[6] = input[2];
  x1[7] = _mm_subs_epi16(__zero, input[5]);

  // stage 2
  __m128i x2[8];
  x2[0] = x1[0];
  x2[1] = x1[1];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x1[2], x1[3], x2[2], x2[3]);
  x2[4] = x1[4];
  x2[5] = x1[5];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x1[6], x1[7], x2[6], x2[7]);

  // stage 3
  __m128i x3[8];
  x3[0] = _mm_adds_epi16(x2[0], x2[2]);
  x3[2] = _mm_subs_epi16(x2[0], x2[2]);
  x3[1] = _mm_adds_epi16(x2[1], x2[3]);
  x3[3] = _mm_subs_epi16(x2[1], x2[3]);
  x3[4] = _mm_adds_epi16(x2[4], x2[6]);
  x3[6] = _mm_subs_epi16(x2[4], x2[6]);
  x3[5] = _mm_adds_epi16(x2[5], x2[7]);
  x3[7] = _mm_subs_epi16(x2[5], x2[7]);

  // stage 4
  __m128i x4[8];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x3[4], x3[5], x4[4], x4[5]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x3[6], x3[7], x4[6], x4[7]);

  // stage 5
  __m128i x5[8];
  x5[0] = _mm_adds_epi16(x4[0], x4[4]);
  x5[4] = _mm_subs_epi16(x4[0], x4[4]);
  x5[1] = _mm_adds_epi16(x4[1], x4[5]);
  x5[5] = _mm_subs_epi16(x4[1], x4[5]);
  x5[2] = _mm_adds_epi16(x4[2], x4[6]);
  x5[6] = _mm_subs_epi16(x4[2], x4[6]);
  x5[3] = _mm_adds_epi16(x4[3], x4[7]);
  x5[7] = _mm_subs_epi16(x4[3], x4[7]);

  // stage 6
  __m128i x6[8];
  btf_16_sse2(cospi_p04_p60, cospi_p60_m04, x5[0], x5[1], x6[0], x6[1]);
  btf_16_sse2(cospi_p20_p44, cospi_p44_m20, x5[2], x5[3], x6[2], x6[3]);
  btf_16_sse2(cospi_p36_p28, cospi_p28_m36, x5[4], x5[5], x6[4], x6[5]);
  btf_16_sse2(cospi_p52_p12, cospi_p12_m52, x5[6], x5[7], x6[6], x6[7]);

  // stage 7
  output[0] = x6[1];
  output[1] = x6[6];
  output[2] = x6[3];
  output[3] = x6[4];
  output[4] = x6[5];
  output[5] = x6[2];
  output[6] = x6[7];
  output[7] = x6[0];
}

static void fadst8x16_new_sse2(const __m128i *input, __m128i *output,
                               int8_t cos_bit) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __zero = _mm_setzero_si128();
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  __m128i cospi_m56_p08 = pair_set_epi16(-cospi[56], cospi[8]);
  __m128i cospi_m24_p40 = pair_set_epi16(-cospi[24], cospi[40]);
  __m128i cospi_p02_p62 = pair_set_epi16(cospi[2], cospi[62]);
  __m128i cospi_p62_m02 = pair_set_epi16(cospi[62], -cospi[2]);
  __m128i cospi_p10_p54 = pair_set_epi16(cospi[10], cospi[54]);
  __m128i cospi_p54_m10 = pair_set_epi16(cospi[54], -cospi[10]);
  __m128i cospi_p18_p46 = pair_set_epi16(cospi[18], cospi[46]);
  __m128i cospi_p46_m18 = pair_set_epi16(cospi[46], -cospi[18]);
  __m128i cospi_p26_p38 = pair_set_epi16(cospi[26], cospi[38]);
  __m128i cospi_p38_m26 = pair_set_epi16(cospi[38], -cospi[26]);
  __m128i cospi_p34_p30 = pair_set_epi16(cospi[34], cospi[30]);
  __m128i cospi_p30_m34 = pair_set_epi16(cospi[30], -cospi[34]);
  __m128i cospi_p42_p22 = pair_set_epi16(cospi[42], cospi[22]);
  __m128i cospi_p22_m42 = pair_set_epi16(cospi[22], -cospi[42]);
  __m128i cospi_p50_p14 = pair_set_epi16(cospi[50], cospi[14]);
  __m128i cospi_p14_m50 = pair_set_epi16(cospi[14], -cospi[50]);
  __m128i cospi_p58_p06 = pair_set_epi16(cospi[58], cospi[6]);
  __m128i cospi_p06_m58 = pair_set_epi16(cospi[6], -cospi[58]);

  // stage 1
  __m128i x1[16];
  x1[0] = input[0];
  x1[1] = _mm_subs_epi16(__zero, input[15]);
  x1[2] = _mm_subs_epi16(__zero, input[7]);
  x1[3] = input[8];
  x1[4] = _mm_subs_epi16(__zero, input[3]);
  x1[5] = input[12];
  x1[6] = input[4];
  x1[7] = _mm_subs_epi16(__zero, input[11]);
  x1[8] = _mm_subs_epi16(__zero, input[1]);
  x1[9] = input[14];
  x1[10] = input[6];
  x1[11] = _mm_subs_epi16(__zero, input[9]);
  x1[12] = input[2];
  x1[13] = _mm_subs_epi16(__zero, input[13]);
  x1[14] = _mm_subs_epi16(__zero, input[5]);
  x1[15] = input[10];

  // stage 2
  __m128i x2[16];
  x2[0] = x1[0];
  x2[1] = x1[1];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x1[2], x1[3], x2[2], x2[3]);
  x2[4] = x1[4];
  x2[5] = x1[5];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x1[6], x1[7], x2[6], x2[7]);
  x2[8] = x1[8];
  x2[9] = x1[9];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x1[10], x1[11], x2[10], x2[11]);
  x2[12] = x1[12];
  x2[13] = x1[13];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x1[14], x1[15], x2[14], x2[15]);

  // stage 3
  __m128i x3[16];
  x3[0] = _mm_adds_epi16(x2[0], x2[2]);
  x3[2] = _mm_subs_epi16(x2[0], x2[2]);
  x3[1] = _mm_adds_epi16(x2[1], x2[3]);
  x3[3] = _mm_subs_epi16(x2[1], x2[3]);
  x3[4] = _mm_adds_epi16(x2[4], x2[6]);
  x3[6] = _mm_subs_epi16(x2[4], x2[6]);
  x3[5] = _mm_adds_epi16(x2[5], x2[7]);
  x3[7] = _mm_subs_epi16(x2[5], x2[7]);
  x3[8] = _mm_adds_epi16(x2[8], x2[10]);
  x3[10] = _mm_subs_epi16(x2[8], x2[10]);
  x3[9] = _mm_adds_epi16(x2[9], x2[11]);
  x3[11] = _mm_subs_epi16(x2[9], x2[11]);
  x3[12] = _mm_adds_epi16(x2[12], x2[14]);
  x3[14] = _mm_subs_epi16(x2[12], x2[14]);
  x3[13] = _mm_adds_epi16(x2[13], x2[15]);
  x3[15] = _mm_subs_epi16(x2[13], x2[15]);

  // stage 4
  __m128i x4[16];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x3[4], x3[5], x4[4], x4[5]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x3[6], x3[7], x4[6], x4[7]);
  x4[8] = x3[8];
  x4[9] = x3[9];
  x4[10] = x3[10];
  x4[11] = x3[11];
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x3[12], x3[13], x4[12], x4[13]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x3[14], x3[15], x4[14], x4[15]);

  // stage 5
  __m128i x5[16];
  x5[0] = _mm_adds_epi16(x4[0], x4[4]);
  x5[4] = _mm_subs_epi16(x4[0], x4[4]);
  x5[1] = _mm_adds_epi16(x4[1], x4[5]);
  x5[5] = _mm_subs_epi16(x4[1], x4[5]);
  x5[2] = _mm_adds_epi16(x4[2], x4[6]);
  x5[6] = _mm_subs_epi16(x4[2], x4[6]);
  x5[3] = _mm_adds_epi16(x4[3], x4[7]);
  x5[7] = _mm_subs_epi16(x4[3], x4[7]);
  x5[8] = _mm_adds_epi16(x4[8], x4[12]);
  x5[12] = _mm_subs_epi16(x4[8], x4[12]);
  x5[9] = _mm_adds_epi16(x4[9], x4[13]);
  x5[13] = _mm_subs_epi16(x4[9], x4[13]);
  x5[10] = _mm_adds_epi16(x4[10], x4[14]);
  x5[14] = _mm_subs_epi16(x4[10], x4[14]);
  x5[11] = _mm_adds_epi16(x4[11], x4[15]);
  x5[15] = _mm_subs_epi16(x4[11], x4[15]);

  // stage 6
  __m128i x6[16];
  x6[0] = x5[0];
  x6[1] = x5[1];
  x6[2] = x5[2];
  x6[3] = x5[3];
  x6[4] = x5[4];
  x6[5] = x5[5];
  x6[6] = x5[6];
  x6[7] = x5[7];
  btf_16_sse2(cospi_p08_p56, cospi_p56_m08, x5[8], x5[9], x6[8], x6[9]);
  btf_16_sse2(cospi_p40_p24, cospi_p24_m40, x5[10], x5[11], x6[10], x6[11]);
  btf_16_sse2(cospi_m56_p08, cospi_p08_p56, x5[12], x5[13], x6[12], x6[13]);
  btf_16_sse2(cospi_m24_p40, cospi_p40_p24, x5[14], x5[15], x6[14], x6[15]);

  // stage 7
  __m128i x7[16];
  x7[0] = _mm_adds_epi16(x6[0], x6[8]);
  x7[8] = _mm_subs_epi16(x6[0], x6[8]);
  x7[1] = _mm_adds_epi16(x6[1], x6[9]);
  x7[9] = _mm_subs_epi16(x6[1], x6[9]);
  x7[2] = _mm_adds_epi16(x6[2], x6[10]);
  x7[10] = _mm_subs_epi16(x6[2], x6[10]);
  x7[3] = _mm_adds_epi16(x6[3], x6[11]);
  x7[11] = _mm_subs_epi16(x6[3], x6[11]);
  x7[4] = _mm_adds_epi16(x6[4], x6[12]);
  x7[12] = _mm_subs_epi16(x6[4], x6[12]);
  x7[5] = _mm_adds_epi16(x6[5], x6[13]);
  x7[13] = _mm_subs_epi16(x6[5], x6[13]);
  x7[6] = _mm_adds_epi16(x6[6], x6[14]);
  x7[14] = _mm_subs_epi16(x6[6], x6[14]);
  x7[7] = _mm_adds_epi16(x6[7], x6[15]);
  x7[15] = _mm_subs_epi16(x6[7], x6[15]);

  // stage 8
  __m128i x8[16];
  btf_16_sse2(cospi_p02_p62, cospi_p62_m02, x7[0], x7[1], x8[0], x8[1]);
  btf_16_sse2(cospi_p10_p54, cospi_p54_m10, x7[2], x7[3], x8[2], x8[3]);
  btf_16_sse2(cospi_p18_p46, cospi_p46_m18, x7[4], x7[5], x8[4], x8[5]);
  btf_16_sse2(cospi_p26_p38, cospi_p38_m26, x7[6], x7[7], x8[6], x8[7]);
  btf_16_sse2(cospi_p34_p30, cospi_p30_m34, x7[8], x7[9], x8[8], x8[9]);
  btf_16_sse2(cospi_p42_p22, cospi_p22_m42, x7[10], x7[11], x8[10], x8[11]);
  btf_16_sse2(cospi_p50_p14, cospi_p14_m50, x7[12], x7[13], x8[12], x8[13]);
  btf_16_sse2(cospi_p58_p06, cospi_p06_m58, x7[14], x7[15], x8[14], x8[15]);

  // stage 9
  output[0] = x8[1];
  output[1] = x8[14];
  output[2] = x8[3];
  output[3] = x8[12];
  output[4] = x8[5];
  output[5] = x8[10];
  output[6] = x8[7];
  output[7] = x8[8];
  output[8] = x8[9];
  output[9] = x8[6];
  output[10] = x8[11];
  output[11] = x8[4];
  output[12] = x8[13];
  output[13] = x8[2];
  output[14] = x8[15];
  output[15] = x8[0];
}

static const transform_1d_sse2 col_txfm4x4_arr[TX_TYPES] = {
  fdct4x4_new_sse2,       // DCT_DCT
  fadst4x4_new_sse2,      // ADST_DCT
  fdct4x4_new_sse2,       // DCT_ADST
  fadst4x4_new_sse2,      // ADST_ADST
  fadst4x4_new_sse2,      // FLIPADST_DCT
  fdct4x4_new_sse2,       // DCT_FLIPADST
  fadst4x4_new_sse2,      // FLIPADST_FLIPADST
  fadst4x4_new_sse2,      // ADST_FLIPADST
  fadst4x4_new_sse2,      // FLIPADST_ADST
  fidentity4x4_new_sse2,  // IDTX
  fdct4x4_new_sse2,       // V_DCT
  fidentity4x4_new_sse2,  // H_DCT
  fadst4x4_new_sse2,      // V_ADST
  fidentity4x4_new_sse2,  // H_ADST
  fadst4x4_new_sse2,      // V_FLIPADST
  fidentity4x4_new_sse2   // H_FLIPADST
};

static const transform_1d_sse2 row_txfm4x4_arr[TX_TYPES] = {
  fdct4x4_new_sse2,       // DCT_DCT
  fdct4x4_new_sse2,       // ADST_DCT
  fadst4x4_new_sse2,      // DCT_ADST
  fadst4x4_new_sse2,      // ADST_ADST
  fdct4x4_new_sse2,       // FLIPADST_DCT
  fadst4x4_new_sse2,      // DCT_FLIPADST
  fadst4x4_new_sse2,      // FLIPADST_FLIPADST
  fadst4x4_new_sse2,      // ADST_FLIPADST
  fadst4x4_new_sse2,      // FLIPADST_ADST
  fidentity4x4_new_sse2,  // IDTX
  fidentity4x4_new_sse2,  // V_DCT
  fdct4x4_new_sse2,       // H_DCT
  fidentity4x4_new_sse2,  // V_ADST
  fadst4x4_new_sse2,      // H_ADST
  fidentity4x4_new_sse2,  // V_FLIPADST
  fadst4x4_new_sse2       // H_FLIPADST
};

static const transform_1d_sse2 col_txfm4x8_arr[TX_TYPES] = {
  fdct4x8_new_sse2,       // DCT_DCT
  fadst4x8_new_sse2,      // ADST_DCT
  fdct4x8_new_sse2,       // DCT_ADST
  fadst4x8_new_sse2,      // ADST_ADST
  fadst4x8_new_sse2,      // FLIPADST_DCT
  fdct4x8_new_sse2,       // DCT_FLIPADST
  fadst4x8_new_sse2,      // FLIPADST_FLIPADST
  fadst4x8_new_sse2,      // ADST_FLIPADST
  fadst4x8_new_sse2,      // FLIPADST_ADST
  fidentity8x8_new_sse2,  // IDTX
  fdct4x8_new_sse2,       // V_DCT
  fidentity8x8_new_sse2,  // H_DCT
  fadst4x8_new_sse2,      // V_ADST
  fidentity8x8_new_sse2,  // H_ADST
  fadst4x8_new_sse2,      // V_FLIPADST
  fidentity8x8_new_sse2   // H_FLIPADST
};

static const transform_1d_sse2 row_txfm8x4_arr[TX_TYPES] = {
  fdct8x4_new_sse2,       // DCT_DCT
  fdct8x4_new_sse2,       // ADST_DCT
  fadst8x4_new_sse2,      // DCT_ADST
  fadst8x4_new_sse2,      // ADST_ADST
  fdct8x4_new_sse2,       // FLIPADST_DCT
  fadst8x4_new_sse2,      // DCT_FLIPADST
  fadst8x4_new_sse2,      // FLIPADST_FLIPADST
  fadst8x4_new_sse2,      // ADST_FLIPADST
  fadst8x4_new_sse2,      // FLIPADST_ADST
  fidentity8x4_new_sse2,  // IDTX
  fidentity8x4_new_sse2,  // V_DCT
  fdct8x4_new_sse2,       // H_DCT
  fidentity8x4_new_sse2,  // V_ADST
  fadst8x4_new_sse2,      // H_ADST
  fidentity8x4_new_sse2,  // V_FLIPADST
  fadst8x4_new_sse2       // H_FLIPADST
};

static const transform_1d_sse2 col_txfm8x4_arr[TX_TYPES] = {
  fdct8x4_new_sse2,       // DCT_DCT
  fadst8x4_new_sse2,      // ADST_DCT
  fdct8x4_new_sse2,       // DCT_ADST
  fadst8x4_new_sse2,      // ADST_ADST
  fadst8x4_new_sse2,      // FLIPADST_DCT
  fdct8x4_new_sse2,       // DCT_FLIPADST
  fadst8x4_new_sse2,      // FLIPADST_FLIPADST
  fadst8x4_new_sse2,      // ADST_FLIPADST
  fadst8x4_new_sse2,      // FLIPADST_ADST
  fidentity8x4_new_sse2,  // IDTX
  fdct8x4_new_sse2,       // V_DCT
  fidentity8x4_new_sse2,  // H_DCT
  fadst8x4_new_sse2,      // V_ADST
  fidentity8x4_new_sse2,  // H_ADST
  fadst8x4_new_sse2,      // V_FLIPADST
  fidentity8x4_new_sse2   // H_FLIPADST
};

static const transform_1d_sse2 row_txfm4x8_arr[TX_TYPES] = {
  fdct4x8_new_sse2,       // DCT_DCT
  fdct4x8_new_sse2,       // ADST_DCT
  fadst4x8_new_sse2,      // DCT_ADST
  fadst4x8_new_sse2,      // ADST_ADST
  fdct4x8_new_sse2,       // FLIPADST_DCT
  fadst4x8_new_sse2,      // DCT_FLIPADST
  fadst4x8_new_sse2,      // FLIPADST_FLIPADST
  fadst4x8_new_sse2,      // ADST_FLIPADST
  fadst4x8_new_sse2,      // FLIPADST_ADST
  fidentity8x8_new_sse2,  // IDTX
  fidentity8x8_new_sse2,  // V_DCT
  fdct4x8_new_sse2,       // H_DCT
  fidentity8x8_new_sse2,  // V_ADST
  fadst4x8_new_sse2,      // H_ADST
  fidentity8x8_new_sse2,  // V_FLIPADST
  fadst4x8_new_sse2       // H_FLIPADST
};

static const transform_1d_sse2 col_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_new_sse2,       // DCT_DCT
  fadst8x8_new_sse2,      // ADST_DCT
  fdct8x8_new_sse2,       // DCT_ADST
  fadst8x8_new_sse2,      // ADST_ADST
  fadst8x8_new_sse2,      // FLIPADST_DCT
  fdct8x8_new_sse2,       // DCT_FLIPADST
  fadst8x8_new_sse2,      // FLIPADST_FLIPADST
  fadst8x8_new_sse2,      // ADST_FLIPADST
  fadst8x8_new_sse2,      // FLIPADST_ADST
  fidentity8x8_new_sse2,  // IDTX
  fdct8x8_new_sse2,       // V_DCT
  fidentity8x8_new_sse2,  // H_DCT
  fadst8x8_new_sse2,      // V_ADST
  fidentity8x8_new_sse2,  // H_ADST
  fadst8x8_new_sse2,      // V_FLIPADST
  fidentity8x8_new_sse2,  // H_FLIPADST
};

static const transform_1d_sse2 row_txfm8x8_arr[TX_TYPES] = {
  fdct8x8_new_sse2,       // DCT_DCT
  fdct8x8_new_sse2,       // ADST_DCT
  fadst8x8_new_sse2,      // DCT_ADST
  fadst8x8_new_sse2,      // ADST_ADST
  fdct8x8_new_sse2,       // FLIPADST_DCT
  fadst8x8_new_sse2,      // DCT_FLIPADST
  fadst8x8_new_sse2,      // FLIPADST_FLIPADST
  fadst8x8_new_sse2,      // ADST_FLIPADST
  fadst8x8_new_sse2,      // FLIPADST_ADST
  fidentity8x8_new_sse2,  // IDTX
  fidentity8x8_new_sse2,  // V_DCT
  fdct8x8_new_sse2,       // H_DCT
  fidentity8x8_new_sse2,  // V_ADST
  fadst8x8_new_sse2,      // H_ADST
  fidentity8x8_new_sse2,  // V_FLIPADST
  fadst8x8_new_sse2       // H_FLIPADST
};

static const transform_1d_sse2 col_txfm8x16_arr[TX_TYPES] = {
  fdct8x16_new_sse2,       // DCT_DCT
  fadst8x16_new_sse2,      // ADST_DCT
  fdct8x16_new_sse2,       // DCT_ADST
  fadst8x16_new_sse2,      // ADST_ADST
  fadst8x16_new_sse2,      // FLIPADST_DCT
  fdct8x16_new_sse2,       // DCT_FLIPADST
  fadst8x16_new_sse2,      // FLIPADST_FLIPADST
  fadst8x16_new_sse2,      // ADST_FLIPADST
  fadst8x16_new_sse2,      // FLIPADST_ADST
  fidentity8x16_new_sse2,  // IDTX
  fdct8x16_new_sse2,       // V_DCT
  fidentity8x16_new_sse2,  // H_DCT
  fadst8x16_new_sse2,      // V_ADST
  fidentity8x16_new_sse2,  // H_ADST
  fadst8x16_new_sse2,      // V_FLIPADST
  fidentity8x16_new_sse2   // H_FLIPADST
};

static const transform_1d_sse2 row_txfm8x16_arr[TX_TYPES] = {
  fdct8x16_new_sse2,       // DCT_DCT
  fdct8x16_new_sse2,       // ADST_DCT
  fadst8x16_new_sse2,      // DCT_ADST
  fadst8x16_new_sse2,      // ADST_ADST
  fdct8x16_new_sse2,       // FLIPADST_DCT
  fadst8x16_new_sse2,      // DCT_FLIPADST
  fadst8x16_new_sse2,      // FLIPADST_FLIPADST
  fadst8x16_new_sse2,      // ADST_FLIPADST
  fadst8x16_new_sse2,      // FLIPADST_ADST
  fidentity8x16_new_sse2,  // IDTX
  fidentity8x16_new_sse2,  // V_DCT
  fdct8x16_new_sse2,       // H_DCT
  fidentity8x16_new_sse2,  // V_ADST
  fadst8x16_new_sse2,      // H_ADST
  fidentity8x16_new_sse2,  // V_FLIPADST
  fadst8x16_new_sse2       // H_FLIPADST
};

static const transform_1d_sse2 row_txfm8x32_arr[TX_TYPES] = {
  av1_fdct8x32_new_sse2,   // DCT_DCT
  NULL,                    // ADST_DCT
  NULL,                    // DCT_ADST
  NULL,                    // ADST_ADST
  NULL,                    // FLIPADST_DCT
  NULL,                    // DCT_FLIPADST
  NULL,                    // FLIPADST_FLIPADST
  NULL,                    // ADST_FLIPADST
  NULL,                    // FLIPADST_ADST
  fidentity8x32_new_sse2,  // IDTX
  fidentity8x32_new_sse2,  // V_DCT
  av1_fdct8x32_new_sse2,   // H_DCT
  NULL,                    // V_ADST
  NULL,                    // H_ADST
  NULL,                    // V_FLIPADST
  NULL                     // H_FLIPADST
};

void av1_lowbd_fwd_txfm2d_4x4_sse2(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[4], buf1[4], *buf;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X4];
  const int txw_idx = get_txw_idx(TX_4X4);
  const int txh_idx = get_txh_idx(TX_4X4);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 4;
  const int height = 4;
  const transform_1d_sse2 col_txfm = col_txfm4x4_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm4x4_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_w4_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit_w4(input, stride, buf0, height);
  }
  round_shift_16bit(buf0, height, shift[0]);
  col_txfm(buf0, buf0, cos_bit_col);
  round_shift_16bit(buf0, height, shift[1]);
  transpose_16bit_4x4(buf0, buf1);

  if (lr_flip) {
    buf = buf0;
    flip_buf_sse2(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit(buf, width, shift[2]);
  transpose_16bit_4x4(buf, buf);
  store_buffer_16bit_to_32bit_w4(buf, output, width, height);
}

void av1_lowbd_fwd_txfm2d_4x8_sse2(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  (void)stride;
  (void)bd;
  __m128i buf0[8], buf1[8], *buf;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X8];
  const int txw_idx = get_txw_idx(TX_4X8);
  const int txh_idx = get_txh_idx(TX_4X8);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 4;
  const int height = 8;
  const transform_1d_sse2 col_txfm = col_txfm4x8_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x4_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_w4_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit_w4(input, stride, buf0, height);
  }
  round_shift_16bit(buf0, height, shift[0]);
  col_txfm(buf0, buf0, cos_bit_col);
  round_shift_16bit(buf0, height, shift[1]);
  transpose_16bit_4x8(buf0, buf1);

  if (lr_flip) {
    buf = buf0;
    flip_buf_sse2(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit(buf, width, shift[2]);
  transpose_16bit_8x4(buf, buf);
  store_rect_buffer_16bit_to_32bit_w4(buf, output, width, height);
}

void av1_lowbd_fwd_txfm2d_4x16_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[16], buf1[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_4X16];
  const int txw_idx = get_txw_idx(TX_4X16);
  const int txh_idx = get_txh_idx(TX_4X16);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 4;
  const int height = 16;
  const transform_1d_sse2 col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x4_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_w4_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit_w4(input, stride, buf0, height);
  }
  round_shift_16bit(buf0, height, shift[0]);
  col_txfm(buf0, buf0, cos_bit_col);
  round_shift_16bit(buf0, height, shift[1]);
  transpose_16bit_4x8(buf0, buf1);
  transpose_16bit_4x8(buf0 + 8, buf1 + 8);

  for (int i = 0; i < 2; i++) {
    __m128i *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_sse2(buf1 + 8 * i, buf, width);
    } else {
      buf = buf1 + 8 * i;
    }
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit(buf, width, shift[2]);
    transpose_16bit_8x4(buf, buf);
    store_buffer_16bit_to_32bit_w4(buf, output + 8 * width * i, width, 8);
  }
}

void av1_lowbd_fwd_txfm2d_8x4_sse2(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[8], buf1[8], *buf;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X4];
  const int txw_idx = get_txw_idx(TX_8X4);
  const int txh_idx = get_txh_idx(TX_8X4);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 8;
  const int height = 4;
  const transform_1d_sse2 col_txfm = col_txfm8x4_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm4x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  if (ud_flip)
    load_buffer_16bit_to_16bit_flip(input, stride, buf0, height);
  else
    load_buffer_16bit_to_16bit(input, stride, buf0, height);
  round_shift_16bit(buf0, height, shift[0]);
  col_txfm(buf0, buf0, cos_bit_col);
  round_shift_16bit(buf0, height, shift[1]);
  transpose_16bit_8x8(buf0, buf1);

  if (lr_flip) {
    buf = buf0;
    flip_buf_sse2(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit(buf, width, shift[2]);
  transpose_16bit_8x8(buf, buf);
  store_rect_buffer_16bit_to_32bit_w8(buf, output, width, height);
}

void av1_lowbd_fwd_txfm2d_8x8_sse2(const int16_t *input, int32_t *output,
                                   int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[8], buf1[8], *buf;
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X8];
  const int txw_idx = get_txw_idx(TX_8X8);
  const int txh_idx = get_txh_idx(TX_8X8);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 8;
  const int height = 8;
  const transform_1d_sse2 col_txfm = col_txfm8x8_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  if (ud_flip)
    load_buffer_16bit_to_16bit_flip(input, stride, buf0, height);
  else
    load_buffer_16bit_to_16bit(input, stride, buf0, height);
  round_shift_16bit(buf0, height, shift[0]);
  col_txfm(buf0, buf0, cos_bit_col);
  round_shift_16bit(buf0, height, shift[1]);
  transpose_16bit_8x8(buf0, buf1);

  if (lr_flip) {
    buf = buf0;
    flip_buf_sse2(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit(buf, width, shift[2]);
  transpose_16bit_8x8(buf, buf);
  store_buffer_16bit_to_32bit_w8(buf, output, width, height);
}

void av1_lowbd_fwd_txfm2d_8x16_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[16], buf1[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X16];
  const int txw_idx = get_txw_idx(TX_8X16);
  const int txh_idx = get_txh_idx(TX_8X16);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 8;
  const int height = 16;
  const transform_1d_sse2 col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit(input, stride, buf0, height);
  }
  round_shift_16bit(buf0, height, shift[0]);
  col_txfm(buf0, buf0, cos_bit_col);
  round_shift_16bit(buf0, height, shift[1]);
  transpose_16bit_8x8(buf0, buf1);
  transpose_16bit_8x8(buf0 + 8, buf1 + 8);

  for (int i = 0; i < 2; i++) {
    __m128i *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_sse2(buf1 + width * i, buf, width);
    } else {
      buf = buf1 + width * i;
    }
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit(buf, width, shift[2]);
    transpose_16bit_8x8(buf, buf);
    store_rect_buffer_16bit_to_32bit_w8(buf, output + 8 * width * i, width, 8);
  }
}

void av1_lowbd_fwd_txfm2d_8x32_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[32], buf1[32];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_8X32];
  const int txw_idx = get_txw_idx(TX_8X32);
  const int txh_idx = get_txh_idx(TX_8X32);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 8;
  const int height = 32;
  const transform_1d_sse2 col_txfm = col_txfm8x32_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x8_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  if (ud_flip) {
    load_buffer_16bit_to_16bit_flip(input, stride, buf0, height);
  } else {
    load_buffer_16bit_to_16bit(input, stride, buf0, height);
  }
  round_shift_16bit(buf0, height, shift[0]);
  col_txfm(buf0, buf0, cos_bit_col);
  round_shift_16bit(buf0, height, shift[1]);
  transpose_16bit_8x8(buf0, buf1);
  transpose_16bit_8x8(buf0 + 8, buf1 + 8);
  transpose_16bit_8x8(buf0 + 16, buf1 + 16);
  transpose_16bit_8x8(buf0 + 24, buf1 + 24);

  for (int i = 0; i < 4; i++) {
    __m128i *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_sse2(buf1 + width * i, buf, width);
    } else {
      buf = buf1 + width * i;
    }
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit(buf, width, shift[2]);
    transpose_16bit_8x8(buf, buf);
    store_buffer_16bit_to_32bit_w8(buf, output + 8 * width * i, width, 8);
  }
}

void av1_lowbd_fwd_txfm2d_16x4_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[16], buf1[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X4];
  const int txw_idx = get_txw_idx(TX_16X4);
  const int txh_idx = get_txh_idx(TX_16X4);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 16;
  const int height = 4;
  const transform_1d_sse2 col_txfm = col_txfm8x4_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x16_arr[tx_type];
  __m128i *buf;
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < 2; i++) {
    if (ud_flip) {
      load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
    } else {
      load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    }
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit(buf0, height, shift[1]);
    transpose_16bit_8x4(buf0, buf1 + 8 * i);
  }

  if (lr_flip) {
    buf = buf0;
    flip_buf_sse2(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit(buf, width, shift[2]);
  transpose_16bit_4x8(buf, buf);
  store_buffer_16bit_to_32bit_w8(buf, output, width, height);
  transpose_16bit_4x8(buf + 8, buf + 8);
  store_buffer_16bit_to_32bit_w8(buf + 8, output + 8, width, height);
}

void av1_lowbd_fwd_txfm2d_16x8_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[16], buf1[16];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X8];
  const int txw_idx = get_txw_idx(TX_16X8);
  const int txh_idx = get_txh_idx(TX_16X8);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 16;
  const int height = 8;
  const transform_1d_sse2 col_txfm = col_txfm8x8_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x16_arr[tx_type];
  __m128i *buf;
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < 2; i++) {
    if (ud_flip) {
      load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
    } else {
      load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    }
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit(buf0, height, shift[1]);
    transpose_16bit_8x8(buf0, buf1 + 8 * i);
  }

  if (lr_flip) {
    buf = buf0;
    flip_buf_sse2(buf1, buf, width);
  } else {
    buf = buf1;
  }
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit(buf, width, shift[2]);
  transpose_16bit_8x8(buf, buf);
  store_rect_buffer_16bit_to_32bit_w8(buf, output, width, height);
  transpose_16bit_8x8(buf + 8, buf + 8);
  store_rect_buffer_16bit_to_32bit_w8(buf + 8, output + 8, width, height);
}

void av1_lowbd_fwd_txfm2d_16x16_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[16], buf1[32];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X16];
  const int txw_idx = get_txw_idx(TX_16X16);
  const int txh_idx = get_txh_idx(TX_16X16);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 16;
  const int height = 16;
  const transform_1d_sse2 col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x16_arr[tx_type];
  int ud_flip, lr_flip;

  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < 2; i++) {
    if (ud_flip) {
      load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
    } else {
      load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    }
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit(buf0, height, shift[1]);
    transpose_16bit_8x8(buf0, buf1 + 0 * width + 8 * i);
    transpose_16bit_8x8(buf0 + 8, buf1 + 1 * width + 8 * i);
  }

  for (int i = 0; i < 2; i++) {
    __m128i *buf;
    if (lr_flip) {
      buf = buf0;
      flip_buf_sse2(buf1 + width * i, buf, width);
    } else {
      buf = buf1 + width * i;
    }
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit(buf, width, shift[2]);
    transpose_16bit_8x8(buf, buf);
    store_buffer_16bit_to_32bit_w8(buf, output + 8 * width * i, width, 8);
    transpose_16bit_8x8(buf + 8, buf + 8);
    store_buffer_16bit_to_32bit_w8(buf + 8, output + 8 * width * i + 8, width,
                                   8);
  }
}

void av1_lowbd_fwd_txfm2d_16x32_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[32], buf1[64];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_16X32];
  const int txw_idx = get_txw_idx(TX_16X32);
  const int txh_idx = get_txh_idx(TX_16X32);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 16;
  const int height = 32;
  const transform_1d_sse2 col_txfm = col_txfm8x32_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x16_arr[tx_type];

  if (col_txfm != NULL && row_txfm != NULL) {
    int ud_flip, lr_flip;
    get_flip_cfg(tx_type, &ud_flip, &lr_flip);

    for (int i = 0; i < 2; i++) {
      if (ud_flip) {
        load_buffer_16bit_to_16bit_flip(input + 8 * i, stride, buf0, height);
      } else {
        load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
      }
      round_shift_16bit(buf0, height, shift[0]);
      col_txfm(buf0, buf0, cos_bit_col);
      round_shift_16bit(buf0, height, shift[1]);
      transpose_16bit_8x8(buf0 + 0 * 8, buf1 + 0 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 1 * 8, buf1 + 1 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 2 * 8, buf1 + 2 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 3 * 8, buf1 + 3 * width + 8 * i);
    }

    for (int i = 0; i < 4; i++) {
      __m128i *buf;
      if (lr_flip) {
        buf = buf0;
        flip_buf_sse2(buf1 + width * i, buf, width);
      } else {
        buf = buf1 + width * i;
      }
      row_txfm(buf, buf, cos_bit_row);
      round_shift_16bit(buf, width, shift[2]);
      transpose_16bit_8x8(buf, buf);
      store_rect_buffer_16bit_to_32bit_w8(buf, output + 8 * width * i, width,
                                          8);
      transpose_16bit_8x8(buf + 8, buf + 8);
      store_rect_buffer_16bit_to_32bit_w8(buf + 8, output + 8 * width * i + 8,
                                          width, 8);
    }
  } else {
    av1_fwd_txfm2d_16x32_c(input, output, stride, tx_type, bd);
  }
}

void av1_lowbd_fwd_txfm2d_32x8_sse2(const int16_t *input, int32_t *output,
                                    int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[32], buf1[32];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X8];
  const int txw_idx = get_txw_idx(TX_32X8);
  const int txh_idx = get_txh_idx(TX_32X8);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 32;
  const int height = 8;
  const transform_1d_sse2 col_txfm = col_txfm8x8_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x32_arr[tx_type];

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
      col_txfm(buf0, buf0, cos_bit_col);
      round_shift_16bit(buf0, height, shift[1]);
      transpose_16bit_8x8(buf0, buf1 + 0 * width + 8 * i);
    }

    for (int i = 0; i < 1; i++) {
      __m128i *buf;
      if (lr_flip) {
        buf = buf0;
        flip_buf_sse2(buf1 + width * i, buf, width);
      } else {
        buf = buf1 + width * i;
      }
      row_txfm(buf, buf, cos_bit_row);
      round_shift_16bit(buf, width, shift[2]);
      transpose_16bit_8x8(buf, buf);
      store_buffer_16bit_to_32bit_w8(buf, output + 8 * width * i, width,
                                     height);
      transpose_16bit_8x8(buf + 8, buf + 8);
      store_buffer_16bit_to_32bit_w8(buf + 8, output + 8 * width * i + 8, width,
                                     height);
      transpose_16bit_8x8(buf + 16, buf + 16);
      store_buffer_16bit_to_32bit_w8(buf + 16, output + 8 * width * i + 16,
                                     width, height);
      transpose_16bit_8x8(buf + 24, buf + 24);
      store_buffer_16bit_to_32bit_w8(buf + 24, output + 8 * width * i + 24,
                                     width, height);
    }
  } else {
    av1_fwd_txfm2d_32x16_c(input, output, stride, tx_type, bd);
  }
}

void av1_lowbd_fwd_txfm2d_32x16_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[32], buf1[64];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X16];
  const int txw_idx = get_txw_idx(TX_32X16);
  const int txh_idx = get_txh_idx(TX_32X16);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 32;
  const int height = 16;
  const transform_1d_sse2 col_txfm = col_txfm8x16_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x32_arr[tx_type];

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
      col_txfm(buf0, buf0, cos_bit_col);
      round_shift_16bit(buf0, height, shift[1]);
      transpose_16bit_8x8(buf0, buf1 + 0 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 8, buf1 + 1 * width + 8 * i);
    }

    for (int i = 0; i < 2; i++) {
      __m128i *buf;
      if (lr_flip) {
        buf = buf0;
        flip_buf_sse2(buf1 + width * i, buf, width);
      } else {
        buf = buf1 + width * i;
      }
      row_txfm(buf, buf, cos_bit_row);
      round_shift_16bit(buf, width, shift[2]);
      transpose_16bit_8x8(buf, buf);
      store_rect_buffer_16bit_to_32bit_w8(buf, output + 8 * width * i, width,
                                          8);
      transpose_16bit_8x8(buf + 8, buf + 8);
      store_rect_buffer_16bit_to_32bit_w8(buf + 8, output + 8 * width * i + 8,
                                          width, 8);
      transpose_16bit_8x8(buf + 16, buf + 16);
      store_rect_buffer_16bit_to_32bit_w8(buf + 16, output + 8 * width * i + 16,
                                          width, 8);
      transpose_16bit_8x8(buf + 24, buf + 24);
      store_rect_buffer_16bit_to_32bit_w8(buf + 24, output + 8 * width * i + 24,
                                          width, 8);
    }
  } else {
    av1_fwd_txfm2d_32x16_c(input, output, stride, tx_type, bd);
  }
}

void av1_lowbd_fwd_txfm2d_32x32_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf0[32], buf1[128];
  const int8_t *shift = av1_fwd_txfm_shift_ls[TX_32X32];
  const int txw_idx = get_txw_idx(TX_32X32);
  const int txh_idx = get_txh_idx(TX_32X32);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = 32;
  const int height = 32;
  const transform_1d_sse2 col_txfm = col_txfm8x32_arr[tx_type];
  const transform_1d_sse2 row_txfm = row_txfm8x32_arr[tx_type];

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
      col_txfm(buf0, buf0, cos_bit_col);
      round_shift_16bit(buf0, height, shift[1]);
      transpose_16bit_8x8(buf0 + 0 * 8, buf1 + 0 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 1 * 8, buf1 + 1 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 2 * 8, buf1 + 2 * width + 8 * i);
      transpose_16bit_8x8(buf0 + 3 * 8, buf1 + 3 * width + 8 * i);
    }

    for (int i = 0; i < 4; i++) {
      __m128i *buf;
      if (lr_flip) {
        buf = buf0;
        flip_buf_sse2(buf1 + width * i, buf, width);
      } else {
        buf = buf1 + width * i;
      }
      row_txfm(buf, buf, cos_bit_row);
      round_shift_16bit(buf, width, shift[2]);
      transpose_16bit_8x8(buf, buf);
      store_buffer_16bit_to_32bit_w8(buf, output + 8 * width * i, width, 8);
      transpose_16bit_8x8(buf + 8, buf + 8);
      store_buffer_16bit_to_32bit_w8(buf + 8, output + 8 * width * i + 8, width,
                                     8);
      transpose_16bit_8x8(buf + 16, buf + 16);
      store_buffer_16bit_to_32bit_w8(buf + 16, output + 8 * width * i + 16,
                                     width, 8);
      transpose_16bit_8x8(buf + 24, buf + 24);
      store_buffer_16bit_to_32bit_w8(buf + 24, output + 8 * width * i + 24,
                                     width, 8);
    }
  } else {
    av1_fwd_txfm2d_32x32_c(input, output, stride, tx_type, bd);
  }
}

void av1_lowbd_fwd_txfm2d_64x16_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_64X16;
  __m128i buf0[64], buf1[128];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_sse2 col_txfm = fdct8x16_new_sse2;
  const transform_1d_sse2 row_txfm = av1_fdct8x64_new_sse2;
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < height_div8; ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }

  for (int i = 0; i < height_div8; i++) {
    __m128i *buf = buf1 + width * i;
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit(buf, width, shift[2]);
    int32_t *output8 = output + 8 * 32 * i;
    for (int j = 0; j < 4; ++j) {
      __m128i *buf8 = buf + 8 * j;
      transpose_16bit_8x8(buf8, buf8);
      store_buffer_16bit_to_32bit_w8(buf8, output8 + 8 * j, 32, 8);
    }
  }
}

void av1_lowbd_fwd_txfm2d_16x64_sse2(const int16_t *input, int32_t *output,
                                     int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  (void)tx_type;
  assert(tx_type == DCT_DCT);
  const TX_SIZE tx_size = TX_16X64;
  __m128i buf0[64], buf1[128];
  const int8_t *shift = av1_fwd_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = av1_fwd_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = av1_fwd_cos_bit_row[txw_idx][txh_idx];
  const int width = tx_size_wide[tx_size];
  const int height = tx_size_high[tx_size];
  const transform_1d_sse2 col_txfm = av1_fdct8x64_new_sse2;
  const transform_1d_sse2 row_txfm = fdct8x16_new_sse2;
  const int width_div8 = (width >> 3);
  const int height_div8 = (height >> 3);

  for (int i = 0; i < width_div8; i++) {
    load_buffer_16bit_to_16bit(input + 8 * i, stride, buf0, height);
    round_shift_16bit(buf0, height, shift[0]);
    col_txfm(buf0, buf0, cos_bit_col);
    round_shift_16bit(buf0, height, shift[1]);
    for (int j = 0; j < height_div8; ++j) {
      transpose_16bit_8x8(buf0 + j * 8, buf1 + j * width + 8 * i);
    }
  }

  for (int i = 0; i < AOMMIN(4, height_div8); i++) {
    __m128i *buf = buf1 + width * i;
    row_txfm(buf, buf, cos_bit_row);
    round_shift_16bit(buf, width, shift[2]);
    int32_t *output8 = output + 8 * width * i;
    for (int j = 0; j < width_div8; ++j) {
      __m128i *buf8 = buf + 8 * j;
      transpose_16bit_8x8(buf8, buf8);
      store_buffer_16bit_to_32bit_w8(buf8, output8 + 8 * j, width, 8);
    }
  }
  // Zero out the bottom 16x32 area.
  memset(output + 16 * 32, 0, 16 * 32 * sizeof(*output));
}

static FwdTxfm2dFunc fwd_txfm2d_func_ls[TX_SIZES_ALL] = {
  av1_lowbd_fwd_txfm2d_4x4_sse2,    // 4x4 transform
  av1_lowbd_fwd_txfm2d_8x8_sse2,    // 8x8 transform
  av1_lowbd_fwd_txfm2d_16x16_sse2,  // 16x16 transform
  av1_lowbd_fwd_txfm2d_32x32_sse2,  // 32x32 transform
  NULL,                             // 64x64 transform
  av1_lowbd_fwd_txfm2d_4x8_sse2,    // 4x8 transform
  av1_lowbd_fwd_txfm2d_8x4_sse2,    // 8x4 transform
  av1_lowbd_fwd_txfm2d_8x16_sse2,   // 8x16 transform
  av1_lowbd_fwd_txfm2d_16x8_sse2,   // 16x8 transform
  av1_lowbd_fwd_txfm2d_16x32_sse2,  // 16x32 transform
  av1_lowbd_fwd_txfm2d_32x16_sse2,  // 32x16 transform
  NULL,                             // 32x64 transform
  NULL,                             // 64x32 transform
  av1_lowbd_fwd_txfm2d_4x16_sse2,   // 4x16 transform
  av1_lowbd_fwd_txfm2d_16x4_sse2,   // 16x4 transform
  av1_lowbd_fwd_txfm2d_8x32_sse2,   // 8x32 transform
  av1_lowbd_fwd_txfm2d_32x8_sse2,   // 32x8 transform
  av1_lowbd_fwd_txfm2d_16x64_sse2,  // 16x64 transform
  av1_lowbd_fwd_txfm2d_64x16_sse2,  // 64x16 transform
};

void av1_lowbd_fwd_txfm_sse2(const int16_t *src_diff, tran_low_t *coeff,
                             int diff_stride, TxfmParam *txfm_param) {
  FwdTxfm2dFunc fwd_txfm2d_func = fwd_txfm2d_func_ls[txfm_param->tx_size];

  if ((fwd_txfm2d_func == NULL) ||
      (txfm_param->lossless && txfm_param->tx_size == TX_4X4))
    av1_lowbd_fwd_txfm_c(src_diff, coeff, diff_stride, txfm_param);
  else
    fwd_txfm2d_func(src_diff, coeff, diff_stride, txfm_param->tx_type,
                    txfm_param->bd);
}
