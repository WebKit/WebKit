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
#include <immintrin.h>
#include <smmintrin.h>

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"
#include "aom/aom_integer.h"

#include "av1/common/reconinter.h"

#define MAX_MASK_VALUE (1 << WEDGE_WEIGHT_BITS)

/**
 * See av1_wedge_sse_from_residuals_c
 */
uint64_t av1_wedge_sse_from_residuals_avx2(const int16_t *r1, const int16_t *d,
                                           const uint8_t *m, int N) {
  int n = -N;

  uint64_t csse;

  const __m256i v_mask_max_w = _mm256_set1_epi16(MAX_MASK_VALUE);
  const __m256i v_zext_q = _mm256_set1_epi64x(~0u);

  __m256i v_acc0_q = _mm256_setzero_si256();

  assert(N % 64 == 0);

  r1 += N;
  d += N;
  m += N;

  do {
    const __m256i v_r0_w = _mm256_lddqu_si256((__m256i *)(r1 + n));
    const __m256i v_d0_w = _mm256_lddqu_si256((__m256i *)(d + n));
    const __m128i v_m01_b = _mm_lddqu_si128((__m128i *)(m + n));

    const __m256i v_rd0l_w = _mm256_unpacklo_epi16(v_d0_w, v_r0_w);
    const __m256i v_rd0h_w = _mm256_unpackhi_epi16(v_d0_w, v_r0_w);
    const __m256i v_m0_w = _mm256_cvtepu8_epi16(v_m01_b);

    const __m256i v_m0l_w = _mm256_unpacklo_epi16(v_m0_w, v_mask_max_w);
    const __m256i v_m0h_w = _mm256_unpackhi_epi16(v_m0_w, v_mask_max_w);

    const __m256i v_t0l_d = _mm256_madd_epi16(v_rd0l_w, v_m0l_w);
    const __m256i v_t0h_d = _mm256_madd_epi16(v_rd0h_w, v_m0h_w);

    const __m256i v_t0_w = _mm256_packs_epi32(v_t0l_d, v_t0h_d);

    const __m256i v_sq0_d = _mm256_madd_epi16(v_t0_w, v_t0_w);

    const __m256i v_sum0_q = _mm256_add_epi64(
        _mm256_and_si256(v_sq0_d, v_zext_q), _mm256_srli_epi64(v_sq0_d, 32));

    v_acc0_q = _mm256_add_epi64(v_acc0_q, v_sum0_q);

    n += 16;
  } while (n);

  v_acc0_q = _mm256_add_epi64(v_acc0_q, _mm256_srli_si256(v_acc0_q, 8));
  __m128i v_acc_q_0 = _mm256_castsi256_si128(v_acc0_q);
  __m128i v_acc_q_1 = _mm256_extracti128_si256(v_acc0_q, 1);
  v_acc_q_0 = _mm_add_epi64(v_acc_q_0, v_acc_q_1);
#if AOM_ARCH_X86_64
  csse = (uint64_t)_mm_extract_epi64(v_acc_q_0, 0);
#else
  xx_storel_64(&csse, v_acc_q_0);
#endif

  return ROUND_POWER_OF_TWO(csse, 2 * WEDGE_WEIGHT_BITS);
}

/**
 * See av1_wedge_sign_from_residuals_c
 */
int8_t av1_wedge_sign_from_residuals_avx2(const int16_t *ds, const uint8_t *m,
                                          int N, int64_t limit) {
  int64_t acc;
  __m256i v_acc0_d = _mm256_setzero_si256();

  // Input size limited to 8192 by the use of 32 bit accumulators and m
  // being between [0, 64]. Overflow might happen at larger sizes,
  // though it is practically impossible on real video input.
  assert(N < 8192);
  assert(N % 64 == 0);

  do {
    const __m256i v_m01_b = _mm256_lddqu_si256((__m256i *)(m));
    const __m256i v_m23_b = _mm256_lddqu_si256((__m256i *)(m + 32));

    const __m256i v_d0_w = _mm256_lddqu_si256((__m256i *)(ds));
    const __m256i v_d1_w = _mm256_lddqu_si256((__m256i *)(ds + 16));
    const __m256i v_d2_w = _mm256_lddqu_si256((__m256i *)(ds + 32));
    const __m256i v_d3_w = _mm256_lddqu_si256((__m256i *)(ds + 48));

    const __m256i v_m0_w =
        _mm256_cvtepu8_epi16(_mm256_castsi256_si128(v_m01_b));
    const __m256i v_m1_w =
        _mm256_cvtepu8_epi16(_mm256_extracti128_si256(v_m01_b, 1));
    const __m256i v_m2_w =
        _mm256_cvtepu8_epi16(_mm256_castsi256_si128(v_m23_b));
    const __m256i v_m3_w =
        _mm256_cvtepu8_epi16(_mm256_extracti128_si256(v_m23_b, 1));

    const __m256i v_p0_d = _mm256_madd_epi16(v_d0_w, v_m0_w);
    const __m256i v_p1_d = _mm256_madd_epi16(v_d1_w, v_m1_w);
    const __m256i v_p2_d = _mm256_madd_epi16(v_d2_w, v_m2_w);
    const __m256i v_p3_d = _mm256_madd_epi16(v_d3_w, v_m3_w);

    const __m256i v_p01_d = _mm256_add_epi32(v_p0_d, v_p1_d);
    const __m256i v_p23_d = _mm256_add_epi32(v_p2_d, v_p3_d);

    const __m256i v_p0123_d = _mm256_add_epi32(v_p01_d, v_p23_d);

    v_acc0_d = _mm256_add_epi32(v_acc0_d, v_p0123_d);

    ds += 64;
    m += 64;

    N -= 64;
  } while (N);

  __m256i v_sign_d = _mm256_srai_epi32(v_acc0_d, 31);
  v_acc0_d = _mm256_add_epi64(_mm256_unpacklo_epi32(v_acc0_d, v_sign_d),
                              _mm256_unpackhi_epi32(v_acc0_d, v_sign_d));

  __m256i v_acc_q = _mm256_add_epi64(v_acc0_d, _mm256_srli_si256(v_acc0_d, 8));

  __m128i v_acc_q_0 = _mm256_castsi256_si128(v_acc_q);
  __m128i v_acc_q_1 = _mm256_extracti128_si256(v_acc_q, 1);
  v_acc_q_0 = _mm_add_epi64(v_acc_q_0, v_acc_q_1);

#if AOM_ARCH_X86_64
  acc = _mm_extract_epi64(v_acc_q_0, 0);
#else
  xx_storel_64(&acc, v_acc_q_0);
#endif

  return acc > limit;
}

/**
 * av1_wedge_compute_delta_squares_c
 */
void av1_wedge_compute_delta_squares_avx2(int16_t *d, const int16_t *a,
                                          const int16_t *b, int N) {
  const __m256i v_neg_w = _mm256_set1_epi32((int)0xffff0001);

  assert(N % 64 == 0);

  do {
    const __m256i v_a0_w = _mm256_lddqu_si256((__m256i *)(a));
    const __m256i v_b0_w = _mm256_lddqu_si256((__m256i *)(b));
    const __m256i v_a1_w = _mm256_lddqu_si256((__m256i *)(a + 16));
    const __m256i v_b1_w = _mm256_lddqu_si256((__m256i *)(b + 16));
    const __m256i v_a2_w = _mm256_lddqu_si256((__m256i *)(a + 32));
    const __m256i v_b2_w = _mm256_lddqu_si256((__m256i *)(b + 32));
    const __m256i v_a3_w = _mm256_lddqu_si256((__m256i *)(a + 48));
    const __m256i v_b3_w = _mm256_lddqu_si256((__m256i *)(b + 48));

    const __m256i v_ab0l_w = _mm256_unpacklo_epi16(v_a0_w, v_b0_w);
    const __m256i v_ab0h_w = _mm256_unpackhi_epi16(v_a0_w, v_b0_w);
    const __m256i v_ab1l_w = _mm256_unpacklo_epi16(v_a1_w, v_b1_w);
    const __m256i v_ab1h_w = _mm256_unpackhi_epi16(v_a1_w, v_b1_w);
    const __m256i v_ab2l_w = _mm256_unpacklo_epi16(v_a2_w, v_b2_w);
    const __m256i v_ab2h_w = _mm256_unpackhi_epi16(v_a2_w, v_b2_w);
    const __m256i v_ab3l_w = _mm256_unpacklo_epi16(v_a3_w, v_b3_w);
    const __m256i v_ab3h_w = _mm256_unpackhi_epi16(v_a3_w, v_b3_w);

    // Negate top word of pairs
    const __m256i v_abl0n_w = _mm256_sign_epi16(v_ab0l_w, v_neg_w);
    const __m256i v_abh0n_w = _mm256_sign_epi16(v_ab0h_w, v_neg_w);
    const __m256i v_abl1n_w = _mm256_sign_epi16(v_ab1l_w, v_neg_w);
    const __m256i v_abh1n_w = _mm256_sign_epi16(v_ab1h_w, v_neg_w);
    const __m256i v_abl2n_w = _mm256_sign_epi16(v_ab2l_w, v_neg_w);
    const __m256i v_abh2n_w = _mm256_sign_epi16(v_ab2h_w, v_neg_w);
    const __m256i v_abl3n_w = _mm256_sign_epi16(v_ab3l_w, v_neg_w);
    const __m256i v_abh3n_w = _mm256_sign_epi16(v_ab3h_w, v_neg_w);

    const __m256i v_r0l_w = _mm256_madd_epi16(v_ab0l_w, v_abl0n_w);
    const __m256i v_r0h_w = _mm256_madd_epi16(v_ab0h_w, v_abh0n_w);
    const __m256i v_r1l_w = _mm256_madd_epi16(v_ab1l_w, v_abl1n_w);
    const __m256i v_r1h_w = _mm256_madd_epi16(v_ab1h_w, v_abh1n_w);
    const __m256i v_r2l_w = _mm256_madd_epi16(v_ab2l_w, v_abl2n_w);
    const __m256i v_r2h_w = _mm256_madd_epi16(v_ab2h_w, v_abh2n_w);
    const __m256i v_r3l_w = _mm256_madd_epi16(v_ab3l_w, v_abl3n_w);
    const __m256i v_r3h_w = _mm256_madd_epi16(v_ab3h_w, v_abh3n_w);

    const __m256i v_r0_w = _mm256_packs_epi32(v_r0l_w, v_r0h_w);
    const __m256i v_r1_w = _mm256_packs_epi32(v_r1l_w, v_r1h_w);
    const __m256i v_r2_w = _mm256_packs_epi32(v_r2l_w, v_r2h_w);
    const __m256i v_r3_w = _mm256_packs_epi32(v_r3l_w, v_r3h_w);

    _mm256_store_si256((__m256i *)(d), v_r0_w);
    _mm256_store_si256((__m256i *)(d + 16), v_r1_w);
    _mm256_store_si256((__m256i *)(d + 32), v_r2_w);
    _mm256_store_si256((__m256i *)(d + 48), v_r3_w);

    a += 64;
    b += 64;
    d += 64;
    N -= 64;
  } while (N);
}
