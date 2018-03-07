/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_rotation.h"
#include "api/videosourceproxy.h"
#include "rtc_base/logging.h"
#include "sdk/android/src/jni/androidvideotracksource.h"
#include "sdk/android/src/jni/classreferenceholder.h"

namespace webrtc {

namespace {

static VideoRotation jintToVideoRotation(jint rotation) {
  RTC_DCHECK(rotation == 0 || rotation == 90 || rotation == 180 ||
             rotation == 270);
  return static_cast<VideoRotation>(rotation);
}

}  // namespace

namespace jni {

static AndroidVideoTrackSource* AndroidVideoTrackSourceFromJavaProxy(
    jlong j_proxy) {
  auto proxy_source = reinterpret_cast<VideoTrackSourceProxy*>(j_proxy);
  return reinterpret_cast<AndroidVideoTrackSource*>(proxy_source->internal());
}

JNI_FUNCTION_DECLARATION(
    void,
    AndroidVideoTrackSourceObserver_nativeOnByteBufferFrameCaptured,
    JNIEnv* jni,
    jclass,
    jlong j_source,
    jbyteArray j_frame,
    jint length,
    jint width,
    jint height,
    jint rotation,
    jlong timestamp) {
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  jbyte* bytes = jni->GetByteArrayElements(j_frame, nullptr);
  source->OnByteBufferFrameCaptured(bytes, length, width, height,
                                    jintToVideoRotation(rotation), timestamp);
  jni->ReleaseByteArrayElements(j_frame, bytes, JNI_ABORT);
}

JNI_FUNCTION_DECLARATION(
    void,
    AndroidVideoTrackSourceObserver_nativeOnTextureFrameCaptured,
    JNIEnv* jni,
    jclass,
    jlong j_source,
    jint j_width,
    jint j_height,
    jint j_oes_texture_id,
    jfloatArray j_transform_matrix,
    jint j_rotation,
    jlong j_timestamp) {
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->OnTextureFrameCaptured(
      j_width, j_height, jintToVideoRotation(j_rotation), j_timestamp,
      NativeHandleImpl(jni, j_oes_texture_id, j_transform_matrix));
}

JNI_FUNCTION_DECLARATION(void,
                         AndroidVideoTrackSourceObserver_nativeOnFrameCaptured,
                         JNIEnv* jni,
                         jclass,
                         jlong j_source,
                         jint j_width,
                         jint j_height,
                         jint j_rotation,
                         jlong j_timestamp_ns,
                         jobject j_video_frame_buffer) {
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->OnFrameCaptured(jni, j_width, j_height, j_timestamp_ns,
                          jintToVideoRotation(j_rotation),
                          j_video_frame_buffer);
}

JNI_FUNCTION_DECLARATION(void,
                         AndroidVideoTrackSourceObserver_nativeCapturerStarted,
                         JNIEnv* jni,
                         jclass,
                         jlong j_source,
                         jboolean j_success) {
  RTC_LOG(LS_INFO) << "AndroidVideoTrackSourceObserve_nativeCapturerStarted";
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->SetState(j_success ? AndroidVideoTrackSource::SourceState::kLive
                             : AndroidVideoTrackSource::SourceState::kEnded);
}

JNI_FUNCTION_DECLARATION(void,
                         AndroidVideoTrackSourceObserver_nativeCapturerStopped,
                         JNIEnv* jni,
                         jclass,
                         jlong j_source) {
  RTC_LOG(LS_INFO) << "AndroidVideoTrackSourceObserve_nativeCapturerStopped";
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->SetState(AndroidVideoTrackSource::SourceState::kEnded);
}

JNI_FUNCTION_DECLARATION(void,
                         VideoSource_nativeAdaptOutputFormat,
                         JNIEnv* jni,
                         jclass,
                         jlong j_source,
                         jint j_width,
                         jint j_height,
                         jint j_fps) {
  RTC_LOG(LS_INFO) << "VideoSource_nativeAdaptOutputFormat";
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->OnOutputFormatRequest(j_width, j_height, j_fps);
}

}  // namespace jni
}  // namespace webrtc
