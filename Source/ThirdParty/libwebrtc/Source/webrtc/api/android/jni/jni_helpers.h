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

#ifndef WEBRTC_API_JAVA_JNI_JNI_HELPERS_H_
#define WEBRTC_API_JAVA_JNI_JNI_HELPERS_H_

#include <jni.h>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/thread_checker.h"

// Abort the process if |jni| has a Java exception pending.
// This macros uses the comma operator to execute ExceptionDescribe
// and ExceptionClear ignoring their return values and sending ""
// to the error stream.
#define CHECK_EXCEPTION(jni)        \
  RTC_CHECK(!jni->ExceptionCheck()) \
      << (jni->ExceptionDescribe(), jni->ExceptionClear(), "")

// Helper that calls ptr->Release() and aborts the process with a useful
// message if that didn't actually delete *ptr because of extra refcounts.
#define CHECK_RELEASE(ptr) \
  RTC_CHECK_EQ(0, (ptr)->Release()) << "Unexpected refcount."

namespace webrtc_jni {

jint InitGlobalJniVariables(JavaVM *jvm);

// Return a |JNIEnv*| usable on this thread or NULL if this thread is detached.
JNIEnv* GetEnv();

JavaVM *GetJVM();

// Return a |JNIEnv*| usable on this thread.  Attaches to |g_jvm| if necessary.
JNIEnv* AttachCurrentThreadIfNeeded();

// Return a |jlong| that will correctly convert back to |ptr|.  This is needed
// because the alternative (of silently passing a 32-bit pointer to a vararg
// function expecting a 64-bit param) picks up garbage in the high 32 bits.
jlong jlongFromPointer(void* ptr);

// JNIEnv-helper methods that RTC_CHECK success: no Java exception thrown and
// found object/class/method/field is non-null.
jmethodID GetMethodID(
    JNIEnv* jni, jclass c, const std::string& name, const char* signature);

jmethodID GetStaticMethodID(
    JNIEnv* jni, jclass c, const char* name, const char* signature);

jfieldID GetFieldID(JNIEnv* jni, jclass c, const char* name,
                    const char* signature);

jclass GetObjectClass(JNIEnv* jni, jobject object);

// Throws an exception if the object field is null.
jobject GetObjectField(JNIEnv* jni, jobject object, jfieldID id);

jobject GetNullableObjectField(JNIEnv* jni, jobject object, jfieldID id);

jstring GetStringField(JNIEnv* jni, jobject object, jfieldID id);

jlong GetLongField(JNIEnv* jni, jobject object, jfieldID id);

jint GetIntField(JNIEnv* jni, jobject object, jfieldID id);

bool GetBooleanField(JNIEnv* jni, jobject object, jfieldID id);

// Returns true if |obj| == null in Java.
bool IsNull(JNIEnv* jni, jobject obj);

// Given a UTF-8 encoded |native| string return a new (UTF-16) jstring.
jstring JavaStringFromStdString(JNIEnv* jni, const std::string& native);

// Given a (UTF-16) jstring return a new UTF-8 native string.
std::string JavaToStdString(JNIEnv* jni, const jstring& j_string);

// Return the (singleton) Java Enum object corresponding to |index|;
jobject JavaEnumFromIndex(JNIEnv* jni, jclass state_class,
                          const std::string& state_class_name, int index);

// Returns the name of a Java enum.
std::string GetJavaEnumName(JNIEnv* jni,
                            const std::string& className,
                            jobject j_enum);

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

// Scoped holder for global Java refs.
template<class T>  // T is jclass, jobject, jintArray, etc.
class ScopedGlobalRef {
 public:
  ScopedGlobalRef(JNIEnv* jni, T obj)
      : obj_(static_cast<T>(jni->NewGlobalRef(obj))) {}
  ~ScopedGlobalRef() {
    DeleteGlobalRef(AttachCurrentThreadIfNeeded(), obj_);
  }
  T operator*() const {
    return obj_;
  }
 private:
  T obj_;
};

// Provides a convenient way to iterate over a Java Iterable using the
// C++ range-for loop.
// E.g. for (jobject value : Iterable(jni, j_iterable)) { ... }
// Note: Since Java iterators cannot be duplicated, the iterator class is not
// copyable to prevent creating multiple C++ iterators that refer to the same
// Java iterator.
class Iterable {
 public:
  Iterable(JNIEnv* jni, jobject iterable) : jni_(jni), iterable_(iterable) {}

  class Iterator {
   public:
    // Creates an iterator representing the end of any collection.
    Iterator();
    // Creates an iterator pointing to the beginning of the specified
    // collection.
    Iterator(JNIEnv* jni, jobject iterable);

    // Move constructor - necessary to be able to return iterator types from
    // functions.
    Iterator(Iterator&& other);

    // Move assignment should not be used.
    Iterator& operator=(Iterator&&) = delete;

    // Advances the iterator one step.
    Iterator& operator++();

    // Provides a way to compare the iterator with itself and with the end
    // iterator.
    // Note: all other comparison results are undefined, just like for C++ input
    // iterators.
    bool operator==(const Iterator& other);
    bool operator!=(const Iterator& other) { return !(*this == other); }
    jobject operator*();

   private:
    bool AtEnd() const;

    JNIEnv* jni_ = nullptr;
    jobject iterator_ = nullptr;
    jobject value_ = nullptr;
    jmethodID has_next_id_ = nullptr;
    jmethodID next_id_ = nullptr;
    rtc::ThreadChecker thread_checker_;

    RTC_DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  Iterable::Iterator begin() { return Iterable::Iterator(jni_, iterable_); }
  Iterable::Iterator end() { return Iterable::Iterator(); }

 private:
  JNIEnv* jni_;
  jobject iterable_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Iterable);
};

}  // namespace webrtc_jni

#endif  // WEBRTC_API_JAVA_JNI_JNI_HELPERS_H_
