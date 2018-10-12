/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/runtime_setting_util.h"
#include "rtc_base/checks.h"

namespace webrtc {

void ReplayRuntimeSetting(AudioProcessing* apm,
                          const webrtc::audioproc::RuntimeSetting& setting) {
  RTC_CHECK(apm);
  // TODO(bugs.webrtc.org/9138): Add ability to handle different types
  // of settings. Currently only CapturePreGain is supported.
  RTC_CHECK(setting.has_capture_pre_gain());
  apm->SetRuntimeSetting(AudioProcessing::RuntimeSetting::CreateCapturePreGain(
      setting.capture_pre_gain()));
}
}  // namespace webrtc
