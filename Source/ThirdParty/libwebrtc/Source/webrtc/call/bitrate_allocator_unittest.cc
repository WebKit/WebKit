/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/bitrate_allocator.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::NiceMock;

namespace webrtc {

namespace {
auto AllocationLimitsEq(uint32_t min_allocatable_rate_bps,
                        uint32_t max_padding_rate_bps,
                        uint32_t max_allocatable_rate_bps) {
  return AllOf(Field(&BitrateAllocationLimits::min_allocatable_rate,
                     DataRate::BitsPerSec(min_allocatable_rate_bps)),
               Field(&BitrateAllocationLimits::max_allocatable_rate,
                     DataRate::BitsPerSec(max_allocatable_rate_bps)),
               Field(&BitrateAllocationLimits::max_padding_rate,
                     DataRate::BitsPerSec(max_padding_rate_bps)));
}

auto AllocationLimitsEq(uint32_t min_allocatable_rate_bps,
                        uint32_t max_padding_rate_bps) {
  return AllOf(Field(&BitrateAllocationLimits::min_allocatable_rate,
                     DataRate::BitsPerSec(min_allocatable_rate_bps)),
               Field(&BitrateAllocationLimits::max_padding_rate,
                     DataRate::BitsPerSec(max_padding_rate_bps)));
}

class MockLimitObserver : public BitrateAllocator::LimitObserver {
 public:
  MOCK_METHOD(void,
              OnAllocationLimitsChanged,
              (BitrateAllocationLimits),
              (override));
};

class TestBitrateObserver : public BitrateAllocatorObserver {
 public:
  TestBitrateObserver()
      : last_bitrate_bps_(0),
        last_fraction_loss_(0),
        last_rtt_ms_(0),
        last_probing_interval_ms_(0),
        protection_ratio_(0.0) {}

  void SetBitrateProtectionRatio(double protection_ratio) {
    protection_ratio_ = protection_ratio;
  }

  uint32_t OnBitrateUpdated(BitrateAllocationUpdate update) override {
    last_bitrate_bps_ = update.target_bitrate.bps();
    last_fraction_loss_ =
        rtc::dchecked_cast<uint8_t>(update.packet_loss_ratio * 256);
    last_rtt_ms_ = update.round_trip_time.ms();
    last_probing_interval_ms_ = update.bwe_period.ms();
    return update.target_bitrate.bps() * protection_ratio_;
  }
  uint32_t last_bitrate_bps_;
  uint8_t last_fraction_loss_;
  int64_t last_rtt_ms_;
  int last_probing_interval_ms_;
  double protection_ratio_;
};

constexpr int64_t kDefaultProbingIntervalMs = 3000;
const double kDefaultBitratePriority = 1.0;

TargetTransferRate CreateTargetRateMessage(uint32_t target_bitrate_bps,
                                           uint8_t fraction_loss,
                                           int64_t rtt_ms,
                                           int64_t bwe_period_ms) {
  TargetTransferRate msg;
  // The timestamp is just for log output, keeping it fixed just means fewer log
  // messages in the test.
  msg.at_time = Timestamp::Seconds(10000);
  msg.target_rate = DataRate::BitsPerSec(target_bitrate_bps);
  msg.stable_target_rate = msg.target_rate;
  msg.network_estimate.bandwidth = msg.target_rate;
  msg.network_estimate.loss_rate_ratio = fraction_loss / 255.0;
  msg.network_estimate.round_trip_time = TimeDelta::Millis(rtt_ms);
  msg.network_estimate.bwe_period = TimeDelta::Millis(bwe_period_ms);
  return msg;
}
}  // namespace

class BitrateAllocatorTest : public ::testing::Test {
 protected:
  BitrateAllocatorTest() : allocator_(new BitrateAllocator(&limit_observer_)) {
    allocator_->OnNetworkEstimateChanged(
        CreateTargetRateMessage(300000u, 0, 0, kDefaultProbingIntervalMs));
  }
  ~BitrateAllocatorTest() {}
  void AddObserver(BitrateAllocatorObserver* observer,
                   uint32_t min_bitrate_bps,
                   uint32_t max_bitrate_bps,
                   uint32_t pad_up_bitrate_bps,
                   bool enforce_min_bitrate,
                   double bitrate_priority) {
    allocator_->AddObserver(
        observer,
        {min_bitrate_bps, max_bitrate_bps, pad_up_bitrate_bps,
         /* priority_bitrate */ 0, enforce_min_bitrate, bitrate_priority});
  }
  MediaStreamAllocationConfig DefaultConfig() const {
    MediaStreamAllocationConfig default_config;
    default_config.min_bitrate_bps = 0;
    default_config.max_bitrate_bps = 1500000;
    default_config.pad_up_bitrate_bps = 0;
    default_config.priority_bitrate_bps = 0;
    default_config.enforce_min_bitrate = true;
    default_config.bitrate_priority = kDefaultBitratePriority;
    return default_config;
  }

  NiceMock<MockLimitObserver> limit_observer_;
  std::unique_ptr<BitrateAllocator> allocator_;
};

TEST_F(BitrateAllocatorTest, RespectsPriorityBitrate) {
  TestBitrateObserver stream_a;
  auto config_a = DefaultConfig();
  config_a.min_bitrate_bps = 100000;
  config_a.priority_bitrate_bps = 0;
  allocator_->AddObserver(&stream_a, config_a);

  TestBitrateObserver stream_b;
  auto config_b = DefaultConfig();
  config_b.min_bitrate_bps = 100000;
  config_b.max_bitrate_bps = 300000;
  config_b.priority_bitrate_bps = 300000;
  allocator_->AddObserver(&stream_b, config_b);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(100000, 0, 0, 0));
  EXPECT_EQ(stream_a.last_bitrate_bps_, 100000u);
  EXPECT_EQ(stream_b.last_bitrate_bps_, 100000u);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(200000, 0, 0, 0));
  EXPECT_EQ(stream_a.last_bitrate_bps_, 100000u);
  EXPECT_EQ(stream_b.last_bitrate_bps_, 100000u);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(300000, 0, 0, 0));
  EXPECT_EQ(stream_a.last_bitrate_bps_, 100000u);
  EXPECT_EQ(stream_b.last_bitrate_bps_, 200000u);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(400000, 0, 0, 0));
  EXPECT_EQ(stream_a.last_bitrate_bps_, 100000u);
  EXPECT_EQ(stream_b.last_bitrate_bps_, 300000u);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(800000, 0, 0, 0));
  EXPECT_EQ(stream_a.last_bitrate_bps_, 500000u);
  EXPECT_EQ(stream_b.last_bitrate_bps_, 300000u);
}

TEST_F(BitrateAllocatorTest, UpdatingBitrateObserver) {
  TestBitrateObserver bitrate_observer;
  const uint32_t kMinSendBitrateBps = 100000;
  const uint32_t kPadUpToBitrateBps = 50000;
  const uint32_t kMaxBitrateBps = 1500000;

  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(
                  kMinSendBitrateBps, kPadUpToBitrateBps, kMaxBitrateBps)));
  AddObserver(&bitrate_observer, kMinSendBitrateBps, kMaxBitrateBps,
              kPadUpToBitrateBps, true, kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer));
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(200000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer));

  // TODO(pbos): Expect capping to 1.5M instead of 3M when not boosting the max
  // bitrate for FEC/retransmissions (see todo in BitrateAllocator).
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(4000000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(3000000, allocator_->GetStartBitrate(&bitrate_observer));

  // Expect `max_padding_bitrate_bps` to change to 0 if the observer is updated.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(
                                   AllocationLimitsEq(kMinSendBitrateBps, 0)));
  AddObserver(&bitrate_observer, kMinSendBitrateBps, 4000000, 0, true,
              kDefaultBitratePriority);
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(
                                   AllocationLimitsEq(kMinSendBitrateBps, 0)));
  EXPECT_EQ(4000000, allocator_->GetStartBitrate(&bitrate_observer));

  AddObserver(&bitrate_observer, kMinSendBitrateBps, kMaxBitrateBps, 0, true,
              kDefaultBitratePriority);
  EXPECT_EQ(3000000, allocator_->GetStartBitrate(&bitrate_observer));
  EXPECT_EQ(3000000u, bitrate_observer.last_bitrate_bps_);
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(kMaxBitrateBps, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(1500000u, bitrate_observer.last_bitrate_bps_);
}

TEST_F(BitrateAllocatorTest, TwoBitrateObserversOneRtcpObserver) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  const uint32_t kObs1StartBitrateBps = 100000;
  const uint32_t kObs2StartBitrateBps = 200000;
  const uint32_t kObs1MaxBitrateBps = 300000;
  const uint32_t kObs2MaxBitrateBps = 300000;

  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(
                  kObs1StartBitrateBps, 0, kObs1MaxBitrateBps)));
  AddObserver(&bitrate_observer_1, kObs1StartBitrateBps, kObs1MaxBitrateBps, 0,
              true, kDefaultBitratePriority);
  EXPECT_EQ(static_cast<int>(kObs1MaxBitrateBps),
            allocator_->GetStartBitrate(&bitrate_observer_1));
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(
                  kObs1StartBitrateBps + kObs2StartBitrateBps, 0,
                  kObs1MaxBitrateBps + kObs2MaxBitrateBps)));
  AddObserver(&bitrate_observer_2, kObs2StartBitrateBps, kObs2MaxBitrateBps, 0,
              true, kDefaultBitratePriority);
  EXPECT_EQ(static_cast<int>(kObs2StartBitrateBps),
            allocator_->GetStartBitrate(&bitrate_observer_2));

  // Test too low start bitrate, hence lower than sum of min. Min bitrates
  // will
  // be allocated to all observers.
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kObs2StartBitrateBps, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0, bitrate_observer_1.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_1.last_rtt_ms_);
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);
  EXPECT_EQ(0, bitrate_observer_2.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_2.last_rtt_ms_);

  // Test a bitrate which should be distributed equally.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(500000, 0, 50, kDefaultProbingIntervalMs));
  const uint32_t kBitrateToShare =
      500000 - kObs2StartBitrateBps - kObs1StartBitrateBps;
  EXPECT_EQ(100000u + kBitrateToShare / 2,
            bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(200000u + kBitrateToShare / 2,
            bitrate_observer_2.last_bitrate_bps_);

  // Limited by 2x max bitrates since we leave room for FEC and
  // retransmissions.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(1500000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(600000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(600000u, bitrate_observer_2.last_bitrate_bps_);

  // Verify that if the bandwidth estimate is set to zero, the allocated
  // rate is
  // zero.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(0, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);
}

TEST_F(BitrateAllocatorTest, RemoveObserverTriggersLimitObserver) {
  TestBitrateObserver bitrate_observer;
  const uint32_t kMinSendBitrateBps = 100000;
  const uint32_t kPadUpToBitrateBps = 50000;
  const uint32_t kMaxBitrateBps = 1500000;

  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(
                  kMinSendBitrateBps, kPadUpToBitrateBps, kMaxBitrateBps)));
  AddObserver(&bitrate_observer, kMinSendBitrateBps, kMaxBitrateBps,
              kPadUpToBitrateBps, true, kDefaultBitratePriority);
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(0, 0)));
  allocator_->RemoveObserver(&bitrate_observer);
}

class BitrateAllocatorTestNoEnforceMin : public ::testing::Test {
 protected:
  BitrateAllocatorTestNoEnforceMin()
      : allocator_(new BitrateAllocator(&limit_observer_)) {
    allocator_->OnNetworkEstimateChanged(
        CreateTargetRateMessage(300000u, 0, 0, kDefaultProbingIntervalMs));
  }
  ~BitrateAllocatorTestNoEnforceMin() {}
  void AddObserver(BitrateAllocatorObserver* observer,
                   uint32_t min_bitrate_bps,
                   uint32_t max_bitrate_bps,
                   uint32_t pad_up_bitrate_bps,
                   bool enforce_min_bitrate,
                   absl::string_view track_id,
                   double bitrate_priority) {
    allocator_->AddObserver(
        observer, {min_bitrate_bps, max_bitrate_bps, pad_up_bitrate_bps, 0,
                   enforce_min_bitrate, bitrate_priority});
  }
  NiceMock<MockLimitObserver> limit_observer_;
  std::unique_ptr<BitrateAllocator> allocator_;
};

// The following three tests verify enforcing a minimum bitrate works as
// intended.
TEST_F(BitrateAllocatorTestNoEnforceMin, OneBitrateObserver) {
  TestBitrateObserver bitrate_observer_1;
  // Expect OnAllocationLimitsChanged with `min_send_bitrate_bps` = 0 since
  // AddObserver is called with `enforce_min_bitrate` = false.
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(0, 0)));
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(0, 120000)));
  AddObserver(&bitrate_observer_1, 100000, 400000, 0, false, "",
              kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));

  // High BWE.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(150000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(150000u, bitrate_observer_1.last_bitrate_bps_);

  // Low BWE.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(10000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);

  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(0, 0)));
  allocator_->RemoveObserver(&bitrate_observer_1);
}

TEST_F(BitrateAllocatorTestNoEnforceMin, ThreeBitrateObservers) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  TestBitrateObserver bitrate_observer_3;
  // Set up the observers with min bitrates at 100000, 200000, and 300000.
  AddObserver(&bitrate_observer_1, 100000, 400000, 0, false, "",
              kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));

  AddObserver(&bitrate_observer_2, 200000, 400000, 0, false, "",
              kDefaultBitratePriority);
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer_2));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);

  AddObserver(&bitrate_observer_3, 300000, 400000, 0, false, "",
              kDefaultBitratePriority);
  EXPECT_EQ(0, allocator_->GetStartBitrate(&bitrate_observer_3));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);

  // High BWE. Make sure the controllers get a fair share of the surplus (i.e.,
  // what is left after each controller gets its min rate).
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(690000, 0, 0, kDefaultProbingIntervalMs));
  // Verify that each observer gets its min rate (sum of min rates is 600000),
  // and that the remaining 90000 is divided equally among the three.
  uint32_t bitrate_to_share = 690000u - 100000u - 200000u - 300000u;
  EXPECT_EQ(100000u + bitrate_to_share / 3,
            bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(200000u + bitrate_to_share / 3,
            bitrate_observer_2.last_bitrate_bps_);
  EXPECT_EQ(300000u + bitrate_to_share / 3,
            bitrate_observer_3.last_bitrate_bps_);

  // BWE below the sum of observer's min bitrate.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(300000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);  // Min bitrate.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);  // Min bitrate.
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_bps_);       // Nothing.

  // Increased BWE, but still below the sum of configured min bitrates for all
  // observers and too little for observer 3. 1 and 2 will share the rest.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(500000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(200000u, bitrate_observer_1.last_bitrate_bps_);  // Min + split.
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_bps_);  // Min + split.
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_bps_);       // Nothing.

  // Below min for all.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(10000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_bps_);

  // Verify that zero estimated bandwidth, means that that all gets zero,
  // regardless of set min bitrate.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(0, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_bps_);

  allocator_->RemoveObserver(&bitrate_observer_1);
  allocator_->RemoveObserver(&bitrate_observer_2);
  allocator_->RemoveObserver(&bitrate_observer_3);
}

TEST_F(BitrateAllocatorTestNoEnforceMin, OneBitrateObserverWithPacketLoss) {
  const uint32_t kMinBitrateBps = 100000;
  const uint32_t kMaxBitrateBps = 400000;
  // Hysteresis adds another 10% or 20kbps to min bitrate.
  const uint32_t kMinStartBitrateBps =
      kMinBitrateBps + std::max(20000u, kMinBitrateBps / 10);

  // Expect OnAllocationLimitsChanged with `min_send_bitrate_bps` = 0 since
  // AddObserver is called with `enforce_min_bitrate` = false.
  TestBitrateObserver bitrate_observer;
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(
                                   AllocationLimitsEq(0, 0, kMaxBitrateBps)));
  AddObserver(&bitrate_observer, kMinBitrateBps, kMaxBitrateBps, 0, false, "",
              kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer));

  // High BWE.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(150000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(150000u, bitrate_observer.last_bitrate_bps_);

  // Add loss and use a part of the bitrate for protection.
  const double kProtectionRatio = 0.4;
  const uint8_t fraction_loss = kProtectionRatio * 256;
  bitrate_observer.SetBitrateProtectionRatio(kProtectionRatio);
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      200000, 0, fraction_loss, kDefaultProbingIntervalMs));
  EXPECT_EQ(200000u, bitrate_observer.last_bitrate_bps_);

  // Above the min threshold, but not enough given the protection used.
  // Limits changed, as we will video is now off and we need to pad up to the
  // start bitrate.
  // Verify the hysteresis is added for the protection.
  const uint32_t kMinStartBitrateWithProtectionBps =
      static_cast<uint32_t>(kMinStartBitrateBps * (1 + kProtectionRatio));
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(
                  0, kMinStartBitrateWithProtectionBps, kMaxBitrateBps)));
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kMinStartBitrateBps + 1000, 0, fraction_loss, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, bitrate_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(kMinStartBitrateWithProtectionBps - 1000, 0,
                              fraction_loss, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, bitrate_observer.last_bitrate_bps_);

  // Just enough to enable video again.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(
                                   AllocationLimitsEq(0, 0, kMaxBitrateBps)));
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(kMinStartBitrateWithProtectionBps, 0,
                              fraction_loss, kDefaultProbingIntervalMs));
  EXPECT_EQ(kMinStartBitrateWithProtectionBps,
            bitrate_observer.last_bitrate_bps_);

  // Remove all protection and make sure video is not paused as earlier.
  bitrate_observer.SetBitrateProtectionRatio(0.0);
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(kMinStartBitrateWithProtectionBps - 1000, 0, 0,
                              kDefaultProbingIntervalMs));
  EXPECT_EQ(kMinStartBitrateWithProtectionBps - 1000,
            bitrate_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kMinStartBitrateBps, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(kMinStartBitrateBps, bitrate_observer.last_bitrate_bps_);

  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(0, 0, 0)));
  allocator_->RemoveObserver(&bitrate_observer);
}

TEST_F(BitrateAllocatorTest,
       TotalAllocationLimitsAreUnaffectedByProtectionRatio) {
  TestBitrateObserver bitrate_observer;

  const uint32_t kMinBitrateBps = 100000;
  const uint32_t kMaxBitrateBps = 400000;

  // Register `bitrate_observer` and expect total allocation limits to change.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(AllocationLimitsEq(
                                   kMinBitrateBps, 0, kMaxBitrateBps)))
      .Times(1);
  MediaStreamAllocationConfig allocation_config = DefaultConfig();
  allocation_config.min_bitrate_bps = kMinBitrateBps;
  allocation_config.max_bitrate_bps = kMaxBitrateBps;
  allocator_->AddObserver(&bitrate_observer, allocation_config);

  // Observer uses 20% of it's allocated bitrate for protection.
  bitrate_observer.SetBitrateProtectionRatio(/*protection_ratio=*/0.2);
  // Total allocation limits are unaffected by the protection rate change.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(_)).Times(0);
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(200000u, 0, 100, kDefaultProbingIntervalMs));

  // Observer uses 0% of it's allocated bitrate for protection.
  bitrate_observer.SetBitrateProtectionRatio(/*protection_ratio=*/0.0);
  // Total allocation limits are unaffected by the protection rate change.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(_)).Times(0);
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(200000u, 0, 100, kDefaultProbingIntervalMs));

  // Observer again uses 20% of it's allocated bitrate for protection.
  bitrate_observer.SetBitrateProtectionRatio(/*protection_ratio=*/0.2);
  // Total allocation limits are unaffected by the protection rate change.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(_)).Times(0);
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(200000u, 0, 100, kDefaultProbingIntervalMs));
}

TEST_F(BitrateAllocatorTestNoEnforceMin, TwoBitrateObserverWithPacketLoss) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;

  AddObserver(&bitrate_observer_1, 100000, 400000, 0, false, "",
              kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));
  AddObserver(&bitrate_observer_2, 200000, 400000, 0, false, "",
              kDefaultBitratePriority);
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer_2));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);

  // Enough bitrate for both.
  bitrate_observer_2.SetBitrateProtectionRatio(0.5);
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(300000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);

  // Above min for observer 2, but too little given the protection used.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(330000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(330000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(100000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(99999, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(119000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(120000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(120000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  // Verify the protection is accounted for before resuming observer 2.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(429000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(400000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(430000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(330000u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->RemoveObserver(&bitrate_observer_1);
  allocator_->RemoveObserver(&bitrate_observer_2);
}

TEST_F(BitrateAllocatorTest, ThreeBitrateObserversLowBweEnforceMin) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  TestBitrateObserver bitrate_observer_3;

  AddObserver(&bitrate_observer_1, 100000, 400000, 0, true,
              kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));

  AddObserver(&bitrate_observer_2, 200000, 400000, 0, true,
              kDefaultBitratePriority);
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer_2));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);

  AddObserver(&bitrate_observer_3, 300000, 400000, 0, true,
              kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_3));
  EXPECT_EQ(100000, static_cast<int>(bitrate_observer_1.last_bitrate_bps_));
  EXPECT_EQ(200000, static_cast<int>(bitrate_observer_2.last_bitrate_bps_));

  // Low BWE. Verify that all observers still get their respective min
  // bitrate.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(1000, 0, 0, kDefaultProbingIntervalMs));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);  // Min cap.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);  // Min cap.
  EXPECT_EQ(300000u, bitrate_observer_3.last_bitrate_bps_);  // Min cap.

  allocator_->RemoveObserver(&bitrate_observer_1);
  allocator_->RemoveObserver(&bitrate_observer_2);
  allocator_->RemoveObserver(&bitrate_observer_3);
}

TEST_F(BitrateAllocatorTest, AddObserverWhileNetworkDown) {
  TestBitrateObserver bitrate_observer_1;
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(50000, 0)));

  AddObserver(&bitrate_observer_1, 50000, 400000, 0, true,
              kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));

  // Set network down, ie, no available bitrate.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(0, 0, 0, kDefaultProbingIntervalMs));

  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);

  TestBitrateObserver bitrate_observer_2;
  // Adding an observer while the network is down should not affect the limits.
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(AllocationLimitsEq(50000 + 50000, 0)));
  AddObserver(&bitrate_observer_2, 50000, 400000, 0, true,
              kDefaultBitratePriority);

  // Expect the start_bitrate to be set as if the network was still up but that
  // the new observer have been notified that the network is down.
  EXPECT_EQ(300000 / 2, allocator_->GetStartBitrate(&bitrate_observer_2));
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  // Set network back up.
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(1500000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(750000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(750000u, bitrate_observer_2.last_bitrate_bps_);
}

TEST_F(BitrateAllocatorTest, MixedEnforecedConfigs) {
  TestBitrateObserver enforced_observer;
  AddObserver(&enforced_observer, 6000, 30000, 0, true,
              kDefaultBitratePriority);
  EXPECT_EQ(60000, allocator_->GetStartBitrate(&enforced_observer));

  TestBitrateObserver not_enforced_observer;
  AddObserver(&not_enforced_observer, 30000, 2500000, 0, false,
              kDefaultBitratePriority);
  EXPECT_EQ(270000, allocator_->GetStartBitrate(&not_enforced_observer));
  EXPECT_EQ(30000u, enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(36000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(6000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(30000u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(35000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(30000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(0u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(5000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(6000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(0u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(36000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(30000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(0u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(55000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(30000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(0u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(56000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(6000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(50000u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(56000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(16000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(40000u, not_enforced_observer.last_bitrate_bps_);

  allocator_->RemoveObserver(&enforced_observer);
  allocator_->RemoveObserver(&not_enforced_observer);
}

TEST_F(BitrateAllocatorTest, AvoidToggleAbsolute) {
  TestBitrateObserver observer;
  AddObserver(&observer, 30000, 300000, 0, false, kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&observer));

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(30000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(30000u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(20000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(30000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(49000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(50000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(50000u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(30000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(30000u, observer.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer);
}

TEST_F(BitrateAllocatorTest, AvoidTogglePercent) {
  TestBitrateObserver observer;
  AddObserver(&observer, 300000, 600000, 0, false, kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&observer));

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(300000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(300000u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(200000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(300000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(329000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(330000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(330000u, observer.last_bitrate_bps_);

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(300000, 0, 50, kDefaultProbingIntervalMs));
  EXPECT_EQ(300000u, observer.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer);
}

TEST_F(BitrateAllocatorTest, PassProbingInterval) {
  TestBitrateObserver observer;
  AddObserver(&observer, 300000, 600000, 0, false, kDefaultBitratePriority);
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&observer));

  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(300000, 0, 50, 5000));
  EXPECT_EQ(5000, observer.last_probing_interval_ms_);

  allocator_->RemoveObserver(&observer);
}

TEST_F(BitrateAllocatorTest, PriorityRateOneObserverBasic) {
  TestBitrateObserver observer;
  const uint32_t kMinSendBitrateBps = 10;
  const uint32_t kMaxSendBitrateBps = 60;
  const uint32_t kNetworkBandwidthBps = 30;

  AddObserver(&observer, kMinSendBitrateBps, kMaxSendBitrateBps, 0, true, 2.0);
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kNetworkBandwidthBps, 0, 0, kDefaultProbingIntervalMs));

  EXPECT_EQ(kNetworkBandwidthBps, observer.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer);
}

// Tests that two observers with the same bitrate priority are allocated
// their bitrate evenly.
TEST_F(BitrateAllocatorTest, PriorityRateTwoObserversBasic) {
  TestBitrateObserver observer_low_1;
  TestBitrateObserver observer_low_2;
  const uint32_t kMinSendBitrateBps = 10;
  const uint32_t kMaxSendBitrateBps = 60;
  const uint32_t kNetworkBandwidthBps = 60;
  AddObserver(&observer_low_1, kMinSendBitrateBps, kMaxSendBitrateBps, 0, false,
              2.0);
  AddObserver(&observer_low_2, kMinSendBitrateBps, kMaxSendBitrateBps, 0, false,
              2.0);
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kNetworkBandwidthBps, 0, 0, kDefaultProbingIntervalMs));

  EXPECT_EQ(kNetworkBandwidthBps / 2, observer_low_1.last_bitrate_bps_);
  EXPECT_EQ(kNetworkBandwidthBps / 2, observer_low_2.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low_1);
  allocator_->RemoveObserver(&observer_low_2);
}

// Tests that there is no difference in functionality when the min bitrate is
// enforced.
TEST_F(BitrateAllocatorTest, PriorityRateTwoObserversBasicMinEnforced) {
  TestBitrateObserver observer_low_1;
  TestBitrateObserver observer_low_2;
  const uint32_t kMinSendBitrateBps = 0;
  const uint32_t kMaxSendBitrateBps = 60;
  const uint32_t kNetworkBandwidthBps = 60;
  AddObserver(&observer_low_1, kMinSendBitrateBps, kMaxSendBitrateBps, 0, true,
              2.0);
  AddObserver(&observer_low_2, kMinSendBitrateBps, kMaxSendBitrateBps, 0, true,
              2.0);
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kNetworkBandwidthBps, 0, 0, kDefaultProbingIntervalMs));

  EXPECT_EQ(kNetworkBandwidthBps / 2, observer_low_1.last_bitrate_bps_);
  EXPECT_EQ(kNetworkBandwidthBps / 2, observer_low_2.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low_1);
  allocator_->RemoveObserver(&observer_low_2);
}

// Tests that if the available bandwidth is the sum of the max bitrate
// of all observers, they will be allocated their max.
TEST_F(BitrateAllocatorTest, PriorityRateTwoObserversBothAllocatedMax) {
  TestBitrateObserver observer_low;
  TestBitrateObserver observer_mid;
  const uint32_t kMinSendBitrateBps = 0;
  const uint32_t kMaxSendBitrateBps = 60;
  const uint32_t kNetworkBandwidthBps = kMaxSendBitrateBps * 2;
  AddObserver(&observer_low, kMinSendBitrateBps, kMaxSendBitrateBps, 0, true,
              2.0);
  AddObserver(&observer_mid, kMinSendBitrateBps, kMaxSendBitrateBps, 0, true,
              4.0);
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kNetworkBandwidthBps, 0, 0, kDefaultProbingIntervalMs));

  EXPECT_EQ(kMaxSendBitrateBps, observer_low.last_bitrate_bps_);
  EXPECT_EQ(kMaxSendBitrateBps, observer_mid.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low);
  allocator_->RemoveObserver(&observer_mid);
}

// Tests that after a higher bitrate priority observer has been allocated its
// max bitrate the lower priority observer will then be allocated the remaining
// bitrate.
TEST_F(BitrateAllocatorTest, PriorityRateTwoObserversOneAllocatedToMax) {
  TestBitrateObserver observer_low;
  TestBitrateObserver observer_mid;
  AddObserver(&observer_low, 10, 50, 0, false, 2.0);
  AddObserver(&observer_mid, 10, 50, 0, false, 4.0);
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(90, 0, 0, kDefaultProbingIntervalMs));

  EXPECT_EQ(40u, observer_low.last_bitrate_bps_);
  EXPECT_EQ(50u, observer_mid.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low);
  allocator_->RemoveObserver(&observer_mid);
}

// Tests that three observers with three different bitrate priorities will all
// be allocated bitrate according to their relative bitrate priority.
TEST_F(BitrateAllocatorTest,
       PriorityRateThreeObserversAllocatedRelativeAmounts) {
  TestBitrateObserver observer_low;
  TestBitrateObserver observer_mid;
  TestBitrateObserver observer_high;
  const uint32_t kMaxBitrate = 100;
  // Not enough bandwidth to fill any observer's max bitrate.
  const uint32_t kNetworkBandwidthBps = 70;
  const double kLowBitratePriority = 2.0;
  const double kMidBitratePriority = 4.0;
  const double kHighBitratePriority = 8.0;
  const double kTotalBitratePriority =
      kLowBitratePriority + kMidBitratePriority + kHighBitratePriority;
  AddObserver(&observer_low, 0, kMaxBitrate, 0, false, kLowBitratePriority);
  AddObserver(&observer_mid, 0, kMaxBitrate, 0, false, kMidBitratePriority);
  AddObserver(&observer_high, 0, kMaxBitrate, 0, false, kHighBitratePriority);
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kNetworkBandwidthBps, 0, 0, kDefaultProbingIntervalMs));

  const double kLowFractionAllocated =
      kLowBitratePriority / kTotalBitratePriority;
  const double kMidFractionAllocated =
      kMidBitratePriority / kTotalBitratePriority;
  const double kHighFractionAllocated =
      kHighBitratePriority / kTotalBitratePriority;
  EXPECT_EQ(kLowFractionAllocated * kNetworkBandwidthBps,
            observer_low.last_bitrate_bps_);
  EXPECT_EQ(kMidFractionAllocated * kNetworkBandwidthBps,
            observer_mid.last_bitrate_bps_);
  EXPECT_EQ(kHighFractionAllocated * kNetworkBandwidthBps,
            observer_high.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low);
  allocator_->RemoveObserver(&observer_mid);
  allocator_->RemoveObserver(&observer_high);
}

// Tests that after the high priority observer has been allocated its maximum
// bitrate, the other two observers are still allocated bitrate according to
// their relative bitrate priority.
TEST_F(BitrateAllocatorTest, PriorityRateThreeObserversHighAllocatedToMax) {
  TestBitrateObserver observer_low;
  const double kLowBitratePriority = 2.0;
  TestBitrateObserver observer_mid;
  const double kMidBitratePriority = 4.0;
  TestBitrateObserver observer_high;
  const double kHighBitratePriority = 8.0;

  const uint32_t kAvailableBitrate = 90;
  const uint32_t kMaxBitrate = 40;
  const uint32_t kMinBitrate = 10;
  // Remaining bitrate after allocating to all mins and knowing that the high
  // priority observer will have its max bitrate allocated.
  const uint32_t kRemainingBitrate =
      kAvailableBitrate - kMaxBitrate - (2 * kMinBitrate);

  AddObserver(&observer_low, kMinBitrate, kMaxBitrate, 0, false,
              kLowBitratePriority);
  AddObserver(&observer_mid, kMinBitrate, kMaxBitrate, 0, false,
              kMidBitratePriority);
  AddObserver(&observer_high, kMinBitrate, kMaxBitrate, 0, false,
              kHighBitratePriority);
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kAvailableBitrate, 0, 0, kDefaultProbingIntervalMs));

  const double kLowFractionAllocated =
      kLowBitratePriority / (kLowBitratePriority + kMidBitratePriority);
  const double kMidFractionAllocated =
      kMidBitratePriority / (kLowBitratePriority + kMidBitratePriority);
  EXPECT_EQ(kMinBitrate + (kRemainingBitrate * kLowFractionAllocated),
            observer_low.last_bitrate_bps_);
  EXPECT_EQ(kMinBitrate + (kRemainingBitrate * kMidFractionAllocated),
            observer_mid.last_bitrate_bps_);
  EXPECT_EQ(40u, observer_high.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low);
  allocator_->RemoveObserver(&observer_mid);
  allocator_->RemoveObserver(&observer_high);
}

// Tests that after the low priority observer has been allocated its maximum
// bitrate, the other two observers are still allocated bitrate according to
// their relative bitrate priority.
TEST_F(BitrateAllocatorTest, PriorityRateThreeObserversLowAllocatedToMax) {
  TestBitrateObserver observer_low;
  const double kLowBitratePriority = 2.0;
  const uint32_t kLowMaxBitrate = 10;
  TestBitrateObserver observer_mid;
  const double kMidBitratePriority = 4.0;
  TestBitrateObserver observer_high;
  const double kHighBitratePriority = 8.0;

  const uint32_t kMinBitrate = 0;
  const uint32_t kMaxBitrate = 60;
  const uint32_t kAvailableBitrate = 100;
  // Remaining bitrate knowing that the low priority observer is allocated its
  // max bitrate. We know this because it is allocated 2.0/14.0 (1/7) of the
  // available bitrate, so 70 bps would be sufficient network bandwidth.
  const uint32_t kRemainingBitrate = kAvailableBitrate - kLowMaxBitrate;

  AddObserver(&observer_low, kMinBitrate, kLowMaxBitrate, 0, false,
              kLowBitratePriority);
  AddObserver(&observer_mid, kMinBitrate, kMaxBitrate, 0, false,
              kMidBitratePriority);
  AddObserver(&observer_high, kMinBitrate, kMaxBitrate, 0, false,
              kHighBitratePriority);
  allocator_->OnNetworkEstimateChanged(CreateTargetRateMessage(
      kAvailableBitrate, 0, 0, kDefaultProbingIntervalMs));

  const double kMidFractionAllocated =
      kMidBitratePriority / (kMidBitratePriority + kHighBitratePriority);
  const double kHighFractionAllocated =
      kHighBitratePriority / (kMidBitratePriority + kHighBitratePriority);
  EXPECT_EQ(kLowMaxBitrate, observer_low.last_bitrate_bps_);
  EXPECT_EQ(kMinBitrate + (kRemainingBitrate * kMidFractionAllocated),
            observer_mid.last_bitrate_bps_);
  EXPECT_EQ(kMinBitrate + (kRemainingBitrate * kHighFractionAllocated),
            observer_high.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low);
  allocator_->RemoveObserver(&observer_mid);
  allocator_->RemoveObserver(&observer_high);
}

// Tests that after two observers are allocated bitrate to their max, the
// the remaining observer is allocated what's left appropriately. This test
// handles an edge case where the medium and high observer reach their
// "relative" max allocation  at the same time. The high has 40 to allocate
// above its min, and the mid has 20 to allocate above its min, which scaled
// by their bitrate priority is the same for each.
TEST_F(BitrateAllocatorTest, PriorityRateThreeObserversTwoAllocatedToMax) {
  TestBitrateObserver observer_low;
  TestBitrateObserver observer_mid;
  TestBitrateObserver observer_high;
  AddObserver(&observer_low, 10, 40, 0, false, 2.0);
  // Scaled allocation above the min allocation is the same for these two,
  // meaning they will get allocated  their max at the same time.
  // Scaled (target allocation) = (max - min) / bitrate priority
  AddObserver(&observer_mid, 10, 30, 0, false, 4.0);
  AddObserver(&observer_high, 10, 50, 0, false, 8.0);
  allocator_->OnNetworkEstimateChanged(
      CreateTargetRateMessage(110, 0, 0, kDefaultProbingIntervalMs));

  EXPECT_EQ(30u, observer_low.last_bitrate_bps_);
  EXPECT_EQ(30u, observer_mid.last_bitrate_bps_);
  EXPECT_EQ(50u, observer_high.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low);
  allocator_->RemoveObserver(&observer_mid);
  allocator_->RemoveObserver(&observer_high);
}

}  // namespace webrtc
