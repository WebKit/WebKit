/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/i420_buffer.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/video_frame.h"
#include "sdk/android/src/jni/wrapped_native_i420_buffer.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jint,
                         VideoFrameBufferTest_nativeGetBufferType,
                         JNIEnv* jni,
                         jclass,
                         jobject video_frame_buffer) {
  const JavaParamRef<jobject> j_video_frame_buffer(video_frame_buffer);
  rtc::scoped_refptr<VideoFrameBuffer> buffer =
      JavaToNativeFrameBuffer(jni, j_video_frame_buffer);
  return static_cast<jint>(buffer->type());
}

JNI_FUNCTION_DECLARATION(jobject,
                         VideoFrameBufferTest_nativeGetNativeI420Buffer,
                         JNIEnv* jni,
                         jclass,
                         jobject i420_buffer) {
  const JavaParamRef<jobject> j_i420_buffer(i420_buffer);
  rtc::scoped_refptr<VideoFrameBuffer> buffer =
      JavaToNativeFrameBuffer(jni, j_i420_buffer);
  const I420BufferInterface* inputBuffer = buffer->GetI420();
  RTC_DCHECK(inputBuffer != nullptr);
  rtc::scoped_refptr<I420Buffer> outputBuffer = I420Buffer::Copy(*inputBuffer);
  return WrapI420Buffer(jni, outputBuffer).Release();
}

}  // namespace jni
}  // namespace webrtc
