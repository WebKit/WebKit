/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
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

#include "aom/aom_integer.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/txb_common.h"
#include "aom_dsp/x86/synonyms.h"

void av1_txb_init_levels_sse4_1(const tran_low_t *const coeff, const int width,
                                const int height, uint8_t *const levels) {
  const int stride = width + TX_PAD_HOR;
  const __m128i zeros = _mm_setzero_si128();

  const int32_t bottom_len = sizeof(*levels) * (TX_PAD_BOTTOM * stride);
  uint8_t *bottom_buf = levels + stride * height;
  uint8_t *bottom_buf_end = bottom_buf + bottom_len;
  do {
    _mm_storeu_si128((__m128i *)(bottom_buf), zeros);
    bottom_buf += 16;
  } while (bottom_buf < bottom_buf_end);

  int i = 0;
  uint8_t *ls = levels;
  const tran_low_t *cf = coeff;
  if (width == 4) {
    do {
      const __m128i coeffA = xx_loadu_128(cf);
      const __m128i coeffB = xx_loadu_128(cf + 4);
      const __m128i coeffAB = _mm_packs_epi32(coeffA, coeffB);
      const __m128i absAB = _mm_abs_epi16(coeffAB);
      const __m128i absAB8 = _mm_packs_epi16(absAB, zeros);
      const __m128i lsAB = _mm_unpacklo_epi32(absAB8, zeros);
      xx_storeu_128(ls, lsAB);
      ls += (stride << 1);
      cf += (width << 1);
      i += 2;
    } while (i < height);
  } else if (width == 8) {
    do {
      const __m128i coeffA = xx_loadu_128(cf);
      const __m128i coeffB = xx_loadu_128(cf + 4);
      const __m128i coeffAB = _mm_packs_epi32(coeffA, coeffB);
      const __m128i absAB = _mm_abs_epi16(coeffAB);
      const __m128i absAB8 = _mm_packs_epi16(absAB, zeros);
      xx_storeu_128(ls, absAB8);
      ls += stride;
      cf += width;
      i += 1;
    } while (i < height);
  } else {
    do {
      int j = 0;
      do {
        const __m128i coeffA = xx_loadu_128(cf);
        const __m128i coeffB = xx_loadu_128(cf + 4);
        const __m128i coeffC = xx_loadu_128(cf + 8);
        const __m128i coeffD = xx_loadu_128(cf + 12);
        const __m128i coeffAB = _mm_packs_epi32(coeffA, coeffB);
        const __m128i coeffCD = _mm_packs_epi32(coeffC, coeffD);
        const __m128i absAB = _mm_abs_epi16(coeffAB);
        const __m128i absCD = _mm_abs_epi16(coeffCD);
        const __m128i absABCD = _mm_packs_epi16(absAB, absCD);
        xx_storeu_128(ls + j, absABCD);
        j += 16;
        cf += 16;
      } while (j < width);
      *(int32_t *)(ls + width) = 0;
      ls += stride;
      i += 1;
    } while (i < height);
  }
}
