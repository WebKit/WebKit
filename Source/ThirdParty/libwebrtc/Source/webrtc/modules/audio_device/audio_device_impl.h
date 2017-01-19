/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IMPL_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IMPL_H

#if defined(WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE)

#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/audio_device_buffer.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc {

class AudioDeviceGeneric;
class AudioManager;
class CriticalSectionWrapper;

class AudioDeviceModuleImpl : public AudioDeviceModule {
 public:
  enum PlatformType {
    kPlatformNotSupported = 0,
    kPlatformWin32 = 1,
    kPlatformWinCe = 2,
    kPlatformLinux = 3,
    kPlatformMac = 4,
    kPlatformAndroid = 5,
    kPlatformIOS = 6
  };

  int32_t CheckPlatform();
  int32_t CreatePlatformSpecificObjects();
  int32_t AttachAudioBuffer();

  AudioDeviceModuleImpl(const int32_t id, const AudioLayer audioLayer);
  ~AudioDeviceModuleImpl() override;

  int64_t TimeUntilNextProcess() override;
  void Process() override;

  // Retrieve the currently utilized audio layer
  int32_t ActiveAudioLayer(AudioLayer* audioLayer) const override;

  // Error handling
  ErrorCode LastError() const override;
  int32_t RegisterEventObserver(AudioDeviceObserver* eventCallback) override;

  // Full-duplex transportation of PCM audio
  int32_t RegisterAudioCallback(AudioTransport* audioCallback) override;

  // Main initializaton and termination
  int32_t Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  // Device enumeration
  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override;

  // Device selection
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(WindowsDeviceType device) override;

  // Audio transport initialization
  int32_t PlayoutIsAvailable(bool* available) override;
  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;
  int32_t RecordingIsAvailable(bool* available) override;
  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;

  // Audio transport control
  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  // Microphone Automatic Gain Control (AGC)
  int32_t SetAGC(bool enable) override;
  bool AGC() const override;

  // Volume control based on the Windows Wave API (Windows only)
  int32_t SetWaveOutVolume(uint16_t volumeLeft, uint16_t volumeRight) override;
  int32_t WaveOutVolume(uint16_t* volumeLeft,
                        uint16_t* volumeRight) const override;

  // Audio mixer initialization
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;

  // Speaker volume controls
  int32_t SpeakerVolumeIsAvailable(bool* available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t* volume) const override;
  int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t* minVolume) const override;
  int32_t SpeakerVolumeStepSize(uint16_t* stepSize) const override;

  // Microphone volume controls
  int32_t MicrophoneVolumeIsAvailable(bool* available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t* volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t* minVolume) const override;
  int32_t MicrophoneVolumeStepSize(uint16_t* stepSize) const override;

  // Speaker mute control
  int32_t SpeakerMuteIsAvailable(bool* available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool* enabled) const override;

  // Microphone mute control
  int32_t MicrophoneMuteIsAvailable(bool* available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool* enabled) const override;

  // Microphone boost control
  int32_t MicrophoneBoostIsAvailable(bool* available) override;
  int32_t SetMicrophoneBoost(bool enable) override;
  int32_t MicrophoneBoost(bool* enabled) const override;

  // Stereo support
  int32_t StereoPlayoutIsAvailable(bool* available) const override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool* enabled) const override;
  int32_t StereoRecordingIsAvailable(bool* available) const override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool* enabled) const override;
  int32_t SetRecordingChannel(const ChannelType channel) override;
  int32_t RecordingChannel(ChannelType* channel) const override;

  // Delay information and control
  int32_t SetPlayoutBuffer(const BufferType type, uint16_t sizeMS = 0) override;
  int32_t PlayoutBuffer(BufferType* type, uint16_t* sizeMS) const override;
  int32_t PlayoutDelay(uint16_t* delayMS) const override;
  int32_t RecordingDelay(uint16_t* delayMS) const override;

  // CPU load
  int32_t CPULoad(uint16_t* load) const override;

  // Recording of raw PCM data
  int32_t StartRawOutputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) override;
  int32_t StopRawOutputFileRecording() override;
  int32_t StartRawInputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) override;
  int32_t StopRawInputFileRecording() override;

  // Native sample rate controls (samples/sec)
  int32_t SetRecordingSampleRate(const uint32_t samplesPerSec) override;
  int32_t RecordingSampleRate(uint32_t* samplesPerSec) const override;
  int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec) override;
  int32_t PlayoutSampleRate(uint32_t* samplesPerSec) const override;

  // Mobile device specific functions
  int32_t ResetAudioDevice() override;
  int32_t SetLoudspeakerStatus(bool enable) override;
  int32_t GetLoudspeakerStatus(bool* enabled) const override;

  bool BuiltInAECIsAvailable() const override;
  int32_t EnableBuiltInAEC(bool enable) override;
  bool BuiltInAGCIsAvailable() const override;
  int32_t EnableBuiltInAGC(bool enable) override;
  bool BuiltInNSIsAvailable() const override;
  int32_t EnableBuiltInNS(bool enable) override;

#if defined(WEBRTC_IOS)
  int GetPlayoutAudioParameters(AudioParameters* params) const override;
  int GetRecordAudioParameters(AudioParameters* params) const override;
#endif  // WEBRTC_IOS

  int32_t Id() { return _id; }
#if defined(WEBRTC_ANDROID)
  // Only use this acccessor for test purposes on Android.
  AudioManager* GetAndroidAudioManagerForTest() {
    return _audioManagerAndroid.get();
  }
#endif
  AudioDeviceBuffer* GetAudioDeviceBuffer() { return &_audioDeviceBuffer; }

 private:
  PlatformType Platform() const;
  AudioLayer PlatformAudioLayer() const;

  CriticalSectionWrapper& _critSect;
  CriticalSectionWrapper& _critSectEventCb;
  CriticalSectionWrapper& _critSectAudioCb;

  AudioDeviceObserver* _ptrCbAudioDeviceObserver;

  AudioDeviceGeneric* _ptrAudioDevice;

  AudioDeviceBuffer _audioDeviceBuffer;
#if defined(WEBRTC_ANDROID)
  std::unique_ptr<AudioManager> _audioManagerAndroid;
#endif
  int32_t _id;
  AudioLayer _platformAudioLayer;
  int64_t _lastProcessTime;
  PlatformType _platformType;
  bool _initialized;
  mutable ErrorCode _lastError;
};

}  // namespace webrtc

#endif  // defined(WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE)

#endif  // WEBRTC_MODULES_INTERFACE_AUDIO_DEVICE_IMPL_H_
