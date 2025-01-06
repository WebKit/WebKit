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
#include <immintrin.h>  // AVX2

#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/x86/synonyms_avx2.h"

static AOM_FORCE_INLINE void aggregate_and_store_sum(uint32_t res[4],
                                                     const __m256i *sum_ref0,
                                                     const __m256i *sum_ref1,
                                                     const __m256i *sum_ref2,
                                                     const __m256i *sum_ref3) {
  // In sum_ref-i the result is saved in the first 4 bytes and the other 4
  // bytes are zeroed.
  // merge sum_ref0 and sum_ref1 also sum_ref2 and sum_ref3
  // 0, 0, 1, 1
  __m256i sum_ref01 = _mm256_castps_si256(_mm256_shuffle_ps(
      _mm256_castsi256_ps(*sum_ref0), _mm256_castsi256_ps(*sum_ref1),
      _MM_SHUFFLE(2, 0, 2, 0)));
  // 2, 2, 3, 3
  __m256i sum_ref23 = _mm256_castps_si256(_mm256_shuffle_ps(
      _mm256_castsi256_ps(*sum_ref2), _mm256_castsi256_ps(*sum_ref3),
      _MM_SHUFFLE(2, 0, 2, 0)));

  // sum adjacent 32 bit integers
  __m256i sum_ref0123 = _mm256_hadd_epi32(sum_ref01, sum_ref23);

  // add the low 128 bit to the high 128 bit
  __m128i sum = _mm_add_epi32(_mm256_castsi256_si128(sum_ref0123),
                              _mm256_extractf128_si256(sum_ref0123, 1));

  _mm_storeu_si128((__m128i *)(res), sum);
}

static AOM_FORCE_INLINE void aom_sadMxNx4d_avx2(
    int M, int N, const uint8_t *src, int src_stride,
    const uint8_t *const ref[4], int ref_stride, uint32_t res[4]) {
  __m256i src_reg, ref0_reg, ref1_reg, ref2_reg, ref3_reg;
  __m256i sum_ref0, sum_ref1, sum_ref2, sum_ref3;
  int i, j;
  const uint8_t *ref0, *ref1, *ref2, *ref3;

  ref0 = ref[0];
  ref1 = ref[1];
  ref2 = ref[2];
  ref3 = ref[3];
  sum_ref0 = _mm256_setzero_si256();
  sum_ref2 = _mm256_setzero_si256();
  sum_ref1 = _mm256_setzero_si256();
  sum_ref3 = _mm256_setzero_si256();

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; j += 32) {
      // load src and all refs
      src_reg = _mm256_loadu_si256((const __m256i *)(src + j));
      ref0_reg = _mm256_loadu_si256((const __m256i *)(ref0 + j));
      ref1_reg = _mm256_loadu_si256((const __m256i *)(ref1 + j));
      ref2_reg = _mm256_loadu_si256((const __m256i *)(ref2 + j));
      ref3_reg = _mm256_loadu_si256((const __m256i *)(ref3 + j));

      // sum of the absolute differences between every ref-i to src
      ref0_reg = _mm256_sad_epu8(ref0_reg, src_reg);
      ref1_reg = _mm256_sad_epu8(ref1_reg, src_reg);
      ref2_reg = _mm256_sad_epu8(ref2_reg, src_reg);
      ref3_reg = _mm256_sad_epu8(ref3_reg, src_reg);
      // sum every ref-i
      sum_ref0 = _mm256_add_epi32(sum_ref0, ref0_reg);
      sum_ref1 = _mm256_add_epi32(sum_ref1, ref1_reg);
      sum_ref2 = _mm256_add_epi32(sum_ref2, ref2_reg);
      sum_ref3 = _mm256_add_epi32(sum_ref3, ref3_reg);
    }
    src += src_stride;
    ref0 += ref_stride;
    ref1 += ref_stride;
    ref2 += ref_stride;
    ref3 += ref_stride;
  }

  aggregate_and_store_sum(res, &sum_ref0, &sum_ref1, &sum_ref2, &sum_ref3);
}

static AOM_FORCE_INLINE void aom_sadMxNx3d_avx2(
    int M, int N, const uint8_t *src, int src_stride,
    const uint8_t *const ref[4], int ref_stride, uint32_t res[4]) {
  __m256i src_reg, ref0_reg, ref1_reg, ref2_reg;
  __m256i sum_ref0, sum_ref1, sum_ref2;
  int i, j;
  const uint8_t *ref0, *ref1, *ref2;
  const __m256i zero = _mm256_setzero_si256();

  ref0 = ref[0];
  ref1 = ref[1];
  ref2 = ref[2];
  sum_ref0 = _mm256_setzero_si256();
  sum_ref2 = _mm256_setzero_si256();
  sum_ref1 = _mm256_setzero_si256();

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; j += 32) {
      // load src and all refs
      src_reg = _mm256_loadu_si256((const __m256i *)(src + j));
      ref0_reg = _mm256_loadu_si256((const __m256i *)(ref0 + j));
      ref1_reg = _mm256_loadu_si256((const __m256i *)(ref1 + j));
      ref2_reg = _mm256_loadu_si256((const __m256i *)(ref2 + j));

      // sum of the absolute differences between every ref-i to src
      ref0_reg = _mm256_sad_epu8(ref0_reg, src_reg);
      ref1_reg = _mm256_sad_epu8(ref1_reg, src_reg);
      ref2_reg = _mm256_sad_epu8(ref2_reg, src_reg);
      // sum every ref-i
      sum_ref0 = _mm256_add_epi32(sum_ref0, ref0_reg);
      sum_ref1 = _mm256_add_epi32(sum_ref1, ref1_reg);
      sum_ref2 = _mm256_add_epi32(sum_ref2, ref2_reg);
    }
    src += src_stride;
    ref0 += ref_stride;
    ref1 += ref_stride;
    ref2 += ref_stride;
  }
  aggregate_and_store_sum(res, &sum_ref0, &sum_ref1, &sum_ref2, &zero);
}

#define SADMXN_AVX2(m, n)                                                      \
  void aom_sad##m##x##n##x4d_avx2(const uint8_t *src, int src_stride,          \
                                  const uint8_t *const ref[4], int ref_stride, \
                                  uint32_t res[4]) {                           \
    aom_sadMxNx4d_avx2(m, n, src, src_stride, ref, ref_stride, res);           \
  }                                                                            \
  void aom_sad##m##x##n##x3d_avx2(const uint8_t *src, int src_stride,          \
                                  const uint8_t *const ref[4], int ref_stride, \
                                  uint32_t res[4]) {                           \
    aom_sadMxNx3d_avx2(m, n, src, src_stride, ref, ref_stride, res);           \
  }

SADMXN_AVX2(32, 16)
SADMXN_AVX2(32, 32)
SADMXN_AVX2(32, 64)

SADMXN_AVX2(64, 32)
SADMXN_AVX2(64, 64)
SADMXN_AVX2(64, 128)

SADMXN_AVX2(128, 64)
SADMXN_AVX2(128, 128)

#if !CONFIG_REALTIME_ONLY
SADMXN_AVX2(32, 8)
SADMXN_AVX2(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#define SAD_SKIP_MXN_AVX2(m, n)                                             \
  void aom_sad_skip_##m##x##n##x4d_avx2(const uint8_t *src, int src_stride, \
                                        const uint8_t *const ref[4],        \
                                        int ref_stride, uint32_t res[4]) {  \
    aom_sadMxNx4d_avx2(m, ((n) >> 1), src, 2 * src_stride, ref,             \
                       2 * ref_stride, res);                                \
    res[0] <<= 1;                                                           \
    res[1] <<= 1;                                                           \
    res[2] <<= 1;                                                           \
    res[3] <<= 1;                                                           \
  }

SAD_SKIP_MXN_AVX2(32, 16)
SAD_SKIP_MXN_AVX2(32, 32)
SAD_SKIP_MXN_AVX2(32, 64)

SAD_SKIP_MXN_AVX2(64, 32)
SAD_SKIP_MXN_AVX2(64, 64)
SAD_SKIP_MXN_AVX2(64, 128)

SAD_SKIP_MXN_AVX2(128, 64)
SAD_SKIP_MXN_AVX2(128, 128)

#if !CONFIG_REALTIME_ONLY
SAD_SKIP_MXN_AVX2(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

static AOM_FORCE_INLINE void aom_sad16xNx3d_avx2(int N, const uint8_t *src,
                                                 int src_stride,
                                                 const uint8_t *const ref[4],
                                                 int ref_stride,
                                                 uint32_t res[4]) {
  __m256i src_reg, ref0_reg, ref1_reg, ref2_reg;
  __m256i sum_ref0, sum_ref1, sum_ref2;
  const uint8_t *ref0, *ref1, *ref2;
  const __m256i zero = _mm256_setzero_si256();
  assert(N % 2 == 0);

  ref0 = ref[0];
  ref1 = ref[1];
  ref2 = ref[2];
  sum_ref0 = _mm256_setzero_si256();
  sum_ref2 = _mm256_setzero_si256();
  sum_ref1 = _mm256_setzero_si256();

  for (int i = 0; i < N; i += 2) {
    // load src and all refs
    src_reg = yy_loadu2_128(src + src_stride, src);
    ref0_reg = yy_loadu2_128(ref0 + ref_stride, ref0);
    ref1_reg = yy_loadu2_128(ref1 + ref_stride, ref1);
    ref2_reg = yy_loadu2_128(ref2 + ref_stride, ref2);

    // sum of the absolute differences between every ref-i to src
    ref0_reg = _mm256_sad_epu8(ref0_reg, src_reg);
    ref1_reg = _mm256_sad_epu8(ref1_reg, src_reg);
    ref2_reg = _mm256_sad_epu8(ref2_reg, src_reg);

    // sum every ref-i
    sum_ref0 = _mm256_add_epi32(sum_ref0, ref0_reg);
    sum_ref1 = _mm256_add_epi32(sum_ref1, ref1_reg);
    sum_ref2 = _mm256_add_epi32(sum_ref2, ref2_reg);

    src += 2 * src_stride;
    ref0 += 2 * ref_stride;
    ref1 += 2 * ref_stride;
    ref2 += 2 * ref_stride;
  }

  aggregate_and_store_sum(res, &sum_ref0, &sum_ref1, &sum_ref2, &zero);
}

static AOM_FORCE_INLINE void aom_sad16xNx4d_avx2(int N, const uint8_t *src,
                                                 int src_stride,
                                                 const uint8_t *const ref[4],
                                                 int ref_stride,
                                                 uint32_t res[4]) {
  __m256i src_reg, ref0_reg, ref1_reg, ref2_reg, ref3_reg;
  __m256i sum_ref0, sum_ref1, sum_ref2, sum_ref3;
  const uint8_t *ref0, *ref1, *ref2, *ref3;
  assert(N % 2 == 0);

  ref0 = ref[0];
  ref1 = ref[1];
  ref2 = ref[2];
  ref3 = ref[3];

  sum_ref0 = _mm256_setzero_si256();
  sum_ref2 = _mm256_setzero_si256();
  sum_ref1 = _mm256_setzero_si256();
  sum_ref3 = _mm256_setzero_si256();

  for (int i = 0; i < N; i += 2) {
    // load src and all refs
    src_reg = yy_loadu2_128(src + src_stride, src);
    ref0_reg = yy_loadu2_128(ref0 + ref_stride, ref0);
    ref1_reg = yy_loadu2_128(ref1 + ref_stride, ref1);
    ref2_reg = yy_loadu2_128(ref2 + ref_stride, ref2);
    ref3_reg = yy_loadu2_128(ref3 + ref_stride, ref3);

    // sum of the absolute differences between every ref-i to src
    ref0_reg = _mm256_sad_epu8(ref0_reg, src_reg);
    ref1_reg = _mm256_sad_epu8(ref1_reg, src_reg);
    ref2_reg = _mm256_sad_epu8(ref2_reg, src_reg);
    ref3_reg = _mm256_sad_epu8(ref3_reg, src_reg);

    // sum every ref-i
    sum_ref0 = _mm256_add_epi32(sum_ref0, ref0_reg);
    sum_ref1 = _mm256_add_epi32(sum_ref1, ref1_reg);
    sum_ref2 = _mm256_add_epi32(sum_ref2, ref2_reg);
    sum_ref3 = _mm256_add_epi32(sum_ref3, ref3_reg);

    src += 2 * src_stride;
    ref0 += 2 * ref_stride;
    ref1 += 2 * ref_stride;
    ref2 += 2 * ref_stride;
    ref3 += 2 * ref_stride;
  }

  aggregate_and_store_sum(res, &sum_ref0, &sum_ref1, &sum_ref2, &sum_ref3);
}

#define SAD16XNX3_AVX2(n)                                                   \
  void aom_sad16x##n##x3d_avx2(const uint8_t *src, int src_stride,          \
                               const uint8_t *const ref[4], int ref_stride, \
                               uint32_t res[4]) {                           \
    aom_sad16xNx3d_avx2(n, src, src_stride, ref, ref_stride, res);          \
  }
#define SAD16XNX4_AVX2(n)                                                   \
  void aom_sad16x##n##x4d_avx2(const uint8_t *src, int src_stride,          \
                               const uint8_t *const ref[4], int ref_stride, \
                               uint32_t res[4]) {                           \
    aom_sad16xNx4d_avx2(n, src, src_stride, ref, ref_stride, res);          \
  }

SAD16XNX4_AVX2(32)
SAD16XNX4_AVX2(16)
SAD16XNX4_AVX2(8)

SAD16XNX3_AVX2(32)
SAD16XNX3_AVX2(16)
SAD16XNX3_AVX2(8)

#if !CONFIG_REALTIME_ONLY
SAD16XNX3_AVX2(64)
SAD16XNX3_AVX2(4)

SAD16XNX4_AVX2(64)
SAD16XNX4_AVX2(4)

#endif  // !CONFIG_REALTIME_ONLY

#define SAD_SKIP_16XN_AVX2(n)                                                 \
  void aom_sad_skip_16x##n##x4d_avx2(const uint8_t *src, int src_stride,      \
                                     const uint8_t *const ref[4],             \
                                     int ref_stride, uint32_t res[4]) {       \
    aom_sad16xNx4d_avx2(((n) >> 1), src, 2 * src_stride, ref, 2 * ref_stride, \
                        res);                                                 \
    res[0] <<= 1;                                                             \
    res[1] <<= 1;                                                             \
    res[2] <<= 1;                                                             \
    res[3] <<= 1;                                                             \
  }

SAD_SKIP_16XN_AVX2(32)
SAD_SKIP_16XN_AVX2(16)

#if !CONFIG_REALTIME_ONLY
SAD_SKIP_16XN_AVX2(64)
#endif  // !CONFIG_REALTIME_ONLY
