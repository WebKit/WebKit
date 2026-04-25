/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/corruption_detection_extension.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "absl/container/inlined_vector.h"
#include "api/array_view.h"
#include "api/transport/rtp/corruption_detection_message.h"

namespace webrtc {
namespace {

constexpr size_t kMandatoryPayloadBytes = 1;
constexpr size_t kConfigurationBytes = 3;
constexpr double kMaxValueForStdDev = 40.0;

}  // namespace

// A description of the extension can be found at
// http://www.webrtc.org/experiments/rtp-hdrext/corruption-detection

bool CorruptionDetectionExtension::Parse(ArrayView<const uint8_t> data,
                                         CorruptionDetectionMessage* message) {
  if (message == nullptr) {
    return false;
  }
  if ((data.size() != kMandatoryPayloadBytes &&
       data.size() <= kConfigurationBytes) ||
      data.size() > kMaxValueSizeBytes) {
    return false;
  }
  message->interpret_sequence_index_as_most_significant_bits_ = data[0] >> 7;
  message->sequence_index_ = data[0] & 0b0111'1111;
  if (data.size() == kMandatoryPayloadBytes) {
    return true;
  }
  message->std_dev_ = data[1] * kMaxValueForStdDev / 255.0;
  uint8_t channel_error_thresholds = data[2];
  message->luma_error_threshold_ = channel_error_thresholds >> 4;
  message->chroma_error_threshold_ = channel_error_thresholds & 0xF;
  message->sample_values_.assign(data.cbegin() + kConfigurationBytes,
                                 data.cend());
  return true;
}

bool CorruptionDetectionExtension::Write(
    ArrayView<uint8_t> data,
    const CorruptionDetectionMessage& message) {
  if (data.size() != ValueSize(message) || data.size() > kMaxValueSizeBytes) {
    return false;
  }

  data[0] = message.sequence_index() & 0b0111'1111;
  if (message.interpret_sequence_index_as_most_significant_bits()) {
    data[0] |= 0b1000'0000;
  }
  if (message.sample_values().empty()) {
    return true;
  }
  data[1] = static_cast<uint8_t>(
      std::round(message.std_dev() / kMaxValueForStdDev * 255.0));
  data[2] = (message.luma_error_threshold() << 4) |
            (message.chroma_error_threshold() & 0xF);
  ArrayView<uint8_t> sample_values = data.subspan(kConfigurationBytes);
  for (size_t i = 0; i < message.sample_values().size(); ++i) {
    sample_values[i] = std::floor(message.sample_values()[i]);
  }
  return true;
}

size_t CorruptionDetectionExtension::ValueSize(
    const CorruptionDetectionMessage& message) {
  if (message.sample_values_.empty()) {
    return kMandatoryPayloadBytes;
  }
  return kConfigurationBytes + message.sample_values_.size();
}

}  // namespace webrtc
