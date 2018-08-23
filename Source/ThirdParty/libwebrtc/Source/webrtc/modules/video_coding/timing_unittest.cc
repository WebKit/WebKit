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
#include "test/gtest.h"

namespace webrtc {
namespace {
const int kFps = 25;
}  // namespace

TEST(ReceiverTiming, Tests) {
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
      clock.TimeInMilliseconds());
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
      clock.TimeInMilliseconds());
  // Since we gradually increase the delay we only get 100 ms every second.
  EXPECT_EQ(jitter_delay_ms - 10, wait_time_ms);

  timestamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.UpdateCurrentDelay(timestamp);
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds());
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
      clock.TimeInMilliseconds());
  EXPECT_EQ(jitter_delay_ms, wait_time_ms);

  // Add decode time estimates for 1 second.
  const uint32_t kDecodeTimeMs = 10;
  for (int i = 0; i < kFps; i++) {
    clock.AdvanceTimeMilliseconds(kDecodeTimeMs);
    timing.StopDecodeTimer(
        timestamp, kDecodeTimeMs, clock.TimeInMilliseconds(),
        timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()));
    timestamp += 90000 / kFps;
    clock.AdvanceTimeMilliseconds(1000 / kFps - kDecodeTimeMs);
    timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
  }
  timing.UpdateCurrentDelay(timestamp);
  wait_time_ms = timing.MaxWaitingTime(
      timing.RenderTimeMs(timestamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds());
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
      clock.TimeInMilliseconds());
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

TEST(ReceiverTiming, WrapAround) {
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

}  // namespace webrtc
