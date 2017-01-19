/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This utility will portably force the volume of the default microphone to max.

#include <stdio.h>

#include "webrtc/modules/audio_device/include/audio_device.h"

using webrtc::AudioDeviceModule;

#if defined(_WIN32)
#define DEFAULT_INPUT_DEVICE (AudioDeviceModule::kDefaultCommunicationDevice)
#else
#define DEFAULT_INPUT_DEVICE (0)
#endif

int main(int /*argc*/, char** /*argv*/) {
  // Create and initialize the ADM.
  rtc::scoped_refptr<AudioDeviceModule> adm(
      AudioDeviceModule::Create(1, AudioDeviceModule::kPlatformDefaultAudio));
  if (!adm.get()) {
    fprintf(stderr, "Failed to create Audio Device Module.\n");
    return 1;
  }
  if (adm->Init() != 0) {
    fprintf(stderr, "Failed to initialize Audio Device Module.\n");
    return 1;
  }
  if (adm->SetRecordingDevice(DEFAULT_INPUT_DEVICE) != 0) {
    fprintf(stderr, "Failed to set the default input device.\n");
    return 1;
  }
  if (adm->InitMicrophone() != 0) {
    fprintf(stderr, "Failed to to initialize the microphone.\n");
    return 1;
  }

  // Set mic volume to max.
  uint32_t max_vol = 0;
  if (adm->MaxMicrophoneVolume(&max_vol) != 0) {
    fprintf(stderr, "Failed to get max volume.\n");
    return 1;
  }
  if (adm->SetMicrophoneVolume(max_vol) != 0) {
    fprintf(stderr, "Failed to set mic volume.\n");
    return 1;
  }

  return 0;
}
