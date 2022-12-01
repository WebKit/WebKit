/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <emmintrin.h>

#include "config/aom_dsp_rtcd.h"

static INLINE void sse_sum_wd4_sse2(const int16_t *data, int stride, int bh,
                                    int *x_sum, int64_t *x2_sum) {
  const int16_t *data_tmp = data;
  __m128i temp_buffer1, temp_buffer2;
  __m128i load_pixels_low, load_pixels_hi, sum_buffer, sse_buffer;
  __m128i one = _mm_set1_epi16(1);
  __m128i regx_sum = _mm_setzero_si128();
  __m128i regx2_sum = regx_sum;

  for (int j = 0; j < (bh >> 1); ++j) {
    // Load 2 rows (8 pixels) at a time.
    load_pixels_low = _mm_loadl_epi64((__m128i const *)(data_tmp));
    load_pixels_hi = _mm_loadl_epi64((__m128i const *)(data_tmp + stride));
    load_pixels_low = _mm_unpacklo_epi64(load_pixels_low, load_pixels_hi);
    sum_buffer = _mm_madd_epi16(load_pixels_low, one);
    sse_buffer = _mm_madd_epi16(load_pixels_low, load_pixels_low);
    regx_sum = _mm_add_epi32(sum_buffer, regx_sum);
    regx2_sum = _mm_add_epi32(sse_buffer, regx2_sum);
    data_tmp += 2 * stride;
  }

  regx_sum = _mm_add_epi32(regx_sum, _mm_srli_si128(regx_sum, 8));
  regx_sum = _mm_add_epi32(regx_sum, _mm_srli_si128(regx_sum, 4));
  *x_sum = _mm_cvtsi128_si32(regx_sum);
  temp_buffer1 = _mm_unpacklo_epi32(regx2_sum, _mm_setzero_si128());
  temp_buffer2 = _mm_unpackhi_epi32(regx2_sum, _mm_setzero_si128());
  regx2_sum = _mm_add_epi64(temp_buffer1, temp_buffer2);
  regx2_sum = _mm_add_epi64(regx2_sum, _mm_srli_si128(regx2_sum, 8));
#if ARCH_X86_64
  *x2_sum += _mm_cvtsi128_si64(regx2_sum);
#else
  {
    int64_t tmp;
    _mm_storel_epi64((__m128i *)&tmp, regx2_sum);
    *x2_sum += tmp;
  }
#endif
}

static INLINE void sse_sum_wd8_sse2(const int16_t *data, int stride, int bh,
                                    int *x_sum, int64_t *x2_sum,
                                    int loop_cycles) {
  const int16_t *data_tmp;
  __m128i temp_buffer1, temp_buffer2;
  __m128i one = _mm_set1_epi16(1);
  __m128i regx_sum = _mm_setzero_si128();
  __m128i regx2_sum = regx_sum;
  __m128i load_pixels, sum_buffer, sse_buffer;

  for (int i = 0; i < loop_cycles; ++i) {
    data_tmp = data + (8 * i);
    for (int j = 0; j < bh; ++j) {
      // Load 1 row (8-pixels) at a time.
      load_pixels = _mm_loadu_si128((__m128i const *)(data_tmp));
      sum_buffer = _mm_madd_epi16(load_pixels, one);
      sse_buffer = _mm_madd_epi16(load_pixels, load_pixels);
      regx_sum = _mm_add_epi32(sum_buffer, regx_sum);
      regx2_sum = _mm_add_epi32(sse_buffer, regx2_sum);
      data_tmp += stride;
    }
  }

  regx_sum = _mm_add_epi32(regx_sum, _mm_srli_si128(regx_sum, 8));
  regx_sum = _mm_add_epi32(regx_sum, _mm_srli_si128(regx_sum, 4));
  *x_sum += _mm_cvtsi128_si32(regx_sum);
  temp_buffer1 = _mm_unpacklo_epi32(regx2_sum, _mm_setzero_si128());
  temp_buffer2 = _mm_unpackhi_epi32(regx2_sum, _mm_setzero_si128());
  regx2_sum = _mm_add_epi64(temp_buffer1, temp_buffer2);
  regx2_sum = _mm_add_epi64(regx2_sum, _mm_srli_si128(regx2_sum, 8));
#if ARCH_X86_64
  *x2_sum += _mm_cvtsi128_si64(regx2_sum);
#else
  {
    int64_t tmp;
    _mm_storel_epi64((__m128i *)&tmp, regx2_sum);
    *x2_sum += tmp;
  }
#endif
}

// This functions adds SSE2 Support for the functions 'get_blk_sse_sum_c'
void aom_get_blk_sse_sum_sse2(const int16_t *data, int stride, int bw, int bh,
                              int *x_sum, int64_t *x2_sum) {
  *x_sum = 0;
  *x2_sum = 0;

  if ((bh & 3) == 0) {
    switch (bw) {
      case 4: sse_sum_wd4_sse2(data, stride, bh, x_sum, x2_sum); break;
      case 8:
      case 16:
        sse_sum_wd8_sse2(data, stride, bh, x_sum, x2_sum, bw >> 3);
        break;
        // For widths 32 and 64, the registers may overflow. So compute
        // partial widths at a time.
      case 32:
        if (bh <= 32) {
          sse_sum_wd8_sse2(data, stride, bh, x_sum, x2_sum, bw >> 3);
          break;
        } else {
          sse_sum_wd8_sse2(data, stride, 32, x_sum, x2_sum, bw >> 3);
          sse_sum_wd8_sse2(data + 32 * stride, stride, 32, x_sum, x2_sum,
                           bw >> 3);
          break;
        }

      case 64:
        if (bh <= 16) {
          sse_sum_wd8_sse2(data, stride, bh, x_sum, x2_sum, bw >> 3);
          break;
        } else {
          for (int i = 0; i < bh; i += 16)
            sse_sum_wd8_sse2(data + i * stride, stride, 16, x_sum, x2_sum,
                             bw >> 3);
          break;
        }

      default: aom_get_blk_sse_sum_c(data, stride, bw, bh, x_sum, x2_sum);
    }
  } else {
    aom_get_blk_sse_sum_c(data, stride, bw, bh, x_sum, x2_sum);
  }
}
