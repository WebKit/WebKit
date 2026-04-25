/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_DATA_H_
#define API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_DATA_H_

#include <vector>

#include "api/array_view.h"

namespace webrtc {

class FrameInstrumentationData {
 public:
  FrameInstrumentationData();

  int sequence_index() const { return sequence_index_; }
  bool is_droppable() const { return droppable_; }
  double std_dev() const { return std_dev_; }
  int luma_error_threshold() const { return luma_error_threshold_; }
  int chroma_error_threshold() const { return chroma_error_threshold_; }
  ArrayView<const double> sample_values() const { return sample_values_; }

  bool SetSequenceIndex(int index);
  void set_droppable(bool droppable) { droppable_ = droppable; }
  bool SetStdDev(double std_dev);
  bool SetLumaErrorThreshold(int threshold);
  bool SetChromaErrorThreshold(int threshold);
  bool SetSampleValues(ArrayView<const double> samples);
  bool SetSampleValues(std::vector<double>&& samples);

  // Convenience methods..
  bool holds_upper_bits() const {
    return !droppable_ && (sequence_index_ & 0b0111'1111) == 0;
  }
  bool is_sync_only() const { return sample_values_.empty(); }

 private:
  int sequence_index_;
  bool droppable_;
  double std_dev_;
  int luma_error_threshold_;
  int chroma_error_threshold_;
  std::vector<double> sample_values_;
};

}  // namespace webrtc

#endif  // API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_DATA_H_
