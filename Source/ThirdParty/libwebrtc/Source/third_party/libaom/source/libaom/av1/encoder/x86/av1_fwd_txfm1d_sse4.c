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

#include "av1/encoder/x86/av1_txfm1d_sse4.h"

void av1_fdct32_sse4_1(__m128i *input, __m128i *output, int cos_bit,
                       const int stride) {
  __m128i buf0[32];
  __m128i buf1[32];
  const int32_t *cospi;

  int startidx = 0 * stride;
  int endidx = 31 * stride;
  // stage 0
  // stage 1
  buf1[0] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[31] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[1] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[30] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[2] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[29] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[3] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[28] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[4] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[27] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[5] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[26] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[6] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[25] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[7] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[24] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[8] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[23] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[9] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[22] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[10] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[21] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[11] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[20] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[12] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[19] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[13] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[18] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[14] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[17] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += stride;
  endidx -= stride;
  buf1[15] = _mm_add_epi32(input[startidx], input[endidx]);
  buf1[16] = _mm_sub_epi32(input[startidx], input[endidx]);

  // stage 2
  cospi = cospi_arr(cos_bit);
  buf0[0] = _mm_add_epi32(buf1[0], buf1[15]);
  buf0[15] = _mm_sub_epi32(buf1[0], buf1[15]);
  buf0[1] = _mm_add_epi32(buf1[1], buf1[14]);
  buf0[14] = _mm_sub_epi32(buf1[1], buf1[14]);
  buf0[2] = _mm_add_epi32(buf1[2], buf1[13]);
  buf0[13] = _mm_sub_epi32(buf1[2], buf1[13]);
  buf0[3] = _mm_add_epi32(buf1[3], buf1[12]);
  buf0[12] = _mm_sub_epi32(buf1[3], buf1[12]);
  buf0[4] = _mm_add_epi32(buf1[4], buf1[11]);
  buf0[11] = _mm_sub_epi32(buf1[4], buf1[11]);
  buf0[5] = _mm_add_epi32(buf1[5], buf1[10]);
  buf0[10] = _mm_sub_epi32(buf1[5], buf1[10]);
  buf0[6] = _mm_add_epi32(buf1[6], buf1[9]);
  buf0[9] = _mm_sub_epi32(buf1[6], buf1[9]);
  buf0[7] = _mm_add_epi32(buf1[7], buf1[8]);
  buf0[8] = _mm_sub_epi32(buf1[7], buf1[8]);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  buf0[18] = buf1[18];
  buf0[19] = buf1[19];
  btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[20], buf1[27], buf0[20],
                      buf0[27], cos_bit);
  btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[21], buf1[26], buf0[21],
                      buf0[26], cos_bit);
  btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[22], buf1[25], buf0[22],
                      buf0[25], cos_bit);
  btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[23], buf1[24], buf0[23],
                      buf0[24], cos_bit);
  buf0[28] = buf1[28];
  buf0[29] = buf1[29];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 3
  cospi = cospi_arr(cos_bit);
  buf1[0] = _mm_add_epi32(buf0[0], buf0[7]);
  buf1[7] = _mm_sub_epi32(buf0[0], buf0[7]);
  buf1[1] = _mm_add_epi32(buf0[1], buf0[6]);
  buf1[6] = _mm_sub_epi32(buf0[1], buf0[6]);
  buf1[2] = _mm_add_epi32(buf0[2], buf0[5]);
  buf1[5] = _mm_sub_epi32(buf0[2], buf0[5]);
  buf1[3] = _mm_add_epi32(buf0[3], buf0[4]);
  buf1[4] = _mm_sub_epi32(buf0[3], buf0[4]);
  buf1[8] = buf0[8];
  buf1[9] = buf0[9];
  btf_32_sse4_1_type0(-cospi[32], cospi[32], buf0[10], buf0[13], buf1[10],
                      buf1[13], cos_bit);
  btf_32_sse4_1_type0(-cospi[32], cospi[32], buf0[11], buf0[12], buf1[11],
                      buf1[12], cos_bit);
  buf1[14] = buf0[14];
  buf1[15] = buf0[15];
  buf1[16] = _mm_add_epi32(buf0[16], buf0[23]);
  buf1[23] = _mm_sub_epi32(buf0[16], buf0[23]);
  buf1[17] = _mm_add_epi32(buf0[17], buf0[22]);
  buf1[22] = _mm_sub_epi32(buf0[17], buf0[22]);
  buf1[18] = _mm_add_epi32(buf0[18], buf0[21]);
  buf1[21] = _mm_sub_epi32(buf0[18], buf0[21]);
  buf1[19] = _mm_add_epi32(buf0[19], buf0[20]);
  buf1[20] = _mm_sub_epi32(buf0[19], buf0[20]);
  buf1[24] = _mm_sub_epi32(buf0[31], buf0[24]);
  buf1[31] = _mm_add_epi32(buf0[31], buf0[24]);
  buf1[25] = _mm_sub_epi32(buf0[30], buf0[25]);
  buf1[30] = _mm_add_epi32(buf0[30], buf0[25]);
  buf1[26] = _mm_sub_epi32(buf0[29], buf0[26]);
  buf1[29] = _mm_add_epi32(buf0[29], buf0[26]);
  buf1[27] = _mm_sub_epi32(buf0[28], buf0[27]);
  buf1[28] = _mm_add_epi32(buf0[28], buf0[27]);

  // stage 4
  cospi = cospi_arr(cos_bit);
  buf0[0] = _mm_add_epi32(buf1[0], buf1[3]);
  buf0[3] = _mm_sub_epi32(buf1[0], buf1[3]);
  buf0[1] = _mm_add_epi32(buf1[1], buf1[2]);
  buf0[2] = _mm_sub_epi32(buf1[1], buf1[2]);
  buf0[4] = buf1[4];
  btf_32_sse4_1_type0(-cospi[32], cospi[32], buf1[5], buf1[6], buf0[5], buf0[6],
                      cos_bit);
  buf0[7] = buf1[7];
  buf0[8] = _mm_add_epi32(buf1[8], buf1[11]);
  buf0[11] = _mm_sub_epi32(buf1[8], buf1[11]);
  buf0[9] = _mm_add_epi32(buf1[9], buf1[10]);
  buf0[10] = _mm_sub_epi32(buf1[9], buf1[10]);
  buf0[12] = _mm_sub_epi32(buf1[15], buf1[12]);
  buf0[15] = _mm_add_epi32(buf1[15], buf1[12]);
  buf0[13] = _mm_sub_epi32(buf1[14], buf1[13]);
  buf0[14] = _mm_add_epi32(buf1[14], buf1[13]);
  buf0[16] = buf1[16];
  buf0[17] = buf1[17];
  btf_32_sse4_1_type0(-cospi[16], cospi[48], buf1[18], buf1[29], buf0[18],
                      buf0[29], cos_bit);
  btf_32_sse4_1_type0(-cospi[16], cospi[48], buf1[19], buf1[28], buf0[19],
                      buf0[28], cos_bit);
  btf_32_sse4_1_type0(-cospi[48], -cospi[16], buf1[20], buf1[27], buf0[20],
                      buf0[27], cos_bit);
  btf_32_sse4_1_type0(-cospi[48], -cospi[16], buf1[21], buf1[26], buf0[21],
                      buf0[26], cos_bit);
  buf0[22] = buf1[22];
  buf0[23] = buf1[23];
  buf0[24] = buf1[24];
  buf0[25] = buf1[25];
  buf0[30] = buf1[30];
  buf0[31] = buf1[31];

  // stage 5
  cospi = cospi_arr(cos_bit);
  btf_32_sse4_1_type0(cospi[32], cospi[32], buf0[0], buf0[1], buf1[0], buf1[1],
                      cos_bit);
  btf_32_sse4_1_type1(cospi[48], cospi[16], buf0[2], buf0[3], buf1[2], buf1[3],
                      cos_bit);
  buf1[4] = _mm_add_epi32(buf0[4], buf0[5]);
  buf1[5] = _mm_sub_epi32(buf0[4], buf0[5]);
  buf1[6] = _mm_sub_epi32(buf0[7], buf0[6]);
  buf1[7] = _mm_add_epi32(buf0[7], buf0[6]);
  buf1[8] = buf0[8];
  btf_32_sse4_1_type0(-cospi[16], cospi[48], buf0[9], buf0[14], buf1[9],
                      buf1[14], cos_bit);
  btf_32_sse4_1_type0(-cospi[48], -cospi[16], buf0[10], buf0[13], buf1[10],
                      buf1[13], cos_bit);
  buf1[11] = buf0[11];
  buf1[12] = buf0[12];
  buf1[15] = buf0[15];
  buf1[16] = _mm_add_epi32(buf0[16], buf0[19]);
  buf1[19] = _mm_sub_epi32(buf0[16], buf0[19]);
  buf1[17] = _mm_add_epi32(buf0[17], buf0[18]);
  buf1[18] = _mm_sub_epi32(buf0[17], buf0[18]);
  buf1[20] = _mm_sub_epi32(buf0[23], buf0[20]);
  buf1[23] = _mm_add_epi32(buf0[23], buf0[20]);
  buf1[21] = _mm_sub_epi32(buf0[22], buf0[21]);
  buf1[22] = _mm_add_epi32(buf0[22], buf0[21]);
  buf1[24] = _mm_add_epi32(buf0[24], buf0[27]);
  buf1[27] = _mm_sub_epi32(buf0[24], buf0[27]);
  buf1[25] = _mm_add_epi32(buf0[25], buf0[26]);
  buf1[26] = _mm_sub_epi32(buf0[25], buf0[26]);
  buf1[28] = _mm_sub_epi32(buf0[31], buf0[28]);
  buf1[31] = _mm_add_epi32(buf0[31], buf0[28]);
  buf1[29] = _mm_sub_epi32(buf0[30], buf0[29]);
  buf1[30] = _mm_add_epi32(buf0[30], buf0[29]);

  // stage 6
  cospi = cospi_arr(cos_bit);
  buf0[0] = buf1[0];
  buf0[1] = buf1[1];
  buf0[2] = buf1[2];
  buf0[3] = buf1[3];
  btf_32_sse4_1_type1(cospi[56], cospi[8], buf1[4], buf1[7], buf0[4], buf0[7],
                      cos_bit);
  btf_32_sse4_1_type1(cospi[24], cospi[40], buf1[5], buf1[6], buf0[5], buf0[6],
                      cos_bit);
  buf0[8] = _mm_add_epi32(buf1[8], buf1[9]);
  buf0[9] = _mm_sub_epi32(buf1[8], buf1[9]);
  buf0[10] = _mm_sub_epi32(buf1[11], buf1[10]);
  buf0[11] = _mm_add_epi32(buf1[11], buf1[10]);
  buf0[12] = _mm_add_epi32(buf1[12], buf1[13]);
  buf0[13] = _mm_sub_epi32(buf1[12], buf1[13]);
  buf0[14] = _mm_sub_epi32(buf1[15], buf1[14]);
  buf0[15] = _mm_add_epi32(buf1[15], buf1[14]);
  buf0[16] = buf1[16];
  btf_32_sse4_1_type0(-cospi[8], cospi[56], buf1[17], buf1[30], buf0[17],
                      buf0[30], cos_bit);
  btf_32_sse4_1_type0(-cospi[56], -cospi[8], buf1[18], buf1[29], buf0[18],
                      buf0[29], cos_bit);
  buf0[19] = buf1[19];
  buf0[20] = buf1[20];
  btf_32_sse4_1_type0(-cospi[40], cospi[24], buf1[21], buf1[26], buf0[21],
                      buf0[26], cos_bit);
  btf_32_sse4_1_type0(-cospi[24], -cospi[40], buf1[22], buf1[25], buf0[22],
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
  btf_32_sse4_1_type1(cospi[60], cospi[4], buf0[8], buf0[15], buf1[8], buf1[15],
                      cos_bit);
  btf_32_sse4_1_type1(cospi[28], cospi[36], buf0[9], buf0[14], buf1[9],
                      buf1[14], cos_bit);
  btf_32_sse4_1_type1(cospi[44], cospi[20], buf0[10], buf0[13], buf1[10],
                      buf1[13], cos_bit);
  btf_32_sse4_1_type1(cospi[12], cospi[52], buf0[11], buf0[12], buf1[11],
                      buf1[12], cos_bit);
  buf1[16] = _mm_add_epi32(buf0[16], buf0[17]);
  buf1[17] = _mm_sub_epi32(buf0[16], buf0[17]);
  buf1[18] = _mm_sub_epi32(buf0[19], buf0[18]);
  buf1[19] = _mm_add_epi32(buf0[19], buf0[18]);
  buf1[20] = _mm_add_epi32(buf0[20], buf0[21]);
  buf1[21] = _mm_sub_epi32(buf0[20], buf0[21]);
  buf1[22] = _mm_sub_epi32(buf0[23], buf0[22]);
  buf1[23] = _mm_add_epi32(buf0[23], buf0[22]);
  buf1[24] = _mm_add_epi32(buf0[24], buf0[25]);
  buf1[25] = _mm_sub_epi32(buf0[24], buf0[25]);
  buf1[26] = _mm_sub_epi32(buf0[27], buf0[26]);
  buf1[27] = _mm_add_epi32(buf0[27], buf0[26]);
  buf1[28] = _mm_add_epi32(buf0[28], buf0[29]);
  buf1[29] = _mm_sub_epi32(buf0[28], buf0[29]);
  buf1[30] = _mm_sub_epi32(buf0[31], buf0[30]);
  buf1[31] = _mm_add_epi32(buf0[31], buf0[30]);

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
  btf_32_sse4_1_type1(cospi[62], cospi[2], buf1[16], buf1[31], buf0[16],
                      buf0[31], cos_bit);
  btf_32_sse4_1_type1(cospi[30], cospi[34], buf1[17], buf1[30], buf0[17],
                      buf0[30], cos_bit);
  btf_32_sse4_1_type1(cospi[46], cospi[18], buf1[18], buf1[29], buf0[18],
                      buf0[29], cos_bit);
  btf_32_sse4_1_type1(cospi[14], cospi[50], buf1[19], buf1[28], buf0[19],
                      buf0[28], cos_bit);
  btf_32_sse4_1_type1(cospi[54], cospi[10], buf1[20], buf1[27], buf0[20],
                      buf0[27], cos_bit);
  btf_32_sse4_1_type1(cospi[22], cospi[42], buf1[21], buf1[26], buf0[21],
                      buf0[26], cos_bit);
  btf_32_sse4_1_type1(cospi[38], cospi[26], buf1[22], buf1[25], buf0[22],
                      buf0[25], cos_bit);
  btf_32_sse4_1_type1(cospi[6], cospi[58], buf1[23], buf1[24], buf0[23],
                      buf0[24], cos_bit);

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

void av1_fadst4_sse4_1(const __m128i *input, __m128i *output,
                       const int8_t cos_bit, const int8_t *stage_range) {
  const int txfm_size = 4;
  const int num_per_128 = 4;
  const int32_t *cospi;
  __m128i buf0[4];
  __m128i buf1[4];
  int col_num = txfm_size / num_per_128;
  int col;
  (void)stage_range;
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
    cospi = cospi_arr(cos_bit);
    btf_32_sse4_1_type0(cospi[8], cospi[56], buf1[0], buf1[1], buf0[0], buf0[1],
                        cos_bit);
    btf_32_sse4_1_type0(cospi[40], cospi[24], buf1[2], buf1[3], buf0[2],
                        buf0[3], cos_bit);

    // stage 3
    buf1[0] = _mm_add_epi32(buf0[0], buf0[2]);
    buf1[2] = _mm_sub_epi32(buf0[0], buf0[2]);
    buf1[1] = _mm_add_epi32(buf0[1], buf0[3]);
    buf1[3] = _mm_sub_epi32(buf0[1], buf0[3]);

    // stage 4
    cospi = cospi_arr(cos_bit);
    buf0[0] = buf1[0];
    buf0[1] = buf1[1];
    btf_32_sse4_1_type0(cospi[32], cospi[32], buf1[2], buf1[3], buf0[2],
                        buf0[3], cos_bit);

    // stage 5
    buf1[0] = buf0[0];
    buf1[1] = _mm_sub_epi32(_mm_setzero_si128(), buf0[2]);
    buf1[2] = buf0[3];
    buf1[3] = _mm_sub_epi32(_mm_setzero_si128(), buf0[1]);

    for (j = 0; j < 4; ++j) {
      output[j * col_num + col] = buf1[j];
    }
  }
}

void av1_fdct64_sse4_1(__m128i *input, __m128i *output, int8_t cos_bit,
                       const int instride, const int outstride) {
  const int32_t *cospi = cospi_arr(cos_bit);
  const __m128i __rounding = _mm_set1_epi32(1 << (cos_bit - 1));

  __m128i cospi_m32 = _mm_set1_epi32(-cospi[32]);
  __m128i cospi_p32 = _mm_set1_epi32(cospi[32]);
  __m128i cospi_m16 = _mm_set1_epi32(-cospi[16]);
  __m128i cospi_p48 = _mm_set1_epi32(cospi[48]);
  __m128i cospi_m48 = _mm_set1_epi32(-cospi[48]);
  __m128i cospi_p16 = _mm_set1_epi32(cospi[16]);
  __m128i cospi_m08 = _mm_set1_epi32(-cospi[8]);
  __m128i cospi_p56 = _mm_set1_epi32(cospi[56]);
  __m128i cospi_m56 = _mm_set1_epi32(-cospi[56]);
  __m128i cospi_m40 = _mm_set1_epi32(-cospi[40]);
  __m128i cospi_p24 = _mm_set1_epi32(cospi[24]);
  __m128i cospi_m24 = _mm_set1_epi32(-cospi[24]);
  __m128i cospi_p08 = _mm_set1_epi32(cospi[8]);
  __m128i cospi_p40 = _mm_set1_epi32(cospi[40]);
  __m128i cospi_p60 = _mm_set1_epi32(cospi[60]);
  __m128i cospi_p04 = _mm_set1_epi32(cospi[4]);
  __m128i cospi_p28 = _mm_set1_epi32(cospi[28]);
  __m128i cospi_p36 = _mm_set1_epi32(cospi[36]);
  __m128i cospi_p44 = _mm_set1_epi32(cospi[44]);
  __m128i cospi_p20 = _mm_set1_epi32(cospi[20]);
  __m128i cospi_p12 = _mm_set1_epi32(cospi[12]);
  __m128i cospi_p52 = _mm_set1_epi32(cospi[52]);
  __m128i cospi_m04 = _mm_set1_epi32(-cospi[4]);
  __m128i cospi_m60 = _mm_set1_epi32(-cospi[60]);
  __m128i cospi_m36 = _mm_set1_epi32(-cospi[36]);
  __m128i cospi_m28 = _mm_set1_epi32(-cospi[28]);
  __m128i cospi_m20 = _mm_set1_epi32(-cospi[20]);
  __m128i cospi_m44 = _mm_set1_epi32(-cospi[44]);
  __m128i cospi_m52 = _mm_set1_epi32(-cospi[52]);
  __m128i cospi_m12 = _mm_set1_epi32(-cospi[12]);
  __m128i cospi_p62 = _mm_set1_epi32(cospi[62]);
  __m128i cospi_p02 = _mm_set1_epi32(cospi[2]);
  __m128i cospi_p30 = _mm_set1_epi32(cospi[30]);
  __m128i cospi_p34 = _mm_set1_epi32(cospi[34]);
  __m128i cospi_p46 = _mm_set1_epi32(cospi[46]);
  __m128i cospi_p18 = _mm_set1_epi32(cospi[18]);
  __m128i cospi_p14 = _mm_set1_epi32(cospi[14]);
  __m128i cospi_p50 = _mm_set1_epi32(cospi[50]);
  __m128i cospi_p54 = _mm_set1_epi32(cospi[54]);
  __m128i cospi_p10 = _mm_set1_epi32(cospi[10]);
  __m128i cospi_p22 = _mm_set1_epi32(cospi[22]);
  __m128i cospi_p42 = _mm_set1_epi32(cospi[42]);
  __m128i cospi_p38 = _mm_set1_epi32(cospi[38]);
  __m128i cospi_p26 = _mm_set1_epi32(cospi[26]);
  __m128i cospi_p06 = _mm_set1_epi32(cospi[6]);
  __m128i cospi_p58 = _mm_set1_epi32(cospi[58]);
  __m128i cospi_p63 = _mm_set1_epi32(cospi[63]);
  __m128i cospi_p01 = _mm_set1_epi32(cospi[1]);
  __m128i cospi_p31 = _mm_set1_epi32(cospi[31]);
  __m128i cospi_p33 = _mm_set1_epi32(cospi[33]);
  __m128i cospi_p47 = _mm_set1_epi32(cospi[47]);
  __m128i cospi_p17 = _mm_set1_epi32(cospi[17]);
  __m128i cospi_p15 = _mm_set1_epi32(cospi[15]);
  __m128i cospi_p49 = _mm_set1_epi32(cospi[49]);
  __m128i cospi_p55 = _mm_set1_epi32(cospi[55]);
  __m128i cospi_p09 = _mm_set1_epi32(cospi[9]);
  __m128i cospi_p23 = _mm_set1_epi32(cospi[23]);
  __m128i cospi_p41 = _mm_set1_epi32(cospi[41]);
  __m128i cospi_p39 = _mm_set1_epi32(cospi[39]);
  __m128i cospi_p25 = _mm_set1_epi32(cospi[25]);
  __m128i cospi_p07 = _mm_set1_epi32(cospi[7]);
  __m128i cospi_p57 = _mm_set1_epi32(cospi[57]);
  __m128i cospi_p59 = _mm_set1_epi32(cospi[59]);
  __m128i cospi_p05 = _mm_set1_epi32(cospi[5]);
  __m128i cospi_p27 = _mm_set1_epi32(cospi[27]);
  __m128i cospi_p37 = _mm_set1_epi32(cospi[37]);
  __m128i cospi_p43 = _mm_set1_epi32(cospi[43]);
  __m128i cospi_p21 = _mm_set1_epi32(cospi[21]);
  __m128i cospi_p11 = _mm_set1_epi32(cospi[11]);
  __m128i cospi_p53 = _mm_set1_epi32(cospi[53]);
  __m128i cospi_p51 = _mm_set1_epi32(cospi[51]);
  __m128i cospi_p13 = _mm_set1_epi32(cospi[13]);
  __m128i cospi_p19 = _mm_set1_epi32(cospi[19]);
  __m128i cospi_p45 = _mm_set1_epi32(cospi[45]);
  __m128i cospi_p35 = _mm_set1_epi32(cospi[35]);
  __m128i cospi_p29 = _mm_set1_epi32(cospi[29]);
  __m128i cospi_p03 = _mm_set1_epi32(cospi[3]);
  __m128i cospi_p61 = _mm_set1_epi32(cospi[61]);

  int startidx = 0 * instride;
  int endidx = 63 * instride;
  // stage 1
  __m128i x1[64];
  x1[0] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[63] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[1] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[62] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[2] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[61] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[3] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[60] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[4] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[59] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[5] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[58] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[6] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[57] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[7] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[56] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[8] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[55] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[9] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[54] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[10] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[53] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[11] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[52] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[12] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[51] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[13] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[50] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[14] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[49] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[15] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[48] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[16] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[47] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[17] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[46] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[18] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[45] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[19] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[44] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[20] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[43] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[21] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[42] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[22] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[41] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[23] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[40] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[24] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[39] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[25] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[38] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[26] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[37] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[27] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[36] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[28] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[35] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[29] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[34] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[30] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[33] = _mm_sub_epi32(input[startidx], input[endidx]);
  startidx += instride;
  endidx -= instride;
  x1[31] = _mm_add_epi32(input[startidx], input[endidx]);
  x1[32] = _mm_sub_epi32(input[startidx], input[endidx]);

  // stage 2
  __m128i x2[64];
  x2[0] = _mm_add_epi32(x1[0], x1[31]);
  x2[31] = _mm_sub_epi32(x1[0], x1[31]);
  x2[1] = _mm_add_epi32(x1[1], x1[30]);
  x2[30] = _mm_sub_epi32(x1[1], x1[30]);
  x2[2] = _mm_add_epi32(x1[2], x1[29]);
  x2[29] = _mm_sub_epi32(x1[2], x1[29]);
  x2[3] = _mm_add_epi32(x1[3], x1[28]);
  x2[28] = _mm_sub_epi32(x1[3], x1[28]);
  x2[4] = _mm_add_epi32(x1[4], x1[27]);
  x2[27] = _mm_sub_epi32(x1[4], x1[27]);
  x2[5] = _mm_add_epi32(x1[5], x1[26]);
  x2[26] = _mm_sub_epi32(x1[5], x1[26]);
  x2[6] = _mm_add_epi32(x1[6], x1[25]);
  x2[25] = _mm_sub_epi32(x1[6], x1[25]);
  x2[7] = _mm_add_epi32(x1[7], x1[24]);
  x2[24] = _mm_sub_epi32(x1[7], x1[24]);
  x2[8] = _mm_add_epi32(x1[8], x1[23]);
  x2[23] = _mm_sub_epi32(x1[8], x1[23]);
  x2[9] = _mm_add_epi32(x1[9], x1[22]);
  x2[22] = _mm_sub_epi32(x1[9], x1[22]);
  x2[10] = _mm_add_epi32(x1[10], x1[21]);
  x2[21] = _mm_sub_epi32(x1[10], x1[21]);
  x2[11] = _mm_add_epi32(x1[11], x1[20]);
  x2[20] = _mm_sub_epi32(x1[11], x1[20]);
  x2[12] = _mm_add_epi32(x1[12], x1[19]);
  x2[19] = _mm_sub_epi32(x1[12], x1[19]);
  x2[13] = _mm_add_epi32(x1[13], x1[18]);
  x2[18] = _mm_sub_epi32(x1[13], x1[18]);
  x2[14] = _mm_add_epi32(x1[14], x1[17]);
  x2[17] = _mm_sub_epi32(x1[14], x1[17]);
  x2[15] = _mm_add_epi32(x1[15], x1[16]);
  x2[16] = _mm_sub_epi32(x1[15], x1[16]);
  x2[32] = x1[32];
  x2[33] = x1[33];
  x2[34] = x1[34];
  x2[35] = x1[35];
  x2[36] = x1[36];
  x2[37] = x1[37];
  x2[38] = x1[38];
  x2[39] = x1[39];
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x1[40], x1[55], x2[40], x2[55],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x1[41], x1[54], x2[41], x2[54],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x1[42], x1[53], x2[42], x2[53],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x1[43], x1[52], x2[43], x2[52],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x1[44], x1[51], x2[44], x2[51],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x1[45], x1[50], x2[45], x2[50],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x1[46], x1[49], x2[46], x2[49],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x1[47], x1[48], x2[47], x2[48],
                          __rounding, cos_bit);
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
  x3[0] = _mm_add_epi32(x2[0], x2[15]);
  x3[15] = _mm_sub_epi32(x2[0], x2[15]);
  x3[1] = _mm_add_epi32(x2[1], x2[14]);
  x3[14] = _mm_sub_epi32(x2[1], x2[14]);
  x3[2] = _mm_add_epi32(x2[2], x2[13]);
  x3[13] = _mm_sub_epi32(x2[2], x2[13]);
  x3[3] = _mm_add_epi32(x2[3], x2[12]);
  x3[12] = _mm_sub_epi32(x2[3], x2[12]);
  x3[4] = _mm_add_epi32(x2[4], x2[11]);
  x3[11] = _mm_sub_epi32(x2[4], x2[11]);
  x3[5] = _mm_add_epi32(x2[5], x2[10]);
  x3[10] = _mm_sub_epi32(x2[5], x2[10]);
  x3[6] = _mm_add_epi32(x2[6], x2[9]);
  x3[9] = _mm_sub_epi32(x2[6], x2[9]);
  x3[7] = _mm_add_epi32(x2[7], x2[8]);
  x3[8] = _mm_sub_epi32(x2[7], x2[8]);
  x3[16] = x2[16];
  x3[17] = x2[17];
  x3[18] = x2[18];
  x3[19] = x2[19];
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x2[20], x2[27], x3[20], x3[27],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x2[21], x2[26], x3[21], x3[26],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x2[22], x2[25], x3[22], x3[25],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x2[23], x2[24], x3[23], x3[24],
                          __rounding, cos_bit);
  x3[28] = x2[28];
  x3[29] = x2[29];
  x3[30] = x2[30];
  x3[31] = x2[31];
  x3[32] = _mm_add_epi32(x2[32], x2[47]);
  x3[47] = _mm_sub_epi32(x2[32], x2[47]);
  x3[33] = _mm_add_epi32(x2[33], x2[46]);
  x3[46] = _mm_sub_epi32(x2[33], x2[46]);
  x3[34] = _mm_add_epi32(x2[34], x2[45]);
  x3[45] = _mm_sub_epi32(x2[34], x2[45]);
  x3[35] = _mm_add_epi32(x2[35], x2[44]);
  x3[44] = _mm_sub_epi32(x2[35], x2[44]);
  x3[36] = _mm_add_epi32(x2[36], x2[43]);
  x3[43] = _mm_sub_epi32(x2[36], x2[43]);
  x3[37] = _mm_add_epi32(x2[37], x2[42]);
  x3[42] = _mm_sub_epi32(x2[37], x2[42]);
  x3[38] = _mm_add_epi32(x2[38], x2[41]);
  x3[41] = _mm_sub_epi32(x2[38], x2[41]);
  x3[39] = _mm_add_epi32(x2[39], x2[40]);
  x3[40] = _mm_sub_epi32(x2[39], x2[40]);
  x3[48] = _mm_sub_epi32(x2[63], x2[48]);
  x3[63] = _mm_add_epi32(x2[63], x2[48]);
  x3[49] = _mm_sub_epi32(x2[62], x2[49]);
  x3[62] = _mm_add_epi32(x2[62], x2[49]);
  x3[50] = _mm_sub_epi32(x2[61], x2[50]);
  x3[61] = _mm_add_epi32(x2[61], x2[50]);
  x3[51] = _mm_sub_epi32(x2[60], x2[51]);
  x3[60] = _mm_add_epi32(x2[60], x2[51]);
  x3[52] = _mm_sub_epi32(x2[59], x2[52]);
  x3[59] = _mm_add_epi32(x2[59], x2[52]);
  x3[53] = _mm_sub_epi32(x2[58], x2[53]);
  x3[58] = _mm_add_epi32(x2[58], x2[53]);
  x3[54] = _mm_sub_epi32(x2[57], x2[54]);
  x3[57] = _mm_add_epi32(x2[57], x2[54]);
  x3[55] = _mm_sub_epi32(x2[56], x2[55]);
  x3[56] = _mm_add_epi32(x2[56], x2[55]);

  // stage 4
  __m128i x4[64];
  x4[0] = _mm_add_epi32(x3[0], x3[7]);
  x4[7] = _mm_sub_epi32(x3[0], x3[7]);
  x4[1] = _mm_add_epi32(x3[1], x3[6]);
  x4[6] = _mm_sub_epi32(x3[1], x3[6]);
  x4[2] = _mm_add_epi32(x3[2], x3[5]);
  x4[5] = _mm_sub_epi32(x3[2], x3[5]);
  x4[3] = _mm_add_epi32(x3[3], x3[4]);
  x4[4] = _mm_sub_epi32(x3[3], x3[4]);
  x4[8] = x3[8];
  x4[9] = x3[9];
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x3[10], x3[13], x4[10], x4[13],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x3[11], x3[12], x4[11], x4[12],
                          __rounding, cos_bit);
  x4[14] = x3[14];
  x4[15] = x3[15];
  x4[16] = _mm_add_epi32(x3[16], x3[23]);
  x4[23] = _mm_sub_epi32(x3[16], x3[23]);
  x4[17] = _mm_add_epi32(x3[17], x3[22]);
  x4[22] = _mm_sub_epi32(x3[17], x3[22]);
  x4[18] = _mm_add_epi32(x3[18], x3[21]);
  x4[21] = _mm_sub_epi32(x3[18], x3[21]);
  x4[19] = _mm_add_epi32(x3[19], x3[20]);
  x4[20] = _mm_sub_epi32(x3[19], x3[20]);
  x4[24] = _mm_sub_epi32(x3[31], x3[24]);
  x4[31] = _mm_add_epi32(x3[31], x3[24]);
  x4[25] = _mm_sub_epi32(x3[30], x3[25]);
  x4[30] = _mm_add_epi32(x3[30], x3[25]);
  x4[26] = _mm_sub_epi32(x3[29], x3[26]);
  x4[29] = _mm_add_epi32(x3[29], x3[26]);
  x4[27] = _mm_sub_epi32(x3[28], x3[27]);
  x4[28] = _mm_add_epi32(x3[28], x3[27]);
  x4[32] = x3[32];
  x4[33] = x3[33];
  x4[34] = x3[34];
  x4[35] = x3[35];
  btf_32_type0_sse4_1_new(cospi_m16, cospi_p48, x3[36], x3[59], x4[36], x4[59],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m16, cospi_p48, x3[37], x3[58], x4[37], x4[58],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m16, cospi_p48, x3[38], x3[57], x4[38], x4[57],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m16, cospi_p48, x3[39], x3[56], x4[39], x4[56],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m48, cospi_m16, x3[40], x3[55], x4[40], x4[55],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m48, cospi_m16, x3[41], x3[54], x4[41], x4[54],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m48, cospi_m16, x3[42], x3[53], x4[42], x4[53],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m48, cospi_m16, x3[43], x3[52], x4[43], x4[52],
                          __rounding, cos_bit);
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
  x5[0] = _mm_add_epi32(x4[0], x4[3]);
  x5[3] = _mm_sub_epi32(x4[0], x4[3]);
  x5[1] = _mm_add_epi32(x4[1], x4[2]);
  x5[2] = _mm_sub_epi32(x4[1], x4[2]);
  x5[4] = x4[4];
  btf_32_type0_sse4_1_new(cospi_m32, cospi_p32, x4[5], x4[6], x5[5], x5[6],
                          __rounding, cos_bit);
  x5[7] = x4[7];
  x5[8] = _mm_add_epi32(x4[8], x4[11]);
  x5[11] = _mm_sub_epi32(x4[8], x4[11]);
  x5[9] = _mm_add_epi32(x4[9], x4[10]);
  x5[10] = _mm_sub_epi32(x4[9], x4[10]);
  x5[12] = _mm_sub_epi32(x4[15], x4[12]);
  x5[15] = _mm_add_epi32(x4[15], x4[12]);
  x5[13] = _mm_sub_epi32(x4[14], x4[13]);
  x5[14] = _mm_add_epi32(x4[14], x4[13]);
  x5[16] = x4[16];
  x5[17] = x4[17];
  btf_32_type0_sse4_1_new(cospi_m16, cospi_p48, x4[18], x4[29], x5[18], x5[29],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m16, cospi_p48, x4[19], x4[28], x5[19], x5[28],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m48, cospi_m16, x4[20], x4[27], x5[20], x5[27],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m48, cospi_m16, x4[21], x4[26], x5[21], x5[26],
                          __rounding, cos_bit);
  x5[22] = x4[22];
  x5[23] = x4[23];
  x5[24] = x4[24];
  x5[25] = x4[25];
  x5[30] = x4[30];
  x5[31] = x4[31];
  x5[32] = _mm_add_epi32(x4[32], x4[39]);
  x5[39] = _mm_sub_epi32(x4[32], x4[39]);
  x5[33] = _mm_add_epi32(x4[33], x4[38]);
  x5[38] = _mm_sub_epi32(x4[33], x4[38]);
  x5[34] = _mm_add_epi32(x4[34], x4[37]);
  x5[37] = _mm_sub_epi32(x4[34], x4[37]);
  x5[35] = _mm_add_epi32(x4[35], x4[36]);
  x5[36] = _mm_sub_epi32(x4[35], x4[36]);
  x5[40] = _mm_sub_epi32(x4[47], x4[40]);
  x5[47] = _mm_add_epi32(x4[47], x4[40]);
  x5[41] = _mm_sub_epi32(x4[46], x4[41]);
  x5[46] = _mm_add_epi32(x4[46], x4[41]);
  x5[42] = _mm_sub_epi32(x4[45], x4[42]);
  x5[45] = _mm_add_epi32(x4[45], x4[42]);
  x5[43] = _mm_sub_epi32(x4[44], x4[43]);
  x5[44] = _mm_add_epi32(x4[44], x4[43]);
  x5[48] = _mm_add_epi32(x4[48], x4[55]);
  x5[55] = _mm_sub_epi32(x4[48], x4[55]);
  x5[49] = _mm_add_epi32(x4[49], x4[54]);
  x5[54] = _mm_sub_epi32(x4[49], x4[54]);
  x5[50] = _mm_add_epi32(x4[50], x4[53]);
  x5[53] = _mm_sub_epi32(x4[50], x4[53]);
  x5[51] = _mm_add_epi32(x4[51], x4[52]);
  x5[52] = _mm_sub_epi32(x4[51], x4[52]);
  x5[56] = _mm_sub_epi32(x4[63], x4[56]);
  x5[63] = _mm_add_epi32(x4[63], x4[56]);
  x5[57] = _mm_sub_epi32(x4[62], x4[57]);
  x5[62] = _mm_add_epi32(x4[62], x4[57]);
  x5[58] = _mm_sub_epi32(x4[61], x4[58]);
  x5[61] = _mm_add_epi32(x4[61], x4[58]);
  x5[59] = _mm_sub_epi32(x4[60], x4[59]);
  x5[60] = _mm_add_epi32(x4[60], x4[59]);

  // stage 6
  __m128i x6[64];
  btf_32_type0_sse4_1_new(cospi_p32, cospi_p32, x5[0], x5[1], x6[0], x6[1],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p48, cospi_p16, x5[2], x5[3], x6[2], x6[3],
                          __rounding, cos_bit);
  x6[4] = _mm_add_epi32(x5[4], x5[5]);
  x6[5] = _mm_sub_epi32(x5[4], x5[5]);
  x6[6] = _mm_sub_epi32(x5[7], x5[6]);
  x6[7] = _mm_add_epi32(x5[7], x5[6]);
  x6[8] = x5[8];
  btf_32_type0_sse4_1_new(cospi_m16, cospi_p48, x5[9], x5[14], x6[9], x6[14],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m48, cospi_m16, x5[10], x5[13], x6[10], x6[13],
                          __rounding, cos_bit);
  x6[11] = x5[11];
  x6[12] = x5[12];
  x6[15] = x5[15];
  x6[16] = _mm_add_epi32(x5[16], x5[19]);
  x6[19] = _mm_sub_epi32(x5[16], x5[19]);
  x6[17] = _mm_add_epi32(x5[17], x5[18]);
  x6[18] = _mm_sub_epi32(x5[17], x5[18]);
  x6[20] = _mm_sub_epi32(x5[23], x5[20]);
  x6[23] = _mm_add_epi32(x5[23], x5[20]);
  x6[21] = _mm_sub_epi32(x5[22], x5[21]);
  x6[22] = _mm_add_epi32(x5[22], x5[21]);
  x6[24] = _mm_add_epi32(x5[24], x5[27]);
  x6[27] = _mm_sub_epi32(x5[24], x5[27]);
  x6[25] = _mm_add_epi32(x5[25], x5[26]);
  x6[26] = _mm_sub_epi32(x5[25], x5[26]);
  x6[28] = _mm_sub_epi32(x5[31], x5[28]);
  x6[31] = _mm_add_epi32(x5[31], x5[28]);
  x6[29] = _mm_sub_epi32(x5[30], x5[29]);
  x6[30] = _mm_add_epi32(x5[30], x5[29]);
  x6[32] = x5[32];
  x6[33] = x5[33];
  btf_32_type0_sse4_1_new(cospi_m08, cospi_p56, x5[34], x5[61], x6[34], x6[61],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m08, cospi_p56, x5[35], x5[60], x6[35], x6[60],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m56, cospi_m08, x5[36], x5[59], x6[36], x6[59],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m56, cospi_m08, x5[37], x5[58], x6[37], x6[58],
                          __rounding, cos_bit);
  x6[38] = x5[38];
  x6[39] = x5[39];
  x6[40] = x5[40];
  x6[41] = x5[41];
  btf_32_type0_sse4_1_new(cospi_m40, cospi_p24, x5[42], x5[53], x6[42], x6[53],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m40, cospi_p24, x5[43], x5[52], x6[43], x6[52],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m24, cospi_m40, x5[44], x5[51], x6[44], x6[51],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m24, cospi_m40, x5[45], x5[50], x6[45], x6[50],
                          __rounding, cos_bit);
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
  btf_32_type1_sse4_1_new(cospi_p56, cospi_p08, x6[4], x6[7], x7[4], x7[7],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p24, cospi_p40, x6[5], x6[6], x7[5], x7[6],
                          __rounding, cos_bit);
  x7[8] = _mm_add_epi32(x6[8], x6[9]);
  x7[9] = _mm_sub_epi32(x6[8], x6[9]);
  x7[10] = _mm_sub_epi32(x6[11], x6[10]);
  x7[11] = _mm_add_epi32(x6[11], x6[10]);
  x7[12] = _mm_add_epi32(x6[12], x6[13]);
  x7[13] = _mm_sub_epi32(x6[12], x6[13]);
  x7[14] = _mm_sub_epi32(x6[15], x6[14]);
  x7[15] = _mm_add_epi32(x6[15], x6[14]);
  x7[16] = x6[16];
  btf_32_type0_sse4_1_new(cospi_m08, cospi_p56, x6[17], x6[30], x7[17], x7[30],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m56, cospi_m08, x6[18], x6[29], x7[18], x7[29],
                          __rounding, cos_bit);
  x7[19] = x6[19];
  x7[20] = x6[20];
  btf_32_type0_sse4_1_new(cospi_m40, cospi_p24, x6[21], x6[26], x7[21], x7[26],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m24, cospi_m40, x6[22], x6[25], x7[22], x7[25],
                          __rounding, cos_bit);
  x7[23] = x6[23];
  x7[24] = x6[24];
  x7[27] = x6[27];
  x7[28] = x6[28];
  x7[31] = x6[31];
  x7[32] = _mm_add_epi32(x6[32], x6[35]);
  x7[35] = _mm_sub_epi32(x6[32], x6[35]);
  x7[33] = _mm_add_epi32(x6[33], x6[34]);
  x7[34] = _mm_sub_epi32(x6[33], x6[34]);
  x7[36] = _mm_sub_epi32(x6[39], x6[36]);
  x7[39] = _mm_add_epi32(x6[39], x6[36]);
  x7[37] = _mm_sub_epi32(x6[38], x6[37]);
  x7[38] = _mm_add_epi32(x6[38], x6[37]);
  x7[40] = _mm_add_epi32(x6[40], x6[43]);
  x7[43] = _mm_sub_epi32(x6[40], x6[43]);
  x7[41] = _mm_add_epi32(x6[41], x6[42]);
  x7[42] = _mm_sub_epi32(x6[41], x6[42]);
  x7[44] = _mm_sub_epi32(x6[47], x6[44]);
  x7[47] = _mm_add_epi32(x6[47], x6[44]);
  x7[45] = _mm_sub_epi32(x6[46], x6[45]);
  x7[46] = _mm_add_epi32(x6[46], x6[45]);
  x7[48] = _mm_add_epi32(x6[48], x6[51]);
  x7[51] = _mm_sub_epi32(x6[48], x6[51]);
  x7[49] = _mm_add_epi32(x6[49], x6[50]);
  x7[50] = _mm_sub_epi32(x6[49], x6[50]);
  x7[52] = _mm_sub_epi32(x6[55], x6[52]);
  x7[55] = _mm_add_epi32(x6[55], x6[52]);
  x7[53] = _mm_sub_epi32(x6[54], x6[53]);
  x7[54] = _mm_add_epi32(x6[54], x6[53]);
  x7[56] = _mm_add_epi32(x6[56], x6[59]);
  x7[59] = _mm_sub_epi32(x6[56], x6[59]);
  x7[57] = _mm_add_epi32(x6[57], x6[58]);
  x7[58] = _mm_sub_epi32(x6[57], x6[58]);
  x7[60] = _mm_sub_epi32(x6[63], x6[60]);
  x7[63] = _mm_add_epi32(x6[63], x6[60]);
  x7[61] = _mm_sub_epi32(x6[62], x6[61]);
  x7[62] = _mm_add_epi32(x6[62], x6[61]);

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
  btf_32_type1_sse4_1_new(cospi_p60, cospi_p04, x7[8], x7[15], x8[8], x8[15],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p28, cospi_p36, x7[9], x7[14], x8[9], x8[14],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p44, cospi_p20, x7[10], x7[13], x8[10], x8[13],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p12, cospi_p52, x7[11], x7[12], x8[11], x8[12],
                          __rounding, cos_bit);
  x8[16] = _mm_add_epi32(x7[16], x7[17]);
  x8[17] = _mm_sub_epi32(x7[16], x7[17]);
  x8[18] = _mm_sub_epi32(x7[19], x7[18]);
  x8[19] = _mm_add_epi32(x7[19], x7[18]);
  x8[20] = _mm_add_epi32(x7[20], x7[21]);
  x8[21] = _mm_sub_epi32(x7[20], x7[21]);
  x8[22] = _mm_sub_epi32(x7[23], x7[22]);
  x8[23] = _mm_add_epi32(x7[23], x7[22]);
  x8[24] = _mm_add_epi32(x7[24], x7[25]);
  x8[25] = _mm_sub_epi32(x7[24], x7[25]);
  x8[26] = _mm_sub_epi32(x7[27], x7[26]);
  x8[27] = _mm_add_epi32(x7[27], x7[26]);
  x8[28] = _mm_add_epi32(x7[28], x7[29]);
  x8[29] = _mm_sub_epi32(x7[28], x7[29]);
  x8[30] = _mm_sub_epi32(x7[31], x7[30]);
  x8[31] = _mm_add_epi32(x7[31], x7[30]);
  x8[32] = x7[32];
  btf_32_type0_sse4_1_new(cospi_m04, cospi_p60, x7[33], x7[62], x8[33], x8[62],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m60, cospi_m04, x7[34], x7[61], x8[34], x8[61],
                          __rounding, cos_bit);
  x8[35] = x7[35];
  x8[36] = x7[36];
  btf_32_type0_sse4_1_new(cospi_m36, cospi_p28, x7[37], x7[58], x8[37], x8[58],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m28, cospi_m36, x7[38], x7[57], x8[38], x8[57],
                          __rounding, cos_bit);
  x8[39] = x7[39];
  x8[40] = x7[40];
  btf_32_type0_sse4_1_new(cospi_m20, cospi_p44, x7[41], x7[54], x8[41], x8[54],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m44, cospi_m20, x7[42], x7[53], x8[42], x8[53],
                          __rounding, cos_bit);
  x8[43] = x7[43];
  x8[44] = x7[44];
  btf_32_type0_sse4_1_new(cospi_m52, cospi_p12, x7[45], x7[50], x8[45], x8[50],
                          __rounding, cos_bit);
  btf_32_type0_sse4_1_new(cospi_m12, cospi_m52, x7[46], x7[49], x8[46], x8[49],
                          __rounding, cos_bit);
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
  btf_32_type1_sse4_1_new(cospi_p62, cospi_p02, x8[16], x8[31], x9[16], x9[31],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p30, cospi_p34, x8[17], x8[30], x9[17], x9[30],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p46, cospi_p18, x8[18], x8[29], x9[18], x9[29],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p14, cospi_p50, x8[19], x8[28], x9[19], x9[28],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p54, cospi_p10, x8[20], x8[27], x9[20], x9[27],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p22, cospi_p42, x8[21], x8[26], x9[21], x9[26],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p38, cospi_p26, x8[22], x8[25], x9[22], x9[25],
                          __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p06, cospi_p58, x8[23], x8[24], x9[23], x9[24],
                          __rounding, cos_bit);
  x9[32] = _mm_add_epi32(x8[32], x8[33]);
  x9[33] = _mm_sub_epi32(x8[32], x8[33]);
  x9[34] = _mm_sub_epi32(x8[35], x8[34]);
  x9[35] = _mm_add_epi32(x8[35], x8[34]);
  x9[36] = _mm_add_epi32(x8[36], x8[37]);
  x9[37] = _mm_sub_epi32(x8[36], x8[37]);
  x9[38] = _mm_sub_epi32(x8[39], x8[38]);
  x9[39] = _mm_add_epi32(x8[39], x8[38]);
  x9[40] = _mm_add_epi32(x8[40], x8[41]);
  x9[41] = _mm_sub_epi32(x8[40], x8[41]);
  x9[42] = _mm_sub_epi32(x8[43], x8[42]);
  x9[43] = _mm_add_epi32(x8[43], x8[42]);
  x9[44] = _mm_add_epi32(x8[44], x8[45]);
  x9[45] = _mm_sub_epi32(x8[44], x8[45]);
  x9[46] = _mm_sub_epi32(x8[47], x8[46]);
  x9[47] = _mm_add_epi32(x8[47], x8[46]);
  x9[48] = _mm_add_epi32(x8[48], x8[49]);
  x9[49] = _mm_sub_epi32(x8[48], x8[49]);
  x9[50] = _mm_sub_epi32(x8[51], x8[50]);
  x9[51] = _mm_add_epi32(x8[51], x8[50]);
  x9[52] = _mm_add_epi32(x8[52], x8[53]);
  x9[53] = _mm_sub_epi32(x8[52], x8[53]);
  x9[54] = _mm_sub_epi32(x8[55], x8[54]);
  x9[55] = _mm_add_epi32(x8[55], x8[54]);
  x9[56] = _mm_add_epi32(x8[56], x8[57]);
  x9[57] = _mm_sub_epi32(x8[56], x8[57]);
  x9[58] = _mm_sub_epi32(x8[59], x8[58]);
  x9[59] = _mm_add_epi32(x8[59], x8[58]);
  x9[60] = _mm_add_epi32(x8[60], x8[61]);
  x9[61] = _mm_sub_epi32(x8[60], x8[61]);
  x9[62] = _mm_sub_epi32(x8[63], x8[62]);
  x9[63] = _mm_add_epi32(x8[63], x8[62]);

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
  btf_32_type1_sse4_1_new(cospi_p63, cospi_p01, x9[32], x9[63], x10[32],
                          x10[63], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p31, cospi_p33, x9[33], x9[62], x10[33],
                          x10[62], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p47, cospi_p17, x9[34], x9[61], x10[34],
                          x10[61], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p15, cospi_p49, x9[35], x9[60], x10[35],
                          x10[60], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p55, cospi_p09, x9[36], x9[59], x10[36],
                          x10[59], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p23, cospi_p41, x9[37], x9[58], x10[37],
                          x10[58], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p39, cospi_p25, x9[38], x9[57], x10[38],
                          x10[57], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p07, cospi_p57, x9[39], x9[56], x10[39],
                          x10[56], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p59, cospi_p05, x9[40], x9[55], x10[40],
                          x10[55], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p27, cospi_p37, x9[41], x9[54], x10[41],
                          x10[54], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p43, cospi_p21, x9[42], x9[53], x10[42],
                          x10[53], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p11, cospi_p53, x9[43], x9[52], x10[43],
                          x10[52], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p51, cospi_p13, x9[44], x9[51], x10[44],
                          x10[51], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p19, cospi_p45, x9[45], x9[50], x10[45],
                          x10[50], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p35, cospi_p29, x9[46], x9[49], x10[46],
                          x10[49], __rounding, cos_bit);
  btf_32_type1_sse4_1_new(cospi_p03, cospi_p61, x9[47], x9[48], x10[47],
                          x10[48], __rounding, cos_bit);

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

void av1_idtx32_sse4_1(__m128i *input, __m128i *output, int cos_bit,
                       const int col_num) {
  (void)cos_bit;
  for (int i = 0; i < 32; i++) {
    output[i * col_num] = _mm_slli_epi32(input[i * col_num], 2);
  }
}
