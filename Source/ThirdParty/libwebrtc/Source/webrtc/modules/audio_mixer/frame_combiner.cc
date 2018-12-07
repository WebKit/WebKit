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

#include "api/array_view.h"
#include "audio/utility/audio_frame_operations.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_mixer/audio_frame_manipulator.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

// Stereo, 48 kHz, 10 ms.
constexpr int kMaximumAmountOfChannels = 2;
constexpr int kMaximumChannelSize = 48 * AudioMixerImpl::kFrameDurationInMs;

using OneChannelBuffer = std::array<float, kMaximumChannelSize>;

void SetAudioFrameFields(const std::vector<AudioFrame*>& mix_list,
                         size_t number_of_channels,
                         int sample_rate,
                         size_t number_of_streams,
                         AudioFrame* audio_frame_for_mixing) {
  const size_t samples_per_channel = static_cast<size_t>(
      (sample_rate * webrtc::AudioMixerImpl::kFrameDurationInMs) / 1000);

  // TODO(minyue): Issue bugs.webrtc.org/3390.
  // Audio frame timestamp. The 'timestamp_' field is set to dummy
  // value '0', because it is only supported in the one channel case and
  // is then updated in the helper functions.
  audio_frame_for_mixing->UpdateFrame(
      0, nullptr, samples_per_channel, sample_rate, AudioFrame::kUndefined,
      AudioFrame::kVadUnknown, number_of_channels);

  if (mix_list.empty()) {
    audio_frame_for_mixing->elapsed_time_ms_ = -1;
  } else if (mix_list.size() == 1) {
    audio_frame_for_mixing->timestamp_ = mix_list[0]->timestamp_;
    audio_frame_for_mixing->elapsed_time_ms_ = mix_list[0]->elapsed_time_ms_;
    audio_frame_for_mixing->ntp_time_ms_ = mix_list[0]->ntp_time_ms_;
  }
}

void MixFewFramesWithNoLimiter(const std::vector<AudioFrame*>& mix_list,
                               AudioFrame* audio_frame_for_mixing) {
  if (mix_list.empty()) {
    audio_frame_for_mixing->Mute();
    return;
  }
  RTC_DCHECK_LE(mix_list.size(), 1);
  std::copy(mix_list[0]->data(),
            mix_list[0]->data() +
                mix_list[0]->num_channels_ * mix_list[0]->samples_per_channel_,
            audio_frame_for_mixing->mutable_data());
}

std::array<OneChannelBuffer, kMaximumAmountOfChannels> MixToFloatFrame(
    const std::vector<AudioFrame*>& mix_list,
    size_t samples_per_channel,
    size_t number_of_channels) {
  // Convert to FloatS16 and mix.
  using OneChannelBuffer = std::array<float, kMaximumChannelSize>;
  std::array<OneChannelBuffer, kMaximumAmountOfChannels> mixing_buffer{};

  for (size_t i = 0; i < mix_list.size(); ++i) {
    const AudioFrame* const frame = mix_list[i];
    for (size_t j = 0; j < number_of_channels; ++j) {
      for (size_t k = 0; k < samples_per_channel; ++k) {
        mixing_buffer[j][k] += frame->data()[number_of_channels * k + j];
      }
    }
  }
  return mixing_buffer;
}

void RunLimiter(AudioFrameView<float> mixing_buffer_view, Limiter* limiter) {
  const size_t sample_rate = mixing_buffer_view.samples_per_channel() * 1000 /
                             AudioMixerImpl::kFrameDurationInMs;
  // TODO(alessiob): Avoid calling SetSampleRate every time.
  limiter->SetSampleRate(sample_rate);
  limiter->Process(mixing_buffer_view);
}

// Both interleaves and rounds.
void InterleaveToAudioFrame(AudioFrameView<const float> mixing_buffer_view,
                            AudioFrame* audio_frame_for_mixing) {
  const size_t number_of_channels = mixing_buffer_view.num_channels();
  const size_t samples_per_channel = mixing_buffer_view.samples_per_channel();
  // Put data in the result frame.
  for (size_t i = 0; i < number_of_channels; ++i) {
    for (size_t j = 0; j < samples_per_channel; ++j) {
      audio_frame_for_mixing->mutable_data()[number_of_channels * j + i] =
          FloatS16ToS16(mixing_buffer_view.channel(i)[j]);
    }
  }
}
}  // namespace

FrameCombiner::FrameCombiner(bool use_limiter)
    : data_dumper_(new ApmDataDumper(0)),
      limiter_(static_cast<size_t>(48000), data_dumper_.get(), "AudioMixer"),
      use_limiter_(use_limiter) {}

FrameCombiner::~FrameCombiner() = default;

void FrameCombiner::Combine(const std::vector<AudioFrame*>& mix_list,
                            size_t number_of_channels,
                            int sample_rate,
                            size_t number_of_streams,
                            AudioFrame* audio_frame_for_mixing) {
  RTC_DCHECK(audio_frame_for_mixing);

  LogMixingStats(mix_list, sample_rate, number_of_streams);

  SetAudioFrameFields(mix_list, number_of_channels, sample_rate,
                      number_of_streams, audio_frame_for_mixing);

  const size_t samples_per_channel = static_cast<size_t>(
      (sample_rate * webrtc::AudioMixerImpl::kFrameDurationInMs) / 1000);

  for (const auto* frame : mix_list) {
    RTC_DCHECK_EQ(samples_per_channel, frame->samples_per_channel_);
    RTC_DCHECK_EQ(sample_rate, frame->sample_rate_hz_);
  }

  // The 'num_channels_' field of frames in 'mix_list' could be
  // different from 'number_of_channels'.
  for (auto* frame : mix_list) {
    RemixFrame(number_of_channels, frame);
  }

  if (number_of_streams <= 1) {
    MixFewFramesWithNoLimiter(mix_list, audio_frame_for_mixing);
    return;
  }

  std::array<OneChannelBuffer, kMaximumAmountOfChannels> mixing_buffer =
      MixToFloatFrame(mix_list, samples_per_channel, number_of_channels);

  // Put float data in an AudioFrameView.
  std::array<float*, kMaximumAmountOfChannels> channel_pointers{};
  for (size_t i = 0; i < number_of_channels; ++i) {
    channel_pointers[i] = &mixing_buffer[i][0];
  }
  AudioFrameView<float> mixing_buffer_view(
      &channel_pointers[0], number_of_channels, samples_per_channel);

  if (use_limiter_) {
    RunLimiter(mixing_buffer_view, &limiter_);
  }

  InterleaveToAudioFrame(mixing_buffer_view, audio_frame_for_mixing);
}

void FrameCombiner::LogMixingStats(const std::vector<AudioFrame*>& mix_list,
                                   int sample_rate,
                                   size_t number_of_streams) const {
  // Log every second.
  uma_logging_counter_++;
  if (uma_logging_counter_ > 1000 / AudioMixerImpl::kFrameDurationInMs) {
    uma_logging_counter_ = 0;
    RTC_HISTOGRAM_COUNTS_100("WebRTC.Audio.AudioMixer.NumIncomingStreams",
                             static_cast<int>(number_of_streams));
    RTC_HISTOGRAM_ENUMERATION(
        "WebRTC.Audio.AudioMixer.NumIncomingActiveStreams",
        static_cast<int>(mix_list.size()),
        AudioMixerImpl::kMaximumAmountOfMixedAudioSources);

    using NativeRate = AudioProcessing::NativeRate;
    static constexpr NativeRate native_rates[] = {
        NativeRate::kSampleRate8kHz, NativeRate::kSampleRate16kHz,
        NativeRate::kSampleRate32kHz, NativeRate::kSampleRate48kHz};
    const auto* rate_position = std::lower_bound(
        std::begin(native_rates), std::end(native_rates), sample_rate);

    RTC_HISTOGRAM_ENUMERATION(
        "WebRTC.Audio.AudioMixer.MixingRate",
        std::distance(std::begin(native_rates), rate_position),
        arraysize(native_rates));
  }
}

}  // namespace webrtc
