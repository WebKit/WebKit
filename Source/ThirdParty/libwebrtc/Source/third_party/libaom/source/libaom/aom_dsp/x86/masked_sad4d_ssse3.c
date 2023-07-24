/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdio.h>
#include <tmmintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/blend.h"
#include "aom/aom_integer.h"
#include "aom_dsp/x86/synonyms.h"

#include "aom_dsp/x86/masked_sad_intrin_ssse3.h"

#define MASK_SAD16XH_ONE_REF(idx)                             \
  a = _mm_loadu_si128((const __m128i *)&ref##idx[x]);         \
  data_l = _mm_unpacklo_epi8(a, b);                           \
  mask_l = _mm_unpacklo_epi8(m, m_inv);                       \
  pred_l = _mm_maddubs_epi16(data_l, mask_l);                 \
  pred_l = xx_roundn_epu16(pred_l, AOM_BLEND_A64_ROUND_BITS); \
                                                              \
  data_r = _mm_unpackhi_epi8(a, b);                           \
  mask_r = _mm_unpackhi_epi8(m, m_inv);                       \
  pred_r = _mm_maddubs_epi16(data_r, mask_r);                 \
  pred_r = xx_roundn_epu16(pred_r, AOM_BLEND_A64_ROUND_BITS); \
                                                              \
  pred = _mm_packus_epi16(pred_l, pred_r);                    \
  res##idx = _mm_add_epi32(res##idx, _mm_sad_epu8(pred, src));

static INLINE void masked_sadx4d_ssse3(const uint8_t *src_ptr, int src_stride,
                                       const uint8_t *a_ptr[4], int a_stride,
                                       const uint8_t *b_ptr, int b_stride,
                                       const uint8_t *m_ptr, int m_stride,
                                       int width, int height, int inv_mask,
                                       unsigned sad_array[4]) {
  int x, y;
  __m128i a;
  __m128i data_l, data_r, mask_l, mask_r, pred_l, pred_r, pred;
  const __m128i mask_max = _mm_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));
  __m128i res0 = _mm_setzero_si128();
  __m128i res1 = _mm_setzero_si128();
  __m128i res2 = _mm_setzero_si128();
  __m128i res3 = _mm_setzero_si128();
  const uint8_t *ref0 = a_ptr[0];
  const uint8_t *ref1 = a_ptr[1];
  const uint8_t *ref2 = a_ptr[2];
  const uint8_t *ref3 = a_ptr[3];

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x += 16) {
      const __m128i src = _mm_loadu_si128((const __m128i *)&src_ptr[x]);
      const __m128i b = _mm_loadu_si128((const __m128i *)&b_ptr[x]);
      const __m128i m_copy = _mm_loadu_si128((const __m128i *)&m_ptr[x]);
      __m128i m_inv = _mm_sub_epi8(mask_max, m_copy);
      __m128i m = inv_mask ? m_inv : m_copy;
      m_inv = inv_mask ? m_copy : m_inv;

      MASK_SAD16XH_ONE_REF(0)
      MASK_SAD16XH_ONE_REF(1)
      MASK_SAD16XH_ONE_REF(2)
      MASK_SAD16XH_ONE_REF(3)
    }

    src_ptr += src_stride;
    ref0 += a_stride;
    ref1 += a_stride;
    ref2 += a_stride;
    ref3 += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  res0 = _mm_add_epi32(_mm_unpacklo_epi32(res0, res1),
                       _mm_unpackhi_epi32(res0, res1));
  res2 = _mm_add_epi32(_mm_unpacklo_epi32(res2, res3),
                       _mm_unpackhi_epi32(res2, res3));

  res0 = _mm_unpacklo_epi64(res0, res2);
  _mm_storeu_si128((__m128i *)sad_array, res0);
}

#define MASK_SAD8XH_ONE_REF(idx)                                               \
  const __m128i a##idx##0 = _mm_loadl_epi64((__m128i *)ref##idx);              \
  const __m128i a##idx##1 = _mm_loadl_epi64((__m128i *)(ref##idx + a_stride)); \
  data_l = _mm_unpacklo_epi8(a##idx##0, b0);                                   \
  mask_l = _mm_unpacklo_epi8(m, m_inv);                                        \
  pred_l = _mm_maddubs_epi16(data_l, mask_l);                                  \
  pred_l = xx_roundn_epu16(pred_l, AOM_BLEND_A64_ROUND_BITS);                  \
                                                                               \
  data_r = _mm_unpacklo_epi8(a##idx##1, b1);                                   \
  mask_r = _mm_unpackhi_epi8(m, m_inv);                                        \
  pred_r = _mm_maddubs_epi16(data_r, mask_r);                                  \
  pred_r = xx_roundn_epu16(pred_r, AOM_BLEND_A64_ROUND_BITS);                  \
                                                                               \
  pred = _mm_packus_epi16(pred_l, pred_r);                                     \
  res##idx = _mm_add_epi32(res##idx, _mm_sad_epu8(pred, src));

void aom_masked_sad8xhx4d_ssse3(const uint8_t *src_ptr, int src_stride,
                                const uint8_t *ref_array[4], int a_stride,
                                const uint8_t *b_ptr, int b_stride,
                                const uint8_t *m_ptr, int m_stride, int height,
                                int inv_mask, unsigned sad_array[4]) {
  const uint8_t *ref0 = ref_array[0];
  const uint8_t *ref1 = ref_array[1];
  const uint8_t *ref2 = ref_array[2];
  const uint8_t *ref3 = ref_array[3];
  __m128i data_l, data_r, pred_l, pred_r, mask_l, mask_r, pred;
  __m128i res0 = _mm_setzero_si128();
  __m128i res1 = _mm_setzero_si128();
  __m128i res2 = _mm_setzero_si128();
  __m128i res3 = _mm_setzero_si128();
  const __m128i mask_max = _mm_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));

  for (int y = 0; y < height; y += 2) {
    const __m128i src = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)src_ptr),
        _mm_loadl_epi64((const __m128i *)(src_ptr + src_stride)));
    const __m128i b0 = _mm_loadl_epi64((__m128i *)b_ptr);
    const __m128i b1 = _mm_loadl_epi64((__m128i *)(b_ptr + b_stride));
    const __m128i m0 = _mm_loadl_epi64((__m128i *)m_ptr);
    const __m128i m1 = _mm_loadl_epi64((__m128i *)(m_ptr + m_stride));
    __m128i m_copy = _mm_unpacklo_epi64(m0, m1);
    __m128i m_inv = _mm_sub_epi8(mask_max, m_copy);
    __m128i m = inv_mask ? m_inv : m_copy;
    m_inv = inv_mask ? m_copy : m_inv;

    MASK_SAD8XH_ONE_REF(0)
    MASK_SAD8XH_ONE_REF(1)
    MASK_SAD8XH_ONE_REF(2)
    MASK_SAD8XH_ONE_REF(3)

    ref0 += 2 * a_stride;
    ref1 += 2 * a_stride;
    ref2 += 2 * a_stride;
    ref3 += 2 * a_stride;
    src_ptr += 2 * src_stride;
    b_ptr += 2 * b_stride;
    m_ptr += 2 * m_stride;
  }
  res0 = _mm_add_epi32(_mm_unpacklo_epi32(res0, res1),
                       _mm_unpackhi_epi32(res0, res1));
  res2 = _mm_add_epi32(_mm_unpacklo_epi32(res2, res3),
                       _mm_unpackhi_epi32(res2, res3));
  res0 = _mm_unpacklo_epi64(res0, res2);
  _mm_storeu_si128((__m128i *)sad_array, res0);
}

#define MASK_SAD4XH_ONE_REF(idx)                                          \
  a = _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(int *)ref##idx),             \
                         _mm_cvtsi32_si128(*(int *)&ref##idx[a_stride])); \
  data = _mm_unpacklo_epi8(a, b);                                         \
  mask = _mm_unpacklo_epi8(m, m_inv);                                     \
  pred = _mm_maddubs_epi16(data, mask);                                   \
  pred = xx_roundn_epu16(pred, AOM_BLEND_A64_ROUND_BITS);                 \
                                                                          \
  pred = _mm_packus_epi16(pred, _mm_setzero_si128());                     \
  res##idx = _mm_add_epi32(res##idx, _mm_sad_epu8(pred, src));

void aom_masked_sad4xhx4d_ssse3(const uint8_t *src_ptr, int src_stride,
                                const uint8_t *ref_array[4], int a_stride,
                                const uint8_t *b_ptr, int b_stride,
                                const uint8_t *m_ptr, int m_stride, int height,
                                int inv_mask, unsigned sad_array[4]) {
  const uint8_t *ref0 = ref_array[0];
  const uint8_t *ref1 = ref_array[1];
  const uint8_t *ref2 = ref_array[2];
  const uint8_t *ref3 = ref_array[3];
  __m128i data, pred, mask;
  __m128i res0 = _mm_setzero_si128();
  __m128i res1 = _mm_setzero_si128();
  __m128i res2 = _mm_setzero_si128();
  __m128i res3 = _mm_setzero_si128();
  __m128i a;
  const __m128i mask_max = _mm_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));

  for (int y = 0; y < height; y += 2) {
    const __m128i src =
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(int *)src_ptr),
                           _mm_cvtsi32_si128(*(int *)&src_ptr[src_stride]));
    const __m128i b =
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(int *)b_ptr),
                           _mm_cvtsi32_si128(*(int *)&b_ptr[b_stride]));
    const __m128i m_copy =
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(int *)m_ptr),
                           _mm_cvtsi32_si128(*(int *)&m_ptr[m_stride]));

    __m128i m_inv = _mm_sub_epi8(mask_max, m_copy);
    __m128i m = inv_mask ? m_inv : m_copy;
    m_inv = inv_mask ? m_copy : m_inv;

    MASK_SAD4XH_ONE_REF(0)
    MASK_SAD4XH_ONE_REF(1)
    MASK_SAD4XH_ONE_REF(2)
    MASK_SAD4XH_ONE_REF(3)

    ref0 += 2 * a_stride;
    ref1 += 2 * a_stride;
    ref2 += 2 * a_stride;
    ref3 += 2 * a_stride;
    src_ptr += 2 * src_stride;
    b_ptr += 2 * b_stride;
    m_ptr += 2 * m_stride;
  }
  res0 = _mm_unpacklo_epi32(res0, res1);
  res2 = _mm_unpacklo_epi32(res2, res3);
  res0 = _mm_unpacklo_epi64(res0, res2);
  _mm_storeu_si128((__m128i *)sad_array, res0);
}

#define MASKSADMXN_SSSE3(m, n)                                                 \
  void aom_masked_sad##m##x##n##x4d_ssse3(                                     \
      const uint8_t *src, int src_stride, const uint8_t *ref[4],               \
      int ref_stride, const uint8_t *second_pred, const uint8_t *msk,          \
      int msk_stride, int inv_mask, unsigned sad_array[4]) {                   \
    masked_sadx4d_ssse3(src, src_stride, ref, ref_stride, second_pred, m, msk, \
                        msk_stride, m, n, inv_mask, sad_array);                \
  }

#define MASKSAD8XN_SSSE3(n)                                                   \
  void aom_masked_sad8x##n##x4d_ssse3(                                        \
      const uint8_t *src, int src_stride, const uint8_t *ref[4],              \
      int ref_stride, const uint8_t *second_pred, const uint8_t *msk,         \
      int msk_stride, int inv_mask, unsigned sad_array[4]) {                  \
    aom_masked_sad8xhx4d_ssse3(src, src_stride, ref, ref_stride, second_pred, \
                               8, msk, msk_stride, n, inv_mask, sad_array);   \
  }

#define MASKSAD4XN_SSSE3(n)                                                   \
  void aom_masked_sad4x##n##x4d_ssse3(                                        \
      const uint8_t *src, int src_stride, const uint8_t *ref[4],              \
      int ref_stride, const uint8_t *second_pred, const uint8_t *msk,         \
      int msk_stride, int inv_mask, unsigned sad_array[4]) {                  \
    aom_masked_sad4xhx4d_ssse3(src, src_stride, ref, ref_stride, second_pred, \
                               4, msk, msk_stride, n, inv_mask, sad_array);   \
  }

MASKSADMXN_SSSE3(128, 128)
MASKSADMXN_SSSE3(128, 64)
MASKSADMXN_SSSE3(64, 128)
MASKSADMXN_SSSE3(64, 64)
MASKSADMXN_SSSE3(64, 32)
MASKSADMXN_SSSE3(32, 64)
MASKSADMXN_SSSE3(32, 32)
MASKSADMXN_SSSE3(32, 16)
MASKSADMXN_SSSE3(16, 32)
MASKSADMXN_SSSE3(16, 16)
MASKSADMXN_SSSE3(16, 8)
MASKSAD8XN_SSSE3(16)
MASKSAD8XN_SSSE3(8)
MASKSAD8XN_SSSE3(4)
MASKSAD4XN_SSSE3(8)
MASKSAD4XN_SSSE3(4)
MASKSAD4XN_SSSE3(16)
MASKSADMXN_SSSE3(16, 4)
MASKSAD8XN_SSSE3(32)
MASKSADMXN_SSSE3(32, 8)
MASKSADMXN_SSSE3(16, 64)
MASKSADMXN_SSSE3(64, 16)
