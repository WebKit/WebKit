/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_RTP_CORRUPTION_DETECTION_MESSAGE_H_
#define API_TRANSPORT_RTP_CORRUPTION_DETECTION_MESSAGE_H_

#include <cstddef>
#include <optional>

#include "absl/container/inlined_vector.h"
#include "api/array_view.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"

namespace webrtc {

class CorruptionDetectionMessage {
 public:
  class Builder;

  CorruptionDetectionMessage() = default;

  CorruptionDetectionMessage(const CorruptionDetectionMessage&) = default;
  CorruptionDetectionMessage& operator=(const CorruptionDetectionMessage&) =
      default;

  ~CorruptionDetectionMessage() = default;

  int sequence_index() const { return sequence_index_; }
  bool interpret_sequence_index_as_most_significant_bits() const {
    return interpret_sequence_index_as_most_significant_bits_;
  }
  double std_dev() const { return std_dev_; }
  int luma_error_threshold() const { return luma_error_threshold_; }
  int chroma_error_threshold() const { return chroma_error_threshold_; }
  ArrayView<const double> sample_values() const {
    return MakeArrayView(sample_values_.data(), sample_values_.size());
  }

  static CorruptionDetectionMessage FromFrameInstrumentationData(
      const FrameInstrumentationData& frame_instrumentation);

 private:
  friend class CorruptionDetectionExtension;

  static const size_t kMaxSampleSize = 13;

  // Sequence index in the Halton sequence.
  // Valid values: [0, 2^7-1]
  int sequence_index_ = 0;

  // Whether to interpret the `sequence_index_` as the most significant bits of
  // the true sequence index.
  bool interpret_sequence_index_as_most_significant_bits_ = false;

  // Standard deviation of the Gaussian filter kernel.
  // Valid values: [0, 40.0]
  double std_dev_ = 0.0;

  // Corruption threshold for the luma layer.
  // Valid values: [0, 2^4 - 1]
  int luma_error_threshold_ = 0;

  // Corruption threshold for the chroma layer.
  // Valid values: [0, 2^4 - 1]
  int chroma_error_threshold_ = 0;

  // An ordered list of samples that are the result of applying the Gaussian
  // filter on the image. The coordinates of the samples and their layer are
  // determined by the Halton sequence.
  // An empty list should be interpreted as a way to keep the `sequence_index`
  // in sync.
  absl::InlinedVector<double, kMaxSampleSize> sample_values_;
};

class CorruptionDetectionMessage::Builder {
 public:
  Builder() = default;
  Builder(const Builder&) = default;
  Builder& operator=(const Builder&) = default;

  ~Builder() = default;

  std::optional<CorruptionDetectionMessage> Build();

  Builder& WithSequenceIndex(int sequence_index);
  Builder& WithInterpretSequenceIndexAsMostSignificantBits(
      bool interpret_sequence_index_as_most_significant_bits);
  Builder& WithStdDev(double std_dev);
  Builder& WithLumaErrorThreshold(int luma_error_threshold);
  Builder& WithChromaErrorThreshold(int chroma_error_threshold);
  Builder& WithSampleValues(const ArrayView<const double>& sample_values);

 private:
  CorruptionDetectionMessage message_;
};

}  // namespace webrtc

#endif  // API_TRANSPORT_RTP_CORRUPTION_DETECTION_MESSAGE_H_
