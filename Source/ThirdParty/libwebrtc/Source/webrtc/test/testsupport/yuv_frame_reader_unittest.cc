/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

namespace {
using Ratio = FrameReader::Ratio;
using RepeatMode = YuvFrameReaderImpl::RepeatMode;

constexpr Resolution kResolution({.width = 1, .height = 1});
constexpr int kDefaultNumFrames = 3;
}  // namespace

class YuvFrameReaderTest : public ::testing::Test {
 protected:
  YuvFrameReaderTest() = default;
  ~YuvFrameReaderTest() override = default;

  void SetUp() override {
    filepath_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                           "yuv_frame_reader_unittest");
    CreateYuvFileAndReader(/*num_frames=*/3, RepeatMode::kSingle);
  }

  void TearDown() override { remove(filepath_.c_str()); }

  void CreateYuvFileAndReader(int num_frames, RepeatMode repeat_mode) {
    FILE* file = fopen(filepath_.c_str(), "wb");
    for (int i = 0; i < num_frames; ++i) {
      uint8_t y = static_cast<uint8_t>(i & 255);
      uint8_t u = static_cast<uint8_t>((i + 1) & 255);
      uint8_t v = static_cast<uint8_t>((i + 2) & 255);
      fwrite(&y, 1, 1, file);
      fwrite(&u, 1, 1, file);
      fwrite(&v, 1, 1, file);
    }
    fclose(file);

    reader_ = CreateYuvFrameReader(filepath_, kResolution, repeat_mode);
  }

  std::string filepath_;
  std::unique_ptr<FrameReader> reader_;
};

TEST_F(YuvFrameReaderTest, num_frames) {
  EXPECT_EQ(kDefaultNumFrames, reader_->num_frames());
}

TEST_F(YuvFrameReaderTest, PullFrame_frameContent) {
  rtc::scoped_refptr<I420BufferInterface> buffer = reader_->PullFrame();
  EXPECT_EQ(0u, *buffer->DataY());
  EXPECT_EQ(1u, *buffer->DataU());
  EXPECT_EQ(2u, *buffer->DataV());
}

TEST_F(YuvFrameReaderTest, ReadFrame_randomOrder) {
  rtc::scoped_refptr<I420BufferInterface> buffer = reader_->ReadFrame(2);
  EXPECT_EQ(2u, *buffer->DataY());
  buffer = reader_->ReadFrame(0);
  EXPECT_EQ(0u, *buffer->DataY());
  buffer = reader_->ReadFrame(1);
  EXPECT_EQ(1u, *buffer->DataY());
}

TEST_F(YuvFrameReaderTest, PullFrame_scale) {
  rtc::scoped_refptr<I420BufferInterface> buffer = reader_->PullFrame(
      /*pulled_frame_num=*/nullptr, Resolution({.width = 2, .height = 2}),
      FrameReader::kNoScale);
  EXPECT_EQ(2, buffer->width());
  EXPECT_EQ(2, buffer->height());
}

class YuvFrameReaderRepeatModeTest
    : public YuvFrameReaderTest,
      public ::testing::WithParamInterface<
          std::tuple<int, RepeatMode, std::vector<uint8_t>>> {};

TEST_P(YuvFrameReaderRepeatModeTest, PullFrame) {
  auto [num_frames, repeat_mode, expected_frames] = GetParam();
  CreateYuvFileAndReader(num_frames, repeat_mode);
  for (auto expected_frame : expected_frames) {
    rtc::scoped_refptr<I420BufferInterface> buffer = reader_->PullFrame();
    EXPECT_EQ(expected_frame, *buffer->DataY());
  }
}

INSTANTIATE_TEST_SUITE_P(
    YuvFrameReaderTest,
    YuvFrameReaderRepeatModeTest,
    ::testing::ValuesIn(
        {std::make_tuple(3, RepeatMode::kSingle, std::vector<uint8_t>{0, 1, 2}),
         std::make_tuple(3,
                         RepeatMode::kRepeat,
                         std::vector<uint8_t>{0, 1, 2, 0, 1, 2}),
         std::make_tuple(3,
                         RepeatMode::kPingPong,
                         std::vector<uint8_t>{0, 1, 2, 1, 0, 1, 2}),
         std::make_tuple(1,
                         RepeatMode::kPingPong,
                         std::vector<uint8_t>{0, 0})}));

class YuvFrameReaderFramerateScaleTest
    : public YuvFrameReaderTest,
      public ::testing::WithParamInterface<
          std::tuple<Ratio, std::vector<int>>> {};

TEST_P(YuvFrameReaderFramerateScaleTest, PullFrame) {
  auto [framerate_scale, expected_frames] = GetParam();
  for (auto expected_frame : expected_frames) {
    int pulled_frame;
    rtc::scoped_refptr<I420BufferInterface> buffer =
        reader_->PullFrame(&pulled_frame, kResolution, framerate_scale);
    EXPECT_EQ(pulled_frame, expected_frame);
  }
}

INSTANTIATE_TEST_SUITE_P(YuvFrameReaderTest,
                         YuvFrameReaderFramerateScaleTest,
                         ::testing::ValuesIn({
                             std::make_tuple(Ratio({.num = 1, .den = 2}),
                                             std::vector<int>{0, 2, 4}),
                             std::make_tuple(Ratio({.num = 2, .den = 3}),
                                             std::vector<int>{0, 1, 3, 4, 6}),
                             std::make_tuple(Ratio({.num = 2, .den = 1}),
                                             std::vector<int>{0, 0, 1, 1}),
                         }));

}  // namespace test
}  // namespace webrtc
