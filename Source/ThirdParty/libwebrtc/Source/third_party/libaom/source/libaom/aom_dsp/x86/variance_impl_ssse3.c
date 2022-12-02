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

#include <tmmintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/synonyms.h"

void aom_var_filter_block2d_bil_first_pass_ssse3(
    const uint8_t *a, uint16_t *b, unsigned int src_pixels_per_line,
    unsigned int pixel_step, unsigned int output_height,
    unsigned int output_width, const uint8_t *filter) {
  // Note: filter[0], filter[1] could be {128, 0}, where 128 will overflow
  // in computation using _mm_maddubs_epi16.
  // Change {128, 0} to {64, 0} and reduce FILTER_BITS by 1 to avoid overflow.
  const int16_t round = (1 << (FILTER_BITS - 1)) >> 1;
  const __m128i r = _mm_set1_epi16(round);
  const uint8_t f0 = filter[0] >> 1;
  const uint8_t f1 = filter[1] >> 1;
  const __m128i filters = _mm_setr_epi8(f0, f1, f0, f1, f0, f1, f0, f1, f0, f1,
                                        f0, f1, f0, f1, f0, f1);
  unsigned int i, j;
  (void)pixel_step;

  if (output_width >= 8) {
    for (i = 0; i < output_height; ++i) {
      for (j = 0; j < output_width; j += 8) {
        // load source
        __m128i source_low = xx_loadl_64(a);
        __m128i source_hi = xx_loadl_64(a + 1);

        // unpack to:
        // { a[0], a[1], a[1], a[2], a[2], a[3], a[3], a[4],
        //   a[4], a[5], a[5], a[6], a[6], a[7], a[7], a[8] }
        __m128i source = _mm_unpacklo_epi8(source_low, source_hi);

        // b[i] = a[i] * filter[0] + a[i + 1] * filter[1]
        __m128i res = _mm_maddubs_epi16(source, filters);

        // round
        res = _mm_srai_epi16(_mm_add_epi16(res, r), FILTER_BITS - 1);

        xx_storeu_128(b, res);

        a += 8;
        b += 8;
      }

      a += src_pixels_per_line - output_width;
    }
  } else {
    const __m128i shuffle_mask =
        _mm_setr_epi8(0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8);
    for (i = 0; i < output_height; ++i) {
      // load source, only first 5 values are meaningful:
      // { a[0], a[1], a[2], a[3], a[4], xxxx }
      __m128i source = xx_loadl_64(a);

      // shuffle, up to the first 8 are useful
      // { a[0], a[1], a[1], a[2], a[2], a[3], a[3], a[4],
      //   a[4], a[5], a[5], a[6], a[6], a[7], a[7], a[8] }
      __m128i source_shuffle = _mm_shuffle_epi8(source, shuffle_mask);

      __m128i res = _mm_maddubs_epi16(source_shuffle, filters);
      res = _mm_srai_epi16(_mm_add_epi16(res, r), FILTER_BITS - 1);

      xx_storel_64(b, res);

      a += src_pixels_per_line;
      b += output_width;
    }
  }
}

void aom_var_filter_block2d_bil_second_pass_ssse3(
    const uint16_t *a, uint8_t *b, unsigned int src_pixels_per_line,
    unsigned int pixel_step, unsigned int output_height,
    unsigned int output_width, const uint8_t *filter) {
  const int16_t round = (1 << FILTER_BITS) >> 1;
  const __m128i r = _mm_set1_epi32(round);
  const __m128i filters =
      _mm_setr_epi16(filter[0], filter[1], filter[0], filter[1], filter[0],
                     filter[1], filter[0], filter[1]);
  const __m128i shuffle_mask =
      _mm_setr_epi8(0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15);
  const __m128i mask =
      _mm_setr_epi8(0, 4, 8, 12, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
  unsigned int i, j;

  for (i = 0; i < output_height; ++i) {
    for (j = 0; j < output_width; j += 4) {
      // load source as:
      // { a[0], a[1], a[2], a[3], a[w], a[w+1], a[w+2], a[w+3] }
      __m128i source1 = xx_loadl_64(a);
      __m128i source2 = xx_loadl_64(a + pixel_step);
      __m128i source = _mm_unpacklo_epi64(source1, source2);

      // shuffle source to:
      // { a[0], a[w], a[1], a[w+1], a[2], a[w+2], a[3], a[w+3] }
      __m128i source_shuffle = _mm_shuffle_epi8(source, shuffle_mask);

      // b[i] = a[i] * filter[0] + a[w + i] * filter[1]
      __m128i res = _mm_madd_epi16(source_shuffle, filters);

      // round
      res = _mm_srai_epi32(_mm_add_epi32(res, r), FILTER_BITS);

      // shuffle to get each lower 8 bit of every 32 bit
      res = _mm_shuffle_epi8(res, mask);

      xx_storel_32(b, res);

      a += 4;
      b += 4;
    }

    a += src_pixels_per_line - output_width;
  }
}
