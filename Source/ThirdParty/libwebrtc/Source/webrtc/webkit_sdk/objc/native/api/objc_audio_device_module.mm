/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "objc_audio_device_module.h"

#include "api/make_ref_counted.h"
#include "rtc_base/logging.h"

#include "sdk/objc/native/src/objc_audio_device.h"

namespace webrtc {

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
    id<RTC_OBJC_TYPE(RTCAudioDevice)> audio_device) {
  RTC_DLOG(LS_INFO) << __FUNCTION__;
  return rtc::make_ref_counted<objc_adm::ObjCAudioDeviceModule>(audio_device);
}

}  // namespace webrtc
