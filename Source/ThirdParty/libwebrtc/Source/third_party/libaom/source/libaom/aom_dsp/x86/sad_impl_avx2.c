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

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

static unsigned int sad32x32(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *ref_ptr, int ref_stride) {
  __m256i s1, s2, r1, r2;
  __m256i sum = _mm256_setzero_si256();
  __m128i sum_i128;
  int i;

  for (i = 0; i < 16; ++i) {
    r1 = _mm256_loadu_si256((__m256i const *)ref_ptr);
    r2 = _mm256_loadu_si256((__m256i const *)(ref_ptr + ref_stride));
    s1 = _mm256_sad_epu8(r1, _mm256_loadu_si256((__m256i const *)src_ptr));
    s2 = _mm256_sad_epu8(
        r2, _mm256_loadu_si256((__m256i const *)(src_ptr + src_stride)));
    sum = _mm256_add_epi32(sum, _mm256_add_epi32(s1, s2));
    ref_ptr += ref_stride << 1;
    src_ptr += src_stride << 1;
  }

  sum = _mm256_add_epi32(sum, _mm256_srli_si256(sum, 8));
  sum_i128 = _mm_add_epi32(_mm256_extracti128_si256(sum, 1),
                           _mm256_castsi256_si128(sum));
  return (unsigned int)_mm_cvtsi128_si32(sum_i128);
}

static unsigned int sad64x32(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *ref_ptr, int ref_stride) {
  unsigned int half_width = 32;
  uint32_t sum = sad32x32(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += half_width;
  ref_ptr += half_width;
  sum += sad32x32(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

static unsigned int sad64x64(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *ref_ptr, int ref_stride) {
  uint32_t sum = sad64x32(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += src_stride << 5;
  ref_ptr += ref_stride << 5;
  sum += sad64x32(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

unsigned int aom_sad128x64_avx2(const uint8_t *src_ptr, int src_stride,
                                const uint8_t *ref_ptr, int ref_stride) {
  unsigned int half_width = 64;
  uint32_t sum = sad64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += half_width;
  ref_ptr += half_width;
  sum += sad64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

unsigned int aom_sad64x128_avx2(const uint8_t *src_ptr, int src_stride,
                                const uint8_t *ref_ptr, int ref_stride) {
  uint32_t sum = sad64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += src_stride << 6;
  ref_ptr += ref_stride << 6;
  sum += sad64x64(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

unsigned int aom_sad128x128_avx2(const uint8_t *src_ptr, int src_stride,
                                 const uint8_t *ref_ptr, int ref_stride) {
  uint32_t sum = aom_sad128x64_avx2(src_ptr, src_stride, ref_ptr, ref_stride);
  src_ptr += src_stride << 6;
  ref_ptr += ref_stride << 6;
  sum += aom_sad128x64_avx2(src_ptr, src_stride, ref_ptr, ref_stride);
  return sum;
}

unsigned int aom_sad_skip_128x64_avx2(const uint8_t *src_ptr, int src_stride,
                                      const uint8_t *ref_ptr, int ref_stride) {
  const uint32_t half_width = 64;
  uint32_t sum = sad64x32(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2);
  src_ptr += half_width;
  ref_ptr += half_width;
  sum += sad64x32(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2);
  return 2 * sum;
}

unsigned int aom_sad_skip_64x128_avx2(const uint8_t *src_ptr, int src_stride,
                                      const uint8_t *ref_ptr, int ref_stride) {
  const uint32_t sum =
      sad64x64(src_ptr, 2 * src_stride, ref_ptr, 2 * ref_stride);
  return 2 * sum;
}

unsigned int aom_sad_skip_128x128_avx2(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *ref_ptr, int ref_stride) {
  const uint32_t sum =
      aom_sad128x64_avx2(src_ptr, 2 * src_stride, ref_ptr, 2 * ref_stride);
  return 2 * sum;
}

static unsigned int sad_w64_avg_avx2(const uint8_t *src_ptr, int src_stride,
                                     const uint8_t *ref_ptr, int ref_stride,
                                     const int h, const uint8_t *second_pred,
                                     const int second_pred_stride) {
  int i;
  __m256i sad1_reg, sad2_reg, ref1_reg, ref2_reg;
  __m256i sum_sad = _mm256_setzero_si256();
  __m256i sum_sad_h;
  __m128i sum_sad128;
  for (i = 0; i < h; i++) {
    ref1_reg = _mm256_loadu_si256((__m256i const *)ref_ptr);
    ref2_reg = _mm256_loadu_si256((__m256i const *)(ref_ptr + 32));
    ref1_reg = _mm256_avg_epu8(
        ref1_reg, _mm256_loadu_si256((__m256i const *)second_pred));
    ref2_reg = _mm256_avg_epu8(
        ref2_reg, _mm256_loadu_si256((__m256i const *)(second_pred + 32)));
    sad1_reg =
        _mm256_sad_epu8(ref1_reg, _mm256_loadu_si256((__m256i const *)src_ptr));
    sad2_reg = _mm256_sad_epu8(
        ref2_reg, _mm256_loadu_si256((__m256i const *)(src_ptr + 32)));
    sum_sad = _mm256_add_epi32(sum_sad, _mm256_add_epi32(sad1_reg, sad2_reg));
    ref_ptr += ref_stride;
    src_ptr += src_stride;
    second_pred += second_pred_stride;
  }
  sum_sad_h = _mm256_srli_si256(sum_sad, 8);
  sum_sad = _mm256_add_epi32(sum_sad, sum_sad_h);
  sum_sad128 = _mm256_extracti128_si256(sum_sad, 1);
  sum_sad128 = _mm_add_epi32(_mm256_castsi256_si128(sum_sad), sum_sad128);
  return (unsigned int)_mm_cvtsi128_si32(sum_sad128);
}

unsigned int aom_sad64x128_avg_avx2(const uint8_t *src_ptr, int src_stride,
                                    const uint8_t *ref_ptr, int ref_stride,
                                    const uint8_t *second_pred) {
  uint32_t sum = sad_w64_avg_avx2(src_ptr, src_stride, ref_ptr, ref_stride, 64,
                                  second_pred, 64);
  src_ptr += src_stride << 6;
  ref_ptr += ref_stride << 6;
  second_pred += 64 << 6;
  sum += sad_w64_avg_avx2(src_ptr, src_stride, ref_ptr, ref_stride, 64,
                          second_pred, 64);
  return sum;
}

unsigned int aom_sad128x64_avg_avx2(const uint8_t *src_ptr, int src_stride,
                                    const uint8_t *ref_ptr, int ref_stride,
                                    const uint8_t *second_pred) {
  unsigned int half_width = 64;
  uint32_t sum = sad_w64_avg_avx2(src_ptr, src_stride, ref_ptr, ref_stride, 64,
                                  second_pred, 128);
  src_ptr += half_width;
  ref_ptr += half_width;
  second_pred += half_width;
  sum += sad_w64_avg_avx2(src_ptr, src_stride, ref_ptr, ref_stride, 64,
                          second_pred, 128);
  return sum;
}

unsigned int aom_sad128x128_avg_avx2(const uint8_t *src_ptr, int src_stride,
                                     const uint8_t *ref_ptr, int ref_stride,
                                     const uint8_t *second_pred) {
  uint32_t sum = aom_sad128x64_avg_avx2(src_ptr, src_stride, ref_ptr,
                                        ref_stride, second_pred);
  src_ptr += src_stride << 6;
  ref_ptr += ref_stride << 6;
  second_pred += 128 << 6;
  sum += aom_sad128x64_avg_avx2(src_ptr, src_stride, ref_ptr, ref_stride,
                                second_pred);
  return sum;
}
