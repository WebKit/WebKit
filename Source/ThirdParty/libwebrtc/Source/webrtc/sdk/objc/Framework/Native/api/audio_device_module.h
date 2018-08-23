/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_FRAMEWORK_NATIVE_API_AUDIO_DEVICE_MODULE_H_
#define SDK_OBJC_FRAMEWORK_NATIVE_API_AUDIO_DEVICE_MODULE_H_

#include <memory>

#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule();

}  // namespace webrtc

#endif  // SDK_OBJC_FRAMEWORK_NATIVE_API_AUDIO_DEVICE_MODULE_H_
