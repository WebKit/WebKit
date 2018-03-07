/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_VIDEO_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_VIDEO_JNI_H_

#include <jni.h>

#include "rtc_base/scoped_ref_ptr.h"

namespace cricket {
class WebRtcVideoEncoderFactory;
class WebRtcVideoDecoderFactory;
}  // namespace cricket

namespace webrtc {
class VideoEncoderFactory;
class VideoDecoderFactory;
}  // namespace webrtc

namespace webrtc {
namespace jni {

class SurfaceTextureHelper;

VideoEncoderFactory* CreateVideoEncoderFactory(JNIEnv* jni,
                                               jobject j_encoder_factory);

VideoDecoderFactory* CreateVideoDecoderFactory(JNIEnv* jni,
                                               jobject j_decoder_factory);

cricket::WebRtcVideoEncoderFactory* CreateLegacyVideoEncoderFactory();
cricket::WebRtcVideoDecoderFactory* CreateLegacyVideoDecoderFactory();

VideoEncoderFactory* WrapLegacyVideoEncoderFactory(
    cricket::WebRtcVideoEncoderFactory* legacy_encoder_factory);
VideoDecoderFactory* WrapLegacyVideoDecoderFactory(
    cricket::WebRtcVideoDecoderFactory* legacy_decoder_factory);

jobject GetJavaSurfaceTextureHelper(
    const rtc::scoped_refptr<SurfaceTextureHelper>& surface_texture_helper);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_VIDEO_JNI_H_
