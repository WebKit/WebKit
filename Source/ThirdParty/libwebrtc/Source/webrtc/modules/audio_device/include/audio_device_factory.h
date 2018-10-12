/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_FACTORY_H_
#define MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_FACTORY_H_

#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

// Creates an AudioDeviceModule (ADM) for Windows based on the Core Audio API.
// The creating thread must be a COM thread; otherwise nullptr will be returned.
// Example (assuming webrtc namespace):
//
//  public:
//   rtc::scoped_refptr<AudioDeviceModule> CreateAudioDevice() {
//     // Tell COM that this thread shall live in the MTA.
//     com_initializer_ = absl::make_unique<webrtc_win::ScopedCOMInitializer>(
//         webrtc_win::ScopedCOMInitializer::kMTA);
//     if (!com_initializer_->Succeeded()) {
//       return nullptr;
//     }
//     return CreateWindowsCoreAudioAudioDeviceModule();
//   }
//
//   private:
//    std::unique_ptr<webrtc_win::ScopedCOMInitializer> com_initializer_;
//
rtc::scoped_refptr<AudioDeviceModule> CreateWindowsCoreAudioAudioDeviceModule();

rtc::scoped_refptr<AudioDeviceModuleForTest>
CreateWindowsCoreAudioAudioDeviceModuleForTest();

}  // namespace webrtc

#endif  //  MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_FACTORY_H_
