/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_AUDIO_TRANSPORT_PROXY_H_
#define AUDIO_AUDIO_TRANSPORT_PROXY_H_

#include "api/audio/audio_mixer.h"
#include "common_audio/resampler/include/push_resampler.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

class AudioTransportProxy : public AudioTransport {
 public:
  AudioTransportProxy(AudioTransport* voe_audio_transport,
                      AudioProcessing* audio_processing,
                      AudioMixer* mixer);

  ~AudioTransportProxy() override;

  int32_t RecordedDataIsAvailable(const void* audioSamples,
                                  const size_t nSamples,
                                  const size_t nBytesPerSample,
                                  const size_t nChannels,
                                  const uint32_t samplesPerSec,
                                  const uint32_t totalDelayMS,
                                  const int32_t clockDrift,
                                  const uint32_t currentMicLevel,
                                  const bool keyPressed,
                                  uint32_t& newMicLevel) override;

  int32_t NeedMorePlayData(const size_t nSamples,
                           const size_t nBytesPerSample,
                           const size_t nChannels,
                           const uint32_t samplesPerSec,
                           void* audioSamples,
                           size_t& nSamplesOut,
                           int64_t* elapsed_time_ms,
                           int64_t* ntp_time_ms) override;

  void PushCaptureData(int voe_channel,
                       const void* audio_data,
                       int bits_per_sample,
                       int sample_rate,
                       size_t number_of_channels,
                       size_t number_of_frames) override;

  void PullRenderData(int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames,
                      void* audio_data,
                      int64_t* elapsed_time_ms,
                      int64_t* ntp_time_ms) override;

 private:
  AudioTransport* voe_audio_transport_;
  AudioProcessing* audio_processing_;
  rtc::scoped_refptr<AudioMixer> mixer_;
  AudioFrame mixed_frame_;
  // Converts mixed audio to the audio device output rate.
  PushResampler<int16_t> resampler_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(AudioTransportProxy);
};
}  // namespace webrtc

#endif  // AUDIO_AUDIO_TRANSPORT_PROXY_H_
