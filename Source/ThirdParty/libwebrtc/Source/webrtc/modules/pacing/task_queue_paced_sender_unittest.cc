/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/task_queue_paced_sender.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "modules/pacing/packet_router.h"
#include "modules/utility/include/mock/mock_process_thread.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace webrtc {
namespace {
constexpr uint32_t kAudioSsrc = 12345;
constexpr uint32_t kVideoSsrc = 234565;
constexpr uint32_t kVideoRtxSsrc = 34567;
constexpr uint32_t kFlexFecSsrc = 45678;
constexpr size_t kDefaultPacketSize = 1234;

class MockPacketRouter : public PacketRouter {
 public:
  MOCK_METHOD2(SendPacket,
               void(std::unique_ptr<RtpPacketToSend> packet,
                    const PacedPacketInfo& cluster_info));
  MOCK_METHOD1(
      GeneratePadding,
      std::vector<std::unique_ptr<RtpPacketToSend>>(size_t target_size_bytes));
};
}  // namespace

namespace test {

class TaskQueuePacedSenderTest : public ::testing::Test {
 public:
  TaskQueuePacedSenderTest()
      : time_controller_(Timestamp::Millis(1234)),
        pacer_(time_controller_.GetClock(),
               &packet_router_,
               /*event_log=*/nullptr,
               /*field_trials=*/nullptr,
               time_controller_.GetTaskQueueFactory()) {}

 protected:
  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(RtpPacketMediaType type) {
    auto packet = std::make_unique<RtpPacketToSend>(nullptr);
    packet->set_packet_type(type);
    switch (type) {
      case RtpPacketMediaType::kAudio:
        packet->SetSsrc(kAudioSsrc);
        break;
      case RtpPacketMediaType::kVideo:
        packet->SetSsrc(kVideoSsrc);
        break;
      case RtpPacketMediaType::kRetransmission:
      case RtpPacketMediaType::kPadding:
        packet->SetSsrc(kVideoRtxSsrc);
        break;
      case RtpPacketMediaType::kForwardErrorCorrection:
        packet->SetSsrc(kFlexFecSsrc);
        break;
    }

    packet->SetPayloadSize(kDefaultPacketSize);
    return packet;
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePackets(
      RtpPacketMediaType type,
      size_t num_packets) {
    std::vector<std::unique_ptr<RtpPacketToSend>> packets;
    for (size_t i = 0; i < num_packets; ++i) {
      packets.push_back(BuildRtpPacket(type));
    }
    return packets;
  }

  Timestamp CurrentTime() { return time_controller_.GetClock()->CurrentTime(); }

  GlobalSimulatedTimeController time_controller_;
  MockPacketRouter packet_router_;
  TaskQueuePacedSender pacer_;
};

TEST_F(TaskQueuePacedSenderTest, PacesPackets) {
  // Insert a number of packets, covering one second.
  static constexpr size_t kPacketsToSend = 42;
  pacer_.SetPacingRates(
      DataRate::BitsPerSec(kDefaultPacketSize * 8 * kPacketsToSend),
      DataRate::Zero());
  pacer_.EnqueuePackets(
      GeneratePackets(RtpPacketMediaType::kVideo, kPacketsToSend));

  // Expect all of them to be sent.
  size_t packets_sent = 0;
  Timestamp end_time = Timestamp::PlusInfinity();
  EXPECT_CALL(packet_router_, SendPacket)
      .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& cluster_info) {
        ++packets_sent;
        if (packets_sent == kPacketsToSend) {
          end_time = time_controller_.GetClock()->CurrentTime();
        }
      });

  const Timestamp start_time = time_controller_.GetClock()->CurrentTime();

  // Packets should be sent over a period of close to 1s. Expect a little lower
  // than this since initial probing is a bit quicker.
  time_controller_.AdvanceTime(TimeDelta::Seconds(1));
  EXPECT_EQ(packets_sent, kPacketsToSend);
  ASSERT_TRUE(end_time.IsFinite());
  EXPECT_NEAR((end_time - start_time).ms<double>(), 1000.0, 50.0);
}

TEST_F(TaskQueuePacedSenderTest, ReschedulesProcessOnRateChange) {
  // Insert a number of packets to be sent 200ms apart.
  const size_t kPacketsPerSecond = 5;
  const DataRate kPacingRate =
      DataRate::BitsPerSec(kDefaultPacketSize * 8 * kPacketsPerSecond);
  pacer_.SetPacingRates(kPacingRate, DataRate::Zero());

  // Send some initial packets to be rid of any probes.
  EXPECT_CALL(packet_router_, SendPacket).Times(kPacketsPerSecond);
  pacer_.EnqueuePackets(
      GeneratePackets(RtpPacketMediaType::kVideo, kPacketsPerSecond));
  time_controller_.AdvanceTime(TimeDelta::Seconds(1));

  // Insert three packets, and record send time of each of them.
  // After the second packet is sent, double the send rate so we can
  // check the third packets is sent after half the wait time.
  Timestamp first_packet_time = Timestamp::MinusInfinity();
  Timestamp second_packet_time = Timestamp::MinusInfinity();
  Timestamp third_packet_time = Timestamp::MinusInfinity();

  EXPECT_CALL(packet_router_, SendPacket)
      .Times(3)
      .WillRepeatedly([&](std::unique_ptr<RtpPacketToSend> packet,
                          const PacedPacketInfo& cluster_info) {
        if (first_packet_time.IsInfinite()) {
          first_packet_time = CurrentTime();
        } else if (second_packet_time.IsInfinite()) {
          second_packet_time = CurrentTime();
          pacer_.SetPacingRates(2 * kPacingRate, DataRate::Zero());
        } else {
          third_packet_time = CurrentTime();
        }
      });

  pacer_.EnqueuePackets(GeneratePackets(RtpPacketMediaType::kVideo, 3));
  time_controller_.AdvanceTime(TimeDelta::Millis(500));
  ASSERT_TRUE(third_packet_time.IsFinite());
  EXPECT_NEAR((second_packet_time - first_packet_time).ms<double>(), 200.0,
              1.0);
  EXPECT_NEAR((third_packet_time - second_packet_time).ms<double>(), 100.0,
              1.0);
}

}  // namespace test
}  // namespace webrtc
