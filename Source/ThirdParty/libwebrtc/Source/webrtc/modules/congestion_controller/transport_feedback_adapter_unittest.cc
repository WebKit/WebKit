/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits>
#include <memory>
#include <vector>

#include "modules/bitrate_controller/include/mock/mock_bitrate_controller.h"
#include "modules/congestion_controller/congestion_controller_unittests_helper.h"
#include "modules/congestion_controller/transport_feedback_adapter.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace webrtc {

namespace {
const PacedPacketInfo kPacingInfo0(0, 5, 2000);
const PacedPacketInfo kPacingInfo1(1, 8, 4000);
const PacedPacketInfo kPacingInfo2(2, 14, 7000);
const PacedPacketInfo kPacingInfo3(3, 20, 10000);
const PacedPacketInfo kPacingInfo4(4, 22, 10000);
}

namespace test {

class MockPacketFeedbackObserver : public webrtc::PacketFeedbackObserver {
 public:
  MOCK_METHOD2(OnPacketAdded, void(uint32_t ssrc, uint16_t seq_num));
  MOCK_METHOD1(OnPacketFeedbackVector,
               void(const std::vector<PacketFeedback>& packet_feedback_vector));
};

class TransportFeedbackAdapterTest : public ::testing::Test {
 public:
  TransportFeedbackAdapterTest() : clock_(0) {}

  virtual ~TransportFeedbackAdapterTest() {}

  virtual void SetUp() {
    adapter_.reset(new TransportFeedbackAdapter(&clock_));
  }

  virtual void TearDown() { adapter_.reset(); }

 protected:
  void OnReceivedEstimatedBitrate(uint32_t bitrate) {}

  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) {}

  void OnSentPacket(const PacketFeedback& packet_feedback) {
    adapter_->AddPacket(kSsrc, packet_feedback.sequence_number,
                        packet_feedback.payload_size,
                        packet_feedback.pacing_info);
    adapter_->OnSentPacket(packet_feedback.sequence_number,
                           packet_feedback.send_time_ms);
  }

  static constexpr uint32_t kSsrc = 8492;

  SimulatedClock clock_;
  std::unique_ptr<TransportFeedbackAdapter> adapter_;
};

TEST_F(TransportFeedbackAdapterTest, ObserverSanity) {
  MockPacketFeedbackObserver mock;
  adapter_->RegisterPacketFeedbackObserver(&mock);

  const std::vector<PacketFeedback> packets = {
      PacketFeedback(100, 200, 0, 1000, kPacingInfo0),
      PacketFeedback(110, 210, 1, 2000, kPacingInfo0),
      PacketFeedback(120, 220, 2, 3000, kPacingInfo0)
  };

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    EXPECT_CALL(mock, OnPacketAdded(kSsrc, packet.sequence_number)).Times(1);
    OnSentPacket(packet);
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  EXPECT_CALL(mock, OnPacketFeedbackVector(_)).Times(1);
  adapter_->OnTransportFeedback(feedback);

  adapter_->DeRegisterPacketFeedbackObserver(&mock);

  // After deregistration, the observer no longers gets indications.
  EXPECT_CALL(mock, OnPacketAdded(_, _)).Times(0);
  const PacketFeedback new_packet(130, 230, 3, 4000, kPacingInfo0);
  OnSentPacket(new_packet);

  rtcp::TransportFeedback second_feedback;
  second_feedback.SetBase(new_packet.sequence_number,
                          new_packet.arrival_time_ms * 1000);
  EXPECT_TRUE(feedback.AddReceivedPacket(new_packet.sequence_number,
                                         new_packet.arrival_time_ms * 1000));
  EXPECT_CALL(mock, OnPacketFeedbackVector(_)).Times(0);
  adapter_->OnTransportFeedback(second_feedback);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(TransportFeedbackAdapterTest, ObserverDoubleRegistrationDeathTest) {
  MockPacketFeedbackObserver mock;
  adapter_->RegisterPacketFeedbackObserver(&mock);
  EXPECT_DEATH(adapter_->RegisterPacketFeedbackObserver(&mock), "");
  adapter_->DeRegisterPacketFeedbackObserver(&mock);
}

TEST_F(TransportFeedbackAdapterTest, ObserverMissingDeRegistrationDeathTest) {
  MockPacketFeedbackObserver mock;
  adapter_->RegisterPacketFeedbackObserver(&mock);
  EXPECT_DEATH(adapter_.reset(), "");
  adapter_->DeRegisterPacketFeedbackObserver(&mock);
}
#endif

TEST_F(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
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

  adapter_->OnTransportFeedback(feedback);
  ComparePacketFeedbackVectors(packets, adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, FeedbackVectorReportsUnreceived) {
  std::vector<PacketFeedback> sent_packets = {
      PacketFeedback(100, 220, 0, 1500, kPacingInfo0),
      PacketFeedback(110, 210, 1, 1500, kPacingInfo0),
      PacketFeedback(120, 220, 2, 1500, kPacingInfo0),
      PacketFeedback(130, 230, 3, 1500, kPacingInfo0),
      PacketFeedback(140, 240, 4, 1500, kPacingInfo0),
      PacketFeedback(150, 250, 5, 1500, kPacingInfo0),
      PacketFeedback(160, 260, 6, 1500, kPacingInfo0)
  };

  for (const PacketFeedback& packet : sent_packets)
    OnSentPacket(packet);

  // Note: Important to include the last packet, as only unreceived packets in
  // between received packets can be inferred.
  std::vector<PacketFeedback> received_packets = {
    sent_packets[0], sent_packets[2], sent_packets[6]
  };

  rtcp::TransportFeedback feedback;
  feedback.SetBase(received_packets[0].sequence_number,
                   received_packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : received_packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                           packet.arrival_time_ms * 1000));
  }

  feedback.Build();

  adapter_->OnTransportFeedback(feedback);
  ComparePacketFeedbackVectors(sent_packets,
                               adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, HandlesDroppedPackets) {
  std::vector<PacketFeedback> packets;
  packets.push_back(PacketFeedback(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(110, 210, 1, 1500, kPacingInfo1));
  packets.push_back(PacketFeedback(120, 220, 2, 1500, kPacingInfo2));
  packets.push_back(PacketFeedback(130, 230, 3, 1500, kPacingInfo3));
  packets.push_back(PacketFeedback(140, 240, 4, 1500, kPacingInfo4));

  const uint16_t kSendSideDropBefore = 1;
  const uint16_t kReceiveSideDropAfter = 3;

  for (const PacketFeedback& packet : packets) {
    if (packet.sequence_number >= kSendSideDropBefore)
      OnSentPacket(packet);
  }

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sequence_number,
                   packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : packets) {
    if (packet.sequence_number <= kReceiveSideDropAfter) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.sequence_number,
                                             packet.arrival_time_ms * 1000));
    }
  }

  feedback.Build();

  std::vector<PacketFeedback> expected_packets(
      packets.begin(), packets.begin() + kReceiveSideDropAfter + 1);
  // Packets that have timed out on the send-side have lost the
  // information stored on the send-side.
  for (size_t i = 0; i < kSendSideDropBefore; ++i) {
    expected_packets[i].send_time_ms = -1;
    expected_packets[i].payload_size = 0;
    expected_packets[i].pacing_info = PacedPacketInfo();
  }

  adapter_->OnTransportFeedback(feedback);
  ComparePacketFeedbackVectors(expected_packets,
                               adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, SendTimeWrapsBothWays) {
  int64_t kHighArrivalTimeMs = rtcp::TransportFeedback::kDeltaScaleFactor *
                               static_cast<int64_t>(1 << 8) *
                               static_cast<int64_t>((1 << 23) - 1) / 1000;
  std::vector<PacketFeedback> packets;
  packets.push_back(
      PacketFeedback(kHighArrivalTimeMs - 64, 200, 0, 1500, PacedPacketInfo()));
  packets.push_back(
      PacketFeedback(kHighArrivalTimeMs + 64, 210, 1, 1500, PacedPacketInfo()));
  packets.push_back(
      PacketFeedback(kHighArrivalTimeMs, 220, 2, 1500, PacedPacketInfo()));

  for (const PacketFeedback& packet : packets)
    OnSentPacket(packet);

  for (size_t i = 0; i < packets.size(); ++i) {
    std::unique_ptr<rtcp::TransportFeedback> feedback(
        new rtcp::TransportFeedback());
    feedback->SetBase(packets[i].sequence_number,
                      packets[i].arrival_time_ms * 1000);

    EXPECT_TRUE(feedback->AddReceivedPacket(packets[i].sequence_number,
                                            packets[i].arrival_time_ms * 1000));

    rtc::Buffer raw_packet = feedback->Build();
    feedback = rtcp::TransportFeedback::ParseFrom(raw_packet.data(),
                                                  raw_packet.size());

    std::vector<PacketFeedback> expected_packets;
    expected_packets.push_back(packets[i]);

    adapter_->OnTransportFeedback(*feedback.get());
    ComparePacketFeedbackVectors(expected_packets,
                                 adapter_->GetTransportFeedbackVector());
  }
}

TEST_F(TransportFeedbackAdapterTest, HandlesArrivalReordering) {
  std::vector<PacketFeedback> packets;
  packets.push_back(PacketFeedback(120, 200, 0, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(PacketFeedback(100, 220, 2, 1500, kPacingInfo0));

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

  // Adapter keeps the packets ordered by sequence number (which is itself
  // assigned by the order of transmission). Reordering by some other criteria,
  // eg. arrival time, is up to the observers.
  adapter_->OnTransportFeedback(feedback);
  ComparePacketFeedbackVectors(packets, adapter_->GetTransportFeedbackVector());
}

TEST_F(TransportFeedbackAdapterTest, TimestampDeltas) {
  std::vector<PacketFeedback> sent_packets;
  const int64_t kSmallDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor * ((1 << 8) - 1);
  const int64_t kLargePositiveDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor *
      std::numeric_limits<int16_t>::max();
  const int64_t kLargeNegativeDeltaUs =
      rtcp::TransportFeedback::kDeltaScaleFactor *
      std::numeric_limits<int16_t>::min();

  PacketFeedback packet_feedback(100, 200, 0, 1500, true, 0, 0,
                                 PacedPacketInfo());
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kSmallDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kSmallDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kLargePositiveDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kLargePositiveDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.send_time_ms += kLargeNegativeDeltaUs / 1000;
  packet_feedback.arrival_time_ms += kLargeNegativeDeltaUs / 1000;
  ++packet_feedback.sequence_number;
  sent_packets.push_back(packet_feedback);

  // Too large, delta - will need two feedback messages.
  packet_feedback.send_time_ms += (kLargePositiveDeltaUs + 1000) / 1000;
  packet_feedback.arrival_time_ms += (kLargePositiveDeltaUs + 1000) / 1000;
  ++packet_feedback.sequence_number;

  // Packets will be added to send history.
  for (const PacketFeedback& packet : sent_packets)
    OnSentPacket(packet);
  OnSentPacket(packet_feedback);

  // Create expected feedback and send into adapter.
  std::unique_ptr<rtcp::TransportFeedback> feedback(
      new rtcp::TransportFeedback());
  feedback->SetBase(sent_packets[0].sequence_number,
                    sent_packets[0].arrival_time_ms * 1000);

  for (const PacketFeedback& packet : sent_packets) {
    EXPECT_TRUE(feedback->AddReceivedPacket(packet.sequence_number,
                                            packet.arrival_time_ms * 1000));
  }
  EXPECT_FALSE(feedback->AddReceivedPacket(
      packet_feedback.sequence_number, packet_feedback.arrival_time_ms * 1000));

  rtc::Buffer raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  std::vector<PacketFeedback> received_feedback;

  EXPECT_TRUE(feedback.get() != nullptr);
  adapter_->OnTransportFeedback(*feedback.get());
  ComparePacketFeedbackVectors(sent_packets,
                               adapter_->GetTransportFeedbackVector());

  // Create a new feedback message and add the trailing item.
  feedback.reset(new rtcp::TransportFeedback());
  feedback->SetBase(packet_feedback.sequence_number,
                    packet_feedback.arrival_time_ms * 1000);
  EXPECT_TRUE(feedback->AddReceivedPacket(
      packet_feedback.sequence_number, packet_feedback.arrival_time_ms * 1000));
  raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  EXPECT_TRUE(feedback.get() != nullptr);
  adapter_->OnTransportFeedback(*feedback.get());
  {
    std::vector<PacketFeedback> expected_packets;
    expected_packets.push_back(packet_feedback);
    ComparePacketFeedbackVectors(expected_packets,
                                 adapter_->GetTransportFeedbackVector());
  }
}
}  // namespace test
}  // namespace webrtc
