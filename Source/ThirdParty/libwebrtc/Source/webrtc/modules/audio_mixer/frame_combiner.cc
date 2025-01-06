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
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/audio/audio_processing.h"
#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_mixer/audio_frame_manipulator.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

void SetAudioFrameFields(rtc::ArrayView<const AudioFrame* const> mix_list,
                         size_t number_of_channels,
                         int sample_rate,
                         size_t /* number_of_streams */,
                         AudioFrame* audio_frame_for_mixing) {
  const size_t samples_per_channel =
      SampleRateToDefaultChannelSize(sample_rate);

  // TODO(minyue): Issue bugs.webrtc.org/3390.
  // Audio frame timestamp. The 'timestamp_' field is set to dummy
  // value '0', because it is only supported in the one channel case and
  // is then updated in the helper functions.
  audio_frame_for_mixing->UpdateFrame(
      0, nullptr, samples_per_channel, sample_rate, AudioFrame::kUndefined,
      AudioFrame::kVadUnknown, number_of_channels);

  if (mix_list.empty()) {
    audio_frame_for_mixing->elapsed_time_ms_ = -1;
  } else {
    audio_frame_for_mixing->timestamp_ = mix_list[0]->timestamp_;
    audio_frame_for_mixing->elapsed_time_ms_ = mix_list[0]->elapsed_time_ms_;
    audio_frame_for_mixing->ntp_time_ms_ = mix_list[0]->ntp_time_ms_;
    std::vector<RtpPacketInfo> packet_infos;
    for (const auto& frame : mix_list) {
      audio_frame_for_mixing->timestamp_ =
          std::min(audio_frame_for_mixing->timestamp_, frame->timestamp_);
      audio_frame_for_mixing->ntp_time_ms_ =
          std::min(audio_frame_for_mixing->ntp_time_ms_, frame->ntp_time_ms_);
      audio_frame_for_mixing->elapsed_time_ms_ = std::max(
          audio_frame_for_mixing->elapsed_time_ms_, frame->elapsed_time_ms_);
      packet_infos.insert(packet_infos.end(), frame->packet_infos_.begin(),
                          frame->packet_infos_.end());
    }
    audio_frame_for_mixing->packet_infos_ =
        RtpPacketInfos(std::move(packet_infos));
  }
}

void MixFewFramesWithNoLimiter(rtc::ArrayView<const AudioFrame* const> mix_list,
                               AudioFrame* audio_frame_for_mixing) {
  if (mix_list.empty()) {
    audio_frame_for_mixing->Mute();
    return;
  }
  RTC_DCHECK_LE(mix_list.size(), 1);
  InterleavedView<int16_t> dst = audio_frame_for_mixing->mutable_data(
      mix_list[0]->samples_per_channel_, mix_list[0]->num_channels_);
  CopySamples(dst, mix_list[0]->data_view());
}

void MixToFloatFrame(rtc::ArrayView<const AudioFrame* const> mix_list,
                     DeinterleavedView<float>& mixing_buffer) {
  const size_t number_of_channels = NumChannels(mixing_buffer);
  // Clear the mixing buffer.
  rtc::ArrayView<float> raw_data = mixing_buffer.data();
  ClearSamples(raw_data);

  // Convert to FloatS16 and mix.
  for (size_t i = 0; i < mix_list.size(); ++i) {
    InterleavedView<const int16_t> frame_data = mix_list[i]->data_view();
    RTC_CHECK(!frame_data.empty());
    for (size_t j = 0; j < number_of_channels; ++j) {
      MonoView<float> channel = mixing_buffer[j];
      for (size_t k = 0; k < SamplesPerChannel(channel); ++k) {
        channel[k] += frame_data[number_of_channels * k + j];
      }
    }
  }
}

void RunLimiter(DeinterleavedView<float> deinterleaved, Limiter* limiter) {
  limiter->SetSamplesPerChannel(deinterleaved.samples_per_channel());
  limiter->Process(deinterleaved);
}

// Both interleaves and rounds.
void InterleaveToAudioFrame(DeinterleavedView<float> deinterleaved,
                            AudioFrame* audio_frame_for_mixing) {
  InterleavedView<int16_t> mixing_data = audio_frame_for_mixing->mutable_data(
      deinterleaved.samples_per_channel(), deinterleaved.num_channels());
  // Put data in the result frame.
  for (size_t i = 0; i < mixing_data.num_channels(); ++i) {
    auto channel = deinterleaved[i];
    for (size_t j = 0; j < mixing_data.samples_per_channel(); ++j) {
      mixing_data[mixing_data.num_channels() * j + i] =
          FloatS16ToS16(channel[j]);
    }
  }
}
}  // namespace

constexpr size_t FrameCombiner::kMaximumNumberOfChannels;
constexpr size_t FrameCombiner::kMaximumChannelSize;

FrameCombiner::FrameCombiner(bool use_limiter)
    : data_dumper_(new ApmDataDumper(0)),
      limiter_(data_dumper_.get(), kMaximumChannelSize, "AudioMixer"),
      use_limiter_(use_limiter) {
  static_assert(kMaximumChannelSize * kMaximumNumberOfChannels <=
                    AudioFrame::kMaxDataSizeSamples,
                "");
}

FrameCombiner::~FrameCombiner() = default;

void FrameCombiner::Combine(rtc::ArrayView<AudioFrame* const> mix_list,
                            size_t number_of_channels,
                            int sample_rate,
                            size_t number_of_streams,
                            AudioFrame* audio_frame_for_mixing) {
  RTC_DCHECK(audio_frame_for_mixing);
  RTC_DCHECK_GT(sample_rate, 0);

  // Note: `mix_list` is allowed to be empty.
  // See FrameCombiner.CombiningZeroFramesShouldProduceSilence.

  // Make sure to cap `number_of_channels` to the kMaximumNumberOfChannels
  // limits since processing from hereon out will be bound by them.
  number_of_channels = std::min(number_of_channels, kMaximumNumberOfChannels);

  SetAudioFrameFields(mix_list, number_of_channels, sample_rate,
                      number_of_streams, audio_frame_for_mixing);

  size_t samples_per_channel = SampleRateToDefaultChannelSize(sample_rate);

#if RTC_DCHECK_IS_ON
  for (const auto* frame : mix_list) {
    RTC_DCHECK_EQ(samples_per_channel, frame->samples_per_channel_);
    RTC_DCHECK_EQ(sample_rate, frame->sample_rate_hz_);
  }
#endif

  // The 'num_channels_' field of frames in 'mix_list' could be
  // different from 'number_of_channels'.
  for (auto* frame : mix_list) {
    RemixFrame(number_of_channels, frame);
  }

  if (number_of_streams <= 1) {
    MixFewFramesWithNoLimiter(mix_list, audio_frame_for_mixing);
    return;
  }

  // Make sure that the size of the view based on the desired
  // `samples_per_channel` and `number_of_channels` doesn't exceed the size of
  // the `mixing_buffer_` buffer.
  RTC_DCHECK_LE(samples_per_channel, kMaximumChannelSize);
  // Since the above check is a DCHECK only, clamp down on `samples_per_channel`
  // to make sure we don't exceed the buffer size in non-dcheck builds.
  // See also FrameCombinerDeathTest.DebugBuildCrashesWithHighRate.
  samples_per_channel = std::min(samples_per_channel, kMaximumChannelSize);
  DeinterleavedView<float> deinterleaved(
      mixing_buffer_.data(), samples_per_channel, number_of_channels);
  MixToFloatFrame(mix_list, deinterleaved);

  if (use_limiter_) {
    RunLimiter(deinterleaved, &limiter_);
  }

  InterleaveToAudioFrame(deinterleaved, audio_frame_for_mixing);
}

}  // namespace webrtc
