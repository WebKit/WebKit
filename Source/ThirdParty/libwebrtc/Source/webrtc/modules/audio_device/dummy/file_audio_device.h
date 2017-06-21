/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_H
#define WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_H

#include <stdio.h>

#include <memory>
#include <string>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"

namespace rtc {
class PlatformThread;
}  // namespace rtc

namespace webrtc {
class EventWrapper;

// This is a fake audio device which plays audio from a file as its microphone
// and plays out into a file.
class FileAudioDevice : public AudioDeviceGeneric {
 public:
  // Constructs a file audio device with |id|. It will read audio from
  // |inputFilename| and record output audio to |outputFilename|.
  //
  // The input file should be a readable 48k stereo raw file, and the output
  // file should point to a writable location. The output format will also be
  // 48k stereo raw audio.
  FileAudioDevice(const int32_t id,
                  const char* inputFilename,
                  const char* outputFilename);
  virtual ~FileAudioDevice();

  // Retrieve the currently utilized audio layer
  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const override;

  // Main initializaton and termination
  InitStatus Init() override;
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
  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override;

  // Audio transport initialization
  int32_t PlayoutIsAvailable(bool& available) override;
  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;
  int32_t RecordingIsAvailable(bool& available) override;
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
  int32_t WaveOutVolume(uint16_t& volumeLeft,
                        uint16_t& volumeRight) const override;

  // Audio mixer initialization
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;

  // Speaker volume controls
  int32_t SpeakerVolumeIsAvailable(bool& available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t& volume) const override;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const override;
  int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const override;

  // Microphone volume controls
  int32_t MicrophoneVolumeIsAvailable(bool& available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t& volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneVolumeStepSize(uint16_t& stepSize) const override;

  // Speaker mute control
  int32_t SpeakerMuteIsAvailable(bool& available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool& enabled) const override;

  // Microphone mute control
  int32_t MicrophoneMuteIsAvailable(bool& available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool& enabled) const override;

  // Microphone boost control
  int32_t MicrophoneBoostIsAvailable(bool& available) override;
  int32_t SetMicrophoneBoost(bool enable) override;
  int32_t MicrophoneBoost(bool& enabled) const override;

  // Stereo support
  int32_t StereoPlayoutIsAvailable(bool& available) override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool& enabled) const override;
  int32_t StereoRecordingIsAvailable(bool& available) override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool& enabled) const override;

  // Delay information and control
  int32_t SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                           uint16_t sizeMS) override;
  int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,
                        uint16_t& sizeMS) const override;
  int32_t PlayoutDelay(uint16_t& delayMS) const override;
  int32_t RecordingDelay(uint16_t& delayMS) const override;

  // CPU load
  int32_t CPULoad(uint16_t& load) const override;

  bool PlayoutWarning() const override;
  bool PlayoutError() const override;
  bool RecordingWarning() const override;
  bool RecordingError() const override;
  void ClearPlayoutWarning() override;
  void ClearPlayoutError() override;
  void ClearRecordingWarning() override;
  void ClearRecordingError() override;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

 private:
  static bool RecThreadFunc(void*);
  static bool PlayThreadFunc(void*);
  bool RecThreadProcess();
  bool PlayThreadProcess();

  int32_t _playout_index;
  int32_t _record_index;
  AudioDeviceModule::BufferType _playBufType;
  AudioDeviceBuffer* _ptrAudioBuffer;
  int8_t* _recordingBuffer;  // In bytes.
  int8_t* _playoutBuffer;  // In bytes.
  uint32_t _recordingFramesLeft;
  uint32_t _playoutFramesLeft;
  rtc::CriticalSection _critSect;

  size_t _recordingBufferSizeIn10MS;
  size_t _recordingFramesIn10MS;
  size_t _playoutFramesIn10MS;

  // TODO(pbos): Make plain members instead of pointers and stop resetting them.
  std::unique_ptr<rtc::PlatformThread> _ptrThreadRec;
  std::unique_ptr<rtc::PlatformThread> _ptrThreadPlay;

  bool _playing;
  bool _recording;
  int64_t _lastCallPlayoutMillis;
  int64_t _lastCallRecordMillis;

  FileWrapper& _outputFile;
  FileWrapper& _inputFile;
  std::string _outputFilename;
  std::string _inputFilename;
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_H
