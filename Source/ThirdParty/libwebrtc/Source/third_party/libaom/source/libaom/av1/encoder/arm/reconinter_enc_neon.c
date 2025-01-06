/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/arm/mem_neon.h"

#include "av1/encoder/reconinter_enc.h"

void aom_upsampled_pred_neon(MACROBLOCKD *xd, const AV1_COMMON *const cm,
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

  const InterpFilterParams *filter_params = av1_get_filter(subpel_search);

  if (!subpel_x_q3 && !subpel_y_q3) {
    if (width > 8) {
      assert(width % 16 == 0);
      int i = height;
      do {
        int j = 0;
        do {
          uint8x16_t r = vld1q_u8(ref + j);
          vst1q_u8(comp_pred + j, r);
          j += 16;
        } while (j < width);
        ref += ref_stride;
        comp_pred += width;
      } while (--i != 0);
    } else if (width == 8) {
      int i = height;
      do {
        uint8x8_t r = vld1_u8(ref);
        vst1_u8(comp_pred, r);
        ref += ref_stride;
        comp_pred += width;
      } while (--i != 0);
    } else {
      assert(width == 4);
      int i = height / 2;
      do {
        uint8x8_t r = load_unaligned_u8(ref, ref_stride);
        vst1_u8(comp_pred, r);
        ref += 2 * ref_stride;
        comp_pred += 2 * width;
      } while (--i != 0);
    }
  } else if (!subpel_y_q3) {
    const int16_t *const filter_x =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q3 << 1);
    aom_convolve8_horiz(ref, ref_stride, comp_pred, width, filter_x, 16, NULL,
                        -1, width, height);
  } else if (!subpel_x_q3) {
    const int16_t *const filter_y =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q3 << 1);
    aom_convolve8_vert(ref, ref_stride, comp_pred, width, NULL, -1, filter_y,
                       16, width, height);
  } else {
    DECLARE_ALIGNED(16, uint8_t,
                    im_block[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);

    const int16_t *const filter_x =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_x_q3 << 1);
    const int16_t *const filter_y =
        av1_get_interp_filter_subpel_kernel(filter_params, subpel_y_q3 << 1);

    const int im_stride = MAX_SB_SIZE;
    const int im_height = (((height - 1) * 8 + subpel_y_q3) >> 3) + SUBPEL_TAPS;

    const int ref_vert_offset = ref_stride * ((SUBPEL_TAPS >> 1) - 1);
    const int im_vert_offset = im_stride * ((filter_params->taps >> 1) - 1);

    assert(im_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_convolve8_horiz(ref - ref_vert_offset, ref_stride, im_block,
                        MAX_SB_SIZE, filter_x, 16, NULL, -1, width, im_height);
    aom_convolve8_vert(im_block + im_vert_offset, MAX_SB_SIZE, comp_pred, width,
                       NULL, -1, filter_y, 16, width, height);
  }
}

void aom_comp_avg_upsampled_pred_neon(MACROBLOCKD *xd,
                                      const AV1_COMMON *const cm, int mi_row,
                                      int mi_col, const MV *const mv,
                                      uint8_t *comp_pred, const uint8_t *pred,
                                      int width, int height, int subpel_x_q3,
                                      int subpel_y_q3, const uint8_t *ref,
                                      int ref_stride, int subpel_search) {
  aom_upsampled_pred_neon(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                          subpel_x_q3, subpel_y_q3, ref, ref_stride,
                          subpel_search);

  aom_comp_avg_pred_neon(comp_pred, pred, width, height, comp_pred, width);
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_upsampled_pred_neon(MACROBLOCKD *xd,
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

  if (!subpel_x_q3 && !subpel_y_q3) {
    const uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
    uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
    if (width > 4) {
      assert(width % 8 == 0);
      int i = height;
      do {
        int j = 0;
        do {
          uint16x8_t r = vld1q_u16(ref + j);
          vst1q_u16(comp_pred + j, r);
          j += 8;
        } while (j < width);
        ref += ref_stride;
        comp_pred += width;
      } while (--i != 0);
    } else if (width == 4) {
      int i = height;
      do {
        uint16x4_t r = vld1_u16(ref);
        vst1_u16(comp_pred, r);
        ref += ref_stride;
        comp_pred += width;
      } while (--i != 0);
    } else {
      assert(width == 2);
      int i = height / 2;
      do {
        uint16x4_t r = load_u16_2x2(ref, ref_stride);
        store_u16x2_strided_x2(comp_pred, width, r);
        ref += 2 * ref_stride;
        comp_pred += 2 * width;
      } while (--i != 0);
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_highbd_convolve8_horiz_neon(ref8, ref_stride, comp_pred8, width, kernel,
                                    16, NULL, -1, width, height, bd);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_highbd_convolve8_vert_neon(ref8, ref_stride, comp_pred8, width, NULL,
                                   -1, kernel, 16, width, height, bd);
  } else {
    DECLARE_ALIGNED(16, uint16_t,
                    temp[((MAX_SB_SIZE + 16) + 16) * MAX_SB_SIZE]);
    const int16_t *const kernel_x =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    const int16_t *const kernel_y =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    const int intermediate_height =
        (((height - 1) * 8 + subpel_y_q3) >> 3) + filter->taps;
    assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_highbd_convolve8_horiz_neon(
        ref8 - ref_stride * ((filter->taps >> 1) - 1), ref_stride,
        CONVERT_TO_BYTEPTR(temp), MAX_SB_SIZE, kernel_x, 16, NULL, -1, width,
        intermediate_height, bd);
    aom_highbd_convolve8_vert_neon(
        CONVERT_TO_BYTEPTR(temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1)),
        MAX_SB_SIZE, comp_pred8, width, NULL, -1, kernel_y, 16, width, height,
        bd);
  }
}

void aom_highbd_comp_avg_upsampled_pred_neon(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, int subpel_search) {
  aom_highbd_upsampled_pred_neon(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                                 height, subpel_x_q3, subpel_y_q3, ref8,
                                 ref_stride, bd, subpel_search);

  aom_highbd_comp_avg_pred_neon(comp_pred8, pred8, width, height, comp_pred8,
                                width);
}

#endif  // CONFIG_AV1_HIGHBITDEPTH
