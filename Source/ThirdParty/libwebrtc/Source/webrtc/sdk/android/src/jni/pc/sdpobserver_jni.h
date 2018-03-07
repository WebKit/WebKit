/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_JNI_H_

#include <memory>
#include <string>

#include "api/peerconnectioninterface.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Adapter for a Java StatsObserver presenting a C++
// CreateSessionDescriptionObserver or SetSessionDescriptionObserver and
// dispatching the callback from C++ back to Java.
template <class T>  // T is one of {Create,Set}SessionDescriptionObserver.
class SdpObserverJni : public T {
 public:
  SdpObserverJni(JNIEnv* jni,
                 jobject j_observer,
                 std::unique_ptr<MediaConstraintsInterface> constraints)
      : constraints_(std::move(constraints)),
        j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)) {}

  virtual ~SdpObserverJni() {}

  // Can't mark override because of templating.
  virtual void OnSuccess() {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onSetSuccess", "()V");
    jni()->CallVoidMethod(*j_observer_global_, m);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  // Can't mark override because of templating.
  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onCreateSuccess",
                              "(Lorg/webrtc/SessionDescription;)V");
    jobject j_sdp = NativeToJavaSessionDescription(jni(), desc);
    jni()->CallVoidMethod(*j_observer_global_, m, j_sdp);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
    // OnSuccess transfers ownership of the description (there's a TODO to make
    // it use unique_ptr...).
    delete desc;
  }

  MediaConstraintsInterface* constraints() { return constraints_.get(); }

 protected:
  // Common implementation for failure of Set & Create types, distinguished by
  // |op| being "Set" or "Create".
  void DoOnFailure(const std::string& op, const std::string& error) {
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "on" + op + "Failure",
                              "(Ljava/lang/String;)V");
    jstring j_error_string = NativeToJavaString(jni(), error);
    jni()->CallVoidMethod(*j_observer_global_, m, j_error_string);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  JNIEnv* jni() { return AttachCurrentThreadIfNeeded(); }

 private:
  std::unique_ptr<MediaConstraintsInterface> constraints_;
  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
};

class CreateSdpObserverJni
    : public SdpObserverJni<CreateSessionDescriptionObserver> {
 public:
  CreateSdpObserverJni(JNIEnv* jni,
                       jobject j_observer,
                       std::unique_ptr<MediaConstraintsInterface> constraints)
      : SdpObserverJni(jni, j_observer, std::move(constraints)) {}

  void OnFailure(const std::string& error) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    SdpObserverJni::DoOnFailure(std::string("Create"), error);
  }
};

class SetSdpObserverJni : public SdpObserverJni<SetSessionDescriptionObserver> {
 public:
  SetSdpObserverJni(JNIEnv* jni,
                    jobject j_observer,
                    std::unique_ptr<MediaConstraintsInterface> constraints)
      : SdpObserverJni(jni, j_observer, std::move(constraints)) {}

  void OnFailure(const std::string& error) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    SdpObserverJni::DoOnFailure(std::string("Set"), error);
  }
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_SDPOBSERVER_JNI_H_
