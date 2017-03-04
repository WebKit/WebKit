/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_SURFACETEXTUREHELPER_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_SURFACETEXTUREHELPER_JNI_H_

#include <jni.h>

#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/sdk/android/src/jni/native_handle_impl.h"
#include "webrtc/api/video/video_frame_buffer.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace webrtc_jni {

// Helper class to create and synchronize access to an Android SurfaceTexture.
// It is used for creating webrtc::VideoFrameBuffers from a SurfaceTexture when
// the SurfaceTexture has been updated.
// When the VideoFrameBuffer is released, this class returns the buffer to the
// java SurfaceTextureHelper so it can be updated safely. The VideoFrameBuffer
// can be released on an arbitrary thread.
// SurfaceTextureHelper is reference counted to make sure that it is not
// destroyed while a VideoFrameBuffer is in use.
// This class is the C++ counterpart of the java class SurfaceTextureHelper.
// It owns the corresponding java object, and calls the java dispose
// method when destroyed.
// Usage:
// 1. Create an instance of this class.
// 2. Get the Java SurfaceTextureHelper with GetJavaSurfaceTextureHelper().
// 3. Register a listener to the Java SurfaceListener and start producing
// new buffers.
// 4. Call CreateTextureFrame to wrap the Java texture in a VideoFrameBuffer.
class SurfaceTextureHelper : public rtc::RefCountInterface {
 public:
  // Might return null if creating the Java SurfaceTextureHelper fails.
  static rtc::scoped_refptr<SurfaceTextureHelper> create(
      JNIEnv* jni, const char* thread_name, jobject j_egl_context);

  jobject GetJavaSurfaceTextureHelper() const;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> CreateTextureFrame(
      int width,
      int height,
      const NativeHandleImpl& native_handle);

  // May be called on arbitrary thread.
  void ReturnTextureFrame() const;

 protected:
  ~SurfaceTextureHelper();
  SurfaceTextureHelper(JNIEnv* jni, jobject j_surface_texture_helper);

 private:
  const ScopedGlobalRef<jobject> j_surface_texture_helper_;
  const jmethodID j_return_texture_method_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_SURFACETEXTUREHELPER_JNI_H_
