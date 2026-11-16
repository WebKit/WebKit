/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/sps_parser.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "common_video/h264/h264_common.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"

namespace webrtc {

// Example SPS can be generated with ffmpeg. Here's an example set of commands,
// runnable on OS X:
// 1) Generate a video, from the camera:
// ffmpeg -f avfoundation -i "0" -video_size 640x360 camera.mov
//
// 2) Scale the video to the desired size:
// ffmpeg -i camera.mov -vf scale=640x360 scaled.mov
//
// 3) Get just the H.264 bitstream in AnnexB:
// ffmpeg -i scaled.mov -vcodec copy -vbsf h264_mp4toannexb -an out.h264
//
// 4) Open out.h264 and find the SPS, generally everything between the first
// two start codes (0 0 0 1 or 0 0 1). The first byte should be 0x67,
// which should be stripped out before being passed to the parser.

static const size_t kSpsBufferMaxSize = 256;

// Generates a fake SPS with basically everything empty but the width/height.
// Pass in a buffer of at least kSpsBufferMaxSize.
// The fake SPS that this generates also always has at least one emulation byte
// at offset 2, since the first two bytes are always 0, and has a 0x3 as the
// level_idc, to make sure the parser doesn't eat all 0x3 bytes.
void GenerateFakeSps(uint16_t width,
                     uint16_t height,
                     int id,
                     uint32_t log2_max_frame_num_minus4,
                     uint32_t log2_max_pic_order_cnt_lsb_minus4,
                     Buffer* out_buffer) {
  uint8_t rbsp[kSpsBufferMaxSize] = {0};
  BitBufferWriter writer(rbsp, kSpsBufferMaxSize);
  // Profile byte.
  writer.WriteUInt8(0);
  // Constraint sets and reserved zero bits.
  writer.WriteUInt8(0);
  // level_idc.
  writer.WriteUInt8(0x3u);
  // seq_paramter_set_id.
  writer.WriteExponentialGolomb(id);
  // Profile is not special, so we skip all the chroma format settings.

  // Now some bit magic.
  // log2_max_frame_num_minus4: ue(v).
  writer.WriteExponentialGolomb(log2_max_frame_num_minus4);
  // pic_order_cnt_type: ue(v). 0 is the type we want.
  writer.WriteExponentialGolomb(0);
  // log2_max_pic_order_cnt_lsb_minus4: ue(v). 0 is fine.
  writer.WriteExponentialGolomb(log2_max_pic_order_cnt_lsb_minus4);
  // max_num_ref_frames: ue(v). 0 is fine.
  writer.WriteExponentialGolomb(0);
  // gaps_in_frame_num_value_allowed_flag: u(1).
  writer.WriteBits(0, 1);
  // Next are width/height. First, calculate the mbs/map_units versions.
  uint16_t width_in_mbs_minus1 = (width + 15) / 16 - 1;

  uint16_t height_in_map_units_minus1 = (height + 15) / 16 - 1;
  // Write each as ue(v).
  writer.WriteExponentialGolomb(width_in_mbs_minus1);
  writer.WriteExponentialGolomb(height_in_map_units_minus1);
  // frame_mbs_only_flag: u(1). Progressive.
  writer.WriteBits(1, 1);
  // direct_8x8_inference_flag: u(1).
  writer.WriteBits(0, 1);
  // frame_cropping_flag: u(1). 1, so we can supply crop.
  writer.WriteBits(1, 1);
  // Now we write the left/right/top/bottom crop. For simplicity, we'll put all
  // the crop at the left/top.
  // We picked a 4:2:0 format, so the crops are 1/2 the pixel crop values.
  // Left/right.
  writer.WriteExponentialGolomb(((16 - (width % 16)) % 16) / 2);
  writer.WriteExponentialGolomb(0);
  // Top/bottom.
  writer.WriteExponentialGolomb(((16 - (height % 16)) % 16) / 2);
  writer.WriteExponentialGolomb(0);

  // vui_parameters_present_flag: u(1)
  writer.WriteBits(0, 1);

  // Get the number of bytes written (including the last partial byte).
  size_t byte_count, bit_offset;
  writer.GetCurrentOffset(&byte_count, &bit_offset);
  if (bit_offset > 0) {
    byte_count++;
  }

  out_buffer->Clear();
  H264::WriteRbsp(std::span(rbsp, byte_count), out_buffer);
}

TEST(H264SpsParserTest, TestSampleSPSHdLandscape) {
  // SPS for a 1280x720 camera capture from ffmpeg on osx. Contains
  // emulation bytes but no cropping.
  const uint8_t buffer[] = {0x7A, 0x00, 0x1F, 0xBC, 0xD9, 0x40, 0x50, 0x05,
                            0xBA, 0x10, 0x00, 0x00, 0x03, 0x00, 0xC0, 0x00,
                            0x00, 0x2A, 0xE0, 0xF1, 0x83, 0x19, 0x60};
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(1280u, sps->width);
  EXPECT_EQ(720u, sps->height);
}

TEST(H264SpsParserTest, TestSampleSPSVgaLandscape) {
  // SPS for a 640x360 camera capture from ffmpeg on osx. Contains emulation
  // bytes and cropping (360 isn't divisible by 16).
  const uint8_t buffer[] = {0x7A, 0x00, 0x1E, 0xBC, 0xD9, 0x40, 0xA0, 0x2F,
                            0xF8, 0x98, 0x40, 0x00, 0x00, 0x03, 0x01, 0x80,
                            0x00, 0x00, 0x56, 0x83, 0xC5, 0x8B, 0x65, 0x80};
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(640u, sps->width);
  EXPECT_EQ(360u, sps->height);
}

TEST(H264SpsParserTest, TestSampleSPSWeirdResolution) {
  // SPS for a 200x400 camera capture from ffmpeg on osx. Horizontal and
  // veritcal crop (neither dimension is divisible by 16).
  const uint8_t buffer[] = {0x7A, 0x00, 0x0D, 0xBC, 0xD9, 0x43, 0x43, 0x3E,
                            0x5E, 0x10, 0x00, 0x00, 0x03, 0x00, 0x60, 0x00,
                            0x00, 0x15, 0xA0, 0xF1, 0x42, 0x99, 0x60};
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(200u, sps->width);
  EXPECT_EQ(400u, sps->height);
}

TEST(H264SpsParserTest, TestSyntheticSPSQvgaLandscape) {
  Buffer buffer;
  GenerateFakeSps(320u, 180u, 1, 0, 0, &buffer);
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->id);
}

TEST(H264SpsParserTest, TestSyntheticSPSWeirdResolution) {
  Buffer buffer;
  GenerateFakeSps(156u, 122u, 2, 0, 0, &buffer);
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(156u, sps->width);
  EXPECT_EQ(122u, sps->height);
  EXPECT_EQ(2u, sps->id);
}

TEST(H264SpsParserTest, TestSampleSPSWithScalingLists) {
  // SPS from a 1920x1080 video. Contains scaling lists (and vertical cropping).
  const uint8_t buffer[] = {0x64, 0x00, 0x2a, 0xad, 0x84, 0x01, 0x0c, 0x20,
                            0x08, 0x61, 0x00, 0x43, 0x08, 0x02, 0x18, 0x40,
                            0x10, 0xc2, 0x00, 0x84, 0x3b, 0x50, 0x3c, 0x01,
                            0x13, 0xf2, 0xcd, 0xc0, 0x40, 0x40, 0x50, 0x00,
                            0x00, 0x00, 0x10, 0x00, 0x00, 0x01, 0xe8, 0x40};
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(1920u, sps->width);
  EXPECT_EQ(1080u, sps->height);
}

TEST(H264SpsParserTest, TestLog2MaxFrameNumMinus4) {
  Buffer buffer;
  GenerateFakeSps(320u, 180u, 1, 0, 0, &buffer);
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->id);
  EXPECT_EQ(4u, sps->log2_max_frame_num);

  GenerateFakeSps(320u, 180u, 1, 12, 0, &buffer);
  sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->id);
  EXPECT_EQ(16u, sps->log2_max_frame_num);

  GenerateFakeSps(320u, 180u, 1, 13, 0, &buffer);
  EXPECT_FALSE(SpsParser::ParseSps(buffer));
}

TEST(H264SpsParserTest, TestLog2MaxPicOrderCntMinus4) {
  Buffer buffer;
  GenerateFakeSps(320u, 180u, 1, 0, 0, &buffer);
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->id);
  EXPECT_EQ(4u, sps->log2_max_pic_order_cnt_lsb);

  GenerateFakeSps(320u, 180u, 1, 0, 12, &buffer);
  EXPECT_TRUE(static_cast<bool>(sps = SpsParser::ParseSps(buffer)));
  EXPECT_EQ(320u, sps->width);
  EXPECT_EQ(180u, sps->height);
  EXPECT_EQ(1u, sps->id);
  EXPECT_EQ(16u, sps->log2_max_pic_order_cnt_lsb);

  GenerateFakeSps(320u, 180u, 1, 0, 13, &buffer);
  EXPECT_FALSE(SpsParser::ParseSps(buffer));
}

TEST(H264SpsParserTest, TestInvalidSpsId) {
  Buffer buffer;
  // Valid sps.id = 31
  GenerateFakeSps(320u, 180u, 31, 0, 0, &buffer);
  EXPECT_NE(SpsParser::ParseSps(buffer), std::nullopt);

  // Invalid sps.id = 32
  GenerateFakeSps(320u, 180u, 32, 0, 0, &buffer);
  EXPECT_EQ(SpsParser::ParseSps(buffer), std::nullopt);
}

void GenerateCustomSps(uint32_t width_in_mbs_minus1,
                       uint32_t height_in_map_units_minus1,
                       bool frame_mbs_only_flag,
                       bool frame_cropping_flag,
                       uint32_t crop_left,
                       uint32_t crop_right,
                       uint32_t crop_top,
                       uint32_t crop_bottom,
                       Buffer* out_buffer,
                       uint8_t profile_idc = 0,
                       uint32_t chroma_format_idc = 1) {
  uint8_t rbsp[kSpsBufferMaxSize] = {0};
  BitBufferWriter writer(rbsp, kSpsBufferMaxSize);
  writer.WriteUInt8(profile_idc);
  writer.WriteUInt8(0);
  writer.WriteUInt8(0x3u);
  writer.WriteExponentialGolomb(0);  // sps_id = 0
  if (profile_idc == 100) {
    writer.WriteExponentialGolomb(chroma_format_idc);
    if (chroma_format_idc == 3) {
      writer.WriteBits(0, 1);  // separate_colour_plane_flag
    }
    writer.WriteExponentialGolomb(0);  // bit_depth_luma_minus8
    writer.WriteExponentialGolomb(0);  // bit_depth_chroma_minus8
    writer.WriteBits(0, 1);            // qpprime_y_zero_transform_bypass_flag
    writer.WriteBits(0, 1);            // seq_scaling_matrix_present_flag
  }
  writer.WriteExponentialGolomb(0);  // log2_max_frame_num_minus4
  writer.WriteExponentialGolomb(0);  // pic_order_cnt_type
  writer.WriteExponentialGolomb(0);  // log2_max_pic_order_cnt_lsb_minus4
  writer.WriteExponentialGolomb(0);  // max_num_ref_frames
  writer.WriteBits(0, 1);            // gaps_in_frame_num_value_allowed_flag

  writer.WriteExponentialGolomb(width_in_mbs_minus1);
  writer.WriteExponentialGolomb(height_in_map_units_minus1);
  writer.WriteBits(frame_mbs_only_flag ? 1 : 0, 1);
  if (!frame_mbs_only_flag) {
    writer.WriteBits(0, 1);  // mb_adaptive_frame_field_flag
  }
  writer.WriteBits(0, 1);  // direct_8x8_inference_flag
  if (frame_cropping_flag) {
    writer.WriteBits(1, 1);
    writer.WriteExponentialGolomb(crop_left);
    writer.WriteExponentialGolomb(crop_right);
    writer.WriteExponentialGolomb(crop_top);
    writer.WriteExponentialGolomb(crop_bottom);
  } else {
    writer.WriteBits(0, 1);
  }
  writer.WriteBits(0, 1);  // vui_parameters_present_flag

  size_t byte_count, bit_offset;
  writer.GetCurrentOffset(&byte_count, &bit_offset);
  if (bit_offset > 0) {
    byte_count++;
  }
  out_buffer->Clear();
  H264::WriteRbsp(std::span(rbsp, byte_count), out_buffer);
}

TEST(H264SpsParserTest, RejectsPicWidthOverflow) {
  Buffer buffer;
  // Dimensions that would wrap around in 32-bit arithmetic (16 * 0x10000000)
  // are safely computed in 64-bit and rejected by IsValidResolution exceeding
  // Level 5.2 limits.
  GenerateCustomSps(/*width_in_mbs_minus1=*/0x0FFFFFFF,
                    /*height_in_map_units_minus1=*/10,
                    /*frame_mbs_only_flag=*/true,
                    /*frame_cropping_flag=*/false, 0, 0, 0, 0, &buffer);
  EXPECT_EQ(SpsParser::ParseSps(buffer), std::nullopt);
}

TEST(H264SpsParserTest, RejectsPicHeightOverflow) {
  Buffer buffer;
  // Dimensions that would wrap around in 32-bit arithmetic (32 * 0x08000000)
  // are safely computed in 64-bit and rejected by IsValidResolution exceeding
  // Level 5.2 limits.
  GenerateCustomSps(/*width_in_mbs_minus1=*/10,
                    /*height_in_map_units_minus1=*/0x07FFFFFF,
                    /*frame_mbs_only_flag=*/false,
                    /*frame_cropping_flag=*/false, 0, 0, 0, 0, &buffer);
  EXPECT_EQ(SpsParser::ParseSps(buffer), std::nullopt);
}

TEST(H264SpsParserTest, RejectsCropUnderflow) {
  Buffer buffer;
  // uncropped width = 16 * (19 + 1) = 320.
  // Chroma 4:2:0 -> crop unit is 2.
  // crop_left = 100, crop_right = 100 -> total crop = 400 > 320.
  GenerateCustomSps(/*width_in_mbs_minus1=*/19,
                    /*height_in_map_units_minus1=*/10,
                    /*frame_mbs_only_flag=*/true,
                    /*frame_cropping_flag=*/true,
                    /*crop_left=*/100, /*crop_right=*/100,
                    /*crop_top=*/0, /*crop_bottom=*/0, &buffer);
  EXPECT_EQ(SpsParser::ParseSps(buffer), std::nullopt);
}

TEST(H264SpsParserTest, RejectsCropEqualToDimensions) {
  Buffer buffer;
  // uncropped width = 16 * (19 + 1) = 320.
  // Chroma 4:2:0 -> crop unit is 2.
  // crop_left = 80, crop_right = 80 -> total crop = 320 == 320 -> width 0.
  GenerateCustomSps(/*width_in_mbs_minus1=*/19,
                    /*height_in_map_units_minus1=*/10,
                    /*frame_mbs_only_flag=*/true,
                    /*frame_cropping_flag=*/true,
                    /*crop_left=*/80, /*crop_right=*/80,
                    /*crop_top=*/0, /*crop_bottom=*/0, &buffer);
  EXPECT_EQ(SpsParser::ParseSps(buffer), std::nullopt);
}

TEST(H264SpsParserTest, RejectsInvalidChromaFormatIdc) {
  Buffer buffer;
  GenerateCustomSps(/*width_in_mbs_minus1=*/19,
                    /*height_in_map_units_minus1=*/10,
                    /*frame_mbs_only_flag=*/true,
                    /*frame_cropping_flag=*/false, 0, 0, 0, 0, &buffer,
                    /*profile_idc=*/100, /*chroma_format_idc=*/4);
  EXPECT_EQ(SpsParser::ParseSps(buffer), std::nullopt);
}

TEST(H264SpsParserTest, AcceptsMaxLevel52Resolution) {
  Buffer buffer;
  // 4096x2304: width_in_mbs_minus1 = 255 (256 MBs), height_in_mbs_minus1 = 143
  // (144 MBs)
  GenerateCustomSps(/*width_in_mbs_minus1=*/255,
                    /*height_in_map_units_minus1=*/143,
                    /*frame_mbs_only_flag=*/true,
                    /*frame_cropping_flag=*/false, 0, 0, 0, 0, &buffer);
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(sps->width, 4096u);
  EXPECT_EQ(sps->height, 2304u);
}

TEST(H264SpsParserTest, RejectsResolutionExceedingLevel52MaxFS) {
  Buffer buffer;
  // 4096x2320 = 256x145 MBs = 37120 MBs > 36864
  GenerateCustomSps(/*width_in_mbs_minus1=*/255,
                    /*height_in_map_units_minus1=*/144,
                    /*frame_mbs_only_flag=*/true,
                    /*frame_cropping_flag=*/false, 0, 0, 0, 0, &buffer);
  EXPECT_EQ(SpsParser::ParseSps(buffer), std::nullopt);
}

TEST(H264SpsParserTest, InterlacedCropping420) {
  Buffer buffer;
  // 320x320 uncropped: width_in_mbs = 20 (minus 1 = 19),
  // height_in_map_units = 10 (minus 1 = 9) with frame_mbs_only_flag = false:
  // uncropped height = 16 * 2 * 10 = 320.
  // Chroma format 4:2:0: crop_unit_x = 2, crop_unit_y = 2 * (2 - 0) = 4.
  GenerateCustomSps(/*width_in_mbs_minus1=*/19,
                    /*height_in_map_units_minus1=*/9,
                    /*frame_mbs_only_flag=*/false,
                    /*frame_cropping_flag=*/true,
                    /*crop_left=*/2, /*crop_right=*/3,
                    /*crop_top=*/2, /*crop_bottom=*/3, &buffer,
                    /*profile_idc=*/100, /*chroma_format_idc=*/1);
  std::optional<SpsParser::SpsState> sps = SpsParser::ParseSps(buffer);
  ASSERT_TRUE(sps.has_value());
  EXPECT_EQ(sps->width, 320u - 2u * (2 + 3));   // 310
  EXPECT_EQ(sps->height, 320u - 4u * (2 + 3));  // 300
}

TEST(H264SpsParserTest, RejectsInterlacedCropUnderflow) {
  Buffer buffer;
  // uncropped height = 16 * 2 * 10 = 320.
  // Chroma 4:2:0 interlaced -> crop unit y is 4.
  // crop_top = 40, crop_bottom = 40 -> total crop y = 4 * 80 = 320 >= 320.
  GenerateCustomSps(/*width_in_mbs_minus1=*/19,
                    /*height_in_map_units_minus1=*/9,
                    /*frame_mbs_only_flag=*/false,
                    /*frame_cropping_flag=*/true,
                    /*crop_left=*/0, /*crop_right=*/0,
                    /*crop_top=*/40, /*crop_bottom=*/40, &buffer,
                    /*profile_idc=*/100, /*chroma_format_idc=*/1);
  EXPECT_EQ(SpsParser::ParseSps(buffer), std::nullopt);
}

}  // namespace webrtc
