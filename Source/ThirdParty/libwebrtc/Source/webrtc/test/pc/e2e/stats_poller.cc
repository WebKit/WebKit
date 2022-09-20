/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/stats_poller.h"

#include <utility>

#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void InternalStatsObserver::PollStats() {
  peer_->GetStats(this);
}

void InternalStatsObserver::OnStatsDelivered(
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  for (auto* observer : observers_) {
    observer->OnStatsReports(pc_label_, report);
  }
}

StatsPoller::StatsPoller(std::vector<StatsObserverInterface*> observers,
                         std::map<std::string, StatsProvider*> peers)
    : observers_(std::move(observers)) {
  webrtc::MutexLock lock(&mutex_);
  for (auto& peer : peers) {
    pollers_.push_back(rtc::make_ref_counted<InternalStatsObserver>(
        peer.first, peer.second, observers_));
  }
}

StatsPoller::StatsPoller(std::vector<StatsObserverInterface*> observers,
                         std::map<std::string, TestPeer*> peers)
    : observers_(std::move(observers)) {
  webrtc::MutexLock lock(&mutex_);
  for (auto& peer : peers) {
    pollers_.push_back(rtc::make_ref_counted<InternalStatsObserver>(
        peer.first, peer.second, observers_));
  }
}

void StatsPoller::PollStatsAndNotifyObservers() {
  webrtc::MutexLock lock(&mutex_);
  for (auto& poller : pollers_) {
    poller->PollStats();
  }
}

void StatsPoller::RegisterParticipantInCall(absl::string_view peer_name,
                                            StatsProvider* peer) {
  webrtc::MutexLock lock(&mutex_);
  pollers_.push_back(rtc::make_ref_counted<InternalStatsObserver>(
      peer_name, peer, observers_));
}

bool StatsPoller::UnregisterParticipantInCall(absl::string_view peer_name) {
  webrtc::MutexLock lock(&mutex_);
  for (auto it = pollers_.begin(); it != pollers_.end(); ++it) {
    if ((*it)->pc_label() == peer_name) {
      pollers_.erase(it);
      return true;
    }
  }
  return false;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
