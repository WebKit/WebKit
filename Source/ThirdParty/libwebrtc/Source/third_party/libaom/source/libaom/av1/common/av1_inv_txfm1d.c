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

#include <stdlib.h>
#include "av1/common/av1_inv_txfm1d.h"
#include "av1/common/av1_txfm.h"

void av1_idct4(const int32_t *input, int32_t *output, int8_t cos_bit,
               const int8_t *stage_range) {
  assert(output != input);
  const int32_t size = 4;
  const int32_t *cospi = cospi_arr(cos_bit);

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[4];

  // stage 0;

  // stage 1;
  stage++;
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[2];
  bf1[2] = input[1];
  bf1[3] = input[3];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
  bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
  bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
}

void av1_idct8(const int32_t *input, int32_t *output, int8_t cos_bit,
               const int8_t *stage_range) {
  assert(output != input);
  const int32_t size = 8;
  const int32_t *cospi = cospi_arr(cos_bit);

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[8];

  // stage 0;

  // stage 1;
  stage++;
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[4];
  bf1[2] = input[2];
  bf1[3] = input[6];
  bf1[4] = input[1];
  bf1[5] = input[5];
  bf1[6] = input[3];
  bf1[7] = input[7];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit);
  bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit);
  bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
  bf1[4] = clamp_value(bf0[4] + bf0[5], stage_range[stage]);
  bf1[5] = clamp_value(bf0[4] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(-bf0[6] + bf0[7], stage_range[stage]);
  bf1[7] = clamp_value(bf0[6] + bf0[7], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
  bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
  bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[7] = bf0[7];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[7], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[6], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[5], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[4], stage_range[stage]);
  bf1[4] = clamp_value(bf0[3] - bf0[4], stage_range[stage]);
  bf1[5] = clamp_value(bf0[2] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(bf0[1] - bf0[6], stage_range[stage]);
  bf1[7] = clamp_value(bf0[0] - bf0[7], stage_range[stage]);
}

void av1_idct16(const int32_t *input, int32_t *output, int8_t cos_bit,
                const int8_t *stage_range) {
  assert(output != input);
  const int32_t size = 16;
  const int32_t *cospi = cospi_arr(cos_bit);

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[16];

  // stage 0;

  // stage 1;
  stage++;
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[8];
  bf1[2] = input[4];
  bf1[3] = input[12];
  bf1[4] = input[2];
  bf1[5] = input[10];
  bf1[6] = input[6];
  bf1[7] = input[14];
  bf1[8] = input[1];
  bf1[9] = input[9];
  bf1[10] = input[5];
  bf1[11] = input[13];
  bf1[12] = input[3];
  bf1[13] = input[11];
  bf1[14] = input[7];
  bf1[15] = input[15];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[60], bf0[8], -cospi[4], bf0[15], cos_bit);
  bf1[9] = half_btf(cospi[28], bf0[9], -cospi[36], bf0[14], cos_bit);
  bf1[10] = half_btf(cospi[44], bf0[10], -cospi[20], bf0[13], cos_bit);
  bf1[11] = half_btf(cospi[12], bf0[11], -cospi[52], bf0[12], cos_bit);
  bf1[12] = half_btf(cospi[52], bf0[11], cospi[12], bf0[12], cos_bit);
  bf1[13] = half_btf(cospi[20], bf0[10], cospi[44], bf0[13], cos_bit);
  bf1[14] = half_btf(cospi[36], bf0[9], cospi[28], bf0[14], cos_bit);
  bf1[15] = half_btf(cospi[4], bf0[8], cospi[60], bf0[15], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit);
  bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit);
  bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit);
  bf1[8] = clamp_value(bf0[8] + bf0[9], stage_range[stage]);
  bf1[9] = clamp_value(bf0[8] - bf0[9], stage_range[stage]);
  bf1[10] = clamp_value(-bf0[10] + bf0[11], stage_range[stage]);
  bf1[11] = clamp_value(bf0[10] + bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(bf0[12] + bf0[13], stage_range[stage]);
  bf1[13] = clamp_value(bf0[12] - bf0[13], stage_range[stage]);
  bf1[14] = clamp_value(-bf0[14] + bf0[15], stage_range[stage]);
  bf1[15] = clamp_value(bf0[14] + bf0[15], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
  bf1[4] = clamp_value(bf0[4] + bf0[5], stage_range[stage]);
  bf1[5] = clamp_value(bf0[4] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(-bf0[6] + bf0[7], stage_range[stage]);
  bf1[7] = clamp_value(bf0[6] + bf0[7], stage_range[stage]);
  bf1[8] = bf0[8];
  bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit);
  bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit);
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = half_btf(-cospi[16], bf0[10], cospi[48], bf0[13], cos_bit);
  bf1[14] = half_btf(cospi[48], bf0[9], cospi[16], bf0[14], cos_bit);
  bf1[15] = bf0[15];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
  bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
  bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[7] = bf0[7];
  bf1[8] = clamp_value(bf0[8] + bf0[11], stage_range[stage]);
  bf1[9] = clamp_value(bf0[9] + bf0[10], stage_range[stage]);
  bf1[10] = clamp_value(bf0[9] - bf0[10], stage_range[stage]);
  bf1[11] = clamp_value(bf0[8] - bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(-bf0[12] + bf0[15], stage_range[stage]);
  bf1[13] = clamp_value(-bf0[13] + bf0[14], stage_range[stage]);
  bf1[14] = clamp_value(bf0[13] + bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[12] + bf0[15], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = clamp_value(bf0[0] + bf0[7], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[6], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[5], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[4], stage_range[stage]);
  bf1[4] = clamp_value(bf0[3] - bf0[4], stage_range[stage]);
  bf1[5] = clamp_value(bf0[2] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(bf0[1] - bf0[6], stage_range[stage]);
  bf1[7] = clamp_value(bf0[0] - bf0[7], stage_range[stage]);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
  bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
  bf1[12] = half_btf(cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
  bf1[13] = half_btf(cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[15], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[14], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[13], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[12], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[11], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[10], stage_range[stage]);
  bf1[6] = clamp_value(bf0[6] + bf0[9], stage_range[stage]);
  bf1[7] = clamp_value(bf0[7] + bf0[8], stage_range[stage]);
  bf1[8] = clamp_value(bf0[7] - bf0[8], stage_range[stage]);
  bf1[9] = clamp_value(bf0[6] - bf0[9], stage_range[stage]);
  bf1[10] = clamp_value(bf0[5] - bf0[10], stage_range[stage]);
  bf1[11] = clamp_value(bf0[4] - bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(bf0[3] - bf0[12], stage_range[stage]);
  bf1[13] = clamp_value(bf0[2] - bf0[13], stage_range[stage]);
  bf1[14] = clamp_value(bf0[1] - bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[0] - bf0[15], stage_range[stage]);
}

void av1_idct32(const int32_t *input, int32_t *output, int8_t cos_bit,
                const int8_t *stage_range) {
  assert(output != input);
  const int32_t size = 32;
  const int32_t *cospi = cospi_arr(cos_bit);

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[32];

  // stage 0;

  // stage 1;
  stage++;
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[16];
  bf1[2] = input[8];
  bf1[3] = input[24];
  bf1[4] = input[4];
  bf1[5] = input[20];
  bf1[6] = input[12];
  bf1[7] = input[28];
  bf1[8] = input[2];
  bf1[9] = input[18];
  bf1[10] = input[10];
  bf1[11] = input[26];
  bf1[12] = input[6];
  bf1[13] = input[22];
  bf1[14] = input[14];
  bf1[15] = input[30];
  bf1[16] = input[1];
  bf1[17] = input[17];
  bf1[18] = input[9];
  bf1[19] = input[25];
  bf1[20] = input[5];
  bf1[21] = input[21];
  bf1[22] = input[13];
  bf1[23] = input[29];
  bf1[24] = input[3];
  bf1[25] = input[19];
  bf1[26] = input[11];
  bf1[27] = input[27];
  bf1[28] = input[7];
  bf1[29] = input[23];
  bf1[30] = input[15];
  bf1[31] = input[31];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = half_btf(cospi[62], bf0[16], -cospi[2], bf0[31], cos_bit);
  bf1[17] = half_btf(cospi[30], bf0[17], -cospi[34], bf0[30], cos_bit);
  bf1[18] = half_btf(cospi[46], bf0[18], -cospi[18], bf0[29], cos_bit);
  bf1[19] = half_btf(cospi[14], bf0[19], -cospi[50], bf0[28], cos_bit);
  bf1[20] = half_btf(cospi[54], bf0[20], -cospi[10], bf0[27], cos_bit);
  bf1[21] = half_btf(cospi[22], bf0[21], -cospi[42], bf0[26], cos_bit);
  bf1[22] = half_btf(cospi[38], bf0[22], -cospi[26], bf0[25], cos_bit);
  bf1[23] = half_btf(cospi[6], bf0[23], -cospi[58], bf0[24], cos_bit);
  bf1[24] = half_btf(cospi[58], bf0[23], cospi[6], bf0[24], cos_bit);
  bf1[25] = half_btf(cospi[26], bf0[22], cospi[38], bf0[25], cos_bit);
  bf1[26] = half_btf(cospi[42], bf0[21], cospi[22], bf0[26], cos_bit);
  bf1[27] = half_btf(cospi[10], bf0[20], cospi[54], bf0[27], cos_bit);
  bf1[28] = half_btf(cospi[50], bf0[19], cospi[14], bf0[28], cos_bit);
  bf1[29] = half_btf(cospi[18], bf0[18], cospi[46], bf0[29], cos_bit);
  bf1[30] = half_btf(cospi[34], bf0[17], cospi[30], bf0[30], cos_bit);
  bf1[31] = half_btf(cospi[2], bf0[16], cospi[62], bf0[31], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[60], bf0[8], -cospi[4], bf0[15], cos_bit);
  bf1[9] = half_btf(cospi[28], bf0[9], -cospi[36], bf0[14], cos_bit);
  bf1[10] = half_btf(cospi[44], bf0[10], -cospi[20], bf0[13], cos_bit);
  bf1[11] = half_btf(cospi[12], bf0[11], -cospi[52], bf0[12], cos_bit);
  bf1[12] = half_btf(cospi[52], bf0[11], cospi[12], bf0[12], cos_bit);
  bf1[13] = half_btf(cospi[20], bf0[10], cospi[44], bf0[13], cos_bit);
  bf1[14] = half_btf(cospi[36], bf0[9], cospi[28], bf0[14], cos_bit);
  bf1[15] = half_btf(cospi[4], bf0[8], cospi[60], bf0[15], cos_bit);
  bf1[16] = clamp_value(bf0[16] + bf0[17], stage_range[stage]);
  bf1[17] = clamp_value(bf0[16] - bf0[17], stage_range[stage]);
  bf1[18] = clamp_value(-bf0[18] + bf0[19], stage_range[stage]);
  bf1[19] = clamp_value(bf0[18] + bf0[19], stage_range[stage]);
  bf1[20] = clamp_value(bf0[20] + bf0[21], stage_range[stage]);
  bf1[21] = clamp_value(bf0[20] - bf0[21], stage_range[stage]);
  bf1[22] = clamp_value(-bf0[22] + bf0[23], stage_range[stage]);
  bf1[23] = clamp_value(bf0[22] + bf0[23], stage_range[stage]);
  bf1[24] = clamp_value(bf0[24] + bf0[25], stage_range[stage]);
  bf1[25] = clamp_value(bf0[24] - bf0[25], stage_range[stage]);
  bf1[26] = clamp_value(-bf0[26] + bf0[27], stage_range[stage]);
  bf1[27] = clamp_value(bf0[26] + bf0[27], stage_range[stage]);
  bf1[28] = clamp_value(bf0[28] + bf0[29], stage_range[stage]);
  bf1[29] = clamp_value(bf0[28] - bf0[29], stage_range[stage]);
  bf1[30] = clamp_value(-bf0[30] + bf0[31], stage_range[stage]);
  bf1[31] = clamp_value(bf0[30] + bf0[31], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit);
  bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit);
  bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit);
  bf1[8] = clamp_value(bf0[8] + bf0[9], stage_range[stage]);
  bf1[9] = clamp_value(bf0[8] - bf0[9], stage_range[stage]);
  bf1[10] = clamp_value(-bf0[10] + bf0[11], stage_range[stage]);
  bf1[11] = clamp_value(bf0[10] + bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(bf0[12] + bf0[13], stage_range[stage]);
  bf1[13] = clamp_value(bf0[12] - bf0[13], stage_range[stage]);
  bf1[14] = clamp_value(-bf0[14] + bf0[15], stage_range[stage]);
  bf1[15] = clamp_value(bf0[14] + bf0[15], stage_range[stage]);
  bf1[16] = bf0[16];
  bf1[17] = half_btf(-cospi[8], bf0[17], cospi[56], bf0[30], cos_bit);
  bf1[18] = half_btf(-cospi[56], bf0[18], -cospi[8], bf0[29], cos_bit);
  bf1[19] = bf0[19];
  bf1[20] = bf0[20];
  bf1[21] = half_btf(-cospi[40], bf0[21], cospi[24], bf0[26], cos_bit);
  bf1[22] = half_btf(-cospi[24], bf0[22], -cospi[40], bf0[25], cos_bit);
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = half_btf(-cospi[40], bf0[22], cospi[24], bf0[25], cos_bit);
  bf1[26] = half_btf(cospi[24], bf0[21], cospi[40], bf0[26], cos_bit);
  bf1[27] = bf0[27];
  bf1[28] = bf0[28];
  bf1[29] = half_btf(-cospi[8], bf0[18], cospi[56], bf0[29], cos_bit);
  bf1[30] = half_btf(cospi[56], bf0[17], cospi[8], bf0[30], cos_bit);
  bf1[31] = bf0[31];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
  bf1[4] = clamp_value(bf0[4] + bf0[5], stage_range[stage]);
  bf1[5] = clamp_value(bf0[4] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(-bf0[6] + bf0[7], stage_range[stage]);
  bf1[7] = clamp_value(bf0[6] + bf0[7], stage_range[stage]);
  bf1[8] = bf0[8];
  bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit);
  bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit);
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = half_btf(-cospi[16], bf0[10], cospi[48], bf0[13], cos_bit);
  bf1[14] = half_btf(cospi[48], bf0[9], cospi[16], bf0[14], cos_bit);
  bf1[15] = bf0[15];
  bf1[16] = clamp_value(bf0[16] + bf0[19], stage_range[stage]);
  bf1[17] = clamp_value(bf0[17] + bf0[18], stage_range[stage]);
  bf1[18] = clamp_value(bf0[17] - bf0[18], stage_range[stage]);
  bf1[19] = clamp_value(bf0[16] - bf0[19], stage_range[stage]);
  bf1[20] = clamp_value(-bf0[20] + bf0[23], stage_range[stage]);
  bf1[21] = clamp_value(-bf0[21] + bf0[22], stage_range[stage]);
  bf1[22] = clamp_value(bf0[21] + bf0[22], stage_range[stage]);
  bf1[23] = clamp_value(bf0[20] + bf0[23], stage_range[stage]);
  bf1[24] = clamp_value(bf0[24] + bf0[27], stage_range[stage]);
  bf1[25] = clamp_value(bf0[25] + bf0[26], stage_range[stage]);
  bf1[26] = clamp_value(bf0[25] - bf0[26], stage_range[stage]);
  bf1[27] = clamp_value(bf0[24] - bf0[27], stage_range[stage]);
  bf1[28] = clamp_value(-bf0[28] + bf0[31], stage_range[stage]);
  bf1[29] = clamp_value(-bf0[29] + bf0[30], stage_range[stage]);
  bf1[30] = clamp_value(bf0[29] + bf0[30], stage_range[stage]);
  bf1[31] = clamp_value(bf0[28] + bf0[31], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
  bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
  bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[7] = bf0[7];
  bf1[8] = clamp_value(bf0[8] + bf0[11], stage_range[stage]);
  bf1[9] = clamp_value(bf0[9] + bf0[10], stage_range[stage]);
  bf1[10] = clamp_value(bf0[9] - bf0[10], stage_range[stage]);
  bf1[11] = clamp_value(bf0[8] - bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(-bf0[12] + bf0[15], stage_range[stage]);
  bf1[13] = clamp_value(-bf0[13] + bf0[14], stage_range[stage]);
  bf1[14] = clamp_value(bf0[13] + bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[12] + bf0[15], stage_range[stage]);
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = half_btf(-cospi[16], bf0[18], cospi[48], bf0[29], cos_bit);
  bf1[19] = half_btf(-cospi[16], bf0[19], cospi[48], bf0[28], cos_bit);
  bf1[20] = half_btf(-cospi[48], bf0[20], -cospi[16], bf0[27], cos_bit);
  bf1[21] = half_btf(-cospi[48], bf0[21], -cospi[16], bf0[26], cos_bit);
  bf1[22] = bf0[22];
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = half_btf(-cospi[16], bf0[21], cospi[48], bf0[26], cos_bit);
  bf1[27] = half_btf(-cospi[16], bf0[20], cospi[48], bf0[27], cos_bit);
  bf1[28] = half_btf(cospi[48], bf0[19], cospi[16], bf0[28], cos_bit);
  bf1[29] = half_btf(cospi[48], bf0[18], cospi[16], bf0[29], cos_bit);
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[7], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[6], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[5], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[4], stage_range[stage]);
  bf1[4] = clamp_value(bf0[3] - bf0[4], stage_range[stage]);
  bf1[5] = clamp_value(bf0[2] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(bf0[1] - bf0[6], stage_range[stage]);
  bf1[7] = clamp_value(bf0[0] - bf0[7], stage_range[stage]);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
  bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
  bf1[12] = half_btf(cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
  bf1[13] = half_btf(cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = clamp_value(bf0[16] + bf0[23], stage_range[stage]);
  bf1[17] = clamp_value(bf0[17] + bf0[22], stage_range[stage]);
  bf1[18] = clamp_value(bf0[18] + bf0[21], stage_range[stage]);
  bf1[19] = clamp_value(bf0[19] + bf0[20], stage_range[stage]);
  bf1[20] = clamp_value(bf0[19] - bf0[20], stage_range[stage]);
  bf1[21] = clamp_value(bf0[18] - bf0[21], stage_range[stage]);
  bf1[22] = clamp_value(bf0[17] - bf0[22], stage_range[stage]);
  bf1[23] = clamp_value(bf0[16] - bf0[23], stage_range[stage]);
  bf1[24] = clamp_value(-bf0[24] + bf0[31], stage_range[stage]);
  bf1[25] = clamp_value(-bf0[25] + bf0[30], stage_range[stage]);
  bf1[26] = clamp_value(-bf0[26] + bf0[29], stage_range[stage]);
  bf1[27] = clamp_value(-bf0[27] + bf0[28], stage_range[stage]);
  bf1[28] = clamp_value(bf0[27] + bf0[28], stage_range[stage]);
  bf1[29] = clamp_value(bf0[26] + bf0[29], stage_range[stage]);
  bf1[30] = clamp_value(bf0[25] + bf0[30], stage_range[stage]);
  bf1[31] = clamp_value(bf0[24] + bf0[31], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 8
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = clamp_value(bf0[0] + bf0[15], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[14], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[13], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[12], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[11], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[10], stage_range[stage]);
  bf1[6] = clamp_value(bf0[6] + bf0[9], stage_range[stage]);
  bf1[7] = clamp_value(bf0[7] + bf0[8], stage_range[stage]);
  bf1[8] = clamp_value(bf0[7] - bf0[8], stage_range[stage]);
  bf1[9] = clamp_value(bf0[6] - bf0[9], stage_range[stage]);
  bf1[10] = clamp_value(bf0[5] - bf0[10], stage_range[stage]);
  bf1[11] = clamp_value(bf0[4] - bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(bf0[3] - bf0[12], stage_range[stage]);
  bf1[13] = clamp_value(bf0[2] - bf0[13], stage_range[stage]);
  bf1[14] = clamp_value(bf0[1] - bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[0] - bf0[15], stage_range[stage]);
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = half_btf(-cospi[32], bf0[20], cospi[32], bf0[27], cos_bit);
  bf1[21] = half_btf(-cospi[32], bf0[21], cospi[32], bf0[26], cos_bit);
  bf1[22] = half_btf(-cospi[32], bf0[22], cospi[32], bf0[25], cos_bit);
  bf1[23] = half_btf(-cospi[32], bf0[23], cospi[32], bf0[24], cos_bit);
  bf1[24] = half_btf(cospi[32], bf0[23], cospi[32], bf0[24], cos_bit);
  bf1[25] = half_btf(cospi[32], bf0[22], cospi[32], bf0[25], cos_bit);
  bf1[26] = half_btf(cospi[32], bf0[21], cospi[32], bf0[26], cos_bit);
  bf1[27] = half_btf(cospi[32], bf0[20], cospi[32], bf0[27], cos_bit);
  bf1[28] = bf0[28];
  bf1[29] = bf0[29];
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 9
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[31], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[30], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[29], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[28], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[27], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[26], stage_range[stage]);
  bf1[6] = clamp_value(bf0[6] + bf0[25], stage_range[stage]);
  bf1[7] = clamp_value(bf0[7] + bf0[24], stage_range[stage]);
  bf1[8] = clamp_value(bf0[8] + bf0[23], stage_range[stage]);
  bf1[9] = clamp_value(bf0[9] + bf0[22], stage_range[stage]);
  bf1[10] = clamp_value(bf0[10] + bf0[21], stage_range[stage]);
  bf1[11] = clamp_value(bf0[11] + bf0[20], stage_range[stage]);
  bf1[12] = clamp_value(bf0[12] + bf0[19], stage_range[stage]);
  bf1[13] = clamp_value(bf0[13] + bf0[18], stage_range[stage]);
  bf1[14] = clamp_value(bf0[14] + bf0[17], stage_range[stage]);
  bf1[15] = clamp_value(bf0[15] + bf0[16], stage_range[stage]);
  bf1[16] = clamp_value(bf0[15] - bf0[16], stage_range[stage]);
  bf1[17] = clamp_value(bf0[14] - bf0[17], stage_range[stage]);
  bf1[18] = clamp_value(bf0[13] - bf0[18], stage_range[stage]);
  bf1[19] = clamp_value(bf0[12] - bf0[19], stage_range[stage]);
  bf1[20] = clamp_value(bf0[11] - bf0[20], stage_range[stage]);
  bf1[21] = clamp_value(bf0[10] - bf0[21], stage_range[stage]);
  bf1[22] = clamp_value(bf0[9] - bf0[22], stage_range[stage]);
  bf1[23] = clamp_value(bf0[8] - bf0[23], stage_range[stage]);
  bf1[24] = clamp_value(bf0[7] - bf0[24], stage_range[stage]);
  bf1[25] = clamp_value(bf0[6] - bf0[25], stage_range[stage]);
  bf1[26] = clamp_value(bf0[5] - bf0[26], stage_range[stage]);
  bf1[27] = clamp_value(bf0[4] - bf0[27], stage_range[stage]);
  bf1[28] = clamp_value(bf0[3] - bf0[28], stage_range[stage]);
  bf1[29] = clamp_value(bf0[2] - bf0[29], stage_range[stage]);
  bf1[30] = clamp_value(bf0[1] - bf0[30], stage_range[stage]);
  bf1[31] = clamp_value(bf0[0] - bf0[31], stage_range[stage]);
}

void av1_iadst4(const int32_t *input, int32_t *output, int8_t cos_bit,
                const int8_t *stage_range) {
  int bit = cos_bit;
  const int32_t *sinpi = sinpi_arr(bit);
  int32_t s0, s1, s2, s3, s4, s5, s6, s7;

  int32_t x0 = input[0];
  int32_t x1 = input[1];
  int32_t x2 = input[2];
  int32_t x3 = input[3];

  if (!(x0 | x1 | x2 | x3)) {
    output[0] = output[1] = output[2] = output[3] = 0;
    return;
  }

  assert(sinpi[1] + sinpi[2] == sinpi[4]);

  // stage 1
  s0 = range_check_value(sinpi[1] * x0, stage_range[1] + bit);
  s1 = range_check_value(sinpi[2] * x0, stage_range[1] + bit);
  s2 = range_check_value(sinpi[3] * x1, stage_range[1] + bit);
  s3 = range_check_value(sinpi[4] * x2, stage_range[1] + bit);
  s4 = range_check_value(sinpi[1] * x2, stage_range[1] + bit);
  s5 = range_check_value(sinpi[2] * x3, stage_range[1] + bit);
  s6 = range_check_value(sinpi[4] * x3, stage_range[1] + bit);

  // stage 2
  // NOTICE: (x0 - x2) here may use one extra bit compared to the
  // opt_range_row/col specified in av1_gen_inv_stage_range()
  s7 = range_check_value((x0 - x2) + x3, stage_range[2]);

  // stage 3
  s0 = range_check_value(s0 + s3, stage_range[3] + bit);
  s1 = range_check_value(s1 - s4, stage_range[3] + bit);
  s3 = range_check_value(s2, stage_range[3] + bit);
  s2 = range_check_value(sinpi[3] * s7, stage_range[3] + bit);

  // stage 4
  s0 = range_check_value(s0 + s5, stage_range[4] + bit);
  s1 = range_check_value(s1 - s6, stage_range[4] + bit);

  // stage 5
  x0 = range_check_value(s0 + s3, stage_range[5] + bit);
  x1 = range_check_value(s1 + s3, stage_range[5] + bit);
  x2 = range_check_value(s2, stage_range[5] + bit);
  x3 = range_check_value(s0 + s1, stage_range[5] + bit);

  // stage 6
  x3 = range_check_value(x3 - s3, stage_range[6] + bit);

  output[0] = round_shift(x0, bit);
  output[1] = round_shift(x1, bit);
  output[2] = round_shift(x2, bit);
  output[3] = round_shift(x3, bit);
}

void av1_iadst8(const int32_t *input, int32_t *output, int8_t cos_bit,
                const int8_t *stage_range) {
  assert(output != input);
  const int32_t size = 8;
  const int32_t *cospi = cospi_arr(cos_bit);

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[8];

  // stage 0;

  // stage 1;
  stage++;
  bf1 = output;
  bf1[0] = input[7];
  bf1[1] = input[0];
  bf1[2] = input[5];
  bf1[3] = input[2];
  bf1[4] = input[3];
  bf1[5] = input[4];
  bf1[6] = input[1];
  bf1[7] = input[6];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[4], bf0[0], cospi[60], bf0[1], cos_bit);
  bf1[1] = half_btf(cospi[60], bf0[0], -cospi[4], bf0[1], cos_bit);
  bf1[2] = half_btf(cospi[20], bf0[2], cospi[44], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[44], bf0[2], -cospi[20], bf0[3], cos_bit);
  bf1[4] = half_btf(cospi[36], bf0[4], cospi[28], bf0[5], cos_bit);
  bf1[5] = half_btf(cospi[28], bf0[4], -cospi[36], bf0[5], cos_bit);
  bf1[6] = half_btf(cospi[52], bf0[6], cospi[12], bf0[7], cos_bit);
  bf1[7] = half_btf(cospi[12], bf0[6], -cospi[52], bf0[7], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[4], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[5], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[6], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[7], stage_range[stage]);
  bf1[4] = clamp_value(bf0[0] - bf0[4], stage_range[stage]);
  bf1[5] = clamp_value(bf0[1] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(bf0[2] - bf0[6], stage_range[stage]);
  bf1[7] = clamp_value(bf0[3] - bf0[7], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit);
  bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit);
  bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit);
  bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[2], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[3], stage_range[stage]);
  bf1[2] = clamp_value(bf0[0] - bf0[2], stage_range[stage]);
  bf1[3] = clamp_value(bf0[1] - bf0[3], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[6], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[7], stage_range[stage]);
  bf1[6] = clamp_value(bf0[4] - bf0[6], stage_range[stage]);
  bf1[7] = clamp_value(bf0[5] - bf0[7], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit);
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit);
  bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = -bf0[4];
  bf1[2] = bf0[6];
  bf1[3] = -bf0[2];
  bf1[4] = bf0[3];
  bf1[5] = -bf0[7];
  bf1[6] = bf0[5];
  bf1[7] = -bf0[1];
}

void av1_iadst16(const int32_t *input, int32_t *output, int8_t cos_bit,
                 const int8_t *stage_range) {
  assert(output != input);
  const int32_t size = 16;
  const int32_t *cospi = cospi_arr(cos_bit);

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[16];

  // stage 0;

  // stage 1;
  stage++;
  bf1 = output;
  bf1[0] = input[15];
  bf1[1] = input[0];
  bf1[2] = input[13];
  bf1[3] = input[2];
  bf1[4] = input[11];
  bf1[5] = input[4];
  bf1[6] = input[9];
  bf1[7] = input[6];
  bf1[8] = input[7];
  bf1[9] = input[8];
  bf1[10] = input[5];
  bf1[11] = input[10];
  bf1[12] = input[3];
  bf1[13] = input[12];
  bf1[14] = input[1];
  bf1[15] = input[14];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[2], bf0[0], cospi[62], bf0[1], cos_bit);
  bf1[1] = half_btf(cospi[62], bf0[0], -cospi[2], bf0[1], cos_bit);
  bf1[2] = half_btf(cospi[10], bf0[2], cospi[54], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[54], bf0[2], -cospi[10], bf0[3], cos_bit);
  bf1[4] = half_btf(cospi[18], bf0[4], cospi[46], bf0[5], cos_bit);
  bf1[5] = half_btf(cospi[46], bf0[4], -cospi[18], bf0[5], cos_bit);
  bf1[6] = half_btf(cospi[26], bf0[6], cospi[38], bf0[7], cos_bit);
  bf1[7] = half_btf(cospi[38], bf0[6], -cospi[26], bf0[7], cos_bit);
  bf1[8] = half_btf(cospi[34], bf0[8], cospi[30], bf0[9], cos_bit);
  bf1[9] = half_btf(cospi[30], bf0[8], -cospi[34], bf0[9], cos_bit);
  bf1[10] = half_btf(cospi[42], bf0[10], cospi[22], bf0[11], cos_bit);
  bf1[11] = half_btf(cospi[22], bf0[10], -cospi[42], bf0[11], cos_bit);
  bf1[12] = half_btf(cospi[50], bf0[12], cospi[14], bf0[13], cos_bit);
  bf1[13] = half_btf(cospi[14], bf0[12], -cospi[50], bf0[13], cos_bit);
  bf1[14] = half_btf(cospi[58], bf0[14], cospi[6], bf0[15], cos_bit);
  bf1[15] = half_btf(cospi[6], bf0[14], -cospi[58], bf0[15], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[8], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[9], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[10], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[11], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[12], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[13], stage_range[stage]);
  bf1[6] = clamp_value(bf0[6] + bf0[14], stage_range[stage]);
  bf1[7] = clamp_value(bf0[7] + bf0[15], stage_range[stage]);
  bf1[8] = clamp_value(bf0[0] - bf0[8], stage_range[stage]);
  bf1[9] = clamp_value(bf0[1] - bf0[9], stage_range[stage]);
  bf1[10] = clamp_value(bf0[2] - bf0[10], stage_range[stage]);
  bf1[11] = clamp_value(bf0[3] - bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(bf0[4] - bf0[12], stage_range[stage]);
  bf1[13] = clamp_value(bf0[5] - bf0[13], stage_range[stage]);
  bf1[14] = clamp_value(bf0[6] - bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[7] - bf0[15], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[8], bf0[8], cospi[56], bf0[9], cos_bit);
  bf1[9] = half_btf(cospi[56], bf0[8], -cospi[8], bf0[9], cos_bit);
  bf1[10] = half_btf(cospi[40], bf0[10], cospi[24], bf0[11], cos_bit);
  bf1[11] = half_btf(cospi[24], bf0[10], -cospi[40], bf0[11], cos_bit);
  bf1[12] = half_btf(-cospi[56], bf0[12], cospi[8], bf0[13], cos_bit);
  bf1[13] = half_btf(cospi[8], bf0[12], cospi[56], bf0[13], cos_bit);
  bf1[14] = half_btf(-cospi[24], bf0[14], cospi[40], bf0[15], cos_bit);
  bf1[15] = half_btf(cospi[40], bf0[14], cospi[24], bf0[15], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[4], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[5], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[6], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[7], stage_range[stage]);
  bf1[4] = clamp_value(bf0[0] - bf0[4], stage_range[stage]);
  bf1[5] = clamp_value(bf0[1] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(bf0[2] - bf0[6], stage_range[stage]);
  bf1[7] = clamp_value(bf0[3] - bf0[7], stage_range[stage]);
  bf1[8] = clamp_value(bf0[8] + bf0[12], stage_range[stage]);
  bf1[9] = clamp_value(bf0[9] + bf0[13], stage_range[stage]);
  bf1[10] = clamp_value(bf0[10] + bf0[14], stage_range[stage]);
  bf1[11] = clamp_value(bf0[11] + bf0[15], stage_range[stage]);
  bf1[12] = clamp_value(bf0[8] - bf0[12], stage_range[stage]);
  bf1[13] = clamp_value(bf0[9] - bf0[13], stage_range[stage]);
  bf1[14] = clamp_value(bf0[10] - bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[11] - bf0[15], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[16], bf0[4], cospi[48], bf0[5], cos_bit);
  bf1[5] = half_btf(cospi[48], bf0[4], -cospi[16], bf0[5], cos_bit);
  bf1[6] = half_btf(-cospi[48], bf0[6], cospi[16], bf0[7], cos_bit);
  bf1[7] = half_btf(cospi[16], bf0[6], cospi[48], bf0[7], cos_bit);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = half_btf(cospi[16], bf0[12], cospi[48], bf0[13], cos_bit);
  bf1[13] = half_btf(cospi[48], bf0[12], -cospi[16], bf0[13], cos_bit);
  bf1[14] = half_btf(-cospi[48], bf0[14], cospi[16], bf0[15], cos_bit);
  bf1[15] = half_btf(cospi[16], bf0[14], cospi[48], bf0[15], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[2], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[3], stage_range[stage]);
  bf1[2] = clamp_value(bf0[0] - bf0[2], stage_range[stage]);
  bf1[3] = clamp_value(bf0[1] - bf0[3], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[6], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[7], stage_range[stage]);
  bf1[6] = clamp_value(bf0[4] - bf0[6], stage_range[stage]);
  bf1[7] = clamp_value(bf0[5] - bf0[7], stage_range[stage]);
  bf1[8] = clamp_value(bf0[8] + bf0[10], stage_range[stage]);
  bf1[9] = clamp_value(bf0[9] + bf0[11], stage_range[stage]);
  bf1[10] = clamp_value(bf0[8] - bf0[10], stage_range[stage]);
  bf1[11] = clamp_value(bf0[9] - bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(bf0[12] + bf0[14], stage_range[stage]);
  bf1[13] = clamp_value(bf0[13] + bf0[15], stage_range[stage]);
  bf1[14] = clamp_value(bf0[12] - bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[13] - bf0[15], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 8
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = half_btf(cospi[32], bf0[2], cospi[32], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[32], bf0[2], -cospi[32], bf0[3], cos_bit);
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = half_btf(cospi[32], bf0[6], cospi[32], bf0[7], cos_bit);
  bf1[7] = half_btf(cospi[32], bf0[6], -cospi[32], bf0[7], cos_bit);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(cospi[32], bf0[10], cospi[32], bf0[11], cos_bit);
  bf1[11] = half_btf(cospi[32], bf0[10], -cospi[32], bf0[11], cos_bit);
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = half_btf(cospi[32], bf0[14], cospi[32], bf0[15], cos_bit);
  bf1[15] = half_btf(cospi[32], bf0[14], -cospi[32], bf0[15], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 9
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = -bf0[8];
  bf1[2] = bf0[12];
  bf1[3] = -bf0[4];
  bf1[4] = bf0[6];
  bf1[5] = -bf0[14];
  bf1[6] = bf0[10];
  bf1[7] = -bf0[2];
  bf1[8] = bf0[3];
  bf1[9] = -bf0[11];
  bf1[10] = bf0[15];
  bf1[11] = -bf0[7];
  bf1[12] = bf0[5];
  bf1[13] = -bf0[13];
  bf1[14] = bf0[9];
  bf1[15] = -bf0[1];
}

void av1_iidentity4_c(const int32_t *input, int32_t *output, int8_t cos_bit,
                      const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 4; ++i) {
    output[i] = round_shift((int64_t)NewSqrt2 * input[i], NewSqrt2Bits);
  }
  assert(stage_range[0] + NewSqrt2Bits <= 32);
}

void av1_iidentity8_c(const int32_t *input, int32_t *output, int8_t cos_bit,
                      const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 8; ++i) output[i] = (int32_t)((int64_t)input[i] * 2);
}

void av1_iidentity16_c(const int32_t *input, int32_t *output, int8_t cos_bit,
                       const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 16; ++i)
    output[i] = round_shift((int64_t)NewSqrt2 * 2 * input[i], NewSqrt2Bits);
  assert(stage_range[0] + NewSqrt2Bits <= 32);
}

void av1_iidentity32_c(const int32_t *input, int32_t *output, int8_t cos_bit,
                       const int8_t *stage_range) {
  (void)cos_bit;
  (void)stage_range;
  for (int i = 0; i < 32; ++i) output[i] = (int32_t)((int64_t)input[i] * 4);
}

void av1_idct64(const int32_t *input, int32_t *output, int8_t cos_bit,
                const int8_t *stage_range) {
  assert(output != input);
  const int32_t size = 64;
  const int32_t *cospi = cospi_arr(cos_bit);

  int32_t stage = 0;
  int32_t *bf0, *bf1;
  int32_t step[64];

  // stage 0;

  // stage 1;
  stage++;
  bf1 = output;
  bf1[0] = input[0];
  bf1[1] = input[32];
  bf1[2] = input[16];
  bf1[3] = input[48];
  bf1[4] = input[8];
  bf1[5] = input[40];
  bf1[6] = input[24];
  bf1[7] = input[56];
  bf1[8] = input[4];
  bf1[9] = input[36];
  bf1[10] = input[20];
  bf1[11] = input[52];
  bf1[12] = input[12];
  bf1[13] = input[44];
  bf1[14] = input[28];
  bf1[15] = input[60];
  bf1[16] = input[2];
  bf1[17] = input[34];
  bf1[18] = input[18];
  bf1[19] = input[50];
  bf1[20] = input[10];
  bf1[21] = input[42];
  bf1[22] = input[26];
  bf1[23] = input[58];
  bf1[24] = input[6];
  bf1[25] = input[38];
  bf1[26] = input[22];
  bf1[27] = input[54];
  bf1[28] = input[14];
  bf1[29] = input[46];
  bf1[30] = input[30];
  bf1[31] = input[62];
  bf1[32] = input[1];
  bf1[33] = input[33];
  bf1[34] = input[17];
  bf1[35] = input[49];
  bf1[36] = input[9];
  bf1[37] = input[41];
  bf1[38] = input[25];
  bf1[39] = input[57];
  bf1[40] = input[5];
  bf1[41] = input[37];
  bf1[42] = input[21];
  bf1[43] = input[53];
  bf1[44] = input[13];
  bf1[45] = input[45];
  bf1[46] = input[29];
  bf1[47] = input[61];
  bf1[48] = input[3];
  bf1[49] = input[35];
  bf1[50] = input[19];
  bf1[51] = input[51];
  bf1[52] = input[11];
  bf1[53] = input[43];
  bf1[54] = input[27];
  bf1[55] = input[59];
  bf1[56] = input[7];
  bf1[57] = input[39];
  bf1[58] = input[23];
  bf1[59] = input[55];
  bf1[60] = input[15];
  bf1[61] = input[47];
  bf1[62] = input[31];
  bf1[63] = input[63];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 2
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = bf0[20];
  bf1[21] = bf0[21];
  bf1[22] = bf0[22];
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = bf0[26];
  bf1[27] = bf0[27];
  bf1[28] = bf0[28];
  bf1[29] = bf0[29];
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  bf1[32] = half_btf(cospi[63], bf0[32], -cospi[1], bf0[63], cos_bit);
  bf1[33] = half_btf(cospi[31], bf0[33], -cospi[33], bf0[62], cos_bit);
  bf1[34] = half_btf(cospi[47], bf0[34], -cospi[17], bf0[61], cos_bit);
  bf1[35] = half_btf(cospi[15], bf0[35], -cospi[49], bf0[60], cos_bit);
  bf1[36] = half_btf(cospi[55], bf0[36], -cospi[9], bf0[59], cos_bit);
  bf1[37] = half_btf(cospi[23], bf0[37], -cospi[41], bf0[58], cos_bit);
  bf1[38] = half_btf(cospi[39], bf0[38], -cospi[25], bf0[57], cos_bit);
  bf1[39] = half_btf(cospi[7], bf0[39], -cospi[57], bf0[56], cos_bit);
  bf1[40] = half_btf(cospi[59], bf0[40], -cospi[5], bf0[55], cos_bit);
  bf1[41] = half_btf(cospi[27], bf0[41], -cospi[37], bf0[54], cos_bit);
  bf1[42] = half_btf(cospi[43], bf0[42], -cospi[21], bf0[53], cos_bit);
  bf1[43] = half_btf(cospi[11], bf0[43], -cospi[53], bf0[52], cos_bit);
  bf1[44] = half_btf(cospi[51], bf0[44], -cospi[13], bf0[51], cos_bit);
  bf1[45] = half_btf(cospi[19], bf0[45], -cospi[45], bf0[50], cos_bit);
  bf1[46] = half_btf(cospi[35], bf0[46], -cospi[29], bf0[49], cos_bit);
  bf1[47] = half_btf(cospi[3], bf0[47], -cospi[61], bf0[48], cos_bit);
  bf1[48] = half_btf(cospi[61], bf0[47], cospi[3], bf0[48], cos_bit);
  bf1[49] = half_btf(cospi[29], bf0[46], cospi[35], bf0[49], cos_bit);
  bf1[50] = half_btf(cospi[45], bf0[45], cospi[19], bf0[50], cos_bit);
  bf1[51] = half_btf(cospi[13], bf0[44], cospi[51], bf0[51], cos_bit);
  bf1[52] = half_btf(cospi[53], bf0[43], cospi[11], bf0[52], cos_bit);
  bf1[53] = half_btf(cospi[21], bf0[42], cospi[43], bf0[53], cos_bit);
  bf1[54] = half_btf(cospi[37], bf0[41], cospi[27], bf0[54], cos_bit);
  bf1[55] = half_btf(cospi[5], bf0[40], cospi[59], bf0[55], cos_bit);
  bf1[56] = half_btf(cospi[57], bf0[39], cospi[7], bf0[56], cos_bit);
  bf1[57] = half_btf(cospi[25], bf0[38], cospi[39], bf0[57], cos_bit);
  bf1[58] = half_btf(cospi[41], bf0[37], cospi[23], bf0[58], cos_bit);
  bf1[59] = half_btf(cospi[9], bf0[36], cospi[55], bf0[59], cos_bit);
  bf1[60] = half_btf(cospi[49], bf0[35], cospi[15], bf0[60], cos_bit);
  bf1[61] = half_btf(cospi[17], bf0[34], cospi[47], bf0[61], cos_bit);
  bf1[62] = half_btf(cospi[33], bf0[33], cospi[31], bf0[62], cos_bit);
  bf1[63] = half_btf(cospi[1], bf0[32], cospi[63], bf0[63], cos_bit);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 3
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = bf0[10];
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = bf0[13];
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = half_btf(cospi[62], bf0[16], -cospi[2], bf0[31], cos_bit);
  bf1[17] = half_btf(cospi[30], bf0[17], -cospi[34], bf0[30], cos_bit);
  bf1[18] = half_btf(cospi[46], bf0[18], -cospi[18], bf0[29], cos_bit);
  bf1[19] = half_btf(cospi[14], bf0[19], -cospi[50], bf0[28], cos_bit);
  bf1[20] = half_btf(cospi[54], bf0[20], -cospi[10], bf0[27], cos_bit);
  bf1[21] = half_btf(cospi[22], bf0[21], -cospi[42], bf0[26], cos_bit);
  bf1[22] = half_btf(cospi[38], bf0[22], -cospi[26], bf0[25], cos_bit);
  bf1[23] = half_btf(cospi[6], bf0[23], -cospi[58], bf0[24], cos_bit);
  bf1[24] = half_btf(cospi[58], bf0[23], cospi[6], bf0[24], cos_bit);
  bf1[25] = half_btf(cospi[26], bf0[22], cospi[38], bf0[25], cos_bit);
  bf1[26] = half_btf(cospi[42], bf0[21], cospi[22], bf0[26], cos_bit);
  bf1[27] = half_btf(cospi[10], bf0[20], cospi[54], bf0[27], cos_bit);
  bf1[28] = half_btf(cospi[50], bf0[19], cospi[14], bf0[28], cos_bit);
  bf1[29] = half_btf(cospi[18], bf0[18], cospi[46], bf0[29], cos_bit);
  bf1[30] = half_btf(cospi[34], bf0[17], cospi[30], bf0[30], cos_bit);
  bf1[31] = half_btf(cospi[2], bf0[16], cospi[62], bf0[31], cos_bit);
  bf1[32] = clamp_value(bf0[32] + bf0[33], stage_range[stage]);
  bf1[33] = clamp_value(bf0[32] - bf0[33], stage_range[stage]);
  bf1[34] = clamp_value(-bf0[34] + bf0[35], stage_range[stage]);
  bf1[35] = clamp_value(bf0[34] + bf0[35], stage_range[stage]);
  bf1[36] = clamp_value(bf0[36] + bf0[37], stage_range[stage]);
  bf1[37] = clamp_value(bf0[36] - bf0[37], stage_range[stage]);
  bf1[38] = clamp_value(-bf0[38] + bf0[39], stage_range[stage]);
  bf1[39] = clamp_value(bf0[38] + bf0[39], stage_range[stage]);
  bf1[40] = clamp_value(bf0[40] + bf0[41], stage_range[stage]);
  bf1[41] = clamp_value(bf0[40] - bf0[41], stage_range[stage]);
  bf1[42] = clamp_value(-bf0[42] + bf0[43], stage_range[stage]);
  bf1[43] = clamp_value(bf0[42] + bf0[43], stage_range[stage]);
  bf1[44] = clamp_value(bf0[44] + bf0[45], stage_range[stage]);
  bf1[45] = clamp_value(bf0[44] - bf0[45], stage_range[stage]);
  bf1[46] = clamp_value(-bf0[46] + bf0[47], stage_range[stage]);
  bf1[47] = clamp_value(bf0[46] + bf0[47], stage_range[stage]);
  bf1[48] = clamp_value(bf0[48] + bf0[49], stage_range[stage]);
  bf1[49] = clamp_value(bf0[48] - bf0[49], stage_range[stage]);
  bf1[50] = clamp_value(-bf0[50] + bf0[51], stage_range[stage]);
  bf1[51] = clamp_value(bf0[50] + bf0[51], stage_range[stage]);
  bf1[52] = clamp_value(bf0[52] + bf0[53], stage_range[stage]);
  bf1[53] = clamp_value(bf0[52] - bf0[53], stage_range[stage]);
  bf1[54] = clamp_value(-bf0[54] + bf0[55], stage_range[stage]);
  bf1[55] = clamp_value(bf0[54] + bf0[55], stage_range[stage]);
  bf1[56] = clamp_value(bf0[56] + bf0[57], stage_range[stage]);
  bf1[57] = clamp_value(bf0[56] - bf0[57], stage_range[stage]);
  bf1[58] = clamp_value(-bf0[58] + bf0[59], stage_range[stage]);
  bf1[59] = clamp_value(bf0[58] + bf0[59], stage_range[stage]);
  bf1[60] = clamp_value(bf0[60] + bf0[61], stage_range[stage]);
  bf1[61] = clamp_value(bf0[60] - bf0[61], stage_range[stage]);
  bf1[62] = clamp_value(-bf0[62] + bf0[63], stage_range[stage]);
  bf1[63] = clamp_value(bf0[62] + bf0[63], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 4
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = bf0[4];
  bf1[5] = bf0[5];
  bf1[6] = bf0[6];
  bf1[7] = bf0[7];
  bf1[8] = half_btf(cospi[60], bf0[8], -cospi[4], bf0[15], cos_bit);
  bf1[9] = half_btf(cospi[28], bf0[9], -cospi[36], bf0[14], cos_bit);
  bf1[10] = half_btf(cospi[44], bf0[10], -cospi[20], bf0[13], cos_bit);
  bf1[11] = half_btf(cospi[12], bf0[11], -cospi[52], bf0[12], cos_bit);
  bf1[12] = half_btf(cospi[52], bf0[11], cospi[12], bf0[12], cos_bit);
  bf1[13] = half_btf(cospi[20], bf0[10], cospi[44], bf0[13], cos_bit);
  bf1[14] = half_btf(cospi[36], bf0[9], cospi[28], bf0[14], cos_bit);
  bf1[15] = half_btf(cospi[4], bf0[8], cospi[60], bf0[15], cos_bit);
  bf1[16] = clamp_value(bf0[16] + bf0[17], stage_range[stage]);
  bf1[17] = clamp_value(bf0[16] - bf0[17], stage_range[stage]);
  bf1[18] = clamp_value(-bf0[18] + bf0[19], stage_range[stage]);
  bf1[19] = clamp_value(bf0[18] + bf0[19], stage_range[stage]);
  bf1[20] = clamp_value(bf0[20] + bf0[21], stage_range[stage]);
  bf1[21] = clamp_value(bf0[20] - bf0[21], stage_range[stage]);
  bf1[22] = clamp_value(-bf0[22] + bf0[23], stage_range[stage]);
  bf1[23] = clamp_value(bf0[22] + bf0[23], stage_range[stage]);
  bf1[24] = clamp_value(bf0[24] + bf0[25], stage_range[stage]);
  bf1[25] = clamp_value(bf0[24] - bf0[25], stage_range[stage]);
  bf1[26] = clamp_value(-bf0[26] + bf0[27], stage_range[stage]);
  bf1[27] = clamp_value(bf0[26] + bf0[27], stage_range[stage]);
  bf1[28] = clamp_value(bf0[28] + bf0[29], stage_range[stage]);
  bf1[29] = clamp_value(bf0[28] - bf0[29], stage_range[stage]);
  bf1[30] = clamp_value(-bf0[30] + bf0[31], stage_range[stage]);
  bf1[31] = clamp_value(bf0[30] + bf0[31], stage_range[stage]);
  bf1[32] = bf0[32];
  bf1[33] = half_btf(-cospi[4], bf0[33], cospi[60], bf0[62], cos_bit);
  bf1[34] = half_btf(-cospi[60], bf0[34], -cospi[4], bf0[61], cos_bit);
  bf1[35] = bf0[35];
  bf1[36] = bf0[36];
  bf1[37] = half_btf(-cospi[36], bf0[37], cospi[28], bf0[58], cos_bit);
  bf1[38] = half_btf(-cospi[28], bf0[38], -cospi[36], bf0[57], cos_bit);
  bf1[39] = bf0[39];
  bf1[40] = bf0[40];
  bf1[41] = half_btf(-cospi[20], bf0[41], cospi[44], bf0[54], cos_bit);
  bf1[42] = half_btf(-cospi[44], bf0[42], -cospi[20], bf0[53], cos_bit);
  bf1[43] = bf0[43];
  bf1[44] = bf0[44];
  bf1[45] = half_btf(-cospi[52], bf0[45], cospi[12], bf0[50], cos_bit);
  bf1[46] = half_btf(-cospi[12], bf0[46], -cospi[52], bf0[49], cos_bit);
  bf1[47] = bf0[47];
  bf1[48] = bf0[48];
  bf1[49] = half_btf(-cospi[52], bf0[46], cospi[12], bf0[49], cos_bit);
  bf1[50] = half_btf(cospi[12], bf0[45], cospi[52], bf0[50], cos_bit);
  bf1[51] = bf0[51];
  bf1[52] = bf0[52];
  bf1[53] = half_btf(-cospi[20], bf0[42], cospi[44], bf0[53], cos_bit);
  bf1[54] = half_btf(cospi[44], bf0[41], cospi[20], bf0[54], cos_bit);
  bf1[55] = bf0[55];
  bf1[56] = bf0[56];
  bf1[57] = half_btf(-cospi[36], bf0[38], cospi[28], bf0[57], cos_bit);
  bf1[58] = half_btf(cospi[28], bf0[37], cospi[36], bf0[58], cos_bit);
  bf1[59] = bf0[59];
  bf1[60] = bf0[60];
  bf1[61] = half_btf(-cospi[4], bf0[34], cospi[60], bf0[61], cos_bit);
  bf1[62] = half_btf(cospi[60], bf0[33], cospi[4], bf0[62], cos_bit);
  bf1[63] = bf0[63];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 5
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = bf0[0];
  bf1[1] = bf0[1];
  bf1[2] = bf0[2];
  bf1[3] = bf0[3];
  bf1[4] = half_btf(cospi[56], bf0[4], -cospi[8], bf0[7], cos_bit);
  bf1[5] = half_btf(cospi[24], bf0[5], -cospi[40], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[40], bf0[5], cospi[24], bf0[6], cos_bit);
  bf1[7] = half_btf(cospi[8], bf0[4], cospi[56], bf0[7], cos_bit);
  bf1[8] = clamp_value(bf0[8] + bf0[9], stage_range[stage]);
  bf1[9] = clamp_value(bf0[8] - bf0[9], stage_range[stage]);
  bf1[10] = clamp_value(-bf0[10] + bf0[11], stage_range[stage]);
  bf1[11] = clamp_value(bf0[10] + bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(bf0[12] + bf0[13], stage_range[stage]);
  bf1[13] = clamp_value(bf0[12] - bf0[13], stage_range[stage]);
  bf1[14] = clamp_value(-bf0[14] + bf0[15], stage_range[stage]);
  bf1[15] = clamp_value(bf0[14] + bf0[15], stage_range[stage]);
  bf1[16] = bf0[16];
  bf1[17] = half_btf(-cospi[8], bf0[17], cospi[56], bf0[30], cos_bit);
  bf1[18] = half_btf(-cospi[56], bf0[18], -cospi[8], bf0[29], cos_bit);
  bf1[19] = bf0[19];
  bf1[20] = bf0[20];
  bf1[21] = half_btf(-cospi[40], bf0[21], cospi[24], bf0[26], cos_bit);
  bf1[22] = half_btf(-cospi[24], bf0[22], -cospi[40], bf0[25], cos_bit);
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = half_btf(-cospi[40], bf0[22], cospi[24], bf0[25], cos_bit);
  bf1[26] = half_btf(cospi[24], bf0[21], cospi[40], bf0[26], cos_bit);
  bf1[27] = bf0[27];
  bf1[28] = bf0[28];
  bf1[29] = half_btf(-cospi[8], bf0[18], cospi[56], bf0[29], cos_bit);
  bf1[30] = half_btf(cospi[56], bf0[17], cospi[8], bf0[30], cos_bit);
  bf1[31] = bf0[31];
  bf1[32] = clamp_value(bf0[32] + bf0[35], stage_range[stage]);
  bf1[33] = clamp_value(bf0[33] + bf0[34], stage_range[stage]);
  bf1[34] = clamp_value(bf0[33] - bf0[34], stage_range[stage]);
  bf1[35] = clamp_value(bf0[32] - bf0[35], stage_range[stage]);
  bf1[36] = clamp_value(-bf0[36] + bf0[39], stage_range[stage]);
  bf1[37] = clamp_value(-bf0[37] + bf0[38], stage_range[stage]);
  bf1[38] = clamp_value(bf0[37] + bf0[38], stage_range[stage]);
  bf1[39] = clamp_value(bf0[36] + bf0[39], stage_range[stage]);
  bf1[40] = clamp_value(bf0[40] + bf0[43], stage_range[stage]);
  bf1[41] = clamp_value(bf0[41] + bf0[42], stage_range[stage]);
  bf1[42] = clamp_value(bf0[41] - bf0[42], stage_range[stage]);
  bf1[43] = clamp_value(bf0[40] - bf0[43], stage_range[stage]);
  bf1[44] = clamp_value(-bf0[44] + bf0[47], stage_range[stage]);
  bf1[45] = clamp_value(-bf0[45] + bf0[46], stage_range[stage]);
  bf1[46] = clamp_value(bf0[45] + bf0[46], stage_range[stage]);
  bf1[47] = clamp_value(bf0[44] + bf0[47], stage_range[stage]);
  bf1[48] = clamp_value(bf0[48] + bf0[51], stage_range[stage]);
  bf1[49] = clamp_value(bf0[49] + bf0[50], stage_range[stage]);
  bf1[50] = clamp_value(bf0[49] - bf0[50], stage_range[stage]);
  bf1[51] = clamp_value(bf0[48] - bf0[51], stage_range[stage]);
  bf1[52] = clamp_value(-bf0[52] + bf0[55], stage_range[stage]);
  bf1[53] = clamp_value(-bf0[53] + bf0[54], stage_range[stage]);
  bf1[54] = clamp_value(bf0[53] + bf0[54], stage_range[stage]);
  bf1[55] = clamp_value(bf0[52] + bf0[55], stage_range[stage]);
  bf1[56] = clamp_value(bf0[56] + bf0[59], stage_range[stage]);
  bf1[57] = clamp_value(bf0[57] + bf0[58], stage_range[stage]);
  bf1[58] = clamp_value(bf0[57] - bf0[58], stage_range[stage]);
  bf1[59] = clamp_value(bf0[56] - bf0[59], stage_range[stage]);
  bf1[60] = clamp_value(-bf0[60] + bf0[63], stage_range[stage]);
  bf1[61] = clamp_value(-bf0[61] + bf0[62], stage_range[stage]);
  bf1[62] = clamp_value(bf0[61] + bf0[62], stage_range[stage]);
  bf1[63] = clamp_value(bf0[60] + bf0[63], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 6
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = half_btf(cospi[32], bf0[0], cospi[32], bf0[1], cos_bit);
  bf1[1] = half_btf(cospi[32], bf0[0], -cospi[32], bf0[1], cos_bit);
  bf1[2] = half_btf(cospi[48], bf0[2], -cospi[16], bf0[3], cos_bit);
  bf1[3] = half_btf(cospi[16], bf0[2], cospi[48], bf0[3], cos_bit);
  bf1[4] = clamp_value(bf0[4] + bf0[5], stage_range[stage]);
  bf1[5] = clamp_value(bf0[4] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(-bf0[6] + bf0[7], stage_range[stage]);
  bf1[7] = clamp_value(bf0[6] + bf0[7], stage_range[stage]);
  bf1[8] = bf0[8];
  bf1[9] = half_btf(-cospi[16], bf0[9], cospi[48], bf0[14], cos_bit);
  bf1[10] = half_btf(-cospi[48], bf0[10], -cospi[16], bf0[13], cos_bit);
  bf1[11] = bf0[11];
  bf1[12] = bf0[12];
  bf1[13] = half_btf(-cospi[16], bf0[10], cospi[48], bf0[13], cos_bit);
  bf1[14] = half_btf(cospi[48], bf0[9], cospi[16], bf0[14], cos_bit);
  bf1[15] = bf0[15];
  bf1[16] = clamp_value(bf0[16] + bf0[19], stage_range[stage]);
  bf1[17] = clamp_value(bf0[17] + bf0[18], stage_range[stage]);
  bf1[18] = clamp_value(bf0[17] - bf0[18], stage_range[stage]);
  bf1[19] = clamp_value(bf0[16] - bf0[19], stage_range[stage]);
  bf1[20] = clamp_value(-bf0[20] + bf0[23], stage_range[stage]);
  bf1[21] = clamp_value(-bf0[21] + bf0[22], stage_range[stage]);
  bf1[22] = clamp_value(bf0[21] + bf0[22], stage_range[stage]);
  bf1[23] = clamp_value(bf0[20] + bf0[23], stage_range[stage]);
  bf1[24] = clamp_value(bf0[24] + bf0[27], stage_range[stage]);
  bf1[25] = clamp_value(bf0[25] + bf0[26], stage_range[stage]);
  bf1[26] = clamp_value(bf0[25] - bf0[26], stage_range[stage]);
  bf1[27] = clamp_value(bf0[24] - bf0[27], stage_range[stage]);
  bf1[28] = clamp_value(-bf0[28] + bf0[31], stage_range[stage]);
  bf1[29] = clamp_value(-bf0[29] + bf0[30], stage_range[stage]);
  bf1[30] = clamp_value(bf0[29] + bf0[30], stage_range[stage]);
  bf1[31] = clamp_value(bf0[28] + bf0[31], stage_range[stage]);
  bf1[32] = bf0[32];
  bf1[33] = bf0[33];
  bf1[34] = half_btf(-cospi[8], bf0[34], cospi[56], bf0[61], cos_bit);
  bf1[35] = half_btf(-cospi[8], bf0[35], cospi[56], bf0[60], cos_bit);
  bf1[36] = half_btf(-cospi[56], bf0[36], -cospi[8], bf0[59], cos_bit);
  bf1[37] = half_btf(-cospi[56], bf0[37], -cospi[8], bf0[58], cos_bit);
  bf1[38] = bf0[38];
  bf1[39] = bf0[39];
  bf1[40] = bf0[40];
  bf1[41] = bf0[41];
  bf1[42] = half_btf(-cospi[40], bf0[42], cospi[24], bf0[53], cos_bit);
  bf1[43] = half_btf(-cospi[40], bf0[43], cospi[24], bf0[52], cos_bit);
  bf1[44] = half_btf(-cospi[24], bf0[44], -cospi[40], bf0[51], cos_bit);
  bf1[45] = half_btf(-cospi[24], bf0[45], -cospi[40], bf0[50], cos_bit);
  bf1[46] = bf0[46];
  bf1[47] = bf0[47];
  bf1[48] = bf0[48];
  bf1[49] = bf0[49];
  bf1[50] = half_btf(-cospi[40], bf0[45], cospi[24], bf0[50], cos_bit);
  bf1[51] = half_btf(-cospi[40], bf0[44], cospi[24], bf0[51], cos_bit);
  bf1[52] = half_btf(cospi[24], bf0[43], cospi[40], bf0[52], cos_bit);
  bf1[53] = half_btf(cospi[24], bf0[42], cospi[40], bf0[53], cos_bit);
  bf1[54] = bf0[54];
  bf1[55] = bf0[55];
  bf1[56] = bf0[56];
  bf1[57] = bf0[57];
  bf1[58] = half_btf(-cospi[8], bf0[37], cospi[56], bf0[58], cos_bit);
  bf1[59] = half_btf(-cospi[8], bf0[36], cospi[56], bf0[59], cos_bit);
  bf1[60] = half_btf(cospi[56], bf0[35], cospi[8], bf0[60], cos_bit);
  bf1[61] = half_btf(cospi[56], bf0[34], cospi[8], bf0[61], cos_bit);
  bf1[62] = bf0[62];
  bf1[63] = bf0[63];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 7
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[3], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[2], stage_range[stage]);
  bf1[2] = clamp_value(bf0[1] - bf0[2], stage_range[stage]);
  bf1[3] = clamp_value(bf0[0] - bf0[3], stage_range[stage]);
  bf1[4] = bf0[4];
  bf1[5] = half_btf(-cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[6] = half_btf(cospi[32], bf0[5], cospi[32], bf0[6], cos_bit);
  bf1[7] = bf0[7];
  bf1[8] = clamp_value(bf0[8] + bf0[11], stage_range[stage]);
  bf1[9] = clamp_value(bf0[9] + bf0[10], stage_range[stage]);
  bf1[10] = clamp_value(bf0[9] - bf0[10], stage_range[stage]);
  bf1[11] = clamp_value(bf0[8] - bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(-bf0[12] + bf0[15], stage_range[stage]);
  bf1[13] = clamp_value(-bf0[13] + bf0[14], stage_range[stage]);
  bf1[14] = clamp_value(bf0[13] + bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[12] + bf0[15], stage_range[stage]);
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = half_btf(-cospi[16], bf0[18], cospi[48], bf0[29], cos_bit);
  bf1[19] = half_btf(-cospi[16], bf0[19], cospi[48], bf0[28], cos_bit);
  bf1[20] = half_btf(-cospi[48], bf0[20], -cospi[16], bf0[27], cos_bit);
  bf1[21] = half_btf(-cospi[48], bf0[21], -cospi[16], bf0[26], cos_bit);
  bf1[22] = bf0[22];
  bf1[23] = bf0[23];
  bf1[24] = bf0[24];
  bf1[25] = bf0[25];
  bf1[26] = half_btf(-cospi[16], bf0[21], cospi[48], bf0[26], cos_bit);
  bf1[27] = half_btf(-cospi[16], bf0[20], cospi[48], bf0[27], cos_bit);
  bf1[28] = half_btf(cospi[48], bf0[19], cospi[16], bf0[28], cos_bit);
  bf1[29] = half_btf(cospi[48], bf0[18], cospi[16], bf0[29], cos_bit);
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  bf1[32] = clamp_value(bf0[32] + bf0[39], stage_range[stage]);
  bf1[33] = clamp_value(bf0[33] + bf0[38], stage_range[stage]);
  bf1[34] = clamp_value(bf0[34] + bf0[37], stage_range[stage]);
  bf1[35] = clamp_value(bf0[35] + bf0[36], stage_range[stage]);
  bf1[36] = clamp_value(bf0[35] - bf0[36], stage_range[stage]);
  bf1[37] = clamp_value(bf0[34] - bf0[37], stage_range[stage]);
  bf1[38] = clamp_value(bf0[33] - bf0[38], stage_range[stage]);
  bf1[39] = clamp_value(bf0[32] - bf0[39], stage_range[stage]);
  bf1[40] = clamp_value(-bf0[40] + bf0[47], stage_range[stage]);
  bf1[41] = clamp_value(-bf0[41] + bf0[46], stage_range[stage]);
  bf1[42] = clamp_value(-bf0[42] + bf0[45], stage_range[stage]);
  bf1[43] = clamp_value(-bf0[43] + bf0[44], stage_range[stage]);
  bf1[44] = clamp_value(bf0[43] + bf0[44], stage_range[stage]);
  bf1[45] = clamp_value(bf0[42] + bf0[45], stage_range[stage]);
  bf1[46] = clamp_value(bf0[41] + bf0[46], stage_range[stage]);
  bf1[47] = clamp_value(bf0[40] + bf0[47], stage_range[stage]);
  bf1[48] = clamp_value(bf0[48] + bf0[55], stage_range[stage]);
  bf1[49] = clamp_value(bf0[49] + bf0[54], stage_range[stage]);
  bf1[50] = clamp_value(bf0[50] + bf0[53], stage_range[stage]);
  bf1[51] = clamp_value(bf0[51] + bf0[52], stage_range[stage]);
  bf1[52] = clamp_value(bf0[51] - bf0[52], stage_range[stage]);
  bf1[53] = clamp_value(bf0[50] - bf0[53], stage_range[stage]);
  bf1[54] = clamp_value(bf0[49] - bf0[54], stage_range[stage]);
  bf1[55] = clamp_value(bf0[48] - bf0[55], stage_range[stage]);
  bf1[56] = clamp_value(-bf0[56] + bf0[63], stage_range[stage]);
  bf1[57] = clamp_value(-bf0[57] + bf0[62], stage_range[stage]);
  bf1[58] = clamp_value(-bf0[58] + bf0[61], stage_range[stage]);
  bf1[59] = clamp_value(-bf0[59] + bf0[60], stage_range[stage]);
  bf1[60] = clamp_value(bf0[59] + bf0[60], stage_range[stage]);
  bf1[61] = clamp_value(bf0[58] + bf0[61], stage_range[stage]);
  bf1[62] = clamp_value(bf0[57] + bf0[62], stage_range[stage]);
  bf1[63] = clamp_value(bf0[56] + bf0[63], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 8
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = clamp_value(bf0[0] + bf0[7], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[6], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[5], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[4], stage_range[stage]);
  bf1[4] = clamp_value(bf0[3] - bf0[4], stage_range[stage]);
  bf1[5] = clamp_value(bf0[2] - bf0[5], stage_range[stage]);
  bf1[6] = clamp_value(bf0[1] - bf0[6], stage_range[stage]);
  bf1[7] = clamp_value(bf0[0] - bf0[7], stage_range[stage]);
  bf1[8] = bf0[8];
  bf1[9] = bf0[9];
  bf1[10] = half_btf(-cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
  bf1[11] = half_btf(-cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
  bf1[12] = half_btf(cospi[32], bf0[11], cospi[32], bf0[12], cos_bit);
  bf1[13] = half_btf(cospi[32], bf0[10], cospi[32], bf0[13], cos_bit);
  bf1[14] = bf0[14];
  bf1[15] = bf0[15];
  bf1[16] = clamp_value(bf0[16] + bf0[23], stage_range[stage]);
  bf1[17] = clamp_value(bf0[17] + bf0[22], stage_range[stage]);
  bf1[18] = clamp_value(bf0[18] + bf0[21], stage_range[stage]);
  bf1[19] = clamp_value(bf0[19] + bf0[20], stage_range[stage]);
  bf1[20] = clamp_value(bf0[19] - bf0[20], stage_range[stage]);
  bf1[21] = clamp_value(bf0[18] - bf0[21], stage_range[stage]);
  bf1[22] = clamp_value(bf0[17] - bf0[22], stage_range[stage]);
  bf1[23] = clamp_value(bf0[16] - bf0[23], stage_range[stage]);
  bf1[24] = clamp_value(-bf0[24] + bf0[31], stage_range[stage]);
  bf1[25] = clamp_value(-bf0[25] + bf0[30], stage_range[stage]);
  bf1[26] = clamp_value(-bf0[26] + bf0[29], stage_range[stage]);
  bf1[27] = clamp_value(-bf0[27] + bf0[28], stage_range[stage]);
  bf1[28] = clamp_value(bf0[27] + bf0[28], stage_range[stage]);
  bf1[29] = clamp_value(bf0[26] + bf0[29], stage_range[stage]);
  bf1[30] = clamp_value(bf0[25] + bf0[30], stage_range[stage]);
  bf1[31] = clamp_value(bf0[24] + bf0[31], stage_range[stage]);
  bf1[32] = bf0[32];
  bf1[33] = bf0[33];
  bf1[34] = bf0[34];
  bf1[35] = bf0[35];
  bf1[36] = half_btf(-cospi[16], bf0[36], cospi[48], bf0[59], cos_bit);
  bf1[37] = half_btf(-cospi[16], bf0[37], cospi[48], bf0[58], cos_bit);
  bf1[38] = half_btf(-cospi[16], bf0[38], cospi[48], bf0[57], cos_bit);
  bf1[39] = half_btf(-cospi[16], bf0[39], cospi[48], bf0[56], cos_bit);
  bf1[40] = half_btf(-cospi[48], bf0[40], -cospi[16], bf0[55], cos_bit);
  bf1[41] = half_btf(-cospi[48], bf0[41], -cospi[16], bf0[54], cos_bit);
  bf1[42] = half_btf(-cospi[48], bf0[42], -cospi[16], bf0[53], cos_bit);
  bf1[43] = half_btf(-cospi[48], bf0[43], -cospi[16], bf0[52], cos_bit);
  bf1[44] = bf0[44];
  bf1[45] = bf0[45];
  bf1[46] = bf0[46];
  bf1[47] = bf0[47];
  bf1[48] = bf0[48];
  bf1[49] = bf0[49];
  bf1[50] = bf0[50];
  bf1[51] = bf0[51];
  bf1[52] = half_btf(-cospi[16], bf0[43], cospi[48], bf0[52], cos_bit);
  bf1[53] = half_btf(-cospi[16], bf0[42], cospi[48], bf0[53], cos_bit);
  bf1[54] = half_btf(-cospi[16], bf0[41], cospi[48], bf0[54], cos_bit);
  bf1[55] = half_btf(-cospi[16], bf0[40], cospi[48], bf0[55], cos_bit);
  bf1[56] = half_btf(cospi[48], bf0[39], cospi[16], bf0[56], cos_bit);
  bf1[57] = half_btf(cospi[48], bf0[38], cospi[16], bf0[57], cos_bit);
  bf1[58] = half_btf(cospi[48], bf0[37], cospi[16], bf0[58], cos_bit);
  bf1[59] = half_btf(cospi[48], bf0[36], cospi[16], bf0[59], cos_bit);
  bf1[60] = bf0[60];
  bf1[61] = bf0[61];
  bf1[62] = bf0[62];
  bf1[63] = bf0[63];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 9
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[15], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[14], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[13], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[12], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[11], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[10], stage_range[stage]);
  bf1[6] = clamp_value(bf0[6] + bf0[9], stage_range[stage]);
  bf1[7] = clamp_value(bf0[7] + bf0[8], stage_range[stage]);
  bf1[8] = clamp_value(bf0[7] - bf0[8], stage_range[stage]);
  bf1[9] = clamp_value(bf0[6] - bf0[9], stage_range[stage]);
  bf1[10] = clamp_value(bf0[5] - bf0[10], stage_range[stage]);
  bf1[11] = clamp_value(bf0[4] - bf0[11], stage_range[stage]);
  bf1[12] = clamp_value(bf0[3] - bf0[12], stage_range[stage]);
  bf1[13] = clamp_value(bf0[2] - bf0[13], stage_range[stage]);
  bf1[14] = clamp_value(bf0[1] - bf0[14], stage_range[stage]);
  bf1[15] = clamp_value(bf0[0] - bf0[15], stage_range[stage]);
  bf1[16] = bf0[16];
  bf1[17] = bf0[17];
  bf1[18] = bf0[18];
  bf1[19] = bf0[19];
  bf1[20] = half_btf(-cospi[32], bf0[20], cospi[32], bf0[27], cos_bit);
  bf1[21] = half_btf(-cospi[32], bf0[21], cospi[32], bf0[26], cos_bit);
  bf1[22] = half_btf(-cospi[32], bf0[22], cospi[32], bf0[25], cos_bit);
  bf1[23] = half_btf(-cospi[32], bf0[23], cospi[32], bf0[24], cos_bit);
  bf1[24] = half_btf(cospi[32], bf0[23], cospi[32], bf0[24], cos_bit);
  bf1[25] = half_btf(cospi[32], bf0[22], cospi[32], bf0[25], cos_bit);
  bf1[26] = half_btf(cospi[32], bf0[21], cospi[32], bf0[26], cos_bit);
  bf1[27] = half_btf(cospi[32], bf0[20], cospi[32], bf0[27], cos_bit);
  bf1[28] = bf0[28];
  bf1[29] = bf0[29];
  bf1[30] = bf0[30];
  bf1[31] = bf0[31];
  bf1[32] = clamp_value(bf0[32] + bf0[47], stage_range[stage]);
  bf1[33] = clamp_value(bf0[33] + bf0[46], stage_range[stage]);
  bf1[34] = clamp_value(bf0[34] + bf0[45], stage_range[stage]);
  bf1[35] = clamp_value(bf0[35] + bf0[44], stage_range[stage]);
  bf1[36] = clamp_value(bf0[36] + bf0[43], stage_range[stage]);
  bf1[37] = clamp_value(bf0[37] + bf0[42], stage_range[stage]);
  bf1[38] = clamp_value(bf0[38] + bf0[41], stage_range[stage]);
  bf1[39] = clamp_value(bf0[39] + bf0[40], stage_range[stage]);
  bf1[40] = clamp_value(bf0[39] - bf0[40], stage_range[stage]);
  bf1[41] = clamp_value(bf0[38] - bf0[41], stage_range[stage]);
  bf1[42] = clamp_value(bf0[37] - bf0[42], stage_range[stage]);
  bf1[43] = clamp_value(bf0[36] - bf0[43], stage_range[stage]);
  bf1[44] = clamp_value(bf0[35] - bf0[44], stage_range[stage]);
  bf1[45] = clamp_value(bf0[34] - bf0[45], stage_range[stage]);
  bf1[46] = clamp_value(bf0[33] - bf0[46], stage_range[stage]);
  bf1[47] = clamp_value(bf0[32] - bf0[47], stage_range[stage]);
  bf1[48] = clamp_value(-bf0[48] + bf0[63], stage_range[stage]);
  bf1[49] = clamp_value(-bf0[49] + bf0[62], stage_range[stage]);
  bf1[50] = clamp_value(-bf0[50] + bf0[61], stage_range[stage]);
  bf1[51] = clamp_value(-bf0[51] + bf0[60], stage_range[stage]);
  bf1[52] = clamp_value(-bf0[52] + bf0[59], stage_range[stage]);
  bf1[53] = clamp_value(-bf0[53] + bf0[58], stage_range[stage]);
  bf1[54] = clamp_value(-bf0[54] + bf0[57], stage_range[stage]);
  bf1[55] = clamp_value(-bf0[55] + bf0[56], stage_range[stage]);
  bf1[56] = clamp_value(bf0[55] + bf0[56], stage_range[stage]);
  bf1[57] = clamp_value(bf0[54] + bf0[57], stage_range[stage]);
  bf1[58] = clamp_value(bf0[53] + bf0[58], stage_range[stage]);
  bf1[59] = clamp_value(bf0[52] + bf0[59], stage_range[stage]);
  bf1[60] = clamp_value(bf0[51] + bf0[60], stage_range[stage]);
  bf1[61] = clamp_value(bf0[50] + bf0[61], stage_range[stage]);
  bf1[62] = clamp_value(bf0[49] + bf0[62], stage_range[stage]);
  bf1[63] = clamp_value(bf0[48] + bf0[63], stage_range[stage]);
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 10
  stage++;
  bf0 = output;
  bf1 = step;
  bf1[0] = clamp_value(bf0[0] + bf0[31], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[30], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[29], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[28], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[27], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[26], stage_range[stage]);
  bf1[6] = clamp_value(bf0[6] + bf0[25], stage_range[stage]);
  bf1[7] = clamp_value(bf0[7] + bf0[24], stage_range[stage]);
  bf1[8] = clamp_value(bf0[8] + bf0[23], stage_range[stage]);
  bf1[9] = clamp_value(bf0[9] + bf0[22], stage_range[stage]);
  bf1[10] = clamp_value(bf0[10] + bf0[21], stage_range[stage]);
  bf1[11] = clamp_value(bf0[11] + bf0[20], stage_range[stage]);
  bf1[12] = clamp_value(bf0[12] + bf0[19], stage_range[stage]);
  bf1[13] = clamp_value(bf0[13] + bf0[18], stage_range[stage]);
  bf1[14] = clamp_value(bf0[14] + bf0[17], stage_range[stage]);
  bf1[15] = clamp_value(bf0[15] + bf0[16], stage_range[stage]);
  bf1[16] = clamp_value(bf0[15] - bf0[16], stage_range[stage]);
  bf1[17] = clamp_value(bf0[14] - bf0[17], stage_range[stage]);
  bf1[18] = clamp_value(bf0[13] - bf0[18], stage_range[stage]);
  bf1[19] = clamp_value(bf0[12] - bf0[19], stage_range[stage]);
  bf1[20] = clamp_value(bf0[11] - bf0[20], stage_range[stage]);
  bf1[21] = clamp_value(bf0[10] - bf0[21], stage_range[stage]);
  bf1[22] = clamp_value(bf0[9] - bf0[22], stage_range[stage]);
  bf1[23] = clamp_value(bf0[8] - bf0[23], stage_range[stage]);
  bf1[24] = clamp_value(bf0[7] - bf0[24], stage_range[stage]);
  bf1[25] = clamp_value(bf0[6] - bf0[25], stage_range[stage]);
  bf1[26] = clamp_value(bf0[5] - bf0[26], stage_range[stage]);
  bf1[27] = clamp_value(bf0[4] - bf0[27], stage_range[stage]);
  bf1[28] = clamp_value(bf0[3] - bf0[28], stage_range[stage]);
  bf1[29] = clamp_value(bf0[2] - bf0[29], stage_range[stage]);
  bf1[30] = clamp_value(bf0[1] - bf0[30], stage_range[stage]);
  bf1[31] = clamp_value(bf0[0] - bf0[31], stage_range[stage]);
  bf1[32] = bf0[32];
  bf1[33] = bf0[33];
  bf1[34] = bf0[34];
  bf1[35] = bf0[35];
  bf1[36] = bf0[36];
  bf1[37] = bf0[37];
  bf1[38] = bf0[38];
  bf1[39] = bf0[39];
  bf1[40] = half_btf(-cospi[32], bf0[40], cospi[32], bf0[55], cos_bit);
  bf1[41] = half_btf(-cospi[32], bf0[41], cospi[32], bf0[54], cos_bit);
  bf1[42] = half_btf(-cospi[32], bf0[42], cospi[32], bf0[53], cos_bit);
  bf1[43] = half_btf(-cospi[32], bf0[43], cospi[32], bf0[52], cos_bit);
  bf1[44] = half_btf(-cospi[32], bf0[44], cospi[32], bf0[51], cos_bit);
  bf1[45] = half_btf(-cospi[32], bf0[45], cospi[32], bf0[50], cos_bit);
  bf1[46] = half_btf(-cospi[32], bf0[46], cospi[32], bf0[49], cos_bit);
  bf1[47] = half_btf(-cospi[32], bf0[47], cospi[32], bf0[48], cos_bit);
  bf1[48] = half_btf(cospi[32], bf0[47], cospi[32], bf0[48], cos_bit);
  bf1[49] = half_btf(cospi[32], bf0[46], cospi[32], bf0[49], cos_bit);
  bf1[50] = half_btf(cospi[32], bf0[45], cospi[32], bf0[50], cos_bit);
  bf1[51] = half_btf(cospi[32], bf0[44], cospi[32], bf0[51], cos_bit);
  bf1[52] = half_btf(cospi[32], bf0[43], cospi[32], bf0[52], cos_bit);
  bf1[53] = half_btf(cospi[32], bf0[42], cospi[32], bf0[53], cos_bit);
  bf1[54] = half_btf(cospi[32], bf0[41], cospi[32], bf0[54], cos_bit);
  bf1[55] = half_btf(cospi[32], bf0[40], cospi[32], bf0[55], cos_bit);
  bf1[56] = bf0[56];
  bf1[57] = bf0[57];
  bf1[58] = bf0[58];
  bf1[59] = bf0[59];
  bf1[60] = bf0[60];
  bf1[61] = bf0[61];
  bf1[62] = bf0[62];
  bf1[63] = bf0[63];
  av1_range_check_buf(stage, input, bf1, size, stage_range[stage]);

  // stage 11
  stage++;
  bf0 = step;
  bf1 = output;
  bf1[0] = clamp_value(bf0[0] + bf0[63], stage_range[stage]);
  bf1[1] = clamp_value(bf0[1] + bf0[62], stage_range[stage]);
  bf1[2] = clamp_value(bf0[2] + bf0[61], stage_range[stage]);
  bf1[3] = clamp_value(bf0[3] + bf0[60], stage_range[stage]);
  bf1[4] = clamp_value(bf0[4] + bf0[59], stage_range[stage]);
  bf1[5] = clamp_value(bf0[5] + bf0[58], stage_range[stage]);
  bf1[6] = clamp_value(bf0[6] + bf0[57], stage_range[stage]);
  bf1[7] = clamp_value(bf0[7] + bf0[56], stage_range[stage]);
  bf1[8] = clamp_value(bf0[8] + bf0[55], stage_range[stage]);
  bf1[9] = clamp_value(bf0[9] + bf0[54], stage_range[stage]);
  bf1[10] = clamp_value(bf0[10] + bf0[53], stage_range[stage]);
  bf1[11] = clamp_value(bf0[11] + bf0[52], stage_range[stage]);
  bf1[12] = clamp_value(bf0[12] + bf0[51], stage_range[stage]);
  bf1[13] = clamp_value(bf0[13] + bf0[50], stage_range[stage]);
  bf1[14] = clamp_value(bf0[14] + bf0[49], stage_range[stage]);
  bf1[15] = clamp_value(bf0[15] + bf0[48], stage_range[stage]);
  bf1[16] = clamp_value(bf0[16] + bf0[47], stage_range[stage]);
  bf1[17] = clamp_value(bf0[17] + bf0[46], stage_range[stage]);
  bf1[18] = clamp_value(bf0[18] + bf0[45], stage_range[stage]);
  bf1[19] = clamp_value(bf0[19] + bf0[44], stage_range[stage]);
  bf1[20] = clamp_value(bf0[20] + bf0[43], stage_range[stage]);
  bf1[21] = clamp_value(bf0[21] + bf0[42], stage_range[stage]);
  bf1[22] = clamp_value(bf0[22] + bf0[41], stage_range[stage]);
  bf1[23] = clamp_value(bf0[23] + bf0[40], stage_range[stage]);
  bf1[24] = clamp_value(bf0[24] + bf0[39], stage_range[stage]);
  bf1[25] = clamp_value(bf0[25] + bf0[38], stage_range[stage]);
  bf1[26] = clamp_value(bf0[26] + bf0[37], stage_range[stage]);
  bf1[27] = clamp_value(bf0[27] + bf0[36], stage_range[stage]);
  bf1[28] = clamp_value(bf0[28] + bf0[35], stage_range[stage]);
  bf1[29] = clamp_value(bf0[29] + bf0[34], stage_range[stage]);
  bf1[30] = clamp_value(bf0[30] + bf0[33], stage_range[stage]);
  bf1[31] = clamp_value(bf0[31] + bf0[32], stage_range[stage]);
  bf1[32] = clamp_value(bf0[31] - bf0[32], stage_range[stage]);
  bf1[33] = clamp_value(bf0[30] - bf0[33], stage_range[stage]);
  bf1[34] = clamp_value(bf0[29] - bf0[34], stage_range[stage]);
  bf1[35] = clamp_value(bf0[28] - bf0[35], stage_range[stage]);
  bf1[36] = clamp_value(bf0[27] - bf0[36], stage_range[stage]);
  bf1[37] = clamp_value(bf0[26] - bf0[37], stage_range[stage]);
  bf1[38] = clamp_value(bf0[25] - bf0[38], stage_range[stage]);
  bf1[39] = clamp_value(bf0[24] - bf0[39], stage_range[stage]);
  bf1[40] = clamp_value(bf0[23] - bf0[40], stage_range[stage]);
  bf1[41] = clamp_value(bf0[22] - bf0[41], stage_range[stage]);
  bf1[42] = clamp_value(bf0[21] - bf0[42], stage_range[stage]);
  bf1[43] = clamp_value(bf0[20] - bf0[43], stage_range[stage]);
  bf1[44] = clamp_value(bf0[19] - bf0[44], stage_range[stage]);
  bf1[45] = clamp_value(bf0[18] - bf0[45], stage_range[stage]);
  bf1[46] = clamp_value(bf0[17] - bf0[46], stage_range[stage]);
  bf1[47] = clamp_value(bf0[16] - bf0[47], stage_range[stage]);
  bf1[48] = clamp_value(bf0[15] - bf0[48], stage_range[stage]);
  bf1[49] = clamp_value(bf0[14] - bf0[49], stage_range[stage]);
  bf1[50] = clamp_value(bf0[13] - bf0[50], stage_range[stage]);
  bf1[51] = clamp_value(bf0[12] - bf0[51], stage_range[stage]);
  bf1[52] = clamp_value(bf0[11] - bf0[52], stage_range[stage]);
  bf1[53] = clamp_value(bf0[10] - bf0[53], stage_range[stage]);
  bf1[54] = clamp_value(bf0[9] - bf0[54], stage_range[stage]);
  bf1[55] = clamp_value(bf0[8] - bf0[55], stage_range[stage]);
  bf1[56] = clamp_value(bf0[7] - bf0[56], stage_range[stage]);
  bf1[57] = clamp_value(bf0[6] - bf0[57], stage_range[stage]);
  bf1[58] = clamp_value(bf0[5] - bf0[58], stage_range[stage]);
  bf1[59] = clamp_value(bf0[4] - bf0[59], stage_range[stage]);
  bf1[60] = clamp_value(bf0[3] - bf0[60], stage_range[stage]);
  bf1[61] = clamp_value(bf0[2] - bf0[61], stage_range[stage]);
  bf1[62] = clamp_value(bf0[1] - bf0[62], stage_range[stage]);
  bf1[63] = clamp_value(bf0[0] - bf0[63], stage_range[stage]);
}
