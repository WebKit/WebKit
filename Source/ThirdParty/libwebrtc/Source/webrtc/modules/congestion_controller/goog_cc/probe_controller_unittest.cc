/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/goog_cc/probe_controller.h"

#include <memory>

#include "api/transport/field_trial_based_config.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {
namespace test {

namespace {

constexpr int kMinBitrateBps = 100;
constexpr int kStartBitrateBps = 300;
constexpr int kMaxBitrateBps = 10000;

constexpr int kExponentialProbingTimeoutMs = 5000;

constexpr int kAlrProbeInterval = 5000;
constexpr int kAlrEndedTimeoutMs = 3000;
constexpr int kBitrateDropTimeoutMs = 5000;
}  // namespace

class ProbeControllerFixture {
 public:
  explicit ProbeControllerFixture(absl::string_view field_trials = "")
      : field_trial_config_(field_trials), clock_(100000000L) {}

  std::unique_ptr<ProbeController> CreateController() {
    return std::make_unique<ProbeController>(&field_trial_config_,
                                             &mock_rtc_event_log);
  }

  Timestamp CurrentTime() { return clock_.CurrentTime(); }
  int64_t NowMs() { return clock_.TimeInMilliseconds(); }
  void AdvanceTimeMilliseconds(int64_t delta_ms) {
    clock_.AdvanceTimeMilliseconds(delta_ms);
  }

  ExplicitKeyValueConfig field_trial_config_;
  SimulatedClock clock_;
  NiceMock<MockRtcEventLog> mock_rtc_event_log;
};

TEST(ProbeControllerTest, InitiatesProbingAtStart) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();

  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  EXPECT_GE(probes.size(), 2u);
}

TEST(ProbeControllerTest, SetsDefaultTargetDurationAndTargetProbeCount) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  std::vector<ProbeClusterConfig> probes = probe_controller->SetBitrates(
      kMinBitrateBps, kStartBitrateBps, kMaxBitrateBps, fixture.NowMs());
  ASSERT_GE(probes.size(), 2u);

  EXPECT_EQ(probes[0].target_duration, TimeDelta::Millis(15));
  EXPECT_EQ(probes[0].target_probe_count, 5);
}

TEST(ProbeControllerTest,
     FieldTrialsOverrideDefaultTargetDurationAndTargetProbeCount) {
  ProbeControllerFixture fixture(
      "WebRTC-Bwe-ProbingBehavior/"
      "min_probe_packets_sent:2,min_probe_duration:123ms/");
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  std::vector<ProbeClusterConfig> probes = probe_controller->SetBitrates(
      kMinBitrateBps, kStartBitrateBps, kMaxBitrateBps, fixture.NowMs());
  ASSERT_GE(probes.size(), 2u);

  EXPECT_EQ(probes[0].target_duration, TimeDelta::Millis(123));
  EXPECT_EQ(probes[0].target_probe_count, 2);
}

TEST(ProbeControllerTest, ProbeOnlyWhenNetworkIsUp) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->OnNetworkAvailability(
      {.at_time = fixture.CurrentTime(), .network_available = false});
  probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                         kMaxBitrateBps, fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);
  probes = probe_controller->OnNetworkAvailability(
      {.at_time = fixture.CurrentTime(), .network_available = true});
  EXPECT_GE(probes.size(), 2u);
}

TEST(ProbeControllerTest, InitiatesProbingOnMaxBitrateIncrease) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  // Long enough to time out exponential probing.
  fixture.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probes =
      probe_controller->SetEstimatedBitrate(kStartBitrateBps, fixture.NowMs());
  probes = probe_controller->Process(fixture.NowMs());
  probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                         kMaxBitrateBps + 100, fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), kMaxBitrateBps + 100);
}

TEST(ProbeControllerTest, ProbesOnMaxBitrateIncreaseOnlyWhenInAlr) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  probes = probe_controller->SetEstimatedBitrate(kMaxBitrateBps - 1,
                                                 fixture.NowMs());

  // Wait long enough to time out exponential probing.
  fixture.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probes = probe_controller->Process(fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);

  // Probe when in alr.
  probe_controller->SetAlrStartTimeMs(fixture.NowMs());
  probes = probe_controller->OnMaxTotalAllocatedBitrate(kMaxBitrateBps + 1,
                                                        fixture.NowMs());
  EXPECT_EQ(probes.size(), 2u);

  // Do not probe when not in alr.
  probe_controller->SetAlrStartTimeMs(absl::nullopt);
  probes = probe_controller->OnMaxTotalAllocatedBitrate(kMaxBitrateBps + 2,
                                                        fixture.NowMs());
  EXPECT_TRUE(probes.empty());
}

TEST(ProbeControllerTest, InitiatesProbingOnMaxBitrateIncreaseAtMaxBitrate) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  // Long enough to time out exponential probing.
  fixture.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probes =
      probe_controller->SetEstimatedBitrate(kStartBitrateBps, fixture.NowMs());
  probes = probe_controller->Process(fixture.NowMs());
  probes =
      probe_controller->SetEstimatedBitrate(kMaxBitrateBps, fixture.NowMs());
  probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                         kMaxBitrateBps + 100, fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), kMaxBitrateBps + 100);
}

TEST(ProbeControllerTest, TestExponentialProbing) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());

  // Repeated probe should only be sent when estimated bitrate climbs above
  // 0.7 * 6 * kStartBitrateBps = 1260.
  probes = probe_controller->SetEstimatedBitrate(1000, fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);

  probes = probe_controller->SetEstimatedBitrate(1800, fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 2 * 1800);
}

TEST(ProbeControllerTest, TestExponentialProbingTimeout) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  // Advance far enough to cause a time out in waiting for probing result.
  fixture.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probes = probe_controller->Process(fixture.NowMs());

  probes = probe_controller->SetEstimatedBitrate(1800, fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);
}

TEST(ProbeControllerTest, RequestProbeInAlr) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  EXPECT_GE(probes.size(), 2u);
  probes = probe_controller->SetEstimatedBitrate(500, fixture.NowMs());

  probe_controller->SetAlrStartTimeMs(fixture.NowMs());
  fixture.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probes = probe_controller->Process(fixture.NowMs());
  probes = probe_controller->SetEstimatedBitrate(250, fixture.NowMs());
  probes = probe_controller->RequestProbe(fixture.NowMs());

  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 0.85 * 500);
}

TEST(ProbeControllerTest, RequestProbeWhenAlrEndedRecently) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  EXPECT_EQ(probes.size(), 2u);
  probes = probe_controller->SetEstimatedBitrate(500, fixture.NowMs());

  probe_controller->SetAlrStartTimeMs(absl::nullopt);
  fixture.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probes = probe_controller->Process(fixture.NowMs());
  probes = probe_controller->SetEstimatedBitrate(250, fixture.NowMs());
  probe_controller->SetAlrEndedTimeMs(fixture.NowMs());
  fixture.AdvanceTimeMilliseconds(kAlrEndedTimeoutMs - 1);
  probes = probe_controller->RequestProbe(fixture.NowMs());

  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 0.85 * 500);
}

TEST(ProbeControllerTest, RequestProbeWhenAlrNotEndedRecently) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  EXPECT_EQ(probes.size(), 2u);
  probes = probe_controller->SetEstimatedBitrate(500, fixture.NowMs());

  probe_controller->SetAlrStartTimeMs(absl::nullopt);
  fixture.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probes = probe_controller->Process(fixture.NowMs());
  probes = probe_controller->SetEstimatedBitrate(250, fixture.NowMs());
  probe_controller->SetAlrEndedTimeMs(fixture.NowMs());
  fixture.AdvanceTimeMilliseconds(kAlrEndedTimeoutMs + 1);
  probes = probe_controller->RequestProbe(fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);
}

TEST(ProbeControllerTest, RequestProbeWhenBweDropNotRecent) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  EXPECT_EQ(probes.size(), 2u);
  probes = probe_controller->SetEstimatedBitrate(500, fixture.NowMs());

  probe_controller->SetAlrStartTimeMs(fixture.NowMs());
  fixture.AdvanceTimeMilliseconds(kAlrProbeInterval + 1);
  probes = probe_controller->Process(fixture.NowMs());
  probes = probe_controller->SetEstimatedBitrate(250, fixture.NowMs());
  fixture.AdvanceTimeMilliseconds(kBitrateDropTimeoutMs + 1);
  probes = probe_controller->RequestProbe(fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);
}

TEST(ProbeControllerTest, PeriodicProbing) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  probe_controller->EnablePeriodicAlrProbing(true);
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  EXPECT_EQ(probes.size(), 2u);
  probes = probe_controller->SetEstimatedBitrate(500, fixture.NowMs());

  int64_t start_time = fixture.NowMs();

  // Expect the controller to send a new probe after 5s has passed.
  probe_controller->SetAlrStartTimeMs(start_time);
  fixture.AdvanceTimeMilliseconds(5000);
  probes = probe_controller->Process(fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 1000);

  probes = probe_controller->SetEstimatedBitrate(500, fixture.NowMs());

  // The following probe should be sent at 10s into ALR.
  probe_controller->SetAlrStartTimeMs(start_time);
  fixture.AdvanceTimeMilliseconds(4000);
  probes = probe_controller->Process(fixture.NowMs());
  probes = probe_controller->SetEstimatedBitrate(500, fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);

  probe_controller->SetAlrStartTimeMs(start_time);
  fixture.AdvanceTimeMilliseconds(1000);
  probes = probe_controller->Process(fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  probes = probe_controller->SetEstimatedBitrate(500, fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);
}

TEST(ProbeControllerTest, PeriodicProbingAfterReset) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  int64_t alr_start_time = fixture.NowMs();

  probe_controller->SetAlrStartTimeMs(alr_start_time);
  probe_controller->EnablePeriodicAlrProbing(true);
  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              kMaxBitrateBps, fixture.NowMs());
  probe_controller->Reset(fixture.NowMs());

  fixture.AdvanceTimeMilliseconds(10000);
  probes = probe_controller->Process(fixture.NowMs());
  // Since bitrates are not yet set, no probe is sent event though we are in ALR
  // mode.
  EXPECT_EQ(probes.size(), 0u);

  probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                         kMaxBitrateBps, fixture.NowMs());
  EXPECT_EQ(probes.size(), 2u);

  // Make sure we use `kStartBitrateBps` as the estimated bitrate
  // until SetEstimatedBitrate is called with an updated estimate.
  fixture.AdvanceTimeMilliseconds(10000);
  probes = probe_controller->Process(fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), kStartBitrateBps * 2);
}

TEST(ProbeControllerTest, TestExponentialProbingOverflow) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  const int64_t kMbpsMultiplier = 1000000;
  auto probes =
      probe_controller->SetBitrates(kMinBitrateBps, 10 * kMbpsMultiplier,
                                    100 * kMbpsMultiplier, fixture.NowMs());
  // Verify that probe bitrate is capped at the specified max bitrate.
  probes = probe_controller->SetEstimatedBitrate(60 * kMbpsMultiplier,
                                                 fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 100 * kMbpsMultiplier);
  // Verify that repeated probes aren't sent.
  probes = probe_controller->SetEstimatedBitrate(100 * kMbpsMultiplier,
                                                 fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);
}

TEST(ProbeControllerTest, TestAllocatedBitrateCap) {
  ProbeControllerFixture fixture;
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();
  const int64_t kMbpsMultiplier = 1000000;
  const int64_t kMaxBitrateBps = 100 * kMbpsMultiplier;
  auto probes = probe_controller->SetBitrates(
      kMinBitrateBps, 10 * kMbpsMultiplier, kMaxBitrateBps, fixture.NowMs());

  // Configure ALR for periodic probing.
  probe_controller->EnablePeriodicAlrProbing(true);
  int64_t alr_start_time = fixture.NowMs();
  probe_controller->SetAlrStartTimeMs(alr_start_time);

  int64_t estimated_bitrate_bps = kMaxBitrateBps / 10;
  probes = probe_controller->SetEstimatedBitrate(estimated_bitrate_bps,
                                                 fixture.NowMs());

  // Set a max allocated bitrate below the current estimate.
  int64_t max_allocated_bps = estimated_bitrate_bps - 1 * kMbpsMultiplier;
  probes = probe_controller->OnMaxTotalAllocatedBitrate(max_allocated_bps,
                                                        fixture.NowMs());
  EXPECT_TRUE(probes.empty());  // No probe since lower than current max.

  // Probes such as ALR capped at 2x the max allocation limit.
  fixture.AdvanceTimeMilliseconds(5000);
  probes = probe_controller->Process(fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 2 * max_allocated_bps);

  // Remove allocation limit.
  EXPECT_TRUE(
      probe_controller->OnMaxTotalAllocatedBitrate(0, fixture.NowMs()).empty());
  fixture.AdvanceTimeMilliseconds(5000);
  probes = probe_controller->Process(fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), estimated_bitrate_bps * 2);
}

TEST(ProbeControllerTest, ConfigurableProbingFieldTrial) {
  ProbeControllerFixture fixture(
      "WebRTC-Bwe-ProbingConfiguration/"
      "p1:2,p2:5,step_size:3,further_probe_threshold:0.8,"
      "alloc_p1:2,alloc_p2/");
  std::unique_ptr<ProbeController> probe_controller =
      fixture.CreateController();

  auto probes = probe_controller->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                              5000000, fixture.NowMs());
  EXPECT_EQ(probes.size(), 2u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 600);
  EXPECT_EQ(probes[1].target_data_rate.bps(), 1500);

  // Repeated probe should only be sent when estimated bitrate climbs above
  // 0.8 * 5 * kStartBitrateBps = 1200.
  probes = probe_controller->SetEstimatedBitrate(1100, fixture.NowMs());
  EXPECT_EQ(probes.size(), 0u);

  probes = probe_controller->SetEstimatedBitrate(1250, fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 3 * 1250);

  fixture.AdvanceTimeMilliseconds(5000);
  probes = probe_controller->Process(fixture.NowMs());

  probe_controller->SetAlrStartTimeMs(fixture.NowMs());
  probes =
      probe_controller->OnMaxTotalAllocatedBitrate(200000, fixture.NowMs());
  EXPECT_EQ(probes.size(), 1u);
  EXPECT_EQ(probes[0].target_data_rate.bps(), 400000);
}

}  // namespace test
}  // namespace webrtc
