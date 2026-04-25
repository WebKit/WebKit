// Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef MODULES_VIDEO_CODING_CODECS_AV1_LIBAOM_SPEED_CONFIG_FACTORY_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_LIBAOM_SPEED_CONFIG_FACTORY_H_

#include "api/field_trials_view.h"
#include "api/video_codecs/encoder_speed_controller.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {

class LibaomSpeedConfigFactory {
 public:
  LibaomSpeedConfigFactory(VideoCodecComplexity complexity,
                           VideoCodecMode mode);

  EncoderSpeedController::Config GetSpeedConfig(
      int width,
      int height,
      int num_temporal_layers,
      const FieldTrialsView& field_trials);

 private:
  const VideoCodecComplexity complexity_;
  const VideoCodecMode mode_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_LIBAOM_SPEED_CONFIG_FACTORY_H_
