/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/internal_defines.h"
#include "webrtc/modules/video_coding/test/test_util.h"
#include "webrtc/modules/video_coding/timing.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

TEST(ReceiverTiming, Tests) {
  SimulatedClock clock(0);
  VCMTiming timing(&clock);
  uint32_t waitTime = 0;
  uint32_t jitterDelayMs = 0;
  uint32_t requiredDecodeTimeMs = 0;
  uint32_t timeStamp = 0;

  timing.Reset();

  timing.UpdateCurrentDelay(timeStamp);

  timing.Reset();

  timing.IncomingTimestamp(timeStamp, clock.TimeInMilliseconds());
  jitterDelayMs = 20;
  timing.SetJitterDelay(jitterDelayMs);
  timing.UpdateCurrentDelay(timeStamp);
  timing.set_render_delay(0);
  waitTime = timing.MaxWaitingTime(
      timing.RenderTimeMs(timeStamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds());
  // First update initializes the render time. Since we have no decode delay
  // we get waitTime = renderTime - now - renderDelay = jitter.
  EXPECT_EQ(jitterDelayMs, waitTime);

  jitterDelayMs += VCMTiming::kDelayMaxChangeMsPerS + 10;
  timeStamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.SetJitterDelay(jitterDelayMs);
  timing.UpdateCurrentDelay(timeStamp);
  waitTime = timing.MaxWaitingTime(
      timing.RenderTimeMs(timeStamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds());
  // Since we gradually increase the delay we only get 100 ms every second.
  EXPECT_EQ(jitterDelayMs - 10, waitTime);

  timeStamp += 90000;
  clock.AdvanceTimeMilliseconds(1000);
  timing.UpdateCurrentDelay(timeStamp);
  waitTime = timing.MaxWaitingTime(
      timing.RenderTimeMs(timeStamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds());
  EXPECT_EQ(waitTime, jitterDelayMs);

  // 300 incoming frames without jitter, verify that this gives the exact wait
  // time.
  for (int i = 0; i < 300; i++) {
    clock.AdvanceTimeMilliseconds(1000 / 25);
    timeStamp += 90000 / 25;
    timing.IncomingTimestamp(timeStamp, clock.TimeInMilliseconds());
  }
  timing.UpdateCurrentDelay(timeStamp);
  waitTime = timing.MaxWaitingTime(
      timing.RenderTimeMs(timeStamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds());
  EXPECT_EQ(waitTime, jitterDelayMs);

  // Add decode time estimates.
  for (int i = 0; i < 10; i++) {
    int64_t startTimeMs = clock.TimeInMilliseconds();
    clock.AdvanceTimeMilliseconds(10);
    timing.StopDecodeTimer(
        timeStamp, clock.TimeInMilliseconds() - startTimeMs,
        clock.TimeInMilliseconds(),
        timing.RenderTimeMs(timeStamp, clock.TimeInMilliseconds()));
    timeStamp += 90000 / 25;
    clock.AdvanceTimeMilliseconds(1000 / 25 - 10);
    timing.IncomingTimestamp(timeStamp, clock.TimeInMilliseconds());
  }
  requiredDecodeTimeMs = 10;
  timing.SetJitterDelay(jitterDelayMs);
  clock.AdvanceTimeMilliseconds(1000);
  timeStamp += 90000;
  timing.UpdateCurrentDelay(timeStamp);
  waitTime = timing.MaxWaitingTime(
      timing.RenderTimeMs(timeStamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds());
  EXPECT_EQ(waitTime, jitterDelayMs);

  int minTotalDelayMs = 200;
  timing.set_min_playout_delay(minTotalDelayMs);
  clock.AdvanceTimeMilliseconds(5000);
  timeStamp += 5 * 90000;
  timing.UpdateCurrentDelay(timeStamp);
  const int kRenderDelayMs = 10;
  timing.set_render_delay(kRenderDelayMs);
  waitTime = timing.MaxWaitingTime(
      timing.RenderTimeMs(timeStamp, clock.TimeInMilliseconds()),
      clock.TimeInMilliseconds());
  // We should at least have minTotalDelayMs - decodeTime (10) - renderTime
  // (10) to wait.
  EXPECT_EQ(waitTime, minTotalDelayMs - requiredDecodeTimeMs - kRenderDelayMs);
  // The total video delay should be equal to the min total delay.
  EXPECT_EQ(minTotalDelayMs, timing.TargetVideoDelay());

  // Reset playout delay.
  timing.set_min_playout_delay(0);
  clock.AdvanceTimeMilliseconds(5000);
  timeStamp += 5 * 90000;
  timing.UpdateCurrentDelay(timeStamp);
}

TEST(ReceiverTiming, WrapAround) {
  const int kFramerate = 25;
  SimulatedClock clock(0);
  VCMTiming timing(&clock);
  // Provoke a wrap-around. The forth frame will have wrapped at 25 fps.
  uint32_t timestamp = 0xFFFFFFFFu - 3 * 90000 / kFramerate;
  for (int i = 0; i < 4; ++i) {
    timing.IncomingTimestamp(timestamp, clock.TimeInMilliseconds());
    clock.AdvanceTimeMilliseconds(1000 / kFramerate);
    timestamp += 90000 / kFramerate;
    int64_t render_time =
        timing.RenderTimeMs(0xFFFFFFFFu, clock.TimeInMilliseconds());
    EXPECT_EQ(3 * 1000 / kFramerate, render_time);
    render_time = timing.RenderTimeMs(89u,  // One second later in 90 kHz.
                                      clock.TimeInMilliseconds());
    EXPECT_EQ(3 * 1000 / kFramerate + 1, render_time);
  }
}

}  // namespace webrtc
