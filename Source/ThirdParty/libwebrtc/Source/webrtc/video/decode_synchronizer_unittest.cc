/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/decode_synchronizer.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "api/metronome/test/fake_metronome.h"
#include "api/units/time_delta.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"
#include "video/frame_decode_scheduler.h"
#include "video/frame_decode_timing.h"

using ::testing::_;
using ::testing::Eq;

namespace webrtc {

class DecodeSynchronizerTest : public ::testing::Test {
 public:
  static constexpr TimeDelta kTickPeriod = TimeDelta::Millis(33);

  DecodeSynchronizerTest()
      : time_controller_(Timestamp::Millis(1337)),
        clock_(time_controller_.GetClock()),
        metronome_(kTickPeriod),
        decode_synchronizer_(clock_,
                             &metronome_,
                             time_controller_.GetMainThread()) {}

 protected:
  GlobalSimulatedTimeController time_controller_;
  Clock* clock_;
  test::ForcedTickMetronome metronome_;
  DecodeSynchronizer decode_synchronizer_;
};

TEST_F(DecodeSynchronizerTest, AllFramesReadyBeforeNextTickDecoded) {
  ::testing::MockFunction<void(uint32_t, Timestamp)> mock_callback1;
  auto scheduler1 = decode_synchronizer_.CreateSynchronizedFrameScheduler();

  testing::MockFunction<void(unsigned int, Timestamp)> mock_callback2;
  auto scheduler2 = decode_synchronizer_.CreateSynchronizedFrameScheduler();

  {
    uint32_t frame_rtp = 90000;
    FrameDecodeTiming::FrameSchedule frame_sched{
        .latest_decode_time =
            clock_->CurrentTime() + kTickPeriod - TimeDelta::Millis(3),
        .render_time = clock_->CurrentTime() + TimeDelta::Millis(60)};
    scheduler1->ScheduleFrame(frame_rtp, frame_sched,
                              mock_callback1.AsStdFunction());
    EXPECT_CALL(mock_callback1,
                Call(Eq(frame_rtp), Eq(frame_sched.render_time)));
  }
  {
    uint32_t frame_rtp = 123456;
    FrameDecodeTiming::FrameSchedule frame_sched{
        .latest_decode_time =
            clock_->CurrentTime() + kTickPeriod - TimeDelta::Millis(2),
        .render_time = clock_->CurrentTime() + TimeDelta::Millis(70)};
    scheduler2->ScheduleFrame(frame_rtp, frame_sched,
                              mock_callback2.AsStdFunction());
    EXPECT_CALL(mock_callback2,
                Call(Eq(frame_rtp), Eq(frame_sched.render_time)));
  }
  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Cleanup
  scheduler1->Stop();
  scheduler2->Stop();
}

TEST_F(DecodeSynchronizerTest, FramesNotDecodedIfDecodeTimeIsInNextInterval) {
  ::testing::MockFunction<void(unsigned int, Timestamp)> mock_callback;
  auto scheduler = decode_synchronizer_.CreateSynchronizedFrameScheduler();

  uint32_t frame_rtp = 90000;
  FrameDecodeTiming::FrameSchedule frame_sched{
      .latest_decode_time =
          clock_->CurrentTime() + kTickPeriod + TimeDelta::Millis(10),
      .render_time =
          clock_->CurrentTime() + kTickPeriod + TimeDelta::Millis(30)};
  scheduler->ScheduleFrame(frame_rtp, frame_sched,
                           mock_callback.AsStdFunction());

  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());
  // No decodes should have happened in this tick.
  ::testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Decode should happen on next tick.
  EXPECT_CALL(mock_callback, Call(Eq(frame_rtp), Eq(frame_sched.render_time)));
  time_controller_.AdvanceTime(kTickPeriod);
  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Cleanup
  scheduler->Stop();
}

TEST_F(DecodeSynchronizerTest, FrameDecodedOnce) {
  ::testing::MockFunction<void(unsigned int, Timestamp)> mock_callback;
  auto scheduler = decode_synchronizer_.CreateSynchronizedFrameScheduler();

  uint32_t frame_rtp = 90000;
  FrameDecodeTiming::FrameSchedule frame_sched{
      .latest_decode_time = clock_->CurrentTime() + TimeDelta::Millis(30),
      .render_time = clock_->CurrentTime() + TimeDelta::Millis(60)};
  scheduler->ScheduleFrame(frame_rtp, frame_sched,
                           mock_callback.AsStdFunction());
  EXPECT_CALL(mock_callback, Call(_, _)).Times(1);
  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());
  ::testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Trigger tick again. No frame should be decoded now.
  time_controller_.AdvanceTime(kTickPeriod);
  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Cleanup
  scheduler->Stop();
}

TEST_F(DecodeSynchronizerTest, FrameWithDecodeTimeInPastDecodedImmediately) {
  ::testing::MockFunction<void(unsigned int, Timestamp)> mock_callback;
  auto scheduler = decode_synchronizer_.CreateSynchronizedFrameScheduler();

  uint32_t frame_rtp = 90000;
  FrameDecodeTiming::FrameSchedule frame_sched{
      .latest_decode_time = clock_->CurrentTime() - TimeDelta::Millis(5),
      .render_time = clock_->CurrentTime() + TimeDelta::Millis(30)};
  EXPECT_CALL(mock_callback, Call(Eq(90000u), _)).Times(1);
  scheduler->ScheduleFrame(frame_rtp, frame_sched,
                           mock_callback.AsStdFunction());
  // Verify the callback was invoked already.
  ::testing::Mock::VerifyAndClearExpectations(&mock_callback);

  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Cleanup
  scheduler->Stop();
}

TEST_F(DecodeSynchronizerTest,
       FrameWithDecodeTimeFarBeforeNextTickDecodedImmediately) {
  ::testing::MockFunction<void(unsigned int, Timestamp)> mock_callback;
  auto scheduler = decode_synchronizer_.CreateSynchronizedFrameScheduler();

  // Frame which would be behind by more than kMaxAllowedFrameDelay after
  // the next tick.
  FrameDecodeTiming::FrameSchedule frame_sched{
      .latest_decode_time = clock_->CurrentTime() + kTickPeriod -
                            FrameDecodeTiming::kMaxAllowedFrameDelay -
                            TimeDelta::Millis(1),
      .render_time = clock_->CurrentTime() + TimeDelta::Millis(30)};
  EXPECT_CALL(mock_callback, Call(Eq(90000u), _)).Times(1);
  scheduler->ScheduleFrame(90000, frame_sched, mock_callback.AsStdFunction());
  // Verify the callback was invoked already.
  ::testing::Mock::VerifyAndClearExpectations(&mock_callback);

  time_controller_.AdvanceTime(kTickPeriod);
  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // A frame that would be behind by exactly kMaxAllowedFrameDelay after next
  // tick should decode at the next tick.
  FrameDecodeTiming::FrameSchedule queued_frame{
      .latest_decode_time = clock_->CurrentTime() + kTickPeriod -
                            FrameDecodeTiming::kMaxAllowedFrameDelay,
      .render_time = clock_->CurrentTime() + TimeDelta::Millis(30)};
  scheduler->ScheduleFrame(180000, queued_frame, mock_callback.AsStdFunction());
  // Verify the callback was invoked already.
  ::testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback, Call(Eq(180000u), _)).Times(1);
  time_controller_.AdvanceTime(kTickPeriod);
  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());

  // Cleanup
  scheduler->Stop();
}

TEST_F(DecodeSynchronizerTest, FramesNotReleasedAfterStop) {
  ::testing::MockFunction<void(unsigned int, Timestamp)> mock_callback;
  auto scheduler = decode_synchronizer_.CreateSynchronizedFrameScheduler();

  uint32_t frame_rtp = 90000;
  FrameDecodeTiming::FrameSchedule frame_sched{
      .latest_decode_time = clock_->CurrentTime() + TimeDelta::Millis(30),
      .render_time = clock_->CurrentTime() + TimeDelta::Millis(60)};
  scheduler->ScheduleFrame(frame_rtp, frame_sched,
                           mock_callback.AsStdFunction());
  // Cleanup
  scheduler->Stop();

  // No callback should occur on this tick since Stop() was called before.
  metronome_.Tick();
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_F(DecodeSynchronizerTest, MetronomeNotListenedWhenNoStreamsAreActive) {
  EXPECT_EQ(0u, metronome_.NumListeners());

  auto scheduler = decode_synchronizer_.CreateSynchronizedFrameScheduler();
  EXPECT_EQ(1u, metronome_.NumListeners());
  auto scheduler2 = decode_synchronizer_.CreateSynchronizedFrameScheduler();
  EXPECT_EQ(1u, metronome_.NumListeners());

  scheduler->Stop();
  EXPECT_EQ(1u, metronome_.NumListeners());
  scheduler2->Stop();
  EXPECT_EQ(0u, metronome_.NumListeners());
}

}  // namespace webrtc
