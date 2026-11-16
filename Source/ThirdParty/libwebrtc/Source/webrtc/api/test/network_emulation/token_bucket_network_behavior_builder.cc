/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/network_emulation/token_bucket_network_behavior_builder.h"

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/function_view.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/network_emulation/token_bucket_network_behavior_config.h"
#include "api/test/network_emulation_manager.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "test/network/token_bucket_network_behavior.h"

namespace webrtc {

TokenBucketNetworkBehaviorNodeBuilder::TokenBucketNetworkBehaviorNodeBuilder(
    NetworkEmulationManager* net)
    : net_(net) {}

TokenBucketNetworkBehaviorNodeBuilder&
TokenBucketNetworkBehaviorNodeBuilder::burst(DataSize burst) {
  config_.burst = burst;
  return *this;
}

TokenBucketNetworkBehaviorNodeBuilder&
TokenBucketNetworkBehaviorNodeBuilder::rate(DataRate rate) {
  config_.rate = rate;
  return *this;
}

TokenBucketNetworkBehaviorNodeBuilder&
TokenBucketNetworkBehaviorNodeBuilder::queue_factory(
    NetworkQueueFactory& queue_factory) {
  queue_factory_ = &queue_factory;
  return *this;
}

EmulatedNetworkNode* TokenBucketNetworkBehaviorNodeBuilder::Build() {
  std::unique_ptr<NetworkQueue> queue;
  if (queue_factory_ != nullptr) {
    queue = queue_factory_->CreateQueue();
  }
  auto traffic_policer =
      std::make_unique<TokenBucketNetworkBehavior>(config_, std::move(queue));
  return net_->CreateEmulatedNode(std::move(traffic_policer));
}

std::pair<EmulatedNetworkNode*,
          absl::AnyInvocable<
              void(FunctionView<void(TokenBucketNetworkBehaviorConfig&)>)>>
TokenBucketNetworkBehaviorNodeBuilder::BuildWithUpdateFunction() {
  std::unique_ptr<NetworkQueue> queue;
  if (queue_factory_ != nullptr) {
    queue = queue_factory_->CreateQueue();
  }
  auto traffic_policer =
      std::make_unique<TokenBucketNetworkBehavior>(config_, std::move(queue));
  absl::AnyInvocable<void(
      FunctionView<void(TokenBucketNetworkBehaviorConfig&)>)>
      update_config_function =
          [traffic_policer = traffic_policer.get()](
              FunctionView<void(TokenBucketNetworkBehaviorConfig&)>
                  configurer) { traffic_policer->UpdateConfig(configurer); };
  return std::make_pair(net_->CreateEmulatedNode(std::move(traffic_policer)),
                        std::move(update_config_function));
}

}  // namespace webrtc
