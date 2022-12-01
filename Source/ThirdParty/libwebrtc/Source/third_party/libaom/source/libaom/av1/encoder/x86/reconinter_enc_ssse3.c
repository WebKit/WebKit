/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
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
#include "config/av1_rtcd.h"

#include "aom_dsp/x86/synonyms.h"

static INLINE void compute_dist_wtd_avg(__m128i *p0, __m128i *p1,
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

void aom_dist_wtd_comp_avg_upsampled_pred_ssse3(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride, const DIST_WTD_COMP_PARAMS *jcp_param, int subpel_search) {
  int n;
  int i;
  aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                     subpel_x_q3, subpel_y_q3, ref, ref_stride, subpel_search);
  /*The total number of pixels must be a multiple of 16 (e.g., 4x4).*/
  assert(!(width * height & 15));
  n = width * height >> 4;

  const uint8_t w0 = (uint8_t)jcp_param->fwd_offset;
  const uint8_t w1 = (uint8_t)jcp_param->bck_offset;
  const __m128i w = _mm_set_epi8(w1, w0, w1, w0, w1, w0, w1, w0, w1, w0, w1, w0,
                                 w1, w0, w1, w0);
  const uint16_t round = ((1 << DIST_PRECISION_BITS) >> 1);
  const __m128i r =
      _mm_set_epi16(round, round, round, round, round, round, round, round);

  for (i = 0; i < n; i++) {
    __m128i p0 = xx_loadu_128(comp_pred);
    __m128i p1 = xx_loadu_128(pred);

    compute_dist_wtd_avg(&p0, &p1, &w, &r, comp_pred);

    comp_pred += 16;
    pred += 16;
  }
}
