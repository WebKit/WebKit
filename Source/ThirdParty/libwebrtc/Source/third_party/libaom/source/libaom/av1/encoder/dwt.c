/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include "config/av1_rtcd.h"
#include "av1/encoder/dwt.h"

// Note: block length must be even for this implementation
static void analysis_53_row(int length, tran_low_t *x, tran_low_t *lowpass,
                            tran_low_t *highpass) {
  int n;
  tran_low_t r, *a, *b;

  n = length >> 1;
  b = highpass;
  a = lowpass;
  while (--n) {
    *a++ = (r = *x++) * 2;
    *b++ = *x - ((r + x[1] + 1) >> 1);
    x++;
  }
  *a = (r = *x++) * 2;
  *b = *x - r;

  n = length >> 1;
  b = highpass;
  a = lowpass;
  r = *highpass;
  while (n--) {
    *a++ += (r + (*b) + 1) >> 1;
    r = *b++;
  }
}

static void analysis_53_col(int length, tran_low_t *x, tran_low_t *lowpass,
                            tran_low_t *highpass) {
  int n;
  tran_low_t r, *a, *b;

  n = length >> 1;
  b = highpass;
  a = lowpass;
  while (--n) {
    *a++ = (r = *x++);
    *b++ = (((*x) * 2) - (r + x[1]) + 2) >> 2;
    x++;
  }
  *a = (r = *x++);
  *b = (*x - r + 1) >> 1;

  n = length >> 1;
  b = highpass;
  a = lowpass;
  r = *highpass;
  while (n--) {
    *a++ += (r + (*b) + 1) >> 1;
    r = *b++;
  }
}

static void dyadic_analyze_53_uint8_input(int levels, int width, int height,
                                          const uint8_t *x, int pitch_x,
                                          tran_low_t *c, int pitch_c,
                                          int dwt_scale_bits, int hbd) {
  int lv, i, j, nh, nw, hh = height, hw = width;
  tran_low_t buffer[2 * DWT_MAX_LENGTH];

  if (hbd) {
    const uint16_t *x16 = CONVERT_TO_SHORTPTR(x);
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        c[i * pitch_c + j] = x16[i * pitch_x + j] << dwt_scale_bits;
      }
    }
  } else {
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        c[i * pitch_c + j] = x[i * pitch_x + j] << dwt_scale_bits;
      }
    }
  }

  for (lv = 0; lv < levels; lv++) {
    nh = hh;
    hh = (hh + 1) >> 1;
    nw = hw;
    hw = (hw + 1) >> 1;
    if ((nh < 2) || (nw < 2)) return;
    for (i = 0; i < nh; i++) {
      memcpy(buffer, &c[i * pitch_c], nw * sizeof(tran_low_t));
      analysis_53_row(nw, buffer, &c[i * pitch_c], &c[i * pitch_c] + hw);
    }
    for (j = 0; j < nw; j++) {
      for (i = 0; i < nh; i++) buffer[i + nh] = c[i * pitch_c + j];
      analysis_53_col(nh, buffer + nh, buffer, buffer + hh);
      for (i = 0; i < nh; i++) c[i * pitch_c + j] = buffer[i];
    }
  }
}

void av1_fdwt8x8_uint8_input_c(const uint8_t *input, tran_low_t *output,
                               int stride, int hbd) {
  dyadic_analyze_53_uint8_input(4, 8, 8, input, stride, output, 8, 2, hbd);
}

static int haar_ac_sad(const tran_low_t *output, int bw, int bh, int stride) {
  int acsad = 0;

  for (int r = 0; r < bh; ++r)
    for (int c = 0; c < bw; ++c) {
      if (r >= bh / 2 || c >= bw / 2) acsad += abs(output[r * stride + c]);
    }
  return acsad;
}

static int haar_ac_sad_8x8_uint8_input(const uint8_t *input, int stride,
                                       int hbd) {
  tran_low_t output[64];

  av1_fdwt8x8_uint8_input(input, output, stride, hbd);
  return haar_ac_sad(output, 8, 8, 8);
}

int64_t av1_haar_ac_sad_mxn_uint8_input(const uint8_t *input, int stride,
                                        int hbd, int num_8x8_rows,
                                        int num_8x8_cols) {
  int64_t wavelet_energy = 0;
  for (int r8 = 0; r8 < num_8x8_rows; ++r8) {
    for (int c8 = 0; c8 < num_8x8_cols; ++c8) {
      wavelet_energy += haar_ac_sad_8x8_uint8_input(
          input + c8 * 8 + r8 * 8 * stride, stride, hbd);
    }
  }
  return wavelet_energy;
}
