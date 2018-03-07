/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/voe_base_impl.h"

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_device/audio_device_impl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "voice_engine/channel.h"
#include "voice_engine/include/voe_errors.h"
#include "voice_engine/transmit_mixer.h"
#include "voice_engine/voice_engine_impl.h"

namespace webrtc {

VoEBase* VoEBase::GetInterface(VoiceEngine* voiceEngine) {
  if (nullptr == voiceEngine) {
    return nullptr;
  }
  VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
  s->AddRef();
  return s;
}

VoEBaseImpl::VoEBaseImpl(voe::SharedData* shared)
    : shared_(shared) {}

VoEBaseImpl::~VoEBaseImpl() {
  TerminateInternal();
}

int32_t VoEBaseImpl::RecordedDataIsAvailable(
    const void* audio_data,
    const size_t number_of_frames,
    const size_t bytes_per_sample,
    const size_t number_of_channels,
    const uint32_t sample_rate,
    const uint32_t audio_delay_milliseconds,
    const int32_t clock_drift,
    const uint32_t volume,
    const bool key_pressed,
    uint32_t& new_mic_volume) {
  RTC_DCHECK_EQ(2 * number_of_channels, bytes_per_sample);
  RTC_DCHECK(shared_->transmit_mixer() != nullptr);
  RTC_DCHECK(shared_->audio_device() != nullptr);

  constexpr uint32_t kMaxVolumeLevel = 255;

  uint32_t max_volume = 0;
  uint16_t voe_mic_level = 0;
  // Check for zero to skip this calculation; the consumer may use this to
  // indicate no volume is available.
  if (volume != 0) {
    // Scale from ADM to VoE level range
    if (shared_->audio_device()->MaxMicrophoneVolume(&max_volume) == 0) {
      if (max_volume) {
        voe_mic_level = static_cast<uint16_t>(
            (volume * kMaxVolumeLevel + static_cast<int>(max_volume / 2)) /
            max_volume);
      }
    }
    // We learned that on certain systems (e.g Linux) the voe_mic_level
    // can be greater than the maxVolumeLevel therefore
    // we are going to cap the voe_mic_level to the maxVolumeLevel
    // and change the maxVolume to volume if it turns out that
    // the voe_mic_level is indeed greater than the maxVolumeLevel.
    if (voe_mic_level > kMaxVolumeLevel) {
      voe_mic_level = kMaxVolumeLevel;
      max_volume = volume;
    }
  }

  // Perform channel-independent operations
  // (APM, mix with file, record to file, mute, etc.)
  shared_->transmit_mixer()->PrepareDemux(
      audio_data, number_of_frames, number_of_channels, sample_rate,
      static_cast<uint16_t>(audio_delay_milliseconds), clock_drift,
      voe_mic_level, key_pressed);

  // Copy the audio frame to each sending channel and perform
  // channel-dependent operations (file mixing, mute, etc.), encode and
  // packetize+transmit the RTP packet.
  shared_->transmit_mixer()->ProcessAndEncodeAudio();

  // Scale from VoE to ADM level range.
  uint32_t new_voe_mic_level = shared_->transmit_mixer()->CaptureLevel();
  if (new_voe_mic_level != voe_mic_level) {
    // Return the new volume if AGC has changed the volume.
    return static_cast<int>((new_voe_mic_level * max_volume +
                             static_cast<int>(kMaxVolumeLevel / 2)) /
                            kMaxVolumeLevel);
  }

  return 0;
}

int32_t VoEBaseImpl::NeedMorePlayData(const size_t nSamples,
                                      const size_t nBytesPerSample,
                                      const size_t nChannels,
                                      const uint32_t samplesPerSec,
                                      void* audioSamples,
                                      size_t& nSamplesOut,
                                      int64_t* elapsed_time_ms,
                                      int64_t* ntp_time_ms) {
  RTC_NOTREACHED();
  return 0;
}

void VoEBaseImpl::PushCaptureData(int voe_channel, const void* audio_data,
                                  int bits_per_sample, int sample_rate,
                                  size_t number_of_channels,
                                  size_t number_of_frames) {
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(voe_channel);
  voe::Channel* channel = ch.channel();
  if (!channel)
    return;
  if (channel->Sending()) {
    // Send the audio to each channel directly without using the APM in the
    // transmit mixer.
    channel->ProcessAndEncodeAudio(static_cast<const int16_t*>(audio_data),
                                   sample_rate, number_of_frames,
                                   number_of_channels);
  }
}

void VoEBaseImpl::PullRenderData(int bits_per_sample,
                                 int sample_rate,
                                 size_t number_of_channels,
                                 size_t number_of_frames,
                                 void* audio_data, int64_t* elapsed_time_ms,
                                 int64_t* ntp_time_ms) {
  RTC_NOTREACHED();
}

int VoEBaseImpl::Init(
    AudioDeviceModule* audio_device,
    AudioProcessing* audio_processing,
    const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory) {
  RTC_DCHECK(audio_device);
  RTC_DCHECK(audio_processing);
  rtc::CritScope cs(shared_->crit_sec());
  if (shared_->process_thread()) {
    shared_->process_thread()->Start();
  }

  shared_->set_audio_device(audio_device);
  shared_->set_audio_processing(audio_processing);

  RTC_DCHECK(decoder_factory);
  decoder_factory_ = decoder_factory;

  return 0;
}

void VoEBaseImpl::Terminate() {
  rtc::CritScope cs(shared_->crit_sec());
  TerminateInternal();
}

int VoEBaseImpl::CreateChannel() {
  return CreateChannel(ChannelConfig());
}

int VoEBaseImpl::CreateChannel(const ChannelConfig& config) {
  rtc::CritScope cs(shared_->crit_sec());
  ChannelConfig config_copy(config);
  config_copy.acm_config.decoder_factory = decoder_factory_;
  voe::ChannelOwner channel_owner =
      shared_->channel_manager().CreateChannel(config_copy);
  return InitializeChannel(&channel_owner);
}

int VoEBaseImpl::InitializeChannel(voe::ChannelOwner* channel_owner) {
  if (channel_owner->channel()->SetEngineInformation(
          *shared_->process_thread(), *shared_->audio_device(),
          shared_->encoder_queue()) != 0) {
    RTC_LOG(LS_ERROR)
        << "CreateChannel() failed to associate engine and channel."
           " Destroying channel.";
    shared_->channel_manager().DestroyChannel(
        channel_owner->channel()->ChannelId());
    return -1;
  } else if (channel_owner->channel()->Init() != 0) {
    RTC_LOG(LS_ERROR)
        << "CreateChannel() failed to initialize channel. Destroying"
           " channel.";
    shared_->channel_manager().DestroyChannel(
        channel_owner->channel()->ChannelId());
    return -1;
  }
  return channel_owner->channel()->ChannelId();
}

int VoEBaseImpl::DeleteChannel(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  {
    voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == nullptr) {
      RTC_LOG(LS_ERROR) << "DeleteChannel() failed to locate channel";
      return -1;
    }
  }

  shared_->channel_manager().DestroyChannel(channel);
  if (StopSend() != 0) {
    return -1;
  }
  if (StopPlayout() != 0) {
    return -1;
  }
  return 0;
}

int VoEBaseImpl::StartPlayout(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == nullptr) {
    RTC_LOG(LS_ERROR) << "StartPlayout() failed to locate channel";
    return -1;
  }
  if (channelPtr->Playing()) {
    return 0;
  }
  if (StartPlayout() != 0) {
    RTC_LOG(LS_ERROR) << "StartPlayout() failed to start playout";
    return -1;
  }
  return channelPtr->StartPlayout();
}

int VoEBaseImpl::StopPlayout(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == nullptr) {
    RTC_LOG(LS_ERROR) << "StopPlayout() failed to locate channel";
    return -1;
  }
  if (channelPtr->StopPlayout() != 0) {
    RTC_LOG_F(LS_WARNING) << "StopPlayout() failed to stop playout for channel "
                          << channel;
  }
  return StopPlayout();
}

int VoEBaseImpl::StartSend(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == nullptr) {
    RTC_LOG(LS_ERROR) << "StartSend() failed to locate channel";
    return -1;
  }
  if (channelPtr->Sending()) {
    return 0;
  }
  if (StartSend() != 0) {
    RTC_LOG(LS_ERROR) << "StartSend() failed to start recording";
    return -1;
  }
  return channelPtr->StartSend();
}

int VoEBaseImpl::StopSend(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == nullptr) {
    RTC_LOG(LS_ERROR) << "StopSend() failed to locate channel";
    return -1;
  }
  channelPtr->StopSend();
  return StopSend();
}

int32_t VoEBaseImpl::StartPlayout() {
  if (!shared_->audio_device()->Playing()) {
    if (shared_->audio_device()->InitPlayout() != 0) {
      RTC_LOG_F(LS_ERROR) << "Failed to initialize playout";
      return -1;
    }
    if (playout_enabled_ && shared_->audio_device()->StartPlayout() != 0) {
      RTC_LOG_F(LS_ERROR) << "Failed to start playout";
      return -1;
    }
  }
  return 0;
}

int32_t VoEBaseImpl::StopPlayout() {
  if (!playout_enabled_) {
    return 0;
  }
  // Stop audio-device playing if no channel is playing out.
  if (shared_->NumOfPlayingChannels() == 0) {
    if (shared_->audio_device()->StopPlayout() != 0) {
      RTC_LOG(LS_ERROR) << "StopPlayout() failed to stop playout";
      return -1;
    }
  }
  return 0;
}

int32_t VoEBaseImpl::StartSend() {
  if (!shared_->audio_device()->Recording()) {
    if (shared_->audio_device()->InitRecording() != 0) {
      RTC_LOG_F(LS_ERROR) << "Failed to initialize recording";
      return -1;
    }
    if (recording_enabled_ && shared_->audio_device()->StartRecording() != 0) {
      RTC_LOG_F(LS_ERROR) << "Failed to start recording";
      return -1;
    }
  }
  return 0;
}

int32_t VoEBaseImpl::StopSend() {
  if (!recording_enabled_) {
    return 0;
  }
  // Stop audio-device recording if no channel is recording.
  if (shared_->NumOfSendingChannels() == 0) {
    if (shared_->audio_device()->StopRecording() != 0) {
      RTC_LOG(LS_ERROR) << "StopSend() failed to stop recording";
      return -1;
    }
    shared_->transmit_mixer()->StopSend();
  }

  return 0;
}

int32_t VoEBaseImpl::SetPlayout(bool enabled) {
  RTC_LOG(INFO) << "SetPlayout(" << enabled << ")";
  if (playout_enabled_ == enabled) {
    return 0;
  }
  playout_enabled_ = enabled;
  if (shared_->NumOfPlayingChannels() == 0) {
    // If there are no channels attempting to play out yet, there's nothing to
    // be done; we should be in a "not playing out" state either way.
    return 0;
  }
  int32_t ret;
  if (enabled) {
    ret = shared_->audio_device()->StartPlayout();
    if (ret != 0) {
      RTC_LOG(LS_ERROR) << "SetPlayout(true) failed to start playout";
    }
  } else {
    ret = shared_->audio_device()->StopPlayout();
    if (ret != 0) {
      RTC_LOG(LS_ERROR) << "SetPlayout(false) failed to stop playout";
    }
  }
  return ret;
}

int32_t VoEBaseImpl::SetRecording(bool enabled) {
  RTC_LOG(INFO) << "SetRecording(" << enabled << ")";
  if (recording_enabled_ == enabled) {
    return 0;
  }
  recording_enabled_ = enabled;
  if (shared_->NumOfSendingChannels() == 0) {
    // If there are no channels attempting to record out yet, there's nothing to
    // be done; we should be in a "not recording" state either way.
    return 0;
  }
  int32_t ret;
  if (enabled) {
    ret = shared_->audio_device()->StartRecording();
    if (ret != 0) {
      RTC_LOG(LS_ERROR) << "SetRecording(true) failed to start recording";
    }
  } else {
    ret = shared_->audio_device()->StopRecording();
    if (ret != 0) {
      RTC_LOG(LS_ERROR) << "SetRecording(false) failed to stop recording";
    }
  }
  return ret;
}

void VoEBaseImpl::TerminateInternal() {
  // Delete any remaining channel objects
  shared_->channel_manager().DestroyAllChannels();

  if (shared_->process_thread()) {
    shared_->process_thread()->Stop();
  }

  shared_->set_audio_device(nullptr);
  shared_->set_audio_processing(nullptr);
}
}  // namespace webrtc
