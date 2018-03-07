/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtpsenderinterface.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/java_native_conversion.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jboolean,
                         RtpSender_nativeSetTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_sender_pointer,
                         jlong j_track_pointer) {
  return reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
      ->SetTrack(reinterpret_cast<MediaStreamTrackInterface*>(j_track_pointer));
}

JNI_FUNCTION_DECLARATION(jlong,
                         RtpSender_nativeGetTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_sender_pointer) {
  return jlongFromPointer(
      reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
          ->track()
          .release());
}

JNI_FUNCTION_DECLARATION(jlong,
                         RtpSender_nativeGetDtmfSender,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_sender_pointer) {
  return jlongFromPointer(
      reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
          ->GetDtmfSender()
          .release());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         RtpSender_nativeSetParameters,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_sender_pointer,
                         jobject j_parameters) {
  if (IsNull(jni, j_parameters)) {
    return false;
  }
  RtpParameters parameters;
  JavaToNativeRtpParameters(jni, j_parameters, &parameters);
  return reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
      ->SetParameters(parameters);
}

JNI_FUNCTION_DECLARATION(jobject,
                         RtpSender_nativeGetParameters,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_sender_pointer) {
  RtpParameters parameters =
      reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)
          ->GetParameters();
  return NativeToJavaRtpParameters(jni, parameters);
}

JNI_FUNCTION_DECLARATION(jstring,
                         RtpSender_nativeId,
                         JNIEnv* jni,
                         jclass,
                         jlong j_rtp_sender_pointer) {
  return NativeToJavaString(
      jni, reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer)->id());
}

}  // namespace jni
}  // namespace webrtc
