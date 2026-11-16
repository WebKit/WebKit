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

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/stats_observer_interface.h"
#include "api/units/time_delta.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/pc/e2e/stats_provider.h"
#include "test/pc/e2e/test_peer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

void InternalStatsObserver::PollStats() {
  {
    MutexLock lock(&mutex_);
    ++pending_requests_;
  }
  if (stats_delay_.IsZero()) {
    peer_->GetStats(this);
  } else {
    // Artificial delay for testing race conditions.
    TaskQueueBase::Current()->PostDelayedTask(
        [this, captured_observer = scoped_refptr<InternalStatsObserver>(
                   this)]() { peer_->GetStats(captured_observer.get()); },
        stats_delay_);
  }
}

void InternalStatsObserver::OnStatsDelivered(
    const scoped_refptr<const RTCStatsReport>& report) {
  for (auto* observer : observers_) {
    observer->OnStatsReports(pc_label_, report);
  }
  {
    MutexLock lock(&mutex_);
    --pending_requests_;
  }
}

bool InternalStatsObserver::IsPolling() const {
  MutexLock lock(&mutex_);
  return pending_requests_ > 0;
}

StatsPoller::StatsPoller(std::vector<StatsObserverInterface*> observers,
                         std::map<std::string, StatsProvider*> peers,
                         TimeDelta stats_delay)
    : observers_(std::move(observers)) {
  MutexLock lock(&mutex_);
  for (auto& peer : peers) {
    pollers_.push_back(make_ref_counted<InternalStatsObserver>(
        peer.first, peer.second, observers_, stats_delay));
  }
}

StatsPoller::StatsPoller(std::vector<StatsObserverInterface*> observers,
                         std::map<std::string, TestPeer*> peers,
                         TimeDelta stats_delay)
    : observers_(std::move(observers)) {
  MutexLock lock(&mutex_);
  for (auto& peer : peers) {
    pollers_.push_back(make_ref_counted<InternalStatsObserver>(
        peer.first, peer.second, observers_, stats_delay));
  }
}

void StatsPoller::PollStatsAndNotifyObservers() {
  MutexLock lock(&mutex_);
  for (auto& poller : pollers_) {
    poller->PollStats();
  }
}

void StatsPoller::RegisterParticipantInCall(absl::string_view peer_name,
                                            StatsProvider* peer) {
  MutexLock lock(&mutex_);
  pollers_.push_back(
      make_ref_counted<InternalStatsObserver>(peer_name, peer, observers_));
}

bool StatsPoller::UnregisterParticipantInCall(absl::string_view peer_name) {
  MutexLock lock(&mutex_);
  for (auto it = pollers_.begin(); it != pollers_.end(); ++it) {
    if ((*it)->pc_label() == peer_name) {
      pollers_.erase(it);
      return true;
    }
  }
  return false;
}

bool StatsPoller::IsPolling() const {
  MutexLock lock(&mutex_);
  for (const auto& poller : pollers_) {
    if (poller->IsPolling()) {
      return true;
    }
  }
  return false;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
