/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <float.h>

#include "av1/encoder/encodeframe_utils.h"
#include "av1/encoder/thirdpass.h"
#include "config/aom_dsp_rtcd.h"

#include "av1/common/enums.h"
#include "av1/common/reconinter.h"

#if !CONFIG_REALTIME_ONLY
#include "av1/encoder/cnn.h"
#include "av1/encoder/partition_model_weights.h"
#include "av1/encoder/partition_cnn_weights.h"
#endif
#include "av1/encoder/encoder.h"

#include "av1/encoder/motion_search_facade.h"
#include "av1/encoder/partition_strategy.h"
#include "av1/encoder/partition_search.h"
#include "av1/encoder/rdopt.h"

#if !CONFIG_REALTIME_ONLY
static AOM_INLINE void simple_motion_search_prune_part_features(
    AV1_COMP *const cpi, MACROBLOCK *x, SIMPLE_MOTION_DATA_TREE *sms_tree,
    int mi_row, int mi_col, BLOCK_SIZE bsize, float *features,
    int features_to_get);

static bool ext_ml_model_decision_before_none(
    AV1_COMP *cpi, const float features_from_motion[FEATURE_SIZE_SMS_SPLIT],
    int *partition_none_allowed, int *partition_horz_allowed,
    int *partition_vert_allowed, int *do_rectangular_split,
    int *do_square_split);

static bool ext_ml_model_decision_before_none_part2(
    AV1_COMP *cpi,
    const float features_from_motion[FEATURE_SIZE_SMS_PRUNE_PART],
    int *prune_horz, int *prune_vert);

static bool ext_ml_model_decision_after_none(
    ExtPartController *const ext_part_controller, const int is_intra_frame,
    const float *const features_after_none, int *do_square_split,
    int *do_rectangular_split);

static bool ext_ml_model_decision_after_none_part2(
    AV1_COMP *const cpi, const float *const features_terminate,
    int *terminate_partition_search);

static bool ext_ml_model_decision_after_split(
    AV1_COMP *const cpi, const float *const features_terminate,
    int *terminate_partition_search);

static bool ext_ml_model_decision_after_split_part2(
    ExtPartController *const ext_part_controller, const int is_intra_frame,
    const float *const features_prune, int *prune_rect_part_horz,
    int *prune_rect_part_vert);

static bool ext_ml_model_decision_after_rect(
    ExtPartController *const ext_part_controller, const int is_intra_frame,
    const float *const features_after_rect, int *horza_partition_allowed,
    int *horzb_partition_allowed, int *verta_partition_allowed,
    int *vertb_partition_allowed);

static bool ext_ml_model_decision_after_part_ab(
    AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize, int part_ctx,
    int64_t best_rd, int64_t rect_part_rd[NUM_RECT_PARTS][SUB_PARTITIONS_RECT],
    int64_t split_rd[SUB_PARTITIONS_SPLIT], int *const partition_horz4_allowed,
    int *const partition_vert4_allowed, unsigned int pb_source_variance,
    int mi_row, int mi_col);

static INLINE int convert_bsize_to_idx(BLOCK_SIZE bsize) {
  switch (bsize) {
    case BLOCK_128X128: return 0;
    case BLOCK_64X64: return 1;
    case BLOCK_32X32: return 2;
    case BLOCK_16X16: return 3;
    case BLOCK_8X8: return 4;
    default: assert(0 && "Invalid bsize"); return -1;
  }
}

static char *get_feature_file_name(int id) {
  static char *feature_file_names[] = {
    "feature_before_partition_none",
    "feature_before_partition_none_prune_rect",
    "feature_after_partition_none_prune",
    "feature_after_partition_none_terminate",
    "feature_after_partition_split_terminate",
    "feature_after_partition_split_prune_rect",
    "feature_after_partition_rect",
    "feature_after_partition_ab",
  };

  return feature_file_names[id];
}

static void write_features_to_file(const char *const path,
                                   const bool is_test_mode,
                                   const float *features,
                                   const int feature_size, const int id,
                                   const BLOCK_SIZE bsize, const int mi_row,
                                   const int mi_col) {
  if (!WRITE_FEATURE_TO_FILE && !is_test_mode) return;

  char filename[256];
  snprintf(filename, sizeof(filename), "%s/%s", path,
           get_feature_file_name(id));
  FILE *pfile = fopen(filename, "a");
  if (pfile == NULL) return;
  if (!is_test_mode) {
    fprintf(pfile, "%d,%d,%d,%d,%d\n", id, (int)bsize, mi_row, mi_col,
            feature_size);
  }
  for (int i = 0; i < feature_size; ++i) {
    fprintf(pfile, "%.6f", features[i]);
    if (i < feature_size - 1) fprintf(pfile, ",");
  }
  fprintf(pfile, "\n");
  fclose(pfile);
}

// TODO(chiyotsai@google.com): This is very much a work in progress. We still
// need to the following:
//   -- add support for hdres
//   -- add support for pruning rectangular partitions
//   -- use reconstructed pixels instead of source pixels for padding
//   -- use chroma pixels in addition to luma pixels
void av1_intra_mode_cnn_partition(const AV1_COMMON *const cm, MACROBLOCK *x,
                                  int quad_tree_idx,
                                  int intra_cnn_based_part_prune_level,
                                  PartitionSearchState *part_state) {
  assert(cm->seq_params->sb_size >= BLOCK_64X64 &&
         "Invalid sb_size for intra_cnn!");
  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const BLOCK_SIZE bsize = blk_params->bsize;

  const int bsize_idx = convert_bsize_to_idx(bsize);

  if (bsize == BLOCK_128X128) {
    return;
  }

  PartitionSearchInfo *part_info = &x->part_search_info;

  // Precompute the CNN part and cache the result in MACROBLOCK
  if (bsize == BLOCK_64X64 && !part_info->cnn_output_valid) {
    const CNN_CONFIG *cnn_config = &av1_intra_mode_cnn_partition_cnn_config;

    // Prepare the output
    const CNN_THREAD_DATA thread_data = { .num_workers = 1, .workers = NULL };
    const int num_outputs = 4;
    const int output_dims[4] = { 1, 2, 4, 8 };
    const int out_chs[4] = { CNN_BRANCH_0_OUT_CH, CNN_BRANCH_1_OUT_CH,
                             CNN_BRANCH_2_OUT_CH, CNN_BRANCH_3_OUT_CH };
    float *output_buffer[CNN_TOT_OUT_CH];

    float **cur_output_buf = output_buffer;
    float *curr_buf_ptr = part_info->cnn_buffer;
    for (int output_idx = 0; output_idx < num_outputs; output_idx++) {
      const int num_chs = out_chs[output_idx];
      const int ch_size = output_dims[output_idx] * output_dims[output_idx];
      for (int ch = 0; ch < num_chs; ch++) {
        cur_output_buf[ch] = curr_buf_ptr;
        curr_buf_ptr += ch_size;
      }
      cur_output_buf += num_chs;
    }

    CNN_MULTI_OUT output = {
      .num_outputs = 4,
      .output_channels = out_chs,
      .output_strides = output_dims,
      .output_buffer = output_buffer,
    };

    // Prepare the input
    const MACROBLOCKD *xd = &x->e_mbd;
    const int bit_depth = xd->bd;
    const int dc_q =
        av1_dc_quant_QTX(x->qindex, 0, bit_depth) >> (bit_depth - 8);
    part_info->log_q = log1pf((float)(dc_q * dc_q) / 256.0f);
    part_info->log_q =
        (part_info->log_q - av1_intra_mode_cnn_partition_mean[0]) /
        av1_intra_mode_cnn_partition_std[0];

    const int width = 65, height = 65,
              stride = x->plane[AOM_PLANE_Y].src.stride;

    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *image[1] = {
        CONVERT_TO_SHORTPTR(x->plane[AOM_PLANE_Y].src.buf) - stride - 1
      };

      if (!av1_cnn_predict_img_multi_out_highbd(image, width, height, stride,
                                                cnn_config, &thread_data,
                                                bit_depth, &output)) {
        aom_internal_error(xd->error_info, AOM_CODEC_MEM_ERROR,
                           "Error allocating CNN data");
        return;
      }
    } else {
      uint8_t *image[1] = { x->plane[AOM_PLANE_Y].src.buf - stride - 1 };

      if (!av1_cnn_predict_img_multi_out(image, width, height, stride,
                                         cnn_config, &thread_data, &output)) {
        aom_internal_error(xd->error_info, AOM_CODEC_MEM_ERROR,
                           "Error allocating CNN data");
        return;
      }
    }

    part_info->cnn_output_valid = 1;
  }

  if (!part_info->cnn_output_valid) {
    return;
  }

  const NN_CONFIG *dnn_configs[5] = {
    NULL,
    &av1_intra_mode_cnn_partition_branch_0_dnn_config,
    &av1_intra_mode_cnn_partition_branch_1_dnn_config,
    &av1_intra_mode_cnn_partition_branch_2_dnn_config,
    &av1_intra_mode_cnn_partition_branch_3_dnn_config,
  };

  const NN_CONFIG *dnn_config = dnn_configs[bsize_idx];

  float dnn_features[100];
  float logits[4] = { 0.0f };

  const float *branch_0 = part_info->cnn_buffer;
  const float *branch_1 = branch_0 + CNN_BRANCH_0_OUT_SIZE;
  const float *branch_2 = branch_1 + CNN_BRANCH_1_OUT_SIZE;
  const float *branch_3 = branch_2 + CNN_BRANCH_2_OUT_SIZE;

  if (bsize == BLOCK_64X64) {
    int f_idx = 0;
    for (int ch_idx = 0; ch_idx < CNN_BRANCH_0_OUT_CH; ch_idx++) {
      dnn_features[f_idx++] = branch_0[ch_idx];
    }

    const int spa_stride = 2 * 2;
    for (int lin_idx = 0; lin_idx < spa_stride; lin_idx++) {
      for (int ch_idx = 0; ch_idx < CNN_BRANCH_1_OUT_CH; ch_idx++) {
        dnn_features[f_idx++] = branch_1[lin_idx + ch_idx * spa_stride];
      }
    }
    dnn_features[f_idx++] = part_info->log_q;
  } else if (bsize == BLOCK_32X32) {
    int f_idx = 0;
    for (int idx = 0; idx < CNN_BRANCH_0_OUT_CH; idx++) {
      dnn_features[f_idx++] = branch_0[idx];
    }

    const int curr_lin_idx = quad_to_linear_1[quad_tree_idx - 1];
    const int spa_stride = 2 * 2;
    for (int ch_idx = 0; ch_idx < CNN_BRANCH_1_OUT_CH; ch_idx++) {
      dnn_features[f_idx++] = branch_1[curr_lin_idx + ch_idx * spa_stride];
    }
    dnn_features[f_idx++] = part_info->log_q;
  } else if (bsize == BLOCK_16X16) {
    int f_idx = 0;
    const int prev_quad_idx = (quad_tree_idx - 1) / 4;
    const int prev_lin_idx = quad_to_linear_1[prev_quad_idx - 1];
    const int prev_spa_stride = 2 * 2;
    for (int ch_idx = 0; ch_idx < CNN_BRANCH_1_OUT_CH; ch_idx++) {
      dnn_features[f_idx++] = branch_1[prev_lin_idx + ch_idx * prev_spa_stride];
    }

    const int curr_lin_idx = quad_to_linear_2[quad_tree_idx - 5];
    const int spa_stride = 4 * 4;
    for (int ch_idx = 0; ch_idx < CNN_BRANCH_2_OUT_CH; ch_idx++) {
      dnn_features[f_idx++] = branch_2[curr_lin_idx + ch_idx * spa_stride];
    }
    dnn_features[f_idx++] = part_info->log_q;
  } else if (bsize == BLOCK_8X8) {
    int f_idx = 0;
    const int prev_quad_idx = (quad_tree_idx - 1) / 4;
    const int prev_lin_idx = quad_to_linear_2[prev_quad_idx - 5];
    const int prev_spa_stride = 4 * 4;
    for (int ch_idx = 0; ch_idx < CNN_BRANCH_2_OUT_CH; ch_idx++) {
      dnn_features[f_idx++] = branch_2[prev_lin_idx + ch_idx * prev_spa_stride];
    }

    const int curr_lin_idx = quad_to_linear_3[quad_tree_idx - 21];
    const int spa_stride = 8 * 8;
    for (int ch_idx = 0; ch_idx < CNN_BRANCH_3_OUT_CH; ch_idx++) {
      dnn_features[f_idx++] = branch_3[curr_lin_idx + ch_idx * spa_stride];
    }
    dnn_features[f_idx++] = part_info->log_q;
  } else {
    assert(0 && "Invalid bsize in intra_cnn partition");
  }

  // Make decision
  av1_nn_predict(dnn_features, dnn_config, 1, logits);

  const int is_720p_or_larger = AOMMIN(cm->width, cm->height) >= 720;
  const int is_480p_or_larger = AOMMIN(cm->width, cm->height) >= 480;
  float split_only_thresh = 100.0f, no_split_thresh = -100.0f;
  if (is_720p_or_larger) {
    split_only_thresh =
        av1_intra_mode_cnn_partition_split_thresh_hdres[bsize_idx];
    no_split_thresh =
        av1_intra_mode_cnn_partition_no_split_thresh_hdres[bsize_idx];
  } else if (is_480p_or_larger) {
    split_only_thresh =
        av1_intra_mode_cnn_partition_split_thresh_midres[bsize_idx];
    no_split_thresh =
        av1_intra_mode_cnn_partition_no_split_thresh_midres[bsize_idx];
  } else {
    split_only_thresh =
        av1_intra_mode_cnn_partition_split_thresh_lowres[bsize_idx];
    no_split_thresh =
        av1_intra_mode_cnn_partition_no_split_thresh_lowres[bsize_idx];
  }

  if (logits[0] > split_only_thresh) {
    // As screen contents tend to choose larger partitions, do not prune
    // PARTITION_NONE when intra_cnn_based_part_prune_level=1.
    if (intra_cnn_based_part_prune_level != 1) {
      part_state->partition_none_allowed = 0;
    }
    part_state->do_square_split = 1;
    av1_disable_rect_partitions(part_state);
  }

  if (logits[0] < no_split_thresh) {
    av1_disable_square_split_partition(part_state);
  }
}

static INLINE int get_simple_motion_search_prune_agg(int qindex,
                                                     int prune_level,
                                                     int is_rect_part) {
  assert(prune_level < TOTAL_AGG_LVLS);
  if (prune_level == NO_PRUNING) {
    return -1;
  }

  // Aggressiveness value for SIMPLE_MOTION_SEARCH_PRUNE_LEVEL except
  // QIDX_BASED_AGG_LVL
  const int sms_prune_agg_levels[TOTAL_SIMPLE_AGG_LVLS] = { 0, 1, 2, 3 };
  if (prune_level < TOTAL_SIMPLE_AGG_LVLS) {
    return sms_prune_agg_levels[prune_level];
  }

  // Map the QIDX_BASED_AGG_LVL to corresponding aggressiveness value.
  // Aggressive pruning for lower quantizers in non-boosted frames to prune
  // rectangular partitions.
  const int qband = is_rect_part ? (qindex <= 90 ? 1 : 0) : 0;
  const int sms_prune_agg_qindex_based[2] = { 1, 2 };
  return sms_prune_agg_qindex_based[qband];
}

void av1_simple_motion_search_based_split(AV1_COMP *const cpi, MACROBLOCK *x,
                                          SIMPLE_MOTION_DATA_TREE *sms_tree,
                                          PartitionSearchState *part_state) {
  const AV1_COMMON *const cm = &cpi->common;
  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const int mi_row = blk_params->mi_row, mi_col = blk_params->mi_col;
  const BLOCK_SIZE bsize = blk_params->bsize;

  const int bsize_idx = convert_bsize_to_idx(bsize);
  const int is_720p_or_larger = AOMMIN(cm->width, cm->height) >= 720;
  const int is_480p_or_larger = AOMMIN(cm->width, cm->height) >= 480;
  // res_idx is 0 for res < 480p, 1 for 480p, 2 for 720p+
  const int res_idx = is_480p_or_larger + is_720p_or_larger;

  assert(bsize_idx >= 0 && bsize_idx <= 4 &&
         "Invalid bsize in simple_motion_search_based_split");

  const float *ml_mean = av1_simple_motion_search_split_mean[bsize_idx];
  const float *ml_std = av1_simple_motion_search_split_std[bsize_idx];
  const NN_CONFIG *nn_config =
      av1_simple_motion_search_split_nn_config[bsize_idx];

  const int agg = get_simple_motion_search_prune_agg(
      x->qindex, cpi->sf.part_sf.simple_motion_search_prune_agg, 0);
  if (agg < 0) {
    return;
  }

  const float split_only_thresh =
      av1_simple_motion_search_split_thresh[agg][res_idx][bsize_idx];
  const float no_split_thresh =
      av1_simple_motion_search_no_split_thresh[agg][res_idx][bsize_idx];

  float features[FEATURE_SIZE_SMS_SPLIT] = { 0.0f };
  simple_motion_search_prune_part_features(cpi, x, sms_tree, mi_row, mi_col,
                                           bsize, features,
                                           FEATURE_SMS_SPLIT_MODEL_FLAG);

  // Write features to file
  write_features_to_file(cpi->oxcf.partition_info_path,
                         cpi->ext_part_controller.test_mode, features,
                         FEATURE_SIZE_SMS_SPLIT, 0, bsize, mi_row, mi_col);

  // Note: it is intended to not normalize the features here, to keep it
  // consistent for all features collected and passed to the external model.
  if (ext_ml_model_decision_before_none(
          cpi, features, &part_state->partition_none_allowed,
          &part_state->partition_rect_allowed[HORZ],
          &part_state->partition_rect_allowed[VERT],
          &part_state->do_rectangular_split, &part_state->do_square_split)) {
    return;
  }

  for (int idx = 0; idx < FEATURE_SIZE_SMS_SPLIT; idx++) {
    features[idx] = (features[idx] - ml_mean[idx]) / ml_std[idx];
  }

  float score = 0.0f;

  av1_nn_predict(features, nn_config, 1, &score);

  if (score > split_only_thresh) {
    av1_set_square_split_only(part_state);
  }

  if (cpi->sf.part_sf.simple_motion_search_split >= 2 &&
      score < no_split_thresh) {
    av1_disable_square_split_partition(part_state);
  }

  // If the score is very low, prune rectangular split since it is unlikely to
  // occur.
  if (cpi->sf.part_sf.simple_motion_search_rect_split) {
    const float scale = res_idx >= 2 ? 3.0f : 2.0f;
    const float rect_split_thresh =
        scale * av1_simple_motion_search_no_split_thresh
                    [cpi->sf.part_sf.simple_motion_search_rect_split][res_idx]
                    [bsize_idx];
    if (score < rect_split_thresh) {
      part_state->do_rectangular_split = 0;
    }
  }
}

// Given a list of ref frames in refs, performs simple_motion_search on each of
// the refs and returns the ref with the smallest sse. Returns -1 if none of the
// ref in the list is available. Also stores the best sse and var in best_sse,
// best_var, respectively. If save_mv is 0, don't update mv_ref_fulls in
// sms_tree. If save_mv is 1, update mv_ref_fulls under sms_tree and the
// subtrees.
static int simple_motion_search_get_best_ref(
    AV1_COMP *const cpi, MACROBLOCK *x, SIMPLE_MOTION_DATA_TREE *sms_tree,
    int mi_row, int mi_col, BLOCK_SIZE bsize, const int *const refs,
    int num_refs, int use_subpixel, int save_mv, unsigned int *best_sse,
    unsigned int *best_var) {
  const AV1_COMMON *const cm = &cpi->common;
  int best_ref = -1;

  if (mi_col >= cm->mi_params.mi_cols || mi_row >= cm->mi_params.mi_rows) {
    // If the whole block is outside of the image, set the var and sse to 0.
    *best_var = 0;
    *best_sse = 0;

    return best_ref;
  }

  // Otherwise do loop through the reference frames and find the one with the
  // minimum SSE
  const int num_planes = 1;

  *best_sse = INT_MAX;

  for (int ref_idx = 0; ref_idx < num_refs; ref_idx++) {
    const int ref = refs[ref_idx];

    if (cpi->ref_frame_flags & av1_ref_frame_flag_list[ref]) {
      const FULLPEL_MV *start_mvs = sms_tree->start_mvs;
      unsigned int curr_sse = 0, curr_var = 0;
      const int_mv best_mv = av1_simple_motion_search_sse_var(
          cpi, x, mi_row, mi_col, bsize, ref, start_mvs[ref], num_planes,
          use_subpixel, &curr_sse, &curr_var);
      if (curr_sse < *best_sse) {
        *best_sse = curr_sse;
        *best_var = curr_var;
        best_ref = ref;
      }

      if (save_mv) {
        sms_tree->start_mvs[ref].row = best_mv.as_mv.row / 8;
        sms_tree->start_mvs[ref].col = best_mv.as_mv.col / 8;

        if (bsize >= BLOCK_8X8) {
          for (int r_idx = 0; r_idx < SUB_PARTITIONS_SPLIT; r_idx++) {
            // Propagate the new motion vectors to a lower level
            SIMPLE_MOTION_DATA_TREE *sub_tree = sms_tree->split[r_idx];
            sub_tree->start_mvs[ref] = sms_tree->start_mvs[ref];
          }
        }
      }
    }
  }

  return best_ref;
}

// Collects features using simple_motion_search and store them in features. The
// features are also cached in SIMPLE_MOTION_DATA_TREE. By default, the features
// collected are the sse and var from the subblocks flagged by features_to_get.
// Furthermore, if features is not NULL, then 7 more features are appended to
// the end of features:
//  - log(1.0 + dc_q ** 2)
//  - whether an above macroblock exists
//  - width of above macroblock
//  - height of above macroblock
//  - whether a left marcoblock exists
//  - width of left macroblock
//  - height of left macroblock
static AOM_INLINE void simple_motion_search_prune_part_features(
    AV1_COMP *const cpi, MACROBLOCK *x, SIMPLE_MOTION_DATA_TREE *sms_tree,
    int mi_row, int mi_col, BLOCK_SIZE bsize, float *features,
    int features_to_get) {
  const int w_mi = mi_size_wide[bsize];
  const int h_mi = mi_size_high[bsize];
  assert(mi_size_wide[bsize] == mi_size_high[bsize]);
  assert(bsize >= BLOCK_8X8);
  assert(cpi->ref_frame_flags & av1_ref_frame_flag_list[LAST_FRAME] ||
         cpi->ref_frame_flags & av1_ref_frame_flag_list[ALTREF_FRAME]);

  // Setting up motion search
  const int ref_list[] = { cpi->rc.is_src_frame_alt_ref ? ALTREF_FRAME
                                                        : LAST_FRAME };
  const int num_refs = 1;
  const int use_subpixel = 1;

  // Doing whole block first to update the mv
  if (!sms_tree->sms_none_valid && features_to_get & FEATURE_SMS_NONE_FLAG) {
    simple_motion_search_get_best_ref(cpi, x, sms_tree, mi_row, mi_col, bsize,
                                      ref_list, num_refs, use_subpixel, 1,
                                      &sms_tree->sms_none_feat[0],
                                      &sms_tree->sms_none_feat[1]);
    sms_tree->sms_none_valid = 1;
  }

  // Split subblocks
  if (features_to_get & FEATURE_SMS_SPLIT_FLAG) {
    const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    for (int r_idx = 0; r_idx < SUB_PARTITIONS_SPLIT; r_idx++) {
      const int sub_mi_col = mi_col + (r_idx & 1) * w_mi / 2;
      const int sub_mi_row = mi_row + (r_idx >> 1) * h_mi / 2;
      SIMPLE_MOTION_DATA_TREE *sub_tree = sms_tree->split[r_idx];

      if (!sub_tree->sms_none_valid) {
        simple_motion_search_get_best_ref(
            cpi, x, sub_tree, sub_mi_row, sub_mi_col, subsize, ref_list,
            num_refs, use_subpixel, 1, &sub_tree->sms_none_feat[0],
            &sub_tree->sms_none_feat[1]);
        sub_tree->sms_none_valid = 1;
      }
    }
  }

  // Rectangular subblocks
  if (!sms_tree->sms_rect_valid && features_to_get & FEATURE_SMS_RECT_FLAG) {
    // Horz subblock
    BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_HORZ);
    for (int r_idx = 0; r_idx < SUB_PARTITIONS_RECT; r_idx++) {
      const int sub_mi_col = mi_col + 0;
      const int sub_mi_row = mi_row + r_idx * h_mi / 2;

      simple_motion_search_get_best_ref(
          cpi, x, sms_tree, sub_mi_row, sub_mi_col, subsize, ref_list, num_refs,
          use_subpixel, 0, &sms_tree->sms_rect_feat[2 * r_idx],
          &sms_tree->sms_rect_feat[2 * r_idx + 1]);
    }

    // Vert subblock
    subsize = get_partition_subsize(bsize, PARTITION_VERT);
    for (int r_idx = 0; r_idx < SUB_PARTITIONS_RECT; r_idx++) {
      const int sub_mi_col = mi_col + r_idx * w_mi / 2;
      const int sub_mi_row = mi_row + 0;

      simple_motion_search_get_best_ref(
          cpi, x, sms_tree, sub_mi_row, sub_mi_col, subsize, ref_list, num_refs,
          use_subpixel, 0, &sms_tree->sms_rect_feat[4 + 2 * r_idx],
          &sms_tree->sms_rect_feat[4 + 2 * r_idx + 1]);
    }
    sms_tree->sms_rect_valid = 1;
  }

  if (!features) return;

  int f_idx = 0;
  if (features_to_get & FEATURE_SMS_NONE_FLAG) {
    for (int sub_idx = 0; sub_idx < 2; sub_idx++) {
      features[f_idx++] = log1pf((float)sms_tree->sms_none_feat[sub_idx]);
    }
  }

  if (features_to_get & FEATURE_SMS_SPLIT_FLAG) {
    for (int sub_idx = 0; sub_idx < SUB_PARTITIONS_SPLIT; sub_idx++) {
      SIMPLE_MOTION_DATA_TREE *sub_tree = sms_tree->split[sub_idx];
      features[f_idx++] = log1pf((float)sub_tree->sms_none_feat[0]);
      features[f_idx++] = log1pf((float)sub_tree->sms_none_feat[1]);
    }
  }

  if (features_to_get & FEATURE_SMS_RECT_FLAG) {
    for (int sub_idx = 0; sub_idx < 8; sub_idx++) {
      features[f_idx++] = log1pf((float)sms_tree->sms_rect_feat[sub_idx]);
    }
  }

  const MACROBLOCKD *xd = &x->e_mbd;
  set_offsets_for_motion_search(cpi, x, mi_row, mi_col, bsize);

  // Q_INDEX
  const int dc_q = av1_dc_quant_QTX(x->qindex, 0, xd->bd) >> (xd->bd - 8);
  features[f_idx++] = log1pf((float)(dc_q * dc_q) / 256.0f);

  // Neighbor stuff
  const int has_above = !!xd->above_mbmi;
  const int has_left = !!xd->left_mbmi;
  const BLOCK_SIZE above_bsize = has_above ? xd->above_mbmi->bsize : bsize;
  const BLOCK_SIZE left_bsize = has_left ? xd->left_mbmi->bsize : bsize;
  features[f_idx++] = (float)has_above;
  features[f_idx++] = (float)mi_size_wide_log2[above_bsize];
  features[f_idx++] = (float)mi_size_high_log2[above_bsize];
  features[f_idx++] = (float)has_left;
  features[f_idx++] = (float)mi_size_wide_log2[left_bsize];
  features[f_idx++] = (float)mi_size_high_log2[left_bsize];
}

void av1_simple_motion_search_prune_rect(AV1_COMP *const cpi, MACROBLOCK *x,
                                         SIMPLE_MOTION_DATA_TREE *sms_tree,
                                         PartitionSearchState *part_state) {
  const AV1_COMMON *const cm = &cpi->common;
  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const int mi_row = blk_params->mi_row, mi_col = blk_params->mi_col;
  const BLOCK_SIZE bsize = blk_params->bsize;

  const int bsize_idx = convert_bsize_to_idx(bsize);
  const int is_720p_or_larger = AOMMIN(cm->width, cm->height) >= 720;
  const int is_480p_or_larger = AOMMIN(cm->width, cm->height) >= 480;
  // res_idx is 0 for lowres, 1 for 48p, 2 for 720p+
  const int res_idx = is_480p_or_larger + is_720p_or_larger;

  // Get model parameters
  const NN_CONFIG *nn_config =
      av1_simple_motion_search_prune_rect_nn_config[bsize_idx];
  const float *ml_mean = av1_simple_motion_search_prune_rect_mean[bsize_idx],
              *ml_std = av1_simple_motion_search_prune_rect_std[bsize_idx];

  const int agg = get_simple_motion_search_prune_agg(
      x->qindex, cpi->sf.part_sf.simple_motion_search_prune_agg, 1);
  if (agg < 0) {
    return;
  }

  const float prune_thresh =
      av1_simple_motion_search_prune_rect_thresh[agg][res_idx][bsize_idx];

  // If there is no valid threshold, return immediately.
  if (!nn_config || prune_thresh == 0.0f) {
    return;
  }

  // Get features
  float features[FEATURE_SIZE_SMS_PRUNE_PART] = { 0.0f };
  simple_motion_search_prune_part_features(cpi, x, sms_tree, mi_row, mi_col,
                                           bsize, features,
                                           FEATURE_SMS_PRUNE_PART_FLAG);

  // Note: it is intended to not normalize the features here, to keep it
  // consistent for all features collected and passed to the external model.
  if (cpi->sf.part_sf.simple_motion_search_prune_rect &&
      !frame_is_intra_only(cm) &&
      (part_state->partition_rect_allowed[HORZ] ||
       part_state->partition_rect_allowed[VERT]) &&
      bsize >= BLOCK_8X8 && !av1_superres_scaled(cm)) {
    // Write features to file
    write_features_to_file(
        cpi->oxcf.partition_info_path, cpi->ext_part_controller.test_mode,
        features, FEATURE_SIZE_SMS_PRUNE_PART, 1, bsize, mi_row, mi_col);

    if (ext_ml_model_decision_before_none_part2(
            cpi, features, &part_state->prune_rect_part[HORZ],
            &part_state->prune_rect_part[VERT])) {
      return;
    }
  }

  for (int f_idx = 0; f_idx < FEATURE_SIZE_SMS_PRUNE_PART; f_idx++) {
    features[f_idx] = (features[f_idx] - ml_mean[f_idx]) / ml_std[f_idx];
  }

  // Get probabilities
  float scores[EXT_PARTITION_TYPES] = { 0.0f },
        probs[EXT_PARTITION_TYPES] = { 0.0f };
  const int num_classes = (bsize == BLOCK_128X128 || bsize == BLOCK_8X8)
                              ? PARTITION_TYPES
                              : EXT_PARTITION_TYPES;

  av1_nn_predict(features, nn_config, 1, scores);

  av1_nn_softmax(scores, probs, num_classes);

  // Determine if we should prune rectangular partitions.
  if (probs[PARTITION_HORZ] <= prune_thresh) {
    part_state->prune_rect_part[HORZ] = 1;
  }
  if (probs[PARTITION_VERT] <= prune_thresh) {
    part_state->prune_rect_part[VERT] = 1;
  }
}

// Early terminates PARTITION_NONE using simple_motion_search features and the
// rate, distortion, and rdcost of PARTITION_NONE. This is only called when:
//  - The frame is a show frame
//  - The frame is not intra only
//  - The current bsize is > BLOCK_8X8
//  - blk_row + blk_height/2 < total_rows and blk_col + blk_width/2 < total_cols
void av1_simple_motion_search_early_term_none(
    AV1_COMP *const cpi, MACROBLOCK *x, SIMPLE_MOTION_DATA_TREE *sms_tree,
    const RD_STATS *none_rdc, PartitionSearchState *part_state) {
  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const int mi_row = blk_params->mi_row, mi_col = blk_params->mi_col;
  const BLOCK_SIZE bsize = blk_params->bsize;

  float features[FEATURE_SIZE_SMS_TERM_NONE] = { 0.0f };
  simple_motion_search_prune_part_features(cpi, x, sms_tree, mi_row, mi_col,
                                           bsize, features,
                                           FEATURE_SMS_PRUNE_PART_FLAG);
  int f_idx = FEATURE_SIZE_SMS_PRUNE_PART;

  features[f_idx++] = log1pf((float)none_rdc->rate);
  features[f_idx++] = log1pf((float)none_rdc->dist);
  features[f_idx++] = log1pf((float)none_rdc->rdcost);

  assert(f_idx == FEATURE_SIZE_SMS_TERM_NONE);

  const float *ml_mean = NULL;
  const float *ml_std = NULL;
  const float *ml_model = NULL;

  if (bsize == BLOCK_128X128) {
    ml_mean = av1_simple_motion_search_term_none_mean_128;
    ml_std = av1_simple_motion_search_term_none_std_128;
    ml_model = av1_simple_motion_search_term_none_model_128;
  } else if (bsize == BLOCK_64X64) {
    ml_mean = av1_simple_motion_search_term_none_mean_64;
    ml_std = av1_simple_motion_search_term_none_std_64;
    ml_model = av1_simple_motion_search_term_none_model_64;
  } else if (bsize == BLOCK_32X32) {
    ml_mean = av1_simple_motion_search_term_none_mean_32;
    ml_std = av1_simple_motion_search_term_none_std_32;
    ml_model = av1_simple_motion_search_term_none_model_32;
  } else if (bsize == BLOCK_16X16) {
    ml_mean = av1_simple_motion_search_term_none_mean_16;
    ml_std = av1_simple_motion_search_term_none_std_16;
    ml_model = av1_simple_motion_search_term_none_model_16;
  } else {
    assert(0 && "Unexpected block size in simple_motion_term_none");
  }

  // Write features to file
  write_features_to_file(cpi->oxcf.partition_info_path,
                         cpi->ext_part_controller.test_mode, features,
                         FEATURE_SIZE_SMS_TERM_NONE, 3, bsize, mi_row, mi_col);

  if (ext_ml_model_decision_after_none_part2(
          cpi, features, &part_state->terminate_partition_search)) {
    return;
  }

  if (ml_model) {
    float score = 0.0f;
    for (f_idx = 0; f_idx < FEATURE_SIZE_SMS_TERM_NONE; f_idx++) {
      score +=
          ml_model[f_idx] * (features[f_idx] - ml_mean[f_idx]) / ml_std[f_idx];
    }
    score += ml_model[FEATURE_SIZE_SMS_TERM_NONE];

    if (score >= 0.0f) {
      part_state->terminate_partition_search = 1;
    }
  }
}

void av1_get_max_min_partition_features(AV1_COMP *const cpi, MACROBLOCK *x,
                                        int mi_row, int mi_col,
                                        float *features) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  const BLOCK_SIZE sb_size = cm->seq_params->sb_size;

  // Currently this only allows 128X128 SB size. May extend it to 64X64 SB size.
  assert(sb_size == BLOCK_128X128);

  int f_idx = 0;

  const int dc_q = av1_dc_quant_QTX(x->qindex, 0, xd->bd) >> (xd->bd - 8);
  const float log_q_sq = log1pf((float)(dc_q * dc_q) / 256.0f);

  // Perform full-pixel single motion search in Y plane of 16x16 mbs in the sb
  float sum_mv_row_sq = 0;
  float sum_mv_row = 0;
  float min_abs_mv_row = FLT_MAX;
  float max_abs_mv_row = 0;

  float sum_mv_col_sq = 0;
  float sum_mv_col = 0;
  float min_abs_mv_col = FLT_MAX;
  float max_abs_mv_col = 0;

  float sum_log_sse_sq = 0;
  float sum_log_sse = 0;
  float min_log_sse = FLT_MAX;
  float max_log_sse = 0;

  const BLOCK_SIZE mb_size = BLOCK_16X16;
  const int mb_rows = block_size_high[sb_size] / block_size_high[mb_size];
  const int mb_cols = block_size_wide[sb_size] / block_size_wide[mb_size];
  const int mb_in_mi_size_high_log2 = mi_size_high_log2[mb_size];
  const int mb_in_mi_size_wide_log2 = mi_size_wide_log2[mb_size];

  for (int mb_row = 0; mb_row < mb_rows; mb_row++)
    for (int mb_col = 0; mb_col < mb_cols; mb_col++) {
      const int this_mi_row = mi_row + (mb_row << mb_in_mi_size_high_log2);
      const int this_mi_col = mi_col + (mb_col << mb_in_mi_size_wide_log2);
      unsigned int sse = 0;
      unsigned int var = 0;
      const FULLPEL_MV start_mv = kZeroFullMv;
      const MV_REFERENCE_FRAME ref =
          cpi->rc.is_src_frame_alt_ref ? ALTREF_FRAME : LAST_FRAME;
      const int_mv best_mv = av1_simple_motion_search_sse_var(
          cpi, x, this_mi_row, this_mi_col, mb_size, ref, start_mv, 1, 0, &sse,
          &var);

      const float mv_row = (float)(best_mv.as_mv.row / 8);
      const float mv_col = (float)(best_mv.as_mv.col / 8);
      const float log_sse = log1pf((float)sse);
      const float abs_mv_row = fabsf(mv_row);
      const float abs_mv_col = fabsf(mv_col);

      sum_mv_row_sq += mv_row * mv_row;
      sum_mv_row += mv_row;
      sum_mv_col_sq += mv_col * mv_col;
      sum_mv_col += mv_col;

      if (abs_mv_row < min_abs_mv_row) min_abs_mv_row = abs_mv_row;
      if (abs_mv_row > max_abs_mv_row) max_abs_mv_row = abs_mv_row;
      if (abs_mv_col < min_abs_mv_col) min_abs_mv_col = abs_mv_col;
      if (abs_mv_col > max_abs_mv_col) max_abs_mv_col = abs_mv_col;

      sum_log_sse_sq += log_sse * log_sse;
      sum_log_sse += log_sse;
      if (log_sse < min_log_sse) min_log_sse = log_sse;
      if (log_sse > max_log_sse) max_log_sse = log_sse;
    }
  const int blks = mb_rows * mb_cols;
  const float avg_mv_row = sum_mv_row / (float)blks;
  const float var_mv_row =
      sum_mv_row_sq / (float)blks - avg_mv_row * avg_mv_row;

  const float avg_mv_col = sum_mv_col / (float)blks;
  const float var_mv_col =
      sum_mv_col_sq / (float)blks - avg_mv_col * avg_mv_col;

  const float avg_log_sse = sum_log_sse / (float)blks;
  const float var_log_sse =
      sum_log_sse_sq / (float)blks - avg_log_sse * avg_log_sse;

  features[f_idx++] = avg_log_sse;
  features[f_idx++] = avg_mv_col;
  features[f_idx++] = avg_mv_row;
  features[f_idx++] = log_q_sq;
  features[f_idx++] = max_abs_mv_col;
  features[f_idx++] = max_abs_mv_row;
  features[f_idx++] = max_log_sse;
  features[f_idx++] = min_abs_mv_col;
  features[f_idx++] = min_abs_mv_row;
  features[f_idx++] = min_log_sse;
  features[f_idx++] = var_log_sse;
  features[f_idx++] = var_mv_col;
  features[f_idx++] = var_mv_row;

  assert(f_idx == FEATURE_SIZE_MAX_MIN_PART_PRED);
}

// Convert result index to block size.
// result idx     block size
//     0          BLOCK_16X16
//     1          BLOCK_32X32
//     2          BLOCK_64X64
//     3          BLOCK_128X128
static BLOCK_SIZE get_block_size(int idx) {
  return (BLOCK_SIZE)((idx + 2) * 3);
}

BLOCK_SIZE av1_predict_max_partition(const AV1_COMP *const cpi,
                                     const MACROBLOCK *const x,
                                     const float *features) {
  float scores[MAX_NUM_CLASSES_MAX_MIN_PART_PRED] = { 0.0f };
  const NN_CONFIG *nn_config = &av1_max_part_pred_nn_config;

  assert(cpi->sf.part_sf.auto_max_partition_based_on_simple_motion !=
         NOT_IN_USE);

  av1_nn_predict(features, nn_config, 1, scores);

  int result = MAX_NUM_CLASSES_MAX_MIN_PART_PRED - 1;
  if (cpi->sf.part_sf.auto_max_partition_based_on_simple_motion ==
      DIRECT_PRED) {
    result = 0;
    float max_score = scores[0];
    for (int i = 1; i < MAX_NUM_CLASSES_MAX_MIN_PART_PRED; ++i) {
      if (scores[i] > max_score) {
        max_score = scores[i];
        result = i;
      }
    }
    return get_block_size(result);
  }

  float probs[MAX_NUM_CLASSES_MAX_MIN_PART_PRED] = { 0.0f };
  av1_nn_softmax(scores, probs, MAX_NUM_CLASSES_MAX_MIN_PART_PRED);

  if (cpi->sf.part_sf.auto_max_partition_based_on_simple_motion ==
      RELAXED_PRED) {
    for (result = MAX_NUM_CLASSES_MAX_MIN_PART_PRED - 1; result >= 0;
         --result) {
      if (result < MAX_NUM_CLASSES_MAX_MIN_PART_PRED - 1) {
        probs[result] += probs[result + 1];
      }
      if (probs[result] > 0.2) break;
    }
  } else if (cpi->sf.part_sf.auto_max_partition_based_on_simple_motion ==
             ADAPT_PRED) {
    const BLOCK_SIZE sb_size = cpi->common.seq_params->sb_size;
    // TODO(debargha): x->source_variance is unavailable at this point,
    // so compute. The redundant recomputation later can be removed.
    const unsigned int source_variance = av1_get_perpixel_variance_facade(
        cpi, &x->e_mbd, &x->plane[0].src, sb_size, AOM_PLANE_Y);
    if (source_variance > 16) {
      const double thresh = source_variance < 128 ? 0.05 : 0.1;
      for (result = MAX_NUM_CLASSES_MAX_MIN_PART_PRED - 1; result >= 0;
           --result) {
        if (result < MAX_NUM_CLASSES_MAX_MIN_PART_PRED - 1) {
          probs[result] += probs[result + 1];
        }
        if (probs[result] > thresh) break;
      }
    }
  }

  return get_block_size(result);
}

// Get the minimum partition block width and height(in log scale) under a
// SIMPLE_MOTION_DATA_TREE.
static AOM_INLINE void get_min_bsize(const SIMPLE_MOTION_DATA_TREE *sms_tree,
                                     int *min_bw, int *min_bh) {
  if (!sms_tree) return;

  const BLOCK_SIZE bsize = sms_tree->block_size;
  if (bsize == BLOCK_4X4) {
    *min_bw = 0;
    *min_bh = 0;
    return;
  }

  PARTITION_TYPE part_type = sms_tree->partitioning;
  if (part_type == PARTITION_INVALID) return;

  if (part_type == PARTITION_SPLIT) {
    for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
      get_min_bsize(sms_tree->split[i], min_bw, min_bh);
    }
  } else {
    if (part_type == PARTITION_HORZ_A || part_type == PARTITION_HORZ_B ||
        part_type == PARTITION_VERT_A || part_type == PARTITION_VERT_B)
      part_type = PARTITION_SPLIT;
    const BLOCK_SIZE subsize = get_partition_subsize(bsize, part_type);
    if (subsize != BLOCK_INVALID) {
      *min_bw = AOMMIN(*min_bw, mi_size_wide_log2[subsize]);
      *min_bh = AOMMIN(*min_bh, mi_size_high_log2[subsize]);
    }
  }
}

static INLINE void add_rd_feature(int64_t rd, int64_t best_rd, float *features,
                                  int *feature_idx) {
  const int rd_valid = rd > 0 && rd < INT64_MAX;
  const float rd_ratio = rd_valid ? (float)rd / best_rd : 1.0f;
  features[(*feature_idx)++] = (float)rd_valid;
  features[(*feature_idx)++] = rd_ratio;
}

#define FEATURES 31
void av1_ml_early_term_after_split(AV1_COMP *const cpi, MACROBLOCK *const x,
                                   SIMPLE_MOTION_DATA_TREE *const sms_tree,
                                   int64_t best_rd, int64_t part_none_rd,
                                   int64_t part_split_rd,
                                   int64_t *split_block_rd,
                                   PartitionSearchState *part_state) {
  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const int mi_row = blk_params->mi_row, mi_col = blk_params->mi_col;
  const BLOCK_SIZE bsize = blk_params->bsize;

  if (best_rd <= 0 || best_rd == INT64_MAX ||
      part_state->terminate_partition_search)
    return;

  const AV1_COMMON *const cm = &cpi->common;
  const int is_480p_or_larger = AOMMIN(cm->width, cm->height) >= 480;
  const NN_CONFIG *nn_config = NULL;
  float thresh = -1e6;
  switch (bsize) {
    case BLOCK_128X128: break;
    case BLOCK_64X64:
      nn_config = &av1_early_term_after_split_nnconfig_64;
      thresh = is_480p_or_larger ? -2.0f : -1.2f;
      break;
    case BLOCK_32X32:
      nn_config = &av1_early_term_after_split_nnconfig_32;
      thresh = is_480p_or_larger ? -2.6f : -2.3f;
      break;
    case BLOCK_16X16:
      nn_config = &av1_early_term_after_split_nnconfig_16;
      thresh = is_480p_or_larger ? -2.0f : -2.4f;
      break;
    case BLOCK_8X8:
      nn_config = &av1_early_term_after_split_nnconfig_8;
      thresh = is_480p_or_larger ? -1.0f : -1.4f;
      break;
    case BLOCK_4X4: break;
    default:
      assert(0 && "Invalid block size in av1_ml_early_term_after_split().");
      break;
  }
  if (!nn_config) return;

  // Use more conservative threshold for level 1.
  if (cpi->sf.part_sf.ml_early_term_after_part_split_level < 2) thresh -= 0.3f;

  const MACROBLOCKD *const xd = &x->e_mbd;
  const int dc_q = av1_dc_quant_QTX(x->qindex, 0, xd->bd) >> (xd->bd - 8);
  const int bs = block_size_wide[bsize];
  int f_idx = 0;
  float features[FEATURES] = { 0.0f };

  features[f_idx++] = log1pf((float)dc_q / 4.0f);
  features[f_idx++] = log1pf((float)best_rd / bs / bs / 1024.0f);

  add_rd_feature(part_none_rd, best_rd, features, &f_idx);
  add_rd_feature(part_split_rd, best_rd, features, &f_idx);

  for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
    add_rd_feature(split_block_rd[i], best_rd, features, &f_idx);
    int min_bw = MAX_SB_SIZE_LOG2;
    int min_bh = MAX_SB_SIZE_LOG2;
    get_min_bsize(sms_tree->split[i], &min_bw, &min_bh);
    features[f_idx++] = (float)min_bw;
    features[f_idx++] = (float)min_bh;
  }

  simple_motion_search_prune_part_features(cpi, x, sms_tree, mi_row, mi_col,
                                           bsize, NULL,
                                           FEATURE_SMS_PRUNE_PART_FLAG);

  features[f_idx++] = log1pf((float)sms_tree->sms_none_feat[1]);

  features[f_idx++] = log1pf((float)sms_tree->split[0]->sms_none_feat[1]);
  features[f_idx++] = log1pf((float)sms_tree->split[1]->sms_none_feat[1]);
  features[f_idx++] = log1pf((float)sms_tree->split[2]->sms_none_feat[1]);
  features[f_idx++] = log1pf((float)sms_tree->split[3]->sms_none_feat[1]);

  features[f_idx++] = log1pf((float)sms_tree->sms_rect_feat[1]);
  features[f_idx++] = log1pf((float)sms_tree->sms_rect_feat[3]);
  features[f_idx++] = log1pf((float)sms_tree->sms_rect_feat[5]);
  features[f_idx++] = log1pf((float)sms_tree->sms_rect_feat[7]);

  assert(f_idx == FEATURES);

  // Write features to file
  write_features_to_file(cpi->oxcf.partition_info_path,
                         cpi->ext_part_controller.test_mode, features, FEATURES,
                         4, bsize, mi_row, mi_col);

  if (ext_ml_model_decision_after_split(
          cpi, features, &part_state->terminate_partition_search)) {
    return;
  }

  float score = 0.0f;
  av1_nn_predict(features, nn_config, 1, &score);
  // Score is indicator of confidence that we should NOT terminate.
  if (score < thresh) {
    part_state->terminate_partition_search = 1;
  }
}
#undef FEATURES

void av1_ml_prune_rect_partition(AV1_COMP *const cpi, const MACROBLOCK *const x,
                                 int64_t best_rd, int64_t none_rd,
                                 const int64_t *split_rd,
                                 PartitionSearchState *part_state) {
  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const int mi_row = blk_params->mi_row, mi_col = blk_params->mi_col;
  const BLOCK_SIZE bsize = blk_params->bsize;

  if (bsize < BLOCK_8X8 || best_rd >= 1000000000) return;
  best_rd = AOMMAX(best_rd, 1);
  const NN_CONFIG *nn_config = NULL;
  const float prob_thresholds[5] = { 0.01f, 0.01f, 0.004f, 0.002f, 0.002f };
  float cur_thresh = 0.0f;
  switch (bsize) {
    case BLOCK_8X8:
      nn_config = &av1_rect_partition_nnconfig_8;
      cur_thresh = prob_thresholds[0];
      break;
    case BLOCK_16X16:
      nn_config = &av1_rect_partition_nnconfig_16;
      cur_thresh = prob_thresholds[1];
      break;
    case BLOCK_32X32:
      nn_config = &av1_rect_partition_nnconfig_32;
      cur_thresh = prob_thresholds[2];
      break;
    case BLOCK_64X64:
      nn_config = &av1_rect_partition_nnconfig_64;
      cur_thresh = prob_thresholds[3];
      break;
    case BLOCK_128X128:
      nn_config = &av1_rect_partition_nnconfig_128;
      cur_thresh = prob_thresholds[4];
      break;
    default: assert(0 && "Unexpected bsize.");
  }
  if (!nn_config) return;

  // 1. Compute input features
  float features[9];

  // RD cost ratios
  for (int i = 0; i < 5; i++) features[i] = 1.0f;
  if (none_rd > 0 && none_rd < 1000000000)
    features[0] = (float)none_rd / (float)best_rd;
  for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++) {
    if (split_rd[i] > 0 && split_rd[i] < 1000000000)
      features[1 + i] = (float)split_rd[i] / (float)best_rd;
  }

  // Variance ratios
  const MACROBLOCKD *const xd = &x->e_mbd;
  int whole_block_variance;
  whole_block_variance = av1_get_perpixel_variance_facade(
      cpi, xd, &x->plane[0].src, bsize, AOM_PLANE_Y);
  whole_block_variance = AOMMAX(whole_block_variance, 1);

  int split_variance[SUB_PARTITIONS_SPLIT];
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
  struct buf_2d buf;
  buf.stride = x->plane[0].src.stride;
  const int bw = block_size_wide[bsize];
  for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
    const int x_idx = (i & 1) * bw / 2;
    const int y_idx = (i >> 1) * bw / 2;
    buf.buf = x->plane[0].src.buf + x_idx + y_idx * buf.stride;
    split_variance[i] =
        av1_get_perpixel_variance_facade(cpi, xd, &buf, subsize, AOM_PLANE_Y);
  }

  for (int i = 0; i < SUB_PARTITIONS_SPLIT; i++)
    features[5 + i] = (float)split_variance[i] / (float)whole_block_variance;

  // Write features to file
  write_features_to_file(cpi->oxcf.partition_info_path,
                         cpi->ext_part_controller.test_mode, features,
                         /*feature_size=*/9, 5, bsize, mi_row, mi_col);

  if (ext_ml_model_decision_after_split_part2(
          &cpi->ext_part_controller, frame_is_intra_only(&cpi->common),
          features, &part_state->prune_rect_part[HORZ],
          &part_state->prune_rect_part[VERT])) {
    return;
  }

  // 2. Do the prediction and prune 0-2 partitions based on their probabilities
  float raw_scores[3] = { 0.0f };
  av1_nn_predict(features, nn_config, 1, raw_scores);
  float probs[3] = { 0.0f };
  av1_nn_softmax(raw_scores, probs, 3);

  // probs[0] is the probability of the fact that both rectangular partitions
  // are worse than current best_rd
  if (probs[1] <= cur_thresh) part_state->prune_rect_part[HORZ] = 1;
  if (probs[2] <= cur_thresh) part_state->prune_rect_part[VERT] = 1;
}

// Use a ML model to predict if horz_a, horz_b, vert_a, and vert_b should be
// considered.
void av1_ml_prune_ab_partition(AV1_COMP *const cpi, int part_ctx, int var_ctx,
                               int64_t best_rd,
                               PartitionSearchState *part_state,
                               int *ab_partitions_allowed) {
  const PartitionBlkParams blk_params = part_state->part_blk_params;
  const int mi_row = blk_params.mi_row;
  const int mi_col = blk_params.mi_col;
  const BLOCK_SIZE bsize = blk_params.bsize;

  if (bsize < BLOCK_8X8 || best_rd >= 1000000000) return;
  const NN_CONFIG *nn_config = NULL;
  switch (bsize) {
    case BLOCK_8X8: nn_config = NULL; break;
    case BLOCK_16X16: nn_config = &av1_ab_partition_nnconfig_16; break;
    case BLOCK_32X32: nn_config = &av1_ab_partition_nnconfig_32; break;
    case BLOCK_64X64: nn_config = &av1_ab_partition_nnconfig_64; break;
    case BLOCK_128X128: nn_config = &av1_ab_partition_nnconfig_128; break;
    default: assert(0 && "Unexpected bsize.");
  }
  if (!nn_config) return;

  // Generate features.
  float features[10];
  int feature_index = 0;
  features[feature_index++] = (float)part_ctx;
  features[feature_index++] = (float)var_ctx;
  const int rdcost = (int)AOMMIN(INT_MAX, best_rd);
  int sub_block_rdcost[8] = { 0 };
  int rd_index = 0;
  for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
    const int64_t *horz_rd = part_state->rect_part_rd[HORZ];
    if (horz_rd[i] > 0 && horz_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)horz_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
    const int64_t *vert_rd = part_state->rect_part_rd[VERT];
    if (vert_rd[i] > 0 && vert_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)vert_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
    const int64_t *split_rd = part_state->split_rd;
    if (split_rd[i] > 0 && split_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)split_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < 8; ++i) {
    // Ratio between the sub-block RD and the whole-block RD.
    float rd_ratio = 1.0f;
    if (sub_block_rdcost[i] > 0 && sub_block_rdcost[i] < rdcost)
      rd_ratio = (float)sub_block_rdcost[i] / (float)rdcost;
    features[feature_index++] = rd_ratio;
  }
  assert(feature_index == 10);

  // Write features to file
  if (!frame_is_intra_only(&cpi->common)) {
    write_features_to_file(cpi->oxcf.partition_info_path,
                           cpi->ext_part_controller.test_mode, features,
                           /*feature_size=*/10, 6, bsize, mi_row, mi_col);
  }

  if (ext_ml_model_decision_after_rect(
          &cpi->ext_part_controller, frame_is_intra_only(&cpi->common),
          features, &ab_partitions_allowed[HORZ_A],
          &ab_partitions_allowed[HORZ_B], &ab_partitions_allowed[VERT_A],
          &ab_partitions_allowed[VERT_B])) {
    return;
  }

  // Calculate scores using the NN model.
  float score[16] = { 0.0f };
  av1_nn_predict(features, nn_config, 1, score);
  int int_score[16];
  int max_score = -1000;
  for (int i = 0; i < 16; ++i) {
    int_score[i] = (int)(100 * score[i]);
    max_score = AOMMAX(int_score[i], max_score);
  }

  // Make decisions based on the model scores.
  int thresh = max_score;
  switch (bsize) {
    case BLOCK_16X16: thresh -= 150; break;
    case BLOCK_32X32: thresh -= 100; break;
    default: break;
  }
  av1_zero_array(ab_partitions_allowed, NUM_AB_PARTS);
  for (int i = 0; i < 16; ++i) {
    if (int_score[i] >= thresh) {
      if ((i >> 0) & 1) ab_partitions_allowed[HORZ_A] = 1;
      if ((i >> 1) & 1) ab_partitions_allowed[HORZ_B] = 1;
      if ((i >> 2) & 1) ab_partitions_allowed[VERT_A] = 1;
      if ((i >> 3) & 1) ab_partitions_allowed[VERT_B] = 1;
    }
  }
}

#define FEATURES 18
#define LABELS 4
// Use a ML model to predict if horz4 and vert4 should be considered.
void av1_ml_prune_4_partition(AV1_COMP *const cpi, MACROBLOCK *const x,
                              int part_ctx, int64_t best_rd,
                              PartitionSearchState *part_state,
                              int *part4_allowed,
                              unsigned int pb_source_variance) {
  const PartitionBlkParams blk_params = part_state->part_blk_params;
  const int mi_row = blk_params.mi_row;
  const int mi_col = blk_params.mi_col;
  const BLOCK_SIZE bsize = blk_params.bsize;

  int64_t(*rect_part_rd)[SUB_PARTITIONS_RECT] = part_state->rect_part_rd;
  int64_t *split_rd = part_state->split_rd;
  if (ext_ml_model_decision_after_part_ab(
          cpi, x, bsize, part_ctx, best_rd, rect_part_rd, split_rd,
          &part4_allowed[HORZ4], &part4_allowed[VERT4], pb_source_variance,
          mi_row, mi_col))
    return;

  if (best_rd >= 1000000000) return;
  int64_t *horz_rd = rect_part_rd[HORZ4];
  int64_t *vert_rd = rect_part_rd[VERT4];
  const NN_CONFIG *nn_config = NULL;
  // 4-way partitions are only allowed for these three square block sizes.
  switch (bsize) {
    case BLOCK_16X16: nn_config = &av1_4_partition_nnconfig_16; break;
    case BLOCK_32X32: nn_config = &av1_4_partition_nnconfig_32; break;
    case BLOCK_64X64: nn_config = &av1_4_partition_nnconfig_64; break;
    default: assert(0 && "Unexpected bsize.");
  }
  if (!nn_config) return;

  // Generate features.
  float features[FEATURES];
  int feature_index = 0;
  features[feature_index++] = (float)part_ctx;
  features[feature_index++] = (float)get_unsigned_bits(pb_source_variance);

  const int rdcost = (int)AOMMIN(INT_MAX, best_rd);
  int sub_block_rdcost[8] = { 0 };
  int rd_index = 0;
  for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
    if (horz_rd[i] > 0 && horz_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)horz_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
    if (vert_rd[i] > 0 && vert_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)vert_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
    if (split_rd[i] > 0 && split_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)split_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < 8; ++i) {
    // Ratio between the sub-block RD and the whole-block RD.
    float rd_ratio = 1.0f;
    if (sub_block_rdcost[i] > 0 && sub_block_rdcost[i] < rdcost)
      rd_ratio = (float)sub_block_rdcost[i] / (float)rdcost;
    features[feature_index++] = rd_ratio;
  }

  // Get variance of the 1:4 and 4:1 sub-blocks.
  unsigned int horz_4_source_var[SUB_PARTITIONS_PART4] = { 0 };
  unsigned int vert_4_source_var[SUB_PARTITIONS_PART4] = { 0 };
  {
    BLOCK_SIZE horz_4_bs = get_partition_subsize(bsize, PARTITION_HORZ_4);
    BLOCK_SIZE vert_4_bs = get_partition_subsize(bsize, PARTITION_VERT_4);

    assert(horz_4_bs != BLOCK_INVALID);
    assert(vert_4_bs != BLOCK_INVALID);

    av1_setup_src_planes(x, cpi->source, mi_row, mi_col,
                         av1_num_planes(&cpi->common), bsize);
    const int src_stride = x->plane[0].src.stride;
    uint8_t *src = x->plane[0].src.buf;
    const MACROBLOCKD *const xd = &x->e_mbd;

    struct buf_2d horz_4_src, vert_4_src;
    horz_4_src.stride = src_stride;
    vert_4_src.stride = src_stride;

    for (int i = 0; i < SUB_PARTITIONS_PART4; ++i) {
      horz_4_src.buf = src + i * block_size_high[horz_4_bs] * src_stride;
      vert_4_src.buf = src + i * block_size_wide[vert_4_bs];

      horz_4_source_var[i] = av1_get_perpixel_variance_facade(
          cpi, xd, &horz_4_src, horz_4_bs, AOM_PLANE_Y);
      vert_4_source_var[i] = av1_get_perpixel_variance_facade(
          cpi, xd, &vert_4_src, vert_4_bs, AOM_PLANE_Y);
    }
  }

  const float denom = (float)(pb_source_variance + 1);
  const float low_b = 0.1f;
  const float high_b = 10.0f;
  for (int i = 0; i < SUB_PARTITIONS_PART4; ++i) {
    // Ratio between the 4:1 sub-block variance and the whole-block variance.
    float var_ratio = (float)(horz_4_source_var[i] + 1) / denom;
    if (var_ratio < low_b) var_ratio = low_b;
    if (var_ratio > high_b) var_ratio = high_b;
    features[feature_index++] = var_ratio;
  }
  for (int i = 0; i < SUB_PARTITIONS_PART4; ++i) {
    // Ratio between the 1:4 sub-block RD and the whole-block RD.
    float var_ratio = (float)(vert_4_source_var[i] + 1) / denom;
    if (var_ratio < low_b) var_ratio = low_b;
    if (var_ratio > high_b) var_ratio = high_b;
    features[feature_index++] = var_ratio;
  }
  assert(feature_index == FEATURES);

  // Write features to file
  if (!frame_is_intra_only(&cpi->common)) {
    write_features_to_file(cpi->oxcf.partition_info_path,
                           cpi->ext_part_controller.test_mode, features,
                           FEATURES, 7, bsize, mi_row, mi_col);
  }

  // Calculate scores using the NN model.
  float score[LABELS] = { 0.0f };
  av1_nn_predict(features, nn_config, 1, score);
  int int_score[LABELS];
  int max_score = -1000;
  for (int i = 0; i < LABELS; ++i) {
    int_score[i] = (int)(100 * score[i]);
    max_score = AOMMAX(int_score[i], max_score);
  }

  // Make decisions based on the model scores.
  int thresh = max_score;
  switch (bsize) {
    case BLOCK_16X16: thresh -= 500; break;
    case BLOCK_32X32: thresh -= 500; break;
    case BLOCK_64X64: thresh -= 200; break;
    default: break;
  }
  av1_zero_array(part4_allowed, NUM_PART4_TYPES);
  for (int i = 0; i < LABELS; ++i) {
    if (int_score[i] >= thresh) {
      if ((i >> 0) & 1) part4_allowed[HORZ4] = 1;
      if ((i >> 1) & 1) part4_allowed[VERT4] = 1;
    }
  }
}
#undef FEATURES
#undef LABELS

#define FEATURES 4
void av1_ml_predict_breakout(AV1_COMP *const cpi, const MACROBLOCK *const x,
                             const RD_STATS *const rd_stats,
                             unsigned int pb_source_variance, int bit_depth,
                             PartitionSearchState *part_state) {
  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const int mi_row = blk_params->mi_row, mi_col = blk_params->mi_col;
  const BLOCK_SIZE bsize = blk_params->bsize;

  const NN_CONFIG *nn_config = NULL;
  int thresh = 0;
  switch (bsize) {
    case BLOCK_8X8:
      nn_config = &av1_partition_breakout_nnconfig_8;
      thresh = cpi->sf.part_sf.ml_partition_search_breakout_thresh[0];
      break;
    case BLOCK_16X16:
      nn_config = &av1_partition_breakout_nnconfig_16;
      thresh = cpi->sf.part_sf.ml_partition_search_breakout_thresh[1];
      break;
    case BLOCK_32X32:
      nn_config = &av1_partition_breakout_nnconfig_32;
      thresh = cpi->sf.part_sf.ml_partition_search_breakout_thresh[2];
      break;
    case BLOCK_64X64:
      nn_config = &av1_partition_breakout_nnconfig_64;
      thresh = cpi->sf.part_sf.ml_partition_search_breakout_thresh[3];
      break;
    case BLOCK_128X128:
      nn_config = &av1_partition_breakout_nnconfig_128;
      thresh = cpi->sf.part_sf.ml_partition_search_breakout_thresh[4];
      break;
    default: assert(0 && "Unexpected bsize.");
  }
  if (!nn_config || thresh < 0) return;

  const float ml_predict_breakout_thresh_scale[3] = { 1.15f, 1.05f, 1.0f };
  thresh = (int)((float)thresh *
                 ml_predict_breakout_thresh_scale
                     [cpi->sf.part_sf.ml_predict_breakout_level - 1]);

  // Generate feature values.
  float features[FEATURES];
  int feature_index = 0;

  const int num_pels_log2 = num_pels_log2_lookup[bsize];
  float rate_f = (float)AOMMIN(rd_stats->rate, INT_MAX);
  rate_f = ((float)x->rdmult / 128.0f / 512.0f / (float)(1 << num_pels_log2)) *
           rate_f;
  features[feature_index++] = rate_f;

  const float dist_f =
      (float)(AOMMIN(rd_stats->dist, INT_MAX) >> num_pels_log2);
  features[feature_index++] = dist_f;

  features[feature_index++] = (float)pb_source_variance;

  const int dc_q = (int)x->plane[0].dequant_QTX[0] >> (bit_depth - 8);
  features[feature_index++] = (float)(dc_q * dc_q) / 256.0f;
  assert(feature_index == FEATURES);

  // Write features to file
  write_features_to_file(cpi->oxcf.partition_info_path,
                         cpi->ext_part_controller.test_mode, features, FEATURES,
                         2, bsize, mi_row, mi_col);

  if (ext_ml_model_decision_after_none(&cpi->ext_part_controller,
                                       frame_is_intra_only(&cpi->common),
                                       features, &part_state->do_square_split,
                                       &part_state->do_rectangular_split)) {
    return;
  }

  // Calculate score using the NN model.
  float score = 0.0f;
  av1_nn_predict(features, nn_config, 1, &score);

  // Make decision.
  if ((int)(score * 100) >= thresh) {
    part_state->do_square_split = 0;
    part_state->do_rectangular_split = 0;
  }
}
#undef FEATURES

void av1_prune_partitions_before_search(AV1_COMP *const cpi,
                                        MACROBLOCK *const x,
                                        SIMPLE_MOTION_DATA_TREE *const sms_tree,
                                        PartitionSearchState *part_state) {
  const AV1_COMMON *const cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;

  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const BLOCK_SIZE bsize = blk_params->bsize;

  if (cpi->third_pass_ctx) {
    int mi_row = blk_params->mi_row;
    int mi_col = blk_params->mi_col;
    double ratio_h, ratio_w;
    av1_get_third_pass_ratio(cpi->third_pass_ctx, 0, cm->height, cm->width,
                             &ratio_h, &ratio_w);
    THIRD_PASS_MI_INFO *this_mi = av1_get_third_pass_mi(
        cpi->third_pass_ctx, 0, mi_row, mi_col, ratio_h, ratio_w);
    BLOCK_SIZE third_pass_bsize =
        av1_get_third_pass_adjusted_blk_size(this_mi, ratio_h, ratio_w);
    // check the actual partition of this block in the second pass
    PARTITION_TYPE third_pass_part =
        av1_third_pass_get_sb_part_type(cpi->third_pass_ctx, this_mi);

    int is_edge = (mi_row + mi_size_high[bsize] >= cm->mi_params.mi_rows) ||
                  (mi_col + mi_size_wide[bsize] >= cm->mi_params.mi_cols);

    if (!is_edge && block_size_wide[bsize] >= 16) {
      // If in second pass we used rectangular partition, then do not search for
      // rectangular partition in the different direction.
      if (third_pass_part != PARTITION_NONE) {
        if (third_pass_part == PARTITION_HORZ ||
            third_pass_part == PARTITION_HORZ_4 ||
            third_pass_part == PARTITION_HORZ_A ||
            third_pass_part == PARTITION_HORZ_B) {
          part_state->partition_rect_allowed[VERT] = 0;
        } else if (third_pass_part == PARTITION_VERT ||
                   third_pass_part == PARTITION_VERT_4 ||
                   third_pass_part == PARTITION_VERT_A ||
                   third_pass_part == PARTITION_VERT_B) {
          part_state->partition_rect_allowed[HORZ] = 0;
        }
      }

      int minSize = AOMMIN(block_size_wide[third_pass_bsize],
                           block_size_high[third_pass_bsize]);
      int maxSize = AOMMAX(block_size_wide[third_pass_bsize],
                           block_size_high[third_pass_bsize]);
      if (block_size_wide[bsize] < minSize / 4) {
        // Current partition is too small, just terminate
        part_state->terminate_partition_search = 1;
        return;
      } else if (block_size_wide[bsize] < minSize / 2) {
        if (third_pass_part != PARTITION_NONE) {
          // Current partition is very small, and in second pass we used
          // rectangular partition. Terminate the search here then.
          part_state->terminate_partition_search = 1;
          return;
        } else {
          // Partition is small, but we still check this partition, only disable
          // further splits.
          // TODO(any): check why this is not covered by the termination for <
          // minSize/4.
          av1_disable_square_split_partition(part_state);
          av1_disable_rect_partitions(part_state);
          return;
        }
      } else if (block_size_wide[bsize] > maxSize) {
        // Partition is larger than in the second pass. Only allow split.
        av1_set_square_split_only(part_state);
        return;
      } else if (block_size_wide[bsize] >= minSize &&
                 block_size_wide[bsize] <= maxSize) {
        // Partition is within a range where it is very likely to find a good
        // choice, so do not prune anything.
        return;
      }
    }
  }

  // Prune rectangular partitions for larger blocks.
  if (bsize > cpi->sf.part_sf.rect_partition_eval_thresh) {
    part_state->do_rectangular_split = 0;
    part_state->partition_rect_allowed[HORZ] = 0;
    part_state->partition_rect_allowed[VERT] = 0;
  }

  // Prune rectangular, AB and 4-way partition based on q index and block size
  if (cpi->sf.part_sf.prune_rectangular_split_based_on_qidx == 1) {
    if (bsize == BLOCK_8X8 && x->qindex < 35)
      av1_disable_rect_partitions(part_state);

  } else if (cpi->sf.part_sf.prune_rectangular_split_based_on_qidx == 2) {
    // Enumeration difference between two square partitions
    const int sqr_bsize_step = BLOCK_32X32 - BLOCK_16X16;
    int max_bsize =
        BLOCK_32X32 - (x->qindex * 3 / QINDEX_RANGE) * sqr_bsize_step;
    max_bsize = AOMMAX(max_bsize, BLOCK_4X4);
    const BLOCK_SIZE max_prune_bsize =
        (BLOCK_SIZE)AOMMIN(max_bsize, BLOCK_32X32);

    // Prune partition
    // qidx 0 to 85: prune bsize below BLOCK_32X32
    // qidx 86 to 170: prune bsize below BLOCK_16X16
    // qidx 171 to 255: prune bsize below BLOCK_8X8
    if (bsize < max_prune_bsize) {
      av1_disable_rect_partitions(part_state);
    }
  }

  if (cpi->sf.part_sf.prune_sub_8x8_partition_level && (bsize == BLOCK_8X8)) {
    const MACROBLOCKD *const xd = &x->e_mbd;
    int prune_sub_8x8;
    if (cpi->sf.part_sf.prune_sub_8x8_partition_level == 2) {
      prune_sub_8x8 = 1;
    } else {
      assert(cpi->sf.part_sf.prune_sub_8x8_partition_level == 1);
      // Prune if both neighbors are available and either is > BLOCK_8X8
      prune_sub_8x8 = xd->left_available && xd->up_available &&
                      (xd->left_mbmi->bsize > BLOCK_8X8 ||
                       xd->above_mbmi->bsize > BLOCK_8X8);
    }
    if (prune_sub_8x8) {
      av1_disable_all_splits(part_state);
    }
  }

  // A CNN-based speed feature pruning out either split or all non-split
  // partition in INTRA frame coding.
  const int try_intra_cnn_based_part_prune =
      frame_is_intra_only(cm) &&
      cpi->sf.part_sf.intra_cnn_based_part_prune_level &&
      cm->seq_params->sb_size >= BLOCK_64X64 && bsize <= BLOCK_64X64 &&
      blk_params->bsize_at_least_8x8 &&
      av1_is_whole_blk_in_frame(blk_params, mi_params);

  if (try_intra_cnn_based_part_prune) {
    av1_intra_mode_cnn_partition(
        &cpi->common, x, x->part_search_info.quad_tree_idx,
        cpi->sf.part_sf.intra_cnn_based_part_prune_level, part_state);
  }

  // Use simple motion search to prune out split or non-split partitions. This
  // must be done prior to PARTITION_SPLIT to propagate the initial mvs to a
  // smaller blocksize.
  const int try_split_only =
      cpi->sf.part_sf.simple_motion_search_split &&
      part_state->do_square_split && blk_params->bsize_at_least_8x8 &&
      av1_is_whole_blk_in_frame(blk_params, mi_params) &&
      !frame_is_intra_only(cm) && !av1_superres_scaled(cm);

  if (try_split_only) {
    av1_simple_motion_search_based_split(cpi, x, sms_tree, part_state);
  }

  // Use simple motion search to prune out rectangular partition in some
  // direction. The results are stored in prune_horz and prune_vert in order to
  // bypass future related pruning checks if a pruning decision has been made.

  // We want to search at least one partition mode, so don't prune if NONE and
  // SPLIT are disabled.
  const int non_rect_part_allowed =
      part_state->do_square_split || part_state->partition_none_allowed;
  // Only run the model if the partitions are not already pruned.
  const int rect_part_allowed = part_state->do_rectangular_split &&
                                ((part_state->partition_rect_allowed[HORZ] &&
                                  !part_state->prune_rect_part[HORZ]) ||
                                 (part_state->partition_rect_allowed[VERT] &&
                                  !part_state->prune_rect_part[VERT]));

  const int try_prune_rect = cpi->sf.part_sf.simple_motion_search_prune_rect &&
                             !frame_is_intra_only(cm) &&
                             non_rect_part_allowed && rect_part_allowed &&
                             !av1_superres_scaled(cm);

  if (try_prune_rect) {
    av1_simple_motion_search_prune_rect(cpi, x, sms_tree, part_state);
  }
}

#ifndef NDEBUG
static AOM_INLINE int is_bsize_square(BLOCK_SIZE bsize) {
  return block_size_wide[bsize] == block_size_high[bsize];
}
#endif  // NDEBUG

void av1_prune_partitions_by_max_min_bsize(SuperBlockEnc *sb_enc,
                                           PartitionSearchState *part_state) {
  assert(is_bsize_square(sb_enc->max_partition_size));
  assert(is_bsize_square(sb_enc->min_partition_size));
  assert(sb_enc->min_partition_size <= sb_enc->max_partition_size);
  const PartitionBlkParams *blk_params = &part_state->part_blk_params;
  const BLOCK_SIZE bsize = blk_params->bsize;
  assert(is_bsize_square(bsize));
  const int max_partition_size_1d = block_size_wide[sb_enc->max_partition_size];
  const int min_partition_size_1d = block_size_wide[sb_enc->min_partition_size];
  const int bsize_1d = block_size_wide[bsize];
  assert(min_partition_size_1d <= max_partition_size_1d);
  const int is_le_min_sq_part = bsize_1d <= min_partition_size_1d;
  const int is_gt_max_sq_part = bsize_1d > max_partition_size_1d;
  if (is_gt_max_sq_part) {
    // If current block size is larger than max, only allow split.
    av1_set_square_split_only(part_state);
  } else if (is_le_min_sq_part) {
    // If current block size is less or equal to min, only allow none if valid
    // block large enough; only allow split otherwise.
    av1_disable_rect_partitions(part_state);

    // only disable square split when current block is not at the picture
    // boundary. otherwise, inherit the square split flag from previous logic
    if (av1_blk_has_rows_and_cols(blk_params)) {
      part_state->do_square_split = 0;
    }
    part_state->partition_none_allowed = !(part_state->do_square_split);
  }
}

// Decide whether to evaluate the AB partition specified by part_type based on
// split and HORZ/VERT info
static int evaluate_ab_partition_based_on_split(
    const PC_TREE *pc_tree, PARTITION_TYPE rect_part,
    const RD_RECT_PART_WIN_INFO *rect_part_win_info, int qindex, int split_idx1,
    int split_idx2) {
  int num_win = 0;
  // Threshold for number of winners
  // Conservative pruning for high quantizers
  const int num_win_thresh = AOMMIN(3 * (2 * (MAXQ - qindex) / MAXQ), 3);
  int sub_part_win =
      (rect_part_win_info == NULL)    ? (pc_tree->partitioning == rect_part)
      : (rect_part == PARTITION_HORZ) ? rect_part_win_info->rect_part_win[HORZ]
                                      : rect_part_win_info->rect_part_win[VERT];
  num_win += (sub_part_win) ? 1 : 0;
  if (pc_tree->split[split_idx1]) {
    num_win +=
        (pc_tree->split[split_idx1]->partitioning == PARTITION_NONE) ? 1 : 0;
  } else {
    num_win += 1;
  }
  if (pc_tree->split[split_idx2]) {
    num_win +=
        (pc_tree->split[split_idx2]->partitioning == PARTITION_NONE) ? 1 : 0;
  } else {
    num_win += 1;
  }
  if (num_win < num_win_thresh) {
    return 0;
  }
  return 1;
}

void av1_prune_ab_partitions(AV1_COMP *cpi, const MACROBLOCK *x,
                             const PC_TREE *pc_tree, int pb_source_variance,
                             int64_t best_rdcost,
                             const RD_RECT_PART_WIN_INFO *rect_part_win_info,
                             bool ext_partition_allowed,
                             PartitionSearchState *part_state,
                             int *ab_partitions_allowed) {
  int64_t *horz_rd = part_state->rect_part_rd[HORZ];
  int64_t *vert_rd = part_state->rect_part_rd[VERT];
  int64_t *split_rd = part_state->split_rd;
  const PartitionCfg *const part_cfg = &cpi->oxcf.part_cfg;
  // The standard AB partitions are allowed initially if ext-partition-types are
  // allowed.
  int horzab_partition_allowed = ext_partition_allowed &&
                                 part_cfg->enable_ab_partitions &&
                                 part_state->partition_rect_allowed[HORZ];
  int vertab_partition_allowed = ext_partition_allowed &&
                                 part_cfg->enable_ab_partitions &&
                                 part_state->partition_rect_allowed[VERT];

  // Pruning: pruning out AB partitions on one main direction based on the
  // current best partition and source variance.
  if (cpi->sf.part_sf.prune_ext_partition_types_search_level) {
    if (cpi->sf.part_sf.prune_ext_partition_types_search_level == 1) {
      // TODO(debargha,huisu@google.com): may need to tune the threshold for
      // pb_source_variance.
      horzab_partition_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                   (pc_tree->partitioning == PARTITION_NONE &&
                                    pb_source_variance < 32) ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
      vertab_partition_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                   (pc_tree->partitioning == PARTITION_NONE &&
                                    pb_source_variance < 32) ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
    } else {
      horzab_partition_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
      vertab_partition_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
    }
    horz_rd[0] = (horz_rd[0] < INT64_MAX ? horz_rd[0] : 0);
    horz_rd[1] = (horz_rd[1] < INT64_MAX ? horz_rd[1] : 0);
    vert_rd[0] = (vert_rd[0] < INT64_MAX ? vert_rd[0] : 0);
    vert_rd[1] = (vert_rd[1] < INT64_MAX ? vert_rd[1] : 0);
    split_rd[0] = (split_rd[0] < INT64_MAX ? split_rd[0] : 0);
    split_rd[1] = (split_rd[1] < INT64_MAX ? split_rd[1] : 0);
    split_rd[2] = (split_rd[2] < INT64_MAX ? split_rd[2] : 0);
    split_rd[3] = (split_rd[3] < INT64_MAX ? split_rd[3] : 0);
  }

  // Pruning: pruning out horz_a or horz_b if the combined rdcost of its
  // subblocks estimated from previous partitions is much higher than the best
  // rd so far.
  ab_partitions_allowed[HORZ_A] = horzab_partition_allowed;
  ab_partitions_allowed[HORZ_B] = horzab_partition_allowed;
  if (cpi->sf.part_sf.prune_ext_partition_types_search_level) {
    const int64_t horz_a_rd = horz_rd[1] + split_rd[0] + split_rd[1];
    const int64_t horz_b_rd = horz_rd[0] + split_rd[2] + split_rd[3];
    switch (cpi->sf.part_sf.prune_ext_partition_types_search_level) {
      case 1:
        ab_partitions_allowed[HORZ_A] &= (horz_a_rd / 16 * 14 < best_rdcost);
        ab_partitions_allowed[HORZ_B] &= (horz_b_rd / 16 * 14 < best_rdcost);
        break;
      case 2:
      default:
        ab_partitions_allowed[HORZ_A] &= (horz_a_rd / 16 * 15 < best_rdcost);
        ab_partitions_allowed[HORZ_B] &= (horz_b_rd / 16 * 15 < best_rdcost);
        break;
    }
  }

  // Pruning: pruning out vert_a or vert_b if the combined rdcost of its
  // subblocks estimated from previous partitions is much higher than the best
  // rd so far.
  ab_partitions_allowed[VERT_A] = vertab_partition_allowed;
  ab_partitions_allowed[VERT_B] = vertab_partition_allowed;
  if (cpi->sf.part_sf.prune_ext_partition_types_search_level) {
    const int64_t vert_a_rd = vert_rd[1] + split_rd[0] + split_rd[2];
    const int64_t vert_b_rd = vert_rd[0] + split_rd[1] + split_rd[3];
    switch (cpi->sf.part_sf.prune_ext_partition_types_search_level) {
      case 1:
        ab_partitions_allowed[VERT_A] &= (vert_a_rd / 16 * 14 < best_rdcost);
        ab_partitions_allowed[VERT_B] &= (vert_b_rd / 16 * 14 < best_rdcost);
        break;
      case 2:
      default:
        ab_partitions_allowed[VERT_A] &= (vert_a_rd / 16 * 15 < best_rdcost);
        ab_partitions_allowed[VERT_B] &= (vert_b_rd / 16 * 15 < best_rdcost);
        break;
    }
  }

  // Pruning: pruning out some ab partitions using a DNN taking rd costs of
  // sub-blocks from previous basic partition types.
  if (cpi->sf.part_sf.ml_prune_partition && ext_partition_allowed &&
      part_state->partition_rect_allowed[HORZ] &&
      part_state->partition_rect_allowed[VERT]) {
    // TODO(huisu@google.com): x->source_variance may not be the current
    // block's variance. The correct one to use is pb_source_variance. Need to
    // re-train the model to fix it.
    av1_ml_prune_ab_partition(cpi, pc_tree->partitioning,
                              get_unsigned_bits(x->source_variance),
                              best_rdcost, part_state, ab_partitions_allowed);
  }

  // Pruning: pruning AB partitions based on the number of horz/vert wins
  // in the current block and sub-blocks in PARTITION_SPLIT.
  if (cpi->sf.part_sf.prune_ext_part_using_split_info >= 2 &&
      ab_partitions_allowed[HORZ_A]) {
    ab_partitions_allowed[HORZ_A] &= evaluate_ab_partition_based_on_split(
        pc_tree, PARTITION_HORZ, rect_part_win_info, x->qindex, 0, 1);
  }
  if (cpi->sf.part_sf.prune_ext_part_using_split_info >= 2 &&
      ab_partitions_allowed[HORZ_B]) {
    ab_partitions_allowed[HORZ_B] &= evaluate_ab_partition_based_on_split(
        pc_tree, PARTITION_HORZ, rect_part_win_info, x->qindex, 2, 3);
  }
  if (cpi->sf.part_sf.prune_ext_part_using_split_info >= 2 &&
      ab_partitions_allowed[VERT_A]) {
    ab_partitions_allowed[VERT_A] &= evaluate_ab_partition_based_on_split(
        pc_tree, PARTITION_VERT, rect_part_win_info, x->qindex, 0, 2);
  }
  if (cpi->sf.part_sf.prune_ext_part_using_split_info >= 2 &&
      ab_partitions_allowed[VERT_B]) {
    ab_partitions_allowed[VERT_B] &= evaluate_ab_partition_based_on_split(
        pc_tree, PARTITION_VERT, rect_part_win_info, x->qindex, 1, 3);
  }
}

// Prepare features for the external model. Specifically, features after
// ab partition is searched.
static void prepare_features_after_part_ab(
    const AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize,
    int part_ctx, int64_t best_rd,
    int64_t rect_part_rd[NUM_RECT_PARTS][SUB_PARTITIONS_RECT],
    int64_t split_rd[SUB_PARTITIONS_SPLIT], unsigned int pb_source_variance,
    int mi_row, int mi_col, aom_partition_features_t *const features) {
  int64_t *horz_rd = rect_part_rd[HORZ];
  int64_t *vert_rd = rect_part_rd[VERT];

  // Generate features.
  int feature_index = 0;
  features->after_part_ab.f[feature_index++] = (float)part_ctx;
  features->after_part_ab.f[feature_index++] =
      (float)get_unsigned_bits(pb_source_variance);

  const int rdcost = (int)AOMMIN(INT_MAX, best_rd);
  int sub_block_rdcost[8] = { 0 };
  int rd_index = 0;
  for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
    if (horz_rd[i] > 0 && horz_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)horz_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < SUB_PARTITIONS_RECT; ++i) {
    if (vert_rd[i] > 0 && vert_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)vert_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < SUB_PARTITIONS_SPLIT; ++i) {
    if (split_rd[i] > 0 && split_rd[i] < 1000000000)
      sub_block_rdcost[rd_index] = (int)split_rd[i];
    ++rd_index;
  }
  for (int i = 0; i < 8; ++i) {
    // Ratio between the sub-block RD and the whole-block RD.
    float rd_ratio = 1.0f;
    if (sub_block_rdcost[i] > 0 && sub_block_rdcost[i] < rdcost)
      rd_ratio = (float)sub_block_rdcost[i] / (float)rdcost;
    features->after_part_ab.f[feature_index++] = rd_ratio;
  }

  // 4-way partitions are only allowed for these three square block sizes.
  assert(bsize == BLOCK_16X16 || bsize == BLOCK_32X32 || bsize == BLOCK_64X64);

  // Get variance of the 1:4 and 4:1 sub-blocks.
  unsigned int horz_4_source_var[SUB_PARTITIONS_PART4] = { 0 };
  unsigned int vert_4_source_var[SUB_PARTITIONS_PART4] = { 0 };
  {
    BLOCK_SIZE horz_4_bs = get_partition_subsize(bsize, PARTITION_HORZ_4);
    BLOCK_SIZE vert_4_bs = get_partition_subsize(bsize, PARTITION_VERT_4);

    assert(horz_4_bs != BLOCK_INVALID);
    assert(vert_4_bs != BLOCK_INVALID);

    av1_setup_src_planes(x, cpi->source, mi_row, mi_col,
                         av1_num_planes(&cpi->common), bsize);
    const int src_stride = x->plane[0].src.stride;
    uint8_t *src = x->plane[0].src.buf;
    const MACROBLOCKD *const xd = &x->e_mbd;

    struct buf_2d horz_4_src, vert_4_src;
    horz_4_src.stride = src_stride;
    vert_4_src.stride = src_stride;

    for (int i = 0; i < SUB_PARTITIONS_PART4; ++i) {
      horz_4_src.buf = src + i * block_size_high[horz_4_bs] * src_stride;
      vert_4_src.buf = src + i * block_size_wide[vert_4_bs];

      horz_4_source_var[i] = av1_get_perpixel_variance_facade(
          cpi, xd, &horz_4_src, horz_4_bs, AOM_PLANE_Y);
      vert_4_source_var[i] = av1_get_perpixel_variance_facade(
          cpi, xd, &vert_4_src, vert_4_bs, AOM_PLANE_Y);
    }
  }

  const float denom = (float)(pb_source_variance + 1);
  const float low_b = 0.1f;
  const float high_b = 10.0f;
  for (int i = 0; i < SUB_PARTITIONS_PART4; ++i) {
    // Ratio between the 4:1 sub-block variance and the whole-block variance.
    float var_ratio = (float)(horz_4_source_var[i] + 1) / denom;
    if (var_ratio < low_b) var_ratio = low_b;
    if (var_ratio > high_b) var_ratio = high_b;
    features->after_part_ab.f[feature_index++] = var_ratio;
  }
  for (int i = 0; i < SUB_PARTITIONS_PART4; ++i) {
    // Ratio between the 1:4 sub-block RD and the whole-block RD.
    float var_ratio = (float)(vert_4_source_var[i] + 1) / denom;
    if (var_ratio < low_b) var_ratio = low_b;
    if (var_ratio > high_b) var_ratio = high_b;
    features->after_part_ab.f[feature_index++] = var_ratio;
  }
  assert(feature_index == 18);
}

// If the external partition model is used, we let it determine partition
// decisions before partition none. Specifically, these parameters:
// partition_none_allowed
// partition_horz_allowed
// partition_vert_allowed
// do_rectangular_split
// do_square_split
static bool ext_ml_model_decision_before_none(
    AV1_COMP *cpi, const float features_from_motion[FEATURE_SIZE_SMS_SPLIT],
    int *partition_none_allowed, int *partition_horz_allowed,
    int *partition_vert_allowed, int *do_rectangular_split,
    int *do_square_split) {
  ExtPartController *const ext_part_controller = &cpi->ext_part_controller;
  if (!ext_part_controller->ready) return false;

  // Setup features.
  aom_partition_features_t features;
  features.id = AOM_EXT_PART_FEATURE_BEFORE_NONE;
  for (int i = 0; i < FEATURE_SIZE_SMS_SPLIT; ++i) {
    features.before_part_none.f[i] = features_from_motion[i];
  }

  // Send necessary features to the external model.
  av1_ext_part_send_features(ext_part_controller, &features);

  // Get partition decisions from the external model.
  aom_partition_decision_t decision;
  const bool valid_decision =
      av1_ext_part_get_partition_decision(ext_part_controller, &decision);
  if (!valid_decision) return false;

  // Populate decisions
  *partition_none_allowed = decision.partition_none_allowed;
  *partition_horz_allowed = decision.partition_rect_allowed[HORZ];
  *partition_vert_allowed = decision.partition_rect_allowed[VERT];
  *do_rectangular_split = decision.do_rectangular_split;
  *do_square_split = decision.do_square_split;

  return true;
}

// If the external partition model is used, we let it determine partition
// decisions before partition none. Specifically, these parameters:
// prune_horz
// prune_vert
static bool ext_ml_model_decision_before_none_part2(
    AV1_COMP *cpi,
    const float features_from_motion[FEATURE_SIZE_SMS_PRUNE_PART],
    int *prune_horz, int *prune_vert) {
  ExtPartController *const ext_part_controller = &cpi->ext_part_controller;
  if (!ext_part_controller->ready) return false;

  // Setup features.
  aom_partition_features_t features;
  features.id = AOM_EXT_PART_FEATURE_BEFORE_NONE_PART2;
  for (int i = 0; i < FEATURE_SIZE_SMS_PRUNE_PART; ++i) {
    features.before_part_none.f_part2[i] = features_from_motion[i];
  }

  // Send necessary features to the external model.
  av1_ext_part_send_features(ext_part_controller, &features);

  // Get partition decisions from the external model.
  aom_partition_decision_t decision;
  const bool valid_decision =
      av1_ext_part_get_partition_decision(ext_part_controller, &decision);
  if (!valid_decision) return false;

  // Populate decisions
  *prune_horz = decision.prune_rect_part[HORZ];
  *prune_vert = decision.prune_rect_part[VERT];

  return true;
}

// If the external partition model is used, we let it determine partition
// decisions after none partition. Specifically, these parameters:
// do_square_split
// do_rectangular_split
bool ext_ml_model_decision_after_none(
    ExtPartController *const ext_part_controller, const int is_intra_frame,
    const float *const features_after_none, int *do_square_split,
    int *do_rectangular_split) {
  if (!ext_part_controller->ready || is_intra_frame) return false;

  // Setup features.
  aom_partition_features_t features;
  features.id = AOM_EXT_PART_FEATURE_AFTER_NONE;
  for (int i = 0; i < 4; ++i) {
    features.after_part_none.f[i] = features_after_none[i];
  }

  // Send necessary features to the external model.
  av1_ext_part_send_features(ext_part_controller, &features);

  // Get partition decisions from the external model.
  aom_partition_decision_t decision;
  const bool valid_decision =
      av1_ext_part_get_partition_decision(ext_part_controller, &decision);
  if (!valid_decision) return false;

  // Populate decisions
  *do_square_split = decision.do_square_split;
  *do_rectangular_split = decision.do_rectangular_split;

  return true;
}

// If the external partition model is used, we let it determine partition
// decisions after none partition. Specifically, these parameters:
// terminate_partition_search
bool ext_ml_model_decision_after_none_part2(
    AV1_COMP *const cpi, const float *const features_terminate,
    int *terminate_partition_search) {
  AV1_COMMON *const cm = &cpi->common;
  ExtPartController *const ext_part_controller = &cpi->ext_part_controller;
  if (!ext_part_controller->ready || frame_is_intra_only(cm)) return false;

  // Setup features.
  aom_partition_features_t features;
  features.id = AOM_EXT_PART_FEATURE_AFTER_NONE_PART2;
  for (int i = 0; i < FEATURE_SIZE_SMS_TERM_NONE; ++i) {
    features.after_part_none.f_terminate[i] = features_terminate[i];
  }

  // Send necessary features to the external model.
  av1_ext_part_send_features(ext_part_controller, &features);

  // Get partition decisions from the external model.
  aom_partition_decision_t decision;
  const bool valid_decision =
      av1_ext_part_get_partition_decision(ext_part_controller, &decision);
  if (!valid_decision) return false;

  // Populate decisions
  *terminate_partition_search = decision.terminate_partition_search;

  return true;
}

// If the external partition model is used, we let it determine partition
// decisions after none partition. Specifically, these parameters:
// terminate_partition_search
bool ext_ml_model_decision_after_split(AV1_COMP *const cpi,
                                       const float *const features_terminate,
                                       int *terminate_partition_search) {
  const AV1_COMMON *const cm = &cpi->common;
  ExtPartController *const ext_part_controller = &cpi->ext_part_controller;
  if (frame_is_intra_only(cm) || !cpi->ext_part_controller.ready) {
    return false;
  }

  // Setup features.
  aom_partition_features_t features;
  features.id = AOM_EXT_PART_FEATURE_AFTER_SPLIT;
  for (int i = 0; i < 31; ++i) {
    features.after_part_split.f_terminate[i] = features_terminate[i];
  }

  // Send necessary features to the external model.
  av1_ext_part_send_features(ext_part_controller, &features);

  // Get partition decisions from the external model.
  aom_partition_decision_t decision;
  const bool valid_decision =
      av1_ext_part_get_partition_decision(ext_part_controller, &decision);
  if (!valid_decision) return false;

  // Populate decisions
  *terminate_partition_search = decision.terminate_partition_search;

  return true;
}

// If the external partition model is used, we let it determine partition
// decisions after none partition. Specifically, these parameters:
// prune_rect_part[HORZ]
// prune_rect_part[VERT]
bool ext_ml_model_decision_after_split_part2(
    ExtPartController *const ext_part_controller, const int is_intra_frame,
    const float *const features_prune, int *prune_rect_part_horz,
    int *prune_rect_part_vert) {
  if (is_intra_frame || !ext_part_controller->ready) {
    return false;
  }

  // Setup features.
  aom_partition_features_t features;
  features.id = AOM_EXT_PART_FEATURE_AFTER_SPLIT_PART2;
  for (int i = 0; i < 9; ++i) {
    features.after_part_split.f_prune_rect[i] = features_prune[i];
  }

  // Send necessary features to the external model.
  av1_ext_part_send_features(ext_part_controller, &features);

  // Get partition decisions from the external model.
  aom_partition_decision_t decision;
  const bool valid_decision =
      av1_ext_part_get_partition_decision(ext_part_controller, &decision);
  if (!valid_decision) return false;

  // Populate decisions
  *prune_rect_part_horz = decision.prune_rect_part[0];
  *prune_rect_part_vert = decision.prune_rect_part[1];

  return true;
}

// If the external partition model is used, we let it determine partition
// decisions after rectangular partition. Specifically, these parameters:
// horza_partition_allowed
// horzb_partition_allowed
// verta_partition_allowed
// vertb_partition_allowed
static bool ext_ml_model_decision_after_rect(
    ExtPartController *const ext_part_controller, const int is_intra_frame,
    const float *const features_after_rect, int *horza_partition_allowed,
    int *horzb_partition_allowed, int *verta_partition_allowed,
    int *vertb_partition_allowed) {
  if (is_intra_frame || !ext_part_controller->ready) return false;

  // Setup features.
  aom_partition_features_t features;
  features.id = AOM_EXT_PART_FEATURE_AFTER_RECT;
  for (int i = 0; i < 10; ++i) {
    features.after_part_rect.f[i] = features_after_rect[i];
  }

  // Send necessary features to the external model.
  av1_ext_part_send_features(ext_part_controller, &features);

  // Get partition decisions from the external model.
  aom_partition_decision_t decision;
  const bool valid_decision =
      av1_ext_part_get_partition_decision(ext_part_controller, &decision);
  if (!valid_decision) return false;

  // Populate decisions
  *horza_partition_allowed = decision.horza_partition_allowed;
  *horzb_partition_allowed = decision.horzb_partition_allowed;
  *verta_partition_allowed = decision.verta_partition_allowed;
  *vertb_partition_allowed = decision.vertb_partition_allowed;

  return true;
}

// If the external partition model is used, we let it determine partition
// decisions after AB partition. Specifically, these parameters:
// partition_vert4_allowed
// partition_horz4_allowed
static bool ext_ml_model_decision_after_part_ab(
    AV1_COMP *const cpi, MACROBLOCK *const x, BLOCK_SIZE bsize, int part_ctx,
    int64_t best_rd, int64_t rect_part_rd[NUM_RECT_PARTS][SUB_PARTITIONS_RECT],
    int64_t split_rd[SUB_PARTITIONS_SPLIT], int *const partition_horz4_allowed,
    int *const partition_vert4_allowed, unsigned int pb_source_variance,
    int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  ExtPartController *const ext_part_controller = &cpi->ext_part_controller;

  if (!frame_is_intra_only(cm) && ext_part_controller->ready) {
    // Setup features.
    aom_partition_features_t features;
    features.id = AOM_EXT_PART_FEATURE_AFTER_AB;
    prepare_features_after_part_ab(cpi, x, bsize, part_ctx, best_rd,
                                   rect_part_rd, split_rd, pb_source_variance,
                                   mi_row, mi_col, &features);

    // Send necessary features to the external model.
    av1_ext_part_send_features(ext_part_controller, &features);

    // Get partition decisions from the external model.
    aom_partition_decision_t decision;
    const bool valid_decision =
        av1_ext_part_get_partition_decision(ext_part_controller, &decision);
    if (!valid_decision) return false;

    // Populate decisions
    *partition_horz4_allowed = decision.partition_horz4_allowed;
    *partition_vert4_allowed = decision.partition_vert4_allowed;

    return true;
  }

  return false;
}

// This function resembles "av1_setup_sms_tree()" in context_tree.c
// with function signature change.
static SIMPLE_MOTION_DATA_TREE *setup_sms_tree(
    AV1_COMP *const cpi, SIMPLE_MOTION_DATA_TREE *sms_tree) {
  AV1_COMMON *const cm = &cpi->common;
  const int stat_generation_stage = is_stat_generation_stage(cpi);
  const int is_sb_size_128 = cm->seq_params->sb_size == BLOCK_128X128;
  const int tree_nodes =
      av1_get_pc_tree_nodes(is_sb_size_128, stat_generation_stage);
  int sms_tree_index = 0;
  SIMPLE_MOTION_DATA_TREE *this_sms;
  int square_index = 1;
  int nodes;
  this_sms = &sms_tree[0];

  if (!stat_generation_stage) {
    const int leaf_factor = is_sb_size_128 ? 4 : 1;
    const int leaf_nodes = 256 * leaf_factor;

    // Sets up all the leaf nodes in the tree.
    for (sms_tree_index = 0; sms_tree_index < leaf_nodes; ++sms_tree_index) {
      SIMPLE_MOTION_DATA_TREE *const tree = &sms_tree[sms_tree_index];
      tree->block_size = square[0];
    }

    // Each node has 4 leaf nodes, fill each block_size level of the tree
    // from leafs to the root.
    for (nodes = leaf_nodes >> 2; nodes > 0; nodes >>= 2) {
      for (int i = 0; i < nodes; ++i) {
        SIMPLE_MOTION_DATA_TREE *const tree = &sms_tree[sms_tree_index];
        tree->block_size = square[square_index];
        for (int j = 0; j < 4; j++) tree->split[j] = this_sms++;
        ++sms_tree_index;
      }
      ++square_index;
    }
  } else {
    // Allocation for firstpass/LAP stage
    // TODO(Mufaddal): refactor square_index to use a common block_size macro
    // from firstpass.c
    SIMPLE_MOTION_DATA_TREE *const tree = &sms_tree[sms_tree_index];
    square_index = 2;
    tree->block_size = square[square_index];
  }

  // Set up the root node for the largest superblock size
  return &sms_tree[tree_nodes - 1];
}

static void write_motion_feature_to_file(
    const char *const path, const int sb_counter, const unsigned int *block_sse,
    const unsigned int *block_var, const int num_blocks, const BLOCK_SIZE bsize,
    const BLOCK_SIZE fixed_block_size, const int mi_row, const int mi_col) {
  char filename[256];
  snprintf(filename, sizeof(filename), "%s/motion_search_feature_sb%d", path,
           sb_counter);
  FILE *pfile = fopen(filename, "w");
  fprintf(pfile, "%d,%d,%d,%d,%d\n", mi_row, mi_col, bsize,
          block_size_wide[fixed_block_size], num_blocks);
  for (int i = 0; i < num_blocks; ++i) {
    fprintf(pfile, "%d", block_sse[i]);
    if (i < num_blocks - 1) fprintf(pfile, ",");
  }
  fprintf(pfile, "\n");
  for (int i = 0; i < num_blocks; ++i) {
    fprintf(pfile, "%d", block_var[i]);
    if (i < num_blocks - 1) fprintf(pfile, ",");
  }
  fprintf(pfile, "\n");
  fclose(pfile);
}

void av1_collect_motion_search_features_sb(AV1_COMP *const cpi, ThreadData *td,
                                           TileDataEnc *tile_data,
                                           const int mi_row, const int mi_col,
                                           const BLOCK_SIZE bsize,
                                           aom_partition_features_t *features) {
  const AV1_COMMON *const cm = &cpi->common;
  if (frame_is_intra_only(cm)) return;

  MACROBLOCK *const x = &td->mb;
  const BLOCK_SIZE fixed_block_size = BLOCK_16X16;
  const int col_step = mi_size_wide[fixed_block_size];
  const int row_step = mi_size_high[fixed_block_size];
  SIMPLE_MOTION_DATA_TREE *sms_tree = NULL;
  const int stat_generation_stage = is_stat_generation_stage(cpi);
  const int is_sb_size_128 = cm->seq_params->sb_size == BLOCK_128X128;
  const int tree_nodes =
      av1_get_pc_tree_nodes(is_sb_size_128, stat_generation_stage);
  CHECK_MEM_ERROR(cm, sms_tree, aom_calloc(tree_nodes, sizeof(*sms_tree)));
  SIMPLE_MOTION_DATA_TREE *sms_root = setup_sms_tree(cpi, sms_tree);
  TileInfo *const tile_info = &tile_data->tile_info;
  av1_set_offsets_without_segment_id(cpi, tile_info, x, mi_row, mi_col, bsize);
  av1_init_simple_motion_search_mvs_for_sb(cpi, NULL, x, sms_root, mi_row,
                                           mi_col);
  av1_reset_simple_motion_tree_partition(sms_root, bsize);
  const int ref_list[] = { cpi->rc.is_src_frame_alt_ref ? ALTREF_FRAME
                                                        : LAST_FRAME };
  const int mi_width =
      AOMMIN(mi_size_wide[bsize], cm->mi_params.mi_cols - mi_col);
  const int mi_height =
      AOMMIN(mi_size_high[bsize], cm->mi_params.mi_rows - mi_row);
  const int col_steps = (mi_width / col_step) + ((mi_width % col_step) > 0);
  const int row_steps = (mi_height / row_step) + ((mi_height % row_step) > 0);
  const int num_blocks = col_steps * row_steps;
  unsigned int *block_sse = aom_calloc(num_blocks, sizeof(*block_sse));
  unsigned int *block_var = aom_calloc(num_blocks, sizeof(*block_var));
  if (!(block_sse && block_var)) {
    aom_free(sms_tree);
    aom_free(block_sse);
    aom_free(block_var);
    aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                       "Error allocating block_sse & block_var");
  }
  int idx = 0;

  for (int row = mi_row;
       row < AOMMIN(mi_row + mi_size_high[bsize], cm->mi_params.mi_rows);
       row += row_step) {
    for (int col = mi_col;
         col < AOMMIN(mi_col + mi_size_wide[bsize], cm->mi_params.mi_cols);
         col += col_step) {
      simple_motion_search_get_best_ref(
          cpi, x, sms_root, row, col, fixed_block_size, ref_list,
          /*num_refs=*/1, /*use_subpixel=*/1,
          /*save_mv=*/1, &block_sse[idx], &block_var[idx]);
      ++idx;
    }
  }
  if (features == NULL) {
    write_motion_feature_to_file(cpi->oxcf.partition_info_path, cpi->sb_counter,
                                 block_sse, block_var, idx, bsize,
                                 fixed_block_size, mi_row, mi_col);
  } else {
    features->sb_features.motion_features.unit_length =
        block_size_wide[fixed_block_size];
    features->sb_features.motion_features.num_units = idx;
    for (int i = 0; i < idx; ++i) {
      features->sb_features.motion_features.block_sse[i] = block_sse[i];
      features->sb_features.motion_features.block_var[i] = block_var[i];
    }
  }

  aom_free(block_sse);
  aom_free(block_var);
  aom_free(sms_tree);
}

void av1_prepare_motion_search_features_block(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    const int mi_row, const int mi_col, const BLOCK_SIZE bsize,
    const int valid_partition_types, unsigned int *block_sse,
    unsigned int *block_var, unsigned int sub_block_sse[4],
    unsigned int sub_block_var[4], unsigned int horz_block_sse[2],
    unsigned int horz_block_var[2], unsigned int vert_block_sse[2],
    unsigned int vert_block_var[2]) {
  const AV1_COMMON *const cm = &cpi->common;
  if (frame_is_intra_only(cm)) return;
  MACROBLOCK *const x = &td->mb;
  SIMPLE_MOTION_DATA_TREE *sms_tree = NULL;
  const int stat_generation_stage = is_stat_generation_stage(cpi);
  const int is_sb_size_128 = cm->seq_params->sb_size == BLOCK_128X128;
  const int tree_nodes =
      av1_get_pc_tree_nodes(is_sb_size_128, stat_generation_stage);
  CHECK_MEM_ERROR(cm, sms_tree, aom_calloc(tree_nodes, sizeof(*sms_tree)));
  SIMPLE_MOTION_DATA_TREE *sms_root = setup_sms_tree(cpi, sms_tree);
  TileInfo *const tile_info = &tile_data->tile_info;
  av1_set_offsets_without_segment_id(cpi, tile_info, x, mi_row, mi_col, bsize);
  av1_reset_simple_motion_tree_partition(sms_root, bsize);
  const int ref_list[] = { cpi->rc.is_src_frame_alt_ref ? ALTREF_FRAME
                                                        : LAST_FRAME };
  const int sub_mi_width = mi_size_wide[bsize] / 2;
  const int sub_mi_height = sub_mi_width;
  simple_motion_search_get_best_ref(
      cpi, x, sms_root, mi_row, mi_col, bsize, ref_list, /*num_refs=*/1,
      /*use_subpixel=*/1, /*save_mv=*/1, block_sse, block_var);
  // Split to 4 sub blocks.
  if (valid_partition_types & (1 << PARTITION_SPLIT)) {
    const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    for (int i = 0; i < 4; ++i) {
      const int row = mi_row + (i >> 1) * sub_mi_height;
      const int col = mi_col + (i & 1) * sub_mi_width;
      simple_motion_search_get_best_ref(cpi, x, sms_root, row, col, subsize,
                                        ref_list, /*num_refs=*/1,
                                        /*use_subpixel=*/1, /*save_mv=*/1,
                                        &sub_block_sse[i], &sub_block_var[i]);
    }
  }
  // Horizontal split
  if (valid_partition_types & (1 << PARTITION_HORZ)) {
    const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_HORZ);
    for (int i = 0; i < 2; ++i) {
      const int row = mi_row + (i & 1) * sub_mi_height;
      const int col = mi_col;
      simple_motion_search_get_best_ref(cpi, x, sms_root, row, col, subsize,
                                        ref_list, /*num_refs=*/1,
                                        /*use_subpixel=*/1, /*save_mv=*/1,
                                        &horz_block_sse[i], &horz_block_var[i]);
    }
  }
  // Vertical split
  if (valid_partition_types & (1 << PARTITION_VERT)) {
    const BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_VERT);
    for (int i = 0; i < 2; ++i) {
      const int row = mi_row;
      const int col = mi_col + (i & 1) * sub_mi_width;
      simple_motion_search_get_best_ref(cpi, x, sms_root, row, col, subsize,
                                        ref_list, /*num_refs=*/1,
                                        /*use_subpixel=*/1, /*save_mv=*/1,
                                        &vert_block_sse[i], &vert_block_var[i]);
    }
  }

  aom_free(sms_tree);
}
#endif  // !CONFIG_REALTIME_ONLY

static INLINE void init_simple_motion_search_mvs(
    SIMPLE_MOTION_DATA_TREE *sms_tree, const FULLPEL_MV *start_mvs) {
  memcpy(sms_tree->start_mvs, start_mvs, sizeof(sms_tree->start_mvs));
  av1_zero(sms_tree->sms_none_feat);
  av1_zero(sms_tree->sms_rect_feat);
  av1_zero(sms_tree->sms_none_valid);
  av1_zero(sms_tree->sms_rect_valid);

  if (sms_tree->block_size >= BLOCK_8X8) {
    init_simple_motion_search_mvs(sms_tree->split[0], start_mvs);
    init_simple_motion_search_mvs(sms_tree->split[1], start_mvs);
    init_simple_motion_search_mvs(sms_tree->split[2], start_mvs);
    init_simple_motion_search_mvs(sms_tree->split[3], start_mvs);
  }
}

void av1_init_simple_motion_search_mvs_for_sb(const AV1_COMP *cpi,
                                              const TileInfo *tile_info,
                                              MACROBLOCK *x,
                                              SIMPLE_MOTION_DATA_TREE *sms_root,
                                              int mi_row, int mi_col) {
  // Use the NEARESTMV of the sb as the start mv
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  FULLPEL_MV ref_mvs[REF_FRAMES];
  const BLOCK_SIZE sb_size = cm->seq_params->sb_size;
  av1_zero(ref_mvs);
  // If tile_info is NULL, assume that the offsets have already been set.
  if (tile_info) {
    av1_set_offsets_without_segment_id(cpi, tile_info, x, mi_row, mi_col,
                                       sb_size);
  }

  MB_MODE_INFO_EXT mbmi_ext;
  const int ref_frame =
      cpi->rc.is_src_frame_alt_ref ? ALTREF_FRAME : LAST_FRAME;
  av1_find_mv_refs(cm, xd, xd->mi[0], ref_frame, mbmi_ext.ref_mv_count,
                   xd->ref_mv_stack, xd->weight, NULL, mbmi_ext.global_mvs,
                   mbmi_ext.mode_context);
  if (mbmi_ext.ref_mv_count[ref_frame] > 0) {
    ref_mvs[ref_frame] =
        get_fullmv_from_mv(&xd->ref_mv_stack[ref_frame][0].this_mv.as_mv);
  } else {
    ref_mvs[ref_frame] =
        get_fullmv_from_mv(&mbmi_ext.global_mvs[ref_frame].as_mv);
  }

  init_simple_motion_search_mvs(sms_root, ref_mvs);
}
