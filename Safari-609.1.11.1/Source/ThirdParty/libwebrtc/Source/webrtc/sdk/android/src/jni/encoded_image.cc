/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/encoded_image.h"

#include "api/video/encoded_image.h"
#include "rtc_base/time_utils.h"
#include "sdk/android/generated_video_jni/EncodedImage_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

ScopedJavaLocalRef<jobject> NativeToJavaFrameType(JNIEnv* env,
                                                  VideoFrameType frame_type) {
  return Java_FrameType_fromNativeIndex(env, static_cast<int>(frame_type));
}

ScopedJavaLocalRef<jobject> NativeToJavaEncodedImage(
    JNIEnv* jni,
    const EncodedImage& image) {
  ScopedJavaLocalRef<jobject> buffer = NewDirectByteBuffer(
      jni, const_cast<uint8_t*>(image.data()), image.size());
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
    const std::vector<VideoFrameType>& frame_types) {
  return NativeToJavaObjectArray(
      env, frame_types, org_webrtc_EncodedImage_00024FrameType_clazz(env),
      &NativeToJavaFrameType);
}

EncodedImage JavaToNativeEncodedImage(JNIEnv* env,
                                      const JavaRef<jobject>& j_encoded_image) {
  const JavaRef<jobject>& j_buffer =
      Java_EncodedImage_getBuffer(env, j_encoded_image);
  const uint8_t* buffer =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(j_buffer.obj()));
  const size_t buffer_size = env->GetDirectBufferCapacity(j_buffer.obj());

  EncodedImage frame;
  frame.Allocate(buffer_size);
  frame.set_size(buffer_size);
  memcpy(frame.data(), buffer, buffer_size);
  frame._encodedWidth = Java_EncodedImage_getEncodedWidth(env, j_encoded_image);
  frame._encodedHeight =
      Java_EncodedImage_getEncodedHeight(env, j_encoded_image);
  frame.rotation_ =
      (VideoRotation)Java_EncodedImage_getRotation(env, j_encoded_image);
  frame._completeFrame =
      Java_EncodedImage_getCompleteFrame(env, j_encoded_image);

  frame.qp_ = JavaToNativeOptionalInt(
                  env, Java_EncodedImage_getQp(env, j_encoded_image))
                  .value_or(-1);

  frame._frameType =
      (VideoFrameType)Java_EncodedImage_getFrameType(env, j_encoded_image);
  return frame;
}

int64_t GetJavaEncodedImageCaptureTimeNs(
    JNIEnv* env,
    const JavaRef<jobject>& j_encoded_image) {
  return Java_EncodedImage_getCaptureTimeNs(env, j_encoded_image);
}

}  // namespace jni
}  // namespace webrtc
