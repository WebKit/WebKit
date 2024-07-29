/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/congestion_control_feedback_generator.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "api/environment/environment_factory.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/ecn_marking.h"
#include "system_wrappers/include/clock.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using rtcp::CongestionControlFeedback;
using ::testing::MockFunction;
using ::testing::SizeIs;
using ::testing::WithoutArgs;

bool PacketInfoIsExpected(const CongestionControlFeedback::PacketInfo& a,
                          const RtpPacketReceived b,
                          Timestamp feedback_send_time) {
  bool equal =
      a.ssrc == b.Ssrc() && a.sequence_number == b.SequenceNumber() &&
      (feedback_send_time - a.arrival_time_offset) == b.arrival_time() &&
      a.ecn == b.ecn();
  RTC_LOG_IF(LS_INFO, !equal)
      << " Not equal got ssrc: " << a.ssrc << ", seq: " << a.sequence_number
      << " arrival_time_offset: " << a.arrival_time_offset.ms()
      << " ecn: " << a.ecn << " expected ssrc:" << b.Ssrc()
      << ", seq: " << b.SequenceNumber() << " ecn: " << b.ecn();
  return equal;
}

MATCHER_P2(PacketInfosAreExpected, expected_vector, feedback_send_time, "") {
  if (expected_vector.size() != arg.size()) {
    RTC_LOG(LS_INFO) << " Wrong size, expected: " << expected_vector.size()
                     << " got: " << arg.size();
    return false;
  }
  for (size_t i = 0; i < expected_vector.size(); ++i) {
    if (!PacketInfoIsExpected(arg[i], expected_vector[i], feedback_send_time)) {
      return false;
    }
  }
  return true;
}

RtpPacketReceived CreatePacket(Timestamp arrival_time,
                               bool marker,
                               uint32_t ssrc = 1234,
                               uint16_t seq = 1,
                               rtc::EcnMarking ecn = rtc::EcnMarking::kNotEct) {
  RtpPacketReceived packet;
  packet.SetSsrc(ssrc);
  packet.SetSequenceNumber(seq);
  packet.SetMarker(marker);
  packet.set_arrival_time(arrival_time);
  return packet;
}

// If possible feedback should  be sent when a packet with marker bit is
// received in order to provide feedback as soon as possible after receiving a
// complete frame. On good networks, this means that a sender may receive
// feedback for every sent frame.
TEST(CongestionControlFeedbackGeneratorTest,
     SendsFeedbackAfterPacketWithMarkerBitReceived) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  EXPECT_GT(generator.Process(clock.CurrentTime()), TimeDelta::Millis(10));
  clock.AdvanceTimeMilliseconds(10);

  EXPECT_CALL(rtcp_sender, Call);
  generator.OnReceivedPacket(
      CreatePacket(clock.CurrentTime(), /*marker=*/false));
  generator.OnReceivedPacket(
      CreatePacket(clock.CurrentTime(), /*marker=*/true));
}

TEST(CongestionControlFeedbackGeneratorTest,
     SendsFeedbackDelayedIfNoPacketWithMarkerBitReceived) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  TimeDelta time_to_next = generator.Process(clock.CurrentTime());
  EXPECT_EQ(time_to_next, TimeDelta::Millis(25));
  clock.AdvanceTimeMilliseconds(10);
  generator.OnReceivedPacket(
      CreatePacket(clock.CurrentTime(), /*marker=*/false));
  // Expect feedback to be delayed another 25ms since no packet with marker is
  // received.
  Timestamp expected_feedback_time =
      clock.CurrentTime() + TimeDelta::Millis(25);
  EXPECT_CALL(rtcp_sender, Call).WillOnce(WithoutArgs([&] {
    EXPECT_EQ(clock.CurrentTime(), expected_feedback_time);
  }));
  clock.AdvanceTime(time_to_next - TimeDelta::Millis(10));
  time_to_next = generator.Process(clock.CurrentTime());
  clock.AdvanceTime(time_to_next);
  time_to_next = generator.Process(clock.CurrentTime());
}

TEST(CongestionControlFeedbackGeneratorTest,
     SendsFeedbackAfterMinTimeIfPacketsWithMarkerBitReceived) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  constexpr TimeDelta kSmallTimeInterval = TimeDelta::Millis(2);
  SimulatedClock clock(123456);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  TimeDelta time_to_next_process = generator.Process(clock.CurrentTime());
  Timestamp expected_feedback_time = clock.CurrentTime();
  EXPECT_CALL(rtcp_sender, Call).Times(2).WillRepeatedly(WithoutArgs([&] {
    EXPECT_EQ(clock.CurrentTime(), expected_feedback_time);
    // Next feedback can not be sent until 25ms after the previouse
    expected_feedback_time += TimeDelta::Millis(25);
  }));

  // 3 packets are received, with an interval kSmallTimeInterval.
  for (int i = 0; i < 3; ++i) {
    generator.OnReceivedPacket(
        CreatePacket(clock.CurrentTime(), /*marker=*/true));
    clock.AdvanceTime(kSmallTimeInterval);
    time_to_next_process -= kSmallTimeInterval;
  }
  clock.AdvanceTime(time_to_next_process);
  time_to_next_process = generator.Process(clock.CurrentTime());
  clock.AdvanceTime(time_to_next_process);
  time_to_next_process = generator.Process(clock.CurrentTime());
}

TEST(CongestionControlFeedbackGeneratorTest,
     FeedbackUtilizeMax5PercentOfConfiguredBwe) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  constexpr TimeDelta kSmallTimeInterval = TimeDelta::Millis(2);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  const DataRate kSendBandwidthEstimate = DataRate::BytesPerSec(10'000);
  const DataSize kPacketOverHead = DataSize::Bytes(25);
  generator.OnSendBandwidthEstimateChanged(kSendBandwidthEstimate);
  generator.SetTransportOverhead(kPacketOverHead);
  TimeDelta time_to_next_process = generator.Process(clock.CurrentTime());
  clock.AdvanceTime(kSmallTimeInterval);
  time_to_next_process -= kSmallTimeInterval;

  // Two packets with marker bit is received within a short duration.
  // Expect the first feedback to be sent immidately and the second to be
  // delayed. Delay depend on send bandwith estimate.
  Timestamp expected_feedback_time = clock.CurrentTime();
  EXPECT_CALL(rtcp_sender, Call)
      .Times(2)
      .WillRepeatedly(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) {
            EXPECT_EQ(clock.CurrentTime(), expected_feedback_time);
            ASSERT_THAT(rtcp_packets, SizeIs(1));
            rtcp::CongestionControlFeedback* rtcp =
                static_cast<rtcp::CongestionControlFeedback*>(
                    rtcp_packets[0].get());
            int rtcp_len = rtcp->BlockLength();
            // Expect at most 5% of send bandwidth to be used. This decide the
            // time to next feedback.
            expected_feedback_time +=
                (DataSize::Bytes(rtcp_len) + kPacketOverHead) /
                (0.05 * kSendBandwidthEstimate);
          });
  generator.OnReceivedPacket(
      CreatePacket(clock.CurrentTime(), /*marker=*/true));
  clock.AdvanceTime(kSmallTimeInterval);
  time_to_next_process -= kSmallTimeInterval;
  generator.OnReceivedPacket(
      CreatePacket(clock.CurrentTime(), /*marker=*/true));

  clock.AdvanceTime(time_to_next_process);
  time_to_next_process = generator.Process(clock.CurrentTime());
  clock.AdvanceTime(time_to_next_process);
  time_to_next_process = generator.Process(clock.CurrentTime());
}

TEST(CongestionControlFeedbackGeneratorTest,
     SendsFeedbackAfterMax250MsIfBweVeryLow) {
  test::ExplicitKeyValueConfig field_trials(
      "WebRTC-RFC8888CongestionControlFeedback/max_send_delta:250ms/");
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  constexpr TimeDelta kSmallTimeInterval = TimeDelta::Millis(2);
  CongestionControlFeedbackGenerator generator(
      CreateEnvironment(&clock, &field_trials), rtcp_sender.AsStdFunction());
  // Regardless of BWE, feedback is sent at least every 250ms.
  generator.OnSendBandwidthEstimateChanged(DataRate::BytesPerSec(100));
  TimeDelta time_to_next_process = generator.Process(clock.CurrentTime());
  clock.AdvanceTime(kSmallTimeInterval);
  time_to_next_process -= kSmallTimeInterval;

  Timestamp expected_feedback_time = clock.CurrentTime();
  EXPECT_CALL(rtcp_sender, Call).Times(2).WillRepeatedly(WithoutArgs([&] {
    EXPECT_EQ(clock.CurrentTime(), expected_feedback_time);
    // Next feedback is not expected to be sent until 250ms after the
    // previouse due to low send bandwidth.
    expected_feedback_time += TimeDelta::Millis(250);
  }));
  generator.OnReceivedPacket(
      CreatePacket(clock.CurrentTime(), /*marker=*/true));
  clock.AdvanceTime(kSmallTimeInterval);
  time_to_next_process -= kSmallTimeInterval;
  generator.OnReceivedPacket(
      CreatePacket(clock.CurrentTime(), /*marker=*/true));

  clock.AdvanceTime(time_to_next_process);
  time_to_next_process = generator.Process(clock.CurrentTime());
  clock.AdvanceTime(time_to_next_process);
  time_to_next_process = generator.Process(clock.CurrentTime());
}

TEST(CongestionControlFeedbackGeneratorTest,
     SortsReceivedPacketsBySsrcAndSeqno) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  constexpr TimeDelta kSmallTimeInterval = TimeDelta::Millis(2);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  TimeDelta time_to_next_process = generator.Process(clock.CurrentTime());

  const std::vector<RtpPacketReceived> kExpectedRtcpPackInfoOrder = {
      // Reordered packet.
      CreatePacket(clock.CurrentTime() + kSmallTimeInterval, /*marker*/ false,
                   /*ssrc=*/123,
                   /*seq=*/0xFFFA),
      CreatePacket(clock.CurrentTime(), /*marker*/ false, /*ssrc=*/123,
                   /*seq=*/1),
      // Reordered packet.
      CreatePacket(clock.CurrentTime() + kSmallTimeInterval,
                   /*marker*/ false, /*ssrc=*/
                   234,
                   /*seq=*/0xFFFC),
      CreatePacket(clock.CurrentTime(), /*marker*/ false, /*ssrc=*/234,
                   /*seq=*/1),
  };

  EXPECT_CALL(rtcp_sender, Call)
      .WillOnce(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) {
            ASSERT_THAT(rtcp_packets, SizeIs(1));
            rtcp::CongestionControlFeedback* rtcp =
                static_cast<rtcp::CongestionControlFeedback*>(
                    rtcp_packets[0].get());
            Timestamp feedback_send_time = clock.CurrentTime();
            EXPECT_THAT(rtcp->packets(),
                        PacketInfosAreExpected(kExpectedRtcpPackInfoOrder,
                                               feedback_send_time));
          });

  std::vector<RtpPacketReceived> receive_time_sorted =
      kExpectedRtcpPackInfoOrder;
  std::sort(receive_time_sorted.begin(), receive_time_sorted.end(),
            [](const RtpPacketReceived& a, const RtpPacketReceived& b) {
              return a.arrival_time() < b.arrival_time();
            });
  for (const RtpPacketReceived& packet : receive_time_sorted) {
    TimeDelta time_to_receive = packet.arrival_time() - clock.CurrentTime();
    time_to_next_process -= time_to_receive;
    clock.AdvanceTime(time_to_receive);
    generator.OnReceivedPacket(packet);
  }
  clock.AdvanceTime(time_to_next_process);
  time_to_next_process = generator.Process(clock.CurrentTime());
  clock.AdvanceTime(time_to_next_process);
  generator.Process(clock.CurrentTime());
}

TEST(CongestionControlFeedbackGeneratorTest,
     ReportsFirstReceivedPacketArrivalTimeButEcnFromCePacketIfDuplicate) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  constexpr TimeDelta kSmallTimeInterval = TimeDelta::Millis(2);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  TimeDelta time_to_next_process = generator.Process(clock.CurrentTime());
  RtpPacketReceived packet_1 =
      CreatePacket(clock.CurrentTime(), /*marker=*/false, /* ssrc=*/1,
                   /* seq=*/2, rtc::EcnMarking::kEct1);
  generator.OnReceivedPacket(packet_1);
  RtpPacketReceived packet_2 = packet_1;
  packet_2.set_arrival_time(clock.CurrentTime() + kSmallTimeInterval);
  packet_2.set_ecn(rtc::EcnMarking::kCe);
  time_to_next_process -= kSmallTimeInterval;
  clock.AdvanceTime(kSmallTimeInterval);
  generator.OnReceivedPacket(packet_2);

  EXPECT_CALL(rtcp_sender, Call)
      .WillOnce(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) {
            ASSERT_THAT(rtcp_packets, SizeIs(1));
            rtcp::CongestionControlFeedback* rtcp =
                static_cast<rtcp::CongestionControlFeedback*>(
                    rtcp_packets[0].get());
            Timestamp feedback_send_time = clock.CurrentTime();
            ASSERT_THAT(rtcp->packets(), SizeIs(1));
            EXPECT_EQ(rtcp->packets()[0].ecn, rtc::EcnMarking::kCe);
            EXPECT_EQ(rtcp->packets()[0].arrival_time_offset,
                      feedback_send_time - packet_1.arrival_time());
          });

  clock.AdvanceTime(time_to_next_process);
  time_to_next_process = generator.Process(clock.CurrentTime());
  clock.AdvanceTime(time_to_next_process);
  generator.Process(clock.CurrentTime());
}

}  // namespace
}  // namespace webrtc
