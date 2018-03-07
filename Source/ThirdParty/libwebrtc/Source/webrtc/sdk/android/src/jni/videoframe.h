/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_VIDEOFRAME_H_
#define SDK_ANDROID_SRC_JNI_VIDEOFRAME_H_

#include <jni.h>

#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/callback.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

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

  void Rotate(VideoRotation rotation);

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
class AndroidVideoFrameBuffer : public VideoFrameBuffer {
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

  rtc::scoped_refptr<I420BufferInterface> ToI420() override;

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
  // Creates a native VideoFrameBuffer from a Java VideoFrame.Buffer.
  static rtc::scoped_refptr<AndroidVideoBuffer> Create(
      JNIEnv* jni,
      jobject j_video_frame_buffer);

  // Similar to the Create() above, but adopts and takes ownership of the Java
  // VideoFrame.Buffer. I.e. retain() will not be called, but release() will be
  // called when the returned AndroidVideoBuffer is destroyed.
  static rtc::scoped_refptr<AndroidVideoBuffer> Adopt(
      JNIEnv* jni,
      jobject j_video_frame_buffer);

  ~AndroidVideoBuffer() override;

  jobject video_frame_buffer() const;

  // Crops a region defined by |crop_x|, |crop_y|, |crop_width| and
  // |crop_height|. Scales it to size |scale_width| x |scale_height|.
  rtc::scoped_refptr<AndroidVideoBuffer> CropAndScale(JNIEnv* jni,
                                                      int crop_x,
                                                      int crop_y,
                                                      int crop_width,
                                                      int crop_height,
                                                      int scale_width,
                                                      int scale_height);

  // Returns an instance of VideoRenderer.I420Frame (deprecated)
  jobject ToJavaI420Frame(JNIEnv* jni, int rotation);

 protected:
  // Should not be called directly. Adopts the Java VideoFrame.Buffer. Use
  // Create() or Adopt() instead for clarity.
  AndroidVideoBuffer(JNIEnv* jni, jobject j_video_frame_buffer);

 private:
  Type type() const override;
  int width() const override;
  int height() const override;

  rtc::scoped_refptr<I420BufferInterface> ToI420() override;

  AndroidType android_type() override { return AndroidType::kJavaBuffer; }

  const int width_;
  const int height_;
  // Holds a VideoFrame.Buffer.
  const ScopedGlobalRef<jobject> j_video_frame_buffer_;
};

VideoFrame JavaToNativeFrame(JNIEnv* jni,
                             jobject j_video_frame,
                             uint32_t timestamp_rtp);

jobject NativeToJavaFrame(JNIEnv* jni, const VideoFrame& frame);

int64_t GetJavaVideoFrameTimestampNs(JNIEnv* jni, jobject j_video_frame);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_VIDEOFRAME_H_
