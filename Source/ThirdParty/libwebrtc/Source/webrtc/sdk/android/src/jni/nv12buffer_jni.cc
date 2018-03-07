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
#include <vector>

#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace jni {

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_NV12Buffer_nativeCropAndScale(JNIEnv* jni,
                                              jclass,
                                              jint crop_x,
                                              jint crop_y,
                                              jint crop_width,
                                              jint crop_height,
                                              jint scale_width,
                                              jint scale_height,
                                              jobject j_src,
                                              jint src_width,
                                              jint src_height,
                                              jint src_stride,
                                              jint src_slice_height,
                                              jobject j_dst_y,
                                              jint dst_stride_y,
                                              jobject j_dst_u,
                                              jint dst_stride_u,
                                              jobject j_dst_v,
                                              jint dst_stride_v) {
  const int src_stride_y = src_stride;
  const int src_stride_uv = src_stride;
  const int crop_chroma_x = crop_x / 2;
  const int crop_chroma_y = crop_y / 2;
  const int crop_chroma_width = (crop_width + 1) / 2;
  const int crop_chroma_height = (crop_height + 1) / 2;
  const int tmp_stride_u = crop_chroma_width;
  const int tmp_stride_v = crop_chroma_width;
  const int tmp_size = crop_chroma_height * (tmp_stride_u + tmp_stride_v);

  uint8_t const* src_y =
      static_cast<uint8_t const*>(jni->GetDirectBufferAddress(j_src));
  uint8_t const* src_uv = src_y + src_slice_height * src_stride_y;

  uint8_t* dst_y = static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_y));
  uint8_t* dst_u = static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_u));
  uint8_t* dst_v = static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_v));

  // Crop using pointer arithmetic.
  src_y += crop_x + crop_y * src_stride_y;
  src_uv += crop_chroma_x + crop_chroma_y * src_stride_uv;

  std::vector<uint8_t> tmp_buffer(tmp_size);
  uint8_t* tmp_u = tmp_buffer.data();
  uint8_t* tmp_v = tmp_u + crop_chroma_height * tmp_stride_u;

  libyuv::SplitUVPlane(src_uv, src_stride_uv, tmp_u, tmp_stride_u, tmp_v,
                       tmp_stride_v, crop_chroma_width, crop_chroma_height);

  libyuv::I420Scale(src_y, src_stride_y, tmp_u, tmp_stride_u, tmp_v,
                    tmp_stride_v, crop_width, crop_height, dst_y, dst_stride_y,
                    dst_u, dst_stride_u, dst_v, dst_stride_v, scale_width,
                    scale_height, libyuv::kFilterBox);
}

}  // namespace jni
}  // namespace webrtc
