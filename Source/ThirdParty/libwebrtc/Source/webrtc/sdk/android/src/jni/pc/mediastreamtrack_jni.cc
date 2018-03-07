/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/mediastreaminterface.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jstring,
                         MediaStreamTrack_getNativeId,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return NativeToJavaString(
      jni, reinterpret_cast<MediaStreamTrackInterface*>(j_p)->id());
}

JNI_FUNCTION_DECLARATION(jstring,
                         MediaStreamTrack_getNativeKind,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return NativeToJavaString(
      jni, reinterpret_cast<MediaStreamTrackInterface*>(j_p)->kind());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStreamTrack_getNativeEnabled,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)->enabled();
}

JNI_FUNCTION_DECLARATION(jobject,
                         MediaStreamTrack_getNativeState,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return JavaEnumFromIndexAndClassName(
      jni, "MediaStreamTrack$State",
      reinterpret_cast<MediaStreamTrackInterface*>(j_p)->state());
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStreamTrack_setNativeEnabled,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p,
                         jboolean enabled) {
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)->set_enabled(
      enabled);
}

}  // namespace jni
}  // namespace webrtc
