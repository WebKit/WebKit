/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/encodedimage.h"

#include "api/video/encoded_image.h"
#include "rtc_base/timeutils.h"
#include "sdk/android/generated_video_jni/jni/EncodedImage_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

ScopedJavaLocalRef<jobject> NativeToJavaFrameType(JNIEnv* env,
                                                  FrameType frame_type) {
  return Java_FrameType_fromNativeIndex(env, frame_type);
}

ScopedJavaLocalRef<jobject> NativeToJavaEncodedImage(
    JNIEnv* jni,
    const EncodedImage& image) {
  ScopedJavaLocalRef<jobject> buffer =
      NewDirectByteBuffer(jni, image._buffer, image._length);
  ScopedJavaLocalRef<jobject> frame_type =
      NativeToJavaFrameType(jni, image._frameType);
  ScopedJavaLocalRef<jobject> qp;
  if (image.qp_ != -1)
    qp = NativeToJavaInteger(jni, image.qp_);
  return Java_EncodedImage_Constructor(
      jni, buffer, static_cast<int>(image._encodedWidth),
      static_cast<int>(image._encodedHeight),
      image.capture_time_ms_ * rtc::kNumNanosecsPerMillisec, frame_type,
      static_cast<jint>(image.rotation_), image._completeFrame, qp);
}

ScopedJavaLocalRef<jobjectArray> NativeToJavaFrameTypeArray(
    JNIEnv* env,
    const std::vector<FrameType>& frame_types) {
  return NativeToJavaObjectArray(
      env, frame_types, org_webrtc_EncodedImage_00024FrameType_clazz(env),
      &NativeToJavaFrameType);
}

}  // namespace jni
}  // namespace webrtc
