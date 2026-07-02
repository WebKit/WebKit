/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "api/numerics/samples_stats_counter.h"
#include "api/scoped_refptr.h"
#include "api/test/metrics/metric.h"
#include "api/test/metrics/metrics_logger.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_tools/video_file_reader.h"
#include "third_party/libyuv/include/libyuv/compare.h"

namespace webrtc {
namespace test {

ResultsContainer::ResultsContainer() {}
ResultsContainer::~ResultsContainer() {}

template <typename FrameMetricFunction>
static double CalculateMetric(
    const FrameMetricFunction& frame_metric_function,
    const scoped_refptr<I420BufferInterface>& ref_buffer,
    const scoped_refptr<I420BufferInterface>& test_buffer) {
  RTC_CHECK_EQ(ref_buffer->width(), test_buffer->width());
  RTC_CHECK_EQ(ref_buffer->height(), test_buffer->height());
  return frame_metric_function(
      ref_buffer->DataY(), ref_buffer->StrideY(), ref_buffer->DataU(),
      ref_buffer->StrideU(), ref_buffer->DataV(), ref_buffer->StrideV(),
      test_buffer->DataY(), test_buffer->StrideY(), test_buffer->DataU(),
      test_buffer->StrideU(), test_buffer->DataV(), test_buffer->StrideV(),
      test_buffer->width(), test_buffer->height());
}

double Psnr(const scoped_refptr<I420BufferInterface>& ref_buffer,
            const scoped_refptr<I420BufferInterface>& test_buffer) {
  // LibYuv sets the max psnr value to 128, we restrict it to 48.
  // In case of 0 mse in one frame, 128 can skew the results significantly.
  return std::min(48.0,
                  CalculateMetric(&libyuv::I420Psnr, ref_buffer, test_buffer));
}

double Ssim(const scoped_refptr<I420BufferInterface>& ref_buffer,
            const scoped_refptr<I420BufferInterface>& test_buffer) {
  return CalculateMetric(&libyuv::I420Ssim, ref_buffer, test_buffer);
}

std::vector<AnalysisResult> RunAnalysis(
    const scoped_refptr<test::Video>& reference_video,
    const scoped_refptr<test::Video>& test_video,
    const std::vector<size_t>& test_frame_indices) {
  std::vector<AnalysisResult> results;
  for (size_t i = 0; i < test_video->number_of_frames(); ++i) {
    const scoped_refptr<I420BufferInterface>& test_frame =
        test_video->GetFrame(i);
    const scoped_refptr<I420BufferInterface>& reference_frame =
        reference_video->GetFrame(i);

    // Fill in the result struct.
    AnalysisResult result;
    result.frame_number = test_frame_indices[i];
    result.psnr_value = Psnr(reference_frame, test_frame);
    result.ssim_value = Ssim(reference_frame, test_frame);
    results.push_back(result);
  }

  return results;
}

std::vector<Cluster> CalculateFrameClusters(
    const std::vector<size_t>& indices) {
  std::vector<Cluster> clusters;

  for (size_t index : indices) {
    if (!clusters.empty() && clusters.back().index == index) {
      // This frame belongs to the previous cluster.
      ++clusters.back().number_of_repeated_frames;
    } else {
      // Start a new cluster.
      clusters.push_back(
          {.index = index,
           /* number_of_repeated_frames= */ .number_of_repeated_frames = 1});
    }
  }

  return clusters;
}

int GetMaxRepeatedFrames(const std::vector<Cluster>& clusters) {
  int max_number_of_repeated_frames = 0;
  for (const Cluster& cluster : clusters) {
    max_number_of_repeated_frames = std::max(max_number_of_repeated_frames,
                                             cluster.number_of_repeated_frames);
  }
  return max_number_of_repeated_frames;
}

int GetMaxSkippedFrames(const std::vector<Cluster>& clusters) {
  size_t max_skipped_frames = 0;
  for (size_t i = 1; i < clusters.size(); ++i) {
    const size_t skipped_frames = clusters[i].index - clusters[i - 1].index - 1;
    max_skipped_frames = std::max(max_skipped_frames, skipped_frames);
  }
  return static_cast<int>(max_skipped_frames);
}

int GetTotalNumberOfSkippedFrames(const std::vector<Cluster>& clusters) {
  // The number of reference frames the test video spans.
  const size_t number_ref_frames =
      clusters.empty() ? 0 : 1 + clusters.back().index - clusters.front().index;
  return static_cast<int>(number_ref_frames - clusters.size());
}

void PrintAnalysisResults(const std::string& label,
                          ResultsContainer& results,
                          MetricsLogger& logger) {
  if (!results.frames.empty()) {
    logger.LogSingleValueMetric("Unique_frames_count", label,
                                results.frames.size(), Unit::kUnitless,
                                ImprovementDirection::kNeitherIsBetter);

    SamplesStatsCounter psnr_values;
    SamplesStatsCounter ssim_values;
    for (const auto& frame : results.frames) {
      psnr_values.AddSample(
          {.value = frame.psnr_value, .time = Timestamp::Zero()});
      ssim_values.AddSample(
          {.value = frame.ssim_value, .time = Timestamp::Zero()});
    }

    logger.LogMetric("PSNR_dB", label, psnr_values, Unit::kUnitless,
                     ImprovementDirection::kNeitherIsBetter);
    logger.LogMetric("SSIM", label, ssim_values, Unit::kUnitless,
                     ImprovementDirection::kNeitherIsBetter);
  }

  logger.LogSingleValueMetric("Max_repeated", label,
                              results.max_repeated_frames, Unit::kUnitless,
                              ImprovementDirection::kNeitherIsBetter);
  logger.LogSingleValueMetric("Max_skipped", label, results.max_skipped_frames,
                              Unit::kUnitless,
                              ImprovementDirection::kNeitherIsBetter);
  logger.LogSingleValueMetric("Total_skipped", label,
                              results.total_skipped_frames, Unit::kUnitless,
                              ImprovementDirection::kNeitherIsBetter);
  logger.LogSingleValueMetric("Decode_errors_reference", label,
                              results.decode_errors_ref, Unit::kUnitless,
                              ImprovementDirection::kNeitherIsBetter);
  logger.LogSingleValueMetric("Decode_errors_test", label,
                              results.decode_errors_test, Unit::kUnitless,
                              ImprovementDirection::kNeitherIsBetter);
}

}  // namespace test
}  // namespace webrtc
