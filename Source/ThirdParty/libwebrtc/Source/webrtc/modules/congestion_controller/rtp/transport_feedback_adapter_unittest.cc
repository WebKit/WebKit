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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Bool;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::TestParamInfo;

const PacedPacketInfo kPacingInfo0(0, 5, 2000);

struct PacketTemplate {
  uint32_t ssrc = 1;
  int64_t transport_sequence_number = 0;
  uint16_t rtp_sequence_number = 2;
  RtpPacketMediaType media_type = RtpPacketMediaType::kVideo;
  DataSize packet_size = DataSize::Bytes(100);

  EcnMarking ecn = EcnMarking::kNotEct;
  Timestamp send_timestamp = Timestamp::Millis(0);
  PacedPacketInfo pacing_info;
  Timestamp receive_timestamp = Timestamp::MinusInfinity();

  bool is_audio = false;
};

std::vector<PacketTemplate> CreatePacketTemplates(
    uint32_t number_of_ssrcs,
    uint32_t packets_per_ssrc,
    int64_t first_transport_sequence_number = 99) {
  int64_t transport_sequence_number = first_transport_sequence_number;
  Timestamp send_time = Timestamp::Millis(200);
  Timestamp receive_time = Timestamp::Millis(100);
  std::vector<PacketTemplate> packets;

  for (uint32_t ssrc = 3; ssrc < 3 + number_of_ssrcs; ++ssrc) {
    for (int rtp_sequence_number = ssrc * 10;
         rtp_sequence_number < static_cast<int>(ssrc * 10 + packets_per_ssrc);
         ++rtp_sequence_number) {
      packets.push_back({
          .ssrc = ssrc,
          .transport_sequence_number = transport_sequence_number++,
          .rtp_sequence_number = static_cast<uint16_t>(rtp_sequence_number),
          .send_timestamp = send_time,
          .pacing_info = kPacingInfo0,
          .receive_timestamp = receive_time,
      });
      send_time += TimeDelta::Millis(10);
      receive_time += TimeDelta::Millis(13);
    }
  }
  return packets;
}

void ComparePacketFeedbackVectors(const std::vector<PacketTemplate>& truth,
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
  TimeDelta arrival_time_delta =
      truth[0].receive_timestamp - input[0].receive_time;
  for (size_t i = 0; i < len; ++i) {
    EXPECT_EQ(truth[i].receive_timestamp.IsFinite(), input[i].IsReceived());
    if (input[i].IsReceived()) {
      EXPECT_EQ(truth[i].receive_timestamp - input[i].receive_time,
                arrival_time_delta);
    }
    EXPECT_EQ(truth[i].send_timestamp, input[i].sent_packet.send_time);
    EXPECT_EQ(truth[i].transport_sequence_number,
              input[i].sent_packet.sequence_number);
    EXPECT_EQ(truth[i].packet_size, input[i].sent_packet.size);
    EXPECT_EQ(truth[i].pacing_info, input[i].sent_packet.pacing_info);
    EXPECT_EQ(truth[i].is_audio, input[i].sent_packet.audio);
  }
}

RtpPacketToSend CreatePacketToSend(PacketTemplate packet) {
  RtpPacketToSend send_packet(nullptr);
  send_packet.SetSsrc(packet.ssrc);
  send_packet.SetPayloadSize(packet.packet_size.bytes() -
                             send_packet.headers_size());
  send_packet.SetSequenceNumber(packet.rtp_sequence_number);
  send_packet.set_transport_sequence_number(packet.transport_sequence_number);
  send_packet.set_packet_type(packet.is_audio ? RtpPacketMediaType::kAudio
                                              : RtpPacketMediaType::kVideo);

  return send_packet;
}

rtcp::TransportFeedback BuildRtcpTransportFeedbackPacket(
    rtc::ArrayView<const PacketTemplate> packets) {
  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].transport_sequence_number,
                   packets[0].receive_timestamp);

  for (const PacketTemplate& packet : packets) {
    if (packet.receive_timestamp.IsFinite()) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.transport_sequence_number,
                                             packet.receive_timestamp));
    }
  }
  return feedback;
}

rtcp::CongestionControlFeedback BuildRtcpCongestionControlFeedbackPacket(
    rtc::ArrayView<const PacketTemplate> packets) {
  // Assume the feedback was sent when the last packet was received.
  Timestamp feedback_sent_time = Timestamp::MinusInfinity();
  for (auto it = packets.crbegin(); it != packets.crend(); ++it) {
    if (it->receive_timestamp.IsFinite()) {
      feedback_sent_time = it->receive_timestamp;
      break;
    }
  }

  std::vector<rtcp::CongestionControlFeedback::PacketInfo> packet_infos;
  for (const PacketTemplate& packet : packets) {
    rtcp::CongestionControlFeedback::PacketInfo packet_info = {
        .ssrc = packet.ssrc,
        .sequence_number = packet.rtp_sequence_number,
        .ecn = packet.ecn};
    if (packet.receive_timestamp.IsFinite()) {
      packet_info.arrival_time_offset =
          feedback_sent_time - packet.receive_timestamp;
    }
    packet_infos.push_back(packet_info);
  }

  SimulatedClock clock(feedback_sent_time);
  uint32_t compact_ntp =
      CompactNtp(clock.ConvertTimestampToNtpTime(feedback_sent_time));
  return rtcp::CongestionControlFeedback(std::move(packet_infos), compact_ntp);
}

Timestamp TimeNow() {
  return Timestamp::Millis(1234);
}

}  // namespace

class TransportFeedbackAdapterTest : public ::testing::TestWithParam<bool> {
 public:
  bool UseRfc8888CongestionControlFeedback() const { return GetParam(); }

  std::optional<TransportPacketsFeedback> CreateAndProcessFeedback(
      rtc::ArrayView<const PacketTemplate> packets,
      TransportFeedbackAdapter& adapter) {
    if (UseRfc8888CongestionControlFeedback()) {
      rtcp::CongestionControlFeedback rtcp_feedback =
          BuildRtcpCongestionControlFeedbackPacket(packets);
      return adapter.ProcessCongestionControlFeedback(rtcp_feedback, TimeNow());
    } else {
      rtcp::TransportFeedback rtcp_feedback =
          BuildRtcpTransportFeedbackPacket(packets);
      return adapter.ProcessTransportFeedback(rtcp_feedback, TimeNow());
    }
  }
};

INSTANTIATE_TEST_SUITE_P(FeedbackFormats,
                         TransportFeedbackAdapterTest,
                         Bool(),
                         [](TestParamInfo<bool> param) {
                           if (param.param)
                             return "CongestionControlFeedback";
                           else
                             return "TransportFeedback";
                         });

TEST_P(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
  TransportFeedbackAdapter adapter;
  std::vector<PacketTemplate> packets =
      CreatePacketTemplates(/*number_of_ssrcs=*/2, /*packets_per_ssrc=*/3);

  for (const PacketTemplate& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(packets, adapter);
  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_P(TransportFeedbackAdapterTest, FeedbackVectorReportsUnreceived) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketTemplate> sent_packets =
      CreatePacketTemplates(/*number_of_ssrcs=*/2, /*packets_per_ssrc=*/3);

  for (const PacketTemplate& packet : sent_packets) {
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  // Note: Important to include the last packet per SSRC, as only unreceived
  // packets in between received packets can be inferred.
  sent_packets[1].receive_timestamp = Timestamp::PlusInfinity();
  sent_packets[4].receive_timestamp = Timestamp::PlusInfinity();
  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(sent_packets, adapter);

  ComparePacketFeedbackVectors(sent_packets,
                               adapted_feedback->packet_feedbacks);
}

TEST_P(TransportFeedbackAdapterTest, HandlesDroppedPackets) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketTemplate> packets =
      CreatePacketTemplates(/*number_of_ssrcs=*/2, /*packets_per_ssrc=*/2,
                            /*first_transport_sequence_number=*/0);

  const uint16_t kSendSideDropBefore = 1;
  const uint16_t kReceiveSideDropAfter = 3;

  std::vector<PacketTemplate> sent_packets;
  for (const PacketTemplate& packet : packets) {
    if (packet.transport_sequence_number >= kSendSideDropBefore) {
      sent_packets.push_back(packet);
    }
  }
  for (const PacketTemplate& packet : sent_packets) {
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::vector<PacketTemplate> received_packets;
  for (const PacketTemplate& packet : packets) {
    if (packet.transport_sequence_number <= kReceiveSideDropAfter) {
      received_packets.push_back(packet);
    }
  }
  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(received_packets, adapter);

  std::vector<PacketTemplate> expected_packets(
      packets.begin() + kSendSideDropBefore,
      packets.begin() + kReceiveSideDropAfter + 1);
  // Packets that have timed out on the send-side have lost the
  // information stored on the send-side. And they will not be reported to
  // observers since we won't know that they come from the same networks.
  ComparePacketFeedbackVectors(expected_packets,
                               adapted_feedback->packet_feedbacks);
}

TEST_P(TransportFeedbackAdapterTest, FeedbackReportsIfPacketIsAudio) {
  TransportFeedbackAdapter adapter;

  PacketTemplate packets[] = {
      {.receive_timestamp = TimeNow(), .is_audio = true}};
  PacketTemplate& packet = packets[0];

  adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                    /*overhead=*/0u, TimeNow());
  adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                            packet.send_timestamp.ms()));

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(packets, adapter);
  ASSERT_THAT(adapted_feedback->packet_feedbacks, SizeIs(1));
  EXPECT_TRUE(adapted_feedback->packet_feedbacks[0].sent_packet.audio);
}

TEST_P(TransportFeedbackAdapterTest, ReceiveTimeWrapsBothWays) {
  TransportFeedbackAdapter adapter;

  TimeDelta kHighArrivalTime =
      rtcp::TransportFeedback::kDeltaTick * (1 << 8) * ((1 << 23) - 1);

  std::vector<PacketTemplate> packets = {
      {.transport_sequence_number = 0,
       .rtp_sequence_number = 102,
       .receive_timestamp =
           Timestamp::Zero() + kHighArrivalTime + TimeDelta::Millis(64)},
      {.transport_sequence_number = 1,
       .rtp_sequence_number = 103,
       .receive_timestamp =
           Timestamp::Zero() + kHighArrivalTime - TimeDelta::Millis(64)},
      {.transport_sequence_number = 2,
       .rtp_sequence_number = 104,
       .receive_timestamp = Timestamp::Zero() + kHighArrivalTime}};

  for (const PacketTemplate& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  for (size_t i = 0; i < packets.size(); ++i) {
    std::vector<PacketTemplate> received_packets = {packets[i]};

    std::optional<TransportPacketsFeedback> result;
    if (UseRfc8888CongestionControlFeedback()) {
      rtcp::CongestionControlFeedback feedback =
          BuildRtcpCongestionControlFeedbackPacket(received_packets);
      rtc::Buffer raw_packet = feedback.Build();
      rtcp::CommonHeader header;
      ASSERT_TRUE(header.Parse(raw_packet.data(), raw_packet.size()));
      rtcp::CongestionControlFeedback parsed_feedback;
      ASSERT_TRUE(parsed_feedback.Parse(header));
      result =
          adapter.ProcessCongestionControlFeedback(parsed_feedback, TimeNow());
    } else {
      rtcp::TransportFeedback feedback =
          BuildRtcpTransportFeedbackPacket(received_packets);
      rtc::Buffer raw_packet = feedback.Build();
      std::unique_ptr<rtcp::TransportFeedback> parsed_feedback =
          rtcp::TransportFeedback::ParseFrom(raw_packet.data(),
                                             raw_packet.size());
      ASSERT_THAT(parsed_feedback, NotNull());
      result = adapter.ProcessTransportFeedback(*parsed_feedback, TimeNow());
    }
    ASSERT_TRUE(result.has_value());
    ComparePacketFeedbackVectors(received_packets, result->packet_feedbacks);
  }
}

TEST_P(TransportFeedbackAdapterTest, HandlesArrivalReordering) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketTemplate> packets = {
      {.transport_sequence_number = 0,
       .rtp_sequence_number = 101,
       .send_timestamp = Timestamp::Millis(200),
       .receive_timestamp = Timestamp::Millis(120)},
      {.transport_sequence_number = 1,
       .rtp_sequence_number = 102,
       .send_timestamp = Timestamp::Millis(210),
       .receive_timestamp = Timestamp::Millis(110)},
      {.transport_sequence_number = 2,
       .rtp_sequence_number = 103,
       .send_timestamp = Timestamp::Millis(220),
       .receive_timestamp = Timestamp::Millis(100)}};

  for (const PacketTemplate& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  // Adapter keeps the packets ordered by sequence number (which is itself
  // assigned by the order of transmission). Reordering by some other criteria,
  // eg. arrival time, is up to the observers.
  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(packets, adapter);
  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_P(TransportFeedbackAdapterTest, IgnoreDuplicatePacketSentCalls) {
  TransportFeedbackAdapter adapter;

  PacketTemplate packet = {};
  // Add a packet and then mark it as sent.
  adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info, 0u,
                    TimeNow());
  std::optional<SentPacket> sent_packet = adapter.ProcessSentPacket(
      rtc::SentPacket(packet.transport_sequence_number,
                      packet.send_timestamp.ms(), rtc::PacketInfo()));
  EXPECT_TRUE(sent_packet.has_value());

  // Call ProcessSentPacket() again with the same sequence number. This packet
  // has already been marked as sent and the call should be ignored.
  std::optional<SentPacket> duplicate_packet = adapter.ProcessSentPacket(
      rtc::SentPacket(packet.transport_sequence_number,
                      packet.send_timestamp.ms(), rtc::PacketInfo()));
  EXPECT_FALSE(duplicate_packet.has_value());
}

TEST_P(TransportFeedbackAdapterTest,
       SendReceiveTimeDiffTimeContinuouseBetweenFeedback) {
  TransportFeedbackAdapter adapter;

  PacketTemplate packets[] = {{.transport_sequence_number = 1,
                               .rtp_sequence_number = 101,
                               .send_timestamp = Timestamp::Millis(100),
                               .pacing_info = kPacingInfo0,
                               .receive_timestamp = Timestamp::Millis(200)},
                              {.transport_sequence_number = 2,
                               .rtp_sequence_number = 102,
                               .send_timestamp = Timestamp::Millis(110),
                               .pacing_info = kPacingInfo0,
                               .receive_timestamp = Timestamp::Millis(210)}};

  for (const PacketTemplate& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead=*/0u, TimeNow());

    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback_1 =
      CreateAndProcessFeedback(std::vector<PacketTemplate>({packets[0]}),
                               adapter);
  std::optional<TransportPacketsFeedback> adapted_feedback_2 =
      CreateAndProcessFeedback(std::vector<PacketTemplate>({packets[1]}),
                               adapter);

  ASSERT_EQ(adapted_feedback_1->packet_feedbacks.size(),
            adapted_feedback_2->packet_feedbacks.size());
  ASSERT_THAT(adapted_feedback_1->packet_feedbacks, testing::SizeIs(1));
  EXPECT_EQ((adapted_feedback_1->packet_feedbacks[0].receive_time -
             adapted_feedback_1->packet_feedbacks[0].sent_packet.send_time)
                .RoundTo(TimeDelta::Millis(1)),
            (adapted_feedback_2->packet_feedbacks[0].receive_time -
             adapted_feedback_2->packet_feedbacks[0].sent_packet.send_time)
                .RoundTo(TimeDelta::Millis(1)));
}

TEST_P(TransportFeedbackAdapterTest, ProcessSentPacketIncreaseOutstandingData) {
  TransportFeedbackAdapter adapter;

  PacketTemplate packet_1 = {.transport_sequence_number = 1,
                             .packet_size = DataSize::Bytes(200)};
  PacketTemplate packet_2 = {.transport_sequence_number = 2,
                             .packet_size = DataSize::Bytes(300)};
  adapter.AddPacket(CreatePacketToSend(packet_1), packet_1.pacing_info,
                    /*overhead=*/0u, TimeNow());
  std::optional<SentPacket> sent_packet_1 =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet_1.transport_sequence_number, packet_1.send_timestamp.ms()));

  ASSERT_TRUE(sent_packet_1.has_value());
  EXPECT_EQ(sent_packet_1->sequence_number, packet_1.transport_sequence_number);
  // Only one packet in flight.
  EXPECT_EQ(sent_packet_1->data_in_flight, packet_1.packet_size);
  EXPECT_EQ(adapter.GetOutstandingData(), packet_1.packet_size);

  adapter.AddPacket(CreatePacketToSend(packet_2), packet_2.pacing_info,
                    /*overhead=*/0u, TimeNow());
  std::optional<SentPacket> sent_packet_2 =
      adapter.ProcessSentPacket(rtc::SentPacket(
          packet_2.transport_sequence_number, packet_2.send_timestamp.ms()));

  ASSERT_TRUE(sent_packet_2.has_value());
  // Two packets in flight.
  EXPECT_EQ(sent_packet_2->data_in_flight,
            packet_1.packet_size + packet_2.packet_size);

  EXPECT_EQ(adapter.GetOutstandingData(),
            packet_1.packet_size + packet_2.packet_size);
}

TEST_P(TransportFeedbackAdapterTest, TransportPacketFeedbackHasDataInFlight) {
  TransportFeedbackAdapter adapter;

  const PacketTemplate packets[] = {
      {
          .transport_sequence_number = 1,
          .rtp_sequence_number = 101,
          .packet_size = DataSize::Bytes(200),
          .send_timestamp = Timestamp::Millis(100),
          .pacing_info = kPacingInfo0,
          .receive_timestamp = Timestamp::Millis(200),
      },
      {
          .transport_sequence_number = 2,
          .rtp_sequence_number = 102,
          .packet_size = DataSize::Bytes(300),
          .send_timestamp = Timestamp::Millis(110),
          .pacing_info = kPacingInfo0,
          .receive_timestamp = Timestamp::Millis(210),
      }};

  for (const PacketTemplate& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                      /*overhead=*/0u, TimeNow());

    adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                              packet.send_timestamp.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback_1 =
      CreateAndProcessFeedback(rtc::MakeArrayView(&packets[0], 1), adapter);
  std::optional<TransportPacketsFeedback> adapted_feedback_2 =
      CreateAndProcessFeedback(rtc::MakeArrayView(&packets[1], 1), adapter);
  EXPECT_EQ(adapted_feedback_1->data_in_flight, packets[1].packet_size);
  EXPECT_EQ(adapted_feedback_2->data_in_flight, DataSize::Zero());
}

TEST(TransportFeedbackAdapterCongestionFeedbackTest,
     CongestionControlFeedbackResultHasEcn) {
  TransportFeedbackAdapter adapter;

  PacketTemplate packet = {
      .transport_sequence_number = 1,
      .rtp_sequence_number = 101,
      .packet_size = DataSize::Bytes(200),
      .send_timestamp = Timestamp::Millis(100),
      .pacing_info = kPacingInfo0,
      .receive_timestamp = Timestamp::Millis(200),
  };
  adapter.AddPacket(CreatePacketToSend(packet), packet.pacing_info,
                    /*overhead=*/0u, TimeNow());
  adapter.ProcessSentPacket(rtc::SentPacket(packet.transport_sequence_number,
                                            packet.send_timestamp.ms()));

  packet.ecn = EcnMarking::kCe;
  rtcp::CongestionControlFeedback rtcp_feedback =
      BuildRtcpCongestionControlFeedbackPacket(rtc::MakeArrayView(&packet, 1));
  std::optional<TransportPacketsFeedback> adapted_feedback =
      adapter.ProcessCongestionControlFeedback(rtcp_feedback, TimeNow());

  ASSERT_THAT(adapted_feedback->packet_feedbacks, SizeIs(1));
  ASSERT_THAT(adapted_feedback->packet_feedbacks[0].ecn, EcnMarking::kCe);
}

}  // namespace webrtc
