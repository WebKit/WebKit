/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/video_jni.h"

namespace webrtc_jni {

cricket::WebRtcVideoEncoderFactory* CreateVideoEncoderFactory() {
  return nullptr;
}

cricket::WebRtcVideoDecoderFactory* CreateVideoDecoderFactory() {
  return nullptr;
}

jobject GetJavaSurfaceTextureHelper(
    const rtc::scoped_refptr<SurfaceTextureHelper>& surface_texture_helper) {
  return nullptr;
}

}  // namespace webrtc_jni
