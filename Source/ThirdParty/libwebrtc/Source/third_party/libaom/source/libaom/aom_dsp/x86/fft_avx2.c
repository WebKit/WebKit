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

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/fft_common.h"

extern void aom_transpose_float_sse2(const float *A, float *B, int n);
extern void aom_fft_unpack_2d_output_sse2(const float *col_fft, float *output,
                                          int n);

// Generate the 1d forward transforms for float using _mm256
GEN_FFT_8(static INLINE void, avx2, float, __m256, _mm256_load_ps,
          _mm256_store_ps, _mm256_set1_ps, _mm256_add_ps, _mm256_sub_ps,
          _mm256_mul_ps)
GEN_FFT_16(static INLINE void, avx2, float, __m256, _mm256_load_ps,
           _mm256_store_ps, _mm256_set1_ps, _mm256_add_ps, _mm256_sub_ps,
           _mm256_mul_ps)
GEN_FFT_32(static INLINE void, avx2, float, __m256, _mm256_load_ps,
           _mm256_store_ps, _mm256_set1_ps, _mm256_add_ps, _mm256_sub_ps,
           _mm256_mul_ps)

void aom_fft8x8_float_avx2(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 8, aom_fft1d_8_avx2,
                 aom_transpose_float_sse2, aom_fft_unpack_2d_output_sse2, 8);
}

void aom_fft16x16_float_avx2(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 16, aom_fft1d_16_avx2,
                 aom_transpose_float_sse2, aom_fft_unpack_2d_output_sse2, 8);
}

void aom_fft32x32_float_avx2(const float *input, float *temp, float *output) {
  aom_fft_2d_gen(input, temp, output, 32, aom_fft1d_32_avx2,
                 aom_transpose_float_sse2, aom_fft_unpack_2d_output_sse2, 8);
}

// Generate the 1d inverse transforms for float using _mm256
GEN_IFFT_8(static INLINE void, avx2, float, __m256, _mm256_load_ps,
           _mm256_store_ps, _mm256_set1_ps, _mm256_add_ps, _mm256_sub_ps,
           _mm256_mul_ps)
GEN_IFFT_16(static INLINE void, avx2, float, __m256, _mm256_load_ps,
            _mm256_store_ps, _mm256_set1_ps, _mm256_add_ps, _mm256_sub_ps,
            _mm256_mul_ps)
GEN_IFFT_32(static INLINE void, avx2, float, __m256, _mm256_load_ps,
            _mm256_store_ps, _mm256_set1_ps, _mm256_add_ps, _mm256_sub_ps,
            _mm256_mul_ps)

void aom_ifft8x8_float_avx2(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 8, aom_fft1d_8_float, aom_fft1d_8_avx2,
                  aom_ifft1d_8_avx2, aom_transpose_float_sse2, 8);
}

void aom_ifft16x16_float_avx2(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 16, aom_fft1d_16_float,
                  aom_fft1d_16_avx2, aom_ifft1d_16_avx2,
                  aom_transpose_float_sse2, 8);
}

void aom_ifft32x32_float_avx2(const float *input, float *temp, float *output) {
  aom_ifft_2d_gen(input, temp, output, 32, aom_fft1d_32_float,
                  aom_fft1d_32_avx2, aom_ifft1d_32_avx2,
                  aom_transpose_float_sse2, 8);
}
