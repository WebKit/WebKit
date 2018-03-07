/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/jni_generator_helper.h"

#include "rtc_base/atomicops.h"
#include "sdk/android/src/jni/class_loader.h"

namespace base {
namespace android {

// If |atomic_class_id| set, it'll return immediately. Otherwise, it will look
// up the class and store it. If there's a race, we take care to only store one
// global reference (and the duplicated effort will happen only once).
jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    base::subtle::AtomicWord* atomic_class_id) {
  static_assert(sizeof(base::subtle::AtomicWord) >= sizeof(jclass),
                "AtomicWord can't be smaller than jclass");
  base::subtle::AtomicWord value =
      rtc::AtomicOps::AcquireLoadPtr(atomic_class_id);
  if (value)
    return reinterpret_cast<jclass>(value);
  jclass clazz = static_cast<jclass>(
      env->NewGlobalRef(webrtc::jni::GetClass(env, class_name)));
  RTC_CHECK(clazz) << class_name;
  base::subtle::AtomicWord null_aw = nullptr;
  base::subtle::AtomicWord cas_result = rtc::AtomicOps::CompareAndSwapPtr(
      atomic_class_id, null_aw,
      reinterpret_cast<base::subtle::AtomicWord>(clazz));
  if (cas_result == null_aw) {
    // We sucessfully stored |clazz| in |atomic_class_id|, so we are
    // intentionally leaking the global ref since it's now stored there.
    return clazz;
  } else {
    // Some other thread came before us and stored a global pointer in
    // |atomic_class_id|. Relase our global ref and return the ref from the
    // other thread.
    env->DeleteGlobalRef(clazz);
    return reinterpret_cast<jclass>(cas_result);
  }
}

// If |atomic_method_id| set, it'll return immediately. Otherwise, it will look
// up the method id and store it. If there's a race, it's ok since the values
// are the same (and the duplicated effort will happen only once).
template <MethodID::Type type>
jmethodID MethodID::LazyGet(JNIEnv* env,
                            jclass clazz,
                            const char* method_name,
                            const char* jni_signature,
                            base::subtle::AtomicWord* atomic_method_id) {
  static_assert(sizeof(base::subtle::AtomicWord) >= sizeof(jmethodID),
                "AtomicWord can't be smaller than jMethodID");
  base::subtle::AtomicWord value =
      rtc::AtomicOps::AcquireLoadPtr(atomic_method_id);
  if (value)
    return reinterpret_cast<jmethodID>(value);
  jmethodID id =
      (type == MethodID::TYPE_STATIC)
          ? webrtc::jni::GetStaticMethodID(env, clazz, method_name,
                                           jni_signature)
          : webrtc::jni::GetMethodID(env, clazz, method_name, jni_signature);
  rtc::AtomicOps::CompareAndSwapPtr(
      atomic_method_id, base::subtle::AtomicWord(nullptr),
      reinterpret_cast<base::subtle::AtomicWord>(id));
  return id;
}

// Various template instantiations.
template jmethodID MethodID::LazyGet<MethodID::TYPE_STATIC>(
    JNIEnv* env,
    jclass clazz,
    const char* method_name,
    const char* jni_signature,
    base::subtle::AtomicWord* atomic_method_id);

template jmethodID MethodID::LazyGet<MethodID::TYPE_INSTANCE>(
    JNIEnv* env,
    jclass clazz,
    const char* method_name,
    const char* jni_signature,
    base::subtle::AtomicWord* atomic_method_id);

}  // namespace android
}  // namespace base
