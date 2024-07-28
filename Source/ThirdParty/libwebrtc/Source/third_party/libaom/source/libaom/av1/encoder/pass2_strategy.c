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

/*!\defgroup gf_group_algo Golden Frame Group
 * \ingroup high_level_algo
 * Algorithms regarding determining the length of GF groups and defining GF
 * group structures.
 * @{
 */
/*! @} - end defgroup gf_group_algo */

#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "config/aom_config.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_codec.h"
#include "aom/aom_encoder.h"

#include "av1/common/av1_common_int.h"

#include "av1/encoder/encoder.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/gop_structure.h"
#include "av1/encoder/pass2_strategy.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rc_utils.h"
#include "av1/encoder/temporal_filter.h"
#include "av1/encoder/thirdpass.h"
#include "av1/encoder/tpl_model.h"
#include "av1/encoder/encode_strategy.h"

#define DEFAULT_KF_BOOST 2300
#define DEFAULT_GF_BOOST 2000
#define GROUP_ADAPTIVE_MAXQ 1

static void init_gf_stats(GF_GROUP_STATS *gf_stats);
static int define_gf_group_pass3(AV1_COMP *cpi, EncodeFrameParams *frame_params,
                                 int is_final_pass);

// Calculate an active area of the image that discounts formatting
// bars and partially discounts other 0 energy areas.
#define MIN_ACTIVE_AREA 0.5
#define MAX_ACTIVE_AREA 1.0
static double calculate_active_area(const FRAME_INFO *frame_info,
                                    const FIRSTPASS_STATS *this_frame) {
  const double active_pct =
      1.0 -
      ((this_frame->intra_skip_pct / 2) +
       ((this_frame->inactive_zone_rows * 2) / (double)frame_info->mb_rows));
  return fclamp(active_pct, MIN_ACTIVE_AREA, MAX_ACTIVE_AREA);
}

// Calculate a modified Error used in distributing bits between easier and
// harder frames.
#define ACT_AREA_CORRECTION 0.5
static double calculate_modified_err_new(const FRAME_INFO *frame_info,
                                         const FIRSTPASS_STATS *total_stats,
                                         const FIRSTPASS_STATS *this_stats,
                                         int vbrbias, double modified_error_min,
                                         double modified_error_max) {
  if (total_stats == NULL) {
    return 0;
  }
  const double av_weight = total_stats->weight / total_stats->count;
  const double av_err =
      (total_stats->coded_error * av_weight) / total_stats->count;
  double modified_error =
      av_err * pow(this_stats->coded_error * this_stats->weight /
                       DOUBLE_DIVIDE_CHECK(av_err),
                   vbrbias / 100.0);

  // Correction for active area. Frames with a reduced active area
  // (eg due to formatting bars) have a higher error per mb for the
  // remaining active MBs. The correction here assumes that coding
  // 0.5N blocks of complexity 2X is a little easier than coding N
  // blocks of complexity X.
  modified_error *=
      pow(calculate_active_area(frame_info, this_stats), ACT_AREA_CORRECTION);

  return fclamp(modified_error, modified_error_min, modified_error_max);
}

static double calculate_modified_err(const FRAME_INFO *frame_info,
                                     const TWO_PASS *twopass,
                                     const AV1EncoderConfig *oxcf,
                                     const FIRSTPASS_STATS *this_frame) {
  const FIRSTPASS_STATS *total_stats = twopass->stats_buf_ctx->total_stats;
  return calculate_modified_err_new(
      frame_info, total_stats, this_frame, oxcf->rc_cfg.vbrbias,
      twopass->modified_error_min, twopass->modified_error_max);
}

// Resets the first pass file to the given position using a relative seek from
// the current position.
static void reset_fpf_position(TWO_PASS_FRAME *p_frame,
                               const FIRSTPASS_STATS *position) {
  p_frame->stats_in = position;
}

static int input_stats(TWO_PASS *p, TWO_PASS_FRAME *p_frame,
                       FIRSTPASS_STATS *fps) {
  if (p_frame->stats_in >= p->stats_buf_ctx->stats_in_end) return EOF;

  *fps = *p_frame->stats_in;
  ++p_frame->stats_in;
  return 1;
}

static int input_stats_lap(TWO_PASS *p, TWO_PASS_FRAME *p_frame,
                           FIRSTPASS_STATS *fps) {
  if (p_frame->stats_in >= p->stats_buf_ctx->stats_in_end) return EOF;

  *fps = *p_frame->stats_in;
  /* Move old stats[0] out to accommodate for next frame stats  */
  memmove(p->frame_stats_arr[0], p->frame_stats_arr[1],
          (p->stats_buf_ctx->stats_in_end - p_frame->stats_in - 1) *
              sizeof(FIRSTPASS_STATS));
  p->stats_buf_ctx->stats_in_end--;
  return 1;
}

// Read frame stats at an offset from the current position.
static const FIRSTPASS_STATS *read_frame_stats(const TWO_PASS *p,
                                               const TWO_PASS_FRAME *p_frame,
                                               int offset) {
  if ((offset >= 0 &&
       p_frame->stats_in + offset >= p->stats_buf_ctx->stats_in_end) ||
      (offset < 0 &&
       p_frame->stats_in + offset < p->stats_buf_ctx->stats_in_start)) {
    return NULL;
  }

  return &p_frame->stats_in[offset];
}

// This function returns the maximum target rate per frame.
static int frame_max_bits(const RATE_CONTROL *rc,
                          const AV1EncoderConfig *oxcf) {
  int64_t max_bits = ((int64_t)rc->avg_frame_bandwidth *
                      (int64_t)oxcf->rc_cfg.vbrmax_section) /
                     100;
  if (max_bits < 0)
    max_bits = 0;
  else if (max_bits > rc->max_frame_bandwidth)
    max_bits = rc->max_frame_bandwidth;

  return (int)max_bits;
}

// Based on history adjust expectations of bits per macroblock.
static void twopass_update_bpm_factor(AV1_COMP *cpi, int rate_err_tol) {
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  const PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;

  // Based on recent history adjust expectations of bits per macroblock.
  double rate_err_factor = 1.0;
  const double adj_limit = AOMMAX(0.2, (double)(100 - rate_err_tol) / 200.0);
  const double min_fac = 1.0 - adj_limit;
  const double max_fac = 1.0 + adj_limit;

  if (cpi->third_pass_ctx && cpi->third_pass_ctx->frame_info_count > 0) {
    int64_t actual_bits = 0;
    int64_t target_bits = 0;
    double factor = 0.0;
    int count = 0;
    for (int i = 0; i < cpi->third_pass_ctx->frame_info_count; i++) {
      actual_bits += cpi->third_pass_ctx->frame_info[i].actual_bits;
      target_bits += cpi->third_pass_ctx->frame_info[i].bits_allocated;
      factor += cpi->third_pass_ctx->frame_info[i].bpm_factor;
      count++;
    }

    if (count == 0) {
      factor = 1.0;
    } else {
      factor /= (double)count;
    }

    factor *= (double)actual_bits / DOUBLE_DIVIDE_CHECK((double)target_bits);

    if ((twopass->bpm_factor <= 1 && factor < twopass->bpm_factor) ||
        (twopass->bpm_factor >= 1 && factor > twopass->bpm_factor)) {
      twopass->bpm_factor = factor;
      twopass->bpm_factor =
          AOMMAX(min_fac, AOMMIN(max_fac, twopass->bpm_factor));
    }
  }

  int err_estimate = p_rc->rate_error_estimate;
  int64_t total_actual_bits = p_rc->total_actual_bits;
  double rolling_arf_group_actual_bits =
      (double)twopass->rolling_arf_group_actual_bits;
  double rolling_arf_group_target_bits =
      (double)twopass->rolling_arf_group_target_bits;

#if CONFIG_FPMT_TEST
  const int is_parallel_frame =
      cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0 ? 1 : 0;
  const int simulate_parallel_frame =
      cpi->ppi->fpmt_unit_test_cfg == PARALLEL_SIMULATION_ENCODE
          ? is_parallel_frame
          : 0;
  total_actual_bits = simulate_parallel_frame ? p_rc->temp_total_actual_bits
                                              : p_rc->total_actual_bits;
  rolling_arf_group_target_bits =
      (double)(simulate_parallel_frame
                   ? p_rc->temp_rolling_arf_group_target_bits
                   : twopass->rolling_arf_group_target_bits);
  rolling_arf_group_actual_bits =
      (double)(simulate_parallel_frame
                   ? p_rc->temp_rolling_arf_group_actual_bits
                   : twopass->rolling_arf_group_actual_bits);
  err_estimate = simulate_parallel_frame ? p_rc->temp_rate_error_estimate
                                         : p_rc->rate_error_estimate;
#endif

  if ((p_rc->bits_off_target && total_actual_bits > 0) &&
      (rolling_arf_group_target_bits >= 1.0)) {
    if (rolling_arf_group_actual_bits > rolling_arf_group_target_bits) {
      double error_fraction =
          (rolling_arf_group_actual_bits - rolling_arf_group_target_bits) /
          rolling_arf_group_target_bits;
      error_fraction = (error_fraction > 1.0) ? 1.0 : error_fraction;
      rate_err_factor = 1.0 + error_fraction;
    } else {
      double error_fraction =
          (rolling_arf_group_target_bits - rolling_arf_group_actual_bits) /
          rolling_arf_group_target_bits;
      rate_err_factor = 1.0 - error_fraction;
    }

    rate_err_factor = AOMMAX(min_fac, AOMMIN(max_fac, rate_err_factor));
  }

  // Is the rate control trending in the right direction. Only make
  // an adjustment if things are getting worse.
  if ((rate_err_factor < 1.0 && err_estimate >= 0) ||
      (rate_err_factor > 1.0 && err_estimate <= 0)) {
    twopass->bpm_factor *= rate_err_factor;
    twopass->bpm_factor = AOMMAX(min_fac, AOMMIN(max_fac, twopass->bpm_factor));
  }
}

static const double q_div_term[(QINDEX_RANGE >> 5) + 1] = { 32.0, 40.0, 46.0,
                                                            52.0, 56.0, 60.0,
                                                            64.0, 68.0, 72.0 };
#define EPMB_SCALER 1250000
static double calc_correction_factor(double err_per_mb, int q) {
  double power_term = 0.90;
  const int index = q >> 5;
  const double divisor =
      q_div_term[index] +
      (((q_div_term[index + 1] - q_div_term[index]) * (q % 32)) / 32.0);
  double error_term = EPMB_SCALER * pow(err_per_mb, power_term);
  return error_term / divisor;
}

// Similar to find_qindex_by_rate() function in ratectrl.c, but includes
// calculation of a correction_factor.
static int find_qindex_by_rate_with_correction(uint64_t desired_bits_per_mb,
                                               aom_bit_depth_t bit_depth,
                                               double error_per_mb,
                                               double group_weight_factor,
                                               int best_qindex,
                                               int worst_qindex) {
  assert(best_qindex <= worst_qindex);
  int low = best_qindex;
  int high = worst_qindex;

  while (low < high) {
    const int mid = (low + high) >> 1;
    const double q_factor = calc_correction_factor(error_per_mb, mid);
    const double q = av1_convert_qindex_to_q(mid, bit_depth);
    const uint64_t mid_bits_per_mb =
        (uint64_t)((q_factor * group_weight_factor) / q);

    if (mid_bits_per_mb > desired_bits_per_mb) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return low;
}

/*!\brief Choose a target maximum Q for a group of frames
 *
 * \ingroup rate_control
 *
 * This function is used to estimate a suitable maximum Q for a
 * group of frames. Inititally it is called to get a crude estimate
 * for the whole clip. It is then called for each ARF/GF group to get
 * a revised estimate for that group.
 *
 * \param[in]    cpi                 Top-level encoder structure
 * \param[in]    av_frame_err        The average per frame coded error score
 *                                   for frames making up this section/group.
 * \param[in]    inactive_zone       Used to mask off /ignore part of the
 *                                   frame. The most common use case is where
 *                                   a wide format video (e.g. 16:9) is
 *                                   letter-boxed into a more square format.
 *                                   Here we want to ignore the bands at the
 *                                   top and bottom.
 * \param[in]    av_target_bandwidth The target bits per frame
 *
 * \return The maximum Q for frames in the group.
 */
static int get_twopass_worst_quality(AV1_COMP *cpi, const double av_frame_err,
                                     double inactive_zone,
                                     int av_target_bandwidth) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;
  inactive_zone = fclamp(inactive_zone, 0.0, 0.9999);

  if (av_target_bandwidth <= 0) {
    return rc->worst_quality;  // Highest value allowed
  } else {
    const int num_mbs = (oxcf->resize_cfg.resize_mode != RESIZE_NONE)
                            ? cpi->initial_mbs
                            : cpi->common.mi_params.MBs;
    const int active_mbs = AOMMAX(1, num_mbs - (int)(num_mbs * inactive_zone));
    const double av_err_per_mb = av_frame_err / (1.0 - inactive_zone);
    const uint64_t target_norm_bits_per_mb =
        ((uint64_t)av_target_bandwidth << BPER_MB_NORMBITS) / active_mbs;
    int rate_err_tol = AOMMIN(rc_cfg->under_shoot_pct, rc_cfg->over_shoot_pct);

    // Update bpm correction factor based on previous GOP rate error.
    twopass_update_bpm_factor(cpi, rate_err_tol);

    // Try and pick a max Q that will be high enough to encode the
    // content at the given rate.
    int q = find_qindex_by_rate_with_correction(
        target_norm_bits_per_mb, cpi->common.seq_params->bit_depth,
        av_err_per_mb, cpi->ppi->twopass.bpm_factor, rc->best_quality,
        rc->worst_quality);

    // Restriction on active max q for constrained quality mode.
    if (rc_cfg->mode == AOM_CQ) q = AOMMAX(q, rc_cfg->cq_level);
    return q;
  }
}

#define INTRA_PART 0.005
#define DEFAULT_DECAY_LIMIT 0.75
#define LOW_SR_DIFF_TRHESH 0.01
#define NCOUNT_FRAME_II_THRESH 5.0
#define LOW_CODED_ERR_PER_MB 0.01

/* This function considers how the quality of prediction may be deteriorating
 * with distance. It comapres the coded error for the last frame and the
 * second reference frame (usually two frames old) and also applies a factor
 * based on the extent of INTRA coding.
 *
 * The decay factor is then used to reduce the contribution of frames further
 * from the alt-ref or golden frame, to the bitframe boost calculation for that
 * alt-ref or golden frame.
 */
static double get_sr_decay_rate(const FIRSTPASS_STATS *frame) {
  double sr_diff = (frame->sr_coded_error - frame->coded_error);
  double sr_decay = 1.0;
  double modified_pct_inter;
  double modified_pcnt_intra;

  modified_pct_inter = frame->pcnt_inter;
  if ((frame->coded_error > LOW_CODED_ERR_PER_MB) &&
      ((frame->intra_error / DOUBLE_DIVIDE_CHECK(frame->coded_error)) <
       (double)NCOUNT_FRAME_II_THRESH)) {
    modified_pct_inter = frame->pcnt_inter - frame->pcnt_neutral;
  }
  modified_pcnt_intra = 100 * (1.0 - modified_pct_inter);

  if ((sr_diff > LOW_SR_DIFF_TRHESH)) {
    double sr_diff_part = ((sr_diff * 0.25) / frame->intra_error);
    sr_decay = 1.0 - sr_diff_part - (INTRA_PART * modified_pcnt_intra);
  }
  return AOMMAX(sr_decay, DEFAULT_DECAY_LIMIT);
}

// This function gives an estimate of how badly we believe the prediction
// quality is decaying from frame to frame.
static double get_zero_motion_factor(const FIRSTPASS_STATS *frame) {
  const double zero_motion_pct = frame->pcnt_inter - frame->pcnt_motion;
  double sr_decay = get_sr_decay_rate(frame);
  return AOMMIN(sr_decay, zero_motion_pct);
}

#define DEFAULT_ZM_FACTOR 0.5
static double get_prediction_decay_rate(const FIRSTPASS_STATS *frame_stats) {
  const double sr_decay_rate = get_sr_decay_rate(frame_stats);
  double zero_motion_factor =
      DEFAULT_ZM_FACTOR * (frame_stats->pcnt_inter - frame_stats->pcnt_motion);

  // Clamp value to range 0.0 to 1.0
  // This should happen anyway if input values are sensibly clamped but checked
  // here just in case.
  if (zero_motion_factor > 1.0)
    zero_motion_factor = 1.0;
  else if (zero_motion_factor < 0.0)
    zero_motion_factor = 0.0;

  return AOMMAX(zero_motion_factor,
                (sr_decay_rate + ((1.0 - sr_decay_rate) * zero_motion_factor)));
}

// Function to test for a condition where a complex transition is followed
// by a static section. For example in slide shows where there is a fade
// between slides. This is to help with more optimal kf and gf positioning.
static int detect_transition_to_still(const FIRSTPASS_INFO *firstpass_info,
                                      int next_stats_index,
                                      const int min_gf_interval,
                                      const int frame_interval,
                                      const int still_interval,
                                      const double loop_decay_rate,
                                      const double last_decay_rate) {
  // Break clause to detect very still sections after motion
  // For example a static image after a fade or other transition
  // instead of a clean scene cut.
  if (frame_interval > min_gf_interval && loop_decay_rate >= 0.999 &&
      last_decay_rate < 0.9) {
    int stats_left =
        av1_firstpass_info_future_count(firstpass_info, next_stats_index);
    if (stats_left >= still_interval) {
      int j;
      // Look ahead a few frames to see if static condition persists...
      for (j = 0; j < still_interval; ++j) {
        const FIRSTPASS_STATS *stats =
            av1_firstpass_info_peek(firstpass_info, next_stats_index + j);
        if (stats->pcnt_inter - stats->pcnt_motion < 0.999) break;
      }
      // Only if it does do we signal a transition to still.
      return j == still_interval;
    }
  }
  return 0;
}

// This function detects a flash through the high relative pcnt_second_ref
// score in the frame following a flash frame. The offset passed in should
// reflect this.
static int detect_flash(const TWO_PASS *twopass,
                        const TWO_PASS_FRAME *twopass_frame, const int offset) {
  const FIRSTPASS_STATS *const next_frame =
      read_frame_stats(twopass, twopass_frame, offset);

  // What we are looking for here is a situation where there is a
  // brief break in prediction (such as a flash) but subsequent frames
  // are reasonably well predicted by an earlier (pre flash) frame.
  // The recovery after a flash is indicated by a high pcnt_second_ref
  // compared to pcnt_inter.
  return next_frame != NULL &&
         next_frame->pcnt_second_ref > next_frame->pcnt_inter &&
         next_frame->pcnt_second_ref >= 0.5;
}

// Update the motion related elements to the GF arf boost calculation.
static void accumulate_frame_motion_stats(const FIRSTPASS_STATS *stats,
                                          GF_GROUP_STATS *gf_stats, double f_w,
                                          double f_h) {
  const double pct = stats->pcnt_motion;

  // Accumulate Motion In/Out of frame stats.
  gf_stats->this_frame_mv_in_out = stats->mv_in_out_count * pct;
  gf_stats->mv_in_out_accumulator += gf_stats->this_frame_mv_in_out;
  gf_stats->abs_mv_in_out_accumulator += fabs(gf_stats->this_frame_mv_in_out);

  // Accumulate a measure of how uniform (or conversely how random) the motion
  // field is (a ratio of abs(mv) / mv).
  if (pct > 0.05) {
    const double mvr_ratio =
        fabs(stats->mvr_abs) / DOUBLE_DIVIDE_CHECK(fabs(stats->MVr));
    const double mvc_ratio =
        fabs(stats->mvc_abs) / DOUBLE_DIVIDE_CHECK(fabs(stats->MVc));

    gf_stats->mv_ratio_accumulator +=
        pct *
        (mvr_ratio < stats->mvr_abs * f_h ? mvr_ratio : stats->mvr_abs * f_h);
    gf_stats->mv_ratio_accumulator +=
        pct *
        (mvc_ratio < stats->mvc_abs * f_w ? mvc_ratio : stats->mvc_abs * f_w);
  }
}

static void accumulate_this_frame_stats(const FIRSTPASS_STATS *stats,
                                        const double mod_frame_err,
                                        GF_GROUP_STATS *gf_stats) {
  gf_stats->gf_group_err += mod_frame_err;
#if GROUP_ADAPTIVE_MAXQ
  gf_stats->gf_group_raw_error += stats->coded_error;
#endif
  gf_stats->gf_group_skip_pct += stats->intra_skip_pct;
  gf_stats->gf_group_inactive_zone_rows += stats->inactive_zone_rows;
}

static void accumulate_next_frame_stats(const FIRSTPASS_STATS *stats,
                                        const int flash_detected,
                                        const int frames_since_key,
                                        const int cur_idx,
                                        GF_GROUP_STATS *gf_stats, int f_w,
                                        int f_h) {
  accumulate_frame_motion_stats(stats, gf_stats, f_w, f_h);
  // sum up the metric values of current gf group
  gf_stats->avg_sr_coded_error += stats->sr_coded_error;
  gf_stats->avg_pcnt_second_ref += stats->pcnt_second_ref;
  gf_stats->avg_new_mv_count += stats->new_mv_count;
  gf_stats->avg_wavelet_energy += stats->frame_avg_wavelet_energy;
  if (fabs(stats->raw_error_stdev) > 0.000001) {
    gf_stats->non_zero_stdev_count++;
    gf_stats->avg_raw_err_stdev += stats->raw_error_stdev;
  }

  // Accumulate the effect of prediction quality decay
  if (!flash_detected) {
    gf_stats->last_loop_decay_rate = gf_stats->loop_decay_rate;
    gf_stats->loop_decay_rate = get_prediction_decay_rate(stats);

    gf_stats->decay_accumulator =
        gf_stats->decay_accumulator * gf_stats->loop_decay_rate;

    // Monitor for static sections.
    if ((frames_since_key + cur_idx - 1) > 1) {
      gf_stats->zero_motion_accumulator = AOMMIN(
          gf_stats->zero_motion_accumulator, get_zero_motion_factor(stats));
    }
  }
}

static void average_gf_stats(const int total_frame, GF_GROUP_STATS *gf_stats) {
  if (total_frame) {
    gf_stats->avg_sr_coded_error /= total_frame;
    gf_stats->avg_pcnt_second_ref /= total_frame;
    gf_stats->avg_new_mv_count /= total_frame;
    gf_stats->avg_wavelet_energy /= total_frame;
  }

  if (gf_stats->non_zero_stdev_count)
    gf_stats->avg_raw_err_stdev /= gf_stats->non_zero_stdev_count;
}

#define BOOST_FACTOR 12.5
static double baseline_err_per_mb(const FRAME_INFO *frame_info) {
  unsigned int screen_area = frame_info->frame_height * frame_info->frame_width;

  // Use a different error per mb factor for calculating boost for
  //  different formats.
  if (screen_area <= 640 * 360) {
    return 500.0;
  } else {
    return 1000.0;
  }
}

static double calc_frame_boost(const PRIMARY_RATE_CONTROL *p_rc,
                               const FRAME_INFO *frame_info,
                               const FIRSTPASS_STATS *this_frame,
                               double this_frame_mv_in_out, double max_boost) {
  double frame_boost;
  const double lq = av1_convert_qindex_to_q(p_rc->avg_frame_qindex[INTER_FRAME],
                                            frame_info->bit_depth);
  const double boost_q_correction = AOMMIN((0.5 + (lq * 0.015)), 1.5);
  const double active_area = calculate_active_area(frame_info, this_frame);

  // Underlying boost factor is based on inter error ratio.
  frame_boost = AOMMAX(baseline_err_per_mb(frame_info) * active_area,
                       this_frame->intra_error * active_area) /
                DOUBLE_DIVIDE_CHECK(this_frame->coded_error);
  frame_boost = frame_boost * BOOST_FACTOR * boost_q_correction;

  // Increase boost for frames where new data coming into frame (e.g. zoom out).
  // Slightly reduce boost if there is a net balance of motion out of the frame
  // (zoom in). The range for this_frame_mv_in_out is -1.0 to +1.0.
  if (this_frame_mv_in_out > 0.0)
    frame_boost += frame_boost * (this_frame_mv_in_out * 2.0);
  // In the extreme case the boost is halved.
  else
    frame_boost += frame_boost * (this_frame_mv_in_out / 2.0);

  return AOMMIN(frame_boost, max_boost * boost_q_correction);
}

static double calc_kf_frame_boost(const PRIMARY_RATE_CONTROL *p_rc,
                                  const FRAME_INFO *frame_info,
                                  const FIRSTPASS_STATS *this_frame,
                                  double *sr_accumulator, double max_boost) {
  double frame_boost;
  const double lq = av1_convert_qindex_to_q(p_rc->avg_frame_qindex[INTER_FRAME],
                                            frame_info->bit_depth);
  const double boost_q_correction = AOMMIN((0.50 + (lq * 0.015)), 2.00);
  const double active_area = calculate_active_area(frame_info, this_frame);

  // Underlying boost factor is based on inter error ratio.
  frame_boost = AOMMAX(baseline_err_per_mb(frame_info) * active_area,
                       this_frame->intra_error * active_area) /
                DOUBLE_DIVIDE_CHECK(
                    (this_frame->coded_error + *sr_accumulator) * active_area);

  // Update the accumulator for second ref error difference.
  // This is intended to give an indication of how much the coded error is
  // increasing over time.
  *sr_accumulator += (this_frame->sr_coded_error - this_frame->coded_error);
  *sr_accumulator = AOMMAX(0.0, *sr_accumulator);

  // Q correction and scaling
  // The 40.0 value here is an experimentally derived baseline minimum.
  // This value is in line with the minimum per frame boost in the alt_ref
  // boost calculation.
  frame_boost = ((frame_boost + 40.0) * boost_q_correction);

  return AOMMIN(frame_boost, max_boost * boost_q_correction);
}

static int get_projected_gfu_boost(const PRIMARY_RATE_CONTROL *p_rc,
                                   int gfu_boost, int frames_to_project,
                                   int num_stats_used_for_gfu_boost) {
  /*
   * If frames_to_project is equal to num_stats_used_for_gfu_boost,
   * it means that gfu_boost was calculated over frames_to_project to
   * begin with(ie; all stats required were available), hence return
   * the original boost.
   */
  if (num_stats_used_for_gfu_boost >= frames_to_project) return gfu_boost;

  double min_boost_factor = sqrt(p_rc->baseline_gf_interval);
  // Get the current tpl factor (number of frames = frames_to_project).
  double tpl_factor = av1_get_gfu_boost_projection_factor(
      min_boost_factor, MAX_GFUBOOST_FACTOR, frames_to_project);
  // Get the tpl factor when number of frames = num_stats_used_for_prior_boost.
  double tpl_factor_num_stats = av1_get_gfu_boost_projection_factor(
      min_boost_factor, MAX_GFUBOOST_FACTOR, num_stats_used_for_gfu_boost);
  int projected_gfu_boost =
      (int)rint((tpl_factor * gfu_boost) / tpl_factor_num_stats);
  return projected_gfu_boost;
}

#define GF_MAX_BOOST 90.0
#define GF_MIN_BOOST 50
#define MIN_DECAY_FACTOR 0.01
int av1_calc_arf_boost(const TWO_PASS *twopass,
                       const TWO_PASS_FRAME *twopass_frame,
                       const PRIMARY_RATE_CONTROL *p_rc, FRAME_INFO *frame_info,
                       int offset, int f_frames, int b_frames,
                       int *num_fpstats_used, int *num_fpstats_required,
                       int project_gfu_boost) {
  int i;
  GF_GROUP_STATS gf_stats;
  init_gf_stats(&gf_stats);
  double boost_score = (double)NORMAL_BOOST;
  int arf_boost;
  int flash_detected = 0;
  if (num_fpstats_used) *num_fpstats_used = 0;

  // Search forward from the proposed arf/next gf position.
  for (i = 0; i < f_frames; ++i) {
    const FIRSTPASS_STATS *this_frame =
        read_frame_stats(twopass, twopass_frame, i + offset);
    if (this_frame == NULL) break;

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(this_frame, &gf_stats,
                                  frame_info->frame_width,
                                  frame_info->frame_height);

    // We want to discount the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, twopass_frame, i + offset) ||
                     detect_flash(twopass, twopass_frame, i + offset + 1);

    // Accumulate the effect of prediction quality decay.
    if (!flash_detected) {
      gf_stats.decay_accumulator *= get_prediction_decay_rate(this_frame);
      gf_stats.decay_accumulator = gf_stats.decay_accumulator < MIN_DECAY_FACTOR
                                       ? MIN_DECAY_FACTOR
                                       : gf_stats.decay_accumulator;
    }

    boost_score +=
        gf_stats.decay_accumulator *
        calc_frame_boost(p_rc, frame_info, this_frame,
                         gf_stats.this_frame_mv_in_out, GF_MAX_BOOST);
    if (num_fpstats_used) (*num_fpstats_used)++;
  }

  arf_boost = (int)boost_score;

  // Reset for backward looking loop.
  boost_score = 0.0;
  init_gf_stats(&gf_stats);
  // Search backward towards last gf position.
  for (i = -1; i >= -b_frames; --i) {
    const FIRSTPASS_STATS *this_frame =
        read_frame_stats(twopass, twopass_frame, i + offset);
    if (this_frame == NULL) break;

    // Update the motion related elements to the boost calculation.
    accumulate_frame_motion_stats(this_frame, &gf_stats,
                                  frame_info->frame_width,
                                  frame_info->frame_height);

    // We want to discount the the flash frame itself and the recovery
    // frame that follows as both will have poor scores.
    flash_detected = detect_flash(twopass, twopass_frame, i + offset) ||
                     detect_flash(twopass, twopass_frame, i + offset + 1);

    // Cumulative effect of prediction quality decay.
    if (!flash_detected) {
      gf_stats.decay_accumulator *= get_prediction_decay_rate(this_frame);
      gf_stats.decay_accumulator = gf_stats.decay_accumulator < MIN_DECAY_FACTOR
                                       ? MIN_DECAY_FACTOR
                                       : gf_stats.decay_accumulator;
    }

    boost_score +=
        gf_stats.decay_accumulator *
        calc_frame_boost(p_rc, frame_info, this_frame,
                         gf_stats.this_frame_mv_in_out, GF_MAX_BOOST);
    if (num_fpstats_used) (*num_fpstats_used)++;
  }
  arf_boost += (int)boost_score;

  if (project_gfu_boost) {
    assert(num_fpstats_required != NULL);
    assert(num_fpstats_used != NULL);
    *num_fpstats_required = f_frames + b_frames;
    arf_boost = get_projected_gfu_boost(p_rc, arf_boost, *num_fpstats_required,
                                        *num_fpstats_used);
  }

  if (arf_boost < ((b_frames + f_frames) * GF_MIN_BOOST))
    arf_boost = ((b_frames + f_frames) * GF_MIN_BOOST);

  return arf_boost;
}

// Calculate a section intra ratio used in setting max loop filter.
static int calculate_section_intra_ratio(const FIRSTPASS_STATS *begin,
                                         const FIRSTPASS_STATS *end,
                                         int section_length) {
  const FIRSTPASS_STATS *s = begin;
  double intra_error = 0.0;
  double coded_error = 0.0;
  int i = 0;

  while (s < end && i < section_length) {
    intra_error += s->intra_error;
    coded_error += s->coded_error;
    ++s;
    ++i;
  }

  return (int)(intra_error / DOUBLE_DIVIDE_CHECK(coded_error));
}

/*!\brief Calculates the bit target for this GF/ARF group
 *
 * \ingroup rate_control
 *
 * Calculates the total bits to allocate in this GF/ARF group.
 *
 * \param[in]    cpi              Top-level encoder structure
 * \param[in]    gf_group_err     Cumulative coded error score for the
 *                                frames making up this group.
 *
 * \return The target total number of bits for this GF/ARF group.
 */
static int64_t calculate_total_gf_group_bits(AV1_COMP *cpi,
                                             double gf_group_err) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  const TWO_PASS *const twopass = &cpi->ppi->twopass;
  const int max_bits = frame_max_bits(rc, &cpi->oxcf);
  int64_t total_group_bits;

  // Calculate the bits to be allocated to the group as a whole.
  if ((twopass->kf_group_bits > 0) && (twopass->kf_group_error_left > 0)) {
    total_group_bits = (int64_t)(twopass->kf_group_bits *
                                 (gf_group_err / twopass->kf_group_error_left));
  } else {
    total_group_bits = 0;
  }

  // Clamp odd edge cases.
  total_group_bits = (total_group_bits < 0) ? 0
                     : (total_group_bits > twopass->kf_group_bits)
                         ? twopass->kf_group_bits
                         : total_group_bits;

  // Clip based on user supplied data rate variability limit.
  if (total_group_bits > (int64_t)max_bits * p_rc->baseline_gf_interval)
    total_group_bits = (int64_t)max_bits * p_rc->baseline_gf_interval;

  return total_group_bits;
}

// Calculate the number of bits to assign to boosted frames in a group.
static int calculate_boost_bits(int frame_count, int boost,
                                int64_t total_group_bits) {
  int allocation_chunks;

  // return 0 for invalid inputs (could arise e.g. through rounding errors)
  if (!boost || (total_group_bits <= 0)) return 0;

  if (frame_count <= 0) return (int)(AOMMIN(total_group_bits, INT_MAX));

  allocation_chunks = (frame_count * 100) + boost;

  // Prevent overflow.
  if (boost > 1023) {
    int divisor = boost >> 10;
    boost /= divisor;
    allocation_chunks /= divisor;
  }

  // Calculate the number of extra bits for use in the boosted frame or frames.
  return AOMMAX((int)(((int64_t)boost * total_group_bits) / allocation_chunks),
                0);
}

// Calculate the boost factor based on the number of bits assigned, i.e. the
// inverse of calculate_boost_bits().
static int calculate_boost_factor(int frame_count, int bits,
                                  int64_t total_group_bits) {
  return (int)(100.0 * frame_count * bits / (total_group_bits - bits));
}

// Reduce the number of bits assigned to keyframe or arf if necessary, to
// prevent bitrate spikes that may break level constraints.
// frame_type: 0: keyframe; 1: arf.
static int adjust_boost_bits_for_target_level(const AV1_COMP *const cpi,
                                              RATE_CONTROL *const rc,
                                              int bits_assigned,
                                              int64_t group_bits,
                                              int frame_type) {
  const AV1_COMMON *const cm = &cpi->common;
  const SequenceHeader *const seq_params = cm->seq_params;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  const int temporal_layer_id = cm->temporal_layer_id;
  const int spatial_layer_id = cm->spatial_layer_id;
  for (int index = 0; index < seq_params->operating_points_cnt_minus_1 + 1;
       ++index) {
    if (!is_in_operating_point(seq_params->operating_point_idc[index],
                               temporal_layer_id, spatial_layer_id)) {
      continue;
    }

    const AV1_LEVEL target_level =
        cpi->ppi->level_params.target_seq_level_idx[index];
    if (target_level >= SEQ_LEVELS) continue;

    assert(is_valid_seq_level_idx(target_level));

    const double level_bitrate_limit = av1_get_max_bitrate_for_level(
        target_level, seq_params->tier[0], seq_params->profile);
    const int target_bits_per_frame =
        (int)(level_bitrate_limit / cpi->framerate);
    if (frame_type == 0) {
      // Maximum bits for keyframe is 8 times the target_bits_per_frame.
      const int level_enforced_max_kf_bits = target_bits_per_frame * 8;
      if (bits_assigned > level_enforced_max_kf_bits) {
        const int frames = rc->frames_to_key - 1;
        p_rc->kf_boost = calculate_boost_factor(
            frames, level_enforced_max_kf_bits, group_bits);
        bits_assigned =
            calculate_boost_bits(frames, p_rc->kf_boost, group_bits);
      }
    } else if (frame_type == 1) {
      // Maximum bits for arf is 4 times the target_bits_per_frame.
      const int level_enforced_max_arf_bits = target_bits_per_frame * 4;
      if (bits_assigned > level_enforced_max_arf_bits) {
        p_rc->gfu_boost =
            calculate_boost_factor(p_rc->baseline_gf_interval,
                                   level_enforced_max_arf_bits, group_bits);
        bits_assigned = calculate_boost_bits(p_rc->baseline_gf_interval,
                                             p_rc->gfu_boost, group_bits);
      }
    } else {
      assert(0);
    }
  }

  return bits_assigned;
}

// Allocate bits to each frame in a GF / ARF group
double layer_fraction[MAX_ARF_LAYERS + 1] = { 1.0,  0.70, 0.55, 0.60,
                                              0.60, 1.0,  1.0 };
static void allocate_gf_group_bits(GF_GROUP *gf_group,
                                   PRIMARY_RATE_CONTROL *const p_rc,
                                   RATE_CONTROL *const rc,
                                   int64_t gf_group_bits, int gf_arf_bits,
                                   int key_frame, int use_arf) {
  int64_t total_group_bits = gf_group_bits;
  int base_frame_bits;
  const int gf_group_size = gf_group->size;
  int layer_frames[MAX_ARF_LAYERS + 1] = { 0 };

  // For key frames the frame target rate is already set and it
  // is also the golden frame.
  // === [frame_index == 0] ===
  int frame_index = !!key_frame;

  // Subtract the extra bits set aside for ARF frames from the Group Total
  if (use_arf) total_group_bits -= gf_arf_bits;

  int num_frames =
      AOMMAX(1, p_rc->baseline_gf_interval - (rc->frames_since_key == 0));
  base_frame_bits = (int)(total_group_bits / num_frames);

  // Check the number of frames in each layer in case we have a
  // non standard group length.
  int max_arf_layer = gf_group->max_layer_depth - 1;
  for (int idx = frame_index; idx < gf_group_size; ++idx) {
    if ((gf_group->update_type[idx] == ARF_UPDATE) ||
        (gf_group->update_type[idx] == INTNL_ARF_UPDATE)) {
      layer_frames[gf_group->layer_depth[idx]]++;
    }
  }

  // Allocate extra bits to each ARF layer
  int i;
  int layer_extra_bits[MAX_ARF_LAYERS + 1] = { 0 };
  assert(max_arf_layer <= MAX_ARF_LAYERS);
  for (i = 1; i <= max_arf_layer; ++i) {
    double fraction = (i == max_arf_layer) ? 1.0 : layer_fraction[i];
    layer_extra_bits[i] =
        (int)((gf_arf_bits * fraction) / AOMMAX(1, layer_frames[i]));
    gf_arf_bits -= (int)(gf_arf_bits * fraction);
  }

  // Now combine ARF layer and baseline bits to give total bits for each frame.
  int arf_extra_bits;
  for (int idx = frame_index; idx < gf_group_size; ++idx) {
    switch (gf_group->update_type[idx]) {
      case ARF_UPDATE:
      case INTNL_ARF_UPDATE:
        arf_extra_bits = layer_extra_bits[gf_group->layer_depth[idx]];
        gf_group->bit_allocation[idx] =
            (base_frame_bits > INT_MAX - arf_extra_bits)
                ? INT_MAX
                : (base_frame_bits + arf_extra_bits);
        break;
      case INTNL_OVERLAY_UPDATE:
      case OVERLAY_UPDATE: gf_group->bit_allocation[idx] = 0; break;
      default: gf_group->bit_allocation[idx] = base_frame_bits; break;
    }
  }

  // Set the frame following the current GOP to 0 bit allocation. For ARF
  // groups, this next frame will be overlay frame, which is the first frame
  // in the next GOP. For GF group, next GOP will overwrite the rate allocation.
  // Setting this frame to use 0 bit (of out the current GOP budget) will
  // simplify logics in reference frame management.
  if (gf_group_size < MAX_STATIC_GF_GROUP_LENGTH)
    gf_group->bit_allocation[gf_group_size] = 0;
}

// Returns true if KF group and GF group both are almost completely static.
static INLINE int is_almost_static(double gf_zero_motion, int kf_zero_motion,
                                   int is_lap_enabled) {
  if (is_lap_enabled) {
    /*
     * when LAP enabled kf_zero_motion is not reliable, so use strict
     * constraint on gf_zero_motion.
     */
    return (gf_zero_motion >= 0.999);
  } else {
    return (gf_zero_motion >= 0.995) &&
           (kf_zero_motion >= STATIC_KF_GROUP_THRESH);
  }
}

#define ARF_ABS_ZOOM_THRESH 4.4
static INLINE int detect_gf_cut(AV1_COMP *cpi, int frame_index, int cur_start,
                                int flash_detected, int active_max_gf_interval,
                                int active_min_gf_interval,
                                GF_GROUP_STATS *gf_stats) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  AV1_COMMON *const cm = &cpi->common;
  // Motion breakout threshold for loop below depends on image size.
  const double mv_ratio_accumulator_thresh = (cm->height + cm->width) / 4.0;

  if (!flash_detected) {
    // Break clause to detect very still sections after motion. For example,
    // a static image after a fade or other transition.

    // TODO(angiebird): This is a temporary change, we will avoid using
    // twopass_frame.stats_in in the follow-up CL
    int index = (int)(cpi->twopass_frame.stats_in -
                      twopass->stats_buf_ctx->stats_in_start);
    if (detect_transition_to_still(&twopass->firstpass_info, index,
                                   rc->min_gf_interval, frame_index - cur_start,
                                   5, gf_stats->loop_decay_rate,
                                   gf_stats->last_loop_decay_rate)) {
      return 1;
    }
  }

  // Some conditions to breakout after min interval.
  if (frame_index - cur_start >= active_min_gf_interval &&
      // If possible don't break very close to a kf
      (rc->frames_to_key - frame_index >= rc->min_gf_interval) &&
      ((frame_index - cur_start) & 0x01) && !flash_detected &&
      (gf_stats->mv_ratio_accumulator > mv_ratio_accumulator_thresh ||
       gf_stats->abs_mv_in_out_accumulator > ARF_ABS_ZOOM_THRESH)) {
    return 1;
  }

  // If almost totally static, we will not use the the max GF length later,
  // so we can continue for more frames.
  if (((frame_index - cur_start) >= active_max_gf_interval + 1) &&
      !is_almost_static(gf_stats->zero_motion_accumulator,
                        twopass->kf_zeromotion_pct, cpi->ppi->lap_enabled)) {
    return 1;
  }
  return 0;
}

static int is_shorter_gf_interval_better(
    AV1_COMP *cpi, const EncodeFrameParams *frame_params) {
  const RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  int gop_length_decision_method = cpi->sf.tpl_sf.gop_length_decision_method;
  int shorten_gf_interval;

  av1_tpl_preload_rc_estimate(cpi, frame_params);

  if (gop_length_decision_method == 2) {
    // GF group length is decided based on GF boost and tpl stats of ARFs from
    // base layer, (base+1) layer.
    shorten_gf_interval =
        (p_rc->gfu_boost <
         p_rc->num_stats_used_for_gfu_boost * GF_MIN_BOOST * 1.4) &&
        !av1_tpl_setup_stats(cpi, 3, frame_params);
  } else {
    int do_complete_tpl = 1;
    GF_GROUP *const gf_group = &cpi->ppi->gf_group;
    int is_temporal_filter_enabled =
        (rc->frames_since_key > 0 && gf_group->arf_index > -1);

    if (gop_length_decision_method == 1) {
      // Check if tpl stats of ARFs from base layer, (base+1) layer,
      // (base+2) layer can decide the GF group length.
      int gop_length_eval = av1_tpl_setup_stats(cpi, 2, frame_params);

      if (gop_length_eval != 2) {
        do_complete_tpl = 0;
        shorten_gf_interval = !gop_length_eval;
      }
    }

    if (do_complete_tpl) {
      // Decide GF group length based on complete tpl stats.
      shorten_gf_interval = !av1_tpl_setup_stats(cpi, 1, frame_params);
      // Tpl stats is reused when the ARF is temporally filtered and GF
      // interval is not shortened.
      if (is_temporal_filter_enabled && !shorten_gf_interval) {
        cpi->skip_tpl_setup_stats = 1;
#if CONFIG_BITRATE_ACCURACY && !CONFIG_THREE_PASS
        assert(cpi->gf_frame_index == 0);
        av1_vbr_rc_update_q_index_list(&cpi->vbr_rc_info, &cpi->ppi->tpl_data,
                                       gf_group,
                                       cpi->common.seq_params->bit_depth);
#endif  // CONFIG_BITRATE_ACCURACY
      }
    }
  }
  return shorten_gf_interval;
}

#define MIN_SHRINK_LEN 6  // the minimum length of gf if we are shrinking
#define SMOOTH_FILT_LEN 7
#define HALF_FILT_LEN (SMOOTH_FILT_LEN / 2)
#define WINDOW_SIZE 7
#define HALF_WIN (WINDOW_SIZE / 2)
// A 7-tap gaussian smooth filter
const double smooth_filt[SMOOTH_FILT_LEN] = { 0.006, 0.061, 0.242, 0.383,
                                              0.242, 0.061, 0.006 };

// Smooth filter intra_error and coded_error in firstpass stats.
// If stats[i].is_flash==1, the ith element should not be used in the filtering.
static void smooth_filter_stats(const FIRSTPASS_STATS *stats, int start_idx,
                                int last_idx, double *filt_intra_err,
                                double *filt_coded_err) {
  int i, j;
  for (i = start_idx; i <= last_idx; i++) {
    double total_wt = 0;
    for (j = -HALF_FILT_LEN; j <= HALF_FILT_LEN; j++) {
      int idx = AOMMIN(AOMMAX(i + j, start_idx), last_idx);
      if (stats[idx].is_flash) continue;

      filt_intra_err[i] +=
          smooth_filt[j + HALF_FILT_LEN] * stats[idx].intra_error;
      total_wt += smooth_filt[j + HALF_FILT_LEN];
    }
    if (total_wt > 0.01) {
      filt_intra_err[i] /= total_wt;
    } else {
      filt_intra_err[i] = stats[i].intra_error;
    }
  }
  for (i = start_idx; i <= last_idx; i++) {
    double total_wt = 0;
    for (j = -HALF_FILT_LEN; j <= HALF_FILT_LEN; j++) {
      int idx = AOMMIN(AOMMAX(i + j, start_idx), last_idx);
      // Coded error involves idx and idx - 1.
      if (stats[idx].is_flash || (idx > 0 && stats[idx - 1].is_flash)) continue;

      filt_coded_err[i] +=
          smooth_filt[j + HALF_FILT_LEN] * stats[idx].coded_error;
      total_wt += smooth_filt[j + HALF_FILT_LEN];
    }
    if (total_wt > 0.01) {
      filt_coded_err[i] /= total_wt;
    } else {
      filt_coded_err[i] = stats[i].coded_error;
    }
  }
}

// Calculate gradient
static void get_gradient(const double *values, int start, int last,
                         double *grad) {
  if (start == last) {
    grad[start] = 0;
    return;
  }
  for (int i = start; i <= last; i++) {
    int prev = AOMMAX(i - 1, start);
    int next = AOMMIN(i + 1, last);
    grad[i] = (values[next] - values[prev]) / (next - prev);
  }
}

static int find_next_scenecut(const FIRSTPASS_STATS *const stats_start,
                              int first, int last) {
  // Identify unstable areas caused by scenecuts.
  // Find the max and 2nd max coded error, and the average of the rest frames.
  // If there is only one frame that yields a huge coded error, it is likely a
  // scenecut.
  double this_ratio, max_prev_ratio, max_next_ratio, max_prev_coded,
      max_next_coded;

  if (last - first == 0) return -1;

  for (int i = first; i <= last; i++) {
    if (stats_start[i].is_flash || (i > 0 && stats_start[i - 1].is_flash))
      continue;
    double temp_intra = AOMMAX(stats_start[i].intra_error, 0.01);
    this_ratio = stats_start[i].coded_error / temp_intra;
    // find the avg ratio in the preceding neighborhood
    max_prev_ratio = 0;
    max_prev_coded = 0;
    for (int j = AOMMAX(first, i - HALF_WIN); j < i; j++) {
      if (stats_start[j].is_flash || (j > 0 && stats_start[j - 1].is_flash))
        continue;
      temp_intra = AOMMAX(stats_start[j].intra_error, 0.01);
      double temp_ratio = stats_start[j].coded_error / temp_intra;
      if (temp_ratio > max_prev_ratio) {
        max_prev_ratio = temp_ratio;
      }
      if (stats_start[j].coded_error > max_prev_coded) {
        max_prev_coded = stats_start[j].coded_error;
      }
    }
    // find the avg ratio in the following neighborhood
    max_next_ratio = 0;
    max_next_coded = 0;
    for (int j = i + 1; j <= AOMMIN(i + HALF_WIN, last); j++) {
      if (stats_start[i].is_flash || (i > 0 && stats_start[i - 1].is_flash))
        continue;
      temp_intra = AOMMAX(stats_start[j].intra_error, 0.01);
      double temp_ratio = stats_start[j].coded_error / temp_intra;
      if (temp_ratio > max_next_ratio) {
        max_next_ratio = temp_ratio;
      }
      if (stats_start[j].coded_error > max_next_coded) {
        max_next_coded = stats_start[j].coded_error;
      }
    }

    if (max_prev_ratio < 0.001 && max_next_ratio < 0.001) {
      // the ratios are very small, only check a small fixed threshold
      if (this_ratio < 0.02) continue;
    } else {
      // check if this frame has a larger ratio than the neighborhood
      double max_sr = stats_start[i].sr_coded_error;
      if (i < last) max_sr = AOMMAX(max_sr, stats_start[i + 1].sr_coded_error);
      double max_sr_fr_ratio =
          max_sr / AOMMAX(stats_start[i].coded_error, 0.01);

      if (max_sr_fr_ratio > 1.2) continue;
      if (this_ratio < 2 * AOMMAX(max_prev_ratio, max_next_ratio) &&
          stats_start[i].coded_error <
              2 * AOMMAX(max_prev_coded, max_next_coded)) {
        continue;
      }
    }
    return i;
  }
  return -1;
}

// Remove the region with index next_region.
// parameter merge: 0: merge with previous; 1: merge with next; 2:
// merge with both, take type from previous if possible
// After removing, next_region will be the index of the next region.
static void remove_region(int merge, REGIONS *regions, int *num_regions,
                          int *next_region) {
  int k = *next_region;
  assert(k < *num_regions);
  if (*num_regions == 1) {
    *num_regions = 0;
    return;
  }
  if (k == 0) {
    merge = 1;
  } else if (k == *num_regions - 1) {
    merge = 0;
  }
  int num_merge = (merge == 2) ? 2 : 1;
  switch (merge) {
    case 0:
      regions[k - 1].last = regions[k].last;
      *next_region = k;
      break;
    case 1:
      regions[k + 1].start = regions[k].start;
      *next_region = k + 1;
      break;
    case 2:
      regions[k - 1].last = regions[k + 1].last;
      *next_region = k;
      break;
    default: assert(0);
  }
  *num_regions -= num_merge;
  for (k = *next_region - (merge == 1); k < *num_regions; k++) {
    regions[k] = regions[k + num_merge];
  }
}

// Insert a region in the cur_region_idx. The start and last should both be in
// the current region. After insertion, the cur_region_idx will point to the
// last region that was splitted from the original region.
static void insert_region(int start, int last, REGION_TYPES type,
                          REGIONS *regions, int *num_regions,
                          int *cur_region_idx) {
  int k = *cur_region_idx;
  REGION_TYPES this_region_type = regions[k].type;
  int this_region_last = regions[k].last;
  int num_add = (start != regions[k].start) + (last != regions[k].last);
  // move the following regions further to the back
  for (int r = *num_regions - 1; r > k; r--) {
    regions[r + num_add] = regions[r];
  }
  *num_regions += num_add;
  if (start > regions[k].start) {
    regions[k].last = start - 1;
    k++;
    regions[k].start = start;
  }
  regions[k].type = type;
  if (last < this_region_last) {
    regions[k].last = last;
    k++;
    regions[k].start = last + 1;
    regions[k].last = this_region_last;
    regions[k].type = this_region_type;
  } else {
    regions[k].last = this_region_last;
  }
  *cur_region_idx = k;
}

// Get the average of stats inside a region.
static void analyze_region(const FIRSTPASS_STATS *stats, int k,
                           REGIONS *regions) {
  int i;
  regions[k].avg_cor_coeff = 0;
  regions[k].avg_sr_fr_ratio = 0;
  regions[k].avg_intra_err = 0;
  regions[k].avg_coded_err = 0;

  int check_first_sr = (k != 0);

  for (i = regions[k].start; i <= regions[k].last; i++) {
    if (i > regions[k].start || check_first_sr) {
      double num_frames =
          (double)(regions[k].last - regions[k].start + check_first_sr);
      double max_coded_error =
          AOMMAX(stats[i].coded_error, stats[i - 1].coded_error);
      double this_ratio =
          stats[i].sr_coded_error / AOMMAX(max_coded_error, 0.001);
      regions[k].avg_sr_fr_ratio += this_ratio / num_frames;
    }

    regions[k].avg_intra_err +=
        stats[i].intra_error / (double)(regions[k].last - regions[k].start + 1);
    regions[k].avg_coded_err +=
        stats[i].coded_error / (double)(regions[k].last - regions[k].start + 1);

    regions[k].avg_cor_coeff +=
        AOMMAX(stats[i].cor_coeff, 0.001) /
        (double)(regions[k].last - regions[k].start + 1);
    regions[k].avg_noise_var +=
        AOMMAX(stats[i].noise_var, 0.001) /
        (double)(regions[k].last - regions[k].start + 1);
  }
}

// Calculate the regions stats of every region.
static void get_region_stats(const FIRSTPASS_STATS *stats, REGIONS *regions,
                             int num_regions) {
  for (int k = 0; k < num_regions; k++) {
    analyze_region(stats, k, regions);
  }
}

// Find tentative stable regions
static int find_stable_regions(const FIRSTPASS_STATS *stats,
                               const double *grad_coded, int this_start,
                               int this_last, REGIONS *regions) {
  int i, j, k = 0;
  regions[k].start = this_start;
  for (i = this_start; i <= this_last; i++) {
    // Check mean and variance of stats in a window
    double mean_intra = 0.001, var_intra = 0.001;
    double mean_coded = 0.001, var_coded = 0.001;
    int count = 0;
    for (j = -HALF_WIN; j <= HALF_WIN; j++) {
      int idx = AOMMIN(AOMMAX(i + j, this_start), this_last);
      if (stats[idx].is_flash || (idx > 0 && stats[idx - 1].is_flash)) continue;
      mean_intra += stats[idx].intra_error;
      var_intra += stats[idx].intra_error * stats[idx].intra_error;
      mean_coded += stats[idx].coded_error;
      var_coded += stats[idx].coded_error * stats[idx].coded_error;
      count++;
    }

    REGION_TYPES cur_type;
    if (count > 0) {
      mean_intra /= (double)count;
      var_intra /= (double)count;
      mean_coded /= (double)count;
      var_coded /= (double)count;
      int is_intra_stable = (var_intra / (mean_intra * mean_intra) < 1.03);
      int is_coded_stable = (var_coded / (mean_coded * mean_coded) < 1.04 &&
                             fabs(grad_coded[i]) / mean_coded < 0.05) ||
                            mean_coded / mean_intra < 0.05;
      int is_coded_small = mean_coded < 0.5 * mean_intra;
      cur_type = (is_intra_stable && is_coded_stable && is_coded_small)
                     ? STABLE_REGION
                     : HIGH_VAR_REGION;
    } else {
      cur_type = HIGH_VAR_REGION;
    }

    // mark a new region if type changes
    if (i == regions[k].start) {
      // first frame in the region
      regions[k].type = cur_type;
    } else if (cur_type != regions[k].type) {
      // Append a new region
      regions[k].last = i - 1;
      regions[k + 1].start = i;
      regions[k + 1].type = cur_type;
      k++;
    }
  }
  regions[k].last = this_last;
  return k + 1;
}

// Clean up regions that should be removed or merged.
static void cleanup_regions(REGIONS *regions, int *num_regions) {
  int k = 0;
  while (k < *num_regions) {
    if ((k > 0 && regions[k - 1].type == regions[k].type &&
         regions[k].type != SCENECUT_REGION) ||
        regions[k].last < regions[k].start) {
      remove_region(0, regions, num_regions, &k);
    } else {
      k++;
    }
  }
}

// Remove regions that are of type and shorter than length.
// Merge it with its neighboring regions.
static void remove_short_regions(REGIONS *regions, int *num_regions,
                                 REGION_TYPES type, int length) {
  int k = 0;
  while (k < *num_regions && (*num_regions) > 1) {
    if ((regions[k].last - regions[k].start + 1 < length &&
         regions[k].type == type)) {
      // merge current region with the previous and next regions
      remove_region(2, regions, num_regions, &k);
    } else {
      k++;
    }
  }
  cleanup_regions(regions, num_regions);
}

static void adjust_unstable_region_bounds(const FIRSTPASS_STATS *stats,
                                          REGIONS *regions, int *num_regions) {
  int i, j, k;
  // Remove regions that are too short. Likely noise.
  remove_short_regions(regions, num_regions, STABLE_REGION, HALF_WIN);
  remove_short_regions(regions, num_regions, HIGH_VAR_REGION, HALF_WIN);

  get_region_stats(stats, regions, *num_regions);

  // Adjust region boundaries. The thresholds are empirically obtained, but
  // overall the performance is not very sensitive to small changes to them.
  for (k = 0; k < *num_regions; k++) {
    if (regions[k].type == STABLE_REGION) continue;
    if (k > 0) {
      // Adjust previous boundary.
      // First find the average intra/coded error in the previous
      // neighborhood.
      double avg_intra_err = 0;
      const int starti = AOMMAX(regions[k - 1].last - WINDOW_SIZE + 1,
                                regions[k - 1].start + 1);
      const int lasti = regions[k - 1].last;
      int counti = 0;
      for (i = starti; i <= lasti; i++) {
        avg_intra_err += stats[i].intra_error;
        counti++;
      }
      if (counti > 0) {
        avg_intra_err = AOMMAX(avg_intra_err / (double)counti, 0.001);
        int count_coded = 0, count_grad = 0;
        for (j = lasti + 1; j <= regions[k].last; j++) {
          const int intra_close =
              fabs(stats[j].intra_error - avg_intra_err) / avg_intra_err < 0.1;
          const int coded_small = stats[j].coded_error / avg_intra_err < 0.1;
          const int coeff_close = stats[j].cor_coeff > 0.995;
          if (!coeff_close || !coded_small) count_coded--;
          if (intra_close && count_coded >= 0 && count_grad >= 0) {
            // this frame probably belongs to the previous stable region
            regions[k - 1].last = j;
            regions[k].start = j + 1;
          } else {
            break;
          }
        }
      }
    }  // if k > 0
    if (k < *num_regions - 1) {
      // Adjust next boundary.
      // First find the average intra/coded error in the next neighborhood.
      double avg_intra_err = 0;
      const int starti = regions[k + 1].start;
      const int lasti = AOMMIN(regions[k + 1].last - 1,
                               regions[k + 1].start + WINDOW_SIZE - 1);
      int counti = 0;
      for (i = starti; i <= lasti; i++) {
        avg_intra_err += stats[i].intra_error;
        counti++;
      }
      if (counti > 0) {
        avg_intra_err = AOMMAX(avg_intra_err / (double)counti, 0.001);
        // At the boundary, coded error is large, but still the frame is stable
        int count_coded = 1, count_grad = 1;
        for (j = starti - 1; j >= regions[k].start; j--) {
          const int intra_close =
              fabs(stats[j].intra_error - avg_intra_err) / avg_intra_err < 0.1;
          const int coded_small =
              stats[j + 1].coded_error / avg_intra_err < 0.1;
          const int coeff_close = stats[j].cor_coeff > 0.995;
          if (!coeff_close || !coded_small) count_coded--;
          if (intra_close && count_coded >= 0 && count_grad >= 0) {
            // this frame probably belongs to the next stable region
            regions[k + 1].start = j;
            regions[k].last = j - 1;
          } else {
            break;
          }
        }
      }
    }  // if k < *num_regions - 1
  }    // end of loop over all regions

  cleanup_regions(regions, num_regions);
  remove_short_regions(regions, num_regions, HIGH_VAR_REGION, HALF_WIN);
  get_region_stats(stats, regions, *num_regions);

  // If a stable regions has higher error than neighboring high var regions,
  // or if the stable region has a lower average correlation,
  // then it should be merged with them
  k = 0;
  while (k < *num_regions && (*num_regions) > 1) {
    if (regions[k].type == STABLE_REGION &&
        (regions[k].last - regions[k].start + 1) < 2 * WINDOW_SIZE &&
        ((k > 0 &&  // previous regions
          (regions[k].avg_coded_err > regions[k - 1].avg_coded_err * 1.01 ||
           regions[k].avg_cor_coeff < regions[k - 1].avg_cor_coeff * 0.999)) &&
         (k < *num_regions - 1 &&  // next region
          (regions[k].avg_coded_err > regions[k + 1].avg_coded_err * 1.01 ||
           regions[k].avg_cor_coeff < regions[k + 1].avg_cor_coeff * 0.999)))) {
      // merge current region with the previous and next regions
      remove_region(2, regions, num_regions, &k);
      analyze_region(stats, k - 1, regions);
    } else if (regions[k].type == HIGH_VAR_REGION &&
               (regions[k].last - regions[k].start + 1) < 2 * WINDOW_SIZE &&
               ((k > 0 &&  // previous regions
                 (regions[k].avg_coded_err <
                      regions[k - 1].avg_coded_err * 0.99 ||
                  regions[k].avg_cor_coeff >
                      regions[k - 1].avg_cor_coeff * 1.001)) &&
                (k < *num_regions - 1 &&  // next region
                 (regions[k].avg_coded_err <
                      regions[k + 1].avg_coded_err * 0.99 ||
                  regions[k].avg_cor_coeff >
                      regions[k + 1].avg_cor_coeff * 1.001)))) {
      // merge current region with the previous and next regions
      remove_region(2, regions, num_regions, &k);
      analyze_region(stats, k - 1, regions);
    } else {
      k++;
    }
  }

  remove_short_regions(regions, num_regions, STABLE_REGION, WINDOW_SIZE);
  remove_short_regions(regions, num_regions, HIGH_VAR_REGION, HALF_WIN);
}

// Identify blending regions.
static void find_blending_regions(const FIRSTPASS_STATS *stats,
                                  REGIONS *regions, int *num_regions) {
  int i, k = 0;
  // Blending regions will have large content change, therefore will have a
  // large consistent change in intra error.
  int count_stable = 0;
  while (k < *num_regions) {
    if (regions[k].type == STABLE_REGION) {
      k++;
      count_stable++;
      continue;
    }
    int dir = 0;
    int start = 0, last;
    for (i = regions[k].start; i <= regions[k].last; i++) {
      // First mark the regions that has consistent large change of intra error.
      if (k == 0 && i == regions[k].start) continue;
      if (stats[i].is_flash || (i > 0 && stats[i - 1].is_flash)) continue;
      double grad = stats[i].intra_error - stats[i - 1].intra_error;
      int large_change = fabs(grad) / AOMMAX(stats[i].intra_error, 0.01) > 0.05;
      int this_dir = 0;
      if (large_change) {
        this_dir = (grad > 0) ? 1 : -1;
      }
      // the current trend continues
      if (dir == this_dir) continue;
      if (dir != 0) {
        // Mark the end of a new large change group and add it
        last = i - 1;
        insert_region(start, last, BLENDING_REGION, regions, num_regions, &k);
      }
      dir = this_dir;
      if (k == 0 && i == regions[k].start + 1) {
        start = i - 1;
      } else {
        start = i;
      }
    }
    if (dir != 0) {
      last = regions[k].last;
      insert_region(start, last, BLENDING_REGION, regions, num_regions, &k);
    }
    k++;
  }

  // If the blending region has very low correlation, mark it as high variance
  // since we probably cannot benefit from it anyways.
  get_region_stats(stats, regions, *num_regions);
  for (k = 0; k < *num_regions; k++) {
    if (regions[k].type != BLENDING_REGION) continue;
    if (regions[k].last == regions[k].start || regions[k].avg_cor_coeff < 0.6 ||
        count_stable == 0)
      regions[k].type = HIGH_VAR_REGION;
  }
  get_region_stats(stats, regions, *num_regions);

  // It is possible for blending to result in a "dip" in intra error (first
  // decrease then increase). Therefore we need to find the dip and combine the
  // two regions.
  k = 1;
  while (k < *num_regions) {
    if (k < *num_regions - 1 && regions[k].type == HIGH_VAR_REGION) {
      // Check if this short high variance regions is actually in the middle of
      // a blending region.
      if (regions[k - 1].type == BLENDING_REGION &&
          regions[k + 1].type == BLENDING_REGION &&
          regions[k].last - regions[k].start < 3) {
        int prev_dir = (stats[regions[k - 1].last].intra_error -
                        stats[regions[k - 1].last - 1].intra_error) > 0
                           ? 1
                           : -1;
        int next_dir = (stats[regions[k + 1].last].intra_error -
                        stats[regions[k + 1].last - 1].intra_error) > 0
                           ? 1
                           : -1;
        if (prev_dir < 0 && next_dir > 0) {
          // This is possibly a mid region of blending. Check the ratios
          double ratio_thres = AOMMIN(regions[k - 1].avg_sr_fr_ratio,
                                      regions[k + 1].avg_sr_fr_ratio) *
                               0.95;
          if (regions[k].avg_sr_fr_ratio > ratio_thres) {
            regions[k].type = BLENDING_REGION;
            remove_region(2, regions, num_regions, &k);
            analyze_region(stats, k - 1, regions);
            continue;
          }
        }
      }
    }
    // Check if we have a pair of consecutive blending regions.
    if (regions[k - 1].type == BLENDING_REGION &&
        regions[k].type == BLENDING_REGION) {
      int prev_dir = (stats[regions[k - 1].last].intra_error -
                      stats[regions[k - 1].last - 1].intra_error) > 0
                         ? 1
                         : -1;
      int next_dir = (stats[regions[k].last].intra_error -
                      stats[regions[k].last - 1].intra_error) > 0
                         ? 1
                         : -1;

      // if both are too short, no need to check
      int total_length = regions[k].last - regions[k - 1].start + 1;
      if (total_length < 4) {
        regions[k - 1].type = HIGH_VAR_REGION;
        k++;
        continue;
      }

      int to_merge = 0;
      if (prev_dir < 0 && next_dir > 0) {
        // In this case we check the last frame in the previous region.
        double prev_length =
            (double)(regions[k - 1].last - regions[k - 1].start + 1);
        double last_ratio, ratio_thres;
        if (prev_length < 2.01) {
          // if the previous region is very short
          double max_coded_error =
              AOMMAX(stats[regions[k - 1].last].coded_error,
                     stats[regions[k - 1].last - 1].coded_error);
          last_ratio = stats[regions[k - 1].last].sr_coded_error /
                       AOMMAX(max_coded_error, 0.001);
          ratio_thres = regions[k].avg_sr_fr_ratio * 0.95;
        } else {
          double max_coded_error =
              AOMMAX(stats[regions[k - 1].last].coded_error,
                     stats[regions[k - 1].last - 1].coded_error);
          last_ratio = stats[regions[k - 1].last].sr_coded_error /
                       AOMMAX(max_coded_error, 0.001);
          double prev_ratio =
              (regions[k - 1].avg_sr_fr_ratio * prev_length - last_ratio) /
              (prev_length - 1.0);
          ratio_thres = AOMMIN(prev_ratio, regions[k].avg_sr_fr_ratio) * 0.95;
        }
        if (last_ratio > ratio_thres) {
          to_merge = 1;
        }
      }

      if (to_merge) {
        remove_region(0, regions, num_regions, &k);
        analyze_region(stats, k - 1, regions);
        continue;
      } else {
        // These are possibly two separate blending regions. Mark the boundary
        // frame as HIGH_VAR_REGION to separate the two.
        int prev_k = k - 1;
        insert_region(regions[prev_k].last, regions[prev_k].last,
                      HIGH_VAR_REGION, regions, num_regions, &prev_k);
        analyze_region(stats, prev_k, regions);
        k = prev_k + 1;
        analyze_region(stats, k, regions);
      }
    }
    k++;
  }
  cleanup_regions(regions, num_regions);
}

// Clean up decision for blendings. Remove blending regions that are too short.
// Also if a very short high var region is between a blending and a stable
// region, just merge it with one of them.
static void cleanup_blendings(REGIONS *regions, int *num_regions) {
  int k = 0;
  while (k<*num_regions && * num_regions> 1) {
    int is_short_blending = regions[k].type == BLENDING_REGION &&
                            regions[k].last - regions[k].start + 1 < 5;
    int is_short_hv = regions[k].type == HIGH_VAR_REGION &&
                      regions[k].last - regions[k].start + 1 < 5;
    int has_stable_neighbor =
        ((k > 0 && regions[k - 1].type == STABLE_REGION) ||
         (k < *num_regions - 1 && regions[k + 1].type == STABLE_REGION));
    int has_blend_neighbor =
        ((k > 0 && regions[k - 1].type == BLENDING_REGION) ||
         (k < *num_regions - 1 && regions[k + 1].type == BLENDING_REGION));
    int total_neighbors = (k > 0) + (k < *num_regions - 1);

    if (is_short_blending ||
        (is_short_hv &&
         has_stable_neighbor + has_blend_neighbor >= total_neighbors)) {
      // Remove this region.Try to determine whether to combine it with the
      // previous or next region.
      int merge;
      double prev_diff =
          (k > 0)
              ? fabs(regions[k].avg_cor_coeff - regions[k - 1].avg_cor_coeff)
              : 1;
      double next_diff =
          (k < *num_regions - 1)
              ? fabs(regions[k].avg_cor_coeff - regions[k + 1].avg_cor_coeff)
              : 1;
      // merge == 0 means to merge with previous, 1 means to merge with next
      merge = prev_diff > next_diff;
      remove_region(merge, regions, num_regions, &k);
    } else {
      k++;
    }
  }
  cleanup_regions(regions, num_regions);
}

static void free_firstpass_stats_buffers(REGIONS *temp_regions,
                                         double *filt_intra_err,
                                         double *filt_coded_err,
                                         double *grad_coded) {
  aom_free(temp_regions);
  aom_free(filt_intra_err);
  aom_free(filt_coded_err);
  aom_free(grad_coded);
}

// Identify stable and unstable regions from first pass stats.
// stats_start points to the first frame to analyze.
// |offset| is the offset from the current frame to the frame stats_start is
// pointing to.
// Returns 0 on success, -1 on memory allocation failure.
static int identify_regions(const FIRSTPASS_STATS *const stats_start,
                            int total_frames, int offset, REGIONS *regions,
                            int *total_regions) {
  int k;
  if (total_frames <= 1) return 0;

  // store the initial decisions
  REGIONS *temp_regions =
      (REGIONS *)aom_malloc(total_frames * sizeof(temp_regions[0]));
  // buffers for filtered stats
  double *filt_intra_err =
      (double *)aom_calloc(total_frames, sizeof(*filt_intra_err));
  double *filt_coded_err =
      (double *)aom_calloc(total_frames, sizeof(*filt_coded_err));
  double *grad_coded = (double *)aom_calloc(total_frames, sizeof(*grad_coded));
  if (!(temp_regions && filt_intra_err && filt_coded_err && grad_coded)) {
    free_firstpass_stats_buffers(temp_regions, filt_intra_err, filt_coded_err,
                                 grad_coded);
    return -1;
  }
  av1_zero_array(temp_regions, total_frames);

  int cur_region = 0, this_start = 0, this_last;

  int next_scenecut = -1;
  do {
    // first get the obvious scenecuts
    next_scenecut =
        find_next_scenecut(stats_start, this_start, total_frames - 1);
    this_last = (next_scenecut >= 0) ? (next_scenecut - 1) : total_frames - 1;

    // low-pass filter the needed stats
    smooth_filter_stats(stats_start, this_start, this_last, filt_intra_err,
                        filt_coded_err);
    get_gradient(filt_coded_err, this_start, this_last, grad_coded);

    // find tentative stable regions and unstable regions
    int num_regions = find_stable_regions(stats_start, grad_coded, this_start,
                                          this_last, temp_regions);

    adjust_unstable_region_bounds(stats_start, temp_regions, &num_regions);

    get_region_stats(stats_start, temp_regions, num_regions);

    // Try to identify blending regions in the unstable regions
    find_blending_regions(stats_start, temp_regions, &num_regions);
    cleanup_blendings(temp_regions, &num_regions);

    // The flash points should all be considered high variance points
    k = 0;
    while (k < num_regions) {
      if (temp_regions[k].type != STABLE_REGION) {
        k++;
        continue;
      }
      int start = temp_regions[k].start;
      int last = temp_regions[k].last;
      for (int i = start; i <= last; i++) {
        if (stats_start[i].is_flash) {
          insert_region(i, i, HIGH_VAR_REGION, temp_regions, &num_regions, &k);
        }
      }
      k++;
    }
    cleanup_regions(temp_regions, &num_regions);

    // copy the regions in the scenecut group
    for (k = 0; k < num_regions; k++) {
      if (temp_regions[k].last < temp_regions[k].start &&
          k == num_regions - 1) {
        num_regions--;
        break;
      }
      regions[k + cur_region] = temp_regions[k];
    }
    cur_region += num_regions;

    // add the scenecut region
    if (next_scenecut > -1) {
      // add the scenecut region, and find the next scenecut
      regions[cur_region].type = SCENECUT_REGION;
      regions[cur_region].start = next_scenecut;
      regions[cur_region].last = next_scenecut;
      cur_region++;
      this_start = next_scenecut + 1;
    }
  } while (next_scenecut >= 0);

  *total_regions = cur_region;
  get_region_stats(stats_start, regions, *total_regions);

  for (k = 0; k < *total_regions; k++) {
    // If scenecuts are very minor, mark them as high variance.
    if (regions[k].type != SCENECUT_REGION ||
        regions[k].avg_cor_coeff *
                (1 - stats_start[regions[k].start].noise_var /
                         regions[k].avg_intra_err) <
            0.8) {
      continue;
    }
    regions[k].type = HIGH_VAR_REGION;
  }
  cleanup_regions(regions, total_regions);
  get_region_stats(stats_start, regions, *total_regions);

  for (k = 0; k < *total_regions; k++) {
    regions[k].start += offset;
    regions[k].last += offset;
  }

  free_firstpass_stats_buffers(temp_regions, filt_intra_err, filt_coded_err,
                               grad_coded);
  return 0;
}

static int find_regions_index(const REGIONS *regions, int num_regions,
                              int frame_idx) {
  for (int k = 0; k < num_regions; k++) {
    if (regions[k].start <= frame_idx && regions[k].last >= frame_idx) {
      return k;
    }
  }
  return -1;
}

/*!\brief Determine the length of future GF groups.
 *
 * \ingroup gf_group_algo
 * This function decides the gf group length of future frames in batch
 *
 * \param[in]    cpi              Top-level encoder structure
 * \param[in]    max_gop_length   Maximum length of the GF group
 * \param[in]    max_intervals    Maximum number of intervals to decide
 *
 * \remark Nothing is returned. Instead, cpi->ppi->rc.gf_intervals is
 * changed to store the decided GF group lengths.
 */
static void calculate_gf_length(AV1_COMP *cpi, int max_gop_length,
                                int max_intervals) {
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  FIRSTPASS_STATS next_frame;
  const FIRSTPASS_STATS *const start_pos = cpi->twopass_frame.stats_in;
  const FIRSTPASS_STATS *const stats = start_pos - (rc->frames_since_key == 0);

  const int f_w = cpi->common.width;
  const int f_h = cpi->common.height;
  int i;

  int flash_detected;

  av1_zero(next_frame);

  if (has_no_stats_stage(cpi)) {
    for (i = 0; i < MAX_NUM_GF_INTERVALS; i++) {
      p_rc->gf_intervals[i] = AOMMIN(rc->max_gf_interval, max_gop_length);
    }
    p_rc->cur_gf_index = 0;
    rc->intervals_till_gf_calculate_due = MAX_NUM_GF_INTERVALS;
    return;
  }

  // TODO(urvang): Try logic to vary min and max interval based on q.
  const int active_min_gf_interval = rc->min_gf_interval;
  const int active_max_gf_interval =
      AOMMIN(rc->max_gf_interval, max_gop_length);
  const int min_shrink_int = AOMMAX(MIN_SHRINK_LEN, active_min_gf_interval);

  i = (rc->frames_since_key == 0);
  max_intervals = cpi->ppi->lap_enabled ? 1 : max_intervals;
  int count_cuts = 1;
  // If cpi->gf_state.arf_gf_boost_lst is 0, we are starting with a KF or GF.
  int cur_start = -1 + !cpi->ppi->gf_state.arf_gf_boost_lst, cur_last;
  int cut_pos[MAX_NUM_GF_INTERVALS + 1] = { -1 };
  int cut_here;
  GF_GROUP_STATS gf_stats;
  init_gf_stats(&gf_stats);
  while (count_cuts < max_intervals + 1) {
    // reaches next key frame, break here
    if (i >= rc->frames_to_key) {
      cut_here = 2;
    } else if (i - cur_start >= rc->static_scene_max_gf_interval) {
      // reached maximum len, but nothing special yet (almost static)
      // let's look at the next interval
      cut_here = 1;
    } else if (EOF == input_stats(twopass, &cpi->twopass_frame, &next_frame)) {
      // reaches last frame, break
      cut_here = 2;
    } else {
      // Test for the case where there is a brief flash but the prediction
      // quality back to an earlier frame is then restored.
      flash_detected = detect_flash(twopass, &cpi->twopass_frame, 0);
      // TODO(bohanli): remove redundant accumulations here, or unify
      // this and the ones in define_gf_group
      accumulate_next_frame_stats(&next_frame, flash_detected,
                                  rc->frames_since_key, i, &gf_stats, f_w, f_h);

      cut_here = detect_gf_cut(cpi, i, cur_start, flash_detected,
                               active_max_gf_interval, active_min_gf_interval,
                               &gf_stats);
    }
    if (cut_here) {
      cur_last = i - 1;  // the current last frame in the gf group
      int ori_last = cur_last;
      // The region frame idx does not start from the same frame as cur_start
      // and cur_last. Need to offset them.
      int offset = rc->frames_since_key - p_rc->regions_offset;
      REGIONS *regions = p_rc->regions;
      int num_regions = p_rc->num_regions;

      int scenecut_idx = -1;
      // only try shrinking if interval smaller than active_max_gf_interval
      if (cur_last - cur_start <= active_max_gf_interval &&
          cur_last > cur_start) {
        // find the region indices of where the first and last frame belong.
        int k_start =
            find_regions_index(regions, num_regions, cur_start + offset);
        int k_last =
            find_regions_index(regions, num_regions, cur_last + offset);
        if (cur_start + offset == 0) k_start = 0;

        // See if we have a scenecut in between
        for (int r = k_start + 1; r <= k_last; r++) {
          if (regions[r].type == SCENECUT_REGION &&
              regions[r].last - offset - cur_start > active_min_gf_interval) {
            scenecut_idx = r;
            break;
          }
        }

        // if the found scenecut is very close to the end, ignore it.
        if (regions[num_regions - 1].last - regions[scenecut_idx].last < 4) {
          scenecut_idx = -1;
        }

        if (scenecut_idx != -1) {
          // If we have a scenecut, then stop at it.
          // TODO(bohanli): add logic here to stop before the scenecut and for
          // the next gop start from the scenecut with GF
          int is_minor_sc =
              (regions[scenecut_idx].avg_cor_coeff *
                   (1 - stats[regions[scenecut_idx].start - offset].noise_var /
                            regions[scenecut_idx].avg_intra_err) >
               0.6);
          cur_last = regions[scenecut_idx].last - offset - !is_minor_sc;
        } else {
          int is_last_analysed = (k_last == num_regions - 1) &&
                                 (cur_last + offset == regions[k_last].last);
          int not_enough_regions =
              k_last - k_start <=
              1 + (regions[k_start].type == SCENECUT_REGION);
          // if we are very close to the end, then do not shrink since it may
          // introduce intervals that are too short
          if (!(is_last_analysed && not_enough_regions)) {
            const double arf_length_factor = 0.1;
            double best_score = 0;
            int best_j = -1;
            const int first_frame = regions[0].start - offset;
            const int last_frame = regions[num_regions - 1].last - offset;
            // score of how much the arf helps the whole GOP
            double base_score = 0.0;
            // Accumulate base_score in
            for (int j = cur_start + 1; j < cur_start + min_shrink_int; j++) {
              if (stats + j >= twopass->stats_buf_ctx->stats_in_end) break;
              base_score = (base_score + 1.0) * stats[j].cor_coeff;
            }
            int met_blending = 0;   // Whether we have met blending areas before
            int last_blending = 0;  // Whether the previous frame if blending
            for (int j = cur_start + min_shrink_int; j <= cur_last; j++) {
              if (stats + j >= twopass->stats_buf_ctx->stats_in_end) break;
              base_score = (base_score + 1.0) * stats[j].cor_coeff;
              int this_reg =
                  find_regions_index(regions, num_regions, j + offset);
              if (this_reg < 0) continue;
              // A GOP should include at most 1 blending region.
              if (regions[this_reg].type == BLENDING_REGION) {
                last_blending = 1;
                if (met_blending) {
                  break;
                } else {
                  base_score = 0;
                  continue;
                }
              } else {
                if (last_blending) met_blending = 1;
                last_blending = 0;
              }

              // Add the factor of how good the neighborhood is for this
              // candidate arf.
              double this_score = arf_length_factor * base_score;
              double temp_accu_coeff = 1.0;
              // following frames
              int count_f = 0;
              for (int n = j + 1; n <= j + 3 && n <= last_frame; n++) {
                if (stats + n >= twopass->stats_buf_ctx->stats_in_end) break;
                temp_accu_coeff *= stats[n].cor_coeff;
                this_score +=
                    temp_accu_coeff *
                    sqrt(AOMMAX(0.5,
                                1 - stats[n].noise_var /
                                        AOMMAX(stats[n].intra_error, 0.001)));
                count_f++;
              }
              // preceding frames
              temp_accu_coeff = 1.0;
              for (int n = j; n > j - 3 * 2 + count_f && n > first_frame; n--) {
                if (stats + n < twopass->stats_buf_ctx->stats_in_start) break;
                temp_accu_coeff *= stats[n].cor_coeff;
                this_score +=
                    temp_accu_coeff *
                    sqrt(AOMMAX(0.5,
                                1 - stats[n].noise_var /
                                        AOMMAX(stats[n].intra_error, 0.001)));
              }

              if (this_score > best_score) {
                best_score = this_score;
                best_j = j;
              }
            }

            // For blending areas, move one more frame in case we missed the
            // first blending frame.
            int best_reg =
                find_regions_index(regions, num_regions, best_j + offset);
            if (best_reg < num_regions - 1 && best_reg > 0) {
              if (regions[best_reg - 1].type == BLENDING_REGION &&
                  regions[best_reg + 1].type == BLENDING_REGION) {
                if (best_j + offset == regions[best_reg].start &&
                    best_j + offset < regions[best_reg].last) {
                  best_j += 1;
                } else if (best_j + offset == regions[best_reg].last &&
                           best_j + offset > regions[best_reg].start) {
                  best_j -= 1;
                }
              }
            }

            if (cur_last - best_j < 2) best_j = cur_last;
            if (best_j > 0 && best_score > 0.1) cur_last = best_j;
            // if cannot find anything, just cut at the original place.
          }
        }
      }
      cut_pos[count_cuts] = cur_last;
      count_cuts++;

      // reset pointers to the shrunken location
      cpi->twopass_frame.stats_in = start_pos + cur_last;
      cur_start = cur_last;
      int cur_region_idx =
          find_regions_index(regions, num_regions, cur_start + 1 + offset);
      if (cur_region_idx >= 0)
        if (regions[cur_region_idx].type == SCENECUT_REGION) cur_start++;

      i = cur_last;

      if (cut_here > 1 && cur_last == ori_last) break;

      // reset accumulators
      init_gf_stats(&gf_stats);
    }
    ++i;
  }

  // save intervals
  rc->intervals_till_gf_calculate_due = count_cuts - 1;
  for (int n = 1; n < count_cuts; n++) {
    p_rc->gf_intervals[n - 1] = cut_pos[n] - cut_pos[n - 1];
  }
  p_rc->cur_gf_index = 0;
  cpi->twopass_frame.stats_in = start_pos;
}

static void correct_frames_to_key(AV1_COMP *cpi) {
  int lookahead_size =
      (int)av1_lookahead_depth(cpi->ppi->lookahead, cpi->compressor_stage);
  if (lookahead_size <
      av1_lookahead_pop_sz(cpi->ppi->lookahead, cpi->compressor_stage)) {
    assert(
        IMPLIES(cpi->oxcf.pass != AOM_RC_ONE_PASS && cpi->ppi->frames_left > 0,
                lookahead_size == cpi->ppi->frames_left));
    cpi->rc.frames_to_key = AOMMIN(cpi->rc.frames_to_key, lookahead_size);
  } else if (cpi->ppi->frames_left > 0) {
    // Correct frames to key based on limit
    cpi->rc.frames_to_key =
        AOMMIN(cpi->rc.frames_to_key, cpi->ppi->frames_left);
  }
}

/*!\brief Define a GF group in one pass mode when no look ahead stats are
 * available.
 *
 * \ingroup gf_group_algo
 * This function defines the structure of a GF group, along with various
 * parameters regarding bit-allocation and quality setup in the special
 * case of one pass encoding where no lookahead stats are avialable.
 *
 * \param[in]    cpi             Top-level encoder structure
 *
 * \remark Nothing is returned. Instead, cpi->ppi->gf_group is changed.
 */
static void define_gf_group_pass0(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  GF_GROUP *const gf_group = &cpi->ppi->gf_group;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const GFConfig *const gf_cfg = &oxcf->gf_cfg;
  int target;

  if (oxcf->q_cfg.aq_mode == CYCLIC_REFRESH_AQ) {
    av1_cyclic_refresh_set_golden_update(cpi);
  } else {
    p_rc->baseline_gf_interval = p_rc->gf_intervals[p_rc->cur_gf_index];
    rc->intervals_till_gf_calculate_due--;
    p_rc->cur_gf_index++;
  }

  // correct frames_to_key when lookahead queue is flushing
  correct_frames_to_key(cpi);

  if (p_rc->baseline_gf_interval > rc->frames_to_key)
    p_rc->baseline_gf_interval = rc->frames_to_key;

  p_rc->gfu_boost = DEFAULT_GF_BOOST;
  p_rc->constrained_gf_group =
      (p_rc->baseline_gf_interval >= rc->frames_to_key) ? 1 : 0;

  gf_group->max_layer_depth_allowed = oxcf->gf_cfg.gf_max_pyr_height;

  // Rare case when the look-ahead is less than the target GOP length, can't
  // generate ARF frame.
  if (p_rc->baseline_gf_interval > gf_cfg->lag_in_frames ||
      !is_altref_enabled(gf_cfg->lag_in_frames, gf_cfg->enable_auto_arf) ||
      p_rc->baseline_gf_interval < rc->min_gf_interval)
    gf_group->max_layer_depth_allowed = 0;

  // Set up the structure of this Group-Of-Pictures (same as GF_GROUP)
  av1_gop_setup_structure(cpi);

  // Allocate bits to each of the frames in the GF group.
  // TODO(sarahparker) Extend this to work with pyramid structure.
  for (int cur_index = 0; cur_index < gf_group->size; ++cur_index) {
    const FRAME_UPDATE_TYPE cur_update_type = gf_group->update_type[cur_index];
    if (oxcf->rc_cfg.mode == AOM_CBR) {
      if (cur_update_type == KF_UPDATE) {
        target = av1_calc_iframe_target_size_one_pass_cbr(cpi);
      } else {
        target = av1_calc_pframe_target_size_one_pass_cbr(cpi, cur_update_type);
      }
    } else {
      if (cur_update_type == KF_UPDATE) {
        target = av1_calc_iframe_target_size_one_pass_vbr(cpi);
      } else {
        target = av1_calc_pframe_target_size_one_pass_vbr(cpi, cur_update_type);
      }
    }
    gf_group->bit_allocation[cur_index] = target;
  }
}

static INLINE void set_baseline_gf_interval(PRIMARY_RATE_CONTROL *p_rc,
                                            int arf_position) {
  p_rc->baseline_gf_interval = arf_position;
}

// initialize GF_GROUP_STATS
static void init_gf_stats(GF_GROUP_STATS *gf_stats) {
  gf_stats->gf_group_err = 0.0;
  gf_stats->gf_group_raw_error = 0.0;
  gf_stats->gf_group_skip_pct = 0.0;
  gf_stats->gf_group_inactive_zone_rows = 0.0;

  gf_stats->mv_ratio_accumulator = 0.0;
  gf_stats->decay_accumulator = 1.0;
  gf_stats->zero_motion_accumulator = 1.0;
  gf_stats->loop_decay_rate = 1.0;
  gf_stats->last_loop_decay_rate = 1.0;
  gf_stats->this_frame_mv_in_out = 0.0;
  gf_stats->mv_in_out_accumulator = 0.0;
  gf_stats->abs_mv_in_out_accumulator = 0.0;

  gf_stats->avg_sr_coded_error = 0.0;
  gf_stats->avg_pcnt_second_ref = 0.0;
  gf_stats->avg_new_mv_count = 0.0;
  gf_stats->avg_wavelet_energy = 0.0;
  gf_stats->avg_raw_err_stdev = 0.0;
  gf_stats->non_zero_stdev_count = 0;
}

static void accumulate_gop_stats(AV1_COMP *cpi, int is_intra_only, int f_w,
                                 int f_h, FIRSTPASS_STATS *next_frame,
                                 const FIRSTPASS_STATS *start_pos,
                                 GF_GROUP_STATS *gf_stats, int *idx) {
  int i, flash_detected;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  RATE_CONTROL *const rc = &cpi->rc;
  FRAME_INFO *frame_info = &cpi->frame_info;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  init_gf_stats(gf_stats);
  av1_zero(*next_frame);

  // If this is a key frame or the overlay from a previous arf then
  // the error score / cost of this frame has already been accounted for.
  i = is_intra_only;
  // get the determined gf group length from p_rc->gf_intervals
  while (i < p_rc->gf_intervals[p_rc->cur_gf_index]) {
    // read in the next frame
    if (EOF == input_stats(twopass, &cpi->twopass_frame, next_frame)) break;
    // Accumulate error score of frames in this gf group.
    double mod_frame_err =
        calculate_modified_err(frame_info, twopass, oxcf, next_frame);
    // accumulate stats for this frame
    accumulate_this_frame_stats(next_frame, mod_frame_err, gf_stats);
    ++i;
  }

  reset_fpf_position(&cpi->twopass_frame, start_pos);

  i = is_intra_only;
  input_stats(twopass, &cpi->twopass_frame, next_frame);
  while (i < p_rc->gf_intervals[p_rc->cur_gf_index]) {
    // read in the next frame
    if (EOF == input_stats(twopass, &cpi->twopass_frame, next_frame)) break;

    // Test for the case where there is a brief flash but the prediction
    // quality back to an earlier frame is then restored.
    flash_detected = detect_flash(twopass, &cpi->twopass_frame, 0);

    // accumulate stats for next frame
    accumulate_next_frame_stats(next_frame, flash_detected,
                                rc->frames_since_key, i, gf_stats, f_w, f_h);

    ++i;
  }

  i = p_rc->gf_intervals[p_rc->cur_gf_index];
  average_gf_stats(i, gf_stats);

  *idx = i;
}

static void update_gop_length(RATE_CONTROL *rc, PRIMARY_RATE_CONTROL *p_rc,
                              int idx, int is_final_pass) {
  if (is_final_pass) {
    rc->intervals_till_gf_calculate_due--;
    p_rc->cur_gf_index++;
  }

  // Was the group length constrained by the requirement for a new KF?
  p_rc->constrained_gf_group = (idx >= rc->frames_to_key) ? 1 : 0;

  set_baseline_gf_interval(p_rc, idx);
  rc->frames_till_gf_update_due = p_rc->baseline_gf_interval;
}

#define MAX_GF_BOOST 5400
#define REDUCE_GF_LENGTH_THRESH 4
#define REDUCE_GF_LENGTH_TO_KEY_THRESH 9
#define REDUCE_GF_LENGTH_BY 1
static void set_gop_bits_boost(AV1_COMP *cpi, int i, int is_intra_only,
                               int is_final_pass, int use_alt_ref,
                               int alt_offset, const FIRSTPASS_STATS *start_pos,
                               GF_GROUP_STATS *gf_stats) {
  // Should we use the alternate reference frame.
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  GF_GROUP *gf_group = &cpi->ppi->gf_group;
  FRAME_INFO *frame_info = &cpi->frame_info;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;

  int ext_len = i - is_intra_only;
  if (use_alt_ref) {
    const int forward_frames = (rc->frames_to_key - i >= ext_len)
                                   ? ext_len
                                   : AOMMAX(0, rc->frames_to_key - i);

    // Calculate the boost for alt ref.
    p_rc->gfu_boost = av1_calc_arf_boost(
        twopass, &cpi->twopass_frame, p_rc, frame_info, alt_offset,
        forward_frames, ext_len, &p_rc->num_stats_used_for_gfu_boost,
        &p_rc->num_stats_required_for_gfu_boost, cpi->ppi->lap_enabled);
  } else {
    reset_fpf_position(&cpi->twopass_frame, start_pos);
    p_rc->gfu_boost = AOMMIN(
        MAX_GF_BOOST,
        av1_calc_arf_boost(
            twopass, &cpi->twopass_frame, p_rc, frame_info, alt_offset, ext_len,
            0, &p_rc->num_stats_used_for_gfu_boost,
            &p_rc->num_stats_required_for_gfu_boost, cpi->ppi->lap_enabled));
  }

#define LAST_ALR_BOOST_FACTOR 0.2f
  p_rc->arf_boost_factor = 1.0;
  if (use_alt_ref && !is_lossless_requested(rc_cfg)) {
    // Reduce the boost of altref in the last gf group
    if (rc->frames_to_key - ext_len == REDUCE_GF_LENGTH_BY ||
        rc->frames_to_key - ext_len == 0) {
      p_rc->arf_boost_factor = LAST_ALR_BOOST_FACTOR;
    }
  }

  // Reset the file position.
  reset_fpf_position(&cpi->twopass_frame, start_pos);
  if (cpi->ppi->lap_enabled) {
    // Since we don't have enough stats to know the actual error of the
    // gf group, we assume error of each frame to be equal to 1 and set
    // the error of the group as baseline_gf_interval.
    gf_stats->gf_group_err = p_rc->baseline_gf_interval;
  }
  // Calculate the bits to be allocated to the gf/arf group as a whole
  p_rc->gf_group_bits =
      calculate_total_gf_group_bits(cpi, gf_stats->gf_group_err);

#if GROUP_ADAPTIVE_MAXQ
  // Calculate an estimate of the maxq needed for the group.
  // We are more aggressive about correcting for sections
  // where there could be significant overshoot than for easier
  // sections where we do not wish to risk creating an overshoot
  // of the allocated bit budget.
  if ((rc_cfg->mode != AOM_Q) && (p_rc->baseline_gf_interval > 1) &&
      is_final_pass) {
    const int vbr_group_bits_per_frame =
        (int)(p_rc->gf_group_bits / p_rc->baseline_gf_interval);
    const double group_av_err =
        gf_stats->gf_group_raw_error / p_rc->baseline_gf_interval;
    const double group_av_skip_pct =
        gf_stats->gf_group_skip_pct / p_rc->baseline_gf_interval;
    const double group_av_inactive_zone =
        ((gf_stats->gf_group_inactive_zone_rows * 2) /
         (p_rc->baseline_gf_interval * (double)cm->mi_params.mb_rows));

    int tmp_q;
    tmp_q = get_twopass_worst_quality(
        cpi, group_av_err, (group_av_skip_pct + group_av_inactive_zone),
        vbr_group_bits_per_frame);
    rc->active_worst_quality = AOMMAX(tmp_q, rc->active_worst_quality >> 1);
  }
#endif

  // Adjust KF group bits and error remaining.
  if (is_final_pass) twopass->kf_group_error_left -= gf_stats->gf_group_err;

  // Reset the file position.
  reset_fpf_position(&cpi->twopass_frame, start_pos);

  // Calculate a section intra ratio used in setting max loop filter.
  if (rc->frames_since_key != 0) {
    twopass->section_intra_rating = calculate_section_intra_ratio(
        start_pos, twopass->stats_buf_ctx->stats_in_end,
        p_rc->baseline_gf_interval);
  }

  av1_gop_bit_allocation(cpi, rc, gf_group, rc->frames_since_key == 0,
                         use_alt_ref, p_rc->gf_group_bits);

  // TODO(jingning): Generalize this condition.
  if (is_final_pass) {
    cpi->ppi->gf_state.arf_gf_boost_lst = use_alt_ref;

    // Reset rolling actual and target bits counters for ARF groups.
    twopass->rolling_arf_group_target_bits = 1;
    twopass->rolling_arf_group_actual_bits = 1;
  }
#if CONFIG_BITRATE_ACCURACY
  if (is_final_pass) {
    av1_vbr_rc_set_gop_bit_budget(&cpi->vbr_rc_info,
                                  p_rc->baseline_gf_interval);
  }
#endif
}

/*!\brief Define a GF group.
 *
 * \ingroup gf_group_algo
 * This function defines the structure of a GF group, along with various
 * parameters regarding bit-allocation and quality setup.
 *
 * \param[in]    cpi             Top-level encoder structure
 * \param[in]    frame_params    Structure with frame parameters
 * \param[in]    is_final_pass   Whether this is the final pass for the
 *                               GF group, or a trial (non-zero)
 *
 * \remark Nothing is returned. Instead, cpi->ppi->gf_group is changed.
 */
static void define_gf_group(AV1_COMP *cpi, EncodeFrameParams *frame_params,
                            int is_final_pass) {
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  FIRSTPASS_STATS next_frame;
  const FIRSTPASS_STATS *const start_pos = cpi->twopass_frame.stats_in;
  GF_GROUP *gf_group = &cpi->ppi->gf_group;
  const GFConfig *const gf_cfg = &oxcf->gf_cfg;
  const RateControlCfg *const rc_cfg = &oxcf->rc_cfg;
  const int f_w = cm->width;
  const int f_h = cm->height;
  int i;
  const int is_intra_only = rc->frames_since_key == 0;

  cpi->ppi->internal_altref_allowed = (gf_cfg->gf_max_pyr_height > 1);

  // Reset the GF group data structures unless this is a key
  // frame in which case it will already have been done.
  if (!is_intra_only) {
    av1_zero(cpi->ppi->gf_group);
    cpi->gf_frame_index = 0;
  }

  if (has_no_stats_stage(cpi)) {
    define_gf_group_pass0(cpi);
    return;
  }

  if (cpi->third_pass_ctx && oxcf->pass == AOM_RC_THIRD_PASS) {
    int ret = define_gf_group_pass3(cpi, frame_params, is_final_pass);
    if (ret == 0) return;

    av1_free_thirdpass_ctx(cpi->third_pass_ctx);
    cpi->third_pass_ctx = NULL;
  }

  // correct frames_to_key when lookahead queue is emptying
  if (cpi->ppi->lap_enabled) {
    correct_frames_to_key(cpi);
  }

  GF_GROUP_STATS gf_stats;
  accumulate_gop_stats(cpi, is_intra_only, f_w, f_h, &next_frame, start_pos,
                       &gf_stats, &i);

  const int can_disable_arf = !gf_cfg->gf_min_pyr_height;

  // If this is a key frame or the overlay from a previous arf then
  // the error score / cost of this frame has already been accounted for.
  const int active_min_gf_interval = rc->min_gf_interval;

  // Disable internal ARFs for "still" gf groups.
  //   zero_motion_accumulator: minimum percentage of (0,0) motion;
  //   avg_sr_coded_error:      average of the SSE per pixel of each frame;
  //   avg_raw_err_stdev:       average of the standard deviation of (0,0)
  //                            motion error per block of each frame.
  const int can_disable_internal_arfs = gf_cfg->gf_min_pyr_height <= 1;
  if (can_disable_internal_arfs &&
      gf_stats.zero_motion_accumulator > MIN_ZERO_MOTION &&
      gf_stats.avg_sr_coded_error < MAX_SR_CODED_ERROR &&
      gf_stats.avg_raw_err_stdev < MAX_RAW_ERR_VAR) {
    cpi->ppi->internal_altref_allowed = 0;
  }

  int use_alt_ref;
  if (can_disable_arf) {
    use_alt_ref =
        !is_almost_static(gf_stats.zero_motion_accumulator,
                          twopass->kf_zeromotion_pct, cpi->ppi->lap_enabled) &&
        p_rc->use_arf_in_this_kf_group && (i < gf_cfg->lag_in_frames) &&
        (i >= MIN_GF_INTERVAL);
  } else {
    use_alt_ref = p_rc->use_arf_in_this_kf_group &&
                  (i < gf_cfg->lag_in_frames) && (i > 2);
  }
  if (use_alt_ref) {
    gf_group->max_layer_depth_allowed = gf_cfg->gf_max_pyr_height;
  } else {
    gf_group->max_layer_depth_allowed = 0;
  }

  int alt_offset = 0;
  // The length reduction strategy is tweaked for certain cases, and doesn't
  // work well for certain other cases.
  const int allow_gf_length_reduction =
      ((rc_cfg->mode == AOM_Q && rc_cfg->cq_level <= 128) ||
       !cpi->ppi->internal_altref_allowed) &&
      !is_lossless_requested(rc_cfg);

  if (allow_gf_length_reduction && use_alt_ref) {
    // adjust length of this gf group if one of the following condition met
    // 1: only one overlay frame left and this gf is too long
    // 2: next gf group is too short to have arf compared to the current gf

    // maximum length of next gf group
    const int next_gf_len = rc->frames_to_key - i;
    const int single_overlay_left =
        next_gf_len == 0 && i > REDUCE_GF_LENGTH_THRESH;
    // the next gf is probably going to have a ARF but it will be shorter than
    // this gf
    const int unbalanced_gf =
        i > REDUCE_GF_LENGTH_TO_KEY_THRESH &&
        next_gf_len + 1 < REDUCE_GF_LENGTH_TO_KEY_THRESH &&
        next_gf_len + 1 >= rc->min_gf_interval;

    if (single_overlay_left || unbalanced_gf) {
      const int roll_back = REDUCE_GF_LENGTH_BY;
      // Reduce length only if active_min_gf_interval will be respected later.
      if (i - roll_back >= active_min_gf_interval + 1) {
        alt_offset = -roll_back;
        i -= roll_back;
        if (is_final_pass) rc->intervals_till_gf_calculate_due = 0;
        p_rc->gf_intervals[p_rc->cur_gf_index] -= roll_back;
        reset_fpf_position(&cpi->twopass_frame, start_pos);
        accumulate_gop_stats(cpi, is_intra_only, f_w, f_h, &next_frame,
                             start_pos, &gf_stats, &i);
      }
    }
  }

  update_gop_length(rc, p_rc, i, is_final_pass);

  // Set up the structure of this Group-Of-Pictures (same as GF_GROUP)
  av1_gop_setup_structure(cpi);

  set_gop_bits_boost(cpi, i, is_intra_only, is_final_pass, use_alt_ref,
                     alt_offset, start_pos, &gf_stats);

  frame_params->frame_type =
      rc->frames_since_key == 0 ? KEY_FRAME : INTER_FRAME;
  frame_params->show_frame =
      !(gf_group->update_type[cpi->gf_frame_index] == ARF_UPDATE ||
        gf_group->update_type[cpi->gf_frame_index] == INTNL_ARF_UPDATE);
}

/*!\brief Define a GF group for the third apss.
 *
 * \ingroup gf_group_algo
 * This function defines the structure of a GF group for the third pass, along
 * with various parameters regarding bit-allocation and quality setup based on
 * the two-pass bitstream.
 * Much of the function still uses the strategies used for the second pass and
 * relies on first pass statistics. It is expected that over time these portions
 * would be replaced with strategies specific to the third pass.
 *
 * \param[in]    cpi             Top-level encoder structure
 * \param[in]    frame_params    Structure with frame parameters
 * \param[in]    is_final_pass   Whether this is the final pass for the
 *                               GF group, or a trial (non-zero)
 *
 * \return       0: Success;
 *              -1: There are conflicts between the bitstream and current config
 *               The values in cpi->ppi->gf_group are also changed.
 */
static int define_gf_group_pass3(AV1_COMP *cpi, EncodeFrameParams *frame_params,
                                 int is_final_pass) {
  if (!cpi->third_pass_ctx) return -1;
  AV1_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  FIRSTPASS_STATS next_frame;
  const FIRSTPASS_STATS *const start_pos = cpi->twopass_frame.stats_in;
  GF_GROUP *gf_group = &cpi->ppi->gf_group;
  const GFConfig *const gf_cfg = &oxcf->gf_cfg;
  const int f_w = cm->width;
  const int f_h = cm->height;
  int i;
  const int is_intra_only = rc->frames_since_key == 0;

  cpi->ppi->internal_altref_allowed = (gf_cfg->gf_max_pyr_height > 1);

  // Reset the GF group data structures unless this is a key
  // frame in which case it will already have been done.
  if (!is_intra_only) {
    av1_zero(cpi->ppi->gf_group);
    cpi->gf_frame_index = 0;
  }

  GF_GROUP_STATS gf_stats;
  accumulate_gop_stats(cpi, is_intra_only, f_w, f_h, &next_frame, start_pos,
                       &gf_stats, &i);

  const int can_disable_arf = !gf_cfg->gf_min_pyr_height;

  // TODO(any): set cpi->ppi->internal_altref_allowed accordingly;

  int use_alt_ref = av1_check_use_arf(cpi->third_pass_ctx);
  if (use_alt_ref == 0 && !can_disable_arf) return -1;
  if (use_alt_ref) {
    gf_group->max_layer_depth_allowed = gf_cfg->gf_max_pyr_height;
  } else {
    gf_group->max_layer_depth_allowed = 0;
  }

  update_gop_length(rc, p_rc, i, is_final_pass);

  // Set up the structure of this Group-Of-Pictures (same as GF_GROUP)
  av1_gop_setup_structure(cpi);

  set_gop_bits_boost(cpi, i, is_intra_only, is_final_pass, use_alt_ref, 0,
                     start_pos, &gf_stats);

  frame_params->frame_type = cpi->third_pass_ctx->frame_info[0].frame_type;
  frame_params->show_frame = cpi->third_pass_ctx->frame_info[0].is_show_frame;
  return 0;
}

// #define FIXED_ARF_BITS
#ifdef FIXED_ARF_BITS
#define ARF_BITS_FRACTION 0.75
#endif
void av1_gop_bit_allocation(const AV1_COMP *cpi, RATE_CONTROL *const rc,
                            GF_GROUP *gf_group, int is_key_frame, int use_arf,
                            int64_t gf_group_bits) {
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  // Calculate the extra bits to be used for boosted frame(s)
#ifdef FIXED_ARF_BITS
  int gf_arf_bits = (int)(ARF_BITS_FRACTION * gf_group_bits);
#else
  int gf_arf_bits = calculate_boost_bits(
      p_rc->baseline_gf_interval - (rc->frames_since_key == 0), p_rc->gfu_boost,
      gf_group_bits);
#endif

  gf_arf_bits = adjust_boost_bits_for_target_level(cpi, rc, gf_arf_bits,
                                                   gf_group_bits, 1);

  // Allocate bits to each of the frames in the GF group.
  allocate_gf_group_bits(gf_group, p_rc, rc, gf_group_bits, gf_arf_bits,
                         is_key_frame, use_arf);
}

// Minimum % intra coding observed in first pass (1.0 = 100%)
#define MIN_INTRA_LEVEL 0.25
// Minimum ratio between the % of intra coding and inter coding in the first
// pass after discounting neutral blocks (discounting neutral blocks in this
// way helps catch scene cuts in clips with very flat areas or letter box
// format clips with image padding.
#define INTRA_VS_INTER_THRESH 2.0
// Hard threshold where the first pass chooses intra for almost all blocks.
// In such a case even if the frame is not a scene cut coding a key frame
// may be a good option.
#define VERY_LOW_INTER_THRESH 0.05
// Maximum threshold for the relative ratio of intra error score vs best
// inter error score.
#define KF_II_ERR_THRESHOLD 1.9
// In real scene cuts there is almost always a sharp change in the intra
// or inter error score.
#define ERR_CHANGE_THRESHOLD 0.4
// For real scene cuts we expect an improvment in the intra inter error
// ratio in the next frame.
#define II_IMPROVEMENT_THRESHOLD 3.5
#define KF_II_MAX 128.0
// Intra / Inter threshold very low
#define VERY_LOW_II 1.5
// Clean slide transitions we expect a sharp single frame spike in error.
#define ERROR_SPIKE 5.0

// Slide show transition detection.
// Tests for case where there is very low error either side of the current frame
// but much higher just for this frame. This can help detect key frames in
// slide shows even where the slides are pictures of different sizes.
// Also requires that intra and inter errors are very similar to help eliminate
// harmful false positives.
// It will not help if the transition is a fade or other multi-frame effect.
static int slide_transition(const FIRSTPASS_STATS *this_frame,
                            const FIRSTPASS_STATS *last_frame,
                            const FIRSTPASS_STATS *next_frame) {
  return (this_frame->intra_error < (this_frame->coded_error * VERY_LOW_II)) &&
         (this_frame->coded_error > (last_frame->coded_error * ERROR_SPIKE)) &&
         (this_frame->coded_error > (next_frame->coded_error * ERROR_SPIKE));
}

// Threshold for use of the lagging second reference frame. High second ref
// usage may point to a transient event like a flash or occlusion rather than
// a real scene cut.
// We adapt the threshold based on number of frames in this key-frame group so
// far.
static double get_second_ref_usage_thresh(int frame_count_so_far) {
  const int adapt_upto = 32;
  const double min_second_ref_usage_thresh = 0.085;
  const double second_ref_usage_thresh_max_delta = 0.035;
  if (frame_count_so_far >= adapt_upto) {
    return min_second_ref_usage_thresh + second_ref_usage_thresh_max_delta;
  }
  return min_second_ref_usage_thresh +
         ((double)frame_count_so_far / (adapt_upto - 1)) *
             second_ref_usage_thresh_max_delta;
}

static int test_candidate_kf(const FIRSTPASS_INFO *firstpass_info,
                             int this_stats_index, int frame_count_so_far,
                             enum aom_rc_mode rc_mode, int scenecut_mode,
                             int num_mbs) {
  const FIRSTPASS_STATS *last_stats =
      av1_firstpass_info_peek(firstpass_info, this_stats_index - 1);
  const FIRSTPASS_STATS *this_stats =
      av1_firstpass_info_peek(firstpass_info, this_stats_index);
  const FIRSTPASS_STATS *next_stats =
      av1_firstpass_info_peek(firstpass_info, this_stats_index + 1);
  if (last_stats == NULL || this_stats == NULL || next_stats == NULL) {
    return 0;
  }

  int is_viable_kf = 0;
  double pcnt_intra = 1.0 - this_stats->pcnt_inter;
  double modified_pcnt_inter =
      this_stats->pcnt_inter - this_stats->pcnt_neutral;
  const double second_ref_usage_thresh =
      get_second_ref_usage_thresh(frame_count_so_far);
  int frames_to_test_after_candidate_key = SCENE_CUT_KEY_TEST_INTERVAL;
  int count_for_tolerable_prediction = 3;

  // We do "-1" because the candidate key is not counted.
  int stats_after_this_stats =
      av1_firstpass_info_future_count(firstpass_info, this_stats_index) - 1;

  if (scenecut_mode == ENABLE_SCENECUT_MODE_1) {
    if (stats_after_this_stats < 3) {
      return 0;
    } else {
      frames_to_test_after_candidate_key = 3;
      count_for_tolerable_prediction = 1;
    }
  }
  // Make sure we have enough stats after the candidate key.
  frames_to_test_after_candidate_key =
      AOMMIN(frames_to_test_after_candidate_key, stats_after_this_stats);

  // Does the frame satisfy the primary criteria of a key frame?
  // See above for an explanation of the test criteria.
  // If so, then examine how well it predicts subsequent frames.
  if (IMPLIES(rc_mode == AOM_Q, frame_count_so_far >= 3) &&
      (this_stats->pcnt_second_ref < second_ref_usage_thresh) &&
      (next_stats->pcnt_second_ref < second_ref_usage_thresh) &&
      ((this_stats->pcnt_inter < VERY_LOW_INTER_THRESH) ||
       slide_transition(this_stats, last_stats, next_stats) ||
       ((pcnt_intra > MIN_INTRA_LEVEL) &&
        (pcnt_intra > (INTRA_VS_INTER_THRESH * modified_pcnt_inter)) &&
        ((this_stats->intra_error /
          DOUBLE_DIVIDE_CHECK(this_stats->coded_error)) <
         KF_II_ERR_THRESHOLD) &&
        ((fabs(last_stats->coded_error - this_stats->coded_error) /
              DOUBLE_DIVIDE_CHECK(this_stats->coded_error) >
          ERR_CHANGE_THRESHOLD) ||
         (fabs(last_stats->intra_error - this_stats->intra_error) /
              DOUBLE_DIVIDE_CHECK(this_stats->intra_error) >
          ERR_CHANGE_THRESHOLD) ||
         ((next_stats->intra_error /
           DOUBLE_DIVIDE_CHECK(next_stats->coded_error)) >
          II_IMPROVEMENT_THRESHOLD))))) {
    int i;
    double boost_score = 0.0;
    double old_boost_score = 0.0;
    double decay_accumulator = 1.0;

    // Examine how well the key frame predicts subsequent frames.
    for (i = 1; i <= frames_to_test_after_candidate_key; ++i) {
      // Get the next frame details
      const FIRSTPASS_STATS *local_next_frame =
          av1_firstpass_info_peek(firstpass_info, this_stats_index + i);
      double next_iiratio =
          (BOOST_FACTOR * local_next_frame->intra_error /
           DOUBLE_DIVIDE_CHECK(local_next_frame->coded_error));

      if (next_iiratio > KF_II_MAX) next_iiratio = KF_II_MAX;

      // Cumulative effect of decay in prediction quality.
      if (local_next_frame->pcnt_inter > 0.85)
        decay_accumulator *= local_next_frame->pcnt_inter;
      else
        decay_accumulator *= (0.85 + local_next_frame->pcnt_inter) / 2.0;

      // Keep a running total.
      boost_score += (decay_accumulator * next_iiratio);

      // Test various breakout clauses.
      // TODO(any): Test of intra error should be normalized to an MB.
      if ((local_next_frame->pcnt_inter < 0.05) || (next_iiratio < 1.5) ||
          (((local_next_frame->pcnt_inter - local_next_frame->pcnt_neutral) <
            0.20) &&
           (next_iiratio < 3.0)) ||
          ((boost_score - old_boost_score) < 3.0) ||
          (local_next_frame->intra_error < (200.0 / (double)num_mbs))) {
        break;
      }

      old_boost_score = boost_score;
    }

    // If there is tolerable prediction for at least the next 3 frames then
    // break out else discard this potential key frame and move on
    if (boost_score > 30.0 && (i > count_for_tolerable_prediction)) {
      is_viable_kf = 1;
    } else {
      is_viable_kf = 0;
    }
  }
  return is_viable_kf;
}

#define FRAMES_TO_CHECK_DECAY 8
#define KF_MIN_FRAME_BOOST 80.0
#define KF_MAX_FRAME_BOOST 128.0
#define MIN_KF_BOOST 600  // Minimum boost for non-static KF interval
#define MAX_KF_BOOST 3200
#define MIN_STATIC_KF_BOOST 5400  // Minimum boost for static KF interval

static int detect_app_forced_key(AV1_COMP *cpi) {
  int num_frames_to_app_forced_key = is_forced_keyframe_pending(
      cpi->ppi->lookahead, cpi->ppi->lookahead->max_sz, cpi->compressor_stage);
  return num_frames_to_app_forced_key;
}

static int get_projected_kf_boost(AV1_COMP *cpi) {
  /*
   * If num_stats_used_for_kf_boost >= frames_to_key, then
   * all stats needed for prior boost calculation are available.
   * Hence projecting the prior boost is not needed in this cases.
   */
  if (cpi->ppi->p_rc.num_stats_used_for_kf_boost >= cpi->rc.frames_to_key)
    return cpi->ppi->p_rc.kf_boost;

  // Get the current tpl factor (number of frames = frames_to_key).
  double tpl_factor = av1_get_kf_boost_projection_factor(cpi->rc.frames_to_key);
  // Get the tpl factor when number of frames = num_stats_used_for_kf_boost.
  double tpl_factor_num_stats = av1_get_kf_boost_projection_factor(
      cpi->ppi->p_rc.num_stats_used_for_kf_boost);
  int projected_kf_boost =
      (int)rint((tpl_factor * cpi->ppi->p_rc.kf_boost) / tpl_factor_num_stats);
  return projected_kf_boost;
}

/*!\brief Determine the location of the next key frame
 *
 * \ingroup gf_group_algo
 * This function decides the placement of the next key frame when a
 * scenecut is detected or the maximum key frame distance is reached.
 *
 * \param[in]    cpi              Top-level encoder structure
 * \param[in]    firstpass_info   struct for firstpass info
 * \param[in]    num_frames_to_detect_scenecut Maximum lookahead frames.
 * \param[in]    search_start_idx   the start index for searching key frame.
 *                                  Set it to one if we already know the
 *                                  current frame is key frame. Otherwise,
 *                                  set it to zero.
 *
 * \return       Number of frames to the next key including the current frame.
 */
static int define_kf_interval(AV1_COMP *cpi,
                              const FIRSTPASS_INFO *firstpass_info,
                              int num_frames_to_detect_scenecut,
                              int search_start_idx) {
  const TWO_PASS *const twopass = &cpi->ppi->twopass;
  const RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const KeyFrameCfg *const kf_cfg = &oxcf->kf_cfg;
  double recent_loop_decay[FRAMES_TO_CHECK_DECAY];
  double decay_accumulator = 1.0;
  int i = 0, j;
  int frames_to_key = search_start_idx;
  int frames_since_key = rc->frames_since_key + 1;
  int scenecut_detected = 0;

  int num_frames_to_next_key = detect_app_forced_key(cpi);

  if (num_frames_to_detect_scenecut == 0) {
    if (num_frames_to_next_key != -1)
      return num_frames_to_next_key;
    else
      return rc->frames_to_key;
  }

  if (num_frames_to_next_key != -1)
    num_frames_to_detect_scenecut =
        AOMMIN(num_frames_to_detect_scenecut, num_frames_to_next_key);

  // Initialize the decay rates for the recent frames to check
  for (j = 0; j < FRAMES_TO_CHECK_DECAY; ++j) recent_loop_decay[j] = 1.0;

  i = 0;
  const int num_mbs = (oxcf->resize_cfg.resize_mode != RESIZE_NONE)
                          ? cpi->initial_mbs
                          : cpi->common.mi_params.MBs;
  const int future_stats_count =
      av1_firstpass_info_future_count(firstpass_info, 0);
  while (frames_to_key < future_stats_count &&
         frames_to_key < num_frames_to_detect_scenecut) {
    // Provided that we are not at the end of the file...
    if ((cpi->ppi->p_rc.enable_scenecut_detection > 0) && kf_cfg->auto_key &&
        frames_to_key + 1 < future_stats_count) {
      double loop_decay_rate;

      // Check for a scene cut.
      if (frames_since_key >= kf_cfg->key_freq_min) {
        scenecut_detected = test_candidate_kf(
            &twopass->firstpass_info, frames_to_key, frames_since_key,
            oxcf->rc_cfg.mode, cpi->ppi->p_rc.enable_scenecut_detection,
            num_mbs);
        if (scenecut_detected) {
          break;
        }
      }

      // How fast is the prediction quality decaying?
      const FIRSTPASS_STATS *next_stats =
          av1_firstpass_info_peek(firstpass_info, frames_to_key + 1);
      loop_decay_rate = get_prediction_decay_rate(next_stats);

      // We want to know something about the recent past... rather than
      // as used elsewhere where we are concerned with decay in prediction
      // quality since the last GF or KF.
      recent_loop_decay[i % FRAMES_TO_CHECK_DECAY] = loop_decay_rate;
      decay_accumulator = 1.0;
      for (j = 0; j < FRAMES_TO_CHECK_DECAY; ++j)
        decay_accumulator *= recent_loop_decay[j];

      // Special check for transition or high motion followed by a
      // static scene.
      if (frames_since_key >= kf_cfg->key_freq_min) {
        scenecut_detected = detect_transition_to_still(
            firstpass_info, frames_to_key + 1, rc->min_gf_interval, i,
            kf_cfg->key_freq_max - i, loop_decay_rate, decay_accumulator);
        if (scenecut_detected) {
          // In the case of transition followed by a static scene, the key frame
          // could be a good predictor for the following frames, therefore we
          // do not use an arf.
          p_rc->use_arf_in_this_kf_group = 0;
          break;
        }
      }

      // Step on to the next frame.
      ++frames_to_key;
      ++frames_since_key;

      // If we don't have a real key frame within the next two
      // key_freq_max intervals then break out of the loop.
      if (frames_to_key >= 2 * kf_cfg->key_freq_max) {
        break;
      }
    } else {
      ++frames_to_key;
      ++frames_since_key;
    }
    ++i;
  }
  if (cpi->ppi->lap_enabled && !scenecut_detected)
    frames_to_key = num_frames_to_next_key;

  return frames_to_key;
}

static double get_kf_group_avg_error(TWO_PASS *twopass,
                                     TWO_PASS_FRAME *twopass_frame,
                                     const FIRSTPASS_STATS *first_frame,
                                     const FIRSTPASS_STATS *start_position,
                                     int frames_to_key) {
  FIRSTPASS_STATS cur_frame = *first_frame;
  int num_frames, i;
  double kf_group_avg_error = 0.0;

  reset_fpf_position(twopass_frame, start_position);

  for (i = 0; i < frames_to_key; ++i) {
    kf_group_avg_error += cur_frame.coded_error;
    if (EOF == input_stats(twopass, twopass_frame, &cur_frame)) break;
  }
  num_frames = i + 1;
  num_frames = AOMMIN(num_frames, frames_to_key);
  kf_group_avg_error = kf_group_avg_error / num_frames;

  return (kf_group_avg_error);
}

static int64_t get_kf_group_bits(AV1_COMP *cpi, double kf_group_err,
                                 double kf_group_avg_error) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  int64_t kf_group_bits;
  if (cpi->ppi->lap_enabled) {
    kf_group_bits = (int64_t)rc->frames_to_key * rc->avg_frame_bandwidth;
    if (cpi->oxcf.rc_cfg.vbr_corpus_complexity_lap) {
      double vbr_corpus_complexity_lap =
          cpi->oxcf.rc_cfg.vbr_corpus_complexity_lap / 10.0;
      /* Get the average corpus complexity of the frame */
      kf_group_bits = (int64_t)(
          kf_group_bits * (kf_group_avg_error / vbr_corpus_complexity_lap));
    }
  } else {
    kf_group_bits = (int64_t)(twopass->bits_left *
                              (kf_group_err / twopass->modified_error_left));
  }

  return kf_group_bits;
}

static int calc_avg_stats(AV1_COMP *cpi, FIRSTPASS_STATS *avg_frame_stat) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  FIRSTPASS_STATS cur_frame;
  av1_zero(cur_frame);
  int num_frames = 0;
  // Accumulate total stat using available number of stats.
  for (num_frames = 0; num_frames < (rc->frames_to_key - 1); ++num_frames) {
    if (EOF == input_stats(twopass, &cpi->twopass_frame, &cur_frame)) break;
    av1_accumulate_stats(avg_frame_stat, &cur_frame);
  }

  if (num_frames < 2) {
    return num_frames;
  }
  // Average the total stat
  avg_frame_stat->weight = avg_frame_stat->weight / num_frames;
  avg_frame_stat->intra_error = avg_frame_stat->intra_error / num_frames;
  avg_frame_stat->frame_avg_wavelet_energy =
      avg_frame_stat->frame_avg_wavelet_energy / num_frames;
  avg_frame_stat->coded_error = avg_frame_stat->coded_error / num_frames;
  avg_frame_stat->sr_coded_error = avg_frame_stat->sr_coded_error / num_frames;
  avg_frame_stat->pcnt_inter = avg_frame_stat->pcnt_inter / num_frames;
  avg_frame_stat->pcnt_motion = avg_frame_stat->pcnt_motion / num_frames;
  avg_frame_stat->pcnt_second_ref =
      avg_frame_stat->pcnt_second_ref / num_frames;
  avg_frame_stat->pcnt_neutral = avg_frame_stat->pcnt_neutral / num_frames;
  avg_frame_stat->intra_skip_pct = avg_frame_stat->intra_skip_pct / num_frames;
  avg_frame_stat->inactive_zone_rows =
      avg_frame_stat->inactive_zone_rows / num_frames;
  avg_frame_stat->inactive_zone_cols =
      avg_frame_stat->inactive_zone_cols / num_frames;
  avg_frame_stat->MVr = avg_frame_stat->MVr / num_frames;
  avg_frame_stat->mvr_abs = avg_frame_stat->mvr_abs / num_frames;
  avg_frame_stat->MVc = avg_frame_stat->MVc / num_frames;
  avg_frame_stat->mvc_abs = avg_frame_stat->mvc_abs / num_frames;
  avg_frame_stat->MVrv = avg_frame_stat->MVrv / num_frames;
  avg_frame_stat->MVcv = avg_frame_stat->MVcv / num_frames;
  avg_frame_stat->mv_in_out_count =
      avg_frame_stat->mv_in_out_count / num_frames;
  avg_frame_stat->new_mv_count = avg_frame_stat->new_mv_count / num_frames;
  avg_frame_stat->count = avg_frame_stat->count / num_frames;
  avg_frame_stat->duration = avg_frame_stat->duration / num_frames;

  return num_frames;
}

static double get_kf_boost_score(AV1_COMP *cpi, double kf_raw_err,
                                 double *zero_motion_accumulator,
                                 double *sr_accumulator, int use_avg_stat) {
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  FIRSTPASS_STATS frame_stat;
  av1_zero(frame_stat);
  int i = 0, num_stat_used = 0;
  double boost_score = 0.0;
  const double kf_max_boost =
      cpi->oxcf.rc_cfg.mode == AOM_Q
          ? AOMMIN(AOMMAX(rc->frames_to_key * 2.0, KF_MIN_FRAME_BOOST),
                   KF_MAX_FRAME_BOOST)
          : KF_MAX_FRAME_BOOST;

  // Calculate the average using available number of stats.
  if (use_avg_stat) num_stat_used = calc_avg_stats(cpi, &frame_stat);

  for (i = num_stat_used; i < (rc->frames_to_key - 1); ++i) {
    if (!use_avg_stat &&
        EOF == input_stats(twopass, &cpi->twopass_frame, &frame_stat))
      break;

    // Monitor for static sections.
    // For the first frame in kf group, the second ref indicator is invalid.
    if (i > 0) {
      *zero_motion_accumulator =
          AOMMIN(*zero_motion_accumulator, get_zero_motion_factor(&frame_stat));
    } else {
      *zero_motion_accumulator = frame_stat.pcnt_inter - frame_stat.pcnt_motion;
    }

    // Not all frames in the group are necessarily used in calculating boost.
    if ((*sr_accumulator < (kf_raw_err * 1.50)) &&
        (i <= rc->max_gf_interval * 2)) {
      double frame_boost;
      double zm_factor;

      // Factor 0.75-1.25 based on how much of frame is static.
      zm_factor = (0.75 + (*zero_motion_accumulator / 2.0));

      if (i < 2) *sr_accumulator = 0.0;
      frame_boost =
          calc_kf_frame_boost(&cpi->ppi->p_rc, frame_info, &frame_stat,
                              sr_accumulator, kf_max_boost);
      boost_score += frame_boost * zm_factor;
    }
  }
  return boost_score;
}

/*!\brief Interval(in seconds) to clip key-frame distance to in LAP.
 */
#define MAX_KF_BITS_INTERVAL_SINGLE_PASS 5

/*!\brief Determine the next key frame group
 *
 * \ingroup gf_group_algo
 * This function decides the placement of the next key frame, and
 * calculates the bit allocation of the KF group and the keyframe itself.
 *
 * \param[in]    cpi              Top-level encoder structure
 * \param[in]    this_frame       Pointer to first pass stats
 */
static void find_next_key_frame(AV1_COMP *cpi, FIRSTPASS_STATS *this_frame) {
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  GF_GROUP *const gf_group = &cpi->ppi->gf_group;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  AV1_COMMON *const cm = &cpi->common;
  CurrentFrame *const current_frame = &cm->current_frame;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const KeyFrameCfg *const kf_cfg = &oxcf->kf_cfg;
  const FIRSTPASS_STATS first_frame = *this_frame;
  FIRSTPASS_STATS next_frame;
  const FIRSTPASS_INFO *firstpass_info = &twopass->firstpass_info;
  av1_zero(next_frame);

  rc->frames_since_key = 0;
  // Use arfs if possible.
  p_rc->use_arf_in_this_kf_group = is_altref_enabled(
      oxcf->gf_cfg.lag_in_frames, oxcf->gf_cfg.enable_auto_arf);

  // Reset the GF group data structures.
  av1_zero(*gf_group);
  cpi->gf_frame_index = 0;

  // KF is always a GF so clear frames till next gf counter.
  rc->frames_till_gf_update_due = 0;

  if (has_no_stats_stage(cpi)) {
    int num_frames_to_app_forced_key = detect_app_forced_key(cpi);
    p_rc->this_key_frame_forced =
        current_frame->frame_number != 0 && rc->frames_to_key == 0;
    if (num_frames_to_app_forced_key != -1)
      rc->frames_to_key = num_frames_to_app_forced_key;
    else
      rc->frames_to_key = AOMMAX(1, kf_cfg->key_freq_max);
    correct_frames_to_key(cpi);
    p_rc->kf_boost = DEFAULT_KF_BOOST;
    gf_group->update_type[0] = KF_UPDATE;
    return;
  }
  int i;
  const FIRSTPASS_STATS *const start_position = cpi->twopass_frame.stats_in;
  int kf_bits = 0;
  double zero_motion_accumulator = 1.0;
  double boost_score = 0.0;
  double kf_raw_err = 0.0;
  double kf_mod_err = 0.0;
  double sr_accumulator = 0.0;
  double kf_group_avg_error = 0.0;
  int frames_to_key, frames_to_key_clipped = INT_MAX;
  int64_t kf_group_bits_clipped = INT64_MAX;

  // Is this a forced key frame by interval.
  p_rc->this_key_frame_forced = p_rc->next_key_frame_forced;

  twopass->kf_group_bits = 0;        // Total bits available to kf group
  twopass->kf_group_error_left = 0;  // Group modified error score.

  kf_raw_err = this_frame->intra_error;
  kf_mod_err = calculate_modified_err(frame_info, twopass, oxcf, this_frame);

  // We assume the current frame is a key frame and we are looking for the next
  // key frame. Therefore search_start_idx = 1
  frames_to_key = define_kf_interval(cpi, firstpass_info, kf_cfg->key_freq_max,
                                     /*search_start_idx=*/1);

  if (frames_to_key != -1) {
    rc->frames_to_key = AOMMIN(kf_cfg->key_freq_max, frames_to_key);
  } else {
    rc->frames_to_key = kf_cfg->key_freq_max;
  }

  if (cpi->ppi->lap_enabled) correct_frames_to_key(cpi);

  // If there is a max kf interval set by the user we must obey it.
  // We already breakout of the loop above at 2x max.
  // This code centers the extra kf if the actual natural interval
  // is between 1x and 2x.
  if (kf_cfg->auto_key && rc->frames_to_key > kf_cfg->key_freq_max) {
    FIRSTPASS_STATS tmp_frame = first_frame;

    rc->frames_to_key /= 2;

    // Reset to the start of the group.
    reset_fpf_position(&cpi->twopass_frame, start_position);
    // Rescan to get the correct error data for the forced kf group.
    for (i = 0; i < rc->frames_to_key; ++i) {
      if (EOF == input_stats(twopass, &cpi->twopass_frame, &tmp_frame)) break;
    }
    p_rc->next_key_frame_forced = 1;
  } else if ((cpi->twopass_frame.stats_in ==
                  twopass->stats_buf_ctx->stats_in_end &&
              is_stat_consumption_stage_twopass(cpi)) ||
             rc->frames_to_key >= kf_cfg->key_freq_max) {
    p_rc->next_key_frame_forced = 1;
  } else {
    p_rc->next_key_frame_forced = 0;
  }

  double kf_group_err = 0;
  for (i = 0; i < rc->frames_to_key; ++i) {
    const FIRSTPASS_STATS *this_stats =
        av1_firstpass_info_peek(&twopass->firstpass_info, i);
    if (this_stats != NULL) {
      // Accumulate kf group error.
      kf_group_err += calculate_modified_err_new(
          frame_info, &firstpass_info->total_stats, this_stats,
          oxcf->rc_cfg.vbrbias, twopass->modified_error_min,
          twopass->modified_error_max);
      ++p_rc->num_stats_used_for_kf_boost;
    }
  }

  // Calculate the number of bits that should be assigned to the kf group.
  if ((twopass->bits_left > 0 && twopass->modified_error_left > 0.0) ||
      (cpi->ppi->lap_enabled && oxcf->rc_cfg.mode != AOM_Q)) {
    // Maximum number of bits for a single normal frame (not key frame).
    const int max_bits = frame_max_bits(rc, oxcf);

    // Maximum number of bits allocated to the key frame group.
    int64_t max_grp_bits;

    if (oxcf->rc_cfg.vbr_corpus_complexity_lap) {
      kf_group_avg_error =
          get_kf_group_avg_error(twopass, &cpi->twopass_frame, &first_frame,
                                 start_position, rc->frames_to_key);
    }

    // Default allocation based on bits left and relative
    // complexity of the section.
    twopass->kf_group_bits =
        get_kf_group_bits(cpi, kf_group_err, kf_group_avg_error);
    // Clip based on maximum per frame rate defined by the user.
    max_grp_bits = (int64_t)max_bits * (int64_t)rc->frames_to_key;
    if (twopass->kf_group_bits > max_grp_bits)
      twopass->kf_group_bits = max_grp_bits;
  } else {
    twopass->kf_group_bits = 0;
  }
  twopass->kf_group_bits = AOMMAX(0, twopass->kf_group_bits);

  if (cpi->ppi->lap_enabled) {
    // In the case of single pass based on LAP, frames to  key may have an
    // inaccurate value, and hence should be clipped to an appropriate
    // interval.
    frames_to_key_clipped =
        (int)(MAX_KF_BITS_INTERVAL_SINGLE_PASS * cpi->framerate);

    // This variable calculates the bits allocated to kf_group with a clipped
    // frames_to_key.
    if (rc->frames_to_key > frames_to_key_clipped) {
      kf_group_bits_clipped =
          (int64_t)((double)twopass->kf_group_bits * frames_to_key_clipped /
                    rc->frames_to_key);
    }
  }

  // Reset the first pass file position.
  reset_fpf_position(&cpi->twopass_frame, start_position);

  // Scan through the kf group collating various stats used to determine
  // how many bits to spend on it.
  boost_score = get_kf_boost_score(cpi, kf_raw_err, &zero_motion_accumulator,
                                   &sr_accumulator, 0);
  reset_fpf_position(&cpi->twopass_frame, start_position);
  // Store the zero motion percentage
  twopass->kf_zeromotion_pct = (int)(zero_motion_accumulator * 100.0);

  // Calculate a section intra ratio used in setting max loop filter.
  twopass->section_intra_rating = calculate_section_intra_ratio(
      start_position, twopass->stats_buf_ctx->stats_in_end, rc->frames_to_key);

  p_rc->kf_boost = (int)boost_score;

  if (cpi->ppi->lap_enabled) {
    if (oxcf->rc_cfg.mode == AOM_Q) {
      p_rc->kf_boost = get_projected_kf_boost(cpi);
    } else {
      // TODO(any): Explore using average frame stats for AOM_Q as well.
      boost_score = get_kf_boost_score(
          cpi, kf_raw_err, &zero_motion_accumulator, &sr_accumulator, 1);
      reset_fpf_position(&cpi->twopass_frame, start_position);
      p_rc->kf_boost += (int)boost_score;
    }
  }

  // Special case for static / slide show content but don't apply
  // if the kf group is very short.
  if ((zero_motion_accumulator > STATIC_KF_GROUP_FLOAT_THRESH) &&
      (rc->frames_to_key > 8)) {
    p_rc->kf_boost = AOMMAX(p_rc->kf_boost, MIN_STATIC_KF_BOOST);
  } else {
    // Apply various clamps for min and max boost
    p_rc->kf_boost = AOMMAX(p_rc->kf_boost, (rc->frames_to_key * 3));
    p_rc->kf_boost = AOMMAX(p_rc->kf_boost, MIN_KF_BOOST);
#ifdef STRICT_RC
    p_rc->kf_boost = AOMMIN(p_rc->kf_boost, MAX_KF_BOOST);
#endif
  }

  // Work out how many bits to allocate for the key frame itself.
  // In case of LAP enabled for VBR, if the frames_to_key value is
  // very high, we calculate the bits based on a clipped value of
  // frames_to_key.
  kf_bits = calculate_boost_bits(
      AOMMIN(rc->frames_to_key, frames_to_key_clipped) - 1, p_rc->kf_boost,
      AOMMIN(twopass->kf_group_bits, kf_group_bits_clipped));
  // printf("kf boost = %d kf_bits = %d kf_zeromotion_pct = %d\n",
  // p_rc->kf_boost,
  //        kf_bits, twopass->kf_zeromotion_pct);
  kf_bits = adjust_boost_bits_for_target_level(cpi, rc, kf_bits,
                                               twopass->kf_group_bits, 0);

  twopass->kf_group_bits -= kf_bits;

  // Save the bits to spend on the key frame.
  gf_group->bit_allocation[0] = kf_bits;
  gf_group->update_type[0] = KF_UPDATE;

  // Note the total error score of the kf group minus the key frame itself.
  if (cpi->ppi->lap_enabled)
    // As we don't have enough stats to know the actual error of the group,
    // we assume the complexity of each frame to be equal to 1, and set the
    // error as the number of frames in the group(minus the keyframe).
    twopass->kf_group_error_left = (double)(rc->frames_to_key - 1);
  else
    twopass->kf_group_error_left = kf_group_err - kf_mod_err;

  // Adjust the count of total modified error left.
  // The count of bits left is adjusted elsewhere based on real coded frame
  // sizes.
  twopass->modified_error_left -= kf_group_err;
}

#define ARF_STATS_OUTPUT 0
#if ARF_STATS_OUTPUT
unsigned int arf_count = 0;
#endif

static int get_section_target_bandwidth(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  CurrentFrame *const current_frame = &cm->current_frame;
  RATE_CONTROL *const rc = &cpi->rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  int64_t section_target_bandwidth;
  const int frames_left = (int)(twopass->stats_buf_ctx->total_stats->count -
                                current_frame->frame_number);
  if (cpi->ppi->lap_enabled)
    section_target_bandwidth = rc->avg_frame_bandwidth;
  else {
    section_target_bandwidth = twopass->bits_left / frames_left;
    section_target_bandwidth = AOMMIN(section_target_bandwidth, INT_MAX);
  }
  return (int)section_target_bandwidth;
}

static INLINE void set_twopass_params_based_on_fp_stats(
    AV1_COMP *cpi, const FIRSTPASS_STATS *this_frame_ptr) {
  if (this_frame_ptr == NULL) return;

  TWO_PASS_FRAME *twopass_frame = &cpi->twopass_frame;
  // The multiplication by 256 reverses a scaling factor of (>> 8)
  // applied when combining MB error values for the frame.
  twopass_frame->mb_av_energy = log1p(this_frame_ptr->intra_error);

  const FIRSTPASS_STATS *const total_stats =
      cpi->ppi->twopass.stats_buf_ctx->total_stats;
  if (is_fp_wavelet_energy_invalid(total_stats) == 0) {
    twopass_frame->frame_avg_haar_energy =
        log1p(this_frame_ptr->frame_avg_wavelet_energy);
  }

  // Set the frame content type flag.
  if (this_frame_ptr->intra_skip_pct >= FC_ANIMATION_THRESH)
    twopass_frame->fr_content_type = FC_GRAPHICS_ANIMATION;
  else
    twopass_frame->fr_content_type = FC_NORMAL;
}

static void process_first_pass_stats(AV1_COMP *cpi,
                                     FIRSTPASS_STATS *this_frame) {
  AV1_COMMON *const cm = &cpi->common;
  CurrentFrame *const current_frame = &cm->current_frame;
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  FIRSTPASS_STATS *total_stats = twopass->stats_buf_ctx->total_stats;

  if (cpi->oxcf.rc_cfg.mode != AOM_Q && current_frame->frame_number == 0 &&
      cpi->gf_frame_index == 0 && total_stats &&
      twopass->stats_buf_ctx->total_left_stats) {
    if (cpi->ppi->lap_enabled) {
      /*
       * Accumulate total_stats using available limited number of stats,
       * and assign it to total_left_stats.
       */
      *twopass->stats_buf_ctx->total_left_stats = *total_stats;
    }
    // Special case code for first frame.
    const int section_target_bandwidth = get_section_target_bandwidth(cpi);
    const double section_length =
        twopass->stats_buf_ctx->total_left_stats->count;
    const double section_error =
        twopass->stats_buf_ctx->total_left_stats->coded_error / section_length;
    const double section_intra_skip =
        twopass->stats_buf_ctx->total_left_stats->intra_skip_pct /
        section_length;
    const double section_inactive_zone =
        (twopass->stats_buf_ctx->total_left_stats->inactive_zone_rows * 2) /
        ((double)cm->mi_params.mb_rows * section_length);
    const int tmp_q = get_twopass_worst_quality(
        cpi, section_error, section_intra_skip + section_inactive_zone,
        section_target_bandwidth);

    rc->active_worst_quality = tmp_q;
    rc->ni_av_qi = tmp_q;
    p_rc->last_q[INTER_FRAME] = tmp_q;
    p_rc->avg_q = av1_convert_qindex_to_q(tmp_q, cm->seq_params->bit_depth);
    p_rc->avg_frame_qindex[INTER_FRAME] = tmp_q;
    p_rc->last_q[KEY_FRAME] = (tmp_q + cpi->oxcf.rc_cfg.best_allowed_q) / 2;
    p_rc->avg_frame_qindex[KEY_FRAME] = p_rc->last_q[KEY_FRAME];
  }

  if (cpi->twopass_frame.stats_in < twopass->stats_buf_ctx->stats_in_end) {
    *this_frame = *cpi->twopass_frame.stats_in;
    ++cpi->twopass_frame.stats_in;
  }
  set_twopass_params_based_on_fp_stats(cpi, this_frame);
}

static void setup_target_rate(AV1_COMP *cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  GF_GROUP *const gf_group = &cpi->ppi->gf_group;

  int target_rate = gf_group->bit_allocation[cpi->gf_frame_index];

  if (has_no_stats_stage(cpi)) {
    av1_rc_set_frame_target(cpi, target_rate, cpi->common.width,
                            cpi->common.height);
  }

  rc->base_frame_target = target_rate;
}

void av1_mark_flashes(FIRSTPASS_STATS *first_stats,
                      FIRSTPASS_STATS *last_stats) {
  FIRSTPASS_STATS *this_stats = first_stats, *next_stats;
  while (this_stats < last_stats - 1) {
    next_stats = this_stats + 1;
    if (next_stats->pcnt_second_ref > next_stats->pcnt_inter &&
        next_stats->pcnt_second_ref >= 0.5) {
      this_stats->is_flash = 1;
    } else {
      this_stats->is_flash = 0;
    }
    this_stats = next_stats;
  }
  // We always treat the last one as none flash.
  if (last_stats - 1 >= first_stats) {
    (last_stats - 1)->is_flash = 0;
  }
}

// Smooth-out the noise variance so it is more stable
// Returns 0 on success, -1 on memory allocation failure.
// TODO(bohanli): Use a better low-pass filter than averaging
static int smooth_filter_noise(FIRSTPASS_STATS *first_stats,
                               FIRSTPASS_STATS *last_stats) {
  int len = (int)(last_stats - first_stats);
  double *smooth_noise = aom_malloc(len * sizeof(*smooth_noise));
  if (!smooth_noise) return -1;

  for (int i = 0; i < len; i++) {
    double total_noise = 0;
    double total_wt = 0;
    for (int j = -HALF_FILT_LEN; j <= HALF_FILT_LEN; j++) {
      int idx = AOMMIN(AOMMAX(i + j, 0), len - 1);
      if (first_stats[idx].is_flash) continue;

      total_noise += first_stats[idx].noise_var;
      total_wt += 1.0;
    }
    if (total_wt > 0.01) {
      total_noise /= total_wt;
    } else {
      total_noise = first_stats[i].noise_var;
    }
    smooth_noise[i] = total_noise;
  }

  for (int i = 0; i < len; i++) {
    first_stats[i].noise_var = smooth_noise[i];
  }

  aom_free(smooth_noise);
  return 0;
}

// Estimate the noise variance of each frame from the first pass stats
void av1_estimate_noise(FIRSTPASS_STATS *first_stats,
                        FIRSTPASS_STATS *last_stats,
                        struct aom_internal_error_info *error_info) {
  FIRSTPASS_STATS *this_stats, *next_stats;
  double C1, C2, C3, noise;
  for (this_stats = first_stats + 2; this_stats < last_stats; this_stats++) {
    this_stats->noise_var = 0.0;
    // flashes tend to have high correlation of innovations, so ignore them.
    if (this_stats->is_flash || (this_stats - 1)->is_flash ||
        (this_stats - 2)->is_flash)
      continue;

    C1 = (this_stats - 1)->intra_error *
         (this_stats->intra_error - this_stats->coded_error);
    C2 = (this_stats - 2)->intra_error *
         ((this_stats - 1)->intra_error - (this_stats - 1)->coded_error);
    C3 = (this_stats - 2)->intra_error *
         (this_stats->intra_error - this_stats->sr_coded_error);
    if (C1 <= 0 || C2 <= 0 || C3 <= 0) continue;
    C1 = sqrt(C1);
    C2 = sqrt(C2);
    C3 = sqrt(C3);

    noise = (this_stats - 1)->intra_error - C1 * C2 / C3;
    noise = AOMMAX(noise, 0.01);
    this_stats->noise_var = noise;
  }

  // Copy noise from the neighbor if the noise value is not trustworthy
  for (this_stats = first_stats + 2; this_stats < last_stats; this_stats++) {
    if (this_stats->is_flash || (this_stats - 1)->is_flash ||
        (this_stats - 2)->is_flash)
      continue;
    if (this_stats->noise_var < 1.0) {
      int found = 0;
      // TODO(bohanli): consider expanding to two directions at the same time
      for (next_stats = this_stats + 1; next_stats < last_stats; next_stats++) {
        if (next_stats->is_flash || (next_stats - 1)->is_flash ||
            (next_stats - 2)->is_flash || next_stats->noise_var < 1.0)
          continue;
        found = 1;
        this_stats->noise_var = next_stats->noise_var;
        break;
      }
      if (found) continue;
      for (next_stats = this_stats - 1; next_stats >= first_stats + 2;
           next_stats--) {
        if (next_stats->is_flash || (next_stats - 1)->is_flash ||
            (next_stats - 2)->is_flash || next_stats->noise_var < 1.0)
          continue;
        this_stats->noise_var = next_stats->noise_var;
        break;
      }
    }
  }

  // copy the noise if this is a flash
  for (this_stats = first_stats + 2; this_stats < last_stats; this_stats++) {
    if (this_stats->is_flash || (this_stats - 1)->is_flash ||
        (this_stats - 2)->is_flash) {
      int found = 0;
      for (next_stats = this_stats + 1; next_stats < last_stats; next_stats++) {
        if (next_stats->is_flash || (next_stats - 1)->is_flash ||
            (next_stats - 2)->is_flash)
          continue;
        found = 1;
        this_stats->noise_var = next_stats->noise_var;
        break;
      }
      if (found) continue;
      for (next_stats = this_stats - 1; next_stats >= first_stats + 2;
           next_stats--) {
        if (next_stats->is_flash || (next_stats - 1)->is_flash ||
            (next_stats - 2)->is_flash)
          continue;
        this_stats->noise_var = next_stats->noise_var;
        break;
      }
    }
  }

  // if we are at the first 2 frames, copy the noise
  for (this_stats = first_stats;
       this_stats < first_stats + 2 && (first_stats + 2) < last_stats;
       this_stats++) {
    this_stats->noise_var = (first_stats + 2)->noise_var;
  }

  if (smooth_filter_noise(first_stats, last_stats) == -1) {
    aom_internal_error(error_info, AOM_CODEC_MEM_ERROR,
                       "Error allocating buffers in smooth_filter_noise()");
  }
}

// Estimate correlation coefficient of each frame with its previous frame.
void av1_estimate_coeff(FIRSTPASS_STATS *first_stats,
                        FIRSTPASS_STATS *last_stats) {
  FIRSTPASS_STATS *this_stats;
  for (this_stats = first_stats + 1; this_stats < last_stats; this_stats++) {
    const double C =
        sqrt(AOMMAX((this_stats - 1)->intra_error *
                        (this_stats->intra_error - this_stats->coded_error),
                    0.001));
    const double cor_coeff =
        C /
        AOMMAX((this_stats - 1)->intra_error - this_stats->noise_var, 0.001);

    this_stats->cor_coeff =
        cor_coeff *
        sqrt(AOMMAX((this_stats - 1)->intra_error - this_stats->noise_var,
                    0.001) /
             AOMMAX(this_stats->intra_error - this_stats->noise_var, 0.001));
    // clip correlation coefficient.
    this_stats->cor_coeff = AOMMIN(AOMMAX(this_stats->cor_coeff, 0), 1);
  }
  first_stats->cor_coeff = 1.0;
}

void av1_get_second_pass_params(AV1_COMP *cpi,
                                EncodeFrameParams *const frame_params,
                                unsigned int frame_flags) {
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  GF_GROUP *const gf_group = &cpi->ppi->gf_group;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  if (cpi->use_ducky_encode &&
      cpi->ducky_encode_info.frame_info.gop_mode == DUCKY_ENCODE_GOP_MODE_RCL) {
    frame_params->frame_type = gf_group->frame_type[cpi->gf_frame_index];
    frame_params->show_frame =
        !(gf_group->update_type[cpi->gf_frame_index] == ARF_UPDATE ||
          gf_group->update_type[cpi->gf_frame_index] == INTNL_ARF_UPDATE);
    if (cpi->gf_frame_index == 0) {
      av1_tf_info_reset(&cpi->ppi->tf_info);
      av1_tf_info_filtering(&cpi->ppi->tf_info, cpi, gf_group);
    }
    return;
  }

  const FIRSTPASS_STATS *const start_pos = cpi->twopass_frame.stats_in;
  int update_total_stats = 0;

  if (is_stat_consumption_stage(cpi) && !cpi->twopass_frame.stats_in) return;

  // Check forced key frames.
  const int frames_to_next_forced_key = detect_app_forced_key(cpi);
  if (frames_to_next_forced_key == 0) {
    rc->frames_to_key = 0;
    frame_flags &= FRAMEFLAGS_KEY;
  } else if (frames_to_next_forced_key > 0 &&
             frames_to_next_forced_key < rc->frames_to_key) {
    rc->frames_to_key = frames_to_next_forced_key;
  }

  assert(cpi->twopass_frame.stats_in != NULL);
  const int update_type = gf_group->update_type[cpi->gf_frame_index];
  frame_params->frame_type = gf_group->frame_type[cpi->gf_frame_index];

  if (cpi->gf_frame_index < gf_group->size && !(frame_flags & FRAMEFLAGS_KEY)) {
    assert(cpi->gf_frame_index < gf_group->size);

    setup_target_rate(cpi);

    // If this is an arf frame then we dont want to read the stats file or
    // advance the input pointer as we already have what we need.
    if (update_type == ARF_UPDATE || update_type == INTNL_ARF_UPDATE) {
      const FIRSTPASS_STATS *const this_frame_ptr =
          read_frame_stats(twopass, &cpi->twopass_frame,
                           gf_group->arf_src_offset[cpi->gf_frame_index]);
      set_twopass_params_based_on_fp_stats(cpi, this_frame_ptr);
      return;
    }
  }

  if (oxcf->rc_cfg.mode == AOM_Q)
    rc->active_worst_quality = oxcf->rc_cfg.cq_level;

  if (cpi->gf_frame_index == gf_group->size) {
    if (cpi->ppi->lap_enabled && cpi->ppi->p_rc.enable_scenecut_detection) {
      const int num_frames_to_detect_scenecut = MAX_GF_LENGTH_LAP + 1;
      const int frames_to_key = define_kf_interval(
          cpi, &twopass->firstpass_info, num_frames_to_detect_scenecut,
          /*search_start_idx=*/0);
      if (frames_to_key != -1)
        rc->frames_to_key = AOMMIN(rc->frames_to_key, frames_to_key);
    }
  }

  FIRSTPASS_STATS this_frame;
  av1_zero(this_frame);
  // call above fn
  if (is_stat_consumption_stage(cpi)) {
    if (cpi->gf_frame_index < gf_group->size || rc->frames_to_key == 0) {
      process_first_pass_stats(cpi, &this_frame);
      update_total_stats = 1;
    }
  } else {
    rc->active_worst_quality = oxcf->rc_cfg.cq_level;
  }

  // Keyframe and section processing.
  FIRSTPASS_STATS this_frame_copy;
  this_frame_copy = this_frame;
  if (rc->frames_to_key <= 0) {
    assert(rc->frames_to_key == 0);
    // Define next KF group and assign bits to it.
    frame_params->frame_type = KEY_FRAME;
    find_next_key_frame(cpi, &this_frame);
    this_frame = this_frame_copy;
  }

  if (rc->frames_to_fwd_kf <= 0)
    rc->frames_to_fwd_kf = oxcf->kf_cfg.fwd_kf_dist;

  // Define a new GF/ARF group. (Should always enter here for key frames).
  if (cpi->gf_frame_index == gf_group->size) {
    av1_tf_info_reset(&cpi->ppi->tf_info);
#if CONFIG_BITRATE_ACCURACY && !CONFIG_THREE_PASS
    vbr_rc_reset_gop_data(&cpi->vbr_rc_info);
#endif  // CONFIG_BITRATE_ACCURACY
    int max_gop_length =
        (oxcf->gf_cfg.lag_in_frames >= 32)
            ? AOMMIN(MAX_GF_INTERVAL, oxcf->gf_cfg.lag_in_frames -
                                          oxcf->algo_cfg.arnr_max_frames / 2)
            : MAX_GF_LENGTH_LAP;

    // Handle forward key frame when enabled.
    if (oxcf->kf_cfg.fwd_kf_dist > 0)
      max_gop_length = AOMMIN(rc->frames_to_fwd_kf + 1, max_gop_length);

    // Use the provided gop size in low delay setting
    if (oxcf->gf_cfg.lag_in_frames == 0) max_gop_length = rc->max_gf_interval;

    // Limit the max gop length for the last gop in 1 pass setting.
    max_gop_length = AOMMIN(max_gop_length, rc->frames_to_key);

    // Identify regions if needed.
    // TODO(bohanli): identify regions for all stats available.
    if (rc->frames_since_key == 0 || rc->frames_since_key == 1 ||
        (p_rc->frames_till_regions_update - rc->frames_since_key <
             rc->frames_to_key &&
         p_rc->frames_till_regions_update - rc->frames_since_key <
             max_gop_length + 1)) {
      // how many frames we can analyze from this frame
      int rest_frames =
          AOMMIN(rc->frames_to_key, MAX_FIRSTPASS_ANALYSIS_FRAMES);
      rest_frames =
          AOMMIN(rest_frames, (int)(twopass->stats_buf_ctx->stats_in_end -
                                    cpi->twopass_frame.stats_in +
                                    (rc->frames_since_key == 0)));
      p_rc->frames_till_regions_update = rest_frames;

      int ret;
      if (cpi->ppi->lap_enabled) {
        av1_mark_flashes(twopass->stats_buf_ctx->stats_in_start,
                         twopass->stats_buf_ctx->stats_in_end);
        av1_estimate_noise(twopass->stats_buf_ctx->stats_in_start,
                           twopass->stats_buf_ctx->stats_in_end,
                           cpi->common.error);
        av1_estimate_coeff(twopass->stats_buf_ctx->stats_in_start,
                           twopass->stats_buf_ctx->stats_in_end);
        ret = identify_regions(cpi->twopass_frame.stats_in, rest_frames,
                               (rc->frames_since_key == 0), p_rc->regions,
                               &p_rc->num_regions);
      } else {
        ret = identify_regions(
            cpi->twopass_frame.stats_in - (rc->frames_since_key == 0),
            rest_frames, 0, p_rc->regions, &p_rc->num_regions);
      }
      if (ret == -1) {
        aom_internal_error(cpi->common.error, AOM_CODEC_MEM_ERROR,
                           "Error allocating buffers in identify_regions");
      }
    }

    int cur_region_idx =
        find_regions_index(p_rc->regions, p_rc->num_regions,
                           rc->frames_since_key - p_rc->regions_offset);
    if ((cur_region_idx >= 0 &&
         p_rc->regions[cur_region_idx].type == SCENECUT_REGION) ||
        rc->frames_since_key == 0) {
      // If we start from a scenecut, then the last GOP's arf boost is not
      // needed for this GOP.
      cpi->ppi->gf_state.arf_gf_boost_lst = 0;
    }

    int need_gf_len = 1;
    if (cpi->third_pass_ctx && oxcf->pass == AOM_RC_THIRD_PASS) {
      // set up bitstream to read
      if (!cpi->third_pass_ctx->input_file_name && oxcf->two_pass_output) {
        cpi->third_pass_ctx->input_file_name = oxcf->two_pass_output;
      }
      av1_open_second_pass_log(cpi, 1);
      THIRD_PASS_GOP_INFO *gop_info = &cpi->third_pass_ctx->gop_info;
      // Read in GOP information from the second pass file.
      av1_read_second_pass_gop_info(cpi->second_pass_log_stream, gop_info,
                                    cpi->common.error);
#if CONFIG_BITRATE_ACCURACY
      TPL_INFO *tpl_info;
      AOM_CHECK_MEM_ERROR(cpi->common.error, tpl_info,
                          aom_malloc(sizeof(*tpl_info)));
      av1_read_tpl_info(tpl_info, cpi->second_pass_log_stream,
                        cpi->common.error);
      aom_free(tpl_info);
#if CONFIG_THREE_PASS
      // TODO(angiebird): Put this part into a func
      cpi->vbr_rc_info.cur_gop_idx++;
#endif  // CONFIG_THREE_PASS
#endif  // CONFIG_BITRATE_ACCURACY
      // Read in third_pass_info from the bitstream.
      av1_set_gop_third_pass(cpi->third_pass_ctx);
      // Read in per-frame info from second-pass encoding
      av1_read_second_pass_per_frame_info(
          cpi->second_pass_log_stream, cpi->third_pass_ctx->frame_info,
          gop_info->num_frames, cpi->common.error);

      p_rc->cur_gf_index = 0;
      p_rc->gf_intervals[0] = cpi->third_pass_ctx->gop_info.gf_length;
      need_gf_len = 0;
    }

    if (need_gf_len) {
      // If we cannot obtain GF group length from second_pass_file
      // TODO(jingning): Resolve the redundant calls here.
      if (rc->intervals_till_gf_calculate_due == 0 || 1) {
        calculate_gf_length(cpi, max_gop_length, MAX_NUM_GF_INTERVALS);
      }

      if (max_gop_length > 16 && oxcf->algo_cfg.enable_tpl_model &&
          oxcf->gf_cfg.lag_in_frames >= 32 &&
          cpi->sf.tpl_sf.gop_length_decision_method != 3) {
        int this_idx = rc->frames_since_key +
                       p_rc->gf_intervals[p_rc->cur_gf_index] -
                       p_rc->regions_offset - 1;
        int this_region =
            find_regions_index(p_rc->regions, p_rc->num_regions, this_idx);
        int next_region =
            find_regions_index(p_rc->regions, p_rc->num_regions, this_idx + 1);
        // TODO(angiebird): Figure out why this_region and next_region are -1 in
        // unit test like AltRefFramePresenceTestLarge (aomedia:3134)
        int is_last_scenecut =
            p_rc->gf_intervals[p_rc->cur_gf_index] >= rc->frames_to_key ||
            (this_region != -1 &&
             p_rc->regions[this_region].type == SCENECUT_REGION) ||
            (next_region != -1 &&
             p_rc->regions[next_region].type == SCENECUT_REGION);

        int ori_gf_int = p_rc->gf_intervals[p_rc->cur_gf_index];

        if (p_rc->gf_intervals[p_rc->cur_gf_index] > 16 &&
            rc->min_gf_interval <= 16) {
          // The calculate_gf_length function is previously used with
          // max_gop_length = 32 with look-ahead gf intervals.
          define_gf_group(cpi, frame_params, 0);
          av1_tf_info_filtering(&cpi->ppi->tf_info, cpi, gf_group);
          this_frame = this_frame_copy;

          if (is_shorter_gf_interval_better(cpi, frame_params)) {
            // A shorter gf interval is better.
            // TODO(jingning): Remove redundant computations here.
            max_gop_length = 16;
            calculate_gf_length(cpi, max_gop_length, 1);
            if (is_last_scenecut &&
                (ori_gf_int - p_rc->gf_intervals[p_rc->cur_gf_index] < 4)) {
              p_rc->gf_intervals[p_rc->cur_gf_index] = ori_gf_int;
            }
          }
        }
      }
    }

    define_gf_group(cpi, frame_params, 0);

    if (gf_group->update_type[cpi->gf_frame_index] != ARF_UPDATE &&
        rc->frames_since_key > 0)
      process_first_pass_stats(cpi, &this_frame);

    define_gf_group(cpi, frame_params, 1);

    // write gop info if needed for third pass. Per-frame info is written after
    // each frame is encoded.
    av1_write_second_pass_gop_info(cpi);

    av1_tf_info_filtering(&cpi->ppi->tf_info, cpi, gf_group);

    rc->frames_till_gf_update_due = p_rc->baseline_gf_interval;
    assert(cpi->gf_frame_index == 0);
#if ARF_STATS_OUTPUT
    {
      FILE *fpfile;
      fpfile = fopen("arf.stt", "a");
      ++arf_count;
      fprintf(fpfile, "%10d %10d %10d %10d %10d\n",
              cpi->common.current_frame.frame_number,
              rc->frames_till_gf_update_due, cpi->ppi->p_rc.kf_boost, arf_count,
              p_rc->gfu_boost);

      fclose(fpfile);
    }
#endif
  }
  assert(cpi->gf_frame_index < gf_group->size);

  if (gf_group->update_type[cpi->gf_frame_index] == ARF_UPDATE ||
      gf_group->update_type[cpi->gf_frame_index] == INTNL_ARF_UPDATE) {
    reset_fpf_position(&cpi->twopass_frame, start_pos);

    const FIRSTPASS_STATS *const this_frame_ptr =
        read_frame_stats(twopass, &cpi->twopass_frame,
                         gf_group->arf_src_offset[cpi->gf_frame_index]);
    set_twopass_params_based_on_fp_stats(cpi, this_frame_ptr);
  } else {
    // Back up this frame's stats for updating total stats during post encode.
    cpi->twopass_frame.this_frame = update_total_stats ? start_pos : NULL;
  }

  frame_params->frame_type = gf_group->frame_type[cpi->gf_frame_index];
  setup_target_rate(cpi);
}

void av1_init_second_pass(AV1_COMP *cpi) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  FRAME_INFO *const frame_info = &cpi->frame_info;
  double frame_rate;
  FIRSTPASS_STATS *stats;

  if (!twopass->stats_buf_ctx->stats_in_end) return;

  av1_mark_flashes(twopass->stats_buf_ctx->stats_in_start,
                   twopass->stats_buf_ctx->stats_in_end);
  av1_estimate_noise(twopass->stats_buf_ctx->stats_in_start,
                     twopass->stats_buf_ctx->stats_in_end, cpi->common.error);
  av1_estimate_coeff(twopass->stats_buf_ctx->stats_in_start,
                     twopass->stats_buf_ctx->stats_in_end);

  stats = twopass->stats_buf_ctx->total_stats;

  *stats = *twopass->stats_buf_ctx->stats_in_end;
  *twopass->stats_buf_ctx->total_left_stats = *stats;

  frame_rate = 10000000.0 * stats->count / stats->duration;
  // Each frame can have a different duration, as the frame rate in the source
  // isn't guaranteed to be constant. The frame rate prior to the first frame
  // encoded in the second pass is a guess. However, the sum duration is not.
  // It is calculated based on the actual durations of all frames from the
  // first pass.
  av1_new_framerate(cpi, frame_rate);
  twopass->bits_left =
      (int64_t)(stats->duration * oxcf->rc_cfg.target_bandwidth / 10000000.0);

#if CONFIG_BITRATE_ACCURACY
  av1_vbr_rc_init(&cpi->vbr_rc_info, twopass->bits_left,
                  (int)round(stats->count));
#endif

#if CONFIG_RATECTRL_LOG
  rc_log_init(&cpi->rc_log);
#endif

  // This variable monitors how far behind the second ref update is lagging.
  twopass->sr_update_lag = 1;

  // Scan the first pass file and calculate a modified total error based upon
  // the bias/power function used to allocate bits.
  {
    const double avg_error =
        stats->coded_error / DOUBLE_DIVIDE_CHECK(stats->count);
    const FIRSTPASS_STATS *s = cpi->twopass_frame.stats_in;
    double modified_error_total = 0.0;
    twopass->modified_error_min =
        (avg_error * oxcf->rc_cfg.vbrmin_section) / 100;
    twopass->modified_error_max =
        (avg_error * oxcf->rc_cfg.vbrmax_section) / 100;
    while (s < twopass->stats_buf_ctx->stats_in_end) {
      modified_error_total +=
          calculate_modified_err(frame_info, twopass, oxcf, s);
      ++s;
    }
    twopass->modified_error_left = modified_error_total;
  }

  // Reset the vbr bits off target counters
  cpi->ppi->p_rc.vbr_bits_off_target = 0;
  cpi->ppi->p_rc.vbr_bits_off_target_fast = 0;

  cpi->ppi->p_rc.rate_error_estimate = 0;

  // Static sequence monitor variables.
  twopass->kf_zeromotion_pct = 100;
  twopass->last_kfgroup_zeromotion_pct = 100;

  // Initialize bits per macro_block estimate correction factor.
  twopass->bpm_factor = 1.0;
  // Initialize actual and target bits counters for ARF groups so that
  // at the start we have a neutral bpm adjustment.
  twopass->rolling_arf_group_target_bits = 1;
  twopass->rolling_arf_group_actual_bits = 1;
}

void av1_init_single_pass_lap(AV1_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->ppi->twopass;

  if (!twopass->stats_buf_ctx->stats_in_end) return;

  // This variable monitors how far behind the second ref update is lagging.
  twopass->sr_update_lag = 1;

  twopass->bits_left = 0;
  twopass->modified_error_min = 0.0;
  twopass->modified_error_max = 0.0;
  twopass->modified_error_left = 0.0;

  // Reset the vbr bits off target counters
  cpi->ppi->p_rc.vbr_bits_off_target = 0;
  cpi->ppi->p_rc.vbr_bits_off_target_fast = 0;

  cpi->ppi->p_rc.rate_error_estimate = 0;

  // Static sequence monitor variables.
  twopass->kf_zeromotion_pct = 100;
  twopass->last_kfgroup_zeromotion_pct = 100;

  // Initialize bits per macro_block estimate correction factor.
  twopass->bpm_factor = 1.0;
  // Initialize actual and target bits counters for ARF groups so that
  // at the start we have a neutral bpm adjustment.
  twopass->rolling_arf_group_target_bits = 1;
  twopass->rolling_arf_group_actual_bits = 1;
}

#define MINQ_ADJ_LIMIT 48
#define MINQ_ADJ_LIMIT_CQ 20
#define HIGH_UNDERSHOOT_RATIO 2
void av1_twopass_postencode_update(AV1_COMP *cpi) {
  TWO_PASS *const twopass = &cpi->ppi->twopass;
  RATE_CONTROL *const rc = &cpi->rc;
  PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;
  const RateControlCfg *const rc_cfg = &cpi->oxcf.rc_cfg;

  // Increment the stats_in pointer.
  if (is_stat_consumption_stage(cpi) &&
      !(cpi->use_ducky_encode && cpi->ducky_encode_info.frame_info.gop_mode ==
                                     DUCKY_ENCODE_GOP_MODE_RCL) &&
      (cpi->gf_frame_index < cpi->ppi->gf_group.size ||
       rc->frames_to_key == 0)) {
    const int update_type = cpi->ppi->gf_group.update_type[cpi->gf_frame_index];
    if (update_type != ARF_UPDATE && update_type != INTNL_ARF_UPDATE) {
      FIRSTPASS_STATS this_frame;
      assert(cpi->twopass_frame.stats_in >
             twopass->stats_buf_ctx->stats_in_start);
      --cpi->twopass_frame.stats_in;
      if (cpi->ppi->lap_enabled) {
        input_stats_lap(twopass, &cpi->twopass_frame, &this_frame);
      } else {
        input_stats(twopass, &cpi->twopass_frame, &this_frame);
      }
    } else if (cpi->ppi->lap_enabled) {
      cpi->twopass_frame.stats_in = twopass->stats_buf_ctx->stats_in_start;
    }
  }

  // VBR correction is done through rc->vbr_bits_off_target. Based on the
  // sign of this value, a limited % adjustment is made to the target rate
  // of subsequent frames, to try and push it back towards 0. This method
  // is designed to prevent extreme behaviour at the end of a clip
  // or group of frames.
  p_rc->vbr_bits_off_target += rc->base_frame_target - rc->projected_frame_size;
  twopass->bits_left = AOMMAX(twopass->bits_left - rc->base_frame_target, 0);

  if (cpi->do_update_vbr_bits_off_target_fast) {
    // Subtract current frame's fast_extra_bits.
    p_rc->vbr_bits_off_target_fast -= rc->frame_level_fast_extra_bits;
    rc->frame_level_fast_extra_bits = 0;
  }

  // Target vs actual bits for this arf group.
  if (twopass->rolling_arf_group_target_bits >
      INT_MAX - rc->base_frame_target) {
    twopass->rolling_arf_group_target_bits = INT_MAX;
  } else {
    twopass->rolling_arf_group_target_bits += rc->base_frame_target;
  }
  twopass->rolling_arf_group_actual_bits += rc->projected_frame_size;

  // Calculate the pct rc error.
  if (p_rc->total_actual_bits) {
    p_rc->rate_error_estimate =
        (int)((p_rc->vbr_bits_off_target * 100) / p_rc->total_actual_bits);
    p_rc->rate_error_estimate = clamp(p_rc->rate_error_estimate, -100, 100);
  } else {
    p_rc->rate_error_estimate = 0;
  }

#if CONFIG_FPMT_TEST
  /* The variables temp_vbr_bits_off_target, temp_bits_left,
   * temp_rolling_arf_group_target_bits, temp_rolling_arf_group_actual_bits
   * temp_rate_error_estimate are introduced for quality simulation purpose,
   * it retains the value previous to the parallel encode frames. The
   * variables are updated based on the update flag.
   *
   * If there exist show_existing_frames between parallel frames, then to
   * retain the temp state do not update it. */
  const int simulate_parallel_frame =
      cpi->ppi->fpmt_unit_test_cfg == PARALLEL_SIMULATION_ENCODE;
  int show_existing_between_parallel_frames =
      (cpi->ppi->gf_group.update_type[cpi->gf_frame_index] ==
           INTNL_OVERLAY_UPDATE &&
       cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index + 1] == 2);

  if (cpi->do_frame_data_update && !show_existing_between_parallel_frames &&
      simulate_parallel_frame) {
    cpi->ppi->p_rc.temp_vbr_bits_off_target = p_rc->vbr_bits_off_target;
    cpi->ppi->p_rc.temp_bits_left = twopass->bits_left;
    cpi->ppi->p_rc.temp_rolling_arf_group_target_bits =
        twopass->rolling_arf_group_target_bits;
    cpi->ppi->p_rc.temp_rolling_arf_group_actual_bits =
        twopass->rolling_arf_group_actual_bits;
    cpi->ppi->p_rc.temp_rate_error_estimate = p_rc->rate_error_estimate;
  }
#endif
  // Update the active best quality pyramid.
  if (!rc->is_src_frame_alt_ref) {
    const int pyramid_level =
        cpi->ppi->gf_group.layer_depth[cpi->gf_frame_index];
    int i;
    for (i = pyramid_level; i <= MAX_ARF_LAYERS; ++i) {
      p_rc->active_best_quality[i] = cpi->common.quant_params.base_qindex;
#if CONFIG_TUNE_VMAF
      if (cpi->vmaf_info.original_qindex != -1 &&
          (cpi->oxcf.tune_cfg.tuning >= AOM_TUNE_VMAF_WITH_PREPROCESSING &&
           cpi->oxcf.tune_cfg.tuning <= AOM_TUNE_VMAF_NEG_MAX_GAIN)) {
        p_rc->active_best_quality[i] = cpi->vmaf_info.original_qindex;
      }
#endif
    }
  }

#if 0
  {
    AV1_COMMON *cm = &cpi->common;
    FILE *fpfile;
    fpfile = fopen("details.stt", "a");
    fprintf(fpfile,
            "%10d %10d %10d %10" PRId64 " %10" PRId64
            " %10d %10d %10d %10.4lf %10.4lf %10.4lf %10.4lf\n",
            cm->current_frame.frame_number, rc->base_frame_target,
            rc->projected_frame_size, rc->total_actual_bits,
            rc->vbr_bits_off_target, p_rc->rate_error_estimate,
            twopass->rolling_arf_group_target_bits,
            twopass->rolling_arf_group_actual_bits,
            (double)twopass->rolling_arf_group_actual_bits /
                (double)twopass->rolling_arf_group_target_bits,
            twopass->bpm_factor,
            av1_convert_qindex_to_q(cpi->common.quant_params.base_qindex,
                                    cm->seq_params->bit_depth),
            av1_convert_qindex_to_q(rc->active_worst_quality,
                                    cm->seq_params->bit_depth));
    fclose(fpfile);
  }
#endif

  if (cpi->common.current_frame.frame_type != KEY_FRAME) {
    twopass->kf_group_bits -= rc->base_frame_target;
    twopass->last_kfgroup_zeromotion_pct = twopass->kf_zeromotion_pct;
  }
  twopass->kf_group_bits = AOMMAX(twopass->kf_group_bits, 0);

  // If the rate control is drifting consider adjustment to min or maxq.
  if ((rc_cfg->mode != AOM_Q) && !cpi->rc.is_src_frame_alt_ref &&
      (p_rc->rolling_target_bits > 0)) {
    int minq_adj_limit;
    int maxq_adj_limit;
    minq_adj_limit =
        (rc_cfg->mode == AOM_CQ ? MINQ_ADJ_LIMIT_CQ : MINQ_ADJ_LIMIT);
    maxq_adj_limit = (rc->worst_quality - rc->active_worst_quality);

    // Undershoot
    if ((rc_cfg->under_shoot_pct < 100) &&
        (p_rc->rolling_actual_bits < p_rc->rolling_target_bits)) {
      int pct_error =
          ((p_rc->rolling_target_bits - p_rc->rolling_actual_bits) * 100) /
          p_rc->rolling_target_bits;

      if ((pct_error >= rc_cfg->under_shoot_pct) &&
          (p_rc->rate_error_estimate > 0)) {
        twopass->extend_minq += 1;
        twopass->extend_maxq -= 1;
      }

      // Overshoot
    } else if ((rc_cfg->over_shoot_pct < 100) &&
               (p_rc->rolling_actual_bits > p_rc->rolling_target_bits)) {
      int pct_error =
          ((p_rc->rolling_actual_bits - p_rc->rolling_target_bits) * 100) /
          p_rc->rolling_target_bits;

      pct_error = clamp(pct_error, 0, 100);
      if ((pct_error >= rc_cfg->over_shoot_pct) &&
          (p_rc->rate_error_estimate < 0)) {
        twopass->extend_maxq += 1;
        twopass->extend_minq -= 1;
      }
    }
    twopass->extend_minq =
        clamp(twopass->extend_minq, -minq_adj_limit, minq_adj_limit);
    twopass->extend_maxq = clamp(twopass->extend_maxq, 0, maxq_adj_limit);

    // If there is a big and undexpected undershoot then feed the extra
    // bits back in quickly. One situation where this may happen is if a
    // frame is unexpectedly almost perfectly predicted by the ARF or GF
    // but not very well predcited by the previous frame.
    if (!frame_is_kf_gf_arf(cpi) && !cpi->rc.is_src_frame_alt_ref) {
      int fast_extra_thresh = rc->base_frame_target / HIGH_UNDERSHOOT_RATIO;
      if (rc->projected_frame_size < fast_extra_thresh) {
        p_rc->vbr_bits_off_target_fast +=
            fast_extra_thresh - rc->projected_frame_size;
        p_rc->vbr_bits_off_target_fast =
            AOMMIN(p_rc->vbr_bits_off_target_fast,
                   (4 * (int64_t)rc->avg_frame_bandwidth));
      }
    }

#if CONFIG_FPMT_TEST
    if (cpi->do_frame_data_update && !show_existing_between_parallel_frames &&
        simulate_parallel_frame) {
      cpi->ppi->p_rc.temp_vbr_bits_off_target_fast =
          p_rc->vbr_bits_off_target_fast;
      cpi->ppi->p_rc.temp_extend_minq = twopass->extend_minq;
      cpi->ppi->p_rc.temp_extend_maxq = twopass->extend_maxq;
    }
#endif
  }

  // Update the frame probabilities obtained from parallel encode frames
  FrameProbInfo *const frame_probs = &cpi->ppi->frame_probs;
#if CONFIG_FPMT_TEST
  /* The variable temp_active_best_quality is introduced only for quality
   * simulation purpose, it retains the value previous to the parallel
   * encode frames. The variable is updated based on the update flag.
   *
   * If there exist show_existing_frames between parallel frames, then to
   * retain the temp state do not update it. */
  if (cpi->do_frame_data_update && !show_existing_between_parallel_frames &&
      simulate_parallel_frame) {
    int i;
    const int pyramid_level =
        cpi->ppi->gf_group.layer_depth[cpi->gf_frame_index];
    if (!rc->is_src_frame_alt_ref) {
      for (i = pyramid_level; i <= MAX_ARF_LAYERS; ++i)
        cpi->ppi->p_rc.temp_active_best_quality[i] =
            p_rc->active_best_quality[i];
    }
  }

  // Update the frame probabilities obtained from parallel encode frames
  FrameProbInfo *const temp_frame_probs_simulation =
      simulate_parallel_frame ? &cpi->ppi->temp_frame_probs_simulation
                              : frame_probs;
  FrameProbInfo *const temp_frame_probs =
      simulate_parallel_frame ? &cpi->ppi->temp_frame_probs : NULL;
#endif
  int i, j, loop;
  // Sequentially do average on temp_frame_probs_simulation which holds
  // probabilities of last frame before parallel encode
  for (loop = 0; loop <= cpi->num_frame_recode; loop++) {
    // Sequentially update tx_type_probs
    if (cpi->do_update_frame_probs_txtype[loop] &&
        (cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0)) {
      const FRAME_UPDATE_TYPE update_type =
          get_frame_update_type(&cpi->ppi->gf_group, cpi->gf_frame_index);
      for (i = 0; i < TX_SIZES_ALL; i++) {
        int left = 1024;

        for (j = TX_TYPES - 1; j >= 0; j--) {
          const int new_prob =
              cpi->frame_new_probs[loop].tx_type_probs[update_type][i][j];
#if CONFIG_FPMT_TEST
          int prob =
              (temp_frame_probs_simulation->tx_type_probs[update_type][i][j] +
               new_prob) >>
              1;
          left -= prob;
          if (j == 0) prob += left;
          temp_frame_probs_simulation->tx_type_probs[update_type][i][j] = prob;
#else
          int prob =
              (frame_probs->tx_type_probs[update_type][i][j] + new_prob) >> 1;
          left -= prob;
          if (j == 0) prob += left;
          frame_probs->tx_type_probs[update_type][i][j] = prob;
#endif
        }
      }
    }

    // Sequentially update obmc_probs
    if (cpi->do_update_frame_probs_obmc[loop] &&
        cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0) {
      const FRAME_UPDATE_TYPE update_type =
          get_frame_update_type(&cpi->ppi->gf_group, cpi->gf_frame_index);

      for (i = 0; i < BLOCK_SIZES_ALL; i++) {
        const int new_prob =
            cpi->frame_new_probs[loop].obmc_probs[update_type][i];
#if CONFIG_FPMT_TEST
        temp_frame_probs_simulation->obmc_probs[update_type][i] =
            (temp_frame_probs_simulation->obmc_probs[update_type][i] +
             new_prob) >>
            1;
#else
        frame_probs->obmc_probs[update_type][i] =
            (frame_probs->obmc_probs[update_type][i] + new_prob) >> 1;
#endif
      }
    }

    // Sequentially update warped_probs
    if (cpi->do_update_frame_probs_warp[loop] &&
        cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0) {
      const FRAME_UPDATE_TYPE update_type =
          get_frame_update_type(&cpi->ppi->gf_group, cpi->gf_frame_index);
      const int new_prob = cpi->frame_new_probs[loop].warped_probs[update_type];
#if CONFIG_FPMT_TEST
      temp_frame_probs_simulation->warped_probs[update_type] =
          (temp_frame_probs_simulation->warped_probs[update_type] + new_prob) >>
          1;
#else
      frame_probs->warped_probs[update_type] =
          (frame_probs->warped_probs[update_type] + new_prob) >> 1;
#endif
    }

    // Sequentially update switchable_interp_probs
    if (cpi->do_update_frame_probs_interpfilter[loop] &&
        cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0) {
      const FRAME_UPDATE_TYPE update_type =
          get_frame_update_type(&cpi->ppi->gf_group, cpi->gf_frame_index);

      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
        int left = 1536;

        for (j = SWITCHABLE_FILTERS - 1; j >= 0; j--) {
          const int new_prob = cpi->frame_new_probs[loop]
                                   .switchable_interp_probs[update_type][i][j];
#if CONFIG_FPMT_TEST
          int prob = (temp_frame_probs_simulation
                          ->switchable_interp_probs[update_type][i][j] +
                      new_prob) >>
                     1;
          left -= prob;
          if (j == 0) prob += left;

          temp_frame_probs_simulation
              ->switchable_interp_probs[update_type][i][j] = prob;
#else
          int prob = (frame_probs->switchable_interp_probs[update_type][i][j] +
                      new_prob) >>
                     1;
          left -= prob;
          if (j == 0) prob += left;
          frame_probs->switchable_interp_probs[update_type][i][j] = prob;
#endif
        }
      }
    }
  }

#if CONFIG_FPMT_TEST
  // Copying temp_frame_probs_simulation to temp_frame_probs based on
  // the flag
  if (cpi->do_frame_data_update &&
      cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0 &&
      simulate_parallel_frame) {
    for (int update_type_idx = 0; update_type_idx < FRAME_UPDATE_TYPES;
         update_type_idx++) {
      for (i = 0; i < BLOCK_SIZES_ALL; i++) {
        temp_frame_probs->obmc_probs[update_type_idx][i] =
            temp_frame_probs_simulation->obmc_probs[update_type_idx][i];
      }
      temp_frame_probs->warped_probs[update_type_idx] =
          temp_frame_probs_simulation->warped_probs[update_type_idx];
      for (i = 0; i < TX_SIZES_ALL; i++) {
        for (j = 0; j < TX_TYPES; j++) {
          temp_frame_probs->tx_type_probs[update_type_idx][i][j] =
              temp_frame_probs_simulation->tx_type_probs[update_type_idx][i][j];
        }
      }
      for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++) {
        for (j = 0; j < SWITCHABLE_FILTERS; j++) {
          temp_frame_probs->switchable_interp_probs[update_type_idx][i][j] =
              temp_frame_probs_simulation
                  ->switchable_interp_probs[update_type_idx][i][j];
        }
      }
    }
  }
#endif
  // Update framerate obtained from parallel encode frames
  if (cpi->common.show_frame &&
      cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index] > 0)
    cpi->framerate = cpi->new_framerate;
#if CONFIG_FPMT_TEST
  // SIMULATION PURPOSE
  int show_existing_between_parallel_frames_cndn =
      (cpi->ppi->gf_group.update_type[cpi->gf_frame_index] ==
           INTNL_OVERLAY_UPDATE &&
       cpi->ppi->gf_group.frame_parallel_level[cpi->gf_frame_index + 1] == 2);
  if (cpi->common.show_frame && !show_existing_between_parallel_frames_cndn &&
      cpi->do_frame_data_update && simulate_parallel_frame)
    cpi->temp_framerate = cpi->framerate;
#endif
}
