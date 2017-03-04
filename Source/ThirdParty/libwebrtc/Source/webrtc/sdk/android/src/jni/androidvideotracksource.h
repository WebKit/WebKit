/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_
#define WEBRTC_API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_

#include "webrtc/sdk/android/src/jni/native_handle_impl.h"
#include "webrtc/sdk/android/src/jni/surfacetexturehelper_jni.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/timestampaligner.h"
#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/media/base/adaptedvideotracksource.h"

namespace webrtc {

class AndroidVideoTrackSource : public rtc::AdaptedVideoTrackSource {
 public:
  AndroidVideoTrackSource(rtc::Thread* signaling_thread,
                          JNIEnv* jni,
                          jobject j_egl_context,
                          bool is_screencast = false);

  bool is_screencast() const override { return is_screencast_; }

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  rtc::Optional<bool> needs_denoising() const override {
    return rtc::Optional<bool>(false);
  }

  // Called by the native capture observer
  void SetState(SourceState state);

  SourceState state() const override { return state_; }

  bool remote() const override { return false; }

  void OnByteBufferFrameCaptured(const void* frame_data,
                                 int length,
                                 int width,
                                 int height,
                                 int rotation,
                                 int64_t timestamp_ns);

  void OnTextureFrameCaptured(int width,
                              int height,
                              int rotation,
                              int64_t timestamp_ns,
                              const webrtc_jni::NativeHandleImpl& handle);

  void OnOutputFormatRequest(int width, int height, int fps);

  rtc::scoped_refptr<webrtc_jni::SurfaceTextureHelper>
  surface_texture_helper() {
    return surface_texture_helper_;
  }

 private:
  rtc::Thread* signaling_thread_;
  rtc::AsyncInvoker invoker_;
  rtc::ThreadChecker camera_thread_checker_;
  SourceState state_;
  rtc::VideoBroadcaster broadcaster_;
  rtc::TimestampAligner timestamp_aligner_;
  webrtc::NV12ToI420Scaler nv12toi420_scaler_;
  webrtc::I420BufferPool buffer_pool_;
  rtc::scoped_refptr<webrtc_jni::SurfaceTextureHelper> surface_texture_helper_;
  const bool is_screencast_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_
