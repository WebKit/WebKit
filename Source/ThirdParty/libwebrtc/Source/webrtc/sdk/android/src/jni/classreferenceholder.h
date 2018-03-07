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

#ifndef SDK_ANDROID_SRC_JNI_CLASSREFERENCEHOLDER_H_
#define SDK_ANDROID_SRC_JNI_CLASSREFERENCEHOLDER_H_

// TODO(magjed): Remove this whole file and replace with either generated JNI
// code or class_loader.h.

#include <jni.h>
#include <map>
#include <string>

namespace webrtc {
namespace jni {

// LoadGlobalClassReferenceHolder must be called in JNI_OnLoad.
void LoadGlobalClassReferenceHolder();
// FreeGlobalClassReferenceHolder must be called in JNI_UnLoad.
void FreeGlobalClassReferenceHolder();

// Deprecated. Most cases of finding classes should be done with generated JNI
// code, and the few remaining cases should use the function from
// class_loader.h.
// Returns a global reference guaranteed to be valid for the lifetime of the
// process.
jclass FindClass(JNIEnv* jni, const char* name);

}  // namespace jni
}  // namespace webrtc

// TODO(magjed): Remove once external clients are updated.
namespace webrtc_jni {

using webrtc::jni::LoadGlobalClassReferenceHolder;
using webrtc::jni::FreeGlobalClassReferenceHolder;

}  // namespace webrtc_jni

#endif  // SDK_ANDROID_SRC_JNI_CLASSREFERENCEHOLDER_H_
