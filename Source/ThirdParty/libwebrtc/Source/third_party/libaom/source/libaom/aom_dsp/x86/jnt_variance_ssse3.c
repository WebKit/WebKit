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
#include <emmintrin.h>  // SSE2
#include <tmmintrin.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/synonyms.h"
#include "aom_dsp/x86/variance_impl_ssse3.h"

static inline void compute_dist_wtd_avg(__m128i *p0, __m128i *p1,
                                        const __m128i *w, const __m128i *r,
                                        void *const result) {
  __m128i p_lo = _mm_unpacklo_epi8(*p0, *p1);
  __m128i mult_lo = _mm_maddubs_epi16(p_lo, *w);
  __m128i round_lo = _mm_add_epi16(mult_lo, *r);
  __m128i shift_lo = _mm_srai_epi16(round_lo, DIST_PRECISION_BITS);

  __m128i p_hi = _mm_unpackhi_epi8(*p0, *p1);
  __m128i mult_hi = _mm_maddubs_epi16(p_hi, *w);
  __m128i round_hi = _mm_add_epi16(mult_hi, *r);
  __m128i shift_hi = _mm_srai_epi16(round_hi, DIST_PRECISION_BITS);

  xx_storeu_128(result, _mm_packus_epi16(shift_lo, shift_hi));
}

void aom_dist_wtd_comp_avg_pred_ssse3(uint8_t *comp_pred, const uint8_t *pred,
                                      int width, int height, const uint8_t *ref,
                                      int ref_stride,
                                      const DIST_WTD_COMP_PARAMS *jcp_param) {
  int i;
  const int8_t w0 = (int8_t)jcp_param->fwd_offset;
  const int8_t w1 = (int8_t)jcp_param->bck_offset;
  const __m128i w = _mm_set_epi8(w1, w0, w1, w0, w1, w0, w1, w0, w1, w0, w1, w0,
                                 w1, w0, w1, w0);
  const int16_t round = (int16_t)((1 << DIST_PRECISION_BITS) >> 1);
  const __m128i r = _mm_set1_epi16(round);

  if (width >= 16) {
    // Read 16 pixels one row at a time
    assert(!(width & 15));
    for (i = 0; i < height; ++i) {
      int j;
      for (j = 0; j < width; j += 16) {
        __m128i p0 = xx_loadu_128(ref);
        __m128i p1 = xx_loadu_128(pred);

        compute_dist_wtd_avg(&p0, &p1, &w, &r, comp_pred);

        comp_pred += 16;
        pred += 16;
        ref += 16;
      }
      ref += ref_stride - width;
    }
  } else if (width >= 8) {
    // Read 8 pixels two row at a time
    assert(!(width & 7));
    assert(!(width & 1));
    for (i = 0; i < height; i += 2) {
      __m128i p0_0 = xx_loadl_64(ref + 0 * ref_stride);
      __m128i p0_1 = xx_loadl_64(ref + 1 * ref_stride);
      __m128i p0 = _mm_unpacklo_epi64(p0_0, p0_1);
      __m128i p1 = xx_loadu_128(pred);

      compute_dist_wtd_avg(&p0, &p1, &w, &r, comp_pred);

      comp_pred += 16;
      pred += 16;
      ref += 2 * ref_stride;
    }
  } else {
    // Read 4 pixels four row at a time
    assert(!(width & 3));
    assert(!(height & 3));
    for (i = 0; i < height; i += 4) {
      const int8_t *row0 = (const int8_t *)ref + 0 * ref_stride;
      const int8_t *row1 = (const int8_t *)ref + 1 * ref_stride;
      const int8_t *row2 = (const int8_t *)ref + 2 * ref_stride;
      const int8_t *row3 = (const int8_t *)ref + 3 * ref_stride;

      __m128i p0 =
          _mm_setr_epi8(row0[0], row0[1], row0[2], row0[3], row1[0], row1[1],
                        row1[2], row1[3], row2[0], row2[1], row2[2], row2[3],
                        row3[0], row3[1], row3[2], row3[3]);
      __m128i p1 = xx_loadu_128(pred);

      compute_dist_wtd_avg(&p0, &p1, &w, &r, comp_pred);

      comp_pred += 16;
      pred += 16;
      ref += 4 * ref_stride;
    }
  }
}

#define DIST_WTD_SUBPIX_AVG_VAR(W, H)                                      \
  uint32_t aom_dist_wtd_sub_pixel_avg_variance##W##x##H##_ssse3(           \
      const uint8_t *a, int a_stride, int xoffset, int yoffset,            \
      const uint8_t *b, int b_stride, uint32_t *sse,                       \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) { \
    uint16_t fdata3[(H + 1) * W];                                          \
    uint8_t temp2[H * W];                                                  \
    DECLARE_ALIGNED(16, uint8_t, temp3[H * W]);                            \
                                                                           \
    aom_var_filter_block2d_bil_first_pass_ssse3(                           \
        a, fdata3, a_stride, 1, H + 1, W, bilinear_filters_2t[xoffset]);   \
    aom_var_filter_block2d_bil_second_pass_ssse3(                          \
        fdata3, temp2, W, W, H, W, bilinear_filters_2t[yoffset]);          \
                                                                           \
    aom_dist_wtd_comp_avg_pred_ssse3(temp3, second_pred, W, H, temp2, W,   \
                                     jcp_param);                           \
                                                                           \
    return aom_variance##W##x##H(temp3, W, b, b_stride, sse);              \
  }

DIST_WTD_SUBPIX_AVG_VAR(128, 128)
DIST_WTD_SUBPIX_AVG_VAR(128, 64)
DIST_WTD_SUBPIX_AVG_VAR(64, 128)
DIST_WTD_SUBPIX_AVG_VAR(64, 64)
DIST_WTD_SUBPIX_AVG_VAR(64, 32)
DIST_WTD_SUBPIX_AVG_VAR(32, 64)
DIST_WTD_SUBPIX_AVG_VAR(32, 32)
DIST_WTD_SUBPIX_AVG_VAR(32, 16)
DIST_WTD_SUBPIX_AVG_VAR(16, 32)
DIST_WTD_SUBPIX_AVG_VAR(16, 16)
DIST_WTD_SUBPIX_AVG_VAR(16, 8)
DIST_WTD_SUBPIX_AVG_VAR(8, 16)
DIST_WTD_SUBPIX_AVG_VAR(8, 8)
DIST_WTD_SUBPIX_AVG_VAR(8, 4)
DIST_WTD_SUBPIX_AVG_VAR(4, 8)
DIST_WTD_SUBPIX_AVG_VAR(4, 4)

#if !CONFIG_REALTIME_ONLY
DIST_WTD_SUBPIX_AVG_VAR(4, 16)
DIST_WTD_SUBPIX_AVG_VAR(16, 4)
DIST_WTD_SUBPIX_AVG_VAR(8, 32)
DIST_WTD_SUBPIX_AVG_VAR(32, 8)
DIST_WTD_SUBPIX_AVG_VAR(16, 64)
DIST_WTD_SUBPIX_AVG_VAR(64, 16)
#endif
