/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/wrapped_native_i420_buffer.h"

#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// TODO(magjed): Write a test for this function.
jobject WrapI420Buffer(
    JNIEnv* jni,
    const rtc::scoped_refptr<I420BufferInterface>& i420_buffer) {
  jclass j_wrapped_native_i420_buffer_class =
      FindClass(jni, "org/webrtc/WrappedNativeI420Buffer");
  jmethodID j_wrapped_native_i420_buffer_ctor_id =
      GetMethodID(jni, j_wrapped_native_i420_buffer_class, "<init>",
                  "(IILjava/nio/ByteBuffer;ILjava/nio/ByteBuffer;ILjava/nio/"
                  "ByteBuffer;IJ)V");

  jobject y_buffer =
      jni->NewDirectByteBuffer(const_cast<uint8_t*>(i420_buffer->DataY()),
                               i420_buffer->StrideY() * i420_buffer->height());
  jobject u_buffer = jni->NewDirectByteBuffer(
      const_cast<uint8_t*>(i420_buffer->DataU()),
      i420_buffer->StrideU() * i420_buffer->ChromaHeight());
  jobject v_buffer = jni->NewDirectByteBuffer(
      const_cast<uint8_t*>(i420_buffer->DataV()),
      i420_buffer->StrideV() * i420_buffer->ChromaHeight());

  jobject j_wrapped_native_i420_buffer = jni->NewObject(
      j_wrapped_native_i420_buffer_class, j_wrapped_native_i420_buffer_ctor_id,
      i420_buffer->width(), i420_buffer->height(), y_buffer,
      i420_buffer->StrideY(), u_buffer, i420_buffer->StrideU(), v_buffer,
      i420_buffer->StrideV(), jlongFromPointer(i420_buffer.get()));
  CHECK_EXCEPTION(jni);

  return j_wrapped_native_i420_buffer;
}

}  // namespace jni
}  // namespace webrtc
