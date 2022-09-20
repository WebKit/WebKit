/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "call/simulated_network.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/units/data_rate.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr int kNotReceived = PacketDeliveryInfo::kNotReceived;
}

TEST(SimulatedNetworkTest, CodelDoesNothingAtCapacity) {
  const TimeDelta kRuntime = TimeDelta::Seconds(30);

  DataRate link_capacity = DataRate::KilobitsPerSec(1000);
  const DataSize packet_size = DataSize::Bytes(1000);

  SimulatedNetwork::Config config;
  config.codel_active_queue_management = true;
  config.queue_delay_ms = 10;
  config.link_capacity_kbps = link_capacity.kbps();
  SimulatedNetwork network(config);

  // Need to round up here as otherwise we actually will choke.
  const TimeDelta packet_inverval =
      packet_size / link_capacity + TimeDelta::Millis(1);

  // Send at capacity and see we get no loss.
  Timestamp start_time = Timestamp::Zero();
  Timestamp current_time = start_time;
  Timestamp next_packet_time = start_time;
  uint64_t next_id = 0;
  std::set<uint64_t> pending;
  while (current_time - start_time < kRuntime) {
    if (current_time >= next_packet_time) {
      bool success = network.EnqueuePacket(PacketInFlightInfo{
          packet_size.bytes<size_t>(), current_time.us(), next_id});
      EXPECT_TRUE(success);
      pending.insert(next_id);
      ++next_id;
      next_packet_time += packet_inverval;
    }
    Timestamp next_delivery = Timestamp::PlusInfinity();
    if (network.NextDeliveryTimeUs())
      next_delivery = Timestamp::Micros(*network.NextDeliveryTimeUs());
    current_time = std::min(next_packet_time, next_delivery);
    if (current_time >= next_delivery) {
      for (PacketDeliveryInfo packet :
           network.DequeueDeliverablePackets(current_time.us())) {
        EXPECT_NE(packet.receive_time_us, kNotReceived);
        pending.erase(packet.packet_id);
      }
    }
  }
  while (network.NextDeliveryTimeUs()) {
    for (PacketDeliveryInfo packet :
         network.DequeueDeliverablePackets(*network.NextDeliveryTimeUs())) {
      EXPECT_NE(packet.receive_time_us, kNotReceived);
      pending.erase(packet.packet_id);
    }
  }
  EXPECT_EQ(pending.size(), 0u);
}

TEST(SimulatedNetworkTest, CodelLimitsDelayAndDropsPacketsOnOverload) {
  const TimeDelta kRuntime = TimeDelta::Seconds(30);
  const TimeDelta kCheckInterval = TimeDelta::Millis(2000);

  DataRate link_capacity = DataRate::KilobitsPerSec(1000);
  const DataSize rough_packet_size = DataSize::Bytes(1500);
  const double overload_rate = 1.5;

  SimulatedNetwork::Config config;
  config.codel_active_queue_management = true;
  config.queue_delay_ms = 10;
  config.link_capacity_kbps = link_capacity.kbps();
  SimulatedNetwork network(config);

  const TimeDelta packet_inverval = rough_packet_size / link_capacity;
  const DataSize packet_size = overload_rate * link_capacity * packet_inverval;
  // Send above capacity and see delays are still controlled at the cost of
  // packet loss.
  Timestamp start_time = Timestamp::Zero();
  Timestamp current_time = start_time;
  Timestamp next_packet_time = start_time;
  Timestamp last_check = start_time;
  uint64_t next_id = 1;
  std::map<uint64_t, int64_t> send_times_us;
  int lost = 0;
  std::vector<int64_t> delays_us;
  while (current_time - start_time < kRuntime) {
    if (current_time >= next_packet_time) {
      bool success = network.EnqueuePacket(PacketInFlightInfo{
          packet_size.bytes<size_t>(), current_time.us(), next_id});
      send_times_us.insert({next_id, current_time.us()});
      ++next_id;
      EXPECT_TRUE(success);
      next_packet_time += packet_inverval;
    }
    Timestamp next_delivery = Timestamp::PlusInfinity();
    if (network.NextDeliveryTimeUs())
      next_delivery = Timestamp::Micros(*network.NextDeliveryTimeUs());
    current_time = std::min(next_packet_time, next_delivery);
    if (current_time >= next_delivery) {
      for (PacketDeliveryInfo packet :
           network.DequeueDeliverablePackets(current_time.us())) {
        if (packet.receive_time_us == kNotReceived) {
          ++lost;
        } else {
          delays_us.push_back(packet.receive_time_us -
                              send_times_us[packet.packet_id]);
        }
        send_times_us.erase(packet.packet_id);
      }
    }
    if (current_time > last_check + kCheckInterval) {
      last_check = current_time;
      TimeDelta average_delay =
          TimeDelta::Micros(absl::c_accumulate(delays_us, 0)) /
          delays_us.size();
      double loss_ratio = static_cast<double>(lost) / (lost + delays_us.size());
      EXPECT_LT(average_delay.ms(), 200)
          << "Time " << (current_time - start_time).ms() << "\n";
      EXPECT_GT(loss_ratio, 0.5 * (overload_rate - 1));
    }
  }
  while (network.NextDeliveryTimeUs()) {
    for (PacketDeliveryInfo packet :
         network.DequeueDeliverablePackets(*network.NextDeliveryTimeUs())) {
      send_times_us.erase(packet.packet_id);
    }
  }
  EXPECT_EQ(send_times_us.size(), 0u);
}
}  // namespace webrtc
