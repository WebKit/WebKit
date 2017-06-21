/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/mac/audio_mixer_manager_mac.h"

#include <unistd.h>  // getpid()

namespace webrtc {

#define WEBRTC_CA_RETURN_ON_ERR(expr)                                  \
  do {                                                                 \
    err = expr;                                                        \
    if (err != noErr) {                                                \
      logCAMsg(kTraceError, kTraceAudioDevice, _id, "Error in " #expr, \
               (const char*) & err);                                   \
      return -1;                                                       \
    }                                                                  \
  } while (0)

#define WEBRTC_CA_LOG_ERR(expr)                                        \
  do {                                                                 \
    err = expr;                                                        \
    if (err != noErr) {                                                \
      logCAMsg(kTraceError, kTraceAudioDevice, _id, "Error in " #expr, \
               (const char*) & err);                                   \
    }                                                                  \
  } while (0)

#define WEBRTC_CA_LOG_WARN(expr)                                         \
  do {                                                                   \
    err = expr;                                                          \
    if (err != noErr) {                                                  \
      logCAMsg(kTraceWarning, kTraceAudioDevice, _id, "Error in " #expr, \
               (const char*) & err);                                     \
    }                                                                    \
  } while (0)

AudioMixerManagerMac::AudioMixerManagerMac(const int32_t id)
    : _id(id),
      _inputDeviceID(kAudioObjectUnknown),
      _outputDeviceID(kAudioObjectUnknown),
      _noInputChannels(0),
      _noOutputChannels(0) {
  WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s constructed",
               __FUNCTION__);
}

AudioMixerManagerMac::~AudioMixerManagerMac() {
  WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destructed",
               __FUNCTION__);
  Close();
}

// ============================================================================
//	                                PUBLIC METHODS
// ============================================================================

int32_t AudioMixerManagerMac::Close() {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  rtc::CritScope lock(&_critSect);

  CloseSpeaker();
  CloseMicrophone();

  return 0;
}

int32_t AudioMixerManagerMac::CloseSpeaker() {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  rtc::CritScope lock(&_critSect);

  _outputDeviceID = kAudioObjectUnknown;
  _noOutputChannels = 0;

  return 0;
}

int32_t AudioMixerManagerMac::CloseMicrophone() {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  rtc::CritScope lock(&_critSect);

  _inputDeviceID = kAudioObjectUnknown;
  _noInputChannels = 0;

  return 0;
}

int32_t AudioMixerManagerMac::OpenSpeaker(AudioDeviceID deviceID) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "AudioMixerManagerMac::OpenSpeaker(id=%d)", deviceID);

  rtc::CritScope lock(&_critSect);

  OSStatus err = noErr;
  UInt32 size = 0;
  pid_t hogPid = -1;

  _outputDeviceID = deviceID;

  // Check which process, if any, has hogged the device.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyHogMode, kAudioDevicePropertyScopeOutput, 0};

  size = sizeof(hogPid);
  WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
      _outputDeviceID, &propertyAddress, 0, NULL, &size, &hogPid));

  if (hogPid == -1) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 " No process has hogged the input device");
  }
  // getpid() is apparently "always successful"
  else if (hogPid == getpid()) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 " Our process has hogged the input device");
  } else {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 " Another process (pid = %d) has hogged the input device",
                 static_cast<int>(hogPid));

    return -1;
  }

  // get number of channels from stream format
  propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;

  // Get the stream format, to be able to read the number of channels.
  AudioStreamBasicDescription streamFormat;
  size = sizeof(AudioStreamBasicDescription);
  memset(&streamFormat, 0, size);
  WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
      _outputDeviceID, &propertyAddress, 0, NULL, &size, &streamFormat));

  _noOutputChannels = streamFormat.mChannelsPerFrame;

  return 0;
}

int32_t AudioMixerManagerMac::OpenMicrophone(AudioDeviceID deviceID) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "AudioMixerManagerMac::OpenMicrophone(id=%d)", deviceID);

  rtc::CritScope lock(&_critSect);

  OSStatus err = noErr;
  UInt32 size = 0;
  pid_t hogPid = -1;

  _inputDeviceID = deviceID;

  // Check which process, if any, has hogged the device.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyHogMode, kAudioDevicePropertyScopeInput, 0};
  size = sizeof(hogPid);
  WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
      _inputDeviceID, &propertyAddress, 0, NULL, &size, &hogPid));
  if (hogPid == -1) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 " No process has hogged the input device");
  }
  // getpid() is apparently "always successful"
  else if (hogPid == getpid()) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 " Our process has hogged the input device");
  } else {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 " Another process (pid = %d) has hogged the input device",
                 static_cast<int>(hogPid));

    return -1;
  }

  // get number of channels from stream format
  propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;

  // Get the stream format, to be able to read the number of channels.
  AudioStreamBasicDescription streamFormat;
  size = sizeof(AudioStreamBasicDescription);
  memset(&streamFormat, 0, size);
  WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
      _inputDeviceID, &propertyAddress, 0, NULL, &size, &streamFormat));

  _noInputChannels = streamFormat.mChannelsPerFrame;

  return 0;
}

bool AudioMixerManagerMac::SpeakerIsInitialized() const {
  WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  return (_outputDeviceID != kAudioObjectUnknown);
}

bool AudioMixerManagerMac::MicrophoneIsInitialized() const {
  WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

  return (_inputDeviceID != kAudioObjectUnknown);
}

int32_t AudioMixerManagerMac::SetSpeakerVolume(uint32_t volume) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "AudioMixerManagerMac::SetSpeakerVolume(volume=%u)", volume);

  rtc::CritScope lock(&_critSect);

  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;
  UInt32 size = 0;
  bool success = false;

  // volume range is 0.0 - 1.0, convert from 0 -255
  const Float32 vol = (Float32)(volume / 255.0);

  assert(vol <= 1.0 && vol >= 0.0);

  // Does the capture device have a master volume control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, 0};
  Boolean isSettable = false;
  err = AudioObjectIsPropertySettable(_outputDeviceID, &propertyAddress,
                                      &isSettable);
  if (err == noErr && isSettable) {
    size = sizeof(vol);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectSetPropertyData(
        _outputDeviceID, &propertyAddress, 0, NULL, size, &vol));

    return 0;
  }

  // Otherwise try to set each channel.
  for (UInt32 i = 1; i <= _noOutputChannels; i++) {
    propertyAddress.mElement = i;
    isSettable = false;
    err = AudioObjectIsPropertySettable(_outputDeviceID, &propertyAddress,
                                        &isSettable);
    if (err == noErr && isSettable) {
      size = sizeof(vol);
      WEBRTC_CA_RETURN_ON_ERR(AudioObjectSetPropertyData(
          _outputDeviceID, &propertyAddress, 0, NULL, size, &vol));
    }
    success = true;
  }

  if (!success) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 " Unable to set a volume on any output channel");
    return -1;
  }

  return 0;
}

int32_t AudioMixerManagerMac::SpeakerVolume(uint32_t& volume) const {
  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;
  UInt32 size = 0;
  unsigned int channels = 0;
  Float32 channelVol = 0;
  Float32 vol = 0;

  // Does the device have a master volume control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, 0};
  Boolean hasProperty =
      AudioObjectHasProperty(_outputDeviceID, &propertyAddress);
  if (hasProperty) {
    size = sizeof(vol);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
        _outputDeviceID, &propertyAddress, 0, NULL, &size, &vol));

    // vol 0.0 to 1.0 -> convert to 0 - 255
    volume = static_cast<uint32_t>(vol * 255 + 0.5);
  } else {
    // Otherwise get the average volume across channels.
    vol = 0;
    for (UInt32 i = 1; i <= _noOutputChannels; i++) {
      channelVol = 0;
      propertyAddress.mElement = i;
      hasProperty = AudioObjectHasProperty(_outputDeviceID, &propertyAddress);
      if (hasProperty) {
        size = sizeof(channelVol);
        WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
            _outputDeviceID, &propertyAddress, 0, NULL, &size, &channelVol));

        vol += channelVol;
        channels++;
      }
    }

    if (channels == 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   " Unable to get a volume on any channel");
      return -1;
    }

    assert(channels > 0);
    // vol 0.0 to 1.0 -> convert to 0 - 255
    volume = static_cast<uint32_t>(255 * vol / channels + 0.5);
  }

  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "     AudioMixerManagerMac::SpeakerVolume() => vol=%i", vol);

  return 0;
}

int32_t AudioMixerManagerMac::MaxSpeakerVolume(uint32_t& maxVolume) const {
  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  // volume range is 0.0 to 1.0
  // we convert that to 0 - 255
  maxVolume = 255;

  return 0;
}

int32_t AudioMixerManagerMac::MinSpeakerVolume(uint32_t& minVolume) const {
  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  // volume range is 0.0 to 1.0
  // we convert that to 0 - 255
  minVolume = 0;

  return 0;
}

int32_t AudioMixerManagerMac::SpeakerVolumeStepSize(uint16_t& stepSize) const {
  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  // volume range is 0.0 to 1.0
  // we convert that to 0 - 255
  stepSize = 1;

  return 0;
}

int32_t AudioMixerManagerMac::SpeakerVolumeIsAvailable(bool& available) {
  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;

  // Does the capture device have a master volume control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, 0};
  Boolean isSettable = false;
  err = AudioObjectIsPropertySettable(_outputDeviceID, &propertyAddress,
                                      &isSettable);
  if (err == noErr && isSettable) {
    available = true;
    return 0;
  }

  // Otherwise try to set each channel.
  for (UInt32 i = 1; i <= _noOutputChannels; i++) {
    propertyAddress.mElement = i;
    isSettable = false;
    err = AudioObjectIsPropertySettable(_outputDeviceID, &propertyAddress,
                                        &isSettable);
    if (err != noErr || !isSettable) {
      available = false;
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   " Volume cannot be set for output channel %d, err=%d", i,
                   err);
      return -1;
    }
  }

  available = true;
  return 0;
}

int32_t AudioMixerManagerMac::SpeakerMuteIsAvailable(bool& available) {
  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;

  // Does the capture device have a master mute control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput, 0};
  Boolean isSettable = false;
  err = AudioObjectIsPropertySettable(_outputDeviceID, &propertyAddress,
                                      &isSettable);
  if (err == noErr && isSettable) {
    available = true;
    return 0;
  }

  // Otherwise try to set each channel.
  for (UInt32 i = 1; i <= _noOutputChannels; i++) {
    propertyAddress.mElement = i;
    isSettable = false;
    err = AudioObjectIsPropertySettable(_outputDeviceID, &propertyAddress,
                                        &isSettable);
    if (err != noErr || !isSettable) {
      available = false;
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   " Mute cannot be set for output channel %d, err=%d", i, err);
      return -1;
    }
  }

  available = true;
  return 0;
}

int32_t AudioMixerManagerMac::SetSpeakerMute(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "AudioMixerManagerMac::SetSpeakerMute(enable=%u)", enable);

  rtc::CritScope lock(&_critSect);

  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;
  UInt32 size = 0;
  UInt32 mute = enable ? 1 : 0;
  bool success = false;

  // Does the render device have a master mute control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput, 0};
  Boolean isSettable = false;
  err = AudioObjectIsPropertySettable(_outputDeviceID, &propertyAddress,
                                      &isSettable);
  if (err == noErr && isSettable) {
    size = sizeof(mute);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectSetPropertyData(
        _outputDeviceID, &propertyAddress, 0, NULL, size, &mute));

    return 0;
  }

  // Otherwise try to set each channel.
  for (UInt32 i = 1; i <= _noOutputChannels; i++) {
    propertyAddress.mElement = i;
    isSettable = false;
    err = AudioObjectIsPropertySettable(_outputDeviceID, &propertyAddress,
                                        &isSettable);
    if (err == noErr && isSettable) {
      size = sizeof(mute);
      WEBRTC_CA_RETURN_ON_ERR(AudioObjectSetPropertyData(
          _outputDeviceID, &propertyAddress, 0, NULL, size, &mute));
    }
    success = true;
  }

  if (!success) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 " Unable to set mute on any input channel");
    return -1;
  }

  return 0;
}

int32_t AudioMixerManagerMac::SpeakerMute(bool& enabled) const {
  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;
  UInt32 size = 0;
  unsigned int channels = 0;
  UInt32 channelMuted = 0;
  UInt32 muted = 0;

  // Does the device have a master volume control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyMute, kAudioDevicePropertyScopeOutput, 0};
  Boolean hasProperty =
      AudioObjectHasProperty(_outputDeviceID, &propertyAddress);
  if (hasProperty) {
    size = sizeof(muted);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
        _outputDeviceID, &propertyAddress, 0, NULL, &size, &muted));

    // 1 means muted
    enabled = static_cast<bool>(muted);
  } else {
    // Otherwise check if all channels are muted.
    for (UInt32 i = 1; i <= _noOutputChannels; i++) {
      muted = 0;
      propertyAddress.mElement = i;
      hasProperty = AudioObjectHasProperty(_outputDeviceID, &propertyAddress);
      if (hasProperty) {
        size = sizeof(channelMuted);
        WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
            _outputDeviceID, &propertyAddress, 0, NULL, &size, &channelMuted));

        muted = (muted && channelMuted);
        channels++;
      }
    }

    if (channels == 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   " Unable to get mute for any channel");
      return -1;
    }

    assert(channels > 0);
    // 1 means muted
    enabled = static_cast<bool>(muted);
  }

  WEBRTC_TRACE(
      kTraceInfo, kTraceAudioDevice, _id,
      "     AudioMixerManagerMac::SpeakerMute() => enabled=%d, enabled");

  return 0;
}

int32_t AudioMixerManagerMac::StereoPlayoutIsAvailable(bool& available) {
  if (_outputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  available = (_noOutputChannels == 2);
  return 0;
}

int32_t AudioMixerManagerMac::StereoRecordingIsAvailable(bool& available) {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  available = (_noInputChannels == 2);
  return 0;
}

int32_t AudioMixerManagerMac::MicrophoneMuteIsAvailable(bool& available) {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;

  // Does the capture device have a master mute control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyMute, kAudioDevicePropertyScopeInput, 0};
  Boolean isSettable = false;
  err = AudioObjectIsPropertySettable(_inputDeviceID, &propertyAddress,
                                      &isSettable);
  if (err == noErr && isSettable) {
    available = true;
    return 0;
  }

  // Otherwise try to set each channel.
  for (UInt32 i = 1; i <= _noInputChannels; i++) {
    propertyAddress.mElement = i;
    isSettable = false;
    err = AudioObjectIsPropertySettable(_inputDeviceID, &propertyAddress,
                                        &isSettable);
    if (err != noErr || !isSettable) {
      available = false;
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   " Mute cannot be set for output channel %d, err=%d", i, err);
      return -1;
    }
  }

  available = true;
  return 0;
}

int32_t AudioMixerManagerMac::SetMicrophoneMute(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "AudioMixerManagerMac::SetMicrophoneMute(enable=%u)", enable);

  rtc::CritScope lock(&_critSect);

  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;
  UInt32 size = 0;
  UInt32 mute = enable ? 1 : 0;
  bool success = false;

  // Does the capture device have a master mute control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyMute, kAudioDevicePropertyScopeInput, 0};
  Boolean isSettable = false;
  err = AudioObjectIsPropertySettable(_inputDeviceID, &propertyAddress,
                                      &isSettable);
  if (err == noErr && isSettable) {
    size = sizeof(mute);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectSetPropertyData(
        _inputDeviceID, &propertyAddress, 0, NULL, size, &mute));

    return 0;
  }

  // Otherwise try to set each channel.
  for (UInt32 i = 1; i <= _noInputChannels; i++) {
    propertyAddress.mElement = i;
    isSettable = false;
    err = AudioObjectIsPropertySettable(_inputDeviceID, &propertyAddress,
                                        &isSettable);
    if (err == noErr && isSettable) {
      size = sizeof(mute);
      WEBRTC_CA_RETURN_ON_ERR(AudioObjectSetPropertyData(
          _inputDeviceID, &propertyAddress, 0, NULL, size, &mute));
    }
    success = true;
  }

  if (!success) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 " Unable to set mute on any input channel");
    return -1;
  }

  return 0;
}

int32_t AudioMixerManagerMac::MicrophoneMute(bool& enabled) const {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;
  UInt32 size = 0;
  unsigned int channels = 0;
  UInt32 channelMuted = 0;
  UInt32 muted = 0;

  // Does the device have a master volume control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyMute, kAudioDevicePropertyScopeInput, 0};
  Boolean hasProperty =
      AudioObjectHasProperty(_inputDeviceID, &propertyAddress);
  if (hasProperty) {
    size = sizeof(muted);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
        _inputDeviceID, &propertyAddress, 0, NULL, &size, &muted));

    // 1 means muted
    enabled = static_cast<bool>(muted);
  } else {
    // Otherwise check if all channels are muted.
    for (UInt32 i = 1; i <= _noInputChannels; i++) {
      muted = 0;
      propertyAddress.mElement = i;
      hasProperty = AudioObjectHasProperty(_inputDeviceID, &propertyAddress);
      if (hasProperty) {
        size = sizeof(channelMuted);
        WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
            _inputDeviceID, &propertyAddress, 0, NULL, &size, &channelMuted));

        muted = (muted && channelMuted);
        channels++;
      }
    }

    if (channels == 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   " Unable to get mute for any channel");
      return -1;
    }

    assert(channels > 0);
    // 1 means muted
    enabled = static_cast<bool>(muted);
  }

  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "     AudioMixerManagerMac::MicrophoneMute() => enabled=%d",
               enabled);

  return 0;
}

int32_t AudioMixerManagerMac::MicrophoneBoostIsAvailable(bool& available) {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  available = false;  // No AudioObjectPropertySelector value for Mic Boost

  return 0;
}

int32_t AudioMixerManagerMac::SetMicrophoneBoost(bool enable) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "AudioMixerManagerMac::SetMicrophoneBoost(enable=%u)", enable);

  rtc::CritScope lock(&_critSect);

  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  // Ensure that the selected microphone has a valid boost control.
  bool available(false);
  MicrophoneBoostIsAvailable(available);
  if (!available) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  it is not possible to enable microphone boost");
    return -1;
  }

  // It is assumed that the call above fails!
  return 0;
}

int32_t AudioMixerManagerMac::MicrophoneBoost(bool& enabled) const {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  // Microphone boost cannot be enabled on this platform!
  enabled = false;

  return 0;
}

int32_t AudioMixerManagerMac::MicrophoneVolumeIsAvailable(bool& available) {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;

  // Does the capture device have a master volume control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput, 0};
  Boolean isSettable = false;
  err = AudioObjectIsPropertySettable(_inputDeviceID, &propertyAddress,
                                      &isSettable);
  if (err == noErr && isSettable) {
    available = true;
    return 0;
  }

  // Otherwise try to set each channel.
  for (UInt32 i = 1; i <= _noInputChannels; i++) {
    propertyAddress.mElement = i;
    isSettable = false;
    err = AudioObjectIsPropertySettable(_inputDeviceID, &propertyAddress,
                                        &isSettable);
    if (err != noErr || !isSettable) {
      available = false;
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   " Volume cannot be set for input channel %d, err=%d", i,
                   err);
      return -1;
    }
  }

  available = true;
  return 0;
}

int32_t AudioMixerManagerMac::SetMicrophoneVolume(uint32_t volume) {
  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "AudioMixerManagerMac::SetMicrophoneVolume(volume=%u)", volume);

  rtc::CritScope lock(&_critSect);

  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;
  UInt32 size = 0;
  bool success = false;

  // volume range is 0.0 - 1.0, convert from 0 - 255
  const Float32 vol = (Float32)(volume / 255.0);

  assert(vol <= 1.0 && vol >= 0.0);

  // Does the capture device have a master volume control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput, 0};
  Boolean isSettable = false;
  err = AudioObjectIsPropertySettable(_inputDeviceID, &propertyAddress,
                                      &isSettable);
  if (err == noErr && isSettable) {
    size = sizeof(vol);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectSetPropertyData(
        _inputDeviceID, &propertyAddress, 0, NULL, size, &vol));

    return 0;
  }

  // Otherwise try to set each channel.
  for (UInt32 i = 1; i <= _noInputChannels; i++) {
    propertyAddress.mElement = i;
    isSettable = false;
    err = AudioObjectIsPropertySettable(_inputDeviceID, &propertyAddress,
                                        &isSettable);
    if (err == noErr && isSettable) {
      size = sizeof(vol);
      WEBRTC_CA_RETURN_ON_ERR(AudioObjectSetPropertyData(
          _inputDeviceID, &propertyAddress, 0, NULL, size, &vol));
    }
    success = true;
  }

  if (!success) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 " Unable to set a level on any input channel");
    return -1;
  }

  return 0;
}

int32_t AudioMixerManagerMac::MicrophoneVolume(uint32_t& volume) const {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  OSStatus err = noErr;
  UInt32 size = 0;
  unsigned int channels = 0;
  Float32 channelVol = 0;
  Float32 volFloat32 = 0;

  // Does the device have a master volume control?
  // If so, use it exclusively.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeInput, 0};
  Boolean hasProperty =
      AudioObjectHasProperty(_inputDeviceID, &propertyAddress);
  if (hasProperty) {
    size = sizeof(volFloat32);
    WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
        _inputDeviceID, &propertyAddress, 0, NULL, &size, &volFloat32));

    // vol 0.0 to 1.0 -> convert to 0 - 255
    volume = static_cast<uint32_t>(volFloat32 * 255 + 0.5);
  } else {
    // Otherwise get the average volume across channels.
    volFloat32 = 0;
    for (UInt32 i = 1; i <= _noInputChannels; i++) {
      channelVol = 0;
      propertyAddress.mElement = i;
      hasProperty = AudioObjectHasProperty(_inputDeviceID, &propertyAddress);
      if (hasProperty) {
        size = sizeof(channelVol);
        WEBRTC_CA_RETURN_ON_ERR(AudioObjectGetPropertyData(
            _inputDeviceID, &propertyAddress, 0, NULL, &size, &channelVol));

        volFloat32 += channelVol;
        channels++;
      }
    }

    if (channels == 0) {
      WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                   " Unable to get a level on any channel");
      return -1;
    }

    assert(channels > 0);
    // vol 0.0 to 1.0 -> convert to 0 - 255
    volume = static_cast<uint32_t>(255 * volFloat32 / channels + 0.5);
  }

  WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
               "     AudioMixerManagerMac::MicrophoneVolume() => vol=%u",
               volume);

  return 0;
}

int32_t AudioMixerManagerMac::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  // volume range is 0.0 to 1.0
  // we convert that to 0 - 255
  maxVolume = 255;

  return 0;
}

int32_t AudioMixerManagerMac::MinMicrophoneVolume(uint32_t& minVolume) const {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  // volume range is 0.0 to 1.0
  // we convert that to 0 - 10
  minVolume = 0;

  return 0;
}

int32_t AudioMixerManagerMac::MicrophoneVolumeStepSize(
    uint16_t& stepSize) const {
  if (_inputDeviceID == kAudioObjectUnknown) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  device ID has not been set");
    return -1;
  }

  // volume range is 0.0 to 1.0
  // we convert that to 0 - 10
  stepSize = 1;

  return 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

// CoreAudio errors are best interpreted as four character strings.
void AudioMixerManagerMac::logCAMsg(const TraceLevel level,
                                    const TraceModule module,
                                    const int32_t id,
                                    const char* msg,
                                    const char* err) {
  assert(msg != NULL);
  assert(err != NULL);

#ifdef WEBRTC_ARCH_BIG_ENDIAN
  WEBRTC_TRACE(level, module, id, "%s: %.4s", msg, err);
#else
  // We need to flip the characters in this case.
  WEBRTC_TRACE(level, module, id, "%s: %.1s%.1s%.1s%.1s", msg, err + 3, err + 2,
               err + 1, err);
#endif
}

}  // namespace webrtc
// EOF
