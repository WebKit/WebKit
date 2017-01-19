/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LEVEL_CONTROLLER_CONSTANTS_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LEVEL_CONTROLLER_CONSTANTS_H_

namespace webrtc {

const float kMaxLcGain = 10;
const float kMaxLcNoisePower = 100.f * 100.f;
const float kTargetLcPeakLevel = 16384.f;
const float kTargetLcPeakLeveldBFS = -6.0206f;

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LEVEL_CONTROLLER_CONSTANTS_H_
