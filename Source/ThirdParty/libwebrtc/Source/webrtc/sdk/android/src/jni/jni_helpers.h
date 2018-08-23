/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contain convenience functions and classes for JNI.
// Before using any of the methods, InitGlobalJniVariables must be called.

#ifndef SDK_ANDROID_SRC_JNI_JNI_HELPERS_H_
#define SDK_ANDROID_SRC_JNI_JNI_HELPERS_H_

#include <jni.h>
#include <string>

#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/jvm.h"

// Convenience macro defining JNI-accessible methods in the org.webrtc package.
// Eliminates unnecessary boilerplate and line-wraps, reducing visual clutter.
#define JNI_FUNCTION_DECLARATION(rettype, name, ...) \
  extern "C" JNIEXPORT rettype JNICALL Java_org_webrtc_##name(__VA_ARGS__)

namespace webrtc {
namespace jni {

// TODO(sakal): Remove once clients have migrated.
using ::webrtc::JavaToStdMapStrings;

// Deprecated, use NativeToJavaPointer.
inline long jlongFromPointer(void* ptr) {
  return NativeToJavaPointer(ptr);
}

ScopedJavaLocalRef<jobject> NewDirectByteBuffer(JNIEnv* env,
                                                void* address,
                                                jlong capacity);

jobject NewGlobalRef(JNIEnv* jni, jobject o);

void DeleteGlobalRef(JNIEnv* jni, jobject o);

// Scope Java local references to the lifetime of this object.  Use in all C++
// callbacks (i.e. entry points that don't originate in a Java callstack
// through a "native" method call).
class ScopedLocalRefFrame {
 public:
  explicit ScopedLocalRefFrame(JNIEnv* jni);
  ~ScopedLocalRefFrame();

 private:
  JNIEnv* jni_;
};

}  // namespace jni
}  // namespace webrtc

// TODO(magjed): Remove once external clients are updated.
namespace webrtc_jni {

using webrtc::AttachCurrentThreadIfNeeded;
using webrtc::jni::InitGlobalJniVariables;

}  // namespace webrtc_jni

#endif  // SDK_ANDROID_SRC_JNI_JNI_HELPERS_H_
