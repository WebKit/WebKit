/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/network/token_bucket_network_behavior.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/function_view.h"
#include "api/sequence_checker.h"
#include "api/test/network_emulation/network_queue.h"
#include "api/test/network_emulation/token_bucket_network_behavior_config.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

TokenBucketNetworkBehavior::TokenBucketNetworkBehavior(
    const TokenBucketNetworkBehaviorConfig& config)
    : TokenBucketNetworkBehavior(config, nullptr) {}

TokenBucketNetworkBehavior::TokenBucketNetworkBehavior(
    const TokenBucketNetworkBehaviorConfig& config,
    std::unique_ptr<NetworkQueue> queue)
    : config_(config), queue_(std::move(queue)), token_bucket_(config.burst) {
  sequence_checker_.Detach();
}

bool TokenBucketNetworkBehavior::EnqueuePacket(PacketInFlightInfo packet_info) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  bool result = false;
  Timestamp time_now = packet_info.send_time();
  RefillTokensSinceLastProcess(time_now);
  if (queue_ != nullptr) {
    result = queue_->EnqueuePacket(packet_info);
    if (next_delivery_time_.IsInfinite()) {
      next_delivery_time_ =
          CalculateNextDequeueTime(time_now, queue_->PeekNextPacket());
    }
  } else {
    // no queue.
    Timestamp next_delivery_time =
        CalculateNextDequeueTime(time_now, packet_info);
    if (next_delivery_time == time_now) {
      // There is enough tokens to deliver the packet immediately.
      PrepareToDeliverPacket(time_now, packet_info);
      return true;
    }
  }
  return result;
}

void TokenBucketNetworkBehavior::PrepareToDeliverPacket(
    Timestamp time_now,
    const PacketInFlightInfo& packet_to_deliver) {
  token_bucket_ -= packet_to_deliver.packet_size();
  PacketDeliveryInfo packet(packet_to_deliver, time_now.us());
  deliverable_packets_.push_back(packet);
  next_delivery_time_ = time_now;
}

void TokenBucketNetworkBehavior::RefillTokensSinceLastProcess(
    Timestamp time_now) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!last_process_time_) {
    last_process_time_ = time_now;
  }

  // Refill token bucket.
  TimeDelta time_delta = time_now - *last_process_time_;
  webrtc::MutexLock lock(&config_lock_);
  if (time_delta > TimeDelta::Zero()) {
    token_bucket_ += config_.rate * time_delta;
    token_bucket_ = std::min(token_bucket_, config_.burst);
  }
  last_process_time_ = time_now;
}

Timestamp TokenBucketNetworkBehavior::CalculateNextDequeueTime(
    Timestamp time_now,
    std::optional<PacketInFlightInfo> packet_info) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!packet_info.has_value()) {
    return Timestamp::PlusInfinity();
  }
  if (packet_info->packet_size() <= token_bucket_) {
    return time_now;
  }
  MutexLock lock(&config_lock_);
  if (config_.rate == DataRate::Zero()) {
    return Timestamp::PlusInfinity();
  }
  TimeDelta time_to_enough_tokens =
      (packet_info->packet_size() - token_bucket_) / config_.rate;
  return time_now + time_to_enough_tokens;
}

std::vector<PacketDeliveryInfo>
TokenBucketNetworkBehavior::DequeueDeliverablePackets(int64_t time_now_us) {
  Timestamp time_now = Timestamp::Micros(time_now_us);
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RefillTokensSinceLastProcess(time_now);
  next_delivery_time_ = Timestamp::PlusInfinity();
  if (queue_ != nullptr) {
    while (CalculateNextDequeueTime(time_now, queue_->PeekNextPacket()) <=
           time_now) {
      std::optional<PacketInFlightInfo> packet =
          queue_->DequeuePacket(time_now);
      RTC_CHECK(packet.has_value());
      PrepareToDeliverPacket(time_now, *packet);
    }
    for (const auto& packet_in_flight_info : queue_->DequeueDroppedPackets()) {
      PacketDeliveryInfo packet(packet_in_flight_info,
                                PacketDeliveryInfo::kNotReceived);
      deliverable_packets_.push_back(packet);
    }
    next_delivery_time_ =
        CalculateNextDequeueTime(time_now, queue_->PeekNextPacket());
  }
  std::vector<PacketDeliveryInfo> delivered_packets;
  delivered_packets.swap(deliverable_packets_);
  return delivered_packets;
}

std::optional<int64_t> TokenBucketNetworkBehavior::NextDeliveryTimeUs() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return next_delivery_time_.IsFinite()
             ? std::make_optional<int64_t>(next_delivery_time_.us())
             : std::nullopt;
}

void TokenBucketNetworkBehavior::UpdateConfig(
    webrtc::FunctionView<void(TokenBucketNetworkBehaviorConfig&)> configurer) {
  MutexLock lock(&config_lock_);
  configurer(config_);
}

}  // namespace webrtc
