/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "rtc_base/timestampaligner.h"
#include "rtc_base/timeutils.h"
#include "sdk/android/generated_video_jni/jni/TimestampAligner_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static jlong JNI_TimestampAligner_RtcTimeNanos(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller) {
  return rtc::TimeNanos();
}

static jlong JNI_TimestampAligner_CreateTimestampAligner(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller) {
  return jlongFromPointer(new rtc::TimestampAligner());
}

static void JNI_TimestampAligner_ReleaseTimestampAligner(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    jlong timestamp_aligner) {
  delete reinterpret_cast<rtc::TimestampAligner*>(timestamp_aligner);
}

static jlong JNI_TimestampAligner_TranslateTimestamp(
    JNIEnv* env,
    const JavaParamRef<jclass>& j_caller,
    jlong timestamp_aligner,
    jlong camera_time_ns) {
  return reinterpret_cast<rtc::TimestampAligner*>(timestamp_aligner)
             ->TranslateTimestamp(camera_time_ns / rtc::kNumNanosecsPerMicrosec,
                                  rtc::TimeMicros()) *
         rtc::kNumNanosecsPerMicrosec;
}

}  // namespace jni
}  // namespace webrtc
