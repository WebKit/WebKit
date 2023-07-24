/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include "aom_dsp/txfm_common.h"
#include "config/aom_dsp_rtcd.h"

void aom_fdct4x4_c(const int16_t *input, tran_low_t *output, int stride) {
  // The 2D transform is done with two passes which are actually pretty
  // similar. In the first one, we transform the columns and transpose
  // the results. In the second one, we transform the rows.
  // We need an intermediate buffer between passes.
  tran_low_t intermediate[4 * 4];
  const tran_low_t *in_low = NULL;
  tran_low_t *out = intermediate;
  // Do the two transform passes
  for (int pass = 0; pass < 2; ++pass) {
    tran_high_t in_high[4];  // canbe16
    tran_high_t step[4];     // canbe16
    tran_low_t temp[4];
    for (int i = 0; i < 4; ++i) {
      // Load inputs.
      if (pass == 0) {
        in_high[0] = input[0 * stride] * 16;
        in_high[1] = input[1 * stride] * 16;
        in_high[2] = input[2 * stride] * 16;
        in_high[3] = input[3 * stride] * 16;
        if (i == 0 && in_high[0]) {
          ++in_high[0];
        }
        ++input;  // Next column
      } else {
        assert(in_low != NULL);
        in_high[0] = in_low[0 * 4];
        in_high[1] = in_low[1 * 4];
        in_high[2] = in_low[2 * 4];
        in_high[3] = in_low[3 * 4];
        ++in_low;  // Next column (which is a transposed row)
      }
      // Transform.
      step[0] = in_high[0] + in_high[3];
      step[1] = in_high[1] + in_high[2];
      step[2] = in_high[1] - in_high[2];
      step[3] = in_high[0] - in_high[3];
      temp[0] = (tran_low_t)fdct_round_shift((step[0] + step[1]) * cospi_16_64);
      temp[2] = (tran_low_t)fdct_round_shift((step[0] - step[1]) * cospi_16_64);
      temp[1] = (tran_low_t)fdct_round_shift(step[2] * cospi_24_64 +
                                             step[3] * cospi_8_64);
      temp[3] = (tran_low_t)fdct_round_shift(-step[2] * cospi_8_64 +
                                             step[3] * cospi_24_64);
      // Only transpose the first pass.
      if (pass == 0) {
        out[0] = temp[0];
        out[1] = temp[1];
        out[2] = temp[2];
        out[3] = temp[3];
        out += 4;
      } else {
        out[0 * 4] = temp[0];
        out[1 * 4] = temp[1];
        out[2 * 4] = temp[2];
        out[3 * 4] = temp[3];
        ++out;
      }
    }
    // Setup in/out for next pass.
    in_low = intermediate;
    out = output;
  }

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j)
      output[j + i * 4] = (output[j + i * 4] + 1) >> 2;
  }
}

void aom_fdct4x4_lp_c(const int16_t *input, int16_t *output, int stride) {
  // The 2D transform is done with two passes which are actually pretty
  // similar. In the first one, we transform the columns and transpose
  // the results. In the second one, we transform the rows.
  // We need an intermediate buffer between passes.
  int16_t intermediate[4 * 4];
  const int16_t *in_low = NULL;
  int16_t *out = intermediate;
  // Do the two transform passes
  for (int pass = 0; pass < 2; ++pass) {
    int32_t in_high[4];  // canbe16
    int32_t step[4];     // canbe16
    int16_t temp[4];
    for (int i = 0; i < 4; ++i) {
      // Load inputs.
      if (pass == 0) {
        in_high[0] = input[0 * stride] * 16;
        in_high[1] = input[1 * stride] * 16;
        in_high[2] = input[2 * stride] * 16;
        in_high[3] = input[3 * stride] * 16;
        ++input;
        if (i == 0 && in_high[0]) {
          ++in_high[0];
        }
      } else {
        assert(in_low != NULL);
        in_high[0] = in_low[0 * 4];
        in_high[1] = in_low[1 * 4];
        in_high[2] = in_low[2 * 4];
        in_high[3] = in_low[3 * 4];
        ++in_low;
      }
      // Transform.
      step[0] = in_high[0] + in_high[3];
      step[1] = in_high[1] + in_high[2];
      step[2] = in_high[1] - in_high[2];
      step[3] = in_high[0] - in_high[3];
      temp[0] = (int16_t)fdct_round_shift((step[0] + step[1]) * cospi_16_64);
      temp[2] = (int16_t)fdct_round_shift((step[0] - step[1]) * cospi_16_64);
      temp[1] = (int16_t)fdct_round_shift(step[2] * cospi_24_64 +
                                          step[3] * cospi_8_64);
      temp[3] = (int16_t)fdct_round_shift(-step[2] * cospi_8_64 +
                                          step[3] * cospi_24_64);
      // Only transpose the first pass.
      if (pass == 0) {
        out[0] = temp[0];
        out[1] = temp[1];
        out[2] = temp[2];
        out[3] = temp[3];
        out += 4;
      } else {
        out[0 * 4] = temp[0];
        out[1 * 4] = temp[1];
        out[2 * 4] = temp[2];
        out[3 * 4] = temp[3];
        ++out;
      }
    }
    // Setup in/out for next pass.
    in_low = intermediate;
    out = output;
  }

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j)
      output[j + i * 4] = (output[j + i * 4] + 1) >> 2;
  }
}

#if CONFIG_INTERNAL_STATS
void aom_fdct8x8_c(const int16_t *input, tran_low_t *final_output, int stride) {
  int i, j;
  tran_low_t intermediate[64];
  int pass;
  tran_low_t *output = intermediate;
  const tran_low_t *in = NULL;

  // Transform columns
  for (pass = 0; pass < 2; ++pass) {
    tran_high_t s0, s1, s2, s3, s4, s5, s6, s7;  // canbe16
    tran_high_t t0, t1, t2, t3;                  // needs32
    tran_high_t x0, x1, x2, x3;                  // canbe16

    for (i = 0; i < 8; i++) {
      // stage 1
      if (pass == 0) {
        s0 = (input[0 * stride] + input[7 * stride]) * 4;
        s1 = (input[1 * stride] + input[6 * stride]) * 4;
        s2 = (input[2 * stride] + input[5 * stride]) * 4;
        s3 = (input[3 * stride] + input[4 * stride]) * 4;
        s4 = (input[3 * stride] - input[4 * stride]) * 4;
        s5 = (input[2 * stride] - input[5 * stride]) * 4;
        s6 = (input[1 * stride] - input[6 * stride]) * 4;
        s7 = (input[0 * stride] - input[7 * stride]) * 4;
        ++input;
      } else {
        s0 = in[0 * 8] + in[7 * 8];
        s1 = in[1 * 8] + in[6 * 8];
        s2 = in[2 * 8] + in[5 * 8];
        s3 = in[3 * 8] + in[4 * 8];
        s4 = in[3 * 8] - in[4 * 8];
        s5 = in[2 * 8] - in[5 * 8];
        s6 = in[1 * 8] - in[6 * 8];
        s7 = in[0 * 8] - in[7 * 8];
        ++in;
      }

      // fdct4(step, step);
      x0 = s0 + s3;
      x1 = s1 + s2;
      x2 = s1 - s2;
      x3 = s0 - s3;
      t0 = (x0 + x1) * cospi_16_64;
      t1 = (x0 - x1) * cospi_16_64;
      t2 = x2 * cospi_24_64 + x3 * cospi_8_64;
      t3 = -x2 * cospi_8_64 + x3 * cospi_24_64;
      output[0] = (tran_low_t)fdct_round_shift(t0);
      output[2] = (tran_low_t)fdct_round_shift(t2);
      output[4] = (tran_low_t)fdct_round_shift(t1);
      output[6] = (tran_low_t)fdct_round_shift(t3);

      // Stage 2
      t0 = (s6 - s5) * cospi_16_64;
      t1 = (s6 + s5) * cospi_16_64;
      t2 = fdct_round_shift(t0);
      t3 = fdct_round_shift(t1);

      // Stage 3
      x0 = s4 + t2;
      x1 = s4 - t2;
      x2 = s7 - t3;
      x3 = s7 + t3;

      // Stage 4
      t0 = x0 * cospi_28_64 + x3 * cospi_4_64;
      t1 = x1 * cospi_12_64 + x2 * cospi_20_64;
      t2 = x2 * cospi_12_64 + x1 * -cospi_20_64;
      t3 = x3 * cospi_28_64 + x0 * -cospi_4_64;
      output[1] = (tran_low_t)fdct_round_shift(t0);
      output[3] = (tran_low_t)fdct_round_shift(t2);
      output[5] = (tran_low_t)fdct_round_shift(t1);
      output[7] = (tran_low_t)fdct_round_shift(t3);
      output += 8;
    }
    in = intermediate;
    output = final_output;
  }

  // Rows
  for (i = 0; i < 8; ++i) {
    for (j = 0; j < 8; ++j) final_output[j + i * 8] /= 2;
  }
}
#endif  // CONFIG_INTERNAL_STATS

#if CONFIG_AV1_HIGHBITDEPTH && CONFIG_INTERNAL_STATS
void aom_highbd_fdct8x8_c(const int16_t *input, tran_low_t *final_output,
                          int stride) {
  aom_fdct8x8_c(input, final_output, stride);
}
#endif
