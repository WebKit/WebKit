/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_instrumentation_evaluation.h"

#include <cstddef>
#include <vector>

#include "api/array_view.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "common_video/frame_instrumentation_data.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {

namespace {

std::vector<FilteredSample> ConvertSampleValuesToFilteredSamples(
    ArrayView<const double> values,
    ArrayView<const FilteredSample> samples) {
  RTC_CHECK_EQ(values.size(), samples.size())
      << "values and samples must have the same size";
  std::vector<FilteredSample> filtered_samples;
  filtered_samples.reserve(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    filtered_samples.push_back({.value = values[i], .plane = samples[i].plane});
  }
  return filtered_samples;
}

}  // namespace

FrameInstrumentationEvaluation::FrameInstrumentationEvaluation(
    CorruptionScoreObserver* observer)
    : observer_(observer), classifier_(/*scale_factor=*/3) {
  RTC_CHECK(observer);
}

void FrameInstrumentationEvaluation::OnInstrumentedFrame(
    const FrameInstrumentationData& data,
    const VideoFrame& frame,
    VideoContentType content_type) {
  if (data.sample_values.empty()) {
    RTC_LOG(LS_WARNING)
        << "Samples are needed to calculate a corruption score.";
    return;
  }

  frame_sampler_.SetCurrentIndex(data.sequence_index);
  std::vector<HaltonFrameSampler::Coordinates> sample_coordinates =
      frame_sampler_.GetSampleCoordinatesForFrame(data.sample_values.size());
  if (sample_coordinates.empty()) {
    RTC_LOG(LS_ERROR) << "Failed to get sample coordinates for frame.";
    return;
  }

  std::vector<FilteredSample> samples = GetSampleValuesForFrame(
      frame, sample_coordinates, frame.width(), frame.height(), data.std_dev);
  if (samples.empty()) {
    RTC_LOG(LS_ERROR) << "Failed to get sample values for frame";
    return;
  }

  std::vector<FilteredSample> data_samples =
      ConvertSampleValuesToFilteredSamples(data.sample_values, samples);
  if (data_samples.empty()) {
    RTC_LOG(LS_ERROR) << "Failed to convert sample values to filtered samples";
    return;
  }

  double score = classifier_.CalculateCorruptionProbability(
      data_samples, samples, data.luma_error_threshold,
      data.chroma_error_threshold);

  observer_->OnCorruptionScore(score, content_type);
}

}  // namespace webrtc
