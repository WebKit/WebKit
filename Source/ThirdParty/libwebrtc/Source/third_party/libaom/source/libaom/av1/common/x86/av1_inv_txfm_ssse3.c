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

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/x86/av1_inv_txfm_ssse3.h"
#include "av1/common/x86/av1_txfm_sse2.h"

// TODO(venkatsanampudi@ittiam.com): move this to header file

// Sqrt2, Sqrt2^2, Sqrt2^3, Sqrt2^4, Sqrt2^5
static int32_t NewSqrt2list[TX_SIZES] = { 5793, 2 * 4096, 2 * 5793, 4 * 4096,
                                          4 * 5793 };

// TODO(binpengsmail@gmail.com): replace some for loop with do {} while

static void idct4_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);

  // stage 1
  __m128i x[4];
  x[0] = input[0];
  x[1] = input[2];
  x[2] = input[1];
  x[3] = input[3];

  // stage 2
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[0], x[1], x[0], x[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x[2], x[3], x[2], x[3]);

  // stage 3
  btf_16_adds_subs_out_sse2(output[0], output[3], x[0], x[3]);
  btf_16_adds_subs_out_sse2(output[1], output[2], x[1], x[2]);
}

static void idct4_w4_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);

  // stage 1
  __m128i x[4];
  x[0] = input[0];
  x[1] = input[2];
  x[2] = input[1];
  x[3] = input[3];

  // stage 2
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[0], x[1], x[0], x[1]);
  btf_16_4p_sse2(cospi_p48_m16, cospi_p16_p48, x[2], x[3], x[2], x[3]);

  // stage 3
  btf_16_adds_subs_out_sse2(output[0], output[3], x[0], x[3]);
  btf_16_adds_subs_out_sse2(output[1], output[2], x[1], x[2]);
}

static void idct8_low1_ssse3(const __m128i *input, __m128i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);

  // stage 1
  __m128i x[2];
  x[0] = input[0];

  // stage 2
  // stage 3
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);

  // stage 4
  // stage 5
  output[0] = x[0];
  output[7] = x[0];
  output[1] = x[1];
  output[6] = x[1];
  output[2] = x[1];
  output[5] = x[1];
  output[3] = x[0];
  output[4] = x[0];
}

static void idct8_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  const __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  const __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  const __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x[8];
  x[0] = input[0];
  x[1] = input[4];
  x[2] = input[2];
  x[3] = input[6];
  x[4] = input[1];
  x[5] = input[5];
  x[6] = input[3];
  x[7] = input[7];

  // stage 2
  btf_16_sse2(cospi_p56_m08, cospi_p08_p56, x[4], x[7], x[4], x[7]);
  btf_16_sse2(cospi_p24_m40, cospi_p40_p24, x[5], x[6], x[5], x[6]);

  // stage 3
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[0], x[1], x[0], x[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x[2], x[3], x[2], x[3]);
  btf_16_adds_subs_sse2(x[4], x[5]);
  btf_16_subs_adds_sse2(x[7], x[6]);

  // stage 4
  btf_16_adds_subs_sse2(x[0], x[3]);
  btf_16_adds_subs_sse2(x[1], x[2]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[5], x[6], x[5], x[6]);

  // stage 5
  btf_16_adds_subs_out_sse2(output[0], output[7], x[0], x[7]);
  btf_16_adds_subs_out_sse2(output[1], output[6], x[1], x[6]);
  btf_16_adds_subs_out_sse2(output[2], output[5], x[2], x[5]);
  btf_16_adds_subs_out_sse2(output[3], output[4], x[3], x[4]);
}

static void idct8_w4_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  const __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  const __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  const __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x[8];
  x[0] = input[0];
  x[1] = input[4];
  x[2] = input[2];
  x[3] = input[6];
  x[4] = input[1];
  x[5] = input[5];
  x[6] = input[3];
  x[7] = input[7];

  // stage 2
  btf_16_4p_sse2(cospi_p56_m08, cospi_p08_p56, x[4], x[7], x[4], x[7]);
  btf_16_4p_sse2(cospi_p24_m40, cospi_p40_p24, x[5], x[6], x[5], x[6]);

  // stage 3
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[0], x[1], x[0], x[1]);
  btf_16_4p_sse2(cospi_p48_m16, cospi_p16_p48, x[2], x[3], x[2], x[3]);
  btf_16_adds_subs_sse2(x[4], x[5]);
  btf_16_subs_adds_sse2(x[7], x[6]);

  // stage 4
  btf_16_adds_subs_sse2(x[0], x[3]);
  btf_16_adds_subs_sse2(x[1], x[2]);
  btf_16_4p_sse2(cospi_m32_p32, cospi_p32_p32, x[5], x[6], x[5], x[6]);

  // stage 5
  btf_16_adds_subs_out_sse2(output[0], output[7], x[0], x[7]);
  btf_16_adds_subs_out_sse2(output[1], output[6], x[1], x[6]);
  btf_16_adds_subs_out_sse2(output[2], output[5], x[2], x[5]);
  btf_16_adds_subs_out_sse2(output[3], output[4], x[3], x[4]);
}

static INLINE void idct16_stage5_sse2(__m128i *x, const int32_t *cospi,
                                      const __m128i __rounding,
                                      int8_t cos_bit) {
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_sse2(x[0], x[3]);
  btf_16_adds_subs_sse2(x[1], x[2]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[5], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[11]);
  btf_16_adds_subs_sse2(x[9], x[10]);
  btf_16_subs_adds_sse2(x[15], x[12]);
  btf_16_subs_adds_sse2(x[14], x[13]);
}

static INLINE void idct16_stage6_sse2(__m128i *x, const int32_t *cospi,
                                      const __m128i __rounding,
                                      int8_t cos_bit) {
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_sse2(x[0], x[7]);
  btf_16_adds_subs_sse2(x[1], x[6]);
  btf_16_adds_subs_sse2(x[2], x[5]);
  btf_16_adds_subs_sse2(x[3], x[4]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[10], x[13], x[10], x[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[11], x[12], x[11], x[12]);
}

static INLINE void idct16_stage7_sse2(__m128i *output, __m128i *x) {
  btf_16_adds_subs_out_sse2(output[0], output[15], x[0], x[15]);
  btf_16_adds_subs_out_sse2(output[1], output[14], x[1], x[14]);
  btf_16_adds_subs_out_sse2(output[2], output[13], x[2], x[13]);
  btf_16_adds_subs_out_sse2(output[3], output[12], x[3], x[12]);
  btf_16_adds_subs_out_sse2(output[4], output[11], x[4], x[11]);
  btf_16_adds_subs_out_sse2(output[5], output[10], x[5], x[10]);
  btf_16_adds_subs_out_sse2(output[6], output[9], x[6], x[9]);
  btf_16_adds_subs_out_sse2(output[7], output[8], x[7], x[8]);
}

static void idct16_low1_ssse3(const __m128i *input, __m128i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);

  // stage 1
  __m128i x[2];
  x[0] = input[0];

  // stage 2
  // stage 3
  // stage 4
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);

  // stage 5
  // stage 6
  // stage 7
  output[0] = x[0];
  output[15] = x[0];
  output[1] = x[1];
  output[14] = x[1];
  output[2] = x[1];
  output[13] = x[1];
  output[3] = x[0];
  output[12] = x[0];
  output[4] = x[0];
  output[11] = x[0];
  output[5] = x[1];
  output[10] = x[1];
  output[6] = x[1];
  output[9] = x[1];
  output[7] = x[0];
  output[8] = x[0];
}

static void idct16_low8_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);

  // stage 1
  __m128i x[16];
  x[0] = input[0];
  x[2] = input[4];
  x[4] = input[2];
  x[6] = input[6];
  x[8] = input[1];
  x[10] = input[5];
  x[12] = input[3];
  x[14] = input[7];

  // stage 2
  btf_16_ssse3(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_ssse3(-cospi[36], cospi[28], x[14], x[9], x[14]);
  btf_16_ssse3(cospi[44], cospi[20], x[10], x[10], x[13]);
  btf_16_ssse3(-cospi[52], cospi[12], x[12], x[11], x[12]);

  // stage 3
  btf_16_ssse3(cospi[56], cospi[8], x[4], x[4], x[7]);
  btf_16_ssse3(-cospi[40], cospi[24], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[9]);
  btf_16_subs_adds_sse2(x[11], x[10]);
  btf_16_adds_subs_sse2(x[12], x[13]);
  btf_16_subs_adds_sse2(x[15], x[14]);

  // stage 4
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_ssse3(cospi[48], cospi[16], x[2], x[2], x[3]);
  btf_16_adds_subs_sse2(x[4], x[5]);
  btf_16_subs_adds_sse2(x[7], x[6]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[9], x[14], x[9], x[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[10], x[13], x[10], x[13]);

  idct16_stage5_sse2(x, cospi, __rounding, cos_bit);
  idct16_stage6_sse2(x, cospi, __rounding, cos_bit);
  idct16_stage7_sse2(output, x);
}

static void idct16_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  const __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  const __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  const __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  const __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  const __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  const __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  const __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  const __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  const __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  const __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  const __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);

  // stage 1
  __m128i x[16];
  x[0] = input[0];
  x[1] = input[8];
  x[2] = input[4];
  x[3] = input[12];
  x[4] = input[2];
  x[5] = input[10];
  x[6] = input[6];
  x[7] = input[14];
  x[8] = input[1];
  x[9] = input[9];
  x[10] = input[5];
  x[11] = input[13];
  x[12] = input[3];
  x[13] = input[11];
  x[14] = input[7];
  x[15] = input[15];

  // stage 2
  btf_16_sse2(cospi_p60_m04, cospi_p04_p60, x[8], x[15], x[8], x[15]);
  btf_16_sse2(cospi_p28_m36, cospi_p36_p28, x[9], x[14], x[9], x[14]);
  btf_16_sse2(cospi_p44_m20, cospi_p20_p44, x[10], x[13], x[10], x[13]);
  btf_16_sse2(cospi_p12_m52, cospi_p52_p12, x[11], x[12], x[11], x[12]);

  // stage 3
  btf_16_sse2(cospi_p56_m08, cospi_p08_p56, x[4], x[7], x[4], x[7]);
  btf_16_sse2(cospi_p24_m40, cospi_p40_p24, x[5], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[9]);
  btf_16_subs_adds_sse2(x[11], x[10]);
  btf_16_adds_subs_sse2(x[12], x[13]);
  btf_16_subs_adds_sse2(x[15], x[14]);

  // stage 4
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[0], x[1], x[0], x[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x[2], x[3], x[2], x[3]);
  btf_16_adds_subs_sse2(x[4], x[5]);
  btf_16_subs_adds_sse2(x[7], x[6]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[9], x[14], x[9], x[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[10], x[13], x[10], x[13]);

  // stage 5~7
  idct16_stage5_sse2(x, cospi, __rounding, cos_bit);
  idct16_stage6_sse2(x, cospi, __rounding, cos_bit);
  idct16_stage7_sse2(output, x);
}

static void idct16_w4_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  const __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  const __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  const __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  const __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  const __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  const __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  const __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  const __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  const __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  const __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  const __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x[16];
  x[0] = input[0];
  x[1] = input[8];
  x[2] = input[4];
  x[3] = input[12];
  x[4] = input[2];
  x[5] = input[10];
  x[6] = input[6];
  x[7] = input[14];
  x[8] = input[1];
  x[9] = input[9];
  x[10] = input[5];
  x[11] = input[13];
  x[12] = input[3];
  x[13] = input[11];
  x[14] = input[7];
  x[15] = input[15];

  // stage 2
  btf_16_4p_sse2(cospi_p60_m04, cospi_p04_p60, x[8], x[15], x[8], x[15]);
  btf_16_4p_sse2(cospi_p28_m36, cospi_p36_p28, x[9], x[14], x[9], x[14]);
  btf_16_4p_sse2(cospi_p44_m20, cospi_p20_p44, x[10], x[13], x[10], x[13]);
  btf_16_4p_sse2(cospi_p12_m52, cospi_p52_p12, x[11], x[12], x[11], x[12]);

  // stage 3
  btf_16_4p_sse2(cospi_p56_m08, cospi_p08_p56, x[4], x[7], x[4], x[7]);
  btf_16_4p_sse2(cospi_p24_m40, cospi_p40_p24, x[5], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[9]);
  btf_16_subs_adds_sse2(x[11], x[10]);
  btf_16_adds_subs_sse2(x[12], x[13]);
  btf_16_subs_adds_sse2(x[15], x[14]);

  // stage 4
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[0], x[1], x[0], x[1]);
  btf_16_4p_sse2(cospi_p48_m16, cospi_p16_p48, x[2], x[3], x[2], x[3]);
  btf_16_adds_subs_sse2(x[4], x[5]);
  btf_16_subs_adds_sse2(x[7], x[6]);
  btf_16_4p_sse2(cospi_m16_p48, cospi_p48_p16, x[9], x[14], x[9], x[14]);
  btf_16_4p_sse2(cospi_m48_m16, cospi_m16_p48, x[10], x[13], x[10], x[13]);

  // stage 5
  btf_16_adds_subs_sse2(x[0], x[3]);
  btf_16_adds_subs_sse2(x[1], x[2]);
  btf_16_4p_sse2(cospi_m32_p32, cospi_p32_p32, x[5], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[11]);
  btf_16_adds_subs_sse2(x[9], x[10]);
  btf_16_subs_adds_sse2(x[15], x[12]);
  btf_16_subs_adds_sse2(x[14], x[13]);

  // stage 6
  btf_16_adds_subs_sse2(x[0], x[7]);
  btf_16_adds_subs_sse2(x[1], x[6]);
  btf_16_adds_subs_sse2(x[2], x[5]);
  btf_16_adds_subs_sse2(x[3], x[4]);
  btf_16_4p_sse2(cospi_m32_p32, cospi_p32_p32, x[10], x[13], x[10], x[13]);
  btf_16_4p_sse2(cospi_m32_p32, cospi_p32_p32, x[11], x[12], x[11], x[12]);

  // stage 7
  idct16_stage7_sse2(output, x);
}

static INLINE void idct32_high16_stage3_sse2(__m128i *x) {
  btf_16_adds_subs_sse2(x[16], x[17]);
  btf_16_subs_adds_sse2(x[19], x[18]);
  btf_16_adds_subs_sse2(x[20], x[21]);
  btf_16_subs_adds_sse2(x[23], x[22]);
  btf_16_adds_subs_sse2(x[24], x[25]);
  btf_16_subs_adds_sse2(x[27], x[26]);
  btf_16_adds_subs_sse2(x[28], x[29]);
  btf_16_subs_adds_sse2(x[31], x[30]);
}

static INLINE void idct32_high16_stage4_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  const __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  const __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  const __m128i cospi_m56_m08 = pair_set_epi16(-cospi[56], -cospi[8]);
  const __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  const __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  const __m128i cospi_m24_m40 = pair_set_epi16(-cospi[24], -cospi[40]);
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x[17], x[30], x[17], x[30]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x[18], x[29], x[18], x[29]);
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x[21], x[26], x[21], x[26]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x[22], x[25], x[22], x[25]);
}

static INLINE void idct32_high24_stage5_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[9], x[14], x[9], x[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[10], x[13], x[10], x[13]);
  btf_16_adds_subs_sse2(x[16], x[19]);
  btf_16_adds_subs_sse2(x[17], x[18]);
  btf_16_subs_adds_sse2(x[23], x[20]);
  btf_16_subs_adds_sse2(x[22], x[21]);
  btf_16_adds_subs_sse2(x[24], x[27]);
  btf_16_adds_subs_sse2(x[25], x[26]);
  btf_16_subs_adds_sse2(x[31], x[28]);
  btf_16_subs_adds_sse2(x[30], x[29]);
}

static INLINE void idct32_high28_stage6_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[5], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[11]);
  btf_16_adds_subs_sse2(x[9], x[10]);
  btf_16_subs_adds_sse2(x[15], x[12]);
  btf_16_subs_adds_sse2(x[14], x[13]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[18], x[29], x[18], x[29]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[19], x[28], x[19], x[28]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[20], x[27], x[20], x[27]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[21], x[26], x[21], x[26]);
}

static INLINE void idct32_stage7_sse2(__m128i *x, const int32_t *cospi,
                                      const __m128i __rounding,
                                      int8_t cos_bit) {
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_sse2(x[0], x[7]);
  btf_16_adds_subs_sse2(x[1], x[6]);
  btf_16_adds_subs_sse2(x[2], x[5]);
  btf_16_adds_subs_sse2(x[3], x[4]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[10], x[13], x[10], x[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[11], x[12], x[11], x[12]);
  btf_16_adds_subs_sse2(x[16], x[23]);
  btf_16_adds_subs_sse2(x[17], x[22]);
  btf_16_adds_subs_sse2(x[18], x[21]);
  btf_16_adds_subs_sse2(x[19], x[20]);
  btf_16_subs_adds_sse2(x[31], x[24]);
  btf_16_subs_adds_sse2(x[30], x[25]);
  btf_16_subs_adds_sse2(x[29], x[26]);
  btf_16_subs_adds_sse2(x[28], x[27]);
}

static INLINE void idct32_stage8_sse2(__m128i *x, const int32_t *cospi,
                                      const __m128i __rounding,
                                      int8_t cos_bit) {
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_sse2(x[0], x[15]);
  btf_16_adds_subs_sse2(x[1], x[14]);
  btf_16_adds_subs_sse2(x[2], x[13]);
  btf_16_adds_subs_sse2(x[3], x[12]);
  btf_16_adds_subs_sse2(x[4], x[11]);
  btf_16_adds_subs_sse2(x[5], x[10]);
  btf_16_adds_subs_sse2(x[6], x[9]);
  btf_16_adds_subs_sse2(x[7], x[8]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[20], x[27], x[20], x[27]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[21], x[26], x[21], x[26]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[22], x[25], x[22], x[25]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[23], x[24], x[23], x[24]);
}

static INLINE void idct32_stage9_sse2(__m128i *output, __m128i *x) {
  btf_16_adds_subs_out_sse2(output[0], output[31], x[0], x[31]);
  btf_16_adds_subs_out_sse2(output[1], output[30], x[1], x[30]);
  btf_16_adds_subs_out_sse2(output[2], output[29], x[2], x[29]);
  btf_16_adds_subs_out_sse2(output[3], output[28], x[3], x[28]);
  btf_16_adds_subs_out_sse2(output[4], output[27], x[4], x[27]);
  btf_16_adds_subs_out_sse2(output[5], output[26], x[5], x[26]);
  btf_16_adds_subs_out_sse2(output[6], output[25], x[6], x[25]);
  btf_16_adds_subs_out_sse2(output[7], output[24], x[7], x[24]);
  btf_16_adds_subs_out_sse2(output[8], output[23], x[8], x[23]);
  btf_16_adds_subs_out_sse2(output[9], output[22], x[9], x[22]);
  btf_16_adds_subs_out_sse2(output[10], output[21], x[10], x[21]);
  btf_16_adds_subs_out_sse2(output[11], output[20], x[11], x[20]);
  btf_16_adds_subs_out_sse2(output[12], output[19], x[12], x[19]);
  btf_16_adds_subs_out_sse2(output[13], output[18], x[13], x[18]);
  btf_16_adds_subs_out_sse2(output[14], output[17], x[14], x[17]);
  btf_16_adds_subs_out_sse2(output[15], output[16], x[15], x[16]);
}

static void idct32_low1_ssse3(const __m128i *input, __m128i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);

  // stage 1
  __m128i x[2];
  x[0] = input[0];

  // stage 2
  // stage 3
  // stage 4
  // stage 5
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);

  // stage 6
  // stage 7
  // stage 8
  // stage 9
  output[0] = x[0];
  output[31] = x[0];
  output[1] = x[1];
  output[30] = x[1];
  output[2] = x[1];
  output[29] = x[1];
  output[3] = x[0];
  output[28] = x[0];
  output[4] = x[0];
  output[27] = x[0];
  output[5] = x[1];
  output[26] = x[1];
  output[6] = x[1];
  output[25] = x[1];
  output[7] = x[0];
  output[24] = x[0];
  output[8] = x[0];
  output[23] = x[0];
  output[9] = x[1];
  output[22] = x[1];
  output[10] = x[1];
  output[21] = x[1];
  output[11] = x[0];
  output[20] = x[0];
  output[12] = x[0];
  output[19] = x[0];
  output[13] = x[1];
  output[18] = x[1];
  output[14] = x[1];
  output[17] = x[1];
  output[15] = x[0];
  output[16] = x[0];
}

static void idct32_low8_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  // stage 1
  __m128i x[32];
  x[0] = input[0];
  x[4] = input[4];
  x[8] = input[2];
  x[12] = input[6];
  x[16] = input[1];
  x[20] = input[5];
  x[24] = input[3];
  x[28] = input[7];

  // stage 2
  btf_16_ssse3(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_ssse3(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_ssse3(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_ssse3(-cospi[58], cospi[6], x[24], x[23], x[24]);

  // stage 3
  btf_16_ssse3(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_ssse3(-cospi[52], cospi[12], x[12], x[11], x[12]);
  x[17] = x[16];
  x[18] = x[19];
  x[21] = x[20];
  x[22] = x[23];
  x[25] = x[24];
  x[26] = x[27];
  x[29] = x[28];
  x[30] = x[31];

  // stage 4
  btf_16_ssse3(cospi[56], cospi[8], x[4], x[4], x[7]);
  x[9] = x[8];
  x[10] = x[11];
  x[13] = x[12];
  x[14] = x[15];
  idct32_high16_stage4_sse2(x, cospi, __rounding, cos_bit);

  // stage 5
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);
  x[5] = x[4];
  x[6] = x[7];
  idct32_high24_stage5_sse2(x, cospi, __rounding, cos_bit);
  // stage 6
  x[3] = x[0];
  x[2] = x[1];
  idct32_high28_stage6_sse2(x, cospi, __rounding, cos_bit);

  idct32_stage7_sse2(x, cospi, __rounding, cos_bit);
  idct32_stage8_sse2(x, cospi, __rounding, cos_bit);
  idct32_stage9_sse2(output, x);
}

static void idct32_low16_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  // stage 1
  __m128i x[32];
  x[0] = input[0];
  x[2] = input[8];
  x[4] = input[4];
  x[6] = input[12];
  x[8] = input[2];
  x[10] = input[10];
  x[12] = input[6];
  x[14] = input[14];
  x[16] = input[1];
  x[18] = input[9];
  x[20] = input[5];
  x[22] = input[13];
  x[24] = input[3];
  x[26] = input[11];
  x[28] = input[7];
  x[30] = input[15];

  // stage 2
  btf_16_ssse3(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_ssse3(-cospi[34], cospi[30], x[30], x[17], x[30]);
  btf_16_ssse3(cospi[46], cospi[18], x[18], x[18], x[29]);
  btf_16_ssse3(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_ssse3(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_ssse3(-cospi[42], cospi[22], x[26], x[21], x[26]);
  btf_16_ssse3(cospi[38], cospi[26], x[22], x[22], x[25]);
  btf_16_ssse3(-cospi[58], cospi[6], x[24], x[23], x[24]);

  // stage 3
  btf_16_ssse3(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_ssse3(-cospi[36], cospi[28], x[14], x[9], x[14]);
  btf_16_ssse3(cospi[44], cospi[20], x[10], x[10], x[13]);
  btf_16_ssse3(-cospi[52], cospi[12], x[12], x[11], x[12]);
  idct32_high16_stage3_sse2(x);

  // stage 4
  btf_16_ssse3(cospi[56], cospi[8], x[4], x[4], x[7]);
  btf_16_ssse3(-cospi[40], cospi[24], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[9]);
  btf_16_subs_adds_sse2(x[11], x[10]);
  btf_16_adds_subs_sse2(x[12], x[13]);
  btf_16_subs_adds_sse2(x[15], x[14]);
  idct32_high16_stage4_sse2(x, cospi, __rounding, cos_bit);

  // stage 5
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_ssse3(cospi[48], cospi[16], x[2], x[2], x[3]);
  btf_16_adds_subs_sse2(x[4], x[5]);
  btf_16_subs_adds_sse2(x[7], x[6]);
  idct32_high24_stage5_sse2(x, cospi, __rounding, cos_bit);

  btf_16_adds_subs_sse2(x[0], x[3]);
  btf_16_adds_subs_sse2(x[1], x[2]);
  idct32_high28_stage6_sse2(x, cospi, __rounding, cos_bit);

  idct32_stage7_sse2(x, cospi, __rounding, cos_bit);
  idct32_stage8_sse2(x, cospi, __rounding, cos_bit);
  idct32_stage9_sse2(output, x);
}

static void idct32_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p62_m02 = pair_set_epi16(cospi[62], -cospi[2]);
  const __m128i cospi_p02_p62 = pair_set_epi16(cospi[2], cospi[62]);
  const __m128i cospi_p30_m34 = pair_set_epi16(cospi[30], -cospi[34]);
  const __m128i cospi_p34_p30 = pair_set_epi16(cospi[34], cospi[30]);
  const __m128i cospi_p46_m18 = pair_set_epi16(cospi[46], -cospi[18]);
  const __m128i cospi_p18_p46 = pair_set_epi16(cospi[18], cospi[46]);
  const __m128i cospi_p14_m50 = pair_set_epi16(cospi[14], -cospi[50]);
  const __m128i cospi_p50_p14 = pair_set_epi16(cospi[50], cospi[14]);
  const __m128i cospi_p54_m10 = pair_set_epi16(cospi[54], -cospi[10]);
  const __m128i cospi_p10_p54 = pair_set_epi16(cospi[10], cospi[54]);
  const __m128i cospi_p22_m42 = pair_set_epi16(cospi[22], -cospi[42]);
  const __m128i cospi_p42_p22 = pair_set_epi16(cospi[42], cospi[22]);
  const __m128i cospi_p38_m26 = pair_set_epi16(cospi[38], -cospi[26]);
  const __m128i cospi_p26_p38 = pair_set_epi16(cospi[26], cospi[38]);
  const __m128i cospi_p06_m58 = pair_set_epi16(cospi[6], -cospi[58]);
  const __m128i cospi_p58_p06 = pair_set_epi16(cospi[58], cospi[6]);
  const __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  const __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  const __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  const __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  const __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  const __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  const __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  const __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  const __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  const __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  const __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  const __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);

  // stage 1
  __m128i x[32];
  x[0] = input[0];
  x[1] = input[16];
  x[2] = input[8];
  x[3] = input[24];
  x[4] = input[4];
  x[5] = input[20];
  x[6] = input[12];
  x[7] = input[28];
  x[8] = input[2];
  x[9] = input[18];
  x[10] = input[10];
  x[11] = input[26];
  x[12] = input[6];
  x[13] = input[22];
  x[14] = input[14];
  x[15] = input[30];
  x[16] = input[1];
  x[17] = input[17];
  x[18] = input[9];
  x[19] = input[25];
  x[20] = input[5];
  x[21] = input[21];
  x[22] = input[13];
  x[23] = input[29];
  x[24] = input[3];
  x[25] = input[19];
  x[26] = input[11];
  x[27] = input[27];
  x[28] = input[7];
  x[29] = input[23];
  x[30] = input[15];
  x[31] = input[31];

  // stage 2
  btf_16_sse2(cospi_p62_m02, cospi_p02_p62, x[16], x[31], x[16], x[31]);
  btf_16_sse2(cospi_p30_m34, cospi_p34_p30, x[17], x[30], x[17], x[30]);
  btf_16_sse2(cospi_p46_m18, cospi_p18_p46, x[18], x[29], x[18], x[29]);
  btf_16_sse2(cospi_p14_m50, cospi_p50_p14, x[19], x[28], x[19], x[28]);
  btf_16_sse2(cospi_p54_m10, cospi_p10_p54, x[20], x[27], x[20], x[27]);
  btf_16_sse2(cospi_p22_m42, cospi_p42_p22, x[21], x[26], x[21], x[26]);
  btf_16_sse2(cospi_p38_m26, cospi_p26_p38, x[22], x[25], x[22], x[25]);
  btf_16_sse2(cospi_p06_m58, cospi_p58_p06, x[23], x[24], x[23], x[24]);

  // stage 3
  btf_16_sse2(cospi_p60_m04, cospi_p04_p60, x[8], x[15], x[8], x[15]);
  btf_16_sse2(cospi_p28_m36, cospi_p36_p28, x[9], x[14], x[9], x[14]);
  btf_16_sse2(cospi_p44_m20, cospi_p20_p44, x[10], x[13], x[10], x[13]);
  btf_16_sse2(cospi_p12_m52, cospi_p52_p12, x[11], x[12], x[11], x[12]);
  idct32_high16_stage3_sse2(x);

  // stage 4
  btf_16_sse2(cospi_p56_m08, cospi_p08_p56, x[4], x[7], x[4], x[7]);
  btf_16_sse2(cospi_p24_m40, cospi_p40_p24, x[5], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[9]);
  btf_16_subs_adds_sse2(x[11], x[10]);
  btf_16_adds_subs_sse2(x[12], x[13]);
  btf_16_subs_adds_sse2(x[15], x[14]);
  idct32_high16_stage4_sse2(x, cospi, __rounding, cos_bit);

  // stage 5
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[0], x[1], x[0], x[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x[2], x[3], x[2], x[3]);
  btf_16_adds_subs_sse2(x[4], x[5]);
  btf_16_adds_subs_sse2(x[7], x[6]);
  idct32_high24_stage5_sse2(x, cospi, __rounding, cos_bit);

  // stage 6
  btf_16_adds_subs_sse2(x[0], x[3]);
  btf_16_adds_subs_sse2(x[1], x[2]);
  idct32_high28_stage6_sse2(x, cospi, __rounding, cos_bit);

  // stage 7~8
  idct32_stage7_sse2(x, cospi, __rounding, cos_bit);
  idct32_stage8_sse2(x, cospi, __rounding, cos_bit);
  idct32_stage9_sse2(output, x);
}

static INLINE void idct64_stage4_high32_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  const __m128i cospi_m04_p60 = pair_set_epi16(-cospi[4], cospi[60]);
  const __m128i cospi_p60_p04 = pair_set_epi16(cospi[60], cospi[4]);
  const __m128i cospi_m60_m04 = pair_set_epi16(-cospi[60], -cospi[4]);
  const __m128i cospi_m36_p28 = pair_set_epi16(-cospi[36], cospi[28]);
  const __m128i cospi_p28_p36 = pair_set_epi16(cospi[28], cospi[36]);
  const __m128i cospi_m28_m36 = pair_set_epi16(-cospi[28], -cospi[36]);
  const __m128i cospi_m20_p44 = pair_set_epi16(-cospi[20], cospi[44]);
  const __m128i cospi_p44_p20 = pair_set_epi16(cospi[44], cospi[20]);
  const __m128i cospi_m44_m20 = pair_set_epi16(-cospi[44], -cospi[20]);
  const __m128i cospi_m52_p12 = pair_set_epi16(-cospi[52], cospi[12]);
  const __m128i cospi_p12_p52 = pair_set_epi16(cospi[12], cospi[52]);
  const __m128i cospi_m12_m52 = pair_set_epi16(-cospi[12], -cospi[52]);
  btf_16_sse2(cospi_m04_p60, cospi_p60_p04, x[33], x[62], x[33], x[62]);
  btf_16_sse2(cospi_m60_m04, cospi_m04_p60, x[34], x[61], x[34], x[61]);
  btf_16_sse2(cospi_m36_p28, cospi_p28_p36, x[37], x[58], x[37], x[58]);
  btf_16_sse2(cospi_m28_m36, cospi_m36_p28, x[38], x[57], x[38], x[57]);
  btf_16_sse2(cospi_m20_p44, cospi_p44_p20, x[41], x[54], x[41], x[54]);
  btf_16_sse2(cospi_m44_m20, cospi_m20_p44, x[42], x[53], x[42], x[53]);
  btf_16_sse2(cospi_m52_p12, cospi_p12_p52, x[45], x[50], x[45], x[50]);
  btf_16_sse2(cospi_m12_m52, cospi_m52_p12, x[46], x[49], x[46], x[49]);
}

static INLINE void idct64_stage5_high48_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  const __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  const __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  const __m128i cospi_m56_m08 = pair_set_epi16(-cospi[56], -cospi[8]);
  const __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  const __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  const __m128i cospi_m24_m40 = pair_set_epi16(-cospi[24], -cospi[40]);
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x[17], x[30], x[17], x[30]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x[18], x[29], x[18], x[29]);
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x[21], x[26], x[21], x[26]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x[22], x[25], x[22], x[25]);
  btf_16_adds_subs_sse2(x[32], x[35]);
  btf_16_adds_subs_sse2(x[33], x[34]);
  btf_16_subs_adds_sse2(x[39], x[36]);
  btf_16_subs_adds_sse2(x[38], x[37]);
  btf_16_adds_subs_sse2(x[40], x[43]);
  btf_16_adds_subs_sse2(x[41], x[42]);
  btf_16_subs_adds_sse2(x[47], x[44]);
  btf_16_subs_adds_sse2(x[46], x[45]);
  btf_16_adds_subs_sse2(x[48], x[51]);
  btf_16_adds_subs_sse2(x[49], x[50]);
  btf_16_subs_adds_sse2(x[55], x[52]);
  btf_16_subs_adds_sse2(x[54], x[53]);
  btf_16_adds_subs_sse2(x[56], x[59]);
  btf_16_adds_subs_sse2(x[57], x[58]);
  btf_16_subs_adds_sse2(x[63], x[60]);
  btf_16_subs_adds_sse2(x[62], x[61]);
}

static INLINE void idct64_stage6_high32_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  const __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  const __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  const __m128i cospi_m56_m08 = pair_set_epi16(-cospi[56], -cospi[8]);
  const __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  const __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  const __m128i cospi_m24_m40 = pair_set_epi16(-cospi[24], -cospi[40]);
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x[34], x[61], x[34], x[61]);
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x[35], x[60], x[35], x[60]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x[36], x[59], x[36], x[59]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x[37], x[58], x[37], x[58]);
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x[42], x[53], x[42], x[53]);
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x[43], x[52], x[43], x[52]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x[44], x[51], x[44], x[51]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x[45], x[50], x[45], x[50]);
}

static INLINE void idct64_stage6_high48_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  btf_16_adds_subs_sse2(x[16], x[19]);
  btf_16_adds_subs_sse2(x[17], x[18]);
  btf_16_subs_adds_sse2(x[23], x[20]);
  btf_16_subs_adds_sse2(x[22], x[21]);
  btf_16_adds_subs_sse2(x[24], x[27]);
  btf_16_adds_subs_sse2(x[25], x[26]);
  btf_16_subs_adds_sse2(x[31], x[28]);
  btf_16_subs_adds_sse2(x[30], x[29]);
  idct64_stage6_high32_sse2(x, cospi, __rounding, cos_bit);
}

static INLINE void idct64_stage7_high48_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[18], x[29], x[18], x[29]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[19], x[28], x[19], x[28]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[20], x[27], x[20], x[27]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[21], x[26], x[21], x[26]);
  btf_16_adds_subs_sse2(x[32], x[39]);
  btf_16_adds_subs_sse2(x[33], x[38]);
  btf_16_adds_subs_sse2(x[34], x[37]);
  btf_16_adds_subs_sse2(x[35], x[36]);
  btf_16_subs_adds_sse2(x[47], x[40]);
  btf_16_subs_adds_sse2(x[46], x[41]);
  btf_16_subs_adds_sse2(x[45], x[42]);
  btf_16_subs_adds_sse2(x[44], x[43]);
  btf_16_adds_subs_sse2(x[48], x[55]);
  btf_16_adds_subs_sse2(x[49], x[54]);
  btf_16_adds_subs_sse2(x[50], x[53]);
  btf_16_adds_subs_sse2(x[51], x[52]);
  btf_16_subs_adds_sse2(x[63], x[56]);
  btf_16_subs_adds_sse2(x[62], x[57]);
  btf_16_subs_adds_sse2(x[61], x[58]);
  btf_16_subs_adds_sse2(x[60], x[59]);
}

static INLINE void idct64_stage8_high48_sse2(__m128i *x, const int32_t *cospi,
                                             const __m128i __rounding,
                                             int8_t cos_bit) {
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  btf_16_adds_subs_sse2(x[16], x[23]);
  btf_16_adds_subs_sse2(x[17], x[22]);
  btf_16_adds_subs_sse2(x[18], x[21]);
  btf_16_adds_subs_sse2(x[19], x[20]);
  btf_16_subs_adds_sse2(x[31], x[24]);
  btf_16_subs_adds_sse2(x[30], x[25]);
  btf_16_subs_adds_sse2(x[29], x[26]);
  btf_16_subs_adds_sse2(x[28], x[27]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[36], x[59], x[36], x[59]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[37], x[58], x[37], x[58]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[38], x[57], x[38], x[57]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[39], x[56], x[39], x[56]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[40], x[55], x[40], x[55]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[41], x[54], x[41], x[54]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[42], x[53], x[42], x[53]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[43], x[52], x[43], x[52]);
}

static INLINE void idct64_stage9_sse2(__m128i *x, const int32_t *cospi,
                                      const __m128i __rounding,
                                      int8_t cos_bit) {
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_sse2(x[0], x[15]);
  btf_16_adds_subs_sse2(x[1], x[14]);
  btf_16_adds_subs_sse2(x[2], x[13]);
  btf_16_adds_subs_sse2(x[3], x[12]);
  btf_16_adds_subs_sse2(x[4], x[11]);
  btf_16_adds_subs_sse2(x[5], x[10]);
  btf_16_adds_subs_sse2(x[6], x[9]);
  btf_16_adds_subs_sse2(x[7], x[8]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[20], x[27], x[20], x[27]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[21], x[26], x[21], x[26]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[22], x[25], x[22], x[25]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[23], x[24], x[23], x[24]);
  btf_16_adds_subs_sse2(x[32], x[47]);
  btf_16_adds_subs_sse2(x[33], x[46]);
  btf_16_adds_subs_sse2(x[34], x[45]);
  btf_16_adds_subs_sse2(x[35], x[44]);
  btf_16_adds_subs_sse2(x[36], x[43]);
  btf_16_adds_subs_sse2(x[37], x[42]);
  btf_16_adds_subs_sse2(x[38], x[41]);
  btf_16_adds_subs_sse2(x[39], x[40]);
  btf_16_subs_adds_sse2(x[63], x[48]);
  btf_16_subs_adds_sse2(x[62], x[49]);
  btf_16_subs_adds_sse2(x[61], x[50]);
  btf_16_subs_adds_sse2(x[60], x[51]);
  btf_16_subs_adds_sse2(x[59], x[52]);
  btf_16_subs_adds_sse2(x[58], x[53]);
  btf_16_subs_adds_sse2(x[57], x[54]);
  btf_16_subs_adds_sse2(x[56], x[55]);
}

static INLINE void idct64_stage10_sse2(__m128i *x, const int32_t *cospi,
                                       const __m128i __rounding,
                                       int8_t cos_bit) {
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_sse2(x[0], x[31]);
  btf_16_adds_subs_sse2(x[1], x[30]);
  btf_16_adds_subs_sse2(x[2], x[29]);
  btf_16_adds_subs_sse2(x[3], x[28]);
  btf_16_adds_subs_sse2(x[4], x[27]);
  btf_16_adds_subs_sse2(x[5], x[26]);
  btf_16_adds_subs_sse2(x[6], x[25]);
  btf_16_adds_subs_sse2(x[7], x[24]);
  btf_16_adds_subs_sse2(x[8], x[23]);
  btf_16_adds_subs_sse2(x[9], x[22]);
  btf_16_adds_subs_sse2(x[10], x[21]);
  btf_16_adds_subs_sse2(x[11], x[20]);
  btf_16_adds_subs_sse2(x[12], x[19]);
  btf_16_adds_subs_sse2(x[13], x[18]);
  btf_16_adds_subs_sse2(x[14], x[17]);
  btf_16_adds_subs_sse2(x[15], x[16]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[40], x[55], x[40], x[55]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[41], x[54], x[41], x[54]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[42], x[53], x[42], x[53]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[43], x[52], x[43], x[52]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[44], x[51], x[44], x[51]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[45], x[50], x[45], x[50]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[46], x[49], x[46], x[49]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[47], x[48], x[47], x[48]);
}

static INLINE void idct64_stage11_sse2(__m128i *output, __m128i *x) {
  btf_16_adds_subs_out_sse2(output[0], output[63], x[0], x[63]);
  btf_16_adds_subs_out_sse2(output[1], output[62], x[1], x[62]);
  btf_16_adds_subs_out_sse2(output[2], output[61], x[2], x[61]);
  btf_16_adds_subs_out_sse2(output[3], output[60], x[3], x[60]);
  btf_16_adds_subs_out_sse2(output[4], output[59], x[4], x[59]);
  btf_16_adds_subs_out_sse2(output[5], output[58], x[5], x[58]);
  btf_16_adds_subs_out_sse2(output[6], output[57], x[6], x[57]);
  btf_16_adds_subs_out_sse2(output[7], output[56], x[7], x[56]);
  btf_16_adds_subs_out_sse2(output[8], output[55], x[8], x[55]);
  btf_16_adds_subs_out_sse2(output[9], output[54], x[9], x[54]);
  btf_16_adds_subs_out_sse2(output[10], output[53], x[10], x[53]);
  btf_16_adds_subs_out_sse2(output[11], output[52], x[11], x[52]);
  btf_16_adds_subs_out_sse2(output[12], output[51], x[12], x[51]);
  btf_16_adds_subs_out_sse2(output[13], output[50], x[13], x[50]);
  btf_16_adds_subs_out_sse2(output[14], output[49], x[14], x[49]);
  btf_16_adds_subs_out_sse2(output[15], output[48], x[15], x[48]);
  btf_16_adds_subs_out_sse2(output[16], output[47], x[16], x[47]);
  btf_16_adds_subs_out_sse2(output[17], output[46], x[17], x[46]);
  btf_16_adds_subs_out_sse2(output[18], output[45], x[18], x[45]);
  btf_16_adds_subs_out_sse2(output[19], output[44], x[19], x[44]);
  btf_16_adds_subs_out_sse2(output[20], output[43], x[20], x[43]);
  btf_16_adds_subs_out_sse2(output[21], output[42], x[21], x[42]);
  btf_16_adds_subs_out_sse2(output[22], output[41], x[22], x[41]);
  btf_16_adds_subs_out_sse2(output[23], output[40], x[23], x[40]);
  btf_16_adds_subs_out_sse2(output[24], output[39], x[24], x[39]);
  btf_16_adds_subs_out_sse2(output[25], output[38], x[25], x[38]);
  btf_16_adds_subs_out_sse2(output[26], output[37], x[26], x[37]);
  btf_16_adds_subs_out_sse2(output[27], output[36], x[27], x[36]);
  btf_16_adds_subs_out_sse2(output[28], output[35], x[28], x[35]);
  btf_16_adds_subs_out_sse2(output[29], output[34], x[29], x[34]);
  btf_16_adds_subs_out_sse2(output[30], output[33], x[30], x[33]);
  btf_16_adds_subs_out_sse2(output[31], output[32], x[31], x[32]);
}

static void idct64_low1_ssse3(const __m128i *input, __m128i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);

  // stage 1
  __m128i x[32];
  x[0] = input[0];

  // stage 2
  // stage 3
  // stage 4
  // stage 5
  // stage 6
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);

  // stage 7
  // stage 8
  // stage 9
  // stage 10
  // stage 11
  output[0] = x[0];
  output[63] = x[0];
  output[1] = x[1];
  output[62] = x[1];
  output[2] = x[1];
  output[61] = x[1];
  output[3] = x[0];
  output[60] = x[0];
  output[4] = x[0];
  output[59] = x[0];
  output[5] = x[1];
  output[58] = x[1];
  output[6] = x[1];
  output[57] = x[1];
  output[7] = x[0];
  output[56] = x[0];
  output[8] = x[0];
  output[55] = x[0];
  output[9] = x[1];
  output[54] = x[1];
  output[10] = x[1];
  output[53] = x[1];
  output[11] = x[0];
  output[52] = x[0];
  output[12] = x[0];
  output[51] = x[0];
  output[13] = x[1];
  output[50] = x[1];
  output[14] = x[1];
  output[49] = x[1];
  output[15] = x[0];
  output[48] = x[0];
  output[16] = x[0];
  output[47] = x[0];
  output[17] = x[1];
  output[46] = x[1];
  output[18] = x[1];
  output[45] = x[1];
  output[19] = x[0];
  output[44] = x[0];
  output[20] = x[0];
  output[43] = x[0];
  output[21] = x[1];
  output[42] = x[1];
  output[22] = x[1];
  output[41] = x[1];
  output[23] = x[0];
  output[40] = x[0];
  output[24] = x[0];
  output[39] = x[0];
  output[25] = x[1];
  output[38] = x[1];
  output[26] = x[1];
  output[37] = x[1];
  output[27] = x[0];
  output[36] = x[0];
  output[28] = x[0];
  output[35] = x[0];
  output[29] = x[1];
  output[34] = x[1];
  output[30] = x[1];
  output[33] = x[1];
  output[31] = x[0];
  output[32] = x[0];
}

static void idct64_low8_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m128i cospi_m04_p60 = pair_set_epi16(-cospi[4], cospi[60]);
  const __m128i cospi_p60_p04 = pair_set_epi16(cospi[60], cospi[4]);
  const __m128i cospi_m36_p28 = pair_set_epi16(-cospi[36], cospi[28]);
  const __m128i cospi_m28_m36 = pair_set_epi16(-cospi[28], -cospi[36]);
  const __m128i cospi_m20_p44 = pair_set_epi16(-cospi[20], cospi[44]);
  const __m128i cospi_p44_p20 = pair_set_epi16(cospi[44], cospi[20]);
  const __m128i cospi_m52_p12 = pair_set_epi16(-cospi[52], cospi[12]);
  const __m128i cospi_m12_m52 = pair_set_epi16(-cospi[12], -cospi[52]);
  const __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  const __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  const __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  const __m128i cospi_m24_m40 = pair_set_epi16(-cospi[24], -cospi[40]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x[64];
  x[0] = input[0];
  x[8] = input[4];
  x[16] = input[2];
  x[24] = input[6];
  x[32] = input[1];
  x[40] = input[5];
  x[48] = input[3];
  x[56] = input[7];

  // stage 2
  btf_16_ssse3(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_ssse3(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_ssse3(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_ssse3(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_ssse3(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_ssse3(-cospi[58], cospi[6], x[24], x[23], x[24]);
  x[33] = x[32];
  x[38] = x[39];
  x[41] = x[40];
  x[46] = x[47];
  x[49] = x[48];
  x[54] = x[55];
  x[57] = x[56];
  x[62] = x[63];

  // stage 4
  btf_16_ssse3(cospi[60], cospi[4], x[8], x[8], x[15]);
  x[17] = x[16];
  x[22] = x[23];
  x[25] = x[24];
  x[30] = x[31];
  btf_16_sse2(cospi_m04_p60, cospi_p60_p04, x[33], x[62], x[33], x[62]);
  btf_16_sse2(cospi_m28_m36, cospi_m36_p28, x[38], x[57], x[38], x[57]);
  btf_16_sse2(cospi_m20_p44, cospi_p44_p20, x[41], x[54], x[41], x[54]);
  btf_16_sse2(cospi_m12_m52, cospi_m52_p12, x[46], x[49], x[46], x[49]);

  // stage 5
  x[9] = x[8];
  x[14] = x[15];
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x[17], x[30], x[17], x[30]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x[22], x[25], x[22], x[25]);
  x[35] = x[32];
  x[34] = x[33];
  x[36] = x[39];
  x[37] = x[38];
  x[43] = x[40];
  x[42] = x[41];
  x[44] = x[47];
  x[45] = x[46];
  x[51] = x[48];
  x[50] = x[49];
  x[52] = x[55];
  x[53] = x[54];
  x[59] = x[56];
  x[58] = x[57];
  x[60] = x[63];
  x[61] = x[62];

  // stage 6
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[9], x[14], x[9], x[14]);
  x[19] = x[16];
  x[18] = x[17];
  x[20] = x[23];
  x[21] = x[22];
  x[27] = x[24];
  x[26] = x[25];
  x[28] = x[31];
  x[29] = x[30];
  idct64_stage6_high32_sse2(x, cospi, __rounding, cos_bit);

  // stage 7
  x[3] = x[0];
  x[2] = x[1];
  x[11] = x[8];
  x[10] = x[9];
  x[12] = x[15];
  x[13] = x[14];
  idct64_stage7_high48_sse2(x, cospi, __rounding, cos_bit);

  // stage 8
  x[7] = x[0];
  x[6] = x[1];
  x[5] = x[2];
  x[4] = x[3];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[10], x[13], x[10], x[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[11], x[12], x[11], x[12]);
  idct64_stage8_high48_sse2(x, cospi, __rounding, cos_bit);

  idct64_stage9_sse2(x, cospi, __rounding, cos_bit);
  idct64_stage10_sse2(x, cospi, __rounding, cos_bit);
  idct64_stage11_sse2(output, x);
}

static void idct64_low16_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x[64];
  x[0] = input[0];
  x[4] = input[8];
  x[8] = input[4];
  x[12] = input[12];
  x[16] = input[2];
  x[20] = input[10];
  x[24] = input[6];
  x[28] = input[14];
  x[32] = input[1];
  x[36] = input[9];
  x[40] = input[5];
  x[44] = input[13];
  x[48] = input[3];
  x[52] = input[11];
  x[56] = input[7];
  x[60] = input[15];

  // stage 2
  btf_16_ssse3(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_ssse3(-cospi[49], cospi[15], x[60], x[35], x[60]);
  btf_16_ssse3(cospi[55], cospi[9], x[36], x[36], x[59]);
  btf_16_ssse3(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_ssse3(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_ssse3(-cospi[53], cospi[11], x[52], x[43], x[52]);
  btf_16_ssse3(cospi[51], cospi[13], x[44], x[44], x[51]);
  btf_16_ssse3(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_ssse3(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_ssse3(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_ssse3(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_ssse3(-cospi[58], cospi[6], x[24], x[23], x[24]);
  x[33] = x[32];
  x[34] = x[35];
  x[37] = x[36];
  x[38] = x[39];
  x[41] = x[40];
  x[42] = x[43];
  x[45] = x[44];
  x[46] = x[47];
  x[49] = x[48];
  x[50] = x[51];
  x[53] = x[52];
  x[54] = x[55];
  x[57] = x[56];
  x[58] = x[59];
  x[61] = x[60];
  x[62] = x[63];

  // stage 4
  btf_16_ssse3(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_ssse3(-cospi[52], cospi[12], x[12], x[11], x[12]);
  x[17] = x[16];
  x[18] = x[19];
  x[21] = x[20];
  x[22] = x[23];
  x[25] = x[24];
  x[26] = x[27];
  x[29] = x[28];
  x[30] = x[31];
  idct64_stage4_high32_sse2(x, cospi, __rounding, cos_bit);

  // stage 5
  btf_16_ssse3(cospi[56], cospi[8], x[4], x[4], x[7]);
  x[9] = x[8];
  x[10] = x[11];
  x[13] = x[12];
  x[14] = x[15];
  idct64_stage5_high48_sse2(x, cospi, __rounding, cos_bit);

  // stage 6
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);
  x[5] = x[4];
  x[6] = x[7];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[9], x[14], x[9], x[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[10], x[13], x[10], x[13]);
  idct64_stage6_high48_sse2(x, cospi, __rounding, cos_bit);

  // stage 7
  x[3] = x[0];
  x[2] = x[1];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[5], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[11]);
  btf_16_adds_subs_sse2(x[9], x[10]);
  btf_16_subs_adds_sse2(x[15], x[12]);
  btf_16_subs_adds_sse2(x[14], x[13]);
  idct64_stage7_high48_sse2(x, cospi, __rounding, cos_bit);

  // stage 8
  btf_16_adds_subs_sse2(x[0], x[7]);
  btf_16_adds_subs_sse2(x[1], x[6]);
  btf_16_adds_subs_sse2(x[2], x[5]);
  btf_16_adds_subs_sse2(x[3], x[4]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[10], x[13], x[10], x[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[11], x[12], x[11], x[12]);
  idct64_stage8_high48_sse2(x, cospi, __rounding, cos_bit);

  idct64_stage9_sse2(x, cospi, __rounding, cos_bit);
  idct64_stage10_sse2(x, cospi, __rounding, cos_bit);
  idct64_stage11_sse2(output, x);
}

static void idct64_low32_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  const __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  const __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  const __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x[64];
  x[0] = input[0];
  x[2] = input[16];
  x[4] = input[8];
  x[6] = input[24];
  x[8] = input[4];
  x[10] = input[20];
  x[12] = input[12];
  x[14] = input[28];
  x[16] = input[2];
  x[18] = input[18];
  x[20] = input[10];
  x[22] = input[26];
  x[24] = input[6];
  x[26] = input[22];
  x[28] = input[14];
  x[30] = input[30];
  x[32] = input[1];
  x[34] = input[17];
  x[36] = input[9];
  x[38] = input[25];
  x[40] = input[5];
  x[42] = input[21];
  x[44] = input[13];
  x[46] = input[29];
  x[48] = input[3];
  x[50] = input[19];
  x[52] = input[11];
  x[54] = input[27];
  x[56] = input[7];
  x[58] = input[23];
  x[60] = input[15];
  x[62] = input[31];

  // stage 2
  btf_16_ssse3(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_ssse3(-cospi[33], cospi[31], x[62], x[33], x[62]);
  btf_16_ssse3(cospi[47], cospi[17], x[34], x[34], x[61]);
  btf_16_ssse3(-cospi[49], cospi[15], x[60], x[35], x[60]);
  btf_16_ssse3(cospi[55], cospi[9], x[36], x[36], x[59]);
  btf_16_ssse3(-cospi[41], cospi[23], x[58], x[37], x[58]);
  btf_16_ssse3(cospi[39], cospi[25], x[38], x[38], x[57]);
  btf_16_ssse3(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_ssse3(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_ssse3(-cospi[37], cospi[27], x[54], x[41], x[54]);
  btf_16_ssse3(cospi[43], cospi[21], x[42], x[42], x[53]);
  btf_16_ssse3(-cospi[53], cospi[11], x[52], x[43], x[52]);
  btf_16_ssse3(cospi[51], cospi[13], x[44], x[44], x[51]);
  btf_16_ssse3(-cospi[45], cospi[19], x[50], x[45], x[50]);
  btf_16_ssse3(cospi[35], cospi[29], x[46], x[46], x[49]);
  btf_16_ssse3(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_ssse3(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_ssse3(-cospi[34], cospi[30], x[30], x[17], x[30]);
  btf_16_ssse3(cospi[46], cospi[18], x[18], x[18], x[29]);
  btf_16_ssse3(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_ssse3(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_ssse3(-cospi[42], cospi[22], x[26], x[21], x[26]);
  btf_16_ssse3(cospi[38], cospi[26], x[22], x[22], x[25]);
  btf_16_ssse3(-cospi[58], cospi[6], x[24], x[23], x[24]);
  btf_16_adds_subs_sse2(x[32], x[33]);
  btf_16_subs_adds_sse2(x[35], x[34]);
  btf_16_adds_subs_sse2(x[36], x[37]);
  btf_16_subs_adds_sse2(x[39], x[38]);
  btf_16_adds_subs_sse2(x[40], x[41]);
  btf_16_subs_adds_sse2(x[43], x[42]);
  btf_16_adds_subs_sse2(x[44], x[45]);
  btf_16_subs_adds_sse2(x[47], x[46]);
  btf_16_adds_subs_sse2(x[48], x[49]);
  btf_16_subs_adds_sse2(x[51], x[50]);
  btf_16_adds_subs_sse2(x[52], x[53]);
  btf_16_subs_adds_sse2(x[55], x[54]);
  btf_16_adds_subs_sse2(x[56], x[57]);
  btf_16_subs_adds_sse2(x[59], x[58]);
  btf_16_adds_subs_sse2(x[60], x[61]);
  btf_16_subs_adds_sse2(x[63], x[62]);

  // stage 4
  btf_16_ssse3(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_ssse3(-cospi[36], cospi[28], x[14], x[9], x[14]);
  btf_16_ssse3(cospi[44], cospi[20], x[10], x[10], x[13]);
  btf_16_ssse3(-cospi[52], cospi[12], x[12], x[11], x[12]);
  btf_16_adds_subs_sse2(x[16], x[17]);
  btf_16_subs_adds_sse2(x[19], x[18]);
  btf_16_adds_subs_sse2(x[20], x[21]);
  btf_16_subs_adds_sse2(x[23], x[22]);
  btf_16_adds_subs_sse2(x[24], x[25]);
  btf_16_subs_adds_sse2(x[27], x[26]);
  btf_16_adds_subs_sse2(x[28], x[29]);
  btf_16_subs_adds_sse2(x[31], x[30]);
  idct64_stage4_high32_sse2(x, cospi, __rounding, cos_bit);

  // stage 5
  btf_16_ssse3(cospi[56], cospi[8], x[4], x[4], x[7]);
  btf_16_ssse3(-cospi[40], cospi[24], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[9]);
  btf_16_subs_adds_sse2(x[11], x[10]);
  btf_16_adds_subs_sse2(x[12], x[13]);
  btf_16_subs_adds_sse2(x[15], x[14]);
  idct64_stage5_high48_sse2(x, cospi, __rounding, cos_bit);

  // stage 6
  btf_16_ssse3(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_ssse3(cospi[48], cospi[16], x[2], x[2], x[3]);
  btf_16_adds_subs_sse2(x[4], x[5]);
  btf_16_subs_adds_sse2(x[7], x[6]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x[9], x[14], x[9], x[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x[10], x[13], x[10], x[13]);
  idct64_stage6_high48_sse2(x, cospi, __rounding, cos_bit);

  // stage 7
  btf_16_adds_subs_sse2(x[0], x[3]);
  btf_16_adds_subs_sse2(x[1], x[2]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[5], x[6], x[5], x[6]);
  btf_16_adds_subs_sse2(x[8], x[11]);
  btf_16_adds_subs_sse2(x[9], x[10]);
  btf_16_subs_adds_sse2(x[15], x[12]);
  btf_16_subs_adds_sse2(x[14], x[13]);
  idct64_stage7_high48_sse2(x, cospi, __rounding, cos_bit);

  // stage 8
  btf_16_adds_subs_sse2(x[0], x[7]);
  btf_16_adds_subs_sse2(x[1], x[6]);
  btf_16_adds_subs_sse2(x[2], x[5]);
  btf_16_adds_subs_sse2(x[3], x[4]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[10], x[13], x[10], x[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x[11], x[12], x[11], x[12]);
  idct64_stage8_high48_sse2(x, cospi, __rounding, cos_bit);

  // stage 9~11
  idct64_stage9_sse2(x, cospi, __rounding, cos_bit);
  idct64_stage10_sse2(x, cospi, __rounding, cos_bit);
  idct64_stage11_sse2(output, x);
}

static void iadst4_sse2(const __m128i *input, __m128i *output) {
  const int32_t *sinpi = sinpi_arr(INV_COS_BIT);
  const __m128i sinpi_p01_p04 = pair_set_epi16(sinpi[1], sinpi[4]);
  const __m128i sinpi_p02_m01 = pair_set_epi16(sinpi[2], -sinpi[1]);
  const __m128i sinpi_p03_p02 = pair_set_epi16(sinpi[3], sinpi[2]);
  const __m128i sinpi_p03_m04 = pair_set_epi16(sinpi[3], -sinpi[4]);
  const __m128i sinpi_p03_m03 = pair_set_epi16(sinpi[3], -sinpi[3]);
  const __m128i sinpi_0_p03 = pair_set_epi16(0, sinpi[3]);
  const __m128i sinpi_p04_p02 = pair_set_epi16(sinpi[4], sinpi[2]);
  const __m128i sinpi_m03_m01 = pair_set_epi16(-sinpi[3], -sinpi[1]);
  __m128i x0[4];
  x0[0] = input[0];
  x0[1] = input[1];
  x0[2] = input[2];
  x0[3] = input[3];

  __m128i u[4];
  u[0] = _mm_unpacklo_epi16(x0[0], x0[2]);
  u[1] = _mm_unpackhi_epi16(x0[0], x0[2]);
  u[2] = _mm_unpacklo_epi16(x0[1], x0[3]);
  u[3] = _mm_unpackhi_epi16(x0[1], x0[3]);

  __m128i x1[16];
  x1[0] = _mm_madd_epi16(u[0], sinpi_p01_p04);  // x0*sin1 + x2*sin4
  x1[1] = _mm_madd_epi16(u[1], sinpi_p01_p04);
  x1[2] = _mm_madd_epi16(u[0], sinpi_p02_m01);  // x0*sin2 - x2*sin1
  x1[3] = _mm_madd_epi16(u[1], sinpi_p02_m01);
  x1[4] = _mm_madd_epi16(u[2], sinpi_p03_p02);  // x1*sin3 + x3*sin2
  x1[5] = _mm_madd_epi16(u[3], sinpi_p03_p02);
  x1[6] = _mm_madd_epi16(u[2], sinpi_p03_m04);  // x1*sin3 - x3*sin4
  x1[7] = _mm_madd_epi16(u[3], sinpi_p03_m04);
  x1[8] = _mm_madd_epi16(u[0], sinpi_p03_m03);  // x0*sin3 - x2*sin3
  x1[9] = _mm_madd_epi16(u[1], sinpi_p03_m03);
  x1[10] = _mm_madd_epi16(u[2], sinpi_0_p03);  // x2*sin3
  x1[11] = _mm_madd_epi16(u[3], sinpi_0_p03);
  x1[12] = _mm_madd_epi16(u[0], sinpi_p04_p02);  // x0*sin4 + x2*sin2
  x1[13] = _mm_madd_epi16(u[1], sinpi_p04_p02);
  x1[14] = _mm_madd_epi16(u[2], sinpi_m03_m01);  // -x1*sin3 - x3*sin1
  x1[15] = _mm_madd_epi16(u[3], sinpi_m03_m01);

  __m128i x2[8];
  x2[0] = _mm_add_epi32(x1[0], x1[4]);  // x0*sin1 +x2*sin4 +x1*sin3 +x3*sin2
  x2[1] = _mm_add_epi32(x1[1], x1[5]);
  x2[2] = _mm_add_epi32(x1[2], x1[6]);  // x0*sin2 -x2*sin1 +x1*sin3 -x3*sin4
  x2[3] = _mm_add_epi32(x1[3], x1[7]);
  x2[4] = _mm_add_epi32(x1[8], x1[10]);  // x0*sin3 -x2*sin3 +x3*sin3
  x2[5] = _mm_add_epi32(x1[9], x1[11]);
  x2[6] = _mm_add_epi32(x1[12], x1[14]);  // x0*sin1 +x2*sin4 +x0*sin2 -x2*sin1
  x2[7] = _mm_add_epi32(x1[13], x1[15]);

  const __m128i rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  for (int i = 0; i < 4; ++i) {
    __m128i out0 = _mm_add_epi32(x2[2 * i], rounding);
    __m128i out1 = _mm_add_epi32(x2[2 * i + 1], rounding);
    out0 = _mm_srai_epi32(out0, INV_COS_BIT);
    out1 = _mm_srai_epi32(out1, INV_COS_BIT);
    output[i] = _mm_packs_epi32(out0, out1);
  }
}

static void iadst4_w4_sse2(const __m128i *input, __m128i *output) {
  const int32_t *sinpi = sinpi_arr(INV_COS_BIT);
  const __m128i sinpi_p01_p04 = pair_set_epi16(sinpi[1], sinpi[4]);
  const __m128i sinpi_p02_m01 = pair_set_epi16(sinpi[2], -sinpi[1]);
  const __m128i sinpi_p03_p02 = pair_set_epi16(sinpi[3], sinpi[2]);
  const __m128i sinpi_p03_m04 = pair_set_epi16(sinpi[3], -sinpi[4]);
  const __m128i sinpi_p03_m03 = pair_set_epi16(sinpi[3], -sinpi[3]);
  const __m128i sinpi_0_p03 = pair_set_epi16(0, sinpi[3]);
  const __m128i sinpi_p04_p02 = pair_set_epi16(sinpi[4], sinpi[2]);
  const __m128i sinpi_m03_m01 = pair_set_epi16(-sinpi[3], -sinpi[1]);
  __m128i x0[4];
  x0[0] = input[0];
  x0[1] = input[1];
  x0[2] = input[2];
  x0[3] = input[3];

  __m128i u[2];
  u[0] = _mm_unpacklo_epi16(x0[0], x0[2]);
  u[1] = _mm_unpacklo_epi16(x0[1], x0[3]);

  __m128i x1[8];
  x1[0] = _mm_madd_epi16(u[0], sinpi_p01_p04);  // x0*sin1 + x2*sin4
  x1[1] = _mm_madd_epi16(u[0], sinpi_p02_m01);  // x0*sin2 - x2*sin1
  x1[2] = _mm_madd_epi16(u[1], sinpi_p03_p02);  // x1*sin3 + x3*sin2
  x1[3] = _mm_madd_epi16(u[1], sinpi_p03_m04);  // x1*sin3 - x3*sin4
  x1[4] = _mm_madd_epi16(u[0], sinpi_p03_m03);  // x0*sin3 - x2*sin3
  x1[5] = _mm_madd_epi16(u[1], sinpi_0_p03);    // x2*sin3
  x1[6] = _mm_madd_epi16(u[0], sinpi_p04_p02);  // x0*sin4 + x2*sin2
  x1[7] = _mm_madd_epi16(u[1], sinpi_m03_m01);  // -x1*sin3 - x3*sin1

  __m128i x2[4];
  x2[0] = _mm_add_epi32(x1[0], x1[2]);  // x0*sin1 + x2*sin4 + x1*sin3 + x3*sin2
  x2[1] = _mm_add_epi32(x1[1], x1[3]);  // x0*sin2 - x2*sin1 + x1*sin3 - x3*sin4
  x2[2] = _mm_add_epi32(x1[4], x1[5]);  // x0*sin3 - x2*sin3 + x3*sin3
  x2[3] = _mm_add_epi32(x1[6], x1[7]);  // x0*sin4 + x2*sin2 - x1*sin3 - x3*sin1

  const __m128i rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  for (int i = 0; i < 4; ++i) {
    __m128i out0 = _mm_add_epi32(x2[i], rounding);
    out0 = _mm_srai_epi32(out0, INV_COS_BIT);
    output[i] = _mm_packs_epi32(out0, out0);
  }
}

static void iadst8_low1_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __zero = _mm_setzero_si128();
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);

  // stage 1
  __m128i x[8];
  x[1] = input[0];

  // stage 2
  btf_16_ssse3(cospi[60], -cospi[4], x[1], x[0], x[1]);

  // stage 3
  x[4] = x[0];
  x[5] = x[1];

  // stage 4
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x[4], x[5], x[4], x[5]);

  // stage 5
  x[2] = x[0];
  x[3] = x[1];
  x[6] = x[4];
  x[7] = x[5];

  // stage 6
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[2], x[3], x[2], x[3]);
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[6], x[7], x[6], x[7]);

  // stage 7
  output[0] = x[0];
  output[1] = _mm_subs_epi16(__zero, x[4]);
  output[2] = x[6];
  output[3] = _mm_subs_epi16(__zero, x[2]);
  output[4] = x[3];
  output[5] = _mm_subs_epi16(__zero, x[7]);
  output[6] = x[5];
  output[7] = _mm_subs_epi16(__zero, x[1]);
}

static void iadst8_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __zero = _mm_setzero_si128();
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  const __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  const __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  const __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  const __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  const __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  const __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  const __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);

  // stage 1
  __m128i x[8];
  x[0] = input[7];
  x[1] = input[0];
  x[2] = input[5];
  x[3] = input[2];
  x[4] = input[3];
  x[5] = input[4];
  x[6] = input[1];
  x[7] = input[6];

  // stage 2
  btf_16_sse2(cospi_p04_p60, cospi_p60_m04, x[0], x[1], x[0], x[1]);
  btf_16_sse2(cospi_p20_p44, cospi_p44_m20, x[2], x[3], x[2], x[3]);
  btf_16_sse2(cospi_p36_p28, cospi_p28_m36, x[4], x[5], x[4], x[5]);
  btf_16_sse2(cospi_p52_p12, cospi_p12_m52, x[6], x[7], x[6], x[7]);

  // stage 3
  btf_16_adds_subs_sse2(x[0], x[4]);
  btf_16_adds_subs_sse2(x[1], x[5]);
  btf_16_adds_subs_sse2(x[2], x[6]);
  btf_16_adds_subs_sse2(x[3], x[7]);

  // stage 4
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x[4], x[5], x[4], x[5]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x[6], x[7], x[6], x[7]);

  // stage 5
  btf_16_adds_subs_sse2(x[0], x[2]);
  btf_16_adds_subs_sse2(x[1], x[3]);
  btf_16_adds_subs_sse2(x[4], x[6]);
  btf_16_adds_subs_sse2(x[5], x[7]);

  // stage 6
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[2], x[3], x[2], x[3]);
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[6], x[7], x[6], x[7]);

  // stage 7
  output[0] = x[0];
  output[1] = _mm_subs_epi16(__zero, x[4]);
  output[2] = x[6];
  output[3] = _mm_subs_epi16(__zero, x[2]);
  output[4] = x[3];
  output[5] = _mm_subs_epi16(__zero, x[7]);
  output[6] = x[5];
  output[7] = _mm_subs_epi16(__zero, x[1]);
}

static void iadst8_w4_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __zero = _mm_setzero_si128();
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  const __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  const __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  const __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  const __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  const __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  const __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  const __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);

  // stage 1
  __m128i x[8];
  x[0] = input[7];
  x[1] = input[0];
  x[2] = input[5];
  x[3] = input[2];
  x[4] = input[3];
  x[5] = input[4];
  x[6] = input[1];
  x[7] = input[6];

  // stage 2
  btf_16_4p_sse2(cospi_p04_p60, cospi_p60_m04, x[0], x[1], x[0], x[1]);
  btf_16_4p_sse2(cospi_p20_p44, cospi_p44_m20, x[2], x[3], x[2], x[3]);
  btf_16_4p_sse2(cospi_p36_p28, cospi_p28_m36, x[4], x[5], x[4], x[5]);
  btf_16_4p_sse2(cospi_p52_p12, cospi_p12_m52, x[6], x[7], x[6], x[7]);

  // stage 3
  btf_16_adds_subs_sse2(x[0], x[4]);
  btf_16_adds_subs_sse2(x[1], x[5]);
  btf_16_adds_subs_sse2(x[2], x[6]);
  btf_16_adds_subs_sse2(x[3], x[7]);

  // stage 4
  btf_16_4p_sse2(cospi_p16_p48, cospi_p48_m16, x[4], x[5], x[4], x[5]);
  btf_16_4p_sse2(cospi_m48_p16, cospi_p16_p48, x[6], x[7], x[6], x[7]);

  // stage 5
  btf_16_adds_subs_sse2(x[0], x[2]);
  btf_16_adds_subs_sse2(x[1], x[3]);
  btf_16_adds_subs_sse2(x[4], x[6]);
  btf_16_adds_subs_sse2(x[5], x[7]);

  // stage 6
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[2], x[3], x[2], x[3]);
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[6], x[7], x[6], x[7]);

  // stage 7
  output[0] = x[0];
  output[1] = _mm_subs_epi16(__zero, x[4]);
  output[2] = x[6];
  output[3] = _mm_subs_epi16(__zero, x[2]);
  output[4] = x[3];
  output[5] = _mm_subs_epi16(__zero, x[7]);
  output[6] = x[5];
  output[7] = _mm_subs_epi16(__zero, x[1]);
}

static INLINE void iadst16_stage3_ssse3(__m128i *x) {
  btf_16_adds_subs_sse2(x[0], x[8]);
  btf_16_adds_subs_sse2(x[1], x[9]);
  btf_16_adds_subs_sse2(x[2], x[10]);
  btf_16_adds_subs_sse2(x[3], x[11]);
  btf_16_adds_subs_sse2(x[4], x[12]);
  btf_16_adds_subs_sse2(x[5], x[13]);
  btf_16_adds_subs_sse2(x[6], x[14]);
  btf_16_adds_subs_sse2(x[7], x[15]);
}

static INLINE void iadst16_stage4_ssse3(__m128i *x, const int32_t *cospi,
                                        const __m128i __rounding,
                                        int8_t cos_bit) {
  const __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  const __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  const __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  const __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  const __m128i cospi_m56_p08 = pair_set_epi16(-cospi[56], cospi[8]);
  const __m128i cospi_m24_p40 = pair_set_epi16(-cospi[24], cospi[40]);
  btf_16_sse2(cospi_p08_p56, cospi_p56_m08, x[8], x[9], x[8], x[9]);
  btf_16_sse2(cospi_p40_p24, cospi_p24_m40, x[10], x[11], x[10], x[11]);
  btf_16_sse2(cospi_m56_p08, cospi_p08_p56, x[12], x[13], x[12], x[13]);
  btf_16_sse2(cospi_m24_p40, cospi_p40_p24, x[14], x[15], x[14], x[15]);
}

static INLINE void iadst16_stage5_ssse3(__m128i *x) {
  btf_16_adds_subs_sse2(x[0], x[4]);
  btf_16_adds_subs_sse2(x[1], x[5]);
  btf_16_adds_subs_sse2(x[2], x[6]);
  btf_16_adds_subs_sse2(x[3], x[7]);
  btf_16_adds_subs_sse2(x[8], x[12]);
  btf_16_adds_subs_sse2(x[9], x[13]);
  btf_16_adds_subs_sse2(x[10], x[14]);
  btf_16_adds_subs_sse2(x[11], x[15]);
}

static INLINE void iadst16_stage6_ssse3(__m128i *x, const int32_t *cospi,
                                        const __m128i __rounding,
                                        int8_t cos_bit) {
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x[4], x[5], x[4], x[5]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x[6], x[7], x[6], x[7]);
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x[12], x[13], x[12], x[13]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x[14], x[15], x[14], x[15]);
}

static INLINE void iadst16_stage7_ssse3(__m128i *x) {
  btf_16_adds_subs_sse2(x[0], x[2]);
  btf_16_adds_subs_sse2(x[1], x[3]);
  btf_16_adds_subs_sse2(x[4], x[6]);
  btf_16_adds_subs_sse2(x[5], x[7]);
  btf_16_adds_subs_sse2(x[8], x[10]);
  btf_16_adds_subs_sse2(x[9], x[11]);
  btf_16_adds_subs_sse2(x[12], x[14]);
  btf_16_adds_subs_sse2(x[13], x[15]);
}

static INLINE void iadst16_stage8_ssse3(__m128i *x, const int32_t *cospi,
                                        const __m128i __rounding,
                                        int8_t cos_bit) {
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[2], x[3], x[2], x[3]);
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[6], x[7], x[6], x[7]);
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[10], x[11], x[10], x[11]);
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x[14], x[15], x[14], x[15]);
}

static INLINE void iadst16_stage9_ssse3(__m128i *output, __m128i *x) {
  const __m128i __zero = _mm_setzero_si128();
  output[0] = x[0];
  output[1] = _mm_subs_epi16(__zero, x[8]);
  output[2] = x[12];
  output[3] = _mm_subs_epi16(__zero, x[4]);
  output[4] = x[6];
  output[5] = _mm_subs_epi16(__zero, x[14]);
  output[6] = x[10];
  output[7] = _mm_subs_epi16(__zero, x[2]);
  output[8] = x[3];
  output[9] = _mm_subs_epi16(__zero, x[11]);
  output[10] = x[15];
  output[11] = _mm_subs_epi16(__zero, x[7]);
  output[12] = x[5];
  output[13] = _mm_subs_epi16(__zero, x[13]);
  output[14] = x[9];
  output[15] = _mm_subs_epi16(__zero, x[1]);
}

static void iadst16_low1_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  const __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);

  // stage 1
  __m128i x[16];
  x[1] = input[0];

  // stage 2
  btf_16_ssse3(cospi[62], -cospi[2], x[1], x[0], x[1]);

  // stage 3
  x[8] = x[0];
  x[9] = x[1];

  // stage 4
  btf_16_sse2(cospi_p08_p56, cospi_p56_m08, x[8], x[9], x[8], x[9]);

  // stage 5
  x[4] = x[0];
  x[5] = x[1];
  x[12] = x[8];
  x[13] = x[9];

  // stage 6
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x[4], x[5], x[4], x[5]);
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x[12], x[13], x[12], x[13]);

  // stage 7
  x[2] = x[0];
  x[3] = x[1];
  x[6] = x[4];
  x[7] = x[5];
  x[10] = x[8];
  x[11] = x[9];
  x[14] = x[12];
  x[15] = x[13];

  iadst16_stage8_ssse3(x, cospi, __rounding, cos_bit);
  iadst16_stage9_ssse3(output, x);
}

static void iadst16_low8_ssse3(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  // stage 1
  __m128i x[16];
  x[1] = input[0];
  x[3] = input[2];
  x[5] = input[4];
  x[7] = input[6];
  x[8] = input[7];
  x[10] = input[5];
  x[12] = input[3];
  x[14] = input[1];

  // stage 2
  btf_16_ssse3(cospi[62], -cospi[2], x[1], x[0], x[1]);
  btf_16_ssse3(cospi[54], -cospi[10], x[3], x[2], x[3]);
  btf_16_ssse3(cospi[46], -cospi[18], x[5], x[4], x[5]);
  btf_16_ssse3(cospi[38], -cospi[26], x[7], x[6], x[7]);
  btf_16_ssse3(cospi[34], cospi[30], x[8], x[8], x[9]);
  btf_16_ssse3(cospi[42], cospi[22], x[10], x[10], x[11]);
  btf_16_ssse3(cospi[50], cospi[14], x[12], x[12], x[13]);
  btf_16_ssse3(cospi[58], cospi[6], x[14], x[14], x[15]);

  // stage 3
  iadst16_stage3_ssse3(x);
  iadst16_stage4_ssse3(x, cospi, __rounding, cos_bit);
  iadst16_stage5_ssse3(x);
  iadst16_stage6_ssse3(x, cospi, __rounding, cos_bit);
  iadst16_stage7_ssse3(x);
  iadst16_stage8_ssse3(x, cospi, __rounding, cos_bit);
  iadst16_stage9_ssse3(output, x);
}
static void iadst16_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m128i cospi_p02_p62 = pair_set_epi16(cospi[2], cospi[62]);
  const __m128i cospi_p62_m02 = pair_set_epi16(cospi[62], -cospi[2]);
  const __m128i cospi_p10_p54 = pair_set_epi16(cospi[10], cospi[54]);
  const __m128i cospi_p54_m10 = pair_set_epi16(cospi[54], -cospi[10]);
  const __m128i cospi_p18_p46 = pair_set_epi16(cospi[18], cospi[46]);
  const __m128i cospi_p46_m18 = pair_set_epi16(cospi[46], -cospi[18]);
  const __m128i cospi_p26_p38 = pair_set_epi16(cospi[26], cospi[38]);
  const __m128i cospi_p38_m26 = pair_set_epi16(cospi[38], -cospi[26]);
  const __m128i cospi_p34_p30 = pair_set_epi16(cospi[34], cospi[30]);
  const __m128i cospi_p30_m34 = pair_set_epi16(cospi[30], -cospi[34]);
  const __m128i cospi_p42_p22 = pair_set_epi16(cospi[42], cospi[22]);
  const __m128i cospi_p22_m42 = pair_set_epi16(cospi[22], -cospi[42]);
  const __m128i cospi_p50_p14 = pair_set_epi16(cospi[50], cospi[14]);
  const __m128i cospi_p14_m50 = pair_set_epi16(cospi[14], -cospi[50]);
  const __m128i cospi_p58_p06 = pair_set_epi16(cospi[58], cospi[6]);
  const __m128i cospi_p06_m58 = pair_set_epi16(cospi[6], -cospi[58]);

  // stage 1
  __m128i x[16];
  x[0] = input[15];
  x[1] = input[0];
  x[2] = input[13];
  x[3] = input[2];
  x[4] = input[11];
  x[5] = input[4];
  x[6] = input[9];
  x[7] = input[6];
  x[8] = input[7];
  x[9] = input[8];
  x[10] = input[5];
  x[11] = input[10];
  x[12] = input[3];
  x[13] = input[12];
  x[14] = input[1];
  x[15] = input[14];

  // stage 2
  btf_16_sse2(cospi_p02_p62, cospi_p62_m02, x[0], x[1], x[0], x[1]);
  btf_16_sse2(cospi_p10_p54, cospi_p54_m10, x[2], x[3], x[2], x[3]);
  btf_16_sse2(cospi_p18_p46, cospi_p46_m18, x[4], x[5], x[4], x[5]);
  btf_16_sse2(cospi_p26_p38, cospi_p38_m26, x[6], x[7], x[6], x[7]);
  btf_16_sse2(cospi_p34_p30, cospi_p30_m34, x[8], x[9], x[8], x[9]);
  btf_16_sse2(cospi_p42_p22, cospi_p22_m42, x[10], x[11], x[10], x[11]);
  btf_16_sse2(cospi_p50_p14, cospi_p14_m50, x[12], x[13], x[12], x[13]);
  btf_16_sse2(cospi_p58_p06, cospi_p06_m58, x[14], x[15], x[14], x[15]);

  // stage 3~9
  iadst16_stage3_ssse3(x);
  iadst16_stage4_ssse3(x, cospi, __rounding, cos_bit);
  iadst16_stage5_ssse3(x);
  iadst16_stage6_ssse3(x, cospi, __rounding, cos_bit);
  iadst16_stage7_ssse3(x);
  iadst16_stage8_ssse3(x, cospi, __rounding, cos_bit);
  iadst16_stage9_ssse3(output, x);
}

static void iadst16_w4_sse2(const __m128i *input, __m128i *output) {
  const int8_t cos_bit = INV_COS_BIT;
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m128i cospi_p02_p62 = pair_set_epi16(cospi[2], cospi[62]);
  const __m128i cospi_p62_m02 = pair_set_epi16(cospi[62], -cospi[2]);
  const __m128i cospi_p10_p54 = pair_set_epi16(cospi[10], cospi[54]);
  const __m128i cospi_p54_m10 = pair_set_epi16(cospi[54], -cospi[10]);
  const __m128i cospi_p18_p46 = pair_set_epi16(cospi[18], cospi[46]);
  const __m128i cospi_p46_m18 = pair_set_epi16(cospi[46], -cospi[18]);
  const __m128i cospi_p26_p38 = pair_set_epi16(cospi[26], cospi[38]);
  const __m128i cospi_p38_m26 = pair_set_epi16(cospi[38], -cospi[26]);
  const __m128i cospi_p34_p30 = pair_set_epi16(cospi[34], cospi[30]);
  const __m128i cospi_p30_m34 = pair_set_epi16(cospi[30], -cospi[34]);
  const __m128i cospi_p42_p22 = pair_set_epi16(cospi[42], cospi[22]);
  const __m128i cospi_p22_m42 = pair_set_epi16(cospi[22], -cospi[42]);
  const __m128i cospi_p50_p14 = pair_set_epi16(cospi[50], cospi[14]);
  const __m128i cospi_p14_m50 = pair_set_epi16(cospi[14], -cospi[50]);
  const __m128i cospi_p58_p06 = pair_set_epi16(cospi[58], cospi[6]);
  const __m128i cospi_p06_m58 = pair_set_epi16(cospi[6], -cospi[58]);
  const __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  const __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  const __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  const __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  const __m128i cospi_m56_p08 = pair_set_epi16(-cospi[56], cospi[8]);
  const __m128i cospi_m24_p40 = pair_set_epi16(-cospi[24], cospi[40]);
  const __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  const __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  const __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  const __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  const __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);

  // stage 1
  __m128i x[16];
  x[0] = input[15];
  x[1] = input[0];
  x[2] = input[13];
  x[3] = input[2];
  x[4] = input[11];
  x[5] = input[4];
  x[6] = input[9];
  x[7] = input[6];
  x[8] = input[7];
  x[9] = input[8];
  x[10] = input[5];
  x[11] = input[10];
  x[12] = input[3];
  x[13] = input[12];
  x[14] = input[1];
  x[15] = input[14];

  // stage 2
  btf_16_4p_sse2(cospi_p02_p62, cospi_p62_m02, x[0], x[1], x[0], x[1]);
  btf_16_4p_sse2(cospi_p10_p54, cospi_p54_m10, x[2], x[3], x[2], x[3]);
  btf_16_4p_sse2(cospi_p18_p46, cospi_p46_m18, x[4], x[5], x[4], x[5]);
  btf_16_4p_sse2(cospi_p26_p38, cospi_p38_m26, x[6], x[7], x[6], x[7]);
  btf_16_4p_sse2(cospi_p34_p30, cospi_p30_m34, x[8], x[9], x[8], x[9]);
  btf_16_4p_sse2(cospi_p42_p22, cospi_p22_m42, x[10], x[11], x[10], x[11]);
  btf_16_4p_sse2(cospi_p50_p14, cospi_p14_m50, x[12], x[13], x[12], x[13]);
  btf_16_4p_sse2(cospi_p58_p06, cospi_p06_m58, x[14], x[15], x[14], x[15]);

  // stage 3
  iadst16_stage3_ssse3(x);

  // stage 4
  btf_16_4p_sse2(cospi_p08_p56, cospi_p56_m08, x[8], x[9], x[8], x[9]);
  btf_16_4p_sse2(cospi_p40_p24, cospi_p24_m40, x[10], x[11], x[10], x[11]);
  btf_16_4p_sse2(cospi_m56_p08, cospi_p08_p56, x[12], x[13], x[12], x[13]);
  btf_16_4p_sse2(cospi_m24_p40, cospi_p40_p24, x[14], x[15], x[14], x[15]);

  // stage 5
  iadst16_stage5_ssse3(x);

  // stage 6
  btf_16_4p_sse2(cospi_p16_p48, cospi_p48_m16, x[4], x[5], x[4], x[5]);
  btf_16_4p_sse2(cospi_m48_p16, cospi_p16_p48, x[6], x[7], x[6], x[7]);
  btf_16_4p_sse2(cospi_p16_p48, cospi_p48_m16, x[12], x[13], x[12], x[13]);
  btf_16_4p_sse2(cospi_m48_p16, cospi_p16_p48, x[14], x[15], x[14], x[15]);

  // stage 7
  iadst16_stage7_ssse3(x);

  // stage 8
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[2], x[3], x[2], x[3]);
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[6], x[7], x[6], x[7]);
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[10], x[11], x[10], x[11]);
  btf_16_4p_sse2(cospi_p32_p32, cospi_p32_m32, x[14], x[15], x[14], x[15]);

  // stage 9
  iadst16_stage9_ssse3(output, x);
}

static void iidentity4_ssse3(const __m128i *input, __m128i *output) {
  const int16_t scale_fractional = (NewSqrt2 - (1 << NewSqrt2Bits));
  const __m128i scale = _mm_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  for (int i = 0; i < 4; ++i) {
    __m128i x = _mm_mulhrs_epi16(input[i], scale);
    output[i] = _mm_adds_epi16(x, input[i]);
  }
}

static void iidentity8_sse2(const __m128i *input, __m128i *output) {
  for (int i = 0; i < 8; ++i) {
    output[i] = _mm_adds_epi16(input[i], input[i]);
  }
}

static void iidentity16_ssse3(const __m128i *input, __m128i *output) {
  const int16_t scale_fractional = 2 * (NewSqrt2 - (1 << NewSqrt2Bits));
  const __m128i scale = _mm_set1_epi16(scale_fractional << (15 - NewSqrt2Bits));
  for (int i = 0; i < 16; ++i) {
    __m128i x = _mm_mulhrs_epi16(input[i], scale);
    __m128i srcx2 = _mm_adds_epi16(input[i], input[i]);
    output[i] = _mm_adds_epi16(x, srcx2);
  }
}

static INLINE __m128i lowbd_get_recon_8x8_sse2(const __m128i pred,
                                               __m128i res) {
  const __m128i zero = _mm_setzero_si128();
  __m128i x0 = _mm_adds_epi16(res, _mm_unpacklo_epi8(pred, zero));
  return _mm_packus_epi16(x0, x0);
}

static INLINE void lowbd_write_buffer_4xn_sse2(__m128i *in, uint8_t *output,
                                               int stride, int flipud,
                                               const int height) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  const __m128i zero = _mm_setzero_si128();
  for (int i = 0; i < height; ++i, j += step) {
    const __m128i v = _mm_cvtsi32_si128(*((uint32_t *)(output + i * stride)));
    __m128i u = _mm_adds_epi16(in[j], _mm_unpacklo_epi8(v, zero));
    u = _mm_packus_epi16(u, zero);
    *((int *)(output + i * stride)) = _mm_cvtsi128_si32(u);
  }
}

static INLINE void lowbd_write_buffer_8xn_sse2(__m128i *in, uint8_t *output,
                                               int stride, int flipud,
                                               const int height) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    const __m128i v = _mm_loadl_epi64((__m128i const *)(output + i * stride));
    const __m128i u = lowbd_get_recon_8x8_sse2(v, in[j]);
    _mm_storel_epi64((__m128i *)(output + i * stride), u);
  }
}

// 1D functions process process 8 pixels at one time.
static const transform_1d_ssse3
    lowbd_txfm_all_1d_w8_arr[TX_SIZES][ITX_TYPES_1D] = {
      { idct4_sse2, iadst4_sse2, iidentity4_ssse3 },
      { idct8_sse2, iadst8_sse2, iidentity8_sse2 },
      { idct16_sse2, iadst16_sse2, iidentity16_ssse3 },
      { idct32_sse2, NULL, NULL },
      { idct64_low32_ssse3, NULL, NULL },
    };

// functions for blocks with eob at DC and within
// topleft 8x8, 16x16, 32x32 corner
static const transform_1d_ssse3
    lowbd_txfm_all_1d_zeros_w8_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { idct4_sse2, idct4_sse2, NULL, NULL },
          { iadst4_sse2, iadst4_sse2, NULL, NULL },
          { iidentity4_ssse3, iidentity4_ssse3, NULL, NULL },
      },
      { { idct8_low1_ssse3, idct8_sse2, NULL, NULL },
        { iadst8_low1_ssse3, iadst8_sse2, NULL, NULL },
        { iidentity8_sse2, iidentity8_sse2, NULL, NULL } },
      {
          { idct16_low1_ssse3, idct16_low8_ssse3, idct16_sse2, NULL },
          { iadst16_low1_ssse3, iadst16_low8_ssse3, iadst16_sse2, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { idct32_low1_ssse3, idct32_low8_ssse3, idct32_low16_ssse3,
          idct32_sse2 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },
      { { idct64_low1_ssse3, idct64_low8_ssse3, idct64_low16_ssse3,
          idct64_low32_ssse3 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };

// 1D functions process process 4 pixels at one time.
// used in 4x4, 4x8, 4x16, 8x4, 16x4
static const transform_1d_ssse3
    lowbd_txfm_all_1d_w4_arr[TX_SIZES][ITX_TYPES_1D] = {
      { idct4_w4_sse2, iadst4_w4_sse2, iidentity4_ssse3 },
      { idct8_w4_sse2, iadst8_w4_sse2, iidentity8_sse2 },
      { idct16_w4_sse2, iadst16_w4_sse2, iidentity16_ssse3 },
      { NULL, NULL, NULL },
      { NULL, NULL, NULL },
    };

static INLINE void iidentity_row_8xn_ssse3(__m128i *out, const int32_t *input,
                                           int stride, int shift, int height,
                                           int txw_idx, int rect_type) {
  const int32_t *input_row = input;
  const __m128i scale = _mm_set1_epi16(NewSqrt2list[txw_idx]);
  const __m128i rounding = _mm_set1_epi16((1 << (NewSqrt2Bits - 1)) +
                                          (1 << (NewSqrt2Bits - shift - 1)));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i scale_rounding = _mm_unpacklo_epi16(scale, rounding);
  if (rect_type != 1 && rect_type != -1) {
    for (int i = 0; i < height; ++i) {
      const __m128i src = load_32bit_to_16bit(input_row);
      input_row += stride;
      __m128i lo = _mm_unpacklo_epi16(src, one);
      __m128i hi = _mm_unpackhi_epi16(src, one);
      lo = _mm_madd_epi16(lo, scale_rounding);
      hi = _mm_madd_epi16(hi, scale_rounding);
      lo = _mm_srai_epi32(lo, NewSqrt2Bits - shift);
      hi = _mm_srai_epi32(hi, NewSqrt2Bits - shift);
      out[i] = _mm_packs_epi32(lo, hi);
    }
  } else {
    const __m128i rect_scale =
        _mm_set1_epi16(NewInvSqrt2 << (15 - NewSqrt2Bits));
    for (int i = 0; i < height; ++i) {
      __m128i src = load_32bit_to_16bit(input_row);
      src = _mm_mulhrs_epi16(src, rect_scale);
      input_row += stride;
      __m128i lo = _mm_unpacklo_epi16(src, one);
      __m128i hi = _mm_unpackhi_epi16(src, one);
      lo = _mm_madd_epi16(lo, scale_rounding);
      hi = _mm_madd_epi16(hi, scale_rounding);
      lo = _mm_srai_epi32(lo, NewSqrt2Bits - shift);
      hi = _mm_srai_epi32(hi, NewSqrt2Bits - shift);
      out[i] = _mm_packs_epi32(lo, hi);
    }
  }
}

static INLINE void iidentity_col_8xn_ssse3(uint8_t *output, int stride,
                                           __m128i *buf, int shift, int height,
                                           int txh_idx) {
  const __m128i scale = _mm_set1_epi16(NewSqrt2list[txh_idx]);
  const __m128i scale_rounding = _mm_set1_epi16(1 << (NewSqrt2Bits - 1));
  const __m128i shift_rounding = _mm_set1_epi32(1 << (-shift - 1));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i scale_coeff = _mm_unpacklo_epi16(scale, scale_rounding);
  const __m128i zero = _mm_setzero_si128();
  for (int h = 0; h < height; ++h) {
    __m128i lo = _mm_unpacklo_epi16(buf[h], one);
    __m128i hi = _mm_unpackhi_epi16(buf[h], one);
    lo = _mm_madd_epi16(lo, scale_coeff);
    hi = _mm_madd_epi16(hi, scale_coeff);
    lo = _mm_srai_epi32(lo, NewSqrt2Bits);
    hi = _mm_srai_epi32(hi, NewSqrt2Bits);
    lo = _mm_add_epi32(lo, shift_rounding);
    hi = _mm_add_epi32(hi, shift_rounding);
    lo = _mm_srai_epi32(lo, -shift);
    hi = _mm_srai_epi32(hi, -shift);
    __m128i x = _mm_packs_epi32(lo, hi);

    const __m128i pred = _mm_loadl_epi64((__m128i const *)(output));
    x = _mm_adds_epi16(x, _mm_unpacklo_epi8(pred, zero));
    const __m128i u = _mm_packus_epi16(x, x);
    _mm_storel_epi64((__m128i *)(output), u);
    output += stride;
  }
}

static INLINE void lowbd_inv_txfm2d_add_idtx_ssse3(const int32_t *input,
                                                   uint8_t *output, int stride,
                                                   TX_SIZE tx_size) {
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int input_stride = AOMMIN(32, txfm_size_col);
  const int row_max = AOMMIN(32, txfm_size_row);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  __m128i buf[32];

  for (int i = 0; i < (input_stride >> 3); ++i) {
    iidentity_row_8xn_ssse3(buf, input + 8 * i, input_stride, shift[0], row_max,
                            txw_idx, rect_type);
    iidentity_col_8xn_ssse3(output + 8 * i, stride, buf, shift[1], row_max,
                            txh_idx);
  }
}

static void lowbd_inv_txfm2d_add_4x4_ssse3(const int32_t *input,
                                           uint8_t *output, int stride,
                                           TX_TYPE tx_type, TX_SIZE tx_size_,
                                           int eob) {
  (void)tx_size_;
  (void)eob;
  __m128i buf[4];
  const TX_SIZE tx_size = TX_4X4;
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_ssse3 row_txfm =
      lowbd_txfm_all_1d_w4_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_ssse3 col_txfm =
      lowbd_txfm_all_1d_w4_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  load_buffer_32bit_to_16bit_w4(input, txfm_size_col, buf, txfm_size_row);
  transpose_16bit_4x4(buf, buf);
  row_txfm(buf, buf);
  if (lr_flip) {
    __m128i temp[4];
    flip_buf_sse2(buf, temp, txfm_size_col);
    transpose_16bit_4x4(temp, buf);
  } else {
    transpose_16bit_4x4(buf, buf);
  }
  col_txfm(buf, buf);
  round_shift_16bit_ssse3(buf, txfm_size_row, shift[1]);
  lowbd_write_buffer_4xn_sse2(buf, output, stride, ud_flip, txfm_size_row);
}

static INLINE __m128i lowbd_get_recon_16x16_sse2(const __m128i pred,
                                                 __m128i res0, __m128i res1) {
  const __m128i zero = _mm_setzero_si128();
  __m128i x0 = _mm_unpacklo_epi8(pred, zero);
  __m128i x1 = _mm_unpackhi_epi8(pred, zero);
  x0 = _mm_adds_epi16(res0, x0);
  x1 = _mm_adds_epi16(res1, x1);
  return _mm_packus_epi16(x0, x1);
}

static INLINE void lowbd_write_buffer_16xn_sse2(__m128i *in, uint8_t *output,
                                                int stride, int flipud,
                                                int height) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    __m128i v = _mm_loadu_si128((__m128i const *)(output + i * stride));
    __m128i u = lowbd_get_recon_16x16_sse2(v, in[j], in[j + height]);
    _mm_storeu_si128((__m128i *)(output + i * stride), u);
  }
}

static INLINE void round_shift_ssse3(const __m128i *input, __m128i *output,
                                     int size) {
  const __m128i scale = _mm_set1_epi16(NewInvSqrt2 * 8);
  for (int i = 0; i < size; ++i) {
    output[i] = _mm_mulhrs_epi16(input[i], scale);
  }
}

static INLINE void lowbd_inv_txfm2d_add_no_identity_ssse3(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  __m128i buf1[64 * 8];
  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_nonzero_w_div8 = (eobx + 8) >> 3;
  const int buf_size_nonzero_h_div8 = (eoby + 8) >> 3;
  const int input_stride = AOMMIN(32, txfm_size_col);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_ssse3 row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_ssse3 col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < buf_size_nonzero_h_div8; i++) {
    __m128i buf0[64];
    const int32_t *input_row = input + i * input_stride * 8;
    for (int j = 0; j < buf_size_nonzero_w_div8; ++j) {
      __m128i *buf0_cur = buf0 + j * 8;
      load_buffer_32bit_to_16bit(input_row + j * 8, input_stride, buf0_cur, 8);
      transpose_16bit_8x8(buf0_cur, buf0_cur);
    }
    if (rect_type == 1 || rect_type == -1) {
      round_shift_ssse3(buf0, buf0, input_stride);  // rect special code
    }
    row_txfm(buf0, buf0);
    round_shift_16bit_ssse3(buf0, txfm_size_col, shift[0]);
    __m128i *_buf1 = buf1 + i * 8;
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        __m128i temp[8];
        flip_buf_sse2(buf0 + 8 * j, temp, 8);
        transpose_16bit_8x8(temp,
                            _buf1 + txfm_size_row * (buf_size_w_div8 - 1 - j));
      }
    } else {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        transpose_16bit_8x8(buf0 + 8 * j, _buf1 + txfm_size_row * j);
      }
    }
  }
  for (int i = 0; i < buf_size_w_div8; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row);
    round_shift_16bit_ssse3(buf1 + i * txfm_size_row, txfm_size_row, shift[1]);
  }

  if (txfm_size_col >= 16) {
    for (int i = 0; i < (txfm_size_col >> 4); i++) {
      lowbd_write_buffer_16xn_sse2(buf1 + i * txfm_size_row * 2,
                                   output + 16 * i, stride, ud_flip,
                                   txfm_size_row);
    }
  } else if (txfm_size_col == 8) {
    lowbd_write_buffer_8xn_sse2(buf1, output, stride, ud_flip, txfm_size_row);
  }
}

static INLINE void lowbd_inv_txfm2d_add_h_identity_ssse3(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  int eobx, eoby;
  get_eobx_eoby_scan_h_identity(&eobx, &eoby, tx_size, eob);
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = (eobx + 8) >> 3;
  const int input_stride = AOMMIN(32, txfm_size_col);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx = lowbd_txfm_all_1d_zeros_idx[eoby];
  assert(fun_idx < 5);
  const transform_1d_ssse3 col_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx];

  assert(col_txfm != NULL);

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < buf_size_w_div8; i++) {
    __m128i buf0[64];
    iidentity_row_8xn_ssse3(buf0, input + 8 * i, input_stride, shift[0],
                            eoby + 1, txw_idx, rect_type);
    col_txfm(buf0, buf0);
    __m128i mshift = _mm_set1_epi16(1 << (15 + shift[1]));
    int k = ud_flip ? (txfm_size_row - 1) : 0;
    const int step = ud_flip ? -1 : 1;
    uint8_t *out = output + 8 * i;
    for (int j = 0; j < txfm_size_row; ++j, k += step) {
      const __m128i v = _mm_loadl_epi64((__m128i const *)(out));
      __m128i res = _mm_mulhrs_epi16(buf0[k], mshift);
      const __m128i u = lowbd_get_recon_8x8_sse2(v, res);
      _mm_storel_epi64((__m128i *)(out), u);
      out += stride;
    }
  }
}

static INLINE void lowbd_inv_txfm2d_add_v_identity_ssse3(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  __m128i buf1[64];
  int eobx, eoby;
  get_eobx_eoby_scan_v_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_h_div8 = (eoby + 8) >> 3;
  const int input_stride = AOMMIN(32, txfm_size_col);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx = lowbd_txfm_all_1d_zeros_idx[eobx];
  const transform_1d_ssse3 row_txfm =
      lowbd_txfm_all_1d_zeros_w8_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx];

  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < buf_size_h_div8; i++) {
    __m128i buf0[64];
    const int32_t *input_row = input + i * input_stride * 8;
    for (int j = 0; j < AOMMIN(4, buf_size_w_div8); ++j) {
      __m128i *buf0_cur = buf0 + j * 8;
      load_buffer_32bit_to_16bit(input_row + j * 8, input_stride, buf0_cur, 8);
      transpose_16bit_8x8(buf0_cur, buf0_cur);
    }
    if (rect_type == 1 || rect_type == -1) {
      round_shift_ssse3(buf0, buf0, input_stride);  // rect special code
    }
    row_txfm(buf0, buf0);
    round_shift_16bit_ssse3(buf0, txfm_size_col, shift[0]);
    __m128i *_buf1 = buf1;
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        __m128i temp[8];
        flip_buf_sse2(buf0 + 8 * j, temp, 8);
        transpose_16bit_8x8(temp, _buf1 + 8 * (buf_size_w_div8 - 1 - j));
      }
    } else {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        transpose_16bit_8x8(buf0 + 8 * j, _buf1 + 8 * j);
      }
    }

    for (int j = 0; j < buf_size_w_div8; ++j) {
      iidentity_col_8xn_ssse3(output + i * 8 * stride + j * 8, stride,
                              buf1 + j * 8, shift[1], 8, txh_idx);
    }
  }
}

// for 32x32,32x64,64x32,64x64,32x8,8x32,16x32,32x16,64x16,16x64
static INLINE void lowbd_inv_txfm2d_add_universe_ssse3(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  switch (tx_type) {
    case DCT_DCT:
      lowbd_inv_txfm2d_add_no_identity_ssse3(input, output, stride, tx_type,
                                             tx_size, eob);
      break;
    case IDTX:
      lowbd_inv_txfm2d_add_idtx_ssse3(input, output, stride, tx_size);
      break;
    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      lowbd_inv_txfm2d_add_h_identity_ssse3(input, output, stride, tx_type,
                                            tx_size, eob);
      break;
    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      lowbd_inv_txfm2d_add_v_identity_ssse3(input, output, stride, tx_type,
                                            tx_size, eob);
      break;
    default:
      lowbd_inv_txfm2d_add_no_identity_ssse3(input, output, stride, tx_type,
                                             tx_size, eob);
      break;
  }
}

static void lowbd_inv_txfm2d_add_4x8_ssse3(const int32_t *input,
                                           uint8_t *output, int stride,
                                           TX_TYPE tx_type, TX_SIZE tx_size_,
                                           int eob) {
  (void)tx_size_;
  (void)eob;
  __m128i buf[8];
  const TX_SIZE tx_size = TX_4X8;
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_ssse3 row_txfm =
      lowbd_txfm_all_1d_w8_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_ssse3 col_txfm =
      lowbd_txfm_all_1d_w4_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  load_buffer_32bit_to_16bit_w4(input, txfm_size_col, buf, txfm_size_row);
  transpose_16bit_4x8(buf, buf);
  round_shift_ssse3(buf, buf, txfm_size_col);  // rect special code
  row_txfm(buf, buf);
  // round_shift_16bit_ssse3(buf, txfm_size_col, shift[0]);// shift[0] is 0
  if (lr_flip) {
    __m128i temp[4];
    flip_buf_sse2(buf, temp, txfm_size_col);
    transpose_16bit_8x4(temp, buf);
  } else {
    transpose_16bit_8x4(buf, buf);
  }
  col_txfm(buf, buf);
  round_shift_16bit_ssse3(buf, txfm_size_row, shift[1]);
  lowbd_write_buffer_4xn_sse2(buf, output, stride, ud_flip, txfm_size_row);
}

static void lowbd_inv_txfm2d_add_8x4_ssse3(const int32_t *input,
                                           uint8_t *output, int stride,
                                           TX_TYPE tx_type, TX_SIZE tx_size_,
                                           int eob) {
  (void)tx_size_;
  (void)eob;
  __m128i buf[8];
  const TX_SIZE tx_size = TX_8X4;
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_ssse3 row_txfm =
      lowbd_txfm_all_1d_w4_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_ssse3 col_txfm =
      lowbd_txfm_all_1d_w8_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  load_buffer_32bit_to_16bit(input, txfm_size_col, buf, txfm_size_row);
  transpose_16bit_8x4(buf, buf);
  round_shift_ssse3(buf, buf, txfm_size_col);  // rect special code
  row_txfm(buf, buf);
  // round_shift_16bit_ssse3(buf, txfm_size_col, shift[0]); // shift[0] is 0
  if (lr_flip) {
    __m128i temp[8];
    flip_buf_sse2(buf, temp, txfm_size_col);
    transpose_16bit_4x8(temp, buf);
  } else {
    transpose_16bit_4x8(buf, buf);
  }
  col_txfm(buf, buf);
  round_shift_16bit_ssse3(buf, txfm_size_row, shift[1]);
  lowbd_write_buffer_8xn_sse2(buf, output, stride, ud_flip, txfm_size_row);
}

static void lowbd_inv_txfm2d_add_4x16_ssse3(const int32_t *input,
                                            uint8_t *output, int stride,
                                            TX_TYPE tx_type, TX_SIZE tx_size_,
                                            int eob) {
  (void)tx_size_;
  (void)eob;
  __m128i buf[16];
  const TX_SIZE tx_size = TX_4X16;
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_ssse3 row_txfm =
      lowbd_txfm_all_1d_w8_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_ssse3 col_txfm =
      lowbd_txfm_all_1d_w4_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  const int row_one_loop = 8;
  for (int i = 0; i < 2; ++i) {
    const int32_t *input_cur = input + i * txfm_size_col * row_one_loop;
    __m128i *buf_cur = buf + i * row_one_loop;
    load_buffer_32bit_to_16bit_w4(input_cur, txfm_size_col, buf_cur,
                                  row_one_loop);
    transpose_16bit_4x8(buf_cur, buf_cur);
    if (row_txfm == iidentity4_ssse3) {
      const __m128i scale = pair_set_epi16(NewSqrt2, 3 << (NewSqrt2Bits - 1));
      const __m128i ones = _mm_set1_epi16(1);
      for (int j = 0; j < 4; ++j) {
        const __m128i buf_lo = _mm_unpacklo_epi16(buf_cur[j], ones);
        const __m128i buf_hi = _mm_unpackhi_epi16(buf_cur[j], ones);
        const __m128i buf_32_lo =
            _mm_srai_epi32(_mm_madd_epi16(buf_lo, scale), (NewSqrt2Bits + 1));
        const __m128i buf_32_hi =
            _mm_srai_epi32(_mm_madd_epi16(buf_hi, scale), (NewSqrt2Bits + 1));
        buf_cur[j] = _mm_packs_epi32(buf_32_lo, buf_32_hi);
      }
    } else {
      row_txfm(buf_cur, buf_cur);
      round_shift_16bit_ssse3(buf_cur, row_one_loop, shift[0]);
    }
    if (lr_flip) {
      __m128i temp[8];
      flip_buf_sse2(buf_cur, temp, txfm_size_col);
      transpose_16bit_8x4(temp, buf_cur);
    } else {
      transpose_16bit_8x4(buf_cur, buf_cur);
    }
  }
  col_txfm(buf, buf);
  round_shift_16bit_ssse3(buf, txfm_size_row, shift[1]);
  lowbd_write_buffer_4xn_sse2(buf, output, stride, ud_flip, txfm_size_row);
}

static void lowbd_inv_txfm2d_add_16x4_ssse3(const int32_t *input,
                                            uint8_t *output, int stride,
                                            TX_TYPE tx_type, TX_SIZE tx_size_,
                                            int eob) {
  (void)tx_size_;
  (void)eob;
  __m128i buf[16];
  const TX_SIZE tx_size = TX_16X4;
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;

  const transform_1d_ssse3 row_txfm =
      lowbd_txfm_all_1d_w4_arr[txw_idx][hitx_1d_tab[tx_type]];
  const transform_1d_ssse3 col_txfm =
      lowbd_txfm_all_1d_w8_arr[txh_idx][vitx_1d_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int row_one_loop = 8;
  for (int i = 0; i < buf_size_w_div8; ++i) {
    const int32_t *input_cur = input + i * row_one_loop;
    __m128i *buf_cur = buf + i * row_one_loop;
    load_buffer_32bit_to_16bit(input_cur, txfm_size_col, buf_cur,
                               txfm_size_row);
    transpose_16bit_8x4(buf_cur, buf_cur);
  }
  if (row_txfm == iidentity16_ssse3) {
    const __m128i scale = pair_set_epi16(2 * NewSqrt2, 3 << (NewSqrt2Bits - 1));
    const __m128i ones = _mm_set1_epi16(1);
    for (int j = 0; j < 16; ++j) {
      const __m128i buf_lo = _mm_unpacklo_epi16(buf[j], ones);
      const __m128i buf_hi = _mm_unpackhi_epi16(buf[j], ones);
      const __m128i buf_32_lo =
          _mm_srai_epi32(_mm_madd_epi16(buf_lo, scale), (NewSqrt2Bits + 1));
      const __m128i buf_32_hi =
          _mm_srai_epi32(_mm_madd_epi16(buf_hi, scale), (NewSqrt2Bits + 1));
      buf[j] = _mm_packs_epi32(buf_32_lo, buf_32_hi);
    }
  } else {
    row_txfm(buf, buf);
    round_shift_16bit_ssse3(buf, txfm_size_col, shift[0]);
  }
  if (lr_flip) {
    __m128i temp[16];
    flip_buf_sse2(buf, temp, 16);
    transpose_16bit_4x8(temp, buf);
    transpose_16bit_4x8(temp + 8, buf + 8);
  } else {
    transpose_16bit_4x8(buf, buf);
    transpose_16bit_4x8(buf + row_one_loop, buf + row_one_loop);
  }
  for (int i = 0; i < buf_size_w_div8; i++) {
    col_txfm(buf + i * row_one_loop, buf + i * row_one_loop);
    round_shift_16bit_ssse3(buf + i * row_one_loop, txfm_size_row, shift[1]);
  }
  lowbd_write_buffer_8xn_sse2(buf, output, stride, ud_flip, 4);
  lowbd_write_buffer_8xn_sse2(buf + 8, output + 8, stride, ud_flip, 4);
}

void av1_lowbd_inv_txfm2d_add_ssse3(const int32_t *input, uint8_t *output,
                                    int stride, TX_TYPE tx_type,
                                    TX_SIZE tx_size, int eob) {
  switch (tx_size) {
    case TX_4X4:
      lowbd_inv_txfm2d_add_4x4_ssse3(input, output, stride, tx_type, tx_size,
                                     eob);
      break;
    case TX_4X8:
      lowbd_inv_txfm2d_add_4x8_ssse3(input, output, stride, tx_type, tx_size,
                                     eob);
      break;
    case TX_8X4:
      lowbd_inv_txfm2d_add_8x4_ssse3(input, output, stride, tx_type, tx_size,
                                     eob);
      break;
    case TX_4X16:
      lowbd_inv_txfm2d_add_4x16_ssse3(input, output, stride, tx_type, tx_size,
                                      eob);
      break;
    case TX_16X4:
      lowbd_inv_txfm2d_add_16x4_ssse3(input, output, stride, tx_type, tx_size,
                                      eob);
      break;
    default:
      lowbd_inv_txfm2d_add_universe_ssse3(input, output, stride, tx_type,
                                          tx_size, eob);
      break;
  }
}

void av1_inv_txfm_add_ssse3(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                            const TxfmParam *txfm_param) {
  if (!txfm_param->lossless) {
    const TX_TYPE tx_type = txfm_param->tx_type;
    av1_lowbd_inv_txfm2d_add_ssse3(dqcoeff, dst, stride, tx_type,
                                   txfm_param->tx_size, txfm_param->eob);

  } else {
    av1_inv_txfm_add_c(dqcoeff, dst, stride, txfm_param);
  }
}
