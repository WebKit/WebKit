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

#include <stdio.h>
#include <tmmintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/blend.h"
#include "aom/aom_integer.h"
#include "aom_dsp/x86/synonyms.h"

#include "aom_dsp/x86/masked_sad_intrin_ssse3.h"

// For width a multiple of 16
static INLINE unsigned int masked_sad_ssse3(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *a_ptr, int a_stride,
                                            const uint8_t *b_ptr, int b_stride,
                                            const uint8_t *m_ptr, int m_stride,
                                            int width, int height);

#define MASKSADMXN_SSSE3(m, n)                                                \
  unsigned int aom_masked_sad##m##x##n##_ssse3(                               \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const uint8_t *msk, int msk_stride,         \
      int invert_mask) {                                                      \
    if (!invert_mask)                                                         \
      return masked_sad_ssse3(src, src_stride, ref, ref_stride, second_pred,  \
                              m, msk, msk_stride, m, n);                      \
    else                                                                      \
      return masked_sad_ssse3(src, src_stride, second_pred, m, ref,           \
                              ref_stride, msk, msk_stride, m, n);             \
  }

#define MASKSAD8XN_SSSE3(n)                                                   \
  unsigned int aom_masked_sad8x##n##_ssse3(                                   \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const uint8_t *msk, int msk_stride,         \
      int invert_mask) {                                                      \
    if (!invert_mask)                                                         \
      return aom_masked_sad8xh_ssse3(src, src_stride, ref, ref_stride,        \
                                     second_pred, 8, msk, msk_stride, n);     \
    else                                                                      \
      return aom_masked_sad8xh_ssse3(src, src_stride, second_pred, 8, ref,    \
                                     ref_stride, msk, msk_stride, n);         \
  }

#define MASKSAD4XN_SSSE3(n)                                                   \
  unsigned int aom_masked_sad4x##n##_ssse3(                                   \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const uint8_t *msk, int msk_stride,         \
      int invert_mask) {                                                      \
    if (!invert_mask)                                                         \
      return aom_masked_sad4xh_ssse3(src, src_stride, ref, ref_stride,        \
                                     second_pred, 4, msk, msk_stride, n);     \
    else                                                                      \
      return aom_masked_sad4xh_ssse3(src, src_stride, second_pred, 4, ref,    \
                                     ref_stride, msk, msk_stride, n);         \
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

#if !CONFIG_REALTIME_ONLY
MASKSAD4XN_SSSE3(16)
MASKSADMXN_SSSE3(16, 4)
MASKSAD8XN_SSSE3(32)
MASKSADMXN_SSSE3(32, 8)
MASKSADMXN_SSSE3(16, 64)
MASKSADMXN_SSSE3(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

static INLINE unsigned int masked_sad_ssse3(const uint8_t *src_ptr,
                                            int src_stride,
                                            const uint8_t *a_ptr, int a_stride,
                                            const uint8_t *b_ptr, int b_stride,
                                            const uint8_t *m_ptr, int m_stride,
                                            int width, int height) {
  int x, y;
  __m128i res = _mm_setzero_si128();
  const __m128i mask_max = _mm_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x += 16) {
      const __m128i src = _mm_loadu_si128((const __m128i *)&src_ptr[x]);
      const __m128i a = _mm_loadu_si128((const __m128i *)&a_ptr[x]);
      const __m128i b = _mm_loadu_si128((const __m128i *)&b_ptr[x]);
      const __m128i m = _mm_loadu_si128((const __m128i *)&m_ptr[x]);
      const __m128i m_inv = _mm_sub_epi8(mask_max, m);

      // Calculate 16 predicted pixels.
      // Note that the maximum value of any entry of 'pred_l' or 'pred_r'
      // is 64 * 255, so we have plenty of space to add rounding constants.
      const __m128i data_l = _mm_unpacklo_epi8(a, b);
      const __m128i mask_l = _mm_unpacklo_epi8(m, m_inv);
      __m128i pred_l = _mm_maddubs_epi16(data_l, mask_l);
      pred_l = xx_roundn_epu16(pred_l, AOM_BLEND_A64_ROUND_BITS);

      const __m128i data_r = _mm_unpackhi_epi8(a, b);
      const __m128i mask_r = _mm_unpackhi_epi8(m, m_inv);
      __m128i pred_r = _mm_maddubs_epi16(data_r, mask_r);
      pred_r = xx_roundn_epu16(pred_r, AOM_BLEND_A64_ROUND_BITS);

      const __m128i pred = _mm_packus_epi16(pred_l, pred_r);
      res = _mm_add_epi32(res, _mm_sad_epu8(pred, src));
    }

    src_ptr += src_stride;
    a_ptr += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  // At this point, we have two 32-bit partial SADs in lanes 0 and 2 of 'res'.
  unsigned int sad = (unsigned int)(_mm_cvtsi128_si32(res) +
                                    _mm_cvtsi128_si32(_mm_srli_si128(res, 8)));
  return sad;
}

unsigned int aom_masked_sad8xh_ssse3(const uint8_t *src_ptr, int src_stride,
                                     const uint8_t *a_ptr, int a_stride,
                                     const uint8_t *b_ptr, int b_stride,
                                     const uint8_t *m_ptr, int m_stride,
                                     int height) {
  int y;
  __m128i res = _mm_setzero_si128();
  const __m128i mask_max = _mm_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));

  for (y = 0; y < height; y += 2) {
    const __m128i src = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)src_ptr),
        _mm_loadl_epi64((const __m128i *)&src_ptr[src_stride]));
    const __m128i a0 = _mm_loadl_epi64((const __m128i *)a_ptr);
    const __m128i a1 = _mm_loadl_epi64((const __m128i *)&a_ptr[a_stride]);
    const __m128i b0 = _mm_loadl_epi64((const __m128i *)b_ptr);
    const __m128i b1 = _mm_loadl_epi64((const __m128i *)&b_ptr[b_stride]);
    const __m128i m =
        _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)m_ptr),
                           _mm_loadl_epi64((const __m128i *)&m_ptr[m_stride]));
    const __m128i m_inv = _mm_sub_epi8(mask_max, m);

    const __m128i data_l = _mm_unpacklo_epi8(a0, b0);
    const __m128i mask_l = _mm_unpacklo_epi8(m, m_inv);
    __m128i pred_l = _mm_maddubs_epi16(data_l, mask_l);
    pred_l = xx_roundn_epu16(pred_l, AOM_BLEND_A64_ROUND_BITS);

    const __m128i data_r = _mm_unpacklo_epi8(a1, b1);
    const __m128i mask_r = _mm_unpackhi_epi8(m, m_inv);
    __m128i pred_r = _mm_maddubs_epi16(data_r, mask_r);
    pred_r = xx_roundn_epu16(pred_r, AOM_BLEND_A64_ROUND_BITS);

    const __m128i pred = _mm_packus_epi16(pred_l, pred_r);
    res = _mm_add_epi32(res, _mm_sad_epu8(pred, src));

    src_ptr += src_stride * 2;
    a_ptr += a_stride * 2;
    b_ptr += b_stride * 2;
    m_ptr += m_stride * 2;
  }
  unsigned int sad = (unsigned int)(_mm_cvtsi128_si32(res) +
                                    _mm_cvtsi128_si32(_mm_srli_si128(res, 8)));
  return sad;
}

unsigned int aom_masked_sad4xh_ssse3(const uint8_t *src_ptr, int src_stride,
                                     const uint8_t *a_ptr, int a_stride,
                                     const uint8_t *b_ptr, int b_stride,
                                     const uint8_t *m_ptr, int m_stride,
                                     int height) {
  int y;
  __m128i res = _mm_setzero_si128();
  const __m128i mask_max = _mm_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));

  for (y = 0; y < height; y += 2) {
    // Load two rows at a time, this seems to be a bit faster
    // than four rows at a time in this case.
    const __m128i src =
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(int *)src_ptr),
                           _mm_cvtsi32_si128(*(int *)&src_ptr[src_stride]));
    const __m128i a =
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(int *)a_ptr),
                           _mm_cvtsi32_si128(*(int *)&a_ptr[a_stride]));
    const __m128i b =
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(int *)b_ptr),
                           _mm_cvtsi32_si128(*(int *)&b_ptr[b_stride]));
    const __m128i m =
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(int *)m_ptr),
                           _mm_cvtsi32_si128(*(int *)&m_ptr[m_stride]));
    const __m128i m_inv = _mm_sub_epi8(mask_max, m);

    const __m128i data = _mm_unpacklo_epi8(a, b);
    const __m128i mask = _mm_unpacklo_epi8(m, m_inv);
    __m128i pred_16bit = _mm_maddubs_epi16(data, mask);
    pred_16bit = xx_roundn_epu16(pred_16bit, AOM_BLEND_A64_ROUND_BITS);

    const __m128i pred = _mm_packus_epi16(pred_16bit, _mm_setzero_si128());
    res = _mm_add_epi32(res, _mm_sad_epu8(pred, src));

    src_ptr += src_stride * 2;
    a_ptr += a_stride * 2;
    b_ptr += b_stride * 2;
    m_ptr += m_stride * 2;
  }
  // At this point, the SAD is stored in lane 0 of 'res'
  return (unsigned int)_mm_cvtsi128_si32(res);
}

// For width a multiple of 8
static INLINE unsigned int highbd_masked_sad_ssse3(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m_ptr, int m_stride,
    int width, int height);

#define HIGHBD_MASKSADMXN_SSSE3(m, n)                                         \
  unsigned int aom_highbd_masked_sad##m##x##n##_ssse3(                        \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,               \
      int ref_stride, const uint8_t *second_pred8, const uint8_t *msk,        \
      int msk_stride, int invert_mask) {                                      \
    if (!invert_mask)                                                         \
      return highbd_masked_sad_ssse3(src8, src_stride, ref8, ref_stride,      \
                                     second_pred8, m, msk, msk_stride, m, n); \
    else                                                                      \
      return highbd_masked_sad_ssse3(src8, src_stride, second_pred8, m, ref8, \
                                     ref_stride, msk, msk_stride, m, n);      \
  }

#define HIGHBD_MASKSAD4XN_SSSE3(n)                                             \
  unsigned int aom_highbd_masked_sad4x##n##_ssse3(                             \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,                \
      int ref_stride, const uint8_t *second_pred8, const uint8_t *msk,         \
      int msk_stride, int invert_mask) {                                       \
    if (!invert_mask)                                                          \
      return aom_highbd_masked_sad4xh_ssse3(src8, src_stride, ref8,            \
                                            ref_stride, second_pred8, 4, msk,  \
                                            msk_stride, n);                    \
    else                                                                       \
      return aom_highbd_masked_sad4xh_ssse3(src8, src_stride, second_pred8, 4, \
                                            ref8, ref_stride, msk, msk_stride, \
                                            n);                                \
  }

HIGHBD_MASKSADMXN_SSSE3(128, 128)
HIGHBD_MASKSADMXN_SSSE3(128, 64)
HIGHBD_MASKSADMXN_SSSE3(64, 128)
HIGHBD_MASKSADMXN_SSSE3(64, 64)
HIGHBD_MASKSADMXN_SSSE3(64, 32)
HIGHBD_MASKSADMXN_SSSE3(32, 64)
HIGHBD_MASKSADMXN_SSSE3(32, 32)
HIGHBD_MASKSADMXN_SSSE3(32, 16)
HIGHBD_MASKSADMXN_SSSE3(16, 32)
HIGHBD_MASKSADMXN_SSSE3(16, 16)
HIGHBD_MASKSADMXN_SSSE3(16, 8)
HIGHBD_MASKSADMXN_SSSE3(8, 16)
HIGHBD_MASKSADMXN_SSSE3(8, 8)
HIGHBD_MASKSADMXN_SSSE3(8, 4)
HIGHBD_MASKSAD4XN_SSSE3(8)
HIGHBD_MASKSAD4XN_SSSE3(4)

#if !CONFIG_REALTIME_ONLY
HIGHBD_MASKSAD4XN_SSSE3(16)
HIGHBD_MASKSADMXN_SSSE3(16, 4)
HIGHBD_MASKSADMXN_SSSE3(8, 32)
HIGHBD_MASKSADMXN_SSSE3(32, 8)
HIGHBD_MASKSADMXN_SSSE3(16, 64)
HIGHBD_MASKSADMXN_SSSE3(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

static INLINE unsigned int highbd_masked_sad_ssse3(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m_ptr, int m_stride,
    int width, int height) {
  const uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a_ptr = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b_ptr = CONVERT_TO_SHORTPTR(b8);
  int x, y;
  __m128i res = _mm_setzero_si128();
  const __m128i mask_max = _mm_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m128i round_const =
      _mm_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m128i one = _mm_set1_epi16(1);

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x += 8) {
      const __m128i src = _mm_loadu_si128((const __m128i *)&src_ptr[x]);
      const __m128i a = _mm_loadu_si128((const __m128i *)&a_ptr[x]);
      const __m128i b = _mm_loadu_si128((const __m128i *)&b_ptr[x]);
      // Zero-extend mask to 16 bits
      const __m128i m = _mm_unpacklo_epi8(
          _mm_loadl_epi64((const __m128i *)&m_ptr[x]), _mm_setzero_si128());
      const __m128i m_inv = _mm_sub_epi16(mask_max, m);

      const __m128i data_l = _mm_unpacklo_epi16(a, b);
      const __m128i mask_l = _mm_unpacklo_epi16(m, m_inv);
      __m128i pred_l = _mm_madd_epi16(data_l, mask_l);
      pred_l = _mm_srai_epi32(_mm_add_epi32(pred_l, round_const),
                              AOM_BLEND_A64_ROUND_BITS);

      const __m128i data_r = _mm_unpackhi_epi16(a, b);
      const __m128i mask_r = _mm_unpackhi_epi16(m, m_inv);
      __m128i pred_r = _mm_madd_epi16(data_r, mask_r);
      pred_r = _mm_srai_epi32(_mm_add_epi32(pred_r, round_const),
                              AOM_BLEND_A64_ROUND_BITS);

      // Note: the maximum value in pred_l/r is (2^bd)-1 < 2^15,
      // so it is safe to do signed saturation here.
      const __m128i pred = _mm_packs_epi32(pred_l, pred_r);
      // There is no 16-bit SAD instruction, so we have to synthesize
      // an 8-element SAD. We do this by storing 4 32-bit partial SADs,
      // and accumulating them at the end
      const __m128i diff = _mm_abs_epi16(_mm_sub_epi16(pred, src));
      res = _mm_add_epi32(res, _mm_madd_epi16(diff, one));
    }

    src_ptr += src_stride;
    a_ptr += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  // At this point, we have four 32-bit partial SADs stored in 'res'.
  res = _mm_hadd_epi32(res, res);
  res = _mm_hadd_epi32(res, res);
  int sad = _mm_cvtsi128_si32(res);
  return sad;
}

unsigned int aom_highbd_masked_sad4xh_ssse3(const uint8_t *src8, int src_stride,
                                            const uint8_t *a8, int a_stride,
                                            const uint8_t *b8, int b_stride,
                                            const uint8_t *m_ptr, int m_stride,
                                            int height) {
  const uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a_ptr = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b_ptr = CONVERT_TO_SHORTPTR(b8);
  int y;
  __m128i res = _mm_setzero_si128();
  const __m128i mask_max = _mm_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m128i round_const =
      _mm_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m128i one = _mm_set1_epi16(1);

  for (y = 0; y < height; y += 2) {
    const __m128i src = _mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)src_ptr),
        _mm_loadl_epi64((const __m128i *)&src_ptr[src_stride]));
    const __m128i a =
        _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)a_ptr),
                           _mm_loadl_epi64((const __m128i *)&a_ptr[a_stride]));
    const __m128i b =
        _mm_unpacklo_epi64(_mm_loadl_epi64((const __m128i *)b_ptr),
                           _mm_loadl_epi64((const __m128i *)&b_ptr[b_stride]));
    // Zero-extend mask to 16 bits
    const __m128i m = _mm_unpacklo_epi8(
        _mm_unpacklo_epi32(_mm_cvtsi32_si128(*(const int *)m_ptr),
                           _mm_cvtsi32_si128(*(const int *)&m_ptr[m_stride])),
        _mm_setzero_si128());
    const __m128i m_inv = _mm_sub_epi16(mask_max, m);

    const __m128i data_l = _mm_unpacklo_epi16(a, b);
    const __m128i mask_l = _mm_unpacklo_epi16(m, m_inv);
    __m128i pred_l = _mm_madd_epi16(data_l, mask_l);
    pred_l = _mm_srai_epi32(_mm_add_epi32(pred_l, round_const),
                            AOM_BLEND_A64_ROUND_BITS);

    const __m128i data_r = _mm_unpackhi_epi16(a, b);
    const __m128i mask_r = _mm_unpackhi_epi16(m, m_inv);
    __m128i pred_r = _mm_madd_epi16(data_r, mask_r);
    pred_r = _mm_srai_epi32(_mm_add_epi32(pred_r, round_const),
                            AOM_BLEND_A64_ROUND_BITS);

    const __m128i pred = _mm_packs_epi32(pred_l, pred_r);
    const __m128i diff = _mm_abs_epi16(_mm_sub_epi16(pred, src));
    res = _mm_add_epi32(res, _mm_madd_epi16(diff, one));

    src_ptr += src_stride * 2;
    a_ptr += a_stride * 2;
    b_ptr += b_stride * 2;
    m_ptr += m_stride * 2;
  }
  res = _mm_hadd_epi32(res, res);
  res = _mm_hadd_epi32(res, res);
  int sad = _mm_cvtsi128_si32(res);
  return sad;
}
