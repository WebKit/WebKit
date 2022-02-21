/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_ADD_ICE_CANDIDATE_OBSERVER_H_
#define SDK_ANDROID_SRC_JNI_PC_ADD_ICE_CANDIDATE_OBSERVER_H_

#include <memory>
#include <string>

#include "api/peer_connection_interface.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

class AddIceCandidateObserverJni final
    : public rtc::RefCountedNonVirtual<AddIceCandidateObserverJni> {
 public:
  AddIceCandidateObserverJni(JNIEnv* env, const JavaRef<jobject>& j_observer);
  ~AddIceCandidateObserverJni() = default;

  void OnComplete(RTCError error);

 private:
  const ScopedJavaGlobalRef<jobject> j_observer_global_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_ADD_ICE_CANDIDATE_OBSERVER_H_
