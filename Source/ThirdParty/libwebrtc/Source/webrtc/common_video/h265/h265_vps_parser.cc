/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_vps_parser.h"

#include "common_video/h265/h265_common.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/logging.h"

#if WEBRTC_WEBKIT_BUILD
#include "common_video/h265/h265_sps_parser.h"
#endif

namespace webrtc {

H265VpsParser::VpsState::VpsState() = default;

// General note: this is based off the 08/2021 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

// Unpack RBSP and parse VPS state from the supplied buffer.
absl::optional<H265VpsParser::VpsState> H265VpsParser::ParseVps(
    rtc::ArrayView<const uint8_t> data) {
  return ParseInternal(H265::ParseRbsp(data));
}

absl::optional<H265VpsParser::VpsState> H265VpsParser::ParseInternal(
    rtc::ArrayView<const uint8_t> buffer) {
  BitstreamReader reader(buffer);

  // Now, we need to use a bit buffer to parse through the actual H265 VPS
  // format. See Section 7.3.2.1 ("Video parameter set RBSP syntax") of the
  // H.265 standard for a complete description.
  VpsState vps;

  // vps_video_parameter_set_id: u(4)
  vps.id = reader.ReadBits(4);

  if (!reader.Ok()) {
    return absl::nullopt;
  }

#if WEBRTC_WEBKIT_BUILD
  // vps_base_layer_internal_flag u(1)
  reader.ConsumeBits(1);
  // vps_base_layer_available_flag u(1)
  reader.ConsumeBits(1);
  // vps_max_layers_minus1 u(6)
  vps.vps_max_sub_layers_minus1 = reader.ReadBits(6);

  if (!reader.Ok() || (vps.vps_max_sub_layers_minus1 >= kMaxSubLayers)) {
    return absl::nullopt;
  }

  //  vps_max_sub_layers_minus1 u(3)
  reader.ConsumeBits(3);
  //  vps_temporal_id_nesting_flag u(1)
  reader.ConsumeBits(1);
  //  vps_reserved_0xffff_16bits u(16)
  reader.ConsumeBits(16);

  auto profile_tier_level = H265SpsParser::ParseProfileTierLevel(true, vps.vps_max_sub_layers_minus1, reader);
  if (!reader.Ok() || !profile_tier_level) {
    return absl::nullopt;
  }

  bool vps_sub_layer_ordering_info_present_flag = reader.Read<bool>();
  for (uint32_t i = (vps_sub_layer_ordering_info_present_flag != 0) ? 0 : vps.vps_max_sub_layers_minus1; i <= vps.vps_max_sub_layers_minus1; i++) {
    // vps_max_dec_pic_buffering_minus1[ i ]: ue(v)
    reader.ReadExponentialGolomb();
    // vps_max_num_reorder_pics[ i ]: ue(v)
    vps.vps_max_num_reorder_pics[i] = reader.ReadExponentialGolomb();
    if (!reader.Ok() || (i > 0 && vps.vps_max_num_reorder_pics[i] < vps.vps_max_num_reorder_pics[i - 1])) {
      return absl::nullopt;
    }

    // vps_max_latency_increase_plus1: ue(v)
    reader.ReadExponentialGolomb();
  }
  if (!vps_sub_layer_ordering_info_present_flag) {
    for (int i = 0; i < vps.vps_max_sub_layers_minus1; ++i) {
      vps.vps_max_num_reorder_pics[i] = vps.vps_max_num_reorder_pics[vps.vps_max_sub_layers_minus1];
    }
  }
  if (!reader.Ok() || !profile_tier_level) {
    return absl::nullopt;
  }
#endif

  return vps;
}

}  // namespace webrtc
