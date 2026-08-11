/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/network_emulation/leaky_bucket_network_queue.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include "api/test/simulated_network.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

LeakyBucketNetworkQueue::LeakyBucketNetworkQueue(const Config& config)
    : max_ect1_sojourn_time_(config.max_ect1_sojourn_time),
      target_ect1_sojourn_time_(config.target_ect1_sojourn_time),
      random_(config.seed) {
  RTC_DCHECK_LE(config.target_ect1_sojourn_time, config.max_ect1_sojourn_time);
}

void LeakyBucketNetworkQueue::SetMaxPacketCapacity(size_t max_capactiy) {
  max_packet_capacity_ = max_capactiy;
}

bool LeakyBucketNetworkQueue::EnqueuePacket(
    const PacketInFlightInfo& packet_info) {
  if (max_packet_capacity_ <= queue_.size()) {
    return false;
  }
  queue_.push(packet_info);
  return true;
}

std::optional<PacketInFlightInfo> LeakyBucketNetworkQueue::PeekNextPacket()
    const {
  if (queue_.empty()) {
    return std::nullopt;
  }
  return queue_.front();
}

std::optional<PacketInFlightInfo> LeakyBucketNetworkQueue::DequeuePacket(
    Timestamp time_now) {
  if (queue_.empty()) {
    return std::nullopt;
  }
  RTC_DCHECK_LE(queue_.front().send_time(), time_now);
  PacketInFlightInfo packet_info = queue_.front();
  queue_.pop();
  MaybeMarkAsCe(time_now, packet_info);
  return packet_info;
}

void LeakyBucketNetworkQueue::MaybeMarkAsCe(Timestamp time_now,
                                            PacketInFlightInfo& packet_info) {
  if (packet_info.ecn != EcnMarking::kEct1 ||
      target_ect1_sojourn_time_.IsInfinite() ||
      max_ect1_sojourn_time_.IsInfinite()) {
    return;
  }
  TimeDelta sojourn_time = time_now - packet_info.send_time();
  double p_mark =
      std::clamp((sojourn_time - target_ect1_sojourn_time_) /
                     (max_ect1_sojourn_time_ - target_ect1_sojourn_time_),
                 0.0, 1.0);
  if (random_.Rand<double>() < p_mark) {
    RTC_LOG(LS_VERBOSE) << "Marking packet " << packet_info.packet_id
                        << " as CE. p_mark: " << p_mark
                        << " sojourn_time: " << sojourn_time;
    packet_info.ecn = EcnMarking::kCe;
  }
}

bool LeakyBucketNetworkQueue::empty() const { return queue_.empty(); }

void LeakyBucketNetworkQueue::DropOldestPacket() {
  dropped_packets_.push_back(queue_.front());
  queue_.pop();
}

std::vector<PacketInFlightInfo>
LeakyBucketNetworkQueue::DequeueDroppedPackets() {
  std::vector<PacketInFlightInfo> dropped_packets;
  dropped_packets.swap(dropped_packets_);
  return dropped_packets;
}

}  // namespace webrtc
