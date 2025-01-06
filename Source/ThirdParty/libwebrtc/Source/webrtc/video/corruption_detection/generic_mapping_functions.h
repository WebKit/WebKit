/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_GENERIC_MAPPING_FUNCTIONS_H_
#define VIDEO_CORRUPTION_DETECTION_GENERIC_MAPPING_FUNCTIONS_H_

#include "api/video/video_codec_type.h"

namespace webrtc {

struct FilterSettings {
  double std_dev = 0.0;
  int luma_error_threshold = 0;
  int chroma_error_threshold = 0;
};

FilterSettings GetCorruptionFilterSettings(int qp, VideoCodecType codec_type);

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_GENERIC_MAPPING_FUNCTIONS_H_
