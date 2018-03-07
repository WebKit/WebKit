/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/dtmfsenderinterface.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jboolean,
                         DtmfSender_nativeCanInsertDtmf,
                         JNIEnv* jni,
                         jclass,
                         jlong j_dtmf_sender_pointer) {
  return reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)
      ->CanInsertDtmf();
}

JNI_FUNCTION_DECLARATION(jboolean,
                         DtmfSender_nativeInsertDtmf,
                         JNIEnv* jni,
                         jclass,
                         jlong j_dtmf_sender_pointer,
                         jstring tones,
                         jint duration,
                         jint inter_tone_gap) {
  return reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)
      ->InsertDtmf(JavaToStdString(jni, tones), duration, inter_tone_gap);
}

JNI_FUNCTION_DECLARATION(jstring,
                         DtmfSender_nativeTones,
                         JNIEnv* jni,
                         jclass,
                         jlong j_dtmf_sender_pointer) {
  return NativeToJavaString(
      jni,
      reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)->tones());
}

JNI_FUNCTION_DECLARATION(jint,
                         DtmfSender_nativeDuration,
                         JNIEnv* jni,
                         jclass,
                         jlong j_dtmf_sender_pointer) {
  return reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)
      ->duration();
}

JNI_FUNCTION_DECLARATION(jint,
                         DtmfSender_nativeInterToneGap,
                         JNIEnv* jni,
                         jclass,
                         jlong j_dtmf_sender_pointer) {
  return reinterpret_cast<DtmfSenderInterface*>(j_dtmf_sender_pointer)
      ->inter_tone_gap();
}

}  // namespace jni
}  // namespace webrtc
