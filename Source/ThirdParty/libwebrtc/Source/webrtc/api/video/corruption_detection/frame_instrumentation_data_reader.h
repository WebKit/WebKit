/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_DATA_READER_H_
#define API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_DATA_READER_H_

#include <optional>

#include "api/transport/rtp/corruption_detection_message.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"

namespace webrtc {

class FrameInstrumentationDataReader {
 public:
  FrameInstrumentationDataReader() = default;

  std::optional<FrameInstrumentationData> ParseMessage(
      const CorruptionDetectionMessage& message);

 private:
  std::optional<int> last_seen_sequence_index_;
};

}  // namespace webrtc

#endif  // API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_DATA_READER_H_
