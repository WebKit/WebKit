/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <memory>

#include "webrtc/base/logging.h"
#include "webrtc/modules/congestion_controller/probe_controller.h"
#include "webrtc/modules/pacing/mock/mock_paced_sender.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;

namespace webrtc {
namespace test {

namespace {

constexpr int kMinBitrateBps = 100;
constexpr int kStartBitrateBps = 300;
constexpr int kMaxBitrateBps = 10000;

constexpr int kExponentialProbingTimeoutMs = 5000;

}  // namespace

class ProbeControllerTest : public ::testing::Test {
 protected:
  ProbeControllerTest() : clock_(0) {
    probe_controller_.reset(new ProbeController(&pacer_, &clock_));
  }
  ~ProbeControllerTest() override {}

  SimulatedClock clock_;
  NiceMock<MockPacedSender> pacer_;
  std::unique_ptr<ProbeController> probe_controller_;
};

TEST_F(ProbeControllerTest, InitiatesProbingAtStart) {
  EXPECT_CALL(pacer_, CreateProbeCluster(_, _)).Times(AtLeast(2));
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps);
}

TEST_F(ProbeControllerTest, InitiatesProbingOnMaxBitrateIncrease) {
  EXPECT_CALL(pacer_, CreateProbeCluster(_, _)).Times(AtLeast(2));
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps);
  // Long enough to time out exponential probing.
  clock_.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  probe_controller_->SetEstimatedBitrate(kStartBitrateBps);

  EXPECT_CALL(pacer_, CreateProbeCluster(kMaxBitrateBps + 100, _));
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps + 100);
}

TEST_F(ProbeControllerTest, TestExponentialProbing) {
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps);



  EXPECT_CALL(pacer_, CreateProbeCluster(2 * 1800, _));
  probe_controller_->SetEstimatedBitrate(1800);
}

TEST_F(ProbeControllerTest, TestExponentialProbingTimeout) {
  probe_controller_->SetBitrates(kMinBitrateBps, kStartBitrateBps,
                                 kMaxBitrateBps);

  // Advance far enough to cause a time out in waiting for probing result.
  clock_.AdvanceTimeMilliseconds(kExponentialProbingTimeoutMs);
  EXPECT_CALL(pacer_, CreateProbeCluster(2 * 1800, _)).Times(0);
  probe_controller_->SetEstimatedBitrate(1800);
}

}  // namespace test
}  // namespace webrtc
