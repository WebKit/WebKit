/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/h265_pps_parser.h"

#include <memory>
#include <optional>
#include <vector>

#include "common_video/h265/h265_common.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/logging.h"

#define IN_RANGE_OR_RETURN_NULL(val, min, max)                                \
  do {                                                                        \
    if (!reader.Ok() || (val) < (min) || (val) > (max)) {                     \
      RTC_LOG(LS_WARNING) << "Error in stream: invalid value, expected " #val \
                             " to be"                                         \
                          << " in range [" << (min) << ":" << (max) << "]"    \
                          << " found " << (val) << " instead";                \
      return std::nullopt;                                                    \
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
      return std::nullopt;                                               \
    }                                                                    \
  } while (0)

namespace {
constexpr int kMaxNumTileColumnWidth = 19;
constexpr int kMaxNumTileRowHeight = 21;
constexpr int kMaxRefIdxActive = 15;
}  // namespace

namespace webrtc {

// General note: this is based off the 08/2021 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

std::optional<H265PpsParser::PpsState> H265PpsParser::ParsePps(
    rtc::ArrayView<const uint8_t> data,
    const H265SpsParser::SpsState* sps) {
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1.1 of the H.265 standard.
  return ParseInternal(H265::ParseRbsp(data), sps);
}

bool H265PpsParser::ParsePpsIds(rtc::ArrayView<const uint8_t> data,
                                uint32_t* pps_id,
                                uint32_t* sps_id) {
  RTC_DCHECK(pps_id);
  RTC_DCHECK(sps_id);
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1.1 of the H.265 standard.
  std::vector<uint8_t> unpacked_buffer = H265::ParseRbsp(data);
  BitstreamReader reader(unpacked_buffer);
  *pps_id = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_FALSE(*pps_id, 0, 63);
  *sps_id = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_FALSE(*sps_id, 0, 15);
  return reader.Ok();
}

std::optional<H265PpsParser::PpsState> H265PpsParser::ParseInternal(
    rtc::ArrayView<const uint8_t> buffer,
    const H265SpsParser::SpsState* sps) {
  BitstreamReader reader(buffer);
  PpsState pps;

  if (!sps) {
    return std::nullopt;
  }

  if (!ParsePpsIdsInternal(reader, pps.pps_id, pps.sps_id)) {
    return std::nullopt;
  }

  // dependent_slice_segments_enabled_flag: u(1)
  pps.dependent_slice_segments_enabled_flag = reader.Read<bool>();
  // output_flag_present_flag: u(1)
  pps.output_flag_present_flag = reader.Read<bool>();
  // num_extra_slice_header_bits: u(3)
  pps.num_extra_slice_header_bits = reader.ReadBits(3);
  IN_RANGE_OR_RETURN_NULL(pps.num_extra_slice_header_bits, 0, 2);
  // sign_data_hiding_enabled_flag: u(1)
  reader.ConsumeBits(1);
  // cabac_init_present_flag: u(1)
  pps.cabac_init_present_flag = reader.Read<bool>();
  // num_ref_idx_l0_default_active_minus1: ue(v)
  pps.num_ref_idx_l0_default_active_minus1 = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(pps.num_ref_idx_l0_default_active_minus1, 0,
                          kMaxRefIdxActive - 1);
  // num_ref_idx_l1_default_active_minus1: ue(v)
  pps.num_ref_idx_l1_default_active_minus1 = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(pps.num_ref_idx_l1_default_active_minus1, 0,
                          kMaxRefIdxActive - 1);
  // init_qp_minus26: se(v)
  pps.init_qp_minus26 = reader.ReadSignedExponentialGolomb();
  pps.qp_bd_offset_y = 6 * sps->bit_depth_luma_minus8;
  // Sanity-check parsed value
  IN_RANGE_OR_RETURN_NULL(pps.init_qp_minus26, -(26 + pps.qp_bd_offset_y), 25);
  // constrained_intra_pred_flag: u(1)log2_min_pcm_luma_coding_block_size_minus3
  reader.ConsumeBits(1);
  // transform_skip_enabled_flag: u(1)
  reader.ConsumeBits(1);
  // cu_qp_delta_enabled_flag: u(1)
  bool cu_qp_delta_enabled_flag = reader.Read<bool>();
  if (cu_qp_delta_enabled_flag) {
    // diff_cu_qp_delta_depth: ue(v)
    uint32_t diff_cu_qp_delta_depth = reader.ReadExponentialGolomb();
    IN_RANGE_OR_RETURN_NULL(diff_cu_qp_delta_depth, 0,
                            sps->log2_diff_max_min_luma_coding_block_size);
  }
  // pps_cb_qp_offset: se(v)
  int32_t pps_cb_qp_offset = reader.ReadSignedExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(pps_cb_qp_offset, -12, 12);
  // pps_cr_qp_offset: se(v)
  int32_t pps_cr_qp_offset = reader.ReadSignedExponentialGolomb();
  IN_RANGE_OR_RETURN_NULL(pps_cr_qp_offset, -12, 12);
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
    IN_RANGE_OR_RETURN_NULL(num_tile_columns_minus1, 0,
                            sps->pic_width_in_ctbs_y - 1);
    TRUE_OR_RETURN(num_tile_columns_minus1 < kMaxNumTileColumnWidth);
    // num_tile_rows_minus1: ue(v)
    uint32_t num_tile_rows_minus1 = reader.ReadExponentialGolomb();
    IN_RANGE_OR_RETURN_NULL(num_tile_rows_minus1, 0,
                            sps->pic_height_in_ctbs_y - 1);
    TRUE_OR_RETURN((num_tile_columns_minus1 != 0) ||
                   (num_tile_rows_minus1 != 0));
    TRUE_OR_RETURN(num_tile_rows_minus1 < kMaxNumTileRowHeight);
    // uniform_spacing_flag: u(1)
    bool uniform_spacing_flag = reader.Read<bool>();
    if (!uniform_spacing_flag) {
      int column_width_minus1[kMaxNumTileColumnWidth];
      column_width_minus1[num_tile_columns_minus1] =
          sps->pic_width_in_ctbs_y - 1;
      for (uint32_t i = 0; i < num_tile_columns_minus1; i++) {
        // column_width_minus1: ue(v)
        column_width_minus1[i] = reader.ReadExponentialGolomb();
        IN_RANGE_OR_RETURN_NULL(
            column_width_minus1[i], 0,
            column_width_minus1[num_tile_columns_minus1] - 1);
        column_width_minus1[num_tile_columns_minus1] -=
            column_width_minus1[i] + 1;
      }
      int row_height_minus1[kMaxNumTileRowHeight];
      row_height_minus1[num_tile_rows_minus1] = sps->pic_height_in_ctbs_y - 1;
      for (uint32_t i = 0; i < num_tile_rows_minus1; i++) {
        // row_height_minus1: ue(v)
        row_height_minus1[i] = reader.ReadExponentialGolomb();
        IN_RANGE_OR_RETURN_NULL(row_height_minus1[i], 0,
                                row_height_minus1[num_tile_rows_minus1] - 1);
        row_height_minus1[num_tile_rows_minus1] -= row_height_minus1[i] + 1;
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
      int pps_beta_offset_div2 = reader.ReadSignedExponentialGolomb();
      IN_RANGE_OR_RETURN_NULL(pps_beta_offset_div2, -6, 6);
      // pps_tc_offset_div2: se(v)
      int pps_tc_offset_div2 = reader.ReadSignedExponentialGolomb();
      IN_RANGE_OR_RETURN_NULL(pps_tc_offset_div2, -6, 6);
    }
  }
  // pps_scaling_list_data_present_flag: u(1)
  bool pps_scaling_list_data_present_flag = 0;
  pps_scaling_list_data_present_flag = reader.Read<bool>();
  if (pps_scaling_list_data_present_flag) {
    // scaling_list_data()
    if (!H265SpsParser::ParseScalingListData(reader)) {
      return std::nullopt;
    }
  }
  // lists_modification_present_flag: u(1)
  pps.lists_modification_present_flag = reader.Read<bool>();

  if (!reader.Ok()) {
    return std::nullopt;
  }

  return pps;
}

bool H265PpsParser::ParsePpsIdsInternal(BitstreamReader& reader,
                                        uint32_t& pps_id,
                                        uint32_t& sps_id) {
  // pic_parameter_set_id: ue(v)
  pps_id = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_FALSE(pps_id, 0, 63);
  // seq_parameter_set_id: ue(v)
  sps_id = reader.ReadExponentialGolomb();
  IN_RANGE_OR_RETURN_FALSE(sps_id, 0, 15);
  return true;
}

}  // namespace webrtc
