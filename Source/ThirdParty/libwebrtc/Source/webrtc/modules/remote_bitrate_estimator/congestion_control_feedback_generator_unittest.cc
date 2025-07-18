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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "api/environment/environment_factory.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/buffer.h"
#include "rtc_base/network/ecn_marking.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using rtcp::CongestionControlFeedback;
using ::testing::MockFunction;
using ::testing::SizeIs;
using ::testing::WithoutArgs;

RtpPacketReceived CreatePacket(Timestamp arrival_time,
                               bool marker,
                               uint32_t ssrc = 1234,
                               uint16_t seq = 1,
                               EcnMarking /* ecn */ = EcnMarking::kNotEct) {
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
     FeedbackFor30KPacketsUtilizeLessThan500kbitPerSecond) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  int number_of_feedback_packets = 0;
  DataSize total_feedback_size;
  EXPECT_CALL(rtcp_sender, Call)
      .WillRepeatedly(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) {
            ASSERT_THAT(rtcp_packets, SizeIs(1));
            number_of_feedback_packets++;
            total_feedback_size +=
                DataSize::Bytes(rtcp_packets[0]->BlockLength());
          });
  Timestamp start_time = clock.CurrentTime();
  Timestamp last_process_time = clock.CurrentTime();
  TimeDelta time_to_next_process = generator.Process(clock.CurrentTime());
  uint16_t rtp_sequence_number = 0;
  // Receive 30 packet per ms in 1s => 30'0000 packets.
  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(1)) {
    for (int i = 0; i < 30; ++i) {
      generator.OnReceivedPacket(CreatePacket(clock.CurrentTime(),
                                              /*marker=*/true, /*ssrc=*/1234,
                                              rtp_sequence_number++));
    }
    if (clock.CurrentTime() >= last_process_time + time_to_next_process) {
      last_process_time = clock.CurrentTime();
      time_to_next_process = generator.Process(clock.CurrentTime());
    }
    clock.AdvanceTime(TimeDelta::Millis(1));
  }

  EXPECT_LE(total_feedback_size / TimeDelta::Seconds(1),
            DataRate::KilobitsPerSec(500));
  EXPECT_EQ(number_of_feedback_packets, 40);
}

TEST(CongestionControlFeedbackGeneratorTest,
     FeedbackFor60KPacketsUtilizeApproximately500kbitPerSecond) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  int number_of_feedback_packets = 0;
  DataSize total_feedback_size;
  DataSize last_feedback_size;
  EXPECT_CALL(rtcp_sender, Call)
      .WillRepeatedly(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) {
            ASSERT_THAT(rtcp_packets, SizeIs(1));
            number_of_feedback_packets++;
            last_feedback_size =
                DataSize::Bytes(rtcp_packets[0]->BlockLength());
            total_feedback_size += last_feedback_size;
          });
  Timestamp start_time = clock.CurrentTime();
  Timestamp last_process_time = clock.CurrentTime();
  TimeDelta time_to_next_process = generator.Process(clock.CurrentTime());
  uint16_t rtp_sequence_number = 0;
  // Receive 60 packet per ms in 1s => 60'0000 packets.
  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(1)) {
    for (int i = 0; i < 60; ++i) {
      generator.OnReceivedPacket(CreatePacket(clock.CurrentTime(),
                                              /*marker=*/true, /*ssrc=*/1234,
                                              rtp_sequence_number++));
    }
    if (clock.CurrentTime() >= last_process_time + time_to_next_process) {
      last_process_time = clock.CurrentTime();
      time_to_next_process = generator.Process(clock.CurrentTime());
    }
    clock.AdvanceTime(TimeDelta::Millis(1));
  }
  EXPECT_LE(total_feedback_size,
            DataSize::Bytes(500'000 / 8) + last_feedback_size);
  EXPECT_LT(number_of_feedback_packets, 40);
}

TEST(CongestionControlFeedbackGeneratorTest,
     CanGenerateRtcpPacketFromTwoSsrcWithMissingPacketsAndWrap) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      rtcp_sender;
  SimulatedClock clock(123456);
  constexpr TimeDelta kSmallTimeInterval = TimeDelta::Millis(2);
  CongestionControlFeedbackGenerator generator(CreateEnvironment(&clock),
                                               rtcp_sender.AsStdFunction());

  TimeDelta time_to_next_process = generator.Process(clock.CurrentTime());

  // Receive packets out of order, with missing packets (between  0xFFA and 1 =
  // 6  and FFFC and 1 = 4) => total 14 packets is expected in the feedback.
  const std::vector<RtpPacketReceived> kReceivedPackets = {
      // Reordered packet.
      CreatePacket(clock.CurrentTime() + kSmallTimeInterval, /*marker*/ false,
                   /*ssrc=*/123,
                   /*seq=*/0xFFFA),
      CreatePacket(clock.CurrentTime(), /*marker*/ false, /*ssrc=*/123,
                   /*seq=*/1),
      // Reordered packet.
      CreatePacket(clock.CurrentTime() + kSmallTimeInterval,
                   /*marker*/ false, /*ssrc=*/
                   /*ssrc=*/234,
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

            ASSERT_THAT(rtcp->packets(), SizeIs(14));
            Buffer buffer = rtcp->Build();
            CongestionControlFeedback parsed_fb;
            rtcp::CommonHeader header;
            EXPECT_TRUE(header.Parse(buffer.data(), buffer.size()));
            EXPECT_TRUE(parsed_fb.Parse(header));
            EXPECT_THAT(parsed_fb.packets(), SizeIs(14));
          });

  std::vector<RtpPacketReceived> receive_time_sorted = kReceivedPackets;
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
                   /* seq=*/2, EcnMarking::kEct1);
  generator.OnReceivedPacket(packet_1);
  RtpPacketReceived packet_2 = packet_1;
  packet_2.set_arrival_time(clock.CurrentTime() + kSmallTimeInterval);
  packet_2.set_ecn(EcnMarking::kCe);
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
            EXPECT_EQ(rtcp->packets()[0].ecn, EcnMarking::kCe);
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
