/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_FAKE_AUDIO_DEVICE_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_FAKE_AUDIO_DEVICE_H_

#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc {

class FakeAudioDeviceModule : public AudioDeviceModule {
 public:
  FakeAudioDeviceModule() {}
  virtual ~FakeAudioDeviceModule() {}
  virtual int32_t AddRef() const { return 0; }
  virtual int32_t Release() const { return 0; }
  virtual int32_t RegisterEventObserver(AudioDeviceObserver* eventCallback) {
    return 0;
  }
  virtual int32_t RegisterAudioCallback(AudioTransport* audioCallback) {
    return 0;
  }
  virtual int32_t Init() { return 0; }
  virtual int32_t InitSpeaker() { return 0; }
  virtual int32_t SetPlayoutDevice(uint16_t index) { return 0; }
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) { return 0; }
  virtual int32_t SetStereoPlayout(bool enable) { return 0; }
  virtual int32_t StopPlayout() { return 0; }
  virtual int32_t InitMicrophone() { return 0; }
  virtual int32_t SetRecordingDevice(uint16_t index) { return 0; }
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) { return 0; }
  virtual int32_t SetStereoRecording(bool enable) { return 0; }
  virtual int32_t SetAGC(bool enable) { return 0; }
  virtual int32_t StopRecording() { return 0; }
  virtual int64_t TimeUntilNextProcess() { return 0; }
  virtual void Process() {}
  virtual int32_t Terminate() { return 0; }

  virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const { return 0; }
  virtual ErrorCode LastError() const { return kAdmErrNone; }
  virtual bool Initialized() const { return true; }
  virtual int16_t PlayoutDevices() { return 0; }
  virtual int16_t RecordingDevices() { return 0; }
  virtual int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) {
    return 0;
  }
  virtual int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) {
    return 0;
  }
  virtual int32_t PlayoutIsAvailable(bool* available) { return 0; }
  virtual int32_t InitPlayout() { return 0; }
  virtual bool PlayoutIsInitialized() const { return true; }
  virtual int32_t RecordingIsAvailable(bool* available) { return 0; }
  virtual int32_t InitRecording() { return 0; }
  virtual bool RecordingIsInitialized() const { return true; }
  virtual int32_t StartPlayout() { return 0; }
  virtual bool Playing() const { return false; }
  virtual int32_t StartRecording() { return 0; }
  virtual bool Recording() const { return false; }
  virtual bool AGC() const { return true; }
  virtual int32_t SetWaveOutVolume(uint16_t volumeLeft,
                           uint16_t volumeRight) {
    return 0;
  }
  virtual int32_t WaveOutVolume(uint16_t* volumeLeft,
                        uint16_t* volumeRight) const {
    return 0;
  }
  virtual bool SpeakerIsInitialized() const { return true; }
  virtual bool MicrophoneIsInitialized() const { return true; }
  virtual int32_t SpeakerVolumeIsAvailable(bool* available) { return 0; }
  virtual int32_t SetSpeakerVolume(uint32_t volume) { return 0; }
  virtual int32_t SpeakerVolume(uint32_t* volume) const { return 0; }
  virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const { return 0; }
  virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const { return 0; }
  virtual int32_t SpeakerVolumeStepSize(uint16_t* stepSize) const { return 0; }
  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) { return 0; }
  virtual int32_t SetMicrophoneVolume(uint32_t volume) { return 0; }
  virtual int32_t MicrophoneVolume(uint32_t* volume) const { return 0; }
  virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const { return 0; }
  virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const { return 0; }
  virtual int32_t MicrophoneVolumeStepSize(uint16_t* stepSize) const {
    return 0;
  }
  virtual int32_t SpeakerMuteIsAvailable(bool* available) { return 0; }
  virtual int32_t SetSpeakerMute(bool enable) { return 0; }
  virtual int32_t SpeakerMute(bool* enabled) const { return 0; }
  virtual int32_t MicrophoneMuteIsAvailable(bool* available) { return 0; }
  virtual int32_t SetMicrophoneMute(bool enable) { return 0; }
  virtual int32_t MicrophoneMute(bool* enabled) const { return 0; }
  virtual int32_t MicrophoneBoostIsAvailable(bool* available) { return 0; }
  virtual int32_t SetMicrophoneBoost(bool enable) { return 0; }
  virtual int32_t MicrophoneBoost(bool* enabled) const { return 0; }
  virtual int32_t StereoPlayoutIsAvailable(bool* available) const {
    *available = false;
    return 0;
  }
  virtual int32_t StereoPlayout(bool* enabled) const { return 0; }
  virtual int32_t StereoRecordingIsAvailable(bool* available) const {
    *available = false;
    return 0;
  }
  virtual int32_t StereoRecording(bool* enabled) const { return 0; }
  virtual int32_t SetRecordingChannel(const ChannelType channel) { return 0; }
  virtual int32_t RecordingChannel(ChannelType* channel) const { return 0; }
  virtual int32_t SetPlayoutBuffer(const BufferType type,
                           uint16_t sizeMS = 0) {
    return 0;
  }
  virtual int32_t PlayoutBuffer(BufferType* type, uint16_t* sizeMS) const {
    return 0;
  }
  virtual int32_t PlayoutDelay(uint16_t* delayMS) const { return 0; }
  virtual int32_t RecordingDelay(uint16_t* delayMS) const { return 0; }
  virtual int32_t CPULoad(uint16_t* load) const { return 0; }
  virtual int32_t StartRawOutputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
    return 0;
  }
  virtual int32_t StopRawOutputFileRecording() { return 0; }
  virtual int32_t StartRawInputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
    return 0;
  }
  virtual int32_t StopRawInputFileRecording() { return 0; }
  virtual int32_t SetRecordingSampleRate(const uint32_t samplesPerSec) {
    return 0;
  }
  virtual int32_t RecordingSampleRate(uint32_t* samplesPerSec) const {
    return 0;
  }
  virtual int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec) {
    return 0;
  }
  virtual int32_t PlayoutSampleRate(uint32_t* samplesPerSec) const { return 0; }
  virtual int32_t ResetAudioDevice() { return 0; }
  virtual int32_t SetLoudspeakerStatus(bool enable) { return 0; }
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const { return 0; }
  virtual bool BuiltInAECIsAvailable() const { return false; }
  virtual int32_t EnableBuiltInAEC(bool enable) { return -1; }
  virtual bool BuiltInAGCIsAvailable() const { return false; }
  virtual int32_t EnableBuiltInAGC(bool enable) { return -1; }
  virtual bool BuiltInNSIsAvailable() const { return false; }
  virtual int32_t EnableBuiltInNS(bool enable) { return -1; }

#if defined(WEBRTC_IOS)
  virtual int GetPlayoutAudioParameters(AudioParameters* params) const {
    return -1;
  }
  virtual int GetRecordAudioParameters(AudioParameters* params) const {
    return -1;
  }
#endif  // WEBRTC_IOS
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_FAKE_AUDIO_DEVICE_H_
