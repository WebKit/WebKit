/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/ios/audio_device_ios.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {

int32_t AudioDeviceIOS::PlayoutBuffer(AudioDeviceModule::BufferType& type,
                                      uint16_t& sizeMS) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const {
  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
  return 0;
}

int32_t AudioDeviceIOS::ResetAudioDevice() {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int16_t AudioDeviceIOS::PlayoutDevices() {
  // TODO(henrika): improve.
  LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int16_t AudioDeviceIOS::RecordingDevices() {
  // TODO(henrika): improve.
  LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int32_t AudioDeviceIOS::InitSpeaker() {
  return 0;
}

bool AudioDeviceIOS::SpeakerIsInitialized() const {
  return true;
}

int32_t AudioDeviceIOS::SpeakerVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetSpeakerVolume(uint32_t volume) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerVolume(uint32_t& volume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SetWaveOutVolume(uint16_t, uint16_t) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::WaveOutVolume(uint16_t&, uint16_t&) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MaxSpeakerVolume(uint32_t& maxVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MinSpeakerVolume(uint32_t& minVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerVolumeStepSize(uint16_t& stepSize) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetSpeakerMute(bool enable) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerMute(bool& enabled) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SetPlayoutDevice(uint16_t index) {
  LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

bool AudioDeviceIOS::PlayoutWarning() const {
  return false;
}

bool AudioDeviceIOS::PlayoutError() const {
  return false;
}

bool AudioDeviceIOS::RecordingWarning() const {
  return false;
}

bool AudioDeviceIOS::RecordingError() const {
  return false;
}

int32_t AudioDeviceIOS::InitMicrophone() {
  return 0;
}

bool AudioDeviceIOS::MicrophoneIsInitialized() const {
  return true;
}

int32_t AudioDeviceIOS::MicrophoneMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetMicrophoneMute(bool enable) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MicrophoneMute(bool& enabled) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MicrophoneBoostIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetMicrophoneBoost(bool enable) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MicrophoneBoost(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t AudioDeviceIOS::StereoRecordingIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetStereoRecording(bool enable) {
  LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::StereoRecording(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t AudioDeviceIOS::StereoPlayoutIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetStereoPlayout(bool enable) {
  LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::StereoPlayout(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t AudioDeviceIOS::SetAGC(bool enable) {
  if (enable) {
    RTC_NOTREACHED() << "Should never be called";
  }
  return -1;
}

bool AudioDeviceIOS::AGC() const {
  return false;
}

int32_t AudioDeviceIOS::MicrophoneVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetMicrophoneVolume(uint32_t volume) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MicrophoneVolume(uint32_t& volume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MinMicrophoneVolume(uint32_t& minVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MicrophoneVolumeStepSize(uint16_t& stepSize) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::PlayoutDeviceName(uint16_t index,
                                          char name[kAdmMaxDeviceNameSize],
                                          char guid[kAdmMaxGuidSize]) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::RecordingDeviceName(uint16_t index,
                                            char name[kAdmMaxDeviceNameSize],
                                            char guid[kAdmMaxGuidSize]) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SetRecordingDevice(uint16_t index) {
  LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::PlayoutIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t AudioDeviceIOS::RecordingIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t AudioDeviceIOS::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type,
    uint16_t sizeMS) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::CPULoad(uint16_t&) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

}  // namespace webrtc
