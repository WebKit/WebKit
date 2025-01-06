/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/dvqa/frames_storage.h"

#include <cstdint>
#include <memory>

#include "api/scoped_refptr.h"
#include "api/test/time_controller.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

VideoFrame Create2x2Frame(uint16_t frame_id) {
  rtc::scoped_refptr<I420Buffer> buffer =
      I420Buffer::Create(/*width=*/2, /*height=*/2);
  memset(buffer->MutableDataY(), static_cast<uint8_t>(frame_id), 4);
  memset(buffer->MutableDataU(), static_cast<uint8_t>(frame_id + 1), 1);
  memset(buffer->MutableDataV(), static_cast<uint8_t>(frame_id + 2), 1);
  return VideoFrame::Builder()
      .set_id(frame_id)
      .set_video_frame_buffer(buffer)
      .set_timestamp_us(1)
      .build();
}

void AssertHasFrame(FramesStorage& storage, uint16_t frame_id) {
  std::optional<VideoFrame> frame = storage.Get(frame_id);
  ASSERT_TRUE(frame.has_value()) << "Frame " << frame_id << " wasn't found";
  EXPECT_EQ(frame->id(), frame_id);
}

class FramesStorageTest : public testing::Test {
 protected:
  FramesStorageTest()
      : time_controller_(std::make_unique<GlobalSimulatedTimeController>(
            Timestamp::Seconds(1000))) {}

  Timestamp NowPlusSeconds(int seconds) {
    return time_controller_->GetClock()->CurrentTime() +
           TimeDelta::Seconds(seconds);
  }

  Clock* GetClock() { return time_controller_->GetClock(); }

  void AdvanceTime(TimeDelta time) { time_controller_->AdvanceTime(time); }

 private:
  std::unique_ptr<TimeController> time_controller_;
};

TEST_F(FramesStorageTest, CanGetAllAddedFrames) {
  VideoFrame frame1 = Create2x2Frame(/*frame_id=*/1);
  VideoFrame frame2 = Create2x2Frame(/*frame_id=*/2);
  VideoFrame frame3 = Create2x2Frame(/*frame_id=*/3);
  VideoFrame frame4 = Create2x2Frame(/*frame_id=*/4);
  VideoFrame frame5 = Create2x2Frame(/*frame_id=*/5);

  FramesStorage storage(TimeDelta::Seconds(1), GetClock());

  storage.Add(frame1, /*captured_time=*/NowPlusSeconds(1));
  storage.Add(frame2, /*captured_time=*/NowPlusSeconds(2));
  storage.Add(frame3, /*captured_time=*/NowPlusSeconds(3));
  storage.Add(frame4, /*captured_time=*/NowPlusSeconds(2));
  storage.Add(frame5, /*captured_time=*/NowPlusSeconds(1));

  AssertHasFrame(storage, /*frame_id=*/1);
  AssertHasFrame(storage, /*frame_id=*/2);
  AssertHasFrame(storage, /*frame_id=*/3);
  AssertHasFrame(storage, /*frame_id=*/4);
  AssertHasFrame(storage, /*frame_id=*/5);
}

TEST_F(FramesStorageTest, CanGetRemainingAddedFramesAfterRemove) {
  VideoFrame frame1 = Create2x2Frame(/*frame_id=*/1);
  VideoFrame frame2 = Create2x2Frame(/*frame_id=*/2);
  VideoFrame frame3 = Create2x2Frame(/*frame_id=*/3);
  VideoFrame frame4 = Create2x2Frame(/*frame_id=*/4);
  VideoFrame frame5 = Create2x2Frame(/*frame_id=*/5);

  FramesStorage storage(TimeDelta::Seconds(1), GetClock());

  storage.Add(frame1, /*captured_time=*/NowPlusSeconds(1));
  storage.Add(frame2, /*captured_time=*/NowPlusSeconds(2));
  storage.Add(frame3, /*captured_time=*/NowPlusSeconds(3));
  storage.Add(frame4, /*captured_time=*/NowPlusSeconds(2));
  storage.Add(frame5, /*captured_time=*/NowPlusSeconds(1));

  storage.Remove(frame1.id());
  storage.Remove(frame2.id());
  storage.Remove(frame3.id());

  AssertHasFrame(storage, /*frame_id=*/4);
  AssertHasFrame(storage, /*frame_id=*/5);
}

TEST_F(FramesStorageTest, AutoCleanupRemovesOnlyOldFrames) {
  VideoFrame frame1 = Create2x2Frame(/*frame_id=*/1);
  VideoFrame frame2 = Create2x2Frame(/*frame_id=*/2);
  VideoFrame frame3 = Create2x2Frame(/*frame_id=*/3);
  VideoFrame frame4 = Create2x2Frame(/*frame_id=*/4);
  VideoFrame frame5 = Create2x2Frame(/*frame_id=*/5);
  VideoFrame frame6 = Create2x2Frame(/*frame_id=*/6);

  FramesStorage storage(TimeDelta::Seconds(1), GetClock());

  storage.Add(frame1, /*captured_time=*/NowPlusSeconds(0));
  storage.Add(frame2, /*captured_time=*/NowPlusSeconds(1));
  storage.Add(frame3, /*captured_time=*/NowPlusSeconds(2));
  storage.Add(frame4, /*captured_time=*/NowPlusSeconds(1));
  storage.Add(frame5, /*captured_time=*/NowPlusSeconds(0));

  AdvanceTime(TimeDelta::Millis(1001));
  storage.Add(frame6, /*captured_time=*/NowPlusSeconds(3));

  AssertHasFrame(storage, /*frame_id=*/2);
  AssertHasFrame(storage, /*frame_id=*/3);
  AssertHasFrame(storage, /*frame_id=*/4);
  EXPECT_FALSE(storage.Get(/*frame_id=*/1).has_value());
  EXPECT_FALSE(storage.Get(/*frame_id=*/5).has_value());
}

TEST_F(FramesStorageTest, AllFramesAutoCleaned) {
  VideoFrame frame1 = Create2x2Frame(/*frame_id=*/1);
  VideoFrame frame2 = Create2x2Frame(/*frame_id=*/2);
  VideoFrame frame3 = Create2x2Frame(/*frame_id=*/3);

  FramesStorage storage(TimeDelta::Seconds(1), GetClock());

  storage.Add(frame1, /*captured_time=*/NowPlusSeconds(0));
  storage.Add(frame2, /*captured_time=*/NowPlusSeconds(0));
  storage.Add(frame3, /*captured_time=*/NowPlusSeconds(0));

  AdvanceTime(TimeDelta::Millis(1001));
  storage.Remove(/*frame_id=*/3);

  EXPECT_FALSE(storage.Get(/*frame_id=*/1).has_value());
  EXPECT_FALSE(storage.Get(/*frame_id=*/2).has_value());
  EXPECT_FALSE(storage.Get(/*frame_id=*/3).has_value());
}

}  // namespace
}  // namespace webrtc
