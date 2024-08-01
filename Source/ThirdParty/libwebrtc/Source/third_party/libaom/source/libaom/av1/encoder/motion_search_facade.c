/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/reconinter.h"

#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/interp_search.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/motion_search_facade.h"
#include "av1/encoder/partition_strategy.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/tpl_model.h"
#include "av1/encoder/tx_search.h"

#define RIGHT_SHIFT_MV(x) (((x) + 3 + ((x) >= 0)) >> 3)

typedef struct {
  int_mv fmv;
  int weight;
} cand_mv_t;

static int compare_weight(const void *a, const void *b) {
  const int diff = ((cand_mv_t *)a)->weight - ((cand_mv_t *)b)->weight;
  if (diff < 0)
    return 1;
  else if (diff > 0)
    return -1;
  return 0;
}

// Allow more mesh searches for screen content type on the ARF.
static int use_fine_search_interval(const AV1_COMP *const cpi) {
  return cpi->is_screen_content_type &&
         cpi->ppi->gf_group.update_type[cpi->gf_frame_index] == ARF_UPDATE &&
         cpi->oxcf.speed <= 2;
}

// Iterate through the tpl and collect the mvs to be used as candidates
static INLINE void get_mv_candidate_from_tpl(const AV1_COMP *const cpi,
                                             const MACROBLOCK *x,
                                             BLOCK_SIZE bsize, int ref,
                                             cand_mv_t *cand, int *cand_count,
                                             int *total_cand_weight) {
  const SuperBlockEnc *sb_enc = &x->sb_enc;
  if (!sb_enc->tpl_data_count) {
    return;
  }

  const AV1_COMMON *cm = &cpi->common;
  const MACROBLOCKD *xd = &x->e_mbd;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  const BLOCK_SIZE tpl_bsize =
      convert_length_to_bsize(cpi->ppi->tpl_data.tpl_bsize_1d);
  const int tplw = mi_size_wide[tpl_bsize];
  const int tplh = mi_size_high[tpl_bsize];
  const int nw = mi_size_wide[bsize] / tplw;
  const int nh = mi_size_high[bsize] / tplh;

  if (nw >= 1 && nh >= 1) {
    const int of_h = mi_row % mi_size_high[cm->seq_params->sb_size];
    const int of_w = mi_col % mi_size_wide[cm->seq_params->sb_size];
    const int start = of_h / tplh * sb_enc->tpl_stride + of_w / tplw;
    int valid = 1;

    // Assign large weight to start_mv, so it is always tested.
    cand[0].weight = nw * nh;

    for (int k = 0; k < nh; k++) {
      for (int l = 0; l < nw; l++) {
        const int_mv mv =
            sb_enc
                ->tpl_mv[start + k * sb_enc->tpl_stride + l][ref - LAST_FRAME];
        if (mv.as_int == INVALID_MV) {
          valid = 0;
          break;
        }

        const FULLPEL_MV fmv = { GET_MV_RAWPEL(mv.as_mv.row),
                                 GET_MV_RAWPEL(mv.as_mv.col) };
        int unique = 1;
        for (int m = 0; m < *cand_count; m++) {
          if (RIGHT_SHIFT_MV(fmv.row) ==
                  RIGHT_SHIFT_MV(cand[m].fmv.as_fullmv.row) &&
              RIGHT_SHIFT_MV(fmv.col) ==
                  RIGHT_SHIFT_MV(cand[m].fmv.as_fullmv.col)) {
            unique = 0;
            cand[m].weight++;
            break;
          }
        }

        if (unique) {
          cand[*cand_count].fmv.as_fullmv = fmv;
          cand[*cand_count].weight = 1;
          (*cand_count)++;
        }
      }
      if (!valid) break;
    }

    if (valid) {
      *total_cand_weight = 2 * nh * nw;
      if (*cand_count > 2)
        qsort(cand, *cand_count, sizeof(cand[0]), &compare_weight);
    }
  }
}

void av1_single_motion_search(const AV1_COMP *const cpi, MACROBLOCK *x,
                              BLOCK_SIZE bsize, int ref_idx, int *rate_mv,
                              int search_range, inter_mode_info *mode_info,
                              int_mv *best_mv,
                              struct HandleInterModeArgs *const args) {
  MACROBLOCKD *xd = &x->e_mbd;
  const AV1_COMMON *cm = &cpi->common;
  const MotionVectorSearchParams *mv_search_params = &cpi->mv_search_params;
  const int num_planes = av1_num_planes(cm);
  MB_MODE_INFO *mbmi = xd->mi[0];
  struct buf_2d backup_yv12[MAX_MB_PLANE] = { { 0, 0, 0, 0, 0 } };
  int bestsme = INT_MAX;
  const int ref = mbmi->ref_frame[ref_idx];
  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const MvCosts *mv_costs = x->mv_costs;

  if (scaled_ref_frame) {
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // full-pixel motion search code to be used without additional
    // modifications.
    for (int i = 0; i < num_planes; i++) {
      backup_yv12[i] = xd->plane[i].pre[ref_idx];
    }
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL,
                         num_planes);
  }

  // Work out the size of the first step in the mv step search.
  // 0 here is maximum length first step. 1 is AOMMAX >> 1 etc.
  int step_param;
  if (cpi->sf.mv_sf.auto_mv_step_size && cm->show_frame) {
    // Take the weighted average of the step_params based on the last frame's
    // max mv magnitude and that based on the best ref mvs of the current
    // block for the given reference.
    step_param = (av1_init_search_range(x->max_mv_context[ref]) +
                  mv_search_params->mv_step_param) /
                 2;
  } else {
    step_param = mv_search_params->mv_step_param;
  }

  const MV ref_mv = av1_get_ref_mv(x, ref_idx).as_mv;
  FULLPEL_MV start_mv;
  if (mbmi->motion_mode != SIMPLE_TRANSLATION)
    start_mv = get_fullmv_from_mv(&mbmi->mv[0].as_mv);
  else
    start_mv = get_fullmv_from_mv(&ref_mv);

  // cand stores start_mv and all possible MVs in a SB.
  cand_mv_t cand[MAX_TPL_BLK_IN_SB * MAX_TPL_BLK_IN_SB + 1];
  av1_zero(cand);
  cand[0].fmv.as_fullmv = start_mv;
  int cnt = 1;
  int total_weight = 0;

  if (!cpi->sf.mv_sf.full_pixel_search_level &&
      mbmi->motion_mode == SIMPLE_TRANSLATION) {
    get_mv_candidate_from_tpl(cpi, x, bsize, ref, cand, &cnt, &total_weight);
  }

  const int cand_cnt = AOMMIN(2, cnt);
  // TODO(any): Test the speed feature for OBMC_CAUSAL mode.
  if (cpi->sf.mv_sf.skip_fullpel_search_using_startmv &&
      mbmi->motion_mode == SIMPLE_TRANSLATION) {
    const int stack_size = args->start_mv_cnt;
    for (int cand_idx = 0; cand_idx < cand_cnt; cand_idx++) {
      int_mv *fmv_cand = &cand[cand_idx].fmv;
      int skip_cand_mv = 0;

      // Check difference between mvs in the stack and candidate mv.
      for (int stack_idx = 0; stack_idx < stack_size; stack_idx++) {
        const uint8_t this_ref_mv_idx = args->ref_mv_idx_stack[stack_idx];
        const FULLPEL_MV *fmv_stack = &args->start_mv_stack[stack_idx];
        const int this_newmv_valid =
            args->single_newmv_valid[this_ref_mv_idx][ref];
        const int row_diff = abs(fmv_stack->row - fmv_cand->as_fullmv.row);
        const int col_diff = abs(fmv_stack->col - fmv_cand->as_fullmv.col);

        if (!this_newmv_valid) continue;

        if (cpi->sf.mv_sf.skip_fullpel_search_using_startmv >= 2) {
          // Prunes the current start_mv candidate, if the absolute mv
          // difference of both row and column are <= 1.
          if (row_diff <= 1 && col_diff <= 1) {
            skip_cand_mv = 1;
            break;
          }
        } else if (cpi->sf.mv_sf.skip_fullpel_search_using_startmv >= 1) {
          // Prunes the current start_mv candidate, if the sum of the absolute
          // mv difference of row and column is <= 1.
          if (row_diff + col_diff <= 1) {
            skip_cand_mv = 1;
            break;
          }
        }
      }
      if (skip_cand_mv) {
        // Ensure atleast one full-pel motion search is not pruned.
        assert(mbmi->ref_mv_idx != 0);
        // Mark the candidate mv as invalid so that motion search gets skipped.
        cand[cand_idx].fmv.as_int = INVALID_MV;
      } else {
        // Store start_mv candidate and corresponding ref_mv_idx of full-pel
        // search in the mv stack (except last ref_mv_idx).
        if (mbmi->ref_mv_idx != MAX_REF_MV_SEARCH - 1) {
          assert(args->start_mv_cnt < (MAX_REF_MV_SEARCH - 1) * 2);
          args->start_mv_stack[args->start_mv_cnt] = fmv_cand->as_fullmv;
          args->ref_mv_idx_stack[args->start_mv_cnt] = mbmi->ref_mv_idx;
          args->start_mv_cnt++;
        }
      }
    }
  }

  // Hot fix for asan complaints when resize mode is on. When resize mode is on,
  // the stride of the reference frame can be different from indicated by
  // MotionVectorSearchParams::search_site_cfg. When this happens, we need to
  // readjust the stride.
  const MV_SPEED_FEATURES *mv_sf = &cpi->sf.mv_sf;
  const SEARCH_METHODS search_method =
      av1_get_default_mv_search_method(x, mv_sf, bsize);
  const search_site_config *src_search_site_cfg =
      av1_get_search_site_config(cpi, x, search_method);

  // Further reduce the search range.
  if (search_range < INT_MAX) {
    const search_site_config *search_site_cfg =
        &src_search_site_cfg[search_method_lookup[search_method]];
    // Max step_param is search_site_cfg->num_search_steps.
    if (search_range < 1) {
      step_param = search_site_cfg->num_search_steps;
    } else {
      while (search_site_cfg->radius[search_site_cfg->num_search_steps -
                                     step_param - 1] > (search_range << 1) &&
             search_site_cfg->num_search_steps - step_param - 1 > 0)
        step_param++;
    }
  }

  int cost_list[5];
  FULLPEL_MV_STATS best_mv_stats;
  int_mv second_best_mv;
  best_mv->as_int = second_best_mv.as_int = INVALID_MV;

  // Allow more mesh searches for screen content type on the ARF.
  const int fine_search_interval = use_fine_search_interval(cpi);
  FULLPEL_MOTION_SEARCH_PARAMS full_ms_params;

  switch (mbmi->motion_mode) {
    case SIMPLE_TRANSLATION: {
      // Perform a search with the top 2 candidates
      int sum_weight = 0;
      for (int m = 0; m < cand_cnt; m++) {
        int_mv smv = cand[m].fmv;
        FULLPEL_MV this_best_mv, this_second_best_mv;
        FULLPEL_MV_STATS this_mv_stats;

        if (smv.as_int == INVALID_MV) continue;

        av1_make_default_fullpel_ms_params(
            &full_ms_params, cpi, x, bsize, &ref_mv, smv.as_fullmv,
            src_search_site_cfg, search_method, fine_search_interval);

        const int thissme =
            av1_full_pixel_search(smv.as_fullmv, &full_ms_params, step_param,
                                  cond_cost_list(cpi, cost_list), &this_best_mv,
                                  &this_mv_stats, &this_second_best_mv);

        if (thissme < bestsme) {
          bestsme = thissme;
          best_mv->as_fullmv = this_best_mv;
          best_mv_stats = this_mv_stats;
          second_best_mv.as_fullmv = this_second_best_mv;
        }

        sum_weight += cand[m].weight;
        if (4 * sum_weight > 3 * total_weight) break;
      }
    } break;
    case OBMC_CAUSAL:
      av1_make_default_fullpel_ms_params(&full_ms_params, cpi, x, bsize,
                                         &ref_mv, start_mv, src_search_site_cfg,
                                         search_method, fine_search_interval);

      bestsme = av1_obmc_full_pixel_search(start_mv, &full_ms_params,
                                           step_param, &best_mv->as_fullmv);
      break;
    default: assert(0 && "Invalid motion mode!\n");
  }
  if (best_mv->as_int == INVALID_MV) return;

  if (scaled_ref_frame) {
    // Swap back the original buffers for subpel motion search.
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[ref_idx] = backup_yv12[i];
    }
  }

  // Terminate search with the current ref_idx based on fullpel mv, rate cost,
  // and other know cost.
  if (cpi->sf.inter_sf.skip_newmv_in_drl >= 2 &&
      mbmi->motion_mode == SIMPLE_TRANSLATION &&
      best_mv->as_int != INVALID_MV) {
    int_mv this_mv;
    this_mv.as_mv = get_mv_from_fullmv(&best_mv->as_fullmv);
    const int ref_mv_idx = mbmi->ref_mv_idx;
    const int this_mv_rate =
        av1_mv_bit_cost(&this_mv.as_mv, &ref_mv, mv_costs->nmv_joint_cost,
                        mv_costs->mv_cost_stack, MV_COST_WEIGHT);
    mode_info[ref_mv_idx].full_search_mv.as_int = this_mv.as_int;
    mode_info[ref_mv_idx].full_mv_rate = this_mv_rate;
    mode_info[ref_mv_idx].full_mv_bestsme = bestsme;

    for (int prev_ref_idx = 0; prev_ref_idx < ref_mv_idx; ++prev_ref_idx) {
      // Check if the motion search result same as previous results
      if (this_mv.as_int == mode_info[prev_ref_idx].full_search_mv.as_int) {
        // Compare the rate cost
        const int prev_rate_cost = mode_info[prev_ref_idx].full_mv_rate +
                                   mode_info[prev_ref_idx].drl_cost;
        const int this_rate_cost =
            this_mv_rate + mode_info[ref_mv_idx].drl_cost;

        if (prev_rate_cost <= this_rate_cost) {
          // If the current rate_cost is worse than the previous rate_cost, then
          // we terminate the search. Since av1_single_motion_search is only
          // called by handle_new_mv in SIMPLE_TRANSLATION mode, we set the
          // best_mv to INVALID mv to signal that we wish to terminate search
          // for the current mode.
          best_mv->as_int = INVALID_MV;
          return;
        }
      }

      // Terminate the evaluation of current ref_mv_idx based on bestsme and
      // drl_cost.
      const int psme = mode_info[prev_ref_idx].full_mv_bestsme;
      if (psme == INT_MAX) continue;
      const int thr =
          cpi->sf.inter_sf.skip_newmv_in_drl == 3 ? (psme + (psme >> 2)) : psme;
      if (cpi->sf.inter_sf.skip_newmv_in_drl >= 3 &&
          mode_info[ref_mv_idx].full_mv_bestsme > thr &&
          mode_info[prev_ref_idx].drl_cost < mode_info[ref_mv_idx].drl_cost) {
        best_mv->as_int = INVALID_MV;
        return;
      }
    }
  }

  if (cpi->common.features.cur_frame_force_integer_mv) {
    convert_fullmv_to_mv(best_mv);
  }

  const int use_fractional_mv =
      bestsme < INT_MAX && cpi->common.features.cur_frame_force_integer_mv == 0;
  int best_mv_rate = 0;
  int mv_rate_calculated = 0;
  if (use_fractional_mv) {
    int_mv fractional_ms_list[3];
    av1_set_fractional_mv(fractional_ms_list);
    int dis; /* TODO: use dis in distortion calculation later. */

    SUBPEL_MOTION_SEARCH_PARAMS ms_params;
    av1_make_default_subpel_ms_params(&ms_params, cpi, x, bsize, &ref_mv,
                                      cost_list);
    MV subpel_start_mv = get_mv_from_fullmv(&best_mv->as_fullmv);
    assert(av1_is_subpelmv_in_range(&ms_params.mv_limits, subpel_start_mv));

    switch (mbmi->motion_mode) {
      case SIMPLE_TRANSLATION:
        if (mv_sf->use_accurate_subpel_search) {
          const int try_second = second_best_mv.as_int != INVALID_MV &&
                                 second_best_mv.as_int != best_mv->as_int &&
                                 (mv_sf->disable_second_mv <= 1);
          const int best_mv_var = mv_search_params->find_fractional_mv_step(
              xd, cm, &ms_params, subpel_start_mv, &best_mv_stats,
              &best_mv->as_mv, &dis, &x->pred_sse[ref], fractional_ms_list);

          if (try_second) {
            struct macroblockd_plane *p = xd->plane;
            const BUFFER_SET orig_dst = {
              { p[0].dst.buf, p[1].dst.buf, p[2].dst.buf },
              { p[0].dst.stride, p[1].dst.stride, p[2].dst.stride },
            };
            int64_t rd = INT64_MAX;
            if (!mv_sf->disable_second_mv) {
              // Calculate actual rd cost.
              mbmi->mv[0].as_mv = best_mv->as_mv;
              av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, &orig_dst,
                                            bsize, 0, 0);
              av1_subtract_plane(x, bsize, 0);
              RD_STATS this_rd_stats;
              av1_init_rd_stats(&this_rd_stats);
              av1_estimate_txfm_yrd(cpi, x, &this_rd_stats, INT64_MAX, bsize,
                                    max_txsize_rect_lookup[bsize]);
              int this_mv_rate = av1_mv_bit_cost(
                  &best_mv->as_mv, &ref_mv, mv_costs->nmv_joint_cost,
                  mv_costs->mv_cost_stack, MV_COST_WEIGHT);
              rd = RDCOST(x->rdmult, this_mv_rate + this_rd_stats.rate,
                          this_rd_stats.dist);
            }

            MV this_best_mv;
            subpel_start_mv = get_mv_from_fullmv(&second_best_mv.as_fullmv);
            if (av1_is_subpelmv_in_range(&ms_params.mv_limits,
                                         subpel_start_mv)) {
              unsigned int sse;
              const int this_var = mv_search_params->find_fractional_mv_step(
                  xd, cm, &ms_params, subpel_start_mv, NULL, &this_best_mv,
                  &dis, &sse, fractional_ms_list);

              if (!mv_sf->disable_second_mv) {
                // If cpi->sf.mv_sf.disable_second_mv is 0, use actual rd cost
                // to choose the better MV.
                mbmi->mv[0].as_mv = this_best_mv;
                av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, &orig_dst,
                                              bsize, 0, 0);
                av1_subtract_plane(x, bsize, 0);
                RD_STATS tmp_rd_stats;
                av1_init_rd_stats(&tmp_rd_stats);
                av1_estimate_txfm_yrd(cpi, x, &tmp_rd_stats, INT64_MAX, bsize,
                                      max_txsize_rect_lookup[bsize]);
                int tmp_mv_rate = av1_mv_bit_cost(
                    &this_best_mv, &ref_mv, mv_costs->nmv_joint_cost,
                    mv_costs->mv_cost_stack, MV_COST_WEIGHT);
                int64_t tmp_rd =
                    RDCOST(x->rdmult, tmp_rd_stats.rate + tmp_mv_rate,
                           tmp_rd_stats.dist);
                if (tmp_rd < rd) {
                  best_mv->as_mv = this_best_mv;
                  x->pred_sse[ref] = sse;
                }
              } else {
                // If cpi->sf.mv_sf.disable_second_mv = 1, use var to decide the
                // best MV.
                if (this_var < best_mv_var) {
                  best_mv->as_mv = this_best_mv;
                  x->pred_sse[ref] = sse;
                }
              }
            }
          }
        } else {
          mv_search_params->find_fractional_mv_step(
              xd, cm, &ms_params, subpel_start_mv, &best_mv_stats,
              &best_mv->as_mv, &dis, &x->pred_sse[ref], NULL);
        }
        break;
      case OBMC_CAUSAL:
        av1_find_best_obmc_sub_pixel_tree_up(
            xd, cm, &ms_params, subpel_start_mv, NULL, &best_mv->as_mv, &dis,
            &x->pred_sse[ref], NULL);
        break;
      default: assert(0 && "Invalid motion mode!\n");
    }

    // Terminate search with the current ref_idx based on subpel mv and rate
    // cost.
    if (cpi->sf.inter_sf.skip_newmv_in_drl >= 1 && args != NULL &&
        mbmi->motion_mode == SIMPLE_TRANSLATION &&
        best_mv->as_int != INVALID_MV) {
      const int ref_mv_idx = mbmi->ref_mv_idx;
      best_mv_rate =
          av1_mv_bit_cost(&best_mv->as_mv, &ref_mv, mv_costs->nmv_joint_cost,
                          mv_costs->mv_cost_stack, MV_COST_WEIGHT);
      mv_rate_calculated = 1;

      for (int prev_ref_idx = 0; prev_ref_idx < ref_mv_idx; ++prev_ref_idx) {
        if (!args->single_newmv_valid[prev_ref_idx][ref]) continue;
        // Check if the motion vectors are the same.
        if (best_mv->as_int == args->single_newmv[prev_ref_idx][ref].as_int) {
          // Skip this evaluation if the previous one is skipped.
          if (mode_info[prev_ref_idx].skip) {
            mode_info[ref_mv_idx].skip = 1;
            break;
          }
          // Compare the rate cost that we current know.
          const int prev_rate_cost =
              args->single_newmv_rate[prev_ref_idx][ref] +
              mode_info[prev_ref_idx].drl_cost;
          const int this_rate_cost =
              best_mv_rate + mode_info[ref_mv_idx].drl_cost;

          if (prev_rate_cost <= this_rate_cost) {
            // If the current rate_cost is worse than the previous rate_cost,
            // then we terminate the search for this ref_mv_idx.
            mode_info[ref_mv_idx].skip = 1;
            break;
          }
        }
      }
    }
  }

  if (mv_rate_calculated) {
    *rate_mv = best_mv_rate;
  } else {
    *rate_mv =
        av1_mv_bit_cost(&best_mv->as_mv, &ref_mv, mv_costs->nmv_joint_cost,
                        mv_costs->mv_cost_stack, MV_COST_WEIGHT);
  }
}

int av1_joint_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                            BLOCK_SIZE bsize, int_mv *cur_mv,
                            const uint8_t *mask, int mask_stride, int *rate_mv,
                            int allow_second_mv, int joint_me_num_refine_iter) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  const int plane = 0;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));
  const int_mv init_mv[2] = { cur_mv[0], cur_mv[1] };
  const int refs[2] = { mbmi->ref_frame[0], mbmi->ref_frame[1] };
  const MvCosts *mv_costs = x->mv_costs;
  int_mv ref_mv[2];
  int ite, ref;

  // Get the prediction block from the 'other' reference frame.
  const int_interpfilters interp_filters =
      av1_broadcast_interp_filter(EIGHTTAP_REGULAR);

  InterPredParams inter_pred_params;
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  // Do joint motion search in compound mode to get more accurate mv.
  struct buf_2d backup_yv12[2][MAX_MB_PLANE];
  int last_besterr[2] = { INT_MAX, INT_MAX };
  const YV12_BUFFER_CONFIG *const scaled_ref_frame[2] = {
    av1_get_scaled_ref_frame(cpi, refs[0]),
    av1_get_scaled_ref_frame(cpi, refs[1])
  };

  // Prediction buffer from second frame.
  DECLARE_ALIGNED(16, uint8_t, second_pred16[MAX_SB_SQUARE * sizeof(uint16_t)]);
  uint8_t *second_pred = get_buf_by_bd(xd, second_pred16);

  int_mv best_mv, second_best_mv;

  // Allow joint search multiple times iteratively for each reference frame
  // and break out of the search loop if it couldn't find a better mv.
  for (ite = 0; ite < (2 * joint_me_num_refine_iter); ite++) {
    struct buf_2d ref_yv12[2];
    int bestsme = INT_MAX;
    int id = ite % 2;  // Even iterations search in the first reference frame,
                       // odd iterations search in the second. The predictor
                       // found for the 'other' reference frame is factored in.
    if (ite >= 2 && cur_mv[!id].as_int == init_mv[!id].as_int) {
      if (cur_mv[id].as_int == init_mv[id].as_int) {
        break;
      } else {
        int_mv cur_int_mv, init_int_mv;
        cur_int_mv.as_mv.col = cur_mv[id].as_mv.col >> 3;
        cur_int_mv.as_mv.row = cur_mv[id].as_mv.row >> 3;
        init_int_mv.as_mv.row = init_mv[id].as_mv.row >> 3;
        init_int_mv.as_mv.col = init_mv[id].as_mv.col >> 3;
        if (cur_int_mv.as_int == init_int_mv.as_int) {
          break;
        }
      }
    }
    for (ref = 0; ref < 2; ++ref) {
      ref_mv[ref] = av1_get_ref_mv(x, ref);
      // Swap out the reference frame for a version that's been scaled to
      // match the resolution of the current frame, allowing the existing
      // motion search code to be used without additional modifications.
      if (scaled_ref_frame[ref]) {
        int i;
        for (i = 0; i < num_planes; i++)
          backup_yv12[ref][i] = xd->plane[i].pre[ref];
        av1_setup_pre_planes(xd, ref, scaled_ref_frame[ref], mi_row, mi_col,
                             NULL, num_planes);
      }
    }

    assert(IMPLIES(scaled_ref_frame[0] != NULL,
                   cm->width == scaled_ref_frame[0]->y_crop_width &&
                       cm->height == scaled_ref_frame[0]->y_crop_height));
    assert(IMPLIES(scaled_ref_frame[1] != NULL,
                   cm->width == scaled_ref_frame[1]->y_crop_width &&
                       cm->height == scaled_ref_frame[1]->y_crop_height));

    // Initialize based on (possibly scaled) prediction buffers.
    ref_yv12[0] = xd->plane[plane].pre[0];
    ref_yv12[1] = xd->plane[plane].pre[1];

    av1_init_inter_params(&inter_pred_params, pw, ph, mi_row * MI_SIZE,
                          mi_col * MI_SIZE, 0, 0, xd->bd, is_cur_buf_hbd(xd), 0,
                          &cm->sf_identity, &ref_yv12[!id], interp_filters);
    inter_pred_params.conv_params = get_conv_params(0, 0, xd->bd);

    // Since we have scaled the reference frames to match the size of the
    // current frame we must use a unit scaling factor during mode selection.
    av1_enc_build_one_inter_predictor(second_pred, pw, &cur_mv[!id].as_mv,
                                      &inter_pred_params);

    // Do full-pixel compound motion search on the current reference frame.
    if (id) xd->plane[plane].pre[0] = ref_yv12[id];

    // Make motion search params
    FULLPEL_MOTION_SEARCH_PARAMS full_ms_params;
    FULLPEL_MV_STATS best_mv_stats;
    const MV_SPEED_FEATURES *mv_sf = &cpi->sf.mv_sf;
    const SEARCH_METHODS search_method =
        av1_get_default_mv_search_method(x, mv_sf, bsize);
    const search_site_config *src_search_sites =
        av1_get_search_site_config(cpi, x, search_method);
    // Use the mv result from the single mode as mv predictor.
    const FULLPEL_MV start_fullmv = get_fullmv_from_mv(&cur_mv[id].as_mv);
    av1_make_default_fullpel_ms_params(&full_ms_params, cpi, x, bsize,
                                       &ref_mv[id].as_mv, start_fullmv,
                                       src_search_sites, search_method,
                                       /*fine_search_interval=*/0);

    av1_set_ms_compound_refs(&full_ms_params.ms_buffers, second_pred, mask,
                             mask_stride, id);

    // Small-range full-pixel motion search.
    if (!mv_sf->disable_extensive_joint_motion_search &&
        mbmi->interinter_comp.type != COMPOUND_WEDGE) {
      bestsme = av1_full_pixel_search(start_fullmv, &full_ms_params, 5, NULL,
                                      &best_mv.as_fullmv, &best_mv_stats,
                                      &second_best_mv.as_fullmv);
    } else {
      bestsme = av1_refining_search_8p_c(&full_ms_params, start_fullmv,
                                         &best_mv.as_fullmv);
      second_best_mv = best_mv;
    }

    const int try_second = second_best_mv.as_int != INVALID_MV &&
                           second_best_mv.as_int != best_mv.as_int &&
                           allow_second_mv;

    // Restore the pointer to the first (possibly scaled) prediction buffer.
    if (id) xd->plane[plane].pre[0] = ref_yv12[0];

    for (ref = 0; ref < 2; ++ref) {
      if (scaled_ref_frame[ref]) {
        // Swap back the original buffers for subpel motion search.
        for (int i = 0; i < num_planes; i++) {
          xd->plane[i].pre[ref] = backup_yv12[ref][i];
        }
        // Re-initialize based on unscaled prediction buffers.
        ref_yv12[ref] = xd->plane[plane].pre[ref];
      }
    }

    // Do sub-pixel compound motion search on the current reference frame.
    if (id) xd->plane[plane].pre[0] = ref_yv12[id];

    if (cpi->common.features.cur_frame_force_integer_mv) {
      convert_fullmv_to_mv(&best_mv);
    }
    if (bestsme < INT_MAX &&
        cpi->common.features.cur_frame_force_integer_mv == 0) {
      int dis; /* TODO: use dis in distortion calculation later. */
      unsigned int sse;
      SUBPEL_MOTION_SEARCH_PARAMS ms_params;
      av1_make_default_subpel_ms_params(&ms_params, cpi, x, bsize,
                                        &ref_mv[id].as_mv, NULL);
      av1_set_ms_compound_refs(&ms_params.var_params.ms_buffers, second_pred,
                               mask, mask_stride, id);
      ms_params.forced_stop = EIGHTH_PEL;
      MV start_mv = get_mv_from_fullmv(&best_mv.as_fullmv);
      assert(av1_is_subpelmv_in_range(&ms_params.mv_limits, start_mv));
      bestsme = cpi->mv_search_params.find_fractional_mv_step(
          xd, cm, &ms_params, start_mv, NULL, &best_mv.as_mv, &dis, &sse, NULL);

      if (try_second) {
        MV this_best_mv;
        MV subpel_start_mv = get_mv_from_fullmv(&second_best_mv.as_fullmv);
        if (av1_is_subpelmv_in_range(&ms_params.mv_limits, subpel_start_mv)) {
          const int thissme = cpi->mv_search_params.find_fractional_mv_step(
              xd, cm, &ms_params, subpel_start_mv, NULL, &this_best_mv, &dis,
              &sse, NULL);
          if (thissme < bestsme) {
            best_mv.as_mv = this_best_mv;
            bestsme = thissme;
          }
        }
      }
    }

    // Restore the pointer to the first prediction buffer.
    if (id) xd->plane[plane].pre[0] = ref_yv12[0];
    if (bestsme < last_besterr[id]) {
      cur_mv[id] = best_mv;
      last_besterr[id] = bestsme;
    } else {
      break;
    }
  }

  *rate_mv = 0;

  for (ref = 0; ref < 2; ++ref) {
    const int_mv curr_ref_mv = av1_get_ref_mv(x, ref);
    *rate_mv += av1_mv_bit_cost(&cur_mv[ref].as_mv, &curr_ref_mv.as_mv,
                                mv_costs->nmv_joint_cost,
                                mv_costs->mv_cost_stack, MV_COST_WEIGHT);
  }

  return AOMMIN(last_besterr[0], last_besterr[1]);
}

// Search for the best mv for one component of a compound,
// given that the other component is fixed.
int av1_compound_single_motion_search(const AV1_COMP *cpi, MACROBLOCK *x,
                                      BLOCK_SIZE bsize, MV *this_mv,
                                      const uint8_t *second_pred,
                                      const uint8_t *mask, int mask_stride,
                                      int *rate_mv, int ref_idx) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const int ref = mbmi->ref_frame[ref_idx];
  const int_mv ref_mv = av1_get_ref_mv(x, ref_idx);
  struct macroblockd_plane *const pd = &xd->plane[0];
  const MvCosts *mv_costs = x->mv_costs;

  struct buf_2d backup_yv12[MAX_MB_PLANE];
  const YV12_BUFFER_CONFIG *const scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);

  // Check that this is either an interinter or an interintra block
  assert(has_second_ref(mbmi) || (ref_idx == 0 && is_interintra_mode(mbmi)));

  // Store the first prediction buffer.
  struct buf_2d orig_yv12;
  if (ref_idx) {
    orig_yv12 = pd->pre[0];
    pd->pre[0] = pd->pre[ref_idx];
  }

  if (scaled_ref_frame) {
    // Swap out the reference frame for a version that's been scaled to
    // match the resolution of the current frame, allowing the existing
    // full-pixel motion search code to be used without additional
    // modifications.
    for (int i = 0; i < num_planes; i++) {
      backup_yv12[i] = xd->plane[i].pre[ref_idx];
    }
    const int mi_row = xd->mi_row;
    const int mi_col = xd->mi_col;
    // The index below needs to be 0 instead of ref_idx since we assume the
    // 0th slot to be used for subsequent searches. Note that the ref_idx
    // reference buffer has been copied to the 0th slot in the code above.
    // Now we need to swap the reference frame for the 0th slot.
    av1_setup_pre_planes(xd, 0, scaled_ref_frame, mi_row, mi_col, NULL,
                         num_planes);
  }

  int bestsme = INT_MAX;
  int_mv best_mv;

  // Make motion search params
  FULLPEL_MOTION_SEARCH_PARAMS full_ms_params;
  FULLPEL_MV_STATS best_mv_stats;
  const SEARCH_METHODS search_method =
      av1_get_default_mv_search_method(x, &cpi->sf.mv_sf, bsize);
  const search_site_config *src_search_sites =
      av1_get_search_site_config(cpi, x, search_method);
  // Use the mv result from the single mode as mv predictor.
  const FULLPEL_MV start_fullmv = get_fullmv_from_mv(this_mv);
  av1_make_default_fullpel_ms_params(&full_ms_params, cpi, x, bsize,
                                     &ref_mv.as_mv, start_fullmv,
                                     src_search_sites, search_method,
                                     /*fine_search_interval=*/0);

  av1_set_ms_compound_refs(&full_ms_params.ms_buffers, second_pred, mask,
                           mask_stride, ref_idx);

  // Small-range full-pixel motion search.
  bestsme = av1_full_pixel_search(start_fullmv, &full_ms_params, 5, NULL,
                                  &best_mv.as_fullmv, &best_mv_stats, NULL);

  if (scaled_ref_frame) {
    // Swap back the original buffers for subpel motion search for the 0th slot.
    for (int i = 0; i < num_planes; i++) {
      xd->plane[i].pre[0] = backup_yv12[i];
    }
  }

  if (cpi->common.features.cur_frame_force_integer_mv) {
    convert_fullmv_to_mv(&best_mv);
  }
  const int use_fractional_mv =
      bestsme < INT_MAX && cpi->common.features.cur_frame_force_integer_mv == 0;
  if (use_fractional_mv) {
    int dis; /* TODO: use dis in distortion calculation later. */
    unsigned int sse;
    SUBPEL_MOTION_SEARCH_PARAMS ms_params;
    av1_make_default_subpel_ms_params(&ms_params, cpi, x, bsize, &ref_mv.as_mv,
                                      NULL);
    av1_set_ms_compound_refs(&ms_params.var_params.ms_buffers, second_pred,
                             mask, mask_stride, ref_idx);
    ms_params.forced_stop = EIGHTH_PEL;
    MV start_mv = get_mv_from_fullmv(&best_mv.as_fullmv);
    assert(av1_is_subpelmv_in_range(&ms_params.mv_limits, start_mv));
    bestsme = cpi->mv_search_params.find_fractional_mv_step(
        xd, cm, &ms_params, start_mv, &best_mv_stats, &best_mv.as_mv, &dis,
        &sse, NULL);
  }

  // Restore the pointer to the first unscaled prediction buffer.
  if (ref_idx) pd->pre[0] = orig_yv12;

  if (bestsme < INT_MAX) *this_mv = best_mv.as_mv;

  *rate_mv = 0;

  *rate_mv += av1_mv_bit_cost(this_mv, &ref_mv.as_mv, mv_costs->nmv_joint_cost,
                              mv_costs->mv_cost_stack, MV_COST_WEIGHT);
  return bestsme;
}

static AOM_INLINE void build_second_inter_pred(const AV1_COMP *cpi,
                                               MACROBLOCK *x, BLOCK_SIZE bsize,
                                               const MV *other_mv, int ref_idx,
                                               uint8_t *second_pred) {
  const AV1_COMMON *const cm = &cpi->common;
  const int pw = block_size_wide[bsize];
  const int ph = block_size_high[bsize];
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  struct macroblockd_plane *const pd = &xd->plane[0];
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;
  const int p_col = ((mi_col * MI_SIZE) >> pd->subsampling_x);
  const int p_row = ((mi_row * MI_SIZE) >> pd->subsampling_y);

  // This function should only ever be called for compound modes
  assert(has_second_ref(mbmi));

  const int plane = 0;
  struct buf_2d ref_yv12 = xd->plane[plane].pre[!ref_idx];

  struct scale_factors sf;
  av1_setup_scale_factors_for_frame(&sf, ref_yv12.width, ref_yv12.height,
                                    cm->width, cm->height);

  InterPredParams inter_pred_params;

  av1_init_inter_params(&inter_pred_params, pw, ph, p_row, p_col,
                        pd->subsampling_x, pd->subsampling_y, xd->bd,
                        is_cur_buf_hbd(xd), 0, &sf, &ref_yv12,
                        mbmi->interp_filters);
  inter_pred_params.conv_params = get_conv_params(0, plane, xd->bd);

  // Get the prediction block from the 'other' reference frame.
  av1_enc_build_one_inter_predictor(second_pred, pw, other_mv,
                                    &inter_pred_params);
}

// Wrapper for av1_compound_single_motion_search, for the common case
// where the second prediction is also an inter mode.
int av1_compound_single_motion_search_interinter(
    const AV1_COMP *cpi, MACROBLOCK *x, BLOCK_SIZE bsize, int_mv *cur_mv,
    const uint8_t *mask, int mask_stride, int *rate_mv, int ref_idx) {
  MACROBLOCKD *xd = &x->e_mbd;
  // This function should only ever be called for compound modes
  assert(has_second_ref(xd->mi[0]));

  // Prediction buffer from second frame.
  DECLARE_ALIGNED(16, uint16_t, second_pred_alloc_16[MAX_SB_SQUARE]);
  uint8_t *second_pred;
  if (is_cur_buf_hbd(xd))
    second_pred = CONVERT_TO_BYTEPTR(second_pred_alloc_16);
  else
    second_pred = (uint8_t *)second_pred_alloc_16;

  MV *this_mv = &cur_mv[ref_idx].as_mv;
  const MV *other_mv = &cur_mv[!ref_idx].as_mv;
  build_second_inter_pred(cpi, x, bsize, other_mv, ref_idx, second_pred);
  return av1_compound_single_motion_search(cpi, x, bsize, this_mv, second_pred,
                                           mask, mask_stride, rate_mv, ref_idx);
}

static AOM_INLINE void do_masked_motion_search_indexed(
    const AV1_COMP *const cpi, MACROBLOCK *x, const int_mv *const cur_mv,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE bsize,
    int_mv *tmp_mv, int *rate_mv, int which) {
  // NOTE: which values: 0 - 0 only, 1 - 1 only, 2 - both
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  BLOCK_SIZE sb_type = mbmi->bsize;
  const uint8_t *mask;
  const int mask_stride = block_size_wide[bsize];

  mask = av1_get_compound_type_mask(comp_data, sb_type);

  tmp_mv[0].as_int = cur_mv[0].as_int;
  tmp_mv[1].as_int = cur_mv[1].as_int;
  if (which == 0 || which == 1) {
    av1_compound_single_motion_search_interinter(cpi, x, bsize, tmp_mv, mask,
                                                 mask_stride, rate_mv, which);
  } else if (which == 2) {
    const int joint_me_num_refine_iter =
        cpi->sf.inter_sf.enable_fast_compound_mode_search == 2
            ? REDUCED_JOINT_ME_REFINE_ITER
            : NUM_JOINT_ME_REFINE_ITER;
    av1_joint_motion_search(cpi, x, bsize, tmp_mv, mask, mask_stride, rate_mv,
                            !cpi->sf.mv_sf.disable_second_mv,
                            joint_me_num_refine_iter);
  }
}

int av1_interinter_compound_motion_search(const AV1_COMP *const cpi,
                                          MACROBLOCK *x,
                                          const int_mv *const cur_mv,
                                          const BLOCK_SIZE bsize,
                                          const PREDICTION_MODE this_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = xd->mi[0];
  int_mv tmp_mv[2];
  int tmp_rate_mv = 0;
  // TODO(jingning): The average compound mode has proper SAD and variance
  // functions implemented, and is triggerd by setting the mask pointer as
  // Null. Need to further implement those for frame distance weighted mode.
  mbmi->interinter_comp.seg_mask =
      mbmi->interinter_comp.type == COMPOUND_AVERAGE ? NULL : xd->seg_mask;
  const INTERINTER_COMPOUND_DATA *compound_data = &mbmi->interinter_comp;

  if (this_mode == NEW_NEWMV) {
    do_masked_motion_search_indexed(cpi, x, cur_mv, compound_data, bsize,
                                    tmp_mv, &tmp_rate_mv, 2);
    mbmi->mv[0].as_int = tmp_mv[0].as_int;
    mbmi->mv[1].as_int = tmp_mv[1].as_int;
  } else if (this_mode >= NEAREST_NEWMV && this_mode <= NEW_NEARMV) {
    // which = 1 if this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV
    // which = 0 if this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV
    int which = (NEWMV == compound_ref1_mode(this_mode));
    do_masked_motion_search_indexed(cpi, x, cur_mv, compound_data, bsize,
                                    tmp_mv, &tmp_rate_mv, which);
    mbmi->mv[which].as_int = tmp_mv[which].as_int;
  }
  return tmp_rate_mv;
}

int_mv av1_simple_motion_search_sse_var(AV1_COMP *const cpi, MACROBLOCK *x,
                                        int mi_row, int mi_col,
                                        BLOCK_SIZE bsize, int ref,
                                        FULLPEL_MV start_mv, int num_planes,
                                        int use_subpixel, unsigned int *sse,
                                        unsigned int *var) {
  assert(num_planes == 1 &&
         "Currently simple_motion_search only supports luma plane");
  assert(!frame_is_intra_only(&cpi->common) &&
         "Simple motion search only enabled for non-key frames");
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;

  set_offsets_for_motion_search(cpi, x, mi_row, mi_col, bsize);

  MB_MODE_INFO *mbmi = xd->mi[0];
  mbmi->bsize = bsize;
  mbmi->ref_frame[0] = ref;
  mbmi->ref_frame[1] = NONE_FRAME;
  mbmi->motion_mode = SIMPLE_TRANSLATION;
  mbmi->interp_filters = av1_broadcast_interp_filter(EIGHTTAP_REGULAR);

  const YV12_BUFFER_CONFIG *yv12 = get_ref_frame_yv12_buf(cm, ref);
  const YV12_BUFFER_CONFIG *scaled_ref_frame =
      av1_get_scaled_ref_frame(cpi, ref);
  struct buf_2d backup_yv12;
  // ref_mv is used to calculate the cost of the motion vector
  const MV ref_mv = kZeroMv;
  const int step_param =
      AOMMIN(cpi->mv_search_params.mv_step_param +
                 cpi->sf.part_sf.simple_motion_search_reduce_search_steps,
             MAX_MVSEARCH_STEPS - 2);
  int cost_list[5];
  const int ref_idx = 0;
  int bestsme;
  int_mv best_mv;
  FULLPEL_MV_STATS best_mv_stats;

  av1_setup_pre_planes(xd, ref_idx, yv12, mi_row, mi_col,
                       get_ref_scale_factors(cm, ref), num_planes);
  set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
  if (scaled_ref_frame) {
    backup_yv12 = xd->plane[AOM_PLANE_Y].pre[ref_idx];
    av1_setup_pre_planes(xd, ref_idx, scaled_ref_frame, mi_row, mi_col, NULL,
                         num_planes);
  }

  // Allow more mesh searches for screen content type on the ARF.
  const int fine_search_interval = use_fine_search_interval(cpi);
  FULLPEL_MOTION_SEARCH_PARAMS full_ms_params;
  const MV_SPEED_FEATURES *mv_sf = &cpi->sf.mv_sf;
  const SEARCH_METHODS search_method =
      av1_get_default_mv_search_method(x, mv_sf, bsize);
  const search_site_config *src_search_sites =
      av1_get_search_site_config(cpi, x, search_method);
  av1_make_default_fullpel_ms_params(&full_ms_params, cpi, x, bsize, &ref_mv,
                                     start_mv, src_search_sites, search_method,
                                     fine_search_interval);

  bestsme = av1_full_pixel_search(start_mv, &full_ms_params, step_param,
                                  cond_cost_list(cpi, cost_list),
                                  &best_mv.as_fullmv, &best_mv_stats, NULL);

  const int use_subpel_search =
      bestsme < INT_MAX && !cpi->common.features.cur_frame_force_integer_mv &&
      use_subpixel &&
      (cpi->sf.mv_sf.simple_motion_subpel_force_stop != FULL_PEL);
  if (scaled_ref_frame) {
    xd->plane[AOM_PLANE_Y].pre[ref_idx] = backup_yv12;
  }
  if (use_subpel_search) {
    int not_used = 0;

    SUBPEL_MOTION_SEARCH_PARAMS ms_params;
    av1_make_default_subpel_ms_params(&ms_params, cpi, x, bsize, &ref_mv,
                                      cost_list);
    // TODO(yunqing): integrate this into av1_make_default_subpel_ms_params().
    ms_params.forced_stop = mv_sf->simple_motion_subpel_force_stop;

    MV subpel_start_mv = get_mv_from_fullmv(&best_mv.as_fullmv);
    assert(av1_is_subpelmv_in_range(&ms_params.mv_limits, subpel_start_mv));

    cpi->mv_search_params.find_fractional_mv_step(
        xd, cm, &ms_params, subpel_start_mv, &best_mv_stats, &best_mv.as_mv,
        &not_used, &x->pred_sse[ref], NULL);

    mbmi->mv[0] = best_mv;

    // Get a copy of the prediction output
    av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize,
                                  AOM_PLANE_Y, AOM_PLANE_Y);
    *var = cpi->ppi->fn_ptr[bsize].vf(
        x->plane[0].src.buf, x->plane[0].src.stride, xd->plane[0].dst.buf,
        xd->plane[0].dst.stride, sse);
  } else {
    // Manually convert from units of pixel to 1/8-pixels if we are not doing
    // subpel search
    convert_fullmv_to_mv(&best_mv);
    *var = best_mv_stats.distortion;
    *sse = best_mv_stats.sse;
  }

  return best_mv;
}
