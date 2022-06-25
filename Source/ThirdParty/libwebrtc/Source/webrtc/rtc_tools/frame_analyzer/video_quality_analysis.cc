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
#include <array>
#include <cstddef>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "test/testsupport/perf_test.h"
#include "third_party/libyuv/include/libyuv/compare.h"

namespace webrtc {
namespace test {

ResultsContainer::ResultsContainer() {}
ResultsContainer::~ResultsContainer() {}

template <typename FrameMetricFunction>
static double CalculateMetric(
    const FrameMetricFunction& frame_metric_function,
    const rtc::scoped_refptr<I420BufferInterface>& ref_buffer,
    const rtc::scoped_refptr<I420BufferInterface>& test_buffer) {
  RTC_CHECK_EQ(ref_buffer->width(), test_buffer->width());
  RTC_CHECK_EQ(ref_buffer->height(), test_buffer->height());
  return frame_metric_function(
      ref_buffer->DataY(), ref_buffer->StrideY(), ref_buffer->DataU(),
      ref_buffer->StrideU(), ref_buffer->DataV(), ref_buffer->StrideV(),
      test_buffer->DataY(), test_buffer->StrideY(), test_buffer->DataU(),
      test_buffer->StrideU(), test_buffer->DataV(), test_buffer->StrideV(),
      test_buffer->width(), test_buffer->height());
}

double Psnr(const rtc::scoped_refptr<I420BufferInterface>& ref_buffer,
            const rtc::scoped_refptr<I420BufferInterface>& test_buffer) {
  // LibYuv sets the max psnr value to 128, we restrict it to 48.
  // In case of 0 mse in one frame, 128 can skew the results significantly.
  return std::min(48.0,
                  CalculateMetric(&libyuv::I420Psnr, ref_buffer, test_buffer));
}

double Ssim(const rtc::scoped_refptr<I420BufferInterface>& ref_buffer,
            const rtc::scoped_refptr<I420BufferInterface>& test_buffer) {
  return CalculateMetric(&libyuv::I420Ssim, ref_buffer, test_buffer);
}

std::vector<AnalysisResult> RunAnalysis(
    const rtc::scoped_refptr<webrtc::test::Video>& reference_video,
    const rtc::scoped_refptr<webrtc::test::Video>& test_video,
    const std::vector<size_t>& test_frame_indices) {
  std::vector<AnalysisResult> results;
  for (size_t i = 0; i < test_video->number_of_frames(); ++i) {
    const rtc::scoped_refptr<I420BufferInterface>& test_frame =
        test_video->GetFrame(i);
    const rtc::scoped_refptr<I420BufferInterface>& reference_frame =
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
      clusters.push_back({index, /* number_of_repeated_frames= */ 1});
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

void PrintAnalysisResults(const std::string& label, ResultsContainer* results) {
  PrintAnalysisResults(stdout, label, results);
}

void PrintAnalysisResults(FILE* output,
                          const std::string& label,
                          ResultsContainer* results) {
  SetPerfResultsOutput(output);

  if (results->frames.size() > 0u) {
    PrintResult("Unique_frames_count", "", label, results->frames.size(),
                "score", false);

    std::vector<double> psnr_values;
    std::vector<double> ssim_values;
    for (const auto& frame : results->frames) {
      psnr_values.push_back(frame.psnr_value);
      ssim_values.push_back(frame.ssim_value);
    }

    PrintResultList("PSNR", "", label, psnr_values, "dB", false);
    PrintResultList("SSIM", "", label, ssim_values, "score", false);
  }

  PrintResult("Max_repeated", "", label, results->max_repeated_frames, "",
              false);
  PrintResult("Max_skipped", "", label, results->max_skipped_frames, "", false);
  PrintResult("Total_skipped", "", label, results->total_skipped_frames, "",
              false);
  PrintResult("Decode_errors_reference", "", label, results->decode_errors_ref,
              "", false);
  PrintResult("Decode_errors_test", "", label, results->decode_errors_test, "",
              false);
}

}  // namespace test
}  // namespace webrtc
