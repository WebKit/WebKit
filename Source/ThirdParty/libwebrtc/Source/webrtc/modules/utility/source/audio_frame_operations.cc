/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/utility/include/audio_frame_operations.h"
#include "webrtc/base/checks.h"

namespace webrtc {
namespace  {

// 2.7ms @ 48kHz, 4ms @ 32kHz, 8ms @ 16kHz.
const size_t kMuteFadeFrames = 128;
const float kMuteFadeInc = 1.0f / kMuteFadeFrames;

}  // namespace {

void AudioFrameOperations::MonoToStereo(const int16_t* src_audio,
                                        size_t samples_per_channel,
                                        int16_t* dst_audio) {
  for (size_t i = 0; i < samples_per_channel; i++) {
    dst_audio[2 * i] = src_audio[i];
    dst_audio[2 * i + 1] = src_audio[i];
  }
}

int AudioFrameOperations::MonoToStereo(AudioFrame* frame) {
  if (frame->num_channels_ != 1) {
    return -1;
  }
  if ((frame->samples_per_channel_ * 2) >= AudioFrame::kMaxDataSizeSamples) {
    // Not enough memory to expand from mono to stereo.
    return -1;
  }

  int16_t data_copy[AudioFrame::kMaxDataSizeSamples];
  memcpy(data_copy, frame->data_,
         sizeof(int16_t) * frame->samples_per_channel_);
  MonoToStereo(data_copy, frame->samples_per_channel_, frame->data_);
  frame->num_channels_ = 2;

  return 0;
}

void AudioFrameOperations::StereoToMono(const int16_t* src_audio,
                                        size_t samples_per_channel,
                                        int16_t* dst_audio) {
  for (size_t i = 0; i < samples_per_channel; i++) {
    dst_audio[i] = (src_audio[2 * i] + src_audio[2 * i + 1]) >> 1;
  }
}

int AudioFrameOperations::StereoToMono(AudioFrame* frame) {
  if (frame->num_channels_ != 2) {
    return -1;
  }

  StereoToMono(frame->data_, frame->samples_per_channel_, frame->data_);
  frame->num_channels_ = 1;

  return 0;
}

void AudioFrameOperations::SwapStereoChannels(AudioFrame* frame) {
  if (frame->num_channels_ != 2) return;

  for (size_t i = 0; i < frame->samples_per_channel_ * 2; i += 2) {
    int16_t temp_data = frame->data_[i];
    frame->data_[i] = frame->data_[i + 1];
    frame->data_[i + 1] = temp_data;
  }
}

void AudioFrameOperations::Mute(AudioFrame* frame, bool previous_frame_muted,
                                bool current_frame_muted) {
  RTC_DCHECK(frame);
  if (!previous_frame_muted && !current_frame_muted) {
    // Not muted, don't touch.
  } else if (previous_frame_muted && current_frame_muted) {
    // Frame fully muted.
    size_t total_samples = frame->samples_per_channel_ * frame->num_channels_;
    RTC_DCHECK_GE(AudioFrame::kMaxDataSizeSamples, total_samples);
    memset(frame->data_, 0, sizeof(frame->data_[0]) * total_samples);
  } else {
    // Limit number of samples to fade, if frame isn't long enough.
    size_t count = kMuteFadeFrames;
    float inc = kMuteFadeInc;
    if (frame->samples_per_channel_ < kMuteFadeFrames) {
      count = frame->samples_per_channel_;
      if (count > 0) {
        inc = 1.0f / count;
      }
    }

    size_t start = 0;
    size_t end = count;
    float start_g = 0.0f;
    if (current_frame_muted) {
      // Fade out the last |count| samples of frame.
      RTC_DCHECK(!previous_frame_muted);
      start = frame->samples_per_channel_ - count;
      end = frame->samples_per_channel_;
      start_g = 1.0f;
      inc = -inc;
    } else {
      // Fade in the first |count| samples of frame.
      RTC_DCHECK(previous_frame_muted);
    }

    // Perform fade.
    size_t channels = frame->num_channels_;
    for (size_t j = 0; j < channels; ++j) {
      float g = start_g;
      for (size_t i = start * channels; i < end * channels; i += channels) {
        g += inc;
        frame->data_[i + j] *= g;
      }
    }
  }
}

int AudioFrameOperations::Scale(float left, float right, AudioFrame& frame) {
  if (frame.num_channels_ != 2) {
    return -1;
  }

  for (size_t i = 0; i < frame.samples_per_channel_; i++) {
    frame.data_[2 * i] =
        static_cast<int16_t>(left * frame.data_[2 * i]);
    frame.data_[2 * i + 1] =
        static_cast<int16_t>(right * frame.data_[2 * i + 1]);
  }
  return 0;
}

int AudioFrameOperations::ScaleWithSat(float scale, AudioFrame& frame) {
  int32_t temp_data = 0;

  // Ensure that the output result is saturated [-32768, +32767].
  for (size_t i = 0; i < frame.samples_per_channel_ * frame.num_channels_;
       i++) {
    temp_data = static_cast<int32_t>(scale * frame.data_[i]);
    if (temp_data < -32768) {
      frame.data_[i] = -32768;
    } else if (temp_data > 32767) {
      frame.data_[i] = 32767;
    } else {
      frame.data_[i] = static_cast<int16_t>(temp_data);
    }
  }
  return 0;
}

}  // namespace webrtc
