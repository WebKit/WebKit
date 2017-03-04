/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/native_handle_impl.h"

#include <memory>

#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/keep_ref_until_done.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ref_ptr.h"

using webrtc::NativeHandleBuffer;

namespace webrtc_jni {

Matrix::Matrix(JNIEnv* jni, jfloatArray a) {
  RTC_CHECK_EQ(16, jni->GetArrayLength(a));
  jfloat* ptr = jni->GetFloatArrayElements(a, nullptr);
  for (int i = 0; i < 16; ++i) {
    elem_[i] = ptr[i];
  }
  jni->ReleaseFloatArrayElements(a, ptr, 0);
}

jfloatArray Matrix::ToJava(JNIEnv* jni) {
  jfloatArray matrix = jni->NewFloatArray(16);
  jni->SetFloatArrayRegion(matrix, 0, 16, elem_);
  return matrix;
}

void Matrix::Rotate(webrtc::VideoRotation rotation) {
  // Texture coordinates are in the range 0 to 1. The transformation of the last
  // row in each rotation matrix is needed for proper translation, e.g, to
  // mirror x, we don't replace x by -x, but by 1-x.
  switch (rotation) {
    case webrtc::kVideoRotation_0:
      break;
    case webrtc::kVideoRotation_90: {
      const float ROTATE_90[16] =
          { elem_[4], elem_[5], elem_[6], elem_[7],
            -elem_[0], -elem_[1], -elem_[2], -elem_[3],
            elem_[8], elem_[9], elem_[10], elem_[11],
            elem_[0] + elem_[12], elem_[1] + elem_[13],
            elem_[2] + elem_[14], elem_[3] + elem_[15]};
      memcpy(elem_, ROTATE_90, sizeof(elem_));
    } break;
    case webrtc::kVideoRotation_180: {
      const float ROTATE_180[16] =
          { -elem_[0], -elem_[1], -elem_[2], -elem_[3],
            -elem_[4], -elem_[5], -elem_[6], -elem_[7],
            elem_[8], elem_[9], elem_[10], elem_[11],
            elem_[0] + elem_[4] + elem_[12], elem_[1] + elem_[5] + elem_[13],
            elem_[2] + elem_[6] + elem_[14], elem_[3] + elem_[11]+ elem_[15]};
        memcpy(elem_, ROTATE_180, sizeof(elem_));
    } break;
    case webrtc::kVideoRotation_270: {
      const float ROTATE_270[16] =
          { -elem_[4], -elem_[5], -elem_[6], -elem_[7],
            elem_[0], elem_[1], elem_[2], elem_[3],
            elem_[8], elem_[9], elem_[10], elem_[11],
            elem_[4] + elem_[12], elem_[5] + elem_[13],
            elem_[6] + elem_[14], elem_[7] + elem_[15]};
      memcpy(elem_, ROTATE_270, sizeof(elem_));
    } break;
  }
}

// Calculates result = a * b, in column-major order.
void Matrix::Multiply(const float a[16], const float b[16], float result[16]) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      float sum = 0;
      for (int k = 0; k < 4; ++k) {
        sum += a[k * 4 + j] * b[i * 4 + k];
      }
      result[i * 4 + j] = sum;
    }
  }
}

// Center crop by keeping xFraction of the width and yFraction of the height,
// so e.g. cropping from 640x480 to 640x360 would use
// xFraction=1, yFraction=360/480.
void Matrix::Crop(float xFraction,
                  float yFraction,
                  float xOffset,
                  float yOffset) {
  const float crop_matrix[16] =
      {xFraction, 0, 0, 0,
       0, yFraction, 0, 0,
       0, 0, 1, 0,
       xOffset, yOffset, 0, 1};
  const Matrix old = *this;
  Multiply(crop_matrix, old.elem_, this->elem_);
}

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

NativeHandleImpl::NativeHandleImpl(int id, const Matrix& matrix)
    : oes_texture_id(id), sampling_matrix(matrix) {}

NativeHandleImpl::NativeHandleImpl(JNIEnv* jni,
                                   jint j_oes_texture_id,
                                   jfloatArray j_transform_matrix)
    : oes_texture_id(j_oes_texture_id),
      sampling_matrix(jni, j_transform_matrix) {}

AndroidTextureBuffer::AndroidTextureBuffer(
    int width,
    int height,
    const NativeHandleImpl& native_handle,
    jobject surface_texture_helper,
    const rtc::Callback0<void>& no_longer_used)
    : webrtc::NativeHandleBuffer(&native_handle_, width, height),
      native_handle_(native_handle),
      surface_texture_helper_(surface_texture_helper),
      no_longer_used_cb_(no_longer_used) {}

AndroidTextureBuffer::~AndroidTextureBuffer() {
  no_longer_used_cb_();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
AndroidTextureBuffer::NativeToI420Buffer() {
  int uv_width = (width()+7) / 8;
  int stride = 8 * uv_width;
  int uv_height = (height()+1)/2;
  size_t size = stride * (height() + uv_height);
  // The data is owned by the frame, and the normal case is that the
  // data is deleted by the frame's destructor callback.
  //
  // TODO(nisse): Use an I420BufferPool. We then need to extend that
  // class, and I420Buffer, to support our memory layout.
  std::unique_ptr<uint8_t, webrtc::AlignedFreeDeleter> yuv_data(
      static_cast<uint8_t*>(webrtc::AlignedMalloc(size, kBufferAlignment)));
  // See YuvConverter.java for the required layout.
  uint8_t* y_data = yuv_data.get();
  uint8_t* u_data = y_data + height() * stride;
  uint8_t* v_data = u_data + stride/2;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> copy =
    new rtc::RefCountedObject<webrtc::WrappedI420Buffer>(
        width(), height(),
        y_data, stride,
        u_data, stride,
        v_data, stride,
        rtc::Bind(&webrtc::AlignedFree, yuv_data.release()));

  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  jmethodID transform_mid = GetMethodID(
      jni,
      GetObjectClass(jni, surface_texture_helper_),
      "textureToYUV",
      "(Ljava/nio/ByteBuffer;IIII[F)V");

  jobject byte_buffer = jni->NewDirectByteBuffer(y_data, size);

  jfloatArray sampling_matrix = native_handle_.sampling_matrix.ToJava(jni);
  jni->CallVoidMethod(surface_texture_helper_,
                      transform_mid,
                      byte_buffer, width(), height(), stride,
                      native_handle_.oes_texture_id, sampling_matrix);
  CHECK_EXCEPTION(jni) << "textureToYUV throwed an exception";

  return copy;
}

rtc::scoped_refptr<AndroidTextureBuffer>
AndroidTextureBuffer::CropScaleAndRotate(int cropped_width,
                                         int cropped_height,
                                         int crop_x,
                                         int crop_y,
                                         int dst_width,
                                         int dst_height,
                                         webrtc::VideoRotation rotation) {
  if (cropped_width == dst_width && cropped_height == dst_height &&
      width() == dst_width && height() == dst_height &&
      rotation == webrtc::kVideoRotation_0) {
    return this;
  }
  int rotated_width = (rotation % 180 == 0) ? dst_width : dst_height;
  int rotated_height = (rotation % 180 == 0) ? dst_height : dst_width;

  // Here we use Bind magic to add a reference count to |this| until the newly
  // created AndroidTextureBuffer is destructed
  rtc::scoped_refptr<AndroidTextureBuffer> buffer(
      new rtc::RefCountedObject<AndroidTextureBuffer>(
          rotated_width, rotated_height, native_handle_,
          surface_texture_helper_, rtc::KeepRefUntilDone(this)));

  if (cropped_width != width() || cropped_height != height()) {
    buffer->native_handle_.sampling_matrix.Crop(
        cropped_width / static_cast<float>(width()),
        cropped_height / static_cast<float>(height()),
        crop_x / static_cast<float>(width()),
        crop_y / static_cast<float>(height()));
  }
  buffer->native_handle_.sampling_matrix.Rotate(rotation);
  return buffer;
}

}  // namespace webrtc_jni
