/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Android's FindClass() is trickier than usual because the app-specific
// ClassLoader is not consulted when there is no app-specific frame on the
// stack.  Consequently, we only look up all classes once in app/webrtc.
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass

#ifndef WEBRTC_API_JAVA_JNI_CLASSREFERENCEHOLDER_H_
#define WEBRTC_API_JAVA_JNI_CLASSREFERENCEHOLDER_H_

#include <jni.h>
#include <map>
#include <string>

namespace webrtc_jni {

// LoadGlobalClassReferenceHolder must be called in JNI_OnLoad.
void LoadGlobalClassReferenceHolder();
// FreeGlobalClassReferenceHolder must be called in JNI_UnLoad.
void FreeGlobalClassReferenceHolder();

// Returns a global reference guaranteed to be valid for the lifetime of the
// process.
jclass FindClass(JNIEnv* jni, const char* name);

// Convenience macro defining JNI-accessible methods in the org.webrtc package.
// Eliminates unnecessary boilerplate and line-wraps, reducing visual clutter.
#define JOW(rettype, name) extern "C" rettype JNIEXPORT JNICALL \
  Java_org_webrtc_##name

}  // namespace webrtc_jni

#endif  // WEBRTC_API_JAVA_JNI_CLASSREFERENCEHOLDER_H_
