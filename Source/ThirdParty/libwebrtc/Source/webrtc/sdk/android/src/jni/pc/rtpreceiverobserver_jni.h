/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_RTPRECEIVEROBSERVER_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_RTPRECEIVEROBSERVER_JNI_H_

#include "api/rtpreceiverinterface.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Adapter between the C++ RtpReceiverObserverInterface and the Java
// RtpReceiver.Observer interface. Wraps an instance of the Java interface and
// dispatches C++ callbacks to Java.
class RtpReceiverObserverJni : public RtpReceiverObserverInterface {
 public:
  RtpReceiverObserverJni(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer) {}

  ~RtpReceiverObserverJni() override {}

  void OnFirstPacketReceived(cricket::MediaType media_type) override;

 private:
  const ScopedGlobalRef<jobject> j_observer_global_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_RTPRECEIVEROBSERVER_JNI_H_
