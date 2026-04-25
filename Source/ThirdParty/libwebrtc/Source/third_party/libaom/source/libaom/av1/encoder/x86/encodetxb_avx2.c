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
#include <emmintrin.h>  // SSE2
#include <smmintrin.h>  /* SSE4.1 */
#include <immintrin.h>  /* AVX2 */

#include "aom/aom_integer.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/txb_common.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/synonyms_avx2.h"

void av1_txb_init_levels_avx2(const tran_low_t *const coeff, const int width,
                              const int height, uint8_t *const levels) {
  const int stride = height + TX_PAD_HOR;
  const __m256i y_zeros = _mm256_setzero_si256();

  const int32_t bottom_len = sizeof(*levels) * (TX_PAD_BOTTOM * stride);
  uint8_t *bottom_buf_end = levels + (width + TX_PAD_BOTTOM) * stride;
  uint8_t *bottom_buf = bottom_buf_end - ((bottom_len + 31) & (~31));

  do {
    yy_storeu_256(bottom_buf, y_zeros);
    bottom_buf += 32;
  } while (bottom_buf < bottom_buf_end);

  int i = 0;
  uint8_t *ls = levels;
  const tran_low_t *cf = coeff;
  if (height == 4) {
    do {
      const __m256i c0 = yy_loadu_256(cf);
      const __m256i c1 = yy_loadu_256(cf + 8);
      const __m256i abs01 = _mm256_abs_epi16(_mm256_packs_epi32(c0, c1));
      const __m256i abs01_8 = _mm256_packs_epi16(abs01, y_zeros);
      const __m256i res_ = _mm256_shuffle_epi32(abs01_8, 0xd8);
      const __m256i res = _mm256_permute4x64_epi64(res_, 0xd8);
      yy_storeu_256(ls, res);
      ls += 32;
      cf += 16;
      i += 4;
    } while (i < width);
  } else if (height == 8) {
    do {
      const __m256i coeffA = yy_loadu_256(cf);
      const __m256i coeffB = yy_loadu_256(cf + 8);
      const __m256i coeffC = yy_loadu_256(cf + 16);
      const __m256i coeffD = yy_loadu_256(cf + 24);
      const __m256i coeffAB = _mm256_packs_epi32(coeffA, coeffB);
      const __m256i coeffCD = _mm256_packs_epi32(coeffC, coeffD);
      const __m256i absAB = _mm256_abs_epi16(coeffAB);
      const __m256i absCD = _mm256_abs_epi16(coeffCD);
      const __m256i absABCD = _mm256_packs_epi16(absAB, absCD);
      const __m256i res_ = _mm256_permute4x64_epi64(absABCD, 0xd8);
      const __m256i res = _mm256_shuffle_epi32(res_, 0xd8);
      const __m128i res0 = _mm256_castsi256_si128(res);
      const __m128i res1 = _mm256_extracti128_si256(res, 1);
      xx_storel_64(ls, res0);
      *(int32_t *)(ls + height) = 0;
      xx_storel_64(ls + stride, _mm_srli_si128(res0, 8));
      *(int32_t *)(ls + height + stride) = 0;
      xx_storel_64(ls + stride * 2, res1);
      *(int32_t *)(ls + height + stride * 2) = 0;
      xx_storel_64(ls + stride * 3, _mm_srli_si128(res1, 8));
      *(int32_t *)(ls + height + stride * 3) = 0;
      cf += 32;
      ls += stride << 2;
      i += 4;
    } while (i < width);
  } else if (height == 16) {
    do {
      const __m256i coeffA = yy_loadu_256(cf);
      const __m256i coeffB = yy_loadu_256(cf + 8);
      const __m256i coeffC = yy_loadu_256(cf + 16);
      const __m256i coeffD = yy_loadu_256(cf + 24);
      const __m256i coeffAB = _mm256_packs_epi32(coeffA, coeffB);
      const __m256i coeffCD = _mm256_packs_epi32(coeffC, coeffD);
      const __m256i absAB = _mm256_abs_epi16(coeffAB);
      const __m256i absCD = _mm256_abs_epi16(coeffCD);
      const __m256i absABCD = _mm256_packs_epi16(absAB, absCD);
      const __m256i res_ = _mm256_permute4x64_epi64(absABCD, 0xd8);
      const __m256i res = _mm256_shuffle_epi32(res_, 0xd8);
      xx_storeu_128(ls, _mm256_castsi256_si128(res));
      xx_storeu_128(ls + stride, _mm256_extracti128_si256(res, 1));
      cf += 32;
      *(int32_t *)(ls + height) = 0;
      *(int32_t *)(ls + stride + height) = 0;
      ls += stride << 1;
      i += 2;
    } while (i < width);
  } else {
    do {
      const __m256i coeffA = yy_loadu_256(cf);
      const __m256i coeffB = yy_loadu_256(cf + 8);
      const __m256i coeffC = yy_loadu_256(cf + 16);
      const __m256i coeffD = yy_loadu_256(cf + 24);
      const __m256i coeffAB = _mm256_packs_epi32(coeffA, coeffB);
      const __m256i coeffCD = _mm256_packs_epi32(coeffC, coeffD);
      const __m256i absAB = _mm256_abs_epi16(coeffAB);
      const __m256i absCD = _mm256_abs_epi16(coeffCD);
      const __m256i absABCD = _mm256_packs_epi16(absAB, absCD);
      const __m256i res_ = _mm256_permute4x64_epi64(absABCD, 0xd8);
      const __m256i res = _mm256_shuffle_epi32(res_, 0xd8);
      yy_storeu_256(ls, res);
      cf += 32;
      *(int32_t *)(ls + height) = 0;
      ls += stride;
      i += 1;
    } while (i < width);
  }
}
