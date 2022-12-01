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

#include "config/aom_config.h"

#if CONFIG_TFLITE
#include "tensorflow/lite/c/c_api.h"
#include "av1/encoder/deltaq4_model.c"
#endif

#include "av1/common/common_data.h"
#include "av1/common/enums.h"
#include "av1/common/idct.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/allintra_vis.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/model_rd.h"
#include "av1/encoder/rdopt_utils.h"

// Process the wiener variance in 16x16 block basis.
static int qsort_comp(const void *elem1, const void *elem2) {
  int a = *((const int *)elem1);
  int b = *((const int *)elem2);
  if (a > b) return 1;
  if (a < b) return -1;
  return 0;
}

void av1_init_mb_wiener_var_buffer(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;

  cpi->weber_bsize = BLOCK_8X8;

  if (cpi->mb_weber_stats) return;

  CHECK_MEM_ERROR(cm, cpi->mb_weber_stats,
                  aom_calloc(cpi->frame_info.mi_rows * cpi->frame_info.mi_cols,
                             sizeof(*cpi->mb_weber_stats)));
}

static int64_t get_satd(AV1_COMP *const cpi, BLOCK_SIZE bsize, int mi_row,
                        int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  const int mi_step = mi_size_wide[cpi->weber_bsize];
  int mb_stride = cpi->frame_info.mi_cols;
  int mb_count = 0;
  int64_t satd = 0;

  for (int row = mi_row; row < mi_row + mi_high; row += mi_step) {
    for (int col = mi_col; col < mi_col + mi_wide; col += mi_step) {
      if (row >= cm->mi_params.mi_rows || col >= cm->mi_params.mi_cols)
        continue;

      satd += cpi->mb_weber_stats[(row / mi_step) * mb_stride + (col / mi_step)]
                  .satd;
      ++mb_count;
    }
  }

  if (mb_count) satd = (int)(satd / mb_count);
  satd = AOMMAX(1, satd);

  return (int)satd;
}

static int64_t get_sse(AV1_COMP *const cpi, BLOCK_SIZE bsize, int mi_row,
                       int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  const int mi_step = mi_size_wide[cpi->weber_bsize];
  int mb_stride = cpi->frame_info.mi_cols;
  int mb_count = 0;
  int64_t distortion = 0;

  for (int row = mi_row; row < mi_row + mi_high; row += mi_step) {
    for (int col = mi_col; col < mi_col + mi_wide; col += mi_step) {
      if (row >= cm->mi_params.mi_rows || col >= cm->mi_params.mi_cols)
        continue;

      distortion +=
          cpi->mb_weber_stats[(row / mi_step) * mb_stride + (col / mi_step)]
              .distortion;
      ++mb_count;
    }
  }

  if (mb_count) distortion = (int)(distortion / mb_count);
  distortion = AOMMAX(1, distortion);

  return (int)distortion;
}

static double get_max_scale(AV1_COMP *const cpi, BLOCK_SIZE bsize, int mi_row,
                            int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];
  const int mi_step = mi_size_wide[cpi->weber_bsize];
  int mb_stride = cpi->frame_info.mi_cols;
  double min_max_scale = 10.0;

  for (int row = mi_row; row < mi_row + mi_high; row += mi_step) {
    for (int col = mi_col; col < mi_col + mi_wide; col += mi_step) {
      if (row >= cm->mi_params.mi_rows || col >= cm->mi_params.mi_cols)
        continue;
      WeberStats *weber_stats =
          &cpi->mb_weber_stats[(row / mi_step) * mb_stride + (col / mi_step)];
      if (weber_stats->max_scale < 1.0) continue;
      if (weber_stats->max_scale < min_max_scale)
        min_max_scale = weber_stats->max_scale;
    }
  }
  return min_max_scale;
}

static int get_window_wiener_var(AV1_COMP *const cpi, BLOCK_SIZE bsize,
                                 int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  const int mi_step = mi_size_wide[cpi->weber_bsize];
  int sb_wiener_var = 0;
  int mb_stride = cpi->frame_info.mi_cols;
  int mb_count = 0;
  double base_num = 1;
  double base_den = 1;
  double base_reg = 1;

  for (int row = mi_row; row < mi_row + mi_high; row += mi_step) {
    for (int col = mi_col; col < mi_col + mi_wide; col += mi_step) {
      if (row >= cm->mi_params.mi_rows || col >= cm->mi_params.mi_cols)
        continue;

      WeberStats *weber_stats =
          &cpi->mb_weber_stats[(row / mi_step) * mb_stride + (col / mi_step)];

      base_num += ((double)weber_stats->distortion) *
                  sqrt((double)weber_stats->src_variance) *
                  weber_stats->rec_pix_max;

      base_den += fabs(
          weber_stats->rec_pix_max * sqrt((double)weber_stats->src_variance) -
          weber_stats->src_pix_max * sqrt((double)weber_stats->rec_variance));

      base_reg += sqrt((double)weber_stats->distortion) *
                  sqrt((double)weber_stats->src_pix_max) * 0.1;
      ++mb_count;
    }
  }

  sb_wiener_var =
      (int)(((base_num + base_reg) / (base_den + base_reg)) / mb_count);
  sb_wiener_var = AOMMAX(1, sb_wiener_var);

  return (int)sb_wiener_var;
}

static int get_var_perceptual_ai(AV1_COMP *const cpi, BLOCK_SIZE bsize,
                                 int mi_row, int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  int sb_wiener_var = get_window_wiener_var(cpi, bsize, mi_row, mi_col);

  if (mi_row >= (mi_high / 2)) {
    sb_wiener_var =
        AOMMIN(sb_wiener_var,
               get_window_wiener_var(cpi, bsize, mi_row - mi_high / 2, mi_col));
  }
  if (mi_row <= (cm->mi_params.mi_rows - mi_high - (mi_high / 2))) {
    sb_wiener_var =
        AOMMIN(sb_wiener_var,
               get_window_wiener_var(cpi, bsize, mi_row + mi_high / 2, mi_col));
  }
  if (mi_col >= (mi_wide / 2)) {
    sb_wiener_var =
        AOMMIN(sb_wiener_var,
               get_window_wiener_var(cpi, bsize, mi_row, mi_col - mi_wide / 2));
  }
  if (mi_col <= (cm->mi_params.mi_cols - mi_wide - (mi_wide / 2))) {
    sb_wiener_var =
        AOMMIN(sb_wiener_var,
               get_window_wiener_var(cpi, bsize, mi_row, mi_col + mi_wide / 2));
  }

  return sb_wiener_var;
}

static double calc_src_mean_var(const uint8_t *const src_buffer,
                                const int buf_stride, const int block_size,
                                const int use_hbd, double *mean) {
  double src_mean = 0.0;
  double src_variance = 0.0;
  for (int pix_row = 0; pix_row < block_size; ++pix_row) {
    for (int pix_col = 0; pix_col < block_size; ++pix_col) {
      int src_pix;
      if (use_hbd) {
        const uint16_t *src = CONVERT_TO_SHORTPTR(src_buffer);
        src_pix = src[pix_row * buf_stride + pix_col];
      } else {
        src_pix = src_buffer[pix_row * buf_stride + pix_col];
      }
      src_mean += src_pix;
      src_variance += src_pix * src_pix;
    }
  }
  const int pix_num = block_size * block_size;
  src_variance -= (src_mean * src_mean) / pix_num;
  src_variance /= pix_num;
  *mean = src_mean / pix_num;
  return src_variance;
}

static BLOCK_SIZE pick_block_size(AV1_COMP *cpi,
                                  const BLOCK_SIZE orig_block_size) {
  const BLOCK_SIZE sub_block_size =
      get_partition_subsize(orig_block_size, PARTITION_SPLIT);
  const int mb_step = mi_size_wide[orig_block_size];
  const int sub_step = mb_step >> 1;
  const TX_SIZE tx_size = max_txsize_lookup[orig_block_size];
  const int block_size = tx_size_wide[tx_size];
  const int split_block_size = block_size >> 1;
  assert(split_block_size >= 8);
  const uint8_t *const buffer = cpi->source->y_buffer;
  const int buf_stride = cpi->source->y_stride;
  const int use_hbd = cpi->source->flags & YV12_FLAG_HIGHBITDEPTH;

  double vote = 0.0;
  for (int mi_row = 0; mi_row < cpi->frame_info.mi_rows; mi_row += mb_step) {
    for (int mi_col = 0; mi_col < cpi->frame_info.mi_cols; mi_col += mb_step) {
      const uint8_t *mb_buffer =
          buffer + mi_row * MI_SIZE * buf_stride + mi_col * MI_SIZE;
      // (1). Calculate mean and var using the original block size
      double mean = 0.0;
      const double orig_var =
          calc_src_mean_var(mb_buffer, buf_stride, block_size, use_hbd, &mean);
      // (2). Calculate mean and var using the split block size
      double split_var[4] = { 0 };
      double split_mean[4] = { 0 };
      int sub_idx = 0;
      for (int row = mi_row; row < mi_row + mb_step; row += sub_step) {
        for (int col = mi_col; col < mi_col + mb_step; col += sub_step) {
          mb_buffer = buffer + row * MI_SIZE * buf_stride + col * MI_SIZE;
          split_var[sub_idx] =
              calc_src_mean_var(mb_buffer, buf_stride, split_block_size,
                                use_hbd, &split_mean[sub_idx]);
          ++sub_idx;
        }
      }
      // (3). Determine whether to use the original or the split block size.
      // If use original, vote += 1.0.
      // If use split, vote -= 1.0.
      double max_split_mean = 0.0;
      double max_split_var = 0.0;
      double geo_split_var = 0.0;
      for (int i = 0; i < 4; ++i) {
        max_split_mean = AOMMAX(max_split_mean, split_mean[i]);
        max_split_var = AOMMAX(max_split_var, split_var[i]);
        geo_split_var += log(0.1 + split_var[i]);
      }
      geo_split_var = exp(geo_split_var / 4);
      const double param_1 = 1.5;
      const double param_2 = 1.0;
      // If the variance of the large block size is considerably larger than the
      // geometric mean of vars of small blocks;
      // Or if the variance of the large block size is larger than the local
      // variance;
      // Or if the variance of the large block size is considerably larger
      // than the mean.
      // It indicates that the source block is not a flat area, therefore we
      // might want to split into smaller block sizes to capture the
      // local characteristics.
      if (orig_var > param_1 * geo_split_var || orig_var > max_split_var ||
          sqrt(orig_var) > param_2 * mean) {
        vote -= 1.0;
      } else {
        vote += 1.0;
      }
    }
  }

  return vote > 0.0 ? orig_block_size : sub_block_size;
}

static int64_t pick_norm_factor_and_block_size(AV1_COMP *const cpi,
                                               BLOCK_SIZE *best_block_size) {
  const AV1_COMMON *const cm = &cpi->common;
  const BLOCK_SIZE sb_size = cm->seq_params->sb_size;
  BLOCK_SIZE last_block_size;
  BLOCK_SIZE this_block_size = sb_size;
  *best_block_size = sb_size;
  // Pick from block size 128x128, 64x64, 32x32 and 16x16.
  do {
    last_block_size = this_block_size;
    assert(this_block_size >= BLOCK_16X16 && this_block_size <= BLOCK_128X128);
    const int block_size = block_size_wide[this_block_size];
    if (block_size < 32) break;
    this_block_size = pick_block_size(cpi, last_block_size);
  } while (this_block_size != last_block_size);
  *best_block_size = this_block_size;

  int64_t norm_factor = 1;
  const BLOCK_SIZE norm_block_size = this_block_size;
  assert(norm_block_size >= BLOCK_16X16 && norm_block_size <= BLOCK_128X128);
  const int norm_step = mi_size_wide[norm_block_size];
  double sb_wiener_log = 0;
  double sb_count = 0;
  for (int mi_row = 0; mi_row < cm->mi_params.mi_rows; mi_row += norm_step) {
    for (int mi_col = 0; mi_col < cm->mi_params.mi_cols; mi_col += norm_step) {
      const int sb_wiener_var =
          get_var_perceptual_ai(cpi, norm_block_size, mi_row, mi_col);
      const int64_t satd = get_satd(cpi, norm_block_size, mi_row, mi_col);
      const int64_t sse = get_sse(cpi, norm_block_size, mi_row, mi_col);
      const double scaled_satd = (double)satd / sqrt((double)sse);
      sb_wiener_log += scaled_satd * log(sb_wiener_var);
      sb_count += scaled_satd;
    }
  }
  if (sb_count > 0) norm_factor = (int64_t)(exp(sb_wiener_log / sb_count));
  norm_factor = AOMMAX(1, norm_factor);

  return norm_factor;
}

static void automatic_intra_tools_off(AV1_COMP *cpi,
                                      const double sum_rec_distortion,
                                      const double sum_est_rate) {
  if (!cpi->oxcf.intra_mode_cfg.auto_intra_tools_off) return;

  // Thresholds
  const int high_quality_qindex = 128;
  const double high_quality_bpp = 2.0;
  const double high_quality_dist_per_pix = 4.0;

  AV1_COMMON *const cm = &cpi->common;
  const int qindex = cm->quant_params.base_qindex;
  const double dist_per_pix =
      (double)sum_rec_distortion / (cm->width * cm->height);
  // The estimate bpp is not accurate, an empirical constant 100 is divided.
  const double estimate_bpp = sum_est_rate / (cm->width * cm->height * 100);

  if (qindex < high_quality_qindex && estimate_bpp > high_quality_bpp &&
      dist_per_pix < high_quality_dist_per_pix) {
    cpi->oxcf.intra_mode_cfg.enable_smooth_intra = 0;
    cpi->oxcf.intra_mode_cfg.enable_paeth_intra = 0;
    cpi->oxcf.intra_mode_cfg.enable_cfl_intra = 0;
    cpi->oxcf.intra_mode_cfg.enable_diagonal_intra = 0;
  }
}

void av1_set_mb_wiener_variance(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  uint8_t *buffer = cpi->source->y_buffer;
  int buf_stride = cpi->source->y_stride;
  ThreadData *td = &cpi->td;
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO mbmi;
  memset(&mbmi, 0, sizeof(mbmi));
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  xd->mi = &mbmi_ptr;

  const SequenceHeader *const seq_params = cm->seq_params;
  if (aom_realloc_frame_buffer(
          &cm->cur_frame->buf, cm->width, cm->height, seq_params->subsampling_x,
          seq_params->subsampling_y, seq_params->use_highbitdepth,
          cpi->oxcf.border_in_pixels, cm->features.byte_alignment, NULL, NULL,
          NULL, cpi->oxcf.tool_cfg.enable_global_motion, 0))
    aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");

  cm->quant_params.base_qindex = cpi->oxcf.rc_cfg.cq_level;
  av1_frame_init_quantizer(cpi);

  DECLARE_ALIGNED(32, int16_t, src_diff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, qcoeff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, dqcoeff[32 * 32]);

  int mi_row, mi_col;

  BLOCK_SIZE bsize = cpi->weber_bsize;
  const TX_SIZE tx_size = max_txsize_lookup[bsize];
  const int block_size = tx_size_wide[tx_size];
  const int coeff_count = block_size * block_size;

  const BitDepthInfo bd_info = get_bit_depth_info(xd);
  cpi->norm_wiener_variance = 0;
  int mb_step = mi_size_wide[bsize];

  double sum_rec_distortion = 0.0;
  double sum_est_rate = 0.0;
  for (mi_row = 0; mi_row < cpi->frame_info.mi_rows; mi_row += mb_step) {
    for (mi_col = 0; mi_col < cpi->frame_info.mi_cols; mi_col += mb_step) {
      PREDICTION_MODE best_mode = DC_PRED;
      int best_intra_cost = INT_MAX;

      xd->up_available = mi_row > 0;
      xd->left_available = mi_col > 0;

      const int mi_width = mi_size_wide[bsize];
      const int mi_height = mi_size_high[bsize];
      set_mode_info_offsets(&cpi->common.mi_params, &cpi->mbmi_ext_info, x, xd,
                            mi_row, mi_col);
      set_mi_row_col(xd, &xd->tile, mi_row, mi_height, mi_col, mi_width,
                     cm->mi_params.mi_rows, cm->mi_params.mi_cols);
      set_plane_n4(xd, mi_size_wide[bsize], mi_size_high[bsize],
                   av1_num_planes(cm));
      xd->mi[0]->bsize = bsize;
      xd->mi[0]->motion_mode = SIMPLE_TRANSLATION;

      av1_setup_dst_planes(xd->plane, bsize, &cm->cur_frame->buf, mi_row,
                           mi_col, 0, av1_num_planes(cm));

      int dst_buffer_stride = xd->plane[0].dst.stride;
      uint8_t *dst_buffer = xd->plane[0].dst.buf;
      uint8_t *mb_buffer =
          buffer + mi_row * MI_SIZE * buf_stride + mi_col * MI_SIZE;

      for (PREDICTION_MODE mode = INTRA_MODE_START; mode < INTRA_MODE_END;
           ++mode) {
        av1_predict_intra_block(
            xd, cm->seq_params->sb_size,
            cm->seq_params->enable_intra_edge_filter, block_size, block_size,
            tx_size, mode, 0, 0, FILTER_INTRA_MODES, dst_buffer,
            dst_buffer_stride, dst_buffer, dst_buffer_stride, 0, 0, 0);

        av1_subtract_block(bd_info, block_size, block_size, src_diff,
                           block_size, mb_buffer, buf_stride, dst_buffer,
                           dst_buffer_stride);
        av1_quick_txfm(0, tx_size, bd_info, src_diff, block_size, coeff);
        int intra_cost = aom_satd(coeff, coeff_count);
        if (intra_cost < best_intra_cost) {
          best_intra_cost = intra_cost;
          best_mode = mode;
        }
      }

      int idx;
      av1_predict_intra_block(xd, cm->seq_params->sb_size,
                              cm->seq_params->enable_intra_edge_filter,
                              block_size, block_size, tx_size, best_mode, 0, 0,
                              FILTER_INTRA_MODES, dst_buffer, dst_buffer_stride,
                              dst_buffer, dst_buffer_stride, 0, 0, 0);
      av1_subtract_block(bd_info, block_size, block_size, src_diff, block_size,
                         mb_buffer, buf_stride, dst_buffer, dst_buffer_stride);
      av1_quick_txfm(0, tx_size, bd_info, src_diff, block_size, coeff);

      const struct macroblock_plane *const p = &x->plane[0];
      uint16_t eob;
      const SCAN_ORDER *const scan_order = &av1_scan_orders[tx_size][DCT_DCT];
      QUANT_PARAM quant_param;
      int pix_num = 1 << num_pels_log2_lookup[txsize_to_bsize[tx_size]];
      av1_setup_quant(tx_size, 0, AV1_XFORM_QUANT_FP, 0, &quant_param);
#if CONFIG_AV1_HIGHBITDEPTH
      if (is_cur_buf_hbd(xd)) {
        av1_highbd_quantize_fp_facade(coeff, pix_num, p, qcoeff, dqcoeff, &eob,
                                      scan_order, &quant_param);
      } else {
        av1_quantize_fp_facade(coeff, pix_num, p, qcoeff, dqcoeff, &eob,
                               scan_order, &quant_param);
      }
#else
      av1_quantize_fp_facade(coeff, pix_num, p, qcoeff, dqcoeff, &eob,
                             scan_order, &quant_param);
#endif  // CONFIG_AV1_HIGHBITDEPTH
      av1_inverse_transform_block(xd, dqcoeff, 0, DCT_DCT, tx_size, dst_buffer,
                                  dst_buffer_stride, eob, 0);
      WeberStats *weber_stats =
          &cpi->mb_weber_stats[(mi_row / mb_step) * cpi->frame_info.mi_cols +
                               (mi_col / mb_step)];

      weber_stats->rec_pix_max = 1;
      weber_stats->rec_variance = 0;
      weber_stats->src_pix_max = 1;
      weber_stats->src_variance = 0;
      weber_stats->distortion = 0;

      int64_t src_mean = 0;
      int64_t rec_mean = 0;
      int64_t dist_mean = 0;

      for (int pix_row = 0; pix_row < block_size; ++pix_row) {
        for (int pix_col = 0; pix_col < block_size; ++pix_col) {
          int src_pix, rec_pix;
#if CONFIG_AV1_HIGHBITDEPTH
          if (is_cur_buf_hbd(xd)) {
            uint16_t *src = CONVERT_TO_SHORTPTR(mb_buffer);
            uint16_t *rec = CONVERT_TO_SHORTPTR(dst_buffer);
            src_pix = src[pix_row * buf_stride + pix_col];
            rec_pix = rec[pix_row * dst_buffer_stride + pix_col];
          } else {
            src_pix = mb_buffer[pix_row * buf_stride + pix_col];
            rec_pix = dst_buffer[pix_row * dst_buffer_stride + pix_col];
          }
#else
          src_pix = mb_buffer[pix_row * buf_stride + pix_col];
          rec_pix = dst_buffer[pix_row * dst_buffer_stride + pix_col];
#endif
          src_mean += src_pix;
          rec_mean += rec_pix;
          dist_mean += src_pix - rec_pix;
          weber_stats->src_variance += src_pix * src_pix;
          weber_stats->rec_variance += rec_pix * rec_pix;
          weber_stats->src_pix_max = AOMMAX(weber_stats->src_pix_max, src_pix);
          weber_stats->rec_pix_max = AOMMAX(weber_stats->rec_pix_max, rec_pix);
          weber_stats->distortion += (src_pix - rec_pix) * (src_pix - rec_pix);
        }
      }

      sum_rec_distortion += weber_stats->distortion;
      int est_block_rate = 0;
      int64_t est_block_dist = 0;
      model_rd_sse_fn[MODELRD_LEGACY](cpi, x, bsize, 0, weber_stats->distortion,
                                      pix_num, &est_block_rate,
                                      &est_block_dist);
      sum_est_rate += est_block_rate;

      weber_stats->src_variance -= (src_mean * src_mean) / pix_num;
      weber_stats->rec_variance -= (rec_mean * rec_mean) / pix_num;
      weber_stats->distortion -= (dist_mean * dist_mean) / pix_num;
      weber_stats->satd = best_intra_cost;

      qcoeff[0] = 0;
      for (idx = 1; idx < coeff_count; ++idx) qcoeff[idx] = abs(qcoeff[idx]);
      qsort(qcoeff, coeff_count, sizeof(*coeff), qsort_comp);

      weber_stats->max_scale = (double)qcoeff[coeff_count - 1];
    }
  }

  // Determine whether to turn off several intra coding tools.
  automatic_intra_tools_off(cpi, sum_rec_distortion, sum_est_rate);

  BLOCK_SIZE norm_block_size = BLOCK_16X16;
  cpi->norm_wiener_variance =
      pick_norm_factor_and_block_size(cpi, &norm_block_size);
  const int norm_step = mi_size_wide[norm_block_size];

  double sb_wiener_log = 0;
  double sb_count = 0;
  for (int its_cnt = 0; its_cnt < 2; ++its_cnt) {
    sb_wiener_log = 0;
    sb_count = 0;
    for (mi_row = 0; mi_row < cm->mi_params.mi_rows; mi_row += norm_step) {
      for (mi_col = 0; mi_col < cm->mi_params.mi_cols; mi_col += norm_step) {
        int sb_wiener_var =
            get_var_perceptual_ai(cpi, norm_block_size, mi_row, mi_col);

        double beta = (double)cpi->norm_wiener_variance / sb_wiener_var;
        double min_max_scale = AOMMAX(
            1.0, get_max_scale(cpi, cm->seq_params->sb_size, mi_row, mi_col));
        beta = 1.0 / AOMMIN(1.0 / beta, min_max_scale);
        beta = AOMMIN(beta, 4);
        beta = AOMMAX(beta, 0.25);

        sb_wiener_var = (int)(cpi->norm_wiener_variance / beta);

        int64_t satd = get_satd(cpi, norm_block_size, mi_row, mi_col);
        int64_t sse = get_sse(cpi, norm_block_size, mi_row, mi_col);
        double scaled_satd = (double)satd / sqrt((double)sse);
        sb_wiener_log += scaled_satd * log(sb_wiener_var);
        sb_count += scaled_satd;
      }
    }

    if (sb_count > 0)
      cpi->norm_wiener_variance = (int64_t)(exp(sb_wiener_log / sb_count));
    cpi->norm_wiener_variance = AOMMAX(1, cpi->norm_wiener_variance);
  }

  aom_free_frame_buffer(&cm->cur_frame->buf);
}

int av1_get_sbq_perceptual_ai(AV1_COMP *const cpi, BLOCK_SIZE bsize, int mi_row,
                              int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int base_qindex = cm->quant_params.base_qindex;
  int sb_wiener_var = get_var_perceptual_ai(cpi, bsize, mi_row, mi_col);
  int offset = 0;
  double beta = (double)cpi->norm_wiener_variance / sb_wiener_var;
  double min_max_scale = AOMMAX(1.0, get_max_scale(cpi, bsize, mi_row, mi_col));
  beta = 1.0 / AOMMIN(1.0 / beta, min_max_scale);

  // Cap beta such that the delta q value is not much far away from the base q.
  beta = AOMMIN(beta, 4);
  beta = AOMMAX(beta, 0.25);
  offset = av1_get_deltaq_offset(cm->seq_params->bit_depth, base_qindex, beta);
  const DeltaQInfo *const delta_q_info = &cm->delta_q_info;
  offset = AOMMIN(offset, delta_q_info->delta_q_res * 20 - 1);
  offset = AOMMAX(offset, -delta_q_info->delta_q_res * 20 + 1);
  int qindex = cm->quant_params.base_qindex + offset;
  qindex = AOMMIN(qindex, MAXQ);
  qindex = AOMMAX(qindex, MINQ);
  if (base_qindex > MINQ) qindex = AOMMAX(qindex, MINQ + 1);

  return qindex;
}

void av1_init_mb_ur_var_buffer(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;

  if (cpi->mb_delta_q) return;

  CHECK_MEM_ERROR(cm, cpi->mb_delta_q,
                  aom_calloc(cpi->frame_info.mb_rows * cpi->frame_info.mb_cols,
                             sizeof(*cpi->mb_delta_q)));
}

#if CONFIG_TFLITE
static int model_predict(BLOCK_SIZE block_size, int num_cols, int num_rows,
                         int bit_depth, uint8_t *y_buffer, int y_stride,
                         float *predicts0, float *predicts1) {
  // Create the model and interpreter options.
  TfLiteModel *model =
      TfLiteModelCreate(av1_deltaq4_model_file, av1_deltaq4_model_fsize);
  if (model == NULL) return 1;

  TfLiteInterpreterOptions *options = TfLiteInterpreterOptionsCreate();
  TfLiteInterpreterOptionsSetNumThreads(options, 2);
  if (options == NULL) {
    TfLiteModelDelete(model);
    return 1;
  }

  // Create the interpreter.
  TfLiteInterpreter *interpreter = TfLiteInterpreterCreate(model, options);
  if (interpreter == NULL) {
    TfLiteInterpreterOptionsDelete(options);
    TfLiteModelDelete(model);
    return 1;
  }

  // Allocate tensors and populate the input tensor data.
  TfLiteInterpreterAllocateTensors(interpreter);
  TfLiteTensor *input_tensor = TfLiteInterpreterGetInputTensor(interpreter, 0);
  if (input_tensor == NULL) {
    TfLiteInterpreterDelete(interpreter);
    TfLiteInterpreterOptionsDelete(options);
    TfLiteModelDelete(model);
    return 1;
  }

  size_t input_size = TfLiteTensorByteSize(input_tensor);
  float *input_data = aom_calloc(input_size, 1);
  if (input_data == NULL) {
    TfLiteInterpreterDelete(interpreter);
    TfLiteInterpreterOptionsDelete(options);
    TfLiteModelDelete(model);
    return 1;
  }

  const int num_mi_w = mi_size_wide[block_size];
  const int num_mi_h = mi_size_high[block_size];
  for (int row = 0; row < num_rows; ++row) {
    for (int col = 0; col < num_cols; ++col) {
      const int row_offset = (row * num_mi_h) << 2;
      const int col_offset = (col * num_mi_w) << 2;

      uint8_t *buf = y_buffer + row_offset * y_stride + col_offset;
      int r = row_offset, pos = 0;
      const float base = (float)((1 << bit_depth) - 1);
      while (r < row_offset + (num_mi_h << 2)) {
        for (int c = 0; c < (num_mi_w << 2); ++c) {
          input_data[pos++] = bit_depth > 8
                                  ? (float)*CONVERT_TO_SHORTPTR(buf + c) / base
                                  : (float)*(buf + c) / base;
        }
        buf += y_stride;
        ++r;
      }
      TfLiteTensorCopyFromBuffer(input_tensor, input_data, input_size);

      // Execute inference.
      if (TfLiteInterpreterInvoke(interpreter) != kTfLiteOk) {
        TfLiteInterpreterDelete(interpreter);
        TfLiteInterpreterOptionsDelete(options);
        TfLiteModelDelete(model);
        return 1;
      }

      // Extract the output tensor data.
      const TfLiteTensor *output_tensor =
          TfLiteInterpreterGetOutputTensor(interpreter, 0);
      if (output_tensor == NULL) {
        TfLiteInterpreterDelete(interpreter);
        TfLiteInterpreterOptionsDelete(options);
        TfLiteModelDelete(model);
        return 1;
      }

      size_t output_size = TfLiteTensorByteSize(output_tensor);
      float output_data[2];

      TfLiteTensorCopyToBuffer(output_tensor, output_data, output_size);
      predicts0[row * num_cols + col] = output_data[0];
      predicts1[row * num_cols + col] = output_data[1];
    }
  }

  // Dispose of the model and interpreter objects.
  TfLiteInterpreterDelete(interpreter);
  TfLiteInterpreterOptionsDelete(options);
  TfLiteModelDelete(model);
  aom_free(input_data);
  return 0;
}

void av1_set_mb_ur_variance(AV1_COMP *cpi) {
  const AV1_COMMON *cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  uint8_t *y_buffer = cpi->source->y_buffer;
  const int y_stride = cpi->source->y_stride;
  const int block_size = cpi->common.seq_params->sb_size;
  const uint32_t bit_depth = cpi->td.mb.e_mbd.bd;

  const int num_mi_w = mi_size_wide[block_size];
  const int num_mi_h = mi_size_high[block_size];
  const int num_cols = (mi_params->mi_cols + num_mi_w - 1) / num_mi_w;
  const int num_rows = (mi_params->mi_rows + num_mi_h - 1) / num_mi_h;

  // TODO(sdeng): fit a better model_1; disable it at this time.
  float *mb_delta_q0, *mb_delta_q1, delta_q_avg0 = 0.0f;
  CHECK_MEM_ERROR(cm, mb_delta_q0,
                  aom_calloc(num_rows * num_cols, sizeof(float)));
  CHECK_MEM_ERROR(cm, mb_delta_q1,
                  aom_calloc(num_rows * num_cols, sizeof(float)));

  if (model_predict(block_size, num_cols, num_rows, bit_depth, y_buffer,
                    y_stride, mb_delta_q0, mb_delta_q1)) {
    aom_internal_error(cm->error, AOM_CODEC_ERROR,
                       "Failed to call TFlite functions.");
  }

  // Loop through each SB block.
  for (int row = 0; row < num_rows; ++row) {
    for (int col = 0; col < num_cols; ++col) {
      const int index = row * num_cols + col;
      delta_q_avg0 += mb_delta_q0[index];
    }
  }

  delta_q_avg0 /= (float)(num_rows * num_cols);

  float scaling_factor;
  const float cq_level = (float)cpi->oxcf.rc_cfg.cq_level / (float)MAXQ;
  if (cq_level < delta_q_avg0) {
    scaling_factor = cq_level / delta_q_avg0;
  } else {
    scaling_factor = 1.0f - (cq_level - delta_q_avg0) / (1.0f - delta_q_avg0);
  }

  for (int row = 0; row < num_rows; ++row) {
    for (int col = 0; col < num_cols; ++col) {
      const int index = row * num_cols + col;
      cpi->mb_delta_q[index] =
          RINT((float)cpi->oxcf.q_cfg.deltaq_strength / 100.0f * (float)MAXQ *
               scaling_factor * (mb_delta_q0[index] - delta_q_avg0));
    }
  }

  aom_free(mb_delta_q0);
  aom_free(mb_delta_q1);
}
#else  // !CONFIG_TFLITE
void av1_set_mb_ur_variance(AV1_COMP *cpi) {
  const AV1_COMMON *cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;
  const MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  uint8_t *y_buffer = cpi->source->y_buffer;
  const int y_stride = cpi->source->y_stride;
  const int block_size = cpi->common.seq_params->sb_size;

  const int num_mi_w = mi_size_wide[block_size];
  const int num_mi_h = mi_size_high[block_size];
  const int num_cols = (mi_params->mi_cols + num_mi_w - 1) / num_mi_w;
  const int num_rows = (mi_params->mi_rows + num_mi_h - 1) / num_mi_h;

  int *mb_delta_q[2];
  CHECK_MEM_ERROR(cm, mb_delta_q[0],
                  aom_calloc(num_rows * num_cols, sizeof(*mb_delta_q[0])));
  CHECK_MEM_ERROR(cm, mb_delta_q[1],
                  aom_calloc(num_rows * num_cols, sizeof(*mb_delta_q[1])));

  // Approximates the model change between current version (Spet 2021) and the
  // baseline (July 2021).
  const double model_change[] = { 3.0, 3.0 };
  // The following parameters are fitted from user labeled data.
  const double a[] = { -24.50 * 4.0, -17.20 * 4.0 };
  const double b[] = { 0.004898, 0.003093 };
  const double c[] = { (29.932 + model_change[0]) * 4.0,
                       (42.100 + model_change[1]) * 4.0 };
  int delta_q_avg[2] = { 0, 0 };
  // Loop through each SB block.
  for (int row = 0; row < num_rows; ++row) {
    for (int col = 0; col < num_cols; ++col) {
      double var = 0.0, num_of_var = 0.0;
      const int index = row * num_cols + col;

      // Loop through each 8x8 block.
      for (int mi_row = row * num_mi_h;
           mi_row < mi_params->mi_rows && mi_row < (row + 1) * num_mi_h;
           mi_row += 2) {
        for (int mi_col = col * num_mi_w;
             mi_col < mi_params->mi_cols && mi_col < (col + 1) * num_mi_w;
             mi_col += 2) {
          struct buf_2d buf;
          const int row_offset_y = mi_row << 2;
          const int col_offset_y = mi_col << 2;

          buf.buf = y_buffer + row_offset_y * y_stride + col_offset_y;
          buf.stride = y_stride;

          unsigned int block_variance;
          block_variance = av1_get_perpixel_variance_facade(
              cpi, xd, &buf, BLOCK_8X8, AOM_PLANE_Y);

          block_variance = AOMMAX(block_variance, 1);
          var += log((double)block_variance);
          num_of_var += 1.0;
        }
      }
      var = exp(var / num_of_var);
      mb_delta_q[0][index] = RINT(a[0] * exp(-b[0] * var) + c[0]);
      mb_delta_q[1][index] = RINT(a[1] * exp(-b[1] * var) + c[1]);
      delta_q_avg[0] += mb_delta_q[0][index];
      delta_q_avg[1] += mb_delta_q[1][index];
    }
  }

  delta_q_avg[0] = RINT((double)delta_q_avg[0] / (num_rows * num_cols));
  delta_q_avg[1] = RINT((double)delta_q_avg[1] / (num_rows * num_cols));

  int model_idx;
  double scaling_factor;
  const int cq_level = cpi->oxcf.rc_cfg.cq_level;
  if (cq_level < delta_q_avg[0]) {
    model_idx = 0;
    scaling_factor = (double)cq_level / delta_q_avg[0];
  } else if (cq_level < delta_q_avg[1]) {
    model_idx = 2;
    scaling_factor =
        (double)(cq_level - delta_q_avg[0]) / (delta_q_avg[1] - delta_q_avg[0]);
  } else {
    model_idx = 1;
    scaling_factor = (double)(MAXQ - cq_level) / (MAXQ - delta_q_avg[1]);
  }

  const double new_delta_q_avg =
      delta_q_avg[0] + scaling_factor * (delta_q_avg[1] - delta_q_avg[0]);
  for (int row = 0; row < num_rows; ++row) {
    for (int col = 0; col < num_cols; ++col) {
      const int index = row * num_cols + col;
      if (model_idx == 2) {
        const double delta_q =
            mb_delta_q[0][index] +
            scaling_factor * (mb_delta_q[1][index] - mb_delta_q[0][index]);
        cpi->mb_delta_q[index] = RINT((double)cpi->oxcf.q_cfg.deltaq_strength /
                                      100.0 * (delta_q - new_delta_q_avg));
      } else {
        cpi->mb_delta_q[index] = RINT(
            (double)cpi->oxcf.q_cfg.deltaq_strength / 100.0 * scaling_factor *
            (mb_delta_q[model_idx][index] - delta_q_avg[model_idx]));
      }
    }
  }

  aom_free(mb_delta_q[0]);
  aom_free(mb_delta_q[1]);
}
#endif

int av1_get_sbq_user_rating_based(AV1_COMP *const cpi, int mi_row, int mi_col) {
  const BLOCK_SIZE bsize = cpi->common.seq_params->sb_size;
  const CommonModeInfoParams *const mi_params = &cpi->common.mi_params;
  AV1_COMMON *const cm = &cpi->common;
  const int base_qindex = cm->quant_params.base_qindex;
  if (base_qindex == MINQ || base_qindex == MAXQ) return base_qindex;

  const int num_mi_w = mi_size_wide[bsize];
  const int num_mi_h = mi_size_high[bsize];
  const int num_cols = (mi_params->mi_cols + num_mi_w - 1) / num_mi_w;
  const int index = (mi_row / num_mi_h) * num_cols + (mi_col / num_mi_w);
  const int delta_q = cpi->mb_delta_q[index];

  int qindex = base_qindex + delta_q;
  qindex = AOMMIN(qindex, MAXQ);
  qindex = AOMMAX(qindex, MINQ + 1);

  return qindex;
}
