/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_NETWORK_EMULATION_DUAL_PI2_NETWORK_QUEUE_H_
#define API_TEST_NETWORK_EMULATION_DUAL_PI2_NETWORK_QUEUE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

#include "api/sequence_checker.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/random.h"

namespace webrtc {

// DualPi2NetworkQueue is a simplified version of the DualPi2 AQM controller in
// https://github.com/L4STeam/linux/. Concepts are described in
// https://datatracker.ietf.org/doc/html/rfc9332.
// Developed for testing purposes.
// Note that this implementation does not support the credit-based system
// (c_protection) from the real implementation and thus a L4S stream can
// completely starve a classic stream.
//
// TODO: bugs.webrtc.org/42225697 - Implement c_protection to better
// support testing of cross traffic with classic TCP.
class DualPi2NetworkQueue : public NetworkQueue {
 public:
  struct Config {
    // Target delay for the queue. The queue will try to keep the delay of the
    // L4S queue below this value.
    TimeDelta target_delay = TimeDelta::Micros(500);
    // Link rate puts a cap on how many bytes in total that can be stored in the
    // queue and still approximately meet the target delay. The cap is
    // calculated as: 2*target_delay * link_rate and applies to both queues
    // combined. If more packets than this are enqueued, they will be CE marked
    // (L4S) or dropped (classic).
    DataRate link_rate = DataRate::PlusInfinity();

    // These constants are used to calculate the proportional and integral
    // factors when updating the marking probability.
    // Values are from the original implementation.
    double alpha = 0.16;
    double beta = 3.2;
    // Coupling factor.
    int k = 2;

    // How often the base marking probability is updated.
    TimeDelta probability_update_interval = TimeDelta::Millis(16);
    int seed = 1;
  };

  DualPi2NetworkQueue() : DualPi2NetworkQueue(Config()) {}
  explicit DualPi2NetworkQueue(const Config& config);

  void SetMaxPacketCapacity(size_t max_packet_capacity) override;
  bool EnqueuePacket(const PacketInFlightInfo& packet_info) override;

  std::optional<PacketInFlightInfo> PeekNextPacket() const override;
  std::optional<PacketInFlightInfo> DequeuePacket(Timestamp time_now) override;
  std::vector<PacketInFlightInfo> DequeueDroppedPackets() override {
    // DualPi2 always tail drop packets.
    return {};
  }
  bool empty() const override;

  // Returns the marking probability of the L4S the l4s queue. Public for
  // testing.
  double l4s_marking_probability() const {
    return base_marking_probability_ * config_.k;
  }
  // Returns the drop probability of the classic queue. Public for
  // testing.
  double classic_drop_probability() const {
    return (base_marking_probability_ * base_marking_probability_);
  }

 private:
  void UpdateBaseMarkingProbability(Timestamp time_now, TimeDelta sojourn_time);
  bool ShouldTakeAction(double marking_probability);

  SequenceChecker sequence_checker_;

  const Config config_;
  const DataSize step_threshold_;

  std::queue<PacketInFlightInfo> l4s_queue_;
  std::queue<PacketInFlightInfo> classic_queue_;

  Random random_;

  std::optional<size_t> max_packet_capacity_;
  DataSize total_queued_size_;
  double base_marking_probability_ = 0;
  Timestamp last_probability_update_time_ = Timestamp::MinusInfinity();
  // The delay of the queue after the last probability update.
  TimeDelta previous_sojourn_time_ = TimeDelta::Zero();
};

class DualPi2NetworkQueueFactory : public NetworkQueueFactory {
 public:
  explicit DualPi2NetworkQueueFactory(const DualPi2NetworkQueue::Config& config)
      : config_(config) {}

  std::unique_ptr<NetworkQueue> CreateQueue() override {
    return std::make_unique<DualPi2NetworkQueue>(config_);
  }

 private:
  const DualPi2NetworkQueue::Config config_;
};
}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_DUAL_PI2_NETWORK_QUEUE_H_
