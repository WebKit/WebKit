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

#include "api/audio/audio_device.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"

namespace webrtc {

// Creates an AudioDeviceModule (ADM) for Windows based on the Core Audio API.
// The creating thread must be a COM thread; otherwise nullptr will be returned.
// By default `automatic_restart` is set to true and it results in support for
// automatic restart of audio if e.g. the existing device is removed. If set to
// false, no attempt to restart audio is performed under these conditions.
//
// Example (assuming webrtc namespace):
//
//  public:
//   scoped_refptr<AudioDeviceModule> CreateAudioDevice() {
//     Environment env = CreateEnvironment();
//     // Tell COM that this thread shall live in the MTA.
//     com_initializer_ = std::make_unique<ScopedCOMInitializer>(
//         ScopedCOMInitializer::kMTA);
//     if (!com_initializer_->Succeeded()) {
//       return nullptr;
//     }
//     // Create the ADM with support for automatic restart if devices are
//     // unplugged.
//     return CreateWindowsCoreAudioAudioDeviceModule(env);
//   }
//
//   private:
//    std::unique_ptr<ScopedCOMInitializer> com_initializer_;
//    std::unique_ptr<TaskQueueFactory> task_queue_factory_;
//
webrtc::scoped_refptr<AudioDeviceModule>
CreateWindowsCoreAudioAudioDeviceModule(const Environment& env,
                                        bool automatic_restart = true);

webrtc::scoped_refptr<AudioDeviceModuleForTest>
CreateWindowsCoreAudioAudioDeviceModuleForTest(const Environment& env,
                                               bool automatic_restart = true);

}  // namespace webrtc

#endif  //  MODULES_AUDIO_DEVICE_INCLUDE_AUDIO_DEVICE_FACTORY_H_
