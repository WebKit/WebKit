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

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {

constexpr Frequency k25Fps = Frequency::Hertz(25);
constexpr Frequency k90kHz = Frequency::KiloHertz(90);

MATCHER(HasConsistentVideoDelayTimings, "") {
  // Delays should be non-negative.
  bool p1 = arg.minimum_delay >= TimeDelta::Zero();
  bool p2 = arg.estimated_max_decode_time >= TimeDelta::Zero();
  bool p3 = arg.render_delay >= TimeDelta::Zero();
  bool p4 = arg.min_playout_delay >= TimeDelta::Zero();
  bool p5 = arg.max_playout_delay >= TimeDelta::Zero();
  bool p6 = arg.target_delay >= TimeDelta::Zero();
  bool p7 = arg.current_delay >= TimeDelta::Zero();
  *result_listener << "\np: " << p1 << p2 << p3 << p4 << p5 << p6 << p7;
  bool p = p1 && p2 && p3 && p4 && p5 && p6 && p7;

  // Delays should be internally consistent.
  bool m1 = arg.minimum_delay <= arg.target_delay;
  if (!m1) {
    *result_listener << "\nminimum_delay: " << ToString(arg.minimum_delay)
                     << ", " << "target_delay: " << ToString(arg.target_delay)
                     << "\n";
  }
  bool m2 = arg.minimum_delay <= arg.current_delay;
  if (!m2) {
    *result_listener << "\nminimum_delay: " << ToString(arg.minimum_delay)
                     << ", "
                     << "current_delay: " << ToString(arg.current_delay);
  }
  bool m3 = arg.target_delay >= arg.min_playout_delay;
  if (!m3) {
    *result_listener << "\ntarget_delay: " << ToString(arg.target_delay) << ", "
                     << "min_playout_delay: " << ToString(arg.min_playout_delay)
                     << "\n";
  }
  // TODO(crbug.com/webrtc/15197): Uncomment when this is guaranteed.
  // bool m4 = arg.target_delay <= arg.max_playout_delay;
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

}  // namespace

TEST(VCMTimingTest, JitterDelay) {
  test::ScopedKeyValueConfig field_trials;
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.Reset();

  uint32_t timestamp = 0;
  timing.UpdateCurrentDelay(timestamp);

  timing.Reset();

  timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  TimeDelta jitter_delay = TimeDelta::Millis(20);
  timing.SetJitterDelay(jitter_delay);
  timing.UpdateCurrentDelay(timestamp);
  timing.set_render_delay(TimeDelta::Zero());
  auto wait_time = timing.MaxWaitingTime(
      timing.RenderTime(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  // First update initializes the render time. Since we have no decode delay
  // we get wait_time = renderTime - now - renderDelay = jitter.
  EXPECT_EQ(jitter_delay, wait_time);

  jitter_delay += TimeDelta::Millis(VCMTiming::kDelayMaxChangeMsPerS + 10);
  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.SetJitterDelay(jitter_delay);
  timing.UpdateCurrentDelay(timestamp);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTime(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  // Since we gradually increase the delay we only get 100 ms every second.
  EXPECT_EQ(jitter_delay - TimeDelta::Millis(10), wait_time);

  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.UpdateCurrentDelay(timestamp);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTime(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay, wait_time);

  // Insert frames without jitter, verify that this gives the exact wait time.
  const int kNumFrames = 300;
  for (int i = 0; i < kNumFrames; i++) {
    clock.AdvanceTime(1 / k25Fps);
    timestamp += k90kHz / k25Fps;
    timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  }
  timing.UpdateCurrentDelay(timestamp);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTime(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay, wait_time);

  // Add decode time estimates for 1 second.
  const TimeDelta kDecodeTime = TimeDelta::Millis(10);
  for (int i = 0; i < k25Fps.hertz(); i++) {
    clock.AdvanceTime(kDecodeTime);
    timing.StopDecodeTimer(kDecodeTime, clock.CurrentTime());
    timestamp += k90kHz / k25Fps;
    clock.AdvanceTime(1 / k25Fps - kDecodeTime);
    timing.IncomingTimestamp(timestamp, clock.CurrentTime());
  }
  timing.UpdateCurrentDelay(timestamp);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTime(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay, wait_time);

  const TimeDelta kMinTotalDelay = TimeDelta::Millis(200);
  timing.set_min_playout_delay(kMinTotalDelay);
  clock.AdvanceTimeMilliseconds(5000);
  timestamp += 5 * 90000;
  timing.UpdateCurrentDelay(timestamp);
  const TimeDelta kRenderDelay = TimeDelta::Millis(10);
  timing.set_render_delay(kRenderDelay);
  wait_time = timing.MaxWaitingTime(
      timing.RenderTime(timestamp, clock.CurrentTime()), clock.CurrentTime(),
      /*too_many_frames_queued=*/false);
  // We should at least have kMinTotalDelayMs - decodeTime (10) - renderTime
  // (10) to wait.
  EXPECT_EQ(kMinTotalDelay - kDecodeTime - kRenderDelay, wait_time);
  // The total video delay should be equal to the min total delay.
  EXPECT_EQ(kMinTotalDelay, timing.TargetVideoDelay());

  // Reset playout delay.
  timing.set_min_playout_delay(TimeDelta::Zero());
  clock.AdvanceTimeMilliseconds(5000);
  timestamp += 5 * 90000;
  timing.UpdateCurrentDelay(timestamp);

  EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, TimestampWrapAround) {
  constexpr auto kStartTime = Timestamp::Millis(1337);
  test::ScopedKeyValueConfig field_trials;
  SimulatedClock clock(kStartTime);
  VCMTiming timing(&clock, field_trials);

  // Provoke a wrap-around. The fifth frame will have wrapped at 25 fps.
  constexpr uint32_t kRtpTicksPerFrame = k90kHz / k25Fps;
  uint32_t timestamp = 0xFFFFFFFFu - 3 * kRtpTicksPerFrame;
  for (int i = 0; i < 5; ++i) {
    timing.IncomingTimestamp(timestamp, clock.CurrentTime());
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
  test::ScopedKeyValueConfig field_trials;
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.Reset();
  // Default is false.
  EXPECT_FALSE(timing.RenderParameters().use_low_latency_rendering);
  // False if min playout delay > 0.
  timing.set_min_playout_delay(TimeDelta::Millis(10));
  timing.set_max_playout_delay(TimeDelta::Millis(20));
  EXPECT_FALSE(timing.RenderParameters().use_low_latency_rendering);
  // True if min==0, max > 0.
  timing.set_min_playout_delay(TimeDelta::Zero());
  EXPECT_TRUE(timing.RenderParameters().use_low_latency_rendering);
  // True if min==max==0.
  timing.set_max_playout_delay(TimeDelta::Zero());
  EXPECT_TRUE(timing.RenderParameters().use_low_latency_rendering);
  // True also for max playout delay==500 ms.
  timing.set_max_playout_delay(TimeDelta::Millis(500));
  EXPECT_TRUE(timing.RenderParameters().use_low_latency_rendering);
  // False if max playout delay > 500 ms.
  timing.set_max_playout_delay(TimeDelta::Millis(501));
  EXPECT_FALSE(timing.RenderParameters().use_low_latency_rendering);

  EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, MaxWaitingTimeIsZeroForZeroRenderTime) {
  // This is the default path when the RTP playout delay header extension is set
  // to min==0 and max==0.
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr TimeDelta kTimeDelta = 1 / Frequency::Hertz(60);
  constexpr Timestamp kZeroRenderTime = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  test::ScopedKeyValueConfig field_trials;
  VCMTiming timing(&clock, field_trials);
  timing.Reset();
  timing.set_max_playout_delay(TimeDelta::Zero());
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTime(kTimeDelta);
    Timestamp now = clock.CurrentTime();
    EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTime, now,
                                    /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
  }
  // Another frame submitted at the same time also returns a negative max
  // waiting time.
  Timestamp now = clock.CurrentTime();
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  // MaxWaitingTime should be less than zero even if there's a burst of frames.
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            TimeDelta::Zero());
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            TimeDelta::Zero());

  EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, MaxWaitingTimeZeroDelayPacingExperiment) {
  // The minimum pacing is enabled by a field trial and active if the RTP
  // playout delay header extension is set to min==0.
  constexpr TimeDelta kMinPacing = TimeDelta::Millis(3);
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr TimeDelta kTimeDelta = 1 / Frequency::Hertz(60);
  constexpr auto kZeroRenderTime = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock, field_trials);
  timing.Reset();
  // MaxWaitingTime() returns zero for evenly spaced video frames.
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTime(kTimeDelta);
    Timestamp now = clock.CurrentTime();
    EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                    /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
    timing.SetLastDecodeScheduledTimestamp(now);
  }
  // Another frame submitted at the same time is paced according to the field
  // trial setting.
  auto now = clock.CurrentTime();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacing);
  // If there's a burst of frames, the wait time is calculated based on next
  // decode time.
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacing);
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacing);
  // Allow a few ms to pass, this should be subtracted from the MaxWaitingTime.
  constexpr TimeDelta kTwoMs = TimeDelta::Millis(2);
  clock.AdvanceTime(kTwoMs);
  now = clock.CurrentTime();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacing - kTwoMs);
  // A frame is decoded at the current time, the wait time should be restored to
  // pacing delay.
  timing.SetLastDecodeScheduledTimestamp(now);
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                  /*too_many_frames_queued=*/false),
            kMinPacing);

  EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, DefaultMaxWaitingTimeUnaffectedByPacingExperiment) {
  // The minimum pacing is enabled by a field trial but should not have any
  // effect if render_time_ms is greater than 0;
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  const TimeDelta kTimeDelta = TimeDelta::Millis(1000.0 / 60.0);
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock, field_trials);
  timing.Reset();
  clock.AdvanceTime(kTimeDelta);
  auto now = clock.CurrentTime();
  Timestamp render_time = now + TimeDelta::Millis(30);
  // Estimate the internal processing delay from the first frame.
  TimeDelta estimated_processing_delay =
      (render_time - now) -
      timing.MaxWaitingTime(render_time, now,
                            /*too_many_frames_queued=*/false);
  EXPECT_GT(estimated_processing_delay, TimeDelta::Zero());

  // Any other frame submitted at the same time should be scheduled according to
  // its render time.
  for (int i = 0; i < 5; ++i) {
    render_time += kTimeDelta;
    EXPECT_EQ(timing.MaxWaitingTime(render_time, now,
                                    /*too_many_frames_queued=*/false),
              render_time - now - estimated_processing_delay);
  }

  EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, MaxWaitingTimeReturnsZeroIfTooManyFramesQueuedIsTrue) {
  // The minimum pacing is enabled by a field trial and active if the RTP
  // playout delay header extension is set to min==0.
  constexpr TimeDelta kMinPacing = TimeDelta::Millis(3);
  test::ScopedKeyValueConfig field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  const TimeDelta kTimeDelta = TimeDelta::Millis(1000.0 / 60.0);
  constexpr auto kZeroRenderTime = Timestamp::Zero();
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock, field_trials);
  timing.Reset();
  // MaxWaitingTime() returns zero for evenly spaced video frames.
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTime(kTimeDelta);
    auto now = clock.CurrentTime();
    EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now,
                                    /*too_many_frames_queued=*/false),
              TimeDelta::Zero());
    timing.SetLastDecodeScheduledTimestamp(now);
  }
  // Another frame submitted at the same time is paced according to the field
  // trial setting.
  auto now_ms = clock.CurrentTime();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                  /*too_many_frames_queued=*/false),
            kMinPacing);
  // MaxWaitingTime returns 0 even if there's a burst of frames if
  // too_many_frames_queued is set to true.
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                  /*too_many_frames_queued=*/true),
            TimeDelta::Zero());
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTime, now_ms,
                                  /*too_many_frames_queued=*/true),
            TimeDelta::Zero());

  EXPECT_THAT(timing.GetTimings(), HasConsistentVideoDelayTimings());
}

TEST(VCMTimingTest, UpdateCurrentDelayCapsWhenOffByMicroseconds) {
  test::ScopedKeyValueConfig field_trials;
  SimulatedClock clock(0);
  VCMTiming timing(&clock, field_trials);
  timing.Reset();

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

TEST(VCMTimingTest, GetTimings) {
  test::ScopedKeyValueConfig field_trials;
  SimulatedClock clock(33);
  VCMTiming timing(&clock, field_trials);
  timing.Reset();

  // Setup.
  TimeDelta render_delay = TimeDelta::Millis(11);
  timing.set_render_delay(render_delay);
  TimeDelta min_playout_delay = TimeDelta::Millis(50);
  timing.set_min_playout_delay(min_playout_delay);
  TimeDelta max_playout_delay = TimeDelta::Millis(500);
  timing.set_max_playout_delay(max_playout_delay);

  // On complete.
  timing.IncomingTimestamp(3000, clock.CurrentTime());
  clock.AdvanceTimeMilliseconds(1);

  // On decodable.
  Timestamp render_time =
      timing.RenderTime(/*next_temporal_unit_rtp=*/3000, clock.CurrentTime());
  TimeDelta minimum_delay = TimeDelta::Millis(123);
  timing.SetJitterDelay(minimum_delay);
  timing.UpdateCurrentDelay(render_time, clock.CurrentTime());
  clock.AdvanceTimeMilliseconds(100);

  // On decoded.
  TimeDelta decode_time = TimeDelta::Millis(4);
  timing.StopDecodeTimer(decode_time, clock.CurrentTime());

  VCMTiming::VideoDelayTimings timings = timing.GetTimings();
  EXPECT_EQ(timings.num_decoded_frames, 1u);
  EXPECT_EQ(timings.minimum_delay, minimum_delay);
  // A single decoded frame is not enough to calculate p95.
  EXPECT_EQ(timings.estimated_max_decode_time, TimeDelta::Zero());
  EXPECT_EQ(timings.render_delay, render_delay);
  EXPECT_EQ(timings.min_playout_delay, min_playout_delay);
  EXPECT_EQ(timings.max_playout_delay, max_playout_delay);
  EXPECT_EQ(timings.target_delay, minimum_delay);
  EXPECT_EQ(timings.current_delay, minimum_delay);
  EXPECT_THAT(timings, HasConsistentVideoDelayTimings());
}

}  // namespace webrtc
