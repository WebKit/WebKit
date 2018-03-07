/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_STATSOBSERVER_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_STATSOBSERVER_JNI_H_

#include "api/peerconnectioninterface.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Adapter for a Java StatsObserver presenting a C++ StatsObserver and
// dispatching the callback from C++ back to Java.
class StatsObserverJni : public StatsObserver {
 public:
  StatsObserverJni(JNIEnv* jni, jobject j_observer);

  void OnComplete(const StatsReports& reports) override;

 private:
  const ScopedGlobalRef<jobject> j_observer_global_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_STATSOBSERVER_JNI_H_
