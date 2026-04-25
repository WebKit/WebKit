/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/frame_instrumentation_data_reader.h"

#include <optional>

#include "api/array_view.h"
#include "api/transport/rtp/corruption_detection_message.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "rtc_base/logging.h"

namespace webrtc {

std::optional<FrameInstrumentationData>
FrameInstrumentationDataReader::ParseMessage(
    const CorruptionDetectionMessage& message) {
  FrameInstrumentationData data;
  if (message.interpret_sequence_index_as_most_significant_bits()) {
    data.SetSequenceIndex(message.sequence_index() << 7);
  } else {
    if (!last_seen_sequence_index_.has_value()) {
      RTC_LOG(LS_WARNING)
          << "Got Corruption Detection Message with relative sequence index "
             "where no earlier sequence index is know. Ignoring.";
      return std::nullopt;
    }

    int upper_bits = *last_seen_sequence_index_ & 0b0011'1111'1000'0000;
    if (message.sequence_index() < (*last_seen_sequence_index_ & 0b0111'1111)) {
      // Assume one and only one wraparound has happened.
      upper_bits += 0b1000'0000;
    }
    // Replace the lowest bits with the bits from the update.
    data.SetSequenceIndex(upper_bits + message.sequence_index());
  }

  // The sequence index field of the message refers to the halton sequence index
  // for the first sample in the message. In order to figure out the next
  // expected sequence index we must increment it by the number of samples.
  ArrayView<const double> sample_values = message.sample_values();
  last_seen_sequence_index_ = data.sequence_index() + sample_values.size();

  if (!sample_values.empty()) {
    data.SetStdDev(message.std_dev());
    data.SetLumaErrorThreshold(message.luma_error_threshold());
    data.SetChromaErrorThreshold(message.chroma_error_threshold());
    data.SetSampleValues(sample_values);
  }

  return data;
}

}  // namespace webrtc
