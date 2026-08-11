/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_DECODER_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_DECODER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "api/rtc_error.h"

namespace webrtc {

// Decodes the video given in `encoded_file_path` if possible. The user cannot
// reach the individual frames. However, the user should be able to reach the
// decoded file from the decoded file path returned by `Decode` if successful.
// The decoded file is in Y4M format.
class FileBasedDecoder {
 public:
  FileBasedDecoder() = default;
  virtual ~FileBasedDecoder() = default;

  // Decodes the encoded file at `encoded_file_path` to a Y4M file. Returns the
  // path to the decoded file if successful.
  virtual RTCErrorOr<std::string> Decode(
      absl::string_view encoded_file_path) = 0;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_DECODER_H_
