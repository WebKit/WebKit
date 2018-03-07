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

#include "common_video/include/video_frame.h"
#include "rtc_base/timeutils.h"
#include "sdk/android/generated_video_jni/jni/EncodedImage_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

jobject NativeToJavaFrameType(JNIEnv* env, FrameType frame_type) {
  return Java_FrameType_fromNativeIndex(env, frame_type);
}

jobject NativeToJavaEncodedImage(JNIEnv* jni, const EncodedImage& image) {
  jobject buffer = jni->NewDirectByteBuffer(image._buffer, image._length);
  jobject frame_type = NativeToJavaFrameType(jni, image._frameType);
  jobject qp =
      (image.qp_ == -1) ? nullptr : NativeToJavaInteger(jni, image.qp_);
  return Java_EncodedImage_Constructor(
      jni, buffer, image._encodedWidth, image._encodedHeight,
      image.capture_time_ms_ * rtc::kNumNanosecsPerMillisec, frame_type,
      static_cast<jint>(image.rotation_), image._completeFrame, qp);
}

jobjectArray NativeToJavaFrameTypeArray(
    JNIEnv* env,
    const std::vector<FrameType>& frame_types) {
  return NativeToJavaObjectArray(
      env, frame_types, org_webrtc_EncodedImage_00024FrameType_clazz(env),
      &NativeToJavaFrameType);
}

}  // namespace jni
}  // namespace webrtc
