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

#include "aom_dsp/blend.h"
#include "aom/aom_integer.h"
#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/masked_sad_intrin_ssse3.h"

static INLINE unsigned int masked_sad32xh_avx2(
    const uint8_t *src_ptr, int src_stride, const uint8_t *a_ptr, int a_stride,
    const uint8_t *b_ptr, int b_stride, const uint8_t *m_ptr, int m_stride,
    int width, int height) {
  int x, y;
  __m256i res = _mm256_setzero_si256();
  const __m256i mask_max = _mm256_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m256i round_scale =
      _mm256_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x += 32) {
      const __m256i src = _mm256_lddqu_si256((const __m256i *)&src_ptr[x]);
      const __m256i a = _mm256_lddqu_si256((const __m256i *)&a_ptr[x]);
      const __m256i b = _mm256_lddqu_si256((const __m256i *)&b_ptr[x]);
      const __m256i m = _mm256_lddqu_si256((const __m256i *)&m_ptr[x]);
      const __m256i m_inv = _mm256_sub_epi8(mask_max, m);

      // Calculate 16 predicted pixels.
      // Note that the maximum value of any entry of 'pred_l' or 'pred_r'
      // is 64 * 255, so we have plenty of space to add rounding constants.
      const __m256i data_l = _mm256_unpacklo_epi8(a, b);
      const __m256i mask_l = _mm256_unpacklo_epi8(m, m_inv);
      __m256i pred_l = _mm256_maddubs_epi16(data_l, mask_l);
      pred_l = _mm256_mulhrs_epi16(pred_l, round_scale);

      const __m256i data_r = _mm256_unpackhi_epi8(a, b);
      const __m256i mask_r = _mm256_unpackhi_epi8(m, m_inv);
      __m256i pred_r = _mm256_maddubs_epi16(data_r, mask_r);
      pred_r = _mm256_mulhrs_epi16(pred_r, round_scale);

      const __m256i pred = _mm256_packus_epi16(pred_l, pred_r);
      res = _mm256_add_epi32(res, _mm256_sad_epu8(pred, src));
    }

    src_ptr += src_stride;
    a_ptr += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  // At this point, we have two 32-bit partial SADs in lanes 0 and 2 of 'res'.
  res = _mm256_shuffle_epi32(res, 0xd8);
  res = _mm256_permute4x64_epi64(res, 0xd8);
  res = _mm256_hadd_epi32(res, res);
  res = _mm256_hadd_epi32(res, res);
  int32_t sad = _mm256_extract_epi32(res, 0);
  return sad;
}

static INLINE __m256i xx_loadu2_m128i(const void *hi, const void *lo) {
  __m128i a0 = _mm_lddqu_si128((const __m128i *)(lo));
  __m128i a1 = _mm_lddqu_si128((const __m128i *)(hi));
  __m256i a = _mm256_castsi128_si256(a0);
  return _mm256_inserti128_si256(a, a1, 1);
}

static INLINE unsigned int masked_sad16xh_avx2(
    const uint8_t *src_ptr, int src_stride, const uint8_t *a_ptr, int a_stride,
    const uint8_t *b_ptr, int b_stride, const uint8_t *m_ptr, int m_stride,
    int height) {
  int y;
  __m256i res = _mm256_setzero_si256();
  const __m256i mask_max = _mm256_set1_epi8((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m256i round_scale =
      _mm256_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  for (y = 0; y < height; y += 2) {
    const __m256i src = xx_loadu2_m128i(src_ptr + src_stride, src_ptr);
    const __m256i a = xx_loadu2_m128i(a_ptr + a_stride, a_ptr);
    const __m256i b = xx_loadu2_m128i(b_ptr + b_stride, b_ptr);
    const __m256i m = xx_loadu2_m128i(m_ptr + m_stride, m_ptr);
    const __m256i m_inv = _mm256_sub_epi8(mask_max, m);

    // Calculate 16 predicted pixels.
    // Note that the maximum value of any entry of 'pred_l' or 'pred_r'
    // is 64 * 255, so we have plenty of space to add rounding constants.
    const __m256i data_l = _mm256_unpacklo_epi8(a, b);
    const __m256i mask_l = _mm256_unpacklo_epi8(m, m_inv);
    __m256i pred_l = _mm256_maddubs_epi16(data_l, mask_l);
    pred_l = _mm256_mulhrs_epi16(pred_l, round_scale);

    const __m256i data_r = _mm256_unpackhi_epi8(a, b);
    const __m256i mask_r = _mm256_unpackhi_epi8(m, m_inv);
    __m256i pred_r = _mm256_maddubs_epi16(data_r, mask_r);
    pred_r = _mm256_mulhrs_epi16(pred_r, round_scale);

    const __m256i pred = _mm256_packus_epi16(pred_l, pred_r);
    res = _mm256_add_epi32(res, _mm256_sad_epu8(pred, src));

    src_ptr += src_stride << 1;
    a_ptr += a_stride << 1;
    b_ptr += b_stride << 1;
    m_ptr += m_stride << 1;
  }
  // At this point, we have two 32-bit partial SADs in lanes 0 and 2 of 'res'.
  res = _mm256_shuffle_epi32(res, 0xd8);
  res = _mm256_permute4x64_epi64(res, 0xd8);
  res = _mm256_hadd_epi32(res, res);
  res = _mm256_hadd_epi32(res, res);
  int32_t sad = _mm256_extract_epi32(res, 0);
  return sad;
}

static INLINE unsigned int aom_masked_sad_avx2(
    const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride,
    const uint8_t *second_pred, const uint8_t *msk, int msk_stride,
    int invert_mask, int m, int n) {
  unsigned int sad;
  if (!invert_mask) {
    switch (m) {
      case 4:
        sad = aom_masked_sad4xh_ssse3(src, src_stride, ref, ref_stride,
                                      second_pred, m, msk, msk_stride, n);
        break;
      case 8:
        sad = aom_masked_sad8xh_ssse3(src, src_stride, ref, ref_stride,
                                      second_pred, m, msk, msk_stride, n);
        break;
      case 16:
        sad = masked_sad16xh_avx2(src, src_stride, ref, ref_stride, second_pred,
                                  m, msk, msk_stride, n);
        break;
      default:
        sad = masked_sad32xh_avx2(src, src_stride, ref, ref_stride, second_pred,
                                  m, msk, msk_stride, m, n);
        break;
    }
  } else {
    switch (m) {
      case 4:
        sad = aom_masked_sad4xh_ssse3(src, src_stride, second_pred, m, ref,
                                      ref_stride, msk, msk_stride, n);
        break;
      case 8:
        sad = aom_masked_sad8xh_ssse3(src, src_stride, second_pred, m, ref,
                                      ref_stride, msk, msk_stride, n);
        break;
      case 16:
        sad = masked_sad16xh_avx2(src, src_stride, second_pred, m, ref,
                                  ref_stride, msk, msk_stride, n);
        break;
      default:
        sad = masked_sad32xh_avx2(src, src_stride, second_pred, m, ref,
                                  ref_stride, msk, msk_stride, m, n);
        break;
    }
  }
  return sad;
}

#define MASKSADMXN_AVX2(m, n)                                                 \
  unsigned int aom_masked_sad##m##x##n##_avx2(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const uint8_t *msk, int msk_stride,         \
      int invert_mask) {                                                      \
    return aom_masked_sad_avx2(src, src_stride, ref, ref_stride, second_pred, \
                               msk, msk_stride, invert_mask, m, n);           \
  }

MASKSADMXN_AVX2(4, 4)
MASKSADMXN_AVX2(4, 8)
MASKSADMXN_AVX2(8, 4)
MASKSADMXN_AVX2(8, 8)
MASKSADMXN_AVX2(8, 16)
MASKSADMXN_AVX2(16, 8)
MASKSADMXN_AVX2(16, 16)
MASKSADMXN_AVX2(16, 32)
MASKSADMXN_AVX2(32, 16)
MASKSADMXN_AVX2(32, 32)
MASKSADMXN_AVX2(32, 64)
MASKSADMXN_AVX2(64, 32)
MASKSADMXN_AVX2(64, 64)
MASKSADMXN_AVX2(64, 128)
MASKSADMXN_AVX2(128, 64)
MASKSADMXN_AVX2(128, 128)
MASKSADMXN_AVX2(4, 16)
MASKSADMXN_AVX2(16, 4)
MASKSADMXN_AVX2(8, 32)
MASKSADMXN_AVX2(32, 8)
MASKSADMXN_AVX2(16, 64)
MASKSADMXN_AVX2(64, 16)

static INLINE unsigned int highbd_masked_sad8xh_avx2(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m_ptr, int m_stride,
    int height) {
  const uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a_ptr = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b_ptr = CONVERT_TO_SHORTPTR(b8);
  int y;
  __m256i res = _mm256_setzero_si256();
  const __m256i mask_max = _mm256_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m256i round_const =
      _mm256_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m256i one = _mm256_set1_epi16(1);

  for (y = 0; y < height; y += 2) {
    const __m256i src = xx_loadu2_m128i(src_ptr + src_stride, src_ptr);
    const __m256i a = xx_loadu2_m128i(a_ptr + a_stride, a_ptr);
    const __m256i b = xx_loadu2_m128i(b_ptr + b_stride, b_ptr);
    // Zero-extend mask to 16 bits
    const __m256i m = _mm256_cvtepu8_epi16(_mm_unpacklo_epi64(
        _mm_loadl_epi64((const __m128i *)(m_ptr)),
        _mm_loadl_epi64((const __m128i *)(m_ptr + m_stride))));
    const __m256i m_inv = _mm256_sub_epi16(mask_max, m);

    const __m256i data_l = _mm256_unpacklo_epi16(a, b);
    const __m256i mask_l = _mm256_unpacklo_epi16(m, m_inv);
    __m256i pred_l = _mm256_madd_epi16(data_l, mask_l);
    pred_l = _mm256_srai_epi32(_mm256_add_epi32(pred_l, round_const),
                               AOM_BLEND_A64_ROUND_BITS);

    const __m256i data_r = _mm256_unpackhi_epi16(a, b);
    const __m256i mask_r = _mm256_unpackhi_epi16(m, m_inv);
    __m256i pred_r = _mm256_madd_epi16(data_r, mask_r);
    pred_r = _mm256_srai_epi32(_mm256_add_epi32(pred_r, round_const),
                               AOM_BLEND_A64_ROUND_BITS);

    // Note: the maximum value in pred_l/r is (2^bd)-1 < 2^15,
    // so it is safe to do signed saturation here.
    const __m256i pred = _mm256_packs_epi32(pred_l, pred_r);
    // There is no 16-bit SAD instruction, so we have to synthesize
    // an 8-element SAD. We do this by storing 4 32-bit partial SADs,
    // and accumulating them at the end
    const __m256i diff = _mm256_abs_epi16(_mm256_sub_epi16(pred, src));
    res = _mm256_add_epi32(res, _mm256_madd_epi16(diff, one));

    src_ptr += src_stride << 1;
    a_ptr += a_stride << 1;
    b_ptr += b_stride << 1;
    m_ptr += m_stride << 1;
  }
  // At this point, we have four 32-bit partial SADs stored in 'res'.
  res = _mm256_hadd_epi32(res, res);
  res = _mm256_hadd_epi32(res, res);
  int sad = _mm256_extract_epi32(res, 0) + _mm256_extract_epi32(res, 4);
  return sad;
}

static INLINE unsigned int highbd_masked_sad16xh_avx2(
    const uint8_t *src8, int src_stride, const uint8_t *a8, int a_stride,
    const uint8_t *b8, int b_stride, const uint8_t *m_ptr, int m_stride,
    int width, int height) {
  const uint16_t *src_ptr = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *a_ptr = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b_ptr = CONVERT_TO_SHORTPTR(b8);
  int x, y;
  __m256i res = _mm256_setzero_si256();
  const __m256i mask_max = _mm256_set1_epi16((1 << AOM_BLEND_A64_ROUND_BITS));
  const __m256i round_const =
      _mm256_set1_epi32((1 << AOM_BLEND_A64_ROUND_BITS) >> 1);
  const __m256i one = _mm256_set1_epi16(1);

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x += 16) {
      const __m256i src = _mm256_lddqu_si256((const __m256i *)&src_ptr[x]);
      const __m256i a = _mm256_lddqu_si256((const __m256i *)&a_ptr[x]);
      const __m256i b = _mm256_lddqu_si256((const __m256i *)&b_ptr[x]);
      // Zero-extend mask to 16 bits
      const __m256i m =
          _mm256_cvtepu8_epi16(_mm_lddqu_si128((const __m128i *)&m_ptr[x]));
      const __m256i m_inv = _mm256_sub_epi16(mask_max, m);

      const __m256i data_l = _mm256_unpacklo_epi16(a, b);
      const __m256i mask_l = _mm256_unpacklo_epi16(m, m_inv);
      __m256i pred_l = _mm256_madd_epi16(data_l, mask_l);
      pred_l = _mm256_srai_epi32(_mm256_add_epi32(pred_l, round_const),
                                 AOM_BLEND_A64_ROUND_BITS);

      const __m256i data_r = _mm256_unpackhi_epi16(a, b);
      const __m256i mask_r = _mm256_unpackhi_epi16(m, m_inv);
      __m256i pred_r = _mm256_madd_epi16(data_r, mask_r);
      pred_r = _mm256_srai_epi32(_mm256_add_epi32(pred_r, round_const),
                                 AOM_BLEND_A64_ROUND_BITS);

      // Note: the maximum value in pred_l/r is (2^bd)-1 < 2^15,
      // so it is safe to do signed saturation here.
      const __m256i pred = _mm256_packs_epi32(pred_l, pred_r);
      // There is no 16-bit SAD instruction, so we have to synthesize
      // an 8-element SAD. We do this by storing 4 32-bit partial SADs,
      // and accumulating them at the end
      const __m256i diff = _mm256_abs_epi16(_mm256_sub_epi16(pred, src));
      res = _mm256_add_epi32(res, _mm256_madd_epi16(diff, one));
    }

    src_ptr += src_stride;
    a_ptr += a_stride;
    b_ptr += b_stride;
    m_ptr += m_stride;
  }
  // At this point, we have four 32-bit partial SADs stored in 'res'.
  res = _mm256_hadd_epi32(res, res);
  res = _mm256_hadd_epi32(res, res);
  int sad = _mm256_extract_epi32(res, 0) + _mm256_extract_epi32(res, 4);
  return sad;
}

static INLINE unsigned int aom_highbd_masked_sad_avx2(
    const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride,
    const uint8_t *second_pred, const uint8_t *msk, int msk_stride,
    int invert_mask, int m, int n) {
  unsigned int sad;
  if (!invert_mask) {
    switch (m) {
      case 4:
        sad =
            aom_highbd_masked_sad4xh_ssse3(src, src_stride, ref, ref_stride,
                                           second_pred, m, msk, msk_stride, n);
        break;
      case 8:
        sad = highbd_masked_sad8xh_avx2(src, src_stride, ref, ref_stride,
                                        second_pred, m, msk, msk_stride, n);
        break;
      default:
        sad = highbd_masked_sad16xh_avx2(src, src_stride, ref, ref_stride,
                                         second_pred, m, msk, msk_stride, m, n);
        break;
    }
  } else {
    switch (m) {
      case 4:
        sad =
            aom_highbd_masked_sad4xh_ssse3(src, src_stride, second_pred, m, ref,
                                           ref_stride, msk, msk_stride, n);
        break;
      case 8:
        sad = highbd_masked_sad8xh_avx2(src, src_stride, second_pred, m, ref,
                                        ref_stride, msk, msk_stride, n);
        break;
      default:
        sad = highbd_masked_sad16xh_avx2(src, src_stride, second_pred, m, ref,
                                         ref_stride, msk, msk_stride, m, n);
        break;
    }
  }
  return sad;
}

#define HIGHBD_MASKSADMXN_AVX2(m, n)                                      \
  unsigned int aom_highbd_masked_sad##m##x##n##_avx2(                     \
      const uint8_t *src8, int src_stride, const uint8_t *ref8,           \
      int ref_stride, const uint8_t *second_pred8, const uint8_t *msk,    \
      int msk_stride, int invert_mask) {                                  \
    return aom_highbd_masked_sad_avx2(src8, src_stride, ref8, ref_stride, \
                                      second_pred8, msk, msk_stride,      \
                                      invert_mask, m, n);                 \
  }

HIGHBD_MASKSADMXN_AVX2(4, 4)
HIGHBD_MASKSADMXN_AVX2(4, 8)
HIGHBD_MASKSADMXN_AVX2(8, 4)
HIGHBD_MASKSADMXN_AVX2(8, 8)
HIGHBD_MASKSADMXN_AVX2(8, 16)
HIGHBD_MASKSADMXN_AVX2(16, 8)
HIGHBD_MASKSADMXN_AVX2(16, 16)
HIGHBD_MASKSADMXN_AVX2(16, 32)
HIGHBD_MASKSADMXN_AVX2(32, 16)
HIGHBD_MASKSADMXN_AVX2(32, 32)
HIGHBD_MASKSADMXN_AVX2(32, 64)
HIGHBD_MASKSADMXN_AVX2(64, 32)
HIGHBD_MASKSADMXN_AVX2(64, 64)
HIGHBD_MASKSADMXN_AVX2(64, 128)
HIGHBD_MASKSADMXN_AVX2(128, 64)
HIGHBD_MASKSADMXN_AVX2(128, 128)
HIGHBD_MASKSADMXN_AVX2(4, 16)
HIGHBD_MASKSADMXN_AVX2(16, 4)
HIGHBD_MASKSADMXN_AVX2(8, 32)
HIGHBD_MASKSADMXN_AVX2(32, 8)
HIGHBD_MASKSADMXN_AVX2(16, 64)
HIGHBD_MASKSADMXN_AVX2(64, 16)
