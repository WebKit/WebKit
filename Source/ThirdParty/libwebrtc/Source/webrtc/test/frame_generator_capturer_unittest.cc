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
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace test {
namespace {
using ::testing::Eq;
using ::testing::Property;

class MockVideoSinkInterfaceVideoFrame
    : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  MOCK_METHOD1(OnFrame, void(const VideoFrame& frame));
  MOCK_METHOD0(OnDiscardedFrame, void());
};
}  // namespace
TEST(FrameGeneratorCapturerTest, CreateFromConfig) {
  GlobalSimulatedTimeController time(Timestamp::Seconds(1000));
  FrameGeneratorCapturerConfig config;
  config.squares_video->width = 300;
  config.squares_video->height = 200;
  config.squares_video->framerate = 20;
  auto capturer = FrameGeneratorCapturer::Create(
      time.GetClock(), *time.GetTaskQueueFactory(), config);
  testing::StrictMock<MockVideoSinkInterfaceVideoFrame> mock_sink;
  capturer->AddOrUpdateSink(&mock_sink, rtc::VideoSinkWants());
  capturer->Start();
  EXPECT_CALL(mock_sink, OnFrame(Property(&VideoFrame::width, Eq(300))))
      .Times(21);
  time.AdvanceTime(TimeDelta::Seconds(1));
}
}  // namespace test
}  // namespace webrtc
