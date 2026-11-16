/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/switching_frame_reader.h"

#include <stdio.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace webrtc {
namespace test {
namespace {

// A simple fake FrameReader that generates frames with a constant luma (Y)
// value.
class ConstantLumaFrameReader : public FrameReader {
 public:
  ConstantLumaFrameReader(Resolution resolution,
                          uint8_t y_value,
                          int total_frames)
      : resolution_(resolution),
        y_value_(y_value),
        total_frames_(total_frames) {}

  scoped_refptr<I420Buffer> PullFrame() override { return PullFrame(nullptr); }

  scoped_refptr<I420Buffer> PullFrame(int* frame_num) override {
    return PullFrame(frame_num, resolution_, kNoScale);
  }

  scoped_refptr<I420Buffer> PullFrame(int* frame_num,
                                      Resolution resolution,
                                      Ratio framerate_scale) override {
    if (frame_num != nullptr) {
      *frame_num = frame_num_;
    }
    scoped_refptr<I420Buffer> buffer = ReadFrame(frame_num_, resolution);
    ++frame_num_;
    return buffer;
  }

  scoped_refptr<I420Buffer> ReadFrame(int frame_num) override {
    return ReadFrame(frame_num, resolution_);
  }

  scoped_refptr<I420Buffer> ReadFrame(int frame_num,
                                      Resolution resolution) override {
    scoped_refptr<I420Buffer> buffer =
        I420Buffer::Create(resolution.width, resolution.height);
    libyuv::I420Rect(buffer->MutableDataY(), buffer->StrideY(),
                     buffer->MutableDataU(), buffer->StrideU(),
                     buffer->MutableDataV(), buffer->StrideV(), 0, 0,
                     buffer->width(), buffer->height(), y_value_, 128, 128);
    return buffer;
  }

  int num_frames() const override { return total_frames_; }

 private:
  const Resolution resolution_;
  const uint8_t y_value_;
  const int total_frames_;
  int frame_num_ = 0;
};

TEST(SwitchingFrameReaderTest, SwitchesBetweenTwoReadersAtInterval) {
  constexpr Resolution kResolution = {.width = 640, .height = 360};
  constexpr int kFps = 30;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(100);

  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(std::make_unique<ConstantLumaFrameReader>(
      kResolution, /*y_val=*/10, 100));
  readers.push_back(std::make_unique<ConstantLumaFrameReader>(
      kResolution, /*y_val=*/20, 100));

  SwitchingFrameReader switching_reader(std::move(readers), kResolution, kFps,
                                        kSwitchInterval);

  // At 30 fps, 100ms corresponds to exactly 3 frames.
  // Frames 0, 1, 2 -> Reader 0 (Y = 10).
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(switching_reader.current_reader_index(), 0);
    scoped_refptr<I420Buffer> frame = switching_reader.PullFrame();
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->DataY()[0], 10);
  }

  // Frames 3, 4, 5 -> Reader 1 (Y = 20).
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(switching_reader.current_reader_index(), 1);
    scoped_refptr<I420Buffer> frame = switching_reader.PullFrame();
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->DataY()[0], 20);
  }

  // Frames 6, 7, 8 -> Loops back to Reader 0 (Y = 10).
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(switching_reader.current_reader_index(), 0);
    scoped_refptr<I420Buffer> frame = switching_reader.PullFrame();
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->DataY()[0], 10);
  }
}

TEST(SwitchingFrameReaderTest, LoopsOverThreeReaders) {
  constexpr Resolution kResolution = {.width = 320, .height = 180};
  constexpr int kFps = 10;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(200);

  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/10, 50));
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/20, 50));
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/30, 50));

  SwitchingFrameReader switching_reader(std::move(readers), kResolution, kFps,
                                        kSwitchInterval);

  // At 10 fps, 200ms is 2 frames per interval.
  // Reader 0
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(switching_reader.current_reader_index(), 0);
    scoped_refptr<I420Buffer> frame = switching_reader.PullFrame();
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->DataY()[0], 10);
  }
  // Reader 1
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(switching_reader.current_reader_index(), 1);
    scoped_refptr<I420Buffer> frame = switching_reader.PullFrame();
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->DataY()[0], 20);
  }
  // Reader 2
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(switching_reader.current_reader_index(), 2);
    scoped_refptr<I420Buffer> frame = switching_reader.PullFrame();
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->DataY()[0], 30);
  }
  // Reader 0 again
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(switching_reader.current_reader_index(), 0);
    scoped_refptr<I420Buffer> frame = switching_reader.PullFrame();
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->DataY()[0], 10);
  }
}

TEST(SwitchingFrameReaderTest, ScalesDifferingInputResolutionToTarget) {
  constexpr Resolution kTargetResolution = {.width = 640, .height = 360};
  constexpr Resolution kSourceRes0 = {.width = 320, .height = 180};
  constexpr Resolution kSourceRes1 = {.width = 1280, .height = 720};
  constexpr int kFps = 30;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Seconds(1);

  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kSourceRes0, /*y_val=*/10, 50));
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kSourceRes1, /*y_val=*/20, 50));

  SwitchingFrameReader switching_reader(std::move(readers), kTargetResolution,
                                        kFps, kSwitchInterval);

  // Pull from reader 0 (native 320x180) -> scaled to 640x360.
  scoped_refptr<I420Buffer> frame0 = switching_reader.PullFrame();
  ASSERT_TRUE(frame0);
  EXPECT_EQ(frame0->width(), kTargetResolution.width);
  EXPECT_EQ(frame0->height(), kTargetResolution.height);
}

TEST(SwitchingFrameReaderTest, ReadFrameAccessesCorrectReaderAndSubIndex) {
  constexpr Resolution kResolution = {.width = 320, .height = 180};
  constexpr int kFps = 10;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(200);  // 2 frames

  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/10, 50));
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/20, 50));

  SwitchingFrameReader switching_reader(std::move(readers), kResolution, kFps,
                                        kSwitchInterval);

  // Frame 0 -> Reader 0
  scoped_refptr<I420Buffer> frame0 = switching_reader.ReadFrame(0);
  ASSERT_TRUE(frame0);
  EXPECT_EQ(frame0->DataY()[0], 10);

  // Frame 2 -> Reader 1 (at sub-index 0)
  scoped_refptr<I420Buffer> frame2 = switching_reader.ReadFrame(2);
  ASSERT_TRUE(frame2);
  EXPECT_EQ(frame2->DataY()[0], 20);

  // Frame 4 -> Reader 0 (at sub-index 2)
  scoped_refptr<I420Buffer> frame4 = switching_reader.ReadFrame(4);
  ASSERT_TRUE(frame4);
  EXPECT_EQ(frame4->DataY()[0], 10);
}

TEST(SwitchingFrameReaderTest, ReportsTotalFramesAsSumOfReaders) {
  constexpr Resolution kResolution = {.width = 320, .height = 180};
  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(std::make_unique<ConstantLumaFrameReader>(kResolution, 10,
                                                              /*frames=*/25));
  readers.push_back(std::make_unique<ConstantLumaFrameReader>(kResolution, 20,
                                                              /*frames=*/35));

  SwitchingFrameReader switching_reader(std::move(readers), kResolution, 30,
                                        TimeDelta::Seconds(1));
  EXPECT_EQ(switching_reader.num_frames(), 60);
}

TEST(SwitchingFrameReaderTest, ReadsFromMultipleResourceFiles) {
  const std::string file1 = test::ResourcePath("foreman_cif_short", "yuv");
  const std::string file2 = test::ResourcePath("foreman_128x96", "yuv");

  constexpr Resolution kTargetResolution = {.width = 640, .height = 360};
  constexpr int kFps = 30;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(100);

  std::unique_ptr<FrameReader> reader = CreateSwitchingFrameReader(
      {file1, file2}, kTargetResolution, kFps, kSwitchInterval,
      YuvFrameReaderImpl::RepeatMode::kRepeat);
  ASSERT_NE(reader, nullptr);

  // At 30 fps, 100ms corresponds to 3 frames from Reader 0 (foreman_cif_short).
  // Scaled from CIF (352x288) to target resolution 640x360.
  for (int i = 0; i < 3; ++i) {
    scoped_refptr<I420Buffer> frame = reader->PullFrame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->width(), kTargetResolution.width);
    EXPECT_EQ(frame->height(), kTargetResolution.height);
  }

  // Next 3 frames are from Reader 1 (foreman_128x96).
  // Scaled from 128x96 to target resolution 640x360.
  for (int i = 0; i < 3; ++i) {
    scoped_refptr<I420Buffer> frame = reader->PullFrame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->width(), kTargetResolution.width);
    EXPECT_EQ(frame->height(), kTargetResolution.height);
  }
}

TEST(SwitchingFrameReaderTest, MixesYuvAndY4mClips) {
  const std::string yuv_file = test::ResourcePath("foreman_128x96", "yuv");

  // Create a temporary Y4M file using Y4mFrameWriterImpl.
  std::string y4m_file =
      test::TempFilename(test::OutputPath(), "switching_frame_reader_y4m");
  Y4mFrameWriterImpl writer(y4m_file, /*width=*/320, /*height=*/240,
                            /*frame_rate=*/30);
  ASSERT_TRUE(writer.Init());
  std::vector<uint8_t> frame_data(writer.FrameLength(), 42);
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(writer.WriteFrame(frame_data.data()));
  }
  writer.Close();

  constexpr Resolution kTargetResolution = {.width = 640, .height = 480};
  constexpr int kFps = 30;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(100);

  std::unique_ptr<FrameReader> reader = CreateSwitchingFrameReader(
      {yuv_file, y4m_file}, kTargetResolution, kFps, kSwitchInterval,
      YuvFrameReaderImpl::RepeatMode::kRepeat);
  ASSERT_NE(reader, nullptr);

  // First 3 frames from Reader 0 (YUV, foreman_128x96).
  for (int i = 0; i < 3; ++i) {
    scoped_refptr<I420Buffer> frame = reader->PullFrame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->width(), kTargetResolution.width);
    EXPECT_EQ(frame->height(), kTargetResolution.height);
  }

  // Next 3 frames from Reader 1 (Y4M, 320x240).
  for (int i = 0; i < 3; ++i) {
    scoped_refptr<I420Buffer> frame = reader->PullFrame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->width(), kTargetResolution.width);
    EXPECT_EQ(frame->height(), kTargetResolution.height);
  }

  remove(y4m_file.c_str());
}

TEST(SwitchingFrameReaderTest, HalfFramerateSkipsFrames) {
  constexpr Resolution kResolution = {.width = 320, .height = 180};
  constexpr int kFps = 30;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(100);

  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/10, 50));
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/20, 50));

  SwitchingFrameReader reader(std::move(readers), kResolution, kFps,
                              kSwitchInterval);

  // For half framerate (1/2), expected pulled frames are 0, 2, 4.
  const std::vector<int> expected_frames = {0, 2, 4};
  for (int expected : expected_frames) {
    int pulled_frame = -1;
    scoped_refptr<I420Buffer> frame = reader.PullFrame(
        &pulled_frame, kResolution, FrameReader::Ratio({.num = 1, .den = 2}));
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(pulled_frame, expected);
  }
}

TEST(SwitchingFrameReaderTest, DoubleFramerateRepeatsFrames) {
  constexpr Resolution kResolution = {.width = 320, .height = 180};
  constexpr int kFps = 30;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(100);

  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/10, 50));
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/20, 50));

  SwitchingFrameReader reader(std::move(readers), kResolution, kFps,
                              kSwitchInterval);

  // For double framerate (2/1), expected pulled frames are 0, 0, 1, 1.
  const std::vector<int> expected_frames = {0, 0, 1, 1};
  for (int expected : expected_frames) {
    int pulled_frame = -1;
    scoped_refptr<I420Buffer> frame = reader.PullFrame(
        &pulled_frame, kResolution, FrameReader::Ratio({.num = 2, .den = 1}));
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(pulled_frame, expected);
  }
}

TEST(SwitchingFrameReaderTest, TwoThirdsFramerateScaling) {
  constexpr Resolution kResolution = {.width = 320, .height = 180};
  constexpr int kFps = 30;
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(100);

  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/10, 50));
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/20, 50));

  SwitchingFrameReader reader(std::move(readers), kResolution, kFps,
                              kSwitchInterval);

  // For 2/3 framerate, expected pulled frames are 0, 1, 3, 4, 6.
  const std::vector<int> expected_frames = {0, 1, 3, 4, 6};
  for (int expected : expected_frames) {
    int pulled_frame = -1;
    scoped_refptr<I420Buffer> frame = reader.PullFrame(
        &pulled_frame, kResolution, FrameReader::Ratio({.num = 2, .den = 3}));
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(pulled_frame, expected);
  }
}

TEST(SwitchingFrameReaderTest, NonIntegerSwitchingIntervalUsesMemoization) {
  constexpr Resolution kResolution = {.width = 320, .height = 180};
  constexpr int kFps = 15;
  // 15 fps and 100ms interval => 1.5 frames per interval.
  // Frame 0 (0 ms): interval 0 -> reader 0 (Y=10)
  // Frame 1 (66 ms): interval 0 -> reader 0 (Y=10)
  // Frame 2 (133 ms): interval 1 -> reader 1 (Y=20)
  // Frame 3 (200 ms): interval 2 -> reader 0 (Y=10)
  // Frame 4 (266 ms): interval 2 -> reader 0 (Y=10)
  // Frame 5 (333 ms): interval 3 -> reader 1 (Y=20)
  constexpr TimeDelta kSwitchInterval = TimeDelta::Millis(100);

  std::vector<std::unique_ptr<FrameReader>> readers;
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/10, 50));
  readers.push_back(
      std::make_unique<ConstantLumaFrameReader>(kResolution, /*y_val=*/20, 50));

  SwitchingFrameReader reader(std::move(readers), kResolution, kFps,
                              kSwitchInterval);

  const std::vector<uint8_t> expected_lumas = {10, 10, 20, 10, 10, 20};
  for (size_t i = 0; i < expected_lumas.size(); ++i) {
    int pulled_frame = -1;
    scoped_refptr<I420Buffer> frame = reader.PullFrame(&pulled_frame);
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(pulled_frame, static_cast<int>(i));
    EXPECT_EQ(frame->DataY()[0], expected_lumas[i]);

    // Also verify ReadFrame gives the identical frame.
    scoped_refptr<I420Buffer> read_frame = reader.ReadFrame(pulled_frame);
    ASSERT_NE(read_frame, nullptr);
    EXPECT_EQ(read_frame->DataY()[0], expected_lumas[i]);
  }
}

}  // namespace
}  // namespace test
}  // namespace webrtc
