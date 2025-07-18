/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/frame_generator_capturer.h"

#include "test/create_frame_generator_capturer.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace test {
namespace {
using ::testing::Eq;
using ::testing::Property;

constexpr int kWidth = 640;
constexpr int kHeight = 360;

class MockVideoSinkInterfaceVideoFrame : public VideoSinkInterface<VideoFrame> {
 public:
  MOCK_METHOD(void, OnFrame, (const VideoFrame& frame), (override));
  MOCK_METHOD(void, OnDiscardedFrame, (), (override));
};
}  // namespace

TEST(FrameGeneratorCapturerTest, CreateFromConfig) {
  GlobalSimulatedTimeController time(Timestamp::Seconds(1000));
  FrameGeneratorCapturerConfig config;
  config.squares_video->width = 300;
  config.squares_video->height = 200;
  config.squares_video->framerate = 20;
  auto capturer = CreateFrameGeneratorCapturer(
      time.GetClock(), *time.GetTaskQueueFactory(), config);
  testing::StrictMock<MockVideoSinkInterfaceVideoFrame> mock_sink;
  capturer->AddOrUpdateSink(&mock_sink, VideoSinkWants());
  capturer->Start();
  EXPECT_CALL(mock_sink, OnFrame(Property(&VideoFrame::width, Eq(300))))
      .Times(21);
  time.AdvanceTime(TimeDelta::Seconds(1));
}

TEST(FrameGeneratorCapturerTest, OnOutputFormatRequest) {
  GlobalSimulatedTimeController time(Timestamp::Seconds(1000));
  FrameGeneratorCapturerConfig config;
  config.squares_video->width = kWidth;
  config.squares_video->height = kHeight;
  config.squares_video->framerate = 20;
  auto capturer = CreateFrameGeneratorCapturer(
      time.GetClock(), *time.GetTaskQueueFactory(), config);
  testing::StrictMock<MockVideoSinkInterfaceVideoFrame> mock_sink;
  capturer->AddOrUpdateSink(&mock_sink, VideoSinkWants());
  capturer->OnOutputFormatRequest(kWidth / 2, kHeight / 2, /*max_fps=*/10);
  capturer->Start();
  EXPECT_CALL(mock_sink, OnFrame(Property(&VideoFrame::width, Eq(kWidth / 2))))
      .Times(11);
  time.AdvanceTime(TimeDelta::Seconds(1));
}

TEST(FrameGeneratorCapturerTest, ChangeResolution) {
  GlobalSimulatedTimeController time(Timestamp::Seconds(1000));
  FrameGeneratorCapturerConfig config;
  config.squares_video->width = kWidth;
  config.squares_video->height = kHeight;
  config.squares_video->framerate = 20;
  auto capturer = CreateFrameGeneratorCapturer(
      time.GetClock(), *time.GetTaskQueueFactory(), config);
  EXPECT_TRUE(capturer->GetResolution());
  EXPECT_EQ(kWidth, capturer->GetResolution()->width);
  EXPECT_EQ(kHeight, capturer->GetResolution()->height);
  capturer->Start();
  time.AdvanceTime(TimeDelta::Seconds(1));
  ASSERT_TRUE(capturer->GetResolution());
  EXPECT_EQ(kWidth, capturer->GetResolution()->width);
  EXPECT_EQ(kHeight, capturer->GetResolution()->height);

  capturer->ChangeResolution(kWidth / 2, kHeight / 2);
  time.AdvanceTime(TimeDelta::Seconds(1));
  ASSERT_TRUE(capturer->GetResolution());
  EXPECT_EQ(kWidth / 2, capturer->GetResolution()->width);
  EXPECT_EQ(kHeight / 2, capturer->GetResolution()->height);
}

TEST(FrameGeneratorCapturerTest, AllowZeroHertz) {
  GlobalSimulatedTimeController time(Timestamp::Seconds(1000));
  FrameGeneratorCapturerConfig config;
  config.image_slides->framerate = 30;
  config.image_slides->change_interval = TimeDelta::Millis(500);
  config.allow_zero_hertz = true;
  auto capturer = CreateFrameGeneratorCapturer(
      time.GetClock(), *time.GetTaskQueueFactory(), config);
  testing::StrictMock<MockVideoSinkInterfaceVideoFrame> mock_sink;
  capturer->AddOrUpdateSink(&mock_sink, VideoSinkWants());
  capturer->Start();
  // The video changes frame every 500ms so during 10s we expect to capture 20
  // frames. The framerate set to 30 is ignored.
  EXPECT_CALL(mock_sink, OnFrame).Times(21);
  time.AdvanceTime(TimeDelta::Seconds(10));
}

TEST(FrameGeneratorCapturerTest, AllowZeroHertzMinimumFps) {
  GlobalSimulatedTimeController time(Timestamp::Seconds(1000));
  FrameGeneratorCapturerConfig config;
  config.image_slides->framerate = 1;
  config.image_slides->change_interval = TimeDelta::Seconds(11);
  config.allow_zero_hertz = true;
  auto capturer = CreateFrameGeneratorCapturer(
      time.GetClock(), *time.GetTaskQueueFactory(), config);
  testing::StrictMock<MockVideoSinkInterfaceVideoFrame> mock_sink;
  capturer->AddOrUpdateSink(&mock_sink, VideoSinkWants());
  capturer->Start();
  // The video frame never changes but the capturer still sends a minimum of one
  // frame per second.
  EXPECT_CALL(mock_sink, OnFrame).Times(11);
  time.AdvanceTime(TimeDelta::Seconds(10));
}

}  // namespace test
}  // namespace webrtc
