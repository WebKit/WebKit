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

#include <immintrin.h>  // AVX2

#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"

static INLINE void read_coeff(const tran_low_t *coeff, intptr_t offset,
                              __m256i *c) {
  const tran_low_t *addr = coeff + offset;

  if (sizeof(tran_low_t) == 4) {
    const __m256i x0 = _mm256_loadu_si256((const __m256i *)addr);
    const __m256i x1 = _mm256_loadu_si256((const __m256i *)addr + 1);
    const __m256i y = _mm256_packs_epi32(x0, x1);
    *c = _mm256_permute4x64_epi64(y, 0xD8);
  } else {
    *c = _mm256_loadu_si256((const __m256i *)addr);
  }
}

int64_t av1_block_error_lp_avx2(const int16_t *coeff, const int16_t *dqcoeff,
                                intptr_t block_size) {
  const __m256i zero = _mm256_setzero_si256();
  __m256i sse_256 = zero;
  __m256i sse_hi;
  __m128i sse_128;
  int64_t sse;

  if (block_size == 16) {
    // Load 16 elements for coeff and dqcoeff.
    const __m256i _coeff = _mm256_loadu_si256((const __m256i *)coeff);
    const __m256i _dqcoeff = _mm256_loadu_si256((const __m256i *)dqcoeff);
    // dqcoeff - coeff
    const __m256i diff = _mm256_sub_epi16(_dqcoeff, _coeff);
    // madd (dqcoeff - coeff)
    const __m256i error_lo = _mm256_madd_epi16(diff, diff);
    // Save the higher 64 bit of each 128 bit lane.
    const __m256i error_hi = _mm256_srli_si256(error_lo, 8);
    // Add the higher 64 bit to the low 64 bit.
    const __m256i error = _mm256_add_epi32(error_lo, error_hi);
    // Expand each double word in the lower 64 bits to quad word.
    sse_256 = _mm256_unpacklo_epi32(error, zero);
  } else {
    for (int i = 0; i < block_size; i += 16) {
      // Load 16 elements for coeff and dqcoeff.
      const __m256i _coeff = _mm256_loadu_si256((const __m256i *)coeff);
      const __m256i _dqcoeff = _mm256_loadu_si256((const __m256i *)dqcoeff);
      const __m256i diff = _mm256_sub_epi16(_dqcoeff, _coeff);
      const __m256i error = _mm256_madd_epi16(diff, diff);
      // Expand each double word of madd (dqcoeff - coeff) to quad word.
      const __m256i exp_error_lo = _mm256_unpacklo_epi32(error, zero);
      const __m256i exp_error_hi = _mm256_unpackhi_epi32(error, zero);
      // Add each quad word of madd (dqcoeff - coeff).
      sse_256 = _mm256_add_epi64(sse_256, exp_error_lo);
      sse_256 = _mm256_add_epi64(sse_256, exp_error_hi);
      coeff += 16;
      dqcoeff += 16;
    }
  }
  // Save the higher 64 bit of each 128 bit lane.
  sse_hi = _mm256_srli_si256(sse_256, 8);
  // Add the higher 64 bit to the low 64 bit.
  sse_256 = _mm256_add_epi64(sse_256, sse_hi);

  // Add each 64 bit from each of the 128 bit lane of the 256 bit.
  sse_128 = _mm_add_epi64(_mm256_castsi256_si128(sse_256),
                          _mm256_extractf128_si256(sse_256, 1));

  // Store the results.
  _mm_storel_epi64((__m128i *)&sse, sse_128);
  return sse;
}

int64_t av1_block_error_avx2(const tran_low_t *coeff, const tran_low_t *dqcoeff,
                             intptr_t block_size, int64_t *ssz) {
  __m256i sse_reg, ssz_reg, coeff_reg, dqcoeff_reg;
  __m256i exp_dqcoeff_lo, exp_dqcoeff_hi, exp_coeff_lo, exp_coeff_hi;
  __m256i sse_reg_64hi, ssz_reg_64hi;
  __m128i sse_reg128, ssz_reg128;
  int64_t sse;
  int i;
  const __m256i zero_reg = _mm256_setzero_si256();

  // init sse and ssz registerd to zero
  sse_reg = _mm256_setzero_si256();
  ssz_reg = _mm256_setzero_si256();

  for (i = 0; i < block_size; i += 16) {
    // load 32 bytes from coeff and dqcoeff
    read_coeff(coeff, i, &coeff_reg);
    read_coeff(dqcoeff, i, &dqcoeff_reg);
    // dqcoeff - coeff
    dqcoeff_reg = _mm256_sub_epi16(dqcoeff_reg, coeff_reg);
    // madd (dqcoeff - coeff)
    dqcoeff_reg = _mm256_madd_epi16(dqcoeff_reg, dqcoeff_reg);
    // madd coeff
    coeff_reg = _mm256_madd_epi16(coeff_reg, coeff_reg);
    // expand each double word of madd (dqcoeff - coeff) to quad word
    exp_dqcoeff_lo = _mm256_unpacklo_epi32(dqcoeff_reg, zero_reg);
    exp_dqcoeff_hi = _mm256_unpackhi_epi32(dqcoeff_reg, zero_reg);
    // expand each double word of madd (coeff) to quad word
    exp_coeff_lo = _mm256_unpacklo_epi32(coeff_reg, zero_reg);
    exp_coeff_hi = _mm256_unpackhi_epi32(coeff_reg, zero_reg);
    // add each quad word of madd (dqcoeff - coeff) and madd (coeff)
    sse_reg = _mm256_add_epi64(sse_reg, exp_dqcoeff_lo);
    ssz_reg = _mm256_add_epi64(ssz_reg, exp_coeff_lo);
    sse_reg = _mm256_add_epi64(sse_reg, exp_dqcoeff_hi);
    ssz_reg = _mm256_add_epi64(ssz_reg, exp_coeff_hi);
  }
  // save the higher 64 bit of each 128 bit lane
  sse_reg_64hi = _mm256_srli_si256(sse_reg, 8);
  ssz_reg_64hi = _mm256_srli_si256(ssz_reg, 8);
  // add the higher 64 bit to the low 64 bit
  sse_reg = _mm256_add_epi64(sse_reg, sse_reg_64hi);
  ssz_reg = _mm256_add_epi64(ssz_reg, ssz_reg_64hi);

  // add each 64 bit from each of the 128 bit lane of the 256 bit
  sse_reg128 = _mm_add_epi64(_mm256_castsi256_si128(sse_reg),
                             _mm256_extractf128_si256(sse_reg, 1));

  ssz_reg128 = _mm_add_epi64(_mm256_castsi256_si128(ssz_reg),
                             _mm256_extractf128_si256(ssz_reg, 1));

  // store the results
  _mm_storel_epi64((__m128i *)(&sse), sse_reg128);

  _mm_storel_epi64((__m128i *)(ssz), ssz_reg128);
  _mm256_zeroupper();
  return sse;
}
