/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/stats_poller.h"

#include "api/stats/rtc_stats_collector_callback.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::testing::Eq;

class TestStatsProvider : public StatsProvider {
 public:
  ~TestStatsProvider() override = default;

  void GetStats(RTCStatsCollectorCallback* callback) override {
    stats_collections_count_++;
  }

  int stats_collections_count() const { return stats_collections_count_; }

 private:
  int stats_collections_count_ = 0;
};

class MockStatsObserver : public StatsObserverInterface {
 public:
  ~MockStatsObserver() override = default;

  MOCK_METHOD(void,
              OnStatsReports,
              (absl::string_view pc_label,
               const rtc::scoped_refptr<const RTCStatsReport>& report));
};

TEST(StatsPollerTest, UnregisterParticipantAddedInCtor) {
  TestStatsProvider alice;
  TestStatsProvider bob;

  MockStatsObserver stats_observer;

  StatsPoller poller(/*observers=*/{&stats_observer},
                     /*peers_to_observe=*/{{"alice", &alice}, {"bob", &bob}});
  poller.PollStatsAndNotifyObservers();

  EXPECT_THAT(alice.stats_collections_count(), Eq(1));
  EXPECT_THAT(bob.stats_collections_count(), Eq(1));

  poller.UnregisterParticipantInCall("bob");
  poller.PollStatsAndNotifyObservers();

  EXPECT_THAT(alice.stats_collections_count(), Eq(2));
  EXPECT_THAT(bob.stats_collections_count(), Eq(1));
}

TEST(StatsPollerTest, UnregisterParticipantRegisteredInCall) {
  TestStatsProvider alice;
  TestStatsProvider bob;

  MockStatsObserver stats_observer;

  StatsPoller poller(/*observers=*/{&stats_observer},
                     /*peers_to_observe=*/{{"alice", &alice}});
  poller.RegisterParticipantInCall("bob", &bob);
  poller.PollStatsAndNotifyObservers();

  EXPECT_THAT(alice.stats_collections_count(), Eq(1));
  EXPECT_THAT(bob.stats_collections_count(), Eq(1));

  poller.UnregisterParticipantInCall("bob");
  poller.PollStatsAndNotifyObservers();

  EXPECT_THAT(alice.stats_collections_count(), Eq(2));
  EXPECT_THAT(bob.stats_collections_count(), Eq(1));
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
