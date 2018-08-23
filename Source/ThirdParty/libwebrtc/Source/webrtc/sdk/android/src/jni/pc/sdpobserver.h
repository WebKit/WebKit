/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_H_
#define SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_H_

#include <memory>
#include <string>

#include "api/peerconnectioninterface.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/sessiondescription.h"

namespace webrtc {
namespace jni {

class CreateSdpObserverJni : public CreateSessionDescriptionObserver {
 public:
  CreateSdpObserverJni(JNIEnv* env,
                       const JavaRef<jobject>& j_observer,
                       std::unique_ptr<MediaConstraintsInterface> constraints);
  ~CreateSdpObserverJni() override;

  MediaConstraintsInterface* constraints() { return constraints_.get(); }

  void OnSuccess(SessionDescriptionInterface* desc) override;
  void OnFailure(RTCError error) override;

 private:
  const ScopedJavaGlobalRef<jobject> j_observer_global_;
  std::unique_ptr<MediaConstraintsInterface> constraints_;
};

class SetSdpObserverJni : public SetSessionDescriptionObserver {
 public:
  SetSdpObserverJni(JNIEnv* env,
                    const JavaRef<jobject>& j_observer,
                    std::unique_ptr<MediaConstraintsInterface> constraints);
  ~SetSdpObserverJni() override;

  MediaConstraintsInterface* constraints() { return constraints_.get(); }

  void OnSuccess() override;
  void OnFailure(RTCError error) override;

 private:
  const ScopedJavaGlobalRef<jobject> j_observer_global_;
  std::unique_ptr<MediaConstraintsInterface> constraints_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_H_
