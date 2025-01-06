/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/render_delay_controller_metrics.h"

#include <optional>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "system_wrappers/include/metrics.h"
#include "test/gtest.h"

namespace webrtc {

// Verify the general functionality of RenderDelayControllerMetrics.
TEST(RenderDelayControllerMetrics, NormalUsage) {
  metrics::Reset();

  RenderDelayControllerMetrics metrics;

  int expected_num_metric_reports = 0;

  for (int j = 0; j < 3; ++j) {
    for (int k = 0; k < kMetricsReportingIntervalBlocks - 1; ++k) {
      metrics.Update(std::nullopt, std::nullopt,
                     ClockdriftDetector::Level::kNone);
    }
    EXPECT_METRIC_EQ(
        metrics::NumSamples("WebRTC.Audio.EchoCanceller.EchoPathDelay"),
        expected_num_metric_reports);
    EXPECT_METRIC_EQ(
        metrics::NumSamples("WebRTC.Audio.EchoCanceller.BufferDelay"),
        expected_num_metric_reports);
    EXPECT_METRIC_EQ(metrics::NumSamples(
                         "WebRTC.Audio.EchoCanceller.ReliableDelayEstimates"),
                     expected_num_metric_reports);
    EXPECT_METRIC_EQ(
        metrics::NumSamples("WebRTC.Audio.EchoCanceller.DelayChanges"),
        expected_num_metric_reports);
    EXPECT_METRIC_EQ(
        metrics::NumSamples("WebRTC.Audio.EchoCanceller.Clockdrift"),
        expected_num_metric_reports);

    // We expect metric reports every kMetricsReportingIntervalBlocks blocks.
    ++expected_num_metric_reports;

    metrics.Update(std::nullopt, std::nullopt,
                   ClockdriftDetector::Level::kNone);
    EXPECT_METRIC_EQ(
        metrics::NumSamples("WebRTC.Audio.EchoCanceller.EchoPathDelay"),
        expected_num_metric_reports);
    EXPECT_METRIC_EQ(
        metrics::NumSamples("WebRTC.Audio.EchoCanceller.BufferDelay"),
        expected_num_metric_reports);
    EXPECT_METRIC_EQ(metrics::NumSamples(
                         "WebRTC.Audio.EchoCanceller.ReliableDelayEstimates"),
                     expected_num_metric_reports);
    EXPECT_METRIC_EQ(
        metrics::NumSamples("WebRTC.Audio.EchoCanceller.DelayChanges"),
        expected_num_metric_reports);
    EXPECT_METRIC_EQ(
        metrics::NumSamples("WebRTC.Audio.EchoCanceller.Clockdrift"),
        expected_num_metric_reports);
  }
}

}  // namespace webrtc
