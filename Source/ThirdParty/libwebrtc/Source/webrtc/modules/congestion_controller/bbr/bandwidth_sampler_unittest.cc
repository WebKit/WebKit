/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// Based on the Quic implementation in Chromium.

#include <algorithm>

#include "modules/congestion_controller/bbr/bandwidth_sampler.h"

#include "test/gtest.h"

namespace webrtc {
namespace bbr {
namespace test {

class BandwidthSamplerPeer {
 public:
  static size_t GetNumberOfTrackedPackets(const BandwidthSampler& sampler) {
    return sampler.connection_state_map_.number_of_present_entries();
  }

  static DataSize GetPacketSize(const BandwidthSampler& sampler,
                                int64_t packet_number) {
    return sampler.connection_state_map_.GetEntry(packet_number)->size;
  }
};

const int64_t kRegularPacketSizeBytes = 1280;
// Enforce divisibility for some of the tests.
static_assert((kRegularPacketSizeBytes & 31) == 0,
              "kRegularPacketSizeBytes has to be five times divisible by 2");

const DataSize kRegularPacketSize = DataSize::bytes(kRegularPacketSizeBytes);

// A test fixture with utility methods for BandwidthSampler tests.
class BandwidthSamplerTest : public ::testing::Test {
 protected:
  BandwidthSamplerTest()
      : clock_(Timestamp::seconds(100)), bytes_in_flight_(DataSize::Zero()) {}

  Timestamp clock_;
  BandwidthSampler sampler_;
  DataSize bytes_in_flight_;

  void SendPacketInner(int64_t packet_number, DataSize bytes) {
    sampler_.OnPacketSent(clock_, packet_number, bytes, bytes_in_flight_);
    bytes_in_flight_ += bytes;
  }

  void SendPacket(int64_t packet_number) {
    SendPacketInner(packet_number, kRegularPacketSize);
  }

  BandwidthSample AckPacketInner(int64_t packet_number) {
    DataSize size =
        BandwidthSamplerPeer::GetPacketSize(sampler_, packet_number);
    bytes_in_flight_ -= size;
    return sampler_.OnPacketAcknowledged(clock_, packet_number);
  }

  // Acknowledge receipt of a packet and expect it to be not app-limited.
  DataRate AckPacket(int64_t packet_number) {
    BandwidthSample sample = AckPacketInner(packet_number);
    EXPECT_FALSE(sample.is_app_limited);
    return sample.bandwidth;
  }

  void LosePacket(int64_t packet_number) {
    DataSize size =
        BandwidthSamplerPeer::GetPacketSize(sampler_, packet_number);
    bytes_in_flight_ -= size;
    sampler_.OnPacketLost(packet_number);
  }

  // Sends one packet and acks it.  Then, send 20 packets.  Finally, send
  // another 20 packets while acknowledging previous 20.
  void Send40PacketsAndAckFirst20(TimeDelta time_between_packets) {
    // Send 20 packets at a constant inter-packet time.
    for (int64_t i = 1; i <= 20; i++) {
      SendPacket(i);
      clock_ += time_between_packets;
    }

    // Ack packets 1 to 20, while sending new packets at the same rate as
    // before.
    for (int64_t i = 1; i <= 20; i++) {
      AckPacket(i);
      SendPacket(i + 20);
      clock_ += time_between_packets;
    }
  }
};

// Test the sampler in a simple stop-and-wait sender setting.
TEST_F(BandwidthSamplerTest, SendAndWait) {
  TimeDelta time_between_packets = TimeDelta::ms(10);
  DataRate expected_bandwidth =
      kRegularPacketSize * 100 / TimeDelta::seconds(1);

  // Send packets at the constant bandwidth.
  for (int64_t i = 1; i < 20; i++) {
    SendPacket(i);
    clock_ += time_between_packets;
    DataRate current_sample = AckPacket(i);
    EXPECT_EQ(expected_bandwidth, current_sample);
  }

  // Send packets at the exponentially decreasing bandwidth.
  for (int64_t i = 20; i < 25; i++) {
    time_between_packets = time_between_packets * 2;
    expected_bandwidth = expected_bandwidth * 0.5;

    SendPacket(i);
    clock_ += time_between_packets;
    DataRate current_sample = AckPacket(i);
    EXPECT_EQ(expected_bandwidth, current_sample);
  }
  EXPECT_EQ(0u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  EXPECT_TRUE(bytes_in_flight_.IsZero());
}

// Test the sampler during regular windowed sender scenario with fixed
// CWND of 20.
TEST_F(BandwidthSamplerTest, SendPaced) {
  const TimeDelta time_between_packets = TimeDelta::ms(1);
  DataRate expected_bandwidth = kRegularPacketSize / time_between_packets;

  Send40PacketsAndAckFirst20(time_between_packets);

  // Ack the packets 21 to 40, arriving at the correct bandwidth.
  DataRate last_bandwidth = DataRate::Zero();
  for (int64_t i = 21; i <= 40; i++) {
    last_bandwidth = AckPacket(i);
    EXPECT_EQ(expected_bandwidth, last_bandwidth);
    clock_ += time_between_packets;
  }
  EXPECT_EQ(0u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  EXPECT_TRUE(bytes_in_flight_.IsZero());
}

// Test the sampler in a scenario where 50% of packets is consistently lost.
TEST_F(BandwidthSamplerTest, SendWithLosses) {
  const TimeDelta time_between_packets = TimeDelta::ms(1);
  DataRate expected_bandwidth = kRegularPacketSize / time_between_packets * 0.5;

  // Send 20 packets, each 1 ms apart.
  for (int64_t i = 1; i <= 20; i++) {
    SendPacket(i);
    clock_ += time_between_packets;
  }

  // Ack packets 1 to 20, losing every even-numbered packet, while sending new
  // packets at the same rate as before.
  for (int64_t i = 1; i <= 20; i++) {
    if (i % 2 == 0) {
      AckPacket(i);
    } else {
      LosePacket(i);
    }
    SendPacket(i + 20);
    clock_ += time_between_packets;
  }

  // Ack the packets 21 to 40 with the same loss pattern.
  DataRate last_bandwidth = DataRate::Zero();
  for (int64_t i = 21; i <= 40; i++) {
    if (i % 2 == 0) {
      last_bandwidth = AckPacket(i);
      EXPECT_EQ(expected_bandwidth, last_bandwidth);
    } else {
      LosePacket(i);
    }
    clock_ += time_between_packets;
  }
  EXPECT_EQ(0u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  EXPECT_TRUE(bytes_in_flight_.IsZero());
}

// Simulate a situation where ACKs arrive in burst and earlier than usual, thus
// producing an ACK rate which is higher than the original send rate.
TEST_F(BandwidthSamplerTest, CompressedAck) {
  const TimeDelta time_between_packets = TimeDelta::ms(1);
  DataRate expected_bandwidth = kRegularPacketSize / time_between_packets;

  Send40PacketsAndAckFirst20(time_between_packets);

  // Simulate an RTT somewhat lower than the one for 1-to-21 transmission.
  clock_ += time_between_packets * 15;

  // Ack the packets 21 to 40 almost immediately at once.
  DataRate last_bandwidth = DataRate::Zero();
  TimeDelta ridiculously_small_time_delta = TimeDelta::us(20);
  for (int64_t i = 21; i <= 40; i++) {
    last_bandwidth = AckPacket(i);
    clock_ += ridiculously_small_time_delta;
  }
  EXPECT_EQ(expected_bandwidth, last_bandwidth);
  EXPECT_EQ(0u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  EXPECT_TRUE(bytes_in_flight_.IsZero());
}

// Tests receiving ACK packets in the reverse order.
TEST_F(BandwidthSamplerTest, ReorderedAck) {
  const TimeDelta time_between_packets = TimeDelta::ms(1);
  DataRate expected_bandwidth = kRegularPacketSize / time_between_packets;

  Send40PacketsAndAckFirst20(time_between_packets);

  // Ack the packets 21 to 40 in the reverse order, while sending packets 41 to
  // 60.
  DataRate last_bandwidth = DataRate::Zero();
  for (int64_t i = 0; i < 20; i++) {
    last_bandwidth = AckPacket(40 - i);
    EXPECT_EQ(expected_bandwidth, last_bandwidth);
    SendPacket(41 + i);
    clock_ += time_between_packets;
  }

  // Ack the packets 41 to 60, now in the regular order.
  for (int64_t i = 41; i <= 60; i++) {
    last_bandwidth = AckPacket(i);
    EXPECT_EQ(expected_bandwidth, last_bandwidth);
    clock_ += time_between_packets;
  }
  EXPECT_EQ(0u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  EXPECT_TRUE(bytes_in_flight_.IsZero());
}

// Test the app-limited logic.
TEST_F(BandwidthSamplerTest, AppLimited) {
  const TimeDelta time_between_packets = TimeDelta::ms(1);
  DataRate expected_bandwidth = kRegularPacketSize / time_between_packets;

  Send40PacketsAndAckFirst20(time_between_packets);

  // We are now app-limited. Ack 21 to 40 as usual, but do not send anything for
  // now.
  sampler_.OnAppLimited();
  for (int64_t i = 21; i <= 40; i++) {
    DataRate current_sample = AckPacket(i);
    EXPECT_EQ(expected_bandwidth, current_sample);
    clock_ += time_between_packets;
  }

  // Enter quiescence.
  clock_ += TimeDelta::seconds(1);

  // Send packets 41 to 60, all of which would be marked as app-limited.
  for (int64_t i = 41; i <= 60; i++) {
    SendPacket(i);
    clock_ += time_between_packets;
  }

  // Ack packets 41 to 60, while sending packets 61 to 80.  41 to 60 should be
  // app-limited and underestimate the bandwidth due to that.
  for (int64_t i = 41; i <= 60; i++) {
    BandwidthSample sample = AckPacketInner(i);
    EXPECT_TRUE(sample.is_app_limited);
    EXPECT_LT(sample.bandwidth, 0.7f * expected_bandwidth);

    SendPacket(i + 20);
    clock_ += time_between_packets;
  }

  // Run out of packets, and then ack packet 61 to 80, all of which should have
  // correct non-app-limited samples.
  for (int64_t i = 61; i <= 80; i++) {
    DataRate last_bandwidth = AckPacket(i);
    EXPECT_EQ(expected_bandwidth, last_bandwidth);
    clock_ += time_between_packets;
  }

  EXPECT_EQ(0u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  EXPECT_TRUE(bytes_in_flight_.IsZero());
}

// Test the samples taken at the first flight of packets sent.
TEST_F(BandwidthSamplerTest, FirstRoundTrip) {
  const TimeDelta time_between_packets = TimeDelta::ms(1);
  const TimeDelta rtt = TimeDelta::ms(800);
  const int num_packets = 10;
  const DataSize num_bytes = kRegularPacketSize * num_packets;
  const DataRate real_bandwidth = num_bytes / rtt;

  for (int64_t i = 1; i <= 10; i++) {
    SendPacket(i);
    clock_ += time_between_packets;
  }

  clock_ += rtt - num_packets * time_between_packets;

  DataRate last_sample = DataRate::Zero();
  for (int64_t i = 1; i <= 10; i++) {
    DataRate sample = AckPacket(i);
    EXPECT_GT(sample, last_sample);
    last_sample = sample;
    clock_ += time_between_packets;
  }

  // The final measured sample for the first flight of sample is expected to be
  // smaller than the real bandwidth, yet it should not lose more than 10%. The
  // specific value of the error depends on the difference between the RTT and
  // the time it takes to exhaust the congestion window (i.e. in the limit when
  // all packets are sent simultaneously, last sample would indicate the real
  // bandwidth).
  EXPECT_LT(last_sample, real_bandwidth);
  EXPECT_GT(last_sample, 0.9f * real_bandwidth);
}

// Test sampler's ability to remove obsolete packets.
TEST_F(BandwidthSamplerTest, RemoveObsoletePackets) {
  SendPacket(1);
  SendPacket(2);
  SendPacket(3);
  SendPacket(4);
  SendPacket(5);

  clock_ += TimeDelta::ms(100);

  EXPECT_EQ(5u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  sampler_.RemoveObsoletePackets(4);
  EXPECT_EQ(2u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  sampler_.OnPacketLost(4);
  EXPECT_EQ(1u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
  AckPacket(5);
  EXPECT_EQ(0u, BandwidthSamplerPeer::GetNumberOfTrackedPackets(sampler_));
}

}  // namespace test
}  // namespace bbr
}  // namespace webrtc
