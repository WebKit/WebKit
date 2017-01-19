/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_MOCK_AUDIO_DEVICE_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_MOCK_AUDIO_DEVICE_H_

#include <string>

#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/test/gmock.h"

namespace webrtc {
namespace test {

class MockAudioDeviceModule : public AudioDeviceModule {
 public:
  // Module.
  MOCK_METHOD0(TimeUntilNextProcess, int64_t());
  MOCK_METHOD0(Process, void());
  MOCK_METHOD1(ProcessThreadAttached, void(ProcessThread*));
  // RefCountedModule.
  MOCK_CONST_METHOD0(AddRef, int32_t());
  MOCK_CONST_METHOD0(Release, int32_t());
  // AudioDeviceModule.
  MOCK_CONST_METHOD1(ActiveAudioLayer, int32_t(AudioLayer* audioLayer));
  MOCK_CONST_METHOD0(LastError, ErrorCode());
  MOCK_METHOD1(RegisterEventObserver,
               int32_t(AudioDeviceObserver* eventCallback));
  MOCK_METHOD1(RegisterAudioCallback, int32_t(AudioTransport* audioCallback));
  MOCK_METHOD0(Init, int32_t());
  MOCK_METHOD0(Terminate, int32_t());
  MOCK_CONST_METHOD0(Initialized, bool());
  MOCK_METHOD0(PlayoutDevices, int16_t());
  MOCK_METHOD0(RecordingDevices, int16_t());
  MOCK_METHOD3(PlayoutDeviceName, int32_t(uint16_t index,
                                          char name[kAdmMaxDeviceNameSize],
                                          char guid[kAdmMaxGuidSize]));
  MOCK_METHOD3(RecordingDeviceName, int32_t(uint16_t index,
                                            char name[kAdmMaxDeviceNameSize],
                                            char guid[kAdmMaxGuidSize]));
  MOCK_METHOD1(SetPlayoutDevice, int32_t(uint16_t index));
  MOCK_METHOD1(SetPlayoutDevice, int32_t(WindowsDeviceType device));
  MOCK_METHOD1(SetRecordingDevice, int32_t(uint16_t index));
  MOCK_METHOD1(SetRecordingDevice, int32_t(WindowsDeviceType device));
  MOCK_METHOD1(PlayoutIsAvailable, int32_t(bool* available));
  MOCK_METHOD0(InitPlayout, int32_t());
  MOCK_CONST_METHOD0(PlayoutIsInitialized, bool());
  MOCK_METHOD1(RecordingIsAvailable, int32_t(bool* available));
  MOCK_METHOD0(InitRecording, int32_t());
  MOCK_CONST_METHOD0(RecordingIsInitialized, bool());
  MOCK_METHOD0(StartPlayout, int32_t());
  MOCK_METHOD0(StopPlayout, int32_t());
  MOCK_CONST_METHOD0(Playing, bool());
  MOCK_METHOD0(StartRecording, int32_t());
  MOCK_METHOD0(StopRecording, int32_t());
  MOCK_CONST_METHOD0(Recording, bool());
  MOCK_METHOD1(SetAGC, int32_t(bool enable));
  MOCK_CONST_METHOD0(AGC, bool());
  MOCK_METHOD2(SetWaveOutVolume, int32_t(uint16_t volumeLeft,
                                         uint16_t volumeRight));
  MOCK_CONST_METHOD2(WaveOutVolume, int32_t(uint16_t* volumeLeft,
                                            uint16_t* volumeRight));
  MOCK_METHOD0(InitSpeaker, int32_t());
  MOCK_CONST_METHOD0(SpeakerIsInitialized, bool());
  MOCK_METHOD0(InitMicrophone, int32_t());
  MOCK_CONST_METHOD0(MicrophoneIsInitialized, bool());
  MOCK_METHOD1(SpeakerVolumeIsAvailable, int32_t(bool* available));
  MOCK_METHOD1(SetSpeakerVolume, int32_t(uint32_t volume));
  MOCK_CONST_METHOD1(SpeakerVolume, int32_t(uint32_t* volume));
  MOCK_CONST_METHOD1(MaxSpeakerVolume, int32_t(uint32_t* maxVolume));
  MOCK_CONST_METHOD1(MinSpeakerVolume, int32_t(uint32_t* minVolume));
  MOCK_CONST_METHOD1(SpeakerVolumeStepSize, int32_t(uint16_t* stepSize));
  MOCK_METHOD1(MicrophoneVolumeIsAvailable, int32_t(bool* available));
  MOCK_METHOD1(SetMicrophoneVolume, int32_t(uint32_t volume));
  MOCK_CONST_METHOD1(MicrophoneVolume, int32_t(uint32_t* volume));
  MOCK_CONST_METHOD1(MaxMicrophoneVolume, int32_t(uint32_t* maxVolume));
  MOCK_CONST_METHOD1(MinMicrophoneVolume, int32_t(uint32_t* minVolume));
  MOCK_CONST_METHOD1(MicrophoneVolumeStepSize, int32_t(uint16_t* stepSize));
  MOCK_METHOD1(SpeakerMuteIsAvailable, int32_t(bool* available));
  MOCK_METHOD1(SetSpeakerMute, int32_t(bool enable));
  MOCK_CONST_METHOD1(SpeakerMute, int32_t(bool* enabled));
  MOCK_METHOD1(MicrophoneMuteIsAvailable, int32_t(bool* available));
  MOCK_METHOD1(SetMicrophoneMute, int32_t(bool enable));
  MOCK_CONST_METHOD1(MicrophoneMute, int32_t(bool* enabled));
  MOCK_METHOD1(MicrophoneBoostIsAvailable, int32_t(bool* available));
  MOCK_METHOD1(SetMicrophoneBoost, int32_t(bool enable));
  MOCK_CONST_METHOD1(MicrophoneBoost, int32_t(bool* enabled));
  MOCK_CONST_METHOD1(StereoPlayoutIsAvailable, int32_t(bool* available));
  MOCK_METHOD1(SetStereoPlayout, int32_t(bool enable));
  MOCK_CONST_METHOD1(StereoPlayout, int32_t(bool* enabled));
  MOCK_CONST_METHOD1(StereoRecordingIsAvailable, int32_t(bool* available));
  MOCK_METHOD1(SetStereoRecording, int32_t(bool enable));
  MOCK_CONST_METHOD1(StereoRecording, int32_t(bool* enabled));
  MOCK_METHOD1(SetRecordingChannel, int32_t(const ChannelType channel));
  MOCK_CONST_METHOD1(RecordingChannel, int32_t(ChannelType* channel));
  MOCK_METHOD2(SetPlayoutBuffer, int32_t(const BufferType type,
                                         uint16_t sizeMS));
  MOCK_CONST_METHOD2(PlayoutBuffer, int32_t(BufferType* type,
                                            uint16_t* sizeMS));
  MOCK_CONST_METHOD1(PlayoutDelay, int32_t(uint16_t* delayMS));
  MOCK_CONST_METHOD1(RecordingDelay, int32_t(uint16_t* delayMS));
  MOCK_CONST_METHOD1(CPULoad, int32_t(uint16_t* load));
  MOCK_METHOD1(StartRawOutputFileRecording,
               int32_t(const char pcmFileNameUTF8[kAdmMaxFileNameSize]));
  MOCK_METHOD0(StopRawOutputFileRecording, int32_t());
  MOCK_METHOD1(StartRawInputFileRecording,
               int32_t(const char pcmFileNameUTF8[kAdmMaxFileNameSize]));
  MOCK_METHOD0(StopRawInputFileRecording, int32_t());
  MOCK_METHOD1(SetRecordingSampleRate, int32_t(const uint32_t samplesPerSec));
  MOCK_CONST_METHOD1(RecordingSampleRate, int32_t(uint32_t* samplesPerSec));
  MOCK_METHOD1(SetPlayoutSampleRate, int32_t(const uint32_t samplesPerSec));
  MOCK_CONST_METHOD1(PlayoutSampleRate, int32_t(uint32_t* samplesPerSec));
  MOCK_METHOD0(ResetAudioDevice, int32_t());
  MOCK_METHOD1(SetLoudspeakerStatus, int32_t(bool enable));
  MOCK_CONST_METHOD1(GetLoudspeakerStatus, int32_t(bool* enabled));
  MOCK_CONST_METHOD0(BuiltInAECIsAvailable, bool());
  MOCK_CONST_METHOD0(BuiltInAGCIsAvailable, bool());
  MOCK_CONST_METHOD0(BuiltInNSIsAvailable, bool());
  MOCK_METHOD1(EnableBuiltInAEC, int32_t(bool enable));
  MOCK_METHOD1(EnableBuiltInAGC, int32_t(bool enable));
  MOCK_METHOD1(EnableBuiltInNS, int32_t(bool enable));
#if defined(WEBRTC_IOS)
  MOCK_CONST_METHOD1(GetPlayoutAudioParameters, int(AudioParameters* params));
  MOCK_CONST_METHOD1(GetRecordAudioParameters, int(AudioParameters* params));
#endif  // WEBRTC_IOS
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_INCLUDE_MOCK_AUDIO_DEVICE_H_
