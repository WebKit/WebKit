/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/base/socket.h"
#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/congestion_controller_unittests_helper.h"
#include "webrtc/modules/congestion_controller/include/mock/mock_congestion_observer.h"
#include "webrtc/modules/pacing/mock/mock_paced_sender.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace webrtc {

namespace {
const webrtc::PacedPacketInfo kPacingInfo0(0, 5, 2000);
const webrtc::PacedPacketInfo kPacingInfo1(1, 8, 4000);

// Helper to convert some time format to resolution used in absolute send time
// header extension, rounded upwards. |t| is the time to convert, in some
// resolution. |denom| is the value to divide |t| by to get whole seconds,
// e.g. |denom| = 1000 if |t| is in milliseconds.
uint32_t AbsSendTime(int64_t t, int64_t denom) {
  return (((t << 18) + (denom >> 1)) / denom) & 0x00fffffful;
}

class MockPacketRouter : public PacketRouter {
 public:
  MOCK_METHOD2(OnReceiveBitrateChanged,
               void(const std::vector<uint32_t>& ssrcs,
                    uint32_t bitrate));
};

const uint32_t kInitialBitrateBps = 60000;

}  // namespace

namespace test {

class CongestionControllerTest : public ::testing::Test {
 protected:
  CongestionControllerTest() : clock_(123456), target_bitrate_observer_(this) {}
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
    EXPECT_CALL(*pacer_, CreateProbeCluster(kInitialBitrateBps * 3));
    EXPECT_CALL(*pacer_, CreateProbeCluster(kInitialBitrateBps * 5));
    controller_->SetBweBitrates(0, kInitialBitrateBps, 5 * kInitialBitrateBps);
  }

  // Custom setup - use an observer that tracks the target bitrate, without
  // prescribing on which iterations it must change (like a mock would).
  void TargetBitrateTrackingSetup() {
    std::unique_ptr<PacedSender> pacer(new NiceMock<MockPacedSender>());
    controller_.reset(new CongestionController(
        &clock_, &target_bitrate_observer_, &remote_bitrate_observer_,
        &event_log_, &packet_router_, std::move(pacer)));
    controller_->SetBweBitrates(0, kInitialBitrateBps, 5 * kInitialBitrateBps);
  }

  void OnSentPacket(const PacketFeedback& packet_feedback) {
    constexpr uint32_t ssrc = 0;
    controller_->AddPacket(ssrc, packet_feedback.sequence_number,
                           packet_feedback.payload_size,
                           packet_feedback.pacing_info);
    controller_->OnSentPacket(rtc::SentPacket(packet_feedback.sequence_number,
                                              packet_feedback.send_time_ms));
  }

  // Allows us to track the target bitrate, without prescribing the exact
  // iterations when this would hapen, like a mock would.
  class TargetBitrateObserver : public CongestionController::Observer {
   public:
    explicit TargetBitrateObserver(CongestionControllerTest* owner)
        : owner_(owner) {}
    ~TargetBitrateObserver() override = default;
    void OnNetworkChanged(uint32_t bitrate_bps,
                          uint8_t fraction_loss,  // 0 - 255.
                          int64_t rtt_ms,
                          int64_t probing_interval_ms) override {
      owner_->target_bitrate_bps_ = rtc::Optional<uint32_t>(bitrate_bps);
    }

   private:
    CongestionControllerTest* owner_;
  };

  void PacketTransmissionAndFeedbackBlock(uint16_t* seq_num,
                                          int64_t runtime_ms,
                                          int64_t delay) {
    int64_t delay_buildup = 0;
    int64_t start_time_ms = clock_.TimeInMilliseconds();
    while (clock_.TimeInMilliseconds() - start_time_ms < runtime_ms) {
      constexpr size_t kPayloadSize = 1000;
      PacketFeedback packet(clock_.TimeInMilliseconds() + delay_buildup,
                            clock_.TimeInMilliseconds(), *seq_num, kPayloadSize,
                            PacedPacketInfo());
      delay_buildup += delay;  // Delay has to increase, or it's just RTT.
      OnSentPacket(packet);
      // Create expected feedback and send into adapter.
      std::unique_ptr<rtcp::TransportFeedback> feedback(
          new rtcp::TransportFeedback());
      feedback->SetBase(packet.sequence_number, packet.arrival_time_ms * 1000);
      EXPECT_TRUE(feedback->AddReceivedPacket(packet.sequence_number,
                                              packet.arrival_time_ms * 1000));
      rtc::Buffer raw_packet = feedback->Build();
      feedback = rtcp::TransportFeedback::ParseFrom(raw_packet.data(),
                                                    raw_packet.size());
      EXPECT_TRUE(feedback.get() != nullptr);
      controller_->OnTransportFeedback(*feedback.get());
      clock_.AdvanceTimeMilliseconds(50);
      controller_->Process();
      ++(*seq_num);
    }
  }

  SimulatedClock clock_;
  StrictMock<MockCongestionObserver> observer_;
  TargetBitrateObserver target_bitrate_observer_;
  NiceMock<MockPacedSender>* pacer_;
  NiceMock<MockRemoteBitrateObserver> remote_bitrate_observer_;
  NiceMock<MockRtcEventLog> event_log_;
  std::unique_ptr<RtcpBandwidthObserver> bandwidth_observer_;
  PacketRouter packet_router_;
  std::unique_ptr<CongestionController> controller_;

  rtc::Optional<uint32_t> target_bitrate_bps_;
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

TEST_F(CongestionControllerTest, OnNetworkRouteChanged) {
  int new_bitrate = 200000;
  testing::Mock::VerifyAndClearExpectations(pacer_);
  EXPECT_CALL(observer_, OnNetworkChanged(new_bitrate, _, _, _));
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(new_bitrate));
  rtc::NetworkRoute route;
  route.local_network_id = 1;
  controller_->OnNetworkRouteChanged(route, new_bitrate, -1, -1);

  // If the bitrate is reset to -1, the new starting bitrate will be
  // the minimum default bitrate kMinBitrateBps.
  EXPECT_CALL(
      observer_,
      OnNetworkChanged(congestion_controller::GetMinBitrateBps(), _, _, _));
  EXPECT_CALL(*pacer_,
              SetEstimatedBitrate(congestion_controller::GetMinBitrateBps()));
  route.local_network_id = 2;
  controller_->OnNetworkRouteChanged(route, -1, -1, -1);
}

TEST_F(CongestionControllerTest, OldFeedback) {
  int new_bitrate = 200000;
  testing::Mock::VerifyAndClearExpectations(pacer_);
  EXPECT_CALL(observer_, OnNetworkChanged(new_bitrate, _, _, _));
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(new_bitrate));

  // Send a few packets on the first network route.
  std::vector<PacketFeedback> packets;
  packets.push_back(PacketFeedback(0, 0, 0, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(10, 10, 1, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(20, 20, 2, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(30, 30, 3, 1500, kPacingInfo1));
  packets.push_back(PacketFeedback(40, 40, 4, 1500, kPacingInfo1));

  for (const PacketFeedback& packet : packets)
    OnSentPacket(packet);

  // Change route and then insert a number of feedback packets.
  rtc::NetworkRoute route;
  route.local_network_id = 1;
  controller_->OnNetworkRouteChanged(route, new_bitrate, -1, -1);

  for (const PacketFeedback& packet : packets) {
    rtcp::TransportFeedback feedback;
    feedback.SetBase(packet.sequence_number, packet.arrival_time_ms * 1000);

    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
    feedback.Build();
    controller_->OnTransportFeedback(feedback);
  }

  // If the bitrate is reset to -1, the new starting bitrate will be
  // the minimum default bitrate kMinBitrateBps.
  EXPECT_CALL(
      observer_,
      OnNetworkChanged(congestion_controller::GetMinBitrateBps(), _, _, _));
  EXPECT_CALL(*pacer_,
              SetEstimatedBitrate(congestion_controller::GetMinBitrateBps()));
  route.local_network_id = 2;
  controller_->OnNetworkRouteChanged(route, -1, -1, -1);
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

TEST(ReceiveSideCongestionControllerTest, OnReceivedPacketWithAbsSendTime) {
  StrictMock<MockPacketRouter> packet_router;
  SimulatedClock clock_(123456);

  ReceiveSideCongestionController controller(&clock_, &packet_router);

  size_t payload_size = 1000;
  RTPHeader header;
  header.ssrc = 0x11eb21c;
  header.extension.hasAbsoluteSendTime = true;

  std::vector<unsigned int> ssrcs;
  EXPECT_CALL(packet_router, OnReceiveBitrateChanged(_, _))
      .WillRepeatedly(SaveArg<0>(&ssrcs));

  for (int i = 0; i < 10; ++i) {
    clock_.AdvanceTimeMilliseconds((1000 * payload_size) / kInitialBitrateBps);
    int64_t now_ms = clock_.TimeInMilliseconds();
    header.extension.absoluteSendTime = AbsSendTime(now_ms, 1000);
    controller.OnReceivedPacket(now_ms, payload_size, header);
  }

  ASSERT_EQ(1u, ssrcs.size());
  EXPECT_EQ(header.ssrc, ssrcs[0]);
}

TEST_F(CongestionControllerTest, ProbeOnRouteChange) {
  testing::Mock::VerifyAndClearExpectations(pacer_);
  EXPECT_CALL(*pacer_, CreateProbeCluster(kInitialBitrateBps * 6));
  EXPECT_CALL(*pacer_, CreateProbeCluster(kInitialBitrateBps * 12));
  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps * 2, _, _, _));
  rtc::NetworkRoute route;
  route.local_network_id = 1;
  controller_->OnNetworkRouteChanged(route, 2 * kInitialBitrateBps, 0,
                                     20 * kInitialBitrateBps);
}

// Estimated bitrate reduced when the feedbacks arrive with such a long delay,
// that the send-time-history no longer holds the feedbacked packets.
TEST_F(CongestionControllerTest, LongFeedbackDelays) {
  TargetBitrateTrackingSetup();

  const int64_t kFeedbackTimeoutMs = 60001;
  const int kMaxConsecutiveFailedLookups = 5;
  for (int i = 0; i < kMaxConsecutiveFailedLookups; ++i) {
    std::vector<PacketFeedback> packets;
    packets.push_back(
        PacketFeedback(i * 100, 2 * i * 100, 0, 1500, kPacingInfo0));
    packets.push_back(
        PacketFeedback(i * 100 + 10, 2 * i * 100 + 10, 1, 1500, kPacingInfo0));
    packets.push_back(
        PacketFeedback(i * 100 + 20, 2 * i * 100 + 20, 2, 1500, kPacingInfo0));
    packets.push_back(
        PacketFeedback(i * 100 + 30, 2 * i * 100 + 30, 3, 1500, kPacingInfo1));
    packets.push_back(
        PacketFeedback(i * 100 + 40, 2 * i * 100 + 40, 4, 1500, kPacingInfo1));

    for (const PacketFeedback& packet : packets)
      OnSentPacket(packet);

    rtcp::TransportFeedback feedback;
    feedback.SetBase(packets[0].sequence_number,
                     packets[0].arrival_time_ms * 1000);

    for (const PacketFeedback& packet : packets) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                             packet.arrival_time_ms * 1000));
    }

    feedback.Build();

    clock_.AdvanceTimeMilliseconds(kFeedbackTimeoutMs);
    PacketFeedback later_packet(kFeedbackTimeoutMs + i * 100 + 40,
                                kFeedbackTimeoutMs + i * 200 + 40, 5, 1500,
                                kPacingInfo1);
    OnSentPacket(later_packet);

    controller_->OnTransportFeedback(feedback);

    // Check that packets have timed out.
    for (PacketFeedback& packet : packets) {
      packet.send_time_ms = -1;
      packet.payload_size = 0;
      packet.pacing_info = PacedPacketInfo();
    }
    ComparePacketFeedbackVectors(packets,
                                 controller_->GetTransportFeedbackVector());
  }

  controller_->Process();

  EXPECT_EQ(kInitialBitrateBps / 2, target_bitrate_bps_);

  // Test with feedback that isn't late enough to time out.
  {
    std::vector<PacketFeedback> packets;
    packets.push_back(PacketFeedback(100, 200, 0, 1500, kPacingInfo0));
    packets.push_back(PacketFeedback(110, 210, 1, 1500, kPacingInfo0));
    packets.push_back(PacketFeedback(120, 220, 2, 1500, kPacingInfo0));
    packets.push_back(PacketFeedback(130, 230, 3, 1500, kPacingInfo1));
    packets.push_back(PacketFeedback(140, 240, 4, 1500, kPacingInfo1));

    for (const PacketFeedback& packet : packets)
      OnSentPacket(packet);

    rtcp::TransportFeedback feedback;
    feedback.SetBase(packets[0].sequence_number,
                     packets[0].arrival_time_ms * 1000);

    for (const PacketFeedback& packet : packets) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                             packet.arrival_time_ms * 1000));
    }

    feedback.Build();

    clock_.AdvanceTimeMilliseconds(kFeedbackTimeoutMs - 1);
    PacketFeedback later_packet(kFeedbackTimeoutMs + 140,
                                kFeedbackTimeoutMs + 240, 5, 1500,
                                kPacingInfo1);
    OnSentPacket(later_packet);

    controller_->OnTransportFeedback(feedback);
    ComparePacketFeedbackVectors(packets,
                                 controller_->GetTransportFeedbackVector());
  }
}

// Bandwidth estimation is updated when feedbacks are received.
// Feedbacks which show an increasing delay cause the estimation to be reduced.
TEST_F(CongestionControllerTest, UpdatesDelayBasedEstimate) {
  TargetBitrateTrackingSetup();

  const int64_t kRunTimeMs = 6000;
  uint16_t seq_num = 0;

  // The test must run and insert packets/feedback long enough that the
  // BWE computes a valid estimate. This is first done in an environment which
  // simulates no bandwidth limitation, and therefore not built-up delay.
  PacketTransmissionAndFeedbackBlock(&seq_num, kRunTimeMs, 0);
  ASSERT_TRUE(target_bitrate_bps_);

  // Repeat, but this time with a building delay, and make sure that the
  // estimation is adjusted downwards.
  uint32_t bitrate_before_delay = *target_bitrate_bps_;
  PacketTransmissionAndFeedbackBlock(&seq_num, kRunTimeMs, 50);
  EXPECT_LT(*target_bitrate_bps_, bitrate_before_delay);
}
}  // namespace test
}  // namespace webrtc
