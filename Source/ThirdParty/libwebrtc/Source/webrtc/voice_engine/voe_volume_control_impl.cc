/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_volume_control_impl.h"

#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoEVolumeControl* VoEVolumeControl::GetInterface(VoiceEngine* voiceEngine) {
  if (NULL == voiceEngine) {
    return NULL;
  }
  VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
  s->AddRef();
  return s;
}

VoEVolumeControlImpl::VoEVolumeControlImpl(voe::SharedData* shared)
    : _shared(shared) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoEVolumeControlImpl::VoEVolumeControlImpl() - ctor");
}

VoEVolumeControlImpl::~VoEVolumeControlImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoEVolumeControlImpl::~VoEVolumeControlImpl() - dtor");
}

int VoEVolumeControlImpl::SetSpeakerVolume(unsigned int volume) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetSpeakerVolume(volume=%u)", volume);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (volume > kMaxVolumeLevel) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetSpeakerVolume() invalid argument");
    return -1;
  }

  uint32_t maxVol(0);
  uint32_t spkrVol(0);

  // scale: [0,kMaxVolumeLevel] -> [0,MaxSpeakerVolume]
  if (_shared->audio_device()->MaxSpeakerVolume(&maxVol) != 0) {
    _shared->SetLastError(VE_MIC_VOL_ERROR, kTraceError,
                          "SetSpeakerVolume() failed to get max volume");
    return -1;
  }
  // Round the value and avoid floating computation.
  spkrVol = (uint32_t)((volume * maxVol + (int)(kMaxVolumeLevel / 2)) /
                       (kMaxVolumeLevel));

  // set the actual volume using the audio mixer
  if (_shared->audio_device()->SetSpeakerVolume(spkrVol) != 0) {
    _shared->SetLastError(VE_MIC_VOL_ERROR, kTraceError,
                          "SetSpeakerVolume() failed to set speaker volume");
    return -1;
  }
  return 0;
}

int VoEVolumeControlImpl::GetSpeakerVolume(unsigned int& volume) {

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  uint32_t spkrVol(0);
  uint32_t maxVol(0);

  if (_shared->audio_device()->SpeakerVolume(&spkrVol) != 0) {
    _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
                          "GetSpeakerVolume() unable to get speaker volume");
    return -1;
  }

  // scale: [0, MaxSpeakerVolume] -> [0, kMaxVolumeLevel]
  if (_shared->audio_device()->MaxSpeakerVolume(&maxVol) != 0) {
    _shared->SetLastError(
        VE_GET_MIC_VOL_ERROR, kTraceError,
        "GetSpeakerVolume() unable to get max speaker volume");
    return -1;
  }
  // Round the value and avoid floating computation.
  volume =
      (uint32_t)((spkrVol * kMaxVolumeLevel + (int)(maxVol / 2)) / (maxVol));

  return 0;
}

int VoEVolumeControlImpl::SetMicVolume(unsigned int volume) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetMicVolume(volume=%u)", volume);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (volume > kMaxVolumeLevel) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetMicVolume() invalid argument");
    return -1;
  }

  uint32_t maxVol(0);
  uint32_t micVol(0);

  // scale: [0, kMaxVolumeLevel] -> [0,MaxMicrophoneVolume]
  if (_shared->audio_device()->MaxMicrophoneVolume(&maxVol) != 0) {
    _shared->SetLastError(VE_MIC_VOL_ERROR, kTraceError,
                          "SetMicVolume() failed to get max volume");
    return -1;
  }

  if (volume == kMaxVolumeLevel) {
    // On Linux running pulse, users are able to set the volume above 100%
    // through the volume control panel, where the +100% range is digital
    // scaling. WebRTC does not support setting the volume above 100%, and
    // simply ignores changing the volume if the user tries to set it to
    // |kMaxVolumeLevel| while the current volume is higher than |maxVol|.
    if (_shared->audio_device()->MicrophoneVolume(&micVol) != 0) {
      _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
                            "SetMicVolume() unable to get microphone volume");
      return -1;
    }
    if (micVol >= maxVol)
      return 0;
  }

  // Round the value and avoid floating point computation.
  micVol = (uint32_t)((volume * maxVol + (int)(kMaxVolumeLevel / 2)) /
                      (kMaxVolumeLevel));

  // set the actual volume using the audio mixer
  if (_shared->audio_device()->SetMicrophoneVolume(micVol) != 0) {
    _shared->SetLastError(VE_MIC_VOL_ERROR, kTraceError,
                          "SetMicVolume() failed to set mic volume");
    return -1;
  }
  return 0;
}

int VoEVolumeControlImpl::GetMicVolume(unsigned int& volume) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  uint32_t micVol(0);
  uint32_t maxVol(0);

  if (_shared->audio_device()->MicrophoneVolume(&micVol) != 0) {
    _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
                          "GetMicVolume() unable to get microphone volume");
    return -1;
  }

  // scale: [0, MaxMicrophoneVolume] -> [0, kMaxVolumeLevel]
  if (_shared->audio_device()->MaxMicrophoneVolume(&maxVol) != 0) {
    _shared->SetLastError(VE_GET_MIC_VOL_ERROR, kTraceError,
                          "GetMicVolume() unable to get max microphone volume");
    return -1;
  }
  if (micVol < maxVol) {
    // Round the value and avoid floating point calculation.
    volume =
        (uint32_t)((micVol * kMaxVolumeLevel + (int)(maxVol / 2)) / (maxVol));
  } else {
    // Truncate the value to the kMaxVolumeLevel.
    volume = kMaxVolumeLevel;
  }
  return 0;
}

int VoEVolumeControlImpl::SetInputMute(int channel, bool enable) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetInputMute(channel=%d, enable=%d)", channel, enable);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    // Mute before demultiplexing <=> affects all channels
    return _shared->transmit_mixer()->SetMute(enable);
  }
  // Mute after demultiplexing <=> affects one channel only
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetInputMute() failed to locate channel");
    return -1;
  }
  return channelPtr->SetInputMute(enable);
}

int VoEVolumeControlImpl::GetInputMute(int channel, bool& enabled) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    enabled = _shared->transmit_mixer()->Mute();
  } else {
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                            "SetInputMute() failed to locate channel");
      return -1;
    }
    enabled = channelPtr->InputMute();
  }
  return 0;
}

int VoEVolumeControlImpl::GetSpeechInputLevel(unsigned int& level) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  int8_t currentLevel = _shared->transmit_mixer()->AudioLevel();
  level = static_cast<unsigned int>(currentLevel);
  return 0;
}

int VoEVolumeControlImpl::GetSpeechOutputLevel(int channel,
                                               unsigned int& level) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    return _shared->output_mixer()->GetSpeechOutputLevel((uint32_t&)level);
  } else {
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                            "GetSpeechOutputLevel() failed to locate channel");
      return -1;
    }
    channelPtr->GetSpeechOutputLevel((uint32_t&)level);
  }
  return 0;
}

int VoEVolumeControlImpl::GetSpeechInputLevelFullRange(unsigned int& level) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  int16_t currentLevel = _shared->transmit_mixer()->AudioLevelFullRange();
  level = static_cast<unsigned int>(currentLevel);
  return 0;
}

int VoEVolumeControlImpl::GetSpeechOutputLevelFullRange(int channel,
                                                        unsigned int& level) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    return _shared->output_mixer()->GetSpeechOutputLevelFullRange(
        (uint32_t&)level);
  } else {
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(
          VE_CHANNEL_NOT_VALID, kTraceError,
          "GetSpeechOutputLevelFullRange() failed to locate channel");
      return -1;
    }
    channelPtr->GetSpeechOutputLevelFullRange((uint32_t&)level);
  }
  return 0;
}

int VoEVolumeControlImpl::SetChannelOutputVolumeScaling(int channel,
                                                        float scaling) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetChannelOutputVolumeScaling(channel=%d, scaling=%3.2f)",
               channel, scaling);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (scaling < kMinOutputVolumeScaling || scaling > kMaxOutputVolumeScaling) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetChannelOutputVolumeScaling() invalid parameter");
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(
        VE_CHANNEL_NOT_VALID, kTraceError,
        "SetChannelOutputVolumeScaling() failed to locate channel");
    return -1;
  }
  return channelPtr->SetChannelOutputVolumeScaling(scaling);
}

int VoEVolumeControlImpl::GetChannelOutputVolumeScaling(int channel,
                                                        float& scaling) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(
        VE_CHANNEL_NOT_VALID, kTraceError,
        "GetChannelOutputVolumeScaling() failed to locate channel");
    return -1;
  }
  return channelPtr->GetChannelOutputVolumeScaling(scaling);
}

int VoEVolumeControlImpl::SetOutputVolumePan(int channel,
                                             float left,
                                             float right) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "SetOutputVolumePan(channel=%d, left=%2.1f, right=%2.1f)",
               channel, left, right);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  bool available(false);
  _shared->audio_device()->StereoPlayoutIsAvailable(&available);
  if (!available) {
    _shared->SetLastError(VE_FUNC_NO_STEREO, kTraceError,
                          "SetOutputVolumePan() stereo playout not supported");
    return -1;
  }
  if ((left < kMinOutputVolumePanning) || (left > kMaxOutputVolumePanning) ||
      (right < kMinOutputVolumePanning) || (right > kMaxOutputVolumePanning)) {
    _shared->SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                          "SetOutputVolumePan() invalid parameter");
    return -1;
  }

  if (channel == -1) {
    // Master balance (affectes the signal after output mixing)
    return _shared->output_mixer()->SetOutputVolumePan(left, right);
  }
  // Per-channel balance (affects the signal before output mixing)
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetOutputVolumePan() failed to locate channel");
    return -1;
  }
  return channelPtr->SetOutputVolumePan(left, right);
}

int VoEVolumeControlImpl::GetOutputVolumePan(int channel,
                                             float& left,
                                             float& right) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  bool available(false);
  _shared->audio_device()->StereoPlayoutIsAvailable(&available);
  if (!available) {
    _shared->SetLastError(VE_FUNC_NO_STEREO, kTraceError,
                          "GetOutputVolumePan() stereo playout not supported");
    return -1;
  }

  if (channel == -1) {
    return _shared->output_mixer()->GetOutputVolumePan(left, right);
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetOutputVolumePan() failed to locate channel");
    return -1;
  }
  return channelPtr->GetOutputVolumePan(left, right);
}

}  // namespace webrtc
