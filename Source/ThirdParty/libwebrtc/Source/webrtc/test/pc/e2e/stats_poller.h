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

#include "api/peer_connection_interface.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/test/stats_observer_interface.h"
#include "test/pc/e2e/test_peer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Helper class that will notify all the webrtc::test::StatsObserverInterface
// objects subscribed.
class InternalStatsObserver : public RTCStatsCollectorCallback {
 public:
  InternalStatsObserver(std::string pc_label,
                        TestPeer* peer,
                        std::vector<StatsObserverInterface*> observers)
      : pc_label_(std::move(pc_label)),
        peer_(peer),
        observers_(std::move(observers)) {}

  void PollStats();

  void OnStatsDelivered(
      const rtc::scoped_refptr<const RTCStatsReport>& report) override;

 private:
  std::string pc_label_;
  TestPeer* peer_;
  std::vector<StatsObserverInterface*> observers_;
};

// Helper class to invoke GetStats on a PeerConnection by passing a
// webrtc::StatsObserver that will notify all the
// webrtc::test::StatsObserverInterface subscribed.
class StatsPoller {
 public:
  StatsPoller(std::vector<StatsObserverInterface*> observers,
              std::map<std::string, TestPeer*> peers_to_observe);

  void PollStatsAndNotifyObservers();

 private:
  std::vector<rtc::scoped_refptr<InternalStatsObserver>> pollers_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_STATS_POLLER_H_
