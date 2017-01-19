/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/base/logging.h"

namespace webrtc {

int32_t AudioDeviceGeneric::SetRecordingSampleRate(
    const uint32_t samplesPerSec) {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

int32_t AudioDeviceGeneric::SetPlayoutSampleRate(const uint32_t samplesPerSec) {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

int32_t AudioDeviceGeneric::SetLoudspeakerStatus(bool enable) {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

int32_t AudioDeviceGeneric::GetLoudspeakerStatus(bool& enable) const {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

int32_t AudioDeviceGeneric::ResetAudioDevice() {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

int32_t AudioDeviceGeneric::SoundDeviceControl(unsigned int par1,
                                               unsigned int par2,
                                               unsigned int par3,
                                               unsigned int par4) {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

bool AudioDeviceGeneric::BuiltInAECIsAvailable() const {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return false;
}

int32_t AudioDeviceGeneric::EnableBuiltInAEC(bool enable) {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

bool AudioDeviceGeneric::BuiltInAGCIsAvailable() const {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return false;
}

int32_t AudioDeviceGeneric::EnableBuiltInAGC(bool enable) {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

bool AudioDeviceGeneric::BuiltInNSIsAvailable() const {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return false;
}

int32_t AudioDeviceGeneric::EnableBuiltInNS(bool enable) {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

#if defined(WEBRTC_IOS)
int AudioDeviceGeneric::GetPlayoutAudioParameters(
    AudioParameters* params) const {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}

int AudioDeviceGeneric::GetRecordAudioParameters(
    AudioParameters* params) const {
  LOG_F(LS_ERROR) << "Not supported on this platform";
  return -1;
}
#endif  // WEBRTC_IOS

}  // namespace webrtc
