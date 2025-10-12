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

#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/variance.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"
#include "aom_util/aom_pthread.h"

#include "av1/common/entropymv.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"  // av1_setup_dst_planes()
#include "av1/common/reconintra.h"
#include "av1/common/txb_common.h"
#include "av1/encoder/aq_variance.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/block.h"
#include "av1/encoder/dwt.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodeframe_utils.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encoder_utils.h"
#include "av1/encoder/encode_strategy.h"
#include "av1/encoder/ethread.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/reconinter_enc.h"

#define OUTPUT_FPF 0

#define FIRST_PASS_Q 10.0
#define INTRA_MODE_PENALTY 1024
#define NEW_MV_MODE_PENALTY 32
#define DARK_THRESH 64

#define NCOUNT_INTRA_THRESH 8192
#define NCOUNT_INTRA_FACTOR 3

#define INVALID_FP_STATS_TO_PREDICT_FLAT_GOP -1

static inline void output_stats(FIRSTPASS_STATS *stats,
                                struct aom_codec_pkt_list *pktlist) {
  struct aom_codec_cx_pkt pkt;
  pkt.kind = AOM_CODEC_STATS_PKT;
  pkt.data.twopass_stats.buf = stats;
  pkt.data.twopass_stats.sz = sizeof(FIRSTPASS_STATS);
  if (pktlist != NULL) aom_codec_pkt_list_add(pktlist, &pkt);

// TEMP debug code
#if OUTPUT_FPF
  {
    FILE *fpfile;
    fpfile = fopen("firstpass.stt", "a");

    fprintf(fpfile,
            "%12.0lf %12.4lf %12.0lf %12.0lf %12.0lf %12.4lf %12.4lf"
            "%12.4lf %12.4lf %12.4lf %12.4lf %12.4lf %12.4lf %12.4lf %12.4lf"
            "%12.4lf %12.4lf %12.0lf %12.0lf %12.0lf %12.4lf %12.4lf\n",
            stats->frame, stats->weight, stats->intra_error, stats->coded_error,
            stats->sr_coded_error, stats->pcnt_inter, stats->pcnt_motion,
            stats->pcnt_second_ref, stats->pcnt_neutral, stats->intra_skip_pct,
            stats->inactive_zone_rows, stats->inactive_zone_cols, stats->MVr,
            stats->mvr_abs, stats->MVc, stats->mvc_abs, stats->MVrv,
            stats->MVcv, stats->mv_in_out_count, stats->new_mv_count,
            stats->count, stats->duration);
    fclose(fpfile);
  }
#endif
}

void av1_twopass_zero_stats(FIRSTPASS_STATS *section) {
  section->frame = 0.0;
  section->weight = 0.0;
  section->intra_error = 0.0;
  section->frame_avg_wavelet_energy = 0.0;
  section->coded_error = 0.0;
  section->log_intra_error = 0.0;
  section->log_coded_error = 0.0;
  section->sr_coded_error = 0.0;
  section->pcnt_inter = 0.0;
  section->pcnt_motion = 0.0;
  section->pcnt_second_ref = 0.0;
  section->pcnt_neutral = 0.0;
  section->intra_skip_pct = 0.0;
  section->inactive_zone_rows = 0.0;
  section->inactive_zone_cols = 0.0;
  section->MVr = 0.0;
  section->mvr_abs = 0.0;
  section->MVc = 0.0;
  section->mvc_abs = 0.0;
  section->MVrv = 0.0;
  section->MVcv = 0.0;
  section->mv_in_out_count = 0.0;
  section->new_mv_count = 0.0;
  section->count = 0.0;
  section->duration = 1.0;
  section->is_flash = 0;
  section->noise_var = 0;
  section->cor_coeff = 1.0;
}

void av1_accumulate_stats(FIRSTPASS_STATS *section,
                          const FIRSTPASS_STATS *frame) {
  section->frame += frame->frame;
  section->weight += frame->weight;
  section->intra_error += frame->intra_error;
  section->log_intra_error += log1p(frame->intra_error);
  section->log_coded_error += log1p(frame->coded_error);
  section->frame_avg_wavelet_energy += frame->frame_avg_wavelet_energy;
  section->coded_error += frame->coded_error;
  section->sr_coded_error += frame->sr_coded_error;
  section->pcnt_inter += frame->pcnt_inter;
  section->pcnt_motion += frame->pcnt_motion;
  section->pcnt_second_ref += frame->pcnt_second_ref;
  section->pcnt_neutral += frame->pcnt_neutral;
  section->intra_skip_pct += frame->intra_skip_pct;
  section->inactive_zone_rows += frame->inactive_zone_rows;
  section->inactive_zone_cols += frame->inactive_zone_cols;
  section->MVr += frame->MVr;
  section->mvr_abs += frame->mvr_abs;
  section->MVc += frame->MVc;
  section->mvc_abs += frame->mvc_abs;
  section->MVrv += frame->MVrv;
  section->MVcv += frame->MVcv;
  section->mv_in_out_count += frame->mv_in_out_count;
  section->new_mv_count += frame->new_mv_count;
  section->count += frame->count;
  section->duration += frame->duration;
}

static int get_unit_rows(const BLOCK_SIZE fp_block_size, const int mb_rows) {
  const int height_mi_log2 = mi_size_high_log2[fp_block_size];
  const int mb_height_mi_log2 = mi_size_high_log2[BLOCK_16X16];
  if (height_mi_log2 > mb_height_mi_log2) {
    return mb_rows >> (height_mi_log2 - mb_height_mi_log2);
  }

  return mb_rows << (mb_height_mi_log2 - height_mi_log2);
}

static int get_unit_cols(const BLOCK_SIZE fp_block_size, const int mb_cols) {
  const int width_mi_log2 = mi_size_wide_log2[fp_block_size];
  const int mb_width_mi_log2 = mi_size_wide_log2[BLOCK_16X16];
  if (width_mi_log2 > mb_width_mi_log2) {
    return mb_cols >> (width_mi_log2 - mb_width_mi_log2);
  }

  return mb_cols << (mb_width_mi_log2 - width_mi_log2);
}

// TODO(chengchen): can we simplify it even if resize has to be considered?
static int get_num_mbs(const BLOCK_SIZE fp_block_size,
                       const int num_mbs_16X16) {
  const int width_mi_log2 = mi_size_wide_log2[fp_block_size];
  const int height_mi_log2 = mi_size_high_log2[fp_block_size];
  const int mb_width_mi_log2 = mi_size_wide_log2[BLOCK_16X16];
  const int mb_height_mi_log2 = mi_size_high_log2[BLOCK_16X16];
  // TODO(chengchen): Now this function assumes a square block is used.
  // It does not support rectangular block sizes.
  assert(width_mi_log2 == height_mi_log2);
  if (width_mi_log2 > mb_width_mi_log2) {
    return num_mbs_16X16 >> ((width_mi_log2 - mb_width_mi_log2) +
                             (height_mi_log2 - mb_height_mi_log2));
  }

  return num_mbs_16X16 << ((mb_width_mi_log2 - width_mi_log2) +
                           (mb_height_mi_log2 - height_mi_log2));
}

void av1_end_first_pass(AV1_COMP *cpi) {
  if (cpi->ppi->twopass.stats_buf_ctx->total_stats && !cpi->ppi->lap_enabled)
    output_stats(cpi->ppi->twopass.stats_buf_ctx->total_stats,
                 cpi->ppi->output_pkt_list);
}

static aom_variance_fn_t get_block_variance_fn(BLOCK_SIZE bsize) {
  switch (bsize) {
    case BLOCK_8X8: return aom_mse8x8;
    case BLOCK_16X8: return aom_mse16x8;
    case BLOCK_8X16: return aom_mse8x16;
    default: return aom_mse16x16;
  }
}

static unsigned int get_prediction_error(BLOCK_SIZE bsize,
                                         const struct buf_2d *src,
                                         const struct buf_2d *ref) {
  unsigned int sse;
  const aom_variance_fn_t fn = get_block_variance_fn(bsize);
  fn(src->buf, src->stride, ref->buf, ref->stride, &sse);
  return sse;
}

#if CONFIG_AV1_HIGHBITDEPTH
static aom_variance_fn_t highbd_get_block_variance_fn(BLOCK_SIZE bsize,
                                                      int bd) {
  switch (bd) {
    default:
      switch (bsize) {
        case BLOCK_8X8: return aom_highbd_8_mse8x8;
        case BLOCK_16X8: return aom_highbd_8_mse16x8;
        case BLOCK_8X16: return aom_highbd_8_mse8x16;
        default: return aom_highbd_8_mse16x16;
      }
    case 10:
      switch (bsize) {
        case BLOCK_8X8: return aom_highbd_10_mse8x8;
        case BLOCK_16X8: return aom_highbd_10_mse16x8;
        case BLOCK_8X16: return aom_highbd_10_mse8x16;
        default: return aom_highbd_10_mse16x16;
      }
    case 12:
      switch (bsize) {
        case BLOCK_8X8: return aom_highbd_12_mse8x8;
        case BLOCK_16X8: return aom_highbd_12_mse16x8;
        case BLOCK_8X16: return aom_highbd_12_mse8x16;
        default: return aom_highbd_12_mse16x16;
      }
  }
}

static unsigned int highbd_get_prediction_error(BLOCK_SIZE bsize,
                                                const struct buf_2d *src,
                                                const struct buf_2d *ref,
                                                int bd) {
  unsigned int sse;
  const aom_variance_fn_t fn = highbd_get_block_variance_fn(bsize, bd);
  fn(src->buf, src->stride, ref->buf, ref->stride, &sse);
  return sse;
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

// Refine the motion search range according to the frame dimension
// for first pass test.
static int get_search_range(int width, int height) {
  int sr = 0;
  const int dim = AOMMIN(width, height);

  while ((dim << sr) < MAX_FULL_PEL_VAL) ++sr;
  return sr;
}

static inline const search_site_config *av1_get_first_pass_search_site_config(
    const AV1_COMP *cpi, MACROBLOCK *x, SEARCH_METHODS search_method) {
  const int ref_stride = x->e_mbd.plane[0].pre[0].stride;

  // For AVIF applications, even the source frames can have changing resolution,
  // so we need to manually check for the strides :(
  // AV1_COMP::mv_search_params.search_site_config is a compressor level cache
  // that's shared by multiple threads. In most cases where all frames have the
  // same resolution, the cache contains the search site config that we need.
  const MotionVectorSearchParams *mv_search_params = &cpi->mv_search_params;
  if (ref_stride == mv_search_params->search_site_cfg[SS_CFG_FPF]->stride) {
    return mv_search_params->search_site_cfg[SS_CFG_FPF];
  }

  // If the cache does not contain the correct stride, then we will need to rely
  // on the thread level config MACROBLOCK::search_site_cfg_buf. If even the
  // thread level config doesn't match, then we need to update it.
  search_method = search_method_lookup[search_method];
  assert(search_method_lookup[search_method] == search_method &&
         "The search_method_lookup table should be idempotent.");
  if (ref_stride != x->search_site_cfg_buf[search_method].stride) {
    av1_refresh_search_site_config(x->search_site_cfg_buf, search_method,
                                   ref_stride);
  }

  return x->search_site_cfg_buf;
}

static inline void first_pass_motion_search(AV1_COMP *cpi, MACROBLOCK *x,
                                            const MV *ref_mv,
                                            FULLPEL_MV *best_mv,
                                            int *best_motion_err) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  FULLPEL_MV start_mv = get_fullmv_from_mv(ref_mv);
  int tmp_err;
  const BLOCK_SIZE bsize = xd->mi[0]->bsize;
  const int new_mv_mode_penalty = NEW_MV_MODE_PENALTY;
  const int sr = get_search_range(cm->width, cm->height);
  const int step_param = cpi->sf.fp_sf.reduce_mv_step_param + sr;

  const search_site_config *first_pass_search_sites =
      av1_get_first_pass_search_site_config(cpi, x, NSTEP);
  const int fine_search_interval =
      cpi->is_screen_content_type && cm->features.allow_intrabc;
  FULLPEL_MOTION_SEARCH_PARAMS ms_params;
  av1_make_default_fullpel_ms_params(&ms_params, cpi, x, bsize, ref_mv,
                                     start_mv, first_pass_search_sites, NSTEP,
                                     fine_search_interval);

  FULLPEL_MV this_best_mv;
  FULLPEL_MV_STATS best_mv_stats;
  tmp_err = av1_full_pixel_search(start_mv, &ms_params, step_param, NULL,
                                  &this_best_mv, &best_mv_stats, NULL);

  if (tmp_err < INT_MAX) {
    aom_variance_fn_ptr_t v_fn_ptr = cpi->ppi->fn_ptr[bsize];
    const MSBuffers *ms_buffers = &ms_params.ms_buffers;
    tmp_err = av1_get_mvpred_sse(&ms_params.mv_cost_params, this_best_mv,
                                 &v_fn_ptr, ms_buffers->src, ms_buffers->ref) +
              new_mv_mode_penalty;
  }

  if (tmp_err < *best_motion_err) {
    *best_motion_err = tmp_err;
    *best_mv = this_best_mv;
  }
}

static BLOCK_SIZE get_bsize(const CommonModeInfoParams *const mi_params,
                            const BLOCK_SIZE fp_block_size, const int unit_row,
                            const int unit_col) {
  const int unit_width = mi_size_wide[fp_block_size];
  const int unit_height = mi_size_high[fp_block_size];
  const int is_half_width =
      unit_width * unit_col + unit_width / 2 >= mi_params->mi_cols;
  const int is_half_height =
      unit_height * unit_row + unit_height / 2 >= mi_params->mi_rows;
  const int max_dimension =
      AOMMAX(block_size_wide[fp_block_size], block_size_high[fp_block_size]);
  int square_block_size = 0;
  // 4X4, 8X8, 16X16, 32X32, 64X64, 128X128
  switch (max_dimension) {
    case 4: square_block_size = 0; break;
    case 8: square_block_size = 1; break;
    case 16: square_block_size = 2; break;
    case 32: square_block_size = 3; break;
    case 64: square_block_size = 4; break;
    case 128: square_block_size = 5; break;
    default: assert(0 && "First pass block size is not supported!"); break;
  }
  if (is_half_width && is_half_height) {
    return subsize_lookup[PARTITION_SPLIT][square_block_size];
  } else if (is_half_width) {
    return subsize_lookup[PARTITION_VERT][square_block_size];
  } else if (is_half_height) {
    return subsize_lookup[PARTITION_HORZ][square_block_size];
  } else {
    return fp_block_size;
  }
}

static int find_fp_qindex(aom_bit_depth_t bit_depth) {
  return av1_find_qindex(FIRST_PASS_Q, bit_depth, 0, QINDEX_RANGE - 1);
}

static double raw_motion_error_stdev(int *raw_motion_err_list,
                                     int raw_motion_err_counts) {
  int64_t sum_raw_err = 0;
  double raw_err_avg = 0;
  double raw_err_stdev = 0;
  if (raw_motion_err_counts == 0) return 0;

  int i;
  for (i = 0; i < raw_motion_err_counts; i++) {
    sum_raw_err += raw_motion_err_list[i];
  }
  raw_err_avg = (double)sum_raw_err / raw_motion_err_counts;
  for (i = 0; i < raw_motion_err_counts; i++) {
    raw_err_stdev += (raw_motion_err_list[i] - raw_err_avg) *
                     (raw_motion_err_list[i] - raw_err_avg);
  }
  // Calculate the standard deviation for the motion error of all the inter
  // blocks of the 0,0 motion using the last source
  // frame as the reference.
  raw_err_stdev = sqrt(raw_err_stdev / raw_motion_err_counts);
  return raw_err_stdev;
}

static inline int calc_wavelet_energy(const AV1EncoderConfig *oxcf) {
  return oxcf->q_cfg.deltaq_mode == DELTA_Q_PERCEPTUAL;
}
typedef struct intra_pred_block_pass1_args {
  const SequenceHeader *seq_params;
  MACROBLOCK *x;
} intra_pred_block_pass1_args;

static inline void copy_rect(uint8_t *dst, int dstride, const uint8_t *src,
                             int sstride, int width, int height, int use_hbd) {
#if CONFIG_AV1_HIGHBITDEPTH
  if (use_hbd) {
    aom_highbd_convolve_copy(CONVERT_TO_SHORTPTR(src), sstride,
                             CONVERT_TO_SHORTPTR(dst), dstride, width, height);
  } else {
    aom_convolve_copy(src, sstride, dst, dstride, width, height);
  }
#else
  (void)use_hbd;
  aom_convolve_copy(src, sstride, dst, dstride, width, height);
#endif
}

static void first_pass_intra_pred_and_calc_diff(int plane, int block,
                                                int blk_row, int blk_col,
                                                BLOCK_SIZE plane_bsize,
                                                TX_SIZE tx_size, void *arg) {
  (void)block;
  struct intra_pred_block_pass1_args *const args = arg;
  MACROBLOCK *const x = args->x;
  MACROBLOCKD *const xd = &x->e_mbd;
  MACROBLOCKD_PLANE *const pd = &xd->plane[plane];
  MACROBLOCK_PLANE *const p = &x->plane[plane];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst = &pd->dst.buf[(blk_row * dst_stride + blk_col) << MI_SIZE_LOG2];
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const SequenceHeader *seq_params = args->seq_params;
  const int src_stride = p->src.stride;
  uint8_t *src = &p->src.buf[(blk_row * src_stride + blk_col) << MI_SIZE_LOG2];

  av1_predict_intra_block(
      xd, seq_params->sb_size, seq_params->enable_intra_edge_filter, pd->width,
      pd->height, tx_size, mbmi->mode, 0, 0, FILTER_INTRA_MODES, src,
      src_stride, dst, dst_stride, blk_col, blk_row, plane);

  av1_subtract_txb(x, plane, plane_bsize, blk_col, blk_row, tx_size);
}

static void first_pass_predict_intra_block_for_luma_plane(
    const SequenceHeader *seq_params, MACROBLOCK *x, BLOCK_SIZE bsize) {
  assert(bsize < BLOCK_SIZES_ALL);
  const MACROBLOCKD *const xd = &x->e_mbd;
  const int plane = AOM_PLANE_Y;
  const MACROBLOCKD_PLANE *const pd = &xd->plane[plane];
  const int ss_x = pd->subsampling_x;
  const int ss_y = pd->subsampling_y;
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, ss_x, ss_y);
  const int dst_stride = pd->dst.stride;
  uint8_t *dst = pd->dst.buf;
  const MACROBLOCK_PLANE *const p = &x->plane[plane];
  const int src_stride = p->src.stride;
  const uint8_t *src = p->src.buf;

  intra_pred_block_pass1_args args = { seq_params, x };
  av1_foreach_transformed_block_in_plane(
      xd, plane_bsize, plane, first_pass_intra_pred_and_calc_diff, &args);

  // copy source data to recon buffer, as the recon buffer will be used as a
  // reference frame subsequently.
  copy_rect(dst, dst_stride, src, src_stride, block_size_wide[bsize],
            block_size_high[bsize], seq_params->use_highbitdepth);
}

#define UL_INTRA_THRESH 50
#define INVALID_ROW -1
// Computes and returns the intra pred error of a block.
// intra pred error: sum of squared error of the intra predicted residual.
// Inputs:
//   cpi: the encoder setting. Only a few params in it will be used.
//   this_frame: the current frame buffer.
//   tile: tile information (not used in first pass, already init to zero)
//   unit_row: row index in the unit of first pass block size.
//   unit_col: column index in the unit of first pass block size.
//   y_offset: the offset of y frame buffer, indicating the starting point of
//             the current block.
//   uv_offset: the offset of u and v frame buffer, indicating the starting
//              point of the current block.
//   fp_block_size: first pass block size.
//   qindex: quantization step size to encode the frame.
//   stats: frame encoding stats.
// Modifies:
//   stats->intra_skip_count
//   stats->image_data_start_row
//   stats->intra_factor
//   stats->brightness_factor
//   stats->intra_error
//   stats->frame_avg_wavelet_energy
// Returns:
//   this_intra_error.
static int firstpass_intra_prediction(
    AV1_COMP *cpi, ThreadData *td, YV12_BUFFER_CONFIG *const this_frame,
    const TileInfo *const tile, const int unit_row, const int unit_col,
    const int y_offset, const int uv_offset, const BLOCK_SIZE fp_block_size,
    const int qindex, FRAME_STATS *const stats) {
  const AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const SequenceHeader *const seq_params = cm->seq_params;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int unit_scale = mi_size_wide[fp_block_size];
  const int num_planes = av1_num_planes(cm);
  const BLOCK_SIZE bsize =
      get_bsize(mi_params, fp_block_size, unit_row, unit_col);

  set_mi_offsets(mi_params, xd, unit_row * unit_scale, unit_col * unit_scale);
  xd->plane[0].dst.buf = this_frame->y_buffer + y_offset;
  if (num_planes > 1) {
    xd->plane[1].dst.buf = this_frame->u_buffer + uv_offset;
    xd->plane[2].dst.buf = this_frame->v_buffer + uv_offset;
  }
  xd->left_available = (unit_col != 0);
  xd->mi[0]->bsize = bsize;
  xd->mi[0]->ref_frame[0] = INTRA_FRAME;
  set_mi_row_col(xd, tile, unit_row * unit_scale, mi_size_high[bsize],
                 unit_col * unit_scale, mi_size_wide[bsize], mi_params->mi_rows,
                 mi_params->mi_cols);
  set_plane_n4(xd, mi_size_wide[bsize], mi_size_high[bsize], num_planes);
  xd->mi[0]->segment_id = 0;
  xd->lossless[xd->mi[0]->segment_id] = (qindex == 0);
  xd->mi[0]->mode = DC_PRED;
  xd->mi[0]->tx_size = TX_4X4;

  if (cpi->sf.fp_sf.disable_recon)
    first_pass_predict_intra_block_for_luma_plane(seq_params, x, bsize);
  else
    av1_encode_intra_block_plane(cpi, x, bsize, 0, DRY_RUN_NORMAL, 0);
  int this_intra_error = aom_get_mb_ss(x->plane[0].src_diff);
  if (seq_params->use_highbitdepth) {
    switch (seq_params->bit_depth) {
      case AOM_BITS_8: break;
      case AOM_BITS_10: this_intra_error >>= 4; break;
      case AOM_BITS_12: this_intra_error >>= 8; break;
      default:
        assert(0 &&
               "seq_params->bit_depth should be AOM_BITS_8, "
               "AOM_BITS_10 or AOM_BITS_12");
        return -1;
    }
  }

  if (this_intra_error < UL_INTRA_THRESH) {
    ++stats->intra_skip_count;
  } else if ((unit_col > 0) && (stats->image_data_start_row == INVALID_ROW)) {
    stats->image_data_start_row = unit_row;
  }

  double log_intra = log1p(this_intra_error);
  if (log_intra < 10.0) {
    stats->intra_factor += 1.0 + ((10.0 - log_intra) * 0.05);
  } else {
    stats->intra_factor += 1.0;
  }

  int level_sample;
  if (seq_params->use_highbitdepth) {
    level_sample = CONVERT_TO_SHORTPTR(x->plane[0].src.buf)[0];
  } else {
    level_sample = x->plane[0].src.buf[0];
  }

  if (seq_params->use_highbitdepth) {
    switch (seq_params->bit_depth) {
      case AOM_BITS_8: break;
      case AOM_BITS_10: level_sample >>= 2; break;
      case AOM_BITS_12: level_sample >>= 4; break;
      default:
        assert(0 &&
               "seq_params->bit_depth should be AOM_BITS_8, "
               "AOM_BITS_10 or AOM_BITS_12");
        return -1;
    }
  }
  if ((level_sample < DARK_THRESH) && (log_intra < 9.0)) {
    stats->brightness_factor += 1.0 + (0.01 * (DARK_THRESH - level_sample));
  } else {
    stats->brightness_factor += 1.0;
  }

  // Intrapenalty below deals with situations where the intra and inter
  // error scores are very low (e.g. a plain black frame).
  // We do not have special cases in first pass for 0,0 and nearest etc so
  // all inter modes carry an overhead cost estimate for the mv.
  // When the error score is very low this causes us to pick all or lots of
  // INTRA modes and throw lots of key frames.
  // This penalty adds a cost matching that of a 0,0 mv to the intra case.
  this_intra_error += INTRA_MODE_PENALTY;

  // Accumulate the intra error.
  stats->intra_error += (int64_t)this_intra_error;

  // Stats based on wavelet energy is used in the following cases :
  // 1. ML model which predicts if a flat structure (golden-frame only structure
  // without ALT-REF and Internal-ARFs) is better. This ML model is enabled in
  // constant quality mode under certain conditions.
  // 2. Delta qindex mode is set as DELTA_Q_PERCEPTUAL.
  // Thus, wavelet energy calculation is enabled for the above cases.
  if (calc_wavelet_energy(&cpi->oxcf)) {
    const int hbd = is_cur_buf_hbd(xd);
    const int stride = x->plane[0].src.stride;
    const int num_8x8_rows = block_size_high[fp_block_size] / 8;
    const int num_8x8_cols = block_size_wide[fp_block_size] / 8;
    const uint8_t *buf = x->plane[0].src.buf;
    stats->frame_avg_wavelet_energy += av1_haar_ac_sad_mxn_uint8_input(
        buf, stride, hbd, num_8x8_rows, num_8x8_cols);
  } else {
    stats->frame_avg_wavelet_energy = INVALID_FP_STATS_TO_PREDICT_FLAT_GOP;
  }

  return this_intra_error;
}

// Returns the sum of square error between source and reference blocks.
static int get_prediction_error_bitdepth(const int is_high_bitdepth,
                                         const int bitdepth,
                                         const BLOCK_SIZE block_size,
                                         const struct buf_2d *src,
                                         const struct buf_2d *ref) {
  (void)is_high_bitdepth;
  (void)bitdepth;
#if CONFIG_AV1_HIGHBITDEPTH
  if (is_high_bitdepth) {
    return highbd_get_prediction_error(block_size, src, ref, bitdepth);
  }
#endif  // CONFIG_AV1_HIGHBITDEPTH
  return get_prediction_error(block_size, src, ref);
}

// Accumulates motion vector stats.
// Modifies member variables of "stats".
static void accumulate_mv_stats(const MV best_mv, const FULLPEL_MV mv,
                                const int mb_row, const int mb_col,
                                const int mb_rows, const int mb_cols,
                                MV *last_non_zero_mv, FRAME_STATS *stats) {
  if (is_zero_mv(&best_mv)) return;

  ++stats->mv_count;
  // Non-zero vector, was it different from the last non zero vector?
  if (!is_equal_mv(&best_mv, last_non_zero_mv)) ++stats->new_mv_count;
  *last_non_zero_mv = best_mv;

  // Does the row vector point inwards or outwards?
  if (mb_row < mb_rows / 2) {
    if (mv.row > 0) {
      --stats->sum_in_vectors;
    } else if (mv.row < 0) {
      ++stats->sum_in_vectors;
    }
  } else if (mb_row > mb_rows / 2) {
    if (mv.row > 0) {
      ++stats->sum_in_vectors;
    } else if (mv.row < 0) {
      --stats->sum_in_vectors;
    }
  }

  // Does the col vector point inwards or outwards?
  if (mb_col < mb_cols / 2) {
    if (mv.col > 0) {
      --stats->sum_in_vectors;
    } else if (mv.col < 0) {
      ++stats->sum_in_vectors;
    }
  } else if (mb_col > mb_cols / 2) {
    if (mv.col > 0) {
      ++stats->sum_in_vectors;
    } else if (mv.col < 0) {
      --stats->sum_in_vectors;
    }
  }
}

// Computes and returns the inter prediction error from the last frame.
// Computes inter prediction errors from the golden and alt ref frams and
// Updates stats accordingly.
// Inputs:
//   cpi: the encoder setting. Only a few params in it will be used.
//   last_frame: the frame buffer of the last frame.
//   golden_frame: the frame buffer of the golden frame.
//   unit_row: row index in the unit of first pass block size.
//   unit_col: column index in the unit of first pass block size.
//   recon_yoffset: the y offset of the reconstructed  frame buffer,
//                  indicating the starting point of the current block.
//   recont_uvoffset: the u/v offset of the reconstructed frame buffer,
//                    indicating the starting point of the current block.
//   src_yoffset: the y offset of the source frame buffer.
//   fp_block_size: first pass block size.
//   this_intra_error: the intra prediction error of this block.
//   raw_motion_err_counts: the count of raw motion vectors.
//   raw_motion_err_list: the array that records the raw motion error.
//   ref_mv: the reference used to start the motion search
//   best_mv: the best mv found
//   last_non_zero_mv: the last non zero mv found in this tile row.
//   stats: frame encoding stats.
//  Modifies:
//    raw_motion_err_list
//    best_ref_mv
//    last_mv
//    stats: many member params in it.
//  Returns:
//    this_inter_error
static int firstpass_inter_prediction(
    AV1_COMP *cpi, ThreadData *td, const YV12_BUFFER_CONFIG *const last_frame,
    const YV12_BUFFER_CONFIG *const golden_frame, const int unit_row,
    const int unit_col, const int recon_yoffset, const int recon_uvoffset,
    const int src_yoffset, const BLOCK_SIZE fp_block_size,
    const int this_intra_error, const int raw_motion_err_counts,
    int *raw_motion_err_list, const MV ref_mv, MV *best_mv,
    MV *last_non_zero_mv, FRAME_STATS *stats) {
  int this_inter_error = this_intra_error;
  AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  CurrentFrame *const current_frame = &cm->current_frame;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int is_high_bitdepth = is_cur_buf_hbd(xd);
  const int bitdepth = xd->bd;
  const int unit_scale = mi_size_wide[fp_block_size];
  const BLOCK_SIZE bsize =
      get_bsize(mi_params, fp_block_size, unit_row, unit_col);
  const int fp_block_size_height = block_size_wide[fp_block_size];
  const int unit_width = mi_size_wide[fp_block_size];
  const int unit_rows = get_unit_rows(fp_block_size, mi_params->mb_rows);
  const int unit_cols = get_unit_cols(fp_block_size, mi_params->mb_cols);
  // Assume 0,0 motion with no mv overhead.
  FULLPEL_MV mv = kZeroFullMv;
  xd->plane[0].pre[0].buf = last_frame->y_buffer + recon_yoffset;
  // Set up limit values for motion vectors to prevent them extending
  // outside the UMV borders.
  av1_set_mv_col_limits(mi_params, &x->mv_limits, unit_col * unit_width,
                        fp_block_size_height >> MI_SIZE_LOG2,
                        cpi->oxcf.border_in_pixels);

  int motion_error =
      get_prediction_error_bitdepth(is_high_bitdepth, bitdepth, bsize,
                                    &x->plane[0].src, &xd->plane[0].pre[0]);

  // Compute the motion error of the 0,0 motion using the last source
  // frame as the reference. Skip the further motion search on
  // reconstructed frame if this error is small.
  // TODO(chiyotsai): The unscaled last source might be different dimension
  // as the current source. See BUG=aomedia:3413
  struct buf_2d unscaled_last_source_buf_2d;
  unscaled_last_source_buf_2d.buf =
      cpi->unscaled_last_source->y_buffer + src_yoffset;
  unscaled_last_source_buf_2d.stride = cpi->unscaled_last_source->y_stride;
  const int raw_motion_error = get_prediction_error_bitdepth(
      is_high_bitdepth, bitdepth, bsize, &x->plane[0].src,
      &unscaled_last_source_buf_2d);
  raw_motion_err_list[raw_motion_err_counts] = raw_motion_error;
  const FIRST_PASS_SPEED_FEATURES *const fp_sf = &cpi->sf.fp_sf;

  if (raw_motion_error > fp_sf->skip_motion_search_threshold) {
    // Test last reference frame using the previous best mv as the
    // starting point (best reference) for the search.
    first_pass_motion_search(cpi, x, &ref_mv, &mv, &motion_error);

    // If the current best reference mv is not centered on 0,0 then do a
    // 0,0 based search as well.
    if ((fp_sf->skip_zeromv_motion_search == 0) && !is_zero_mv(&ref_mv)) {
      FULLPEL_MV tmp_mv = kZeroFullMv;
      int tmp_err = INT_MAX;
      first_pass_motion_search(cpi, x, &kZeroMv, &tmp_mv, &tmp_err);

      if (tmp_err < motion_error) {
        motion_error = tmp_err;
        mv = tmp_mv;
      }
    }
  }

  // Motion search in 2nd reference frame.
  int gf_motion_error = motion_error;
  if ((current_frame->frame_number > 1) && golden_frame != NULL) {
    FULLPEL_MV tmp_mv = kZeroFullMv;
    // Assume 0,0 motion with no mv overhead.
    av1_setup_pre_planes(xd, 0, golden_frame, 0, 0, NULL, 1);
    xd->plane[0].pre[0].buf += recon_yoffset;
    gf_motion_error =
        get_prediction_error_bitdepth(is_high_bitdepth, bitdepth, bsize,
                                      &x->plane[0].src, &xd->plane[0].pre[0]);
    first_pass_motion_search(cpi, x, &kZeroMv, &tmp_mv, &gf_motion_error);
  }
  if (gf_motion_error < motion_error && gf_motion_error < this_intra_error) {
    ++stats->second_ref_count;
  }
  // In accumulating a score for the 2nd reference frame take the
  // best of the motion predicted score and the intra coded error
  // (just as will be done for) accumulation of "coded_error" for
  // the last frame.
  if ((current_frame->frame_number > 1) && golden_frame != NULL) {
    stats->sr_coded_error += AOMMIN(gf_motion_error, this_intra_error);
  } else {
    // TODO(chengchen): I believe logically this should also be changed to
    // stats->sr_coded_error += AOMMIN(gf_motion_error, this_intra_error).
    stats->sr_coded_error += motion_error;
  }

  // Reset to last frame as reference buffer.
  xd->plane[0].pre[0].buf = last_frame->y_buffer + recon_yoffset;
  if (av1_num_planes(&cpi->common) > 1) {
    xd->plane[1].pre[0].buf = last_frame->u_buffer + recon_uvoffset;
    xd->plane[2].pre[0].buf = last_frame->v_buffer + recon_uvoffset;
  }

  // Start by assuming that intra mode is best.
  *best_mv = kZeroMv;

  if (motion_error <= this_intra_error) {
    // Keep a count of cases where the inter and intra were very close
    // and very low. This helps with scene cut detection for example in
    // cropped clips with black bars at the sides or top and bottom.
    if (((this_intra_error - INTRA_MODE_PENALTY) * 9 <= motion_error * 10) &&
        (this_intra_error < (2 * INTRA_MODE_PENALTY))) {
      stats->neutral_count += 1.0;
      // Also track cases where the intra is not much worse than the inter
      // and use this in limiting the GF/arf group length.
    } else if ((this_intra_error > NCOUNT_INTRA_THRESH) &&
               (this_intra_error < (NCOUNT_INTRA_FACTOR * motion_error))) {
      stats->neutral_count +=
          (double)motion_error / DOUBLE_DIVIDE_CHECK((double)this_intra_error);
    }

    *best_mv = get_mv_from_fullmv(&mv);
    this_inter_error = motion_error;
    xd->mi[0]->mode = NEWMV;
    xd->mi[0]->mv[0].as_mv = *best_mv;
    xd->mi[0]->tx_size = TX_4X4;
    xd->mi[0]->ref_frame[0] = LAST_FRAME;
    xd->mi[0]->ref_frame[1] = NONE_FRAME;

    if (fp_sf->disable_recon == 0) {
      av1_enc_build_inter_predictor(cm, xd, unit_row * unit_scale,
                                    unit_col * unit_scale, NULL, bsize,
                                    AOM_PLANE_Y, AOM_PLANE_Y);
      av1_encode_sby_pass1(cpi, x, bsize);
    }
    stats->sum_mvr += best_mv->row;
    stats->sum_mvr_abs += abs(best_mv->row);
    stats->sum_mvc += best_mv->col;
    stats->sum_mvc_abs += abs(best_mv->col);
    stats->sum_mvrs += best_mv->row * best_mv->row;
    stats->sum_mvcs += best_mv->col * best_mv->col;
    ++stats->inter_count;

    accumulate_mv_stats(*best_mv, mv, unit_row, unit_col, unit_rows, unit_cols,
                        last_non_zero_mv, stats);
  }

  return this_inter_error;
}

// Normalize the first pass stats.
// Error / counters are normalized to each MB.
// MVs are normalized to the width/height of the frame.
static void normalize_firstpass_stats(FIRSTPASS_STATS *fps,
                                      double num_mbs_16x16, double f_w,
                                      double f_h) {
  fps->coded_error /= num_mbs_16x16;
  fps->sr_coded_error /= num_mbs_16x16;
  fps->intra_error /= num_mbs_16x16;
  fps->frame_avg_wavelet_energy /= num_mbs_16x16;
  fps->log_coded_error = log1p(fps->coded_error);
  fps->log_intra_error = log1p(fps->intra_error);
  fps->MVr /= f_h;
  fps->mvr_abs /= f_h;
  fps->MVc /= f_w;
  fps->mvc_abs /= f_w;
  fps->MVrv /= (f_h * f_h);
  fps->MVcv /= (f_w * f_w);
  fps->new_mv_count /= num_mbs_16x16;
}

// Updates the first pass stats of this frame.
// Input:
//   cpi: the encoder setting. Only a few params in it will be used.
//   stats: stats accumulated for this frame.
//   raw_err_stdev: the statndard deviation for the motion error of all the
//                  inter blocks of the (0,0) motion using the last source
//                  frame as the reference.
//   frame_number: current frame number.
//   ts_duration: Duration of the frame / collection of frames.
// Updates:
//   twopass->total_stats: the accumulated stats.
//   twopass->stats_buf_ctx->stats_in_end: the pointer to the current stats,
//                                         update its value and its position
//                                         in the buffer.
static void update_firstpass_stats(AV1_COMP *cpi,
                                   const FRAME_STATS *const stats,
                                   const double raw_err_stdev,
                                   const int frame_number,
                                   const int64_t ts_duration,
                                   const BLOCK_SIZE fp_block_size) {
  TWO_PASS *twopass = &cpi->ppi->twopass;
  AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  FIRSTPASS_STATS *this_frame_stats = twopass->stats_buf_ctx->stats_in_end;
  FIRSTPASS_STATS fps;
  // The minimum error here insures some bit allocation to frames even
  // in static regions. The allocation per MB declines for larger formats
  // where the typical "real" energy per MB also falls.
  // Initial estimate here uses sqrt(mbs) to define the min_err, where the
  // number of mbs is proportional to the image area.
  const int num_mbs_16X16 = (cpi->oxcf.resize_cfg.resize_mode != RESIZE_NONE)
                                ? cpi->initial_mbs
                                : mi_params->MBs;
  // Number of actual units used in the first pass, it can be other square
  // block sizes than 16X16.
  const int num_mbs = get_num_mbs(fp_block_size, num_mbs_16X16);
  const double min_err = 200 * sqrt(num_mbs);

  fps.weight = stats->intra_factor * stats->brightness_factor;
  fps.frame = frame_number;
  fps.coded_error = (double)(stats->coded_error >> 8) + min_err;
  fps.sr_coded_error = (double)(stats->sr_coded_error >> 8) + min_err;
  fps.intra_error = (double)(stats->intra_error >> 8) + min_err;
  fps.frame_avg_wavelet_energy = (double)stats->frame_avg_wavelet_energy;
  fps.count = 1.0;
  fps.pcnt_inter = (double)stats->inter_count / num_mbs;
  fps.pcnt_second_ref = (double)stats->second_ref_count / num_mbs;
  fps.pcnt_neutral = (double)stats->neutral_count / num_mbs;
  fps.intra_skip_pct = (double)stats->intra_skip_count / num_mbs;
  fps.inactive_zone_rows = (double)stats->image_data_start_row;
  fps.inactive_zone_cols = 0.0;  // Placeholder: not currently supported.
  fps.raw_error_stdev = raw_err_stdev;
  fps.is_flash = 0;
  fps.noise_var = 0.0;
  fps.cor_coeff = 1.0;
  fps.log_coded_error = 0.0;
  fps.log_intra_error = 0.0;

  if (stats->mv_count > 0) {
    fps.MVr = (double)stats->sum_mvr / stats->mv_count;
    fps.mvr_abs = (double)stats->sum_mvr_abs / stats->mv_count;
    fps.MVc = (double)stats->sum_mvc / stats->mv_count;
    fps.mvc_abs = (double)stats->sum_mvc_abs / stats->mv_count;
    fps.MVrv = ((double)stats->sum_mvrs -
                ((double)stats->sum_mvr * stats->sum_mvr / stats->mv_count)) /
               stats->mv_count;
    fps.MVcv = ((double)stats->sum_mvcs -
                ((double)stats->sum_mvc * stats->sum_mvc / stats->mv_count)) /
               stats->mv_count;
    fps.mv_in_out_count = (double)stats->sum_in_vectors / (stats->mv_count * 2);
    fps.new_mv_count = stats->new_mv_count;
    fps.pcnt_motion = (double)stats->mv_count / num_mbs;
  } else {
    fps.MVr = 0.0;
    fps.mvr_abs = 0.0;
    fps.MVc = 0.0;
    fps.mvc_abs = 0.0;
    fps.MVrv = 0.0;
    fps.MVcv = 0.0;
    fps.mv_in_out_count = 0.0;
    fps.new_mv_count = 0.0;
    fps.pcnt_motion = 0.0;
  }

  // TODO(paulwilkins):  Handle the case when duration is set to 0, or
  // something less than the full time between subsequent values of
  // cpi->source_time_stamp.
  fps.duration = (double)ts_duration;

  normalize_firstpass_stats(&fps, num_mbs_16X16, cm->width, cm->height);

  // We will store the stats inside the persistent twopass struct (and NOT the
  // local variable 'fps'), and then cpi->output_pkt_list will point to it.
  *this_frame_stats = fps;
  if (!cpi->ppi->lap_enabled) {
    output_stats(this_frame_stats, cpi->ppi->output_pkt_list);
  } else {
    av1_firstpass_info_push(&twopass->firstpass_info, this_frame_stats);
  }
  if (cpi->ppi->twopass.stats_buf_ctx->total_stats != NULL) {
    av1_accumulate_stats(cpi->ppi->twopass.stats_buf_ctx->total_stats, &fps);
  }
  twopass->stats_buf_ctx->stats_in_end++;
  // When ducky encode is on, we always use linear buffer for stats_buf_ctx.
  if (cpi->use_ducky_encode == 0) {
    // TODO(angiebird): Figure out why first pass uses circular buffer.
    /* In the case of two pass, first pass uses it as a circular buffer,
     * when LAP is enabled it is used as a linear buffer*/
    if ((cpi->oxcf.pass == AOM_RC_FIRST_PASS) &&
        (twopass->stats_buf_ctx->stats_in_end >=
         twopass->stats_buf_ctx->stats_in_buf_end)) {
      twopass->stats_buf_ctx->stats_in_end =
          twopass->stats_buf_ctx->stats_in_start;
    }
  }
}

static void print_reconstruction_frame(
    const YV12_BUFFER_CONFIG *const last_frame, int frame_number,
    int do_print) {
  if (!do_print) return;

  char filename[512];
  FILE *recon_file;
  snprintf(filename, sizeof(filename), "enc%04d.yuv", frame_number);

  if (frame_number == 0) {
    recon_file = fopen(filename, "wb");
  } else {
    recon_file = fopen(filename, "ab");
  }

  fwrite(last_frame->buffer_alloc, last_frame->frame_size, 1, recon_file);
  fclose(recon_file);
}

static FRAME_STATS accumulate_frame_stats(FRAME_STATS *mb_stats, int mb_rows,
                                          int mb_cols) {
  FRAME_STATS stats = { 0 };
  int i, j;

  stats.image_data_start_row = INVALID_ROW;
  for (j = 0; j < mb_rows; j++) {
    for (i = 0; i < mb_cols; i++) {
      FRAME_STATS mb_stat = mb_stats[j * mb_cols + i];
      stats.brightness_factor += mb_stat.brightness_factor;
      stats.coded_error += mb_stat.coded_error;
      stats.frame_avg_wavelet_energy += mb_stat.frame_avg_wavelet_energy;
      if (stats.image_data_start_row == INVALID_ROW &&
          mb_stat.image_data_start_row != INVALID_ROW) {
        stats.image_data_start_row = mb_stat.image_data_start_row;
      }
      stats.inter_count += mb_stat.inter_count;
      stats.intra_error += mb_stat.intra_error;
      stats.intra_factor += mb_stat.intra_factor;
      stats.intra_skip_count += mb_stat.intra_skip_count;
      stats.mv_count += mb_stat.mv_count;
      stats.neutral_count += mb_stat.neutral_count;
      stats.new_mv_count += mb_stat.new_mv_count;
      stats.second_ref_count += mb_stat.second_ref_count;
      stats.sr_coded_error += mb_stat.sr_coded_error;
      stats.sum_in_vectors += mb_stat.sum_in_vectors;
      stats.sum_mvc += mb_stat.sum_mvc;
      stats.sum_mvc_abs += mb_stat.sum_mvc_abs;
      stats.sum_mvcs += mb_stat.sum_mvcs;
      stats.sum_mvr += mb_stat.sum_mvr;
      stats.sum_mvr_abs += mb_stat.sum_mvr_abs;
      stats.sum_mvrs += mb_stat.sum_mvrs;
    }
  }
  return stats;
}

static void setup_firstpass_data(AV1_COMMON *const cm,
                                 FirstPassData *firstpass_data,
                                 const int unit_rows, const int unit_cols) {
  CHECK_MEM_ERROR(cm, firstpass_data->raw_motion_err_list,
                  aom_calloc(unit_rows * unit_cols,
                             sizeof(*firstpass_data->raw_motion_err_list)));
  CHECK_MEM_ERROR(
      cm, firstpass_data->mb_stats,
      aom_calloc(unit_rows * unit_cols, sizeof(*firstpass_data->mb_stats)));
  for (int j = 0; j < unit_rows; j++) {
    for (int i = 0; i < unit_cols; i++) {
      firstpass_data->mb_stats[j * unit_cols + i].image_data_start_row =
          INVALID_ROW;
    }
  }
}

void av1_free_firstpass_data(FirstPassData *firstpass_data) {
  aom_free(firstpass_data->raw_motion_err_list);
  firstpass_data->raw_motion_err_list = NULL;
  aom_free(firstpass_data->mb_stats);
  firstpass_data->mb_stats = NULL;
}

int av1_get_unit_rows_in_tile(const TileInfo *tile,
                              const BLOCK_SIZE fp_block_size) {
  const int unit_height_log2 = mi_size_high_log2[fp_block_size];
  const int mi_rows = tile->mi_row_end - tile->mi_row_start;
  const int unit_rows = CEIL_POWER_OF_TWO(mi_rows, unit_height_log2);

  return unit_rows;
}

int av1_get_unit_cols_in_tile(const TileInfo *tile,
                              const BLOCK_SIZE fp_block_size) {
  const int unit_width_log2 = mi_size_wide_log2[fp_block_size];
  const int mi_cols = tile->mi_col_end - tile->mi_col_start;
  const int unit_cols = CEIL_POWER_OF_TWO(mi_cols, unit_width_log2);

  return unit_cols;
}

#define FIRST_PASS_ALT_REF_DISTANCE 16
static void first_pass_tile(AV1_COMP *cpi, ThreadData *td,
                            TileDataEnc *tile_data,
                            const BLOCK_SIZE fp_block_size) {
  TileInfo *tile = &tile_data->tile_info;
  const int unit_height = mi_size_high[fp_block_size];
  const int unit_height_log2 = mi_size_high_log2[fp_block_size];
  for (int mi_row = tile->mi_row_start; mi_row < tile->mi_row_end;
       mi_row += unit_height) {
    av1_first_pass_row(cpi, td, tile_data, mi_row >> unit_height_log2,
                       fp_block_size);
  }
}

static void first_pass_tiles(AV1_COMP *cpi, const BLOCK_SIZE fp_block_size) {
  AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;

  av1_alloc_src_diff_buf(cm, &cpi->td.mb);
  for (int tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (int tile_col = 0; tile_col < tile_cols; ++tile_col) {
      TileDataEnc *const tile_data =
          &cpi->tile_data[tile_row * tile_cols + tile_col];
      first_pass_tile(cpi, &cpi->td, tile_data, fp_block_size);
    }
  }
}

void av1_first_pass_row(AV1_COMP *cpi, ThreadData *td, TileDataEnc *tile_data,
                        const int unit_row, const BLOCK_SIZE fp_block_size) {
  MACROBLOCK *const x = &td->mb;
  AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const SequenceHeader *const seq_params = cm->seq_params;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  TileInfo *tile = &tile_data->tile_info;
  const int qindex = find_fp_qindex(seq_params->bit_depth);
  const int fp_block_size_width = block_size_high[fp_block_size];
  const int fp_block_size_height = block_size_wide[fp_block_size];
  const int unit_width = mi_size_wide[fp_block_size];
  const int unit_width_log2 = mi_size_wide_log2[fp_block_size];
  const int unit_height_log2 = mi_size_high_log2[fp_block_size];
  const int unit_cols = mi_params->mb_cols * 4 / unit_width;
  int raw_motion_err_counts = 0;
  int unit_row_in_tile = unit_row - (tile->mi_row_start >> unit_height_log2);
  int unit_col_start = tile->mi_col_start >> unit_width_log2;
  int unit_cols_in_tile = av1_get_unit_cols_in_tile(tile, fp_block_size);
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &mt_info->enc_row_mt;
  AV1EncRowMultiThreadSync *const row_mt_sync = &tile_data->row_mt_sync;

  const YV12_BUFFER_CONFIG *last_frame =
      av1_get_scaled_ref_frame(cpi, LAST_FRAME);
  if (!last_frame) {
    last_frame = get_ref_frame_yv12_buf(cm, LAST_FRAME);
  }
  const YV12_BUFFER_CONFIG *golden_frame =
      av1_get_scaled_ref_frame(cpi, GOLDEN_FRAME);
  if (!golden_frame) {
    golden_frame = get_ref_frame_yv12_buf(cm, GOLDEN_FRAME);
  }
  YV12_BUFFER_CONFIG *const this_frame = &cm->cur_frame->buf;

  PICK_MODE_CONTEXT *ctx = td->firstpass_ctx;
  FRAME_STATS *mb_stats =
      cpi->firstpass_data.mb_stats + unit_row * unit_cols + unit_col_start;
  int *raw_motion_err_list = cpi->firstpass_data.raw_motion_err_list +
                             unit_row * unit_cols + unit_col_start;
  MV *first_top_mv = &tile_data->firstpass_top_mv;

  for (int i = 0; i < num_planes; ++i) {
    x->plane[i].coeff = ctx->coeff[i];
    x->plane[i].qcoeff = ctx->qcoeff[i];
    x->plane[i].eobs = ctx->eobs[i];
    x->plane[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
    x->plane[i].dqcoeff = ctx->dqcoeff[i];
  }

  const int src_y_stride = cpi->source->y_stride;
  const int recon_y_stride = this_frame->y_stride;
  const int recon_uv_stride = this_frame->uv_stride;
  const int uv_mb_height =
      fp_block_size_height >> (this_frame->y_height > this_frame->uv_height);

  MV best_ref_mv = kZeroMv;
  MV last_mv;

  // Reset above block coeffs.
  xd->up_available = (unit_row_in_tile != 0);
  int recon_yoffset = (unit_row * recon_y_stride * fp_block_size_height) +
                      (unit_col_start * fp_block_size_width);
  int src_yoffset = (unit_row * src_y_stride * fp_block_size_height) +
                    (unit_col_start * fp_block_size_width);
  int recon_uvoffset = (unit_row * recon_uv_stride * uv_mb_height) +
                       (unit_col_start * uv_mb_height);

  // Set up limit values for motion vectors to prevent them extending
  // outside the UMV borders.
  av1_set_mv_row_limits(
      mi_params, &x->mv_limits, (unit_row << unit_height_log2),
      (fp_block_size_height >> MI_SIZE_LOG2), cpi->oxcf.border_in_pixels);

  av1_setup_src_planes(x, cpi->source, unit_row << unit_height_log2,
                       tile->mi_col_start, num_planes, fp_block_size);

  // Fix - zero the 16x16 block first. This ensures correct this_intra_error for
  // block sizes smaller than 16x16.
  av1_zero_array(x->plane[0].src_diff, 256);

  for (int unit_col_in_tile = 0; unit_col_in_tile < unit_cols_in_tile;
       unit_col_in_tile++) {
    const int unit_col = unit_col_start + unit_col_in_tile;

    enc_row_mt->sync_read_ptr(row_mt_sync, unit_row_in_tile, unit_col_in_tile);

#if CONFIG_MULTITHREAD
    if (cpi->ppi->p_mt_info.num_workers > 1) {
      pthread_mutex_lock(enc_row_mt->mutex_);
      bool firstpass_mt_exit = enc_row_mt->firstpass_mt_exit;
      pthread_mutex_unlock(enc_row_mt->mutex_);
      // Exit in case any worker has encountered an error.
      if (firstpass_mt_exit) return;
    }
#endif

    if (unit_col_in_tile == 0) {
      last_mv = *first_top_mv;
    }
    int this_intra_error = firstpass_intra_prediction(
        cpi, td, this_frame, tile, unit_row, unit_col, recon_yoffset,
        recon_uvoffset, fp_block_size, qindex, mb_stats);

    if (!frame_is_intra_only(cm)) {
      const int this_inter_error = firstpass_inter_prediction(
          cpi, td, last_frame, golden_frame, unit_row, unit_col, recon_yoffset,
          recon_uvoffset, src_yoffset, fp_block_size, this_intra_error,
          raw_motion_err_counts, raw_motion_err_list, best_ref_mv, &best_ref_mv,
          &last_mv, mb_stats);
      if (unit_col_in_tile == 0) {
        *first_top_mv = last_mv;
      }
      mb_stats->coded_error += this_inter_error;
      ++raw_motion_err_counts;
    } else {
      mb_stats->sr_coded_error += this_intra_error;
      mb_stats->coded_error += this_intra_error;
    }

    // Adjust to the next column of MBs.
    x->plane[0].src.buf += fp_block_size_width;
    if (num_planes > 1) {
      x->plane[1].src.buf += uv_mb_height;
      x->plane[2].src.buf += uv_mb_height;
    }

    recon_yoffset += fp_block_size_width;
    src_yoffset += fp_block_size_width;
    recon_uvoffset += uv_mb_height;
    mb_stats++;

    enc_row_mt->sync_write_ptr(row_mt_sync, unit_row_in_tile, unit_col_in_tile,
                               unit_cols_in_tile);
  }
}

void av1_noop_first_pass_frame(AV1_COMP *cpi, const int64_t ts_duration) {
  AV1_COMMON *const cm = &cpi->common;
  CurrentFrame *const current_frame = &cm->current_frame;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  int max_mb_rows = mi_params->mb_rows;
  int max_mb_cols = mi_params->mb_cols;
  if (cpi->oxcf.frm_dim_cfg.forced_max_frame_width) {
    int max_mi_cols = size_in_mi(cpi->oxcf.frm_dim_cfg.forced_max_frame_width);
    max_mb_cols = ROUND_POWER_OF_TWO(max_mi_cols, 2);
  }
  if (cpi->oxcf.frm_dim_cfg.forced_max_frame_height) {
    int max_mi_rows = size_in_mi(cpi->oxcf.frm_dim_cfg.forced_max_frame_height);
    max_mb_rows = ROUND_POWER_OF_TWO(max_mi_rows, 2);
  }
  const int unit_rows = get_unit_rows(BLOCK_16X16, max_mb_rows);
  const int unit_cols = get_unit_cols(BLOCK_16X16, max_mb_cols);
  setup_firstpass_data(cm, &cpi->firstpass_data, unit_rows, unit_cols);
  FRAME_STATS *mb_stats = cpi->firstpass_data.mb_stats;
  FRAME_STATS stats = accumulate_frame_stats(mb_stats, unit_rows, unit_cols);
  av1_free_firstpass_data(&cpi->firstpass_data);
  update_firstpass_stats(cpi, &stats, 1.0, current_frame->frame_number,
                         ts_duration, BLOCK_16X16);
}

void av1_first_pass(AV1_COMP *cpi, const int64_t ts_duration) {
  MACROBLOCK *const x = &cpi->td.mb;
  AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  CurrentFrame *const current_frame = &cm->current_frame;
  const SequenceHeader *const seq_params = cm->seq_params;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  const int qindex = find_fp_qindex(seq_params->bit_depth);
  const int ref_frame_flags_backup = cpi->ref_frame_flags;
  cpi->ref_frame_flags = av1_ref_frame_flag_list[LAST_FRAME] |
                         av1_ref_frame_flag_list[GOLDEN_FRAME];

  // Detect if the key frame is screen content type.
  if (frame_is_intra_only(cm)) {
    FeatureFlags *const features = &cm->features;
    assert(cpi->source != NULL);
    xd->cur_buf = cpi->source;
    av1_set_screen_content_options(cpi, features);
  }

  // Prepare the speed features
  av1_set_speed_features_framesize_independent(cpi, cpi->oxcf.speed);

  // Unit size for the first pass encoding.
  const BLOCK_SIZE fp_block_size =
      get_fp_block_size(cpi->is_screen_content_type);

  int max_mb_rows = mi_params->mb_rows;
  int max_mb_cols = mi_params->mb_cols;
  if (cpi->oxcf.frm_dim_cfg.forced_max_frame_width) {
    int max_mi_cols = size_in_mi(cpi->oxcf.frm_dim_cfg.forced_max_frame_width);
    max_mb_cols = ROUND_POWER_OF_TWO(max_mi_cols, 2);
  }
  if (cpi->oxcf.frm_dim_cfg.forced_max_frame_height) {
    int max_mi_rows = size_in_mi(cpi->oxcf.frm_dim_cfg.forced_max_frame_height);
    max_mb_rows = ROUND_POWER_OF_TWO(max_mi_rows, 2);
  }

  // Number of rows in the unit size.
  // Note max_mb_rows and max_mb_cols are in the unit of 16x16.
  const int unit_rows = get_unit_rows(fp_block_size, max_mb_rows);
  const int unit_cols = get_unit_cols(fp_block_size, max_mb_cols);

  // Set fp_block_size, for the convenience of multi-thread usage.
  cpi->fp_block_size = fp_block_size;

  setup_firstpass_data(cm, &cpi->firstpass_data, unit_rows, unit_cols);
  int *raw_motion_err_list = cpi->firstpass_data.raw_motion_err_list;
  FRAME_STATS *mb_stats = cpi->firstpass_data.mb_stats;

  // multi threading info
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  AV1EncRowMultiThreadInfo *const enc_row_mt = &mt_info->enc_row_mt;

  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;
  if (cpi->allocated_tiles < tile_cols * tile_rows) {
    av1_alloc_tile_data(cpi);
  }

  av1_init_tile_data(cpi);

  const YV12_BUFFER_CONFIG *last_frame = NULL;
  const YV12_BUFFER_CONFIG *golden_frame = NULL;
  if (!frame_is_intra_only(cm)) {
    av1_scale_references(cpi, EIGHTTAP_REGULAR, 0, 0);
    last_frame = av1_is_scaled(get_ref_scale_factors_const(cm, LAST_FRAME))
                     ? av1_get_scaled_ref_frame(cpi, LAST_FRAME)
                     : get_ref_frame_yv12_buf(cm, LAST_FRAME);
    golden_frame = av1_is_scaled(get_ref_scale_factors_const(cm, GOLDEN_FRAME))
                       ? av1_get_scaled_ref_frame(cpi, GOLDEN_FRAME)
                       : get_ref_frame_yv12_buf(cm, GOLDEN_FRAME);
  }

  YV12_BUFFER_CONFIG *const this_frame = &cm->cur_frame->buf;
  // First pass code requires valid last and new frame buffers.
  assert(this_frame != NULL);
  assert(frame_is_intra_only(cm) || (last_frame != NULL));

  av1_setup_frame_size(cpi);
  av1_set_mv_search_params(cpi);

  set_mi_offsets(mi_params, xd, 0, 0);
  xd->mi[0]->bsize = fp_block_size;

  // Do not use periodic key frames.
  cpi->rc.frames_to_key = INT_MAX;

  av1_set_quantizer(
      cm, cpi->oxcf.q_cfg.qm_minlevel, cpi->oxcf.q_cfg.qm_maxlevel, qindex,
      cpi->oxcf.q_cfg.enable_chroma_deltaq, cpi->oxcf.q_cfg.enable_hdr_deltaq,
      cpi->oxcf.mode == ALLINTRA, cpi->oxcf.tune_cfg.tuning);

  av1_setup_block_planes(xd, seq_params->subsampling_x,
                         seq_params->subsampling_y, num_planes);

  av1_setup_src_planes(x, cpi->source, 0, 0, num_planes, fp_block_size);
  av1_setup_dst_planes(xd->plane, seq_params->sb_size, this_frame, 0, 0, 0,
                       num_planes);

  if (!frame_is_intra_only(cm)) {
    av1_setup_pre_planes(xd, 0, last_frame, 0, 0, NULL, num_planes);
  }

  set_mi_offsets(mi_params, xd, 0, 0);

  // Don't store luma on the fist pass since chroma is not computed
  xd->cfl.store_y = 0;
  av1_frame_init_quantizer(cpi);

  av1_default_coef_probs(cm);
  av1_init_mode_probs(cm->fc);
  av1_init_mv_probs(cm);
  av1_initialize_rd_consts(cpi);

  enc_row_mt->sync_read_ptr = av1_row_mt_sync_read_dummy;
  enc_row_mt->sync_write_ptr = av1_row_mt_sync_write_dummy;

  if (mt_info->num_workers > 1) {
    enc_row_mt->sync_read_ptr = av1_row_mt_sync_read;
    enc_row_mt->sync_write_ptr = av1_row_mt_sync_write;
    av1_fp_encode_tiles_row_mt(cpi);
  } else {
    first_pass_tiles(cpi, fp_block_size);
  }

  FRAME_STATS stats = accumulate_frame_stats(mb_stats, unit_rows, unit_cols);
  int total_raw_motion_err_count =
      frame_is_intra_only(cm) ? 0 : unit_rows * unit_cols;
  const double raw_err_stdev =
      raw_motion_error_stdev(raw_motion_err_list, total_raw_motion_err_count);
  av1_free_firstpass_data(&cpi->firstpass_data);
  av1_dealloc_src_diff_buf(&cpi->td.mb, av1_num_planes(cm));

  // Clamp the image start to rows/2. This number of rows is discarded top
  // and bottom as dead data so rows / 2 means the frame is blank.
  if ((stats.image_data_start_row > unit_rows / 2) ||
      (stats.image_data_start_row == INVALID_ROW)) {
    stats.image_data_start_row = unit_rows / 2;
  }
  // Exclude any image dead zone
  if (stats.image_data_start_row > 0) {
    stats.intra_skip_count =
        AOMMAX(0, stats.intra_skip_count -
                      (stats.image_data_start_row * unit_cols * 2));
  }

  TWO_PASS *twopass = &cpi->ppi->twopass;
  const int num_mbs_16X16 = (cpi->oxcf.resize_cfg.resize_mode != RESIZE_NONE)
                                ? cpi->initial_mbs
                                : mi_params->MBs;
  // Number of actual units used in the first pass, it can be other square
  // block sizes than 16X16.
  const int num_mbs = get_num_mbs(fp_block_size, num_mbs_16X16);
  stats.intra_factor = stats.intra_factor / (double)num_mbs;
  stats.brightness_factor = stats.brightness_factor / (double)num_mbs;
  FIRSTPASS_STATS *this_frame_stats = twopass->stats_buf_ctx->stats_in_end;
  update_firstpass_stats(cpi, &stats, raw_err_stdev,
                         current_frame->frame_number, ts_duration,
                         fp_block_size);

  // Copy the previous Last Frame back into gf buffer if the prediction is good
  // enough... but also don't allow it to lag too far.
  if ((twopass->sr_update_lag > 3) ||
      ((current_frame->frame_number > 0) &&
       (this_frame_stats->pcnt_inter > 0.20) &&
       ((this_frame_stats->intra_error /
         DOUBLE_DIVIDE_CHECK(this_frame_stats->coded_error)) > 2.0))) {
    if (golden_frame != NULL) {
      assign_frame_buffer_p(
          &cm->ref_frame_map[get_ref_frame_map_idx(cm, GOLDEN_FRAME)],
          cm->ref_frame_map[get_ref_frame_map_idx(cm, LAST_FRAME)]);
    }
    twopass->sr_update_lag = 1;
  } else {
    ++twopass->sr_update_lag;
  }

  aom_extend_frame_borders(this_frame, num_planes);

  // The frame we just compressed now becomes the last frame.
  assign_frame_buffer_p(
      &cm->ref_frame_map[get_ref_frame_map_idx(cm, LAST_FRAME)], cm->cur_frame);

  // Special case for the first frame. Copy into the GF buffer as a second
  // reference.
  if (current_frame->frame_number == 0 &&
      get_ref_frame_map_idx(cm, GOLDEN_FRAME) != INVALID_IDX) {
    assign_frame_buffer_p(
        &cm->ref_frame_map[get_ref_frame_map_idx(cm, GOLDEN_FRAME)],
        cm->ref_frame_map[get_ref_frame_map_idx(cm, LAST_FRAME)]);
  }

  print_reconstruction_frame(last_frame, current_frame->frame_number,
                             /*do_print=*/0);

  ++current_frame->frame_number;
  cpi->ref_frame_flags = ref_frame_flags_backup;
  if (!frame_is_intra_only(cm)) {
    release_scaled_references(cpi);
  }
}

aom_codec_err_t av1_firstpass_info_init(FIRSTPASS_INFO *firstpass_info,
                                        FIRSTPASS_STATS *ext_stats_buf,
                                        int ext_stats_buf_size) {
  assert(IMPLIES(ext_stats_buf == NULL, ext_stats_buf_size == 0));
  if (ext_stats_buf == NULL) {
    firstpass_info->stats_buf = firstpass_info->static_stats_buf;
    firstpass_info->stats_buf_size =
        sizeof(firstpass_info->static_stats_buf) /
        sizeof(firstpass_info->static_stats_buf[0]);
    firstpass_info->start_index = 0;
    firstpass_info->cur_index = 0;
    firstpass_info->stats_count = 0;
    firstpass_info->future_stats_count = 0;
    firstpass_info->past_stats_count = 0;
    av1_zero(firstpass_info->total_stats);
    if (ext_stats_buf_size == 0) {
      return AOM_CODEC_OK;
    } else {
      return AOM_CODEC_ERROR;
    }
  } else {
    firstpass_info->stats_buf = ext_stats_buf;
    firstpass_info->stats_buf_size = ext_stats_buf_size;
    firstpass_info->start_index = 0;
    firstpass_info->cur_index = 0;
    firstpass_info->stats_count = firstpass_info->stats_buf_size;
    firstpass_info->future_stats_count = firstpass_info->stats_count;
    firstpass_info->past_stats_count = 0;
    av1_zero(firstpass_info->total_stats);
    for (int i = 0; i < firstpass_info->stats_count; ++i) {
      av1_accumulate_stats(&firstpass_info->total_stats,
                           &firstpass_info->stats_buf[i]);
    }
  }
  return AOM_CODEC_OK;
}

aom_codec_err_t av1_firstpass_info_move_cur_index(
    FIRSTPASS_INFO *firstpass_info) {
  assert(firstpass_info->future_stats_count +
             firstpass_info->past_stats_count ==
         firstpass_info->stats_count);
  if (firstpass_info->future_stats_count > 1) {
    firstpass_info->cur_index =
        (firstpass_info->cur_index + 1) % firstpass_info->stats_buf_size;
    --firstpass_info->future_stats_count;
    ++firstpass_info->past_stats_count;
    return AOM_CODEC_OK;
  } else {
    return AOM_CODEC_ERROR;
  }
}

aom_codec_err_t av1_firstpass_info_pop(FIRSTPASS_INFO *firstpass_info) {
  if (firstpass_info->stats_count > 0 && firstpass_info->past_stats_count > 0) {
    const int next_start =
        (firstpass_info->start_index + 1) % firstpass_info->stats_buf_size;
    firstpass_info->start_index = next_start;
    --firstpass_info->stats_count;
    --firstpass_info->past_stats_count;
    return AOM_CODEC_OK;
  } else {
    return AOM_CODEC_ERROR;
  }
}

aom_codec_err_t av1_firstpass_info_move_cur_index_and_pop(
    FIRSTPASS_INFO *firstpass_info) {
  aom_codec_err_t ret = av1_firstpass_info_move_cur_index(firstpass_info);
  if (ret != AOM_CODEC_OK) return ret;
  ret = av1_firstpass_info_pop(firstpass_info);
  return ret;
}

aom_codec_err_t av1_firstpass_info_push(FIRSTPASS_INFO *firstpass_info,
                                        const FIRSTPASS_STATS *input_stats) {
  if (firstpass_info->stats_count < firstpass_info->stats_buf_size) {
    const int next_index =
        (firstpass_info->start_index + firstpass_info->stats_count) %
        firstpass_info->stats_buf_size;
    firstpass_info->stats_buf[next_index] = *input_stats;
    ++firstpass_info->stats_count;
    ++firstpass_info->future_stats_count;
    av1_accumulate_stats(&firstpass_info->total_stats, input_stats);
    return AOM_CODEC_OK;
  } else {
    return AOM_CODEC_ERROR;
  }
}

const FIRSTPASS_STATS *av1_firstpass_info_peek(
    const FIRSTPASS_INFO *firstpass_info, int offset_from_cur) {
  if (offset_from_cur >= -firstpass_info->past_stats_count &&
      offset_from_cur < firstpass_info->future_stats_count) {
    const int index = (firstpass_info->cur_index + offset_from_cur) %
                      firstpass_info->stats_buf_size;
    return &firstpass_info->stats_buf[index];
  } else {
    return NULL;
  }
}

int av1_firstpass_info_future_count(const FIRSTPASS_INFO *firstpass_info,
                                    int offset_from_cur) {
  if (offset_from_cur < firstpass_info->future_stats_count) {
    return firstpass_info->future_stats_count - offset_from_cur;
  }
  return 0;
}
