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
#include <stdint.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"

// SAD, SAD_SKIP and SAD_AVG for 64xh blocks
#if !CONFIG_HIGHWAY
static inline unsigned int sad64xh_avx2(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  int i;
  __m256i sad1_reg, sad2_reg, ref1_reg, ref2_reg;
  __m256i sum_sad = _mm256_setzero_si256();
  __m256i sum_sad_h;
  __m128i sum_sad128;
  for (i = 0; i < h; i++) {
    ref1_reg = _mm256_loadu_si256((__m256i const *)ref_ptr);
    ref2_reg = _mm256_loadu_si256((__m256i const *)(ref_ptr + 32));
    sad1_reg =
        _mm256_sad_epu8(ref1_reg, _mm256_loadu_si256((__m256i const *)src_ptr));
    sad2_reg = _mm256_sad_epu8(
        ref2_reg, _mm256_loadu_si256((__m256i const *)(src_ptr + 32)));
    sum_sad = _mm256_add_epi32(sum_sad, _mm256_add_epi32(sad1_reg, sad2_reg));
    ref_ptr += ref_stride;
    src_ptr += src_stride;
  }
  sum_sad_h = _mm256_srli_si256(sum_sad, 8);
  sum_sad = _mm256_add_epi32(sum_sad, sum_sad_h);
  sum_sad128 = _mm256_extracti128_si256(sum_sad, 1);
  sum_sad128 = _mm_add_epi32(_mm256_castsi256_si128(sum_sad), sum_sad128);
  unsigned int res = (unsigned int)_mm_cvtsi128_si32(sum_sad128);
  _mm256_zeroupper();
  return res;
}

#define FSAD64_H(h)                                                           \
  unsigned int aom_sad64x##h##_avx2(const uint8_t *src_ptr, int src_stride,   \
                                    const uint8_t *ref_ptr, int ref_stride) { \
    return sad64xh_avx2(src_ptr, src_stride, ref_ptr, ref_stride, h);         \
  }

#define FSADS64_H(h)                                                          \
  unsigned int aom_sad_skip_64x##h##_avx2(                                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,         \
      int ref_stride) {                                                       \
    return 2 * sad64xh_avx2(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2, \
                            h / 2);                                           \
  }

#define FSAD64  \
  FSAD64_H(64)  \
  FSAD64_H(32)  \
  FSADS64_H(64) \
  FSADS64_H(32)

/* clang-format off */
FSAD64
/* clang-format on */

#undef FSAD64
#undef FSAD64_H

#define FSADAVG64_H(h)                                                        \
  unsigned int aom_sad64x##h##_avg_avx2(                                      \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,         \
      int ref_stride, const uint8_t *second_pred) {                           \
    int i;                                                                    \
    __m256i sad1_reg, sad2_reg, ref1_reg, ref2_reg;                           \
    __m256i sum_sad = _mm256_setzero_si256();                                 \
    __m256i sum_sad_h;                                                        \
    __m128i sum_sad128;                                                       \
    for (i = 0; i < h; i++) {                                                 \
      ref1_reg = _mm256_loadu_si256((__m256i const *)ref_ptr);                \
      ref2_reg = _mm256_loadu_si256((__m256i const *)(ref_ptr + 32));         \
      ref1_reg = _mm256_avg_epu8(                                             \
          ref1_reg, _mm256_loadu_si256((__m256i const *)second_pred));        \
      ref2_reg = _mm256_avg_epu8(                                             \
          ref2_reg, _mm256_loadu_si256((__m256i const *)(second_pred + 32))); \
      sad1_reg = _mm256_sad_epu8(                                             \
          ref1_reg, _mm256_loadu_si256((__m256i const *)src_ptr));            \
      sad2_reg = _mm256_sad_epu8(                                             \
          ref2_reg, _mm256_loadu_si256((__m256i const *)(src_ptr + 32)));     \
      sum_sad =                                                               \
          _mm256_add_epi32(sum_sad, _mm256_add_epi32(sad1_reg, sad2_reg));    \
      ref_ptr += ref_stride;                                                  \
      src_ptr += src_stride;                                                  \
      second_pred += 64;                                                      \
    }                                                                         \
    sum_sad_h = _mm256_srli_si256(sum_sad, 8);                                \
    sum_sad = _mm256_add_epi32(sum_sad, sum_sad_h);                           \
    sum_sad128 = _mm256_extracti128_si256(sum_sad, 1);                        \
    sum_sad128 = _mm_add_epi32(_mm256_castsi256_si128(sum_sad), sum_sad128);  \
    unsigned int res = (unsigned int)_mm_cvtsi128_si32(sum_sad128);           \
    _mm256_zeroupper();                                                       \
    return res;                                                               \
  }

#define FSADAVG64 \
  FSADAVG64_H(64) \
  FSADAVG64_H(32)

/* clang-format off */
FSADAVG64
/* clang-format on */

#undef FSADAVG64
#undef FSADAVG64_H
#endif  // !CONFIG_HIGHWAY

// SAD, SAD_SKIP and SAD_AVG for 32xh blocks
static inline unsigned int sad32xh_avx2(const uint8_t *src_ptr, int src_stride,
                                        const uint8_t *ref_ptr, int ref_stride,
                                        int h) {
  int i;
  __m256i sad1_reg, sad2_reg, ref1_reg, ref2_reg;
  __m256i sum_sad = _mm256_setzero_si256();
  __m256i sum_sad_h;
  __m128i sum_sad128;
  int ref2_stride = ref_stride << 1;
  int src2_stride = src_stride << 1;
  int max = h >> 1;
  for (i = 0; i < max; i++) {
    ref1_reg = _mm256_loadu_si256((__m256i const *)ref_ptr);
    ref2_reg = _mm256_loadu_si256((__m256i const *)(ref_ptr + ref_stride));
    sad1_reg =
        _mm256_sad_epu8(ref1_reg, _mm256_loadu_si256((__m256i const *)src_ptr));
    sad2_reg = _mm256_sad_epu8(
        ref2_reg, _mm256_loadu_si256((__m256i const *)(src_ptr + src_stride)));
    sum_sad = _mm256_add_epi32(sum_sad, _mm256_add_epi32(sad1_reg, sad2_reg));
    ref_ptr += ref2_stride;
    src_ptr += src2_stride;
  }
  sum_sad_h = _mm256_srli_si256(sum_sad, 8);
  sum_sad = _mm256_add_epi32(sum_sad, sum_sad_h);
  sum_sad128 = _mm256_extracti128_si256(sum_sad, 1);
  sum_sad128 = _mm_add_epi32(_mm256_castsi256_si128(sum_sad), sum_sad128);
  unsigned int res = (unsigned int)_mm_cvtsi128_si32(sum_sad128);
  _mm256_zeroupper();
  return res;
}

#define FSAD32_H(h)                                                           \
  unsigned int aom_sad32x##h##_avx2(const uint8_t *src_ptr, int src_stride,   \
                                    const uint8_t *ref_ptr, int ref_stride) { \
    return sad32xh_avx2(src_ptr, src_stride, ref_ptr, ref_stride, h);         \
  }

#define FSADS32_H(h)                                                          \
  unsigned int aom_sad_skip_32x##h##_avx2(                                    \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,         \
      int ref_stride) {                                                       \
    return 2 * sad32xh_avx2(src_ptr, src_stride * 2, ref_ptr, ref_stride * 2, \
                            h / 2);                                           \
  }

#define FSAD32  \
  FSAD32_H(64)  \
  FSAD32_H(32)  \
  FSAD32_H(16)  \
  FSADS32_H(64) \
  FSADS32_H(32) \
  FSADS32_H(16)

/* clang-format off */
FSAD32
/* clang-format on */

#undef FSAD32
#undef FSAD32_H

#define FSADAVG32_H(h)                                                        \
  unsigned int aom_sad32x##h##_avg_avx2(                                      \
      const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr,         \
      int ref_stride, const uint8_t *second_pred) {                           \
    int i;                                                                    \
    __m256i sad1_reg, sad2_reg, ref1_reg, ref2_reg;                           \
    __m256i sum_sad = _mm256_setzero_si256();                                 \
    __m256i sum_sad_h;                                                        \
    __m128i sum_sad128;                                                       \
    int ref2_stride = ref_stride << 1;                                        \
    int src2_stride = src_stride << 1;                                        \
    int max = h >> 1;                                                         \
    for (i = 0; i < max; i++) {                                               \
      ref1_reg = _mm256_loadu_si256((__m256i const *)ref_ptr);                \
      ref2_reg = _mm256_loadu_si256((__m256i const *)(ref_ptr + ref_stride)); \
      ref1_reg = _mm256_avg_epu8(                                             \
          ref1_reg, _mm256_loadu_si256((__m256i const *)second_pred));        \
      ref2_reg = _mm256_avg_epu8(                                             \
          ref2_reg, _mm256_loadu_si256((__m256i const *)(second_pred + 32))); \
      sad1_reg = _mm256_sad_epu8(                                             \
          ref1_reg, _mm256_loadu_si256((__m256i const *)src_ptr));            \
      sad2_reg = _mm256_sad_epu8(                                             \
          ref2_reg,                                                           \
          _mm256_loadu_si256((__m256i const *)(src_ptr + src_stride)));       \
      sum_sad =                                                               \
          _mm256_add_epi32(sum_sad, _mm256_add_epi32(sad1_reg, sad2_reg));    \
      ref_ptr += ref2_stride;                                                 \
      src_ptr += src2_stride;                                                 \
      second_pred += 64;                                                      \
    }                                                                         \
    sum_sad_h = _mm256_srli_si256(sum_sad, 8);                                \
    sum_sad = _mm256_add_epi32(sum_sad, sum_sad_h);                           \
    sum_sad128 = _mm256_extracti128_si256(sum_sad, 1);                        \
    sum_sad128 = _mm_add_epi32(_mm256_castsi256_si128(sum_sad), sum_sad128);  \
    unsigned int res = (unsigned int)_mm_cvtsi128_si32(sum_sad128);           \
    _mm256_zeroupper();                                                       \
    return res;                                                               \
  }

#define FSADAVG32 \
  FSADAVG32_H(64) \
  FSADAVG32_H(32) \
  FSADAVG32_H(16)

/* clang-format off */
FSADAVG32
/* clang-format on */

#undef FSADAVG32
#undef FSADAVG32_H
