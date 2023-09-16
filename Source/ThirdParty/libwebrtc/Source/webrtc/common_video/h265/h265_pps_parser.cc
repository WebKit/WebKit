/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_pps_parser.h"

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_sps_parser.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/logging.h"

#define RETURN_EMPTY_ON_FAIL(x) \
  if (!(x)) {                   \
    return absl::nullopt;       \
  }

namespace {
const int kMaxPicInitQpDeltaValue = 25;
const int kMinPicInitQpDeltaValue = -26;
}  // namespace

namespace webrtc {

// General note: this is based off the 06/2019 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

absl::optional<H265PpsParser::PpsState> H265PpsParser::ParsePps(
    const uint8_t* data,
    size_t length) {
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1.1 of the H.265 standard.
  return ParseInternal(H265::ParseRbsp(data, length));
}

bool H265PpsParser::ParsePpsIds(const uint8_t* data,
                                size_t length,
                                uint32_t* pps_id,
                                uint32_t* sps_id) {
  RTC_DCHECK(pps_id);
  RTC_DCHECK(sps_id);
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1.1 of the H.265 standard.
  std::vector<uint8_t> unpacked_buffer = H265::ParseRbsp(data, length);
  BitstreamReader reader(unpacked_buffer);
  *pps_id = reader.ReadExponentialGolomb();
  *sps_id = reader.ReadExponentialGolomb();
  return reader.Ok();
}

absl::optional<uint32_t> H265PpsParser::ParsePpsIdFromSliceSegmentLayerRbsp(
    const uint8_t* data,
    size_t length,
    uint8_t nalu_type) {
  std::vector<uint8_t> unpacked_buffer = H265::ParseRbsp(data, length);
  BitstreamReader slice_reader(unpacked_buffer);

  // first_slice_segment_in_pic_flag: u(1)
  slice_reader.ConsumeBits(1);
  if (!slice_reader.Ok()) {
    return absl::nullopt;
  }

  if (nalu_type >= H265::NaluType::kBlaWLp &&
      nalu_type <= H265::NaluType::kRsvIrapVcl23) {
    // no_output_of_prior_pics_flag: u(1)
    slice_reader.ConsumeBits(1);
  }

  // slice_pic_parameter_set_id: ue(v)
  uint32_t slice_pic_parameter_set_id = slice_reader.ReadExponentialGolomb();
  if (!slice_reader.Ok()) {
    return absl::nullopt;
  }

  return slice_pic_parameter_set_id;
}

absl::optional<H265PpsParser::PpsState> H265PpsParser::ParseInternal(
    rtc::ArrayView<const uint8_t> buffer) {
  BitstreamReader reader(buffer);
  PpsState pps;

  if(!ParsePpsIdsInternal(reader, pps.id, pps.sps_id)){
    return absl::nullopt;
  }

  // dependent_slice_segments_enabled_flag: u(1)
  pps.dependent_slice_segments_enabled_flag = reader.Read<bool>();
  // output_flag_present_flag: u(1)
  pps.output_flag_present_flag = reader.Read<bool>();
  // num_extra_slice_header_bits: u(3)
  pps.num_extra_slice_header_bits = reader.ReadBits(3);
  // sign_data_hiding_enabled_flag: u(1)
  reader.ConsumeBits(1);
  // cabac_init_present_flag: u(1)
  pps.cabac_init_present_flag = reader.Read<bool>();
  // num_ref_idx_l0_default_active_minus1: ue(v)
  pps.num_ref_idx_l0_default_active_minus1 = reader.ReadExponentialGolomb();
  // num_ref_idx_l1_default_active_minus1: ue(v)
  pps.num_ref_idx_l1_default_active_minus1 = reader.ReadExponentialGolomb();
  // init_qp_minus26: se(v)
  pps.pic_init_qp_minus26 = reader.ReadSignedExponentialGolomb();
  // Sanity-check parsed value
  if (pps.pic_init_qp_minus26 > kMaxPicInitQpDeltaValue ||
      pps.pic_init_qp_minus26 < kMinPicInitQpDeltaValue) {
    return absl::nullopt;
  }
  // constrained_intra_pred_flag: u(1)
  reader.ConsumeBits(1);
  // transform_skip_enabled_flag: u(1)
  reader.ConsumeBits(1);
  // cu_qp_delta_enabled_flag: u(1)
  bool cu_qp_delta_enabled_flag = reader.Read<bool>();
  if (cu_qp_delta_enabled_flag) {
    // diff_cu_qp_delta_depth: ue(v)
    reader.ReadExponentialGolomb();
  }
  // pps_cb_qp_offset: se(v)
  reader.ReadSignedExponentialGolomb();
  // pps_cr_qp_offset: se(v)
  reader.ReadSignedExponentialGolomb();
  // pps_slice_chroma_qp_offsets_present_flag: u(1)
  reader.ConsumeBits(1);
  // weighted_pred_flag: u(1)
  pps.weighted_pred_flag = reader.Read<bool>();
  // weighted_bipred_flag: u(1)
  pps.weighted_bipred_flag = reader.Read<bool>();
  // transquant_bypass_enabled_flag: u(1)
  reader.ConsumeBits(1);
  // tiles_enabled_flag: u(1)
  bool tiles_enabled_flag = reader.Read<bool>();
  // entropy_coding_sync_enabled_flag: u(1)
  reader.ConsumeBits(1);
  if (tiles_enabled_flag) {
    // num_tile_columns_minus1: ue(v)
    uint32_t num_tile_columns_minus1 = reader.ReadExponentialGolomb();
    // num_tile_rows_minus1: ue(v)
    uint32_t num_tile_rows_minus1 = reader.ReadExponentialGolomb();
    // uniform_spacing_flag: u(1)
    bool uniform_spacing_flag = reader.Read<bool>();
    if (!uniform_spacing_flag) {
      for (uint32_t i = 0; i < num_tile_columns_minus1; i++) {
        // column_width_minus1: ue(v)
        reader.ReadExponentialGolomb();
      }
      for (uint32_t i = 0; i < num_tile_rows_minus1; i++) {
        // row_height_minus1: ue(v)
        reader.ReadExponentialGolomb();
      }
      // loop_filter_across_tiles_enabled_flag: u(1)
      reader.ConsumeBits(1);
    }
  }
  // pps_loop_filter_across_slices_enabled_flag: u(1)
  reader.ConsumeBits(1);
  // deblocking_filter_control_present_flag: u(1)
  bool deblocking_filter_control_present_flag = reader.Read<bool>();
  if (deblocking_filter_control_present_flag) {
    // deblocking_filter_override_enabled_flag: u(1)
    reader.ConsumeBits(1);
    // pps_deblocking_filter_disabled_flag: u(1)
    bool pps_deblocking_filter_disabled_flag = reader.Read<bool>();
    if (!pps_deblocking_filter_disabled_flag) {
      // pps_beta_offset_div2: se(v)
      reader.ReadSignedExponentialGolomb();
      // pps_tc_offset_div2: se(v)
      reader.ReadSignedExponentialGolomb();
    }
  }
  // pps_scaling_list_data_present_flag: u(1)
  bool pps_scaling_list_data_present_flag = 0;
  pps_scaling_list_data_present_flag = reader.Read<bool>();
  if (pps_scaling_list_data_present_flag) {
    // scaling_list_data()
    if (!H265SpsParser::ParseScalingListData(reader)) {
      return absl::nullopt;
    }
  }
  // lists_modification_present_flag: u(1)
  pps.lists_modification_present_flag = reader.Read<bool>();
  // log2_parallel_merge_level_minus2: ue(v)
  reader.ReadExponentialGolomb();
  // slice_segment_header_extension_present_flag: u(1)
  reader.ConsumeBits(1);

  if (!reader.Ok()) {
    return absl::nullopt;
  }

  return pps;
}

bool H265PpsParser::ParsePpsIdsInternal(BitstreamReader& reader,
                                        uint32_t& pps_id,
                                        uint32_t& sps_id) {
  // pic_parameter_set_id: ue(v)
  pps_id = reader.ReadExponentialGolomb();
  if (!reader.Ok())
    return false;
  // seq_parameter_set_id: ue(v)
  sps_id = reader.ReadExponentialGolomb();
  if (!reader.Ok())
    return false;
  return true;
}

}  // namespace webrtc
