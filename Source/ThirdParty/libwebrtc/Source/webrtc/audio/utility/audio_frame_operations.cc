/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/utility/audio_frame_operations.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/modules/include/module_common_types.h"

namespace webrtc {
namespace {

// 2.7ms @ 48kHz, 4ms @ 32kHz, 8ms @ 16kHz.
const size_t kMuteFadeFrames = 128;
const float kMuteFadeInc = 1.0f / kMuteFadeFrames;

}  // namespace

void AudioFrameOperations::Add(const AudioFrame& frame_to_add,
                               AudioFrame* result_frame) {
  // Sanity check.
  RTC_DCHECK(result_frame);
  RTC_DCHECK_GT(result_frame->num_channels_, 0);
  RTC_DCHECK_EQ(result_frame->num_channels_, frame_to_add.num_channels_);

  bool no_previous_data = false;
  if (result_frame->samples_per_channel_ != frame_to_add.samples_per_channel_) {
    // Special case we have no data to start with.
    RTC_DCHECK_EQ(result_frame->samples_per_channel_, 0);
    result_frame->samples_per_channel_ = frame_to_add.samples_per_channel_;
    no_previous_data = true;
  }

  if (result_frame->vad_activity_ == AudioFrame::kVadActive ||
      frame_to_add.vad_activity_ == AudioFrame::kVadActive) {
    result_frame->vad_activity_ = AudioFrame::kVadActive;
  } else if (result_frame->vad_activity_ == AudioFrame::kVadUnknown ||
             frame_to_add.vad_activity_ == AudioFrame::kVadUnknown) {
    result_frame->vad_activity_ = AudioFrame::kVadUnknown;
  }

  if (result_frame->speech_type_ != frame_to_add.speech_type_)
    result_frame->speech_type_ = AudioFrame::kUndefined;

  if (no_previous_data) {
    std::copy(frame_to_add.data_, frame_to_add.data_ +
                                      frame_to_add.samples_per_channel_ *
                                          result_frame->num_channels_,
              result_frame->data_);
  } else {
    for (size_t i = 0;
         i < result_frame->samples_per_channel_ * result_frame->num_channels_;
         i++) {
      const int32_t wrap_guard = static_cast<int32_t>(result_frame->data_[i]) +
                           static_cast<int32_t>(frame_to_add.data_[i]);
      result_frame->data_[i] = rtc::saturated_cast<int16_t>(wrap_guard);
    }
  }
  return;
}

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
    dst_audio[i] =
        (static_cast<int32_t>(src_audio[2 * i]) + src_audio[2 * i + 1]) >> 1;
  }
}

int AudioFrameOperations::StereoToMono(AudioFrame* frame) {
  if (frame->num_channels_ != 2) {
    return -1;
  }

  RTC_DCHECK_LE(frame->samples_per_channel_ * 2,
                AudioFrame::kMaxDataSizeSamples);

  StereoToMono(frame->data_, frame->samples_per_channel_, frame->data_);
  frame->num_channels_ = 1;

  return 0;
}

void AudioFrameOperations::QuadToStereo(const int16_t* src_audio,
                                        size_t samples_per_channel,
                                        int16_t* dst_audio) {
  for (size_t i = 0; i < samples_per_channel; i++) {
    dst_audio[i * 2] =
        (static_cast<int32_t>(src_audio[4 * i]) + src_audio[4 * i + 1]) >> 1;
    dst_audio[i * 2 + 1] =
        (static_cast<int32_t>(src_audio[4 * i + 2]) + src_audio[4 * i + 3]) >>
        1;
  }
}

int AudioFrameOperations::QuadToStereo(AudioFrame* frame) {
  if (frame->num_channels_ != 4) {
    return -1;
  }

  RTC_DCHECK_LE(frame->samples_per_channel_ * 4,
                AudioFrame::kMaxDataSizeSamples);

  QuadToStereo(frame->data_, frame->samples_per_channel_, frame->data_);
  frame->num_channels_ = 2;

  return 0;
}

void AudioFrameOperations::QuadToMono(const int16_t* src_audio,
                                      size_t samples_per_channel,
                                      int16_t* dst_audio) {
  for (size_t i = 0; i < samples_per_channel; i++) {
    dst_audio[i] =
        (static_cast<int32_t>(src_audio[4 * i]) + src_audio[4 * i + 1] +
         src_audio[4 * i + 2] + src_audio[4 * i + 3]) >> 2;
  }
}

int AudioFrameOperations::QuadToMono(AudioFrame* frame) {
  if (frame->num_channels_ != 4) {
    return -1;
  }

  RTC_DCHECK_LE(frame->samples_per_channel_ * 4,
                AudioFrame::kMaxDataSizeSamples);

  QuadToMono(frame->data_, frame->samples_per_channel_, frame->data_);
  frame->num_channels_ = 1;

  return 0;
}

void AudioFrameOperations::DownmixChannels(const int16_t* src_audio,
                                           size_t src_channels,
                                           size_t samples_per_channel,
                                           size_t dst_channels,
                                           int16_t* dst_audio) {
  if (src_channels == 2 && dst_channels == 1) {
    StereoToMono(src_audio, samples_per_channel, dst_audio);
    return;
  } else if (src_channels == 4 && dst_channels == 2) {
    QuadToStereo(src_audio, samples_per_channel, dst_audio);
    return;
  } else if (src_channels == 4 && dst_channels == 1) {
    QuadToMono(src_audio, samples_per_channel, dst_audio);
    return;
  }

  RTC_NOTREACHED() << "src_channels: " << src_channels
                   << ", dst_channels: " << dst_channels;
}

int AudioFrameOperations::DownmixChannels(size_t dst_channels,
                                          AudioFrame* frame) {
  if (frame->num_channels_ == 2 && dst_channels == 1) {
    return StereoToMono(frame);
  } else if (frame->num_channels_ == 4 && dst_channels == 2) {
    return QuadToStereo(frame);
  } else if (frame->num_channels_ == 4 && dst_channels == 1) {
    return QuadToMono(frame);
  }

  return -1;
}

void AudioFrameOperations::SwapStereoChannels(AudioFrame* frame) {
  RTC_DCHECK(frame);
  if (frame->num_channels_ != 2) {
    return;
  }

  for (size_t i = 0; i < frame->samples_per_channel_ * 2; i += 2) {
    int16_t temp_data = frame->data_[i];
    frame->data_[i] = frame->data_[i + 1];
    frame->data_[i + 1] = temp_data;
  }
}

void AudioFrameOperations::Mute(AudioFrame* frame,
                                bool previous_frame_muted,
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

void AudioFrameOperations::Mute(AudioFrame* frame) {
  Mute(frame, true, true);
}

void AudioFrameOperations::ApplyHalfGain(AudioFrame* frame) {
  RTC_DCHECK(frame);
  RTC_DCHECK_GT(frame->num_channels_, 0);
  if (frame->num_channels_ < 1) {
    return;
  }

  for (size_t i = 0; i < frame->samples_per_channel_ * frame->num_channels_;
       i++) {
    frame->data_[i] = frame->data_[i] >> 1;
  }
}

int AudioFrameOperations::Scale(float left, float right, AudioFrame& frame) {
  if (frame.num_channels_ != 2) {
    return -1;
  }

  for (size_t i = 0; i < frame.samples_per_channel_; i++) {
    frame.data_[2 * i] = static_cast<int16_t>(left * frame.data_[2 * i]);
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
