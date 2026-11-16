/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_MOCK_FILE_BASED_ENCODER_WITH_IVF_TRANSFORM_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_MOCK_FILE_BASED_ENCODER_WITH_IVF_TRANSFORM_H_

#include <string>

#include "api/rtc_error.h"
#include "api/units/data_rate.h"
#include "api/video/video_codec_type.h"
#include "test/gmock.h"
#include "video/corruption_detection/evaluation/file_based_encoder_with_ivf_transform.h"
#include "video/corruption_detection/evaluation/test_clip.h"

namespace webrtc {

class MockFileBasedEncoderWithIvfTransform
    : public FileBasedEncoderWithIvfTransform {
 public:
  MOCK_METHOD((RTCErrorOr<std::string>),
              Encode,
              (const TestClip& clip, DataRate bitrate),
              (override));

  MOCK_METHOD((RTCErrorOr<std::string>), TransformToIvf, (), (override));

  MOCK_METHOD(VideoCodecType, GetCodec, (), (const, override));
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_MOCK_FILE_BASED_ENCODER_WITH_IVF_TRANSFORM_H_
