/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_NETWORK_QUEUE_H_
#define API_TEST_NETWORK_EMULATION_NETWORK_QUEUE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "api/test/simulated_network.h"
#include "api/units/timestamp.h"

namespace webrtc {

// NetworkQueue defines the interface for a queue used in network simulation.
// The purpose is to allow for different AQM implementations.
// A queue should not modify PacketInFlightInfo except for the explicit
// congestion notification field (ecn).
class NetworkQueue {
 public:
  // Max capacity a queue is expected to handle.
  constexpr static size_t kMaxPacketCapacity = 10000;

  virtual ~NetworkQueue() = default;
  // Sets the max capacity of the queue. If there are already more than
  // `max_capacitiy` packets in the queue, the behaviour depends on the
  // implementation.
  virtual void SetMaxPacketCapacity(size_t max_capactiy) = 0;
  // Enqueues a packet.
  // Must return true if the packet is enqueued successfully, false otherwise.
  virtual bool EnqueuePacket(const PacketInFlightInfo& packet_info) = 0;
  // Next packet that can be dequeued.
  virtual std::optional<PacketInFlightInfo> PeekNextPacket() const = 0;
  // Dequeues a packet.
  // or std::nullopt if there are no enqueued packets.
  virtual std::optional<PacketInFlightInfo> DequeuePacket(
      Timestamp time_now) = 0;

  // Dequeues all packets that are dropped by the queue itself after being
  // enqueued.
  virtual std::vector<PacketInFlightInfo> DequeueDroppedPackets() = 0;
  virtual bool empty() const = 0;
};

class NetworkQueueFactory {
 public:
  virtual ~NetworkQueueFactory() = default;
  virtual std::unique_ptr<NetworkQueue> CreateQueue() = 0;
};

}  // namespace webrtc
#endif  // API_TEST_NETWORK_EMULATION_NETWORK_QUEUE_H_
