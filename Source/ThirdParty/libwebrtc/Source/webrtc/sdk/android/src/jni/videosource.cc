/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/videosourceproxy.h"
#include "rtc_base/logging.h"
#include "sdk/android/generated_video_jni/jni/VideoSource_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/androidvideotracksource.h"

namespace webrtc {
namespace jni {

namespace {
AndroidVideoTrackSource* AndroidVideoTrackSourceFromJavaProxy(jlong j_proxy) {
  auto* proxy_source = reinterpret_cast<VideoTrackSourceProxy*>(j_proxy);
  return reinterpret_cast<AndroidVideoTrackSource*>(proxy_source->internal());
}
}  // namespace

static jlong JNI_VideoSource_GetInternalSource(JNIEnv* jni,
                                               const JavaParamRef<jclass>&,
                                               jlong j_source) {
  return NativeToJavaPointer(AndroidVideoTrackSourceFromJavaProxy(j_source));
}

static void JNI_VideoSource_AdaptOutputFormat(JNIEnv* jni,
                                              const JavaParamRef<jclass>&,
                                              jlong j_source,
                                              jint j_landscape_width,
                                              jint j_landscape_height,
                                              jint j_portrait_width,
                                              jint j_portrait_height,
                                              jint j_fps) {
  RTC_LOG(LS_INFO) << "VideoSource_nativeAdaptOutputFormat";
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->OnOutputFormatRequest(j_landscape_width, j_landscape_height,
                                j_portrait_width, j_portrait_height, j_fps);
}

}  // namespace jni
}  // namespace webrtc
