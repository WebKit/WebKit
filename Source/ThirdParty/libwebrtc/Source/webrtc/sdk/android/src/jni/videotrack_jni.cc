/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/base/logging.h"

namespace webrtc_jni {

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_VideoTrack_nativeAddRenderer(JNIEnv* jni,
                                             jclass,
                                             jlong j_video_track_pointer,
                                             jlong j_renderer_pointer) {
  LOG(LS_INFO) << "VideoTrack::nativeAddRenderer";
  reinterpret_cast<webrtc::VideoTrackInterface*>(j_video_track_pointer)
      ->AddOrUpdateSink(
          reinterpret_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(
              j_renderer_pointer),
          rtc::VideoSinkWants());
}

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_VideoTrack_nativeRemoveRenderer(JNIEnv* jni,
                                                jclass,
                                                jlong j_video_track_pointer,
                                                jlong j_renderer_pointer) {
  reinterpret_cast<webrtc::VideoTrackInterface*>(j_video_track_pointer)
      ->RemoveSink(
          reinterpret_cast<rtc::VideoSinkInterface<webrtc::VideoFrame>*>(
              j_renderer_pointer));
}

}  // namespace webrtc_jni
