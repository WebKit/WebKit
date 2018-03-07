/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "sdk/android/src/jni/surfacetexturehelper_jni.h"

#include "rtc_base/bind.h"
#include "rtc_base/logging.h"
#include "sdk/android/generated_video_jni/jni/SurfaceTextureHelper_jni.h"

namespace webrtc {
namespace jni {

rtc::scoped_refptr<SurfaceTextureHelper> SurfaceTextureHelper::create(
    JNIEnv* jni,
    const char* thread_name,
    jobject j_egl_context) {
  jobject j_surface_texture_helper = Java_SurfaceTextureHelper_create(
      jni, jni->NewStringUTF(thread_name), j_egl_context);
  CHECK_EXCEPTION(jni)
      << "error during initialization of Java SurfaceTextureHelper";
  if (IsNull(jni, j_surface_texture_helper))
    return nullptr;
  return new rtc::RefCountedObject<SurfaceTextureHelper>(
      jni, j_surface_texture_helper);
}

SurfaceTextureHelper::SurfaceTextureHelper(JNIEnv* jni,
                                           jobject j_surface_texture_helper)
    : j_surface_texture_helper_(jni, j_surface_texture_helper) {}

SurfaceTextureHelper::~SurfaceTextureHelper() {
  RTC_LOG(LS_INFO) << "SurfaceTextureHelper dtor";
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  Java_SurfaceTextureHelper_dispose(jni, *j_surface_texture_helper_);
}

jobject SurfaceTextureHelper::GetJavaSurfaceTextureHelper() const {
  return *j_surface_texture_helper_;
}

void SurfaceTextureHelper::ReturnTextureFrame() const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  Java_SurfaceTextureHelper_returnTextureFrame(jni, *j_surface_texture_helper_);
}

rtc::scoped_refptr<VideoFrameBuffer> SurfaceTextureHelper::CreateTextureFrame(
    int width,
    int height,
    const NativeHandleImpl& native_handle) {
  return new rtc::RefCountedObject<AndroidTextureBuffer>(
      width, height, native_handle, *j_surface_texture_helper_,
      rtc::Bind(&SurfaceTextureHelper::ReturnTextureFrame, this));
}

}  // namespace jni
}  // namespace webrtc
