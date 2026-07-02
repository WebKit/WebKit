/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_TOKEN_BUCKET_NETWORK_BEHAVIOR_H_
#define TEST_NETWORK_TOKEN_BUCKET_NETWORK_BEHAVIOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "api/function_view.h"
#include "api/sequence_checker.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/network_emulation/token_bucket_network_behavior_config.h"
#include "api/test/simulated_network.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

// The TokenBucketNetworkBehavior forwards enqueued packets at the rate limit.
// It is implemented using the token bucket algorithm
// (https://en.wikipedia.org/wiki/Token_bucket), allowing bursts of packets
// through.
//  If no queue is specified, packets are dropped over the rate limit instead of
//  queuing. This is typically used to rate limit inbound traffic.
//
// The token bucket is configured to have a maximum size and a constant refill
// rate.
// When a packet comes in, it is queued and and dequeued when there are enough
// tokens in the bucket. If there is no queue and there are no tokens available,
// the packet is dropped.
class TokenBucketNetworkBehavior : public NetworkBehaviorInterface {
 public:
  explicit TokenBucketNetworkBehavior(
      const TokenBucketNetworkBehaviorConfig& config);
  TokenBucketNetworkBehavior(const TokenBucketNetworkBehaviorConfig& config,
                             std::unique_ptr<NetworkQueue> queue);

  bool EnqueuePacket(PacketInFlightInfo packet_info) override;
  std::vector<PacketDeliveryInfo> DequeueDeliverablePackets(
      int64_t time_now_us) override;
  std::optional<int64_t> NextDeliveryTimeUs() const override;

  using ConfigFunction =
      webrtc::FunctionView<void(TokenBucketNetworkBehaviorConfig&)>;
  void UpdateConfig(ConfigFunction configurer);

 private:
  Timestamp CalculateNextDequeueTime(
      Timestamp time_now,
      std::optional<PacketInFlightInfo> packet_info);

  void RefillTokensSinceLastProcess(Timestamp time_now);
  void PrepareToDeliverPacket(Timestamp time_now,
                              const PacketInFlightInfo& packet_to_deliver);

  SequenceChecker sequence_checker_;
  Mutex config_lock_;
  TokenBucketNetworkBehaviorConfig config_;
  std::unique_ptr<NetworkQueue> queue_;

  DataSize token_bucket_;
  std::vector<PacketDeliveryInfo> deliverable_packets_;
  std::optional<Timestamp> last_process_time_;
  Timestamp next_delivery_time_ = Timestamp::PlusInfinity();
};

}  // namespace webrtc

#endif  //  TEST_NETWORK_TOKEN_BUCKET_NETWORK_BEHAVIOR_H_
