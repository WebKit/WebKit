/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/adm_helpers.h"

#include "webrtc/base/logging.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc {
namespace adm_helpers {

// On Windows Vista and newer, Microsoft introduced the concept of "Default
// Communications Device". This means that there are two types of default
// devices (old Wave Audio style default and Default Communications Device).
//
// On Windows systems which only support Wave Audio style default, uses either
// -1 or 0 to select the default device.
//
// Using a #define for AUDIO_DEVICE since we will call *different* versions of
// the ADM functions, depending on the ID type.
#if defined(WEBRTC_WIN)
#define AUDIO_DEVICE_ID \
    (AudioDeviceModule::WindowsDeviceType::kDefaultCommunicationDevice)
#else
#define AUDIO_DEVICE_ID (0u)
#endif  // defined(WEBRTC_WIN)

void SetRecordingDevice(AudioDeviceModule* adm) {
  RTC_DCHECK(adm);

  // Save recording status and stop recording.
  const bool was_recording = adm->Recording();
  if (was_recording && adm->StopRecording() != 0) {
    LOG(LS_ERROR) << "Unable to stop recording.";
    return;
  }

  // Set device and stereo mode.
  if (adm->SetRecordingChannel(AudioDeviceModule::kChannelBoth) != 0) {
    LOG(LS_ERROR) << "Unable to set recording channel to kChannelBoth.";
  }
  if (adm->SetRecordingDevice(AUDIO_DEVICE_ID) != 0) {
    LOG(LS_ERROR) << "Unable to set recording device.";
    return;
  }

  // Init microphone, so user can do volume settings etc.
  if (adm->InitMicrophone() != 0) {
    LOG(LS_ERROR) << "Unable to access microphone.";
  }

  // Set number of channels
  bool available = false;
  if (adm->StereoRecordingIsAvailable(&available) != 0) {
    LOG(LS_ERROR) << "Failed to query stereo recording.";
  }
  if (adm->SetStereoRecording(available) != 0) {
    LOG(LS_ERROR) << "Failed to set stereo recording mode.";
  }

  // Restore recording if it was enabled already when calling this function.
  if (was_recording) {
    if (adm->InitRecording() != 0) {
      LOG(LS_ERROR) << "Failed to initialize recording.";
      return;
    }
    if (adm->StartRecording() != 0) {
      LOG(LS_ERROR) << "Failed to start recording.";
      return;
    }
  }

  LOG(LS_INFO) << "Set recording device.";
}

void SetPlayoutDevice(AudioDeviceModule* adm) {
  RTC_DCHECK(adm);

  // Save playing status and stop playout.
  const bool was_playing = adm->Playing();
  if (was_playing && adm->StopPlayout() != 0) {
    LOG(LS_ERROR) << "Unable to stop playout.";
  }

  // Set device.
  if (adm->SetPlayoutDevice(AUDIO_DEVICE_ID) != 0) {
    LOG(LS_ERROR) << "Unable to set playout device.";
    return;
  }

  // Init speaker, so user can do volume settings etc.
  if (adm->InitSpeaker() != 0) {
    LOG(LS_ERROR) << "Unable to access speaker.";
  }

  // Set number of channels
  bool available = false;
  if (adm->StereoPlayoutIsAvailable(&available) != 0) {
    LOG(LS_ERROR) << "Failed to query stereo playout.";
  }
  if (adm->SetStereoPlayout(available) != 0) {
    LOG(LS_ERROR) << "Failed to set stereo playout mode.";
  }

  // Restore recording if it was enabled already when calling this function.
  if (was_playing) {
    if (adm->InitPlayout() != 0) {
      LOG(LS_ERROR) << "Failed to initialize playout.";
      return;
    }
    if (adm->StartPlayout() != 0) {
      LOG(LS_ERROR) << "Failed to start playout.";
      return;
    }
  }

  LOG(LS_INFO) << "Set playout device.";
}

}  // namespace adm_helpers
}  // namespace webrtc
