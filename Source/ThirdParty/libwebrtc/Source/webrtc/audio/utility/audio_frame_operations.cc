/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/utility/audio_frame_operations.h"

#include <string.h>

#include <algorithm>
#include <cstdint>
#include <utility>

#include "common_audio/include/audio_util.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace {

// 2.7ms @ 48kHz, 4ms @ 32kHz, 8ms @ 16kHz.
const size_t kMuteFadeFrames = 128;
const float kMuteFadeInc = 1.0f / kMuteFadeFrames;

}  // namespace

void AudioFrameOperations::QuadToStereo(rtc::ArrayView<const int16_t> src_audio,
                                        size_t samples_per_channel,
                                        rtc::ArrayView<int16_t> dst_audio) {
  RTC_DCHECK_EQ(src_audio.size(), samples_per_channel * 4);
  RTC_DCHECK_EQ(dst_audio.size(), samples_per_channel * 2);
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

  if (!frame->muted()) {
    auto current_data = frame->data_view();
    QuadToStereo(current_data, frame->samples_per_channel_,
                 frame->mutable_data(frame->samples_per_channel_, 2));
  } else {
    frame->num_channels_ = 2;
  }

  return 0;
}

void AudioFrameOperations::DownmixChannels(
    rtc::ArrayView<const int16_t> src_audio,
    size_t src_channels,
    size_t samples_per_channel,
    size_t dst_channels,
    rtc::ArrayView<int16_t> dst_audio) {
  RTC_DCHECK_EQ(src_audio.size(), src_channels * samples_per_channel);
  RTC_DCHECK_EQ(dst_audio.size(), dst_channels * samples_per_channel);
  if (src_channels > 1 && dst_channels == 1) {
    DownmixInterleavedToMono(src_audio.data(), samples_per_channel,
                             src_channels, &dst_audio[0]);
  } else if (src_channels == 4 && dst_channels == 2) {
    QuadToStereo(src_audio, samples_per_channel, dst_audio);
  } else {
    RTC_DCHECK_NOTREACHED() << "src_channels: " << src_channels
                            << ", dst_channels: " << dst_channels;
  }
}

void AudioFrameOperations::DownmixChannels(size_t dst_channels,
                                           AudioFrame* frame) {
  RTC_DCHECK_LE(frame->samples_per_channel_ * frame->num_channels_,
                AudioFrame::kMaxDataSizeSamples);
  if (frame->num_channels_ > 1 && dst_channels == 1) {
    if (!frame->muted()) {
      DownmixInterleavedToMono(frame->data(), frame->samples_per_channel_,
                               frame->num_channels_, frame->mutable_data());
    }
    frame->num_channels_ = 1;
  } else if (frame->num_channels_ == 4 && dst_channels == 2) {
    int err = QuadToStereo(frame);
    RTC_DCHECK_EQ(err, 0);
  } else {
    RTC_DCHECK_NOTREACHED() << "src_channels: " << frame->num_channels_
                            << ", dst_channels: " << dst_channels;
  }
}

void AudioFrameOperations::UpmixChannels(size_t target_number_of_channels,
                                         AudioFrame* frame) {
  RTC_DCHECK_EQ(frame->num_channels_, 1);
  RTC_DCHECK_LE(frame->samples_per_channel_ * target_number_of_channels,
                AudioFrame::kMaxDataSizeSamples);

  if (frame->num_channels_ != 1 ||
      frame->samples_per_channel_ * target_number_of_channels >
          AudioFrame::kMaxDataSizeSamples) {
    return;
  }

  if (!frame->muted()) {
    // Up-mixing done in place. Going backwards through the frame ensure nothing
    // is irrevocably overwritten.
    auto frame_data = frame->mutable_data(frame->samples_per_channel_,
                                          target_number_of_channels);
    for (int i = frame->samples_per_channel_ - 1; i >= 0; --i) {
      for (size_t j = 0; j < target_number_of_channels; ++j) {
        frame_data[target_number_of_channels * i + j] = frame_data[i];
      }
    }
  } else {
    frame->num_channels_ = target_number_of_channels;
  }
}

void AudioFrameOperations::SwapStereoChannels(AudioFrame* frame) {
  RTC_DCHECK(frame);
  if (frame->num_channels_ != 2 || frame->muted()) {
    return;
  }

  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel_ * 2; i += 2) {
    std::swap(frame_data[i], frame_data[i + 1]);
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
    frame->Mute();
  } else {
    // Fade is a no-op on a muted frame.
    if (frame->muted()) {
      return;
    }

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
      // Fade out the last `count` samples of frame.
      RTC_DCHECK(!previous_frame_muted);
      start = frame->samples_per_channel_ - count;
      end = frame->samples_per_channel_;
      start_g = 1.0f;
      inc = -inc;
    } else {
      // Fade in the first `count` samples of frame.
      RTC_DCHECK(previous_frame_muted);
    }

    // Perform fade.
    int16_t* frame_data = frame->mutable_data();
    size_t channels = frame->num_channels_;
    for (size_t j = 0; j < channels; ++j) {
      float g = start_g;
      for (size_t i = start * channels; i < end * channels; i += channels) {
        g += inc;
        frame_data[i + j] *= g;
      }
    }
  }
}

void AudioFrameOperations::Mute(AudioFrame* frame) {
  Mute(frame, true, true);
}

int AudioFrameOperations::ScaleWithSat(float scale, AudioFrame* frame) {
  if (frame->muted()) {
    return 0;
  }

  int16_t* frame_data = frame->mutable_data();
  for (size_t i = 0; i < frame->samples_per_channel_ * frame->num_channels_;
       i++) {
    frame_data[i] = rtc::saturated_cast<int16_t>(scale * frame_data[i]);
  }
  return 0;
}
}  // namespace webrtc
