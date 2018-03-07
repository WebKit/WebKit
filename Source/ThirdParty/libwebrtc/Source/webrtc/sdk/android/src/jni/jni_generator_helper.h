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
// be compatible with the generated code targeted for Chromium. We are bypassing
// the wrapping done by Chromium's JniIntWrapper, JavaRef, and
// ScopedJavaLocalRef, and use raw jobjects instead.

#ifndef SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
#define SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_

#include "sdk/android/src/jni/jni_helpers.h"

#define CHECK_CLAZZ(env, jcaller, clazz, ...) RTC_DCHECK(clazz);
#define CHECK_NATIVE_PTR(env, jcaller, native_ptr, method_name, ...) \
  RTC_DCHECK(native_ptr) << method_name;

#define BASE_EXPORT
#define JNI_REGISTRATION_EXPORT __attribute__((visibility("default")))
#define JNI_GENERATOR_EXPORT extern "C" JNIEXPORT JNICALL

namespace jni_generator {
inline void CheckException(JNIEnv* env) {
  CHECK_EXCEPTION(env);
}
}  // namespace jni_generator

namespace {  // NOLINT(build/namespaces)
// Bypass JniIntWrapper.
// TODO(magjed): Start using Chromium's JniIntWrapper.
typedef jint JniIntWrapper;
inline jint as_jint(JniIntWrapper wrapper) {
  return wrapper;
}
}  // namespace

namespace base {

namespace subtle {
// This needs to be a type that is big enough to store a jobject/jclass.
typedef void* AtomicWord;
}  // namespace subtle

namespace android {

// Implement JavaRef and ScopedJavaLocalRef as a shallow wrapper on top of a
// jobject/jclass, with no scoped destruction.
// TODO(magjed): Start using Chromium's scoped Java refs.
template <typename T>
class JavaRef {
 public:
  JavaRef() {}
  JavaRef(JNIEnv* env, T obj) : obj_(obj) {}
  T obj() const { return obj_; }

  // Implicit on purpose.
  JavaRef(const T& obj) : obj_(obj) {}  // NOLINT(runtime/explicit)
  operator T() const { return obj_; }

 private:
  T obj_;
};

// TODO(magjed): This looks weird, but it is safe. We don't use DeleteLocalRef
// in WebRTC, we use ScopedLocalRefFrame instead. We should probably switch to
// using DeleteLocalRef though.
template <typename T>
using ScopedJavaLocalRef = JavaRef<T>;
template <typename T>
using JavaParamRef = JavaRef<T>;

// This function will initialize |atomic_class_id| to contain a global ref to
// the given class, and will return that ref on subsequent calls. The caller is
// responsible to zero-initialize |atomic_class_id|. It's fine to
// simultaneously call this on multiple threads referencing the same
// |atomic_method_id|.
jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    base::subtle::AtomicWord* atomic_class_id);

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
                           base::subtle::AtomicWord* atomic_method_id);
};

}  // namespace android
}  // namespace base

#endif  // SDK_ANDROID_SRC_JNI_JNI_GENERATOR_HELPER_H_
