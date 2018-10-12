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

#if defined(WEBRTC_WIN)
#include "modules/audio_device/win/audio_device_module_win.h"
#include "modules/audio_device/win/core_audio_input_win.h"
#include "modules/audio_device/win/core_audio_output_win.h"
#include "modules/audio_device/win/core_audio_utility_win.h"
#endif

#include "absl/memory/memory.h"
#include "rtc_base/logging.h"

namespace webrtc {

rtc::scoped_refptr<AudioDeviceModule>
CreateWindowsCoreAudioAudioDeviceModule() {
  RTC_DLOG(INFO) << __FUNCTION__;
  return CreateWindowsCoreAudioAudioDeviceModuleForTest();
}

rtc::scoped_refptr<AudioDeviceModuleForTest>
CreateWindowsCoreAudioAudioDeviceModuleForTest() {
  RTC_DLOG(INFO) << __FUNCTION__;
  // Returns NULL if Core Audio is not supported or if COM has not been
  // initialized correctly using webrtc_win::ScopedCOMInitializer.
  if (!webrtc_win::core_audio_utility::IsSupported()) {
    RTC_LOG(LS_ERROR)
        << "Unable to create ADM since Core Audio is not supported";
    return nullptr;
  }
  return CreateWindowsCoreAudioAudioDeviceModuleFromInputAndOutput(
      absl::make_unique<webrtc_win::CoreAudioInput>(),
      absl::make_unique<webrtc_win::CoreAudioOutput>());
}

}  // namespace webrtc
