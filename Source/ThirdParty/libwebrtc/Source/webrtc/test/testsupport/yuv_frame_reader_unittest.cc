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
constexpr char kFrameContent[3][3] = {{0, 1, 2}, {1, 2, 3}, {2, 3, 4}};
constexpr int kNumFrames = sizeof(kFrameContent) / sizeof(kFrameContent[0]);
}  // namespace

class YuvFrameReaderTest : public ::testing::Test {
 protected:
  YuvFrameReaderTest() = default;
  ~YuvFrameReaderTest() override = default;

  void SetUp() override {
    filepath_ = webrtc::test::TempFilename(webrtc::test::OutputPath(),
                                           "yuv_frame_reader_unittest");
    FILE* file = fopen(filepath_.c_str(), "wb");
    fwrite(kFrameContent, 1, sizeof(kFrameContent), file);
    fclose(file);

    reader_ = CreateYuvFrameReader(filepath_, kResolution);
  }

  void TearDown() override { remove(filepath_.c_str()); }

  std::string filepath_;
  std::unique_ptr<FrameReader> reader_;
};

TEST_F(YuvFrameReaderTest, num_frames) {
  EXPECT_EQ(kNumFrames, reader_->num_frames());
}

TEST_F(YuvFrameReaderTest, PullFrame_frameContent) {
  rtc::scoped_refptr<I420BufferInterface> buffer = reader_->PullFrame();
  EXPECT_EQ(kFrameContent[0][0], *buffer->DataY());
  EXPECT_EQ(kFrameContent[0][1], *buffer->DataU());
  EXPECT_EQ(kFrameContent[0][2], *buffer->DataV());
}

TEST_F(YuvFrameReaderTest, ReadFrame_randomOrder) {
  std::vector<int> expected_frames = {2, 0, 1};
  std::vector<int> actual_frames;
  for (int frame_num : expected_frames) {
    rtc::scoped_refptr<I420BufferInterface> buffer =
        reader_->ReadFrame(frame_num);
    actual_frames.push_back(*buffer->DataY());
  }
  EXPECT_EQ(expected_frames, actual_frames);
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
          std::tuple<RepeatMode, std::vector<int>>> {};

TEST_P(YuvFrameReaderRepeatModeTest, PullFrame) {
  RepeatMode mode = std::get<0>(GetParam());
  std::vector<int> expected_frames = std::get<1>(GetParam());

  reader_ = CreateYuvFrameReader(filepath_, kResolution, mode);
  std::vector<int> read_frames;
  for (size_t i = 0; i < expected_frames.size(); ++i) {
    rtc::scoped_refptr<I420BufferInterface> buffer = reader_->PullFrame();
    read_frames.push_back(*buffer->DataY());
  }
  EXPECT_EQ(expected_frames, read_frames);
}

INSTANTIATE_TEST_SUITE_P(
    YuvFrameReaderTest,
    YuvFrameReaderRepeatModeTest,
    ::testing::ValuesIn(
        {std::make_tuple(RepeatMode::kSingle, std::vector<int>{0, 1, 2}),
         std::make_tuple(RepeatMode::kRepeat,
                         std::vector<int>{0, 1, 2, 0, 1, 2}),
         std::make_tuple(RepeatMode::kPingPong,
                         std::vector<int>{0, 1, 2, 1, 0, 1, 2})}));

class YuvFrameReaderFramerateScaleTest
    : public YuvFrameReaderTest,
      public ::testing::WithParamInterface<
          std::tuple<Ratio, std::vector<int>>> {};

TEST_P(YuvFrameReaderFramerateScaleTest, PullFrame) {
  Ratio framerate_scale = std::get<0>(GetParam());
  std::vector<int> expected_frames = std::get<1>(GetParam());

  std::vector<int> actual_frames;
  for (size_t i = 0; i < expected_frames.size(); ++i) {
    int pulled_frame;
    rtc::scoped_refptr<I420BufferInterface> buffer =
        reader_->PullFrame(&pulled_frame, kResolution, framerate_scale);
    actual_frames.push_back(pulled_frame);
  }
  EXPECT_EQ(expected_frames, actual_frames);
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
