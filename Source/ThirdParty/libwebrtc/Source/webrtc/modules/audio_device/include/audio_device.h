/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_H_
#define MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_H_

#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/include/module.h"

namespace webrtc {

class AudioDeviceModule : public RefCountedModule {
 public:
  enum ErrorCode {
    kAdmErrNone = 0,
    kAdmErrArgument = 1
  };

  enum AudioLayer {
    kPlatformDefaultAudio = 0,
    kWindowsCoreAudio = 2,
    kLinuxAlsaAudio = 3,
    kLinuxPulseAudio = 4,
    kAndroidJavaAudio = 5,
    kAndroidOpenSLESAudio = 6,
    kAndroidJavaInputAndOpenSLESOutputAudio = 7,
    kDummyAudio = 8
  };

  enum WindowsDeviceType {
    kDefaultCommunicationDevice = -1,
    kDefaultDevice = -2
  };

  enum BufferType {
    kFixedBufferSize  = 0,
    kAdaptiveBufferSize = 1
  };

  enum ChannelType {
    kChannelLeft = 0,
    kChannelRight = 1,
    kChannelBoth = 2
  };

 public:
  // Create an ADM.
  static rtc::scoped_refptr<AudioDeviceModule> Create(
      const int32_t id,
      const AudioLayer audio_layer);

  // Retrieve the currently utilized audio layer
  virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const = 0;

  // Error handling
  virtual ErrorCode LastError() const = 0;
  virtual int32_t RegisterEventObserver(AudioDeviceObserver* eventCallback) = 0;

  // Full-duplex transportation of PCM audio
  virtual int32_t RegisterAudioCallback(AudioTransport* audioCallback) = 0;

  // Main initialization and termination
  virtual int32_t Init() = 0;
  virtual int32_t Terminate() = 0;
  virtual bool Initialized() const = 0;

  // Device enumeration
  virtual int16_t PlayoutDevices() = 0;
  virtual int16_t RecordingDevices() = 0;
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[kAdmMaxDeviceNameSize],
                                    char guid[kAdmMaxGuidSize]) = 0;
  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]) = 0;

  // Device selection
  virtual int32_t SetPlayoutDevice(uint16_t index) = 0;
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) = 0;
  virtual int32_t SetRecordingDevice(uint16_t index) = 0;
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) = 0;

  // Audio transport initialization
  virtual int32_t PlayoutIsAvailable(bool* available) = 0;
  virtual int32_t InitPlayout() = 0;
  virtual bool PlayoutIsInitialized() const = 0;
  virtual int32_t RecordingIsAvailable(bool* available) = 0;
  virtual int32_t InitRecording() = 0;
  virtual bool RecordingIsInitialized() const = 0;

  // Audio transport control
  virtual int32_t StartPlayout() = 0;
  virtual int32_t StopPlayout() = 0;
  virtual bool Playing() const = 0;
  virtual int32_t StartRecording() = 0;
  virtual int32_t StopRecording() = 0;
  virtual bool Recording() const = 0;

  // Microphone Automatic Gain Control (AGC)
  virtual int32_t SetAGC(bool enable) = 0;
  virtual bool AGC() const = 0;

  // Volume control based on the Windows Wave API (Windows only)
  virtual int32_t SetWaveOutVolume(uint16_t volumeLeft,
                                   uint16_t volumeRight) = 0;
  virtual int32_t WaveOutVolume(uint16_t* volumeLeft,
                                uint16_t* volumeRight) const = 0;

  // Audio mixer initialization
  virtual int32_t InitSpeaker() = 0;
  virtual bool SpeakerIsInitialized() const = 0;
  virtual int32_t InitMicrophone() = 0;
  virtual bool MicrophoneIsInitialized() const = 0;

  // Speaker volume controls
  virtual int32_t SpeakerVolumeIsAvailable(bool* available) = 0;
  virtual int32_t SetSpeakerVolume(uint32_t volume) = 0;
  virtual int32_t SpeakerVolume(uint32_t* volume) const = 0;
  virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const = 0;
  virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const = 0;
  virtual int32_t SpeakerVolumeStepSize(uint16_t* stepSize) const = 0;

  // Microphone volume controls
  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) = 0;
  virtual int32_t SetMicrophoneVolume(uint32_t volume) = 0;
  virtual int32_t MicrophoneVolume(uint32_t* volume) const = 0;
  virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const = 0;
  virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const = 0;
  virtual int32_t MicrophoneVolumeStepSize(uint16_t* stepSize) const = 0;

  // Speaker mute control
  virtual int32_t SpeakerMuteIsAvailable(bool* available) = 0;
  virtual int32_t SetSpeakerMute(bool enable) = 0;
  virtual int32_t SpeakerMute(bool* enabled) const = 0;

  // Microphone mute control
  virtual int32_t MicrophoneMuteIsAvailable(bool* available) = 0;
  virtual int32_t SetMicrophoneMute(bool enable) = 0;
  virtual int32_t MicrophoneMute(bool* enabled) const = 0;

  // Microphone boost control
  virtual int32_t MicrophoneBoostIsAvailable(bool* available) = 0;
  virtual int32_t SetMicrophoneBoost(bool enable) = 0;
  virtual int32_t MicrophoneBoost(bool* enabled) const = 0;

  // Stereo support
  virtual int32_t StereoPlayoutIsAvailable(bool* available) const = 0;
  virtual int32_t SetStereoPlayout(bool enable) = 0;
  virtual int32_t StereoPlayout(bool* enabled) const = 0;
  virtual int32_t StereoRecordingIsAvailable(bool* available) const = 0;
  virtual int32_t SetStereoRecording(bool enable) = 0;
  virtual int32_t StereoRecording(bool* enabled) const = 0;
  virtual int32_t SetRecordingChannel(const ChannelType channel) = 0;
  virtual int32_t RecordingChannel(ChannelType* channel) const = 0;

  // Delay information and control
  virtual int32_t SetPlayoutBuffer(const BufferType type,
                                   uint16_t sizeMS = 0) = 0;
  virtual int32_t PlayoutBuffer(BufferType* type, uint16_t* sizeMS) const = 0;
  virtual int32_t PlayoutDelay(uint16_t* delayMS) const = 0;
  virtual int32_t RecordingDelay(uint16_t* delayMS) const = 0;

  // CPU load
  virtual int32_t CPULoad(uint16_t* load) const = 0;

  // Recording of raw PCM data
  virtual int32_t StartRawOutputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) = 0;
  virtual int32_t StopRawOutputFileRecording() = 0;
  virtual int32_t StartRawInputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) = 0;
  virtual int32_t StopRawInputFileRecording() = 0;

  // Native sample rate controls (samples/sec)
  virtual int32_t SetRecordingSampleRate(const uint32_t samplesPerSec) = 0;
  virtual int32_t RecordingSampleRate(uint32_t* samplesPerSec) const = 0;
  virtual int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec) = 0;
  virtual int32_t PlayoutSampleRate(uint32_t* samplesPerSec) const = 0;

  // Mobile device specific functions
  virtual int32_t ResetAudioDevice() = 0;
  virtual int32_t SetLoudspeakerStatus(bool enable) = 0;
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const = 0;

  // Only supported on Android.
  virtual bool BuiltInAECIsAvailable() const = 0;
  virtual bool BuiltInAGCIsAvailable() const = 0;
  virtual bool BuiltInNSIsAvailable() const = 0;

  // Enables the built-in audio effects. Only supported on Android.
  virtual int32_t EnableBuiltInAEC(bool enable) = 0;
  virtual int32_t EnableBuiltInAGC(bool enable) = 0;
  virtual int32_t EnableBuiltInNS(bool enable) = 0;

// Only supported on iOS.
#if defined(WEBRTC_IOS)
  virtual int GetPlayoutAudioParameters(AudioParameters* params) const = 0;
  virtual int GetRecordAudioParameters(AudioParameters* params) const = 0;
#endif  // WEBRTC_IOS

 protected:
  ~AudioDeviceModule() override {}
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_H_
