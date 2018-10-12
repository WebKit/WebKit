/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/hardware_codecs.h"
#include "rtc_base/checks.h"

#ifdef WEBRTC_ANDROID
#include "modules/video_coding/codecs/test/android_codec_factory_helper.h"
#endif
#ifdef WEBRTC_MAC
#include "modules/video_coding/codecs/test/objc_codec_factory_helper.h"
#endif

namespace webrtc {
namespace test {
std::unique_ptr<VideoEncoderFactory> CreateHardwareEncoderFactory() {
#ifdef WEBRTC_ANDROID
  InitializeAndroidObjects();
  return CreateAndroidEncoderFactory();
#else
#ifdef WEBRTC_MAC
  return CreateObjCEncoderFactory();
#else
  RTC_NOTREACHED() << "Hardware encoder not implemented on this platform.";
  return nullptr;
#endif
#endif
}
std::unique_ptr<VideoDecoderFactory> CreateHardwareDecoderFactory() {
#ifdef WEBRTC_ANDROID
  InitializeAndroidObjects();
  return CreateAndroidDecoderFactory();
#else
#ifdef WEBRTC_MAC
  return CreateObjCDecoderFactory();
#else
  RTC_NOTREACHED() << "Hardware decoder not implemented on this platform.";
  return nullptr;
#endif
#endif
}
}  // namespace test
}  // namespace webrtc
