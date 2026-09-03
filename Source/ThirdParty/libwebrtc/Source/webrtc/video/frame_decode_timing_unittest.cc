/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_decode_timing.h"

#include <cstdint>
#include <optional>

#include "api/field_trials.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/video_coding/timing/timing.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/video_receive_stream2.h"

namespace webrtc {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;

namespace {

constexpr uint32_t kNextRtp = 90000;
constexpr uint32_t kLastRtp = 180000;
constexpr Frequency k25Fps = Frequency::Hertz(25);
constexpr TimeDelta kDecodeTime = TimeDelta::Millis(20);
constexpr TimeDelta kRenderDelay = TimeDelta::Millis(15);

void UpdateDecodeTimer(VCMTiming& timing,
                       SimulatedClock& clock,
                       TimeDelta decode_time) {
  for (int i = 0; i < k25Fps.hertz(); ++i) {
    clock.AdvanceTime(decode_time);
    timing.UpdateDecodeTimeEstimate(decode_time, clock.CurrentTime());
    clock.AdvanceTime(1 / k25Fps - decode_time);
  }
}

class FrameDecodeTimingTest : public ::testing::Test {
 public:
  FrameDecodeTimingTest()
      : field_trials_(CreateTestFieldTrials()),
        clock_(Timestamp::Millis(1000)),
        timing_(&clock_, field_trials_, /*render_delay=*/TimeDelta::Zero()),
        frame_decode_scheduler_(&clock_, &timing_, field_trials_) {
    timing_.OnCompleteFrame({.rtp_timestamp = kNextRtp,
                             .time = clock_.CurrentTime(),
                             .last_spatial_layer = true});
  }

 protected:
  FieldTrials field_trials_;
  SimulatedClock clock_;
  VCMTiming timing_;
  FrameDecodeTiming frame_decode_scheduler_;
};

TEST_F(FrameDecodeTimingTest, ReturnsWaitTimesWhenValid) {
  const TimeDelta kDecodeDelay = TimeDelta::Millis(42);
  timing_.SetMinimumDelay(kDecodeDelay);

  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  kNextRtp, kLastRtp, kMaxWaitForFrame, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime() + kDecodeDelay)),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(clock_.CurrentTime() + kDecodeDelay)))));
}

TEST_F(FrameDecodeTimingTest, FastForwardsFrameTooFarInThePast) {
  const TimeDelta kDecodeDelay =
      -FrameDecodeTiming::kMaxAllowedFrameDelay - TimeDelta::Millis(1);
  timing_.SetMinimumDelay(kDecodeDelay);
  timing_.set_min_playout_delay(kDecodeDelay);

  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  kNextRtp, kLastRtp, kMaxWaitForFrame, false),
              Eq(std::nullopt));
}

TEST_F(FrameDecodeTimingTest, NoFastForwardIfOnlyFrameToDecode) {
  const TimeDelta kDecodeDelay =
      -FrameDecodeTiming::kMaxAllowedFrameDelay - TimeDelta::Millis(1);
  timing_.SetMinimumDelay(kDecodeDelay);
  timing_.set_min_playout_delay(kDecodeDelay);

  // Negative `kDecodeDelay` means that `latest_decode_time` is now.
  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  kNextRtp, kNextRtp, kMaxWaitForFrame, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime())),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(clock_.CurrentTime() + kDecodeDelay)))));
}

TEST_F(FrameDecodeTimingTest, MaxWaitCapped) {
  const TimeDelta kDecodeDelay = kMaxWaitForFrame * 2;
  timing_.SetMinimumDelay(kDecodeDelay);

  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  kNextRtp, kLastRtp, kMaxWaitForFrame, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime() + kMaxWaitForFrame)),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(clock_.CurrentTime() + kDecodeDelay)))));
}

TEST_F(FrameDecodeTimingTest, MaxWaitCappedForKey) {
  const TimeDelta kDecodeDelay = kMaxWaitForKeyFrame * 2;
  timing_.SetMinimumDelay(kDecodeDelay);

  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  kNextRtp, kLastRtp, kMaxWaitForKeyFrame, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime() + kMaxWaitForKeyFrame)),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(clock_.CurrentTime() + kDecodeDelay)))));
}

TEST(FrameDecodeTimingMaxWaitingTimeTest, IsZeroForZeroRenderTime) {
  // This is the default path when the RTP playout delay header extension is set
  // to min==0 and max==0.
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr TimeDelta kTimeDelta = 1 / Frequency::Hertz(60);
  constexpr Timestamp kZeroRenderTime = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  FieldTrials field_trials = CreateTestFieldTrials();
  VCMTiming timing(&clock, field_trials, kRenderDelay);
  timing.set_playout_delay({TimeDelta::Zero(), TimeDelta::Zero()});
  FrameDecodeTiming decode_timing(&clock, &timing, field_trials);

  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTime(kTimeDelta);
    Timestamp now = clock.CurrentTime();
    EXPECT_LT(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                           /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
  }
  // Another frame submitted at the same time also returns a negative max
  // waiting time.
  Timestamp now = clock.CurrentTime();
  EXPECT_LT(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  // MaxWaitingTime should be less than zero even if there's a burst of frames.
  EXPECT_LT(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  EXPECT_LT(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  EXPECT_LT(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
}

TEST(FrameDecodeTimingMaxWaitingTimeTest, WithZeroDelayPacingActive) {
  // The minimum pacing is enabled by a field trial and active if the RTP
  // playout delay header extension is set to min==0.
  constexpr TimeDelta kMinPacing = TimeDelta::Millis(3);
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr TimeDelta kTimeDelta = 1 / Frequency::Hertz(60);
  constexpr Timestamp kZeroRenderTime = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock, field_trials, kRenderDelay);
  FrameDecodeTiming decode_timing(&clock, &timing, field_trials);

  // MaxWaitingTime() returns zero for evenly spaced video frames.
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTime(kTimeDelta);
    Timestamp now = clock.CurrentTime();
    EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                           /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
    decode_timing.SetLastDecodeScheduledTimestamp(now);
  }
  // Another frame submitted at the same time is paced according to the field
  // trial setting.
  Timestamp now = clock.CurrentTime();
  EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            kMinPacing);
  // If there's a burst of frames, the wait time is calculated based on next
  // decode time.
  EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            kMinPacing);
  EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            kMinPacing);
  // Allow a few ms to pass, this should be subtracted from the MaxWaitingTime.
  constexpr TimeDelta kTwoMs = TimeDelta::Millis(2);
  clock.AdvanceTime(kTwoMs);
  now = clock.CurrentTime();
  EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            kMinPacing - kTwoMs);
  // A frame is decoded at the current time, the wait time should be restored to
  // pacing delay.
  decode_timing.SetLastDecodeScheduledTimestamp(now);
  EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                         /*too_many_frames_queued=*/false),
            kMinPacing);
}

TEST(FrameDecodeTimingMaxWaitingTimeTest,
     DefaultMaxWaitingTimeUnaffectedByPacingExperiment) {
  // The minimum pacing is enabled by a field trial but should not have any
  // effect if render_time is greater than 0;
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  const TimeDelta kTimeDelta = TimeDelta::Millis(1000.0 / 60.0);
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock, field_trials, kRenderDelay);
  FrameDecodeTiming decode_timing(&clock, &timing, field_trials);

  clock.AdvanceTime(kTimeDelta);
  Timestamp now = clock.CurrentTime();
  Timestamp render_time = now + TimeDelta::Millis(30);
  // Estimate the internal processing delay from the first frame.
  TimeDelta estimated_processing_delay =
      (render_time - now) -
      decode_timing.MaxWaitingTime(render_time, now,
                                   /*too_many_frames_queued=*/false);
  EXPECT_GT(estimated_processing_delay, TimeDelta::Zero());

  // Any other frame submitted at the same time should be scheduled according to
  // its render time.
  for (int i = 0; i < 5; ++i) {
    render_time += kTimeDelta;
    EXPECT_EQ(decode_timing.MaxWaitingTime(render_time, now,
                                           /*too_many_frames_queued=*/false),
              render_time - now - estimated_processing_delay);
  }
}

TEST(FrameDecodeTimingMaxWaitingTimeTest, ReturnsZeroIfTooManyFramesAreQueued) {
  // The minimum pacing is enabled by a field trial and active if the RTP
  // playout delay header extension is set to min==0.
  constexpr TimeDelta kMinPacing = TimeDelta::Millis(3);
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  const TimeDelta kTimeDelta = TimeDelta::Millis(1000.0 / 60.0);
  constexpr Timestamp kZeroRenderTime = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock, field_trials, kRenderDelay);
  FrameDecodeTiming decode_timing(&clock, &timing, field_trials);

  // MaxWaitingTime() returns zero for evenly spaced video frames.
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTime(kTimeDelta);
    Timestamp now = clock.CurrentTime();
    EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now,
                                           /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
    decode_timing.SetLastDecodeScheduledTimestamp(now);
  }
  // Another frame submitted at the same time is paced according to the field
  // trial setting.
  Timestamp now_ms = clock.CurrentTime();
  EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                         /*too_many_frames_queued=*/false),
            kMinPacing);
  // MaxWaitingTime returns 0 even if there's a burst of frames if
  // too_many_frames_queued is set to true.
  EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                         /*too_many_frames_queued=*/true),
            TimeDelta::Zero());
  EXPECT_EQ(decode_timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                         /*too_many_frames_queued=*/true),
            TimeDelta::Zero());
}

TEST(FrameDecodeTimingMaxWaitingTimeTest, WithVaryingRenderTimes) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials, kRenderDelay);
  UpdateDecodeTimer(timing, clock, kDecodeTime);
  FrameDecodeTiming decode_timing(&clock, &timing, field_trials);

  Timestamp on_time = clock.CurrentTime() + kDecodeTime + kRenderDelay;

  // Early frame.
  Timestamp render_time = on_time + TimeDelta::Millis(1);
  EXPECT_EQ(decode_timing.MaxWaitingTime(render_time, clock.CurrentTime(),
                                         /*too_many_frames_queued=*/false),
            TimeDelta::Millis(1));

  // Exactly on time.
  render_time = on_time;
  EXPECT_EQ(decode_timing.MaxWaitingTime(render_time, clock.CurrentTime(),
                                         /*too_many_frames_queued=*/false),
            TimeDelta::Zero());

  // Late frame.
  render_time = on_time - TimeDelta::Millis(1);
  EXPECT_EQ(decode_timing.MaxWaitingTime(render_time, clock.CurrentTime(),
                                         /*too_many_frames_queued=*/false),
            TimeDelta::Millis(-1));
}

}  // namespace
}  // namespace webrtc
