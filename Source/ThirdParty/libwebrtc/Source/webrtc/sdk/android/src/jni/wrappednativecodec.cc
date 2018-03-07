/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/wrappednativecodec.h"

#include "sdk/android/generated_video_jni/jni/WrappedNativeVideoDecoder_jni.h"
#include "sdk/android/generated_video_jni/jni/WrappedNativeVideoEncoder_jni.h"
#include "sdk/android/src/jni/class_loader.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/videodecoderwrapper.h"
#include "sdk/android/src/jni/videoencoderwrapper.h"

namespace webrtc {
namespace jni {

std::unique_ptr<VideoDecoder> JavaToNativeVideoDecoder(JNIEnv* jni,
                                                       jobject j_decoder) {
  jclass wrapped_native_decoder_class =
      GetClass(jni, "org/webrtc/WrappedNativeVideoDecoder");

  VideoDecoder* decoder;
  if (jni->IsInstanceOf(j_decoder, wrapped_native_decoder_class)) {
    jlong native_decoder =
        Java_WrappedNativeVideoDecoder_createNativeDecoder(jni, j_decoder);
    decoder = reinterpret_cast<VideoDecoder*>(native_decoder);
  } else {
    decoder = new VideoDecoderWrapper(jni, j_decoder);
  }

  return std::unique_ptr<VideoDecoder>(decoder);
}

std::unique_ptr<VideoEncoder> JavaToNativeVideoEncoder(JNIEnv* jni,
                                                       jobject j_encoder) {
  VideoEncoder* encoder;
  if (Java_WrappedNativeVideoEncoder_isInstanceOf(jni, j_encoder)) {
    jlong native_encoder =
        Java_WrappedNativeVideoEncoder_createNativeEncoder(jni, j_encoder);
    encoder = reinterpret_cast<VideoEncoder*>(native_encoder);
  } else {
    encoder = new VideoEncoderWrapper(jni, j_encoder);
  }

  return std::unique_ptr<VideoEncoder>(encoder);
}

bool IsWrappedSoftwareEncoder(JNIEnv* jni, jobject j_encoder) {
  return Java_WrappedNativeVideoEncoder_isWrappedSoftwareEncoder(jni,
                                                                 j_encoder);
}

}  // namespace jni
}  // namespace webrtc
