/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_sps_parser.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "common_video/h265/h265_common.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/logging.h"

#define IN_RANGE_OR_RETURN_NULL(val, min, max)                                \
  do {                                                                        \
    if (!reader.Ok() || (val) < (min) || (val) > (max)) {                     \
      RTC_LOG(LS_WARNING) << "Error in stream: invalid value, expected " #val \
                             " to be"                                         \
                          << " in range [" << (min) << ":" << (max) << "]"    \
                          << " found " << (val) << " instead";                \
      return absl::nullopt;                                                   \
    }                                                                         \
  } while (0)

#define IN_RANGE_OR_RETURN_FALSE(val, min, max)                               \
  do {                                                                        \
    if (!reader.Ok() || (val) < (min) || (val) > (max)) {                     \
      RTC_LOG(LS_WARNING) << "Error in stream: invalid value, expected " #val \
                             " to be"                                         \
                          << " in range [" << (min) << ":" << (max) << "]"    \
                          << " found " << (val) << " instead";                \
      return false;                                                           \
    }                                                                         \
  } while (0)

#define TRUE_OR_RETURN(a)                                                \
  do {                                                                   \
    if (!reader.Ok() || !(a)) {                                          \
      RTC_LOG(LS_WARNING) << "Error in stream: invalid value, expected " \
                          << #a;                                         \
      return absl::nullopt;                                              \
    }                                                                    \
  } while (0)

namespace {
using OptionalSps = absl::optional<webrtc::H265SpsParser::SpsState>;
using OptionalShortTermRefPicSet =
    absl::optional<webrtc::H265SpsParser::ShortTermRefPicSet>;
using OptionalProfileTierLevel =
    absl::optional<webrtc::H265SpsParser::ProfileTierLevel>;

constexpr int kMaxNumSizeIds = 4;
constexpr int kMaxNumMatrixIds = 6;
constexpr int kMaxNumCoefs = 64;
}  // namespace

namespace webrtc {

#if WEBRTC_WEBKIT_BUILD
const uint32_t kMaxSPSLongTermRefPics = 32;
const uint32_t kMaxSPSPics = 16;
const uint32_t kMaxSPSShortTermRefPics = 64;
#endif

H265SpsParser::ShortTermRefPicSet::ShortTermRefPicSet() = default;

H265SpsParser::ProfileTierLevel::ProfileTierLevel() = default;

int H265SpsParser::GetMaxLumaPs(int general_level_idc) {
  // From Table A.8 - General tier and level limits.
  // |general_level_idc| is 30x the actual level.
  if (general_level_idc <= 30)  // level 1
    return 36864;
  if (general_level_idc <= 60)  // level 2
    return 122880;
  if (general_level_idc <= 63)  // level 2.1
    return 245760;
  if (general_level_idc <= 90)  // level 3
    return 552960;
  if (general_level_idc <= 93)  // level 3.1
    return 983040;
  if (general_level_idc <= 123)  // level 4, 4.1
    return 2228224;
  if (general_level_idc <= 156)  // level 5, 5.1, 5.2
    return 8912896;
  // level 6, 6.1, 6.2 - beyond that there's no actual limit.
  return 35651584;
}

size_t H265SpsParser::GetDpbMaxPicBuf(int general_profile_idc) {
  // From A.4.2 - Profile-specific level limits for the video profiles.
  // If sps_curr_pic_ref_enabled_flag is required to be zero, than this is 6
  // otherwise it is 7.
  return (general_profile_idc >= kProfileIdcMain &&
          general_profile_idc <= kProfileIdcHighThroughput)
             ? 6
             : 7;
}

// General note: this is based off the 08/2021 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

// Unpack RBSP and parse SPS state from the supplied buffer.
absl::optional<H265SpsParser::SpsState> H265SpsParser::ParseSps(
    const uint8_t* data,
    size_t length) {
  RTC_DCHECK(data);
  return ParseSpsInternal(H265::ParseRbsp(data, length));
}

bool H265SpsParser::ParseScalingListData(BitstreamReader& reader) {
  int32_t scaling_list_dc_coef_minus8[kMaxNumSizeIds][kMaxNumMatrixIds] = {};
  for (int size_id = 0; size_id < kMaxNumSizeIds; size_id++) {
    for (int matrix_id = 0; matrix_id < kMaxNumMatrixIds;
         matrix_id += (size_id == 3) ? 3 : 1) {
      // scaling_list_pred_mode_flag: u(1)
      bool scaling_list_pred_mode_flag = reader.Read<bool>();
      if (!scaling_list_pred_mode_flag) {
        // scaling_list_pred_matrix_id_delta: ue(v)
        int scaling_list_pred_matrix_id_delta = reader.ReadExponentialGolomb();
        if (size_id <= 2) {
          IN_RANGE_OR_RETURN_FALSE(scaling_list_pred_matrix_id_delta, 0,
                                   matrix_id);
        } else {  // size_id == 3
          IN_RANGE_OR_RETURN_FALSE(scaling_list_pred_matrix_id_delta, 0,
                                   matrix_id / 3);
        }
      } else {
        uint32_t coef_num = std::min(kMaxNumCoefs, 1 << (4 + (size_id << 1)));
        if (size_id > 1) {
          // scaling_list_dc_coef_minus8: se(v)
          scaling_list_dc_coef_minus8[size_id - 2][matrix_id] =
              reader.ReadSignedExponentialGolomb();
          IN_RANGE_OR_RETURN_FALSE(
              scaling_list_dc_coef_minus8[size_id - 2][matrix_id], -7, 247);
        }
        for (uint32_t i = 0; i < coef_num; i++) {
          // scaling_list_delta_coef: se(v)
          int32_t scaling_list_delta_coef =
              reader.ReadSignedExponentialGolomb();
          IN_RANGE_OR_RETURN_FALSE(scaling_list_delta_coef, -128, 127);
        }
      }
    }
  }
  return reader.Ok();
}

absl::optional<H265SpsParser::ShortTermRefPicSet>
H265SpsParser::ParseShortTermRefPicSet(
    uint32_t st_rps_idx,
    uint32_t num_short_term_ref_pic_sets,
    const std::vector<H265SpsParser::ShortTermRefPicSet>&
        short_term_ref_pic_set,
    uint32_t sps_max_dec_pic_buffering_minus1,
    BitstreamReader& reader) {
  H265SpsParser::ShortTermRefPicSet st_ref_pic_set;

  bool inter_ref_pic_set_prediction_flag = false;
  if (st_rps_idx != 0) {
    // inter_ref_pic_set_prediction_flag: u(1)
    inter_ref_pic_set_prediction_flag = reader.Read<bool>();
  }

  if (inter_ref_pic_set_prediction_flag) {
    uint32_t delta_idx_minus1 = 0;
    if (st_rps_idx == num_short_term_ref_pic_sets) {
      // delta_idx_minus1: ue(v)
      delta_idx_minus1 = reader.ReadExponentialGolomb();
      IN_RANGE_OR_RETURN_NULL(delta_idx_minus1, 0, st_rps_idx - 1);
    }
    // delta_rps_sign: u(1)
    int delta_rps_sign = reader.ReadBits(1);
    // abs_delta_rps_minus1: ue(v)
    int abs_delta_rps_minus1 = reader.ReadExponentialGolomb();
    IN_RANGE_OR_RETURN_NULL(abs_delta_rps_minus1, 0, 0x7FFF);
    int delta_rps = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);
    uint32_t ref_rps_idx = st_rps_idx - (delta_idx_minus1 + 1);
    uint32_t num_delta_pocs =
        short_term_ref_pic_set[ref_rps_idx].num_delta_pocs;
    IN_RANGE_OR_RETURN_NULL(num_delta_pocs, 0, kMaxShortTermRefPicSets);
    const ShortTermRefPicSet& ref_set = short_term_ref_pic_set[ref_rps_idx];
    bool used_by_curr_pic_flag[kMaxShortTermRefPicSets] = {};
    bool use_delta_flag[kMaxShortTermRefPicSets] = {};
    // 7.4.8 - use_delta_flag defaults to 1 if not present.
    std::fill_n(use_delta_flag, kMaxShortTermRefPicSets, true);

    for (uint32_t j = 0; j <= num_delta_pocs; j++) {
      // used_by_curr_pic_flag: u(1)
      used_by_curr_pic_flag[j] = reader.Read<bool>();
      if (!used_by_curr_pic_flag[j]) {
        // use_delta_flag: u(1)
        use_delta_flag[j] = reader.Read<bool>();
      }
    }

    // Calculate delta_poc_s{0,1}, used_by_curr_pic_s{0,1}, num_negative_pics
    // and num_positive_pics.
    // Equation 7-61
    int i = 0;
    IN_RANGE_OR_RETURN_NULL(
        ref_set.num_negative_pics + ref_set.num_positive_pics, 0,
        kMaxShortTermRefPicSets);
    for (int j = ref_set.num_positive_pics - 1; j >= 0; --j) {
      int d_poc = ref_set.delta_poc_s1[j] + delta_rps;
      if (d_poc < 0 && use_delta_flag[ref_set.num_negative_pics + j]) {
        st_ref_pic_set.delta_poc_s0[i] = d_poc;
        st_ref_pic_set.used_by_curr_pic_s0[i++] =
            used_by_curr_pic_flag[ref_set.num_negative_pics + j];
      }
    }
    if (delta_rps < 0 && use_delta_flag[ref_set.num_delta_pocs]) {
      st_ref_pic_set.delta_poc_s0[i] = delta_rps;
      st_ref_pic_set.used_by_curr_pic_s0[i++] =
          used_by_curr_pic_flag[ref_set.num_delta_pocs];
    }
    for (uint32_t j = 0; j < ref_set.num_negative_pics; ++j) {
      int d_poc = ref_set.delta_poc_s0[j] + delta_rps;
      if (d_poc < 0 && use_delta_flag[j]) {
        st_ref_pic_set.delta_poc_s0[i] = d_poc;
        st_ref_pic_set.used_by_curr_pic_s0[i++] = used_by_curr_pic_flag[j];
      }
    }
    st_ref_pic_set.num_negative_pics = i;
    // Equation 7-62
    i = 0;
    for (int j = ref_set.num_negative_pics - 1; j >= 0; --j) {
      int d_poc = ref_set.delta_poc_s0[j] + delta_rps;
      if (d_poc > 0 && use_delta_flag[j]) {
        st_ref_pic_set.delta_poc_s1[i] = d_poc;
        st_ref_pic_set.used_by_curr_pic_s1[i++] = used_by_curr_pic_flag[j];
      }
    }
    if (delta_rps > 0 && use_delta_flag[ref_set.num_delta_pocs]) {
      st_ref_pic_set.delta_poc_s1[i] = delta_rps;
      st_ref_pic_set.used_by_curr_pic_s1[i++] =
          used_by_curr_pic_flag[ref_set.num_delta_pocs];
    }
    for (uint32_t j = 0; j < ref_set.num_positive_pics; ++j) {
      int d_poc = ref_set.delta_poc_s1[j] + delta_rps;
      if (d_poc > 0 && use_delta_flag[ref_set.num_negative_pics + j]) {
        st_ref_pic_set.delta_poc_s1[i] = d_poc;
        st_ref_pic_set.used_by_curr_pic_s1[i++] =
            used_by_curr_pic_flag[ref_set.num_negative_pics + j];
      }
    }
    st_ref_pic_set.num_positive_pics = i;
    IN_RANGE_OR_RETURN_NULL(st_ref_pic_set.num_negative_pics, 0,
                            sps_max_dec_pic_buffering_minus1);
    IN_RANGE_OR_RETURN_NULL(
        st_ref_pic_set.num_positive_pics, 0,
        sps_max_dec_pic_buffering_minus1 - st_ref_pic_set.num_negative_pics);

  } else {
    // num_negative_pics: ue(v)
    st_ref_pic_set.num_negative_pics = reader.ReadExponentialGolomb();
    // num_positive_pics: ue(v)
    st_ref_pic_set.num_positive_pics = reader.ReadExponentialGolomb();
#if WEBRTC_WEBKIT_BUILD
    if (!reader.Ok() || st_ref_pic_set.num_negative_pics > kMaxSPSPics || st_ref_pic_set.num_positive_pics > kMaxSPSPics
        || (st_ref_pic_set.num_negative_pics + st_ref_pic_set.num_positive_pics) > kMaxSPSPics) {
      return absl::nullopt;
    }
#endif
    IN_RANGE_OR_RETURN_NULL(st_ref_pic_set.num_negative_pics, 0,
                            sps_max_dec_pic_buffering_minus1);
    IN_RANGE_OR_RETURN_NULL(
        st_ref_pic_set.num_positive_pics, 0,
        sps_max_dec_pic_buffering_minus1 - st_ref_pic_set.num_negative_pics);

    for (uint32_t i = 0; i < st_ref_pic_set.num_negative_pics; i++) {
      // delta_poc_s0_minus1: ue(v)
      int delta_poc_s0_minus1 = 0;
      delta_poc_s0_minus1 = reader.ReadExponentialGolomb();
      IN_RANGE_OR_RETURN_NULL(delta_poc_s0_minus1, 0, 0x7FFF);
      if (i == 0) {
        st_ref_pic_set.delta_poc_s0[i] = -(delta_poc_s0_minus1 + 1);
      } else {
        st_ref_pic_set.delta_poc_s0[i] =
            st_ref_pic_set.delta_poc_s0[i - 1] - (delta_poc_s0_minus1 + 1);
      }
      // used_by_curr_pic_s0_flag: u(1)
      st_ref_pic_set.used_by_curr_pic_s0[i] = reader.Read<bool>();
    }

    for (uint32_t i = 0; i < st_ref_pic_set.num_positive_pics; i++) {
      // delta_poc_s1_minus1: ue(v)
      int delta_poc_s1_minus1 = 0;
      delta_poc_s1_minus1 = reader.ReadExponentialGolomb();
      IN_RANGE_OR_RETURN_NULL(delta_poc_s1_minus1, 0, 0x7FFF);
      if (i == 0) {
        st_ref_pic_set.delta_poc_s1[i] = delta_poc_s1_minus1 + 1;
      } else {
        st_ref_pic_set.delta_poc_s1[i] =
            st_ref_pic_set.delta_poc_s1[i - 1] + delta_poc_s1_minus1 + 1;
      }
      // used_by_curr_pic_s1_flag: u(1)
      st_ref_pic_set.used_by_curr_pic_s1[i] = reader.Read<bool>();
    }
  }

  st_ref_pic_set.num_delta_pocs =
      st_ref_pic_set.num_negative_pics + st_ref_pic_set.num_positive_pics;

  if (!reader.Ok()) {
    return absl::nullopt;
  }

  return OptionalShortTermRefPicSet(st_ref_pic_set);
}

absl::optional<H265SpsParser::ProfileTierLevel>
H265SpsParser::ParseProfileTierLevel(bool profile_present,
                                     int max_num_sub_layers_minus1,
                                     BitstreamReader& reader) {
  H265SpsParser::ProfileTierLevel pf_tier_level;
  // 7.4.4
  if (profile_present) {
    int general_profile_space;
    general_profile_space = reader.ReadBits(2);
    TRUE_OR_RETURN(general_profile_space == 0);
    // general_tier_flag or reserved 0: u(1)
    reader.ConsumeBits(1);
    pf_tier_level.general_profile_idc = reader.ReadBits(5);
    IN_RANGE_OR_RETURN_NULL(pf_tier_level.general_profile_idc, 0, 11);
    uint16_t general_profile_compatibility_flag_high16 = reader.ReadBits(16);
    uint16_t general_profile_compatibility_flag_low16 = reader.ReadBits(16);
    pf_tier_level.general_profile_compatibility_flags =
        (general_profile_compatibility_flag_high16 << 16) +
        general_profile_compatibility_flag_low16;
    pf_tier_level.general_progressive_source_flag = reader.ReadBits(1);
    pf_tier_level.general_interlaced_source_flag = reader.ReadBits(1);
    if (!reader.Ok() || (!pf_tier_level.general_progressive_source_flag &&
                         pf_tier_level.general_interlaced_source_flag)) {
      RTC_LOG(LS_WARNING) << "Interlaced streams not supported";
      return absl::nullopt;
    }
    pf_tier_level.general_non_packed_constraint_flag = reader.ReadBits(1);
    pf_tier_level.general_frame_only_constraint_flag = reader.ReadBits(1);
    // general_reserved_zero_7bits
    reader.ConsumeBits(7);
    pf_tier_level.general_one_picture_only_constraint_flag = reader.ReadBits(1);
    // general_reserved_zero_35bits
    reader.ConsumeBits(35);
    // general_inbld_flag
    reader.ConsumeBits(1);
  }
  pf_tier_level.general_level_idc = reader.ReadBits(8);
  bool sub_layer_profile_present_flag[8] = {};
  bool sub_layer_level_present_flag[8] = {};
  for (int i = 0; i < max_num_sub_layers_minus1; ++i) {
    sub_layer_profile_present_flag[i] = reader.ReadBits(1);
    sub_layer_level_present_flag[i] = reader.ReadBits(1);
  }
  if (max_num_sub_layers_minus1 > 0) {
    for (int i = max_num_sub_layers_minus1; i < 8; i++) {
      reader.ConsumeBits(2);
    }
  }
  for (int i = 0; i < max_num_sub_layers_minus1; i++) {
    if (sub_layer_profile_present_flag[i]) {
      // sub_layer_profile_space
      reader.ConsumeBits(2);
      // sub_layer_tier_flag
      reader.ConsumeBits(1);
      // sub_layer_profile_idc
      reader.ConsumeBits(5);
      // sub_layer_profile_compatibility_flag
      reader.ConsumeBits(32);
      // sub_layer_{progressive,interlaced}_source_flag
      reader.ConsumeBits(2);
      // Ignore sub_layer_non_packed_constraint_flag and
      // sub_layer_frame_only_constraint_flag.
      reader.ConsumeBits(2);
      // Skip the compatibility flags, they are always 43 bits.
      reader.ConsumeBits(43);
      // sub_layer_inbld_flag
      reader.ConsumeBits(1);
    }
    if (sub_layer_level_present_flag[i]) {
      // sub_layer_level_idc
      reader.ConsumeBits(8);
    }
  }

  if (!reader.Ok()) {
    return absl::nullopt;
  }

  return OptionalProfileTierLevel(pf_tier_level);
}

absl::optional<H265SpsParser::SpsState> H265SpsParser::ParseSpsInternal(
    rtc::ArrayView<const uint8_t> buffer) {
  BitstreamReader reader(buffer);

  // Now, we need to use a bit buffer to parse through the actual H265 SPS
  // format. See Section 7.3.2.2.1 ("General sequence parameter set data
  // syntax") of the H.265 standard for a complete description.
  // Since we only care about resolution, we ignore the majority of fields, but
  // we still have to actively parse through a lot of the data, since many of
  // the fields have variable size.
  // We're particularly interested in:
  // chroma_format_idc -> affects crop units
  // pic_{width,height}_* -> resolution of the frame in macroblocks (16x16).
  // frame_crop_*_offset -> crop information
  SpsState sps;

  // sps_video_parameter_set_id: u(4)
  uint32_t sps_video_parameter_set_id = 0;
  sps_video_parameter_set_id = reader.ReadBits(4);
  IN_RANGE_OR_RETURN_NULL(sps_video_parameter_set_id, 0, 15);

  // sps_max_sub_layers_minus1: u(3)
  uint32_t sps_max_sub_layers_minus1 = 0;
  sps_max_sub_layers_minus1 = reader.ReadBits(3);
  IN_RANGE_OR_RETURN_NULL(sps_max_sub_layers_minus1, 0, kMaxSubLayers - 1);
  sps.sps_max_sub_layers_minus1 = sps_max_sub_layers_minus1;
  // sps_temporal_id_nesting_flag: u(1)
  reader.ConsumeBits(1);
  // profile_tier_level(1, sps_max_sub_layers_minus1).
  OptionalProfileTierLevel profile_tier_level =
      ParseProfileTierLevel(true, sps.sps_max_sub_layers_minus1, reader);
  if (!profile_tier_level) {
    return absl::nullopt;
  }
  // sps_seq_parameter_set_id: ue(v)
  sps.sps_id = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(sps.sps_id, 0, 15);
  // chrome_format_idc: ue(v)
  sps.chroma_format_idc = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(sps.chroma_format_idc, 0, 3);
  if (sps.chroma_format_idc == 3) {
    // seperate_colour_plane_flag: u(1)
    sps.separate_colour_plane_flag = reader.Read<bool>();
  }
  uint32_t pic_width_in_luma_samples = 0;
  uint32_t pic_height_in_luma_samples = 0;
  // pic_width_in_luma_samples: ue(v)
  pic_width_in_luma_samples = reader.ReadExponentialGolomb();
  TRUE_OR_RETURN(pic_width_in_luma_samples != 0);
  // pic_height_in_luma_samples: ue(v)
  pic_height_in_luma_samples = reader.ReadExponentialGolomb();
  TRUE_OR_RETURN(pic_height_in_luma_samples != 0);

  // Equation A-2: Calculate max_dpb_size.
  uint32_t max_luma_ps = GetMaxLumaPs(profile_tier_level->general_level_idc);
  uint32_t max_dpb_size = 0;
  uint32_t pic_size_in_samples_y = pic_height_in_luma_samples;
  pic_size_in_samples_y *= pic_width_in_luma_samples;
  size_t max_dpb_pic_buf =
      GetDpbMaxPicBuf(profile_tier_level->general_profile_idc);
  if (pic_size_in_samples_y <= (max_luma_ps >> 2))
    max_dpb_size = std::min(4 * max_dpb_pic_buf, size_t{16});
  else if (pic_size_in_samples_y <= (max_luma_ps >> 1))
    max_dpb_size = std::min(2 * max_dpb_pic_buf, size_t{16});
  else if (pic_size_in_samples_y <= ((3 * max_luma_ps) >> 2))
    max_dpb_size = std::min((4 * max_dpb_pic_buf) / 3, size_t{16});
  else
    max_dpb_size = max_dpb_pic_buf;

  // conformance_window_flag: u(1)
  bool conformance_window_flag = reader.Read<bool>();

  uint32_t conf_win_left_offset = 0;
  uint32_t conf_win_right_offset = 0;
  uint32_t conf_win_top_offset = 0;
  uint32_t conf_win_bottom_offset = 0;
  int sub_width_c =
      ((1 == sps.chroma_format_idc) || (2 == sps.chroma_format_idc)) &&
              (0 == sps.separate_colour_plane_flag)
          ? 2
          : 1;
  int sub_height_c =
      (1 == sps.chroma_format_idc) && (0 == sps.separate_colour_plane_flag) ? 2
                                                                            : 1;
  if (conformance_window_flag) {
    // conf_win_left_offset: ue(v)
    conf_win_left_offset = reader.ReadExponentialGolomb();
    // conf_win_right_offset: ue(v)
    conf_win_right_offset = reader.ReadExponentialGolomb();
    // conf_win_top_offset: ue(v)
    conf_win_top_offset = reader.ReadExponentialGolomb();
    // conf_win_bottom_offset: ue(v)
    conf_win_bottom_offset = reader.ReadExponentialGolomb();
    uint32_t width_crop = conf_win_left_offset;
    width_crop += conf_win_right_offset;
    width_crop *= sub_width_c;
    TRUE_OR_RETURN(width_crop < pic_width_in_luma_samples);
    uint32_t height_crop = conf_win_top_offset;
    height_crop += conf_win_bottom_offset;
    height_crop *= sub_height_c;
    TRUE_OR_RETURN(height_crop < pic_height_in_luma_samples);
  }

#if WEBRTC_WEBKIT_BUILD
  // log2_max_pic_order_cnt_lsb_minus4 is used with
  // BitstreamReader::ConsumeBits, which can read at most INT_MAX bits at
  // a time. We also have to avoid overflow when adding 4 to the on-wire
  // golomb value, e.g., for evil input data.
  const uint32_t kMaxLog2LsbMinus4 = std::numeric_limits<int>::max() - 4;
#endif

  // bit_depth_luma_minus8: ue(v)
  sps.bit_depth_luma_minus8 = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(sps.bit_depth_luma_minus8, 0, 8);
  // bit_depth_chroma_minus8: ue(v)
  uint32_t bit_depth_chroma_minus8 = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(bit_depth_chroma_minus8, 0, 8);
  // log2_max_pic_order_cnt_lsb_minus4: ue(v)
  sps.log2_max_pic_order_cnt_lsb_minus4 = reader.ReadExponentialGolomb();
#if WEBRTC_WEBKIT_BUILD
  if (!reader.Ok() || sps.log2_max_pic_order_cnt_lsb_minus4 > kMaxLog2LsbMinus4) {
    return absl::nullopt;
  }
#endif
  IN_RANGE_OR_RETURN_NULL(sps.log2_max_pic_order_cnt_lsb_minus4, 0, 12);
  uint32_t sps_sub_layer_ordering_info_present_flag = 0;
  // sps_sub_layer_ordering_info_present_flag: u(1)
  sps_sub_layer_ordering_info_present_flag = reader.Read<bool>();
  uint32_t sps_max_num_reorder_pics[kMaxSubLayers] = {};
  for (uint32_t i = (sps_sub_layer_ordering_info_present_flag != 0)
                        ? 0
                        : sps_max_sub_layers_minus1;
       i <= sps_max_sub_layers_minus1; i++) {
    // sps_max_dec_pic_buffering_minus1: ue(v)
    sps.sps_max_dec_pic_buffering_minus1[i] = reader.ReadExponentialGolomb();
    IN_RANGE_OR_RETURN_NULL(sps.sps_max_dec_pic_buffering_minus1[i], 0,
                            max_dpb_size - 1);
    // sps_max_num_reorder_pics: ue(v)
    sps_max_num_reorder_pics[i] = reader.ReadExponentialGolomb();
    IN_RANGE_OR_RETURN_NULL(sps_max_num_reorder_pics[i], 0,
                            sps.sps_max_dec_pic_buffering_minus1[i]);
    if (i > 0) {
      TRUE_OR_RETURN(sps.sps_max_dec_pic_buffering_minus1[i] >=
                     sps.sps_max_dec_pic_buffering_minus1[i - 1]);
      TRUE_OR_RETURN(sps_max_num_reorder_pics[i] >=
                     sps_max_num_reorder_pics[i - 1]);
    }
    // sps_max_latency_increase_plus1: ue(v)
    reader.ReadExponentialGolomb();
  }
  if (!sps_sub_layer_ordering_info_present_flag) {
    // Fill in the default values for the other sublayers.
    for (uint32_t i = 0; i < sps_max_sub_layers_minus1; ++i) {
      sps.sps_max_dec_pic_buffering_minus1[i] =
          sps.sps_max_dec_pic_buffering_minus1[sps_max_sub_layers_minus1];
    }
  }
  // log2_min_luma_coding_block_size_minus3: ue(v)
  sps.log2_min_luma_coding_block_size_minus3 = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(sps.log2_min_luma_coding_block_size_minus3, 0, 27);
  // log2_diff_max_min_luma_coding_block_size: ue(v)
  sps.log2_diff_max_min_luma_coding_block_size = reader.ReadExponentialGolomb();
  int min_cb_log2_size_y = sps.log2_min_luma_coding_block_size_minus3 + 3;
  int ctb_log2_size_y = min_cb_log2_size_y;
  ctb_log2_size_y += sps.log2_diff_max_min_luma_coding_block_size;
  IN_RANGE_OR_RETURN_NULL(ctb_log2_size_y, 0, 30);
  int min_cb_size_y = 1 << min_cb_log2_size_y;
  int ctb_size_y = 1 << ctb_log2_size_y;
  sps.pic_width_in_ctbs_y =
      std::ceil(static_cast<float>(pic_width_in_luma_samples) / ctb_size_y);
  sps.pic_height_in_ctbs_y =
      std::ceil(static_cast<float>(pic_height_in_luma_samples) / ctb_size_y);
  TRUE_OR_RETURN(pic_width_in_luma_samples % min_cb_size_y == 0);
  TRUE_OR_RETURN(pic_height_in_luma_samples % min_cb_size_y == 0);
  // log2_min_luma_transform_block_size_minus2: ue(v)
  int log2_min_luma_transform_block_size_minus2 =
      reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(log2_min_luma_transform_block_size_minus2, 0,
                          min_cb_log2_size_y - 3);
  int min_tb_log2_size_y = log2_min_luma_transform_block_size_minus2 + 2;
  // log2_diff_max_min_luma_transform_block_size: ue(v)
  int log2_diff_max_min_luma_transform_block_size =
      reader.ReadExponentialGolomb();
  TRUE_OR_RETURN(log2_diff_max_min_luma_transform_block_size <=
                 std::min(ctb_log2_size_y, 5) - min_tb_log2_size_y);
  // max_transform_hierarchy_depth_inter: ue(v)
  int max_transform_hierarchy_depth_inter = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(max_transform_hierarchy_depth_inter, 0,
                          ctb_log2_size_y - min_tb_log2_size_y);
  // max_transform_hierarchy_depth_intra: ue(v)
  int max_transform_hierarchy_depth_intra = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(max_transform_hierarchy_depth_intra, 0,
                          ctb_log2_size_y - min_tb_log2_size_y);
  // scaling_list_enabled_flag: u(1)
  bool scaling_list_enabled_flag = reader.Read<bool>();
  if (scaling_list_enabled_flag) {
    // sps_scaling_list_data_present_flag: u(1)
    bool sps_scaling_list_data_present_flag = reader.Read<bool>();
    if (sps_scaling_list_data_present_flag) {
      // scaling_list_data()
      if (!ParseScalingListData(reader)) {
        return absl::nullopt;
      }
    }
  }

  // amp_enabled_flag: u(1)
  reader.ConsumeBits(1);
  // sample_adaptive_offset_enabled_flag: u(1)
  sps.sample_adaptive_offset_enabled_flag = reader.Read<bool>();
  // pcm_enabled_flag: u(1)
  bool pcm_enabled_flag = reader.Read<bool>();
  if (pcm_enabled_flag) {
    // pcm_sample_bit_depth_luma_minus1: u(4)
    reader.ConsumeBits(4);
    // pcm_sample_bit_depth_chroma_minus1: u(4)
    reader.ConsumeBits(4);
    // log2_min_pcm_luma_coding_block_size_minus3: ue(v)
    int log2_min_pcm_luma_coding_block_size_minus3 =
        reader.ReadExponentialGolomb();
    IN_RANGE_OR_RETURN_NULL(log2_min_pcm_luma_coding_block_size_minus3, 0, 2);
    int log2_min_ipcm_cb_size_y =
        log2_min_pcm_luma_coding_block_size_minus3 + 3;
    IN_RANGE_OR_RETURN_NULL(log2_min_ipcm_cb_size_y,
                            std::min(min_cb_log2_size_y, 5),
                            std::min(ctb_log2_size_y, 5));
    // log2_diff_max_min_pcm_luma_coding_block_size: ue(v)
    int log2_diff_max_min_pcm_luma_coding_block_size =
        reader.ReadExponentialGolomb();
    TRUE_OR_RETURN(log2_diff_max_min_pcm_luma_coding_block_size <=
                   std::min(ctb_log2_size_y, 5) - log2_min_ipcm_cb_size_y);
    // pcm_loop_filter_disabled_flag: u(1)
    reader.ConsumeBits(1);
  }

  // num_short_term_ref_pic_sets: ue(v)
  sps.num_short_term_ref_pic_sets = reader.ReadExponentialGolomb();
#if WEBRTC_WEBKIT_BUILD
    if (!reader.Ok() || sps.num_short_term_ref_pic_sets > kMaxSPSShortTermRefPics) {
      return absl::nullopt;
    }
#endif
  IN_RANGE_OR_RETURN_NULL(sps.num_short_term_ref_pic_sets, 0,
                          kMaxShortTermRefPicSets);
  sps.short_term_ref_pic_set.resize(sps.num_short_term_ref_pic_sets);
  for (uint32_t st_rps_idx = 0; st_rps_idx < sps.num_short_term_ref_pic_sets;
       st_rps_idx++) {
    uint32_t sps_max_dec_pic_buffering_minus1 =
        sps.sps_max_dec_pic_buffering_minus1[sps.sps_max_sub_layers_minus1];
    // st_ref_pic_set()
    OptionalShortTermRefPicSet ref_pic_set = ParseShortTermRefPicSet(
        st_rps_idx, sps.num_short_term_ref_pic_sets, sps.short_term_ref_pic_set,
        sps_max_dec_pic_buffering_minus1, reader);
    if (ref_pic_set) {
      sps.short_term_ref_pic_set[st_rps_idx] = *ref_pic_set;
    } else {
      return absl::nullopt;
    }
  }

  // long_term_ref_pics_present_flag: u(1)
  sps.long_term_ref_pics_present_flag = reader.Read<bool>();
  if (sps.long_term_ref_pics_present_flag) {
    // num_long_term_ref_pics_sps: ue(v)
    sps.num_long_term_ref_pics_sps = reader.ReadExponentialGolomb();
#if WEBRTC_WEBKIT_BUILD
    if (!reader.Ok() || sps.num_long_term_ref_pics_sps > kMaxSPSLongTermRefPics) {
      return absl::nullopt;
    }
#endif
    IN_RANGE_OR_RETURN_NULL(sps.num_long_term_ref_pics_sps, 0,
                            kMaxLongTermRefPicSets);
    sps.used_by_curr_pic_lt_sps_flag.resize(sps.num_long_term_ref_pics_sps, 0);
    for (uint32_t i = 0; i < sps.num_long_term_ref_pics_sps; i++) {
      // lt_ref_pic_poc_lsb_sps: u(v)
      uint32_t lt_ref_pic_poc_lsb_sps_bits =
          sps.log2_max_pic_order_cnt_lsb_minus4 + 4;
      reader.ConsumeBits(lt_ref_pic_poc_lsb_sps_bits);
      // used_by_curr_pic_lt_sps_flag: u(1)
      sps.used_by_curr_pic_lt_sps_flag[i] = reader.Read<bool>();
    }
  }

  // sps_temporal_mvp_enabled_flag: u(1)
  sps.sps_temporal_mvp_enabled_flag = reader.Read<bool>();

  // Far enough! We don't use the rest of the SPS.

  sps.vps_id = sps_video_parameter_set_id;

  sps.pic_width_in_luma_samples = pic_width_in_luma_samples;
  sps.pic_height_in_luma_samples = pic_height_in_luma_samples;

  // Start with the resolution determined by the pic_width/pic_height fields.
  sps.width = pic_width_in_luma_samples;
  sps.height = pic_height_in_luma_samples;

  if (conformance_window_flag) {
    int sub_width_c =
        ((1 == sps.chroma_format_idc) || (2 == sps.chroma_format_idc)) &&
                (0 == sps.separate_colour_plane_flag)
            ? 2
            : 1;
    int sub_height_c =
        (1 == sps.chroma_format_idc) && (0 == sps.separate_colour_plane_flag)
            ? 2
            : 1;
    // the offset includes the pixel within conformance window. so don't need to
    // +1 as per spec
    sps.width -= sub_width_c * (conf_win_right_offset + conf_win_left_offset);
    sps.height -= sub_height_c * (conf_win_top_offset + conf_win_bottom_offset);
  }

  if (!reader.Ok()) {
    return absl::nullopt;
  }

  return OptionalSps(sps);
}

}  // namespace webrtc
