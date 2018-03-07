/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jlong,
                         VP9Encoder_createNativeEncoder,
                         JNIEnv* jni,
                         jobject) {
  return jlongFromPointer(VP9Encoder::Create().release());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         VP9Encoder_isSupported,
                         JNIEnv* jni,
                         jclass) {
  return VP9Encoder::IsSupported();
}

JNI_FUNCTION_DECLARATION(jlong,
                         VP9Decoder_createNativeDecoder,
                         JNIEnv* jni,
                         jobject) {
  return jlongFromPointer(VP9Decoder::Create().release());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         VP9Decoder_isSupported,
                         JNIEnv* jni,
                         jclass) {
  return VP9Decoder::IsSupported();
}

}  // namespace jni
}  // namespace webrtc
