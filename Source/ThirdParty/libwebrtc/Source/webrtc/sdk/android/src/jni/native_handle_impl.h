/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_NATIVE_HANDLE_IMPL_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_NATIVE_HANDLE_IMPL_H_

#include <jni.h>

#include "webrtc/api/video/video_rotation.h"
#include "webrtc/common_video/include/video_frame_buffer.h"

namespace webrtc_jni {

// Open gl texture matrix, in column-major order. Operations are
// in-place.
class Matrix {
 public:
  Matrix(JNIEnv* jni, jfloatArray a);

  jfloatArray ToJava(JNIEnv* jni);

  // Crop arguments are relative to original size.
  void Crop(float cropped_width,
            float cropped_height,
            float crop_x,
            float crop_y);

  void Rotate(webrtc::VideoRotation rotation);

 private:
  static void Multiply(const float a[16], const float b[16], float result[16]);
  float elem_[16];
};

// Wrapper for texture object.
struct NativeHandleImpl {
  NativeHandleImpl(JNIEnv* jni,
                   jint j_oes_texture_id,
                   jfloatArray j_transform_matrix);

  NativeHandleImpl(int id, const Matrix& matrix);

  const int oes_texture_id;
  Matrix sampling_matrix;
};

class AndroidTextureBuffer : public webrtc::NativeHandleBuffer {
 public:
  AndroidTextureBuffer(int width,
                       int height,
                       const NativeHandleImpl& native_handle,
                       jobject surface_texture_helper,
                       const rtc::Callback0<void>& no_longer_used);
  ~AndroidTextureBuffer();
  rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() override;

  // First crop, then scale to dst resolution, and then rotate.
  rtc::scoped_refptr<AndroidTextureBuffer> CropScaleAndRotate(
      int cropped_width,
      int cropped_height,
      int crop_x,
      int crop_y,
      int dst_width,
      int dst_height,
      webrtc::VideoRotation rotation);

 private:
  NativeHandleImpl native_handle_;
  // Raw object pointer, relying on the caller, i.e.,
  // AndroidVideoCapturerJni or the C++ SurfaceTextureHelper, to keep
  // a global reference. TODO(nisse): Make this a reference to the C++
  // SurfaceTextureHelper instead, but that requires some refactoring
  // of AndroidVideoCapturerJni.
  jobject surface_texture_helper_;
  rtc::Callback0<void> no_longer_used_cb_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_NATIVE_HANDLE_IMPL_H_
