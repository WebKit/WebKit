/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_receiver.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"


namespace webrtc {
namespace testing {
namespace bwe {

// This test fixture is used to instantiate tests running with adaptive video
// senders.
class BweSimulation : public BweTest,
                      public ::testing::TestWithParam<BandwidthEstimatorType> {
 public:
  BweSimulation()
      : BweTest(), random_(Clock::GetRealTimeClock()->TimeInMicroseconds()) {}
  virtual ~BweSimulation() {}

 protected:
  void SetUp() override {
    BweTest::SetUp();
    VerboseLogging(true);
  }

  Random random_;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(BweSimulation);
};

INSTANTIATE_TEST_CASE_P(VideoSendersTest,
                        BweSimulation,
                        ::testing::Values(kRembEstimator,
                                          kSendSideEstimator,
                                          kNadaEstimator));

TEST_P(BweSimulation, SprintUplinkTest) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output",
                             bwe_names[GetParam()]);
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  RateCounterFilter counter2(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("sprint-uplink", "rx")));
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Verizon4gDownlinkTest) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  VideoSender sender(&downlink_, &source, GetParam());
  RateCounterFilter counter1(&downlink_, 0, "sender_output",
                             bwe_names[GetParam()] + "_up");
  TraceBasedDeliveryFilter filter(&downlink_, 0, "link_capacity");
  RateCounterFilter counter2(&downlink_, 0, "Receiver",
                             bwe_names[GetParam()] + "_down");
  PacketReceiver receiver(&downlink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("verizon4g-downlink", "rx")));
  RunFor(22 * 60 * 1000);
}

TEST_P(BweSimulation, Choke1000kbps500kbps1000kbpsBiDirectional) {
  const int kFlowIds[] = {0, 1};
  const size_t kNumFlows = sizeof(kFlowIds) / sizeof(kFlowIds[0]);

  AdaptiveVideoSource source(kFlowIds[0], 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter choke(&uplink_, kFlowIds[0]);
  RateCounterFilter counter(&uplink_, kFlowIds[0], "Receiver_0",
                            bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, kFlowIds[0], GetParam(), true, false);

  AdaptiveVideoSource source2(kFlowIds[1], 30, 300, 0, 0);
  VideoSender sender2(&downlink_, &source2, GetParam());
  ChokeFilter choke2(&downlink_, kFlowIds[1]);
  DelayFilter delay(&downlink_, CreateFlowIds(kFlowIds, kNumFlows));
  RateCounterFilter counter2(&downlink_, kFlowIds[1], "Receiver_1",
                             bwe_names[GetParam()]);
  PacketReceiver receiver2(&downlink_, kFlowIds[1], GetParam(), true, false);

  choke2.set_capacity_kbps(500);
  delay.SetOneWayDelayMs(0);

  choke.set_capacity_kbps(1000);
  choke.set_max_delay_ms(500);
  RunFor(60 * 1000);
  choke.set_capacity_kbps(500);
  RunFor(60 * 1000);
  choke.set_capacity_kbps(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Choke1000kbps500kbps1000kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter choke(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, false);

  choke.set_capacity_kbps(1000);
  choke.set_max_delay_ms(500);
  RunFor(60 * 1000);
  choke.set_capacity_kbps(500);
  RunFor(60 * 1000);
  choke.set_capacity_kbps(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke1000kbps500kbps1000kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.set_capacity_kbps(1000);
  filter.set_max_delay_ms(500);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(500);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(1000);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke10000kbps) {
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.set_capacity_kbps(10000);
  filter.set_max_delay_ms(500);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke200kbps30kbps200kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.set_capacity_kbps(200);
  filter.set_max_delay_ms(500);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(30);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(200);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Choke200kbps30kbps200kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.set_capacity_kbps(200);
  filter.set_max_delay_ms(500);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(30);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(200);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, PacerChoke50kbps15kbps50kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.set_capacity_kbps(50);
  filter.set_max_delay_ms(500);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(15);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(50);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, Choke50kbps15kbps50kbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.set_capacity_kbps(50);
  filter.set_max_delay_ms(500);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(15);
  RunFor(60 * 1000);
  filter.set_capacity_kbps(50);
  RunFor(60 * 1000);
}

TEST_P(BweSimulation, GoogleWifiTrace3Mbps) {
  AdaptiveVideoSource source(0, 30, 300, 0, 0);
  VideoSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output",
                             bwe_names[GetParam()]);
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  filter.set_max_delay_ms(500);
  RateCounterFilter counter2(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

TEST_P(BweSimulation, LinearIncreasingCapacity) {
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000000);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.set_max_delay_ms(500);
  const int kStartingCapacityKbps = 150;
  const int kEndingCapacityKbps = 1500;
  const int kStepKbps = 5;
  const int kStepTimeMs = 1000;

  for (int i = kStartingCapacityKbps; i <= kEndingCapacityKbps;
       i += kStepKbps) {
    filter.set_capacity_kbps(i);
    RunFor(kStepTimeMs);
  }
}

TEST_P(BweSimulation, LinearDecreasingCapacity) {
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000000);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  ChokeFilter filter(&uplink_, 0);
  RateCounterFilter counter(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  filter.set_max_delay_ms(500);
  const int kStartingCapacityKbps = 1500;
  const int kEndingCapacityKbps = 150;
  const int kStepKbps = -5;
  const int kStepTimeMs = 1000;

  for (int i = kStartingCapacityKbps; i >= kEndingCapacityKbps;
       i += kStepKbps) {
    filter.set_capacity_kbps(i);
    RunFor(kStepTimeMs);
  }
}

TEST_P(BweSimulation, PacerGoogleWifiTrace3Mbps) {
  PeriodicKeyFrameSource source(0, 30, 300, 0, 0, 1000);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output",
                             bwe_names[GetParam()]);
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  filter.set_max_delay_ms(500);
  RateCounterFilter counter2(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

TEST_P(BweSimulation, PacerGoogleWifiTrace3MbpsLowFramerate) {
  PeriodicKeyFrameSource source(0, 5, 300, 0, 0, 1000);
  PacedVideoSender sender(&uplink_, &source, GetParam());
  RateCounterFilter counter1(&uplink_, 0, "sender_output",
                             bwe_names[GetParam()]);
  TraceBasedDeliveryFilter filter(&uplink_, 0, "link_capacity");
  filter.set_max_delay_ms(500);
  RateCounterFilter counter2(&uplink_, 0, "Receiver", bwe_names[GetParam()]);
  PacketReceiver receiver(&uplink_, 0, GetParam(), true, true);
  ASSERT_TRUE(filter.Init(test::ResourcePath("google-wifi-3mbps", "rx")));
  RunFor(300 * 1000);
}

TEST_P(BweSimulation, SelfFairnessTest) {
  Random prng(Clock::GetRealTimeClock()->TimeInMicroseconds());
  const int kAllFlowIds[] = {0, 1, 2, 3};
  const size_t kNumFlows = sizeof(kAllFlowIds) / sizeof(kAllFlowIds[0]);
  std::unique_ptr<VideoSource> sources[kNumFlows];
  std::unique_ptr<VideoSender> senders[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    // Streams started 20 seconds apart to give them different advantage when
    // competing for the bandwidth.
    sources[i].reset(new AdaptiveVideoSource(kAllFlowIds[i], 30, 300, 0,
                                             i * prng.Rand(39999)));
    senders[i].reset(new VideoSender(&uplink_, sources[i].get(), GetParam()));
  }

  ChokeFilter choke(&uplink_, CreateFlowIds(kAllFlowIds, kNumFlows));
  choke.set_capacity_kbps(1000);

  std::unique_ptr<RateCounterFilter> rate_counters[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    rate_counters[i].reset(
        new RateCounterFilter(&uplink_, CreateFlowIds(&kAllFlowIds[i], 1),
                              "Receiver", bwe_names[GetParam()]));
  }

  RateCounterFilter total_utilization(
      &uplink_, CreateFlowIds(kAllFlowIds, kNumFlows), "total_utilization",
      "Total_link_utilization");

  std::unique_ptr<PacketReceiver> receivers[kNumFlows];
  for (size_t i = 0; i < kNumFlows; ++i) {
    receivers[i].reset(new PacketReceiver(&uplink_, kAllFlowIds[i], GetParam(),
                                          i == 0, false));
  }

  RunFor(30 * 60 * 1000);
}

TEST_P(BweSimulation, PacedSelfFairness50msTest) {
  const int64_t kAverageOffsetMs = 20 * 1000;
  const int kNumRmcatFlows = 4;
  int64_t offsets_ms[kNumRmcatFlows];
  offsets_ms[0] = random_.Rand(2 * kAverageOffsetMs);
  for (int i = 1; i < kNumRmcatFlows; ++i) {
    offsets_ms[i] = offsets_ms[i - 1] + random_.Rand(2 * kAverageOffsetMs);
  }
  RunFairnessTest(GetParam(), kNumRmcatFlows, 0, 1000, 3000, 50, 50, 0,
                  offsets_ms);
}

TEST_P(BweSimulation, PacedSelfFairness500msTest) {
  const int64_t kAverageOffsetMs = 20 * 1000;
  const int kNumRmcatFlows = 4;
  int64_t offsets_ms[kNumRmcatFlows];
  offsets_ms[0] = random_.Rand(2 * kAverageOffsetMs);
  for (int i = 1; i < kNumRmcatFlows; ++i) {
    offsets_ms[i] = offsets_ms[i - 1] + random_.Rand(2 * kAverageOffsetMs);
  }
  RunFairnessTest(GetParam(), kNumRmcatFlows, 0, 1000, 3000, 500, 50, 0,
                  offsets_ms);
}

TEST_P(BweSimulation, PacedSelfFairness1000msTest) {
  const int64_t kAverageOffsetMs = 20 * 1000;
  const int kNumRmcatFlows = 4;
  int64_t offsets_ms[kNumRmcatFlows];
  offsets_ms[0] = random_.Rand(2 * kAverageOffsetMs);
  for (int i = 1; i < kNumRmcatFlows; ++i) {
    offsets_ms[i] = offsets_ms[i - 1] + random_.Rand(2 * kAverageOffsetMs);
  }
  RunFairnessTest(GetParam(), 4, 0, 1000, 3000, 1000, 50, 0, offsets_ms);
}

TEST_P(BweSimulation, TcpFairness50msTest) {
  const int64_t kAverageOffsetMs = 20 * 1000;
  int64_t offset_ms[] = {random_.Rand(2 * kAverageOffsetMs), 0};
  RunFairnessTest(GetParam(), 1, 1, 1000, 2000, 50, 50, 0, offset_ms);
}

TEST_P(BweSimulation, TcpFairness500msTest) {
  const int64_t kAverageOffsetMs = 20 * 1000;
  int64_t offset_ms[] = {random_.Rand(2 * kAverageOffsetMs), 0};
  RunFairnessTest(GetParam(), 1, 1, 1000, 2000, 500, 50, 0, offset_ms);
}

TEST_P(BweSimulation, TcpFairness1000msTest) {
  const int kAverageOffsetMs = 20 * 1000;
  int64_t offset_ms[] = {random_.Rand(2 * kAverageOffsetMs), 0};
  RunFairnessTest(GetParam(), 1, 1, 1000, 2000, 1000, 50, 0, offset_ms);
}

// The following test cases begin with "Evaluation" as a reference to the
// Internet draft https://tools.ietf.org/html/draft-ietf-rmcat-eval-test-01.

TEST_P(BweSimulation, Evaluation1) {
  RunVariableCapacity1SingleFlow(GetParam());
}

TEST_P(BweSimulation, Evaluation2) {
  const size_t kNumFlows = 2;
  RunVariableCapacity2MultipleFlows(GetParam(), kNumFlows);
}

TEST_P(BweSimulation, Evaluation3) {
  RunBidirectionalFlow(GetParam());
}

TEST_P(BweSimulation, Evaluation4) {
  RunSelfFairness(GetParam());
}

TEST_P(BweSimulation, Evaluation5) {
  RunRoundTripTimeFairness(GetParam());
}

TEST_P(BweSimulation, Evaluation6) {
  RunLongTcpFairness(GetParam());
}

// Different calls to the Evaluation7 will create the same FileSizes
// and StartingTimes as long as the seeds remain unchanged. This is essential
// when calling it with multiple estimators for comparison purposes.
TEST_P(BweSimulation, Evaluation7) {
  const int kNumTcpFiles = 10;
  RunMultipleShortTcpFairness(GetParam(),
                              BweTest::GetFileSizesBytes(kNumTcpFiles),
                              BweTest::GetStartingTimesMs(kNumTcpFiles));
}

TEST_P(BweSimulation, Evaluation8) {
  RunPauseResumeFlows(GetParam());
}

// Following test cases begin with "GccComparison" run the
// evaluation test cases for both GCC and other calling RMCAT.

TEST_P(BweSimulation, GccComparison1) {
  RunVariableCapacity1SingleFlow(GetParam());
  BweTest gcc_test(false);
  gcc_test.RunVariableCapacity1SingleFlow(kSendSideEstimator);
}

TEST_P(BweSimulation, GccComparison2) {
  const size_t kNumFlows = 2;
  RunVariableCapacity2MultipleFlows(GetParam(), kNumFlows);
  BweTest gcc_test(false);
  gcc_test.RunVariableCapacity2MultipleFlows(kSendSideEstimator, kNumFlows);
}

TEST_P(BweSimulation, GccComparison3) {
  RunBidirectionalFlow(GetParam());
  BweTest gcc_test(false);
  gcc_test.RunBidirectionalFlow(kSendSideEstimator);
}

TEST_P(BweSimulation, GccComparison4) {
  RunSelfFairness(GetParam());
  BweTest gcc_test(false);
  gcc_test.RunSelfFairness(GetParam());
}

TEST_P(BweSimulation, GccComparison5) {
  RunRoundTripTimeFairness(GetParam());
  BweTest gcc_test(false);
  gcc_test.RunRoundTripTimeFairness(kSendSideEstimator);
}

TEST_P(BweSimulation, GccComparison6) {
  RunLongTcpFairness(GetParam());
  BweTest gcc_test(false);
  gcc_test.RunLongTcpFairness(kSendSideEstimator);
}

TEST_P(BweSimulation, GccComparison7) {
  const int kNumTcpFiles = 10;

  std::vector<int> tcp_file_sizes_bytes =
      BweTest::GetFileSizesBytes(kNumTcpFiles);
  std::vector<int64_t> tcp_starting_times_ms =
      BweTest::GetStartingTimesMs(kNumTcpFiles);

  RunMultipleShortTcpFairness(GetParam(), tcp_file_sizes_bytes,
                              tcp_starting_times_ms);

  BweTest gcc_test(false);
  gcc_test.RunMultipleShortTcpFairness(kSendSideEstimator, tcp_file_sizes_bytes,
                                       tcp_starting_times_ms);
}

TEST_P(BweSimulation, GccComparison8) {
  RunPauseResumeFlows(GetParam());
  BweTest gcc_test(false);
  gcc_test.RunPauseResumeFlows(kSendSideEstimator);
}

TEST_P(BweSimulation, GccComparisonChoke) {
  int array[] = {1000, 500, 1000};
  std::vector<int> capacities_kbps(array, array + 3);
  RunChoke(GetParam(), capacities_kbps);

  BweTest gcc_test(false);
  gcc_test.RunChoke(kSendSideEstimator, capacities_kbps);
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

