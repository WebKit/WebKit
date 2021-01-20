// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/video_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::AspectRatioType;
using webm::Colour;
using webm::DisplayUnit;
using webm::ElementParserTest;
using webm::FlagInterlaced;
using webm::Id;
using webm::Projection;
using webm::ProjectionType;
using webm::StereoMode;
using webm::Video;
using webm::VideoParser;

namespace {

class VideoParserTest : public ElementParserTest<VideoParser, Id::kVideo> {};

TEST_F(VideoParserTest, DefaultParse) {
  ParseAndVerify();

  const Video video = parser_.value();

  EXPECT_FALSE(video.interlaced.is_present());
  EXPECT_EQ(FlagInterlaced::kUnspecified, video.interlaced.value());

  EXPECT_FALSE(video.stereo_mode.is_present());
  EXPECT_EQ(StereoMode::kMono, video.stereo_mode.value());

  EXPECT_FALSE(video.alpha_mode.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.alpha_mode.value());

  EXPECT_FALSE(video.pixel_width.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_width.value());

  EXPECT_FALSE(video.pixel_height.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_height.value());

  EXPECT_FALSE(video.pixel_crop_bottom.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_bottom.value());

  EXPECT_FALSE(video.pixel_crop_top.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_top.value());

  EXPECT_FALSE(video.pixel_crop_left.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_left.value());

  EXPECT_FALSE(video.pixel_crop_right.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_right.value());

  EXPECT_FALSE(video.display_width.is_present());
  EXPECT_EQ(video.pixel_width.value(), video.display_width.value());

  EXPECT_FALSE(video.display_height.is_present());
  EXPECT_EQ(video.pixel_height.value(), video.display_height.value());

  EXPECT_FALSE(video.display_unit.is_present());
  EXPECT_EQ(DisplayUnit::kPixels, video.display_unit.value());

  EXPECT_FALSE(video.aspect_ratio_type.is_present());
  EXPECT_EQ(AspectRatioType::kFreeResizing, video.aspect_ratio_type.value());

  EXPECT_FALSE(video.frame_rate.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.frame_rate.value());

  EXPECT_FALSE(video.colour.is_present());
  EXPECT_EQ(Colour{}, video.colour.value());

  EXPECT_FALSE(video.projection.is_present());
  EXPECT_EQ(Projection{}, video.projection.value());
}

TEST_F(VideoParserTest, DefaultValues) {
  SetReaderData({
      0x9A,  // ID = 0x9A (FlagInterlaced).
      0x20, 0x00, 0x00,  // Size = 0.

      0x53, 0xB8,  // ID = 0x53B8 (StereoMode).
      0x20, 0x00, 0x00,  // Size = 0.

      0x53, 0xC0,  // ID = 0x53C0 (AlphaMode).
      0x20, 0x00, 0x00,  // Size = 0.

      0xB0,  // ID = 0xB0 (PixelWidth).
      0x20, 0x00, 0x00,  // Size = 0.

      0xBA,  // ID = 0xBA (PixelHeight).
      0x20, 0x00, 0x00,  // Size = 0.

      0x54, 0xAA,  // ID = 0x54AA (PixelCropBottom).
      0x20, 0x00, 0x00,  // Size = 0.

      0x54, 0xBB,  // ID = 0x54BB (PixelCropTop).
      0x20, 0x00, 0x00,  // Size = 0.

      0x54, 0xCC,  // ID = 0x54CC (PixelCropLeft).
      0x20, 0x00, 0x00,  // Size = 0.

      0x54, 0xDD,  // ID = 0x54DD (PixelCropRight).
      0x20, 0x00, 0x00,  // Size = 0.

      0x54, 0xB0,  // ID = 0x54B0 (DisplayWidth).
      0x20, 0x00, 0x00,  // Size = 0.

      0x54, 0xBA,  // ID = 0x54BA (DisplayHeight).
      0x20, 0x00, 0x00,  // Size = 0.

      0x54, 0xB2,  // ID = 0x54B2 (DisplayUnit).
      0x20, 0x00, 0x00,  // Size = 0.

      0x54, 0xB3,  // ID = 0x54B3 (AspectRatioType).
      0x20, 0x00, 0x00,  // Size = 0.

      0x23, 0x83, 0xE3,  // ID = 0x2383E3 (FrameRate).
      0x80,  // Size = 0.

      0x55, 0xB0,  // ID = 0x55B0 (Colour).
      0x20, 0x00, 0x00,  // Size = 0.

      0x76, 0x70,  // ID = 0x7670 (Projection).
      0x20, 0x00, 0x00,  // Size = 0.
  });

  ParseAndVerify();

  const Video video = parser_.value();

  EXPECT_TRUE(video.interlaced.is_present());
  EXPECT_EQ(FlagInterlaced::kUnspecified, video.interlaced.value());

  EXPECT_TRUE(video.stereo_mode.is_present());
  EXPECT_EQ(StereoMode::kMono, video.stereo_mode.value());

  EXPECT_TRUE(video.alpha_mode.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.alpha_mode.value());

  EXPECT_TRUE(video.pixel_width.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_width.value());

  EXPECT_TRUE(video.pixel_height.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_height.value());

  EXPECT_TRUE(video.pixel_crop_bottom.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_bottom.value());

  EXPECT_TRUE(video.pixel_crop_top.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_top.value());

  EXPECT_TRUE(video.pixel_crop_left.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_left.value());

  EXPECT_TRUE(video.pixel_crop_right.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_right.value());

  EXPECT_TRUE(video.display_width.is_present());
  EXPECT_EQ(video.pixel_width.value(), video.display_width.value());

  EXPECT_TRUE(video.display_height.is_present());
  EXPECT_EQ(video.pixel_height.value(), video.display_height.value());

  EXPECT_TRUE(video.display_unit.is_present());
  EXPECT_EQ(DisplayUnit::kPixels, video.display_unit.value());

  EXPECT_TRUE(video.aspect_ratio_type.is_present());
  EXPECT_EQ(AspectRatioType::kFreeResizing, video.aspect_ratio_type.value());

  EXPECT_TRUE(video.frame_rate.is_present());
  EXPECT_EQ(0, video.frame_rate.value());

  EXPECT_TRUE(video.colour.is_present());
  EXPECT_EQ(Colour{}, video.colour.value());

  EXPECT_TRUE(video.projection.is_present());
  EXPECT_EQ(Projection{}, video.projection.value());
}

TEST_F(VideoParserTest, CustomValues) {
  SetReaderData({
      0x9A,  // ID = 0x9A (FlagInterlaced).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x02,  // Body (value = progressive).

      0x53, 0xB8,  // ID = 0x53B8 (StereoMode).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x02,  // Body (value = top-bottom (right eye first)).

      0x53, 0xC0,  // ID = 0x53C0 (AlphaMode).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x03,  // Body (value = 3).

      0xB0,  // ID = 0xB0 (PixelWidth).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x04,  // Body (value = 4).

      0xBA,  // ID = 0xBA (PixelHeight).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x05,  // Body (value = 5).

      0x54, 0xAA,  // ID = 0x54AA (PixelCropBottom).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x06,  // Body (value = 6).

      0x54, 0xBB,  // ID = 0x54BB (PixelCropTop).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x07,  // Body (value = 7).

      0x54, 0xCC,  // ID = 0x54CC (PixelCropLeft).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x08,  // Body (value = 8).

      0x54, 0xDD,  // ID = 0x54DD (PixelCropRight).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x09,  // Body (value = 9).

      0x54, 0xB0,  // ID = 0x54B0 (DisplayWidth).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x0A,  // Body (value = 10).

      0x54, 0xBA,  // ID = 0x54BA (DisplayHeight).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x0B,  // Body (value = 11).

      0x54, 0xB2,  // ID = 0x54B2 (DisplayUnit).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x02,  // Body (value = inches).

      0x54, 0xB3,  // ID = 0x54B3 (AspectRatioType).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x01,  // Body (value = keep aspect ratio).

      0x23, 0x83, 0xE3,  // ID = 0x2383E3 (FrameRate).
      0x84,  // Size = 4.
      0x40, 0x0F, 0x1B, 0xBD,  // Body (value = 2.2360680103302001953125f).

      0x55, 0xB0,  // ID = 0x55B0 (Colour).
      0x10, 0x00, 0x00, 0x07,  // Size = 7.

      0x55, 0xB2,  //   ID = 0x55B2 (BitsPerChannel).
      0x10, 0x00, 0x00, 0x01,  //   Size = 1.
      0x01,  //   Body (value = 1).

      0x76, 0x70,  // ID = 0x7670 (Projection).
      0x10, 0x00, 0x00, 0x07,  // Size = 7.

      0x76, 0x71,  //   ID = 0x7671 (ProjectionType).
      0x10, 0x00, 0x00, 0x01,  //   Size = 1.
      0x02,  //   Body (value = cube map).
  });

  ParseAndVerify();

  const Video video = parser_.value();

  EXPECT_TRUE(video.interlaced.is_present());
  EXPECT_EQ(FlagInterlaced::kProgressive, video.interlaced.value());

  EXPECT_TRUE(video.stereo_mode.is_present());
  EXPECT_EQ(StereoMode::kTopBottomRightFirst, video.stereo_mode.value());

  EXPECT_TRUE(video.alpha_mode.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(3), video.alpha_mode.value());

  EXPECT_TRUE(video.pixel_width.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(4), video.pixel_width.value());

  EXPECT_TRUE(video.pixel_height.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(5), video.pixel_height.value());

  EXPECT_TRUE(video.pixel_crop_bottom.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(6), video.pixel_crop_bottom.value());

  EXPECT_TRUE(video.pixel_crop_top.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(7), video.pixel_crop_top.value());

  EXPECT_TRUE(video.pixel_crop_left.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(8), video.pixel_crop_left.value());

  EXPECT_TRUE(video.pixel_crop_right.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(9), video.pixel_crop_right.value());

  EXPECT_TRUE(video.display_width.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(10), video.display_width.value());

  EXPECT_TRUE(video.display_height.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(11), video.display_height.value());

  EXPECT_TRUE(video.display_unit.is_present());
  EXPECT_EQ(DisplayUnit::kInches, video.display_unit.value());

  EXPECT_TRUE(video.aspect_ratio_type.is_present());
  EXPECT_EQ(AspectRatioType::kKeep, video.aspect_ratio_type.value());

  EXPECT_TRUE(video.frame_rate.is_present());
  EXPECT_EQ(2.2360680103302001953125, video.frame_rate.value());

  EXPECT_TRUE(video.colour.is_present());
  EXPECT_TRUE(video.colour.value().bits_per_channel.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1),
            video.colour.value().bits_per_channel.value());

  EXPECT_TRUE(video.projection.is_present());
  EXPECT_TRUE(video.projection.value().type.is_present());
  EXPECT_EQ(ProjectionType::kCubeMap, video.projection.value().type.value());
}

TEST_F(VideoParserTest, AbsentDisplaySize) {
  SetReaderData({
      0xB0,  // ID = 0xB0 (PixelWidth).
      0x81,  // Size = 1.
      0x01,  // Body (value = 1).

      0xBA,  // ID = 0xBA (PixelHeight).
      0x81,  // Size = 1.
      0x02,  // Body (value = 2).
  });

  ParseAndVerify();

  const Video video = parser_.value();

  EXPECT_FALSE(video.interlaced.is_present());
  EXPECT_EQ(FlagInterlaced::kUnspecified, video.interlaced.value());

  EXPECT_FALSE(video.stereo_mode.is_present());
  EXPECT_EQ(StereoMode::kMono, video.stereo_mode.value());

  EXPECT_FALSE(video.alpha_mode.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.alpha_mode.value());

  EXPECT_TRUE(video.pixel_width.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), video.pixel_width.value());

  EXPECT_TRUE(video.pixel_height.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), video.pixel_height.value());

  EXPECT_FALSE(video.pixel_crop_bottom.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_bottom.value());

  EXPECT_FALSE(video.pixel_crop_top.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_top.value());

  EXPECT_FALSE(video.pixel_crop_left.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_left.value());

  EXPECT_FALSE(video.pixel_crop_right.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_right.value());

  EXPECT_FALSE(video.display_width.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), video.display_width.value());

  EXPECT_FALSE(video.display_height.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), video.display_height.value());

  EXPECT_FALSE(video.display_unit.is_present());
  EXPECT_EQ(DisplayUnit::kPixels, video.display_unit.value());

  EXPECT_FALSE(video.aspect_ratio_type.is_present());
  EXPECT_EQ(AspectRatioType::kFreeResizing, video.aspect_ratio_type.value());

  EXPECT_FALSE(video.frame_rate.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.frame_rate.value());

  EXPECT_FALSE(video.colour.is_present());
  EXPECT_EQ(Colour{}, video.colour.value());
}

TEST_F(VideoParserTest, DefaultDisplaySize) {
  SetReaderData({
      0xB0,  // ID = 0xB0 (PixelWidth).
      0x40, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).

      0xBA,  // ID = 0xBA (PixelHeight).
      0x40, 0x01,  // Size = 1.
      0x02,  // Body (value = 2).

      0x54, 0xB0,  // ID = 0x54B0 (DisplayWidth).
      0x40, 0x00,  // Size = 0.

      0x54, 0xBA,  // ID = 0x54BA (DisplayHeight).
      0x40, 0x00,  // Size = 0.
  });

  ParseAndVerify();

  const Video video = parser_.value();

  EXPECT_FALSE(video.interlaced.is_present());
  EXPECT_EQ(FlagInterlaced::kUnspecified, video.interlaced.value());

  EXPECT_FALSE(video.stereo_mode.is_present());
  EXPECT_EQ(StereoMode::kMono, video.stereo_mode.value());

  EXPECT_FALSE(video.alpha_mode.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.alpha_mode.value());

  EXPECT_TRUE(video.pixel_width.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), video.pixel_width.value());

  EXPECT_TRUE(video.pixel_height.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), video.pixel_height.value());

  EXPECT_FALSE(video.pixel_crop_bottom.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_bottom.value());

  EXPECT_FALSE(video.pixel_crop_top.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_top.value());

  EXPECT_FALSE(video.pixel_crop_left.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_left.value());

  EXPECT_FALSE(video.pixel_crop_right.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.pixel_crop_right.value());

  EXPECT_TRUE(video.display_width.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), video.display_width.value());

  EXPECT_TRUE(video.display_height.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), video.display_height.value());

  EXPECT_FALSE(video.display_unit.is_present());
  EXPECT_EQ(DisplayUnit::kPixels, video.display_unit.value());

  EXPECT_FALSE(video.aspect_ratio_type.is_present());
  EXPECT_EQ(AspectRatioType::kFreeResizing, video.aspect_ratio_type.value());

  EXPECT_FALSE(video.frame_rate.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), video.frame_rate.value());

  EXPECT_FALSE(video.colour.is_present());
  EXPECT_EQ(Colour{}, video.colour.value());
}

}  // namespace
