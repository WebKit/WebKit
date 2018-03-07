/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/ios/audio_device_ios.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

int32_t AudioDeviceIOS::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const {
  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
  return 0;
}

int16_t AudioDeviceIOS::PlayoutDevices() {
  // TODO(henrika): improve.
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int16_t AudioDeviceIOS::RecordingDevices() {
  // TODO(henrika): improve.
  RTC_LOG_F(LS_WARNING) << "Not implemented";
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

int32_t AudioDeviceIOS::MaxSpeakerVolume(uint32_t& maxVolume) const {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MinSpeakerVolume(uint32_t& minVolume) const {
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
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_NOTREACHED() << "Not implemented";
  return -1;
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

int32_t AudioDeviceIOS::StereoRecordingIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetStereoRecording(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
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
  RTC_LOG_F(LS_WARNING) << "Not implemented";
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
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType) {
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

}  // namespace webrtc
