/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
// Do not include this file directly. It's intended to be used only by the JNI
// generation script. We are exporting types in strange namespaces in order to
// be compatible with the generated code targeted for Chromium.

#ifndef SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
#define SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_

#include <jni.h>
#include <atomic>

#include "rtc_base/checks.h"
#include "sdk/android/native_api/jni/jni_int_wrapper.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

#define CHECK_CLAZZ(env, jcaller, clazz, ...) RTC_DCHECK(clazz);
#define CHECK_NATIVE_PTR(env, jcaller, native_ptr, method_name, ...) \
  RTC_DCHECK(native_ptr) << method_name;

#define BASE_EXPORT
#define JNI_REGISTRATION_EXPORT __attribute__((visibility("default")))
#define JNI_GENERATOR_EXPORT extern "C" JNIEXPORT JNICALL

#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

namespace jni_generator {
inline void CheckException(JNIEnv* env) {
  CHECK_EXCEPTION(env);
}
}  // namespace jni_generator

namespace webrtc {

// This function will initialize |atomic_class_id| to contain a global ref to
// the given class, and will return that ref on subsequent calls. The caller is
// responsible to zero-initialize |atomic_class_id|. It's fine to
// simultaneously call this on multiple threads referencing the same
// |atomic_method_id|.
jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    std::atomic<jclass>* atomic_class_id);

// This class is a wrapper for JNIEnv Get(Static)MethodID.
class MethodID {
 public:
  enum Type {
    TYPE_STATIC,
    TYPE_INSTANCE,
  };

  // This function will initialize |atomic_method_id| to contain a ref to
  // the given method, and will return that ref on subsequent calls. The caller
  // is responsible to zero-initialize |atomic_method_id|. It's fine to
  // simultaneously call this on multiple threads referencing the same
  // |atomic_method_id|.
  template <Type type>
  static jmethodID LazyGet(JNIEnv* env,
                           jclass clazz,
                           const char* method_name,
                           const char* jni_signature,
                           std::atomic<jmethodID>* atomic_method_id);
};

}  // namespace webrtc

// Re-export relevant classes into the namespaces the script expects.
namespace base {
namespace android {

using webrtc::JavaParamRef;
using webrtc::JavaRef;
using webrtc::ScopedJavaLocalRef;
using webrtc::LazyGetClass;
using webrtc::MethodID;

}  // namespace android
}  // namespace base

#endif  // SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
