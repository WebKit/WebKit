/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/metric_recorder.h"

#include <math.h>
#include <algorithm>
#include <vector>

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace testing {
namespace bwe {

class MetricRecorderTest : public ::testing::Test {
 public:
  MetricRecorderTest() : metric_recorder_("Test", 0, nullptr, nullptr) {}

  ~MetricRecorderTest() {}

 protected:
  MetricRecorder metric_recorder_;
};

TEST_F(MetricRecorderTest, NoPackets) {
  EXPECT_EQ(metric_recorder_.AverageBitrateKbps(0), 0);
  EXPECT_EQ(metric_recorder_.DelayStdDev(), 0.0);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(0), 0);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(5), 0);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(95), 0);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(100), 0);
}

TEST_F(MetricRecorderTest, RegularPackets) {
  const size_t kPayloadSizeBytes = 1200;
  const int64_t kDelayMs = 20;
  const int64_t kInterpacketGapMs = 5;
  const int kNumPackets = 1000;

  for (int i = 0; i < kNumPackets; ++i) {
    int64_t arrival_time_ms = kInterpacketGapMs * i + kDelayMs;
    metric_recorder_.UpdateTimeMs(arrival_time_ms);
    metric_recorder_.PushDelayMs(kDelayMs, arrival_time_ms);
    metric_recorder_.PushThroughputBytes(kPayloadSizeBytes, arrival_time_ms);
  }

  EXPECT_NEAR(
      metric_recorder_.AverageBitrateKbps(0),
      static_cast<uint32_t>(kPayloadSizeBytes * 8) / (kInterpacketGapMs), 10);

  EXPECT_EQ(metric_recorder_.DelayStdDev(), 0.0);

  EXPECT_EQ(metric_recorder_.NthDelayPercentile(0), kDelayMs);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(5), kDelayMs);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(95), kDelayMs);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(100), kDelayMs);
}

TEST_F(MetricRecorderTest, VariableDelayPackets) {
  const size_t kPayloadSizeBytes = 1200;
  const int64_t kInterpacketGapMs = 2000;
  const int kNumPackets = 1000;

  std::vector<int64_t> delays_ms;
  for (int i = 0; i < kNumPackets; ++i) {
    delays_ms.push_back(static_cast<int64_t>(i + 1));
  }
  // Order of packets should not matter here.
  std::random_shuffle(delays_ms.begin(), delays_ms.end());

  int first_received_ms = delays_ms[0];
  int64_t last_received_ms = 0;
  for (int i = 0; i < kNumPackets; ++i) {
    int64_t arrival_time_ms = kInterpacketGapMs * i + delays_ms[i];
    last_received_ms = std::max(last_received_ms, arrival_time_ms);
    metric_recorder_.UpdateTimeMs(arrival_time_ms);
    metric_recorder_.PushDelayMs(delays_ms[i], arrival_time_ms);
    metric_recorder_.PushThroughputBytes(kPayloadSizeBytes, arrival_time_ms);
  }

  size_t received_bits = kPayloadSizeBytes * 8 * kNumPackets;
  EXPECT_NEAR(metric_recorder_.AverageBitrateKbps(0),
              static_cast<uint32_t>(received_bits) /
                  ((last_received_ms - first_received_ms)),
              10);

  double expected_x = (kNumPackets + 1) / 2.0;
  double expected_x2 = ((kNumPackets + 1) * (2 * kNumPackets + 1)) / 6.0;
  double var = expected_x2 - pow(expected_x, 2.0);
  EXPECT_NEAR(metric_recorder_.DelayStdDev(), sqrt(var), kNumPackets / 1000.0);

  EXPECT_EQ(metric_recorder_.NthDelayPercentile(0), 1);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(5), (5 * kNumPackets) / 100);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(95), (95 * kNumPackets) / 100);
  EXPECT_EQ(metric_recorder_.NthDelayPercentile(100), kNumPackets);
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
