/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_sps_parser.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/logging.h"

namespace {
typedef absl::optional<webrtc::H265SpsParser::SpsState> OptionalSps;

#define RETURN_EMPTY_ON_FAIL(x) \
  if (!(x)) {                   \
    return OptionalSps();       \
  }
}  // namespace

namespace webrtc {

H265SpsParser::SpsState::SpsState() = default;

// General note: this is based off the 02/2018 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

// Unpack RBSP and parse SPS state from the supplied buffer.
absl::optional<H265SpsParser::SpsState> H265SpsParser::ParseSps(
    const uint8_t* data,
    size_t length) {
  std::vector<uint8_t> unpacked_buffer = H265::ParseRbsp(data, length);
  rtc::BitBuffer bit_buffer(unpacked_buffer.data(), unpacked_buffer.size());
  return ParseSpsUpToVui(&bit_buffer);
}

absl::optional<H265SpsParser::SpsState> H265SpsParser::ParseSpsUpToVui(
    rtc::BitBuffer* buffer) {
  // Now, we need to use a bit buffer to parse through the actual HEVC SPS
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

  // The golomb values we have to read, not just consume.
  uint32_t golomb_ignored;

  // separate_colour_plane_flag is optional (assumed 0), but has implications
  // about the ChromaArrayType, which modifies how we treat crop coordinates.
  uint32_t separate_colour_plane_flag = 0;

  // chroma_format_idc will be ChromaArrayType if separate_colour_plane_flag is
  // 0. It defaults to 1, when not specified.
  uint32_t chroma_format_idc = 1;

  // sps_video_parameter_set_id: u(4)
  uint32_t sps_video_parameter_set_id = 0;
  RETURN_EMPTY_ON_FAIL(buffer->ReadBits(&sps_video_parameter_set_id, 4));
  // sps_max_sub_layers_minus1: u(3)
  uint32_t sps_max_sub_layers_minus1 = 0;
  RETURN_EMPTY_ON_FAIL(buffer->ReadBits(&sps_max_sub_layers_minus1, 3));
  // sps_temporal_id_nesting_flag: u(1)
  RETURN_EMPTY_ON_FAIL(buffer->ConsumeBits(1));
  // profile_tier_level(1, sps_max_sub_layers_minus1). We are acutally not
  // using them, so read/skip over it.
  // general_profile_space+general_tier_flag+general_prfile_idc: u(8)
  RETURN_EMPTY_ON_FAIL(buffer->ConsumeBytes(1));
  // general_profile_compatabilitiy_flag[32]
  RETURN_EMPTY_ON_FAIL(buffer->ConsumeBytes(4));
  // general_progressive_source_flag + interlaced_source_flag+
  // non-packed_constraint flag + frame_only_constraint_flag: u(4)
  RETURN_EMPTY_ON_FAIL(buffer->ConsumeBits(4));
  // general_profile_idc decided flags or reserved.  u(43)
  RETURN_EMPTY_ON_FAIL(buffer->ConsumeBits(43));
  // general_inbld_flag or reserved 0: u(1)
  RETURN_EMPTY_ON_FAIL(buffer->ConsumeBits(1));
  // general_level_idc: u(8)
  RETURN_EMPTY_ON_FAIL(buffer->ConsumeBytes(1));
  // if max_sub_layers_minus1 >=1, read the sublayer profile information
  std::vector<uint32_t> sub_layer_profile_present_flags;
  std::vector<uint32_t> sub_layer_level_present_flags;
  uint32_t sub_layer_profile_present = 0;
  uint32_t sub_layer_level_present = 0;
  for (uint32_t i = 0; i < sps_max_sub_layers_minus1; i++) {
    // sublayer_profile_present_flag and sublayer_level_presnet_flag:  u(2)
    RETURN_EMPTY_ON_FAIL(buffer->ReadBits(&sub_layer_profile_present, 1));
    RETURN_EMPTY_ON_FAIL(buffer->ReadBits(&sub_layer_level_present, 1));
    sub_layer_profile_present_flags.push_back(sub_layer_profile_present);
    sub_layer_level_present_flags.push_back(sub_layer_level_present);
  }
  if (sps_max_sub_layers_minus1 > 0) {
    for (uint32_t j = sps_max_sub_layers_minus1; j < 8; j++) {
      // reserved 2 bits: u(2)
      RETURN_EMPTY_ON_FAIL(buffer->ConsumeBits(2));
    }
  }
  for (uint32_t k = 0; k < sps_max_sub_layers_minus1; k++) {
    if (sub_layer_profile_present_flags[k]) {  //
      // sub_layer profile_space/tier_flag/profile_idc. ignored. u(8)
      RETURN_EMPTY_ON_FAIL(buffer->ConsumeBytes(1));
      // profile_compatability_flag:  u(32)
      RETURN_EMPTY_ON_FAIL(buffer->ConsumeBytes(4));
      // sub_layer progressive_source_flag/interlaced_source_flag/
      // non_packed_constraint_flag/frame_only_constraint_flag: u(4)
      RETURN_EMPTY_ON_FAIL(buffer->ConsumeBits(4));
      // following 43-bits are profile_idc specific. We simply read/skip it.
      // u(43)
      RETURN_EMPTY_ON_FAIL(buffer->ConsumeBits(43));
      // 1-bit profile_idc specific inbld flag.  We simply read/skip it. u(1)
      RETURN_EMPTY_ON_FAIL(buffer->ConsumeBits(1));
    }
    if (sub_layer_level_present_flags[k]) {
      // sub_layer_level_idc: u(8)
      RETURN_EMPTY_ON_FAIL(buffer->ConsumeBytes(1));
    }
  }
  // sps_seq_parameter_set_id: ue(v)
  RETURN_EMPTY_ON_FAIL(buffer->ReadExponentialGolomb(&golomb_ignored));
  // chrome_format_idc: ue(v)
  RETURN_EMPTY_ON_FAIL(buffer->ReadExponentialGolomb(&chroma_format_idc));
  if (chroma_format_idc == 3) {
    // seperate_colour_plane_flag: u(1)
    RETURN_EMPTY_ON_FAIL(buffer->ReadBits(&separate_colour_plane_flag, 1));
  }
  uint32_t pic_width_in_luma_samples = 0;
  uint32_t pic_height_in_luma_samples = 0;
  // pic_width_in_luma_samples: ue(v)
  RETURN_EMPTY_ON_FAIL(
      buffer->ReadExponentialGolomb(&pic_width_in_luma_samples));
  // pic_height_in_luma_samples: ue(v)
  RETURN_EMPTY_ON_FAIL(
      buffer->ReadExponentialGolomb(&pic_height_in_luma_samples));
  // conformance_window_flag: u(1)
  uint32_t conformance_window_flag = 0;
  RETURN_EMPTY_ON_FAIL(buffer->ReadBits(&conformance_window_flag, 1));

  uint32_t conf_win_left_offset = 0;
  uint32_t conf_win_right_offset = 0;
  uint32_t conf_win_top_offset = 0;
  uint32_t conf_win_bottom_offset = 0;
  if (conformance_window_flag) {
    // conf_win_left_offset: ue(v)
    RETURN_EMPTY_ON_FAIL(buffer->ReadExponentialGolomb(&conf_win_left_offset));
    // conf_win_right_offset: ue(v)
    RETURN_EMPTY_ON_FAIL(buffer->ReadExponentialGolomb(&conf_win_right_offset));
    // conf_win_top_offset: ue(v)
    RETURN_EMPTY_ON_FAIL(buffer->ReadExponentialGolomb(&conf_win_top_offset));
    // conf_win_bottom_offset: ue(v)
    RETURN_EMPTY_ON_FAIL(
        buffer->ReadExponentialGolomb(&conf_win_bottom_offset));
  }

  // Far enough! We don't use the rest of the SPS.

  sps.vps_id = sps_video_parameter_set_id;

  // Start with the resolution determined by the pic_width/pic_height fields.
  sps.width = pic_width_in_luma_samples;
  sps.height = pic_height_in_luma_samples;

  if (conformance_window_flag) {
    int sub_width_c = ((1 == chroma_format_idc) || (2 == chroma_format_idc)) &&
                              (0 == separate_colour_plane_flag)
                          ? 2
                          : 1;
    int sub_height_c =
        (1 == chroma_format_idc) && (0 == separate_colour_plane_flag) ? 2 : 1;
    // the offset includes the pixel within conformance window. so don't need to
    // +1 as per spec
    sps.width -= sub_width_c * (conf_win_right_offset + conf_win_left_offset);
    sps.height -= sub_height_c * (conf_win_top_offset + conf_win_bottom_offset);
  }

  return OptionalSps(sps);
}

}  // namespace webrtc
