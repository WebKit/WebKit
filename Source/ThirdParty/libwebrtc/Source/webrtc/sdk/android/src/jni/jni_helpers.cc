/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

#include "webrtc/sdk/android/src/jni/classreferenceholder.h"

#include <asm/unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace webrtc_jni {

static JavaVM* g_jvm = nullptr;

static pthread_once_t g_jni_ptr_once = PTHREAD_ONCE_INIT;

// Key for per-thread JNIEnv* data.  Non-NULL in threads attached to |g_jvm| by
// AttachCurrentThreadIfNeeded(), NULL in unattached threads and threads that
// were attached by the JVM because of a Java->native call.
static pthread_key_t g_jni_ptr;

JavaVM *GetJVM() {
  RTC_CHECK(g_jvm) << "JNI_OnLoad failed to run?";
  return g_jvm;
}

// Return a |JNIEnv*| usable on this thread or NULL if this thread is detached.
JNIEnv* GetEnv() {
  void* env = nullptr;
  jint status = g_jvm->GetEnv(&env, JNI_VERSION_1_6);
  RTC_CHECK(((env != nullptr) && (status == JNI_OK)) ||
            ((env == nullptr) && (status == JNI_EDETACHED)))
      << "Unexpected GetEnv return: " << status << ":" << env;
  return reinterpret_cast<JNIEnv*>(env);
}

static void ThreadDestructor(void* prev_jni_ptr) {
  // This function only runs on threads where |g_jni_ptr| is non-NULL, meaning
  // we were responsible for originally attaching the thread, so are responsible
  // for detaching it now.  However, because some JVM implementations (notably
  // Oracle's http://goo.gl/eHApYT) also use the pthread_key_create mechanism,
  // the JVMs accounting info for this thread may already be wiped out by the
  // time this is called. Thus it may appear we are already detached even though
  // it was our responsibility to detach!  Oh well.
  if (!GetEnv())
    return;

  RTC_CHECK(GetEnv() == prev_jni_ptr)
      << "Detaching from another thread: " << prev_jni_ptr << ":" << GetEnv();
  jint status = g_jvm->DetachCurrentThread();
  RTC_CHECK(status == JNI_OK) << "Failed to detach thread: " << status;
  RTC_CHECK(!GetEnv()) << "Detaching was a successful no-op???";
}

static void CreateJNIPtrKey() {
  RTC_CHECK(!pthread_key_create(&g_jni_ptr, &ThreadDestructor))
      << "pthread_key_create";
}

jint InitGlobalJniVariables(JavaVM *jvm) {
  RTC_CHECK(!g_jvm) << "InitGlobalJniVariables!";
  g_jvm = jvm;
  RTC_CHECK(g_jvm) << "InitGlobalJniVariables handed NULL?";

  RTC_CHECK(!pthread_once(&g_jni_ptr_once, &CreateJNIPtrKey)) << "pthread_once";

  JNIEnv* jni = nullptr;
  if (jvm->GetEnv(reinterpret_cast<void**>(&jni), JNI_VERSION_1_6) != JNI_OK)
    return -1;

  return JNI_VERSION_1_6;
}

// Return thread ID as a string.
static std::string GetThreadId() {
  char buf[21];  // Big enough to hold a kuint64max plus terminating NULL.
  RTC_CHECK_LT(snprintf(buf, sizeof(buf), "%ld",
                        static_cast<long>(syscall(__NR_gettid))),
               sizeof(buf))
      << "Thread id is bigger than uint64??";
  return std::string(buf);
}

// Return the current thread's name.
static std::string GetThreadName() {
  char name[17] = {0};
  if (prctl(PR_GET_NAME, name) != 0)
    return std::string("<noname>");
  return std::string(name);
}

// Return a |JNIEnv*| usable on this thread.  Attaches to |g_jvm| if necessary.
JNIEnv* AttachCurrentThreadIfNeeded() {
  JNIEnv* jni = GetEnv();
  if (jni)
    return jni;
  RTC_CHECK(!pthread_getspecific(g_jni_ptr))
      << "TLS has a JNIEnv* but not attached?";

  std::string name(GetThreadName() + " - " + GetThreadId());
  JavaVMAttachArgs args;
  args.version = JNI_VERSION_1_6;
  args.name = &name[0];
  args.group = nullptr;
  // Deal with difference in signatures between Oracle's jni.h and Android's.
#ifdef _JAVASOFT_JNI_H_  // Oracle's jni.h violates the JNI spec!
  void* env = nullptr;
#else
  JNIEnv* env = nullptr;
#endif
  RTC_CHECK(!g_jvm->AttachCurrentThread(&env, &args))
      << "Failed to attach thread";
  RTC_CHECK(env) << "AttachCurrentThread handed back NULL!";
  jni = reinterpret_cast<JNIEnv*>(env);
  RTC_CHECK(!pthread_setspecific(g_jni_ptr, jni)) << "pthread_setspecific";
  return jni;
}

// Return a |jlong| that will correctly convert back to |ptr|.  This is needed
// because the alternative (of silently passing a 32-bit pointer to a vararg
// function expecting a 64-bit param) picks up garbage in the high 32 bits.
jlong jlongFromPointer(void* ptr) {
  static_assert(sizeof(intptr_t) <= sizeof(jlong),
                "Time to rethink the use of jlongs");
  // Going through intptr_t to be obvious about the definedness of the
  // conversion from pointer to integral type.  intptr_t to jlong is a standard
  // widening by the static_assert above.
  jlong ret = reinterpret_cast<intptr_t>(ptr);
  RTC_DCHECK(reinterpret_cast<void*>(ret) == ptr);
  return ret;
}

// JNIEnv-helper methods that RTC_CHECK success: no Java exception thrown and
// found object/class/method/field is non-null.
jmethodID GetMethodID(
    JNIEnv* jni, jclass c, const std::string& name, const char* signature) {
  jmethodID m = jni->GetMethodID(c, name.c_str(), signature);
  CHECK_EXCEPTION(jni) << "error during GetMethodID: " << name << ", "
                       << signature;
  RTC_CHECK(m) << name << ", " << signature;
  return m;
}

jmethodID GetStaticMethodID(
    JNIEnv* jni, jclass c, const char* name, const char* signature) {
  jmethodID m = jni->GetStaticMethodID(c, name, signature);
  CHECK_EXCEPTION(jni) << "error during GetStaticMethodID: " << name << ", "
                       << signature;
  RTC_CHECK(m) << name << ", " << signature;
  return m;
}

jfieldID GetFieldID(
    JNIEnv* jni, jclass c, const char* name, const char* signature) {
  jfieldID f = jni->GetFieldID(c, name, signature);
  CHECK_EXCEPTION(jni) << "error during GetFieldID";
  RTC_CHECK(f) << name << ", " << signature;
  return f;
}

jfieldID GetStaticFieldID(JNIEnv* jni,
                          jclass c,
                          const char* name,
                          const char* signature) {
  jfieldID f = jni->GetStaticFieldID(c, name, signature);
  CHECK_EXCEPTION(jni) << "error during GetStaticFieldID";
  RTC_CHECK(f) << name << ", " << signature;
  return f;
}

jclass GetObjectClass(JNIEnv* jni, jobject object) {
  jclass c = jni->GetObjectClass(object);
  CHECK_EXCEPTION(jni) << "error during GetObjectClass";
  RTC_CHECK(c) << "GetObjectClass returned NULL";
  return c;
}

jobject GetObjectField(JNIEnv* jni, jobject object, jfieldID id) {
  jobject o = jni->GetObjectField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetObjectField";
  RTC_CHECK(!IsNull(jni, o)) << "GetObjectField returned NULL";
  return o;
}

jobject GetStaticObjectField(JNIEnv* jni, jclass c, jfieldID id) {
  jobject o = jni->GetStaticObjectField(c, id);
  CHECK_EXCEPTION(jni) << "error during GetStaticObjectField";
  RTC_CHECK(!IsNull(jni, o)) << "GetStaticObjectField returned NULL";
  return o;
}

jobject GetNullableObjectField(JNIEnv* jni, jobject object, jfieldID id) {
  jobject o = jni->GetObjectField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetObjectField";
  return o;
}

jstring GetStringField(JNIEnv* jni, jobject object, jfieldID id) {
  return static_cast<jstring>(GetObjectField(jni, object, id));
}

jlong GetLongField(JNIEnv* jni, jobject object, jfieldID id) {
  jlong l = jni->GetLongField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetLongField";
  return l;
}

jint GetIntField(JNIEnv* jni, jobject object, jfieldID id) {
  jint i = jni->GetIntField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetIntField";
  return i;
}

bool GetBooleanField(JNIEnv* jni, jobject object, jfieldID id) {
  jboolean b = jni->GetBooleanField(object, id);
  CHECK_EXCEPTION(jni) << "error during GetBooleanField";
  return b;
}

bool IsNull(JNIEnv* jni, jobject obj) {
  return jni->IsSameObject(obj, nullptr);
}

// Given a UTF-8 encoded |native| string return a new (UTF-16) jstring.
jstring JavaStringFromStdString(JNIEnv* jni, const std::string& native) {
  jstring jstr = jni->NewStringUTF(native.c_str());
  CHECK_EXCEPTION(jni) << "error during NewStringUTF";
  return jstr;
}

// Given a jstring, reinterprets it to a new native string.
std::string JavaToStdString(JNIEnv* jni, const jstring& j_string) {
  // Invoke String.getBytes(String charsetName) method to convert |j_string|
  // to a byte array.
  const jclass string_class = GetObjectClass(jni, j_string);
  const jmethodID get_bytes =
      GetMethodID(jni, string_class, "getBytes", "(Ljava/lang/String;)[B");
  const jstring charset_name = jni->NewStringUTF("ISO-8859-1");
  CHECK_EXCEPTION(jni) << "error during NewStringUTF";
  const jbyteArray j_byte_array =
      (jbyteArray)jni->CallObjectMethod(j_string, get_bytes, charset_name);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";

  const size_t len = jni->GetArrayLength(j_byte_array);
  CHECK_EXCEPTION(jni) << "error during GetArrayLength";
  std::vector<char> buf(len);
  jni->GetByteArrayRegion(j_byte_array, 0, len,
                          reinterpret_cast<jbyte*>(&buf[0]));
  CHECK_EXCEPTION(jni) << "error during GetByteArrayRegion";

  return std::string(buf.begin(), buf.end());
}

// Return the (singleton) Java Enum object corresponding to |index|;
jobject JavaEnumFromIndex(JNIEnv* jni, jclass state_class,
                          const std::string& state_class_name, int index) {
  jmethodID state_values_id = GetStaticMethodID(
      jni, state_class, "values", ("()[L" + state_class_name  + ";").c_str());
  jobjectArray state_values = static_cast<jobjectArray>(
      jni->CallStaticObjectMethod(state_class, state_values_id));
  CHECK_EXCEPTION(jni) << "error during CallStaticObjectMethod";
  jobject ret = jni->GetObjectArrayElement(state_values, index);
  CHECK_EXCEPTION(jni) << "error during GetObjectArrayElement";
  return ret;
}

std::string GetJavaEnumName(JNIEnv* jni,
                            const std::string& className,
                            jobject j_enum) {
  jclass enumClass = FindClass(jni, className.c_str());
  jmethodID nameMethod =
      GetMethodID(jni, enumClass, "name", "()Ljava/lang/String;");
  jstring name =
      reinterpret_cast<jstring>(jni->CallObjectMethod(j_enum, nameMethod));
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod for " << className
                       << ".name";
  return JavaToStdString(jni, name);
}

jobject NewGlobalRef(JNIEnv* jni, jobject o) {
  jobject ret = jni->NewGlobalRef(o);
  CHECK_EXCEPTION(jni) << "error during NewGlobalRef";
  RTC_CHECK(ret);
  return ret;
}

void DeleteGlobalRef(JNIEnv* jni, jobject o) {
  jni->DeleteGlobalRef(o);
  CHECK_EXCEPTION(jni) << "error during DeleteGlobalRef";
}

// Scope Java local references to the lifetime of this object.  Use in all C++
// callbacks (i.e. entry points that don't originate in a Java callstack
// through a "native" method call).
ScopedLocalRefFrame::ScopedLocalRefFrame(JNIEnv* jni) : jni_(jni) {
  RTC_CHECK(!jni_->PushLocalFrame(0)) << "Failed to PushLocalFrame";
}
ScopedLocalRefFrame::~ScopedLocalRefFrame() {
  jni_->PopLocalFrame(nullptr);
}

// Creates an iterator representing the end of any collection.
Iterable::Iterator::Iterator() : iterator_(nullptr) {}

// Creates an iterator pointing to the beginning of the specified collection.
Iterable::Iterator::Iterator(JNIEnv* jni, jobject iterable) : jni_(jni) {
  jclass j_class = GetObjectClass(jni, iterable);
  jmethodID iterator_id =
      GetMethodID(jni, j_class, "iterator", "()Ljava/util/Iterator;");
  iterator_ = jni->CallObjectMethod(iterable, iterator_id);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  RTC_CHECK(iterator_ != nullptr);

  jclass iterator_class = GetObjectClass(jni, iterator_);
  has_next_id_ = GetMethodID(jni, iterator_class, "hasNext", "()Z");
  next_id_ = GetMethodID(jni, iterator_class, "next", "()Ljava/lang/Object;");

  // Start at the first element in the collection.
  ++(*this);
}

// Move constructor - necessary to be able to return iterator types from
// functions.
Iterable::Iterator::Iterator(Iterator&& other)
    : jni_(std::move(other.jni_)),
      iterator_(std::move(other.iterator_)),
      value_(std::move(other.value_)),
      has_next_id_(std::move(other.has_next_id_)),
      next_id_(std::move(other.next_id_)),
      thread_checker_(std::move(other.thread_checker_)){};

// Advances the iterator one step.
Iterable::Iterator& Iterable::Iterator::operator++() {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  if (AtEnd()) {
    // Can't move past the end.
    return *this;
  }
  bool has_next = jni_->CallBooleanMethod(iterator_, has_next_id_);
  CHECK_EXCEPTION(jni_) << "error during CallBooleanMethod";
  if (!has_next) {
    iterator_ = nullptr;
    value_ = nullptr;
    return *this;
  }

  value_ = jni_->CallObjectMethod(iterator_, next_id_);
  CHECK_EXCEPTION(jni_) << "error during CallObjectMethod";
  return *this;
}

// Provides a way to compare the iterator with itself and with the end iterator.
// Note: all other comparison results are undefined, just like for C++ input
// iterators.
bool Iterable::Iterator::operator==(const Iterable::Iterator& other) {
  // Two different active iterators should never be compared.
  RTC_DCHECK(this == &other || AtEnd() || other.AtEnd());
  return AtEnd() == other.AtEnd();
}

jobject Iterable::Iterator::operator*() {
  RTC_CHECK(!AtEnd());
  return value_;
}

bool Iterable::Iterator::AtEnd() const {
  RTC_CHECK(thread_checker_.CalledOnValidThread());
  return jni_ == nullptr || IsNull(jni_, iterator_);
}

}  // namespace webrtc_jni
