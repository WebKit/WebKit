/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/call/call.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/fake_network_pipe.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::Invoke;

namespace webrtc {

class TestDemuxer : public Demuxer {
 public:
  void IncomingPacket(NetworkPacket* packet) {
    DeliverPacket(packet, PacketTime());
  }

  MOCK_METHOD1(SetReceiver, void(PacketReceiver* receiver));
  MOCK_METHOD2(DeliverPacket,
               void(const NetworkPacket* packet,
                    const PacketTime& packet_time));
};

class ReorderTestDemuxer : public TestDemuxer {
 public:
  void DeliverPacket(const NetworkPacket* packet,
                     const PacketTime& packet_time) override {
    RTC_DCHECK_GE(packet->data_length(), sizeof(int));
    int seq_num;
    memcpy(&seq_num, packet->data(), sizeof(int));
    delivered_sequence_numbers_.push_back(seq_num);
  }
  std::vector<int> delivered_sequence_numbers_;
};

class MockReceiver : public PacketReceiver {
 public:
  MOCK_METHOD4(
      DeliverPacket,
      DeliveryStatus(MediaType, const uint8_t*, size_t, const PacketTime&));
};

class FakeNetworkPipeTest : public ::testing::Test {
 public:
  FakeNetworkPipeTest() : fake_clock_(12345) {}

 protected:
  void SendPackets(FakeNetworkPipe* pipe, int number_packets, int packet_size) {
    RTC_DCHECK_GE(packet_size, sizeof(int));
    std::unique_ptr<uint8_t[]> packet(new uint8_t[packet_size]);
    for (int i = 0; i < number_packets; ++i) {
      // Set a sequence number for the packets by
      // using the first bytes in the packet.
      memcpy(packet.get(), &i, sizeof(int));
      pipe->SendPacket(packet.get(), packet_size);
    }
  }

  int PacketTimeMs(int capacity_kbps, int packet_size) const {
    return 8 * packet_size / capacity_kbps;
  }

  SimulatedClock fake_clock_;
};

// Test the capacity link and verify we get as many packets as we expect.
TEST_F(FakeNetworkPipeTest, CapacityTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 20;
  config.link_capacity_kbps = 80;
  TestDemuxer* demuxer = new TestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));

  // Add 10 packets of 1000 bytes, = 80 kb, and verify it takes one second to
  // get through the pipe.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Time haven't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(0);
  pipe->Process();

  // Advance enough time to release one packet.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(1);
  pipe->Process();

  // Release all but one packet
  fake_clock_.AdvanceTimeMilliseconds(9 * kPacketTimeMs - 1);
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(8);
  pipe->Process();

  // And the last one.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(1);
  pipe->Process();
}

// Test the extra network delay.
TEST_F(FakeNetworkPipeTest, ExtraDelayTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 20;
  config.queue_delay_ms = 100;
  config.link_capacity_kbps = 80;
  TestDemuxer* demuxer = new TestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));

  const int kNumPackets = 2;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Increase more than kPacketTimeMs, but not more than the extra delay.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(0);
  pipe->Process();

  // Advance the network delay to get the first packet.
  fake_clock_.AdvanceTimeMilliseconds(config.queue_delay_ms);
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(1);
  pipe->Process();

  // Advance one more kPacketTimeMs to get the last packet.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(1);
  pipe->Process();
}

// Test the number of buffers and packets are dropped when sending too many
// packets too quickly.
TEST_F(FakeNetworkPipeTest, QueueLengthTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 2;
  config.link_capacity_kbps = 80;
  TestDemuxer* demuxer = new TestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);

  // Increase time enough to deliver all three packets, verify only two are
  // delivered.
  fake_clock_.AdvanceTimeMilliseconds(3 * kPacketTimeMs);
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(2);
  pipe->Process();
}

// Test we get statistics as expected.
TEST_F(FakeNetworkPipeTest, StatisticsTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 2;
  config.queue_delay_ms = 20;
  config.link_capacity_kbps = 80;
  TestDemuxer* demuxer = new TestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(3 * kPacketTimeMs +
                                      config.queue_delay_ms);

  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(2);
  pipe->Process();

  // Packet 1: kPacketTimeMs + config.queue_delay_ms,
  // packet 2: 2 * kPacketTimeMs + config.queue_delay_ms => 170 ms average.
  EXPECT_EQ(pipe->AverageDelay(), 170);
  EXPECT_EQ(pipe->sent_packets(), 2u);
  EXPECT_EQ(pipe->dropped_packets(), 1u);
  EXPECT_EQ(pipe->PercentageLoss(), 1/3.f);
}

// Change the link capacity half-way through the test and verify that the
// delivery times change accordingly.
TEST_F(FakeNetworkPipeTest, ChangingCapacityWithEmptyPipeTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 20;
  config.link_capacity_kbps = 80;
  TestDemuxer* demuxer = new TestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));

  // Add 10 packets of 1000 bytes, = 80 kb, and verify it takes one second to
  // get through the pipe.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  int packet_time_ms = PacketTimeMs(config.link_capacity_kbps, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(0);
  pipe->Process();

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_ms);
    EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(1);
    pipe->Process();
  }

  // Change the capacity.
  config.link_capacity_kbps /= 2;  // Reduce to 50%.
  pipe->SetConfig(config);

  // Add another 10 packets of 1000 bytes, = 80 kb, and verify it takes two
  // seconds to get them through the pipe.
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  packet_time_ms = PacketTimeMs(config.link_capacity_kbps, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(0);
  pipe->Process();

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_ms);
    EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(1);
    pipe->Process();
  }

  // Check that all the packets were sent.
  EXPECT_EQ(static_cast<size_t>(2 * kNumPackets), pipe->sent_packets());
  fake_clock_.AdvanceTimeMilliseconds(pipe->TimeUntilNextProcess());
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(0);
  pipe->Process();
}

// Change the link capacity half-way through the test and verify that the
// delivery times change accordingly.
TEST_F(FakeNetworkPipeTest, ChangingCapacityWithPacketsInPipeTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 20;
  config.link_capacity_kbps = 80;
  TestDemuxer* demuxer = new TestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));

  // Add 10 packets of 1000 bytes, = 80 kb.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link at the initial speed.
  int packet_time_1_ms = PacketTimeMs(config.link_capacity_kbps, kPacketSize);

  // Change the capacity.
  config.link_capacity_kbps *= 2;  // Double the capacity.
  pipe->SetConfig(config);

  // Add another 10 packets of 1000 bytes, = 80 kb, and verify it takes two
  // seconds to get them through the pipe.
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link at the new capacity.
  int packet_time_2_ms = PacketTimeMs(config.link_capacity_kbps, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(0);
  pipe->Process();

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_1_ms);
    EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(1);
    pipe->Process();
  }

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_2_ms);
    EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(1);
    pipe->Process();
  }

  // Check that all the packets were sent.
  EXPECT_EQ(static_cast<size_t>(2 * kNumPackets), pipe->sent_packets());
  fake_clock_.AdvanceTimeMilliseconds(pipe->TimeUntilNextProcess());
  EXPECT_CALL(*demuxer, DeliverPacket(_, _)).Times(0);
  pipe->Process();
}

// At first disallow reordering and then allow reordering.
TEST_F(FakeNetworkPipeTest, DisallowReorderingThenAllowReordering) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 1000;
  config.link_capacity_kbps = 800;
  config.queue_delay_ms = 100;
  config.delay_standard_deviation_ms = 10;
  ReorderTestDemuxer* demuxer = new ReorderTestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));

  const uint32_t kNumPackets = 100;
  const int kPacketSize = 10;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  pipe->Process();

  // Confirm that all packets have been delivered in order.
  EXPECT_EQ(kNumPackets, demuxer->delivered_sequence_numbers_.size());
  int last_seq_num = -1;
  for (int seq_num : demuxer->delivered_sequence_numbers_) {
    EXPECT_GT(seq_num, last_seq_num);
    last_seq_num = seq_num;
  }

  config.allow_reordering = true;
  pipe->SetConfig(config);
  SendPackets(pipe.get(), kNumPackets, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  demuxer->delivered_sequence_numbers_.clear();
  pipe->Process();

  // Confirm that all packets have been delivered
  // and that reordering has occured.
  EXPECT_EQ(kNumPackets, demuxer->delivered_sequence_numbers_.size());
  bool reordering_has_occured = false;
  last_seq_num = -1;
  for (int seq_num : demuxer->delivered_sequence_numbers_) {
    if (last_seq_num > seq_num) {
      reordering_has_occured = true;
      break;
    }
    last_seq_num = seq_num;
  }
  EXPECT_TRUE(reordering_has_occured);
}

TEST_F(FakeNetworkPipeTest, BurstLoss) {
  const int kLossPercent = 5;
  const int kAvgBurstLength = 3;
  const int kNumPackets = 10000;
  const int kPacketSize = 10;

  FakeNetworkPipe::Config config;
  config.queue_length_packets = kNumPackets;
  config.loss_percent = kLossPercent;
  config.avg_burst_loss_length = kAvgBurstLength;
  ReorderTestDemuxer* demuxer = new ReorderTestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));

  SendPackets(pipe.get(), kNumPackets, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  pipe->Process();

  // Check that the average loss is |kLossPercent| percent.
  int lost_packets = kNumPackets - demuxer->delivered_sequence_numbers_.size();
  double loss_fraction = lost_packets / static_cast<double>(kNumPackets);

  EXPECT_NEAR(kLossPercent / 100.0, loss_fraction, 0.05);

  // Find the number of bursts that has occurred.
  size_t received_packets = demuxer->delivered_sequence_numbers_.size();
  int num_bursts = 0;
  for (size_t i = 0; i < received_packets - 1; ++i) {
    int diff = demuxer->delivered_sequence_numbers_[i + 1] -
               demuxer->delivered_sequence_numbers_[i];
    if (diff > 1)
      ++num_bursts;
  }

  double average_burst_length = static_cast<double>(lost_packets) / num_bursts;

  EXPECT_NEAR(kAvgBurstLength, average_burst_length, 0.3);
}

TEST_F(FakeNetworkPipeTest, SetReceiver) {
  FakeNetworkPipe::Config config;
  TestDemuxer* demuxer = new TestDemuxer();
  std::unique_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(
      &fake_clock_, config, std::unique_ptr<Demuxer>(demuxer)));
  MockReceiver packet_receiver;
  EXPECT_CALL(*demuxer, SetReceiver(&packet_receiver)).Times(1);
  pipe->SetReceiver(&packet_receiver);
}

TEST(DemuxerImplTest, Demuxing) {
  constexpr uint8_t kVideoPayloadType = 100;
  constexpr uint8_t kAudioPayloadType = 101;
  constexpr int64_t kTimeNow = 12345;
  constexpr int64_t kArrivalTime = kTimeNow - 1;
  constexpr size_t kPacketSize = 10;
  DemuxerImpl demuxer({{kVideoPayloadType, MediaType::VIDEO},
                       {kAudioPayloadType, MediaType::AUDIO}});

  MockReceiver mock_receiver;
  demuxer.SetReceiver(&mock_receiver);

  std::vector<uint8_t> data(kPacketSize);
  data[1] = kVideoPayloadType;
  std::unique_ptr<NetworkPacket> packet(
      new NetworkPacket(&data[0], kPacketSize, kTimeNow, kArrivalTime));
  EXPECT_CALL(mock_receiver, DeliverPacket(MediaType::VIDEO, _, _, _))
      .WillOnce(Return(PacketReceiver::DELIVERY_OK));
  demuxer.DeliverPacket(packet.get(), PacketTime());

  data[1] = kAudioPayloadType;
  packet.reset(
      new NetworkPacket(&data[0], kPacketSize, kTimeNow, kArrivalTime));
  EXPECT_CALL(mock_receiver, DeliverPacket(MediaType::AUDIO, _, _, _))
      .WillOnce(Return(PacketReceiver::DELIVERY_OK));
  demuxer.DeliverPacket(packet.get(), PacketTime());
}

}  // namespace webrtc
