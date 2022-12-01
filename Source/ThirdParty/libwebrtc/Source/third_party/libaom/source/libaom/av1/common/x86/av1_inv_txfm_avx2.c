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
#include "av1/common/x86/av1_txfm_sse2.h"
#include "av1/common/x86/av1_inv_txfm_avx2.h"
#include "av1/common/x86/av1_inv_txfm_ssse3.h"

// TODO(venkatsanampudi@ittiam.com): move this to header file

// Sqrt2, Sqrt2^2, Sqrt2^3, Sqrt2^4, Sqrt2^5
static int32_t NewSqrt2list[TX_SIZES] = { 5793, 2 * 4096, 2 * 5793, 4 * 4096,
                                          4 * 5793 };

static INLINE void idct16_stage5_avx2(__m256i *x1, const int32_t *cospi,
                                      const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_avx2(&x1[0], &x1[3]);
  btf_16_adds_subs_avx2(&x1[1], &x1[2]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x1[5], &x1[6], _r, cos_bit);

  btf_16_adds_subs_avx2(&x1[8], &x1[11]);
  btf_16_adds_subs_avx2(&x1[9], &x1[10]);
  btf_16_adds_subs_avx2(&x1[15], &x1[12]);
  btf_16_adds_subs_avx2(&x1[14], &x1[13]);
}

static INLINE void idct16_stage6_avx2(__m256i *x, const int32_t *cospi,
                                      const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_avx2(&x[0], &x[7]);
  btf_16_adds_subs_avx2(&x[1], &x[6]);
  btf_16_adds_subs_avx2(&x[2], &x[5]);
  btf_16_adds_subs_avx2(&x[3], &x[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[10], &x[13], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[11], &x[12], _r, cos_bit);
}

static INLINE void idct16_stage7_avx2(__m256i *output, __m256i *x1) {
  btf_16_adds_subs_out_avx2(&output[0], &output[15], x1[0], x1[15]);
  btf_16_adds_subs_out_avx2(&output[1], &output[14], x1[1], x1[14]);
  btf_16_adds_subs_out_avx2(&output[2], &output[13], x1[2], x1[13]);
  btf_16_adds_subs_out_avx2(&output[3], &output[12], x1[3], x1[12]);
  btf_16_adds_subs_out_avx2(&output[4], &output[11], x1[4], x1[11]);
  btf_16_adds_subs_out_avx2(&output[5], &output[10], x1[5], x1[10]);
  btf_16_adds_subs_out_avx2(&output[6], &output[9], x1[6], x1[9]);
  btf_16_adds_subs_out_avx2(&output[7], &output[8], x1[7], x1[8]);
}

static void idct16_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
  __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
  __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
  __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
  __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
  __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
  __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);
  __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
  __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);

  // stage 1
  __m256i x1[16];
  x1[0] = input[0];
  x1[1] = input[8];
  x1[2] = input[4];
  x1[3] = input[12];
  x1[4] = input[2];
  x1[5] = input[10];
  x1[6] = input[6];
  x1[7] = input[14];
  x1[8] = input[1];
  x1[9] = input[9];
  x1[10] = input[5];
  x1[11] = input[13];
  x1[12] = input[3];
  x1[13] = input[11];
  x1[14] = input[7];
  x1[15] = input[15];

  // stage 2
  btf_16_w16_avx2(cospi_p60_m04, cospi_p04_p60, &x1[8], &x1[15], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p28_m36, cospi_p36_p28, &x1[9], &x1[14], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p44_m20, cospi_p20_p44, &x1[10], &x1[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p12_m52, cospi_p52_p12, &x1[11], &x1[12], _r,
                  INV_COS_BIT);

  // stage 3
  btf_16_w16_avx2(cospi_p56_m08, cospi_p08_p56, &x1[4], &x1[7], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p24_m40, cospi_p40_p24, &x1[5], &x1[6], _r,
                  INV_COS_BIT);
  btf_16_adds_subs_avx2(&x1[8], &x1[9]);
  btf_16_adds_subs_avx2(&x1[11], &x1[10]);
  btf_16_adds_subs_avx2(&x1[12], &x1[13]);
  btf_16_adds_subs_avx2(&x1[15], &x1[14]);

  // stage 4
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[0], &x1[1], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p48_m16, cospi_p16_p48, &x1[2], &x1[3], _r,
                  INV_COS_BIT);
  btf_16_adds_subs_avx2(&x1[4], &x1[5]);
  btf_16_adds_subs_avx2(&x1[7], &x1[6]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[9], &x1[14], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[10], &x1[13], _r,
                  INV_COS_BIT);

  idct16_stage5_avx2(x1, cospi, _r, INV_COS_BIT);
  idct16_stage6_avx2(x1, cospi, _r, INV_COS_BIT);
  idct16_stage7_avx2(output, x1);
}

static void idct16_low8_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);

  // stage 1
  __m256i x1[16];
  x1[0] = input[0];
  x1[2] = input[4];
  x1[4] = input[2];
  x1[6] = input[6];
  x1[8] = input[1];
  x1[10] = input[5];
  x1[12] = input[3];
  x1[14] = input[7];

  // stage 2
  btf_16_w16_0_avx2(cospi[60], cospi[4], x1[8], x1[8], x1[15]);
  btf_16_w16_0_avx2(-cospi[36], cospi[28], x1[14], x1[9], x1[14]);
  btf_16_w16_0_avx2(cospi[44], cospi[20], x1[10], x1[10], x1[13]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x1[12], x1[11], x1[12]);

  // stage 3
  btf_16_w16_0_avx2(cospi[56], cospi[8], x1[4], x1[4], x1[7]);
  btf_16_w16_0_avx2(-cospi[40], cospi[24], x1[6], x1[5], x1[6]);
  btf_16_adds_subs_avx2(&x1[8], &x1[9]);
  btf_16_adds_subs_avx2(&x1[11], &x1[10]);
  btf_16_adds_subs_avx2(&x1[12], &x1[13]);
  btf_16_adds_subs_avx2(&x1[15], &x1[14]);

  // stage 4
  btf_16_w16_0_avx2(cospi[32], cospi[32], x1[0], x1[0], x1[1]);
  btf_16_w16_0_avx2(cospi[48], cospi[16], x1[2], x1[2], x1[3]);
  btf_16_adds_subs_avx2(&x1[4], &x1[5]);
  btf_16_adds_subs_avx2(&x1[7], &x1[6]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x1[9], &x1[14], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x1[10], &x1[13], _r,
                  INV_COS_BIT);

  idct16_stage5_avx2(x1, cospi, _r, INV_COS_BIT);
  idct16_stage6_avx2(x1, cospi, _r, INV_COS_BIT);
  idct16_stage7_avx2(output, x1);
}

static void idct16_low1_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);

  // stage 1
  __m256i x1[2];
  x1[0] = input[0];

  // stage 2
  // stage 3
  // stage 4
  btf_16_w16_0_avx2(cospi[32], cospi[32], x1[0], x1[0], x1[1]);

  // stage 5
  // stage 6
  output[0] = x1[0];
  output[1] = x1[1];
  output[2] = x1[1];
  output[3] = x1[0];
  output[4] = x1[0];
  output[5] = x1[1];
  output[6] = x1[1];
  output[7] = x1[0];
  output[8] = x1[0];
  output[9] = x1[1];
  output[10] = x1[1];
  output[11] = x1[0];
  output[12] = x1[0];
  output[13] = x1[1];
  output[14] = x1[1];
  output[15] = x1[0];
}

static INLINE void iadst16_stage3_avx2(__m256i *x) {
  btf_16_adds_subs_avx2(&x[0], &x[8]);
  btf_16_adds_subs_avx2(&x[1], &x[9]);
  btf_16_adds_subs_avx2(&x[2], &x[10]);
  btf_16_adds_subs_avx2(&x[3], &x[11]);
  btf_16_adds_subs_avx2(&x[4], &x[12]);
  btf_16_adds_subs_avx2(&x[5], &x[13]);
  btf_16_adds_subs_avx2(&x[6], &x[14]);
  btf_16_adds_subs_avx2(&x[7], &x[15]);
}

static INLINE void iadst16_stage4_avx2(__m256i *x, const int32_t *cospi,
                                       const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  const __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  const __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  const __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  const __m256i cospi_m56_p08 = pair_set_w16_epi16(-cospi[56], cospi[8]);
  const __m256i cospi_m24_p40 = pair_set_w16_epi16(-cospi[24], cospi[40]);
  btf_16_w16_avx2(cospi_p08_p56, cospi_p56_m08, &x[8], &x[9], _r, cos_bit);
  btf_16_w16_avx2(cospi_p40_p24, cospi_p24_m40, &x[10], &x[11], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_p08, cospi_p08_p56, &x[12], &x[13], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_p40, cospi_p40_p24, &x[14], &x[15], _r, cos_bit);
}

static INLINE void iadst16_stage5_avx2(__m256i *x) {
  btf_16_adds_subs_avx2(&x[0], &x[4]);
  btf_16_adds_subs_avx2(&x[1], &x[5]);
  btf_16_adds_subs_avx2(&x[2], &x[6]);
  btf_16_adds_subs_avx2(&x[3], &x[7]);
  btf_16_adds_subs_avx2(&x[8], &x[12]);
  btf_16_adds_subs_avx2(&x[9], &x[13]);
  btf_16_adds_subs_avx2(&x[10], &x[14]);
  btf_16_adds_subs_avx2(&x[11], &x[15]);
}

static INLINE void iadst16_stage6_avx2(__m256i *x, const int32_t *cospi,
                                       const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  const __m256i cospi_m48_p16 = pair_set_w16_epi16(-cospi[48], cospi[16]);
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x[4], &x[5], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x[6], &x[7], _r, cos_bit);
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x[12], &x[13], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_p16, cospi_p16_p48, &x[14], &x[15], _r, cos_bit);
}

static INLINE void iadst16_stage7_avx2(__m256i *x) {
  btf_16_adds_subs_avx2(&x[0], &x[2]);
  btf_16_adds_subs_avx2(&x[1], &x[3]);
  btf_16_adds_subs_avx2(&x[4], &x[6]);
  btf_16_adds_subs_avx2(&x[5], &x[7]);
  btf_16_adds_subs_avx2(&x[8], &x[10]);
  btf_16_adds_subs_avx2(&x[9], &x[11]);
  btf_16_adds_subs_avx2(&x[12], &x[14]);
  btf_16_adds_subs_avx2(&x[13], &x[15]);
}

static INLINE void iadst16_stage8_avx2(__m256i *x1, const int32_t *cospi,
                                       const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[2], &x1[3], _r, cos_bit);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[6], &x1[7], _r, cos_bit);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[10], &x1[11], _r, cos_bit);
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[14], &x1[15], _r, cos_bit);
}

static INLINE void iadst16_stage9_avx2(__m256i *output, __m256i *x1) {
  const __m256i __zero = _mm256_setzero_si256();
  output[0] = x1[0];
  output[1] = _mm256_subs_epi16(__zero, x1[8]);
  output[2] = x1[12];
  output[3] = _mm256_subs_epi16(__zero, x1[4]);
  output[4] = x1[6];
  output[5] = _mm256_subs_epi16(__zero, x1[14]);
  output[6] = x1[10];
  output[7] = _mm256_subs_epi16(__zero, x1[2]);
  output[8] = x1[3];
  output[9] = _mm256_subs_epi16(__zero, x1[11]);
  output[10] = x1[15];
  output[11] = _mm256_subs_epi16(__zero, x1[7]);
  output[12] = x1[5];
  output[13] = _mm256_subs_epi16(__zero, x1[13]);
  output[14] = x1[9];
  output[15] = _mm256_subs_epi16(__zero, x1[1]);
}

static void iadst16_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);

  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p02_p62 = pair_set_w16_epi16(cospi[2], cospi[62]);
  __m256i cospi_p62_m02 = pair_set_w16_epi16(cospi[62], -cospi[2]);
  __m256i cospi_p10_p54 = pair_set_w16_epi16(cospi[10], cospi[54]);
  __m256i cospi_p54_m10 = pair_set_w16_epi16(cospi[54], -cospi[10]);
  __m256i cospi_p18_p46 = pair_set_w16_epi16(cospi[18], cospi[46]);
  __m256i cospi_p46_m18 = pair_set_w16_epi16(cospi[46], -cospi[18]);
  __m256i cospi_p26_p38 = pair_set_w16_epi16(cospi[26], cospi[38]);
  __m256i cospi_p38_m26 = pair_set_w16_epi16(cospi[38], -cospi[26]);
  __m256i cospi_p34_p30 = pair_set_w16_epi16(cospi[34], cospi[30]);
  __m256i cospi_p30_m34 = pair_set_w16_epi16(cospi[30], -cospi[34]);
  __m256i cospi_p42_p22 = pair_set_w16_epi16(cospi[42], cospi[22]);
  __m256i cospi_p22_m42 = pair_set_w16_epi16(cospi[22], -cospi[42]);
  __m256i cospi_p50_p14 = pair_set_w16_epi16(cospi[50], cospi[14]);
  __m256i cospi_p14_m50 = pair_set_w16_epi16(cospi[14], -cospi[50]);
  __m256i cospi_p58_p06 = pair_set_w16_epi16(cospi[58], cospi[6]);
  __m256i cospi_p06_m58 = pair_set_w16_epi16(cospi[6], -cospi[58]);

  // stage 1
  __m256i x1[16];
  x1[0] = input[15];
  x1[1] = input[0];
  x1[2] = input[13];
  x1[3] = input[2];
  x1[4] = input[11];
  x1[5] = input[4];
  x1[6] = input[9];
  x1[7] = input[6];
  x1[8] = input[7];
  x1[9] = input[8];
  x1[10] = input[5];
  x1[11] = input[10];
  x1[12] = input[3];
  x1[13] = input[12];
  x1[14] = input[1];
  x1[15] = input[14];

  // stage 2
  btf_16_w16_avx2(cospi_p02_p62, cospi_p62_m02, &x1[0], &x1[1], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p10_p54, cospi_p54_m10, &x1[2], &x1[3], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p18_p46, cospi_p46_m18, &x1[4], &x1[5], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p26_p38, cospi_p38_m26, &x1[6], &x1[7], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p34_p30, cospi_p30_m34, &x1[8], &x1[9], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p42_p22, cospi_p22_m42, &x1[10], &x1[11], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p50_p14, cospi_p14_m50, &x1[12], &x1[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p58_p06, cospi_p06_m58, &x1[14], &x1[15], _r,
                  INV_COS_BIT);

  iadst16_stage3_avx2(x1);
  iadst16_stage4_avx2(x1, cospi, _r, INV_COS_BIT);
  iadst16_stage5_avx2(x1);
  iadst16_stage6_avx2(x1, cospi, _r, INV_COS_BIT);
  iadst16_stage7_avx2(x1);
  iadst16_stage8_avx2(x1, cospi, _r, INV_COS_BIT);
  iadst16_stage9_avx2(output, x1);
}

static void iadst16_low8_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  // stage 1
  __m256i x1[16];
  x1[1] = input[0];
  x1[3] = input[2];
  x1[5] = input[4];
  x1[7] = input[6];
  x1[8] = input[7];
  x1[10] = input[5];
  x1[12] = input[3];
  x1[14] = input[1];

  // stage 2
  btf_16_w16_0_avx2(cospi[62], -cospi[2], x1[1], x1[0], x1[1]);
  btf_16_w16_0_avx2(cospi[54], -cospi[10], x1[3], x1[2], x1[3]);
  btf_16_w16_0_avx2(cospi[46], -cospi[18], x1[5], x1[4], x1[5]);
  btf_16_w16_0_avx2(cospi[38], -cospi[26], x1[7], x1[6], x1[7]);
  btf_16_w16_0_avx2(cospi[34], cospi[30], x1[8], x1[8], x1[9]);
  btf_16_w16_0_avx2(cospi[42], cospi[22], x1[10], x1[10], x1[11]);
  btf_16_w16_0_avx2(cospi[50], cospi[14], x1[12], x1[12], x1[13]);
  btf_16_w16_0_avx2(cospi[58], cospi[06], x1[14], x1[14], x1[15]);

  iadst16_stage3_avx2(x1);
  iadst16_stage4_avx2(x1, cospi, _r, INV_COS_BIT);
  iadst16_stage5_avx2(x1);
  iadst16_stage6_avx2(x1, cospi, _r, INV_COS_BIT);
  iadst16_stage7_avx2(x1);
  iadst16_stage8_avx2(x1, cospi, _r, INV_COS_BIT);
  iadst16_stage9_avx2(output, x1);
}

static void iadst16_low1_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  const __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  const __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);
  const __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);

  // stage 1
  __m256i x1[16];
  x1[1] = input[0];

  // stage 2
  btf_16_w16_0_avx2(cospi[62], -cospi[2], x1[1], x1[0], x1[1]);

  // stage 3
  x1[8] = x1[0];
  x1[9] = x1[1];

  // stage 4
  btf_16_w16_avx2(cospi_p08_p56, cospi_p56_m08, &x1[8], &x1[9], _r,
                  INV_COS_BIT);

  // stage 5
  x1[4] = x1[0];
  x1[5] = x1[1];

  x1[12] = x1[8];
  x1[13] = x1[9];

  // stage 6
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x1[4], &x1[5], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p16_p48, cospi_p48_m16, &x1[12], &x1[13], _r,
                  INV_COS_BIT);

  // stage 7
  x1[2] = x1[0];
  x1[3] = x1[1];
  x1[6] = x1[4];
  x1[7] = x1[5];
  x1[10] = x1[8];
  x1[11] = x1[9];
  x1[14] = x1[12];
  x1[15] = x1[13];

  iadst16_stage8_avx2(x1, cospi, _r, INV_COS_BIT);
  iadst16_stage9_avx2(output, x1);
}

static INLINE void idct32_high16_stage3_avx2(__m256i *x) {
  btf_16_adds_subs_avx2(&x[16], &x[17]);
  btf_16_adds_subs_avx2(&x[19], &x[18]);
  btf_16_adds_subs_avx2(&x[20], &x[21]);
  btf_16_adds_subs_avx2(&x[23], &x[22]);
  btf_16_adds_subs_avx2(&x[24], &x[25]);
  btf_16_adds_subs_avx2(&x[27], &x[26]);
  btf_16_adds_subs_avx2(&x[28], &x[29]);
  btf_16_adds_subs_avx2(&x[31], &x[30]);
}

static INLINE void idct32_high16_stage4_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  const __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  const __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[17], &x[30], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x[18], &x[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x[21], &x[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[22], &x[25], _r, cos_bit);
}

static INLINE void idct32_high24_stage5_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[9], &x[14], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[10], &x[13], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[16], &x[19]);
  btf_16_adds_subs_avx2(&x[17], &x[18]);
  btf_16_adds_subs_avx2(&x[23], &x[20]);
  btf_16_adds_subs_avx2(&x[22], &x[21]);
  btf_16_adds_subs_avx2(&x[24], &x[27]);
  btf_16_adds_subs_avx2(&x[25], &x[26]);
  btf_16_adds_subs_avx2(&x[31], &x[28]);
  btf_16_adds_subs_avx2(&x[30], &x[29]);
}

static INLINE void idct32_high28_stage6_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[5], &x[6], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[8], &x[11]);
  btf_16_adds_subs_avx2(&x[9], &x[10]);
  btf_16_adds_subs_avx2(&x[15], &x[12]);
  btf_16_adds_subs_avx2(&x[14], &x[13]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[18], &x[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[19], &x[28], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[20], &x[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[21], &x[26], _r, cos_bit);
}

static INLINE void idct32_stage7_avx2(__m256i *x, const int32_t *cospi,
                                      const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_avx2(&x[0], &x[7]);
  btf_16_adds_subs_avx2(&x[1], &x[6]);
  btf_16_adds_subs_avx2(&x[2], &x[5]);
  btf_16_adds_subs_avx2(&x[3], &x[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[10], &x[13], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[11], &x[12], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[16], &x[23]);
  btf_16_adds_subs_avx2(&x[17], &x[22]);
  btf_16_adds_subs_avx2(&x[18], &x[21]);
  btf_16_adds_subs_avx2(&x[19], &x[20]);
  btf_16_adds_subs_avx2(&x[31], &x[24]);
  btf_16_adds_subs_avx2(&x[30], &x[25]);
  btf_16_adds_subs_avx2(&x[29], &x[26]);
  btf_16_adds_subs_avx2(&x[28], &x[27]);
}

static INLINE void idct32_stage8_avx2(__m256i *x, const int32_t *cospi,
                                      const __m256i _r, int8_t cos_bit) {
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_avx2(&x[0], &x[15]);
  btf_16_adds_subs_avx2(&x[1], &x[14]);
  btf_16_adds_subs_avx2(&x[2], &x[13]);
  btf_16_adds_subs_avx2(&x[3], &x[12]);
  btf_16_adds_subs_avx2(&x[4], &x[11]);
  btf_16_adds_subs_avx2(&x[5], &x[10]);
  btf_16_adds_subs_avx2(&x[6], &x[9]);
  btf_16_adds_subs_avx2(&x[7], &x[8]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[20], &x[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[21], &x[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[22], &x[25], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[23], &x[24], _r, cos_bit);
}

static INLINE void idct32_stage9_avx2(__m256i *output, __m256i *x) {
  btf_16_adds_subs_out_avx2(&output[0], &output[31], x[0], x[31]);
  btf_16_adds_subs_out_avx2(&output[1], &output[30], x[1], x[30]);
  btf_16_adds_subs_out_avx2(&output[2], &output[29], x[2], x[29]);
  btf_16_adds_subs_out_avx2(&output[3], &output[28], x[3], x[28]);
  btf_16_adds_subs_out_avx2(&output[4], &output[27], x[4], x[27]);
  btf_16_adds_subs_out_avx2(&output[5], &output[26], x[5], x[26]);
  btf_16_adds_subs_out_avx2(&output[6], &output[25], x[6], x[25]);
  btf_16_adds_subs_out_avx2(&output[7], &output[24], x[7], x[24]);
  btf_16_adds_subs_out_avx2(&output[8], &output[23], x[8], x[23]);
  btf_16_adds_subs_out_avx2(&output[9], &output[22], x[9], x[22]);
  btf_16_adds_subs_out_avx2(&output[10], &output[21], x[10], x[21]);
  btf_16_adds_subs_out_avx2(&output[11], &output[20], x[11], x[20]);
  btf_16_adds_subs_out_avx2(&output[12], &output[19], x[12], x[19]);
  btf_16_adds_subs_out_avx2(&output[13], &output[18], x[13], x[18]);
  btf_16_adds_subs_out_avx2(&output[14], &output[17], x[14], x[17]);
  btf_16_adds_subs_out_avx2(&output[15], &output[16], x[15], x[16]);
}

static void idct32_low1_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);

  // stage 1
  __m256i x[2];
  x[0] = input[0];

  // stage 2
  // stage 3
  // stage 4
  // stage 5
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);

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

static void idct32_low8_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  // stage 1
  __m256i x[32];
  x[0] = input[0];
  x[4] = input[4];
  x[8] = input[2];
  x[12] = input[6];
  x[16] = input[1];
  x[20] = input[5];
  x[24] = input[3];
  x[28] = input[7];

  // stage 2
  btf_16_w16_0_avx2(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_w16_0_avx2(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x[24], x[23], x[24]);

  // stage 3
  btf_16_w16_0_avx2(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x[12], x[11], x[12]);
  x[17] = x[16];
  x[18] = x[19];
  x[21] = x[20];
  x[22] = x[23];
  x[25] = x[24];
  x[26] = x[27];
  x[29] = x[28];
  x[30] = x[31];

  // stage 4
  btf_16_w16_0_avx2(cospi[56], cospi[8], x[4], x[4], x[7]);
  x[9] = x[8];
  x[10] = x[11];
  x[13] = x[12];
  x[14] = x[15];
  idct32_high16_stage4_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 5
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);
  x[5] = x[4];
  x[6] = x[7];
  idct32_high24_stage5_avx2(x, cospi, _r, INV_COS_BIT);
  // stage 6
  x[3] = x[0];
  x[2] = x[1];
  idct32_high28_stage6_avx2(x, cospi, _r, INV_COS_BIT);

  idct32_stage7_avx2(x, cospi, _r, INV_COS_BIT);
  idct32_stage8_avx2(x, cospi, _r, INV_COS_BIT);
  idct32_stage9_avx2(output, x);
}

static void idct32_low16_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  // stage 1
  __m256i x[32];
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
  btf_16_w16_0_avx2(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_w16_0_avx2(-cospi[34], cospi[30], x[30], x[17], x[30]);
  btf_16_w16_0_avx2(cospi[46], cospi[18], x[18], x[18], x[29]);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_w16_0_avx2(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_w16_0_avx2(-cospi[42], cospi[22], x[26], x[21], x[26]);
  btf_16_w16_0_avx2(cospi[38], cospi[26], x[22], x[22], x[25]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x[24], x[23], x[24]);

  // stage 3
  btf_16_w16_0_avx2(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_w16_0_avx2(-cospi[36], cospi[28], x[14], x[9], x[14]);
  btf_16_w16_0_avx2(cospi[44], cospi[20], x[10], x[10], x[13]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x[12], x[11], x[12]);
  idct32_high16_stage3_avx2(x);

  // stage 4
  btf_16_w16_0_avx2(cospi[56], cospi[8], x[4], x[4], x[7]);
  btf_16_w16_0_avx2(-cospi[40], cospi[24], x[6], x[5], x[6]);
  btf_16_adds_subs_avx2(&x[8], &x[9]);
  btf_16_adds_subs_avx2(&x[11], &x[10]);
  btf_16_adds_subs_avx2(&x[12], &x[13]);
  btf_16_adds_subs_avx2(&x[15], &x[14]);
  idct32_high16_stage4_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 5
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_w16_0_avx2(cospi[48], cospi[16], x[2], x[2], x[3]);
  btf_16_adds_subs_avx2(&x[4], &x[5]);
  btf_16_adds_subs_avx2(&x[7], &x[6]);
  idct32_high24_stage5_avx2(x, cospi, _r, INV_COS_BIT);

  btf_16_adds_subs_avx2(&x[0], &x[3]);
  btf_16_adds_subs_avx2(&x[1], &x[2]);
  idct32_high28_stage6_avx2(x, cospi, _r, INV_COS_BIT);

  idct32_stage7_avx2(x, cospi, _r, INV_COS_BIT);
  idct32_stage8_avx2(x, cospi, _r, INV_COS_BIT);
  idct32_stage9_avx2(output, x);
}

static void idct32_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  __m256i cospi_p62_m02 = pair_set_w16_epi16(cospi[62], -cospi[2]);
  __m256i cospi_p02_p62 = pair_set_w16_epi16(cospi[2], cospi[62]);
  __m256i cospi_p30_m34 = pair_set_w16_epi16(cospi[30], -cospi[34]);
  __m256i cospi_p34_p30 = pair_set_w16_epi16(cospi[34], cospi[30]);
  __m256i cospi_p46_m18 = pair_set_w16_epi16(cospi[46], -cospi[18]);
  __m256i cospi_p18_p46 = pair_set_w16_epi16(cospi[18], cospi[46]);
  __m256i cospi_p14_m50 = pair_set_w16_epi16(cospi[14], -cospi[50]);
  __m256i cospi_p50_p14 = pair_set_w16_epi16(cospi[50], cospi[14]);
  __m256i cospi_p54_m10 = pair_set_w16_epi16(cospi[54], -cospi[10]);
  __m256i cospi_p10_p54 = pair_set_w16_epi16(cospi[10], cospi[54]);
  __m256i cospi_p22_m42 = pair_set_w16_epi16(cospi[22], -cospi[42]);
  __m256i cospi_p42_p22 = pair_set_w16_epi16(cospi[42], cospi[22]);
  __m256i cospi_p38_m26 = pair_set_w16_epi16(cospi[38], -cospi[26]);
  __m256i cospi_p26_p38 = pair_set_w16_epi16(cospi[26], cospi[38]);
  __m256i cospi_p06_m58 = pair_set_w16_epi16(cospi[6], -cospi[58]);
  __m256i cospi_p58_p06 = pair_set_w16_epi16(cospi[58], cospi[6]);
  __m256i cospi_p60_m04 = pair_set_w16_epi16(cospi[60], -cospi[4]);
  __m256i cospi_p04_p60 = pair_set_w16_epi16(cospi[4], cospi[60]);
  __m256i cospi_p28_m36 = pair_set_w16_epi16(cospi[28], -cospi[36]);
  __m256i cospi_p36_p28 = pair_set_w16_epi16(cospi[36], cospi[28]);
  __m256i cospi_p44_m20 = pair_set_w16_epi16(cospi[44], -cospi[20]);
  __m256i cospi_p20_p44 = pair_set_w16_epi16(cospi[20], cospi[44]);
  __m256i cospi_p12_m52 = pair_set_w16_epi16(cospi[12], -cospi[52]);
  __m256i cospi_p52_p12 = pair_set_w16_epi16(cospi[52], cospi[12]);
  __m256i cospi_p56_m08 = pair_set_w16_epi16(cospi[56], -cospi[8]);
  __m256i cospi_p08_p56 = pair_set_w16_epi16(cospi[8], cospi[56]);
  __m256i cospi_p24_m40 = pair_set_w16_epi16(cospi[24], -cospi[40]);
  __m256i cospi_p40_p24 = pair_set_w16_epi16(cospi[40], cospi[24]);
  __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  __m256i cospi_p32_m32 = pair_set_w16_epi16(cospi[32], -cospi[32]);
  __m256i cospi_p48_m16 = pair_set_w16_epi16(cospi[48], -cospi[16]);
  __m256i cospi_p16_p48 = pair_set_w16_epi16(cospi[16], cospi[48]);

  // stage 1
  __m256i x1[32];
  x1[0] = input[0];
  x1[1] = input[16];
  x1[2] = input[8];
  x1[3] = input[24];
  x1[4] = input[4];
  x1[5] = input[20];
  x1[6] = input[12];
  x1[7] = input[28];
  x1[8] = input[2];
  x1[9] = input[18];
  x1[10] = input[10];
  x1[11] = input[26];
  x1[12] = input[6];
  x1[13] = input[22];
  x1[14] = input[14];
  x1[15] = input[30];
  x1[16] = input[1];
  x1[17] = input[17];
  x1[18] = input[9];
  x1[19] = input[25];
  x1[20] = input[5];
  x1[21] = input[21];
  x1[22] = input[13];
  x1[23] = input[29];
  x1[24] = input[3];
  x1[25] = input[19];
  x1[26] = input[11];
  x1[27] = input[27];
  x1[28] = input[7];
  x1[29] = input[23];
  x1[30] = input[15];
  x1[31] = input[31];

  // stage 2
  btf_16_w16_avx2(cospi_p62_m02, cospi_p02_p62, &x1[16], &x1[31], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p30_m34, cospi_p34_p30, &x1[17], &x1[30], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p46_m18, cospi_p18_p46, &x1[18], &x1[29], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p14_m50, cospi_p50_p14, &x1[19], &x1[28], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p54_m10, cospi_p10_p54, &x1[20], &x1[27], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p22_m42, cospi_p42_p22, &x1[21], &x1[26], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p38_m26, cospi_p26_p38, &x1[22], &x1[25], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p06_m58, cospi_p58_p06, &x1[23], &x1[24], _r,
                  INV_COS_BIT);

  // stage 3
  btf_16_w16_avx2(cospi_p60_m04, cospi_p04_p60, &x1[8], &x1[15], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p28_m36, cospi_p36_p28, &x1[9], &x1[14], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p44_m20, cospi_p20_p44, &x1[10], &x1[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p12_m52, cospi_p52_p12, &x1[11], &x1[12], _r,
                  INV_COS_BIT);
  idct32_high16_stage3_avx2(x1);

  // stage 4
  btf_16_w16_avx2(cospi_p56_m08, cospi_p08_p56, &x1[4], &x1[7], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p24_m40, cospi_p40_p24, &x1[5], &x1[6], _r,
                  INV_COS_BIT);
  btf_16_adds_subs_avx2(&x1[8], &x1[9]);
  btf_16_adds_subs_avx2(&x1[11], &x1[10]);
  btf_16_adds_subs_avx2(&x1[12], &x1[13]);
  btf_16_adds_subs_avx2(&x1[15], &x1[14]);
  idct32_high16_stage4_avx2(x1, cospi, _r, INV_COS_BIT);

  // stage 5
  btf_16_w16_avx2(cospi_p32_p32, cospi_p32_m32, &x1[0], &x1[1], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_p48_m16, cospi_p16_p48, &x1[2], &x1[3], _r,
                  INV_COS_BIT);
  btf_16_adds_subs_avx2(&x1[4], &x1[5]);
  btf_16_adds_subs_avx2(&x1[7], &x1[6]);
  idct32_high24_stage5_avx2(x1, cospi, _r, INV_COS_BIT);

  // stage 6
  btf_16_adds_subs_avx2(&x1[0], &x1[3]);
  btf_16_adds_subs_avx2(&x1[1], &x1[2]);
  idct32_high28_stage6_avx2(x1, cospi, _r, INV_COS_BIT);

  idct32_stage7_avx2(x1, cospi, _r, INV_COS_BIT);
  idct32_stage8_avx2(x1, cospi, _r, INV_COS_BIT);
  idct32_stage9_avx2(output, x1);
}

static INLINE void idct64_stage4_high32_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m04_p60 = pair_set_w16_epi16(-cospi[4], cospi[60]);
  const __m256i cospi_p60_p04 = pair_set_w16_epi16(cospi[60], cospi[4]);
  const __m256i cospi_m60_m04 = pair_set_w16_epi16(-cospi[60], -cospi[4]);
  const __m256i cospi_m36_p28 = pair_set_w16_epi16(-cospi[36], cospi[28]);
  const __m256i cospi_p28_p36 = pair_set_w16_epi16(cospi[28], cospi[36]);
  const __m256i cospi_m28_m36 = pair_set_w16_epi16(-cospi[28], -cospi[36]);
  const __m256i cospi_m20_p44 = pair_set_w16_epi16(-cospi[20], cospi[44]);
  const __m256i cospi_p44_p20 = pair_set_w16_epi16(cospi[44], cospi[20]);
  const __m256i cospi_m44_m20 = pair_set_w16_epi16(-cospi[44], -cospi[20]);
  const __m256i cospi_m52_p12 = pair_set_w16_epi16(-cospi[52], cospi[12]);
  const __m256i cospi_p12_p52 = pair_set_w16_epi16(cospi[12], cospi[52]);
  const __m256i cospi_m12_m52 = pair_set_w16_epi16(-cospi[12], -cospi[52]);
  btf_16_w16_avx2(cospi_m04_p60, cospi_p60_p04, &x[33], &x[62], _r, cos_bit);
  btf_16_w16_avx2(cospi_m60_m04, cospi_m04_p60, &x[34], &x[61], _r, cos_bit);
  btf_16_w16_avx2(cospi_m36_p28, cospi_p28_p36, &x[37], &x[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m28_m36, cospi_m36_p28, &x[38], &x[57], _r, cos_bit);
  btf_16_w16_avx2(cospi_m20_p44, cospi_p44_p20, &x[41], &x[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m44_m20, cospi_m20_p44, &x[42], &x[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m52_p12, cospi_p12_p52, &x[45], &x[50], _r, cos_bit);
  btf_16_w16_avx2(cospi_m12_m52, cospi_m52_p12, &x[46], &x[49], _r, cos_bit);
}

static INLINE void idct64_stage5_high48_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  const __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  const __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[17], &x[30], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x[18], &x[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x[21], &x[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[22], &x[25], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[32], &x[35]);
  btf_16_adds_subs_avx2(&x[33], &x[34]);
  btf_16_adds_subs_avx2(&x[39], &x[36]);
  btf_16_adds_subs_avx2(&x[38], &x[37]);
  btf_16_adds_subs_avx2(&x[40], &x[43]);
  btf_16_adds_subs_avx2(&x[41], &x[42]);
  btf_16_adds_subs_avx2(&x[47], &x[44]);
  btf_16_adds_subs_avx2(&x[46], &x[45]);
  btf_16_adds_subs_avx2(&x[48], &x[51]);
  btf_16_adds_subs_avx2(&x[49], &x[50]);
  btf_16_adds_subs_avx2(&x[55], &x[52]);
  btf_16_adds_subs_avx2(&x[54], &x[53]);
  btf_16_adds_subs_avx2(&x[56], &x[59]);
  btf_16_adds_subs_avx2(&x[57], &x[58]);
  btf_16_adds_subs_avx2(&x[63], &x[60]);
  btf_16_adds_subs_avx2(&x[62], &x[61]);
}

static INLINE void idct64_stage6_high32_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  const __m256i cospi_m56_m08 = pair_set_w16_epi16(-cospi[56], -cospi[8]);
  const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  const __m256i cospi_p24_p40 = pair_set_w16_epi16(cospi[24], cospi[40]);
  const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[34], &x[61], _r, cos_bit);
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[35], &x[60], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x[36], &x[59], _r, cos_bit);
  btf_16_w16_avx2(cospi_m56_m08, cospi_m08_p56, &x[37], &x[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x[42], &x[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m40_p24, cospi_p24_p40, &x[43], &x[52], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[44], &x[51], _r, cos_bit);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[45], &x[50], _r, cos_bit);
}

static INLINE void idct64_stage6_high48_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  btf_16_adds_subs_avx2(&x[16], &x[19]);
  btf_16_adds_subs_avx2(&x[17], &x[18]);
  btf_16_adds_subs_avx2(&x[23], &x[20]);
  btf_16_adds_subs_avx2(&x[22], &x[21]);
  btf_16_adds_subs_avx2(&x[24], &x[27]);
  btf_16_adds_subs_avx2(&x[25], &x[26]);
  btf_16_adds_subs_avx2(&x[31], &x[28]);
  btf_16_adds_subs_avx2(&x[30], &x[29]);
  idct64_stage6_high32_avx2(x, cospi, _r, cos_bit);
}

static INLINE void idct64_stage7_high48_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[18], &x[29], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[19], &x[28], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[20], &x[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[21], &x[26], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[32], &x[39]);
  btf_16_adds_subs_avx2(&x[33], &x[38]);
  btf_16_adds_subs_avx2(&x[34], &x[37]);
  btf_16_adds_subs_avx2(&x[35], &x[36]);
  btf_16_adds_subs_avx2(&x[47], &x[40]);
  btf_16_adds_subs_avx2(&x[46], &x[41]);
  btf_16_adds_subs_avx2(&x[45], &x[42]);
  btf_16_adds_subs_avx2(&x[44], &x[43]);
  btf_16_adds_subs_avx2(&x[48], &x[55]);
  btf_16_adds_subs_avx2(&x[49], &x[54]);
  btf_16_adds_subs_avx2(&x[50], &x[53]);
  btf_16_adds_subs_avx2(&x[51], &x[52]);
  btf_16_adds_subs_avx2(&x[63], &x[56]);
  btf_16_adds_subs_avx2(&x[62], &x[57]);
  btf_16_adds_subs_avx2(&x[61], &x[58]);
  btf_16_adds_subs_avx2(&x[60], &x[59]);
}

static INLINE void idct64_stage8_high48_avx2(__m256i *x, const int32_t *cospi,
                                             const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  btf_16_adds_subs_avx2(&x[16], &x[23]);
  btf_16_adds_subs_avx2(&x[17], &x[22]);
  btf_16_adds_subs_avx2(&x[18], &x[21]);
  btf_16_adds_subs_avx2(&x[19], &x[20]);
  btf_16_adds_subs_avx2(&x[31], &x[24]);
  btf_16_adds_subs_avx2(&x[30], &x[25]);
  btf_16_adds_subs_avx2(&x[29], &x[26]);
  btf_16_adds_subs_avx2(&x[28], &x[27]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[36], &x[59], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[37], &x[58], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[38], &x[57], _r, cos_bit);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[39], &x[56], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[40], &x[55], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[41], &x[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[42], &x[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[43], &x[52], _r, cos_bit);
}

static INLINE void idct64_stage9_avx2(__m256i *x, const int32_t *cospi,
                                      const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_avx2(&x[0], &x[15]);
  btf_16_adds_subs_avx2(&x[1], &x[14]);
  btf_16_adds_subs_avx2(&x[2], &x[13]);
  btf_16_adds_subs_avx2(&x[3], &x[12]);
  btf_16_adds_subs_avx2(&x[4], &x[11]);
  btf_16_adds_subs_avx2(&x[5], &x[10]);
  btf_16_adds_subs_avx2(&x[6], &x[9]);
  btf_16_adds_subs_avx2(&x[7], &x[8]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[20], &x[27], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[21], &x[26], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[22], &x[25], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[23], &x[24], _r, cos_bit);
  btf_16_adds_subs_avx2(&x[32], &x[47]);
  btf_16_adds_subs_avx2(&x[33], &x[46]);
  btf_16_adds_subs_avx2(&x[34], &x[45]);
  btf_16_adds_subs_avx2(&x[35], &x[44]);
  btf_16_adds_subs_avx2(&x[36], &x[43]);
  btf_16_adds_subs_avx2(&x[37], &x[42]);
  btf_16_adds_subs_avx2(&x[38], &x[41]);
  btf_16_adds_subs_avx2(&x[39], &x[40]);
  btf_16_adds_subs_avx2(&x[63], &x[48]);
  btf_16_adds_subs_avx2(&x[62], &x[49]);
  btf_16_adds_subs_avx2(&x[61], &x[50]);
  btf_16_adds_subs_avx2(&x[60], &x[51]);
  btf_16_adds_subs_avx2(&x[59], &x[52]);
  btf_16_adds_subs_avx2(&x[58], &x[53]);
  btf_16_adds_subs_avx2(&x[57], &x[54]);
  btf_16_adds_subs_avx2(&x[56], &x[55]);
}

static INLINE void idct64_stage10_avx2(__m256i *x, const int32_t *cospi,
                                       const __m256i _r, int8_t cos_bit) {
  (void)cos_bit;
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  btf_16_adds_subs_avx2(&x[0], &x[31]);
  btf_16_adds_subs_avx2(&x[1], &x[30]);
  btf_16_adds_subs_avx2(&x[2], &x[29]);
  btf_16_adds_subs_avx2(&x[3], &x[28]);
  btf_16_adds_subs_avx2(&x[4], &x[27]);
  btf_16_adds_subs_avx2(&x[5], &x[26]);
  btf_16_adds_subs_avx2(&x[6], &x[25]);
  btf_16_adds_subs_avx2(&x[7], &x[24]);
  btf_16_adds_subs_avx2(&x[8], &x[23]);
  btf_16_adds_subs_avx2(&x[9], &x[22]);
  btf_16_adds_subs_avx2(&x[10], &x[21]);
  btf_16_adds_subs_avx2(&x[11], &x[20]);
  btf_16_adds_subs_avx2(&x[12], &x[19]);
  btf_16_adds_subs_avx2(&x[13], &x[18]);
  btf_16_adds_subs_avx2(&x[14], &x[17]);
  btf_16_adds_subs_avx2(&x[15], &x[16]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[40], &x[55], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[41], &x[54], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[42], &x[53], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[43], &x[52], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[44], &x[51], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[45], &x[50], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[46], &x[49], _r, cos_bit);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[47], &x[48], _r, cos_bit);
}

static INLINE void idct64_stage11_avx2(__m256i *output, __m256i *x) {
  btf_16_adds_subs_out_avx2(&output[0], &output[63], x[0], x[63]);
  btf_16_adds_subs_out_avx2(&output[1], &output[62], x[1], x[62]);
  btf_16_adds_subs_out_avx2(&output[2], &output[61], x[2], x[61]);
  btf_16_adds_subs_out_avx2(&output[3], &output[60], x[3], x[60]);
  btf_16_adds_subs_out_avx2(&output[4], &output[59], x[4], x[59]);
  btf_16_adds_subs_out_avx2(&output[5], &output[58], x[5], x[58]);
  btf_16_adds_subs_out_avx2(&output[6], &output[57], x[6], x[57]);
  btf_16_adds_subs_out_avx2(&output[7], &output[56], x[7], x[56]);
  btf_16_adds_subs_out_avx2(&output[8], &output[55], x[8], x[55]);
  btf_16_adds_subs_out_avx2(&output[9], &output[54], x[9], x[54]);
  btf_16_adds_subs_out_avx2(&output[10], &output[53], x[10], x[53]);
  btf_16_adds_subs_out_avx2(&output[11], &output[52], x[11], x[52]);
  btf_16_adds_subs_out_avx2(&output[12], &output[51], x[12], x[51]);
  btf_16_adds_subs_out_avx2(&output[13], &output[50], x[13], x[50]);
  btf_16_adds_subs_out_avx2(&output[14], &output[49], x[14], x[49]);
  btf_16_adds_subs_out_avx2(&output[15], &output[48], x[15], x[48]);
  btf_16_adds_subs_out_avx2(&output[16], &output[47], x[16], x[47]);
  btf_16_adds_subs_out_avx2(&output[17], &output[46], x[17], x[46]);
  btf_16_adds_subs_out_avx2(&output[18], &output[45], x[18], x[45]);
  btf_16_adds_subs_out_avx2(&output[19], &output[44], x[19], x[44]);
  btf_16_adds_subs_out_avx2(&output[20], &output[43], x[20], x[43]);
  btf_16_adds_subs_out_avx2(&output[21], &output[42], x[21], x[42]);
  btf_16_adds_subs_out_avx2(&output[22], &output[41], x[22], x[41]);
  btf_16_adds_subs_out_avx2(&output[23], &output[40], x[23], x[40]);
  btf_16_adds_subs_out_avx2(&output[24], &output[39], x[24], x[39]);
  btf_16_adds_subs_out_avx2(&output[25], &output[38], x[25], x[38]);
  btf_16_adds_subs_out_avx2(&output[26], &output[37], x[26], x[37]);
  btf_16_adds_subs_out_avx2(&output[27], &output[36], x[27], x[36]);
  btf_16_adds_subs_out_avx2(&output[28], &output[35], x[28], x[35]);
  btf_16_adds_subs_out_avx2(&output[29], &output[34], x[29], x[34]);
  btf_16_adds_subs_out_avx2(&output[30], &output[33], x[30], x[33]);
  btf_16_adds_subs_out_avx2(&output[31], &output[32], x[31], x[32]);
}

static void idct64_low1_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);

  // stage 1
  __m256i x[32];
  x[0] = input[0];

  // stage 2
  // stage 3
  // stage 4
  // stage 5
  // stage 6
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);

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

static void idct64_low8_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));
  const __m256i cospi_m04_p60 = pair_set_w16_epi16(-cospi[4], cospi[60]);
  const __m256i cospi_p60_p04 = pair_set_w16_epi16(cospi[60], cospi[4]);
  const __m256i cospi_m36_p28 = pair_set_w16_epi16(-cospi[36], cospi[28]);
  const __m256i cospi_m28_m36 = pair_set_w16_epi16(-cospi[28], -cospi[36]);
  const __m256i cospi_m20_p44 = pair_set_w16_epi16(-cospi[20], cospi[44]);
  const __m256i cospi_p44_p20 = pair_set_w16_epi16(cospi[44], cospi[20]);
  const __m256i cospi_m52_p12 = pair_set_w16_epi16(-cospi[52], cospi[12]);
  const __m256i cospi_m12_m52 = pair_set_w16_epi16(-cospi[12], -cospi[52]);
  const __m256i cospi_m08_p56 = pair_set_w16_epi16(-cospi[8], cospi[56]);
  const __m256i cospi_p56_p08 = pair_set_w16_epi16(cospi[56], cospi[8]);
  const __m256i cospi_m40_p24 = pair_set_w16_epi16(-cospi[40], cospi[24]);
  const __m256i cospi_m24_m40 = pair_set_w16_epi16(-cospi[24], -cospi[40]);
  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x[64];
  x[0] = input[0];
  x[8] = input[4];
  x[16] = input[2];
  x[24] = input[6];
  x[32] = input[1];
  x[40] = input[5];
  x[48] = input[3];
  x[56] = input[7];

  // stage 2
  btf_16_w16_0_avx2(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_w16_0_avx2(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_w16_0_avx2(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_w16_0_avx2(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_w16_0_avx2(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x[24], x[23], x[24]);
  x[33] = x[32];
  x[38] = x[39];
  x[41] = x[40];
  x[46] = x[47];
  x[49] = x[48];
  x[54] = x[55];
  x[57] = x[56];
  x[62] = x[63];

  // stage 4
  btf_16_w16_0_avx2(cospi[60], cospi[4], x[8], x[8], x[15]);
  x[17] = x[16];
  x[22] = x[23];
  x[25] = x[24];
  x[30] = x[31];
  btf_16_w16_avx2(cospi_m04_p60, cospi_p60_p04, &x[33], &x[62], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m28_m36, cospi_m36_p28, &x[38], &x[57], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m20_p44, cospi_p44_p20, &x[41], &x[54], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m12_m52, cospi_m52_p12, &x[46], &x[49], _r,
                  INV_COS_BIT);

  // stage 5
  x[9] = x[8];
  x[14] = x[15];
  btf_16_w16_avx2(cospi_m08_p56, cospi_p56_p08, &x[17], &x[30], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m24_m40, cospi_m40_p24, &x[22], &x[25], _r,
                  INV_COS_BIT);
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
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[9], &x[14], _r, INV_COS_BIT);
  x[19] = x[16];
  x[18] = x[17];
  x[20] = x[23];
  x[21] = x[22];
  x[27] = x[24];
  x[26] = x[25];
  x[28] = x[31];
  x[29] = x[30];
  idct64_stage6_high32_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 7
  x[3] = x[0];
  x[2] = x[1];
  x[11] = x[8];
  x[10] = x[9];
  x[12] = x[15];
  x[13] = x[14];
  idct64_stage7_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 8
  x[7] = x[0];
  x[6] = x[1];
  x[5] = x[2];
  x[4] = x[3];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[10], &x[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[11], &x[12], _r,
                  INV_COS_BIT);
  idct64_stage8_high48_avx2(x, cospi, _r, INV_COS_BIT);

  idct64_stage9_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage10_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage11_avx2(output, x);
}

static void idct64_low16_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x[64];
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
  btf_16_w16_0_avx2(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_w16_0_avx2(-cospi[49], cospi[15], x[60], x[35], x[60]);
  btf_16_w16_0_avx2(cospi[55], cospi[9], x[36], x[36], x[59]);
  btf_16_w16_0_avx2(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_w16_0_avx2(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_w16_0_avx2(-cospi[53], cospi[11], x[52], x[43], x[52]);
  btf_16_w16_0_avx2(cospi[51], cospi[13], x[44], x[44], x[51]);
  btf_16_w16_0_avx2(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_w16_0_avx2(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_w16_0_avx2(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x[24], x[23], x[24]);
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
  btf_16_w16_0_avx2(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x[12], x[11], x[12]);
  x[17] = x[16];
  x[18] = x[19];
  x[21] = x[20];
  x[22] = x[23];
  x[25] = x[24];
  x[26] = x[27];
  x[29] = x[28];
  x[30] = x[31];
  idct64_stage4_high32_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 5
  btf_16_w16_0_avx2(cospi[56], cospi[8], x[4], x[4], x[7]);
  x[9] = x[8];
  x[10] = x[11];
  x[13] = x[12];
  x[14] = x[15];
  idct64_stage5_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 6
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);
  x[5] = x[4];
  x[6] = x[7];
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[9], &x[14], _r, INV_COS_BIT);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[10], &x[13], _r,
                  INV_COS_BIT);
  idct64_stage6_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 7
  x[3] = x[0];
  x[2] = x[1];
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[5], &x[6], _r, INV_COS_BIT);
  btf_16_adds_subs_avx2(&x[8], &x[11]);
  btf_16_adds_subs_avx2(&x[9], &x[10]);
  btf_16_adds_subs_avx2(&x[15], &x[12]);
  btf_16_adds_subs_avx2(&x[14], &x[13]);
  idct64_stage7_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 8
  btf_16_adds_subs_avx2(&x[0], &x[7]);
  btf_16_adds_subs_avx2(&x[1], &x[6]);
  btf_16_adds_subs_avx2(&x[2], &x[5]);
  btf_16_adds_subs_avx2(&x[3], &x[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[10], &x[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[11], &x[12], _r,
                  INV_COS_BIT);
  idct64_stage8_high48_avx2(x, cospi, _r, INV_COS_BIT);

  idct64_stage9_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage10_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage11_avx2(output, x);
}

static void idct64_low32_avx2(const __m256i *input, __m256i *output) {
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m256i _r = _mm256_set1_epi32(1 << (INV_COS_BIT - 1));

  const __m256i cospi_p32_p32 = pair_set_w16_epi16(cospi[32], cospi[32]);
  const __m256i cospi_m16_p48 = pair_set_w16_epi16(-cospi[16], cospi[48]);
  const __m256i cospi_p48_p16 = pair_set_w16_epi16(cospi[48], cospi[16]);
  const __m256i cospi_m48_m16 = pair_set_w16_epi16(-cospi[48], -cospi[16]);
  const __m256i cospi_m32_p32 = pair_set_w16_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m256i x[64];
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
  btf_16_w16_0_avx2(cospi[63], cospi[1], x[32], x[32], x[63]);
  btf_16_w16_0_avx2(-cospi[33], cospi[31], x[62], x[33], x[62]);
  btf_16_w16_0_avx2(cospi[47], cospi[17], x[34], x[34], x[61]);
  btf_16_w16_0_avx2(-cospi[49], cospi[15], x[60], x[35], x[60]);
  btf_16_w16_0_avx2(cospi[55], cospi[9], x[36], x[36], x[59]);
  btf_16_w16_0_avx2(-cospi[41], cospi[23], x[58], x[37], x[58]);
  btf_16_w16_0_avx2(cospi[39], cospi[25], x[38], x[38], x[57]);
  btf_16_w16_0_avx2(-cospi[57], cospi[7], x[56], x[39], x[56]);
  btf_16_w16_0_avx2(cospi[59], cospi[5], x[40], x[40], x[55]);
  btf_16_w16_0_avx2(-cospi[37], cospi[27], x[54], x[41], x[54]);
  btf_16_w16_0_avx2(cospi[43], cospi[21], x[42], x[42], x[53]);
  btf_16_w16_0_avx2(-cospi[53], cospi[11], x[52], x[43], x[52]);
  btf_16_w16_0_avx2(cospi[51], cospi[13], x[44], x[44], x[51]);
  btf_16_w16_0_avx2(-cospi[45], cospi[19], x[50], x[45], x[50]);
  btf_16_w16_0_avx2(cospi[35], cospi[29], x[46], x[46], x[49]);
  btf_16_w16_0_avx2(-cospi[61], cospi[3], x[48], x[47], x[48]);

  // stage 3
  btf_16_w16_0_avx2(cospi[62], cospi[2], x[16], x[16], x[31]);
  btf_16_w16_0_avx2(-cospi[34], cospi[30], x[30], x[17], x[30]);
  btf_16_w16_0_avx2(cospi[46], cospi[18], x[18], x[18], x[29]);
  btf_16_w16_0_avx2(-cospi[50], cospi[14], x[28], x[19], x[28]);
  btf_16_w16_0_avx2(cospi[54], cospi[10], x[20], x[20], x[27]);
  btf_16_w16_0_avx2(-cospi[42], cospi[22], x[26], x[21], x[26]);
  btf_16_w16_0_avx2(cospi[38], cospi[26], x[22], x[22], x[25]);
  btf_16_w16_0_avx2(-cospi[58], cospi[6], x[24], x[23], x[24]);
  btf_16_adds_subs_avx2(&x[32], &x[33]);
  btf_16_adds_subs_avx2(&x[35], &x[34]);
  btf_16_adds_subs_avx2(&x[36], &x[37]);
  btf_16_adds_subs_avx2(&x[39], &x[38]);
  btf_16_adds_subs_avx2(&x[40], &x[41]);
  btf_16_adds_subs_avx2(&x[43], &x[42]);
  btf_16_adds_subs_avx2(&x[44], &x[45]);
  btf_16_adds_subs_avx2(&x[47], &x[46]);
  btf_16_adds_subs_avx2(&x[48], &x[49]);
  btf_16_adds_subs_avx2(&x[51], &x[50]);
  btf_16_adds_subs_avx2(&x[52], &x[53]);
  btf_16_adds_subs_avx2(&x[55], &x[54]);
  btf_16_adds_subs_avx2(&x[56], &x[57]);
  btf_16_adds_subs_avx2(&x[59], &x[58]);
  btf_16_adds_subs_avx2(&x[60], &x[61]);
  btf_16_adds_subs_avx2(&x[63], &x[62]);

  // stage 4
  btf_16_w16_0_avx2(cospi[60], cospi[4], x[8], x[8], x[15]);
  btf_16_w16_0_avx2(-cospi[36], cospi[28], x[14], x[9], x[14]);
  btf_16_w16_0_avx2(cospi[44], cospi[20], x[10], x[10], x[13]);
  btf_16_w16_0_avx2(-cospi[52], cospi[12], x[12], x[11], x[12]);
  btf_16_adds_subs_avx2(&x[16], &x[17]);
  btf_16_adds_subs_avx2(&x[19], &x[18]);
  btf_16_adds_subs_avx2(&x[20], &x[21]);
  btf_16_adds_subs_avx2(&x[23], &x[22]);
  btf_16_adds_subs_avx2(&x[24], &x[25]);
  btf_16_adds_subs_avx2(&x[27], &x[26]);
  btf_16_adds_subs_avx2(&x[28], &x[29]);
  btf_16_adds_subs_avx2(&x[31], &x[30]);
  idct64_stage4_high32_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 5
  btf_16_w16_0_avx2(cospi[56], cospi[8], x[4], x[4], x[7]);
  btf_16_w16_0_avx2(-cospi[40], cospi[24], x[6], x[5], x[6]);
  btf_16_adds_subs_avx2(&x[8], &x[9]);
  btf_16_adds_subs_avx2(&x[11], &x[10]);
  btf_16_adds_subs_avx2(&x[12], &x[13]);
  btf_16_adds_subs_avx2(&x[15], &x[14]);
  idct64_stage5_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 6
  btf_16_w16_0_avx2(cospi[32], cospi[32], x[0], x[0], x[1]);
  btf_16_w16_0_avx2(cospi[48], cospi[16], x[2], x[2], x[3]);
  btf_16_adds_subs_avx2(&x[4], &x[5]);
  btf_16_adds_subs_avx2(&x[7], &x[6]);
  btf_16_w16_avx2(cospi_m16_p48, cospi_p48_p16, &x[9], &x[14], _r, INV_COS_BIT);
  btf_16_w16_avx2(cospi_m48_m16, cospi_m16_p48, &x[10], &x[13], _r,
                  INV_COS_BIT);
  idct64_stage6_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 7
  btf_16_adds_subs_avx2(&x[0], &x[3]);
  btf_16_adds_subs_avx2(&x[1], &x[2]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[5], &x[6], _r, INV_COS_BIT);
  btf_16_adds_subs_avx2(&x[8], &x[11]);
  btf_16_adds_subs_avx2(&x[9], &x[10]);
  btf_16_adds_subs_avx2(&x[15], &x[12]);
  btf_16_adds_subs_avx2(&x[14], &x[13]);
  idct64_stage7_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 8
  btf_16_adds_subs_avx2(&x[0], &x[7]);
  btf_16_adds_subs_avx2(&x[1], &x[6]);
  btf_16_adds_subs_avx2(&x[2], &x[5]);
  btf_16_adds_subs_avx2(&x[3], &x[4]);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[10], &x[13], _r,
                  INV_COS_BIT);
  btf_16_w16_avx2(cospi_m32_p32, cospi_p32_p32, &x[11], &x[12], _r,
                  INV_COS_BIT);
  idct64_stage8_high48_avx2(x, cospi, _r, INV_COS_BIT);

  // stage 9~11
  idct64_stage9_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage10_avx2(x, cospi, _r, INV_COS_BIT);
  idct64_stage11_avx2(output, x);
}

typedef void (*transform_1d_avx2)(const __m256i *input, __m256i *output);

// 1D functions process 16 pixels at one time.
static const transform_1d_avx2
    lowbd_txfm_all_1d_zeros_w16_arr[TX_SIZES][ITX_TYPES_1D][4] = {
      {
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },
      {
          { idct16_low1_avx2, idct16_low8_avx2, idct16_avx2, NULL },
          { iadst16_low1_avx2, iadst16_low8_avx2, iadst16_avx2, NULL },
          { NULL, NULL, NULL, NULL },
      },
      { { idct32_low1_avx2, idct32_low8_avx2, idct32_low16_avx2, idct32_avx2 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } },
      { { idct64_low1_avx2, idct64_low8_avx2, idct64_low16_avx2,
          idct64_low32_avx2 },
        { NULL, NULL, NULL, NULL },
        { NULL, NULL, NULL, NULL } }
    };

// only process w >= 16 h >= 16
static INLINE void lowbd_inv_txfm2d_add_no_identity_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  __m256i buf1[64 * 16];
  int eobx, eoby;
  get_eobx_eoby_scan_default(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div16 = txfm_size_col >> 4;
  const int buf_size_nonzero_w_div16 = (eobx + 16) >> 4;
  const int buf_size_nonzero_h_div16 = (eoby + 16) >> 4;
  const int input_stride = AOMMIN(32, txfm_size_col);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_avx2 row_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];
  const transform_1d_avx2 col_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const __m256i scale0 = _mm256_set1_epi16(1 << (15 + shift[0]));
  for (int i = 0; i < buf_size_nonzero_h_div16; i++) {
    __m256i buf0[64];
    const int32_t *input_row = input + (i << 4) * input_stride;
    for (int j = 0; j < buf_size_nonzero_w_div16; ++j) {
      __m256i *buf0_cur = buf0 + j * 16;
      const int32_t *input_cur = input_row + j * 16;
      load_buffer_32bit_to_16bit_w16_avx2(input_cur, input_stride, buf0_cur,
                                          16);
      transpose_16bit_16x16_avx2(buf0_cur, buf0_cur);
    }
    if (rect_type == 1 || rect_type == -1) {
      round_shift_avx2(buf0, buf0, input_stride);  // rect special code
    }
    row_txfm(buf0, buf0);
    for (int j = 0; j < txfm_size_col; ++j) {
      buf0[j] = _mm256_mulhrs_epi16(buf0[j], scale0);
    }

    __m256i *buf1_cur = buf1 + (i << 4);
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        __m256i temp[16];
        flip_buf_avx2(buf0 + 16 * j, temp, 16);
        int offset = txfm_size_row * (buf_size_w_div16 - 1 - j);
        transpose_16bit_16x16_avx2(temp, buf1_cur + offset);
      }
    } else {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        transpose_16bit_16x16_avx2(buf0 + 16 * j, buf1_cur + txfm_size_row * j);
      }
    }
  }
  const __m256i scale1 = _mm256_set1_epi16(1 << (15 + shift[1]));
  for (int i = 0; i < buf_size_w_div16; i++) {
    __m256i *buf1_cur = buf1 + i * txfm_size_row;
    col_txfm(buf1_cur, buf1_cur);
    for (int j = 0; j < txfm_size_row; ++j) {
      buf1_cur[j] = _mm256_mulhrs_epi16(buf1_cur[j], scale1);
    }
  }
  for (int i = 0; i < buf_size_w_div16; i++) {
    lowbd_write_buffer_16xn_avx2(buf1 + i * txfm_size_row, output + 16 * i,
                                 stride, ud_flip, txfm_size_row);
  }
}

static INLINE void iidentity_row_16xn_avx2(__m256i *out, const int32_t *input,
                                           int stride, int shift, int height,
                                           int txw_idx, int rect_type) {
  const int32_t *input_row = input;
  const __m256i scale = _mm256_set1_epi16(NewSqrt2list[txw_idx]);
  const __m256i _r = _mm256_set1_epi16((1 << (NewSqrt2Bits - 1)) +
                                       (1 << (NewSqrt2Bits - shift - 1)));
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i scale__r = _mm256_unpacklo_epi16(scale, _r);
  if (rect_type != 1 && rect_type != -1) {
    for (int i = 0; i < height; ++i) {
      const __m256i src = load_32bit_to_16bit_w16_avx2(input_row);
      input_row += stride;
      __m256i lo = _mm256_unpacklo_epi16(src, one);
      __m256i hi = _mm256_unpackhi_epi16(src, one);
      lo = _mm256_madd_epi16(lo, scale__r);
      hi = _mm256_madd_epi16(hi, scale__r);
      lo = _mm256_srai_epi32(lo, NewSqrt2Bits - shift);
      hi = _mm256_srai_epi32(hi, NewSqrt2Bits - shift);
      out[i] = _mm256_packs_epi32(lo, hi);
    }
  } else {
    const __m256i rect_scale =
        _mm256_set1_epi16(NewInvSqrt2 << (15 - NewSqrt2Bits));
    for (int i = 0; i < height; ++i) {
      __m256i src = load_32bit_to_16bit_w16_avx2(input_row);
      src = _mm256_mulhrs_epi16(src, rect_scale);
      input_row += stride;
      __m256i lo = _mm256_unpacklo_epi16(src, one);
      __m256i hi = _mm256_unpackhi_epi16(src, one);
      lo = _mm256_madd_epi16(lo, scale__r);
      hi = _mm256_madd_epi16(hi, scale__r);
      lo = _mm256_srai_epi32(lo, NewSqrt2Bits - shift);
      hi = _mm256_srai_epi32(hi, NewSqrt2Bits - shift);
      out[i] = _mm256_packs_epi32(lo, hi);
    }
  }
}

static INLINE void iidentity_col_16xn_avx2(uint8_t *output, int stride,
                                           __m256i *buf, int shift, int height,
                                           int txh_idx) {
  const __m256i scale = _mm256_set1_epi16(NewSqrt2list[txh_idx]);
  const __m256i scale__r = _mm256_set1_epi16(1 << (NewSqrt2Bits - 1));
  const __m256i shift__r = _mm256_set1_epi32(1 << (-shift - 1));
  const __m256i one = _mm256_set1_epi16(1);
  const __m256i scale_coeff = _mm256_unpacklo_epi16(scale, scale__r);
  for (int h = 0; h < height; ++h) {
    __m256i lo = _mm256_unpacklo_epi16(buf[h], one);
    __m256i hi = _mm256_unpackhi_epi16(buf[h], one);
    lo = _mm256_madd_epi16(lo, scale_coeff);
    hi = _mm256_madd_epi16(hi, scale_coeff);
    lo = _mm256_srai_epi32(lo, NewSqrt2Bits);
    hi = _mm256_srai_epi32(hi, NewSqrt2Bits);
    lo = _mm256_add_epi32(lo, shift__r);
    hi = _mm256_add_epi32(hi, shift__r);
    lo = _mm256_srai_epi32(lo, -shift);
    hi = _mm256_srai_epi32(hi, -shift);
    const __m256i x = _mm256_packs_epi32(lo, hi);
    write_recon_w16_avx2(x, output);
    output += stride;
  }
}

static INLINE void lowbd_inv_txfm2d_add_idtx_avx2(const int32_t *input,
                                                  uint8_t *output, int stride,
                                                  TX_SIZE tx_size,
                                                  int32_t eob) {
  (void)eob;
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int input_stride = AOMMIN(32, txfm_size_col);
  const int row_max = AOMMIN(32, txfm_size_row);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);
  __m256i buf[32];
  for (int i = 0; i < input_stride; i += 16) {
    iidentity_row_16xn_avx2(buf, input + i, input_stride, shift[0], row_max,
                            txw_idx, rect_type);
    iidentity_col_16xn_avx2(output + i, stride, buf, shift[1], row_max,
                            txh_idx);
  }
}

static INLINE void lowbd_inv_txfm2d_add_h_identity_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  int eobx, eoby;
  get_eobx_eoby_scan_h_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int txfm_size_col_notzero = AOMMIN(32, txfm_size_col);
  const int input_stride = txfm_size_col_notzero;
  const int buf_size_w_div16 = (eobx + 16) >> 4;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx_y = lowbd_txfm_all_1d_zeros_idx[eoby];
  const transform_1d_avx2 col_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[txh_idx][vitx_1d_tab[tx_type]][fun_idx_y];

  assert(col_txfm != NULL);

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < buf_size_w_div16; i++) {
    __m256i buf0[64];
    iidentity_row_16xn_avx2(buf0, input + (i << 4), input_stride, shift[0],
                            eoby + 1, txw_idx, rect_type);
    col_txfm(buf0, buf0);
    __m256i mshift = _mm256_set1_epi16(1 << (15 + shift[1]));
    int k = ud_flip ? (txfm_size_row - 1) : 0;
    const int step = ud_flip ? -1 : 1;
    for (int j = 0; j < txfm_size_row; ++j, k += step) {
      __m256i res = _mm256_mulhrs_epi16(buf0[k], mshift);
      write_recon_w16_avx2(res, output + (i << 4) + j * stride);
    }
  }
}

static INLINE void lowbd_inv_txfm2d_add_v_identity_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  __m256i buf1[64];
  int eobx, eoby;
  get_eobx_eoby_scan_v_identity(&eobx, &eoby, tx_size, eob);
  const int8_t *shift = av1_inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div16 = txfm_size_col >> 4;
  const int buf_size_h_div16 = (eoby + 16) >> 4;
  const int input_stride = AOMMIN(32, txfm_size_col);
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const int fun_idx_x = lowbd_txfm_all_1d_zeros_idx[eobx];
  const transform_1d_avx2 row_txfm =
      lowbd_txfm_all_1d_zeros_w16_arr[txw_idx][hitx_1d_tab[tx_type]][fun_idx_x];

  assert(row_txfm != NULL);

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < buf_size_h_div16; i++) {
    __m256i buf0[64];
    const int32_t *input_row = input + i * input_stride * 16;
    for (int j = 0; j < AOMMIN(4, buf_size_w_div16); ++j) {
      __m256i *buf0_cur = buf0 + j * 16;
      load_buffer_32bit_to_16bit_w16_avx2(input_row + j * 16, input_stride,
                                          buf0_cur, 16);
      transpose_16bit_16x16_avx2(buf0_cur, buf0_cur);
    }
    if (rect_type == 1 || rect_type == -1) {
      round_shift_avx2(buf0, buf0, input_stride);  // rect special code
    }
    row_txfm(buf0, buf0);
    round_shift_16bit_w16_avx2(buf0, txfm_size_col, shift[0]);
    __m256i *_buf1 = buf1;
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        __m256i temp[16];
        flip_buf_avx2(buf0 + 16 * j, temp, 16);
        transpose_16bit_16x16_avx2(temp,
                                   _buf1 + 16 * (buf_size_w_div16 - 1 - j));
      }
    } else {
      for (int j = 0; j < buf_size_w_div16; ++j) {
        transpose_16bit_16x16_avx2(buf0 + 16 * j, _buf1 + 16 * j);
      }
    }
    for (int j = 0; j < buf_size_w_div16; ++j) {
      iidentity_col_16xn_avx2(output + i * 16 * stride + j * 16, stride,
                              buf1 + j * 16, shift[1], 16, txh_idx);
    }
  }
}

// for 32x32,32x64,64x32,64x64,16x32,32x16,64x16,16x64
static INLINE void lowbd_inv_txfm2d_add_universe_avx2(
    const int32_t *input, uint8_t *output, int stride, TX_TYPE tx_type,
    TX_SIZE tx_size, int eob) {
  (void)eob;
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:   // ADST in vertical, DCT in horizontal
    case DCT_ADST:   // DCT  in vertical, ADST in horizontal
    case ADST_ADST:  // ADST in both directions
    case FLIPADST_DCT:
    case DCT_FLIPADST:
    case FLIPADST_FLIPADST:
    case ADST_FLIPADST:
    case FLIPADST_ADST:
      lowbd_inv_txfm2d_add_no_identity_avx2(input, output, stride, tx_type,
                                            tx_size, eob);
      break;
    case IDTX:
      lowbd_inv_txfm2d_add_idtx_avx2(input, output, stride, tx_size, eob);
      break;
    case V_DCT:
    case V_ADST:
    case V_FLIPADST:
      lowbd_inv_txfm2d_add_h_identity_avx2(input, output, stride, tx_type,
                                           tx_size, eob);
      break;
    case H_DCT:
    case H_ADST:
    case H_FLIPADST:
      lowbd_inv_txfm2d_add_v_identity_avx2(input, output, stride, tx_type,
                                           tx_size, eob);
      break;
    default:
      av1_lowbd_inv_txfm2d_add_ssse3(input, output, stride, tx_type, tx_size,
                                     eob);
      break;
  }
}

void av1_lowbd_inv_txfm2d_add_avx2(const int32_t *input, uint8_t *output,
                                   int stride, TX_TYPE tx_type, TX_SIZE tx_size,
                                   int eob) {
  switch (tx_size) {
    case TX_4X4:
    case TX_8X8:
    case TX_4X8:
    case TX_8X4:
    case TX_8X16:
    case TX_16X8:
    case TX_4X16:
    case TX_16X4:
    case TX_8X32:
    case TX_32X8:
      av1_lowbd_inv_txfm2d_add_ssse3(input, output, stride, tx_type, tx_size,
                                     eob);
      break;
    case TX_16X16:
    case TX_32X32:
    case TX_64X64:
    case TX_16X32:
    case TX_32X16:
    case TX_32X64:
    case TX_64X32:
    case TX_16X64:
    case TX_64X16:
    default:
      lowbd_inv_txfm2d_add_universe_avx2(input, output, stride, tx_type,
                                         tx_size, eob);
      break;
  }
}

void av1_inv_txfm_add_avx2(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  if (!txfm_param->lossless) {
    av1_lowbd_inv_txfm2d_add_avx2(dqcoeff, dst, stride, tx_type,
                                  txfm_param->tx_size, txfm_param->eob);
  } else {
    av1_inv_txfm_add_c(dqcoeff, dst, stride, txfm_param);
  }
}
