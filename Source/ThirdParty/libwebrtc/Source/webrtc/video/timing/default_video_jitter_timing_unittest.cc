/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/default_video_jitter_timing.h"

#include <cstdint>
#include <optional>

#include "api/environment/environment.h"
#include "api/field_trials.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr uint32_t kRtpTimestamp = 12345;
constexpr Timestamp kInitialTime = Timestamp::Millis(789);
constexpr DataSize kFrameSize = DataSize::Bytes(1000);
constexpr TimeDelta kRtt = TimeDelta::Millis(100);

TEST(DefaultVideoJitterTimingTest, LocalTimeReturnsNulloptInitially) {
  SimulatedClock clock(kInitialTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  DefaultVideoJitterTiming timing(env);

  EXPECT_EQ(timing.LocalTime(kRtpTimestamp), std::nullopt);
}

TEST(DefaultVideoJitterTimingTest, LocalTimeReturnsTimeAfterUpdate) {
  SimulatedClock clock(kInitialTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  DefaultVideoJitterTiming timing(env);

  timing.OnCompleteFrame({.rtp_timestamp = kRtpTimestamp,
                          .time = clock.CurrentTime(),
                          .last_spatial_layer = true,
                          .was_retransmitted = false});
  EXPECT_EQ(timing.LocalTime(kRtpTimestamp), clock.CurrentTime());
}

TEST(DefaultVideoJitterTimingTest, LocalTimeReturnsNulloptAfterReset) {
  SimulatedClock clock(kInitialTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  DefaultVideoJitterTiming timing(env);

  timing.OnCompleteFrame({.rtp_timestamp = kRtpTimestamp,
                          .time = clock.CurrentTime(),
                          .last_spatial_layer = true,
                          .was_retransmitted = false});
  EXPECT_EQ(timing.LocalTime(kRtpTimestamp), clock.CurrentTime());

  timing.Reset();
  EXPECT_EQ(timing.LocalTime(kRtpTimestamp), std::nullopt);
}

TEST(DefaultVideoJitterTimingTest, LocalTimeNotUpdatedByRetransmittedFrame) {
  SimulatedClock clock(kInitialTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  DefaultVideoJitterTiming timing(env);

  timing.OnCompleteFrame({.rtp_timestamp = kRtpTimestamp,
                          .time = clock.CurrentTime(),
                          .last_spatial_layer = true,
                          .was_retransmitted = true});
  EXPECT_EQ(timing.LocalTime(kRtpTimestamp), std::nullopt);
}

TEST(DefaultVideoJitterTimingTest, LocalTimeNotUpdatedByNonLastSpatialLayer) {
  SimulatedClock clock(kInitialTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  DefaultVideoJitterTiming timing(env);

  timing.OnCompleteFrame({.rtp_timestamp = kRtpTimestamp,
                          .time = clock.CurrentTime(),
                          .last_spatial_layer = false,
                          .was_retransmitted = false});
  EXPECT_EQ(timing.LocalTime(kRtpTimestamp), std::nullopt);
}

TEST(DefaultVideoJitterTimingTest,
     LocalTimeUpdatedByNonLastSpatialLayerWhenMarkerBitOnlyDisabled) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-IncomingTimestampOnMarkerBitOnly/Disabled/");
  SimulatedClock clock(kInitialTime);
  Environment env =
      CreateTestEnvironment({.field_trials = field_trials, .time = &clock});
  DefaultVideoJitterTiming timing(env);

  timing.OnCompleteFrame({.rtp_timestamp = kRtpTimestamp,
                          .time = clock.CurrentTime(),
                          .last_spatial_layer = false,
                          .was_retransmitted = false});
  EXPECT_NE(timing.LocalTime(kRtpTimestamp), std::nullopt);
}

TEST(DefaultVideoJitterTimingTest, OnContinuousTemporalUnitsReturnsNullopt) {
  SimulatedClock clock(kInitialTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  DefaultVideoJitterTiming timing(env);

  EXPECT_EQ(timing.OnContinuousTemporalUnits({{123}}, clock.CurrentTime()),
            std::nullopt);
}

TEST(DefaultVideoJitterTimingTest, OnDecodableTemporalUnitReturnsEstimate) {
  SimulatedClock clock(kInitialTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  DefaultVideoJitterTiming timing(env);

  EXPECT_NE(timing.OnDecodableTemporalUnit({.rtp_timestamp = kRtpTimestamp,
                                            .size = kFrameSize,
                                            .time = clock.CurrentTime(),
                                            .was_retransmitted = false}),
            std::nullopt);
}

TEST(DefaultVideoJitterTimingTest,
     OnDecodableTemporalUnitReturnsNulloptOnRetransmission) {
  SimulatedClock clock(kInitialTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  DefaultVideoJitterTiming timing(env);

  EXPECT_EQ(timing.OnDecodableTemporalUnit({.rtp_timestamp = kRtpTimestamp,
                                            .size = kFrameSize,
                                            .time = clock.CurrentTime(),
                                            .was_retransmitted = true}),
            std::nullopt);
}

TEST(DefaultVideoJitterTimingTest, EstimateIncludesRttAfterRetransmission) {
  constexpr double kMargin = 0.8;
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-JitterEstimatorConfig/nack_limit:1/");
  SimulatedClock clock(kInitialTime);
  Environment env =
      CreateTestEnvironment({.field_trials = field_trials, .time = &clock});
  DefaultVideoJitterTiming timing(env);

  timing.OnNetworkUpdate({.rtt = kRtt});

  std::optional<TimeDelta> initial_estimate =
      timing.OnDecodableTemporalUnit({.rtp_timestamp = 3000,
                                      .size = kFrameSize,
                                      .time = clock.CurrentTime(),
                                      .was_retransmitted = false});
  ASSERT_TRUE(initial_estimate.has_value());

  // Retransmitted frame.
  clock.AdvanceTime(TimeDelta::Millis(33));
  EXPECT_EQ(timing.OnDecodableTemporalUnit({.rtp_timestamp = 6000,
                                            .size = kFrameSize,
                                            .time = clock.CurrentTime(),
                                            .was_retransmitted = true}),
            std::nullopt);

  // Get estimate after retransmission.
  clock.AdvanceTime(TimeDelta::Millis(33));
  EXPECT_GT(timing.OnDecodableTemporalUnit({.rtp_timestamp = 9000,
                                            .size = kFrameSize,
                                            .time = clock.CurrentTime(),
                                            .was_retransmitted = false}),
            *initial_estimate + kMargin * kRtt);
}

TEST(DefaultVideoJitterTimingTest, ResetClearsJitterEstimator) {
  constexpr double kMargin = 0.5;
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-JitterEstimatorConfig/nack_limit:1/");
  SimulatedClock clock(kInitialTime);
  Environment env =
      CreateTestEnvironment({.field_trials = field_trials, .time = &clock});
  DefaultVideoJitterTiming timing(env);

  timing.OnNetworkUpdate({.rtt = kRtt});

  std::optional<TimeDelta> initial_estimate =
      timing.OnDecodableTemporalUnit({.rtp_timestamp = 3000,
                                      .size = kFrameSize,
                                      .time = clock.CurrentTime(),
                                      .was_retransmitted = false});
  ASSERT_TRUE(initial_estimate.has_value());

  // Retransmitted frame.
  clock.AdvanceTime(TimeDelta::Millis(33));
  EXPECT_EQ(timing.OnDecodableTemporalUnit({.rtp_timestamp = 6000,
                                            .size = kFrameSize,
                                            .time = clock.CurrentTime(),
                                            .was_retransmitted = true}),
            std::nullopt);

  timing.Reset();

  // Get estimate after retransmission and reset, should not include RTT.
  clock.AdvanceTime(TimeDelta::Millis(33));
  EXPECT_LT(timing.OnDecodableTemporalUnit({.rtp_timestamp = 9000,
                                            .size = kFrameSize,
                                            .time = clock.CurrentTime(),
                                            .was_retransmitted = false}),
            *initial_estimate + kMargin * kRtt);
}

}  // namespace
}  // namespace webrtc
