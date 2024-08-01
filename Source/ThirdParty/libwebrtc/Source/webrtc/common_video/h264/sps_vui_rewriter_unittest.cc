/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/sps_vui_rewriter.h"

#include <cstdint>
#include <vector>

#include "api/video/color_space.h"
#include "common_video/h264/h264_common.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
enum SpsMode {
  kNoRewriteRequired_VuiOptimal,
  kRewriteRequired_NoVui,
  kRewriteRequired_NoBitstreamRestriction,
  kRewriteRequired_VuiSuboptimal,
};

static const size_t kSpsBufferMaxSize = 256;
static const size_t kWidth = 640;
static const size_t kHeight = 480;

static const uint8_t kStartSequence[] = {0x00, 0x00, 0x00, 0x01};
static const uint8_t kAud[] = {H264::NaluType::kAud, 0x09, 0x10};
static const uint8_t kSpsNaluType[] = {H264::NaluType::kSps};
static const uint8_t kIdr1[] = {H264::NaluType::kIdr, 0xFF, 0x00, 0x00, 0x04};
static const uint8_t kIdr2[] = {H264::NaluType::kIdr, 0xFF, 0x00, 0x11};

struct VuiHeader {
  uint32_t vui_parameters_present_flag;
  uint32_t bitstream_restriction_flag;
  uint32_t max_num_reorder_frames;
  uint32_t max_dec_frame_buffering;
  uint32_t video_signal_type_present_flag;
  uint32_t video_full_range_flag;
  uint32_t colour_description_present_flag;
  uint8_t colour_primaries;
  uint8_t transfer_characteristics;
  uint8_t matrix_coefficients;
};

static const VuiHeader kVuiNotPresent = {
    /* vui_parameters_present_flag= */ 0,
    /* bitstream_restriction_flag= */ 0,
    /* max_num_reorder_frames= */ 0,
    /* max_dec_frame_buffering= */ 0,
    /* video_signal_type_present_flag= */ 0,
    /* video_full_range_flag= */ 0,
    /* colour_description_present_flag= */ 0,
    /* colour_primaries= */ 0,
    /* transfer_characteristics= */ 0,
    /* matrix_coefficients= */ 0};

static const VuiHeader kVuiNoBitstreamRestriction = {
    /* vui_parameters_present_flag= */ 1,
    /* bitstream_restriction_flag= */ 0,
    /* max_num_reorder_frames= */ 0,
    /* max_dec_frame_buffering= */ 0,
    /* video_signal_type_present_flag= */ 0,
    /* video_full_range_flag= */ 0,
    /* colour_description_present_flag= */ 0,
    /* colour_primaries= */ 0,
    /* transfer_characteristics= */ 0,
    /* matrix_coefficients= */ 0};

static const VuiHeader kVuiNoFrameBuffering = {
    /* vui_parameters_present_flag= */ 1,
    /* bitstream_restriction_flag= */ 1,
    /* max_num_reorder_frames= */ 0,
    /* max_dec_frame_buffering= */ 1,
    /* video_signal_type_present_flag= */ 0,
    /* video_full_range_flag= */ 0,
    /* colour_description_present_flag= */ 0,
    /* colour_primaries= */ 0,
    /* transfer_characteristics= */ 0,
    /* matrix_coefficients= */ 0};

static const VuiHeader kVuiFrameBuffering = {
    /* vui_parameters_present_flag= */ 1,
    /* bitstream_restriction_flag= */ 1,
    /* max_num_reorder_frames= */ 3,
    /* max_dec_frame_buffering= */ 3,
    /* video_signal_type_present_flag= */ 0,
    /* video_full_range_flag= */ 0,
    /* colour_description_present_flag= */ 0,
    /* colour_primaries= */ 0,
    /* transfer_characteristics= */ 0,
    /* matrix_coefficients= */ 0};

static const VuiHeader kVuiNoVideoSignalType = {
    /* vui_parameters_present_flag= */ 1,
    /* bitstream_restriction_flag= */ 1,
    /* max_num_reorder_frames= */ 0,
    /* max_dec_frame_buffering= */ 1,
    /* video_signal_type_present_flag= */ 0,
    /* video_full_range_flag= */ 0,
    /* colour_description_present_flag= */ 0,
    /* colour_primaries= */ 0,
    /* transfer_characteristics= */ 0,
    /* matrix_coefficients= */ 0};

static const VuiHeader kVuiLimitedRangeNoColourDescription = {
    /* vui_parameters_present_flag= */ 1,
    /* bitstream_restriction_flag= */ 1,
    /* max_num_reorder_frames= */ 0,
    /* max_dec_frame_buffering= */ 1,
    /* video_signal_type_present_flag= */ 1,
    /* video_full_range_flag= */ 0,
    /* colour_description_present_flag= */ 0,
    /* colour_primaries= */ 0,
    /* transfer_characteristics= */ 0,
    /* matrix_coefficients= */ 0};

static const VuiHeader kVuiFullRangeNoColourDescription = {
    /* vui_parameters_present_flag= */ 1,
    /* bitstream_restriction_flag= */ 1,
    /* max_num_reorder_frames= */ 0,
    /* max_dec_frame_buffering= */ 1,
    /* video_signal_type_present_flag= */ 1,
    /* video_full_range_flag= */ 1,
    /* colour_description_present_flag= */ 0,
    /* colour_primaries= */ 0,
    /* transfer_characteristics= */ 0,
    /* matrix_coefficients= */ 0};

static const VuiHeader kVuiLimitedRangeBt709Color = {
    /* vui_parameters_present_flag= */ 1,
    /* bitstream_restriction_flag= */ 1,
    /* max_num_reorder_frames= */ 0,
    /* max_dec_frame_buffering= */ 1,
    /* video_signal_type_present_flag= */ 1,
    /* video_full_range_flag= */ 0,
    /* colour_description_present_flag= */ 1,
    /* colour_primaries= */ 1,
    /* transfer_characteristics= */ 1,
    /* matrix_coefficients= */ 1};

static const webrtc::ColorSpace kColorSpaceH264Default(
    ColorSpace::PrimaryID::kUnspecified,
    ColorSpace::TransferID::kUnspecified,
    ColorSpace::MatrixID::kUnspecified,
    ColorSpace::RangeID::kLimited);

static const webrtc::ColorSpace kColorSpacePrimariesBt709(
    ColorSpace::PrimaryID::kBT709,
    ColorSpace::TransferID::kUnspecified,
    ColorSpace::MatrixID::kUnspecified,
    ColorSpace::RangeID::kLimited);

static const webrtc::ColorSpace kColorSpaceTransferBt709(
    ColorSpace::PrimaryID::kUnspecified,
    ColorSpace::TransferID::kBT709,
    ColorSpace::MatrixID::kUnspecified,
    ColorSpace::RangeID::kLimited);

static const webrtc::ColorSpace kColorSpaceMatrixBt709(
    ColorSpace::PrimaryID::kUnspecified,
    ColorSpace::TransferID::kUnspecified,
    ColorSpace::MatrixID::kBT709,
    ColorSpace::RangeID::kLimited);

static const webrtc::ColorSpace kColorSpaceFullRange(
    ColorSpace::PrimaryID::kBT709,
    ColorSpace::TransferID::kUnspecified,
    ColorSpace::MatrixID::kUnspecified,
    ColorSpace::RangeID::kFull);

static const webrtc::ColorSpace kColorSpaceBt709LimitedRange(
    ColorSpace::PrimaryID::kBT709,
    ColorSpace::TransferID::kBT709,
    ColorSpace::MatrixID::kBT709,
    ColorSpace::RangeID::kLimited);
}  // namespace

// Generates a fake SPS with basically everything empty and with characteristics
// based off SpsMode.
// Pass in a buffer of at least kSpsBufferMaxSize.
// The fake SPS that this generates also always has at least one emulation byte
// at offset 2, since the first two bytes are always 0, and has a 0x3 as the
// level_idc, to make sure the parser doesn't eat all 0x3 bytes.
void GenerateFakeSps(const VuiHeader& vui, rtc::Buffer* out_buffer) {
  uint8_t rbsp[kSpsBufferMaxSize] = {0};
  rtc::BitBufferWriter writer(rbsp, kSpsBufferMaxSize);
  // Profile byte.
  writer.WriteUInt8(0);
  // Constraint sets and reserved zero bits.
  writer.WriteUInt8(0);
  // level_idc.
  writer.WriteUInt8(3);
  // seq_paramter_set_id.
  writer.WriteExponentialGolomb(0);
  // Profile is not special, so we skip all the chroma format settings.

  // Now some bit magic.
  // log2_max_frame_num_minus4: ue(v). 0 is fine.
  writer.WriteExponentialGolomb(0);
  // pic_order_cnt_type: ue(v).
  writer.WriteExponentialGolomb(0);
  // log2_max_pic_order_cnt_lsb_minus4: ue(v). 0 is fine.
  writer.WriteExponentialGolomb(0);

  // max_num_ref_frames: ue(v). Use 1, to make optimal/suboptimal more obvious.
  writer.WriteExponentialGolomb(1);
  // gaps_in_frame_num_value_allowed_flag: u(1).
  writer.WriteBits(0, 1);
  // Next are width/height. First, calculate the mbs/map_units versions.
  uint16_t width_in_mbs_minus1 = (kWidth + 15) / 16 - 1;

  // For the height, we're going to define frame_mbs_only_flag, so we need to
  // divide by 2. See the parser for the full calculation.
  uint16_t height_in_map_units_minus1 = ((kHeight + 15) / 16 - 1) / 2;
  // Write each as ue(v).
  writer.WriteExponentialGolomb(width_in_mbs_minus1);
  writer.WriteExponentialGolomb(height_in_map_units_minus1);
  // frame_mbs_only_flag: u(1). Needs to be false.
  writer.WriteBits(0, 1);
  // mb_adaptive_frame_field_flag: u(1).
  writer.WriteBits(0, 1);
  // direct_8x8_inferene_flag: u(1).
  writer.WriteBits(0, 1);
  // frame_cropping_flag: u(1). 1, so we can supply crop.
  writer.WriteBits(1, 1);
  // Now we write the left/right/top/bottom crop. For simplicity, we'll put all
  // the crop at the left/top.
  // We picked a 4:2:0 format, so the crops are 1/2 the pixel crop values.
  // Left/right.
  writer.WriteExponentialGolomb(((16 - (kWidth % 16)) % 16) / 2);
  writer.WriteExponentialGolomb(0);
  // Top/bottom.
  writer.WriteExponentialGolomb(((16 - (kHeight % 16)) % 16) / 2);
  writer.WriteExponentialGolomb(0);

  // Finally! The VUI.
  // vui_parameters_present_flag: u(1)
  writer.WriteBits(vui.vui_parameters_present_flag, 1);
  if (vui.vui_parameters_present_flag) {
    // aspect_ratio_info_present_flag, overscan_info_present_flag. Both u(1).
    writer.WriteBits(0, 2);

    writer.WriteBits(vui.video_signal_type_present_flag, 1);
    if (vui.video_signal_type_present_flag) {
      // video_format: u(3). 5 = Unspecified
      writer.WriteBits(5, 3);
      writer.WriteBits(vui.video_full_range_flag, 1);
      writer.WriteBits(vui.colour_description_present_flag, 1);
      if (vui.colour_description_present_flag) {
        writer.WriteUInt8(vui.colour_primaries);
        writer.WriteUInt8(vui.transfer_characteristics);
        writer.WriteUInt8(vui.matrix_coefficients);
      }
    }

    // chroma_loc_info_present_flag, timing_info_present_flag,
    // nal_hrd_parameters_present_flag, vcl_hrd_parameters_present_flag,
    // pic_struct_present_flag, All u(1)
    writer.WriteBits(0, 5);

    writer.WriteBits(vui.bitstream_restriction_flag, 1);
    if (vui.bitstream_restriction_flag) {
      // Write some defaults. Shouldn't matter for parsing, though.
      // motion_vectors_over_pic_boundaries_flag: u(1)
      writer.WriteBits(1, 1);
      // max_bytes_per_pic_denom: ue(v)
      writer.WriteExponentialGolomb(2);
      // max_bits_per_mb_denom: ue(v)
      writer.WriteExponentialGolomb(1);
      // log2_max_mv_length_horizontal: ue(v)
      // log2_max_mv_length_vertical: ue(v)
      writer.WriteExponentialGolomb(16);
      writer.WriteExponentialGolomb(16);

      // Next are the limits we care about.
      writer.WriteExponentialGolomb(vui.max_num_reorder_frames);
      writer.WriteExponentialGolomb(vui.max_dec_frame_buffering);
    }
  }

  // Get the number of bytes written (including the last partial byte).
  size_t byte_count, bit_offset;
  writer.GetCurrentOffset(&byte_count, &bit_offset);
  if (bit_offset > 0) {
    byte_count++;
  }

  H264::WriteRbsp(rtc::MakeArrayView(rbsp, byte_count), out_buffer);
}

void TestSps(const VuiHeader& vui,
             const ColorSpace* color_space,
             SpsVuiRewriter::ParseResult expected_parse_result) {
  rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
  rtc::Buffer original_sps;
  GenerateFakeSps(vui, &original_sps);

  absl::optional<SpsParser::SpsState> sps;
  rtc::Buffer rewritten_sps;
  SpsVuiRewriter::ParseResult result = SpsVuiRewriter::ParseAndRewriteSps(
      original_sps, &sps, color_space, &rewritten_sps,
      SpsVuiRewriter::Direction::kIncoming);
  EXPECT_EQ(expected_parse_result, result);
  ASSERT_TRUE(sps);
  EXPECT_EQ(sps->width, kWidth);
  EXPECT_EQ(sps->height, kHeight);
  if (vui.vui_parameters_present_flag) {
    EXPECT_EQ(sps->vui_params_present, 1u);
  }

  if (result == SpsVuiRewriter::ParseResult::kVuiRewritten) {
    // Ensure that added/rewritten SPS is parsable.
    rtc::Buffer tmp;
    result = SpsVuiRewriter::ParseAndRewriteSps(
        rewritten_sps, &sps, nullptr, &tmp,
        SpsVuiRewriter::Direction::kIncoming);
    EXPECT_EQ(SpsVuiRewriter::ParseResult::kVuiOk, result);
    ASSERT_TRUE(sps);
    EXPECT_EQ(sps->width, kWidth);
    EXPECT_EQ(sps->height, kHeight);
    EXPECT_EQ(sps->vui_params_present, 1u);
  }
}

class SpsVuiRewriterTest : public ::testing::Test,
                           public ::testing::WithParamInterface<
                               ::testing::tuple<VuiHeader,
                                                const ColorSpace*,
                                                SpsVuiRewriter::ParseResult>> {
};

TEST_P(SpsVuiRewriterTest, RewriteVui) {
  VuiHeader vui = ::testing::get<0>(GetParam());
  const ColorSpace* color_space = ::testing::get<1>(GetParam());
  SpsVuiRewriter::ParseResult expected_parse_result =
      ::testing::get<2>(GetParam());
  TestSps(vui, color_space, expected_parse_result);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SpsVuiRewriterTest,
    ::testing::Values(
        std::make_tuple(kVuiNoFrameBuffering,
                        nullptr,
                        SpsVuiRewriter::ParseResult::kVuiOk),
        std::make_tuple(kVuiNoVideoSignalType,
                        &kColorSpaceH264Default,
                        SpsVuiRewriter::ParseResult::kVuiOk),
        std::make_tuple(kVuiLimitedRangeBt709Color,
                        &kColorSpaceBt709LimitedRange,
                        SpsVuiRewriter::ParseResult::kVuiOk),
        std::make_tuple(kVuiNotPresent,
                        nullptr,
                        SpsVuiRewriter::ParseResult::kVuiRewritten),
        std::make_tuple(kVuiNoBitstreamRestriction,
                        nullptr,
                        SpsVuiRewriter::ParseResult::kVuiRewritten),
        std::make_tuple(kVuiFrameBuffering,
                        nullptr,
                        SpsVuiRewriter::ParseResult::kVuiRewritten),
        std::make_tuple(kVuiLimitedRangeNoColourDescription,
                        &kColorSpaceFullRange,
                        SpsVuiRewriter::ParseResult::kVuiRewritten),
        std::make_tuple(kVuiNoVideoSignalType,
                        &kColorSpacePrimariesBt709,
                        SpsVuiRewriter::ParseResult::kVuiRewritten),
        std::make_tuple(kVuiNoVideoSignalType,
                        &kColorSpaceTransferBt709,
                        SpsVuiRewriter::ParseResult::kVuiRewritten),
        std::make_tuple(kVuiNoVideoSignalType,
                        &kColorSpaceMatrixBt709,
                        SpsVuiRewriter::ParseResult::kVuiRewritten),
        std::make_tuple(kVuiFullRangeNoColourDescription,
                        &kColorSpaceH264Default,
                        SpsVuiRewriter::ParseResult::kVuiRewritten),
        std::make_tuple(kVuiLimitedRangeBt709Color,
                        &kColorSpaceH264Default,
                        SpsVuiRewriter::ParseResult::kVuiRewritten)));

TEST(SpsVuiRewriterOutgoingVuiTest, ParseOutgoingBitstreamOptimalVui) {
  rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

  rtc::Buffer optimal_sps;
  GenerateFakeSps(kVuiNoFrameBuffering, &optimal_sps);

  rtc::Buffer buffer;
  buffer.AppendData(kStartSequence);
  buffer.AppendData(optimal_sps);
  buffer.AppendData(kStartSequence);
  buffer.AppendData(kIdr1);

  EXPECT_THAT(SpsVuiRewriter::ParseOutgoingBitstreamAndRewrite(buffer, nullptr),
              ::testing::ElementsAreArray(buffer));
}

TEST(SpsVuiRewriterOutgoingVuiTest, ParseOutgoingBitstreamNoVui) {
  rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

  rtc::Buffer sps;
  GenerateFakeSps(kVuiNotPresent, &sps);

  rtc::Buffer buffer;
  buffer.AppendData(kStartSequence);
  buffer.AppendData(kIdr1);
  buffer.AppendData(kStartSequence);
  buffer.AppendData(kSpsNaluType);
  buffer.AppendData(sps);
  buffer.AppendData(kStartSequence);
  buffer.AppendData(kIdr2);

  rtc::Buffer optimal_sps;
  GenerateFakeSps(kVuiNoFrameBuffering, &optimal_sps);

  rtc::Buffer expected_buffer;
  expected_buffer.AppendData(kStartSequence);
  expected_buffer.AppendData(kIdr1);
  expected_buffer.AppendData(kStartSequence);
  expected_buffer.AppendData(kSpsNaluType);
  expected_buffer.AppendData(optimal_sps);
  expected_buffer.AppendData(kStartSequence);
  expected_buffer.AppendData(kIdr2);

  EXPECT_THAT(SpsVuiRewriter::ParseOutgoingBitstreamAndRewrite(buffer, nullptr),
              ::testing::ElementsAreArray(expected_buffer));
}

TEST(SpsVuiRewriterOutgoingAudTest, ParseOutgoingBitstreamWithAud) {
  rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

  rtc::Buffer optimal_sps;
  GenerateFakeSps(kVuiNoFrameBuffering, &optimal_sps);

  rtc::Buffer buffer;
  buffer.AppendData(kStartSequence);
  buffer.AppendData(kAud);
  buffer.AppendData(kStartSequence);
  buffer.AppendData(optimal_sps);
  buffer.AppendData(kStartSequence);
  buffer.AppendData(kIdr1);

  rtc::Buffer expected_buffer;
  expected_buffer.AppendData(kStartSequence);
  expected_buffer.AppendData(optimal_sps);
  expected_buffer.AppendData(kStartSequence);
  expected_buffer.AppendData(kIdr1);

  EXPECT_THAT(SpsVuiRewriter::ParseOutgoingBitstreamAndRewrite(buffer, nullptr),
              ::testing::ElementsAreArray(expected_buffer));
}
}  // namespace webrtc
