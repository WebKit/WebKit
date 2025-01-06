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
#include <optional>
#include <utility>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

using ::testing::_;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::Optional;

TEST(TaskQueueFrameDecodeSchedulerTest, FrameYieldedAfterSpecifiedPeriod) {
  GlobalSimulatedTimeController time_controller_(Timestamp::Seconds(2000));
  TaskQueueFrameDecodeScheduler scheduler(time_controller_.GetClock(),
                                          time_controller_.GetMainThread());
  constexpr TimeDelta decode_delay = TimeDelta::Millis(5);

  const Timestamp now = time_controller_.GetClock()->CurrentTime();
  const uint32_t rtp = 90000;
  const Timestamp render_time = now + TimeDelta::Millis(15);
  FrameDecodeTiming::FrameSchedule schedule = {
      .latest_decode_time = now + decode_delay, .render_time = render_time};

  MockFunction<void(uint32_t, Timestamp)> ready_cb;
  scheduler.ScheduleFrame(rtp, schedule, ready_cb.AsStdFunction());
  EXPECT_CALL(ready_cb, Call(_, _)).Times(0);
  EXPECT_THAT(scheduler.ScheduledRtpTimestamp(), Optional(rtp));
  time_controller_.AdvanceTime(TimeDelta::Zero());
  // Check that `ready_cb` has not been invoked yet.
  ::testing::Mock::VerifyAndClearExpectations(&ready_cb);

  EXPECT_CALL(ready_cb, Call(rtp, render_time)).Times(1);
  time_controller_.AdvanceTime(decode_delay);

  scheduler.Stop();
}

TEST(TaskQueueFrameDecodeSchedulerTest, NegativeDecodeDelayIsRoundedToZero) {
  GlobalSimulatedTimeController time_controller_(Timestamp::Seconds(2000));
  TaskQueueFrameDecodeScheduler scheduler(time_controller_.GetClock(),
                                          time_controller_.GetMainThread());
  constexpr TimeDelta decode_delay = TimeDelta::Millis(-5);
  const Timestamp now = time_controller_.GetClock()->CurrentTime();
  const uint32_t rtp = 90000;
  const Timestamp render_time = now + TimeDelta::Millis(15);
  FrameDecodeTiming::FrameSchedule schedule = {
      .latest_decode_time = now + decode_delay, .render_time = render_time};

  MockFunction<void(uint32_t, Timestamp)> ready_cb;
  EXPECT_CALL(ready_cb, Call(rtp, render_time)).Times(1);
  scheduler.ScheduleFrame(rtp, schedule, ready_cb.AsStdFunction());
  EXPECT_THAT(scheduler.ScheduledRtpTimestamp(), Optional(rtp));
  time_controller_.AdvanceTime(TimeDelta::Zero());

  scheduler.Stop();
}

TEST(TaskQueueFrameDecodeSchedulerTest, CancelOutstanding) {
  GlobalSimulatedTimeController time_controller_(Timestamp::Seconds(2000));
  TaskQueueFrameDecodeScheduler scheduler(time_controller_.GetClock(),
                                          time_controller_.GetMainThread());
  constexpr TimeDelta decode_delay = TimeDelta::Millis(50);
  const Timestamp now = time_controller_.GetClock()->CurrentTime();
  const uint32_t rtp = 90000;
  FrameDecodeTiming::FrameSchedule schedule = {
      .latest_decode_time = now + decode_delay,
      .render_time = now + TimeDelta::Millis(75)};

  MockFunction<void(uint32_t, Timestamp)> ready_cb;
  EXPECT_CALL(ready_cb, Call).Times(0);
  scheduler.ScheduleFrame(rtp, schedule, ready_cb.AsStdFunction());
  EXPECT_THAT(scheduler.ScheduledRtpTimestamp(), Optional(rtp));
  time_controller_.AdvanceTime(decode_delay / 2);
  EXPECT_THAT(scheduler.ScheduledRtpTimestamp(), Optional(rtp));
  scheduler.CancelOutstanding();
  EXPECT_THAT(scheduler.ScheduledRtpTimestamp(), Eq(std::nullopt));
  time_controller_.AdvanceTime(decode_delay / 2);

  scheduler.Stop();
}

}  // namespace webrtc
