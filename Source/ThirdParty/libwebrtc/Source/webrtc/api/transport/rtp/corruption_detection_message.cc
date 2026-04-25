/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/rtp/corruption_detection_message.h"

#include <optional>
#include <utility>

#include "api/array_view.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

constexpr double kMaxStdDev = 40.0;
constexpr int kMaxErrorThreshold = 15;

}  // namespace

CorruptionDetectionMessage
CorruptionDetectionMessage::FromFrameInstrumentationData(
    const FrameInstrumentationData& frame_instrumentation) {
  int transmitted_sequence_index =
      frame_instrumentation.holds_upper_bits()
          ? frame_instrumentation.sequence_index() >> 7
          : (frame_instrumentation.sequence_index() & 0b0111'1111);
  Builder builder;
  builder.WithSequenceIndex(transmitted_sequence_index)
      .WithInterpretSequenceIndexAsMostSignificantBits(
          frame_instrumentation.holds_upper_bits());
  if (!frame_instrumentation.is_sync_only()) {
    builder.WithStdDev(frame_instrumentation.std_dev())
        .WithLumaErrorThreshold(frame_instrumentation.luma_error_threshold())
        .WithChromaErrorThreshold(
            frame_instrumentation.chroma_error_threshold())
        .WithSampleValues(frame_instrumentation.sample_values());
  }

  // The setter values of `FrameInstrumentationData` has already validated the
  // values, so this should never return nullopt;
  std::optional<CorruptionDetectionMessage> message = builder.Build();
  RTC_DCHECK(message.has_value());

  return std::move(*message);
}

std::optional<CorruptionDetectionMessage>
CorruptionDetectionMessage::Builder::Build() {
  if (message_.sequence_index_ < 0 || message_.sequence_index_ > 0b0111'1111) {
    return std::nullopt;
  }
  if (message_.std_dev_ < 0.0 || message_.std_dev_ > kMaxStdDev) {
    return std::nullopt;
  }
  if (message_.luma_error_threshold_ < 0 ||
      message_.luma_error_threshold_ > kMaxErrorThreshold) {
    return std::nullopt;
  }
  if (message_.chroma_error_threshold_ < 0 ||
      message_.chroma_error_threshold_ > kMaxErrorThreshold) {
    return std::nullopt;
  }
  if (message_.sample_values_.size() > kMaxSampleSize) {
    return std::nullopt;
  }
  for (double sample_value : message_.sample_values_) {
    if (sample_value < 0.0 || sample_value > 255.0) {
      return std::nullopt;
    }
  }
  return message_;
}

CorruptionDetectionMessage::Builder&
CorruptionDetectionMessage::Builder::WithSequenceIndex(int sequence_index) {
  message_.sequence_index_ = sequence_index;
  return *this;
}

CorruptionDetectionMessage::Builder& CorruptionDetectionMessage::Builder::
    WithInterpretSequenceIndexAsMostSignificantBits(
        bool interpret_sequence_index_as_most_significant_bits) {
  message_.interpret_sequence_index_as_most_significant_bits_ =
      interpret_sequence_index_as_most_significant_bits;
  return *this;
}

CorruptionDetectionMessage::Builder&
CorruptionDetectionMessage::Builder::WithStdDev(double std_dev) {
  message_.std_dev_ = std_dev;
  return *this;
}

CorruptionDetectionMessage::Builder&
CorruptionDetectionMessage::Builder::WithLumaErrorThreshold(
    int luma_error_threshold) {
  message_.luma_error_threshold_ = luma_error_threshold;
  return *this;
}

CorruptionDetectionMessage::Builder&
CorruptionDetectionMessage::Builder::WithChromaErrorThreshold(
    int chroma_error_threshold) {
  message_.chroma_error_threshold_ = chroma_error_threshold;
  return *this;
}

CorruptionDetectionMessage::Builder&
CorruptionDetectionMessage::Builder::WithSampleValues(
    const ArrayView<const double>& sample_values) {
  message_.sample_values_.assign(sample_values.cbegin(), sample_values.cend());
  return *this;
}

}  // namespace webrtc
