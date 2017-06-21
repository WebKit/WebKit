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

#include "webrtc/api/video/video_frame.h"
#include "webrtc/api/video/video_frame_buffer.h"
#include "webrtc/api/video/video_rotation.h"
#include "webrtc/base/callback.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc_jni {

// Open gl texture matrix, in column-major order. Operations are
// in-place.
class Matrix {
 public:
  Matrix(JNIEnv* jni, jfloatArray a);

  static Matrix fromAndroidGraphicsMatrix(JNIEnv* jni, jobject j_matrix);

  jfloatArray ToJava(JNIEnv* jni) const;

  // Crop arguments are relative to original size.
  void Crop(float cropped_width,
            float cropped_height,
            float crop_x,
            float crop_y);

  void Rotate(webrtc::VideoRotation rotation);

 private:
  Matrix() {}

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

// Base class to differentiate between the old texture frames and the new
// Java-based frames.
// TODO(sakal): Remove this and AndroidTextureBuffer once they are no longer
// needed.
class AndroidVideoFrameBuffer : public webrtc::VideoFrameBuffer {
 public:
  enum class AndroidType { kTextureBuffer, kJavaBuffer };

  virtual AndroidType android_type() = 0;
};

class AndroidTextureBuffer : public AndroidVideoFrameBuffer {
 public:
  AndroidTextureBuffer(int width,
                       int height,
                       const NativeHandleImpl& native_handle,
                       jobject surface_texture_helper,
                       const rtc::Callback0<void>& no_longer_used);
  ~AndroidTextureBuffer();

  NativeHandleImpl native_handle_impl() const;

 private:
  Type type() const override;
  int width() const override;
  int height() const override;

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;

  AndroidType android_type() override { return AndroidType::kTextureBuffer; }

  const int width_;
  const int height_;
  NativeHandleImpl native_handle_;
  // Raw object pointer, relying on the caller, i.e.,
  // AndroidVideoCapturerJni or the C++ SurfaceTextureHelper, to keep
  // a global reference. TODO(nisse): Make this a reference to the C++
  // SurfaceTextureHelper instead, but that requires some refactoring
  // of AndroidVideoCapturerJni.
  jobject surface_texture_helper_;
  rtc::Callback0<void> no_longer_used_cb_;
};

class AndroidVideoBuffer : public AndroidVideoFrameBuffer {
 public:
  AndroidVideoBuffer(JNIEnv* jni,
                     jmethodID j_retain_id,
                     jmethodID j_release_id,
                     int width,
                     int height,
                     const Matrix& matrix,
                     jobject j_video_frame_buffer);
  ~AndroidVideoBuffer() override;

  jobject video_frame_buffer() const;

  // Returns an instance of VideoRenderer.I420Frame (deprecated)
  jobject ToJavaI420Frame(JNIEnv* jni, int width, int height, int rotation);

 private:
  Type type() const override;
  int width() const override;
  int height() const override;

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;

  AndroidType android_type() override { return AndroidType::kJavaBuffer; }

  const jmethodID j_release_id_;
  const int width_;
  const int height_;
  const Matrix matrix_;
  // Holds a VideoFrame.Buffer.
  ScopedGlobalRef<jobject> j_video_frame_buffer_;
};

class AndroidVideoBufferFactory {
 public:
  explicit AndroidVideoBufferFactory(JNIEnv* jni);

  webrtc::VideoFrame CreateFrame(JNIEnv* jni,
                                 jobject j_video_frame,
                                 uint32_t timestamp_rtp) const;

  rtc::scoped_refptr<AndroidVideoBuffer> CreateBuffer(
      int width,
      int height,
      const Matrix& matrix,
      jobject j_video_frame_buffer) const;

 private:
  ScopedGlobalRef<jclass> j_video_frame_class_;
  jmethodID j_get_buffer_id_;
  jmethodID j_get_width_id_;
  jmethodID j_get_height_id_;
  jmethodID j_get_rotation_id_;
  jmethodID j_get_transform_matrix_id_;
  jmethodID j_get_timestamp_ns_id_;

  ScopedGlobalRef<jclass> j_video_frame_buffer_class_;
  jmethodID j_retain_id_;
  jmethodID j_release_id_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_NATIVE_HANDLE_IMPL_H_
