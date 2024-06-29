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

#include "config/aom_config.h"

#include "aom_util/aom_pthread.h"

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
#include "av1/encoder/ethread.h"
#include "av1/encoder/hybrid_fwd_txfm.h"
#include "av1/encoder/model_rd.h"
#include "av1/encoder/rdopt_utils.h"

#define MB_WIENER_PRED_BLOCK_SIZE BLOCK_128X128
#define MB_WIENER_PRED_BUF_STRIDE 128

void av1_alloc_mb_wiener_var_pred_buf(AV1_COMMON *cm, ThreadData *td) {
  const int is_high_bitdepth = is_cur_buf_hbd(&td->mb.e_mbd);
  assert(MB_WIENER_PRED_BLOCK_SIZE < BLOCK_SIZES_ALL);
  const int buf_width = block_size_wide[MB_WIENER_PRED_BLOCK_SIZE];
  const int buf_height = block_size_high[MB_WIENER_PRED_BLOCK_SIZE];
  assert(buf_width == MB_WIENER_PRED_BUF_STRIDE);
  const size_t buf_size =
      (buf_width * buf_height * sizeof(*td->wiener_tmp_pred_buf))
      << is_high_bitdepth;
  CHECK_MEM_ERROR(cm, td->wiener_tmp_pred_buf, aom_memalign(32, buf_size));
}

void av1_dealloc_mb_wiener_var_pred_buf(ThreadData *td) {
  aom_free(td->wiener_tmp_pred_buf);
  td->wiener_tmp_pred_buf = NULL;
}

void av1_init_mb_wiener_var_buffer(AV1_COMP *cpi) {
  AV1_COMMON *cm = &cpi->common;

  // This block size is also used to determine number of workers in
  // multi-threading. If it is changed, one needs to change it accordingly in
  // "compute_num_ai_workers()".
  cpi->weber_bsize = BLOCK_8X8;

  if (cpi->oxcf.enable_rate_guide_deltaq) {
    if (cpi->mb_weber_stats && cpi->prep_rate_estimates &&
        cpi->ext_rate_distribution)
      return;
  } else {
    if (cpi->mb_weber_stats) return;
  }

  CHECK_MEM_ERROR(cm, cpi->mb_weber_stats,
                  aom_calloc(cpi->frame_info.mi_rows * cpi->frame_info.mi_cols,
                             sizeof(*cpi->mb_weber_stats)));

  if (cpi->oxcf.enable_rate_guide_deltaq) {
    CHECK_MEM_ERROR(
        cm, cpi->prep_rate_estimates,
        aom_calloc(cpi->frame_info.mi_rows * cpi->frame_info.mi_cols,
                   sizeof(*cpi->prep_rate_estimates)));

    CHECK_MEM_ERROR(
        cm, cpi->ext_rate_distribution,
        aom_calloc(cpi->frame_info.mi_rows * cpi->frame_info.mi_cols,
                   sizeof(*cpi->ext_rate_distribution)));
  }
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

static int rate_estimator(const tran_low_t *qcoeff, int eob, TX_SIZE tx_size) {
  const SCAN_ORDER *const scan_order = &av1_scan_orders[tx_size][DCT_DCT];

  assert((1 << num_pels_log2_lookup[txsize_to_bsize[tx_size]]) >= eob);
  int rate_cost = 1;

  for (int idx = 0; idx < eob; ++idx) {
    int abs_level = abs(qcoeff[scan_order->scan[idx]]);
    rate_cost += (int)(log1p(abs_level) / log(2.0)) + 1 + (abs_level > 0);
  }

  return (rate_cost << AV1_PROB_COST_SHIFT);
}

void av1_calc_mb_wiener_var_row(AV1_COMP *const cpi, MACROBLOCK *x,
                                MACROBLOCKD *xd, const int mi_row,
                                int16_t *src_diff, tran_low_t *coeff,
                                tran_low_t *qcoeff, tran_low_t *dqcoeff,
                                double *sum_rec_distortion,
                                double *sum_est_rate, uint8_t *pred_buffer) {
  AV1_COMMON *const cm = &cpi->common;
  uint8_t *buffer = cpi->source->y_buffer;
  int buf_stride = cpi->source->y_stride;
  MB_MODE_INFO mbmi;
  memset(&mbmi, 0, sizeof(mbmi));
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  xd->mi = &mbmi_ptr;
  const BLOCK_SIZE bsize = cpi->weber_bsize;
  const TX_SIZE tx_size = max_txsize_lookup[bsize];
  const int block_size = tx_size_wide[tx_size];
  const int coeff_count = block_size * block_size;
  const int mb_step = mi_size_wide[bsize];
  const BitDepthInfo bd_info = get_bit_depth_info(xd);
  const MultiThreadInfo *const mt_info = &cpi->mt_info;
  const AV1EncAllIntraMultiThreadInfo *const intra_mt = &mt_info->intra_mt;
  AV1EncRowMultiThreadSync *const intra_row_mt_sync =
      &cpi->ppi->intra_row_mt_sync;
  const int mi_cols = cm->mi_params.mi_cols;
  const int mt_thread_id = mi_row / mb_step;
  // TODO(chengchen): test different unit step size
  const int mt_unit_step = mi_size_wide[MB_WIENER_MT_UNIT_SIZE];
  const int mt_unit_cols = (mi_cols + (mt_unit_step >> 1)) / mt_unit_step;
  int mt_unit_col = 0;
  const int is_high_bitdepth = is_cur_buf_hbd(xd);

  uint8_t *dst_buffer = pred_buffer;
  const int dst_buffer_stride = MB_WIENER_PRED_BUF_STRIDE;

  if (is_high_bitdepth) {
    uint16_t *pred_buffer_16 = (uint16_t *)pred_buffer;
    dst_buffer = CONVERT_TO_BYTEPTR(pred_buffer_16);
  }

  for (int mi_col = 0; mi_col < mi_cols; mi_col += mb_step) {
    if (mi_col % mt_unit_step == 0) {
      intra_mt->intra_sync_read_ptr(intra_row_mt_sync, mt_thread_id,
                                    mt_unit_col);
#if CONFIG_MULTITHREAD
      const int num_workers =
          AOMMIN(mt_info->num_mod_workers[MOD_AI], mt_info->num_workers);
      if (num_workers > 1) {
        const AV1EncRowMultiThreadInfo *const enc_row_mt = &mt_info->enc_row_mt;
        pthread_mutex_lock(enc_row_mt->mutex_);
        const bool exit = enc_row_mt->mb_wiener_mt_exit;
        pthread_mutex_unlock(enc_row_mt->mutex_);
        // Stop further processing in case any worker has encountered an error.
        if (exit) break;
      }
#endif
    }

    PREDICTION_MODE best_mode = DC_PRED;
    int best_intra_cost = INT_MAX;
    const int mi_width = mi_size_wide[bsize];
    const int mi_height = mi_size_high[bsize];
    set_mode_info_offsets(&cpi->common.mi_params, &cpi->mbmi_ext_info, x, xd,
                          mi_row, mi_col);
    set_mi_row_col(xd, &xd->tile, mi_row, mi_height, mi_col, mi_width,
                   AOMMIN(mi_row + mi_height, cm->mi_params.mi_rows),
                   AOMMIN(mi_col + mi_width, cm->mi_params.mi_cols));
    set_plane_n4(xd, mi_size_wide[bsize], mi_size_high[bsize],
                 av1_num_planes(cm));
    xd->mi[0]->bsize = bsize;
    xd->mi[0]->motion_mode = SIMPLE_TRANSLATION;
    // Set above and left mbmi to NULL as they are not available in the
    // preprocessing stage.
    // They are used to detemine intra edge filter types in intra prediction.
    if (xd->up_available) {
      xd->above_mbmi = NULL;
    }
    if (xd->left_available) {
      xd->left_mbmi = NULL;
    }
    uint8_t *mb_buffer =
        buffer + mi_row * MI_SIZE * buf_stride + mi_col * MI_SIZE;
    for (PREDICTION_MODE mode = INTRA_MODE_START; mode < INTRA_MODE_END;
         ++mode) {
      // TODO(chengchen): Here we use src instead of reconstructed frame as
      // the intra predictor to make single and multithread version match.
      // Ideally we want to use the reconstructed.
      av1_predict_intra_block(
          xd, cm->seq_params->sb_size, cm->seq_params->enable_intra_edge_filter,
          block_size, block_size, tx_size, mode, 0, 0, FILTER_INTRA_MODES,
          mb_buffer, buf_stride, dst_buffer, dst_buffer_stride, 0, 0, 0);
      av1_subtract_block(bd_info, block_size, block_size, src_diff, block_size,
                         mb_buffer, buf_stride, dst_buffer, dst_buffer_stride);
      av1_quick_txfm(0, tx_size, bd_info, src_diff, block_size, coeff);
      int intra_cost = aom_satd(coeff, coeff_count);
      if (intra_cost < best_intra_cost) {
        best_intra_cost = intra_cost;
        best_mode = mode;
      }
    }

    av1_predict_intra_block(
        xd, cm->seq_params->sb_size, cm->seq_params->enable_intra_edge_filter,
        block_size, block_size, tx_size, best_mode, 0, 0, FILTER_INTRA_MODES,
        mb_buffer, buf_stride, dst_buffer, dst_buffer_stride, 0, 0, 0);
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
    av1_quantize_fp_facade(coeff, pix_num, p, qcoeff, dqcoeff, &eob, scan_order,
                           &quant_param);
#endif  // CONFIG_AV1_HIGHBITDEPTH

    if (cpi->oxcf.enable_rate_guide_deltaq) {
      const int rate_cost = rate_estimator(qcoeff, eob, tx_size);
      cpi->prep_rate_estimates[(mi_row / mb_step) * cpi->frame_info.mi_cols +
                               (mi_col / mb_step)] = rate_cost;
    }

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

    if (cpi->oxcf.intra_mode_cfg.auto_intra_tools_off) {
      *sum_rec_distortion += weber_stats->distortion;
      int est_block_rate = 0;
      int64_t est_block_dist = 0;
      model_rd_sse_fn[MODELRD_LEGACY](cpi, x, bsize, 0, weber_stats->distortion,
                                      pix_num, &est_block_rate,
                                      &est_block_dist);
      *sum_est_rate += est_block_rate;
    }

    weber_stats->src_variance -= (src_mean * src_mean) / pix_num;
    weber_stats->rec_variance -= (rec_mean * rec_mean) / pix_num;
    weber_stats->distortion -= (dist_mean * dist_mean) / pix_num;
    weber_stats->satd = best_intra_cost;

    qcoeff[0] = 0;
    int max_scale = 0;
    for (int idx = 1; idx < coeff_count; ++idx) {
      const int abs_qcoeff = abs(qcoeff[idx]);
      max_scale = AOMMAX(max_scale, abs_qcoeff);
    }
    weber_stats->max_scale = max_scale;

    if ((mi_col + mb_step) % mt_unit_step == 0 ||
        (mi_col + mb_step) >= mi_cols) {
      intra_mt->intra_sync_write_ptr(intra_row_mt_sync, mt_thread_id,
                                     mt_unit_col, mt_unit_cols);
      ++mt_unit_col;
    }
  }
  // Set the pointer to null since mbmi is only allocated inside this function.
  xd->mi = NULL;
}

static void calc_mb_wiener_var(AV1_COMP *const cpi, double *sum_rec_distortion,
                               double *sum_est_rate) {
  MACROBLOCK *x = &cpi->td.mb;
  MACROBLOCKD *xd = &x->e_mbd;
  const BLOCK_SIZE bsize = cpi->weber_bsize;
  const int mb_step = mi_size_wide[bsize];
  DECLARE_ALIGNED(32, int16_t, src_diff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, coeff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, qcoeff[32 * 32]);
  DECLARE_ALIGNED(32, tran_low_t, dqcoeff[32 * 32]);
  for (int mi_row = 0; mi_row < cpi->frame_info.mi_rows; mi_row += mb_step) {
    av1_calc_mb_wiener_var_row(cpi, x, xd, mi_row, src_diff, coeff, qcoeff,
                               dqcoeff, sum_rec_distortion, sum_est_rate,
                               cpi->td.wiener_tmp_pred_buf);
  }
}

static int64_t estimate_wiener_var_norm(AV1_COMP *const cpi,
                                        const BLOCK_SIZE norm_block_size) {
  const AV1_COMMON *const cm = &cpi->common;
  int64_t norm_factor = 1;
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

static void ext_rate_guided_quantization(AV1_COMP *cpi) {
  // Calculation uses 8x8.
  const int mb_step = mi_size_wide[cpi->weber_bsize];
  // Accumulate to 16x16, step size is in the unit of mi.
  const int block_step = 4;

  const char *filename = cpi->oxcf.rate_distribution_info;
  FILE *pfile = fopen(filename, "r");
  if (pfile == NULL) {
    assert(pfile != NULL);
    return;
  }

  double ext_rate_sum = 0.0;
  for (int row = 0; row < cpi->frame_info.mi_rows; row += block_step) {
    for (int col = 0; col < cpi->frame_info.mi_cols; col += block_step) {
      float val;
      const int fields_converted = fscanf(pfile, "%f", &val);
      if (fields_converted != 1) {
        assert(fields_converted == 1);
        fclose(pfile);
        return;
      }
      ext_rate_sum += val;
      cpi->ext_rate_distribution[(row / mb_step) * cpi->frame_info.mi_cols +
                                 (col / mb_step)] = val;
    }
  }
  fclose(pfile);

  int uniform_rate_sum = 0;
  for (int row = 0; row < cpi->frame_info.mi_rows; row += block_step) {
    for (int col = 0; col < cpi->frame_info.mi_cols; col += block_step) {
      int rate_sum = 0;
      for (int r = 0; r < block_step; r += mb_step) {
        for (int c = 0; c < block_step; c += mb_step) {
          const int mi_row = row + r;
          const int mi_col = col + c;
          rate_sum += cpi->prep_rate_estimates[(mi_row / mb_step) *
                                                   cpi->frame_info.mi_cols +
                                               (mi_col / mb_step)];
        }
      }
      uniform_rate_sum += rate_sum;
    }
  }

  const double scale = uniform_rate_sum / ext_rate_sum;
  cpi->ext_rate_scale = scale;
}

void av1_set_mb_wiener_variance(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const SequenceHeader *const seq_params = cm->seq_params;
  if (aom_realloc_frame_buffer(
          &cm->cur_frame->buf, cm->width, cm->height, seq_params->subsampling_x,
          seq_params->subsampling_y, seq_params->use_highbitdepth,
          cpi->oxcf.border_in_pixels, cm->features.byte_alignment, NULL, NULL,
          NULL, cpi->alloc_pyramid, 0))
    aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffer");
  av1_alloc_mb_wiener_var_pred_buf(&cpi->common, &cpi->td);
  cpi->norm_wiener_variance = 0;

  MACROBLOCK *x = &cpi->td.mb;
  MACROBLOCKD *xd = &x->e_mbd;
  // xd->mi needs to be setup since it is used in av1_frame_init_quantizer.
  MB_MODE_INFO mbmi;
  memset(&mbmi, 0, sizeof(mbmi));
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  xd->mi = &mbmi_ptr;
  cm->quant_params.base_qindex = cpi->oxcf.rc_cfg.cq_level;
  av1_frame_init_quantizer(cpi);

  double sum_rec_distortion = 0.0;
  double sum_est_rate = 0.0;

  MultiThreadInfo *const mt_info = &cpi->mt_info;
  const int num_workers =
      AOMMIN(mt_info->num_mod_workers[MOD_AI], mt_info->num_workers);
  AV1EncAllIntraMultiThreadInfo *const intra_mt = &mt_info->intra_mt;
  intra_mt->intra_sync_read_ptr = av1_row_mt_sync_read_dummy;
  intra_mt->intra_sync_write_ptr = av1_row_mt_sync_write_dummy;
  // Calculate differential contrast for each block for the entire image.
  // TODO(chengchen): properly accumulate the distortion and rate in
  // av1_calc_mb_wiener_var_mt(). Until then, call calc_mb_wiener_var() if
  // auto_intra_tools_off is true.
  if (num_workers > 1 && !cpi->oxcf.intra_mode_cfg.auto_intra_tools_off) {
    intra_mt->intra_sync_read_ptr = av1_row_mt_sync_read;
    intra_mt->intra_sync_write_ptr = av1_row_mt_sync_write;
    av1_calc_mb_wiener_var_mt(cpi, num_workers, &sum_rec_distortion,
                              &sum_est_rate);
  } else {
    calc_mb_wiener_var(cpi, &sum_rec_distortion, &sum_est_rate);
  }

  // Determine whether to turn off several intra coding tools.
  automatic_intra_tools_off(cpi, sum_rec_distortion, sum_est_rate);

  // Read external rate distribution and use it to guide delta quantization
  if (cpi->oxcf.enable_rate_guide_deltaq) ext_rate_guided_quantization(cpi);

  const BLOCK_SIZE norm_block_size = cm->seq_params->sb_size;
  cpi->norm_wiener_variance = estimate_wiener_var_norm(cpi, norm_block_size);
  const int norm_step = mi_size_wide[norm_block_size];

  double sb_wiener_log = 0;
  double sb_count = 0;
  for (int its_cnt = 0; its_cnt < 2; ++its_cnt) {
    sb_wiener_log = 0;
    sb_count = 0;
    for (int mi_row = 0; mi_row < cm->mi_params.mi_rows; mi_row += norm_step) {
      for (int mi_col = 0; mi_col < cm->mi_params.mi_cols;
           mi_col += norm_step) {
        int sb_wiener_var =
            get_var_perceptual_ai(cpi, norm_block_size, mi_row, mi_col);

        double beta = (double)cpi->norm_wiener_variance / sb_wiener_var;
        double min_max_scale = AOMMAX(
            1.0, get_max_scale(cpi, cm->seq_params->sb_size, mi_row, mi_col));

        beta = AOMMIN(beta, 4);
        beta = AOMMAX(beta, 0.25);

        if (beta < 1 / min_max_scale) continue;

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

  // Set the pointer to null since mbmi is only allocated inside this function.
  xd->mi = NULL;
  aom_free_frame_buffer(&cm->cur_frame->buf);
  av1_dealloc_mb_wiener_var_pred_buf(&cpi->td);
}

static int get_rate_guided_quantizer(AV1_COMP *const cpi, BLOCK_SIZE bsize,
                                     int mi_row, int mi_col) {
  // Calculation uses 8x8.
  const int mb_step = mi_size_wide[cpi->weber_bsize];
  // Accumulate to 16x16
  const int block_step = mi_size_wide[BLOCK_16X16];
  double sb_rate_hific = 0.0;
  double sb_rate_uniform = 0.0;
  for (int row = mi_row; row < mi_row + mi_size_wide[bsize];
       row += block_step) {
    for (int col = mi_col; col < mi_col + mi_size_high[bsize];
         col += block_step) {
      sb_rate_hific +=
          cpi->ext_rate_distribution[(row / mb_step) * cpi->frame_info.mi_cols +
                                     (col / mb_step)];

      for (int r = 0; r < block_step; r += mb_step) {
        for (int c = 0; c < block_step; c += mb_step) {
          const int this_row = row + r;
          const int this_col = col + c;
          sb_rate_uniform +=
              cpi->prep_rate_estimates[(this_row / mb_step) *
                                           cpi->frame_info.mi_cols +
                                       (this_col / mb_step)];
        }
      }
    }
  }
  sb_rate_hific *= cpi->ext_rate_scale;

  const double weight = 1.0;
  const double rate_diff =
      weight * (sb_rate_hific - sb_rate_uniform) / sb_rate_uniform;
  double scale = pow(2, rate_diff);

  scale = scale * scale;
  double min_max_scale = AOMMAX(1.0, get_max_scale(cpi, bsize, mi_row, mi_col));
  scale = 1.0 / AOMMIN(1.0 / scale, min_max_scale);

  AV1_COMMON *const cm = &cpi->common;
  const int base_qindex = cm->quant_params.base_qindex;
  int offset =
      av1_get_deltaq_offset(cm->seq_params->bit_depth, base_qindex, scale);
  const DeltaQInfo *const delta_q_info = &cm->delta_q_info;
  const int max_offset = delta_q_info->delta_q_res * 10;
  offset = AOMMIN(offset, max_offset - 1);
  offset = AOMMAX(offset, -max_offset + 1);
  int qindex = cm->quant_params.base_qindex + offset;
  qindex = AOMMIN(qindex, MAXQ);
  qindex = AOMMAX(qindex, MINQ);
  if (base_qindex > MINQ) qindex = AOMMAX(qindex, MINQ + 1);

  return qindex;
}

int av1_get_sbq_perceptual_ai(AV1_COMP *const cpi, BLOCK_SIZE bsize, int mi_row,
                              int mi_col) {
  if (cpi->oxcf.enable_rate_guide_deltaq) {
    return get_rate_guided_quantizer(cpi, bsize, mi_row, mi_col);
  }

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
