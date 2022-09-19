/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"

#include <limits>
#include <memory>
#include <vector>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace webrtc {

namespace {
constexpr uint32_t kSsrc = 8492;
const PacedPacketInfo kPacingInfo0(0, 5, 2000);
const PacedPacketInfo kPacingInfo1(1, 8, 4000);
const PacedPacketInfo kPacingInfo2(2, 14, 7000);
const PacedPacketInfo kPacingInfo3(3, 20, 10000);
const PacedPacketInfo kPacingInfo4(4, 22, 10000);

void ComparePacketFeedbackVectors(const std::vector<PacketResult>& truth,
                                  const std::vector<PacketResult>& input) {
  ASSERT_EQ(truth.size(), input.size());
  size_t len = truth.size();
  // truth contains the input data for the test, and input is what will be
  // sent to the bandwidth estimator. truth.arrival_tims_ms is used to
  // populate the transport feedback messages. As these times may be changed
  // (because of resolution limits in the packets, and because of the time
  // base adjustment performed by the TransportFeedbackAdapter at the first
  // packet, the truth[x].arrival_time and input[x].arrival_time may not be
  // equal. However, the difference must be the same for all x.
  TimeDelta arrival_time_delta = truth[0].receive_time - input[0].receive_time;
  for (size_t i = 0; i < len; ++i) {
    RTC_CHECK(truth[i].IsReceived());
    if (input[i].IsReceived()) {
      EXPECT_EQ(truth[i].receive_time - input[i].receive_time,
                arrival_time_delta);
    }
    EXPECT_EQ(truth[i].sent_packet.send_time, input[i].sent_packet.send_time);
    EXPECT_EQ(truth[i].sent_packet.sequence_number,
              input[i].sent_packet.sequence_number);
    EXPECT_EQ(truth[i].sent_packet.size, input[i].sent_packet.size);
    EXPECT_EQ(truth[i].sent_packet.pacing_info,
              input[i].sent_packet.pacing_info);
  }
}

PacketResult CreatePacket(int64_t receive_time_ms,
                          int64_t send_time_ms,
                          int64_t sequence_number,
                          size_t payload_size,
                          const PacedPacketInfo& pacing_info) {
  PacketResult res;
  res.receive_time = Timestamp::Millis(receive_time_ms);
  res.sent_packet.send_time = Timestamp::Millis(send_time_ms);
  res.sent_packet.sequence_number = sequence_number;
  res.sent_packet.size = DataSize::Bytes(payload_size);
  res.sent_packet.pacing_info = pacing_info;
  return res;
}

class MockStreamFeedbackObserver : public webrtc::StreamFeedbackObserver {
 public:
  MOCK_METHOD(void,
              OnPacketFeedbackVector,
              (std::vector<StreamPacketInfo> packet_feedback_vector),
              (override));
};

}  // namespace

class TransportFeedbackAdapterTest : public ::testing::Test {
 public:
  TransportFeedbackAdapterTest() : clock_(0) {}

  virtual ~TransportFeedbackAdapterTest() {}

  virtual void SetUp() { adapter_.reset(new TransportFeedbackAdapter()); }

  virtual void TearDown() { adapter_.reset(); }

 protected:
  void OnReceivedEstimatedBitrate(uint32_t bitrate) {}

  void OnReceivedRtcpReceiverReport(const ReportBlockList& report_blocks,
                                    int64_t rtt,
                                    int64_t now_ms) {}

  void OnSentPacket(const PacketResult& packet_feedback) {
    RtpPacketSendInfo packet_info;
    packet_info.media_ssrc = kSsrc;
    packet_info.transport_sequence_number =
        packet_feedback.sent_packet.sequence_number;
    packet_info.rtp_sequence_number = 0;
    packet_info.length = packet_feedback.sent_packet.size.bytes();
    packet_info.pacing_info = packet_feedback.sent_packet.pacing_info;
    packet_info.packet_type = RtpPacketMediaType::kVideo;
    adapter_->AddPacket(RtpPacketSendInfo(packet_info), 0u,
                        clock_.CurrentTime());
    adapter_->ProcessSentPacket(rtc::SentPacket(
        packet_feedback.sent_packet.sequence_number,
        packet_feedback.sent_packet.send_time.ms(), rtc::PacketInfo()));
  }

  SimulatedClock clock_;
  std::unique_ptr<TransportFeedbackAdapter> adapter_;
};

TEST_F(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
  std::vector<PacketResult> packets;
  packets.push_back(CreatePacket(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(120, 220, 2, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(130, 230, 3, 1500, kPacingInfo1));
  packets.push_back(CreatePacket(140, 240, 4, 1500, kPacingInfo1));

  for (const auto& packet : packets)
    OnSentPacket(packet);

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sent_packet.sequence_number,
                   packets[0].receive_time);

  for (const auto& packet : packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sent_packet.sequence_number,
                                           packet.receive_time));
  }

  feedback.Build();

  auto result =
      adapter_->ProcessTransportFeedback(feedback, clock_.CurrentTime());
  ComparePacketFeedbackVectors(packets, result->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, FeedbackVectorReportsUnreceived) {
  std::vector<PacketResult> sent_packets = {
      CreatePacket(100, 220, 0, 1500, kPacingInfo0),
      CreatePacket(110, 210, 1, 1500, kPacingInfo0),
      CreatePacket(120, 220, 2, 1500, kPacingInfo0),
      CreatePacket(130, 230, 3, 1500, kPacingInfo0),
      CreatePacket(140, 240, 4, 1500, kPacingInfo0),
      CreatePacket(150, 250, 5, 1500, kPacingInfo0),
      CreatePacket(160, 260, 6, 1500, kPacingInfo0)};

  for (const auto& packet : sent_packets)
    OnSentPacket(packet);

  // Note: Important to include the last packet, as only unreceived packets in
  // between received packets can be inferred.
  std::vector<PacketResult> received_packets = {
      sent_packets[0], sent_packets[2], sent_packets[6]};

  rtcp::TransportFeedback feedback;
  feedback.SetBase(received_packets[0].sent_packet.sequence_number,
                   received_packets[0].receive_time);

  for (const auto& packet : received_packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sent_packet.sequence_number,
                                           packet.receive_time));
  }

  feedback.Build();

  auto res = adapter_->ProcessTransportFeedback(feedback, clock_.CurrentTime());
  ComparePacketFeedbackVectors(sent_packets, res->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, HandlesDroppedPackets) {
  std::vector<PacketResult> packets;
  packets.push_back(CreatePacket(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(110, 210, 1, 1500, kPacingInfo1));
  packets.push_back(CreatePacket(120, 220, 2, 1500, kPacingInfo2));
  packets.push_back(CreatePacket(130, 230, 3, 1500, kPacingInfo3));
  packets.push_back(CreatePacket(140, 240, 4, 1500, kPacingInfo4));

  const uint16_t kSendSideDropBefore = 1;
  const uint16_t kReceiveSideDropAfter = 3;

  for (const auto& packet : packets) {
    if (packet.sent_packet.sequence_number >= kSendSideDropBefore)
      OnSentPacket(packet);
  }

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sent_packet.sequence_number,
                   packets[0].receive_time);

  for (const auto& packet : packets) {
    if (packet.sent_packet.sequence_number <= kReceiveSideDropAfter) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.sent_packet.sequence_number,
                                             packet.receive_time));
    }
  }

  feedback.Build();

  std::vector<PacketResult> expected_packets(
      packets.begin() + kSendSideDropBefore,
      packets.begin() + kReceiveSideDropAfter + 1);
  // Packets that have timed out on the send-side have lost the
  // information stored on the send-side. And they will not be reported to
  // observers since we won't know that they come from the same networks.

  auto res = adapter_->ProcessTransportFeedback(feedback, clock_.CurrentTime());
  ComparePacketFeedbackVectors(expected_packets, res->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, SendTimeWrapsBothWays) {
  TimeDelta kHighArrivalTime =
      rtcp::TransportFeedback::kDeltaTick * (1 << 8) * ((1 << 23) - 1);
  std::vector<PacketResult> packets;
  packets.push_back(CreatePacket(kHighArrivalTime.ms() + 64, 210, 0, 1500,
                                 PacedPacketInfo()));
  packets.push_back(CreatePacket(kHighArrivalTime.ms() - 64, 210, 1, 1500,
                                 PacedPacketInfo()));
  packets.push_back(
      CreatePacket(kHighArrivalTime.ms(), 220, 2, 1500, PacedPacketInfo()));

  for (const auto& packet : packets)
    OnSentPacket(packet);

  for (size_t i = 0; i < packets.size(); ++i) {
    std::unique_ptr<rtcp::TransportFeedback> feedback(
        new rtcp::TransportFeedback());
    feedback->SetBase(packets[i].sent_packet.sequence_number,
                      packets[i].receive_time);

    EXPECT_TRUE(feedback->AddReceivedPacket(
        packets[i].sent_packet.sequence_number, packets[i].receive_time));

    rtc::Buffer raw_packet = feedback->Build();
    feedback = rtcp::TransportFeedback::ParseFrom(raw_packet.data(),
                                                  raw_packet.size());

    std::vector<PacketResult> expected_packets;
    expected_packets.push_back(packets[i]);

    auto res = adapter_->ProcessTransportFeedback(*feedback.get(),
                                                  clock_.CurrentTime());
    ComparePacketFeedbackVectors(expected_packets, res->packet_feedbacks);
  }
}

TEST_F(TransportFeedbackAdapterTest, HandlesArrivalReordering) {
  std::vector<PacketResult> packets;
  packets.push_back(CreatePacket(120, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(100, 220, 2, 1500, kPacingInfo0));

  for (const auto& packet : packets)
    OnSentPacket(packet);

  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sent_packet.sequence_number,
                   packets[0].receive_time);

  for (const auto& packet : packets) {
    EXPECT_TRUE(feedback.AddReceivedPacket(packet.sent_packet.sequence_number,
                                           packet.receive_time));
  }

  feedback.Build();

  // Adapter keeps the packets ordered by sequence number (which is itself
  // assigned by the order of transmission). Reordering by some other criteria,
  // eg. arrival time, is up to the observers.
  auto res = adapter_->ProcessTransportFeedback(feedback, clock_.CurrentTime());
  ComparePacketFeedbackVectors(packets, res->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, TimestampDeltas) {
  std::vector<PacketResult> sent_packets;
  // TODO(srte): Consider using us resolution in the constants.
  const TimeDelta kSmallDelta = (rtcp::TransportFeedback::kDeltaTick * 0xFF)
                                    .RoundDownTo(TimeDelta::Millis(1));
  const TimeDelta kLargePositiveDelta = (rtcp::TransportFeedback::kDeltaTick *
                                         std::numeric_limits<int16_t>::max())
                                            .RoundDownTo(TimeDelta::Millis(1));
  const TimeDelta kLargeNegativeDelta = (rtcp::TransportFeedback::kDeltaTick *
                                         std::numeric_limits<int16_t>::min())
                                            .RoundDownTo(TimeDelta::Millis(1));

  PacketResult packet_feedback;
  packet_feedback.sent_packet.sequence_number = 1;
  packet_feedback.sent_packet.send_time = Timestamp::Millis(100);
  packet_feedback.receive_time = Timestamp::Millis(200);
  packet_feedback.sent_packet.size = DataSize::Bytes(1500);
  sent_packets.push_back(packet_feedback);

  // TODO(srte): This rounding maintains previous behavior, but should ot be
  // required.
  packet_feedback.sent_packet.send_time += kSmallDelta;
  packet_feedback.receive_time += kSmallDelta;
  ++packet_feedback.sent_packet.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.sent_packet.send_time += kLargePositiveDelta;
  packet_feedback.receive_time += kLargePositiveDelta;
  ++packet_feedback.sent_packet.sequence_number;
  sent_packets.push_back(packet_feedback);

  packet_feedback.sent_packet.send_time += kLargeNegativeDelta;
  packet_feedback.receive_time += kLargeNegativeDelta;
  ++packet_feedback.sent_packet.sequence_number;
  sent_packets.push_back(packet_feedback);

  // Too large, delta - will need two feedback messages.
  packet_feedback.sent_packet.send_time +=
      kLargePositiveDelta + TimeDelta::Millis(1);
  packet_feedback.receive_time += kLargePositiveDelta + TimeDelta::Millis(1);
  ++packet_feedback.sent_packet.sequence_number;

  // Packets will be added to send history.
  for (const auto& packet : sent_packets)
    OnSentPacket(packet);
  OnSentPacket(packet_feedback);

  // Create expected feedback and send into adapter.
  std::unique_ptr<rtcp::TransportFeedback> feedback(
      new rtcp::TransportFeedback());
  feedback->SetBase(sent_packets[0].sent_packet.sequence_number,
                    sent_packets[0].receive_time);

  for (const auto& packet : sent_packets) {
    EXPECT_TRUE(feedback->AddReceivedPacket(packet.sent_packet.sequence_number,
                                            packet.receive_time));
  }
  EXPECT_FALSE(
      feedback->AddReceivedPacket(packet_feedback.sent_packet.sequence_number,
                                  packet_feedback.receive_time));

  rtc::Buffer raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  std::vector<PacketResult> received_feedback;

  EXPECT_TRUE(feedback.get() != nullptr);
  auto res =
      adapter_->ProcessTransportFeedback(*feedback.get(), clock_.CurrentTime());
  ComparePacketFeedbackVectors(sent_packets, res->packet_feedbacks);

  // Create a new feedback message and add the trailing item.
  feedback.reset(new rtcp::TransportFeedback());
  feedback->SetBase(packet_feedback.sent_packet.sequence_number,
                    packet_feedback.receive_time);
  EXPECT_TRUE(
      feedback->AddReceivedPacket(packet_feedback.sent_packet.sequence_number,
                                  packet_feedback.receive_time));
  raw_packet = feedback->Build();
  feedback =
      rtcp::TransportFeedback::ParseFrom(raw_packet.data(), raw_packet.size());

  EXPECT_TRUE(feedback.get() != nullptr);
  {
    auto res = adapter_->ProcessTransportFeedback(*feedback.get(),
                                                  clock_.CurrentTime());
    std::vector<PacketResult> expected_packets;
    expected_packets.push_back(packet_feedback);
    ComparePacketFeedbackVectors(expected_packets, res->packet_feedbacks);
  }
}

TEST_F(TransportFeedbackAdapterTest, IgnoreDuplicatePacketSentCalls) {
  auto packet = CreatePacket(100, 200, 0, 1500, kPacingInfo0);

  // Add a packet and then mark it as sent.
  RtpPacketSendInfo packet_info;
  packet_info.media_ssrc = kSsrc;
  packet_info.transport_sequence_number = packet.sent_packet.sequence_number;
  packet_info.length = packet.sent_packet.size.bytes();
  packet_info.pacing_info = packet.sent_packet.pacing_info;
  packet_info.packet_type = RtpPacketMediaType::kVideo;
  adapter_->AddPacket(packet_info, 0u, clock_.CurrentTime());
  absl::optional<SentPacket> sent_packet = adapter_->ProcessSentPacket(
      rtc::SentPacket(packet.sent_packet.sequence_number,
                      packet.sent_packet.send_time.ms(), rtc::PacketInfo()));
  EXPECT_TRUE(sent_packet.has_value());

  // Call ProcessSentPacket() again with the same sequence number. This packet
  // has already been marked as sent and the call should be ignored.
  absl::optional<SentPacket> duplicate_packet = adapter_->ProcessSentPacket(
      rtc::SentPacket(packet.sent_packet.sequence_number,
                      packet.sent_packet.send_time.ms(), rtc::PacketInfo()));
  EXPECT_FALSE(duplicate_packet.has_value());
}

}  // namespace webrtc
