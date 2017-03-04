/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>

#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/sdk/android/src/jni/native_handle_impl.h"
#include "webrtc/system_wrappers/include/metrics.h"

// Enables collection of native histograms and creating them.
namespace webrtc_jni {

JOW(jlong, Histogram_nativeCreateCounts)
(JNIEnv* jni, jclass, jstring j_name, jint min, jint max, jint buckets) {
  std::string name = JavaToStdString(jni, j_name);
  return jlongFromPointer(
      webrtc::metrics::HistogramFactoryGetCounts(name, min, max, buckets));
}

JOW(jlong, Histogram_nativeCreateEnumeration)
(JNIEnv* jni, jclass, jstring j_name, jint max) {
  std::string name = JavaToStdString(jni, j_name);
  return jlongFromPointer(
      webrtc::metrics::HistogramFactoryGetEnumeration(name, max));
}

JOW(void, Histogram_nativeAddSample)
(JNIEnv* jni, jclass, jlong histogram, jint sample) {
  if (histogram) {
    HistogramAdd(reinterpret_cast<webrtc::metrics::Histogram*>(histogram),
                 sample);
  }
}

}  // namespace webrtc_jni
