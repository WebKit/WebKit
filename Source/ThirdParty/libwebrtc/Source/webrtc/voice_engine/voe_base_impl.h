/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_BASE_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_BASE_IMPL_H

#include "webrtc/voice_engine/include/voe_base.h"

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class ProcessThread;

class VoEBaseImpl : public VoEBase,
                    public AudioTransport,
                    public AudioDeviceObserver {
 public:
  int RegisterVoiceEngineObserver(VoiceEngineObserver& observer) override;
  int DeRegisterVoiceEngineObserver() override;

  int Init(AudioDeviceModule* external_adm = nullptr,
           AudioProcessing* audioproc = nullptr,
           const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory =
               nullptr) override;
  AudioProcessing* audio_processing() override {
    return shared_->audio_processing();
  }
  AudioDeviceModule* audio_device_module() override {
    return shared_->audio_device();
  }
  voe::TransmitMixer* transmit_mixer() override {
    return shared_->transmit_mixer();
  }
  int Terminate() override;

  int CreateChannel() override;
  int CreateChannel(const ChannelConfig& config) override;
  int DeleteChannel(int channel) override;

  int StartReceive(int channel) override;
  int StartPlayout(int channel) override;
  int StartSend(int channel) override;
  int StopPlayout(int channel) override;
  int StopSend(int channel) override;

  int GetVersion(char version[1024]) override;

  int LastError() override;

  AudioTransport* audio_transport() override { return this; }

  int AssociateSendChannel(int channel, int accociate_send_channel) override;

  // AudioTransport
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

  // AudioDeviceObserver
  void OnErrorIsReported(const ErrorCode error) override;
  void OnWarningIsReported(const WarningCode warning) override;

 protected:
  VoEBaseImpl(voe::SharedData* shared);
  ~VoEBaseImpl() override;

 private:
  int32_t StartPlayout();
  int32_t StopPlayout();
  int32_t StartSend();
  int32_t StopSend();
  int32_t TerminateInternal();

  // Helper function to process the recorded data with AudioProcessing Module,
  // demultiplex the data to specific voe channels, encode and send to the
  // network. When |number_of_VoE_channels| is 0, it will demultiplex the
  // data to all the existing VoE channels.
  // It returns new AGC microphone volume or 0 if no volume changes
  // should be done.
  int ProcessRecordedDataWithAPM(
      const int voe_channels[], size_t number_of_voe_channels,
      const void* audio_data, uint32_t sample_rate, size_t number_of_channels,
      size_t number_of_frames, uint32_t audio_delay_milliseconds,
      int32_t clock_drift, uint32_t volume, bool key_pressed);

  void GetPlayoutData(int sample_rate, size_t number_of_channels,
                      size_t number_of_frames, bool feed_data_to_apm,
                      void* audio_data, int64_t* elapsed_time_ms,
                      int64_t* ntp_time_ms);

  // Initialize channel by setting Engine Information then initializing
  // channel.
  int InitializeChannel(voe::ChannelOwner* channel_owner);
  VoiceEngineObserver* voiceEngineObserverPtr_;
  rtc::CriticalSection callbackCritSect_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;

  AudioFrame audioFrame_;
  voe::SharedData* shared_;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_BASE_IMPL_H
