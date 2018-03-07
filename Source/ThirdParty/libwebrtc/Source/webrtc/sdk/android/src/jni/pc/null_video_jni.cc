/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/video_jni.h"

namespace webrtc {
namespace jni {

VideoEncoderFactory* CreateVideoEncoderFactory(JNIEnv* jni,
                                               jobject j_encoder_factory) {
  return nullptr;
}

VideoDecoderFactory* CreateVideoDecoderFactory(JNIEnv* jni,
                                               jobject j_decoder_factory) {
  return nullptr;
}

cricket::WebRtcVideoEncoderFactory* CreateLegacyVideoEncoderFactory() {
  return nullptr;
}

cricket::WebRtcVideoDecoderFactory* CreateLegacyVideoDecoderFactory() {
  return nullptr;
}

VideoEncoderFactory* WrapLegacyVideoEncoderFactory(
    cricket::WebRtcVideoEncoderFactory* legacy_encoder_factory) {
  return nullptr;
}

VideoDecoderFactory* WrapLegacyVideoDecoderFactory(
    cricket::WebRtcVideoDecoderFactory* legacy_decoder_factory) {
  return nullptr;
}

jobject GetJavaSurfaceTextureHelper(
    const rtc::scoped_refptr<SurfaceTextureHelper>& surface_texture_helper) {
  return nullptr;
}

}  // namespace jni
}  // namespace webrtc
