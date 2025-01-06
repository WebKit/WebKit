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

#include <smmintrin.h>  // SSE4.1

#include <assert.h>

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/blend.h"

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/blend_sse4.h"
#include "aom_dsp/x86/blend_mask_sse4.h"

#include "config/aom_dsp_rtcd.h"

//////////////////////////////////////////////////////////////////////////////
// No sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_w4_sse4_1(uint8_t *dst, uint32_t dst_stride,
                                     const uint8_t *src0, uint32_t src0_stride,
                                     const uint8_t *src1, uint32_t src1_stride,
                                     const uint8_t *mask, uint32_t mask_stride,
                                     int w, int h) {
  (void)w;
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  do {
    const __m128i v_m0_b = xx_loadl_32(mask);
    const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);
    const __m128i v_res_b = blend_4_u8(src0, src1, &v_m0_b, &v_m1_b, &_r);
    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_w8_sse4_1(uint8_t *dst, uint32_t dst_stride,
                                     const uint8_t *src0, uint32_t src0_stride,
                                     const uint8_t *src1, uint32_t src1_stride,
                                     const uint8_t *mask, uint32_t mask_stride,
                                     int w, int h) {
  (void)w;
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  do {
    const __m128i v_m0_b = xx_loadl_64(mask);
    const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);
    const __m128i v_res_b = blend_8_u8(src0, src1, &v_m0_b, &v_m1_b, &_r);
    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_m0_b = xx_loadu_128(mask + c);
      const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

      const __m128i v_res_b =
          blend_16_u8(src0 + c, src1 + c, &v_m0_b, &v_m1_b, &_r);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sx_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;

  const __m128i v_shuffle_b = xx_loadu_128(g_blend_a64_mask_shuffle);
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  do {
    const __m128i v_r_b = xx_loadl_64(mask);
    const __m128i v_r0_s_b = _mm_shuffle_epi8(v_r_b, v_shuffle_b);
    const __m128i v_r_lo_b = _mm_unpacklo_epi64(v_r0_s_b, v_r0_s_b);
    const __m128i v_r_hi_b = _mm_unpackhi_epi64(v_r0_s_b, v_r0_s_b);
    const __m128i v_m0_b = _mm_avg_epu8(v_r_lo_b, v_r_hi_b);
    const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

    const __m128i v_res_b = blend_4_u8(src0, src1, &v_m0_b, &v_m1_b, &_r);
    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;

  const __m128i v_shuffle_b = xx_loadu_128(g_blend_a64_mask_shuffle);
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  do {
    const __m128i v_r_b = xx_loadu_128(mask);
    const __m128i v_r0_s_b = _mm_shuffle_epi8(v_r_b, v_shuffle_b);
    const __m128i v_r_lo_b = _mm_unpacklo_epi64(v_r0_s_b, v_r0_s_b);
    const __m128i v_r_hi_b = _mm_unpackhi_epi64(v_r0_s_b, v_r0_s_b);
    const __m128i v_m0_b = _mm_avg_epu8(v_r_lo_b, v_r_hi_b);
    const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

    const __m128i v_res_b = blend_8_u8(src0, src1, &v_m0_b, &v_m1_b, &_r);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_shuffle_b = xx_loadu_128(g_blend_a64_mask_shuffle);
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));

  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_r0_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_r1_b = xx_loadu_128(mask + 2 * c + 16);
      const __m128i v_r0_s_b = _mm_shuffle_epi8(v_r0_b, v_shuffle_b);
      const __m128i v_r1_s_b = _mm_shuffle_epi8(v_r1_b, v_shuffle_b);
      const __m128i v_r_lo_b = _mm_unpacklo_epi64(v_r0_s_b, v_r1_s_b);
      const __m128i v_r_hi_b = _mm_unpackhi_epi64(v_r0_s_b, v_r1_s_b);
      const __m128i v_m0_b = _mm_avg_epu8(v_r_lo_b, v_r_hi_b);
      const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

      const __m128i v_res_b =
          blend_16_u8(src0 + c, src1 + c, &v_m0_b, &v_m1_b, &_r);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sy_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;

  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));

  do {
    const __m128i v_ra_b = xx_loadl_32(mask);
    const __m128i v_rb_b = xx_loadl_32(mask + mask_stride);
    const __m128i v_m0_b = _mm_avg_epu8(v_ra_b, v_rb_b);
    const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

    const __m128i v_res_b = blend_4_u8(src0, src1, &v_m0_b, &v_m1_b, &_r);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sy_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;

  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_m0_b = _mm_avg_epu8(v_ra_b, v_rb_b);
    const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);
    const __m128i v_res_b = blend_8_u8(src0, src1, &v_m0_b, &v_m1_b, &_r);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sy_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_ra_b = xx_loadu_128(mask + c);
      const __m128i v_rb_b = xx_loadu_128(mask + c + mask_stride);
      const __m128i v_m0_b = _mm_avg_epu8(v_ra_b, v_rb_b);
      const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

      const __m128i v_res_b =
          blend_16_u8(src0 + c, src1 + c, &v_m0_b, &v_m1_b, &_r);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal and Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static void blend_a64_mask_sx_sy_w4_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_shuffle_b = xx_loadu_128(g_blend_a64_mask_shuffle);
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  (void)w;

  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_r_s_b = _mm_shuffle_epi8(v_rvs_b, v_shuffle_b);
    const __m128i v_r0_s_w = _mm_cvtepu8_epi16(v_r_s_b);
    const __m128i v_r1_s_w = _mm_cvtepu8_epi16(_mm_srli_si128(v_r_s_b, 8));
    const __m128i v_rs_w = _mm_add_epi16(v_r0_s_w, v_r1_s_w);
    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m0_b = _mm_packus_epi16(v_m0_w, v_m0_w);
    const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

    const __m128i v_res_b = blend_4_u8(src0, src1, &v_m0_b, &v_m1_b, &_r);

    xx_storel_32(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_sy_w8_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_shuffle_b = xx_loadu_128(g_blend_a64_mask_shuffle);
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  (void)w;

  do {
    const __m128i v_ra_b = xx_loadu_128(mask);
    const __m128i v_rb_b = xx_loadu_128(mask + mask_stride);

    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_r_s_b = _mm_shuffle_epi8(v_rvs_b, v_shuffle_b);
    const __m128i v_r0_s_w = _mm_cvtepu8_epi16(v_r_s_b);
    const __m128i v_r1_s_w = _mm_cvtepu8_epi16(_mm_srli_si128(v_r_s_b, 8));
    const __m128i v_rs_w = _mm_add_epi16(v_r0_s_w, v_r1_s_w);
    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m0_b = _mm_packus_epi16(v_m0_w, v_m0_w);
    const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

    const __m128i v_res_b = blend_8_u8(src0, src1, &v_m0_b, &v_m1_b, &_r);

    xx_storel_64(dst, v_res_b);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_sx_sy_w16n_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const uint8_t *src0,
    uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  const __m128i v_zmask_b =
      _mm_set_epi8(0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
  const __m128i v_maxval_b = _mm_set1_epi8(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i _r = _mm_set1_epi16(1 << (15 - AOM_BLEND_A64_ROUND_BITS));
  do {
    int c;
    for (c = 0; c < w; c += 16) {
      const __m128i v_ral_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_rah_b = xx_loadu_128(mask + 2 * c + 16);
      const __m128i v_rbl_b = xx_loadu_128(mask + mask_stride + 2 * c);
      const __m128i v_rbh_b = xx_loadu_128(mask + mask_stride + 2 * c + 16);
      const __m128i v_rvsl_b = _mm_add_epi8(v_ral_b, v_rbl_b);
      const __m128i v_rvsh_b = _mm_add_epi8(v_rah_b, v_rbh_b);
      const __m128i v_rvsal_w = _mm_and_si128(v_rvsl_b, v_zmask_b);
      const __m128i v_rvsah_w = _mm_and_si128(v_rvsh_b, v_zmask_b);
      const __m128i v_rvsbl_w =
          _mm_and_si128(_mm_srli_si128(v_rvsl_b, 1), v_zmask_b);
      const __m128i v_rvsbh_w =
          _mm_and_si128(_mm_srli_si128(v_rvsh_b, 1), v_zmask_b);
      const __m128i v_rsl_w = _mm_add_epi16(v_rvsal_w, v_rvsbl_w);
      const __m128i v_rsh_w = _mm_add_epi16(v_rvsah_w, v_rvsbh_w);

      const __m128i v_m0l_w = xx_roundn_epu16(v_rsl_w, 2);
      const __m128i v_m0h_w = xx_roundn_epu16(v_rsh_w, 2);
      const __m128i v_m0_b = _mm_packus_epi16(v_m0l_w, v_m0h_w);
      const __m128i v_m1_b = _mm_sub_epi8(v_maxval_b, v_m0_b);

      const __m128i v_res_b =
          blend_16_u8(src0 + c, src1 + c, &v_m0_b, &v_m1_b, &_r);

      xx_storeu_128(dst + c, v_res_b);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

//////////////////////////////////////////////////////////////////////////////
// Dispatch
//////////////////////////////////////////////////////////////////////////////

void aom_blend_a64_mask_sse4_1(uint8_t *dst, uint32_t dst_stride,
                               const uint8_t *src0, uint32_t src0_stride,
                               const uint8_t *src1, uint32_t src1_stride,
                               const uint8_t *mask, uint32_t mask_stride, int w,
                               int h, int subw, int subh) {
  typedef void (*blend_fn)(
      uint8_t * dst, uint32_t dst_stride, const uint8_t *src0,
      uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride,
      const uint8_t *mask, uint32_t mask_stride, int w, int h);

  // Dimensions are: width_index X subx X suby
  static const blend_fn blend[3][2][2] = {
    { // w % 16 == 0
      { blend_a64_mask_w16n_sse4_1, blend_a64_mask_sy_w16n_sse4_1 },
      { blend_a64_mask_sx_w16n_sse4_1, blend_a64_mask_sx_sy_w16n_sse4_1 } },
    { // w == 4
      { blend_a64_mask_w4_sse4_1, blend_a64_mask_sy_w4_sse4_1 },
      { blend_a64_mask_sx_w4_sse4_1, blend_a64_mask_sx_sy_w4_sse4_1 } },
    { // w == 8
      { blend_a64_mask_w8_sse4_1, blend_a64_mask_sy_w8_sse4_1 },
      { blend_a64_mask_sx_w8_sse4_1, blend_a64_mask_sx_sy_w8_sse4_1 } }
  };

  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  if (UNLIKELY((h | w) & 3)) {  // if (w <= 2 || h <= 2)
    aom_blend_a64_mask_c(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                         mask, mask_stride, w, h, subw, subh);
  } else {
    blend[(w >> 2) & 3][subw != 0][subh != 0](dst, dst_stride, src0,
                                              src0_stride, src1, src1_stride,
                                              mask, mask_stride, w, h);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
//////////////////////////////////////////////////////////////////////////////
// No sub-sampling
//////////////////////////////////////////////////////////////////////////////

static inline void blend_a64_mask_bn_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_m0_b = xx_loadl_32(mask);
    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                              src1_stride, mask, mask_stride, h, blend_4_b10);
}

static void blend_a64_mask_b12_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                              src1_stride, mask, mask_stride, h, blend_4_b12);
}

static inline void blend_a64_mask_bn_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h,
    blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_m0_b = xx_loadl_64(mask + c);
      const __m128i v_m0_w = _mm_cvtepu8_epi16(v_m0_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                               src1_stride, mask, mask_stride, w, h,
                               blend_8_b10);
}

static void blend_a64_mask_b12_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                               src1_stride, mask, mask_stride, w, h,
                               blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal sub-sampling
//////////////////////////////////////////////////////////////////////////////

static inline void blend_a64_mask_bn_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_zmask_b =
      _mm_set_epi8(0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_r_b = xx_loadl_64(mask);
    const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

    const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sx_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b10);
}

static void blend_a64_mask_b12_sx_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sx_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b12);
}

static inline void blend_a64_mask_bn_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h,
    blend_unit_fn blend) {
  const __m128i v_zmask_b =
      _mm_set_epi8(0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_r_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_a_b = _mm_avg_epu8(v_r_b, _mm_srli_si128(v_r_b, 1));

      const __m128i v_m0_w = _mm_and_si128(v_a_b, v_zmask_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sx_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h,
                                  blend_8_b10);
}

static void blend_a64_mask_b12_sx_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sx_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h,
                                  blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static inline void blend_a64_mask_bn_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_ra_b = xx_loadl_32(mask);
    const __m128i v_rb_b = xx_loadl_32(mask + mask_stride);
    const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

    const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b10);
}

static void blend_a64_mask_b12_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, h,
                                 blend_4_b12);
}

static inline void blend_a64_mask_bn_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h,
    blend_unit_fn blend) {
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_ra_b = xx_loadl_64(mask + c);
      const __m128i v_rb_b = xx_loadl_64(mask + c + mask_stride);
      const __m128i v_a_b = _mm_avg_epu8(v_ra_b, v_rb_b);

      const __m128i v_m0_w = _mm_cvtepu8_epi16(v_a_b);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h,
                                  blend_8_b10);
}

static void blend_a64_mask_b12_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h,
                                  blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Horizontal and Vertical sub-sampling
//////////////////////////////////////////////////////////////////////////////

static inline void blend_a64_mask_bn_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, blend_unit_fn blend) {
  const __m128i v_zmask_b =
      _mm_set_epi8(0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    const __m128i v_ra_b = xx_loadl_64(mask);
    const __m128i v_rb_b = xx_loadl_64(mask + mask_stride);
    const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
    const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
    const __m128i v_rvsb_w =
        _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
    const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

    const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
    const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

    const __m128i v_res_w = blend(src0, src1, v_m0_w, v_m1_w);

    xx_storel_64(dst, v_res_w);

    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sx_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                    src1_stride, mask, mask_stride, h,
                                    blend_4_b10);
}

static void blend_a64_mask_b12_sx_sy_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  (void)w;
  blend_a64_mask_bn_sx_sy_w4_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                    src1_stride, mask, mask_stride, h,
                                    blend_4_b12);
}

static inline void blend_a64_mask_bn_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h,
    blend_unit_fn blend) {
  const __m128i v_zmask_b =
      _mm_set_epi8(0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);
  const __m128i v_maxval_w = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  do {
    int c;
    for (c = 0; c < w; c += 8) {
      const __m128i v_ra_b = xx_loadu_128(mask + 2 * c);
      const __m128i v_rb_b = xx_loadu_128(mask + 2 * c + mask_stride);
      const __m128i v_rvs_b = _mm_add_epi8(v_ra_b, v_rb_b);
      const __m128i v_rvsa_w = _mm_and_si128(v_rvs_b, v_zmask_b);
      const __m128i v_rvsb_w =
          _mm_and_si128(_mm_srli_si128(v_rvs_b, 1), v_zmask_b);
      const __m128i v_rs_w = _mm_add_epi16(v_rvsa_w, v_rvsb_w);

      const __m128i v_m0_w = xx_roundn_epu16(v_rs_w, 2);
      const __m128i v_m1_w = _mm_sub_epi16(v_maxval_w, v_m0_w);

      const __m128i v_res_w = blend(src0 + c, src1 + c, v_m0_w, v_m1_w);

      xx_storeu_128(dst + c, v_res_w);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += 2 * mask_stride;
  } while (--h);
}

static void blend_a64_mask_b10_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sx_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                     src1_stride, mask, mask_stride, w, h,
                                     blend_8_b10);
}

static void blend_a64_mask_b12_sx_sy_w8n_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const uint16_t *src0,
    uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h) {
  blend_a64_mask_bn_sx_sy_w8n_sse4_1(dst, dst_stride, src0, src0_stride, src1,
                                     src1_stride, mask, mask_stride, w, h,
                                     blend_8_b12);
}

//////////////////////////////////////////////////////////////////////////////
// Dispatch
//////////////////////////////////////////////////////////////////////////////
void aom_highbd_blend_a64_mask_sse4_1(uint8_t *dst_8, uint32_t dst_stride,
                                      const uint8_t *src0_8,
                                      uint32_t src0_stride,
                                      const uint8_t *src1_8,
                                      uint32_t src1_stride, const uint8_t *mask,
                                      uint32_t mask_stride, int w, int h,
                                      int subw, int subh, int bd) {
  typedef void (*blend_fn)(
      uint16_t * dst, uint32_t dst_stride, const uint16_t *src0,
      uint32_t src0_stride, const uint16_t *src1, uint32_t src1_stride,
      const uint8_t *mask, uint32_t mask_stride, int w, int h);

  // Dimensions are: bd_index X width_index X subw X subh
  static const blend_fn blend[2][2][2][2] = {
    {   // bd == 8 or 10
      { // w % 8 == 0
        { blend_a64_mask_b10_w8n_sse4_1, blend_a64_mask_b10_sy_w8n_sse4_1 },
        { blend_a64_mask_b10_sx_w8n_sse4_1,
          blend_a64_mask_b10_sx_sy_w8n_sse4_1 } },
      { // w == 4
        { blend_a64_mask_b10_w4_sse4_1, blend_a64_mask_b10_sy_w4_sse4_1 },
        { blend_a64_mask_b10_sx_w4_sse4_1,
          blend_a64_mask_b10_sx_sy_w4_sse4_1 } } },
    {   // bd == 12
      { // w % 8 == 0
        { blend_a64_mask_b12_w8n_sse4_1, blend_a64_mask_b12_sy_w8n_sse4_1 },
        { blend_a64_mask_b12_sx_w8n_sse4_1,
          blend_a64_mask_b12_sx_sy_w8n_sse4_1 } },
      { // w == 4
        { blend_a64_mask_b12_w4_sse4_1, blend_a64_mask_b12_sy_w4_sse4_1 },
        { blend_a64_mask_b12_sx_w4_sse4_1,
          blend_a64_mask_b12_sx_sy_w4_sse4_1 } } }
  };

  assert(IMPLIES(src0_8 == dst_8, src0_stride == dst_stride));
  assert(IMPLIES(src1_8 == dst_8, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  assert(bd == 8 || bd == 10 || bd == 12);
  if (UNLIKELY((h | w) & 3)) {  // if (w <= 2 || h <= 2)
    aom_highbd_blend_a64_mask_c(dst_8, dst_stride, src0_8, src0_stride, src1_8,
                                src1_stride, mask, mask_stride, w, h, subw,
                                subh, bd);
  } else {
    uint16_t *const dst = CONVERT_TO_SHORTPTR(dst_8);
    const uint16_t *const src0 = CONVERT_TO_SHORTPTR(src0_8);
    const uint16_t *const src1 = CONVERT_TO_SHORTPTR(src1_8);

    blend[bd == 12][(w >> 2) & 1][subw != 0][subh != 0](
        dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
        mask_stride, w, h);
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static inline void blend_a64_d16_mask_w16_sse41(
    uint8_t *dst, const CONV_BUF_TYPE *src0, const CONV_BUF_TYPE *src1,
    const __m128i *m0, const __m128i *m1, const __m128i *v_round_offset,
    const __m128i *v_maxval, int shift) {
  const __m128i max_minus_m0 = _mm_sub_epi16(*v_maxval, *m0);
  const __m128i max_minus_m1 = _mm_sub_epi16(*v_maxval, *m1);
  const __m128i s0_0 = xx_loadu_128(src0);
  const __m128i s0_1 = xx_loadu_128(src0 + 8);
  const __m128i s1_0 = xx_loadu_128(src1);
  const __m128i s1_1 = xx_loadu_128(src1 + 8);
  __m128i res0_lo = _mm_madd_epi16(_mm_unpacklo_epi16(s0_0, s1_0),
                                   _mm_unpacklo_epi16(*m0, max_minus_m0));
  __m128i res0_hi = _mm_madd_epi16(_mm_unpackhi_epi16(s0_0, s1_0),
                                   _mm_unpackhi_epi16(*m0, max_minus_m0));
  __m128i res1_lo = _mm_madd_epi16(_mm_unpacklo_epi16(s0_1, s1_1),
                                   _mm_unpacklo_epi16(*m1, max_minus_m1));
  __m128i res1_hi = _mm_madd_epi16(_mm_unpackhi_epi16(s0_1, s1_1),
                                   _mm_unpackhi_epi16(*m1, max_minus_m1));
  res0_lo = _mm_srai_epi32(_mm_sub_epi32(res0_lo, *v_round_offset), shift);
  res0_hi = _mm_srai_epi32(_mm_sub_epi32(res0_hi, *v_round_offset), shift);
  res1_lo = _mm_srai_epi32(_mm_sub_epi32(res1_lo, *v_round_offset), shift);
  res1_hi = _mm_srai_epi32(_mm_sub_epi32(res1_hi, *v_round_offset), shift);
  const __m128i res0 = _mm_packs_epi32(res0_lo, res0_hi);
  const __m128i res1 = _mm_packs_epi32(res1_lo, res1_hi);
  const __m128i res = _mm_packus_epi16(res0, res1);

  _mm_storeu_si128((__m128i *)(dst), res);
}

static inline void lowbd_blend_a64_d16_mask_subw0_subh0_w16_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift) {
  const __m128i v_maxval = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m = xx_loadu_128(mask + j);
      const __m128i m0 = _mm_cvtepu8_epi16(m);
      const __m128i m1 = _mm_cvtepu8_epi16(_mm_srli_si128(m, 8));

      blend_a64_d16_mask_w16_sse41(dst + j, src0 + j, src1 + j, &m0, &m1,
                                   round_offset, &v_maxval, shift);
    }
    mask += mask_stride;
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
  }
}

static inline void lowbd_blend_a64_d16_mask_subw1_subh1_w16_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift) {
  const __m128i v_maxval = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i one_b = _mm_set1_epi8(1);
  const __m128i two_w = _mm_set1_epi16(2);
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m_i00 = xx_loadu_128(mask + 2 * j);
      const __m128i m_i01 = xx_loadu_128(mask + 2 * j + 16);
      const __m128i m_i10 = xx_loadu_128(mask + mask_stride + 2 * j);
      const __m128i m_i11 = xx_loadu_128(mask + mask_stride + 2 * j + 16);

      const __m128i m0_ac = _mm_adds_epu8(m_i00, m_i10);
      const __m128i m1_ac = _mm_adds_epu8(m_i01, m_i11);
      const __m128i m0_acbd = _mm_maddubs_epi16(m0_ac, one_b);
      const __m128i m1_acbd = _mm_maddubs_epi16(m1_ac, one_b);
      const __m128i m0 = _mm_srli_epi16(_mm_add_epi16(m0_acbd, two_w), 2);
      const __m128i m1 = _mm_srli_epi16(_mm_add_epi16(m1_acbd, two_w), 2);

      blend_a64_d16_mask_w16_sse41(dst + j, src0 + j, src1 + j, &m0, &m1,
                                   round_offset, &v_maxval, shift);
    }
    mask += mask_stride << 1;
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
  }
}

static inline void lowbd_blend_a64_d16_mask_subw1_subh0_w16_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift) {
  const __m128i v_maxval = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i one_b = _mm_set1_epi8(1);
  const __m128i zeros = _mm_setzero_si128();
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m_i00 = xx_loadu_128(mask + 2 * j);
      const __m128i m_i01 = xx_loadu_128(mask + 2 * j + 16);
      const __m128i m0_ac = _mm_maddubs_epi16(m_i00, one_b);
      const __m128i m1_ac = _mm_maddubs_epi16(m_i01, one_b);
      const __m128i m0 = _mm_avg_epu16(m0_ac, zeros);
      const __m128i m1 = _mm_avg_epu16(m1_ac, zeros);

      blend_a64_d16_mask_w16_sse41(dst + j, src0 + j, src1 + j, &m0, &m1,
                                   round_offset, &v_maxval, shift);
    }
    mask += mask_stride;
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
  }
}

static inline void lowbd_blend_a64_d16_mask_subw0_subh1_w16_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift) {
  const __m128i v_maxval = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i zeros = _mm_setzero_si128();
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m_i00 = xx_loadu_128(mask + j);
      const __m128i m_i10 = xx_loadu_128(mask + mask_stride + j);

      const __m128i m_ac = _mm_avg_epu8(_mm_adds_epu8(m_i00, m_i10), zeros);
      const __m128i m0 = _mm_cvtepu8_epi16(m_ac);
      const __m128i m1 = _mm_cvtepu8_epi16(_mm_srli_si128(m_ac, 8));

      blend_a64_d16_mask_w16_sse41(dst + j, src0 + j, src1 + j, &m0, &m1,
                                   round_offset, &v_maxval, shift);
    }
    mask += mask_stride << 1;
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
  }
}

void aom_lowbd_blend_a64_d16_mask_sse4_1(
    uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh,
    ConvolveParams *conv_params) {
  const int bd = 8;
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;

  const int round_offset =
      ((1 << (round_bits + bd)) + (1 << (round_bits + bd - 1)) -
       (1 << (round_bits - 1)))
      << AOM_BLEND_A64_ROUND_BITS;

  const int shift = round_bits + AOM_BLEND_A64_ROUND_BITS;
  assert(IMPLIES((void *)src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES((void *)src1 == dst, src1_stride == dst_stride));

  assert(h >= 4);
  assert(w >= 4);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  const __m128i v_round_offset = _mm_set1_epi32(round_offset);

  if (subw == 0 && subh == 0) {
    switch (w) {
      case 4:
        aom_lowbd_blend_a64_d16_mask_subw0_subh0_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      case 8:
        aom_lowbd_blend_a64_d16_mask_subw0_subh0_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      default:
        lowbd_blend_a64_d16_mask_subw0_subh0_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift);
        break;
    }

  } else if (subw == 1 && subh == 1) {
    switch (w) {
      case 4:
        aom_lowbd_blend_a64_d16_mask_subw1_subh1_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      case 8:
        aom_lowbd_blend_a64_d16_mask_subw1_subh1_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      default:
        lowbd_blend_a64_d16_mask_subw1_subh1_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift);
        break;
    }
  } else if (subw == 1 && subh == 0) {
    switch (w) {
      case 4:
        aom_lowbd_blend_a64_d16_mask_subw1_subh0_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      case 8:
        aom_lowbd_blend_a64_d16_mask_subw1_subh0_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      default:
        lowbd_blend_a64_d16_mask_subw1_subh0_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift);
        break;
    }
  } else {
    switch (w) {
      case 4:
        aom_lowbd_blend_a64_d16_mask_subw0_subh1_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      case 8:
        aom_lowbd_blend_a64_d16_mask_subw0_subh1_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift);
        break;
      default:
        lowbd_blend_a64_d16_mask_subw0_subh1_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift);
        break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// aom_highbd_blend_a64_d16_mask_sse4_1()
//////////////////////////////////////////////////////////////////////////////
#if CONFIG_AV1_HIGHBITDEPTH
static inline void highbd_blend_a64_d16_mask_w4_sse4_1(
    uint16_t *dst, int dst_stride, const CONV_BUF_TYPE *src0, int src0_stride,
    const CONV_BUF_TYPE *src1, int src1_stride, const __m128i *mask0a,
    const __m128i *mask0b, const __m128i *round_offset, int shift,
    const __m128i *clip_low, const __m128i *clip_high,
    const __m128i *mask_max) {
  // Load 4 pixels from each of 4 rows from each source
  const __m128i s0a = xx_loadu_2x64(src0, src0 + src0_stride);
  const __m128i s0b =
      xx_loadu_2x64(src0 + 2 * src0_stride, src0 + 3 * src0_stride);
  const __m128i s1a = xx_loadu_2x64(src1, src1 + src1_stride);
  const __m128i s1b =
      xx_loadu_2x64(src1 + 2 * src1_stride, src1 + 3 * src1_stride);

  // Generate the inverse masks
  const __m128i mask1a = _mm_sub_epi16(*mask_max, *mask0a);
  const __m128i mask1b = _mm_sub_epi16(*mask_max, *mask0b);

  // Multiply each mask by the respective source
  const __m128i mul0a_highs = _mm_mulhi_epu16(*mask0a, s0a);
  const __m128i mul0a_lows = _mm_mullo_epi16(*mask0a, s0a);
  const __m128i mul0ah = _mm_unpackhi_epi16(mul0a_lows, mul0a_highs);
  const __m128i mul0al = _mm_unpacklo_epi16(mul0a_lows, mul0a_highs);
  const __m128i mul1a_highs = _mm_mulhi_epu16(mask1a, s1a);
  const __m128i mul1a_lows = _mm_mullo_epi16(mask1a, s1a);
  const __m128i mul1ah = _mm_unpackhi_epi16(mul1a_lows, mul1a_highs);
  const __m128i mul1al = _mm_unpacklo_epi16(mul1a_lows, mul1a_highs);

  const __m128i mul0b_highs = _mm_mulhi_epu16(*mask0b, s0b);
  const __m128i mul0b_lows = _mm_mullo_epi16(*mask0b, s0b);
  const __m128i mul0bh = _mm_unpackhi_epi16(mul0b_lows, mul0b_highs);
  const __m128i mul0bl = _mm_unpacklo_epi16(mul0b_lows, mul0b_highs);
  const __m128i mul1b_highs = _mm_mulhi_epu16(mask1b, s1b);
  const __m128i mul1b_lows = _mm_mullo_epi16(mask1b, s1b);
  const __m128i mul1bh = _mm_unpackhi_epi16(mul1b_lows, mul1b_highs);
  const __m128i mul1bl = _mm_unpacklo_epi16(mul1b_lows, mul1b_highs);

  const __m128i sumah = _mm_add_epi32(mul0ah, mul1ah);
  const __m128i sumal = _mm_add_epi32(mul0al, mul1al);
  const __m128i sumbh = _mm_add_epi32(mul0bh, mul1bh);
  const __m128i sumbl = _mm_add_epi32(mul0bl, mul1bl);

  const __m128i roundah =
      _mm_srai_epi32(_mm_sub_epi32(sumah, *round_offset), shift);
  const __m128i roundbh =
      _mm_srai_epi32(_mm_sub_epi32(sumbh, *round_offset), shift);
  const __m128i roundal =
      _mm_srai_epi32(_mm_sub_epi32(sumal, *round_offset), shift);
  const __m128i roundbl =
      _mm_srai_epi32(_mm_sub_epi32(sumbl, *round_offset), shift);

  const __m128i packa = _mm_packs_epi32(roundal, roundah);
  const __m128i packb = _mm_packs_epi32(roundbl, roundbh);

  const __m128i clipa =
      _mm_min_epi16(_mm_max_epi16(packa, *clip_low), *clip_high);
  const __m128i clipb =
      _mm_min_epi16(_mm_max_epi16(packb, *clip_low), *clip_high);

  xx_storel_64(dst, _mm_srli_si128(clipa, 8));
  xx_storel_64(dst + dst_stride, clipa);
  xx_storel_64(dst + 2 * dst_stride, _mm_srli_si128(clipb, 8));
  xx_storel_64(dst + 3 * dst_stride, clipb);
}

static inline void highbd_blend_a64_d16_mask_subw0_subh0_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h,
    const __m128i *round_offset, int shift, const __m128i *clip_low,
    const __m128i *clip_high, const __m128i *mask_max) {
  do {
    const __m128i mask0a8 =
        _mm_set_epi32(0, 0, *(int32_t *)mask, *(int32_t *)(mask + mask_stride));
    const __m128i mask0b8 =
        _mm_set_epi32(0, 0, *(int32_t *)(mask + 2 * mask_stride),
                      *(int32_t *)(mask + 3 * mask_stride));
    const __m128i mask0a = _mm_cvtepu8_epi16(mask0a8);
    const __m128i mask0b = _mm_cvtepu8_epi16(mask0b8);

    highbd_blend_a64_d16_mask_w4_sse4_1(
        dst, dst_stride, src0, src0_stride, src1, src1_stride, &mask0a, &mask0b,
        round_offset, shift, clip_low, clip_high, mask_max);

    dst += dst_stride * 4;
    src0 += src0_stride * 4;
    src1 += src1_stride * 4;
    mask += mask_stride * 4;
  } while (h -= 4);
}

static inline void highbd_blend_a64_d16_mask_subw1_subh1_w4_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h,
    const __m128i *round_offset, int shift, const __m128i *clip_low,
    const __m128i *clip_high, const __m128i *mask_max) {
  const __m128i one_b = _mm_set1_epi8(1);
  const __m128i two_w = _mm_set1_epi16(2);
  do {
    // Load 8 pixels from each of 8 rows of mask,
    // (saturating) add together rows then use madd to add adjacent pixels
    // Finally, divide each value by 4 (with rounding)
    const __m128i m02 = _mm_set_epi64x(*(int64_t *)(mask),
                                       *(int64_t *)(mask + 2 * mask_stride));
    const __m128i m13 = _mm_set_epi64x(*(int64_t *)(mask + mask_stride),
                                       *(int64_t *)(mask + 3 * mask_stride));
    const __m128i m0123 = _mm_maddubs_epi16(_mm_adds_epu8(m02, m13), one_b);
    const __m128i mask_0a = _mm_srli_epi16(_mm_add_epi16(m0123, two_w), 2);
    const __m128i m46 = _mm_set_epi64x(*(int64_t *)(mask + 4 * mask_stride),
                                       *(int64_t *)(mask + 6 * mask_stride));
    const __m128i m57 = _mm_set_epi64x(*(int64_t *)(mask + 5 * mask_stride),
                                       *(int64_t *)(mask + 7 * mask_stride));
    const __m128i m4567 = _mm_maddubs_epi16(_mm_adds_epu8(m46, m57), one_b);
    const __m128i mask_0b = _mm_srli_epi16(_mm_add_epi16(m4567, two_w), 2);

    highbd_blend_a64_d16_mask_w4_sse4_1(
        dst, dst_stride, src0, src0_stride, src1, src1_stride, &mask_0a,
        &mask_0b, round_offset, shift, clip_low, clip_high, mask_max);

    dst += dst_stride * 4;
    src0 += src0_stride * 4;
    src1 += src1_stride * 4;
    mask += mask_stride * 8;
  } while (h -= 4);
}

static inline void highbd_blend_a64_d16_mask_w8_sse4_1(
    uint16_t *dst, int dst_stride, const CONV_BUF_TYPE *src0, int src0_stride,
    const CONV_BUF_TYPE *src1, int src1_stride, const __m128i *mask0a,
    const __m128i *mask0b, const __m128i *round_offset, int shift,
    const __m128i *clip_low, const __m128i *clip_high,
    const __m128i *max_mask) {
  // Load 8x pixels from each of 2 rows from each source
  const __m128i s0a = xx_loadu_128(src0);
  const __m128i s0b = xx_loadu_128(src0 + src0_stride);
  const __m128i s1a = xx_loadu_128(src1);
  const __m128i s1b = xx_loadu_128(src1 + src1_stride);

  // Generate inverse masks
  const __m128i mask1a = _mm_sub_epi16(*max_mask, *mask0a);
  const __m128i mask1b = _mm_sub_epi16(*max_mask, *mask0b);

  // Multiply sources by respective masks
  const __m128i mul0a_highs = _mm_mulhi_epu16(*mask0a, s0a);
  const __m128i mul0a_lows = _mm_mullo_epi16(*mask0a, s0a);
  const __m128i mul0ah = _mm_unpackhi_epi16(mul0a_lows, mul0a_highs);
  const __m128i mul0al = _mm_unpacklo_epi16(mul0a_lows, mul0a_highs);

  const __m128i mul1a_highs = _mm_mulhi_epu16(mask1a, s1a);
  const __m128i mul1a_lows = _mm_mullo_epi16(mask1a, s1a);
  const __m128i mul1ah = _mm_unpackhi_epi16(mul1a_lows, mul1a_highs);
  const __m128i mul1al = _mm_unpacklo_epi16(mul1a_lows, mul1a_highs);

  const __m128i sumah = _mm_add_epi32(mul0ah, mul1ah);
  const __m128i sumal = _mm_add_epi32(mul0al, mul1al);

  const __m128i mul0b_highs = _mm_mulhi_epu16(*mask0b, s0b);
  const __m128i mul0b_lows = _mm_mullo_epi16(*mask0b, s0b);
  const __m128i mul0bh = _mm_unpackhi_epi16(mul0b_lows, mul0b_highs);
  const __m128i mul0bl = _mm_unpacklo_epi16(mul0b_lows, mul0b_highs);

  const __m128i mul1b_highs = _mm_mulhi_epu16(mask1b, s1b);
  const __m128i mul1b_lows = _mm_mullo_epi16(mask1b, s1b);
  const __m128i mul1bh = _mm_unpackhi_epi16(mul1b_lows, mul1b_highs);
  const __m128i mul1bl = _mm_unpacklo_epi16(mul1b_lows, mul1b_highs);

  const __m128i sumbh = _mm_add_epi32(mul0bh, mul1bh);
  const __m128i sumbl = _mm_add_epi32(mul0bl, mul1bl);

  const __m128i roundah =
      _mm_srai_epi32(_mm_sub_epi32(sumah, *round_offset), shift);
  const __m128i roundal =
      _mm_srai_epi32(_mm_sub_epi32(sumal, *round_offset), shift);
  const __m128i roundbh =
      _mm_srai_epi32(_mm_sub_epi32(sumbh, *round_offset), shift);
  const __m128i roundbl =
      _mm_srai_epi32(_mm_sub_epi32(sumbl, *round_offset), shift);

  const __m128i packa = _mm_packs_epi32(roundal, roundah);
  const __m128i clipa =
      _mm_min_epi16(_mm_max_epi16(packa, *clip_low), *clip_high);
  const __m128i packb = _mm_packs_epi32(roundbl, roundbh);
  const __m128i clipb =
      _mm_min_epi16(_mm_max_epi16(packb, *clip_low), *clip_high);

  xx_storeu_128(dst, clipa);
  xx_storeu_128(dst + dst_stride, clipb);
}

static inline void highbd_blend_a64_d16_mask_subw0_subh0_w8_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h,
    const __m128i *round_offset, int shift, const __m128i *clip_low,
    const __m128i *clip_high, const __m128i *max_mask) {
  do {
    const __m128i mask0a = _mm_cvtepu8_epi16(xx_loadl_64(mask));
    const __m128i mask0b = _mm_cvtepu8_epi16(xx_loadl_64(mask + mask_stride));
    highbd_blend_a64_d16_mask_w8_sse4_1(
        dst, dst_stride, src0, src0_stride, src1, src1_stride, &mask0a, &mask0b,
        round_offset, shift, clip_low, clip_high, max_mask);

    dst += dst_stride * 2;
    src0 += src0_stride * 2;
    src1 += src1_stride * 2;
    mask += mask_stride * 2;
  } while (h -= 2);
}

static inline void highbd_blend_a64_d16_mask_subw1_subh1_w8_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h,
    const __m128i *round_offset, int shift, const __m128i *clip_low,
    const __m128i *clip_high, const __m128i *max_mask) {
  const __m128i one_b = _mm_set1_epi8(1);
  const __m128i two_w = _mm_set1_epi16(2);
  do {
    const __m128i mask_thisrowa = xx_loadu_128(mask);
    const __m128i mask_nextrowa = xx_loadu_128(mask + mask_stride);
    const __m128i mask_thisrowb = xx_loadu_128(mask + 2 * mask_stride);
    const __m128i mask_nextrowb = xx_loadu_128(mask + 3 * mask_stride);
    const __m128i mask_bothrowsa = _mm_adds_epu8(mask_thisrowa, mask_nextrowa);
    const __m128i mask_bothrowsb = _mm_adds_epu8(mask_thisrowb, mask_nextrowb);
    const __m128i mask_16a = _mm_maddubs_epi16(mask_bothrowsa, one_b);
    const __m128i mask_16b = _mm_maddubs_epi16(mask_bothrowsb, one_b);
    const __m128i mask_sa = _mm_srli_epi16(_mm_add_epi16(mask_16a, two_w), 2);
    const __m128i mask_sb = _mm_srli_epi16(_mm_add_epi16(mask_16b, two_w), 2);

    highbd_blend_a64_d16_mask_w8_sse4_1(
        dst, dst_stride, src0, src0_stride, src1, src1_stride, &mask_sa,
        &mask_sb, round_offset, shift, clip_low, clip_high, max_mask);

    dst += dst_stride * 2;
    src0 += src0_stride * 2;
    src1 += src1_stride * 2;
    mask += mask_stride * 4;
  } while (h -= 2);
}

static inline void highbd_blend_a64_d16_mask_w16_sse4_1(
    uint16_t *dst, const CONV_BUF_TYPE *src0, const CONV_BUF_TYPE *src1,
    const __m128i *round_offset, int shift, const __m128i *mask0l,
    const __m128i *mask0h, const __m128i *clip_low, const __m128i *clip_high,
    const __m128i *mask_max) {
  // Load 16x u16 pixels for this row from each src
  const __m128i s0l = xx_loadu_128(src0);
  const __m128i s0h = xx_loadu_128(src0 + 8);
  const __m128i s1l = xx_loadu_128(src1);
  const __m128i s1h = xx_loadu_128(src1 + 8);

  // Calculate inverse masks
  const __m128i mask1h = _mm_sub_epi16(*mask_max, *mask0h);
  const __m128i mask1l = _mm_sub_epi16(*mask_max, *mask0l);

  const __m128i mul0_highs = _mm_mulhi_epu16(*mask0h, s0h);
  const __m128i mul0_lows = _mm_mullo_epi16(*mask0h, s0h);
  const __m128i mul0h = _mm_unpackhi_epi16(mul0_lows, mul0_highs);
  const __m128i mul0l = _mm_unpacklo_epi16(mul0_lows, mul0_highs);

  const __m128i mul1_highs = _mm_mulhi_epu16(mask1h, s1h);
  const __m128i mul1_lows = _mm_mullo_epi16(mask1h, s1h);
  const __m128i mul1h = _mm_unpackhi_epi16(mul1_lows, mul1_highs);
  const __m128i mul1l = _mm_unpacklo_epi16(mul1_lows, mul1_highs);

  const __m128i mulhh = _mm_add_epi32(mul0h, mul1h);
  const __m128i mulhl = _mm_add_epi32(mul0l, mul1l);

  const __m128i mul2_highs = _mm_mulhi_epu16(*mask0l, s0l);
  const __m128i mul2_lows = _mm_mullo_epi16(*mask0l, s0l);
  const __m128i mul2h = _mm_unpackhi_epi16(mul2_lows, mul2_highs);
  const __m128i mul2l = _mm_unpacklo_epi16(mul2_lows, mul2_highs);

  const __m128i mul3_highs = _mm_mulhi_epu16(mask1l, s1l);
  const __m128i mul3_lows = _mm_mullo_epi16(mask1l, s1l);
  const __m128i mul3h = _mm_unpackhi_epi16(mul3_lows, mul3_highs);
  const __m128i mul3l = _mm_unpacklo_epi16(mul3_lows, mul3_highs);

  const __m128i mullh = _mm_add_epi32(mul2h, mul3h);
  const __m128i mulll = _mm_add_epi32(mul2l, mul3l);

  const __m128i reshh =
      _mm_srai_epi32(_mm_sub_epi32(mulhh, *round_offset), shift);
  const __m128i reshl =
      _mm_srai_epi32(_mm_sub_epi32(mulhl, *round_offset), shift);
  const __m128i reslh =
      _mm_srai_epi32(_mm_sub_epi32(mullh, *round_offset), shift);
  const __m128i resll =
      _mm_srai_epi32(_mm_sub_epi32(mulll, *round_offset), shift);

  // Signed saturating pack from i32 to i16:
  const __m128i packh = _mm_packs_epi32(reshl, reshh);
  const __m128i packl = _mm_packs_epi32(resll, reslh);

  // Clip the values to the valid range
  const __m128i cliph =
      _mm_min_epi16(_mm_max_epi16(packh, *clip_low), *clip_high);
  const __m128i clipl =
      _mm_min_epi16(_mm_max_epi16(packl, *clip_low), *clip_high);

  // Store 16 pixels
  xx_storeu_128(dst, clipl);
  xx_storeu_128(dst + 8, cliph);
}

static inline void highbd_blend_a64_d16_mask_subw0_subh0_w16_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift, const __m128i *clip_low,
    const __m128i *clip_high, const __m128i *mask_max) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j += 16) {
      // Load 16x u8 alpha-mask values and pad to u16
      const __m128i masks_u8 = xx_loadu_128(mask + j);
      const __m128i mask0l = _mm_cvtepu8_epi16(masks_u8);
      const __m128i mask0h = _mm_cvtepu8_epi16(_mm_srli_si128(masks_u8, 8));

      highbd_blend_a64_d16_mask_w16_sse4_1(
          dst + j, src0 + j, src1 + j, round_offset, shift, &mask0l, &mask0h,
          clip_low, clip_high, mask_max);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride;
  }
}

static inline void highbd_blend_a64_d16_mask_subw1_subh1_w16_sse4_1(
    uint16_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int h, int w,
    const __m128i *round_offset, int shift, const __m128i *clip_low,
    const __m128i *clip_high, const __m128i *mask_max) {
  const __m128i one_b = _mm_set1_epi8(1);
  const __m128i two_w = _mm_set1_epi16(2);
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j += 16) {
      const __m128i m_i00 = xx_loadu_128(mask + 2 * j);
      const __m128i m_i01 = xx_loadu_128(mask + 2 * j + 16);
      const __m128i m_i10 = xx_loadu_128(mask + mask_stride + 2 * j);
      const __m128i m_i11 = xx_loadu_128(mask + mask_stride + 2 * j + 16);

      const __m128i m0_ac = _mm_adds_epu8(m_i00, m_i10);
      const __m128i m1_ac = _mm_adds_epu8(m_i01, m_i11);
      const __m128i m0_acbd = _mm_maddubs_epi16(m0_ac, one_b);
      const __m128i m1_acbd = _mm_maddubs_epi16(m1_ac, one_b);
      const __m128i mask_l = _mm_srli_epi16(_mm_add_epi16(m0_acbd, two_w), 2);
      const __m128i mask_h = _mm_srli_epi16(_mm_add_epi16(m1_acbd, two_w), 2);

      highbd_blend_a64_d16_mask_w16_sse4_1(
          dst + j, src0 + j, src1 + j, round_offset, shift, &mask_l, &mask_h,
          clip_low, clip_high, mask_max);
    }
    dst += dst_stride;
    src0 += src0_stride;
    src1 += src1_stride;
    mask += mask_stride * 2;
  }
}

void aom_highbd_blend_a64_d16_mask_sse4_1(
    uint8_t *dst8, uint32_t dst_stride, const CONV_BUF_TYPE *src0,
    uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride,
    const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh,
    ConvolveParams *conv_params, const int bd) {
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int32_t round_offset =
      ((1 << (round_bits + bd)) + (1 << (round_bits + bd - 1)) -
       (1 << (round_bits - 1)))
      << AOM_BLEND_A64_ROUND_BITS;
  const __m128i v_round_offset = _mm_set1_epi32(round_offset);
  const int shift = round_bits + AOM_BLEND_A64_ROUND_BITS;

  const __m128i clip_low = _mm_setzero_si128();
  const __m128i clip_high = _mm_set1_epi16((1 << bd) - 1);
  const __m128i mask_max = _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);

  assert(IMPLIES((void *)src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES((void *)src1 == dst, src1_stride == dst_stride));

  assert(h >= 4);
  assert(w >= 4);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));

  if (subw == 0 && subh == 0) {
    switch (w) {
      case 4:
        highbd_blend_a64_d16_mask_subw0_subh0_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift, &clip_low, &clip_high,
            &mask_max);
        break;
      case 8:
        highbd_blend_a64_d16_mask_subw0_subh0_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift, &clip_low, &clip_high,
            &mask_max);
        break;
      default:  // >=16
        highbd_blend_a64_d16_mask_subw0_subh0_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift, &clip_low, &clip_high,
            &mask_max);
        break;
    }

  } else if (subw == 1 && subh == 1) {
    switch (w) {
      case 4:
        highbd_blend_a64_d16_mask_subw1_subh1_w4_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift, &clip_low, &clip_high,
            &mask_max);
        break;
      case 8:
        highbd_blend_a64_d16_mask_subw1_subh1_w8_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, &v_round_offset, shift, &clip_low, &clip_high,
            &mask_max);
        break;
      default:  // >=16
        highbd_blend_a64_d16_mask_subw1_subh1_w16_sse4_1(
            dst, dst_stride, src0, src0_stride, src1, src1_stride, mask,
            mask_stride, h, w, &v_round_offset, shift, &clip_low, &clip_high,
            &mask_max);
        break;
    }
  } else {
    // Sub-sampling in only one axis doesn't seem to happen very much, so fall
    // back to the vanilla C implementation instead of having all the optimised
    // code for these.
    aom_highbd_blend_a64_d16_mask_c(dst8, dst_stride, src0, src0_stride, src1,
                                    src1_stride, mask, mask_stride, w, h, subw,
                                    subh, conv_params, bd);
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
