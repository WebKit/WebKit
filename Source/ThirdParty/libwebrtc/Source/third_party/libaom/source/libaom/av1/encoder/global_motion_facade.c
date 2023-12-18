/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_dsp/binary_codes_writer.h"

#include "aom_dsp/flow_estimation/corner_detect.h"
#include "aom_dsp/flow_estimation/flow_estimation.h"
#include "aom_dsp/pyramid.h"
#include "av1/common/warped_motion.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/ethread.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/global_motion_facade.h"

// Range of model types to search
#define FIRST_GLOBAL_TRANS_TYPE ROTZOOM
#define LAST_GLOBAL_TRANS_TYPE ROTZOOM

// Computes the cost for the warp parameters.
static int gm_get_params_cost(const WarpedMotionParams *gm,
                              const WarpedMotionParams *ref_gm, int allow_hp) {
  int params_cost = 0;
  int trans_bits, trans_prec_diff;
  switch (gm->wmtype) {
    case AFFINE:
    case ROTZOOM:
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS),
          (gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[3] >> GM_ALPHA_PREC_DIFF),
          (gm->wmmat[3] >> GM_ALPHA_PREC_DIFF));
      if (gm->wmtype >= AFFINE) {
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[4] >> GM_ALPHA_PREC_DIFF),
            (gm->wmmat[4] >> GM_ALPHA_PREC_DIFF));
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS),
            (gm->wmmat[5] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      }
      AOM_FALLTHROUGH_INTENDED;
    case TRANSLATION:
      trans_bits = (gm->wmtype == TRANSLATION)
                       ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                       : GM_ABS_TRANS_BITS;
      trans_prec_diff = (gm->wmtype == TRANSLATION)
                            ? GM_TRANS_ONLY_PREC_DIFF + !allow_hp
                            : GM_TRANS_PREC_DIFF;
      params_cost += aom_count_signed_primitive_refsubexpfin(
          (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[0] >> trans_prec_diff),
          (gm->wmmat[0] >> trans_prec_diff));
      params_cost += aom_count_signed_primitive_refsubexpfin(
          (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[1] >> trans_prec_diff),
          (gm->wmmat[1] >> trans_prec_diff));
      AOM_FALLTHROUGH_INTENDED;
    case IDENTITY: break;
    default: assert(0);
  }
  return (params_cost << AV1_PROB_COST_SHIFT);
}

// For the given reference frame, computes the global motion parameters for
// different motion models and finds the best.
static AOM_INLINE void compute_global_motion_for_ref_frame(
    AV1_COMP *cpi, struct aom_internal_error_info *error_info,
    YV12_BUFFER_CONFIG *ref_buf[REF_FRAMES], int frame,
    MotionModel *motion_models, uint8_t *segment_map, const int segment_map_w,
    const int segment_map_h, const WarpedMotionParams *ref_params) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->td.mb.e_mbd;
  int src_width = cpi->source->y_crop_width;
  int src_height = cpi->source->y_crop_height;
  int src_stride = cpi->source->y_stride;
  assert(ref_buf[frame] != NULL);
  int bit_depth = cpi->common.seq_params->bit_depth;
  GlobalMotionMethod global_motion_method = default_global_motion_method;
  int num_refinements = cpi->sf.gm_sf.num_refinement_steps;
  bool mem_alloc_failed = false;

  // Select the best model based on fractional error reduction.
  // By initializing this to erroradv_tr, the same logic which is used to
  // select the best model will automatically filter out any model which
  // doesn't meet the required quality threshold
  double best_erroradv = erroradv_tr;
  for (TransformationType model = FIRST_GLOBAL_TRANS_TYPE;
       model <= LAST_GLOBAL_TRANS_TYPE; ++model) {
    if (!aom_compute_global_motion(
            model, cpi->source, ref_buf[frame], bit_depth, global_motion_method,
            motion_models, RANSAC_NUM_MOTIONS, &mem_alloc_failed)) {
      if (mem_alloc_failed) {
        aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                           "Failed to allocate global motion buffers");
      }
      continue;
    }

    for (int i = 0; i < RANSAC_NUM_MOTIONS; ++i) {
      if (motion_models[i].num_inliers == 0) continue;

      WarpedMotionParams tmp_wm_params;
      av1_convert_model_to_params(motion_models[i].params, &tmp_wm_params);

      // Skip models that we won't use (IDENTITY or TRANSLATION)
      //
      // For IDENTITY type models, we don't need to evaluate anything because
      // all the following logic is effectively comparing the estimated model
      // to an identity model.
      //
      // For TRANSLATION type global motion models, gm_get_motion_vector() gives
      // the wrong motion vector (see comments in that function for details).
      // As translation-type models do not give much gain, we can avoid this bug
      // by never choosing a TRANSLATION type model
      if (tmp_wm_params.wmtype <= TRANSLATION) continue;

      av1_compute_feature_segmentation_map(
          segment_map, segment_map_w, segment_map_h, motion_models[i].inliers,
          motion_models[i].num_inliers);

      int64_t ref_frame_error = av1_segmented_frame_error(
          is_cur_buf_hbd(xd), xd->bd, ref_buf[frame]->y_buffer,
          ref_buf[frame]->y_stride, cpi->source->y_buffer, src_stride,
          src_width, src_height, segment_map, segment_map_w);

      if (ref_frame_error == 0) continue;

      const int64_t warp_error = av1_refine_integerized_param(
          &tmp_wm_params, tmp_wm_params.wmtype, is_cur_buf_hbd(xd), xd->bd,
          ref_buf[frame]->y_buffer, ref_buf[frame]->y_crop_width,
          ref_buf[frame]->y_crop_height, ref_buf[frame]->y_stride,
          cpi->source->y_buffer, src_width, src_height, src_stride,
          num_refinements, ref_frame_error, segment_map, segment_map_w);

      // av1_refine_integerized_param() can return a simpler model type than
      // its input, so re-check model type here
      if (tmp_wm_params.wmtype <= TRANSLATION) continue;

      double erroradvantage = (double)warp_error / ref_frame_error;

      if (erroradvantage < best_erroradv) {
        best_erroradv = erroradvantage;
        // Save the wm_params modified by
        // av1_refine_integerized_param() rather than motion index to
        // avoid rerunning refine() below.
        memcpy(&(cm->global_motion[frame]), &tmp_wm_params,
               sizeof(WarpedMotionParams));
      }
    }
  }

  if (!av1_get_shear_params(&cm->global_motion[frame]))
    cm->global_motion[frame] = default_warp_params;

#if 0
  // We never choose translational models, so this code is disabled
  if (cm->global_motion[frame].wmtype == TRANSLATION) {
    cm->global_motion[frame].wmmat[0] =
        convert_to_trans_prec(cm->features.allow_high_precision_mv,
                              cm->global_motion[frame].wmmat[0]) *
        GM_TRANS_ONLY_DECODE_FACTOR;
    cm->global_motion[frame].wmmat[1] =
        convert_to_trans_prec(cm->features.allow_high_precision_mv,
                              cm->global_motion[frame].wmmat[1]) *
        GM_TRANS_ONLY_DECODE_FACTOR;
  }
#endif

  if (cm->global_motion[frame].wmtype == IDENTITY) return;

  // If the best error advantage found doesn't meet the threshold for
  // this motion type, revert to IDENTITY.
  if (!av1_is_enough_erroradvantage(
          best_erroradv,
          gm_get_params_cost(&cm->global_motion[frame], ref_params,
                             cm->features.allow_high_precision_mv))) {
    cm->global_motion[frame] = default_warp_params;
  }
}

// Computes global motion for the given reference frame.
void av1_compute_gm_for_valid_ref_frames(
    AV1_COMP *cpi, struct aom_internal_error_info *error_info,
    YV12_BUFFER_CONFIG *ref_buf[REF_FRAMES], int frame,
    MotionModel *motion_models, uint8_t *segment_map, int segment_map_w,
    int segment_map_h) {
  AV1_COMMON *const cm = &cpi->common;
  const WarpedMotionParams *ref_params =
      cm->prev_frame ? &cm->prev_frame->global_motion[frame]
                     : &default_warp_params;

  compute_global_motion_for_ref_frame(cpi, error_info, ref_buf, frame,
                                      motion_models, segment_map, segment_map_w,
                                      segment_map_h, ref_params);
}

// Loops over valid reference frames and computes global motion estimation.
static AOM_INLINE void compute_global_motion_for_references(
    AV1_COMP *cpi, YV12_BUFFER_CONFIG *ref_buf[REF_FRAMES],
    FrameDistPair reference_frame[REF_FRAMES - 1], int num_ref_frames,
    MotionModel *motion_models, uint8_t *segment_map, const int segment_map_w,
    const int segment_map_h) {
  AV1_COMMON *const cm = &cpi->common;
  struct aom_internal_error_info *const error_info =
      cpi->td.mb.e_mbd.error_info;
  // Compute global motion w.r.t. reference frames starting from the nearest ref
  // frame in a given direction.
  for (int frame = 0; frame < num_ref_frames; frame++) {
    int ref_frame = reference_frame[frame].frame;
    av1_compute_gm_for_valid_ref_frames(cpi, error_info, ref_buf, ref_frame,
                                        motion_models, segment_map,
                                        segment_map_w, segment_map_h);
    // If global motion w.r.t. current ref frame is
    // INVALID/TRANSLATION/IDENTITY, skip the evaluation of global motion w.r.t
    // the remaining ref frames in that direction.
    if (cpi->sf.gm_sf.prune_ref_frame_for_gm_search &&
        cm->global_motion[ref_frame].wmtype <= TRANSLATION)
      break;
  }
}

// Compares the distance in 'a' and 'b'. Returns 1 if the frame corresponding to
// 'a' is farther, -1 if the frame corresponding to 'b' is farther, 0 otherwise.
static int compare_distance(const void *a, const void *b) {
  const int diff =
      ((FrameDistPair *)a)->distance - ((FrameDistPair *)b)->distance;
  if (diff > 0)
    return 1;
  else if (diff < 0)
    return -1;
  return 0;
}

static int disable_gm_search_based_on_stats(const AV1_COMP *const cpi) {
  int is_gm_present = 1;

  // Check number of GM models only in GF groups with ARF frames. GM param
  // estimation is always done in the case of GF groups with no ARF frames (flat
  // gops)
  if (cpi->ppi->gf_group.arf_index > -1) {
    // valid_gm_model_found is initialized to INT32_MAX in the beginning of
    // every GF group.
    // Therefore, GM param estimation is always done for all frames until
    // at least 1 frame each of ARF_UPDATE, INTNL_ARF_UPDATE and LF_UPDATE are
    // encoded in a GF group For subsequent frames, GM param estimation is
    // disabled, if no valid models have been found in all the three update
    // types.
    is_gm_present = (cpi->ppi->valid_gm_model_found[ARF_UPDATE] != 0) ||
                    (cpi->ppi->valid_gm_model_found[INTNL_ARF_UPDATE] != 0) ||
                    (cpi->ppi->valid_gm_model_found[LF_UPDATE] != 0);
  }
  return !is_gm_present;
}

// Prunes reference frames for global motion estimation based on the speed
// feature 'gm_search_type'.
static int do_gm_search_logic(SPEED_FEATURES *const sf, int frame) {
  (void)frame;
  switch (sf->gm_sf.gm_search_type) {
    case GM_FULL_SEARCH: return 1;
    case GM_REDUCED_REF_SEARCH_SKIP_L2_L3:
      return !(frame == LAST2_FRAME || frame == LAST3_FRAME);
    case GM_REDUCED_REF_SEARCH_SKIP_L2_L3_ARF2:
      return !(frame == LAST2_FRAME || frame == LAST3_FRAME ||
               (frame == ALTREF2_FRAME));
    case GM_SEARCH_CLOSEST_REFS_ONLY: return 1;
    case GM_DISABLE_SEARCH: return 0;
    default: assert(0);
  }
  return 1;
}

// Populates valid reference frames in past/future directions in
// 'reference_frames' and their count in 'num_ref_frames'.
static AOM_INLINE void update_valid_ref_frames_for_gm(
    AV1_COMP *cpi, YV12_BUFFER_CONFIG *ref_buf[REF_FRAMES],
    FrameDistPair reference_frames[MAX_DIRECTIONS][REF_FRAMES - 1],
    int *num_ref_frames) {
  AV1_COMMON *const cm = &cpi->common;
  int *num_past_ref_frames = &num_ref_frames[0];
  int *num_future_ref_frames = &num_ref_frames[1];
  const GF_GROUP *gf_group = &cpi->ppi->gf_group;
  int ref_pruning_enabled = is_frame_eligible_for_ref_pruning(
      gf_group, cpi->sf.inter_sf.selective_ref_frame, 1, cpi->gf_frame_index);
  int cur_frame_gm_disabled = 0;
  int pyr_lvl = cm->cur_frame->pyramid_level;

  if (cpi->sf.gm_sf.disable_gm_search_based_on_stats) {
    cur_frame_gm_disabled = disable_gm_search_based_on_stats(cpi);
  }

  for (int frame = ALTREF_FRAME; frame >= LAST_FRAME; --frame) {
    const MV_REFERENCE_FRAME ref_frame[2] = { frame, NONE_FRAME };
    RefCntBuffer *buf = get_ref_frame_buf(cm, frame);
    const int ref_disabled =
        !(cpi->ref_frame_flags & av1_ref_frame_flag_list[frame]);
    ref_buf[frame] = NULL;
    cm->global_motion[frame] = default_warp_params;
    // Skip global motion estimation for invalid ref frames
    if (buf == NULL ||
        (ref_disabled && cpi->sf.hl_sf.recode_loop != DISALLOW_RECODE)) {
      continue;
    } else {
      ref_buf[frame] = &buf->buf;
    }

    int prune_ref_frames =
        ref_pruning_enabled &&
        prune_ref_by_selective_ref_frame(cpi, NULL, ref_frame,
                                         cm->cur_frame->ref_display_order_hint);
    int ref_pyr_lvl = buf->pyramid_level;

    if (ref_buf[frame]->y_crop_width == cpi->source->y_crop_width &&
        ref_buf[frame]->y_crop_height == cpi->source->y_crop_height &&
        do_gm_search_logic(&cpi->sf, frame) && !prune_ref_frames &&
        ref_pyr_lvl <= pyr_lvl && !cur_frame_gm_disabled) {
      assert(ref_buf[frame] != NULL);
      const int relative_frame_dist = av1_encoder_get_relative_dist(
          buf->display_order_hint, cm->cur_frame->display_order_hint);
      // Populate past and future ref frames.
      // reference_frames[0][] indicates past direction and
      // reference_frames[1][] indicates future direction.
      if (relative_frame_dist == 0) {
        // Skip global motion estimation for frames at the same nominal instant.
        // This will generally be either a "real" frame coded against a
        // temporal filtered version, or a higher spatial layer coded against
        // a lower spatial layer. In either case, the optimal motion model will
        // be IDENTITY, so we don't need to search explicitly.
      } else if (relative_frame_dist < 0) {
        reference_frames[0][*num_past_ref_frames].distance =
            abs(relative_frame_dist);
        reference_frames[0][*num_past_ref_frames].frame = frame;
        (*num_past_ref_frames)++;
      } else {
        reference_frames[1][*num_future_ref_frames].distance =
            abs(relative_frame_dist);
        reference_frames[1][*num_future_ref_frames].frame = frame;
        (*num_future_ref_frames)++;
      }
    }
  }
}

// Initializes parameters used for computing global motion.
static AOM_INLINE void setup_global_motion_info_params(AV1_COMP *cpi) {
  GlobalMotionInfo *const gm_info = &cpi->gm_info;
  YV12_BUFFER_CONFIG *source = cpi->source;

  gm_info->segment_map_w =
      (source->y_crop_width + WARP_ERROR_BLOCK - 1) >> WARP_ERROR_BLOCK_LOG;
  gm_info->segment_map_h =
      (source->y_crop_height + WARP_ERROR_BLOCK - 1) >> WARP_ERROR_BLOCK_LOG;

  memset(gm_info->reference_frames, -1,
         sizeof(gm_info->reference_frames[0][0]) * MAX_DIRECTIONS *
             (REF_FRAMES - 1));
  av1_zero(gm_info->num_ref_frames);

  // Populate ref_buf for valid ref frames in global motion
  update_valid_ref_frames_for_gm(cpi, gm_info->ref_buf,
                                 gm_info->reference_frames,
                                 gm_info->num_ref_frames);

  // Sort the past and future ref frames in the ascending order of their
  // distance from the current frame. reference_frames[0] => past direction
  // and reference_frames[1] => future direction.
  qsort(gm_info->reference_frames[0], gm_info->num_ref_frames[0],
        sizeof(gm_info->reference_frames[0][0]), compare_distance);
  qsort(gm_info->reference_frames[1], gm_info->num_ref_frames[1],
        sizeof(gm_info->reference_frames[1][0]), compare_distance);

  if (cpi->sf.gm_sf.gm_search_type == GM_SEARCH_CLOSEST_REFS_ONLY) {
    // Filter down to the nearest two ref frames.
    // Prefer one past and one future ref over two past refs, even if
    // the second past ref is closer
    if (gm_info->num_ref_frames[1] > 0) {
      gm_info->num_ref_frames[0] = AOMMIN(gm_info->num_ref_frames[0], 1);
      gm_info->num_ref_frames[1] = AOMMIN(gm_info->num_ref_frames[1], 1);
    } else {
      gm_info->num_ref_frames[0] = AOMMIN(gm_info->num_ref_frames[0], 2);
    }
  }
}

// Computes global motion w.r.t. valid reference frames.
static AOM_INLINE void global_motion_estimation(AV1_COMP *cpi) {
  GlobalMotionInfo *const gm_info = &cpi->gm_info;
  GlobalMotionData *gm_data = &cpi->td.gm_data;

  // Compute global motion w.r.t. past reference frames and future reference
  // frames
  for (int dir = 0; dir < MAX_DIRECTIONS; dir++) {
    if (gm_info->num_ref_frames[dir] > 0)
      compute_global_motion_for_references(
          cpi, gm_info->ref_buf, gm_info->reference_frames[dir],
          gm_info->num_ref_frames[dir], gm_data->motion_models,
          gm_data->segment_map, gm_info->segment_map_w, gm_info->segment_map_h);
  }
}

// Global motion estimation for the current frame is computed.This computation
// happens once per frame and the winner motion model parameters are stored in
// cm->cur_frame->global_motion.
void av1_compute_global_motion_facade(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  GlobalMotionInfo *const gm_info = &cpi->gm_info;

  if (cpi->oxcf.tool_cfg.enable_global_motion) {
    if (cpi->gf_frame_index == 0) {
      for (int i = 0; i < FRAME_UPDATE_TYPES; i++) {
        cpi->ppi->valid_gm_model_found[i] = INT32_MAX;
#if CONFIG_FPMT_TEST
        if (cpi->ppi->fpmt_unit_test_cfg == PARALLEL_SIMULATION_ENCODE)
          cpi->ppi->temp_valid_gm_model_found[i] = INT32_MAX;
#endif
      }
    }
  }

  if (cpi->common.current_frame.frame_type == INTER_FRAME && cpi->source &&
      cpi->oxcf.tool_cfg.enable_global_motion && !gm_info->search_done) {
    setup_global_motion_info_params(cpi);
    gm_alloc_data(cpi, &cpi->td.gm_data);
    if (cpi->mt_info.num_workers > 1)
      av1_global_motion_estimation_mt(cpi);
    else
      global_motion_estimation(cpi);
    gm_dealloc_data(&cpi->td.gm_data);
    gm_info->search_done = 1;
  }
  memcpy(cm->cur_frame->global_motion, cm->global_motion,
         sizeof(cm->cur_frame->global_motion));
}
