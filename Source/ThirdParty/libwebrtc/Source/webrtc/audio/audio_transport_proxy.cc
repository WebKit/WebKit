/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_transport_proxy.h"

namespace webrtc {

namespace {
// Resample audio in |frame| to given sample rate preserving the
// channel count and place the result in |destination|.
int Resample(const AudioFrame& frame,
             const int destination_sample_rate,
             PushResampler<int16_t>* resampler,
             int16_t* destination) {
  const int number_of_channels = static_cast<int>(frame.num_channels_);
  const int target_number_of_samples_per_channel =
      destination_sample_rate / 100;
  resampler->InitializeIfNeeded(frame.sample_rate_hz_, destination_sample_rate,
                                number_of_channels);

  // TODO(yujo): make resampler take an AudioFrame, and add special case
  // handling of muted frames.
  return resampler->Resample(
      frame.data(), frame.samples_per_channel_ * number_of_channels,
      destination, number_of_channels * target_number_of_samples_per_channel);
}
}  // namespace

AudioTransportProxy::AudioTransportProxy(AudioTransport* voe_audio_transport,
                                         AudioProcessing* apm,
                                         AudioMixer* mixer)
    : voe_audio_transport_(voe_audio_transport), apm_(apm), mixer_(mixer) {
  RTC_DCHECK(voe_audio_transport);
  RTC_DCHECK(apm);
  RTC_DCHECK(mixer);
}

AudioTransportProxy::~AudioTransportProxy() {}

int32_t AudioTransportProxy::RecordedDataIsAvailable(
    const void* audioSamples,
    const size_t nSamples,
    const size_t nBytesPerSample,
    const size_t nChannels,
    const uint32_t samplesPerSec,
    const uint32_t totalDelayMS,
    const int32_t clockDrift,
    const uint32_t currentMicLevel,
    const bool keyPressed,
    uint32_t& newMicLevel) {  // NOLINT: to avoid changing APIs
  // Pass call through to original audio transport instance.
  return voe_audio_transport_->RecordedDataIsAvailable(
      audioSamples, nSamples, nBytesPerSample, nChannels, samplesPerSec,
      totalDelayMS, clockDrift, currentMicLevel, keyPressed, newMicLevel);
}

int32_t AudioTransportProxy::NeedMorePlayData(const size_t nSamples,
                                              const size_t nBytesPerSample,
                                              const size_t nChannels,
                                              const uint32_t samplesPerSec,
                                              void* audioSamples,
                                              size_t& nSamplesOut,
                                              int64_t* elapsed_time_ms,
                                              int64_t* ntp_time_ms) {
  RTC_DCHECK_EQ(sizeof(int16_t) * nChannels, nBytesPerSample);
  RTC_DCHECK_GE(nChannels, 1);
  RTC_DCHECK_LE(nChannels, 2);
  RTC_DCHECK_GE(
      samplesPerSec,
      static_cast<uint32_t>(AudioProcessing::NativeRate::kSampleRate8kHz));

  // 100 = 1 second / data duration (10 ms).
  RTC_DCHECK_EQ(nSamples * 100, samplesPerSec);
  RTC_DCHECK_LE(nBytesPerSample * nSamples * nChannels,
                AudioFrame::kMaxDataSizeBytes);

  mixer_->Mix(nChannels, &mixed_frame_);
  *elapsed_time_ms = mixed_frame_.elapsed_time_ms_;
  *ntp_time_ms = mixed_frame_.ntp_time_ms_;

  const auto error = apm_->ProcessReverseStream(&mixed_frame_);
  RTC_DCHECK_EQ(error, AudioProcessing::kNoError);

  nSamplesOut = Resample(mixed_frame_, samplesPerSec, &resampler_,
                         static_cast<int16_t*>(audioSamples));
  RTC_DCHECK_EQ(nSamplesOut, nChannels * nSamples);
  return 0;
}

void AudioTransportProxy::PushCaptureData(int voe_channel,
                                          const void* audio_data,
                                          int bits_per_sample,
                                          int sample_rate,
                                          size_t number_of_channels,
                                          size_t number_of_frames) {
  // This is part of deprecated VoE interface operating on specific
  // VoE channels. It should not be used.
  RTC_NOTREACHED();
}

void AudioTransportProxy::PullRenderData(int bits_per_sample,
                                         int sample_rate,
                                         size_t number_of_channels,
                                         size_t number_of_frames,
                                         void* audio_data,
                                         int64_t* elapsed_time_ms,
                                         int64_t* ntp_time_ms) {
  RTC_DCHECK_EQ(bits_per_sample, 16);
  RTC_DCHECK_GE(number_of_channels, 1);
  RTC_DCHECK_LE(number_of_channels, 2);
  RTC_DCHECK_GE(sample_rate, AudioProcessing::NativeRate::kSampleRate8kHz);

  // 100 = 1 second / data duration (10 ms).
  RTC_DCHECK_EQ(number_of_frames * 100, sample_rate);

  // 8 = bits per byte.
  RTC_DCHECK_LE(bits_per_sample / 8 * number_of_frames * number_of_channels,
                AudioFrame::kMaxDataSizeBytes);
  mixer_->Mix(number_of_channels, &mixed_frame_);
  *elapsed_time_ms = mixed_frame_.elapsed_time_ms_;
  *ntp_time_ms = mixed_frame_.ntp_time_ms_;

  const auto output_samples = Resample(mixed_frame_, sample_rate, &resampler_,
                                       static_cast<int16_t*>(audio_data));
  RTC_DCHECK_EQ(output_samples, number_of_channels * number_of_frames);
}

}  // namespace webrtc
