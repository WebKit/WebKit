/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_
#define API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_

#include <jni.h>

#include "common_video/include/i420_buffer_pool.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/base/adaptedvideotracksource.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timestampaligner.h"
#include "sdk/android/src/jni/videoframe.h"

namespace webrtc {
namespace jni {

class AndroidVideoTrackSource : public rtc::AdaptedVideoTrackSource {
 public:
  AndroidVideoTrackSource(rtc::Thread* signaling_thread,
                          JNIEnv* jni,
                          bool is_screencast = false);
  ~AndroidVideoTrackSource() override;

  bool is_screencast() const override;

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  absl::optional<bool> needs_denoising() const override;

  // Called by the native capture observer
  void SetState(SourceState state);

  SourceState state() const override;

  bool remote() const override;

  void OnFrameCaptured(JNIEnv* jni,
                       int width,
                       int height,
                       int64_t timestamp_ns,
                       VideoRotation rotation,
                       const JavaRef<jobject>& j_video_frame_buffer);

  void OnOutputFormatRequest(int width, int height, int fps);

 private:
  rtc::Thread* signaling_thread_;
  rtc::AsyncInvoker invoker_;
  rtc::ThreadChecker camera_thread_checker_;
  SourceState state_;
  rtc::TimestampAligner timestamp_aligner_;
  const bool is_screencast_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // API_ANDROID_JNI_ANDROIDVIDEOTRACKSOURCE_H_
