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
#include <limits.h>

#include "config/aom_scale_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/psnr.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"

#include "av1/common/av1_common_int.h"
#include "av1/common/av1_loopfilter.h"
#include "av1/common/quant_common.h"

#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/picklpf.h"

static void yv12_copy_plane(const YV12_BUFFER_CONFIG *src_bc,
                            YV12_BUFFER_CONFIG *dst_bc, int plane) {
  switch (plane) {
    case 0: aom_yv12_copy_y(src_bc, dst_bc); break;
    case 1: aom_yv12_copy_u(src_bc, dst_bc); break;
    case 2: aom_yv12_copy_v(src_bc, dst_bc); break;
    default: assert(plane >= 0 && plane <= 2); break;
  }
}

int av1_get_max_filter_level(const AV1_COMP *cpi) {
  if (is_stat_consumption_stage_twopass(cpi)) {
    return cpi->ppi->twopass.section_intra_rating > 8 ? MAX_LOOP_FILTER * 3 / 4
                                                      : MAX_LOOP_FILTER;
  } else {
    return MAX_LOOP_FILTER;
  }
}

static int64_t try_filter_frame(const YV12_BUFFER_CONFIG *sd,
                                AV1_COMP *const cpi, int filt_level,
                                int partial_frame, int plane, int dir) {
  MultiThreadInfo *const mt_info = &cpi->mt_info;
  int num_workers = mt_info->num_mod_workers[MOD_LPF];
  AV1_COMMON *const cm = &cpi->common;
  int64_t filt_err;

  assert(plane >= 0 && plane <= 2);
  int filter_level[2] = { filt_level, filt_level };
  if (plane == 0 && dir == 0) filter_level[1] = cm->lf.filter_level[1];
  if (plane == 0 && dir == 1) filter_level[0] = cm->lf.filter_level[0];

  // set base filters for use of av1_get_filter_level when in DELTA_LF mode
  switch (plane) {
    case 0:
      cm->lf.filter_level[0] = filter_level[0];
      cm->lf.filter_level[1] = filter_level[1];
      break;
    case 1: cm->lf.filter_level_u = filter_level[0]; break;
    case 2: cm->lf.filter_level_v = filter_level[0]; break;
  }

  // lpf_opt_level = 1 : Enables dual/quad loop-filtering.
  int lpf_opt_level = is_inter_tx_size_search_level_one(&cpi->sf.tx_sf);

  av1_loop_filter_frame_mt(&cm->cur_frame->buf, cm, &cpi->td.mb.e_mbd, plane,
                           plane + 1, partial_frame, mt_info->workers,
                           num_workers, &mt_info->lf_row_sync, lpf_opt_level);

  filt_err = aom_get_sse_plane(sd, &cm->cur_frame->buf, plane,
                               cm->seq_params->use_highbitdepth);

  // Re-instate the unfiltered frame
  yv12_copy_plane(&cpi->last_frame_uf, &cm->cur_frame->buf, plane);

  return filt_err;
}

static int search_filter_level(const YV12_BUFFER_CONFIG *sd, AV1_COMP *cpi,
                               int partial_frame,
                               const int *last_frame_filter_level, int plane,
                               int dir) {
  const AV1_COMMON *const cm = &cpi->common;
  const int min_filter_level = 0;
  const int max_filter_level = av1_get_max_filter_level(cpi);
  int filt_direction = 0;
  int64_t best_err;
  int filt_best;

  // Start the search at the previous frame filter level unless it is now out of
  // range.
  int lvl;
  switch (plane) {
    case 0:
      switch (dir) {
        case 2:
          lvl = (last_frame_filter_level[0] + last_frame_filter_level[1] + 1) >>
                1;
          break;
        case 0:
        case 1: lvl = last_frame_filter_level[dir]; break;
        default: assert(dir >= 0 && dir <= 2); return 0;
      }
      break;
    case 1: lvl = last_frame_filter_level[2]; break;
    case 2: lvl = last_frame_filter_level[3]; break;
    default: assert(plane >= 0 && plane <= 2); return 0;
  }
  int filt_mid = clamp(lvl, min_filter_level, max_filter_level);
  int filter_step = filt_mid < 16 ? 4 : filt_mid / 4;
  // Sum squared error at each filter level
  int64_t ss_err[MAX_LOOP_FILTER + 1];

  const int use_coarse_search = cpi->sf.lpf_sf.use_coarse_filter_level_search;
  assert(use_coarse_search <= 1);
  static const int min_filter_step_lookup[2] = { 0, 2 };
  // min_filter_step_thesh determines the stopping criteria for the search.
  // The search is terminated when filter_step equals min_filter_step_thesh.
  const int min_filter_step_thesh = min_filter_step_lookup[use_coarse_search];

  // Set each entry to -1
  memset(ss_err, 0xFF, sizeof(ss_err));
  yv12_copy_plane(&cm->cur_frame->buf, &cpi->last_frame_uf, plane);
  best_err = try_filter_frame(sd, cpi, filt_mid, partial_frame, plane, dir);
  filt_best = filt_mid;
  ss_err[filt_mid] = best_err;

  while (filter_step > min_filter_step_thesh) {
    const int filt_high = AOMMIN(filt_mid + filter_step, max_filter_level);
    const int filt_low = AOMMAX(filt_mid - filter_step, min_filter_level);

    // Bias against raising loop filter in favor of lowering it.
    int64_t bias = (best_err >> (15 - (filt_mid / 8))) * filter_step;

    if ((is_stat_consumption_stage_twopass(cpi)) &&
        (cpi->ppi->twopass.section_intra_rating < 20))
      bias = (bias * cpi->ppi->twopass.section_intra_rating) / 20;

    // yx, bias less for large block size
    if (cm->features.tx_mode != ONLY_4X4) bias >>= 1;

    if (filt_direction <= 0 && filt_low != filt_mid) {
      // Get Low filter error score
      if (ss_err[filt_low] < 0) {
        ss_err[filt_low] =
            try_filter_frame(sd, cpi, filt_low, partial_frame, plane, dir);
      }
      // If value is close to the best so far then bias towards a lower loop
      // filter value.
      if (ss_err[filt_low] < (best_err + bias)) {
        // Was it actually better than the previous best?
        if (ss_err[filt_low] < best_err) {
          best_err = ss_err[filt_low];
        }
        filt_best = filt_low;
      }
    }

    // Now look at filt_high
    if (filt_direction >= 0 && filt_high != filt_mid) {
      if (ss_err[filt_high] < 0) {
        ss_err[filt_high] =
            try_filter_frame(sd, cpi, filt_high, partial_frame, plane, dir);
      }
      // If value is significantly better than previous best, bias added against
      // raising filter value
      if (ss_err[filt_high] < (best_err - bias)) {
        best_err = ss_err[filt_high];
        filt_best = filt_high;
      }
    }

    // Half the step distance if the best filter value was the same as last time
    if (filt_best == filt_mid) {
      filter_step /= 2;
      filt_direction = 0;
    } else {
      filt_direction = (filt_best < filt_mid) ? -1 : 1;
      filt_mid = filt_best;
    }
  }

  return filt_best;
}

void av1_pick_filter_level(const YV12_BUFFER_CONFIG *sd, AV1_COMP *cpi,
                           LPF_PICK_METHOD method) {
  AV1_COMMON *const cm = &cpi->common;
  const SequenceHeader *const seq_params = cm->seq_params;
  const int num_planes = av1_num_planes(cm);
  struct loopfilter *const lf = &cm->lf;
  int disable_filter_rt_screen = 0;
  (void)sd;

  lf->sharpness_level = 0;
  cpi->td.mb.rdmult = cpi->rd.RDMULT;

  if (cpi->oxcf.tune_cfg.content == AOM_CONTENT_SCREEN &&
      cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ &&
      cpi->sf.rt_sf.skip_lf_screen)
    disable_filter_rt_screen = av1_cyclic_refresh_disable_lf_cdef(cpi);

  if (disable_filter_rt_screen ||
      cpi->oxcf.algo_cfg.loopfilter_control == LOOPFILTER_NONE ||
      (cpi->oxcf.algo_cfg.loopfilter_control == LOOPFILTER_REFERENCE &&
       cpi->svc.non_reference_frame)) {
    lf->filter_level[0] = 0;
    lf->filter_level[1] = 0;
    return;
  }

  if (method == LPF_PICK_MINIMAL_LPF) {
    lf->filter_level[0] = 0;
    lf->filter_level[1] = 0;
  } else if (method >= LPF_PICK_FROM_Q) {
    const int min_filter_level = 0;
    const int max_filter_level = av1_get_max_filter_level(cpi);
    const int q = av1_ac_quant_QTX(cm->quant_params.base_qindex, 0,
                                   seq_params->bit_depth);
    // based on tests result for rtc test set
    // 0.04590 boosted or 0.02295 non-booseted in 18-bit fixed point
    const int strength_boost_q_treshold = 0;
    int inter_frame_multiplier =
        (q > strength_boost_q_treshold ||
         (cpi->sf.rt_sf.use_nonrd_pick_mode &&
          cpi->common.width * cpi->common.height > 352 * 288))
            ? 12034
            : 6017;
    // These values were determined by linear fitting the result of the
    // searched level for 8 bit depth:
    // Keyframes: filt_guess = q * 0.06699 - 1.60817
    // Other frames: filt_guess = q * inter_frame_multiplier + 2.48225
    //
    // And high bit depth separately:
    // filt_guess = q * 0.316206 + 3.87252
    int filt_guess;
    switch (seq_params->bit_depth) {
      case AOM_BITS_8:
        filt_guess =
            (cm->current_frame.frame_type == KEY_FRAME)
                ? ROUND_POWER_OF_TWO(q * 17563 - 421574, 18)
                : ROUND_POWER_OF_TWO(q * inter_frame_multiplier + 650707, 18);
        break;
      case AOM_BITS_10:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 4060632, 20);
        break;
      case AOM_BITS_12:
        filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 16242526, 22);
        break;
      default:
        assert(0 &&
               "bit_depth should be AOM_BITS_8, AOM_BITS_10 "
               "or AOM_BITS_12");
        return;
    }
    if (seq_params->bit_depth != AOM_BITS_8 &&
        cm->current_frame.frame_type == KEY_FRAME)
      filt_guess -= 4;
    // TODO(chengchen): retrain the model for Y, U, V filter levels
    lf->filter_level[0] = clamp(filt_guess, min_filter_level, max_filter_level);
    lf->filter_level[1] = clamp(filt_guess, min_filter_level, max_filter_level);
    lf->filter_level_u = clamp(filt_guess, min_filter_level, max_filter_level);
    lf->filter_level_v = clamp(filt_guess, min_filter_level, max_filter_level);
    if (cpi->oxcf.algo_cfg.loopfilter_control == LOOPFILTER_SELECTIVELY &&
        !frame_is_intra_only(cm) && !cpi->rc.high_source_sad) {
      if (cpi->oxcf.tune_cfg.content == AOM_CONTENT_SCREEN) {
        lf->filter_level[0] = 0;
        lf->filter_level[1] = 0;
      } else {
        const int num4x4 = (cm->width >> 2) * (cm->height >> 2);
        const int newmv_thresh = 7;
        const int distance_since_key_thresh = 5;
        if ((cpi->td.rd_counts.newmv_or_intra_blocks * 100 / num4x4) <
                newmv_thresh &&
            cpi->rc.frames_since_key > distance_since_key_thresh) {
          lf->filter_level[0] = 0;
          lf->filter_level[1] = 0;
        }
      }
    }
  } else {
    int last_frame_filter_level[4] = { 0 };
    if (!frame_is_intra_only(cm)) {
      last_frame_filter_level[0] = cpi->ppi->filter_level[0];
      last_frame_filter_level[1] = cpi->ppi->filter_level[1];
      last_frame_filter_level[2] = cpi->ppi->filter_level_u;
      last_frame_filter_level[3] = cpi->ppi->filter_level_v;
    }
    // The frame buffer last_frame_uf is used to store the non-loop filtered
    // reconstructed frame in search_filter_level().
    if (aom_realloc_frame_buffer(
            &cpi->last_frame_uf, cm->width, cm->height,
            seq_params->subsampling_x, seq_params->subsampling_y,
            seq_params->use_highbitdepth, cpi->oxcf.border_in_pixels,
            cm->features.byte_alignment, NULL, NULL, NULL, 0, 0))
      aom_internal_error(cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate last frame buffer");

    lf->filter_level[0] = lf->filter_level[1] =
        search_filter_level(sd, cpi, method == LPF_PICK_FROM_SUBIMAGE,
                            last_frame_filter_level, 0, 2);
    if (method != LPF_PICK_FROM_FULL_IMAGE_NON_DUAL) {
      lf->filter_level[0] =
          search_filter_level(sd, cpi, method == LPF_PICK_FROM_SUBIMAGE,
                              last_frame_filter_level, 0, 0);
      lf->filter_level[1] =
          search_filter_level(sd, cpi, method == LPF_PICK_FROM_SUBIMAGE,
                              last_frame_filter_level, 0, 1);
    }

    if (num_planes > 1) {
      lf->filter_level_u =
          search_filter_level(sd, cpi, method == LPF_PICK_FROM_SUBIMAGE,
                              last_frame_filter_level, 1, 0);
      lf->filter_level_v =
          search_filter_level(sd, cpi, method == LPF_PICK_FROM_SUBIMAGE,
                              last_frame_filter_level, 2, 0);
    }
  }
}
