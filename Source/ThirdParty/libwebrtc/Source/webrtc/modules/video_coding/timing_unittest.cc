/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing.h"

#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const int kFps = 25;
}  // namespace

TEST(ReceiverTimingTest, JitterDelay) {
  SimulatedClock clock(0);
  VCMTiming timing(&clock);
  timing.Reset();

  uint32_t timestamp = 0;
  timing.UpdateCurrentDelay(timestamp);

  timing.Reset();

  timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
  uint32_t jitter_delay_ms = 20;
  timing.SetJitterDelay(jitter_delay_ms);
  timing.UpdateCurrentDelay(timestamp);
  timing.set_render_delay(0);
  uint32_t wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds(), /*too_many_frames_queued=*/false);
  // First update initializes the render time. Since we have no decode delay
  // we get wait_time_ms = renderTime - now - renderDelay = jitter.
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  jitter_delay_ms += VCMTiming::kDelayMaxChangeMsPerS + 10;
  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.SetJitterDelay(jitter_delay_ms);
  timing.UpdateCurrentDelay(timestamp);
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds(), /*too_many_frames_queued=*/false);
  // Since we gradually increase the delay we only get 100 ms every second.
  EXPECT_EQ(jitter_delay_ms - 10, wait_time_ms);

  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.UpdateCurrentDelay(timestamp);
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds(), /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  // Insert frames without jitter, verify that this gives the exact wait time.
  const int kNumFrames = 300;
  for (int i = 0; i < kNumFrames; i++) {
    clock.AdvanceTimeMilliseconds(1000 / kFps);
    timestamp += 90000 / kFps;
    timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
  }
  timing.UpdateCurrentDelay(timestamp);
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds(), /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  // Add decode time estimates for 1 second.
  const uint32_t kDecodeTimeMs = 10;
  for (int i = 0; i < kFps; i++) {
    clock.AdvanceTimeMilliseconds(kDecodeTimeMs);
    timing.StopDecodeTimer(kDecodeTimeMs, clock.TimeInMilliseconds());
    timestamp += 90000 / kFps;
    clock.AdvanceTimeMilliseconds(1000 / kFps - kDecodeTimeMs);
    timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
  }
  timing.UpdateCurrentDelay(timestamp);
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds(), /*too_many_frames_queued=*/false);
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  const int kMinTotalDelayMs = 200;
  timing.set_min_playout_delay(kMinTotalDelayMs);
  clock.AdvanceTimeMilliseconds(5000);
  timestamp += 5 * 90000;
  timing.UpdateCurrentDelay(timestamp);
  const int kRenderDelayMs = 10;
  timing.set_render_delay(kRenderDelayMs);
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds(), /*too_many_frames_queued=*/false);
  // We should at least have kMinTotalDelayMs - decodeTime (10) - renderTime
  // (10) to wait.
  EXPECT_EQ(kMinTotalDelayMs - kDecodeTimeMs - kRenderDelayMs, wait_time_ms);
  // The total video delay should be equal to the min total delay.
  EXPECT_EQ(kMinTotalDelayMs, timing.TargetVideoDelay());

  // Reset playout delay.
  timing.set_min_playout_delay(0);
  clock.AdvanceTimeMilliseconds(5000);
  timestamp += 5 * 90000;
  timing.UpdateCurrentDelay(timestamp);
}

TEST(ReceiverTimingTest, TimestampWrapAround) {
  SimulatedClock clock(0);
  VCMTiming timing(&clock);
  // Provoke a wrap-around. The fifth frame will have wrapped at 25 fps.
  uint32_t timestamp = 0xFFFFFFFFu - 3 * 90000 / kFps;
  for (int i = 0; i < 5; ++i) {
    timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
    clock.AdvanceTimeMilliseconds(1000 / kFps);
    timestamp += 90000 / kFps;
    EXPECT_EQ(3 * 1000 / kFps,
              timing.RenderTimeMs(0xFFFFFFFFu, clock.TimeInMilliseconds()));
    EXPECT_EQ(3 * 1000 / kFps + 1,
              timing.RenderTimeMs(89u,  // One ms later in 90 kHz.
                                  clock.TimeInMilliseconds()));
  }
}

TEST(ReceiverTimingTest, MaxWaitingTimeIsZeroForZeroRenderTime) {
  // This is the default path when the RTP playout delay header extension is set
  // to min==0 and max==0.
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
  constexpr int64_t kZeroRenderTimeMs = 0;
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock);
  timing.Reset();
  timing.set_max_playout_delay(0);
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTimeMilliseconds(kTimeDeltaMs);
    int64_t now_ms = clock.TimeInMilliseconds();
    EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                    /*too_many_frames_queued=*/false),
              0);
  }
  // Another frame submitted at the same time also returns a negative max
  // waiting time.
  int64_t now_ms = clock.TimeInMilliseconds();
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            0);
  // MaxWaitingTime should be less than zero even if there's a burst of frames.
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            0);
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            0);
  EXPECT_LT(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            0);
}

TEST(ReceiverTimingTest, MaxWaitingTimeZeroDelayPacingExperiment) {
  // The minimum pacing is enabled by a field trial and active if the RTP
  // playout delay header extension is set to min==0.
  constexpr int64_t kMinPacingMs = 3;
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
  constexpr int64_t kZeroRenderTimeMs = 0;
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock);
  timing.Reset();
  // MaxWaitingTime() returns zero for evenly spaced video frames.
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTimeMilliseconds(kTimeDeltaMs);
    int64_t now_ms = clock.TimeInMilliseconds();
    EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                    /*too_many_frames_queued=*/false),
              0);
    timing.SetLastDecodeScheduledTimestamp(now_ms);
  }
  // Another frame submitted at the same time is paced according to the field
  // trial setting.
  int64_t now_ms = clock.TimeInMilliseconds();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
  // If there's a burst of frames, the wait time is calculated based on next
  // decode time.
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
  // Allow a few ms to pass, this should be subtracted from the MaxWaitingTime.
  constexpr int64_t kTwoMs = 2;
  clock.AdvanceTimeMilliseconds(kTwoMs);
  now_ms = clock.TimeInMilliseconds();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs - kTwoMs);
  // A frame is decoded at the current time, the wait time should be restored to
  // pacing delay.
  timing.SetLastDecodeScheduledTimestamp(now_ms);
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
}

TEST(ReceiverTimingTest, DefaultMaxWaitingTimeUnaffectedByPacingExperiment) {
  // The minimum pacing is enabled by a field trial but should not have any
  // effect if render_time_ms is greater than 0;
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock);
  timing.Reset();
  clock.AdvanceTimeMilliseconds(kTimeDeltaMs);
  int64_t now_ms = clock.TimeInMilliseconds();
  int64_t render_time_ms = now_ms + 30;
  // Estimate the internal processing delay from the first frame.
  int64_t estimated_processing_delay =
      (render_time_ms - now_ms) -
      timing.MaxWaitingTime(render_time_ms, now_ms,
                            /*too_many_frames_queued=*/false);
  EXPECT_GT(estimated_processing_delay, 0);

  // Any other frame submitted at the same time should be scheduled according to
  // its render time.
  for (int i = 0; i < 5; ++i) {
    render_time_ms += kTimeDeltaMs;
    EXPECT_EQ(timing.MaxWaitingTime(render_time_ms, now_ms,
                                    /*too_many_frames_queued=*/false),
              render_time_ms - now_ms - estimated_processing_delay);
  }
}

TEST(ReceiverTiminTest, MaxWaitingTimeReturnsZeroIfTooManyFramesQueuedIsTrue) {
  // The minimum pacing is enabled by a field trial and active if the RTP
  // playout delay header extension is set to min==0.
  constexpr int64_t kMinPacingMs = 3;
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-ZeroPlayoutDelay/min_pacing:3ms/");
  constexpr int64_t kStartTimeUs = 3.15e13;  // About one year in us.
  constexpr int64_t kTimeDeltaMs = 1000.0 / 60.0;
  constexpr int64_t kZeroRenderTimeMs = 0;
  SimulatedClock clock(kStartTimeUs);
  VCMTiming timing(&clock);
  timing.Reset();
  // MaxWaitingTime() returns zero for evenly spaced video frames.
  for (int i = 0; i < 10; ++i) {
    clock.AdvanceTimeMilliseconds(kTimeDeltaMs);
    int64_t now_ms = clock.TimeInMilliseconds();
    EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                    /*too_many_frames_queued=*/false),
              0);
    timing.SetLastDecodeScheduledTimestamp(now_ms);
  }
  // Another frame submitted at the same time is paced according to the field
  // trial setting.
  int64_t now_ms = clock.TimeInMilliseconds();
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/false),
            kMinPacingMs);
  // MaxWaitingTime returns 0 even if there's a burst of frames if
  // too_many_frames_queued is set to true.
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/true),
            0);
  EXPECT_EQ(timing.MaxWaitingTime(kZeroRenderTimeMs, now_ms,
                                  /*too_many_frames_queued=*/true),
            0);
}

}  // namespace webrtc
