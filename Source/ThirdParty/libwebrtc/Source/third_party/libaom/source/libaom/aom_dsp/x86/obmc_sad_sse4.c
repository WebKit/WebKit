/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <immintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"
#include "aom/aom_integer.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/x86/obmc_intrinsic_ssse3.h"
#include "aom_dsp/x86/synonyms.h"

////////////////////////////////////////////////////////////////////////////////
// 8 bit
////////////////////////////////////////////////////////////////////////////////

static AOM_FORCE_INLINE unsigned int obmc_sad_w4(const uint8_t *pre,
                                                 const int pre_stride,
                                                 const int32_t *wsrc,
                                                 const int32_t *mask,
                                                 const int height) {
  const int pre_step = pre_stride - 4;
  int n = 0;
  __m128i v_sad_d = _mm_setzero_si128();

  do {
    const __m128i v_p_b = xx_loadl_32(pre + n);
    const __m128i v_m_d = xx_load_128(mask + n);
    const __m128i v_w_d = xx_load_128(wsrc + n);

    const __m128i v_p_d = _mm_cvtepu8_epi32(v_p_b);

    // Values in both pre and mask fit in 15 bits, and are packed at 32 bit
    // boundaries. We use pmaddwd, as it has lower latency on Haswell
    // than pmulld but produces the same result with these inputs.
    const __m128i v_pm_d = _mm_madd_epi16(v_p_d, v_m_d);

    const __m128i v_diff_d = _mm_sub_epi32(v_w_d, v_pm_d);
    const __m128i v_absdiff_d = _mm_abs_epi32(v_diff_d);

    // Rounded absolute difference
    const __m128i v_rad_d = xx_roundn_epu32(v_absdiff_d, 12);

    v_sad_d = _mm_add_epi32(v_sad_d, v_rad_d);

    n += 4;

    if (n % 4 == 0) pre += pre_step;
  } while (n < 4 * height);

  return xx_hsum_epi32_si32(v_sad_d);
}

static AOM_FORCE_INLINE unsigned int obmc_sad_w8n(
    const uint8_t *pre, const int pre_stride, const int32_t *wsrc,
    const int32_t *mask, const int width, const int height) {
  const int pre_step = pre_stride - width;
  int n = 0;
  __m128i v_sad_d = _mm_setzero_si128();

  assert(width >= 8);
  assert(IS_POWER_OF_TWO(width));

  do {
    const __m128i v_p1_b = xx_loadl_32(pre + n + 4);
    const __m128i v_m1_d = xx_load_128(mask + n + 4);
    const __m128i v_w1_d = xx_load_128(wsrc + n + 4);
    const __m128i v_p0_b = xx_loadl_32(pre + n);
    const __m128i v_m0_d = xx_load_128(mask + n);
    const __m128i v_w0_d = xx_load_128(wsrc + n);

    const __m128i v_p0_d = _mm_cvtepu8_epi32(v_p0_b);
    const __m128i v_p1_d = _mm_cvtepu8_epi32(v_p1_b);

    // Values in both pre and mask fit in 15 bits, and are packed at 32 bit
    // boundaries. We use pmaddwd, as it has lower latency on Haswell
    // than pmulld but produces the same result with these inputs.
    const __m128i v_pm0_d = _mm_madd_epi16(v_p0_d, v_m0_d);
    const __m128i v_pm1_d = _mm_madd_epi16(v_p1_d, v_m1_d);

    const __m128i v_diff0_d = _mm_sub_epi32(v_w0_d, v_pm0_d);
    const __m128i v_diff1_d = _mm_sub_epi32(v_w1_d, v_pm1_d);
    const __m128i v_absdiff0_d = _mm_abs_epi32(v_diff0_d);
    const __m128i v_absdiff1_d = _mm_abs_epi32(v_diff1_d);

    // Rounded absolute difference
    const __m128i v_rad0_d = xx_roundn_epu32(v_absdiff0_d, 12);
    const __m128i v_rad1_d = xx_roundn_epu32(v_absdiff1_d, 12);

    v_sad_d = _mm_add_epi32(v_sad_d, v_rad0_d);
    v_sad_d = _mm_add_epi32(v_sad_d, v_rad1_d);

    n += 8;

    if (n % width == 0) pre += pre_step;
  } while (n < width * height);

  return xx_hsum_epi32_si32(v_sad_d);
}

#define OBMCSADWXH(w, h)                                       \
  unsigned int aom_obmc_sad##w##x##h##_sse4_1(                 \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc, \
      const int32_t *msk) {                                    \
    if (w == 4) {                                              \
      return obmc_sad_w4(pre, pre_stride, wsrc, msk, h);       \
    } else {                                                   \
      return obmc_sad_w8n(pre, pre_stride, wsrc, msk, w, h);   \
    }                                                          \
  }

OBMCSADWXH(128, 128)
OBMCSADWXH(128, 64)
OBMCSADWXH(64, 128)
OBMCSADWXH(64, 64)
OBMCSADWXH(64, 32)
OBMCSADWXH(32, 64)
OBMCSADWXH(32, 32)
OBMCSADWXH(32, 16)
OBMCSADWXH(16, 32)
OBMCSADWXH(16, 16)
OBMCSADWXH(16, 8)
OBMCSADWXH(8, 16)
OBMCSADWXH(8, 8)
OBMCSADWXH(8, 4)
OBMCSADWXH(4, 8)
OBMCSADWXH(4, 4)
OBMCSADWXH(4, 16)
OBMCSADWXH(16, 4)
OBMCSADWXH(8, 32)
OBMCSADWXH(32, 8)
OBMCSADWXH(16, 64)
OBMCSADWXH(64, 16)

////////////////////////////////////////////////////////////////////////////////
// High bit-depth
////////////////////////////////////////////////////////////////////////////////

static AOM_FORCE_INLINE unsigned int hbd_obmc_sad_w4(const uint8_t *pre8,
                                                     const int pre_stride,
                                                     const int32_t *wsrc,
                                                     const int32_t *mask,
                                                     const int height) {
  const uint16_t *pre = CONVERT_TO_SHORTPTR(pre8);
  const int pre_step = pre_stride - 4;
  int n = 0;
  __m128i v_sad_d = _mm_setzero_si128();

  do {
    const __m128i v_p_w = xx_loadl_64(pre + n);
    const __m128i v_m_d = xx_load_128(mask + n);
    const __m128i v_w_d = xx_load_128(wsrc + n);

    const __m128i v_p_d = _mm_cvtepu16_epi32(v_p_w);

    // Values in both pre and mask fit in 15 bits, and are packed at 32 bit
    // boundaries. We use pmaddwd, as it has lower latency on Haswell
    // than pmulld but produces the same result with these inputs.
    const __m128i v_pm_d = _mm_madd_epi16(v_p_d, v_m_d);

    const __m128i v_diff_d = _mm_sub_epi32(v_w_d, v_pm_d);
    const __m128i v_absdiff_d = _mm_abs_epi32(v_diff_d);

    // Rounded absolute difference
    const __m128i v_rad_d = xx_roundn_epu32(v_absdiff_d, 12);

    v_sad_d = _mm_add_epi32(v_sad_d, v_rad_d);

    n += 4;

    if (n % 4 == 0) pre += pre_step;
  } while (n < 4 * height);

  return xx_hsum_epi32_si32(v_sad_d);
}

static AOM_FORCE_INLINE unsigned int hbd_obmc_sad_w8n(
    const uint8_t *pre8, const int pre_stride, const int32_t *wsrc,
    const int32_t *mask, const int width, const int height) {
  const uint16_t *pre = CONVERT_TO_SHORTPTR(pre8);
  const int pre_step = pre_stride - width;
  int n = 0;
  __m128i v_sad_d = _mm_setzero_si128();

  assert(width >= 8);
  assert(IS_POWER_OF_TWO(width));

  do {
    const __m128i v_p1_w = xx_loadl_64(pre + n + 4);
    const __m128i v_m1_d = xx_load_128(mask + n + 4);
    const __m128i v_w1_d = xx_load_128(wsrc + n + 4);
    const __m128i v_p0_w = xx_loadl_64(pre + n);
    const __m128i v_m0_d = xx_load_128(mask + n);
    const __m128i v_w0_d = xx_load_128(wsrc + n);

    const __m128i v_p0_d = _mm_cvtepu16_epi32(v_p0_w);
    const __m128i v_p1_d = _mm_cvtepu16_epi32(v_p1_w);

    // Values in both pre and mask fit in 15 bits, and are packed at 32 bit
    // boundaries. We use pmaddwd, as it has lower latency on Haswell
    // than pmulld but produces the same result with these inputs.
    const __m128i v_pm0_d = _mm_madd_epi16(v_p0_d, v_m0_d);
    const __m128i v_pm1_d = _mm_madd_epi16(v_p1_d, v_m1_d);

    const __m128i v_diff0_d = _mm_sub_epi32(v_w0_d, v_pm0_d);
    const __m128i v_diff1_d = _mm_sub_epi32(v_w1_d, v_pm1_d);
    const __m128i v_absdiff0_d = _mm_abs_epi32(v_diff0_d);
    const __m128i v_absdiff1_d = _mm_abs_epi32(v_diff1_d);

    // Rounded absolute difference
    const __m128i v_rad0_d = xx_roundn_epu32(v_absdiff0_d, 12);
    const __m128i v_rad1_d = xx_roundn_epu32(v_absdiff1_d, 12);

    v_sad_d = _mm_add_epi32(v_sad_d, v_rad0_d);
    v_sad_d = _mm_add_epi32(v_sad_d, v_rad1_d);

    n += 8;

    if (n % width == 0) pre += pre_step;
  } while (n < width * height);

  return xx_hsum_epi32_si32(v_sad_d);
}

#define HBD_OBMCSADWXH(w, h)                                      \
  unsigned int aom_highbd_obmc_sad##w##x##h##_sse4_1(             \
      const uint8_t *pre, int pre_stride, const int32_t *wsrc,    \
      const int32_t *mask) {                                      \
    if (w == 4) {                                                 \
      return hbd_obmc_sad_w4(pre, pre_stride, wsrc, mask, h);     \
    } else {                                                      \
      return hbd_obmc_sad_w8n(pre, pre_stride, wsrc, mask, w, h); \
    }                                                             \
  }

HBD_OBMCSADWXH(128, 128)
HBD_OBMCSADWXH(128, 64)
HBD_OBMCSADWXH(64, 128)
HBD_OBMCSADWXH(64, 64)
HBD_OBMCSADWXH(64, 32)
HBD_OBMCSADWXH(32, 64)
HBD_OBMCSADWXH(32, 32)
HBD_OBMCSADWXH(32, 16)
HBD_OBMCSADWXH(16, 32)
HBD_OBMCSADWXH(16, 16)
HBD_OBMCSADWXH(16, 8)
HBD_OBMCSADWXH(8, 16)
HBD_OBMCSADWXH(8, 8)
HBD_OBMCSADWXH(8, 4)
HBD_OBMCSADWXH(4, 8)
HBD_OBMCSADWXH(4, 4)
HBD_OBMCSADWXH(4, 16)
HBD_OBMCSADWXH(16, 4)
HBD_OBMCSADWXH(8, 32)
HBD_OBMCSADWXH(32, 8)
HBD_OBMCSADWXH(16, 64)
HBD_OBMCSADWXH(64, 16)
