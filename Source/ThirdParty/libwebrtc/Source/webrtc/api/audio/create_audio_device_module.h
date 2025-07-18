/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CREATE_AUDIO_DEVICE_MODULE_H_
#define API_AUDIO_CREATE_AUDIO_DEVICE_MODULE_H_

#include "absl/base/nullability.h"
#include "api/audio/audio_device.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"

namespace webrtc {

absl_nullable scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
    const Environment& env,
    AudioDeviceModule::AudioLayer audio_layer);

}  // namespace webrtc

#endif  // API_AUDIO_CREATE_AUDIO_DEVICE_MODULE_H_
