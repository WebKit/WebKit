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

#include "common_video/h265/h265_common.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"

namespace webrtc {

static constexpr size_t kSpsBufferMaxSize = 256;

// Generates a fake SPS with basically everything empty but the width/height,
// max_num_sublayer_minus1 and num_short_term_ref_pic_sets.
// Pass in a buffer of at least kSpsBufferMaxSize.
// The fake SPS that this generates also always has at least one emulation byte
// at offset 2, since the first two bytes are always 0, and has a 0x3 as the
// level_idc, to make sure the parser doesn't eat all 0x3 bytes.
// num_short_term_ref_pic_sets is set to 11 followed with 11
// short_term_ref_pic_set data in this fake sps.
void WriteSps(uint16_t width,
              uint16_t height,
              int id,
              uint32_t max_num_sublayer_minus1,
              bool sub_layer_ordering_info_present_flag,
              bool long_term_ref_pics_present_flag,
              rtc::Buffer* out_buffer) {
  uint8_t rbsp[kSpsBufferMaxSize] = {0};
  rtc::BitBufferWriter writer(rbsp, kSpsBufferMaxSize);
  // sps_video_parameter_set_id
  writer.WriteBits(0, 4);
  // sps_max_sub_layers_minus1
  writer.WriteBits(max_num_sublayer_minus1, 3);
  // sps_temporal_id_nesting_flag
  writer.WriteBits(1, 1);
  // profile_tier_level(profilePresentFlag=1, maxNumSublayersMinus1=0)
  // profile-space=0, tier=0, profile-idc=1
  writer.WriteBits(0, 2);
  writer.WriteBits(0, 1);
  writer.WriteBits(1, 5);
  // general_prfile_compatibility_flag[32]
  writer.WriteBits(0, 32);
  // general_progressive_source_flag
  writer.WriteBits(1, 1);
  // general_interlace_source_flag
  writer.WriteBits(0, 1);
  // general_non_packed_constraint_flag
  writer.WriteBits(0, 1);
  // general_frame_only_constraint_flag
  writer.WriteBits(1, 1);
  // general_reserved_zero_7bits
  writer.WriteBits(0, 7);
  // general_one_picture_only_flag
  writer.WriteBits(0, 1);
  // general_reserved_zero_35bits
  writer.WriteBits(0, 35);
  // general_inbld_flag
  writer.WriteBits(0, 1);
  // general_level_idc
  writer.WriteBits(93, 8);
  // if max_sub_layers_minus1 >=1, read the sublayer profile information
  std::vector<uint32_t> sub_layer_profile_present_flags;
  std::vector<uint32_t> sub_layer_level_present_flags;
  for (uint32_t i = 0; i < max_num_sublayer_minus1; i++) {
    // sublayer_profile_present_flag and sublayer_level_presnet_flag:  u(2)
    writer.WriteBits(1, 1);
    writer.WriteBits(1, 1);
    sub_layer_profile_present_flags.push_back(1);
    sub_layer_level_present_flags.push_back(1);
  }
  if (max_num_sublayer_minus1 > 0) {
    for (uint32_t j = max_num_sublayer_minus1; j < 8; j++) {
      // reserved 2 bits: u(2)
      writer.WriteBits(0, 2);
    }
  }
  for (uint32_t k = 0; k < max_num_sublayer_minus1; k++) {
    if (sub_layer_profile_present_flags[k]) {  //
      // sub_layer profile_space/tier_flag/profile_idc. ignored. u(8)
      writer.WriteBits(0, 8);
      // profile_compatability_flag:  u(32)
      writer.WriteBits(0, 32);
      // sub_layer progressive_source_flag/interlaced_source_flag/
      // non_packed_constraint_flag/frame_only_constraint_flag: u(4)
      writer.WriteBits(0, 4);
      // following 43-bits are profile_idc specific. We simply read/skip it.
      // u(43)
      writer.WriteBits(0, 43);
      // 1-bit profile_idc specific inbld flag.  We simply read/skip it. u(1)
      writer.WriteBits(0, 1);
    }
    if (sub_layer_level_present_flags[k]) {
      // sub_layer_level_idc: u(8)
      writer.WriteBits(0, 8);
    }
  }

  // seq_parameter_set_id
  writer.WriteExponentialGolomb(id);
  // chroma_format_idc
  writer.WriteExponentialGolomb(2);
  if (width % 8 != 0 || height % 8 != 0) {
    int width_delta = 8 - width % 8;
    int height_delta = 8 - height % 8;
    if (width_delta != 8) {
      // pic_width_in_luma_samples
      writer.WriteExponentialGolomb(width + width_delta);
    } else {
      writer.WriteExponentialGolomb(width);
    }
    if (height_delta != 8) {
      // pic_height_in_luma_samples
      writer.WriteExponentialGolomb(height + height_delta);
    } else {
      writer.WriteExponentialGolomb(height);
    }
    // conformance_window_flag
    writer.WriteBits(1, 1);
    // conf_win_left_offset
    writer.WriteExponentialGolomb((width % 8) / 2);
    // conf_win_right_offset
    writer.WriteExponentialGolomb(0);
    // conf_win_top_offset
    writer.WriteExponentialGolomb(height_delta);
    // conf_win_bottom_offset
    writer.WriteExponentialGolomb(0);
  } else {
    // pic_width_in_luma_samples
    writer.WriteExponentialGolomb(width);
    // pic_height_in_luma_samples
    writer.WriteExponentialGolomb(height);
    // conformance_window_flag
    writer.WriteBits(0, 1);
  }
  // bit_depth_luma_minus8
  writer.WriteExponentialGolomb(0);
  // bit_depth_chroma_minus8
  writer.WriteExponentialGolomb(0);
  // log2_max_pic_order_cnt_lsb_minus4
  writer.WriteExponentialGolomb(4);
  // sps_sub_layer_ordering_info_present_flag
  writer.WriteBits(sub_layer_ordering_info_present_flag, 1);
  for (uint32_t i = (sub_layer_ordering_info_present_flag != 0)
                        ? 0
                        : max_num_sublayer_minus1;
       i <= max_num_sublayer_minus1; i++) {
    // sps_max_dec_pic_buffering_minus1: ue(v)
    writer.WriteExponentialGolomb(4);
    // sps_max_num_reorder_pics: ue(v)
    writer.WriteExponentialGolomb(3);
    // sps_max_latency_increase_plus1: ue(v)
    writer.WriteExponentialGolomb(0);
  }
  // log2_min_luma_coding_block_size_minus3
  writer.WriteExponentialGolomb(0);
  // log2_diff_max_min_luma_coding_block_size
  writer.WriteExponentialGolomb(3);
  // log2_min_luma_transform_block_size_minus2
  writer.WriteExponentialGolomb(0);
  // log2_diff_max_min_luma_transform_block_size
  writer.WriteExponentialGolomb(3);
  // max_transform_hierarchy_depth_inter
  writer.WriteExponentialGolomb(0);
  // max_transform_hierarchy_depth_intra
  writer.WriteExponentialGolomb(0);
  // scaling_list_enabled_flag
  writer.WriteBits(0, 1);
  // apm_enabled_flag
  writer.WriteBits(0, 1);
  // sample_adaptive_offset_enabled_flag
  writer.WriteBits(1, 1);
  // pcm_enabled_flag
  writer.WriteBits(0, 1);
  // num_short_term_ref_pic_sets
  writer.WriteExponentialGolomb(11);
  // short_term_ref_pic_set[0]
  // num_negative_pics
  writer.WriteExponentialGolomb(4);
  // num_positive_pics
  writer.WriteExponentialGolomb(0);
  // delta_poc_s0_minus1
  writer.WriteExponentialGolomb(7);
  // used_by_curr_pic_s0_flag
  writer.WriteBits(1, 1);
  for (int i = 0; i < 2; i++) {
    // delta_poc_s0_minus1
    writer.WriteExponentialGolomb(1);
    // used_by_curr_pic_s0_flag
    writer.WriteBits(1, 1);
  }
  // delta_poc_s0_minus1
  writer.WriteExponentialGolomb(3);
  // used_by_curr_pic_s0_flag
  writer.WriteBits(1, 1);
  // short_term_ref_pic_set[1]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(0, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(3);
  for (int i = 0; i < 2; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  for (int i = 0; i < 2; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(0, 1);
    // use_delta_flag
    writer.WriteBits(0, 1);
  }
  // used_by_curr_pic_flag
  writer.WriteBits(1, 1);
  // short_term_ref_pic_set[2]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(0, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(1);
  for (int i = 0; i < 4; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  // short_term_ref_pic_set[3]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(0, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(0);
  // used_by_curr_pic_flag
  writer.WriteBits(1, 1);
  // used_by_curr_pic_flag
  writer.WriteBits(0, 1);
  // use_delta_flag
  writer.WriteBits(0, 1);
  for (int i = 0; i < 3; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  // short_term_ref_pic_set[4]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(1, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(1);
  for (int i = 0; i < 4; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  // used_by_curr_pic_flag
  writer.WriteBits(0, 1);
  // use_delta_flag
  writer.WriteBits(0, 1);
  // short_term_ref_pic_set[5]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(1, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(2);
  for (int i = 0; i < 4; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  // used_by_curr_pic_flag
  writer.WriteBits(0, 1);
  // use_delta_flag
  writer.WriteBits(0, 1);
  // short_term_ref_pic_set[6]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(0, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(0);
  // used_by_curr_pic_flag
  writer.WriteBits(1, 1);
  // used_by_curr_pic_flag
  writer.WriteBits(0, 1);
  // use_delta_flag
  writer.WriteBits(0, 1);
  for (int i = 0; i < 3; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  // short_term_ref_pic_set[7]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(1, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(1);
  for (int i = 0; i < 4; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  // used_by_curr_pic_flag
  writer.WriteBits(0, 1);
  // use_delta_flag
  writer.WriteBits(0, 1);
  // short_term_ref_pic_set[8]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(0, 1);
  // num_negative_pics
  writer.WriteExponentialGolomb(1);
  // num_positive_pics
  writer.WriteExponentialGolomb(0);
  // delta_poc_s0_minus1
  writer.WriteExponentialGolomb(7);
  // used_by_curr_pic_s0_flag
  writer.WriteBits(1, 1);
  // short_term_ref_pic_set[9]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(0, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(3);
  for (int i = 0; i < 2; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  // short_term_ref_pic_set[10]
  // inter_ref_pic_set_prediction_flag
  writer.WriteBits(1, 1);
  // delta_rps_sign
  writer.WriteBits(0, 1);
  // abs_delta_rps_minus1
  writer.WriteExponentialGolomb(1);
  for (int i = 0; i < 3; i++) {
    // used_by_curr_pic_flag
    writer.WriteBits(1, 1);
  }
  // long_term_ref_pics_present_flag
  writer.WriteBits(long_term_ref_pics_present_flag, 1);
  if (long_term_ref_pics_present_flag) {
    // num_long_term_ref_pics_sps
    writer.WriteExponentialGolomb(1);
    // lt_ref_pic_poc_lsb_sps
    writer.WriteExponentialGolomb(1);
    // used_by_curr_pic_lt_sps_flag
    writer.WriteBits(1, 8);
  }
  // sps_temproal_mvp_enabled_flag
  writer.WriteBits(1, 1);

  // Get the number of bytes written (including the last partial byte).
  size_t byte_count, bit_offset;
  writer.GetCurrentOffset(&byte_count, &bit_offset);
  if (bit_offset > 0) {
    byte_count++;
  }

  out_buffer->Clear();
  H265::WriteRbsp(rtc::MakeArrayView(rbsp, byte_count), out_buffer);
}

class H265SpsParserTest : public ::testing::Test {
 public:
  H265SpsParserTest() {}
  ~H265SpsParserTest() override {}
};

TEST_F(H265SpsParserTest, TestSampleSPSHdLandscape) {
  // SPS for a 1280x720 camera capture from ffmpeg on linux. Contains
  // emulation bytes but no cropping. This buffer is generated
  // with following command:
  // 1) ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 1280x720 camera.h265
  //
  // 2) Open camera.h265 and find the SPS, generally everything between the
  // second and third start codes (0 0 0 1 or 0 0 1). The first two bytes should
  // be 0x42 and 0x01, which should be stripped out before being passed to the
  // parser.
  const uint8_t buffer[] = {0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9d,
                            0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x5d, 0xb0,
                            0x02, 0x80, 0x80, 0x2d, 0x16, 0x59, 0x59, 0xa4,
                            0x93, 0x2b, 0x80, 0x40, 0x00, 0x00, 0x03, 0x00,
                            0x40, 0x00, 0x00, 0x07, 0x82};
  std::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(1280u, sps->width);
  EXPECT_EQ(720u, sps->height);
}

TEST_F(H265SpsParserTest, TestSampleSPSVerticalCropLandscape) {
  // SPS for a 640x260 camera captureH265SpsParser::ParseSps(buffer.data(),
  // buffer.size()) from ffmpeg on Linux,. Contains emulation bytes and vertical
  // cropping (crop from 640x264). The buffer is generated
  // with following command:
  // 1) Generate a video, from the camera:
  // ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 640x264 camera.h265
  //
  // 2) Crop the video to expected size(for example, 640x260 which will crop
  // from 640x264):
  // ffmpeg -i camera.h265 -filter:v crop=640:260:200:200 -c:v libx265
  // cropped.h265
  //
  // 3) Open cropped.h265 and find the SPS, generally everything between the
  // second and third start codes (0 0 0 1 or 0 0 1). The first two bytes should
  // be 0x42 and 0x01, which should be stripped out before being passed to the
  // parser.
  const uint8_t buffer[] = {0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9d,
                            0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x3f, 0xb0,
                            0x05, 0x02, 0x01, 0x09, 0xf2, 0xe5, 0x95, 0x9a,
                            0x49, 0x32, 0xb8, 0x04, 0x00, 0x00, 0x03, 0x00,
                            0x04, 0x00, 0x00, 0x03, 0x00, 0x78, 0x20};
  std::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(640u, sps->width);
  EXPECT_EQ(260u, sps->height);
}

TEST_F(H265SpsParserTest, TestSampleSPSHorizontalAndVerticalCrop) {
  // SPS for a 260x260 camera capture from ffmpeg on Linux. Contains emulation
  // bytes. Horizontal and veritcal crop (Crop from 264x264). The buffer is
  // generated with following command:
  // 1) Generate a video, from the camera:
  // ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 264x264 camera.h265
  //
  // 2) Crop the video to expected size(for example, 260x260 which will crop
  // from 264x264):
  // ffmpeg -i camera.h265 -filter:v crop=260:260:200:200 -c:v libx265
  // cropped.h265
  //
  // 3) Open cropped.h265 and find the SPS, generally everything between the
  // second and third start codes (0 0 0 1 or 0 0 1). The first two bytes should
  // be 0x42 and 0x01, which should be stripped out before being passed to the
  // parser.
  const uint8_t buffer[] = {0x01, 0x04, 0x08, 0x00, 0x00, 0x03, 0x00, 0x9d,
                            0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x3c, 0xb0,
                            0x08, 0x48, 0x04, 0x27, 0x72, 0xe5, 0x95, 0x9a,
                            0x49, 0x32, 0xb8, 0x04, 0x00, 0x00, 0x03, 0x00,
                            0x04, 0x00, 0x00, 0x03, 0x00, 0x78, 0x20};
  std::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(260u, sps->width);
  EXPECT_EQ(260u, sps->height);
}

TEST_F(H265SpsParserTest, TestSyntheticSPSQvgaLandscape) {
  rtc::Buffer buffer;
  WriteSps(320u, 180u, 1, 0, 1, 0, &buffer);
  std::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->sps_id);
}

TEST_F(H265SpsParserTest, TestSyntheticSPSWeirdResolution) {
  rtc::Buffer buffer;
  WriteSps(156u, 122u, 2, 0, 1, 0, &buffer);
  std::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(156u, sps->width);
  EXPECT_EQ(122u, sps->height);
  EXPECT_EQ(2u, sps->sps_id);
}

TEST_F(H265SpsParserTest, TestLog2MaxSubLayersMinus1) {
  rtc::Buffer buffer;
  WriteSps(320u, 180u, 1, 0, 1, 0, &buffer);
  std::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->sps_id);
  EXPECT_EQ(0u, sps->sps_max_sub_layers_minus1);

  WriteSps(320u, 180u, 1, 6, 1, 0, &buffer);
  std::optional<H265SpsParser::SpsState> sps1 = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps1.has_value());
  EXPECT_EQ(320u, sps1->width);
  EXPECT_EQ(180u, sps1->height);
  EXPECT_EQ(1u, sps1->sps_id);
  EXPECT_EQ(6u, sps1->sps_max_sub_layers_minus1);

  WriteSps(320u, 180u, 1, 7, 1, 0, &buffer);
  std::optional<H265SpsParser::SpsState> result =
      H265SpsParser::ParseSps(buffer);
  EXPECT_FALSE(result.has_value());
}

TEST_F(H265SpsParserTest, TestSubLayerOrderingInfoPresentFlag) {
  rtc::Buffer buffer;
  WriteSps(320u, 180u, 1, 6, 1, 0, &buffer);
  std::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->sps_id);
  EXPECT_EQ(6u, sps->sps_max_sub_layers_minus1);

  WriteSps(320u, 180u, 1, 6, 1, 0, &buffer);
  std::optional<H265SpsParser::SpsState> sps1 = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps1.has_value());
  EXPECT_EQ(320u, sps1->width);
  EXPECT_EQ(180u, sps1->height);
  EXPECT_EQ(1u, sps1->sps_id);
  EXPECT_EQ(6u, sps1->sps_max_sub_layers_minus1);
}

TEST_F(H265SpsParserTest, TestLongTermRefPicsPresentFlag) {
  rtc::Buffer buffer;
  WriteSps(320u, 180u, 1, 0, 1, 0, &buffer);
  std::optional<H265SpsParser::SpsState> sps = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->sps_id);
  EXPECT_EQ(0u, sps->long_term_ref_pics_present_flag);

  WriteSps(320u, 180u, 1, 6, 1, 1, &buffer);
  std::optional<H265SpsParser::SpsState> sps1 = H265SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps1.has_value());
  EXPECT_EQ(320u, sps1->width);
  EXPECT_EQ(180u, sps1->height);
  EXPECT_EQ(1u, sps1->sps_id);
  EXPECT_EQ(1u, sps1->long_term_ref_pics_present_flag);
}

}  // namespace webrtc
