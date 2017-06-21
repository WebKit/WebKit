/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_RTCSTATSCOLLECTORCALLBACKWRAPPER_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_RTCSTATSCOLLECTORCALLBACKWRAPPER_H_

#include <jni.h>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc_jni {

// Adapter for a Java RTCStatsCollectorCallback presenting a C++
// RTCStatsCollectorCallback and dispatching the callback from C++ back to
// Java.
class RTCStatsCollectorCallbackWrapper
    : public webrtc::RTCStatsCollectorCallback {
 public:
  RTCStatsCollectorCallbackWrapper(JNIEnv* jni, jobject j_callback);

  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;

 private:
  // Helper functions for converting C++ RTCStatsReport to Java equivalent.
  jobject ReportToJava(
      JNIEnv* jni,
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report);
  jobject StatsToJava(JNIEnv* jni, const webrtc::RTCStats& stats);
  jobject MemberToJava(JNIEnv* jni,
                       const webrtc::RTCStatsMemberInterface* member);

  const ScopedGlobalRef<jobject> j_callback_global_;
  const ScopedGlobalRef<jclass> j_callback_class_;
  const jclass j_stats_report_class_;
  const jmethodID j_stats_report_ctor_;
  const jclass j_stats_class_;
  const jmethodID j_stats_ctor_;
  const jclass j_linked_hash_map_class_;
  const jmethodID j_linked_hash_map_ctor_;
  const jmethodID j_linked_hash_map_put_;
  const jclass j_boolean_class_;
  const jmethodID j_boolean_ctor_;
  const jclass j_integer_class_;
  const jmethodID j_integer_ctor_;
  const jclass j_long_class_;
  const jmethodID j_long_ctor_;
  const jclass j_big_integer_class_;
  const jmethodID j_big_integer_ctor_;
  const jclass j_double_class_;
  const jmethodID j_double_ctor_;
  const jclass j_string_class_;
};

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_RTCSTATSCOLLECTORCALLBACKWRAPPER_H_
