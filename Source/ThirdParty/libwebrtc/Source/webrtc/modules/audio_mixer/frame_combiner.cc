/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_mixer/frame_combiner.h"

#include <algorithm>
#include <array>
#include <functional>
#include <memory>

#include "api/array_view.h"
#include "audio/utility/audio_frame_operations.h"
#include "modules/audio_mixer/audio_frame_manipulator.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// Stereo, 48 kHz, 10 ms.
constexpr int kMaximalFrameSize = 2 * 48 * 10;

void CombineZeroFrames(bool use_limiter,
                       AudioProcessing* limiter,
                       AudioFrame* audio_frame_for_mixing) {
  audio_frame_for_mixing->elapsed_time_ms_ = -1;
  AudioFrameOperations::Mute(audio_frame_for_mixing);
  // The limiter should still process a zero frame to avoid jumps in
  // its gain curve.
  if (use_limiter) {
    RTC_DCHECK(limiter);
    // The limiter smoothly increases frames with half gain to full
    // volume.  Here there's no need to apply half gain, since the frame
    // is zero anyway.
    limiter->ProcessStream(audio_frame_for_mixing);
  }
}

void CombineOneFrame(const AudioFrame* input_frame,
                     bool use_limiter,
                     AudioProcessing* limiter,
                     AudioFrame* audio_frame_for_mixing) {
  audio_frame_for_mixing->timestamp_ = input_frame->timestamp_;
  audio_frame_for_mixing->elapsed_time_ms_ = input_frame->elapsed_time_ms_;
  // TODO(yujo): can we optimize muted frames?
  std::copy(input_frame->data(),
            input_frame->data() +
                input_frame->num_channels_ * input_frame->samples_per_channel_,
            audio_frame_for_mixing->mutable_data());
  if (use_limiter) {
    AudioFrameOperations::ApplyHalfGain(audio_frame_for_mixing);
    RTC_DCHECK(limiter);
    limiter->ProcessStream(audio_frame_for_mixing);
    AudioFrameOperations::Add(*audio_frame_for_mixing, audio_frame_for_mixing);
  }
}

// Lower-level helper function called from Combine(...) when there
// are several input frames.
//
// TODO(aleloi): change interface to ArrayView<int16_t> output_frame
// once we have gotten rid of the APM limiter.
//
// Only the 'data' field of output_frame should be modified. The
// rest are used for potentially sending the output to the APM
// limiter.
void CombineMultipleFrames(
    const std::vector<rtc::ArrayView<const int16_t>>& input_frames,
    bool use_limiter,
    AudioProcessing* limiter,
    AudioFrame* audio_frame_for_mixing) {
  RTC_DCHECK(!input_frames.empty());
  RTC_DCHECK(audio_frame_for_mixing);

  const size_t frame_length = input_frames.front().size();
  for (const auto& frame : input_frames) {
    RTC_DCHECK_EQ(frame_length, frame.size());
  }

  // Algorithm: int16 frames are added to a sufficiently large
  // statically allocated int32 buffer. For > 2 participants this is
  // more efficient than addition in place in the int16 audio
  // frame. The audio quality loss due to halving the samples is
  // smaller than 16-bit addition in place.
  RTC_DCHECK_GE(kMaximalFrameSize, frame_length);
  std::array<int32_t, kMaximalFrameSize> add_buffer;

  add_buffer.fill(0);

  for (const auto& frame : input_frames) {
    // TODO(yujo): skip this for muted frames.
    std::transform(frame.begin(), frame.end(), add_buffer.begin(),
                   add_buffer.begin(), std::plus<int32_t>());
  }

  if (use_limiter) {
    // Halve all samples to avoid saturation before limiting.
    std::transform(add_buffer.begin(), add_buffer.begin() + frame_length,
                   audio_frame_for_mixing->mutable_data(), [](int32_t a) {
                     return rtc::saturated_cast<int16_t>(a / 2);
                   });

    // Smoothly limit the audio.
    RTC_DCHECK(limiter);
    const int error = limiter->ProcessStream(audio_frame_for_mixing);
    if (error != limiter->kNoError) {
      RTC_LOG_F(LS_ERROR) << "Error from AudioProcessing: " << error;
      RTC_NOTREACHED();
    }

    // And now we can safely restore the level. This procedure results in
    // some loss of resolution, deemed acceptable.
    //
    // It's possible to apply the gain in the AGC (with a target level of 0 dbFS
    // and compression gain of 6 dB). However, in the transition frame when this
    // is enabled (moving from one to two audio sources) it has the potential to
    // create discontinuities in the mixed frame.
    //
    // Instead we double the frame (with addition since left-shifting a
    // negative value is undefined).
    AudioFrameOperations::Add(*audio_frame_for_mixing, audio_frame_for_mixing);
  } else {
    std::transform(add_buffer.begin(), add_buffer.begin() + frame_length,
                   audio_frame_for_mixing->mutable_data(),
                   [](int32_t a) { return rtc::saturated_cast<int16_t>(a); });
  }
}

std::unique_ptr<AudioProcessing> CreateLimiter() {
  Config config;
  config.Set<ExperimentalAgc>(new ExperimentalAgc(false));

  std::unique_ptr<AudioProcessing> limiter(AudioProcessing::Create(config));
  RTC_DCHECK(limiter);

  webrtc::AudioProcessing::Config apm_config;
  apm_config.residual_echo_detector.enabled = false;
  limiter->ApplyConfig(apm_config);

  const auto check_no_error = [](int x) {
    RTC_DCHECK_EQ(x, AudioProcessing::kNoError);
  };
  auto* const gain_control = limiter->gain_control();
  check_no_error(gain_control->set_mode(GainControl::kFixedDigital));

  // We smoothly limit the mixed frame to -7 dbFS. -6 would correspond to the
  // divide-by-2 but -7 is used instead to give a bit of headroom since the
  // AGC is not a hard limiter.
  check_no_error(gain_control->set_target_level_dbfs(7));

  check_no_error(gain_control->set_compression_gain_db(0));
  check_no_error(gain_control->enable_limiter(true));
  check_no_error(gain_control->Enable(true));
  return limiter;
}
}  // namespace

FrameCombiner::FrameCombiner(bool use_apm_limiter)
    : use_apm_limiter_(use_apm_limiter),
      limiter_(use_apm_limiter ? CreateLimiter() : nullptr) {}

FrameCombiner::~FrameCombiner() = default;

void FrameCombiner::Combine(const std::vector<AudioFrame*>& mix_list,
                            size_t number_of_channels,
                            int sample_rate,
                            size_t number_of_streams,
                            AudioFrame* audio_frame_for_mixing) const {
  RTC_DCHECK(audio_frame_for_mixing);
  const size_t samples_per_channel = static_cast<size_t>(
      (sample_rate * webrtc::AudioMixerImpl::kFrameDurationInMs) / 1000);

  for (const auto* frame : mix_list) {
    RTC_DCHECK_EQ(samples_per_channel, frame->samples_per_channel_);
    RTC_DCHECK_EQ(sample_rate, frame->sample_rate_hz_);
  }

  // Frames could be both stereo and mono.
  for (auto* frame : mix_list) {
    RemixFrame(number_of_channels, frame);
  }

  // TODO(aleloi): Issue bugs.webrtc.org/3390.
  // Audio frame timestamp. The 'timestamp_' field is set to dummy
  // value '0', because it is only supported in the one channel case and
  // is then updated in the helper functions.
  audio_frame_for_mixing->UpdateFrame(
      0, nullptr, samples_per_channel, sample_rate, AudioFrame::kUndefined,
      AudioFrame::kVadUnknown, number_of_channels);

  const bool use_limiter_this_round = use_apm_limiter_ && number_of_streams > 1;

  if (mix_list.empty()) {
    CombineZeroFrames(use_limiter_this_round, limiter_.get(),
                      audio_frame_for_mixing);
  } else if (mix_list.size() == 1) {
    CombineOneFrame(mix_list.front(), use_limiter_this_round, limiter_.get(),
                    audio_frame_for_mixing);
  } else {
    std::vector<rtc::ArrayView<const int16_t>> input_frames;
    for (size_t i = 0; i < mix_list.size(); ++i) {
      input_frames.push_back(rtc::ArrayView<const int16_t>(
          mix_list[i]->data(), samples_per_channel * number_of_channels));
    }
    CombineMultipleFrames(input_frames, use_limiter_this_round, limiter_.get(),
                          audio_frame_for_mixing);
  }
}

}  // namespace webrtc
