/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_mixer/audio_frame_manipulator.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/utility/include/audio_frame_operations.h"

namespace webrtc {

uint32_t AudioMixerCalculateEnergy(const AudioFrame& audio_frame) {
  uint32_t energy = 0;
  for (size_t position = 0; position < audio_frame.samples_per_channel_;
       position++) {
    // TODO(aleloi): This can overflow. Convert to floats.
    energy += audio_frame.data_[position] * audio_frame.data_[position];
  }
  return energy;
}

void Ramp(float start_gain, float target_gain, AudioFrame* audio_frame) {
  RTC_DCHECK(audio_frame);
  RTC_DCHECK_GE(start_gain, 0.0f);
  RTC_DCHECK_GE(target_gain, 0.0f);

  size_t samples = audio_frame->samples_per_channel_;
  RTC_DCHECK_LT(0u, samples);
  float increment = (target_gain - start_gain) / samples;
  float gain = start_gain;
  for (size_t i = 0; i < samples; ++i) {
    // If the audio is interleaved of several channels, we want to
    // apply the same gain change to the ith sample of every channel.
    for (size_t ch = 0; ch < audio_frame->num_channels_; ++ch) {
      audio_frame->data_[audio_frame->num_channels_ * i + ch] *= gain;
    }
    gain += increment;
  }
}

void RemixFrame(size_t target_number_of_channels, AudioFrame* frame) {
  RTC_DCHECK_GE(target_number_of_channels, 1u);
  RTC_DCHECK_LE(target_number_of_channels, 2u);
  if (frame->num_channels_ == 1 && target_number_of_channels == 2) {
    AudioFrameOperations::MonoToStereo(frame);
  } else if (frame->num_channels_ == 2 && target_number_of_channels == 1) {
    AudioFrameOperations::StereoToMono(frame);
  }
}
}  // namespace webrtc
