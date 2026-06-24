/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/timing.h"

#include <cstdint>

#include "api/field_trials.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr Frequency k25Fps = Frequency::Hertz(25);
constexpr Frequency k90kHz = Frequency::KiloHertz(90);
constexpr TimeDelta kMinimumDelay = TimeDelta::Millis(100);
constexpr TimeDelta kDecodeTime = TimeDelta::Millis(20);
constexpr TimeDelta kRenderDelay = TimeDelta::Millis(15);
constexpr Timestamp kUnusedTimestamp = Timestamp::MinusInfinity();

MATCHER(HasConsistentVideoDelayTimings, "") {
  // Delays should be non-negative.
  bool p1 = arg.minimum_delay >= TimeDelta::Zero();
  bool p2 = arg.estimated_max_decode_time >= TimeDelta::Zero();
  bool p3 = arg.render_delay >= TimeDelta::Zero();
  bool p4 = arg.min_playout_delay >= TimeDelta::Zero();
  bool p5 = arg.max_playout_delay >= TimeDelta::Zero();
  bool p6 = arg.stats_target_delay >= TimeDelta::Zero();
  bool p7 = arg.current_delay >= TimeDelta::Zero();
  *result_listener << "\np: " << p1 << p2 << p3 << p4 << p5 << p6 << p7;
  bool p = p1 && p2 && p3 && p4 && p5 && p6 && p7;

  // Delays should be internally consistent.
  bool m1 = arg.minimum_delay <= arg.stats_target_delay;
  if (!m1) {
    *result_listener << "\nminimum_delay: " << ToString(arg.minimum_delay)
                     << ", " << "stats_target_delay: "
                     << ToString(arg.stats_target_delay) << "\n";
  }
  bool m2 = arg.minimum_delay <= arg.current_delay;
  if (!m2) {
    *result_listener << "\nminimum_delay: " << ToString(arg.minimum_delay)
                     << ", "
                     << "current_delay: " << ToString(arg.current_delay);
  }
  bool m3 = arg.stats_target_delay >= arg.min_playout_delay;
  if (!m3) {
    *result_listener << "\nstats_target_delay: "
                     << ToString(arg.stats_target_delay) << ", "
                     << "min_playout_delay: " << ToString(arg.min_playout_delay)
                     << "\n";
  }
  // TODO(crbug.com/webrtc/15197): Uncomment when this is guaranteed.
  // bool m4 = arg.stats_target_delay <= arg.max_playout_delay;
  bool m5 = arg.current_delay >= arg.min_playout_delay;
  if (!m5) {
    *result_listener << "\ncurrent_delay: " << ToString(arg.current_delay)
                     << ", "
                     << "min_playout_delay: " << ToString(arg.min_playout_delay)
                     << "\n";
  }
  bool m6 = arg.current_delay <= arg.max_playout_delay;
  if (!m6) {
    *result_listener << "\ncurrent_delay: " << ToString(arg.current_delay)
                     << ", "
                     << "max_playout_delay: " << ToString(arg.max_playout_delay)
                     << "\n";
  }
  bool m = m1 && m2 && m3 && m5 && m6;

  return p && m;
}

void UpdateDecodeTimer(VCMTiming& timing,
                       SimulatedClock& clock,
                       TimeDelta decode_time) {
  for (int i = 0; i < k25Fps.hertz(); ++i) {
    clock.AdvanceTime(decode_time);
    timing.StopDecodeTimer(decode_time, clock.CurrentTime());
    clock.AdvanceTime(1 / k25Fps - decode_time);
  }
}

TEST(VCMTimingTest, TimestampWrapAround) {
  constexpr auto kStartTime = Timestamp::Millis(1337);
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(kStartTime);
  VCMTiming timing(&clock, field_trials);

  // Provoke a wrap-around. The fifth frame will have wrapped at 25 fps.
  constexpr uint32_t kRtpTicksPerFrame = k90kHz / k25Fps;
  uint32_t timestamp = 0xFFFFFFFFu - 3 * kRtpTicksPerFrame;
  for (int i = 0; i < 5; ++i) {
    timing.OnCompleteTemporalUnit(timestamp, clock.CurrentTime());
    clock.AdvanceTime(1 / k25Fps);
    timestamp += kRtpTicksPerFrame;
    EXPECT_EQ(kStartTime + 3 / k25Fps,
              timing.RenderTime(0xFFFFFFFFu, clock.CurrentTime()));
    // One ms later in 90 kHz.
    EXPECT_EQ(kStartTime + 3 / k25Fps + TimeDelta::Millis(1),
              timing.RenderTime(89u, clock.CurrentTime()));
  }

  EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, UseLowLatencyRenderer) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  // Default is false.
  EXPECT_FALSE(timing.RenderParameters().use_low_latency_rendering);
  // False if min playout delay > 0.
  timing.set_playout_delay({TimeDelta::Millis(10), TimeDelta::Millis(20)});
  EXPECT_FALSE(timing.RenderParameters().use_low_latency_rendering);
  // True if min==0, max > 0.
  timing.set_playout_delay({TimeDelta::Zero(), TimeDelta::Millis(20)});
  EXPECT_TRUE(timing.RenderParameters().use_low_latency_rendering);
  // True if min==max==0.
  timing.set_playout_delay({TimeDelta::Zero(), TimeDelta::Zero()});
  EXPECT_TRUE(timing.RenderParameters().use_low_latency_rendering);
  // True also for max playout delay==500 ms.
  timing.set_playout_delay({TimeDelta::Zero(), TimeDelta::Millis(500)});
  EXPECT_TRUE(timing.RenderParameters().use_low_latency_rendering);
  // False if max playout delay > 500 ms.
  timing.set_playout_delay({TimeDelta::Zero(), TimeDelta::Millis(501)});
  EXPECT_FALSE(timing.RenderParameters().use_low_latency_rendering);

  EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, UpdateCurrentDelayCapsWhenOffByMicroseconds) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);

  // Set larger initial current delay.
  timing.set_min_playout_delay(TimeDelta::Millis(200));
  timing.UpdateCurrentDelay(Timestamp::Millis(900), Timestamp::Millis(1000));

  // Add a few microseconds to ensure that the delta of decode time is 0 after
  // rounding, and should reset to the target delay.
  timing.set_min_playout_delay(TimeDelta::Millis(50));
  Timestamp decode_time = Timestamp::Millis(1337);
  Timestamp render_time =
      decode_time + TimeDelta::Millis(10) + TimeDelta::Micros(37);
  timing.UpdateCurrentDelay(render_time, decode_time);
  EXPECT_EQ(timing.GetTimings().current_delay, timing.TargetVideoDelay());

  // TODO(crbug.com/webrtc/15197): Fix this.
  // EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, StopDecodeTimerClearsOldEstimates) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);

  UpdateDecodeTimer(timing, clock, kDecodeTime);
  EXPECT_EQ(timing.GetTimings().estimated_max_decode_time, kDecodeTime);

  // Advance time beyond the filter window, old estimates should be cleared.
  clock.AdvanceTime(TimeDelta::Seconds(30));
  timing.StopDecodeTimer(TimeDelta::Millis(3), clock.CurrentTime());
  EXPECT_EQ(timing.GetTimings().estimated_max_decode_time,
            TimeDelta::Millis(3));
}

TEST(VCMTimingTest, GetMinPlayoutDelay) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);

  timing.set_min_playout_delay(TimeDelta::Millis(123));
  EXPECT_EQ(timing.min_playout_delay(), TimeDelta::Millis(123));
}

TEST(VCMTimingTest, InitialVideoDelayTimings) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);

  VCMTiming::VideoDelayTimings timings = timing.GetTimings();
  EXPECT_EQ(timings.num_decoded_frames, 0u);
  EXPECT_EQ(timings.minimum_delay, TimeDelta::Zero());
  EXPECT_EQ(timings.estimated_max_decode_time, TimeDelta::Zero());
  EXPECT_EQ(timings.render_delay,
            VCMTiming::VideoDelayTimings::kDefaultRenderDelay);
  EXPECT_EQ(timings.min_playout_delay, TimeDelta::Zero());
  EXPECT_EQ(timings.stats_target_delay, TimeDelta::Zero());
  EXPECT_EQ(timings.current_delay, TimeDelta::Zero());
  EXPECT_THAT(timings, HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, GetTimings) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(33);
  VCMTiming timing(&clock, field_trials);

  // Setup.
  TimeDelta render_delay = TimeDelta::Millis(11);
  timing.set_render_delay(render_delay);
  TimeDelta min_playout_delay = TimeDelta::Millis(50);
  TimeDelta max_playout_delay = TimeDelta::Millis(500);
  timing.set_playout_delay({min_playout_delay, max_playout_delay});

  // On complete.
  timing.OnCompleteTemporalUnit(3000, clock.CurrentTime());
  clock.AdvanceTimeMilliseconds(1);

  // On decodable.
  Timestamp render_time =
      timing.RenderTime(/*next_temporal_unit_rtp=*/3000, clock.CurrentTime());
  TimeDelta minimum_delay = TimeDelta::Millis(123);
  timing.SetMinimumDelay(minimum_delay);
  timing.UpdateCurrentDelay(render_time, clock.CurrentTime());
  clock.AdvanceTimeMilliseconds(100);

  // On decoded.
  UpdateDecodeTimer(timing, clock, kDecodeTime);

  VCMTiming::VideoDelayTimings timings = timing.GetTimings();
  EXPECT_GT(timings.num_decoded_frames, 0u);
  EXPECT_EQ(timings.minimum_delay, minimum_delay);
  EXPECT_EQ(timings.estimated_max_decode_time, kDecodeTime);
  EXPECT_EQ(timings.render_delay, render_delay);
  EXPECT_EQ(timings.min_playout_delay, min_playout_delay);
  EXPECT_EQ(timings.max_playout_delay, max_playout_delay);
  EXPECT_EQ(timings.stats_target_delay, minimum_delay);
  EXPECT_EQ(timings.current_delay, minimum_delay);
  EXPECT_THAT(timings, HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, Reset) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(Timestamp::Millis(33));
  VCMTiming timing(&clock, field_trials);

  timing.set_render_delay(TimeDelta::Millis(11));
  TimeDelta min_playout_delay = TimeDelta::Millis(50);
  TimeDelta max_playout_delay = TimeDelta::Millis(500);
  timing.set_playout_delay({min_playout_delay, max_playout_delay});

  // On complete.
  timing.OnCompleteTemporalUnit(3000, clock.CurrentTime());

  // On decodable.
  Timestamp render_time = timing.RenderTime(3000, clock.CurrentTime());
  timing.SetMinimumDelay(TimeDelta::Millis(123));
  timing.UpdateCurrentDelay(render_time, clock.CurrentTime());

  // On decoded.
  UpdateDecodeTimer(timing, clock, kDecodeTime);

  timing.Reset();

  VCMTiming::VideoDelayTimings timings = timing.GetTimings();
  EXPECT_GT(timings.num_decoded_frames, 0u);
  EXPECT_EQ(timings.minimum_delay, TimeDelta::Zero());
  EXPECT_EQ(timings.estimated_max_decode_time, TimeDelta::Zero());
  EXPECT_EQ(timings.render_delay,
            VCMTiming::VideoDelayTimings::kDefaultRenderDelay);
  EXPECT_EQ(timings.min_playout_delay, TimeDelta::Zero());
  EXPECT_EQ(timings.max_playout_delay, max_playout_delay);
  EXPECT_EQ(timings.stats_target_delay, TimeDelta::Zero());
  EXPECT_EQ(timings.current_delay, TimeDelta::Zero());
  EXPECT_THAT(timings, HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, GetTimingsBeforeAndAfterValidRtpTimestamp) {
  SimulatedClock clock(33);
  FieldTrials field_trials = CreateTestFieldTrials();
  VCMTiming timing(&clock, field_trials);

  // Setup.
  TimeDelta min_playout_delay = TimeDelta::Millis(50);
  timing.set_playout_delay({min_playout_delay, TimeDelta::Millis(500)});

  // On decodable frames before valid rtp timestamp.
  constexpr int decodable_frame_cnt = 10;
  constexpr uint32_t any_time_elapsed = 17;
  constexpr uint32_t rtp_ts_base = 3000;
  constexpr uint32_t rtp_ts_delta_10fps = 9000;
  constexpr uint32_t frame_ts_delta_10fps = 100;
  uint32_t rtp_ts = rtp_ts_base;

  for (int i = 0; i < decodable_frame_cnt; i++) {
    clock.AdvanceTimeMilliseconds(any_time_elapsed);
    rtp_ts += rtp_ts_delta_10fps;

    Timestamp render_time = timing.RenderTime(rtp_ts, clock.CurrentTime());
    // Render time should be CurrentTime, because timing.OnCompleteTemporalUnit
    // has not been called yet.
    EXPECT_EQ(render_time, clock.CurrentTime());
  }

  // On frame complete, which one not 'metadata.delayed_by_retransmission'
  Timestamp valid_frame_ts = clock.CurrentTime();
  timing.OnCompleteTemporalUnit(rtp_ts, valid_frame_ts);

  clock.AdvanceTimeMilliseconds(any_time_elapsed);
  rtp_ts += rtp_ts_delta_10fps;

  Timestamp render_time = timing.RenderTime(rtp_ts, clock.CurrentTime());
  // Render time should be relative to the latest valid frame timestamp.
  EXPECT_EQ(render_time, valid_frame_ts +
                             TimeDelta::Millis(frame_ts_delta_10fps) +
                             min_playout_delay);
}

TEST(VCMTimingTest, RenderTimeAccountsForCurrentDelay) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(Timestamp::Millis(88));
  VCMTiming timing(&clock, field_trials);

  timing.set_playout_delay({TimeDelta::Millis(100), TimeDelta::Millis(200)});
  timing.OnCompleteTemporalUnit(/*rtp_timestamp=*/0, clock.CurrentTime());
  // Current delay is initialized to minimum delay.
  timing.SetMinimumDelay(TimeDelta::Millis(123));

  EXPECT_EQ(timing.RenderTime(/*rtp_timestamp=*/0, kUnusedTimestamp),
            clock.CurrentTime() + TimeDelta::Millis(123));
}

TEST(VCMTimingTest, RenderTimeRespectsMinPlayoutDelay) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(Timestamp::Millis(88));
  VCMTiming timing(&clock, field_trials);

  timing.set_playout_delay({TimeDelta::Millis(100), TimeDelta::Millis(200)});
  timing.OnCompleteTemporalUnit(/*rtp_timestamp=*/0, clock.CurrentTime());
  // Current delay is initialized to minimum delay.
  timing.SetMinimumDelay(TimeDelta::Millis(90));

  EXPECT_EQ(timing.RenderTime(/*rtp_timestamp=*/0, kUnusedTimestamp),
            clock.CurrentTime() + TimeDelta::Millis(100));
}

TEST(VCMTimingTest, RenderTimeRespectsMaxPlayoutDelay) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(Timestamp::Millis(88));
  VCMTiming timing(&clock, field_trials);

  timing.set_playout_delay({TimeDelta::Millis(100), TimeDelta::Millis(200)});
  timing.OnCompleteTemporalUnit(/*rtp_timestamp=*/0, clock.CurrentTime());
  // Current delay is initialized to minimum delay.
  timing.SetMinimumDelay(TimeDelta::Millis(210));

  EXPECT_EQ(timing.RenderTime(/*rtp_timestamp=*/0, kUnusedTimestamp),
            clock.CurrentTime() + TimeDelta::Millis(200));
}

TEST(VCMTimingTest, IncreasesCurrentDelayWhenFrameIsLate) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.SetMinimumDelay(kMinimumDelay);
  timing.set_render_delay(kRenderDelay);

  // Current delay is initialized to minimum delay.
  EXPECT_EQ(timing.GetTimings().current_delay, kMinimumDelay);
  EXPECT_EQ(timing.TargetVideoDelay(), kMinimumDelay + kRenderDelay);

  const TimeDelta kFrameDelay = TimeDelta::Millis(4);
  // Current delay should be increased to get closer to target delay.
  Timestamp render_time = clock.CurrentTime() + kRenderDelay;
  Timestamp actual_decode_time = clock.CurrentTime() + kFrameDelay;
  timing.UpdateCurrentDelay(render_time, actual_decode_time);

  EXPECT_EQ(timing.GetTimings().current_delay, kMinimumDelay + kFrameDelay);
}

TEST(VCMTimingTest, CapsCurrentDelayIncreaseToTarget) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.SetMinimumDelay(kMinimumDelay);
  timing.set_render_delay(kRenderDelay);

  // Current delay is initialized to minimum delay.
  EXPECT_EQ(timing.GetTimings().current_delay, kMinimumDelay);
  EXPECT_EQ(timing.TargetVideoDelay(), kMinimumDelay + kRenderDelay);

  const TimeDelta kFrameDelay = TimeDelta::Millis(588);
  // Current delay should be increased but not exceed target delay.
  Timestamp render_time = clock.CurrentTime() + kRenderDelay;
  Timestamp actual_decode_time = clock.CurrentTime() + kFrameDelay;
  timing.UpdateCurrentDelay(render_time, actual_decode_time);

  EXPECT_EQ(timing.GetTimings().current_delay, kMinimumDelay + kRenderDelay);
}

TEST(VCMTimingTest, KeepsCurrentDelayWhenFrameIsEarly) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.SetMinimumDelay(kMinimumDelay);
  timing.set_render_delay(kRenderDelay);

  // Current delay is initialized to minimum delay.
  EXPECT_EQ(timing.GetTimings().current_delay, kMinimumDelay);
  EXPECT_EQ(timing.TargetVideoDelay(), kMinimumDelay + kRenderDelay);

  // Frame is early.
  // Delay should remain unchanged.
  Timestamp render_time = clock.CurrentTime() + kRenderDelay * 2;
  Timestamp actual_decode_time = clock.CurrentTime();
  timing.UpdateCurrentDelay(render_time, actual_decode_time);

  EXPECT_EQ(timing.GetTimings().current_delay, kMinimumDelay);
}

TEST(VCMTimingTest, IncreasesCurrentDelayWhenFrameIsLateWithDecodeTime) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.SetMinimumDelay(kMinimumDelay);
  timing.set_render_delay(kRenderDelay);
  UpdateDecodeTimer(timing, clock, kDecodeTime);

  // Current delay is initialized to minimum delay.
  EXPECT_EQ(timing.GetTimings().current_delay, kMinimumDelay);
  EXPECT_EQ(timing.TargetVideoDelay(),
            kMinimumDelay + kDecodeTime + kRenderDelay);

  const TimeDelta kFrameDelay = TimeDelta::Millis(4);
  // Current delay should be increased to get closer to target delay.
  Timestamp render_time = clock.CurrentTime() + kDecodeTime + kRenderDelay;
  Timestamp actual_decode_time = clock.CurrentTime() + kFrameDelay;
  timing.UpdateCurrentDelay(render_time, actual_decode_time);

  EXPECT_EQ(timing.GetTimings().current_delay, kMinimumDelay + kFrameDelay);
}

TEST(VCMTimingTest, DecreasesCurrentDelayToTarget) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.SetMinimumDelay(kMinimumDelay);
  timing.set_render_delay(kRenderDelay);

  // Current delay should be increased to target for late frame.
  timing.UpdateCurrentDelay(clock.CurrentTime(),
                            clock.CurrentTime() + TimeDelta::Millis(588));
  EXPECT_EQ(timing.GetTimings().current_delay, timing.TargetVideoDelay());

  // Reduce minimum delay.
  timing.SetMinimumDelay(kMinimumDelay / 2);
  EXPECT_EQ(timing.TargetVideoDelay(), kMinimumDelay / 2 + kRenderDelay);

  // Current delay should be decreased to new target for frame on-time.
  timing.UpdateCurrentDelay(clock.CurrentTime() + kRenderDelay,
                            clock.CurrentTime());
  EXPECT_EQ(timing.GetTimings().current_delay,
            kMinimumDelay / 2 + kRenderDelay);
}

TEST(VCMTimingTest, MinPlayoutDelayUpdatesTargetDelay) {
  FieldTrials field_trials = CreateTestFieldTrials();
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.SetMinimumDelay(kMinimumDelay);
  timing.set_render_delay(kRenderDelay);

  const TimeDelta kMinPlayout =
      kMinimumDelay + kRenderDelay + TimeDelta::Millis(50);
  timing.set_min_playout_delay(kMinPlayout);

  EXPECT_EQ(timing.TargetVideoDelay(), kMinPlayout);
}

}  // namespace
}  // namespace webrtc
