/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <emmintrin.h>

#include "av1/common/cfl.h"
#include "config/av1_rtcd.h"

static INLINE __m128i fill_sum_epi32(__m128i l0) {
  l0 = _mm_add_epi32(l0, _mm_shuffle_epi32(l0, _MM_SHUFFLE(1, 0, 3, 2)));
  return _mm_add_epi32(l0, _mm_shuffle_epi32(l0, _MM_SHUFFLE(2, 3, 0, 1)));
}

static INLINE void subtract_average_sse2(const uint16_t *src_ptr,
                                         int16_t *dst_ptr, int width,
                                         int height, int round_offset,
                                         int num_pel_log2) {
  const __m128i zeros = _mm_setzero_si128();
  const __m128i round_offset_epi32 = _mm_set1_epi32(round_offset);
  const __m128i *src = (__m128i *)src_ptr;
  const __m128i *const end = src + height * CFL_BUF_LINE_I128;
  const int step = CFL_BUF_LINE_I128 * (1 + (width == 8) + 3 * (width == 4));

  __m128i sum = zeros;
  do {
    __m128i l0;
    if (width == 4) {
      l0 = _mm_add_epi16(_mm_loadl_epi64(src),
                         _mm_loadl_epi64(src + CFL_BUF_LINE_I128));
      __m128i l1 = _mm_add_epi16(_mm_loadl_epi64(src + 2 * CFL_BUF_LINE_I128),
                                 _mm_loadl_epi64(src + 3 * CFL_BUF_LINE_I128));
      sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                                             _mm_unpacklo_epi16(l1, zeros)));
    } else {
      if (width == 8) {
        l0 = _mm_add_epi16(_mm_loadu_si128(src),
                           _mm_loadu_si128(src + CFL_BUF_LINE_I128));
      } else {
        l0 = _mm_add_epi16(_mm_loadu_si128(src), _mm_loadu_si128(src + 1));
      }
      sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                                             _mm_unpackhi_epi16(l0, zeros)));
      if (width == 32) {
        l0 = _mm_add_epi16(_mm_loadu_si128(src + 2), _mm_loadu_si128(src + 3));
        sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(l0, zeros),
                                               _mm_unpackhi_epi16(l0, zeros)));
      }
    }
    src += step;
  } while (src < end);

  sum = fill_sum_epi32(sum);

  __m128i avg_epi16 =
      _mm_srli_epi32(_mm_add_epi32(sum, round_offset_epi32), num_pel_log2);
  avg_epi16 = _mm_packs_epi32(avg_epi16, avg_epi16);

  src = (__m128i *)src_ptr;
  __m128i *dst = (__m128i *)dst_ptr;
  do {
    if (width == 4) {
      _mm_storel_epi64(dst, _mm_sub_epi16(_mm_loadl_epi64(src), avg_epi16));
    } else {
      _mm_storeu_si128(dst, _mm_sub_epi16(_mm_loadu_si128(src), avg_epi16));
      if (width > 8) {
        _mm_storeu_si128(dst + 1,
                         _mm_sub_epi16(_mm_loadu_si128(src + 1), avg_epi16));
        if (width == 32) {
          _mm_storeu_si128(dst + 2,
                           _mm_sub_epi16(_mm_loadu_si128(src + 2), avg_epi16));
          _mm_storeu_si128(dst + 3,
                           _mm_sub_epi16(_mm_loadu_si128(src + 3), avg_epi16));
        }
      }
    }
    src += CFL_BUF_LINE_I128;
    dst += CFL_BUF_LINE_I128;
  } while (src < end);
}

CFL_SUB_AVG_FN(sse2)
