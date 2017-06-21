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

#include "third_party/libyuv/include/libyuv/scale.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc_jni {

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_VideoFileRenderer_nativeI420Scale(JNIEnv* jni,
                                                  jclass,
                                                  jobject j_src_buffer_y,
                                                  jint j_src_stride_y,
                                                  jobject j_src_buffer_u,
                                                  jint j_src_stride_u,
                                                  jobject j_src_buffer_v,
                                                  jint j_src_stride_v,
                                                  jint width,
                                                  jint height,
                                                  jbyteArray j_dst_buffer,
                                                  jint dstWidth,
                                                  jint dstHeight) {
  size_t src_size_y = jni->GetDirectBufferCapacity(j_src_buffer_y);
  size_t src_size_u = jni->GetDirectBufferCapacity(j_src_buffer_u);
  size_t src_size_v = jni->GetDirectBufferCapacity(j_src_buffer_v);
  size_t dst_size = jni->GetDirectBufferCapacity(j_dst_buffer);
  int dst_stride = dstWidth;
  RTC_CHECK_GE(src_size_y, j_src_stride_y * height);
  RTC_CHECK_GE(src_size_u, j_src_stride_u * height / 4);
  RTC_CHECK_GE(src_size_v, j_src_stride_v * height / 4);
  RTC_CHECK_GE(dst_size, dst_stride * dstHeight * 3 / 2);
  uint8_t* src_y =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_src_buffer_y));
  uint8_t* src_u =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_src_buffer_u));
  uint8_t* src_v =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_src_buffer_v));
  uint8_t* dst =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_buffer));

  uint8_t* dst_y = dst;
  size_t dst_stride_y = dst_stride;
  uint8_t* dst_u = dst + dst_stride * dstHeight;
  size_t dst_stride_u = dst_stride / 2;
  uint8_t* dst_v = dst + dst_stride * dstHeight * 5 / 4;
  size_t dst_stride_v = dst_stride / 2;

  int ret = libyuv::I420Scale(
      src_y, j_src_stride_y, src_u, j_src_stride_u, src_v, j_src_stride_v,
      width, height, dst_y, dst_stride_y, dst_u, dst_stride_u, dst_v,
      dst_stride_v, dstWidth, dstHeight, libyuv::kFilterBilinear);
  if (ret) {
    LOG(LS_ERROR) << "Error scaling I420 frame: " << ret;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_webrtc_VideoFileRenderer_nativeCreateNativeByteBuffer(JNIEnv* jni,
                                                               jclass,
                                                               jint size) {
  void* new_data = ::operator new(size);
  jobject byte_buffer = jni->NewDirectByteBuffer(new_data, size);
  return byte_buffer;
}

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_VideoFileRenderer_nativeFreeNativeByteBuffer(
    JNIEnv* jni,
    jclass,
    jobject byte_buffer) {
  void* data = jni->GetDirectBufferAddress(byte_buffer);
  ::operator delete(data);
}

}  // namespace webrtc_jni
