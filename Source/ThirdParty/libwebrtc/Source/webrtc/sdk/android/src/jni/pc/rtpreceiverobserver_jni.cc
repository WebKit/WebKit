/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/rtpreceiverobserver_jni.h"

#include "sdk/android/src/jni/pc/java_native_conversion.h"

namespace webrtc {
namespace jni {

void RtpReceiverObserverJni::OnFirstPacketReceived(
    cricket::MediaType media_type) {
  JNIEnv* const jni = AttachCurrentThreadIfNeeded();

  jmethodID j_on_first_packet_received_mid = GetMethodID(
      jni, GetObjectClass(jni, *j_observer_global_), "onFirstPacketReceived",
      "(Lorg/webrtc/MediaStreamTrack$MediaType;)V");
  // Get the Java version of media type.
  jobject JavaMediaType = NativeToJavaMediaType(jni, media_type);
  // Trigger the callback function.
  jni->CallVoidMethod(*j_observer_global_, j_on_first_packet_received_mid,
                      JavaMediaType);
  CHECK_EXCEPTION(jni) << "error during CallVoidMethod";
}

}  // namespace jni
}  // namespace webrtc
