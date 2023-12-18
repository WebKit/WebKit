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
#include <immintrin.h>

#include "aom_dsp/x86/synonyms.h"

#include "aom/aom_integer.h"

#include "av1/common/reconinter.h"

#define MAX_MASK_VALUE (1 << WEDGE_WEIGHT_BITS)

/**
 * See av1_wedge_sse_from_residuals_c
 */
uint64_t av1_wedge_sse_from_residuals_sse2(const int16_t *r1, const int16_t *d,
                                           const uint8_t *m, int N) {
  int n = -N;
  int n8 = n + 8;

  uint64_t csse;

  const __m128i v_mask_max_w = _mm_set1_epi16(MAX_MASK_VALUE);
  const __m128i v_zext_q = xx_set1_64_from_32i(~0);

  __m128i v_acc0_q = _mm_setzero_si128();

  assert(N % 64 == 0);

  r1 += N;
  d += N;
  m += N;

  do {
    const __m128i v_r0_w = xx_load_128(r1 + n);
    const __m128i v_r1_w = xx_load_128(r1 + n8);
    const __m128i v_d0_w = xx_load_128(d + n);
    const __m128i v_d1_w = xx_load_128(d + n8);
    const __m128i v_m01_b = xx_load_128(m + n);

    const __m128i v_rd0l_w = _mm_unpacklo_epi16(v_d0_w, v_r0_w);
    const __m128i v_rd0h_w = _mm_unpackhi_epi16(v_d0_w, v_r0_w);
    const __m128i v_rd1l_w = _mm_unpacklo_epi16(v_d1_w, v_r1_w);
    const __m128i v_rd1h_w = _mm_unpackhi_epi16(v_d1_w, v_r1_w);
    const __m128i v_m0_w = _mm_unpacklo_epi8(v_m01_b, _mm_setzero_si128());
    const __m128i v_m1_w = _mm_unpackhi_epi8(v_m01_b, _mm_setzero_si128());

    const __m128i v_m0l_w = _mm_unpacklo_epi16(v_m0_w, v_mask_max_w);
    const __m128i v_m0h_w = _mm_unpackhi_epi16(v_m0_w, v_mask_max_w);
    const __m128i v_m1l_w = _mm_unpacklo_epi16(v_m1_w, v_mask_max_w);
    const __m128i v_m1h_w = _mm_unpackhi_epi16(v_m1_w, v_mask_max_w);

    const __m128i v_t0l_d = _mm_madd_epi16(v_rd0l_w, v_m0l_w);
    const __m128i v_t0h_d = _mm_madd_epi16(v_rd0h_w, v_m0h_w);
    const __m128i v_t1l_d = _mm_madd_epi16(v_rd1l_w, v_m1l_w);
    const __m128i v_t1h_d = _mm_madd_epi16(v_rd1h_w, v_m1h_w);

    const __m128i v_t0_w = _mm_packs_epi32(v_t0l_d, v_t0h_d);
    const __m128i v_t1_w = _mm_packs_epi32(v_t1l_d, v_t1h_d);

    const __m128i v_sq0_d = _mm_madd_epi16(v_t0_w, v_t0_w);
    const __m128i v_sq1_d = _mm_madd_epi16(v_t1_w, v_t1_w);

    const __m128i v_sum0_q = _mm_add_epi64(_mm_and_si128(v_sq0_d, v_zext_q),
                                           _mm_srli_epi64(v_sq0_d, 32));
    const __m128i v_sum1_q = _mm_add_epi64(_mm_and_si128(v_sq1_d, v_zext_q),
                                           _mm_srli_epi64(v_sq1_d, 32));

    v_acc0_q = _mm_add_epi64(v_acc0_q, v_sum0_q);
    v_acc0_q = _mm_add_epi64(v_acc0_q, v_sum1_q);

    n8 += 16;
    n += 16;
  } while (n);

  v_acc0_q = _mm_add_epi64(v_acc0_q, _mm_srli_si128(v_acc0_q, 8));

#if AOM_ARCH_X86_64
  csse = (uint64_t)_mm_cvtsi128_si64(v_acc0_q);
#else
  xx_storel_64(&csse, v_acc0_q);
#endif

  return ROUND_POWER_OF_TWO(csse, 2 * WEDGE_WEIGHT_BITS);
}

/**
 * See av1_wedge_sign_from_residuals_c
 */
int8_t av1_wedge_sign_from_residuals_sse2(const int16_t *ds, const uint8_t *m,
                                          int N, int64_t limit) {
  int64_t acc;

  __m128i v_sign_d;
  __m128i v_acc0_d = _mm_setzero_si128();
  __m128i v_acc1_d = _mm_setzero_si128();
  __m128i v_acc_q;

  // Input size limited to 8192 by the use of 32 bit accumulators and m
  // being between [0, 64]. Overflow might happen at larger sizes,
  // though it is practically impossible on real video input.
  assert(N < 8192);
  assert(N % 64 == 0);

  do {
    const __m128i v_m01_b = xx_load_128(m);
    const __m128i v_m23_b = xx_load_128(m + 16);
    const __m128i v_m45_b = xx_load_128(m + 32);
    const __m128i v_m67_b = xx_load_128(m + 48);

    const __m128i v_d0_w = xx_load_128(ds);
    const __m128i v_d1_w = xx_load_128(ds + 8);
    const __m128i v_d2_w = xx_load_128(ds + 16);
    const __m128i v_d3_w = xx_load_128(ds + 24);
    const __m128i v_d4_w = xx_load_128(ds + 32);
    const __m128i v_d5_w = xx_load_128(ds + 40);
    const __m128i v_d6_w = xx_load_128(ds + 48);
    const __m128i v_d7_w = xx_load_128(ds + 56);

    const __m128i v_m0_w = _mm_unpacklo_epi8(v_m01_b, _mm_setzero_si128());
    const __m128i v_m1_w = _mm_unpackhi_epi8(v_m01_b, _mm_setzero_si128());
    const __m128i v_m2_w = _mm_unpacklo_epi8(v_m23_b, _mm_setzero_si128());
    const __m128i v_m3_w = _mm_unpackhi_epi8(v_m23_b, _mm_setzero_si128());
    const __m128i v_m4_w = _mm_unpacklo_epi8(v_m45_b, _mm_setzero_si128());
    const __m128i v_m5_w = _mm_unpackhi_epi8(v_m45_b, _mm_setzero_si128());
    const __m128i v_m6_w = _mm_unpacklo_epi8(v_m67_b, _mm_setzero_si128());
    const __m128i v_m7_w = _mm_unpackhi_epi8(v_m67_b, _mm_setzero_si128());

    const __m128i v_p0_d = _mm_madd_epi16(v_d0_w, v_m0_w);
    const __m128i v_p1_d = _mm_madd_epi16(v_d1_w, v_m1_w);
    const __m128i v_p2_d = _mm_madd_epi16(v_d2_w, v_m2_w);
    const __m128i v_p3_d = _mm_madd_epi16(v_d3_w, v_m3_w);
    const __m128i v_p4_d = _mm_madd_epi16(v_d4_w, v_m4_w);
    const __m128i v_p5_d = _mm_madd_epi16(v_d5_w, v_m5_w);
    const __m128i v_p6_d = _mm_madd_epi16(v_d6_w, v_m6_w);
    const __m128i v_p7_d = _mm_madd_epi16(v_d7_w, v_m7_w);

    const __m128i v_p01_d = _mm_add_epi32(v_p0_d, v_p1_d);
    const __m128i v_p23_d = _mm_add_epi32(v_p2_d, v_p3_d);
    const __m128i v_p45_d = _mm_add_epi32(v_p4_d, v_p5_d);
    const __m128i v_p67_d = _mm_add_epi32(v_p6_d, v_p7_d);

    const __m128i v_p0123_d = _mm_add_epi32(v_p01_d, v_p23_d);
    const __m128i v_p4567_d = _mm_add_epi32(v_p45_d, v_p67_d);

    v_acc0_d = _mm_add_epi32(v_acc0_d, v_p0123_d);
    v_acc1_d = _mm_add_epi32(v_acc1_d, v_p4567_d);

    ds += 64;
    m += 64;

    N -= 64;
  } while (N);

  v_sign_d = _mm_cmplt_epi32(v_acc0_d, _mm_setzero_si128());
  v_acc0_d = _mm_add_epi64(_mm_unpacklo_epi32(v_acc0_d, v_sign_d),
                           _mm_unpackhi_epi32(v_acc0_d, v_sign_d));

  v_sign_d = _mm_cmplt_epi32(v_acc1_d, _mm_setzero_si128());
  v_acc1_d = _mm_add_epi64(_mm_unpacklo_epi32(v_acc1_d, v_sign_d),
                           _mm_unpackhi_epi32(v_acc1_d, v_sign_d));

  v_acc_q = _mm_add_epi64(v_acc0_d, v_acc1_d);

  v_acc_q = _mm_add_epi64(v_acc_q, _mm_srli_si128(v_acc_q, 8));

#if AOM_ARCH_X86_64
  acc = _mm_cvtsi128_si64(v_acc_q);
#else
  xx_storel_64(&acc, v_acc_q);
#endif

  return acc > limit;
}

// Negate under mask
static INLINE __m128i negm_epi16(__m128i v_v_w, __m128i v_mask_w) {
  return _mm_sub_epi16(_mm_xor_si128(v_v_w, v_mask_w), v_mask_w);
}

/**
 * av1_wedge_compute_delta_squares_c
 */
void av1_wedge_compute_delta_squares_sse2(int16_t *d, const int16_t *a,
                                          const int16_t *b, int N) {
  const __m128i v_neg_w = _mm_set_epi16((short)0xffff, 0, (short)0xffff, 0,
                                        (short)0xffff, 0, (short)0xffff, 0);

  assert(N % 64 == 0);

  do {
    const __m128i v_a0_w = xx_load_128(a);
    const __m128i v_b0_w = xx_load_128(b);
    const __m128i v_a1_w = xx_load_128(a + 8);
    const __m128i v_b1_w = xx_load_128(b + 8);
    const __m128i v_a2_w = xx_load_128(a + 16);
    const __m128i v_b2_w = xx_load_128(b + 16);
    const __m128i v_a3_w = xx_load_128(a + 24);
    const __m128i v_b3_w = xx_load_128(b + 24);

    const __m128i v_ab0l_w = _mm_unpacklo_epi16(v_a0_w, v_b0_w);
    const __m128i v_ab0h_w = _mm_unpackhi_epi16(v_a0_w, v_b0_w);
    const __m128i v_ab1l_w = _mm_unpacklo_epi16(v_a1_w, v_b1_w);
    const __m128i v_ab1h_w = _mm_unpackhi_epi16(v_a1_w, v_b1_w);
    const __m128i v_ab2l_w = _mm_unpacklo_epi16(v_a2_w, v_b2_w);
    const __m128i v_ab2h_w = _mm_unpackhi_epi16(v_a2_w, v_b2_w);
    const __m128i v_ab3l_w = _mm_unpacklo_epi16(v_a3_w, v_b3_w);
    const __m128i v_ab3h_w = _mm_unpackhi_epi16(v_a3_w, v_b3_w);

    // Negate top word of pairs
    const __m128i v_abl0n_w = negm_epi16(v_ab0l_w, v_neg_w);
    const __m128i v_abh0n_w = negm_epi16(v_ab0h_w, v_neg_w);
    const __m128i v_abl1n_w = negm_epi16(v_ab1l_w, v_neg_w);
    const __m128i v_abh1n_w = negm_epi16(v_ab1h_w, v_neg_w);
    const __m128i v_abl2n_w = negm_epi16(v_ab2l_w, v_neg_w);
    const __m128i v_abh2n_w = negm_epi16(v_ab2h_w, v_neg_w);
    const __m128i v_abl3n_w = negm_epi16(v_ab3l_w, v_neg_w);
    const __m128i v_abh3n_w = negm_epi16(v_ab3h_w, v_neg_w);

    const __m128i v_r0l_w = _mm_madd_epi16(v_ab0l_w, v_abl0n_w);
    const __m128i v_r0h_w = _mm_madd_epi16(v_ab0h_w, v_abh0n_w);
    const __m128i v_r1l_w = _mm_madd_epi16(v_ab1l_w, v_abl1n_w);
    const __m128i v_r1h_w = _mm_madd_epi16(v_ab1h_w, v_abh1n_w);
    const __m128i v_r2l_w = _mm_madd_epi16(v_ab2l_w, v_abl2n_w);
    const __m128i v_r2h_w = _mm_madd_epi16(v_ab2h_w, v_abh2n_w);
    const __m128i v_r3l_w = _mm_madd_epi16(v_ab3l_w, v_abl3n_w);
    const __m128i v_r3h_w = _mm_madd_epi16(v_ab3h_w, v_abh3n_w);

    const __m128i v_r0_w = _mm_packs_epi32(v_r0l_w, v_r0h_w);
    const __m128i v_r1_w = _mm_packs_epi32(v_r1l_w, v_r1h_w);
    const __m128i v_r2_w = _mm_packs_epi32(v_r2l_w, v_r2h_w);
    const __m128i v_r3_w = _mm_packs_epi32(v_r3l_w, v_r3h_w);

    xx_store_128(d, v_r0_w);
    xx_store_128(d + 8, v_r1_w);
    xx_store_128(d + 16, v_r2_w);
    xx_store_128(d + 24, v_r3_w);

    a += 32;
    b += 32;
    d += 32;
    N -= 32;
  } while (N);
}
