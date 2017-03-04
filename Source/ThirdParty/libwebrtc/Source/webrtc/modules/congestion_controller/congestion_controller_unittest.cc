/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/congestion_controller/include/mock/mock_congestion_controller.h"
#include "webrtc/modules/pacing/mock/mock_paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace {

// Helper to convert some time format to resolution used in absolute send time
// header extension, rounded upwards. |t| is the time to convert, in some
// resolution. |denom| is the value to divide |t| by to get whole seconds,
// e.g. |denom| = 1000 if |t| is in milliseconds.
uint32_t AbsSendTime(int64_t t, int64_t denom) {
  return (((t << 18) + (denom >> 1)) / denom) & 0x00fffffful;
}

}  // namespace

namespace webrtc {
namespace test {

class CongestionControllerTest : public ::testing::Test {
 protected:
  CongestionControllerTest() : clock_(123456) {}
  ~CongestionControllerTest() override {}

  void SetUp() override {
    pacer_ = new NiceMock<MockPacedSender>();
    std::unique_ptr<PacedSender> pacer(pacer_);  // Passes ownership.
    controller_.reset(new CongestionController(
        &clock_, &observer_, &remote_bitrate_observer_, &event_log_,
        &packet_router_, std::move(pacer)));
    bandwidth_observer_.reset(
        controller_->GetBitrateController()->CreateRtcpBandwidthObserver());

    // Set the initial bitrate estimate and expect the |observer| and |pacer_|
    // to be updated.
    EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps, _, _, _));
    EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps));
    controller_->SetBweBitrates(0, kInitialBitrateBps, 5 * kInitialBitrateBps);
  }

  SimulatedClock clock_;
  StrictMock<MockCongestionObserver> observer_;
  NiceMock<MockPacedSender>* pacer_;
  NiceMock<MockRemoteBitrateObserver> remote_bitrate_observer_;
  NiceMock<MockRtcEventLog> event_log_;
  std::unique_ptr<RtcpBandwidthObserver> bandwidth_observer_;
  PacketRouter packet_router_;
  std::unique_ptr<CongestionController> controller_;
  const uint32_t kInitialBitrateBps = 60000;
};

TEST_F(CongestionControllerTest, OnNetworkChanged) {
  // Test no change.
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps * 2, _, _, _));
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps * 2));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps * 2);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps, _, _, _));
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();
}

TEST_F(CongestionControllerTest, OnSendQueueFull) {
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs + 1));

  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _, _));
  controller_->Process();

  // Let the pacer not be full next time the controller checks.
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs - 1));

  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps, _, _, _));
  controller_->Process();
}

TEST_F(CongestionControllerTest, OnSendQueueFullAndEstimateChange) {
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs + 1));
  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _, _));
  controller_->Process();

  // Receive new estimate but let the queue still be full.
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps * 2);
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs + 1));
  //  The send pacer should get the new estimate though.
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps * 2));
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  // Let the pacer not be full next time the controller checks.
  // |OnNetworkChanged| should be called with the new estimate.
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs - 1));
  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps * 2, _, _, _));
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();
}

TEST_F(CongestionControllerTest, SignalNetworkState) {
  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _, _));
  controller_->SignalNetworkState(kNetworkDown);

  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps, _, _, _));
  controller_->SignalNetworkState(kNetworkUp);

  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _, _));
  controller_->SignalNetworkState(kNetworkDown);
}

TEST_F(CongestionControllerTest, ResetBweAndBitrates) {
  int new_bitrate = 200000;
  EXPECT_CALL(observer_, OnNetworkChanged(new_bitrate, _, _, _));
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(new_bitrate));
  controller_->ResetBweAndBitrates(new_bitrate, -1, -1);

  // If the bitrate is reset to -1, the new starting bitrate will be
  // the minimum default bitrate kMinBitrateBps.
  EXPECT_CALL(
      observer_,
      OnNetworkChanged(congestion_controller::GetMinBitrateBps(), _, _, _));
  EXPECT_CALL(*pacer_,
              SetEstimatedBitrate(congestion_controller::GetMinBitrateBps()));
  controller_->ResetBweAndBitrates(-1, -1, -1);
}

TEST_F(CongestionControllerTest,
       SignalNetworkStateAndQueueIsFullAndEstimateChange) {
  // Send queue is full
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillRepeatedly(Return(PacedSender::kMaxQueueLengthMs + 1));
  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _, _));
  controller_->Process();

  // Queue is full and network is down. Expect no bitrate change.
  controller_->SignalNetworkState(kNetworkDown);
  controller_->Process();

  // Queue is full but network is up. Expect no bitrate change.
  controller_->SignalNetworkState(kNetworkUp);
  controller_->Process();

  // Receive new estimate but let the queue still be full.
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps * 2));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps * 2);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  // Let the pacer not be full next time the controller checks.
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs - 1));
  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps * 2, _, _, _));
  controller_->Process();
}

TEST_F(CongestionControllerTest, GetPacerQueuingDelayMs) {
  EXPECT_CALL(observer_, OnNetworkChanged(_, _, _, _)).Times(AtLeast(1));

  const int64_t kQueueTimeMs = 123;
  EXPECT_CALL(*pacer_, QueueInMs()).WillRepeatedly(Return(kQueueTimeMs));
  EXPECT_EQ(kQueueTimeMs, controller_->GetPacerQueuingDelayMs());

  // Expect zero pacer delay when network is down.
  controller_->SignalNetworkState(kNetworkDown);
  EXPECT_EQ(0, controller_->GetPacerQueuingDelayMs());

  // Network is up, pacer delay should be reported.
  controller_->SignalNetworkState(kNetworkUp);
  EXPECT_EQ(kQueueTimeMs, controller_->GetPacerQueuingDelayMs());
}

TEST_F(CongestionControllerTest, GetProbingInterval) {
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  EXPECT_CALL(observer_, OnNetworkChanged(_, _, _, testing::Ne(0)));
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(_));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps * 2);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();
}

TEST_F(CongestionControllerTest, OnReceivedPacketWithAbsSendTime) {
  NiceMock<MockCongestionObserver> observer;
  StrictMock<MockRemoteBitrateObserver> remote_bitrate_observer;
  std::unique_ptr<PacedSender> pacer(new NiceMock<MockPacedSender>());
  controller_.reset(
      new CongestionController(&clock_, &observer, &remote_bitrate_observer,
                               &event_log_, &packet_router_, std::move(pacer)));

  size_t payload_size = 1000;
  RTPHeader header;
  header.ssrc = 0x11eb21c;
  header.extension.hasAbsoluteSendTime = true;

  std::vector<unsigned int> ssrcs;
  EXPECT_CALL(remote_bitrate_observer, OnReceiveBitrateChanged(_, _))
      .WillRepeatedly(SaveArg<0>(&ssrcs));

  for (int i = 0; i < 10; ++i) {
    clock_.AdvanceTimeMilliseconds((1000 * payload_size) / kInitialBitrateBps);
    int64_t now_ms = clock_.TimeInMilliseconds();
    header.extension.absoluteSendTime = AbsSendTime(now_ms, 1000);
    controller_->OnReceivedPacket(now_ms, payload_size, header);
  }

  ASSERT_EQ(1u, ssrcs.size());
  EXPECT_EQ(header.ssrc, ssrcs[0]);
}
}  // namespace test
}  // namespace webrtc
