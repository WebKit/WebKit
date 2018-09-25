/*
 *  Copyright (c) 2017 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <emmintrin.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"

void vpx_comp_avg_pred_sse2(uint8_t *comp, const uint8_t *pred, int width,
                            int height, const uint8_t *ref, int ref_stride) {
  /* comp and pred must be 16 byte aligned. */
  assert(((intptr_t)comp & 0xf) == 0);
  assert(((intptr_t)pred & 0xf) == 0);
  if (width > 8) {
    int x, y;
    for (y = 0; y < height; ++y) {
      for (x = 0; x < width; x += 16) {
        const __m128i p = _mm_load_si128((const __m128i *)(pred + x));
        const __m128i r = _mm_loadu_si128((const __m128i *)(ref + x));
        const __m128i avg = _mm_avg_epu8(p, r);
        _mm_store_si128((__m128i *)(comp + x), avg);
      }
      comp += width;
      pred += width;
      ref += ref_stride;
    }
  } else {  // width must be 4 or 8.
    int i;
    // Process 16 elements at a time. comp and pred have width == stride and
    // therefore live in contigious memory. 4*4, 4*8, 8*4, 8*8, and 8*16 are all
    // divisible by 16 so just ref needs to be massaged when loading.
    for (i = 0; i < width * height; i += 16) {
      const __m128i p = _mm_load_si128((const __m128i *)pred);
      __m128i r;
      __m128i avg;
      if (width == ref_stride) {
        r = _mm_loadu_si128((const __m128i *)ref);
        ref += 16;
      } else if (width == 4) {
        r = _mm_set_epi32(*(const uint32_t *)(ref + 3 * ref_stride),
                          *(const uint32_t *)(ref + 2 * ref_stride),
                          *(const uint32_t *)(ref + ref_stride),
                          *(const uint32_t *)(ref));

        ref += 4 * ref_stride;
      } else {
        const __m128i r_0 = _mm_loadl_epi64((const __m128i *)ref);
        assert(width == 8);
        r = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(r_0),
                                          (const __m64 *)(ref + ref_stride)));

        ref += 2 * ref_stride;
      }
      avg = _mm_avg_epu8(p, r);
      _mm_store_si128((__m128i *)comp, avg);

      pred += 16;
      comp += 16;
    }
  }
}
