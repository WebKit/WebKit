/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/include/audio_device_factory.h"

#include <memory>

#include "api/audio/audio_device.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "rtc_base/logging.h"

#if defined(WEBRTC_WIN)
#include "modules/audio_device/win/audio_device_module_win.h"
#include "modules/audio_device/win/core_audio_input_win.h"
#include "modules/audio_device/win/core_audio_output_win.h"
#include "modules/audio_device/win/core_audio_utility_win.h"
#endif

namespace webrtc {

webrtc::scoped_refptr<AudioDeviceModule>
CreateWindowsCoreAudioAudioDeviceModule(const Environment& env,
                                        bool automatic_restart) {
  RTC_DLOG(LS_INFO) << __FUNCTION__;
  return CreateWindowsCoreAudioAudioDeviceModuleForTest(env, automatic_restart);
}

webrtc::scoped_refptr<AudioDeviceModuleForTest>
CreateWindowsCoreAudioAudioDeviceModuleForTest(const Environment& env,
                                               bool automatic_restart) {
  RTC_DLOG(LS_INFO) << __FUNCTION__;
  // Returns NULL if Core Audio is not supported or if COM has not been
  // initialized correctly using ScopedCOMInitializer.
  if (!webrtc_win::core_audio_utility::IsSupported()) {
    RTC_LOG(LS_ERROR)
        << "Unable to create ADM since Core Audio is not supported";
    return nullptr;
  }
  return CreateWindowsCoreAudioAudioDeviceModuleFromInputAndOutput(
      env, std::make_unique<webrtc_win::CoreAudioInput>(env, automatic_restart),
      std::make_unique<webrtc_win::CoreAudioOutput>(env, automatic_restart));
}

}  // namespace webrtc
