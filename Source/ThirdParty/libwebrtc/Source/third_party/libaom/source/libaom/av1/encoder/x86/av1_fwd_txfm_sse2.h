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
#ifndef AOM_AV1_ENCODER_X86_AV1_FWD_TXFM_SSE2_H_
#define AOM_AV1_ENCODER_X86_AV1_FWD_TXFM_SSE2_H_

#include <immintrin.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/x86/transpose_sse2.h"
#include "aom_dsp/x86/txfm_common_sse2.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_fdct8x32_new_sse2(const __m128i *input, __m128i *output,
                           int8_t cos_bit);
void av1_fdct8x64_new_sse2(const __m128i *input, __m128i *output,
                           int8_t cos_bit);

static INLINE void fidentity4x4_new_sse2(const __m128i *const input,
                                         __m128i *const output,
                                         const int8_t cos_bit) {
  (void)cos_bit;
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 4; ++i) {
    const __m128i a = _mm_unpacklo_epi16(input[i], one);
    const __m128i b = scale_round_sse2(a, NewSqrt2);
    output[i] = _mm_packs_epi32(b, b);
  }
}

static INLINE void fidentity8x4_new_sse2(const __m128i *const input,
                                         __m128i *const output,
                                         const int8_t cos_bit) {
  (void)cos_bit;
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 4; ++i) {
    const __m128i a_lo = _mm_unpacklo_epi16(input[i], one);
    const __m128i a_hi = _mm_unpackhi_epi16(input[i], one);
    const __m128i b_lo = scale_round_sse2(a_lo, NewSqrt2);
    const __m128i b_hi = scale_round_sse2(a_hi, NewSqrt2);
    output[i] = _mm_packs_epi32(b_lo, b_hi);
  }
}

static INLINE void fidentity8x8_new_sse2(const __m128i *input, __m128i *output,
                                         int8_t cos_bit) {
  (void)cos_bit;

  output[0] = _mm_adds_epi16(input[0], input[0]);
  output[1] = _mm_adds_epi16(input[1], input[1]);
  output[2] = _mm_adds_epi16(input[2], input[2]);
  output[3] = _mm_adds_epi16(input[3], input[3]);
  output[4] = _mm_adds_epi16(input[4], input[4]);
  output[5] = _mm_adds_epi16(input[5], input[5]);
  output[6] = _mm_adds_epi16(input[6], input[6]);
  output[7] = _mm_adds_epi16(input[7], input[7]);
}

static INLINE void fidentity8x16_new_sse2(const __m128i *input, __m128i *output,
                                          int8_t cos_bit) {
  (void)cos_bit;
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 16; ++i) {
    const __m128i a_lo = _mm_unpacklo_epi16(input[i], one);
    const __m128i a_hi = _mm_unpackhi_epi16(input[i], one);
    const __m128i b_lo = scale_round_sse2(a_lo, 2 * NewSqrt2);
    const __m128i b_hi = scale_round_sse2(a_hi, 2 * NewSqrt2);
    output[i] = _mm_packs_epi32(b_lo, b_hi);
  }
}

static INLINE void fidentity8x32_new_sse2(const __m128i *input, __m128i *output,
                                          int8_t cos_bit) {
  (void)cos_bit;
  for (int i = 0; i < 32; ++i) {
    output[i] = _mm_slli_epi16(input[i], 2);
  }
}

static const transform_1d_sse2 col_txfm8x32_arr[TX_TYPES] = {
  av1_fdct8x32_new_sse2,   // DCT_DCT
  NULL,                    // ADST_DCT
  NULL,                    // DCT_ADST
  NULL,                    // ADST_ADST
  NULL,                    // FLIPADST_DCT
  NULL,                    // DCT_FLIPADST
  NULL,                    // FLIPADST_FLIPADST
  NULL,                    // ADST_FLIPADST
  NULL,                    // FLIPADST_ADST
  fidentity8x32_new_sse2,  // IDTX
  av1_fdct8x32_new_sse2,   // V_DCT
  fidentity8x32_new_sse2,  // H_DCT
  NULL,                    // V_ADST
  NULL,                    // H_ADST
  NULL,                    // V_FLIPADST
  NULL                     // H_FLIPADST
};

#ifdef __cplusplus
}
#endif

#endif  // AOM_AV1_ENCODER_X86_AV1_FWD_TXFM_SSE2_H_
