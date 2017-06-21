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

#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc_jni {

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_FileVideoCapturer_nativeI420ToNV21(JNIEnv* jni,
                                                   jclass,
                                                   jbyteArray j_src_buffer,
                                                   jint width,
                                                   jint height,
                                                   jbyteArray j_dst_buffer) {
  size_t src_size = jni->GetArrayLength(j_src_buffer);
  size_t dst_size = jni->GetArrayLength(j_dst_buffer);
  int src_stride = width;
  int dst_stride = width;
  RTC_CHECK_GE(src_size, src_stride * height * 3 / 2);
  RTC_CHECK_GE(dst_size, dst_stride * height * 3 / 2);

  jbyte* src_bytes = jni->GetByteArrayElements(j_src_buffer, 0);
  uint8_t* src = reinterpret_cast<uint8_t*>(src_bytes);
  jbyte* dst_bytes = jni->GetByteArrayElements(j_dst_buffer, 0);
  uint8_t* dst = reinterpret_cast<uint8_t*>(dst_bytes);

  uint8_t* src_y = src;
  size_t src_stride_y = src_stride;
  uint8_t* src_u = src + src_stride * height;
  size_t src_stride_u = src_stride / 2;
  uint8_t* src_v = src + src_stride * height * 5 / 4;
  size_t src_stride_v = src_stride / 2;

  uint8_t* dst_y = dst;
  size_t dst_stride_y = dst_stride;
  size_t dst_stride_uv = dst_stride;
  uint8_t* dst_uv = dst + dst_stride * height;

  int ret = libyuv::I420ToNV21(src_y, src_stride_y, src_u, src_stride_u, src_v,
                               src_stride_v, dst_y, dst_stride_y, dst_uv,
                               dst_stride_uv, width, height);
  jni->ReleaseByteArrayElements(j_src_buffer, src_bytes, 0);
  jni->ReleaseByteArrayElements(j_dst_buffer, dst_bytes, 0);
  if (ret) {
    LOG(LS_ERROR) << "Error converting I420 frame to NV21: " << ret;
  }
}

}  // namespace webrtc_jni
