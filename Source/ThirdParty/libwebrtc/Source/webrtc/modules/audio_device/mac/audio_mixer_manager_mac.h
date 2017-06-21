/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_MAC_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_MAC_H

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/typedefs.h"

#include <CoreAudio/CoreAudio.h>

namespace webrtc {

class AudioMixerManagerMac {
 public:
  int32_t OpenSpeaker(AudioDeviceID deviceID);
  int32_t OpenMicrophone(AudioDeviceID deviceID);
  int32_t SetSpeakerVolume(uint32_t volume);
  int32_t SpeakerVolume(uint32_t& volume) const;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const;
  int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const;
  int32_t SpeakerVolumeIsAvailable(bool& available);
  int32_t SpeakerMuteIsAvailable(bool& available);
  int32_t SetSpeakerMute(bool enable);
  int32_t SpeakerMute(bool& enabled) const;
  int32_t StereoPlayoutIsAvailable(bool& available);
  int32_t StereoRecordingIsAvailable(bool& available);
  int32_t MicrophoneMuteIsAvailable(bool& available);
  int32_t SetMicrophoneMute(bool enable);
  int32_t MicrophoneMute(bool& enabled) const;
  int32_t MicrophoneBoostIsAvailable(bool& available);
  int32_t SetMicrophoneBoost(bool enable);
  int32_t MicrophoneBoost(bool& enabled) const;
  int32_t MicrophoneVolumeIsAvailable(bool& available);
  int32_t SetMicrophoneVolume(uint32_t volume);
  int32_t MicrophoneVolume(uint32_t& volume) const;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const;
  int32_t MicrophoneVolumeStepSize(uint16_t& stepSize) const;
  int32_t Close();
  int32_t CloseSpeaker();
  int32_t CloseMicrophone();
  bool SpeakerIsInitialized() const;
  bool MicrophoneIsInitialized() const;

 public:
  AudioMixerManagerMac(const int32_t id);
  ~AudioMixerManagerMac();

 private:
  static void logCAMsg(const TraceLevel level,
                       const TraceModule module,
                       const int32_t id,
                       const char* msg,
                       const char* err);

 private:
  rtc::CriticalSection _critSect;
  int32_t _id;

  AudioDeviceID _inputDeviceID;
  AudioDeviceID _outputDeviceID;

  uint16_t _noInputChannels;
  uint16_t _noOutputChannels;
};

}  // namespace webrtc

#endif  // AUDIO_MIXER_MAC_H
