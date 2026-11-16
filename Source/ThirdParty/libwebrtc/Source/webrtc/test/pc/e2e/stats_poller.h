/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_STATS_POLLER_H_
#define TEST_PC_E2E_STATS_POLLER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/stats/rtc_stats_report.h"
#include "api/test/stats_observer_interface.h"
#include "api/units/time_delta.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "test/pc/e2e/stats_provider.h"
#include "test/pc/e2e/test_peer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Helper class that will notify all the test::StatsObserverInterface
// objects subscribed.
class InternalStatsObserver : public RTCStatsCollectorCallback {
 public:
  InternalStatsObserver(absl::string_view pc_label,
                        StatsProvider* peer,
                        std::vector<StatsObserverInterface*> observers,
                        TimeDelta stats_delay = TimeDelta::Zero())
      : pc_label_(pc_label),
        peer_(peer),
        observers_(std::move(observers)),
        stats_delay_(stats_delay) {}

  std::string pc_label() const { return pc_label_; }

  void PollStats();

  void OnStatsDelivered(
      const scoped_refptr<const RTCStatsReport>& report) override;

  bool IsPolling() const;

 private:
  std::string pc_label_;
  StatsProvider* const peer_;
  std::vector<StatsObserverInterface*> observers_;
  TimeDelta stats_delay_;
  mutable Mutex mutex_;
  int pending_requests_ RTC_GUARDED_BY(mutex_) = 0;
};

// Helper class to invoke GetStats on a PeerConnection by passing a
// StatsObserver that will notify all the test::StatsObserverInterface
// subscribed.
class StatsPoller {
 public:
  StatsPoller(std::vector<StatsObserverInterface*> observers,
              std::map<std::string, StatsProvider*> peers_to_observe,
              TimeDelta stats_delay = TimeDelta::Zero());
  StatsPoller(std::vector<StatsObserverInterface*> observers,
              std::map<std::string, TestPeer*> peers_to_observe,
              TimeDelta stats_delay = TimeDelta::Zero());

  void PollStatsAndNotifyObservers();

  bool IsPolling() const;

  void RegisterParticipantInCall(absl::string_view peer_name,
                                 StatsProvider* peer);
  // Unregister participant from stats poller. Returns true if participant was
  // removed and false if participant wasn't found.
  bool UnregisterParticipantInCall(absl::string_view peer_name);

 private:
  const std::vector<StatsObserverInterface*> observers_;
  mutable Mutex mutex_;
  std::vector<scoped_refptr<InternalStatsObserver>> pollers_
      RTC_GUARDED_BY(mutex_);
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_STATS_POLLER_H_
