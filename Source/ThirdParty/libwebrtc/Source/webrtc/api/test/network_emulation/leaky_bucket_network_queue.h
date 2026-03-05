/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_LEAKY_BUCKET_NETWORK_QUEUE_H_
#define API_TEST_NETWORK_EMULATION_LEAKY_BUCKET_NETWORK_QUEUE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "api/test/network_emulation/network_queue.h"
#include "api/test/simulated_network.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/random.h"

namespace webrtc {

// A network queue that uses a leaky bucket to limit the number of packets that
// can be queued.
class LeakyBucketNetworkQueue : public NetworkQueue {
 public:
  struct Config {
    int seed = 1;
    // If enqueued packets are sent as ECT1 and sojourn time is larger than
    // `target_ect1_sojourn_time`, packets will be marked as CE with
    // probability (sojourn_time - `target_ect1_sojourn_time`) /
    // (`max_ect1_sojourn_time` - `target_ect1_sojourn_time`)
    TimeDelta max_ect1_sojourn_time = TimeDelta::PlusInfinity();
    TimeDelta target_ect1_sojourn_time = TimeDelta::PlusInfinity();
  };

  LeakyBucketNetworkQueue() : LeakyBucketNetworkQueue(Config()) {}
  explicit LeakyBucketNetworkQueue(const Config& config);
  // If `max_capacity` is larger than current queue length, existing packets are
  // not dropped. But the queue will not accept new packets until queue length
  // is below `max_capacity`,
  void SetMaxPacketCapacity(size_t max_capactiy) override;

  bool EnqueuePacket(const PacketInFlightInfo& packet_info) override;
  std::optional<PacketInFlightInfo> PeekNextPacket() const override;
  std::optional<PacketInFlightInfo> DequeuePacket(Timestamp time_now) override;
  std::vector<PacketInFlightInfo> DequeueDroppedPackets() override;
  bool empty() const override;

  void DropOldestPacket();

 private:
  void MaybeMarkAsCe(Timestamp time_now, PacketInFlightInfo& packet_info);

  size_t max_packet_capacity_ = kMaxPacketCapacity;
  const TimeDelta max_ect1_sojourn_time_;
  const TimeDelta target_ect1_sojourn_time_;

  Random random_;
  std::queue<PacketInFlightInfo> queue_;
  std::vector<PacketInFlightInfo> dropped_packets_;
};

class LeakyBucketNetworkQueueFactory : public NetworkQueueFactory {
 public:
  LeakyBucketNetworkQueueFactory()
      : LeakyBucketNetworkQueueFactory(LeakyBucketNetworkQueue::Config()) {}
  explicit LeakyBucketNetworkQueueFactory(
      const LeakyBucketNetworkQueue::Config& config)
      : config_(config) {}

  std::unique_ptr<NetworkQueue> CreateQueue() override {
    return std::make_unique<LeakyBucketNetworkQueue>(config_);
  }

 private:
  const LeakyBucketNetworkQueue::Config config_;
};
}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_LEAKY_BUCKET_NETWORK_QUEUE_H_
