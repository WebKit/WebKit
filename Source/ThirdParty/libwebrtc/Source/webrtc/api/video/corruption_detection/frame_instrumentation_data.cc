/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/frame_instrumentation_data.h"

#include <utility>
#include <vector>

#include "api/array_view.h"

namespace webrtc {

constexpr int kMaxSequenceIndex = (1 << 14) - 1;
constexpr double kMaxStdDev = 40.0;
constexpr int kMaxErrorThreshold = 15;

FrameInstrumentationData::FrameInstrumentationData()
    : sequence_index_(0),
      droppable_(false),
      std_dev_(0),
      luma_error_threshold_(0),
      chroma_error_threshold_(0) {}

bool FrameInstrumentationData::SetSequenceIndex(int index) {
  if (index < 0 || index > kMaxSequenceIndex)
    return false;

  sequence_index_ = index;
  return true;
}

bool FrameInstrumentationData::SetStdDev(double std_dev) {
  if (std_dev < 0.0 || std_dev > kMaxStdDev)
    return false;

  std_dev_ = std_dev;
  return true;
}

bool FrameInstrumentationData::SetLumaErrorThreshold(int threshold) {
  if (threshold < 0 || threshold > kMaxErrorThreshold)
    return false;

  luma_error_threshold_ = threshold;
  return true;
}

bool FrameInstrumentationData::SetChromaErrorThreshold(int threshold) {
  if (threshold < 0 || threshold > kMaxErrorThreshold)
    return false;

  chroma_error_threshold_ = threshold;
  return true;
}

bool FrameInstrumentationData::SetSampleValues(
    webrtc::ArrayView<const double> samples) {
  for (double sample_value : samples) {
    if (sample_value < 0.0 || sample_value > 255.0) {
      return false;
    }
  }
  sample_values_.assign(samples.begin(), samples.end());
  return true;
}

bool FrameInstrumentationData::SetSampleValues(std::vector<double>&& samples) {
  for (double sample_value : samples) {
    if (sample_value < 0.0 || sample_value > 255.0) {
      return false;
    }
  }
  sample_values_ = std::move(samples);
  return true;
}

}  // namespace webrtc
