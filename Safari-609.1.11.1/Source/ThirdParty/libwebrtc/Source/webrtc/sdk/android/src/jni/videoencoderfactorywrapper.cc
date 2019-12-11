/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videoencoderfactorywrapper.h"

#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/logging.h"
#include "sdk/android/generated_video_jni/jni/VideoEncoderFactory_jni.h"
#include "sdk/android/native_api/jni/class_loader.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/videocodecinfo.h"
#include "sdk/android/src/jni/videoencoderwrapper.h"

namespace webrtc {
namespace jni {

VideoEncoderFactoryWrapper::VideoEncoderFactoryWrapper(
    JNIEnv* jni,
    const JavaRef<jobject>& encoder_factory)
    : encoder_factory_(jni, encoder_factory) {
  const ScopedJavaLocalRef<jobjectArray> j_supported_codecs =
      Java_VideoEncoderFactory_getSupportedCodecs(jni, encoder_factory);
  supported_formats_ = JavaToNativeVector<SdpVideoFormat>(
      jni, j_supported_codecs, &VideoCodecInfoToSdpVideoFormat);
}
VideoEncoderFactoryWrapper::~VideoEncoderFactoryWrapper() = default;

std::unique_ptr<VideoEncoder> VideoEncoderFactoryWrapper::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_codec_info =
      SdpVideoFormatToVideoCodecInfo(jni, format);
  ScopedJavaLocalRef<jobject> encoder = Java_VideoEncoderFactory_createEncoder(
      jni, encoder_factory_, j_codec_info);
  if (!encoder.obj())
    return nullptr;
  return JavaToNativeVideoEncoder(jni, encoder);
}

std::vector<SdpVideoFormat> VideoEncoderFactoryWrapper::GetSupportedFormats()
    const {
  return supported_formats_;
}

VideoEncoderFactory::CodecInfo VideoEncoderFactoryWrapper::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_codec_info =
      SdpVideoFormatToVideoCodecInfo(jni, format);
  ScopedJavaLocalRef<jobject> encoder = Java_VideoEncoderFactory_createEncoder(
      jni, encoder_factory_, j_codec_info);

  CodecInfo codec_info;
  // Check if this is a wrapped native software encoder implementation.
  codec_info.is_hardware_accelerated = IsHardwareVideoEncoder(jni, encoder);
  codec_info.has_internal_source = false;
  return codec_info;
}

}  // namespace jni
}  // namespace webrtc
