/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
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

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/synonyms.h"

#include "av1/common/av1_common_int.h"
#include "av1/common/blockd.h"
#include "av1/common/mvref_common.h"
#include "av1/common/obmc.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/encoder/reconinter_enc.h"

void aom_upsampled_pred_sse2(MACROBLOCKD *xd, const struct AV1Common *const cm,
                             int mi_row, int mi_col, const MV *const mv,
                             uint8_t *comp_pred, int width, int height,
                             int subpel_x_q3, int subpel_y_q3,
                             const uint8_t *ref, int ref_stride,
                             int subpel_search) {
  // expect xd == NULL only in tests
  if (xd != NULL) {
    const MB_MODE_INFO *mi = xd->mi[0];
    const int ref_num = 0;
    const int is_intrabc = is_intrabc_block(mi);
    const struct scale_factors *const sf =
        is_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref_num];
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      int plane = 0;
      const int mi_x = mi_col * MI_SIZE;
      const int mi_y = mi_row * MI_SIZE;
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const struct buf_2d *const dst_buf = &pd->dst;
      const struct buf_2d *const pre_buf =
          is_intrabc ? dst_buf : &pd->pre[ref_num];

      InterPredParams inter_pred_params;
      inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);
      const int_interpfilters filters =
          av1_broadcast_interp_filter(EIGHTTAP_REGULAR);
      av1_init_inter_params(
          &inter_pred_params, width, height, mi_y >> pd->subsampling_y,
          mi_x >> pd->subsampling_x, pd->subsampling_x, pd->subsampling_y,
          xd->bd, is_cur_buf_hbd(xd), is_intrabc, sf, pre_buf, filters);
      av1_enc_build_one_inter_predictor(comp_pred, width, mv,
                                        &inter_pred_params);
      return;
    }
  }

  const InterpFilterParams *filter = av1_get_filter(subpel_search);
  // (TODO:yunqing) 2-tap case uses 4-tap functions since there is no SIMD for
  // 2-tap yet.
  int filter_taps = (subpel_search <= USE_4_TAPS) ? 4 : SUBPEL_TAPS;

  if (!subpel_x_q3 && !subpel_y_q3) {
    if (width >= 16) {
      int i;
      assert(!(width & 15));
      /*Read 16 pixels one row at a time.*/
      for (i = 0; i < height; i++) {
        int j;
        for (j = 0; j < width; j += 16) {
          xx_storeu_128(comp_pred, xx_loadu_128(ref));
          comp_pred += 16;
          ref += 16;
        }
        ref += ref_stride - width;
      }
    } else if (width >= 8) {
      int i;
      assert(!(width & 7));
      assert(!(height & 1));
      /*Read 8 pixels two rows at a time.*/
      for (i = 0; i < height; i += 2) {
        __m128i s0 = xx_loadl_64(ref + 0 * ref_stride);
        __m128i s1 = xx_loadl_64(ref + 1 * ref_stride);
        xx_storeu_128(comp_pred, _mm_unpacklo_epi64(s0, s1));
        comp_pred += 16;
        ref += 2 * ref_stride;
      }
    } else {
      int i;
      assert(!(width & 3));
      assert(!(height & 3));
      /*Read 4 pixels four rows at a time.*/
      for (i = 0; i < height; i++) {
        const __m128i row0 = xx_loadl_64(ref + 0 * ref_stride);
        const __m128i row1 = xx_loadl_64(ref + 1 * ref_stride);
        const __m128i row2 = xx_loadl_64(ref + 2 * ref_stride);
        const __m128i row3 = xx_loadl_64(ref + 3 * ref_stride);
        const __m128i reg = _mm_unpacklo_epi64(_mm_unpacklo_epi32(row0, row1),
                                               _mm_unpacklo_epi32(row2, row3));
        xx_storeu_128(comp_pred, reg);
        comp_pred += 16;
        ref += 4 * ref_stride;
      }
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_convolve8_horiz(ref, ref_stride, comp_pred, width, kernel, 16, NULL, -1,
                        width, height);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_convolve8_vert(ref, ref_stride, comp_pred, width, NULL, -1, kernel, 16,
                       width, height);
  } else {
    DECLARE_ALIGNED(16, uint8_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    const int16_t *const kernel_x =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    const int16_t *const kernel_y =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    const uint8_t *ref_start = ref - ref_stride * ((filter_taps >> 1) - 1);
    uint8_t *temp_start_horiz = (subpel_search <= USE_4_TAPS)
                                    ? temp + (filter_taps >> 1) * MAX_SB_SIZE
                                    : temp;
    uint8_t *temp_start_vert = temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1);
    int intermediate_height =
        (((height - 1) * 8 + subpel_y_q3) >> 3) + filter_taps;
    assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_convolve8_horiz(ref_start, ref_stride, temp_start_horiz, MAX_SB_SIZE,
                        kernel_x, 16, NULL, -1, width, intermediate_height);
    aom_convolve8_vert(temp_start_vert, MAX_SB_SIZE, comp_pred, width, NULL, -1,
                       kernel_y, 16, width, height);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_upsampled_pred_sse2(MACROBLOCKD *xd,
                                    const struct AV1Common *const cm,
                                    int mi_row, int mi_col, const MV *const mv,
                                    uint8_t *comp_pred8, int width, int height,
                                    int subpel_x_q3, int subpel_y_q3,
                                    const uint8_t *ref8, int ref_stride, int bd,
                                    int subpel_search) {
  // expect xd == NULL only in tests
  if (xd != NULL) {
    const MB_MODE_INFO *mi = xd->mi[0];
    const int ref_num = 0;
    const int is_intrabc = is_intrabc_block(mi);
    const struct scale_factors *const sf =
        is_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref_num];
    const int is_scaled = av1_is_scaled(sf);

    if (is_scaled) {
      int plane = 0;
      const int mi_x = mi_col * MI_SIZE;
      const int mi_y = mi_row * MI_SIZE;
      const struct macroblockd_plane *const pd = &xd->plane[plane];
      const struct buf_2d *const dst_buf = &pd->dst;
      const struct buf_2d *const pre_buf =
          is_intrabc ? dst_buf : &pd->pre[ref_num];

      InterPredParams inter_pred_params;
      inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);
      const int_interpfilters filters =
          av1_broadcast_interp_filter(EIGHTTAP_REGULAR);
      av1_init_inter_params(
          &inter_pred_params, width, height, mi_y >> pd->subsampling_y,
          mi_x >> pd->subsampling_x, pd->subsampling_x, pd->subsampling_y,
          xd->bd, is_cur_buf_hbd(xd), is_intrabc, sf, pre_buf, filters);
      av1_enc_build_one_inter_predictor(comp_pred8, width, mv,
                                        &inter_pred_params);
      return;
    }
  }

  const InterpFilterParams *filter = av1_get_filter(subpel_search);
  int filter_taps = (subpel_search <= USE_4_TAPS) ? 4 : SUBPEL_TAPS;
  if (!subpel_x_q3 && !subpel_y_q3) {
    uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
    uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
    if (width >= 8) {
      int i;
      assert(!(width & 7));
      /*Read 8 pixels one row at a time.*/
      for (i = 0; i < height; i++) {
        int j;
        for (j = 0; j < width; j += 8) {
          __m128i s0 = _mm_loadu_si128((const __m128i *)ref);
          _mm_storeu_si128((__m128i *)comp_pred, s0);
          comp_pred += 8;
          ref += 8;
        }
        ref += ref_stride - width;
      }
    } else {
      int i;
      assert(!(width & 3));
      /*Read 4 pixels two rows at a time.*/
      for (i = 0; i < height; i += 2) {
        __m128i s0 = _mm_loadl_epi64((const __m128i *)ref);
        __m128i s1 = _mm_loadl_epi64((const __m128i *)(ref + ref_stride));
        __m128i t0 = _mm_unpacklo_epi64(s0, s1);
        _mm_storeu_si128((__m128i *)comp_pred, t0);
        comp_pred += 8;
        ref += 2 * ref_stride;
      }
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_highbd_convolve8_horiz(ref8, ref_stride, comp_pred8, width, kernel, 16,
                               NULL, -1, width, height, bd);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_highbd_convolve8_vert(ref8, ref_stride, comp_pred8, width, NULL, -1,
                              kernel, 16, width, height, bd);
  } else {
    DECLARE_ALIGNED(16, uint16_t,
                    temp[((MAX_SB_SIZE + 16) + 16) * MAX_SB_SIZE]);
    const int16_t *const kernel_x =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    const int16_t *const kernel_y =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    const uint8_t *ref_start = ref8 - ref_stride * ((filter_taps >> 1) - 1);
    uint16_t *temp_start_horiz = (subpel_search <= USE_4_TAPS)
                                     ? temp + (filter_taps >> 1) * MAX_SB_SIZE
                                     : temp;
    uint16_t *temp_start_vert = temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1);
    const int intermediate_height =
        (((height - 1) * 8 + subpel_y_q3) >> 3) + filter_taps;
    assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_highbd_convolve8_horiz(
        ref_start, ref_stride, CONVERT_TO_BYTEPTR(temp_start_horiz),
        MAX_SB_SIZE, kernel_x, 16, NULL, -1, width, intermediate_height, bd);
    aom_highbd_convolve8_vert(CONVERT_TO_BYTEPTR(temp_start_vert), MAX_SB_SIZE,
                              comp_pred8, width, NULL, -1, kernel_y, 16, width,
                              height, bd);
  }
}

void aom_highbd_comp_avg_upsampled_pred_sse2(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, int subpel_search) {
  aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                            height, subpel_x_q3, subpel_y_q3, ref8, ref_stride,
                            bd, subpel_search);
  uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *comp_pred16 = CONVERT_TO_SHORTPTR(comp_pred8);
  /*The total number of pixels must be a multiple of 8 (e.g., 4x4).*/
  assert(!(width * height & 7));
  int n = width * height >> 3;
  for (int i = 0; i < n; i++) {
    __m128i s0 = _mm_loadu_si128((const __m128i *)comp_pred16);
    __m128i p0 = _mm_loadu_si128((const __m128i *)pred);
    _mm_storeu_si128((__m128i *)comp_pred16, _mm_avg_epu16(s0, p0));
    comp_pred16 += 8;
    pred += 8;
  }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void aom_comp_avg_upsampled_pred_sse2(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride, int subpel_search) {
  int n;
  int i;
  aom_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                     subpel_x_q3, subpel_y_q3, ref, ref_stride, subpel_search);
  /*The total number of pixels must be a multiple of 16 (e.g., 4x4).*/
  assert(!(width * height & 15));
  n = width * height >> 4;
  for (i = 0; i < n; i++) {
    __m128i s0 = xx_loadu_128(comp_pred);
    __m128i p0 = xx_loadu_128(pred);
    xx_storeu_128(comp_pred, _mm_avg_epu8(s0, p0));
    comp_pred += 16;
    pred += 16;
  }
}
