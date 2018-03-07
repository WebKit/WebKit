/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Android's FindClass() is tricky because the app-specific ClassLoader is not
// consulted when there is no app-specific frame on the stack (i.e. when called
// from a thread created from native C++ code). These helper functions provide a
// workaround for this.
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass

#ifndef SDK_ANDROID_SRC_JNI_CLASS_LOADER_H_
#define SDK_ANDROID_SRC_JNI_CLASS_LOADER_H_

#include <jni.h>

namespace webrtc {
namespace jni {

// This method should be called from JNI_OnLoad and before any calls to
// FindClass.
void InitClassLoader(JNIEnv* env);

// This function is identical to JNIEnv::FindClass except that it works from any
// thread. This function loads and returns a local reference to the class with
// the given name. The name argument is a fully-qualified class name. For
// example, the fully-qualified class name for the java.lang.String class is:
// "java/lang/String". This function will be used from the JNI generated code
// and should rarely be used manually.
jclass GetClass(JNIEnv* env, const char* name);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_CLASS_LOADER_H_
