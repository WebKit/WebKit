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

namespace webrtc {
namespace webrtc_pc_e2e {

void InternalStatsObserver::PollStats() {
  peer_->pc()->GetStats(this, nullptr,
                        webrtc::PeerConnectionInterface::StatsOutputLevel::
                            kStatsOutputLevelStandard);
}

void InternalStatsObserver::OnComplete(const StatsReports& reports) {
  for (auto* observer : observers_) {
    observer->OnStatsReports(pc_label_, reports);
  }
}

StatsPoller::StatsPoller(std::vector<StatsObserverInterface*> observers,
                         std::map<std::string, TestPeer*> peers) {
  for (auto& peer : peers) {
    pollers_.push_back(new rtc::RefCountedObject<InternalStatsObserver>(
        peer.first, peer.second, observers));
  }
}

void StatsPoller::PollStatsAndNotifyObservers() {
  for (auto& poller : pollers_) {
    poller->PollStats();
  }
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
