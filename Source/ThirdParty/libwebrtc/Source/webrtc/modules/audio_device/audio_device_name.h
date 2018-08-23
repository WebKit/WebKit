/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_AUDIO_DEVICE_NAME_H_
#define MODULES_AUDIO_DEVICE_AUDIO_DEVICE_NAME_H_

#include <string>
#include <vector>

namespace webrtc {

struct AudioDeviceName {
  // Unique ID of the generic default device.
  static const char kDefaultDeviceId[];

  // Unique ID of the generic default communications device.
  static const char kDefaultCommunicationsDeviceId[];

  AudioDeviceName() = default;
  AudioDeviceName(const std::string device_name, const std::string unique_id);

  ~AudioDeviceName() = default;

  // Support copy and move.
  AudioDeviceName(const AudioDeviceName& other) = default;
  AudioDeviceName(AudioDeviceName&&) = default;
  AudioDeviceName& operator=(const AudioDeviceName&) = default;
  AudioDeviceName& operator=(AudioDeviceName&&) = default;

  bool IsValid();

  std::string device_name;  // Friendly name of the device.
  std::string unique_id;    // Unique identifier for the device.
};

typedef std::vector<AudioDeviceName> AudioDeviceNames;

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_AUDIO_DEVICE_NAME_H_
