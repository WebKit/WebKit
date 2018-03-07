/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>
#include <vector>

#include "call/bitrate_allocator.h"
#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::NiceMock;
using ::testing::_;

namespace webrtc {

class MockLimitObserver : public BitrateAllocator::LimitObserver {
 public:
  MOCK_METHOD2(OnAllocationLimitsChanged,
               void(uint32_t min_send_bitrate_bps,
                    uint32_t max_padding_bitrate_bps));
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

  uint32_t OnBitrateUpdated(uint32_t bitrate_bps,
                            uint8_t fraction_loss,
                            int64_t rtt,
                            int64_t probing_interval_ms) override {
    last_bitrate_bps_ = bitrate_bps;
    last_fraction_loss_ = fraction_loss;
    last_rtt_ms_ = rtt;
    last_probing_interval_ms_ = probing_interval_ms;
    return bitrate_bps * protection_ratio_;
  }
  uint32_t last_bitrate_bps_;
  uint8_t last_fraction_loss_;
  int64_t last_rtt_ms_;
  int last_probing_interval_ms_;
  double protection_ratio_;
};

namespace {
constexpr int64_t kDefaultProbingIntervalMs = 3000;
}

class BitrateAllocatorTest : public ::testing::Test {
 protected:
  BitrateAllocatorTest() : allocator_(new BitrateAllocator(&limit_observer_)) {
    allocator_->OnNetworkChanged(300000u, 0, 0, kDefaultProbingIntervalMs);
  }
  ~BitrateAllocatorTest() {}

  NiceMock<MockLimitObserver> limit_observer_;
  std::unique_ptr<BitrateAllocator> allocator_;
};

TEST_F(BitrateAllocatorTest, UpdatingBitrateObserver) {
  TestBitrateObserver bitrate_observer;
  const uint32_t kMinSendBitrateBps = 100000;
  const uint32_t kPadUpToBitrateBps = 50000;

  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(kMinSendBitrateBps,
                                                         kPadUpToBitrateBps));
  allocator_->AddObserver(&bitrate_observer, kMinSendBitrateBps, 1500000,
                          kPadUpToBitrateBps, true, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer));
  allocator_->OnNetworkChanged(200000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer));

  // TODO(pbos): Expect capping to 1.5M instead of 3M when not boosting the max
  // bitrate for FEC/retransmissions (see todo in BitrateAllocator).
  allocator_->OnNetworkChanged(4000000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(3000000, allocator_->GetStartBitrate(&bitrate_observer));

  // Expect |max_padding_bitrate_bps| to change to 0 if the observer is updated.
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(kMinSendBitrateBps, 0));
  allocator_->AddObserver(&bitrate_observer, kMinSendBitrateBps, 4000000, 0,
                          true, "");
  EXPECT_EQ(4000000, allocator_->GetStartBitrate(&bitrate_observer));

  allocator_->AddObserver(&bitrate_observer, kMinSendBitrateBps, 1500000, 0,
                          true, "");
  EXPECT_EQ(3000000, allocator_->GetStartBitrate(&bitrate_observer));
  EXPECT_EQ(3000000u, bitrate_observer.last_bitrate_bps_);
  allocator_->OnNetworkChanged(1500000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(1500000u, bitrate_observer.last_bitrate_bps_);
}

TEST_F(BitrateAllocatorTest, TwoBitrateObserversOneRtcpObserver) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(100000, 0));
  allocator_->AddObserver(&bitrate_observer_1, 100000, 300000, 0, true, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));
  EXPECT_CALL(limit_observer_,
              OnAllocationLimitsChanged(100000 + 200000, 0));
  allocator_->AddObserver(&bitrate_observer_2, 200000, 300000, 0, true, "");
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer_2));

  // Test too low start bitrate, hence lower than sum of min. Min bitrates
  // will
  // be allocated to all observers.
  allocator_->OnNetworkChanged(200000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0, bitrate_observer_1.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_1.last_rtt_ms_);
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);
  EXPECT_EQ(0, bitrate_observer_2.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_2.last_rtt_ms_);

  // Test a bitrate which should be distributed equally.
  allocator_->OnNetworkChanged(500000, 0, 50, kDefaultProbingIntervalMs);
  const uint32_t kBitrateToShare = 500000 - 200000 - 100000;
  EXPECT_EQ(100000u + kBitrateToShare / 2,
            bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(200000u + kBitrateToShare / 2,
            bitrate_observer_2.last_bitrate_bps_);

  // Limited by 2x max bitrates since we leave room for FEC and
  // retransmissions.
  allocator_->OnNetworkChanged(1500000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(600000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(600000u, bitrate_observer_2.last_bitrate_bps_);

  // Verify that if the bandwidth estimate is set to zero, the allocated
  // rate is
  // zero.
  allocator_->OnNetworkChanged(0, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);
}

TEST_F(BitrateAllocatorTest, RemoveObserverTriggersLimitObserver) {
  TestBitrateObserver bitrate_observer;
  const uint32_t kMinSendBitrateBps = 100000;
  const uint32_t kPadUpToBitrateBps = 50000;

  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(kMinSendBitrateBps,
                                                         kPadUpToBitrateBps));
  allocator_->AddObserver(&bitrate_observer, kMinSendBitrateBps, 1500000,
                          kPadUpToBitrateBps, true, "");
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(0, 0));
  allocator_->RemoveObserver(&bitrate_observer);
}

class BitrateAllocatorTestNoEnforceMin : public ::testing::Test {
 protected:
  BitrateAllocatorTestNoEnforceMin()
      : allocator_(new BitrateAllocator(&limit_observer_)) {
    allocator_->OnNetworkChanged(300000u, 0, 0, kDefaultProbingIntervalMs);
  }
  ~BitrateAllocatorTestNoEnforceMin() {}

  NiceMock<MockLimitObserver> limit_observer_;
  std::unique_ptr<BitrateAllocator> allocator_;
};

// The following three tests verify enforcing a minimum bitrate works as
// intended.
TEST_F(BitrateAllocatorTestNoEnforceMin, OneBitrateObserver) {
  TestBitrateObserver bitrate_observer_1;
  // Expect OnAllocationLimitsChanged with |min_send_bitrate_bps| = 0 since
  // AddObserver is called with |enforce_min_bitrate| = false.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(0, 120000));
  allocator_->AddObserver(&bitrate_observer_1, 100000, 400000, 0, false, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));

  // High BWE.
  allocator_->OnNetworkChanged(150000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(150000u, bitrate_observer_1.last_bitrate_bps_);

  // Low BWE.
  allocator_->OnNetworkChanged(10000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);

  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(0, 0));
  allocator_->RemoveObserver(&bitrate_observer_1);
}

TEST_F(BitrateAllocatorTestNoEnforceMin, ThreeBitrateObservers) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  TestBitrateObserver bitrate_observer_3;
  // Set up the observers with min bitrates at 100000, 200000, and 300000.
  allocator_->AddObserver(&bitrate_observer_1, 100000, 400000, 0, false, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));

  allocator_->AddObserver(&bitrate_observer_2, 200000, 400000, 0, false, "");
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer_2));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);

  allocator_->AddObserver(&bitrate_observer_3, 300000, 400000, 0, false, "");
  EXPECT_EQ(0, allocator_->GetStartBitrate(&bitrate_observer_3));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);

  // High BWE. Make sure the controllers get a fair share of the surplus (i.e.,
  // what is left after each controller gets its min rate).
  allocator_->OnNetworkChanged(690000, 0, 0, kDefaultProbingIntervalMs);
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
  allocator_->OnNetworkChanged(300000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);  // Min bitrate.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);  // Min bitrate.
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_bps_);  // Nothing.

  // Increased BWE, but still below the sum of configured min bitrates for all
  // observers and too little for observer 3. 1 and 2 will share the rest.
  allocator_->OnNetworkChanged(500000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(200000u, bitrate_observer_1.last_bitrate_bps_);  // Min + split.
  EXPECT_EQ(300000u, bitrate_observer_2.last_bitrate_bps_);  // Min + split.
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_bps_);  // Nothing.

  // Below min for all.
  allocator_->OnNetworkChanged(10000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_bps_);

  // Verify that zero estimated bandwidth, means that that all gets zero,
  // regardless of set min bitrate.
  allocator_->OnNetworkChanged(0, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_bps_);

  allocator_->RemoveObserver(&bitrate_observer_1);
  allocator_->RemoveObserver(&bitrate_observer_2);
  allocator_->RemoveObserver(&bitrate_observer_3);
}

TEST_F(BitrateAllocatorTestNoEnforceMin, OneBitrateObserverWithPacketLoss) {
  TestBitrateObserver bitrate_observer;
  // Expect OnAllocationLimitsChanged with |min_send_bitrate_bps| = 0 since
  // AddObserver is called with |enforce_min_bitrate| = false.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(0, 168000));
  allocator_->AddObserver(&bitrate_observer, 100000, 400000, 0, false, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer));

  // High BWE.
  allocator_->OnNetworkChanged(150000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(150000u, bitrate_observer.last_bitrate_bps_);

  // Add loss and use a part of the bitrate for protection.
  double protection_ratio = 0.4;
  uint8_t fraction_loss = protection_ratio * 256;
  bitrate_observer.SetBitrateProtectionRatio(protection_ratio);
  allocator_->OnNetworkChanged(200000, 0, fraction_loss,
                               kDefaultProbingIntervalMs);
  EXPECT_EQ(200000u, bitrate_observer.last_bitrate_bps_);

  // Above the min threshold, but not enough given the protection used.
  allocator_->OnNetworkChanged(139000, 0, fraction_loss,
                               kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, bitrate_observer.last_bitrate_bps_);

  // Verify the hysteresis is added for the protection.
  allocator_->OnNetworkChanged(150000, 0, fraction_loss,
                               kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, bitrate_observer.last_bitrate_bps_);

  // Just enough to enable video again.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(0, 0));
  allocator_->OnNetworkChanged(168000, 0, fraction_loss,
                               kDefaultProbingIntervalMs);
  EXPECT_EQ(168000u, bitrate_observer.last_bitrate_bps_);

  // Remove all protection and make sure video is not paused as earlier.
  bitrate_observer.SetBitrateProtectionRatio(0.0);
  allocator_->OnNetworkChanged(140000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(140000u, bitrate_observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(139000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(139000u, bitrate_observer.last_bitrate_bps_);

  allocator_->RemoveObserver(&bitrate_observer);
}

TEST_F(BitrateAllocatorTestNoEnforceMin, TwoBitrateObserverWithPacketLoss) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;

  allocator_->AddObserver(&bitrate_observer_1, 100000, 400000, 0, false, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));
  allocator_->AddObserver(&bitrate_observer_2, 200000, 400000, 0, false, "");
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer_2));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);

  // Enough bitrate for both.
  bitrate_observer_2.SetBitrateProtectionRatio(0.5);
  allocator_->OnNetworkChanged(300000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);

  // Above min for observer 2, but too little given the protection used.
  allocator_->OnNetworkChanged(330000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(330000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkChanged(100000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkChanged(99999, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkChanged(119000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkChanged(120000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(120000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  // Verify the protection is accounted for before resuming observer 2.
  allocator_->OnNetworkChanged(429000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(400000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->OnNetworkChanged(430000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(330000u, bitrate_observer_2.last_bitrate_bps_);

  allocator_->RemoveObserver(&bitrate_observer_1);
  allocator_->RemoveObserver(&bitrate_observer_2);
}

TEST_F(BitrateAllocatorTest, ThreeBitrateObserversLowBweEnforceMin) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  TestBitrateObserver bitrate_observer_3;

  allocator_->AddObserver(&bitrate_observer_1, 100000, 400000, 0, true, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));

  allocator_->AddObserver(&bitrate_observer_2, 200000, 400000, 0, true, "");
  EXPECT_EQ(200000, allocator_->GetStartBitrate(&bitrate_observer_2));
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);

  allocator_->AddObserver(&bitrate_observer_3, 300000, 400000, 0, true, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_3));
  EXPECT_EQ(100000, static_cast<int>(bitrate_observer_1.last_bitrate_bps_));
  EXPECT_EQ(200000, static_cast<int>(bitrate_observer_2.last_bitrate_bps_));

  // Low BWE. Verify that all observers still get their respective min
  // bitrate.
  allocator_->OnNetworkChanged(1000, 0, 0, kDefaultProbingIntervalMs);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_bps_);  // Min cap.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_bps_);  // Min cap.
  EXPECT_EQ(300000u, bitrate_observer_3.last_bitrate_bps_);  // Min cap.

  allocator_->RemoveObserver(&bitrate_observer_1);
  allocator_->RemoveObserver(&bitrate_observer_2);
  allocator_->RemoveObserver(&bitrate_observer_3);
}

TEST_F(BitrateAllocatorTest, AddObserverWhileNetworkDown) {
  TestBitrateObserver bitrate_observer_1;
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(50000, 0));

  allocator_->AddObserver(&bitrate_observer_1, 50000, 400000, 0, true, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&bitrate_observer_1));

  // Set network down, ie, no available bitrate.
  allocator_->OnNetworkChanged(0, 0, 0, kDefaultProbingIntervalMs);

  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);

  TestBitrateObserver bitrate_observer_2;
  // Adding an observer while the network is down should not affect the limits.
  EXPECT_CALL(limit_observer_, OnAllocationLimitsChanged(50000 + 50000, 0));
  allocator_->AddObserver(&bitrate_observer_2, 50000, 400000, 0, true, "");

  // Expect the start_bitrate to be set as if the network was still up but that
  // the new observer have been notified that the network is down.
  EXPECT_EQ(300000 / 2, allocator_->GetStartBitrate(&bitrate_observer_2));
  EXPECT_EQ(0u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_bps_);

  // Set network back up.
  allocator_->OnNetworkChanged(1500000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(750000u, bitrate_observer_1.last_bitrate_bps_);
  EXPECT_EQ(750000u, bitrate_observer_2.last_bitrate_bps_);
}

TEST_F(BitrateAllocatorTest, MixedEnforecedConfigs) {
  TestBitrateObserver enforced_observer;
  allocator_->AddObserver(&enforced_observer, 6000, 30000, 0, true, "");
  EXPECT_EQ(60000, allocator_->GetStartBitrate(&enforced_observer));

  TestBitrateObserver not_enforced_observer;
  allocator_->AddObserver(&not_enforced_observer, 30000, 2500000, 0, false, "");
  EXPECT_EQ(270000, allocator_->GetStartBitrate(&not_enforced_observer));
  EXPECT_EQ(30000u, enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(36000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(6000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(30000u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(35000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(30000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(0u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(5000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(6000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(0u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(36000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(30000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(0u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(55000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(30000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(0u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(56000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(6000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(50000u, not_enforced_observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(56000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(16000u, enforced_observer.last_bitrate_bps_);
  EXPECT_EQ(40000u, not_enforced_observer.last_bitrate_bps_);

  allocator_->RemoveObserver(&enforced_observer);
  allocator_->RemoveObserver(&not_enforced_observer);
}

TEST_F(BitrateAllocatorTest, AvoidToggleAbsolute) {
  TestBitrateObserver observer;
  allocator_->AddObserver(&observer, 30000, 300000, 0, false, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&observer));

  allocator_->OnNetworkChanged(30000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(30000u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(20000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(30000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(49000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(50000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(50000u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(30000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(30000u, observer.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer);
}

TEST_F(BitrateAllocatorTest, AvoidTogglePercent) {
  TestBitrateObserver observer;
  allocator_->AddObserver(&observer, 300000, 600000, 0, false, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&observer));

  allocator_->OnNetworkChanged(300000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(300000u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(200000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(300000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(329000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(0u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(330000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(330000u, observer.last_bitrate_bps_);

  allocator_->OnNetworkChanged(300000, 0, 50, kDefaultProbingIntervalMs);
  EXPECT_EQ(300000u, observer.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer);
}

TEST_F(BitrateAllocatorTest, PassProbingInterval) {
  TestBitrateObserver observer;
  allocator_->AddObserver(&observer, 300000, 600000, 0, false, "");
  EXPECT_EQ(300000, allocator_->GetStartBitrate(&observer));

  allocator_->OnNetworkChanged(300000, 0, 50, 5000);
  EXPECT_EQ(5000, observer.last_probing_interval_ms_);

  allocator_->RemoveObserver(&observer);
}

TEST_F(BitrateAllocatorTest, PriorityRateOneObserverBasic) {
  TestBitrateObserver observer;
  const uint32_t kMinSendBitrateBps = 10;
  const uint32_t kMaxSendBitrateBps = 60;
  const uint32_t kNetworkBandwidthBps = 30;

  allocator_->AddObserver(&observer, kMinSendBitrateBps, kMaxSendBitrateBps, 0,
                          true, "", 2.0);
  allocator_->OnNetworkChanged(kNetworkBandwidthBps, 0, 0,
                               kDefaultProbingIntervalMs);

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
  allocator_->AddObserver(&observer_low_1, kMinSendBitrateBps,
                          kMaxSendBitrateBps, 0, false, "low1", 2.0);
  allocator_->AddObserver(&observer_low_2, kMinSendBitrateBps,
                          kMaxSendBitrateBps, 0, false, "low2", 2.0);
  allocator_->OnNetworkChanged(kNetworkBandwidthBps, 0, 0,
                               kDefaultProbingIntervalMs);

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
  allocator_->AddObserver(&observer_low_1, kMinSendBitrateBps,
                          kMaxSendBitrateBps, 0, true, "low1", 2.0);
  allocator_->AddObserver(&observer_low_2, kMinSendBitrateBps,
                          kMaxSendBitrateBps, 0, true, "low2", 2.0);
  allocator_->OnNetworkChanged(kNetworkBandwidthBps, 0, 0,
                               kDefaultProbingIntervalMs);

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
  allocator_->AddObserver(&observer_low, kMinSendBitrateBps, kMaxSendBitrateBps,
                          0, true, "low", 2.0);
  allocator_->AddObserver(&observer_mid, kMinSendBitrateBps, kMaxSendBitrateBps,
                          0, true, "mid", 4.0);
  allocator_->OnNetworkChanged(kNetworkBandwidthBps, 0, 0,
                               kDefaultProbingIntervalMs);

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
  allocator_->AddObserver(&observer_low, 10, 50, 0, false, "low", 2.0);
  allocator_->AddObserver(&observer_mid, 10, 50, 0, false, "mid", 4.0);
  allocator_->OnNetworkChanged(90, 0, 0, kDefaultProbingIntervalMs);

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
  allocator_->AddObserver(&observer_low, 0, kMaxBitrate, 0, false, "low",
                          kLowBitratePriority);
  allocator_->AddObserver(&observer_mid, 0, kMaxBitrate, 0, false, "mid",
                          kMidBitratePriority);
  allocator_->AddObserver(&observer_high, 0, kMaxBitrate, 0, false, "high",
                          kHighBitratePriority);
  allocator_->OnNetworkChanged(kNetworkBandwidthBps, 0, 0,
                               kDefaultProbingIntervalMs);

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

  allocator_->AddObserver(&observer_low, kMinBitrate, kMaxBitrate, 0, false,
                          "low", kLowBitratePriority);
  allocator_->AddObserver(&observer_mid, kMinBitrate, kMaxBitrate, 0, false,
                          "mid", kMidBitratePriority);
  allocator_->AddObserver(&observer_high, kMinBitrate, kMaxBitrate, 0, false,
                          "high", kHighBitratePriority);
  allocator_->OnNetworkChanged(kAvailableBitrate, 0, 0,
                               kDefaultProbingIntervalMs);

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

  allocator_->AddObserver(&observer_low, kMinBitrate, kLowMaxBitrate, 0, false,
                          "low", kLowBitratePriority);
  allocator_->AddObserver(&observer_mid, kMinBitrate, kMaxBitrate, 0, false,
                          "mid", kMidBitratePriority);
  allocator_->AddObserver(&observer_high, kMinBitrate, kMaxBitrate, 0, false,
                          "high", kHighBitratePriority);
  allocator_->OnNetworkChanged(kAvailableBitrate, 0, 0,
                               kDefaultProbingIntervalMs);

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
  allocator_->AddObserver(&observer_low, 10, 40, 0, false, "low", 2.0);
  // Scaled allocation above the min allocation is the same for these two,
  // meaning they will get allocated  their max at the same time.
  // Scaled (target allocation) = (max - min) / bitrate priority
  allocator_->AddObserver(&observer_mid, 10, 30, 0, false, "mid", 4.0);
  allocator_->AddObserver(&observer_high, 10, 50, 0, false, "high", 8.0);
  allocator_->OnNetworkChanged(110, 0, 0, kDefaultProbingIntervalMs);

  EXPECT_EQ(30u, observer_low.last_bitrate_bps_);
  EXPECT_EQ(30u, observer_mid.last_bitrate_bps_);
  EXPECT_EQ(50u, observer_high.last_bitrate_bps_);

  allocator_->RemoveObserver(&observer_low);
  allocator_->RemoveObserver(&observer_mid);
  allocator_->RemoveObserver(&observer_high);
}

}  // namespace webrtc
