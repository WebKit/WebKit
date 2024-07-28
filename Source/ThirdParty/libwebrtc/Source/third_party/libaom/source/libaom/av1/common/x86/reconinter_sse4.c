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

#include <emmintrin.h>  // SSE2
#include <smmintrin.h>  /* SSE4.1 */

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "av1/common/blockd.h"
#include "config/av1_rtcd.h"

static INLINE __m128i calc_mask(const __m128i mask_base, const __m128i s0,
                                const __m128i s1) {
  const __m128i diff = _mm_abs_epi16(_mm_sub_epi16(s0, s1));
  return _mm_abs_epi16(_mm_add_epi16(mask_base, _mm_srli_epi16(diff, 4)));
  // clamp(diff, 0, 64) can be skiped for diff is always in the range ( 38, 54)
}

void av1_build_compound_diffwtd_mask_sse4_1(uint8_t *mask,
                                            DIFFWTD_MASK_TYPE mask_type,
                                            const uint8_t *src0, int stride0,
                                            const uint8_t *src1, int stride1,
                                            int h, int w) {
  const int mb = (mask_type == DIFFWTD_38_INV) ? AOM_BLEND_A64_MAX_ALPHA : 0;
  const __m128i mask_base = _mm_set1_epi16(38 - mb);
  int i = 0;
  if (4 == w) {
    do {
      const __m128i s0A = _mm_cvtsi32_si128(*(int *)src0);
      const __m128i s0B = _mm_cvtsi32_si128(*(int *)(src0 + stride0));
      const __m128i s0AB = _mm_unpacklo_epi32(s0A, s0B);
      const __m128i s0 = _mm_cvtepu8_epi16(s0AB);

      const __m128i s1A = _mm_cvtsi32_si128(*(int *)src1);
      const __m128i s1B = _mm_cvtsi32_si128(*(int *)(src1 + stride1));
      const __m128i s1AB = _mm_unpacklo_epi32(s1A, s1B);
      const __m128i s1 = _mm_cvtepu8_epi16(s1AB);

      const __m128i m16 = calc_mask(mask_base, s0, s1);
      const __m128i m8 = _mm_packus_epi16(m16, m16);

      *(int *)mask = _mm_cvtsi128_si32(m8);
      *(int *)(mask + w) = _mm_extract_epi32(m8, 1);
      src0 += (stride0 << 1);
      src1 += (stride1 << 1);
      mask += 8;
      i += 2;
    } while (i < h);
  } else if (8 == w) {
    do {
      __m128i s0 = _mm_loadl_epi64((__m128i const *)src0);
      __m128i s1 = _mm_loadl_epi64((__m128i const *)src1);
      s0 = _mm_cvtepu8_epi16(s0);
      s1 = _mm_cvtepu8_epi16(s1);
      const __m128i m16 = calc_mask(mask_base, s0, s1);
      const __m128i m8 = _mm_packus_epi16(m16, m16);
      _mm_storel_epi64((__m128i *)mask, m8);
      src0 += stride0;
      src1 += stride1;
      mask += 8;
      i += 1;
    } while (i < h);
  } else {
    const __m128i zero = _mm_setzero_si128();
    do {
      int j = 0;
      do {
        const __m128i s0 = _mm_load_si128((__m128i const *)(src0 + j));
        const __m128i s1 = _mm_load_si128((__m128i const *)(src1 + j));
        const __m128i s0L = _mm_cvtepu8_epi16(s0);
        const __m128i s1L = _mm_cvtepu8_epi16(s1);
        const __m128i s0H = _mm_unpackhi_epi8(s0, zero);
        const __m128i s1H = _mm_unpackhi_epi8(s1, zero);

        const __m128i m16L = calc_mask(mask_base, s0L, s1L);
        const __m128i m16H = calc_mask(mask_base, s0H, s1H);

        const __m128i m8 = _mm_packus_epi16(m16L, m16H);
        _mm_store_si128((__m128i *)(mask + j), m8);
        j += 16;
      } while (j < w);
      src0 += stride0;
      src1 += stride1;
      mask += w;
      i += 1;
    } while (i < h);
  }
}

void av1_build_compound_diffwtd_mask_d16_sse4_1(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const CONV_BUF_TYPE *src0,
    int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w,
    ConvolveParams *conv_params, int bd) {
  const int which_inverse = (mask_type == DIFFWTD_38) ? 0 : 1;
  const int mask_base = 38;
  int round =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1 + (bd - 8);
  const __m128i round_const = _mm_set1_epi16((1 << round) >> 1);
  const __m128i mask_base_16 = _mm_set1_epi16(mask_base);
  const __m128i clip_diff = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i add_const =
      _mm_set1_epi16((which_inverse ? AOM_BLEND_A64_MAX_ALPHA : 0));
  const __m128i add_sign = _mm_set1_epi16((which_inverse ? -1 : 1));

  int i, j;
  // When rounding constant is added, there is a possibility of overflow.
  // However that much precision is not required. Code should very well work for
  // other values of DIFF_FACTOR_LOG2 and AOM_BLEND_A64_MAX_ALPHA as well. But
  // there is a possibility of corner case bugs.
  assert(DIFF_FACTOR_LOG2 == 4);
  assert(AOM_BLEND_A64_MAX_ALPHA == 64);
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; j += 8) {
      const __m128i data_src0 =
          _mm_loadu_si128((__m128i *)&src0[(i * src0_stride) + j]);
      const __m128i data_src1 =
          _mm_loadu_si128((__m128i *)&src1[(i * src1_stride) + j]);

      const __m128i diffa = _mm_subs_epu16(data_src0, data_src1);
      const __m128i diffb = _mm_subs_epu16(data_src1, data_src0);
      const __m128i diff = _mm_max_epu16(diffa, diffb);
      const __m128i diff_round =
          _mm_srli_epi16(_mm_adds_epu16(diff, round_const), round);
      const __m128i diff_factor = _mm_srli_epi16(diff_round, DIFF_FACTOR_LOG2);
      const __m128i diff_mask = _mm_adds_epi16(diff_factor, mask_base_16);
      __m128i diff_clamp = _mm_min_epi16(diff_mask, clip_diff);
      // clamp to 0 can be skipped since we are using add and saturate
      // instruction

      const __m128i diff_sign = _mm_sign_epi16(diff_clamp, add_sign);
      const __m128i diff_const_16 = _mm_add_epi16(diff_sign, add_const);

      // 8 bit conversion and saturation to uint8
      const __m128i res_8 = _mm_packus_epi16(diff_const_16, diff_const_16);

      // Store values into the destination buffer
      __m128i *const dst = (__m128i *)&mask[i * w + j];

      if ((w - j) > 4) {
        _mm_storel_epi64(dst, res_8);
      } else {  // w==4
        *(int *)dst = _mm_cvtsi128_si32(res_8);
      }
    }
  }
}
