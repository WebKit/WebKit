/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
s * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <xmmintrin.h>

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/fft_common.h"

static inline void transpose4x4(const float *A, float *B, const int lda,
                                const int ldb) {
  __m128 row1 = _mm_load_ps(&A[0 * lda]);
  __m128 row2 = _mm_load_ps(&A[1 * lda]);
  __m128 row3 = _mm_load_ps(&A[2 * lda]);
  __m128 row4 = _mm_load_ps(&A[3 * lda]);
  _MM_TRANSPOSE4_PS(row1, row2, row3, row4);
  _mm_store_ps(&B[0 * ldb], row1);
  _mm_store_ps(&B[1 * ldb], row2);
  _mm_store_ps(&B[2 * ldb], row3);
  _mm_store_ps(&B[3 * ldb], row4);
}

// Referenced by fft_avx2.c.
void aom_transpose_float_sse2(const float *A, float *B, int n);

void aom_transpose_float_sse2(const float *A, float *B, int n) {
  for (int y = 0; y < n; y += 4) {
    for (int x = 0; x < n; x += 4) {
      transpose4x4(A + y * n + x, B + x * n + y, n, n);
    }
  }
}

// Referenced by fft_avx2.c.
void aom_fft_unpack_2d_output_sse2(const float *packed, float *output, int n);

void aom_fft_unpack_2d_output_sse2(const float *packed, float *output, int n) {
  const int n2 = n / 2;
  output[0] = packed[0];
  output[1] = 0;
  output[2 * (n2 * n)] = packed[n2 * n];
  output[2 * (n2 * n) + 1] = 0;

  output[2 * n2] = packed[n2];
  output[2 * n2 + 1] = 0;
  output[2 * (n2 * n + n2)] = packed[n2 * n + n2];
  output[2 * (n2 * n + n2) + 1] = 0;

  for (int c = 1; c < n2; ++c) {
    output[2 * (0 * n + c)] = packed[c];
    output[2 * (0 * n + c) + 1] = packed[c + n2];
    output[2 * (n2 * n + c) + 0] = packed[n2 * n + c];
    output[2 * (n2 * n + c) + 1] = packed[n2 * n + c + n2];
  }
  for (int r = 1; r < n2; ++r) {
    output[2 * (r * n + 0)] = packed[r * n];
    output[2 * (r * n + 0) + 1] = packed[(r + n2) * n];
    output[2 * (r * n + n2) + 0] = packed[r * n + n2];
    output[2 * (r * n + n2) + 1] = packed[(r + n2) * n + n2];

    for (int c = 1; c < AOMMIN(n2, 4); ++c) {
      output[2 * (r * n + c)] =
          packed[r * n + c] - packed[(r + n2) * n + c + n2];
      output[2 * (r * n + c) + 1] =
          packed[(r + n2) * n + c] + packed[r * n + c + n2];
    }

    for (int c = 4; c < n2; c += 4) {
      __m128 real1 = _mm_load_ps(packed + r * n + c);
      __m128 real2 = _mm_load_ps(packed + (r + n2) * n + c + n2);
      __m128 imag1 = _mm_load_ps(packed + (r + n2) * n + c);
      __m128 imag2 = _mm_load_ps(packed + r * n + c + n2);
      real1 = _mm_sub_ps(real1, real2);
      imag1 = _mm_add_ps(imag1, imag2);
      _mm_store_ps(output + 2 * (r * n + c), _mm_unpacklo_ps(real1, imag1));
      _mm_store_ps(output + 2 * (r * n + c + 2), _mm_unpackhi_ps(real1, imag1));
    }

    int r2 = r + n2;
    int r3 = n - r2;
    output[2 * (r2 * n + 0)] = packed[r3 * n];
    output[2 * (r2 * n + 0) + 1] = -packed[(r3 + n2) * n];
    output[2 * (r2 * n + n2)] = packed[r3 * n + n2];
    output[2 * (r2 * n + n2) + 1] = -packed[(r3 + n2) * n + n2];
    for (int c = 1; c < AOMMIN(4, n2); ++c) {
      output[2 * (r2 * n + c)] =
          packed[r3 * n + c] + packed[(r3 + n2) * n + c + n2];
      output[2 * (r2 * n + c) + 1] =
          -packed[(r3 + n2) * n + c] + packed[r3 * n + c + n2];
    }
    for (int c = 4; c < n2; c += 4) {
      __m128 real1 = _mm_load_ps(packed + r3 * n + c);
      __m128 real2 = _mm_load_ps(packed + (r3 + n2) * n + c + n2);
      __m128 imag1 = _mm_load_ps(packed + (r3 + n2) * n + c);
      __m128 imag2 = _mm_load_ps(packed + r3 * n + c + n2);
      real1 = _mm_add_ps(real1, real2);
      imag1 = _mm_sub_ps(imag2, imag1);
      _mm_store_ps(output + 2 * (r2 * n + c), _mm_unpacklo_ps(real1, imag1));
      _mm_store_ps(output + 2 * (r2 * n + c + 2),
                   _mm_unpackhi_ps(real1, imag1));
    }
  }
}

// Generate definitions for 1d transforms using float and __mm128
GEN_FFT_4(static inline void, sse2, float, __m128, _mm_load_ps, _mm_store_ps,
          _mm_set1_ps, _mm_add_ps, _mm_sub_ps)
GEN_FFT_8(static inline void, sse2, float, __m128, _mm_load_ps, _mm_store_ps,
          _mm_set1_ps, _mm_add_ps, _mm_sub_ps, _mm_mul_ps)
GEN_FFT_16(static inline void, sse2, float, __m128, _mm_load_ps, _mm_store_ps,
           _mm_set1_ps, _mm_add_ps, _mm_sub_ps, _mm_mul_ps)
GEN_FFT_32(static inline void, sse2, float, __m128, _mm_load_ps, _mm_store_ps,
           _mm_set1_ps, _mm_add_ps, _mm_sub_ps, _mm_mul_ps)

void aom_fft4x4_float_sse2(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 4, aom_fft1d_4_sse2,
                 aom_transpose_float_sse2, aom_fft_unpack_2d_output_sse2, 4);
}

void aom_fft8x8_float_sse2(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 8, aom_fft1d_8_sse2,
                 aom_transpose_float_sse2, aom_fft_unpack_2d_output_sse2, 4);
}

void aom_fft16x16_float_sse2(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 16, aom_fft1d_16_sse2,
                 aom_transpose_float_sse2, aom_fft_unpack_2d_output_sse2, 4);
}

void aom_fft32x32_float_sse2(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 32, aom_fft1d_32_sse2,
                 aom_transpose_float_sse2, aom_fft_unpack_2d_output_sse2, 4);
}

// Generate definitions for 1d inverse transforms using float and mm128
GEN_IFFT_4(static inline void, sse2, float, __m128, _mm_load_ps, _mm_store_ps,
           _mm_set1_ps, _mm_add_ps, _mm_sub_ps)
GEN_IFFT_8(static inline void, sse2, float, __m128, _mm_load_ps, _mm_store_ps,
           _mm_set1_ps, _mm_add_ps, _mm_sub_ps, _mm_mul_ps)
GEN_IFFT_16(static inline void, sse2, float, __m128, _mm_load_ps, _mm_store_ps,
            _mm_set1_ps, _mm_add_ps, _mm_sub_ps, _mm_mul_ps)
GEN_IFFT_32(static inline void, sse2, float, __m128, _mm_load_ps, _mm_store_ps,
            _mm_set1_ps, _mm_add_ps, _mm_sub_ps, _mm_mul_ps)

void aom_ifft4x4_float_sse2(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 4, aom_fft1d_4_float, aom_fft1d_4_sse2,
                  aom_ifft1d_4_sse2, aom_transpose_float_sse2, 4);
}

void aom_ifft8x8_float_sse2(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 8, aom_fft1d_8_float, aom_fft1d_8_sse2,
                  aom_ifft1d_8_sse2, aom_transpose_float_sse2, 4);
}

void aom_ifft16x16_float_sse2(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 16, aom_fft1d_16_float,
                  aom_fft1d_16_sse2, aom_ifft1d_16_sse2,
                  aom_transpose_float_sse2, 4);
}

void aom_ifft32x32_float_sse2(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 32, aom_fft1d_32_float,
                  aom_fft1d_32_sse2, aom_ifft1d_32_sse2,
                  aom_transpose_float_sse2, 4);
}
