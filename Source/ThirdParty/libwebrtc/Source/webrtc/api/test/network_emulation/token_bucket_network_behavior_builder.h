/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_TOKEN_BUCKET_NETWORK_BEHAVIOR_BUILDER_H_
#define API_TEST_NETWORK_EMULATION_TOKEN_BUCKET_NETWORK_BEHAVIOR_BUILDER_H_

#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/function_view.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/network_emulation/token_bucket_network_behavior_config.h"
#include "api/test/network_emulation_manager.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"

namespace webrtc {

// Builder for a TokenBucketNetworkBehavior
//
// TokenBucketNetworkBehavior forwards enqueued packets at the rate limit.
// It is implemented using the token bucket algorithm
// (https://en.wikipedia.org/wiki/Token_bucket), allowing bursts of packets
// through.
//  If no queue is factory is specified, packets are dropped over the rate limit
//  instead of queuing. This is typically used to rate limit inbound traffic.
//
// The token bucket is configured to have a maximum size and a constant refill
// rate.
// When a packet comes in, it is queued and and dequeued when there are enough
// tokens in the bucket. If there is no queue and there are no tokens available,
// the packet is dropped.
class TokenBucketNetworkBehaviorNodeBuilder {
 public:
  explicit TokenBucketNetworkBehaviorNodeBuilder(NetworkEmulationManager* net);
  TokenBucketNetworkBehaviorNodeBuilder& burst(DataSize burst);
  TokenBucketNetworkBehaviorNodeBuilder& rate(DataRate rate);
  // If set, `queue_factory` must outlive the Builder.
  // Per default, no queue is created and the policer uses a zero capacity
  // queue, dropping packets immediately if they don't fit in the burst.
  TokenBucketNetworkBehaviorNodeBuilder& queue_factory(
      NetworkQueueFactory& queue_factory);
  EmulatedNetworkNode* Build();
  std::pair<EmulatedNetworkNode*,
            absl::AnyInvocable<
                void(FunctionView<void(TokenBucketNetworkBehaviorConfig&)>)>>
  BuildWithUpdateFunction();

 private:
  NetworkEmulationManager* const net_;
  TokenBucketNetworkBehaviorConfig config_;
  NetworkQueueFactory* queue_factory_ = nullptr;
};

}  // namespace webrtc

#endif  // API_TEST_NETWORK_EMULATION_TOKEN_BUCKET_NETWORK_BEHAVIOR_BUILDER_H_
