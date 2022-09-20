/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/task_queue_frame_decode_scheduler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/task_queue.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Optional;

class TaskQueueFrameDecodeSchedulerTest : public ::testing::Test {
 public:
  TaskQueueFrameDecodeSchedulerTest()
      : time_controller_(Timestamp::Millis(2000)),
        task_queue_(time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
            "scheduler",
            TaskQueueFactory::Priority::NORMAL)),
        scheduler_(std::make_unique<TaskQueueFrameDecodeScheduler>(
            time_controller_.GetClock(),
            task_queue_.Get())) {}

  ~TaskQueueFrameDecodeSchedulerTest() override {
    if (scheduler_) {
      OnQueue([&] {
        scheduler_->Stop();
        scheduler_ = nullptr;
      });
    }
  }

  void FrameReadyForDecode(uint32_t timestamp, Timestamp render_time) {
    last_rtp_ = timestamp;
    last_render_time_ = render_time;
  }

 protected:
  void OnQueue(absl::AnyInvocable<void() &&> t) {
    task_queue_.PostTask(std::move(t));
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

  GlobalSimulatedTimeController time_controller_;
  rtc::TaskQueue task_queue_;
  std::unique_ptr<FrameDecodeScheduler> scheduler_;
  absl::optional<uint32_t> last_rtp_;
  absl::optional<Timestamp> last_render_time_;
};

TEST_F(TaskQueueFrameDecodeSchedulerTest, FrameYieldedAfterSpecifiedPeriod) {
  constexpr TimeDelta decode_delay = TimeDelta::Millis(5);
  const Timestamp now = time_controller_.GetClock()->CurrentTime();
  const uint32_t rtp = 90000;
  const Timestamp render_time = now + TimeDelta::Millis(15);
  FrameDecodeTiming::FrameSchedule schedule = {
      .latest_decode_time = now + decode_delay, .render_time = render_time};
  OnQueue([&] {
    scheduler_->ScheduleFrame(
        rtp, schedule,
        absl::bind_front(
            &TaskQueueFrameDecodeSchedulerTest::FrameReadyForDecode, this));
    EXPECT_THAT(scheduler_->ScheduledRtpTimestamp(), Optional(rtp));
  });
  EXPECT_THAT(last_rtp_, Eq(absl::nullopt));

  time_controller_.AdvanceTime(decode_delay);
  EXPECT_THAT(last_rtp_, Optional(rtp));
  EXPECT_THAT(last_render_time_, Optional(render_time));
}

TEST_F(TaskQueueFrameDecodeSchedulerTest, NegativeDecodeDelayIsRoundedToZero) {
  constexpr TimeDelta decode_delay = TimeDelta::Millis(-5);
  const Timestamp now = time_controller_.GetClock()->CurrentTime();
  const uint32_t rtp = 90000;
  const Timestamp render_time = now + TimeDelta::Millis(15);
  FrameDecodeTiming::FrameSchedule schedule = {
      .latest_decode_time = now + decode_delay, .render_time = render_time};
  OnQueue([&] {
    scheduler_->ScheduleFrame(
        rtp, schedule,
        absl::bind_front(
            &TaskQueueFrameDecodeSchedulerTest::FrameReadyForDecode, this));
    EXPECT_THAT(scheduler_->ScheduledRtpTimestamp(), Optional(rtp));
  });
  EXPECT_THAT(last_rtp_, Optional(rtp));
  EXPECT_THAT(last_render_time_, Optional(render_time));
}

TEST_F(TaskQueueFrameDecodeSchedulerTest, CancelOutstanding) {
  constexpr TimeDelta decode_delay = TimeDelta::Millis(50);
  const Timestamp now = time_controller_.GetClock()->CurrentTime();
  const uint32_t rtp = 90000;
  FrameDecodeTiming::FrameSchedule schedule = {
      .latest_decode_time = now + decode_delay,
      .render_time = now + TimeDelta::Millis(75)};
  OnQueue([&] {
    scheduler_->ScheduleFrame(
        rtp, schedule,
        absl::bind_front(
            &TaskQueueFrameDecodeSchedulerTest::FrameReadyForDecode, this));
    EXPECT_THAT(scheduler_->ScheduledRtpTimestamp(), Optional(rtp));
  });
  time_controller_.AdvanceTime(decode_delay / 2);
  OnQueue([&] {
    EXPECT_THAT(scheduler_->ScheduledRtpTimestamp(), Optional(rtp));
    scheduler_->CancelOutstanding();
    EXPECT_THAT(scheduler_->ScheduledRtpTimestamp(), Eq(absl::nullopt));
  });
  time_controller_.AdvanceTime(decode_delay / 2);
  EXPECT_THAT(last_rtp_, Eq(absl::nullopt));
  EXPECT_THAT(last_render_time_, Eq(absl::nullopt));
}

}  // namespace webrtc
