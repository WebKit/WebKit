/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videodecoderfactorywrapper.h"

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/logging.h"
#include "sdk/android/src/jni/wrappednativecodec.h"

namespace webrtc {
namespace jni {

VideoDecoderFactoryWrapper::VideoDecoderFactoryWrapper(JNIEnv* jni,
                                                       jobject decoder_factory)
    : decoder_factory_(jni, decoder_factory) {
  jclass decoder_factory_class = jni->GetObjectClass(*decoder_factory_);
  create_decoder_method_ =
      jni->GetMethodID(decoder_factory_class, "createDecoder",
                       "(Ljava/lang/String;)Lorg/webrtc/VideoDecoder;");
}

std::unique_ptr<VideoDecoder> VideoDecoderFactoryWrapper::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jstring name = NativeToJavaString(jni, format.name);
  jobject decoder =
      jni->CallObjectMethod(*decoder_factory_, create_decoder_method_, name);
  return decoder != nullptr ? JavaToNativeVideoDecoder(jni, decoder) : nullptr;
}

std::vector<SdpVideoFormat> VideoDecoderFactoryWrapper::GetSupportedFormats()
    const {
  // TODO(andersc): VideoDecoderFactory.java does not have this method.
  return std::vector<SdpVideoFormat>();
}

}  // namespace jni
}  // namespace webrtc
