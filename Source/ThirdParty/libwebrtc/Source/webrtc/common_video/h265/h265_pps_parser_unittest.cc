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

#include <algorithm>

#include "common_video/h265/h265_common.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr size_t kPpsBufferMaxSize = 256;
constexpr uint32_t kIgnored = 0;
}  // namespace

void WritePps(const H265PpsParser::PpsState& pps,
              bool cu_qp_delta_enabled_flag,
              bool tiles_enabled_flag,
              bool uniform_spacing_flag,
              bool deblocking_filter_control_present_flag,
              bool pps_deblocking_filter_disabled_flag,
              bool pps_scaling_list_data_present_flag,
              bool scaling_list_pred_mode_flag,
              rtc::Buffer* out_buffer) {
  uint8_t data[kPpsBufferMaxSize] = {0};
  rtc::BitBufferWriter bit_buffer(data, kPpsBufferMaxSize);

  // pic_parameter_set_id: ue(v)
  bit_buffer.WriteExponentialGolomb(pps.pps_id);
  // seq_parameter_set_id: ue(v)
  bit_buffer.WriteExponentialGolomb(pps.sps_id);
  // dependent_slice_segments_enabled_flag: u(1)
  bit_buffer.WriteBits(pps.dependent_slice_segments_enabled_flag, 1);
  // output_flag_present_flag: u(1)
  bit_buffer.WriteBits(pps.output_flag_present_flag, 1);
  // num_extra_slice_header_bits: u(3)
  bit_buffer.WriteBits(pps.num_extra_slice_header_bits, 3);
  // sign_data_hiding_enabled_flag: u(1)
  bit_buffer.WriteBits(1, 1);
  // cabac_init_present_flag: u(1)
  bit_buffer.WriteBits(pps.cabac_init_present_flag, 1);
  // num_ref_idx_l0_default_active_minus1: ue(v)
  bit_buffer.WriteExponentialGolomb(pps.num_ref_idx_l0_default_active_minus1);
  // num_ref_idx_l1_default_active_minus1: ue(v)
  bit_buffer.WriteExponentialGolomb(pps.num_ref_idx_l1_default_active_minus1);
  // init_qp_minus26: se(v)
  bit_buffer.WriteSignedExponentialGolomb(pps.init_qp_minus26);
  // constrained_intra_pred_flag: u(1)
  bit_buffer.WriteBits(0, 1);
  // transform_skip_enabled_flag: u(1)
  bit_buffer.WriteBits(0, 1);
  // cu_qp_delta_enabled_flag: u(1)
  bit_buffer.WriteBits(cu_qp_delta_enabled_flag, 1);
  if (cu_qp_delta_enabled_flag) {
    // diff_cu_qp_delta_depth: ue(v)
    bit_buffer.WriteExponentialGolomb(kIgnored);
  }
  // pps_cb_qp_offset: se(v)
  bit_buffer.WriteSignedExponentialGolomb(kIgnored);
  // pps_cr_qp_offset: se(v)
  bit_buffer.WriteSignedExponentialGolomb(kIgnored);
  // pps_slice_chroma_qp_offsets_present_flag: u(1)
  bit_buffer.WriteBits(0, 1);
  // weighted_pred_flag: u(1)
  bit_buffer.WriteBits(pps.weighted_pred_flag, 1);
  // weighted_bipred_flag: u(1)
  bit_buffer.WriteBits(pps.weighted_bipred_flag, 1);
  // transquant_bypass_enabled_flag: u(1)
  bit_buffer.WriteBits(0, 1);
  // tiles_enabled_flag: u(1)
  bit_buffer.WriteBits(tiles_enabled_flag, 1);
  // entropy_coding_sync_enabled_flag: u(1)
  bit_buffer.WriteBits(1, 1);
  if (tiles_enabled_flag) {
    // num_tile_columns_minus1: ue(v)
    bit_buffer.WriteExponentialGolomb(6);
    // num_tile_rows_minus1: ue(v)
    bit_buffer.WriteExponentialGolomb(1);
    // uniform_spacing_flag: u(1)
    bit_buffer.WriteBits(0, 1);
    if (!uniform_spacing_flag) {
      for (uint32_t i = 0; i < 6; i++) {
        // column_width_minus1: ue(v)
        bit_buffer.WriteExponentialGolomb(kIgnored);
      }
      for (uint32_t i = 0; i < 1; i++) {
        // row_height_minus1: ue(v)
        bit_buffer.WriteExponentialGolomb(kIgnored);
      }
      // loop_filter_across_tiles_enabled_flag: u(1)
      bit_buffer.WriteBits(0, 1);
    }
  }
  // pps_loop_filter_across_slices_enabled_flag: u(1)
  bit_buffer.WriteBits(1, 1);
  // deblocking_filter_control_present_flag: u(1)
  bit_buffer.WriteBits(deblocking_filter_control_present_flag, 1);
  if (deblocking_filter_control_present_flag) {
    // deblocking_filter_override_enabled_flag: u(1)
    bit_buffer.WriteBits(0, 1);
    // pps_deblocking_filter_disabled_flag: u(1)
    bit_buffer.WriteBits(pps_deblocking_filter_disabled_flag, 1);
    if (!pps_deblocking_filter_disabled_flag) {
      // pps_beta_offset_div2: se(v)
      bit_buffer.WriteSignedExponentialGolomb(kIgnored);
      // pps_tc_offset_div2: se(v)
      bit_buffer.WriteSignedExponentialGolomb(kIgnored);
    }
  }
  // pps_scaling_list_data_present_flag: u(1)
  bit_buffer.WriteBits(pps_scaling_list_data_present_flag, 1);
  if (pps_scaling_list_data_present_flag) {
    for (int size_id = 0; size_id < 4; size_id++) {
      for (int matrix_id = 0; matrix_id < 6;
           matrix_id += (size_id == 3) ? 3 : 1) {
        // scaling_list_pred_mode_flag: u(1)
        bit_buffer.WriteBits(scaling_list_pred_mode_flag, 1);
        if (!scaling_list_pred_mode_flag) {
          // scaling_list_pred_matrix_id_delta: ue(v)
          bit_buffer.WriteExponentialGolomb(kIgnored);
        } else {
          uint32_t coef_num = std::min(64, 1 << (4 + (size_id << 1)));
          if (size_id > 1) {
            // scaling_list_dc_coef_minus8: se(v)
            bit_buffer.WriteSignedExponentialGolomb(kIgnored);
          }
          for (uint32_t i = 0; i < coef_num; i++) {
            // scaling_list_delta_coef: se(v)
            bit_buffer.WriteSignedExponentialGolomb(kIgnored);
          }
        }
      }
    }
  }
  // lists_modification_present_flag: u(1)
  bit_buffer.WriteBits(pps.lists_modification_present_flag, 1);
  // log2_parallel_merge_level_minus2: ue(v)
  bit_buffer.WriteExponentialGolomb(kIgnored);
  // slice_segment_header_extension_present_flag: u(1)
  bit_buffer.WriteBits(0, 1);

  size_t byte_offset;
  size_t bit_offset;
  bit_buffer.GetCurrentOffset(&byte_offset, &bit_offset);
  if (bit_offset > 0) {
    bit_buffer.WriteBits(0, 8 - bit_offset);
    bit_buffer.GetCurrentOffset(&byte_offset, &bit_offset);
  }

  H265::WriteRbsp(rtc::MakeArrayView(data, byte_offset), out_buffer);
}

class H265PpsParserTest : public ::testing::Test {
 public:
  H265PpsParserTest() {}
  ~H265PpsParserTest() override {}

  void RunTest() {
    VerifyParsing(generated_pps_, false, false, false, false, false, false,
                  false);
    // Enable flags to cover more path
    VerifyParsing(generated_pps_, true, true, false, true, true, true, false);
  }

  void VerifyParsing(const H265PpsParser::PpsState& pps,
                     bool cu_qp_delta_enabled_flag,
                     bool tiles_enabled_flag,
                     bool uniform_spacing_flag,
                     bool deblocking_filter_control_present_flag,
                     bool pps_deblocking_filter_disabled_flag,
                     bool pps_scaling_list_data_present_flag,
                     bool scaling_list_pred_mode_flag) {
    buffer_.Clear();
    WritePps(pps, cu_qp_delta_enabled_flag, tiles_enabled_flag,
             uniform_spacing_flag, deblocking_filter_control_present_flag,
             pps_deblocking_filter_disabled_flag,
             pps_scaling_list_data_present_flag, scaling_list_pred_mode_flag,
             &buffer_);
    const uint8_t sps_buffer[] = {
        0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9d, 0x08, 0x00,
        0x00, 0x03, 0x00, 0x00, 0x5d, 0xb0, 0x02, 0x80, 0x80, 0x2d,
        0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0x80, 0x40, 0x00, 0x00,
        0x03, 0x00, 0x40, 0x00, 0x00, 0x07, 0x82};
    H265SpsParser::SpsState parsed_sps =
        H265SpsParser::ParseSps(sps_buffer).value();
    parsed_pps_ = H265PpsParser::ParsePps(buffer_, &parsed_sps);
    ASSERT_TRUE(parsed_pps_);
    EXPECT_EQ(pps.dependent_slice_segments_enabled_flag,
              parsed_pps_->dependent_slice_segments_enabled_flag);
    EXPECT_EQ(pps.cabac_init_present_flag,
              parsed_pps_->cabac_init_present_flag);
    EXPECT_EQ(pps.output_flag_present_flag,
              parsed_pps_->output_flag_present_flag);
    EXPECT_EQ(pps.num_extra_slice_header_bits,
              parsed_pps_->num_extra_slice_header_bits);
    EXPECT_EQ(pps.num_ref_idx_l0_default_active_minus1,
              parsed_pps_->num_ref_idx_l0_default_active_minus1);
    EXPECT_EQ(pps.num_ref_idx_l1_default_active_minus1,
              parsed_pps_->num_ref_idx_l1_default_active_minus1);
    EXPECT_EQ(pps.init_qp_minus26, parsed_pps_->init_qp_minus26);
    EXPECT_EQ(pps.weighted_pred_flag, parsed_pps_->weighted_pred_flag);
    EXPECT_EQ(pps.weighted_bipred_flag, parsed_pps_->weighted_bipred_flag);
    EXPECT_EQ(pps.lists_modification_present_flag,
              parsed_pps_->lists_modification_present_flag);
    EXPECT_EQ(pps.pps_id, parsed_pps_->pps_id);
    EXPECT_EQ(pps.sps_id, parsed_pps_->sps_id);
  }

  H265PpsParser::PpsState generated_pps_;
  rtc::Buffer buffer_;
  std::optional<H265PpsParser::PpsState> parsed_pps_;
  std::optional<H265SpsParser::SpsState> parsed_sps_;
};

TEST_F(H265PpsParserTest, ZeroPps) {
  RunTest();
}

TEST_F(H265PpsParserTest, MaxPps) {
  generated_pps_.dependent_slice_segments_enabled_flag = true;
  generated_pps_.init_qp_minus26 = 25;
  generated_pps_.num_extra_slice_header_bits = 1;  // 1 bit value.
  generated_pps_.weighted_bipred_flag = true;
  generated_pps_.weighted_pred_flag = true;
  generated_pps_.cabac_init_present_flag = true;
  generated_pps_.pps_id = 2;
  generated_pps_.sps_id = 1;
  RunTest();

  generated_pps_.init_qp_minus26 = -25;
  RunTest();
}

}  // namespace webrtc
