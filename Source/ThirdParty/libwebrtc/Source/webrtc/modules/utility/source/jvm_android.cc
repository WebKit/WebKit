/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <android/log.h>

#include <memory>

#include "webrtc/modules/utility/include/jvm_android.h"

#include "webrtc/base/checks.h"

#define TAG "JVM"
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace webrtc {

JVM* g_jvm;

// TODO(henrika): add more clases here if needed.
struct {
  const char* name;
  jclass clazz;
} loaded_classes[] = {
  {"org/webrtc/voiceengine/BuildInfo", nullptr},
  {"org/webrtc/voiceengine/WebRtcAudioManager", nullptr},
  {"org/webrtc/voiceengine/WebRtcAudioRecord", nullptr},
  {"org/webrtc/voiceengine/WebRtcAudioTrack", nullptr},
};

// Android's FindClass() is trickier than usual because the app-specific
// ClassLoader is not consulted when there is no app-specific frame on the
// stack.  Consequently, we only look up all classes once in native WebRTC.
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
void LoadClasses(JNIEnv* jni) {
  ALOGD("LoadClasses");
  for (auto& c : loaded_classes) {
    jclass localRef = FindClass(jni, c.name);
    ALOGD("name: %s", c.name);
    CHECK_EXCEPTION(jni) << "Error during FindClass: " << c.name;
    RTC_CHECK(localRef) << c.name;
    jclass globalRef = reinterpret_cast<jclass>(jni->NewGlobalRef(localRef));
    CHECK_EXCEPTION(jni) << "Error during NewGlobalRef: " << c.name;
    RTC_CHECK(globalRef) << c.name;
    c.clazz = globalRef;
  }
}

void FreeClassReferences(JNIEnv* jni) {
  for (auto& c : loaded_classes) {
    jni->DeleteGlobalRef(c.clazz);
    c.clazz = nullptr;
  }
}

jclass LookUpClass(const char* name) {
  for (auto& c : loaded_classes) {
    if (strcmp(c.name, name) == 0)
      return c.clazz;
  }
  RTC_CHECK(false) << "Unable to find class in lookup table";
  return 0;
}

// AttachCurrentThreadIfNeeded implementation.
AttachCurrentThreadIfNeeded::AttachCurrentThreadIfNeeded()
    : attached_(false) {
  ALOGD("AttachCurrentThreadIfNeeded::ctor%s", GetThreadInfo().c_str());
  JavaVM* jvm = JVM::GetInstance()->jvm();
  RTC_CHECK(jvm);
  JNIEnv* jni = GetEnv(jvm);
  if (!jni) {
    ALOGD("Attaching thread to JVM");
    JNIEnv* env = nullptr;
    jint ret = jvm->AttachCurrentThread(&env, nullptr);
    attached_ = (ret == JNI_OK);
  }
}

AttachCurrentThreadIfNeeded::~AttachCurrentThreadIfNeeded() {
  ALOGD("AttachCurrentThreadIfNeeded::dtor%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (attached_) {
    ALOGD("Detaching thread from JVM");
    jint res = JVM::GetInstance()->jvm()->DetachCurrentThread();
    RTC_CHECK(res == JNI_OK) << "DetachCurrentThread failed: " << res;
  }
}

// GlobalRef implementation.
GlobalRef::GlobalRef(JNIEnv* jni, jobject object)
    : jni_(jni), j_object_(NewGlobalRef(jni, object)) {
  ALOGD("GlobalRef::ctor%s", GetThreadInfo().c_str());
}

GlobalRef::~GlobalRef() {
  ALOGD("GlobalRef::dtor%s", GetThreadInfo().c_str());
  DeleteGlobalRef(jni_, j_object_);
}

jboolean GlobalRef::CallBooleanMethod(jmethodID methodID, ...) {
  va_list args;
  va_start(args, methodID);
  jboolean res = jni_->CallBooleanMethodV(j_object_, methodID, args);
  CHECK_EXCEPTION(jni_) << "Error during CallBooleanMethod";
  va_end(args);
  return res;
}

jint GlobalRef::CallIntMethod(jmethodID methodID, ...) {
  va_list args;
  va_start(args, methodID);
  jint res = jni_->CallIntMethodV(j_object_, methodID, args);
  CHECK_EXCEPTION(jni_) << "Error during CallIntMethod";
  va_end(args);
  return res;
}

void GlobalRef::CallVoidMethod(jmethodID methodID, ...) {
  va_list args;
  va_start(args, methodID);
  jni_->CallVoidMethodV(j_object_, methodID, args);
  CHECK_EXCEPTION(jni_) << "Error during CallVoidMethod";
  va_end(args);
}

// NativeRegistration implementation.
NativeRegistration::NativeRegistration(JNIEnv* jni, jclass clazz)
    : JavaClass(jni, clazz), jni_(jni) {
  ALOGD("NativeRegistration::ctor%s", GetThreadInfo().c_str());
}

NativeRegistration::~NativeRegistration() {
  ALOGD("NativeRegistration::dtor%s", GetThreadInfo().c_str());
  jni_->UnregisterNatives(j_class_);
  CHECK_EXCEPTION(jni_) << "Error during UnregisterNatives";
}

std::unique_ptr<GlobalRef> NativeRegistration::NewObject(
    const char* name, const char* signature, ...) {
  ALOGD("NativeRegistration::NewObject%s", GetThreadInfo().c_str());
  va_list args;
  va_start(args, signature);
  jobject obj = jni_->NewObjectV(j_class_,
                                 GetMethodID(jni_, j_class_, name, signature),
                                 args);
  CHECK_EXCEPTION(jni_) << "Error during NewObjectV";
  va_end(args);
  return std::unique_ptr<GlobalRef>(new GlobalRef(jni_, obj));
}

// JavaClass implementation.
jmethodID JavaClass::GetMethodId(
    const char* name, const char* signature) {
  return GetMethodID(jni_, j_class_, name, signature);
}

jmethodID JavaClass::GetStaticMethodId(
    const char* name, const char* signature) {
  return GetStaticMethodID(jni_, j_class_, name, signature);
}

jobject JavaClass::CallStaticObjectMethod(jmethodID methodID, ...) {
  va_list args;
  va_start(args, methodID);
  jobject res = jni_->CallStaticObjectMethod(j_class_, methodID, args);
  CHECK_EXCEPTION(jni_) << "Error during CallStaticObjectMethod";
  return res;
}

jint JavaClass::CallStaticIntMethod(jmethodID methodID, ...) {
  va_list args;
  va_start(args, methodID);
  jint res = jni_->CallStaticIntMethod(j_class_, methodID, args);
  CHECK_EXCEPTION(jni_) << "Error during CallStaticIntMethod";
  return res;
}

// JNIEnvironment implementation.
JNIEnvironment::JNIEnvironment(JNIEnv* jni) : jni_(jni) {
  ALOGD("JNIEnvironment::ctor%s", GetThreadInfo().c_str());
}

JNIEnvironment::~JNIEnvironment() {
  ALOGD("JNIEnvironment::dtor%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

std::unique_ptr<NativeRegistration> JNIEnvironment::RegisterNatives(
    const char* name, const JNINativeMethod *methods, int num_methods) {
  ALOGD("JNIEnvironment::RegisterNatives(%s)", name);
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  jclass clazz = LookUpClass(name);
  jni_->RegisterNatives(clazz, methods, num_methods);
  CHECK_EXCEPTION(jni_) << "Error during RegisterNatives";
  return std::unique_ptr<NativeRegistration>(
      new NativeRegistration(jni_, clazz));
}

std::string JNIEnvironment::JavaToStdString(const jstring& j_string) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  const char* jchars = jni_->GetStringUTFChars(j_string, nullptr);
  CHECK_EXCEPTION(jni_);
  const int size = jni_->GetStringUTFLength(j_string);
  CHECK_EXCEPTION(jni_);
  std::string ret(jchars, size);
  jni_->ReleaseStringUTFChars(j_string, jchars);
  CHECK_EXCEPTION(jni_);
  return ret;
}

// static
void JVM::Initialize(JavaVM* jvm) {
  ALOGD("JVM::Initialize%s", GetThreadInfo().c_str());
  RTC_CHECK(!g_jvm);
  g_jvm = new JVM(jvm);
}

void JVM::Initialize(JavaVM* jvm, jobject context) {
  Initialize(jvm);

  // Pass in the context to the new ContextUtils class.
  JNIEnv* jni = g_jvm->jni();
  jclass context_utils = FindClass(jni, "org/webrtc/ContextUtils");
  jmethodID initialize_method = jni->GetStaticMethodID(
      context_utils, "initialize", "(Landroid/content/Context;)V");
  jni->CallStaticVoidMethod(context_utils, initialize_method, context);
}

// static
void JVM::Uninitialize() {
  ALOGD("JVM::Uninitialize%s", GetThreadInfo().c_str());
  RTC_DCHECK(g_jvm);
  delete g_jvm;
  g_jvm = nullptr;
}

// static
JVM* JVM::GetInstance() {
  RTC_DCHECK(g_jvm);
  return g_jvm;
}

JVM::JVM(JavaVM* jvm) : jvm_(jvm) {
  ALOGD("JVM::JVM%s", GetThreadInfo().c_str());
  RTC_CHECK(jni()) << "AttachCurrentThread() must be called on this thread.";
  LoadClasses(jni());
}

JVM::~JVM() {
  ALOGD("JVM::~JVM%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  FreeClassReferences(jni());
}

std::unique_ptr<JNIEnvironment> JVM::environment() {
  ALOGD("JVM::environment%s", GetThreadInfo().c_str());
  // The JNIEnv is used for thread-local storage. For this reason, we cannot
  // share a JNIEnv between threads. If a piece of code has no other way to get
  // its JNIEnv, we should share the JavaVM, and use GetEnv to discover the
  // thread's JNIEnv. (Assuming it has one, if not, use AttachCurrentThread).
  // See // http://developer.android.com/training/articles/perf-jni.html.
  JNIEnv* jni = GetEnv(jvm_);
  if (!jni) {
    ALOGE("AttachCurrentThread() has not been called on this thread.");
    return std::unique_ptr<JNIEnvironment>();
  }
  return std::unique_ptr<JNIEnvironment>(new JNIEnvironment(jni));
}

JavaClass JVM::GetClass(const char* name) {
  ALOGD("JVM::GetClass(%s)%s", name, GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return JavaClass(jni(), LookUpClass(name));
}

}  // namespace webrtc
