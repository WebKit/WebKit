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

#include "sdk/android/src/jni/jni_helpers.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(void,
                         YuvHelper_I420Copy,
                         JNIEnv* jni,
                         jclass,
                         jobject j_src_y,
                         jint src_stride_y,
                         jobject j_src_u,
                         jint src_stride_u,
                         jobject j_src_v,
                         jint src_stride_v,
                         jobject j_dst_y,
                         jint dst_stride_y,
                         jobject j_dst_u,
                         jint dst_stride_u,
                         jobject j_dst_v,
                         jint dst_stride_v,
                         jint width,
                         jint height) {
  const uint8_t* src_y =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_y));
  const uint8_t* src_u =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_u));
  const uint8_t* src_v =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_v));
  uint8_t* dst_y = static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_y));
  uint8_t* dst_u = static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_u));
  uint8_t* dst_v = static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_v));

  libyuv::I420Copy(src_y, src_stride_y, src_u, src_stride_u, src_v,
                   src_stride_v, dst_y, dst_stride_y, dst_u, dst_stride_u,
                   dst_v, dst_stride_v, width, height);
}

JNI_FUNCTION_DECLARATION(void,
                         YuvHelper_I420ToNV12,
                         JNIEnv* jni,
                         jclass,
                         jobject j_src_y,
                         jint src_stride_y,
                         jobject j_src_u,
                         jint src_stride_u,
                         jobject j_src_v,
                         jint src_stride_v,
                         jobject j_dst_y,
                         jint dst_stride_y,
                         jobject j_dst_uv,
                         jint dst_stride_uv,
                         jint width,
                         jint height) {
  const uint8_t* src_y =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_y));
  const uint8_t* src_u =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_u));
  const uint8_t* src_v =
      static_cast<const uint8_t*>(jni->GetDirectBufferAddress(j_src_v));
  uint8_t* dst_y = static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_y));
  uint8_t* dst_uv =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_uv));

  libyuv::I420ToNV12(src_y, src_stride_y, src_u, src_stride_u, src_v,
                     src_stride_v, dst_y, dst_stride_y, dst_uv, dst_stride_uv,
                     width, height);
}

}  // namespace jni
}  // namespace webrtc
