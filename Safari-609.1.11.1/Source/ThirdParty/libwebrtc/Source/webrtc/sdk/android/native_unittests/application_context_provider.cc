/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "sdk/android/native_unittests/application_context_provider.h"

#include "sdk/android/generated_native_unittests_jni/ApplicationContextProvider_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace test {

ScopedJavaLocalRef<jobject> GetAppContextForTest(JNIEnv* jni) {
  return ScopedJavaLocalRef<jobject>(
      jni::Java_ApplicationContextProvider_getApplicationContextForTest(jni));
}

}  // namespace test
}  // namespace webrtc
