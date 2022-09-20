/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/fixed_fps_video_frame_writer_adapter.h"

#include <memory>
#include <utility>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/video_frame_writer.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace test {
namespace {

constexpr TimeDelta kOneSecond = TimeDelta::Seconds(1);

using ::testing::ElementsAre;

class InMemoryVideoWriter : public VideoFrameWriter {
 public:
  ~InMemoryVideoWriter() override = default;

  bool WriteFrame(const webrtc::VideoFrame& frame) override {
    MutexLock lock(&mutex_);
    received_frames_.push_back(frame);
    return true;
  }

  void Close() override {}

  std::vector<VideoFrame> received_frames() const {
    MutexLock lock(&mutex_);
    return received_frames_;
  }

 private:
  mutable Mutex mutex_;
  std::vector<VideoFrame> received_frames_ RTC_GUARDED_BY(mutex_);
};

VideoFrame EmptyFrameWithId(uint16_t frame_id) {
  return VideoFrame::Builder()
      .set_video_frame_buffer(I420Buffer::Create(1, 1))
      .set_id(frame_id)
      .build();
}

std::vector<uint16_t> FrameIds(const std::vector<VideoFrame>& frames) {
  std::vector<uint16_t> out;
  for (const VideoFrame& frame : frames) {
    out.push_back(frame.id());
  }
  return out;
}

std::unique_ptr<TimeController> CreateSimulatedTimeController() {
  // Using an offset of 100000 to get nice fixed width and readable
  // timestamps in typical test scenarios.
  const Timestamp kSimulatedStartTime = Timestamp::Seconds(100000);
  return std::make_unique<GlobalSimulatedTimeController>(kSimulatedStartTime);
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     WhenWrittenWithSameFpsVideoIsCorrect) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(fps, time_controller->GetClock(),
                                               std::move(inmemory_writer));

  for (int i = 1; i <= 30; ++i) {
    video_writer.WriteFrame(EmptyFrameWithId(i));
    time_controller->AdvanceTime(kOneSecond / fps);
  }
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(
      FrameIds(received_frames),
      ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                  19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30));
}

TEST(FixedFpsVideoFrameWriterAdapterTest, FrameIsRepeatedWhenThereIsAFreeze) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(fps, time_controller->GetClock(),
                                               std::move(inmemory_writer));

  // Write 10 frames
  for (int i = 1; i <= 10; ++i) {
    video_writer.WriteFrame(EmptyFrameWithId(i));
    time_controller->AdvanceTime(kOneSecond / fps);
  }

  // Freeze for 4 frames
  time_controller->AdvanceTime(4 * kOneSecond / fps);

  // Write 10 more frames
  for (int i = 11; i <= 20; ++i) {
    video_writer.WriteFrame(EmptyFrameWithId(i));
    time_controller->AdvanceTime(kOneSecond / fps);
  }
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames),
              ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10, 10, 11, 12,
                          13, 14, 15, 16, 17, 18, 19, 20));
}

TEST(FixedFpsVideoFrameWriterAdapterTest, NoFramesWritten) {
  auto time_controller = CreateSimulatedTimeController();
  int fps = 25;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(fps, time_controller->GetClock(),
                                               std::move(inmemory_writer));
  time_controller->AdvanceTime(TimeDelta::Millis(100));
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  ASSERT_TRUE(received_frames.empty());
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     FreezeInTheMiddleAndNewFrameReceivedBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(
      kFps, time_controller->GetClock(), std::move(inmemory_writer));
  video_writer.WriteFrame(EmptyFrameWithId(1));
  time_controller->AdvanceTime(2.3 * kInterval);
  video_writer.WriteFrame(EmptyFrameWithId(2));
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 1, 2));
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     FreezeInTheMiddleAndNewFrameReceivedAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(
      kFps, time_controller->GetClock(), std::move(inmemory_writer));
  video_writer.WriteFrame(EmptyFrameWithId(1));
  time_controller->AdvanceTime(2.5 * kInterval);
  video_writer.WriteFrame(EmptyFrameWithId(2));
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 1, 1, 2));
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     NewFrameReceivedBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(
      kFps, time_controller->GetClock(), std::move(inmemory_writer));
  video_writer.WriteFrame(EmptyFrameWithId(1));
  time_controller->AdvanceTime(0.3 * kInterval);
  video_writer.WriteFrame(EmptyFrameWithId(2));
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(2));
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     NewFrameReceivedAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(
      kFps, time_controller->GetClock(), std::move(inmemory_writer));
  video_writer.WriteFrame(EmptyFrameWithId(1));
  time_controller->AdvanceTime(0.5 * kInterval);
  video_writer.WriteFrame(EmptyFrameWithId(2));
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 2));
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     FreeezeAtTheEndAndDestroyBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(
      kFps, time_controller->GetClock(), std::move(inmemory_writer));
  video_writer.WriteFrame(EmptyFrameWithId(1));
  time_controller->AdvanceTime(2.3 * kInterval);
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 1, 1));
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     FreeezeAtTheEndAndDestroyAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(
      kFps, time_controller->GetClock(), std::move(inmemory_writer));
  video_writer.WriteFrame(EmptyFrameWithId(1));
  time_controller->AdvanceTime(2.5 * kInterval);
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1, 1, 1));
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     DestroyBeforeMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(
      kFps, time_controller->GetClock(), std::move(inmemory_writer));
  video_writer.WriteFrame(EmptyFrameWithId(1));
  time_controller->AdvanceTime(0.3 * kInterval);
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1));
}

TEST(FixedFpsVideoFrameWriterAdapterTest,
     DestroyAfterMiddleOfExpectedInterval) {
  auto time_controller = CreateSimulatedTimeController();
  constexpr int kFps = 10;
  constexpr TimeDelta kInterval = kOneSecond / kFps;

  auto inmemory_writer = std::make_unique<InMemoryVideoWriter>();
  InMemoryVideoWriter* inmemory_writer_ref = inmemory_writer.get();

  FixedFpsVideoFrameWriterAdapter video_writer(
      kFps, time_controller->GetClock(), std::move(inmemory_writer));
  video_writer.WriteFrame(EmptyFrameWithId(1));
  time_controller->AdvanceTime(0.5 * kInterval);
  video_writer.Close();

  std::vector<VideoFrame> received_frames =
      inmemory_writer_ref->received_frames();
  EXPECT_THAT(FrameIds(received_frames), ElementsAre(1));
}

}  // namespace
}  // namespace test
}  // namespace webrtc
