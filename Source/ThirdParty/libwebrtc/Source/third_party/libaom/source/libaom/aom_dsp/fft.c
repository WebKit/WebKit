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

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/fft_common.h"

static INLINE void simple_transpose(const float *A, float *B, int n) {
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      B[y * n + x] = A[x * n + y];
    }
  }
}

// The 1d transform is real to complex and packs the complex results in
// a way to take advantage of conjugate symmetry (e.g., the n/2 + 1 real
// components, followed by the n/2 - 1 imaginary components). After the
// transform is done on the rows, the first n/2 + 1 columns are real, and
// the remaining are the imaginary components. After the transform on the
// columns, the region of [0, n/2]x[0, n/2] contains the real part of
// fft of the real columns. The real part of the 2d fft also includes the
// imaginary part of transformed imaginary columns. This function assembles
// the correct outputs while putting the real and imaginary components
// next to each other.
static INLINE void unpack_2d_output(const float *col_fft, float *output,
                                    int n) {
  for (int y = 0; y <= n / 2; ++y) {
    const int y2 = y + n / 2;
    const int y_extra = y2 > n / 2 && y2 < n;

    for (int x = 0; x <= n / 2; ++x) {
      const int x2 = x + n / 2;
      const int x_extra = x2 > n / 2 && x2 < n;
      output[2 * (y * n + x)] =
          col_fft[y * n + x] - (x_extra && y_extra ? col_fft[y2 * n + x2] : 0);
      output[2 * (y * n + x) + 1] = (y_extra ? col_fft[y2 * n + x] : 0) +
                                    (x_extra ? col_fft[y * n + x2] : 0);
      if (y_extra) {
        output[2 * ((n - y) * n + x)] =
            col_fft[y * n + x] +
            (x_extra && y_extra ? col_fft[y2 * n + x2] : 0);
        output[2 * ((n - y) * n + x) + 1] =
            -(y_extra ? col_fft[y2 * n + x] : 0) +
            (x_extra ? col_fft[y * n + x2] : 0);
      }
    }
  }
}

void aom_fft_2d_gen(const float *input, float *temp, float *output, int n,
                    aom_fft_1d_func_t tform, aom_fft_transpose_func_t transpose,
                    aom_fft_unpack_func_t unpack, int vec_size) {
  for (int x = 0; x < n; x += vec_size) {
    tform(input + x, output + x, n);
  }
  transpose(output, temp, n);

  for (int x = 0; x < n; x += vec_size) {
    tform(temp + x, output + x, n);
  }
  transpose(output, temp, n);

  unpack(temp, output, n);
}

static INLINE void store_float(float *output, float input) { *output = input; }
static INLINE float add_float(float a, float b) { return a + b; }
static INLINE float sub_float(float a, float b) { return a - b; }
static INLINE float mul_float(float a, float b) { return a * b; }

GEN_FFT_2(void, float, float, float, *, store_float)
GEN_FFT_4(void, float, float, float, *, store_float, (float), add_float,
          sub_float)
GEN_FFT_8(void, float, float, float, *, store_float, (float), add_float,
          sub_float, mul_float)
GEN_FFT_16(void, float, float, float, *, store_float, (float), add_float,
           sub_float, mul_float)
GEN_FFT_32(void, float, float, float, *, store_float, (float), add_float,
           sub_float, mul_float)

void aom_fft2x2_float_c(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 2, aom_fft1d_2_float, simple_transpose,
                 unpack_2d_output, 1);
}

void aom_fft4x4_float_c(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 4, aom_fft1d_4_float, simple_transpose,
                 unpack_2d_output, 1);
}

void aom_fft8x8_float_c(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 8, aom_fft1d_8_float, simple_transpose,
                 unpack_2d_output, 1);
}

void aom_fft16x16_float_c(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 16, aom_fft1d_16_float, simple_transpose,
                 unpack_2d_output, 1);
}

void aom_fft32x32_float_c(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 32, aom_fft1d_32_float, simple_transpose,
                 unpack_2d_output, 1);
}

void aom_ifft_2d_gen(const float *input, float *temp, float *output, int n,
                     aom_fft_1d_func_t fft_single, aom_fft_1d_func_t fft_multi,
                     aom_fft_1d_func_t ifft_multi,
                     aom_fft_transpose_func_t transpose, int vec_size) {
  // Column 0 and n/2 have conjugate symmetry, so we can directly do the ifft
  // and get real outputs.
  for (int y = 0; y <= n / 2; ++y) {
    output[y * n] = input[2 * y * n];
    output[y * n + 1] = input[2 * (y * n + n / 2)];
  }
  for (int y = n / 2 + 1; y < n; ++y) {
    output[y * n] = input[2 * (y - n / 2) * n + 1];
    output[y * n + 1] = input[2 * ((y - n / 2) * n + n / 2) + 1];
  }

  for (int i = 0; i < 2; i += vec_size) {
    ifft_multi(output + i, temp + i, n);
  }

  // For the other columns, since we don't have a full ifft for complex inputs
  // we have to split them into the real and imaginary counterparts.
  // Pack the real component, then the imaginary components.
  for (int y = 0; y < n; ++y) {
    for (int x = 1; x < n / 2; ++x) {
      output[y * n + (x + 1)] = input[2 * (y * n + x)];
    }
    for (int x = 1; x < n / 2; ++x) {
      output[y * n + (x + n / 2)] = input[2 * (y * n + x) + 1];
    }
  }
  for (int y = 2; y < vec_size; y++) {
    fft_single(output + y, temp + y, n);
  }
  // This is the part that can be sped up with SIMD
  for (int y = AOMMAX(2, vec_size); y < n; y += vec_size) {
    fft_multi(output + y, temp + y, n);
  }

  // Put the 0 and n/2 th results in the correct place.
  for (int x = 0; x < n; ++x) {
    output[x] = temp[x * n];
    output[(n / 2) * n + x] = temp[x * n + 1];
  }
  // This rearranges and transposes.
  for (int y = 1; y < n / 2; ++y) {
    // Fill in the real columns
    for (int x = 0; x <= n / 2; ++x) {
      output[x + y * n] =
          temp[(y + 1) + x * n] +
          ((x > 0 && x < n / 2) ? temp[(y + n / 2) + (x + n / 2) * n] : 0);
    }
    for (int x = n / 2 + 1; x < n; ++x) {
      output[x + y * n] = temp[(y + 1) + (n - x) * n] -
                          temp[(y + n / 2) + ((n - x) + n / 2) * n];
    }
    // Fill in the imag columns
    for (int x = 0; x <= n / 2; ++x) {
      output[x + (y + n / 2) * n] =
          temp[(y + n / 2) + x * n] -
          ((x > 0 && x < n / 2) ? temp[(y + 1) + (x + n / 2) * n] : 0);
    }
    for (int x = n / 2 + 1; x < n; ++x) {
      output[x + (y + n / 2) * n] = temp[(y + 1) + ((n - x) + n / 2) * n] +
                                    temp[(y + n / 2) + (n - x) * n];
    }
  }
  for (int y = 0; y < n; y += vec_size) {
    ifft_multi(output + y, temp + y, n);
  }
  transpose(temp, output, n);
}

GEN_IFFT_2(void, float, float, float, *, store_float)
GEN_IFFT_4(void, float, float, float, *, store_float, (float), add_float,
           sub_float)
GEN_IFFT_8(void, float, float, float, *, store_float, (float), add_float,
           sub_float, mul_float)
GEN_IFFT_16(void, float, float, float, *, store_float, (float), add_float,
            sub_float, mul_float)
GEN_IFFT_32(void, float, float, float, *, store_float, (float), add_float,
            sub_float, mul_float)

void aom_ifft2x2_float_c(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 2, aom_fft1d_2_float, aom_fft1d_2_float,
                  aom_ifft1d_2_float, simple_transpose, 1);
}

void aom_ifft4x4_float_c(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 4, aom_fft1d_4_float, aom_fft1d_4_float,
                  aom_ifft1d_4_float, simple_transpose, 1);
}

void aom_ifft8x8_float_c(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 8, aom_fft1d_8_float, aom_fft1d_8_float,
                  aom_ifft1d_8_float, simple_transpose, 1);
}

void aom_ifft16x16_float_c(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 16, aom_fft1d_16_float,
                  aom_fft1d_16_float, aom_ifft1d_16_float, simple_transpose, 1);
}

void aom_ifft32x32_float_c(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 32, aom_fft1d_32_float,
                  aom_fft1d_32_float, aom_ifft1d_32_float, simple_transpose, 1);
}
