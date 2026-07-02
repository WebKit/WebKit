/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_MOCK_FILE_BASED_DECODER_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_MOCK_FILE_BASED_DECODER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "api/rtc_error.h"
#include "test/gmock.h"
#include "video/corruption_detection/evaluation/file_based_decoder.h"

namespace webrtc {

class MockFileBasedDecoder : public FileBasedDecoder {
 public:
  MOCK_METHOD(RTCErrorOr<std::string>,
              Decode,
              (absl::string_view encoded_file_path),
              (override));
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_MOCK_FILE_BASED_DECODER_H_
