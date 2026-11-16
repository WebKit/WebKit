/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/clock_aligner.h"

#include "api/environment/environment.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/near_matcher.h"

namespace webrtc {
namespace {

using ::testing::Bool;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;

class ClockAlignerTest : public TestWithParam<bool> {
 protected:
  bool IsFixEnabled() const { return GetParam(); }

  Environment CreateEnvironment(Clock* clock) const {
    return CreateTestEnvironment(
        {.field_trials = IsFixEnabled() ? "WebRTC-ClockAligner/Enabled/"
                                        : "WebRTC-ClockAligner/Disabled/",
         .time = clock});
  }
};

// Occurs when the clock runs faster than WebRTC's monotonic clock,
// or when the system wall clock steps forward (e.g. NTP forward sync).
TEST_P(ClockAlignerTest, RatchetsDownWhenAheadOfClock) {
  SimulatedClock clock(Timestamp::Seconds(100));
  Environment env = CreateEnvironment(&clock);
  ClockAligner aligner(env);

  const Timestamp kEpoch = Timestamp::Seconds(10);

  // First packet establishes offset.
  EXPECT_EQ(aligner.Align(kEpoch), clock.CurrentTime());

  // Advance time by 10 ms. Socket indicates packet received 25 ms later.
  // Arrival time would be in the future, so offset ratchets down in both modes.
  clock.AdvanceTime(TimeDelta::Millis(10));
  EXPECT_EQ(aligner.Align(kEpoch + TimeDelta::Millis(25)), clock.CurrentTime());
}

// Occurs when NTP (or cellular NITZ / manual clock adjustments) steps the
// system wall clock backwards while packets are actively flowing.
TEST_P(ClockAlignerTest, RecoversFromBackwardClockStep) {
  SimulatedClock clock(Timestamp::Seconds(100));
  Environment env = CreateEnvironment(&clock);
  ClockAligner aligner(env);

  const Timestamp kEpoch = Timestamp::Seconds(10);

  // First packet establishes offset.
  EXPECT_EQ(aligner.Align(kEpoch), clock.CurrentTime());

  // Advance time by 20 ms. Timestamp steps backward by 450 ms (e.g. NTP
  // adjustment).
  clock.AdvanceTime(TimeDelta::Millis(20));
  Timestamp time = kEpoch + TimeDelta::Millis(20) - TimeDelta::Millis(450);

  if (IsFixEnabled()) {
    // With the fix enabled, a backward clock step triggers immediate
    // re-anchoring to current time.
    EXPECT_EQ(aligner.Align(time), clock.CurrentTime());
  } else {
    // In legacy behavior, the offset is not updated and the arrival time
    // latches 450 ms in the past.
    Timestamp arrival_time = aligner.Align(time);
    EXPECT_EQ(clock.CurrentTime() - arrival_time, TimeDelta::Millis(450));
  }
}

// Occurs when a burst of packets arrives at the network interface and is
// queued in the kernel receive buffer (SO_RCVBUF) while the WebRTC thread
// is busy or delayed.
TEST_P(ClockAlignerTest, PreservesBurstSpacing) {
  SimulatedClock clock(Timestamp::Seconds(100));
  Environment env = CreateEnvironment(&clock);
  ClockAligner aligner(env);

  const Timestamp kEpoch = Timestamp::Seconds(10);
  const Timestamp kStartTime = clock.CurrentTime();

  // First packet at t = 0 ms.
  EXPECT_EQ(aligner.Align(kEpoch), kStartTime);

  // Userspace thread was busy and wakes up at t = 30 ms to drain packets.
  clock.AdvanceTime(TimeDelta::Millis(30));

  // Packet 2 arrived at socket 5 ms after packet 1.
  Timestamp arrival_2 = aligner.Align(kEpoch + TimeDelta::Millis(5));
  EXPECT_THAT(arrival_2 - kStartTime, Near(TimeDelta::Millis(5)));

  // Packet 3 arrived at socket 15 ms after packet 1 (10 ms after packet 2),
  // read in same userspace turn without advancing clock.
  Timestamp arrival_3 = aligner.Align(kEpoch + TimeDelta::Millis(15));
  EXPECT_EQ(arrival_3 - arrival_2, TimeDelta::Millis(10));
}

// Occurs after periods with no traffic (e.g. audio mute, DTX/silence periods,
// network reconnects, or ICE candidate pair switching) during which the
// socket clock may have drifted or been adjusted.
TEST_P(ClockAlignerTest, ReanchorsAfterIdleGap) {
  SimulatedClock clock(Timestamp::Seconds(100));
  Environment env = CreateEnvironment(&clock);
  ClockAligner aligner(env);

  const Timestamp kEpoch = Timestamp::Seconds(10);

  // First packet establishes offset.
  EXPECT_EQ(aligner.Align(kEpoch), clock.CurrentTime());

  // Connection idle gap of 2 seconds.
  clock.AdvanceTime(TimeDelta::Seconds(2));

  // Socket timestamp advanced by only 1600 ms while monotonic time advanced by
  // 2000 ms.
  Timestamp time = kEpoch + TimeDelta::Millis(1600);

  if (IsFixEnabled()) {
    // Elapsed time exceeded the idle threshold; offset is re-anchored.
    EXPECT_EQ(aligner.Align(time), clock.CurrentTime());
  } else {
    // In legacy behavior, the offset is not re-anchored and arrival time lags
    // by 400 ms.
    Timestamp arrival_time = aligner.Align(time);
    EXPECT_EQ(clock.CurrentTime() - arrival_time, TimeDelta::Millis(400));
  }
}

// Occurs when the system clock steps backward during an inter-packet gap but
// the socket timestamp still advances, or when packets sit in OS receive
// buffers during long thread starvation or application suspension.
TEST_P(ClockAlignerTest, ReanchorsOnMaxStaleness) {
  SimulatedClock clock(Timestamp::Seconds(100));
  Environment env = CreateEnvironment(&clock);
  ClockAligner aligner(env);

  const Timestamp kEpoch = Timestamp::Seconds(10);

  // First packet establishes offset.
  EXPECT_EQ(aligner.Align(kEpoch), clock.CurrentTime());

  // Advance time by 700 ms.
  clock.AdvanceTime(TimeDelta::Millis(700));

  // Socket timestamp advanced by only 100 ms (arrival time lag is 600 ms).
  Timestamp time = kEpoch + TimeDelta::Millis(100);

  if (IsFixEnabled()) {
    // Lag exceeds staleness threshold; offset is re-anchored.
    EXPECT_EQ(aligner.Align(time), clock.CurrentTime());
  } else {
    // In legacy behavior, arrival time lags by 600 ms in the past.
    Timestamp arrival_time = aligner.Align(time);
    EXPECT_EQ(clock.CurrentTime() - arrival_time, TimeDelta::Millis(600));
  }
}

// Occurs when the socket clock oscillator runs slightly slower than the host
// monotonic clock (hardware clock frequency drift, typically tens or hundreds
// of ppm).
TEST_P(ClockAlignerTest, TracksPositiveClockDrift) {
  SimulatedClock clock(Timestamp::Seconds(100));
  Environment env = CreateEnvironment(&clock);
  ClockAligner aligner(env);

  const Timestamp kEpoch = Timestamp::Seconds(10);

  // First packet establishes offset (offset = 100s - 10s = 90s).
  EXPECT_EQ(aligner.Align(kEpoch), clock.CurrentTime());

  // Socket clock runs slightly slower: over 1000 ms real time, socket advanced
  // only 999 ms. Drift is 1 ms over 1000 ms = 1000 ppm.
  clock.AdvanceTime(TimeDelta::Millis(1000));
  Timestamp time = kEpoch + TimeDelta::Millis(999);

  if (IsFixEnabled()) {
    // Offset expands by 1 ms to track the clock drift.
    EXPECT_EQ(aligner.Align(time), clock.CurrentTime());
  } else {
    // Legacy behavior never increases offset; arrival time lags by 1 ms.
    Timestamp arrival_time = aligner.Align(time);
    EXPECT_EQ(clock.CurrentTime() - arrival_time, TimeDelta::Millis(1));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClockAlignerTest,
                         Bool(),
                         [](const TestParamInfo<bool>& info) {
                           return info.param ? "FixEnabled" : "FixDisabled";
                         });

}  // namespace
}  // namespace webrtc
