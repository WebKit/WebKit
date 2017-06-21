/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>

#include "webrtc/p2p/base/packetlossestimator.h"

#include "webrtc/base/checks.h"

namespace cricket {

PacketLossEstimator::PacketLossEstimator(int64_t consider_lost_after_ms,
                                         int64_t forget_after_ms)
    : consider_lost_after_ms_(consider_lost_after_ms),
      forget_after_ms_(forget_after_ms) {
  RTC_DCHECK_LT(consider_lost_after_ms, forget_after_ms);
}

void PacketLossEstimator::ExpectResponse(std::string id, int64_t sent_time) {
  tracked_packets_[id] = PacketInfo{sent_time, false};

  // Called to free memory in case the client hasn't called UpdateResponseRate
  // in a while.
  MaybeForgetOldRequests(sent_time);
}

void PacketLossEstimator::ReceivedResponse(std::string id,
                                           int64_t received_time) {
  auto iter = tracked_packets_.find(id);
  if (iter != tracked_packets_.end()) {
    auto& packet_info = iter->second;
    packet_info.response_received = true;
  }

  // Called to free memory in case the client hasn't called UpdateResponseRate
  // in a while.
  MaybeForgetOldRequests(received_time);
}

void PacketLossEstimator::UpdateResponseRate(int64_t now) {
  int responses_expected = 0;
  int responses_received = 0;

  for (auto iter = tracked_packets_.begin(); iter != tracked_packets_.end();) {
    const auto& packet_info = iter->second;
    if (Forget(packet_info, now)) {
      iter = tracked_packets_.erase(iter);
      continue;
    }
    if (packet_info.response_received) {
      responses_expected += 1;
      responses_received += 1;
    } else if (ConsiderLost(packet_info, now)) {
      responses_expected += 1;
    }
    ++iter;
  }

  if (responses_expected > 0) {
    response_rate_ =
        static_cast<double>(responses_received) / responses_expected;
  } else {
    response_rate_ = 1.0;
  }

  last_forgot_at_ = now;
}

void PacketLossEstimator::MaybeForgetOldRequests(int64_t now) {
  if (now - last_forgot_at_ <= forget_after_ms_) {
    return;
  }

  for (auto iter = tracked_packets_.begin(); iter != tracked_packets_.end();) {
    const auto& packet_info = iter->second;
    if (Forget(packet_info, now)) {
      iter = tracked_packets_.erase(iter);
    } else {
      ++iter;
    }
  }

  last_forgot_at_ = now;
}

bool PacketLossEstimator::ConsiderLost(const PacketInfo& packet_info,
                                       int64_t now) const {
  return packet_info.sent_time < now - consider_lost_after_ms_;
}

bool PacketLossEstimator::Forget(const PacketInfo& packet_info,
                                 int64_t now) const {
  return now - packet_info.sent_time > forget_after_ms_;
}

std::size_t PacketLossEstimator::tracked_packet_count_for_testing() const {
  return tracked_packets_.size();
}

std::string PacketLossEstimator::TrackedPacketsStringForTesting(
    std::size_t max) const {
  std::ostringstream oss;

  size_t count = 0;
  for (const auto& p : tracked_packets_) {
    const auto& id = p.first;
    const auto& packet_info = p.second;
    oss << "{ " << id << ", " << packet_info.sent_time << "}, ";
    count += 1;
    if (count == max) {
      oss << "...";
      break;
    }
  }

  return oss.str();
}

}  // namespace cricket
