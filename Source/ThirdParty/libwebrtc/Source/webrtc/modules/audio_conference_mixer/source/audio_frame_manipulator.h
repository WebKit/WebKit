/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_AUDIO_FRAME_MANIPULATOR_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_AUDIO_FRAME_MANIPULATOR_H_

#include "webrtc/typedefs.h"

namespace webrtc {
class AudioFrame;

// Updates the audioFrame's energy (based on its samples).
uint32_t CalculateEnergy(const AudioFrame& audioFrame);

// Apply linear step function that ramps in/out the audio samples in audioFrame
void RampIn(AudioFrame& audioFrame);
void RampOut(AudioFrame& audioFrame);

}  // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_AUDIO_FRAME_MANIPULATOR_H_
