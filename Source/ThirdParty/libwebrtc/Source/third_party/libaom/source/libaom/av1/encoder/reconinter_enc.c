/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <stdio.h>
#include <limits.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"

#include "av1/common/av1_common_int.h"
#include "av1/common/blockd.h"
#include "av1/common/mvref_common.h"
#include "av1/common/obmc.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/encoder/reconinter_enc.h"

static void enc_calc_subpel_params(const MV *const src_mv,
                                   InterPredParams *const inter_pred_params,
                                   MACROBLOCKD *xd, int mi_x, int mi_y, int ref,
                                   uint8_t **mc_buf, uint8_t **pre,
                                   SubpelParams *subpel_params,
                                   int *src_stride) {
  // These are part of the function signature to use this function through a
  // function pointer. See typedef of 'CalcSubpelParamsFunc'.
  (void)xd;
  (void)mi_x;
  (void)mi_y;
  (void)ref;
  (void)mc_buf;

  const struct scale_factors *sf = inter_pred_params->scale_factors;

  struct buf_2d *pre_buf = &inter_pred_params->ref_frame_buf;
  int ssx = inter_pred_params->subsampling_x;
  int ssy = inter_pred_params->subsampling_y;
  int orig_pos_y = inter_pred_params->pix_row << SUBPEL_BITS;
  orig_pos_y += src_mv->row * (1 << (1 - ssy));
  int orig_pos_x = inter_pred_params->pix_col << SUBPEL_BITS;
  orig_pos_x += src_mv->col * (1 << (1 - ssx));
  int pos_y = sf->scale_value_y(orig_pos_y, sf);
  int pos_x = sf->scale_value_x(orig_pos_x, sf);
  pos_x += SCALE_EXTRA_OFF;
  pos_y += SCALE_EXTRA_OFF;

  const int top = -AOM_LEFT_TOP_MARGIN_SCALED(ssy);
  const int left = -AOM_LEFT_TOP_MARGIN_SCALED(ssx);
  const int bottom = (pre_buf->height + AOM_INTERP_EXTEND) << SCALE_SUBPEL_BITS;
  const int right = (pre_buf->width + AOM_INTERP_EXTEND) << SCALE_SUBPEL_BITS;
  pos_y = clamp(pos_y, top, bottom);
  pos_x = clamp(pos_x, left, right);

  subpel_params->subpel_x = pos_x & SCALE_SUBPEL_MASK;
  subpel_params->subpel_y = pos_y & SCALE_SUBPEL_MASK;
  subpel_params->xs = sf->x_step_q4;
  subpel_params->ys = sf->y_step_q4;
  *pre = pre_buf->buf0 + (pos_y >> SCALE_SUBPEL_BITS) * pre_buf->stride +
         (pos_x >> SCALE_SUBPEL_BITS);
  *src_stride = pre_buf->stride;
}

void av1_enc_build_one_inter_predictor(uint8_t *dst, int dst_stride,
                                       const MV *src_mv,
                                       InterPredParams *inter_pred_params) {
  av1_build_one_inter_predictor(
      dst, dst_stride, src_mv, inter_pred_params, NULL /* xd */, 0 /* mi_x */,
      0 /* mi_y */, inter_pred_params->conv_params.do_average /* ref */,
      NULL /* mc_buf */, enc_calc_subpel_params);
}

static void enc_build_inter_predictors(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                       int plane, const MB_MODE_INFO *mi,
                                       int bw, int bh, int mi_x, int mi_y) {
  av1_build_inter_predictors(cm, xd, plane, mi, 0 /* build_for_obmc */, bw, bh,
                             mi_x, mi_y, NULL /* mc_buf */,
                             enc_calc_subpel_params);
}

void av1_enc_build_inter_predictor_y(MACROBLOCKD *xd, int mi_row, int mi_col) {
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
  struct macroblockd_plane *const pd = &xd->plane[AOM_PLANE_Y];
  InterPredParams inter_pred_params;

  struct buf_2d *const dst_buf = &pd->dst;
  uint8_t *const dst = dst_buf->buf;
  const MV mv = xd->mi[0]->mv[0].as_mv;
  const struct scale_factors *const sf = xd->block_ref_scale_factors[0];

  av1_init_inter_params(&inter_pred_params, pd->width, pd->height, mi_y, mi_x,
                        pd->subsampling_x, pd->subsampling_y, xd->bd,
                        is_cur_buf_hbd(xd), false, sf, pd->pre,
                        xd->mi[0]->interp_filters);

  inter_pred_params.conv_params = get_conv_params_no_round(
      0, AOM_PLANE_Y, xd->tmp_conv_dst, MAX_SB_SIZE, false, xd->bd);

  inter_pred_params.conv_params.use_dist_wtd_comp_avg = 0;
  av1_enc_build_one_inter_predictor(dst, dst_buf->stride, &mv,
                                    &inter_pred_params);
}

void av1_enc_build_inter_predictor(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                   int mi_row, int mi_col,
                                   const BUFFER_SET *ctx, BLOCK_SIZE bsize,
                                   int plane_from, int plane_to) {
  for (int plane = plane_from; plane <= plane_to; ++plane) {
    if (plane && !xd->is_chroma_ref) break;
    const int mi_x = mi_col * MI_SIZE;
    const int mi_y = mi_row * MI_SIZE;
    enc_build_inter_predictors(cm, xd, plane, xd->mi[0], xd->plane[plane].width,
                               xd->plane[plane].height, mi_x, mi_y);

    if (is_interintra_pred(xd->mi[0])) {
      BUFFER_SET default_ctx = {
        { xd->plane[0].dst.buf, xd->plane[1].dst.buf, xd->plane[2].dst.buf },
        { xd->plane[0].dst.stride, xd->plane[1].dst.stride,
          xd->plane[2].dst.stride }
      };
      if (!ctx) {
        ctx = &default_ctx;
      }
      av1_build_interintra_predictor(cm, xd, xd->plane[plane].dst.buf,
                                     xd->plane[plane].dst.stride, ctx, plane,
                                     bsize);
    }
  }
}

static void setup_address_for_obmc(MACROBLOCKD *xd, int mi_row_offset,
                                   int mi_col_offset, MB_MODE_INFO *ref_mbmi,
                                   struct build_prediction_ctxt *ctxt,
                                   const int num_planes) {
  const BLOCK_SIZE ref_bsize = AOMMAX(BLOCK_8X8, ref_mbmi->bsize);
  const int ref_mi_row = xd->mi_row + mi_row_offset;
  const int ref_mi_col = xd->mi_col + mi_col_offset;

  for (int plane = 0; plane < num_planes; ++plane) {
    struct macroblockd_plane *const pd = &xd->plane[plane];
    setup_pred_plane(&pd->dst, ref_bsize, ctxt->tmp_buf[plane],
                     ctxt->tmp_width[plane], ctxt->tmp_height[plane],
                     ctxt->tmp_stride[plane], mi_row_offset, mi_col_offset,
                     NULL, pd->subsampling_x, pd->subsampling_y);
  }

  const MV_REFERENCE_FRAME frame = ref_mbmi->ref_frame[0];

  const RefCntBuffer *const ref_buf = get_ref_frame_buf(ctxt->cm, frame);
  const struct scale_factors *const sf =
      get_ref_scale_factors_const(ctxt->cm, frame);

  xd->block_ref_scale_factors[0] = sf;
  if ((!av1_is_valid_scale(sf)))
    aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                       "Reference frame has invalid dimensions");

  av1_setup_pre_planes(xd, 0, &ref_buf->buf, ref_mi_row, ref_mi_col, sf,
                       num_planes);
}

static INLINE void build_obmc_prediction(MACROBLOCKD *xd, int rel_mi_row,
                                         int rel_mi_col, uint8_t op_mi_size,
                                         int dir, MB_MODE_INFO *above_mbmi,
                                         void *fun_ctxt, const int num_planes) {
  struct build_prediction_ctxt *ctxt = (struct build_prediction_ctxt *)fun_ctxt;
  setup_address_for_obmc(xd, rel_mi_row, rel_mi_col, above_mbmi, ctxt,
                         num_planes);

  const int mi_x = (xd->mi_col + rel_mi_col) << MI_SIZE_LOG2;
  const int mi_y = (xd->mi_row + rel_mi_row) << MI_SIZE_LOG2;

  const BLOCK_SIZE bsize = xd->mi[0]->bsize;

  InterPredParams inter_pred_params;

  for (int j = 0; j < num_planes; ++j) {
    const struct macroblockd_plane *pd = &xd->plane[j];
    int bw = 0, bh = 0;

    if (dir) {
      // prepare left reference block size
      bw = clamp(block_size_wide[bsize] >> (pd->subsampling_x + 1), 4,
                 block_size_wide[BLOCK_64X64] >> (pd->subsampling_x + 1));
      bh = (op_mi_size << MI_SIZE_LOG2) >> pd->subsampling_y;
    } else {
      // prepare above reference block size
      bw = (op_mi_size * MI_SIZE) >> pd->subsampling_x;
      bh = clamp(block_size_high[bsize] >> (pd->subsampling_y + 1), 4,
                 block_size_high[BLOCK_64X64] >> (pd->subsampling_y + 1));
    }

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, dir)) continue;

    const struct buf_2d *const pre_buf = &pd->pre[0];
    const MV mv = above_mbmi->mv[0].as_mv;

    av1_init_inter_params(&inter_pred_params, bw, bh, mi_y >> pd->subsampling_y,
                          mi_x >> pd->subsampling_x, pd->subsampling_x,
                          pd->subsampling_y, xd->bd, is_cur_buf_hbd(xd), 0,
                          xd->block_ref_scale_factors[0], pre_buf,
                          above_mbmi->interp_filters);
    inter_pred_params.conv_params = get_conv_params(0, j, xd->bd);

    av1_enc_build_one_inter_predictor(pd->dst.buf, pd->dst.stride, &mv,
                                      &inter_pred_params);
  }
}

void av1_build_prediction_by_above_preds(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         uint8_t *tmp_buf[MAX_MB_PLANE],
                                         int tmp_width[MAX_MB_PLANE],
                                         int tmp_height[MAX_MB_PLANE],
                                         int tmp_stride[MAX_MB_PLANE]) {
  if (!xd->up_available) return;
  struct build_prediction_ctxt ctxt = {
    cm, tmp_buf, tmp_width, tmp_height, tmp_stride, xd->mb_to_right_edge, NULL
  };
  BLOCK_SIZE bsize = xd->mi[0]->bsize;
  foreach_overlappable_nb_above(cm, xd,
                                max_neighbor_obmc[mi_size_wide_log2[bsize]],
                                build_obmc_prediction, &ctxt);
}

void av1_build_prediction_by_left_preds(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                        uint8_t *tmp_buf[MAX_MB_PLANE],
                                        int tmp_width[MAX_MB_PLANE],
                                        int tmp_height[MAX_MB_PLANE],
                                        int tmp_stride[MAX_MB_PLANE]) {
  if (!xd->left_available) return;
  struct build_prediction_ctxt ctxt = {
    cm, tmp_buf, tmp_width, tmp_height, tmp_stride, xd->mb_to_bottom_edge, NULL
  };
  BLOCK_SIZE bsize = xd->mi[0]->bsize;
  foreach_overlappable_nb_left(cm, xd,
                               max_neighbor_obmc[mi_size_high_log2[bsize]],
                               build_obmc_prediction, &ctxt);
}

void av1_build_obmc_inter_predictors_sb(const AV1_COMMON *cm, MACROBLOCKD *xd) {
  const int num_planes = av1_num_planes(cm);
  uint8_t *dst_buf1[MAX_MB_PLANE], *dst_buf2[MAX_MB_PLANE];
  int dst_stride1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_stride2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_width2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height1[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };
  int dst_height2[MAX_MB_PLANE] = { MAX_SB_SIZE, MAX_SB_SIZE, MAX_SB_SIZE };

  av1_setup_obmc_dst_bufs(xd, dst_buf1, dst_buf2);

  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  av1_build_prediction_by_above_preds(cm, xd, dst_buf1, dst_width1, dst_height1,
                                      dst_stride1);
  av1_build_prediction_by_left_preds(cm, xd, dst_buf2, dst_width2, dst_height2,
                                     dst_stride2);
  av1_setup_dst_planes(xd->plane, xd->mi[0]->bsize, &cm->cur_frame->buf, mi_row,
                       mi_col, 0, num_planes);
  av1_build_obmc_inter_prediction(cm, xd, dst_buf1, dst_stride1, dst_buf2,
                                  dst_stride2);
}

void av1_build_inter_predictors_for_planes_single_buf(
    MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane_from, int plane_to, int ref,
    uint8_t *ext_dst[], int ext_dst_stride[]) {
  assert(bsize < BLOCK_SIZES_ALL);
  const MB_MODE_INFO *mi = xd->mi[0];
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const int mi_x = mi_col * MI_SIZE;
  const int mi_y = mi_row * MI_SIZE;
  WarpTypesAllowed warp_types;
  const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[ref]];
  warp_types.global_warp_allowed = is_global_mv_block(mi, wm->wmtype);
  warp_types.local_warp_allowed = mi->motion_mode == WARPED_CAUSAL;

  for (int plane = plane_from; plane <= plane_to; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const BLOCK_SIZE plane_bsize =
        get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];

    InterPredParams inter_pred_params;

    av1_init_inter_params(&inter_pred_params, bw, bh, mi_y >> pd->subsampling_y,
                          mi_x >> pd->subsampling_x, pd->subsampling_x,
                          pd->subsampling_y, xd->bd, is_cur_buf_hbd(xd), 0,
                          xd->block_ref_scale_factors[ref], &pd->pre[ref],
                          mi->interp_filters);
    inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);
    av1_init_warp_params(&inter_pred_params, &warp_types, ref, xd, mi);

    uint8_t *const dst = get_buf_by_bd(xd, ext_dst[plane]);
    const MV mv = mi->mv[ref].as_mv;

    av1_enc_build_one_inter_predictor(dst, ext_dst_stride[plane], &mv,
                                      &inter_pred_params);
  }
}

static void build_masked_compound(
    uint8_t *dst, int dst_stride, const uint8_t *src0, int src0_stride,
    const uint8_t *src1, int src1_stride,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type, int h,
    int w) {
  // Derive subsampling from h and w passed in. May be refactored to
  // pass in subsampling factors directly.
  const int subh = (2 << mi_size_high_log2[sb_type]) == h;
  const int subw = (2 << mi_size_wide_log2[sb_type]) == w;
  const uint8_t *mask = av1_get_compound_type_mask(comp_data, sb_type);
  aom_blend_a64_mask(dst, dst_stride, src0, src0_stride, src1, src1_stride,
                     mask, block_size_wide[sb_type], w, h, subw, subh);
}

#if CONFIG_AV1_HIGHBITDEPTH
static void build_masked_compound_highbd(
    uint8_t *dst_8, int dst_stride, const uint8_t *src0_8, int src0_stride,
    const uint8_t *src1_8, int src1_stride,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type, int h,
    int w, int bd) {
  // Derive subsampling from h and w passed in. May be refactored to
  // pass in subsampling factors directly.
  const int subh = (2 << mi_size_high_log2[sb_type]) == h;
  const int subw = (2 << mi_size_wide_log2[sb_type]) == w;
  const uint8_t *mask = av1_get_compound_type_mask(comp_data, sb_type);
  // const uint8_t *mask =
  //     av1_get_contiguous_soft_mask(wedge_index, wedge_sign, sb_type);
  aom_highbd_blend_a64_mask(dst_8, dst_stride, src0_8, src0_stride, src1_8,
                            src1_stride, mask, block_size_wide[sb_type], w, h,
                            subw, subh, bd);
}
#endif

static void build_wedge_inter_predictor_from_buf(
    MACROBLOCKD *xd, int plane, int x, int y, int w, int h, uint8_t *ext_dst0,
    int ext_dst_stride0, uint8_t *ext_dst1, int ext_dst_stride1) {
  MB_MODE_INFO *const mbmi = xd->mi[0];
  const int is_compound = has_second_ref(mbmi);
  MACROBLOCKD_PLANE *const pd = &xd->plane[plane];
  struct buf_2d *const dst_buf = &pd->dst;
  uint8_t *const dst = dst_buf->buf + dst_buf->stride * y + x;
  mbmi->interinter_comp.seg_mask = xd->seg_mask;
  const INTERINTER_COMPOUND_DATA *comp_data = &mbmi->interinter_comp;
  const int is_hbd = is_cur_buf_hbd(xd);

  if (is_compound && is_masked_compound_type(comp_data->type)) {
    if (!plane && comp_data->type == COMPOUND_DIFFWTD) {
#if CONFIG_AV1_HIGHBITDEPTH
      if (is_hbd) {
        av1_build_compound_diffwtd_mask_highbd(
            comp_data->seg_mask, comp_data->mask_type,
            CONVERT_TO_BYTEPTR(ext_dst0), ext_dst_stride0,
            CONVERT_TO_BYTEPTR(ext_dst1), ext_dst_stride1, h, w, xd->bd);
      } else {
        av1_build_compound_diffwtd_mask(
            comp_data->seg_mask, comp_data->mask_type, ext_dst0,
            ext_dst_stride0, ext_dst1, ext_dst_stride1, h, w);
      }
#else
      (void)is_hbd;
      av1_build_compound_diffwtd_mask(comp_data->seg_mask, comp_data->mask_type,
                                      ext_dst0, ext_dst_stride0, ext_dst1,
                                      ext_dst_stride1, h, w);
#endif  // CONFIG_AV1_HIGHBITDEPTH
    }
#if CONFIG_AV1_HIGHBITDEPTH
    if (is_hbd) {
      build_masked_compound_highbd(
          dst, dst_buf->stride, CONVERT_TO_BYTEPTR(ext_dst0), ext_dst_stride0,
          CONVERT_TO_BYTEPTR(ext_dst1), ext_dst_stride1, comp_data, mbmi->bsize,
          h, w, xd->bd);
    } else {
      build_masked_compound(dst, dst_buf->stride, ext_dst0, ext_dst_stride0,
                            ext_dst1, ext_dst_stride1, comp_data, mbmi->bsize,
                            h, w);
    }
#else
    build_masked_compound(dst, dst_buf->stride, ext_dst0, ext_dst_stride0,
                          ext_dst1, ext_dst_stride1, comp_data, mbmi->bsize, h,
                          w);
#endif
  } else {
#if CONFIG_AV1_HIGHBITDEPTH
    if (is_hbd) {
      aom_highbd_convolve_copy(CONVERT_TO_SHORTPTR(ext_dst0), ext_dst_stride0,
                               CONVERT_TO_SHORTPTR(dst), dst_buf->stride, w, h);
    } else {
      aom_convolve_copy(ext_dst0, ext_dst_stride0, dst, dst_buf->stride, w, h);
    }
#else
    aom_convolve_copy(ext_dst0, ext_dst_stride0, dst, dst_buf->stride, w, h);
#endif
  }
}

void av1_build_wedge_inter_predictor_from_buf(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                              int plane_from, int plane_to,
                                              uint8_t *ext_dst0[],
                                              int ext_dst_stride0[],
                                              uint8_t *ext_dst1[],
                                              int ext_dst_stride1[]) {
  int plane;
  assert(bsize < BLOCK_SIZES_ALL);
  for (plane = plane_from; plane <= plane_to; ++plane) {
    const BLOCK_SIZE plane_bsize = get_plane_block_size(
        bsize, xd->plane[plane].subsampling_x, xd->plane[plane].subsampling_y);
    const int bw = block_size_wide[plane_bsize];
    const int bh = block_size_high[plane_bsize];
    build_wedge_inter_predictor_from_buf(
        xd, plane, 0, 0, bw, bh, ext_dst0[plane], ext_dst_stride0[plane],
        ext_dst1[plane], ext_dst_stride1[plane]);
  }
}

// Get pred block from up-sampled reference.
void aom_upsampled_pred_c(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                          int mi_row, int mi_col, const MV *const mv,
                          uint8_t *comp_pred, int width, int height,
                          int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                          int ref_stride, int subpel_search) {
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

  if (!subpel_x_q3 && !subpel_y_q3) {
    for (int i = 0; i < height; i++) {
      memcpy(comp_pred, ref, width * sizeof(*comp_pred));
      comp_pred += width;
      ref += ref_stride;
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_convolve8_horiz_c(ref, ref_stride, comp_pred, width, kernel, 16, NULL,
                          -1, width, height);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_convolve8_vert_c(ref, ref_stride, comp_pred, width, NULL, -1, kernel,
                         16, width, height);
  } else {
    DECLARE_ALIGNED(16, uint8_t,
                    temp[((MAX_SB_SIZE * 2 + 16) + 16) * MAX_SB_SIZE]);
    const int16_t *const kernel_x =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    const int16_t *const kernel_y =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    const int intermediate_height =
        (((height - 1) * 8 + subpel_y_q3) >> 3) + filter->taps;
    assert(intermediate_height <= (MAX_SB_SIZE * 2 + 16) + 16);
    aom_convolve8_horiz_c(ref - ref_stride * ((filter->taps >> 1) - 1),
                          ref_stride, temp, MAX_SB_SIZE, kernel_x, 16, NULL, -1,
                          width, intermediate_height);
    aom_convolve8_vert_c(temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1),
                         MAX_SB_SIZE, comp_pred, width, NULL, -1, kernel_y, 16,
                         width, height);
  }
}

void aom_comp_avg_upsampled_pred_c(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                   int mi_row, int mi_col, const MV *const mv,
                                   uint8_t *comp_pred, const uint8_t *pred,
                                   int width, int height, int subpel_x_q3,
                                   int subpel_y_q3, const uint8_t *ref,
                                   int ref_stride, int subpel_search) {
  int i, j;

  aom_upsampled_pred_c(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                       subpel_x_q3, subpel_y_q3, ref, ref_stride,
                       subpel_search);
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      comp_pred[j] = ROUND_POWER_OF_TWO(comp_pred[j] + pred[j], 1);
    }
    comp_pred += width;
    pred += width;
  }
}

void aom_comp_mask_upsampled_pred_c(MACROBLOCKD *xd, const AV1_COMMON *const cm,
                                    int mi_row, int mi_col, const MV *const mv,
                                    uint8_t *comp_pred, const uint8_t *pred,
                                    int width, int height, int subpel_x_q3,
                                    int subpel_y_q3, const uint8_t *ref,
                                    int ref_stride, const uint8_t *mask,
                                    int mask_stride, int invert_mask,
                                    int subpel_search) {
  if (subpel_x_q3 | subpel_y_q3) {
    aom_upsampled_pred_c(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                         subpel_x_q3, subpel_y_q3, ref, ref_stride,
                         subpel_search);
    ref = comp_pred;
    ref_stride = width;
  }
  aom_comp_mask_pred_c(comp_pred, pred, width, height, ref, ref_stride, mask,
                       mask_stride, invert_mask);
}

void aom_dist_wtd_comp_avg_upsampled_pred_c(
    MACROBLOCKD *xd, const AV1_COMMON *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
    int ref_stride, const DIST_WTD_COMP_PARAMS *jcp_param, int subpel_search) {
  int i, j;
  const int fwd_offset = jcp_param->fwd_offset;
  const int bck_offset = jcp_param->bck_offset;

  aom_upsampled_pred_c(xd, cm, mi_row, mi_col, mv, comp_pred, width, height,
                       subpel_x_q3, subpel_y_q3, ref, ref_stride,
                       subpel_search);

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      int tmp = pred[j] * bck_offset + comp_pred[j] * fwd_offset;
      tmp = ROUND_POWER_OF_TWO(tmp, DIST_PRECISION_BITS);
      comp_pred[j] = (uint8_t)tmp;
    }
    comp_pred += width;
    pred += width;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
void aom_highbd_upsampled_pred_c(MACROBLOCKD *xd,
                                 const struct AV1Common *const cm, int mi_row,
                                 int mi_col, const MV *const mv,
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
    for (int i = 0; i < height; i++) {
      memcpy(comp_pred, ref, width * sizeof(*comp_pred));
      comp_pred += width;
      ref += ref_stride;
    }
  } else if (!subpel_y_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_x_q3 << 1);
    aom_highbd_convolve8_horiz_c(ref8, ref_stride, comp_pred8, width, kernel,
                                 16, NULL, -1, width, height, bd);
  } else if (!subpel_x_q3) {
    const int16_t *const kernel =
        av1_get_interp_filter_subpel_kernel(filter, subpel_y_q3 << 1);
    aom_highbd_convolve8_vert_c(ref8, ref_stride, comp_pred8, width, NULL, -1,
                                kernel, 16, width, height, bd);
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
    aom_highbd_convolve8_horiz_c(ref8 - ref_stride * ((filter->taps >> 1) - 1),
                                 ref_stride, CONVERT_TO_BYTEPTR(temp),
                                 MAX_SB_SIZE, kernel_x, 16, NULL, -1, width,
                                 intermediate_height, bd);
    aom_highbd_convolve8_vert_c(
        CONVERT_TO_BYTEPTR(temp + MAX_SB_SIZE * ((filter->taps >> 1) - 1)),
        MAX_SB_SIZE, comp_pred8, width, NULL, -1, kernel_y, 16, width, height,
        bd);
  }
}

void aom_highbd_comp_avg_upsampled_pred_c(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, int subpel_search) {
  int i, j;

  const uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                            height, subpel_x_q3, subpel_y_q3, ref8, ref_stride,
                            bd, subpel_search);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      comp_pred[j] = ROUND_POWER_OF_TWO(pred[j] + comp_pred[j], 1);
    }
    comp_pred += width;
    pred += width;
  }
}

void aom_highbd_dist_wtd_comp_avg_upsampled_pred_c(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, int bd, const DIST_WTD_COMP_PARAMS *jcp_param,
    int subpel_search) {
  int i, j;
  const int fwd_offset = jcp_param->fwd_offset;
  const int bck_offset = jcp_param->bck_offset;
  const uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  uint16_t *comp_pred = CONVERT_TO_SHORTPTR(comp_pred8);
  aom_highbd_upsampled_pred_c(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                              height, subpel_x_q3, subpel_y_q3, ref8,
                              ref_stride, bd, subpel_search);

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      int tmp = pred[j] * bck_offset + comp_pred[j] * fwd_offset;
      tmp = ROUND_POWER_OF_TWO(tmp, DIST_PRECISION_BITS);
      comp_pred[j] = (uint16_t)tmp;
    }
    comp_pred += width;
    pred += width;
  }
}

void aom_highbd_comp_mask_upsampled_pred(
    MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
    const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
    int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
    int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask,
    int bd, int subpel_search) {
  aom_highbd_upsampled_pred(xd, cm, mi_row, mi_col, mv, comp_pred8, width,
                            height, subpel_x_q3, subpel_y_q3, ref8, ref_stride,
                            bd, subpel_search);
  aom_highbd_comp_mask_pred(comp_pred8, pred8, width, height, comp_pred8, width,
                            mask, mask_stride, invert_mask);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH
