/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/create_audio_device_module.h"

#include "absl/base/nullability.h"
#include "api/audio/audio_device.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "modules/audio_device/audio_device_impl.h"

namespace webrtc {

absl_nullable scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
    const Environment& env,
    AudioDeviceModule::AudioLayer audio_layer) {
  return AudioDeviceModuleImpl::Create(env, audio_layer);
}

}  // namespace webrtc
