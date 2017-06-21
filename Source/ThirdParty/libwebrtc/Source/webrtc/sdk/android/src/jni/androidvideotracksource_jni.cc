/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/video/video_rotation.h"
#include "webrtc/api/videosourceproxy.h"
#include "webrtc/base/logging.h"
#include "webrtc/sdk/android/src/jni/androidvideotracksource.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"

// Identifiers are over 80 characters long so this is needed to fit them on one
// line.
#define JOW_OBSERVER_METHOD(rettype, name) \
  JOW(rettype, AndroidVideoTrackSourceObserver_##name)

static webrtc::VideoRotation jintToVideoRotation(jint rotation) {
  RTC_DCHECK(rotation == 0 || rotation == 90 || rotation == 180 ||
             rotation == 270);
  return static_cast<webrtc::VideoRotation>(rotation);
}

namespace webrtc_jni {

static webrtc::AndroidVideoTrackSource* AndroidVideoTrackSourceFromJavaProxy(
    jlong j_proxy) {
  auto proxy_source = reinterpret_cast<webrtc::VideoTrackSourceProxy*>(j_proxy);
  return reinterpret_cast<webrtc::AndroidVideoTrackSource*>(
      proxy_source->internal());
}

JOW_OBSERVER_METHOD(void, nativeOnByteBufferFrameCaptured)
(JNIEnv* jni,
 jclass,
 jlong j_source,
 jbyteArray j_frame,
 jint length,
 jint width,
 jint height,
 jint rotation,
 jlong timestamp) {
  webrtc::AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  jbyte* bytes = jni->GetByteArrayElements(j_frame, nullptr);
  source->OnByteBufferFrameCaptured(bytes, length, width, height,
                                    jintToVideoRotation(rotation), timestamp);
  jni->ReleaseByteArrayElements(j_frame, bytes, JNI_ABORT);
}

JOW_OBSERVER_METHOD(void, nativeOnTextureFrameCaptured)
(JNIEnv* jni,
 jclass,
 jlong j_source,
 jint j_width,
 jint j_height,
 jint j_oes_texture_id,
 jfloatArray j_transform_matrix,
 jint j_rotation,
 jlong j_timestamp) {
  webrtc::AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->OnTextureFrameCaptured(
      j_width, j_height, jintToVideoRotation(j_rotation), j_timestamp,
      NativeHandleImpl(jni, j_oes_texture_id, j_transform_matrix));
}

JOW_OBSERVER_METHOD(void, nativeCapturerStarted)
(JNIEnv* jni, jclass, jlong j_source, jboolean j_success) {
  LOG(LS_INFO) << "AndroidVideoTrackSourceObserve_nativeCapturerStarted";
  webrtc::AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->SetState(j_success
                       ? webrtc::AndroidVideoTrackSource::SourceState::kLive
                       : webrtc::AndroidVideoTrackSource::SourceState::kEnded);
}

JOW_OBSERVER_METHOD(void, nativeCapturerStopped)
(JNIEnv* jni, jclass, jlong j_source) {
  LOG(LS_INFO) << "AndroidVideoTrackSourceObserve_nativeCapturerStopped";
  webrtc::AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->SetState(webrtc::AndroidVideoTrackSource::SourceState::kEnded);
}

JOW(void, VideoSource_nativeAdaptOutputFormat)
(JNIEnv* jni, jclass, jlong j_source, jint j_width, jint j_height, jint j_fps) {
  LOG(LS_INFO) << "VideoSource_nativeAdaptOutputFormat";
  webrtc::AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->OnOutputFormatRequest(j_width, j_height, j_fps);
}

}  // namespace webrtc_jni
