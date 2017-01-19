/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_WIN
#include <sys/types.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <sstream>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_receiver.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::string;

namespace webrtc {
namespace testing {
namespace bwe {

// This test fixture is used to instantiate tests running with adaptive video
// senders.
class BweFeedbackTest
    : public BweTest,
      public ::testing::TestWithParam<BandwidthEstimatorType> {
 public:
#ifdef WEBRTC_WIN
  BweFeedbackTest()
      : BweTest(), random_(Clock::GetRealTimeClock()->TimeInMicroseconds()) {}
#else
  BweFeedbackTest()
      : BweTest(),
        // Multiply the time by a random-ish odd number derived from the PID.
        random_((getpid() | 1) *
                Clock::GetRealTimeClock()->TimeInMicroseconds()) {}
#endif
  virtual ~BweFeedbackTest() {}

 protected:
  Random random_;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(BweFeedbackTest);
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest,
                        BweFeedbackTest,
                        ::testing::Values(kRembEstimator, kSendSideEstimator));

TEST_P(BweFeedbackTest, ConstantCapacity) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  const int kCapacityKbps = 1000;
  filter.set_capacity_kbps(kCapacityKbps);
  filter.set_max_delay_ms(500);
  RunFor(180 * 1000);
  PrintResults(kCapacityKbps, counter.GetBitrateStats(), 0,
               receiver.GetDelayStats(), counter.GetBitrateStats());
}

TEST_P(BweFeedbackTest, Choke1000kbps500kbps1000kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  const int kHighCapacityKbps = 1000;
  const int kLowCapacityKbps = 500;
  filter.set_capacity_kbps(kHighCapacityKbps);
  filter.set_max_delay_ms(500);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(kLowCapacityKbps);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(kHighCapacityKbps);
  RunFor(60 * 1000);
  PrintResults((2 * kHighCapacityKbps + kLowCapacityKbps) / 3.0,
               counter.GetBitrateStats(), 0, receiver.GetDelayStats(),
               counter.GetBitrateStats());
}

TEST_P(BweFeedbackTest, Choke200kbps30kbps200kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  const int kHighCapacityKbps = 200;
  const int kLowCapacityKbps = 30;
  filter.set_capacity_kbps(kHighCapacityKbps);
  filter.set_max_delay_ms(500);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(kLowCapacityKbps);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(kHighCapacityKbps);
  RunFor(60 * 1000);

  PrintResults((2 * kHighCapacityKbps + kLowCapacityKbps) / 3.0,
               counter.GetBitrateStats(), 0, receiver.GetDelayStats(),
               counter.GetBitrateStats());
}

TEST_P(BweFeedbackTest, Verizon4gDownlinkTest) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output",
                             bwe_names[GetParam()]);
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  RateCounterFilter counter2(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  ASSERT_TRUE(filter.Init(test::ResourcePath("verizon4g-downlink", "rx")));
  RunFor(22 * 60 * 1000);
  PrintResults(filter.GetBitrateStats().GetMean(), counter2.GetBitrateStats(),
               0, receiver.GetDelayStats(), counter2.GetBitrateStats());
}

// webrtc:3277
TEST_P(BweFeedbackTest, GoogleWifiTrace3Mbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output",
                             bwe_names[GetParam()]);
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  filter.set_max_delay_ms(500);
  RateCounterFilter counter2(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), false, false);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
  PrintResults(filter.GetBitrateStats().GetMean(), counter2.GetBitrateStats(),
               0, receiver.GetDelayStats(), counter2.GetBitrateStats());
}

TEST_P(BweFeedbackTest, PacedSelfFairness50msTest) {
  int64_t kRttMs = 100;
  int64_t kMaxJitterMs = 15;

  const int kNumRmcatFlows = 4;
  int64_t offset_ms[kNumRmcatFlows];
  for (int i = 0; i < kNumRmcatFlows; ++i) {
    offset_ms[i] = std::max(0, 5000 * i + random_.Rand(-1000, 1000));
  }

  RunFairnessTest(GetParam(), kNumRmcatFlows, 0, 300, 3000, 50, kRttMs,
                  kMaxJitterMs, offset_ms);
}

TEST_P(BweFeedbackTest, PacedSelfFairness500msTest) {
  int64_t kRttMs = 100;
  int64_t kMaxJitterMs = 15;

  const int kNumRmcatFlows = 4;
  int64_t offset_ms[kNumRmcatFlows];
  for (int i = 0; i < kNumRmcatFlows; ++i) {
    offset_ms[i] = std::max(0, 5000 * i + random_.Rand(-1000, 1000));
  }

  RunFairnessTest(GetParam(), kNumRmcatFlows, 0, 300, 3000, 500, kRttMs,
                  kMaxJitterMs, offset_ms);
}

TEST_P(BweFeedbackTest, PacedSelfFairness1000msTest) {
  int64_t kRttMs = 100;
  int64_t kMaxJitterMs = 15;

  const int kNumRmcatFlows = 4;
  int64_t offset_ms[kNumRmcatFlows];
  for (int i = 0; i < kNumRmcatFlows; ++i) {
    offset_ms[i] = std::max(0, 5000 * i + random_.Rand(-1000, 1000));
  }

  RunFairnessTest(GetParam(), kNumRmcatFlows, 0, 300, 3000, 1000, kRttMs,
                  kMaxJitterMs, offset_ms);
}

TEST_P(BweFeedbackTest, TcpFairness50msTest) {
  int64_t kRttMs = 100;
  int64_t kMaxJitterMs = 15;

  int64_t offset_ms[2];  // One TCP, one RMCAT flow.
  for (int i = 0; i < 2; ++i) {
    offset_ms[i] = std::max(0, 5000 * i + random_.Rand(-1000, 1000));
  }

  RunFairnessTest(GetParam(), 1, 1, 300, 2000, 50, kRttMs, kMaxJitterMs,
                  offset_ms);
}

TEST_P(BweFeedbackTest, TcpFairness500msTest) {
  int64_t kRttMs = 100;
  int64_t kMaxJitterMs = 15;

  int64_t offset_ms[2];  // One TCP, one RMCAT flow.
  for (int i = 0; i < 2; ++i) {
    offset_ms[i] = std::max(0, 5000 * i + random_.Rand(-1000, 1000));
  }

  RunFairnessTest(GetParam(), 1, 1, 300, 2000, 500, kRttMs, kMaxJitterMs,
                  offset_ms);
}

TEST_P(BweFeedbackTest, TcpFairness1000msTest) {
  int64_t kRttMs = 100;
  int64_t kMaxJitterMs = 15;

  int64_t offset_ms[2];  // One TCP, one RMCAT flow.
  for (int i = 0; i < 2; ++i) {
    offset_ms[i] = std::max(0, 5000 * i + random_.Rand(-1000, 1000));
  }

  RunFairnessTest(GetParam(), 1, 1, 300, 2000, 1000, kRttMs, kMaxJitterMs,
                  offset_ms);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
