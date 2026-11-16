/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_ENCODER_WITH_IVF_TRANSFORM_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_ENCODER_WITH_IVF_TRANSFORM_H_

#include <string>

#include "api/rtc_error.h"
#include "video/corruption_detection/evaluation/file_based_encoder.h"

namespace webrtc {

class FileBasedEncoderWithIvfTransform : public FileBasedEncoder {
 public:
  // Transforms the encoded video by the `Encode` method to IVF format. It
  // returns the path to the IVF file if successful.
  virtual RTCErrorOr<std::string> TransformToIvf() = 0;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_FILE_BASED_ENCODER_WITH_IVF_TRANSFORM_H_
