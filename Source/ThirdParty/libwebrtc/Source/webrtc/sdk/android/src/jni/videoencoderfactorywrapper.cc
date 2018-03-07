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
#include "sdk/android/src/jni/class_loader.h"
#include "sdk/android/src/jni/videocodecinfo.h"
#include "sdk/android/src/jni/wrappednativecodec.h"

namespace webrtc {
namespace jni {

VideoEncoderFactoryWrapper::VideoEncoderFactoryWrapper(JNIEnv* jni,
                                                       jobject encoder_factory)
    : encoder_factory_(jni, encoder_factory) {
  jclass encoder_factory_class = jni->GetObjectClass(*encoder_factory_);
  create_encoder_method_ = jni->GetMethodID(
      encoder_factory_class, "createEncoder",
      "(Lorg/webrtc/VideoCodecInfo;)Lorg/webrtc/VideoEncoder;");
  get_supported_codecs_method_ =
      jni->GetMethodID(encoder_factory_class, "getSupportedCodecs",
                       "()[Lorg/webrtc/VideoCodecInfo;");

  supported_formats_ = GetSupportedFormats(jni);
}

std::unique_ptr<VideoEncoder> VideoEncoderFactoryWrapper::CreateVideoEncoder(
    const SdpVideoFormat& format) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject j_codec_info = SdpVideoFormatToVideoCodecInfo(jni, format);
  jobject encoder = jni->CallObjectMethod(*encoder_factory_,
                                          create_encoder_method_, j_codec_info);
  return encoder != nullptr ? JavaToNativeVideoEncoder(jni, encoder) : nullptr;
}

VideoEncoderFactory::CodecInfo VideoEncoderFactoryWrapper::QueryVideoEncoder(
    const SdpVideoFormat& format) const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  jobject j_codec_info = SdpVideoFormatToVideoCodecInfo(jni, format);
  jobject encoder = jni->CallObjectMethod(*encoder_factory_,
                                          create_encoder_method_, j_codec_info);

  CodecInfo codec_info;
  // Check if this is a wrapped native software encoder implementation.
  codec_info.is_hardware_accelerated = !IsWrappedSoftwareEncoder(jni, encoder);
  codec_info.has_internal_source = false;
  return codec_info;
}

std::vector<SdpVideoFormat> VideoEncoderFactoryWrapper::GetSupportedFormats(
    JNIEnv* jni) const {
  const jobjectArray j_supported_codecs = static_cast<jobjectArray>(
      jni->CallObjectMethod(*encoder_factory_, get_supported_codecs_method_));
  const jsize supported_codecs_count = jni->GetArrayLength(j_supported_codecs);

  std::vector<SdpVideoFormat> supported_formats;
  for (jsize i = 0; i < supported_codecs_count; i++) {
    jobject j_supported_codec =
        jni->GetObjectArrayElement(j_supported_codecs, i);
    supported_formats.push_back(
        VideoCodecInfoToSdpVideoFormat(jni, j_supported_codec));
  }
  return supported_formats;
}

}  // namespace jni
}  // namespace webrtc
