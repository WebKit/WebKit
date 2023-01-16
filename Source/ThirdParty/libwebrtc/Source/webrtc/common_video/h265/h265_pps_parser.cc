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

#include "common_video/h264/h264_common.h"
#include "common_video/h265/h265_common.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/logging.h"

#define RETURN_EMPTY_ON_FAIL(x) \
  if (!(x)) {                   \
    return absl::nullopt;        \
  }

namespace {
const int kMaxPicInitQpDeltaValue = 25;
const int kMinPicInitQpDeltaValue = -26;
}  // namespace

namespace webrtc {

// General note: this is based off the 02/2018 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

absl::optional<H265PpsParser::PpsState> H265PpsParser::ParsePps(
    const uint8_t* data,
    size_t length) {
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1 of the H.264 standard.
  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(data, length);
  BitstreamReader bit_buffer(unpacked_buffer);
  return ParseInternal(&bit_buffer);
}

bool H265PpsParser::ParsePpsIds(const uint8_t* data,
                                size_t length,
                                uint32_t* pps_id,
                                uint32_t* sps_id) {
  RTC_DCHECK(pps_id);
  RTC_DCHECK(sps_id);
  // First, parse out rbsp, which is basically the source buffer minus emulation
  // bytes (the last byte of a 0x00 0x00 0x03 sequence). RBSP is defined in
  // section 7.3.1 of the H.265 standard.
  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(data, length);
  BitstreamReader bit_buffer(unpacked_buffer);
  return ParsePpsIdsInternal(&bit_buffer, pps_id, sps_id);
}

absl::optional<uint32_t> H265PpsParser::ParsePpsIdFromSliceSegmentLayerRbsp(
    const uint8_t* data,
    size_t length,
    uint8_t nalu_type) {
  BitstreamReader slice_reader(rtc::MakeArrayView(data, length));

  // first_slice_segment_in_pic_flag: u(1)
  slice_reader.ConsumeBits(1);

  if (nalu_type >= H265::NaluType::kBlaWLp &&
      nalu_type <= H265::NaluType::kRsvIrapVcl23) {
    // no_output_of_prior_pics_flag: u(1)
    slice_reader.ConsumeBits(1);
  }

  // slice_pic_parameter_set_id: ue(v)
  uint32_t slice_pic_parameter_set_id = slice_reader.ReadExponentialGolomb();

  return slice_pic_parameter_set_id;
}

absl::optional<H265PpsParser::PpsState> H265PpsParser::ParseInternal(
    BitstreamReader* bit_buffer) {
  PpsState pps;

  RETURN_EMPTY_ON_FAIL(ParsePpsIdsInternal(bit_buffer, &pps.id, &pps.sps_id));

  uint32_t bits_tmp;
  uint32_t golomb_ignored;
  // entropy_coding_mode_flag: u(1)
  uint32_t entropy_coding_mode_flag = bit_buffer->ReadBits(1);
  pps.entropy_coding_mode_flag = entropy_coding_mode_flag != 0;
  // bottom_field_pic_order_in_frame_present_flag: u(1)
  uint32_t bottom_field_pic_order_in_frame_present_flag = bit_buffer->ReadBits(1);
  pps.bottom_field_pic_order_in_frame_present_flag =
      bottom_field_pic_order_in_frame_present_flag != 0;

  // num_slice_groups_minus1: ue(v)
  uint32_t num_slice_groups_minus1 = bit_buffer->ReadExponentialGolomb();
  if (num_slice_groups_minus1 > 0) {
    // slice_group_map_type: ue(v)
    uint32_t slice_group_map_type = bit_buffer->ReadExponentialGolomb();
    if (slice_group_map_type == 0) {
      for (uint32_t i_group = 0; i_group <= num_slice_groups_minus1;
           ++i_group) {
        // run_length_minus1[iGroup]: ue(v)
        golomb_ignored = bit_buffer->ReadExponentialGolomb();
      }
    } else if (slice_group_map_type == 1) {
      // TODO(sprang): Implement support for dispersed slice group map type.
      // See 8.2.2.2 Specification for dispersed slice group map type.
    } else if (slice_group_map_type == 2) {
      for (uint32_t i_group = 0; i_group <= num_slice_groups_minus1;
           ++i_group) {
        // top_left[iGroup]: ue(v)
        golomb_ignored = bit_buffer->ReadExponentialGolomb();
        // bottom_right[iGroup]: ue(v)
          golomb_ignored = bit_buffer->ReadExponentialGolomb();
      }
    } else if (slice_group_map_type == 3 || slice_group_map_type == 4 ||
               slice_group_map_type == 5) {
      // slice_group_change_direction_flag: u(1)
        bits_tmp = bit_buffer->ReadBits(1);
      // slice_group_change_rate_minus1: ue(v)
        golomb_ignored = bit_buffer->ReadExponentialGolomb();
    } else if (slice_group_map_type == 6) {
      // pic_size_in_map_units_minus1: ue(v)
      uint32_t pic_size_in_map_units_minus1 = bit_buffer->ReadExponentialGolomb();
      uint32_t slice_group_id_bits = 0;
      uint32_t num_slice_groups = num_slice_groups_minus1 + 1;
      // If num_slice_groups is not a power of two an additional bit is required
      // to account for the ceil() of log2() below.
      if ((num_slice_groups & (num_slice_groups - 1)) != 0)
        ++slice_group_id_bits;
      while (num_slice_groups > 0) {
        num_slice_groups >>= 1;
        ++slice_group_id_bits;
      }
      for (uint32_t i = 0; i <= pic_size_in_map_units_minus1; i++) {
        // slice_group_id[i]: u(v)
        // Represented by ceil(log2(num_slice_groups_minus1 + 1)) bits.
          bits_tmp = bit_buffer->ReadBits(slice_group_id_bits);
      }
    }
  }
  // num_ref_idx_l0_default_active_minus1: ue(v)
  bit_buffer->ReadExponentialGolomb();
  // num_ref_idx_l1_default_active_minus1: ue(v)
  bit_buffer->ReadExponentialGolomb();
  // weighted_pred_flag: u(1)
  uint32_t weighted_pred_flag;
  weighted_pred_flag = bit_buffer->ReadBits(1);
  pps.weighted_pred_flag = weighted_pred_flag != 0;
  // weighted_bipred_idc: u(2)
  pps.weighted_bipred_idc = bit_buffer->ReadBits(2);

  // pic_init_qp_minus26: se(v)
  pps.pic_init_qp_minus26 = bit_buffer->ReadSignedExponentialGolomb();
  // Sanity-check parsed value
  if (pps.pic_init_qp_minus26 > kMaxPicInitQpDeltaValue ||
      pps.pic_init_qp_minus26 < kMinPicInitQpDeltaValue) {
    RETURN_EMPTY_ON_FAIL(false);
  }
  // pic_init_qs_minus26: se(v)
  bit_buffer->ReadExponentialGolomb();
  // chroma_qp_index_offset: se(v)
  bit_buffer->ReadExponentialGolomb();
  // deblocking_filter_control_present_flag: u(1)
  // constrained_intra_pred_flag: u(1)
  bits_tmp = bit_buffer->ReadBits(2);
  // redundant_pic_cnt_present_flag: u(1)
  pps.redundant_pic_cnt_present_flag = bit_buffer->ReadBits(1);

  // Ignore -Wunused-but-set-variable warnings.
  (void)bits_tmp;
  (void)golomb_ignored;

  return pps;
}

bool H265PpsParser::ParsePpsIdsInternal(BitstreamReader* bit_buffer,
                                        uint32_t* pps_id,
                                        uint32_t* sps_id) {
  // pic_parameter_set_id: ue(v)
  *pps_id = bit_buffer->ReadExponentialGolomb();
  // seq_parameter_set_id: ue(v)
  *sps_id = bit_buffer->ReadExponentialGolomb();
  return true;
}

}  // namespace webrtc
