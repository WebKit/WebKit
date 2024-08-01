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

#include "config/aom_config.h"

#include "av1/encoder/encodemv.h"
#if !CONFIG_REALTIME_ONLY
#include "av1/encoder/misc_model_weights.h"
#endif  // !CONFIG_REALTIME_ONLY
#include "av1/encoder/mv_prec.h"

#if !CONFIG_REALTIME_ONLY
static AOM_INLINE int_mv get_ref_mv_for_mv_stats(
    const MB_MODE_INFO *mbmi, const MB_MODE_INFO_EXT_FRAME *mbmi_ext_frame,
    int ref_idx) {
  int ref_mv_idx = mbmi->ref_mv_idx;
  if (mbmi->mode == NEAR_NEWMV || mbmi->mode == NEW_NEARMV) {
    assert(has_second_ref(mbmi));
    ref_mv_idx += 1;
  }

  const MV_REFERENCE_FRAME *ref_frames = mbmi->ref_frame;
  const int8_t ref_frame_type = av1_ref_frame_type(ref_frames);
  const CANDIDATE_MV *curr_ref_mv_stack = mbmi_ext_frame->ref_mv_stack;

  if (ref_frames[1] > INTRA_FRAME) {
    assert(ref_idx == 0 || ref_idx == 1);
    return ref_idx ? curr_ref_mv_stack[ref_mv_idx].comp_mv
                   : curr_ref_mv_stack[ref_mv_idx].this_mv;
  }

  assert(ref_idx == 0);
  return ref_mv_idx < mbmi_ext_frame->ref_mv_count
             ? curr_ref_mv_stack[ref_mv_idx].this_mv
             : mbmi_ext_frame->global_mvs[ref_frame_type];
}

static AOM_INLINE int get_symbol_cost(const aom_cdf_prob *cdf, int symbol) {
  const aom_cdf_prob cur_cdf = AOM_ICDF(cdf[symbol]);
  const aom_cdf_prob prev_cdf = symbol ? AOM_ICDF(cdf[symbol - 1]) : 0;
  const aom_cdf_prob p15 = AOMMAX(cur_cdf - prev_cdf, EC_MIN_PROB);

  return av1_cost_symbol(p15);
}

static AOM_INLINE int keep_one_comp_stat(MV_STATS *mv_stats, int comp_val,
                                         int comp_idx, const AV1_COMP *cpi,
                                         int *rates) {
  assert(comp_val != 0 && "mv component should not have zero value!");
  const int sign = comp_val < 0;
  const int mag = sign ? -comp_val : comp_val;
  const int mag_minus_1 = mag - 1;
  int offset;
  const int mv_class = av1_get_mv_class(mag_minus_1, &offset);
  const int int_part = offset >> 3;         // int mv data
  const int frac_part = (offset >> 1) & 3;  // fractional mv data
  const int high_part = offset & 1;         // high precision mv data
  const int use_hp = cpi->common.features.allow_high_precision_mv;
  int r_idx = 0;

  const MACROBLOCK *const x = &cpi->td.mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  nmv_context *nmvc = &ec_ctx->nmvc;
  nmv_component *mvcomp_ctx = nmvc->comps;
  nmv_component *cur_mvcomp_ctx = &mvcomp_ctx[comp_idx];
  aom_cdf_prob *sign_cdf = cur_mvcomp_ctx->sign_cdf;
  aom_cdf_prob *class_cdf = cur_mvcomp_ctx->classes_cdf;
  aom_cdf_prob *class0_cdf = cur_mvcomp_ctx->class0_cdf;
  aom_cdf_prob(*bits_cdf)[3] = cur_mvcomp_ctx->bits_cdf;
  aom_cdf_prob *frac_part_cdf = mv_class
                                    ? (cur_mvcomp_ctx->fp_cdf)
                                    : (cur_mvcomp_ctx->class0_fp_cdf[int_part]);
  aom_cdf_prob *high_part_cdf =
      mv_class ? (cur_mvcomp_ctx->hp_cdf) : (cur_mvcomp_ctx->class0_hp_cdf);

  const int sign_rate = get_symbol_cost(sign_cdf, sign);
  rates[r_idx++] = sign_rate;
  update_cdf(sign_cdf, sign, 2);

  const int class_rate = get_symbol_cost(class_cdf, mv_class);
  rates[r_idx++] = class_rate;
  update_cdf(class_cdf, mv_class, MV_CLASSES);

  int int_bit_rate = 0;
  if (mv_class == MV_CLASS_0) {
    int_bit_rate = get_symbol_cost(class0_cdf, int_part);
    update_cdf(class0_cdf, int_part, CLASS0_SIZE);
  } else {
    const int n = mv_class + CLASS0_BITS - 1;  // number of bits
    for (int i = 0; i < n; ++i) {
      int_bit_rate += get_symbol_cost(bits_cdf[i], (int_part >> i) & 1);
      update_cdf(bits_cdf[i], (int_part >> i) & 1, 2);
    }
  }
  rates[r_idx++] = int_bit_rate;
  const int frac_part_rate = get_symbol_cost(frac_part_cdf, frac_part);
  rates[r_idx++] = frac_part_rate;
  update_cdf(frac_part_cdf, frac_part, MV_FP_SIZE);
  const int high_part_rate =
      use_hp ? get_symbol_cost(high_part_cdf, high_part) : 0;
  if (use_hp) {
    update_cdf(high_part_cdf, high_part, 2);
  }
  rates[r_idx++] = high_part_rate;

  mv_stats->last_bit_zero += !high_part;
  mv_stats->last_bit_nonzero += high_part;
  const int total_rate =
      (sign_rate + class_rate + int_bit_rate + frac_part_rate + high_part_rate);
  return total_rate;
}

static AOM_INLINE void keep_one_mv_stat(MV_STATS *mv_stats, const MV *ref_mv,
                                        const MV *cur_mv, const AV1_COMP *cpi) {
  const MACROBLOCK *const x = &cpi->td.mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  nmv_context *nmvc = &ec_ctx->nmvc;
  aom_cdf_prob *joint_cdf = nmvc->joints_cdf;
  const int use_hp = cpi->common.features.allow_high_precision_mv;

  const MV diff = { cur_mv->row - ref_mv->row, cur_mv->col - ref_mv->col };
  const int mv_joint = av1_get_mv_joint(&diff);
  // TODO(chiyotsai@google.com): Estimate hp_diff when we are using lp
  const MV hp_diff = diff;
  const int hp_mv_joint = av1_get_mv_joint(&hp_diff);
  const MV truncated_diff = { (diff.row / 2) * 2, (diff.col / 2) * 2 };
  const MV lp_diff = use_hp ? truncated_diff : diff;
  const int lp_mv_joint = av1_get_mv_joint(&lp_diff);

  const int mv_joint_rate = get_symbol_cost(joint_cdf, mv_joint);
  const int hp_mv_joint_rate = get_symbol_cost(joint_cdf, hp_mv_joint);
  const int lp_mv_joint_rate = get_symbol_cost(joint_cdf, lp_mv_joint);

  update_cdf(joint_cdf, mv_joint, MV_JOINTS);

  mv_stats->total_mv_rate += mv_joint_rate;
  mv_stats->hp_total_mv_rate += hp_mv_joint_rate;
  mv_stats->lp_total_mv_rate += lp_mv_joint_rate;
  mv_stats->mv_joint_count[mv_joint]++;

  for (int comp_idx = 0; comp_idx < 2; comp_idx++) {
    const int comp_val = comp_idx ? diff.col : diff.row;
    const int hp_comp_val = comp_idx ? hp_diff.col : hp_diff.row;
    const int lp_comp_val = comp_idx ? lp_diff.col : lp_diff.row;
    int rates[5];
    av1_zero_array(rates, 5);

    const int comp_rate =
        comp_val ? keep_one_comp_stat(mv_stats, comp_val, comp_idx, cpi, rates)
                 : 0;
    // TODO(chiyotsai@google.com): Properly get hp rate when use_hp is false
    const int hp_rate =
        hp_comp_val ? rates[0] + rates[1] + rates[2] + rates[3] + rates[4] : 0;
    const int lp_rate =
        lp_comp_val ? rates[0] + rates[1] + rates[2] + rates[3] : 0;

    mv_stats->total_mv_rate += comp_rate;
    mv_stats->hp_total_mv_rate += hp_rate;
    mv_stats->lp_total_mv_rate += lp_rate;
  }
}

static AOM_INLINE void collect_mv_stats_b(MV_STATS *mv_stats,
                                          const AV1_COMP *cpi, int mi_row,
                                          int mi_col) {
  const AV1_COMMON *cm = &cpi->common;
  const CommonModeInfoParams *const mi_params = &cm->mi_params;

  if (mi_row >= mi_params->mi_rows || mi_col >= mi_params->mi_cols) {
    return;
  }

  const MB_MODE_INFO *mbmi =
      mi_params->mi_grid_base[mi_row * mi_params->mi_stride + mi_col];
  const MB_MODE_INFO_EXT_FRAME *mbmi_ext_frame =
      cpi->mbmi_ext_info.frame_base +
      get_mi_ext_idx(mi_row, mi_col, cm->mi_params.mi_alloc_bsize,
                     cpi->mbmi_ext_info.stride);

  if (!is_inter_block(mbmi)) {
    mv_stats->intra_count++;
    return;
  }
  mv_stats->inter_count++;

  const PREDICTION_MODE mode = mbmi->mode;
  const int is_compound = has_second_ref(mbmi);

  if (mode == NEWMV || mode == NEW_NEWMV) {
    // All mvs are new
    for (int ref_idx = 0; ref_idx < 1 + is_compound; ++ref_idx) {
      const MV ref_mv =
          get_ref_mv_for_mv_stats(mbmi, mbmi_ext_frame, ref_idx).as_mv;
      const MV cur_mv = mbmi->mv[ref_idx].as_mv;
      keep_one_mv_stat(mv_stats, &ref_mv, &cur_mv, cpi);
    }
  } else if (mode == NEAREST_NEWMV || mode == NEAR_NEWMV ||
             mode == NEW_NEARESTMV || mode == NEW_NEARMV) {
    // has exactly one new_mv
    mv_stats->default_mvs += 1;

    const int ref_idx = (mode == NEAREST_NEWMV || mode == NEAR_NEWMV);
    const MV ref_mv =
        get_ref_mv_for_mv_stats(mbmi, mbmi_ext_frame, ref_idx).as_mv;
    const MV cur_mv = mbmi->mv[ref_idx].as_mv;

    keep_one_mv_stat(mv_stats, &ref_mv, &cur_mv, cpi);
  } else {
    // No new_mv
    mv_stats->default_mvs += 1 + is_compound;
  }

  // Add texture information
  const BLOCK_SIZE bsize = mbmi->bsize;
  const int num_rows = block_size_high[bsize];
  const int num_cols = block_size_wide[bsize];
  const int y_stride = cpi->source->y_stride;
  const int px_row = 4 * mi_row, px_col = 4 * mi_col;
  const int buf_is_hbd = cpi->source->flags & YV12_FLAG_HIGHBITDEPTH;
  const int bd = cm->seq_params->bit_depth;
  if (buf_is_hbd) {
    uint16_t *source_buf =
        CONVERT_TO_SHORTPTR(cpi->source->y_buffer) + px_row * y_stride + px_col;
    for (int row = 0; row < num_rows - 1; row++) {
      for (int col = 0; col < num_cols - 1; col++) {
        const int offset = row * y_stride + col;
        const int horz_diff =
            abs(source_buf[offset + 1] - source_buf[offset]) >> (bd - 8);
        const int vert_diff =
            abs(source_buf[offset + y_stride] - source_buf[offset]) >> (bd - 8);
        mv_stats->horz_text += horz_diff;
        mv_stats->vert_text += vert_diff;
        mv_stats->diag_text += horz_diff * vert_diff;
      }
    }
  } else {
    uint8_t *source_buf = cpi->source->y_buffer + px_row * y_stride + px_col;
    for (int row = 0; row < num_rows - 1; row++) {
      for (int col = 0; col < num_cols - 1; col++) {
        const int offset = row * y_stride + col;
        const int horz_diff = abs(source_buf[offset + 1] - source_buf[offset]);
        const int vert_diff =
            abs(source_buf[offset + y_stride] - source_buf[offset]);
        mv_stats->horz_text += horz_diff;
        mv_stats->vert_text += vert_diff;
        mv_stats->diag_text += horz_diff * vert_diff;
      }
    }
  }
}

// Split block
static AOM_INLINE void collect_mv_stats_sb(MV_STATS *mv_stats,
                                           const AV1_COMP *cpi, int mi_row,
                                           int mi_col, BLOCK_SIZE bsize) {
  assert(bsize < BLOCK_SIZES_ALL);
  const AV1_COMMON *cm = &cpi->common;

  if (mi_row >= cm->mi_params.mi_rows || mi_col >= cm->mi_params.mi_cols)
    return;

  const PARTITION_TYPE partition = get_partition(cm, mi_row, mi_col, bsize);
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);

  const int hbs = mi_size_wide[bsize] / 2;
  const int qbs = mi_size_wide[bsize] / 4;
  switch (partition) {
    case PARTITION_NONE:
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col);
      break;
    case PARTITION_HORZ:
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col);
      collect_mv_stats_b(mv_stats, cpi, mi_row + hbs, mi_col);
      break;
    case PARTITION_VERT:
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col);
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col + hbs);
      break;
    case PARTITION_SPLIT:
      collect_mv_stats_sb(mv_stats, cpi, mi_row, mi_col, subsize);
      collect_mv_stats_sb(mv_stats, cpi, mi_row, mi_col + hbs, subsize);
      collect_mv_stats_sb(mv_stats, cpi, mi_row + hbs, mi_col, subsize);
      collect_mv_stats_sb(mv_stats, cpi, mi_row + hbs, mi_col + hbs, subsize);
      break;
    case PARTITION_HORZ_A:
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col);
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col + hbs);
      collect_mv_stats_b(mv_stats, cpi, mi_row + hbs, mi_col);
      break;
    case PARTITION_HORZ_B:
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col);
      collect_mv_stats_b(mv_stats, cpi, mi_row + hbs, mi_col);
      collect_mv_stats_b(mv_stats, cpi, mi_row + hbs, mi_col + hbs);
      break;
    case PARTITION_VERT_A:
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col);
      collect_mv_stats_b(mv_stats, cpi, mi_row + hbs, mi_col);
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col + hbs);
      break;
    case PARTITION_VERT_B:
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col);
      collect_mv_stats_b(mv_stats, cpi, mi_row, mi_col + hbs);
      collect_mv_stats_b(mv_stats, cpi, mi_row + hbs, mi_col + hbs);
      break;
    case PARTITION_HORZ_4:
      for (int i = 0; i < 4; ++i) {
        const int this_mi_row = mi_row + i * qbs;
        collect_mv_stats_b(mv_stats, cpi, this_mi_row, mi_col);
      }
      break;
    case PARTITION_VERT_4:
      for (int i = 0; i < 4; ++i) {
        const int this_mi_col = mi_col + i * qbs;
        collect_mv_stats_b(mv_stats, cpi, mi_row, this_mi_col);
      }
      break;
    default: assert(0);
  }
}

static AOM_INLINE void collect_mv_stats_tile(MV_STATS *mv_stats,
                                             const AV1_COMP *cpi,
                                             const TileInfo *tile_info) {
  const AV1_COMMON *cm = &cpi->common;
  const int mi_row_start = tile_info->mi_row_start;
  const int mi_row_end = tile_info->mi_row_end;
  const int mi_col_start = tile_info->mi_col_start;
  const int mi_col_end = tile_info->mi_col_end;
  const int sb_size_mi = cm->seq_params->mib_size;
  BLOCK_SIZE sb_size = cm->seq_params->sb_size;
  for (int mi_row = mi_row_start; mi_row < mi_row_end; mi_row += sb_size_mi) {
    for (int mi_col = mi_col_start; mi_col < mi_col_end; mi_col += sb_size_mi) {
      collect_mv_stats_sb(mv_stats, cpi, mi_row, mi_col, sb_size);
    }
  }
}

void av1_collect_mv_stats(AV1_COMP *cpi, int current_q) {
  MV_STATS *mv_stats = &cpi->mv_stats;
  const AV1_COMMON *cm = &cpi->common;
  const int tile_cols = cm->tiles.cols;
  const int tile_rows = cm->tiles.rows;

  for (int tile_row = 0; tile_row < tile_rows; tile_row++) {
    TileInfo tile_info;
    av1_tile_set_row(&tile_info, cm, tile_row);
    for (int tile_col = 0; tile_col < tile_cols; tile_col++) {
      const int tile_idx = tile_row * tile_cols + tile_col;
      av1_tile_set_col(&tile_info, cm, tile_col);
      cpi->tile_data[tile_idx].tctx = *cm->fc;
      cpi->td.mb.e_mbd.tile_ctx = &cpi->tile_data[tile_idx].tctx;
      collect_mv_stats_tile(mv_stats, cpi, &tile_info);
    }
  }

  mv_stats->q = current_q;
  mv_stats->order = cpi->common.current_frame.order_hint;
  mv_stats->valid = 1;
}

static AOM_INLINE int get_smart_mv_prec(AV1_COMP *cpi, const MV_STATS *mv_stats,
                                        int current_q) {
  const AV1_COMMON *cm = &cpi->common;
  const int order_hint = cpi->common.current_frame.order_hint;
  const int order_diff = order_hint - mv_stats->order;
  const float area = (float)(cm->width * cm->height);
  float features[MV_PREC_FEATURE_SIZE] = {
    (float)current_q,
    (float)mv_stats->q,
    (float)order_diff,
    mv_stats->inter_count / area,
    mv_stats->intra_count / area,
    mv_stats->default_mvs / area,
    mv_stats->mv_joint_count[0] / area,
    mv_stats->mv_joint_count[1] / area,
    mv_stats->mv_joint_count[2] / area,
    mv_stats->mv_joint_count[3] / area,
    mv_stats->last_bit_zero / area,
    mv_stats->last_bit_nonzero / area,
    mv_stats->total_mv_rate / area,
    mv_stats->hp_total_mv_rate / area,
    mv_stats->lp_total_mv_rate / area,
    mv_stats->horz_text / area,
    mv_stats->vert_text / area,
    mv_stats->diag_text / area,
  };

  for (int f_idx = 0; f_idx < MV_PREC_FEATURE_SIZE; f_idx++) {
    features[f_idx] =
        (features[f_idx] - av1_mv_prec_mean[f_idx]) / av1_mv_prec_std[f_idx];
  }
  float score = 0.0f;

  av1_nn_predict(features, &av1_mv_prec_dnn_config, 1, &score);

  const int use_high_hp = score >= 0.0f;
  return use_high_hp;
}
#endif  // !CONFIG_REALTIME_ONLY

void av1_pick_and_set_high_precision_mv(AV1_COMP *cpi, int qindex) {
  int use_hp = qindex < HIGH_PRECISION_MV_QTHRESH;
#if !CONFIG_REALTIME_ONLY
  MV_STATS *mv_stats = &cpi->mv_stats;
#endif  // !CONFIG_REALTIME_ONLY

  if (cpi->sf.hl_sf.high_precision_mv_usage == QTR_ONLY) {
    use_hp = 0;
  }
#if !CONFIG_REALTIME_ONLY
  else if (cpi->sf.hl_sf.high_precision_mv_usage == LAST_MV_DATA &&
           av1_frame_allows_smart_mv(cpi) && mv_stats->valid) {
    use_hp = get_smart_mv_prec(cpi, mv_stats, qindex);
  }
#endif  // !CONFIG_REALTIME_ONLY

  av1_set_high_precision_mv(cpi, use_hp,
                            cpi->common.features.cur_frame_force_integer_mv);
}
