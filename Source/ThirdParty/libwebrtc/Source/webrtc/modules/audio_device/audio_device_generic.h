/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_GENERIC_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_GENERIC_H

#include "webrtc/modules/audio_device/audio_device_buffer.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc {

class AudioDeviceGeneric {
 public:
  // For use with UMA logging. Must be kept in sync with histograms.xml in
  // Chrome, located at
  // https://cs.chromium.org/chromium/src/tools/metrics/histograms/histograms.xml
  enum class InitStatus {
    OK = 0,
    PLAYOUT_ERROR = 1,
    RECORDING_ERROR = 2,
    OTHER_ERROR = 3,
    NUM_STATUSES = 4
  };
  // Retrieve the currently utilized audio layer
  virtual int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const = 0;

  // Main initializaton and termination
  virtual InitStatus Init() = 0;
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
  virtual int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) = 0;
  virtual int32_t SetRecordingDevice(uint16_t index) = 0;
  virtual int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) = 0;

  // Audio transport initialization
  virtual int32_t PlayoutIsAvailable(bool& available) = 0;
  virtual int32_t InitPlayout() = 0;
  virtual bool PlayoutIsInitialized() const = 0;
  virtual int32_t RecordingIsAvailable(bool& available) = 0;
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
  virtual int32_t WaveOutVolume(uint16_t& volumeLeft,
                                uint16_t& volumeRight) const = 0;

  // Audio mixer initialization
  virtual int32_t InitSpeaker() = 0;
  virtual bool SpeakerIsInitialized() const = 0;
  virtual int32_t InitMicrophone() = 0;
  virtual bool MicrophoneIsInitialized() const = 0;

  // Speaker volume controls
  virtual int32_t SpeakerVolumeIsAvailable(bool& available) = 0;
  virtual int32_t SetSpeakerVolume(uint32_t volume) = 0;
  virtual int32_t SpeakerVolume(uint32_t& volume) const = 0;
  virtual int32_t MaxSpeakerVolume(uint32_t& maxVolume) const = 0;
  virtual int32_t MinSpeakerVolume(uint32_t& minVolume) const = 0;
  virtual int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const = 0;

  // Microphone volume controls
  virtual int32_t MicrophoneVolumeIsAvailable(bool& available) = 0;
  virtual int32_t SetMicrophoneVolume(uint32_t volume) = 0;
  virtual int32_t MicrophoneVolume(uint32_t& volume) const = 0;
  virtual int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const = 0;
  virtual int32_t MinMicrophoneVolume(uint32_t& minVolume) const = 0;
  virtual int32_t MicrophoneVolumeStepSize(uint16_t& stepSize) const = 0;

  // Speaker mute control
  virtual int32_t SpeakerMuteIsAvailable(bool& available) = 0;
  virtual int32_t SetSpeakerMute(bool enable) = 0;
  virtual int32_t SpeakerMute(bool& enabled) const = 0;

  // Microphone mute control
  virtual int32_t MicrophoneMuteIsAvailable(bool& available) = 0;
  virtual int32_t SetMicrophoneMute(bool enable) = 0;
  virtual int32_t MicrophoneMute(bool& enabled) const = 0;

  // Microphone boost control
  virtual int32_t MicrophoneBoostIsAvailable(bool& available) = 0;
  virtual int32_t SetMicrophoneBoost(bool enable) = 0;
  virtual int32_t MicrophoneBoost(bool& enabled) const = 0;

  // Stereo support
  virtual int32_t StereoPlayoutIsAvailable(bool& available) = 0;
  virtual int32_t SetStereoPlayout(bool enable) = 0;
  virtual int32_t StereoPlayout(bool& enabled) const = 0;
  virtual int32_t StereoRecordingIsAvailable(bool& available) = 0;
  virtual int32_t SetStereoRecording(bool enable) = 0;
  virtual int32_t StereoRecording(bool& enabled) const = 0;

  // Delay information and control
  virtual int32_t SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                                   uint16_t sizeMS = 0) = 0;
  virtual int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,
                                uint16_t& sizeMS) const = 0;
  virtual int32_t PlayoutDelay(uint16_t& delayMS) const = 0;
  virtual int32_t RecordingDelay(uint16_t& delayMS) const = 0;

  // CPU load
  virtual int32_t CPULoad(uint16_t& load) const = 0;

  // Native sample rate controls (samples/sec)
  virtual int32_t SetRecordingSampleRate(const uint32_t samplesPerSec);
  virtual int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec);

  // Speaker audio routing (for mobile devices)
  virtual int32_t SetLoudspeakerStatus(bool enable);
  virtual int32_t GetLoudspeakerStatus(bool& enable) const;

  // Reset Audio Device (for mobile devices)
  virtual int32_t ResetAudioDevice();

  // Sound Audio Device control (for WinCE only)
  virtual int32_t SoundDeviceControl(unsigned int par1 = 0,
                                     unsigned int par2 = 0,
                                     unsigned int par3 = 0,
                                     unsigned int par4 = 0);

  // Android only
  virtual bool BuiltInAECIsAvailable() const;
  virtual bool BuiltInAGCIsAvailable() const;
  virtual bool BuiltInNSIsAvailable() const;

  // Windows Core Audio and Android only.
  virtual int32_t EnableBuiltInAEC(bool enable);
  virtual int32_t EnableBuiltInAGC(bool enable);
  virtual int32_t EnableBuiltInNS(bool enable);

  // iOS only.
  // TODO(henrika): add Android support.
#if defined(WEBRTC_IOS)
  virtual int GetPlayoutAudioParameters(AudioParameters* params) const;
  virtual int GetRecordAudioParameters(AudioParameters* params) const;
#endif  // WEBRTC_IOS

  virtual bool PlayoutWarning() const = 0;
  virtual bool PlayoutError() const = 0;
  virtual bool RecordingWarning() const = 0;
  virtual bool RecordingError() const = 0;
  virtual void ClearPlayoutWarning() = 0;
  virtual void ClearPlayoutError() = 0;
  virtual void ClearRecordingWarning() = 0;
  virtual void ClearRecordingError() = 0;

  virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) = 0;

  virtual ~AudioDeviceGeneric() {}
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_GENERIC_H
