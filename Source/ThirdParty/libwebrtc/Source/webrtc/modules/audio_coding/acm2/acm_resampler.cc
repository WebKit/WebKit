/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_resampler.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "absl/algorithm/container.h"
#include "api/audio/audio_frame.h"
#include "api/audio/audio_view.h"
#include "api/audio/channel_layout.h"
#include "audio/utility/audio_frame_operations.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace acm2 {

ResamplerHelper::ResamplerHelper() {
  absl::c_fill(last_audio_buffer_, 0);
}

bool ResamplerHelper::MaybeResample(int desired_sample_rate_hz,
                                    AudioFrame* audio_frame) {
  const int current_sample_rate_hz = audio_frame->sample_rate_hz_;
  RTC_DCHECK_NE(current_sample_rate_hz, 0);
  RTC_DCHECK_GT(desired_sample_rate_hz, 0);

  // Update if resampling is required.
  // TODO(tommi): `desired_sample_rate_hz` should never be -1.
  // Remove the first check.
  const bool need_resampling =
      (desired_sample_rate_hz != -1) &&
      (current_sample_rate_hz != desired_sample_rate_hz);

  if (need_resampling) {
    const size_t target_size =
        audio_frame->num_channels_ *
        SampleRateToDefaultChannelSize(desired_sample_rate_hz);
    if (target_size > AudioFrame::kMaxDataSizeSamples) {
      RTC_LOG(LS_ERROR) << "AudioFrame cannot hold resampled data.";
      AudioFrameOperations::Mute(audio_frame);
      audio_frame->SetSampleRateAndChannelSize(desired_sample_rate_hz);
      audio_frame->SetLayoutAndNumChannels(
          CHANNEL_LAYOUT_UNSUPPORTED,
          AudioFrame::kMaxDataSizeSamples / audio_frame->samples_per_channel());
      return false;
    }
  }

  // The resampler's delay is shorter than a frame, so a muted frame that
  // follows a muted frame resamples to silence. Frames rejected above don't
  // reach the resampler and don't count.
  const bool previous_frame_muted = last_frame_muted_;
  last_frame_muted_ = audio_frame->muted();
  const bool silent = audio_frame->muted() && previous_frame_muted;

  if (need_resampling && silent) {
    // Keep the frame muted instead of resampling silence. Audio after it is
    // still resampled as if after silence: the resampler's last input was a
    // muted frame, or the resampler gets primed with silence below.
    audio_frame->SetSampleRateAndChannelSize(desired_sample_rate_hz);
    return true;
  }

  if (need_resampling && !resampled_last_output_frame_) {
    // Prime the resampler with the last frame.
    if (previous_frame_muted) {
      // `last_audio_buffer_` holds zeros only up to the size of the muted
      // frame it stored last, and this frame can be larger.
      absl::c_fill(last_audio_buffer_, 0);
    }
    InterleavedView<const int16_t> src(last_audio_buffer_.data(),
                                       audio_frame->samples_per_channel(),
                                       audio_frame->num_channels());
    std::array<int16_t, AudioFrame::kMaxDataSizeSamples> temp_output;
    InterleavedView<int16_t> dst(
        temp_output.data(),
        SampleRateToDefaultChannelSize(desired_sample_rate_hz),
        audio_frame->num_channels_);
    resampler_.Resample(src, dst);
  }

  // TODO(bugs.webrtc.org/3923) Glitches in the output may appear if the output
  // rate from NetEq changes.
  if (need_resampling) {
    // Grab the source view of the current layout before changing properties.
    InterleavedView<const int16_t> src = audio_frame->data_view();
    audio_frame->SetSampleRateAndChannelSize(desired_sample_rate_hz);
    InterleavedView<int16_t> dst = audio_frame->mutable_data(
        audio_frame->samples_per_channel(), audio_frame->num_channels());
    resampler_.Resample(src, dst);
    resampled_last_output_frame_ = true;
  } else {
    resampled_last_output_frame_ = false;
    // We might end up here ONLY if codec is changed.
  }

  // Store current audio in `last_audio_buffer_` for next time.
  InterleavedView<int16_t> dst(last_audio_buffer_.data(),
                               audio_frame->samples_per_channel(),
                               audio_frame->num_channels());
  CopySamples(dst, audio_frame->data_view());

  return true;
}

}  // namespace acm2
}  // namespace webrtc
