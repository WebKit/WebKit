/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "av1/ratectrl_qmode.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <functional>
#include <numeric>
#include <sstream>
#include <vector>

#include "aom/aom_codec.h"
#include "av1/encoder/pass2_strategy.h"
#include "av1/encoder/tpl_model.h"

namespace aom {

// This is used before division to ensure that the divisor isn't zero or
// too close to zero.
static double ModifyDivisor(double divisor) {
  const double kEpsilon = 0.0000001;
  return (divisor < 0 ? std::min(divisor, -kEpsilon)
                      : std::max(divisor, kEpsilon));
}

GopFrame GopFrameInvalid() {
  GopFrame gop_frame = {};
  gop_frame.is_valid = false;
  gop_frame.coding_idx = -1;
  gop_frame.order_idx = -1;
  return gop_frame;
}

void SetGopFrameByType(GopFrameType gop_frame_type, GopFrame *gop_frame) {
  gop_frame->update_type = gop_frame_type;
  switch (gop_frame_type) {
    case GopFrameType::kRegularKey:
      gop_frame->is_key_frame = 1;
      gop_frame->is_arf_frame = 0;
      gop_frame->is_show_frame = 1;
      gop_frame->is_golden_frame = 1;
      gop_frame->encode_ref_mode = EncodeRefMode::kRegular;
      break;
    case GopFrameType::kRegularGolden:
      gop_frame->is_key_frame = 0;
      gop_frame->is_arf_frame = 0;
      gop_frame->is_show_frame = 1;
      gop_frame->is_golden_frame = 1;
      gop_frame->encode_ref_mode = EncodeRefMode::kRegular;
      break;
    case GopFrameType::kRegularArf:
      gop_frame->is_key_frame = 0;
      gop_frame->is_arf_frame = 1;
      gop_frame->is_show_frame = 0;
      gop_frame->is_golden_frame = 1;
      gop_frame->encode_ref_mode = EncodeRefMode::kRegular;
      break;
    case GopFrameType::kIntermediateArf:
      gop_frame->is_key_frame = 0;
      gop_frame->is_arf_frame = 1;
      gop_frame->is_show_frame = 0;
      gop_frame->is_golden_frame = 0;
      gop_frame->encode_ref_mode = EncodeRefMode::kRegular;
      break;
    case GopFrameType::kRegularLeaf:
      gop_frame->is_key_frame = 0;
      gop_frame->is_arf_frame = 0;
      gop_frame->is_show_frame = 1;
      gop_frame->is_golden_frame = 0;
      gop_frame->encode_ref_mode = EncodeRefMode::kRegular;
      break;
    case GopFrameType::kIntermediateOverlay:
      gop_frame->is_key_frame = 0;
      gop_frame->is_arf_frame = 0;
      gop_frame->is_show_frame = 1;
      gop_frame->is_golden_frame = 0;
      gop_frame->encode_ref_mode = EncodeRefMode::kShowExisting;
      break;
    case GopFrameType::kOverlay:
      gop_frame->is_key_frame = 0;
      gop_frame->is_arf_frame = 0;
      gop_frame->is_show_frame = 1;
      gop_frame->is_golden_frame = 0;
      gop_frame->encode_ref_mode = EncodeRefMode::kOverlay;
      break;
  }
}

GopFrame GopFrameBasic(int global_coding_idx_offset,
                       int global_order_idx_offset, int coding_idx,
                       int order_idx, int depth, int display_idx,
                       GopFrameType gop_frame_type) {
  GopFrame gop_frame = {};
  gop_frame.is_valid = true;
  gop_frame.coding_idx = coding_idx;
  gop_frame.order_idx = order_idx;
  gop_frame.display_idx = display_idx;
  gop_frame.global_coding_idx = global_coding_idx_offset + coding_idx;
  gop_frame.global_order_idx = global_order_idx_offset + order_idx;
  SetGopFrameByType(gop_frame_type, &gop_frame);
  gop_frame.colocated_ref_idx = -1;
  gop_frame.update_ref_idx = -1;
  gop_frame.layer_depth = depth + kLayerDepthOffset;
  return gop_frame;
}

// This function create gop frames with indices of display order from
// order_start to order_end - 1. The function will recursively introduce
// intermediate ARF untill maximum depth is met or the number of regular frames
// in between two ARFs are less than 3. Than the regular frames will be added
// into the gop_struct.
void ConstructGopMultiLayer(GopStruct *gop_struct,
                            RefFrameManager *ref_frame_manager, int max_depth,
                            int depth, int order_start, int order_end) {
  GopFrame gop_frame;
  int num_frames = order_end - order_start;
  const int global_coding_idx_offset = gop_struct->global_coding_idx_offset;
  const int global_order_idx_offset = gop_struct->global_order_idx_offset;
  // If there are less than kMinIntervalToAddArf frames, stop introducing ARF
  if (depth < max_depth && num_frames >= kMinIntervalToAddArf) {
    int order_mid = (order_start + order_end) / 2;
    // intermediate ARF
    gop_frame = GopFrameBasic(
        global_coding_idx_offset, global_order_idx_offset,
        static_cast<int>(gop_struct->gop_frame_list.size()), order_mid, depth,
        gop_struct->display_tracker, GopFrameType::kIntermediateArf);
    ref_frame_manager->UpdateRefFrameTable(&gop_frame);
    gop_struct->gop_frame_list.push_back(gop_frame);
    ConstructGopMultiLayer(gop_struct, ref_frame_manager, max_depth, depth + 1,
                           order_start, order_mid);
    // show existing intermediate ARF
    gop_frame =
        GopFrameBasic(global_coding_idx_offset, global_order_idx_offset,
                      static_cast<int>(gop_struct->gop_frame_list.size()),
                      order_mid, max_depth, gop_struct->display_tracker,
                      GopFrameType::kIntermediateOverlay);
    ref_frame_manager->UpdateRefFrameTable(&gop_frame);
    gop_struct->gop_frame_list.push_back(gop_frame);
    ++gop_struct->display_tracker;
    ConstructGopMultiLayer(gop_struct, ref_frame_manager, max_depth, depth + 1,
                           order_mid + 1, order_end);
  } else {
    // regular frame
    for (int i = order_start; i < order_end; ++i) {
      gop_frame = GopFrameBasic(
          global_coding_idx_offset, global_order_idx_offset,
          static_cast<int>(gop_struct->gop_frame_list.size()), i, max_depth,
          gop_struct->display_tracker, GopFrameType::kRegularLeaf);
      ref_frame_manager->UpdateRefFrameTable(&gop_frame);
      gop_struct->gop_frame_list.push_back(gop_frame);
      ++gop_struct->display_tracker;
    }
  }
}

GopStruct ConstructGop(RefFrameManager *ref_frame_manager, int show_frame_count,
                       bool has_key_frame, int global_coding_idx_offset,
                       int global_order_idx_offset) {
  GopStruct gop_struct;
  gop_struct.show_frame_count = show_frame_count;
  gop_struct.global_coding_idx_offset = global_coding_idx_offset;
  gop_struct.global_order_idx_offset = global_order_idx_offset;
  int order_start = 0;
  int order_end = show_frame_count - 1;

  // TODO(jingning): Re-enable the use of pyramid coding structure.
  bool has_arf_frame = show_frame_count > kMinIntervalToAddArf;

  gop_struct.display_tracker = 0;

  GopFrame gop_frame;
  if (has_key_frame) {
    const int key_frame_depth = -1;
    ref_frame_manager->Reset();
    gop_frame = GopFrameBasic(
        global_coding_idx_offset, global_order_idx_offset,
        static_cast<int>(gop_struct.gop_frame_list.size()), order_start,
        key_frame_depth, gop_struct.display_tracker, GopFrameType::kRegularKey);
    ref_frame_manager->UpdateRefFrameTable(&gop_frame);
    gop_struct.gop_frame_list.push_back(gop_frame);
    order_start++;
    ++gop_struct.display_tracker;
  }

  const int arf_depth = 0;
  if (has_arf_frame) {
    // Use multi-layer pyrmaid coding structure.
    gop_frame = GopFrameBasic(
        global_coding_idx_offset, global_order_idx_offset,
        static_cast<int>(gop_struct.gop_frame_list.size()), order_end,
        arf_depth, gop_struct.display_tracker, GopFrameType::kRegularArf);
    ref_frame_manager->UpdateRefFrameTable(&gop_frame);
    gop_struct.gop_frame_list.push_back(gop_frame);
    ConstructGopMultiLayer(&gop_struct, ref_frame_manager,
                           ref_frame_manager->ForwardMaxSize(), arf_depth + 1,
                           order_start, order_end);
    // Overlay
    gop_frame =
        GopFrameBasic(global_coding_idx_offset, global_order_idx_offset,
                      static_cast<int>(gop_struct.gop_frame_list.size()),
                      order_end, ref_frame_manager->ForwardMaxSize(),
                      gop_struct.display_tracker, GopFrameType::kOverlay);
    ref_frame_manager->UpdateRefFrameTable(&gop_frame);
    gop_struct.gop_frame_list.push_back(gop_frame);
    ++gop_struct.display_tracker;
  } else {
    // Use IPPP format.
    for (int i = order_start; i <= order_end; ++i) {
      gop_frame = GopFrameBasic(
          global_coding_idx_offset, global_order_idx_offset,
          static_cast<int>(gop_struct.gop_frame_list.size()), i, arf_depth + 1,
          gop_struct.display_tracker, GopFrameType::kRegularLeaf);
      ref_frame_manager->UpdateRefFrameTable(&gop_frame);
      gop_struct.gop_frame_list.push_back(gop_frame);
      ++gop_struct.display_tracker;
    }
  }

  return gop_struct;
}

Status AV1RateControlQMode::SetRcParam(const RateControlParam &rc_param) {
  std::ostringstream error_message;
  if (rc_param.max_gop_show_frame_count <
      std::max(4, rc_param.min_gop_show_frame_count)) {
    error_message << "max_gop_show_frame_count ("
                  << rc_param.max_gop_show_frame_count
                  << ") must be at least 4 and may not be less than "
                     "min_gop_show_frame_count ("
                  << rc_param.min_gop_show_frame_count << ")";
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  if (rc_param.ref_frame_table_size < 1 || rc_param.ref_frame_table_size > 8) {
    error_message << "ref_frame_table_size (" << rc_param.ref_frame_table_size
                  << ") must be in the range [1, 8].";
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  if (rc_param.max_ref_frames < 1 || rc_param.max_ref_frames > 7) {
    error_message << "max_ref_frames (" << rc_param.max_ref_frames
                  << ") must be in the range [1, 7].";
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  if (rc_param.max_depth < 1 || rc_param.max_depth > 5) {
    error_message << "max_depth (" << rc_param.max_depth
                  << ") must be in the range [1, 5].";
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  if (rc_param.base_q_index < 0 || rc_param.base_q_index > 255) {
    error_message << "base_q_index (" << rc_param.base_q_index
                  << ") must be in the range [0, 255].";
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  if (rc_param.frame_width < 16 || rc_param.frame_width > 16384 ||
      rc_param.frame_height < 16 || rc_param.frame_height > 16384) {
    error_message << "frame_width (" << rc_param.frame_width
                  << ") and frame_height (" << rc_param.frame_height
                  << ") must be in the range [16, 16384].";
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  rc_param_ = rc_param;
  return { AOM_CODEC_OK, "" };
}

// Threshold for use of the lagging second reference frame. High second ref
// usage may point to a transient event like a flash or occlusion rather than
// a real scene cut.
// We adapt the threshold based on number of frames in this key-frame group so
// far.
static double GetSecondRefUsageThreshold(int frame_count_so_far) {
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

// Slide show transition detection.
// Tests for case where there is very low error either side of the current frame
// but much higher just for this frame. This can help detect key frames in
// slide shows even where the slides are pictures of different sizes.
// Also requires that intra and inter errors are very similar to help eliminate
// harmful false positives.
// It will not help if the transition is a fade or other multi-frame effect.
static bool DetectSlideTransition(const FIRSTPASS_STATS &this_frame,
                                  const FIRSTPASS_STATS &last_frame,
                                  const FIRSTPASS_STATS &next_frame) {
  // Intra / Inter threshold very low
  constexpr double kVeryLowII = 1.5;
  // Clean slide transitions we expect a sharp single frame spike in error.
  constexpr double kErrorSpike = 5.0;

  // TODO(angiebird): Understand the meaning of these conditions.
  return (this_frame.intra_error < (this_frame.coded_error * kVeryLowII)) &&
         (this_frame.coded_error > (last_frame.coded_error * kErrorSpike)) &&
         (this_frame.coded_error > (next_frame.coded_error * kErrorSpike));
}

// Check if there is a significant intra/inter error change between the current
// frame and its neighbor. If so, we should further test whether the current
// frame should be a key frame.
static bool DetectIntraInterErrorChange(const FIRSTPASS_STATS &this_stats,
                                        const FIRSTPASS_STATS &last_stats,
                                        const FIRSTPASS_STATS &next_stats) {
  // Minimum % intra coding observed in first pass (1.0 = 100%)
  constexpr double kMinIntraLevel = 0.25;
  // Minimum ratio between the % of intra coding and inter coding in the first
  // pass after discounting neutral blocks (discounting neutral blocks in this
  // way helps catch scene cuts in clips with very flat areas or letter box
  // format clips with image padding.
  constexpr double kIntraVsInterRatio = 2.0;

  const double modified_pcnt_inter =
      this_stats.pcnt_inter - this_stats.pcnt_neutral;
  const double pcnt_intra_min =
      std::max(kMinIntraLevel, kIntraVsInterRatio * modified_pcnt_inter);

  // In real scene cuts there is almost always a sharp change in the intra
  // or inter error score.
  constexpr double kErrorChangeThreshold = 0.4;
  const double last_this_error_ratio =
      fabs(last_stats.coded_error - this_stats.coded_error) /
      ModifyDivisor(this_stats.coded_error);

  const double this_next_error_ratio =
      fabs(last_stats.intra_error - this_stats.intra_error) /
      ModifyDivisor(this_stats.intra_error);

  // Maximum threshold for the relative ratio of intra error score vs best
  // inter error score.
  constexpr double kThisIntraCodedErrorRatioMax = 1.9;
  const double this_intra_coded_error_ratio =
      this_stats.intra_error / ModifyDivisor(this_stats.coded_error);

  // For real scene cuts we expect an improvment in the intra inter error
  // ratio in the next frame.
  constexpr double kNextIntraCodedErrorRatioMin = 3.5;
  const double next_intra_coded_error_ratio =
      next_stats.intra_error / ModifyDivisor(next_stats.coded_error);

  double pcnt_intra = 1.0 - this_stats.pcnt_inter;
  return pcnt_intra > pcnt_intra_min &&
         this_intra_coded_error_ratio < kThisIntraCodedErrorRatioMax &&
         (last_this_error_ratio > kErrorChangeThreshold ||
          this_next_error_ratio > kErrorChangeThreshold ||
          next_intra_coded_error_ratio > kNextIntraCodedErrorRatioMin);
}

// Check whether the candidate can be a key frame.
// This is a rewrite of test_candidate_kf().
static bool TestCandidateKey(const FirstpassInfo &first_pass_info,
                             int candidate_key_idx, int frames_since_prev_key) {
  const auto &stats_list = first_pass_info.stats_list;
  const int stats_count = static_cast<int>(stats_list.size());
  if (candidate_key_idx + 1 >= stats_count || candidate_key_idx - 1 < 0) {
    return false;
  }
  const auto &last_stats = stats_list[candidate_key_idx - 1];
  const auto &this_stats = stats_list[candidate_key_idx];
  const auto &next_stats = stats_list[candidate_key_idx + 1];

  if (frames_since_prev_key < 3) return false;
  const double second_ref_usage_threshold =
      GetSecondRefUsageThreshold(frames_since_prev_key);
  if (this_stats.pcnt_second_ref >= second_ref_usage_threshold) return false;
  if (next_stats.pcnt_second_ref >= second_ref_usage_threshold) return false;

  // Hard threshold where the first pass chooses intra for almost all blocks.
  // In such a case even if the frame is not a scene cut coding a key frame
  // may be a good option.
  constexpr double kVeryLowInterThreshold = 0.05;
  if (this_stats.pcnt_inter < kVeryLowInterThreshold ||
      DetectSlideTransition(this_stats, last_stats, next_stats) ||
      DetectIntraInterErrorChange(this_stats, last_stats, next_stats)) {
    double boost_score = 0.0;
    double decay_accumulator = 1.0;

    // We do "-1" because the candidate key is not counted.
    int stats_after_this_stats = stats_count - candidate_key_idx - 1;

    // Number of frames required to test for scene cut detection
    constexpr int kSceneCutKeyTestIntervalMax = 16;

    // Make sure we have enough stats after the candidate key.
    const int frames_to_test_after_candidate_key =
        std::min(kSceneCutKeyTestIntervalMax, stats_after_this_stats);

    // Examine how well the key frame predicts subsequent frames.
    int i;
    for (i = 1; i <= frames_to_test_after_candidate_key; ++i) {
      // Get the next frame details
      const auto &stats = stats_list[candidate_key_idx + i];

      // Cumulative effect of decay in prediction quality.
      if (stats.pcnt_inter > 0.85) {
        decay_accumulator *= stats.pcnt_inter;
      } else {
        decay_accumulator *= (0.85 + stats.pcnt_inter) / 2.0;
      }

      constexpr double kBoostFactor = 12.5;
      double next_iiratio =
          (kBoostFactor * stats.intra_error / ModifyDivisor(stats.coded_error));
      next_iiratio = std::min(next_iiratio, 128.0);
      double boost_score_increment = decay_accumulator * next_iiratio;

      // Keep a running total.
      boost_score += boost_score_increment;

      // Test various breakout clauses.
      // TODO(any): Test of intra error should be normalized to an MB.
      // TODO(angiebird): Investigate the following questions.
      // Question 1: next_iiratio (intra_error / coded_error) * kBoostFactor
      // We know intra_error / coded_error >= 1 and kBoostFactor = 12.5,
      // therefore, (intra_error / coded_error) * kBoostFactor will always
      // greater than 1.5. Is "next_iiratio < 1.5" always false?
      // Question 2: Similar to question 1, is "next_iiratio < 3.0" always true?
      // Question 3: Why do we need to divide 200 with num_mbs_16x16?
      if ((stats.pcnt_inter < 0.05) || (next_iiratio < 1.5) ||
          (((stats.pcnt_inter - stats.pcnt_neutral) < 0.20) &&
           (next_iiratio < 3.0)) ||
          (boost_score_increment < 3.0) ||
          (stats.intra_error <
           (200.0 / static_cast<double>(first_pass_info.num_mbs_16x16)))) {
        break;
      }
    }

    // If there is tolerable prediction for at least the next 3 frames then
    // break out else discard this potential key frame and move on
    const int count_for_tolerable_prediction = 3;
    if (boost_score > 30.0 && (i > count_for_tolerable_prediction)) {
      return true;
    }
  }
  return false;
}

// Compute key frame location from first_pass_info.
std::vector<int> GetKeyFrameList(const FirstpassInfo &first_pass_info) {
  std::vector<int> key_frame_list;
  key_frame_list.push_back(0);  // The first frame is always a key frame
  int candidate_key_idx = 1;
  while (candidate_key_idx <
         static_cast<int>(first_pass_info.stats_list.size())) {
    const int frames_since_prev_key = candidate_key_idx - key_frame_list.back();
    // Check for a scene cut.
    const bool scenecut_detected = TestCandidateKey(
        first_pass_info, candidate_key_idx, frames_since_prev_key);
    if (scenecut_detected) {
      key_frame_list.push_back(candidate_key_idx);
    }
    ++candidate_key_idx;
  }
  return key_frame_list;
}

// initialize GF_GROUP_STATS
static void InitGFStats(GF_GROUP_STATS *gf_stats) {
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

static int FindRegionIndex(const std::vector<REGIONS> &regions, int frame_idx) {
  for (int k = 0; k < static_cast<int>(regions.size()); k++) {
    if (regions[k].start <= frame_idx && regions[k].last >= frame_idx) {
      return k;
    }
  }
  return -1;
}

// This function detects a flash through the high relative pcnt_second_ref
// score in the frame following a flash frame. The offset passed in should
// reflect this.
static bool DetectFlash(const std::vector<FIRSTPASS_STATS> &stats_list,
                        int index) {
  int next_index = index + 1;
  if (next_index >= static_cast<int>(stats_list.size())) return false;
  const FIRSTPASS_STATS &next_frame = stats_list[next_index];

  // What we are looking for here is a situation where there is a
  // brief break in prediction (such as a flash) but subsequent frames
  // are reasonably well predicted by an earlier (pre flash) frame.
  // The recovery after a flash is indicated by a high pcnt_second_ref
  // compared to pcnt_inter.
  return next_frame.pcnt_second_ref > next_frame.pcnt_inter &&
         next_frame.pcnt_second_ref >= 0.5;
}

#define MIN_SHRINK_LEN 6

// This function takes in a suggesting gop interval from cur_start to cur_last,
// analyzes firstpass stats and region stats and then return a better gop cut
// location.
// TODO(b/231517281): Simplify the indices once we have an unit test.
// We are using four indices here, order_index, cur_start, cur_last, and
// frames_since_key. Ideally, only three indices are needed.
// 1) start_index = order_index + cur_start
// 2) end_index = order_index + cur_end
// 3) key_index
int FindBetterGopCut(const std::vector<FIRSTPASS_STATS> &stats_list,
                     const std::vector<REGIONS> &regions_list,
                     int min_gop_show_frame_count, int max_gop_show_frame_count,
                     int order_index, int cur_start, int cur_last,
                     int frames_since_key) {
  // only try shrinking if interval smaller than active_max_gf_interval
  if (cur_last - cur_start > max_gop_show_frame_count ||
      cur_start >= cur_last) {
    return cur_last;
  }
  int num_regions = static_cast<int>(regions_list.size());
  int num_stats = static_cast<int>(stats_list.size());
  const int min_shrink_int = std::max(MIN_SHRINK_LEN, min_gop_show_frame_count);

  // find the region indices of where the first and last frame belong.
  int k_start = FindRegionIndex(regions_list, cur_start + frames_since_key);
  int k_last = FindRegionIndex(regions_list, cur_last + frames_since_key);
  if (cur_start + frames_since_key == 0) k_start = 0;

  int scenecut_idx = -1;
  // See if we have a scenecut in between
  for (int r = k_start + 1; r <= k_last; r++) {
    if (regions_list[r].type == SCENECUT_REGION &&
        regions_list[r].last - frames_since_key - cur_start >
            min_gop_show_frame_count) {
      scenecut_idx = r;
      break;
    }
  }

  // if the found scenecut is very close to the end, ignore it.
  if (scenecut_idx >= 0 &&
      regions_list[num_regions - 1].last - regions_list[scenecut_idx].last <
          4) {
    scenecut_idx = -1;
  }

  if (scenecut_idx != -1) {
    // If we have a scenecut, then stop at it.
    // TODO(bohanli): add logic here to stop before the scenecut and for
    // the next gop start from the scenecut with GF
    int is_minor_sc =
        (regions_list[scenecut_idx].avg_cor_coeff *
             (1 - stats_list[order_index + regions_list[scenecut_idx].start -
                             frames_since_key]
                          .noise_var /
                      regions_list[scenecut_idx].avg_intra_err) >
         0.6);
    cur_last =
        regions_list[scenecut_idx].last - frames_since_key - !is_minor_sc;
  } else {
    int is_last_analysed =
        (k_last == num_regions - 1) &&
        (cur_last + frames_since_key == regions_list[k_last].last);
    int not_enough_regions =
        k_last - k_start <= 1 + (regions_list[k_start].type == SCENECUT_REGION);
    // if we are very close to the end, then do not shrink since it may
    // introduce intervals that are too short
    if (!(is_last_analysed && not_enough_regions)) {
      const double arf_length_factor = 0.1;
      double best_score = 0;
      int best_j = -1;
      const int first_frame = regions_list[0].start - frames_since_key;
      const int last_frame =
          regions_list[num_regions - 1].last - frames_since_key;
      // score of how much the arf helps the whole GOP
      double base_score = 0.0;
      // Accumulate base_score in
      for (int j = cur_start + 1; j < cur_start + min_shrink_int; j++) {
        if (order_index + j >= num_stats) break;
        base_score = (base_score + 1.0) * stats_list[order_index + j].cor_coeff;
      }
      int met_blending = 0;   // Whether we have met blending areas before
      int last_blending = 0;  // Whether the previous frame if blending
      for (int j = cur_start + min_shrink_int; j <= cur_last; j++) {
        if (order_index + j >= num_stats) break;
        base_score = (base_score + 1.0) * stats_list[order_index + j].cor_coeff;
        int this_reg = FindRegionIndex(regions_list, j + frames_since_key);
        if (this_reg < 0) continue;
        // A GOP should include at most 1 blending region.
        if (regions_list[this_reg].type == BLENDING_REGION) {
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
          if (order_index + n >= num_stats) break;
          temp_accu_coeff *= stats_list[order_index + n].cor_coeff;
          this_score +=
              temp_accu_coeff *
              (1 - stats_list[order_index + n].noise_var /
                       AOMMAX(regions_list[this_reg].avg_intra_err, 0.001));
          count_f++;
        }
        // preceding frames
        temp_accu_coeff = 1.0;
        for (int n = j; n > j - 3 * 2 + count_f && n > first_frame; n--) {
          if (order_index + n < 0) break;
          temp_accu_coeff *= stats_list[order_index + n].cor_coeff;
          this_score +=
              temp_accu_coeff *
              (1 - stats_list[order_index + n].noise_var /
                       AOMMAX(regions_list[this_reg].avg_intra_err, 0.001));
        }

        if (this_score > best_score) {
          best_score = this_score;
          best_j = j;
        }
      }

      // For blending areas, move one more frame in case we missed the
      // first blending frame.
      int best_reg = FindRegionIndex(regions_list, best_j + frames_since_key);
      if (best_reg < num_regions - 1 && best_reg > 0) {
        if (regions_list[best_reg - 1].type == BLENDING_REGION &&
            regions_list[best_reg + 1].type == BLENDING_REGION) {
          if (best_j + frames_since_key == regions_list[best_reg].start &&
              best_j + frames_since_key < regions_list[best_reg].last) {
            best_j += 1;
          } else if (best_j + frames_since_key == regions_list[best_reg].last &&
                     best_j + frames_since_key > regions_list[best_reg].start) {
            best_j -= 1;
          }
        }
      }

      if (cur_last - best_j < 2) best_j = cur_last;
      if (best_j > 0 && best_score > 0.1) cur_last = best_j;
      // if cannot find anything, just cut at the original place.
    }
  }

  return cur_last;
}

// Function to test for a condition where a complex transition is followed
// by a static section. For example in slide shows where there is a fade
// between slides. This is to help with more optimal kf and gf positioning.
static bool DetectTransitionToStill(
    const std::vector<FIRSTPASS_STATS> &stats_list, int next_stats_index,
    int min_gop_show_frame_count, int frame_interval, int still_interval,
    double loop_decay_rate, double last_decay_rate) {
  // Break clause to detect very still sections after motion
  // For example a static image after a fade or other transition
  // instead of a clean scene cut.
  if (frame_interval > min_gop_show_frame_count && loop_decay_rate >= 0.999 &&
      last_decay_rate < 0.9) {
    int stats_count = static_cast<int>(stats_list.size());
    int stats_left = stats_count - next_stats_index;
    if (stats_left >= still_interval) {
      // Look ahead a few frames to see if static condition persists...
      int j;
      for (j = 0; j < still_interval; ++j) {
        const FIRSTPASS_STATS &stats = stats_list[next_stats_index + j];
        if (stats.pcnt_inter - stats.pcnt_motion < 0.999) break;
      }
      // Only if it does do we signal a transition to still.
      return j == still_interval;
    }
  }
  return false;
}

static int DetectGopCut(const std::vector<FIRSTPASS_STATS> &stats_list,
                        int start_idx, int candidate_cut_idx, int next_key_idx,
                        int flash_detected, int min_gop_show_frame_count,
                        int max_gop_show_frame_count, int frame_width,
                        int frame_height, const GF_GROUP_STATS &gf_stats) {
  (void)max_gop_show_frame_count;
  const int candidate_gop_size = candidate_cut_idx - start_idx;

  if (!flash_detected) {
    // Break clause to detect very still sections after motion. For example,
    // a static image after a fade or other transition.
    if (DetectTransitionToStill(stats_list, start_idx, min_gop_show_frame_count,
                                candidate_gop_size, 5, gf_stats.loop_decay_rate,
                                gf_stats.last_loop_decay_rate)) {
      return 1;
    }
    const double arf_abs_zoom_thresh = 4.4;
    // Motion breakout threshold for loop below depends on image size.
    const double mv_ratio_accumulator_thresh =
        (frame_height + frame_width) / 4.0;
    // Some conditions to breakout after min interval.
    if (candidate_gop_size >= min_gop_show_frame_count &&
        // If possible don't break very close to a kf
        (next_key_idx - candidate_cut_idx >= min_gop_show_frame_count) &&
        (candidate_gop_size & 0x01) &&
        (gf_stats.mv_ratio_accumulator > mv_ratio_accumulator_thresh ||
         gf_stats.abs_mv_in_out_accumulator > arf_abs_zoom_thresh)) {
      return 1;
    }
  }

  // TODO(b/231489624): Check if we need this part.
  // If almost totally static, we will not use the the max GF length later,
  // so we can continue for more frames.
  // if ((candidate_gop_size >= active_max_gf_interval + 1) &&
  //     !is_almost_static(gf_stats->zero_motion_accumulator,
  //                       twopass->kf_zeromotion_pct, cpi->ppi->lap_enabled)) {
  //   return 0;
  // }
  return 0;
}

/*!\brief Determine the length of future GF groups.
 *
 * \ingroup gf_group_algo
 * This function decides the gf group length of future frames in batch
 *
 * \param[in]    rc_param         Rate control parameters
 * \param[in]    stats_list       List of first pass stats
 * \param[in]    regions_list     List of regions from av1_identify_regions
 * \param[in]    order_index      Index of current frame in stats_list
 * \param[in]    frames_since_key Number of frames since the last key frame
 * \param[in]    frames_to_key    Number of frames to the next key frame
 *
 * \return Returns a vector of decided GF group lengths.
 */
static std::vector<int> PartitionGopIntervals(
    const RateControlParam &rc_param,
    const std::vector<FIRSTPASS_STATS> &stats_list,
    const std::vector<REGIONS> &regions_list, int order_index,
    int frames_since_key, int frames_to_key) {
  int i = 0;
  // If cpi->gf_state.arf_gf_boost_lst is 0, we are starting with a KF or GF.
  int cur_start = 0;
  // Each element is the last frame of the previous GOP. If there are n GOPs,
  // you need n + 1 cuts to find the durations. So cut_pos starts out with -1,
  // which is the last frame of the previous GOP.
  std::vector<int> cut_pos(1, -1);
  int cut_here = 0;
  GF_GROUP_STATS gf_stats;
  InitGFStats(&gf_stats);
  int num_stats = static_cast<int>(stats_list.size());

  while (i + order_index < num_stats) {
    // reaches next key frame, break here
    if (i >= frames_to_key - 1) {
      cut_here = 2;
    } else if (i - cur_start >= rc_param.max_gop_show_frame_count) {
      // reached maximum len, but nothing special yet (almost static)
      // let's look at the next interval
      cut_here = 2;
    } else {
      // Test for the case where there is a brief flash but the prediction
      // quality back to an earlier frame is then restored.
      const int gop_start_idx = cur_start + order_index;
      const int candidate_gop_cut_idx = i + order_index;
      const int next_key_idx = frames_to_key + order_index;
      const bool flash_detected =
          DetectFlash(stats_list, candidate_gop_cut_idx);

      // TODO(bohanli): remove redundant accumulations here, or unify
      // this and the ones in define_gf_group
      const FIRSTPASS_STATS *stats = &stats_list[candidate_gop_cut_idx];
      av1_accumulate_next_frame_stats(stats, flash_detected, frames_since_key,
                                      i, &gf_stats, rc_param.frame_width,
                                      rc_param.frame_height);

      // TODO(angiebird): Can we simplify this part? Looks like we are going to
      // change the gop cut index with FindBetterGopCut() anyway.
      cut_here = DetectGopCut(
          stats_list, gop_start_idx, candidate_gop_cut_idx, next_key_idx,
          flash_detected, rc_param.min_gop_show_frame_count,
          rc_param.max_gop_show_frame_count, rc_param.frame_width,
          rc_param.frame_height, gf_stats);
    }

    if (!cut_here) {
      ++i;
      continue;
    }

    // the current last frame in the gf group
    int original_last = cut_here > 1 ? i : i - 1;
    int cur_last = FindBetterGopCut(
        stats_list, regions_list, rc_param.min_gop_show_frame_count,
        rc_param.max_gop_show_frame_count, order_index, cur_start,
        original_last, frames_since_key);
    // only try shrinking if interval smaller than active_max_gf_interval
    cut_pos.push_back(cur_last);

    // reset pointers to the shrunken location
    cur_start = cur_last;
    int cur_region_idx =
        FindRegionIndex(regions_list, cur_start + 1 + frames_since_key);
    if (cur_region_idx >= 0)
      if (regions_list[cur_region_idx].type == SCENECUT_REGION) cur_start++;

    // reset accumulators
    InitGFStats(&gf_stats);
    i = cur_last + 1;

    if (cut_here == 2 && i >= frames_to_key) break;
  }

  std::vector<int> gf_intervals;
  // save intervals
  for (size_t n = 1; n < cut_pos.size(); n++) {
    gf_intervals.push_back(cut_pos[n] - cut_pos[n - 1]);
  }

  return gf_intervals;
}

StatusOr<GopStructList> AV1RateControlQMode::DetermineGopInfo(
    const FirstpassInfo &firstpass_info) {
  const int stats_size = static_cast<int>(firstpass_info.stats_list.size());
  GopStructList gop_list;
  RefFrameManager ref_frame_manager(rc_param_.ref_frame_table_size);

  int global_coding_idx_offset = 0;
  int global_order_idx_offset = 0;
  std::vector<int> key_frame_list = GetKeyFrameList(firstpass_info);
  key_frame_list.push_back(stats_size);  // a sentinel value
  for (size_t ki = 0; ki + 1 < key_frame_list.size(); ++ki) {
    int frames_to_key = key_frame_list[ki + 1] - key_frame_list[ki];
    int key_order_index = key_frame_list[ki];  // The key frame's display order

    std::vector<REGIONS> regions_list(MAX_FIRSTPASS_ANALYSIS_FRAMES);
    int total_regions = 0;
    av1_identify_regions(firstpass_info.stats_list.data() + key_order_index,
                         frames_to_key, 0, regions_list.data(), &total_regions);
    regions_list.resize(total_regions);
    std::vector<int> gf_intervals = PartitionGopIntervals(
        rc_param_, firstpass_info.stats_list, regions_list, key_order_index,
        /*frames_since_key=*/0, frames_to_key);
    for (size_t gi = 0; gi < gf_intervals.size(); ++gi) {
      const bool has_key_frame = gi == 0;
      const int show_frame_count = gf_intervals[gi];
      GopStruct gop =
          ConstructGop(&ref_frame_manager, show_frame_count, has_key_frame,
                       global_coding_idx_offset, global_order_idx_offset);
      assert(gop.show_frame_count == show_frame_count);
      global_coding_idx_offset += static_cast<int>(gop.gop_frame_list.size());
      global_order_idx_offset += gop.show_frame_count;
      gop_list.push_back(gop);
    }
  }
  return gop_list;
}

TplFrameDepStats CreateTplFrameDepStats(int frame_height, int frame_width,
                                        int min_block_size) {
  const int unit_rows = (frame_height + min_block_size - 1) / min_block_size;
  const int unit_cols = (frame_width + min_block_size - 1) / min_block_size;
  TplFrameDepStats frame_dep_stats;
  frame_dep_stats.unit_size = min_block_size;
  frame_dep_stats.unit_stats.resize(unit_rows);
  for (auto &row : frame_dep_stats.unit_stats) {
    row.resize(unit_cols);
  }
  return frame_dep_stats;
}

TplUnitDepStats TplBlockStatsToDepStats(const TplBlockStats &block_stats,
                                        int unit_count) {
  TplUnitDepStats dep_stats = {};
  dep_stats.intra_cost = block_stats.intra_cost * 1.0 / unit_count;
  dep_stats.inter_cost = block_stats.inter_cost * 1.0 / unit_count;
  // In rare case, inter_cost may be greater than intra_cost.
  // If so, we need to modify inter_cost such that inter_cost <= intra_cost
  // because it is required by GetPropagationFraction()
  dep_stats.inter_cost = std::min(dep_stats.intra_cost, dep_stats.inter_cost);
  dep_stats.mv = block_stats.mv;
  dep_stats.ref_frame_index = block_stats.ref_frame_index;
  return dep_stats;
}

namespace {
Status ValidateBlockStats(const TplFrameStats &frame_stats,
                          const TplBlockStats &block_stats,
                          int min_block_size) {
  if (block_stats.col >= frame_stats.frame_width ||
      block_stats.row >= frame_stats.frame_height) {
    std::ostringstream error_message;
    error_message << "Block position (" << block_stats.col << ", "
                  << block_stats.row
                  << ") is out of range; frame dimensions are "
                  << frame_stats.frame_width << " x "
                  << frame_stats.frame_height;
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  if (block_stats.col % min_block_size != 0 ||
      block_stats.row % min_block_size != 0 ||
      block_stats.width % min_block_size != 0 ||
      block_stats.height % min_block_size != 0) {
    std::ostringstream error_message;
    error_message
        << "Invalid block position or dimension, must be a multiple of "
        << min_block_size << "; col = " << block_stats.col
        << ", row = " << block_stats.row << ", width = " << block_stats.width
        << ", height = " << block_stats.height;
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  return { AOM_CODEC_OK, "" };
}

Status ValidateTplStats(const GopStruct &gop_struct,
                        const TplGopStats &tpl_gop_stats) {
  constexpr char kAdvice[] =
      "Do the current RateControlParam settings match those used to generate "
      "the TPL stats?";
  if (gop_struct.gop_frame_list.size() !=
      tpl_gop_stats.frame_stats_list.size()) {
    std::ostringstream error_message;
    error_message << "Frame count of GopStruct ("
                  << gop_struct.gop_frame_list.size()
                  << ") doesn't match frame count of TPL stats ("
                  << tpl_gop_stats.frame_stats_list.size() << "). " << kAdvice;
    return { AOM_CODEC_INVALID_PARAM, error_message.str() };
  }
  for (int i = 0; i < static_cast<int>(gop_struct.gop_frame_list.size()); ++i) {
    const bool is_ref_frame = gop_struct.gop_frame_list[i].update_ref_idx >= 0;
    const bool has_tpl_stats =
        !tpl_gop_stats.frame_stats_list[i].block_stats_list.empty();
    if (is_ref_frame && !has_tpl_stats) {
      std::ostringstream error_message;
      error_message << "The frame with global_coding_idx "
                    << gop_struct.gop_frame_list[i].global_coding_idx
                    << " is a reference frame, but has no TPL stats. "
                    << kAdvice;
      return { AOM_CODEC_INVALID_PARAM, error_message.str() };
    }
  }
  return { AOM_CODEC_OK, "" };
}
}  // namespace

StatusOr<TplFrameDepStats> CreateTplFrameDepStatsWithoutPropagation(
    const TplFrameStats &frame_stats) {
  if (frame_stats.block_stats_list.empty()) {
    return TplFrameDepStats();
  }
  const int min_block_size = frame_stats.min_block_size;
  const int unit_rows =
      (frame_stats.frame_height + min_block_size - 1) / min_block_size;
  const int unit_cols =
      (frame_stats.frame_width + min_block_size - 1) / min_block_size;
  TplFrameDepStats frame_dep_stats = CreateTplFrameDepStats(
      frame_stats.frame_height, frame_stats.frame_width, min_block_size);
  for (const TplBlockStats &block_stats : frame_stats.block_stats_list) {
    Status status =
        ValidateBlockStats(frame_stats, block_stats, min_block_size);
    if (!status.ok()) {
      return status;
    }
    const int block_unit_row = block_stats.row / min_block_size;
    const int block_unit_col = block_stats.col / min_block_size;
    // The block must start within the frame boundaries, but it may extend past
    // the right edge or bottom of the frame. Find the number of unit rows and
    // columns in the block which are fully within the frame.
    const int block_unit_rows = std::min(block_stats.height / min_block_size,
                                         unit_rows - block_unit_row);
    const int block_unit_cols = std::min(block_stats.width / min_block_size,
                                         unit_cols - block_unit_col);
    const int unit_count = block_unit_rows * block_unit_cols;
    TplUnitDepStats unit_stats =
        TplBlockStatsToDepStats(block_stats, unit_count);
    for (int r = 0; r < block_unit_rows; r++) {
      for (int c = 0; c < block_unit_cols; c++) {
        frame_dep_stats.unit_stats[block_unit_row + r][block_unit_col + c] =
            unit_stats;
      }
    }
  }
  return frame_dep_stats;
}

int GetRefCodingIdxList(const TplUnitDepStats &unit_dep_stats,
                        const RefFrameTable &ref_frame_table,
                        int *ref_coding_idx_list) {
  int ref_frame_count = 0;
  for (int i = 0; i < kBlockRefCount; ++i) {
    ref_coding_idx_list[i] = -1;
    int ref_frame_index = unit_dep_stats.ref_frame_index[i];
    if (ref_frame_index != -1) {
      assert(ref_frame_index < static_cast<int>(ref_frame_table.size()));
      ref_coding_idx_list[i] = ref_frame_table[ref_frame_index].coding_idx;
      ref_frame_count++;
    }
  }
  return ref_frame_count;
}

int GetBlockOverlapArea(int r0, int c0, int r1, int c1, int size) {
  const int r_low = std::max(r0, r1);
  const int r_high = std::min(r0 + size, r1 + size);
  const int c_low = std::max(c0, c1);
  const int c_high = std::min(c0 + size, c1 + size);
  if (r_high >= r_low && c_high >= c_low) {
    return (r_high - r_low) * (c_high - c_low);
  }
  return 0;
}

// TODO(angiebird): Merge TplFrameDepStatsAccumulateIntraCost and
// TplFrameDepStatsAccumulate.
double TplFrameDepStatsAccumulateIntraCost(
    const TplFrameDepStats &frame_dep_stats) {
  auto getIntraCost = [](double sum, const TplUnitDepStats &unit) {
    return sum + unit.intra_cost;
  };
  double sum = 0;
  for (const auto &row : frame_dep_stats.unit_stats) {
    sum = std::accumulate(row.begin(), row.end(), sum, getIntraCost);
  }
  return sum;
}

double TplFrameDepStatsAccumulate(const TplFrameDepStats &frame_dep_stats) {
  auto getOverallCost = [](double sum, const TplUnitDepStats &unit) {
    return sum + unit.propagation_cost + unit.intra_cost;
  };
  double sum = 0;
  for (const auto &row : frame_dep_stats.unit_stats) {
    sum = std::accumulate(row.begin(), row.end(), sum, getOverallCost);
  }
  return sum;
}

// This is a generalization of GET_MV_RAWPEL that allows for an arbitrary
// number of fractional bits.
// TODO(angiebird): Add unit test to this function
int GetFullpelValue(int subpel_value, int subpel_bits) {
  const int subpel_scale = (1 << subpel_bits);
  const int sign = subpel_value >= 0 ? 1 : -1;
  int fullpel_value = (abs(subpel_value) + subpel_scale / 2) >> subpel_bits;
  fullpel_value *= sign;
  return fullpel_value;
}

double GetPropagationFraction(const TplUnitDepStats &unit_dep_stats) {
  assert(unit_dep_stats.intra_cost >= unit_dep_stats.inter_cost);
  return (unit_dep_stats.intra_cost - unit_dep_stats.inter_cost) /
         ModifyDivisor(unit_dep_stats.intra_cost);
}

void TplFrameDepStatsPropagate(int coding_idx,
                               const RefFrameTable &ref_frame_table,
                               TplGopDepStats *tpl_gop_dep_stats) {
  assert(!tpl_gop_dep_stats->frame_dep_stats_list.empty());
  TplFrameDepStats *frame_dep_stats =
      &tpl_gop_dep_stats->frame_dep_stats_list[coding_idx];

  if (frame_dep_stats->unit_stats.empty()) return;

  const int unit_size = frame_dep_stats->unit_size;
  const int frame_unit_rows =
      static_cast<int>(frame_dep_stats->unit_stats.size());
  const int frame_unit_cols =
      static_cast<int>(frame_dep_stats->unit_stats[0].size());
  for (int unit_row = 0; unit_row < frame_unit_rows; ++unit_row) {
    for (int unit_col = 0; unit_col < frame_unit_cols; ++unit_col) {
      TplUnitDepStats &unit_dep_stats =
          frame_dep_stats->unit_stats[unit_row][unit_col];
      int ref_coding_idx_list[kBlockRefCount] = { -1, -1 };
      int ref_frame_count = GetRefCodingIdxList(unit_dep_stats, ref_frame_table,
                                                ref_coding_idx_list);
      if (ref_frame_count == 0) continue;
      for (int i = 0; i < kBlockRefCount; ++i) {
        if (ref_coding_idx_list[i] == -1) continue;
        assert(
            ref_coding_idx_list[i] <
            static_cast<int>(tpl_gop_dep_stats->frame_dep_stats_list.size()));
        TplFrameDepStats &ref_frame_dep_stats =
            tpl_gop_dep_stats->frame_dep_stats_list[ref_coding_idx_list[i]];
        assert(!ref_frame_dep_stats.unit_stats.empty());
        const auto &mv = unit_dep_stats.mv[i];
        const int mv_row = GetFullpelValue(mv.row, mv.subpel_bits);
        const int mv_col = GetFullpelValue(mv.col, mv.subpel_bits);
        const int ref_pixel_r = unit_row * unit_size + mv_row;
        const int ref_pixel_c = unit_col * unit_size + mv_col;
        const int ref_unit_row_low =
            (unit_row * unit_size + mv_row) / unit_size;
        const int ref_unit_col_low =
            (unit_col * unit_size + mv_col) / unit_size;

        for (int j = 0; j < 2; ++j) {
          for (int k = 0; k < 2; ++k) {
            const int ref_unit_row = ref_unit_row_low + j;
            const int ref_unit_col = ref_unit_col_low + k;
            if (ref_unit_row >= 0 && ref_unit_row < frame_unit_rows &&
                ref_unit_col >= 0 && ref_unit_col < frame_unit_cols) {
              const int overlap_area = GetBlockOverlapArea(
                  ref_pixel_r, ref_pixel_c, ref_unit_row * unit_size,
                  ref_unit_col * unit_size, unit_size);
              const double overlap_ratio =
                  overlap_area * 1.0 / (unit_size * unit_size);
              const double propagation_fraction =
                  GetPropagationFraction(unit_dep_stats);
              const double propagation_ratio =
                  1.0 / ref_frame_count * overlap_ratio * propagation_fraction;
              TplUnitDepStats &ref_unit_stats =
                  ref_frame_dep_stats.unit_stats[ref_unit_row][ref_unit_col];
              ref_unit_stats.propagation_cost +=
                  (unit_dep_stats.intra_cost +
                   unit_dep_stats.propagation_cost) *
                  propagation_ratio;
            }
          }
        }
      }
    }
  }
}

std::vector<RefFrameTable> AV1RateControlQMode::GetRefFrameTableList(
    const GopStruct &gop_struct, RefFrameTable ref_frame_table) {
  if (gop_struct.global_coding_idx_offset == 0) {
    // For the first GOP, ref_frame_table need not be initialized. This is fine,
    // because the first frame (a key frame) will fully initialize it.
    ref_frame_table.assign(rc_param_.ref_frame_table_size, GopFrameInvalid());
  } else {
    // It's not the first GOP, so ref_frame_table must be valid.
    assert(static_cast<int>(ref_frame_table.size()) ==
           rc_param_.ref_frame_table_size);
    assert(std::all_of(ref_frame_table.begin(), ref_frame_table.end(),
                       std::mem_fn(&GopFrame::is_valid)));
    // Reset the frame processing order of the initial ref_frame_table.
    for (GopFrame &gop_frame : ref_frame_table) gop_frame.coding_idx = -1;
  }

  std::vector<RefFrameTable> ref_frame_table_list;
  ref_frame_table_list.push_back(ref_frame_table);
  for (const GopFrame &gop_frame : gop_struct.gop_frame_list) {
    if (gop_frame.is_key_frame) {
      ref_frame_table.assign(rc_param_.ref_frame_table_size, gop_frame);
    } else if (gop_frame.update_ref_idx != -1) {
      assert(gop_frame.update_ref_idx <
             static_cast<int>(ref_frame_table.size()));
      ref_frame_table[gop_frame.update_ref_idx] = gop_frame;
    }
    ref_frame_table_list.push_back(ref_frame_table);
  }
  return ref_frame_table_list;
}

StatusOr<TplGopDepStats> ComputeTplGopDepStats(
    const TplGopStats &tpl_gop_stats,
    const std::vector<RefFrameTable> &ref_frame_table_list) {
  const int frame_count =
      static_cast<int>(tpl_gop_stats.frame_stats_list.size());
  // Create the struct to store TPL dependency stats
  TplGopDepStats tpl_gop_dep_stats;

  tpl_gop_dep_stats.frame_dep_stats_list.reserve(frame_count);
  for (int coding_idx = 0; coding_idx < frame_count; coding_idx++) {
    const StatusOr<TplFrameDepStats> tpl_frame_dep_stats =
        CreateTplFrameDepStatsWithoutPropagation(
            tpl_gop_stats.frame_stats_list[coding_idx]);
    if (!tpl_frame_dep_stats.ok()) {
      return tpl_frame_dep_stats.status();
    }
    tpl_gop_dep_stats.frame_dep_stats_list.push_back(
        std::move(*tpl_frame_dep_stats));
  }

  // Back propagation
  for (int coding_idx = frame_count - 1; coding_idx >= 0; coding_idx--) {
    auto &ref_frame_table = ref_frame_table_list[coding_idx];
    // TODO(angiebird): Handle/test the case where reference frame
    // is in the previous GOP
    TplFrameDepStatsPropagate(coding_idx, ref_frame_table, &tpl_gop_dep_stats);
  }
  return tpl_gop_dep_stats;
}

static int GetRDMult(const GopFrame &gop_frame, int qindex) {
  // TODO(angiebird):
  // 1) Check if these rdmult rules are good in our use case.
  // 2) Support high-bit-depth mode
  if (gop_frame.is_golden_frame) {
    // Assume ARF_UPDATE/GF_UPDATE share the same remult rule.
    return av1_compute_rd_mult_based_on_qindex(AOM_BITS_8, GF_UPDATE, qindex);
  } else if (gop_frame.is_key_frame) {
    return av1_compute_rd_mult_based_on_qindex(AOM_BITS_8, KF_UPDATE, qindex);
  } else {
    // Assume LF_UPDATE/OVERLAY_UPDATE/INTNL_OVERLAY_UPDATE/INTNL_ARF_UPDATE
    // share the same remult rule.
    return av1_compute_rd_mult_based_on_qindex(AOM_BITS_8, LF_UPDATE, qindex);
  }
}

StatusOr<GopEncodeInfo> AV1RateControlQMode::GetGopEncodeInfo(
    const GopStruct &gop_struct, const TplGopStats &tpl_gop_stats,
    const RefFrameTable &ref_frame_table_snapshot_init) {
  Status status = ValidateTplStats(gop_struct, tpl_gop_stats);
  if (!status.ok()) {
    return status;
  }

  const std::vector<RefFrameTable> ref_frame_table_list =
      GetRefFrameTableList(gop_struct, ref_frame_table_snapshot_init);

  GopEncodeInfo gop_encode_info;
  gop_encode_info.final_snapshot = ref_frame_table_list.back();
  StatusOr<TplGopDepStats> gop_dep_stats =
      ComputeTplGopDepStats(tpl_gop_stats, ref_frame_table_list);
  if (!gop_dep_stats.ok()) {
    return gop_dep_stats.status();
  }
  const int frame_count =
      static_cast<int>(tpl_gop_stats.frame_stats_list.size());
  for (int i = 0; i < frame_count; i++) {
    FrameEncodeParameters param;
    const GopFrame &gop_frame = gop_struct.gop_frame_list[i];

    if (gop_frame.update_type == GopFrameType::kOverlay ||
        gop_frame.update_type == GopFrameType::kIntermediateOverlay) {
      param.q_index = rc_param_.base_q_index;
    } else {
      const TplFrameDepStats &frame_dep_stats =
          gop_dep_stats->frame_dep_stats_list[i];
      const double cost_without_propagation =
          TplFrameDepStatsAccumulateIntraCost(frame_dep_stats);
      const double cost_with_propagation =
          TplFrameDepStatsAccumulate(frame_dep_stats);
      const double frame_importance =
          cost_with_propagation / cost_without_propagation;
      // Imitate the behavior of av1_tpl_get_qstep_ratio()
      const double qstep_ratio = sqrt(1 / frame_importance);
      param.q_index = av1_get_q_index_from_qstep_ratio(rc_param_.base_q_index,
                                                       qstep_ratio, AOM_BITS_8);
      if (rc_param_.base_q_index) param.q_index = AOMMAX(param.q_index, 1);
    }
    param.rdmult = GetRDMult(gop_frame, param.q_index);
    gop_encode_info.param_list.push_back(param);
  }
  return gop_encode_info;
}

}  // namespace aom
