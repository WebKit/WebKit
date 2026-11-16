/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/encoder_speed_controller.h"

#include <memory>

#include "api/units/time_delta.h"
#include "modules/video_coding/utility/encoder_speed_controller_impl.h"

namespace webrtc {

std::unique_ptr<EncoderSpeedController> EncoderSpeedController::Create(
    const Config& config,
    TimeDelta start_frame_interval) {
  return EncoderSpeedControllerImpl::Create(config, start_frame_interval);
}

}  // namespace webrtc
