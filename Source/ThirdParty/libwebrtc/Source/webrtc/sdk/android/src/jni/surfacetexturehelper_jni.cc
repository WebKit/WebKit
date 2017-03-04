/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "webrtc/sdk/android/src/jni/surfacetexturehelper_jni.h"

#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc_jni {

rtc::scoped_refptr<SurfaceTextureHelper> SurfaceTextureHelper::create(
    JNIEnv* jni,
    const char* thread_name,
    jobject j_egl_context) {
  jobject j_surface_texture_helper = jni->CallStaticObjectMethod(
      FindClass(jni, "org/webrtc/SurfaceTextureHelper"),
      GetStaticMethodID(jni, FindClass(jni, "org/webrtc/SurfaceTextureHelper"),
                        "create",
                        "(Ljava/lang/String;Lorg/webrtc/EglBase$Context;)"
                        "Lorg/webrtc/SurfaceTextureHelper;"),
      jni->NewStringUTF(thread_name), j_egl_context);
  CHECK_EXCEPTION(jni)
      << "error during initialization of Java SurfaceTextureHelper";
  if (IsNull(jni, j_surface_texture_helper))
    return nullptr;
  return new rtc::RefCountedObject<SurfaceTextureHelper>(
      jni, j_surface_texture_helper);
}

SurfaceTextureHelper::SurfaceTextureHelper(JNIEnv* jni,
                                           jobject j_surface_texture_helper)
    : j_surface_texture_helper_(jni, j_surface_texture_helper),
      j_return_texture_method_(
          GetMethodID(jni,
                      FindClass(jni, "org/webrtc/SurfaceTextureHelper"),
                      "returnTextureFrame",
                      "()V")) {
  CHECK_EXCEPTION(jni) << "error during initialization of SurfaceTextureHelper";
}

SurfaceTextureHelper::~SurfaceTextureHelper() {
  LOG(LS_INFO) << "SurfaceTextureHelper dtor";
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  jni->CallVoidMethod(
      *j_surface_texture_helper_,
      GetMethodID(jni, FindClass(jni, "org/webrtc/SurfaceTextureHelper"),
                  "dispose", "()V"));

  CHECK_EXCEPTION(jni) << "error during SurfaceTextureHelper.dispose()";
}

jobject SurfaceTextureHelper::GetJavaSurfaceTextureHelper() const {
  return *j_surface_texture_helper_;
}

void SurfaceTextureHelper::ReturnTextureFrame() const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  jni->CallVoidMethod(*j_surface_texture_helper_, j_return_texture_method_);

  CHECK_EXCEPTION(
      jni) << "error during SurfaceTextureHelper.returnTextureFrame";
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
SurfaceTextureHelper::CreateTextureFrame(int width, int height,
    const NativeHandleImpl& native_handle) {
  return new rtc::RefCountedObject<AndroidTextureBuffer>(
      width, height, native_handle, *j_surface_texture_helper_,
      rtc::Bind(&SurfaceTextureHelper::ReturnTextureFrame, this));
}

}  // namespace webrtc_jni
