/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include <limits>

#include "api/datachannelinterface.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "sdk/android/generated_peerconnection_jni/jni/DataChannel_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/datachannel.h"

namespace webrtc {
namespace jni {

namespace {
// Adapter for a Java DataChannel$Observer presenting a C++ DataChannelObserver
// and dispatching the callback from C++ back to Java.
class DataChannelObserverJni : public DataChannelObserver {
 public:
  DataChannelObserverJni(JNIEnv* jni, jobject j_observer);
  virtual ~DataChannelObserverJni() {}

  void OnBufferedAmountChange(uint64_t previous_amount) override;
  void OnStateChange() override;
  void OnMessage(const DataBuffer& buffer) override;

 private:
  const ScopedGlobalRef<jobject> j_observer_global_;
};

DataChannelObserverJni::DataChannelObserverJni(JNIEnv* jni, jobject j_observer)
    : j_observer_global_(jni, j_observer) {}

void DataChannelObserverJni::OnBufferedAmountChange(uint64_t previous_amount) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_Observer_onBufferedAmountChange(env, *j_observer_global_,
                                       previous_amount);
}

void DataChannelObserverJni::OnStateChange() {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_Observer_onStateChange(env, *j_observer_global_);
}

void DataChannelObserverJni::OnMessage(const DataBuffer& buffer) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject byte_buffer = env->NewDirectByteBuffer(
      const_cast<char*>(buffer.data.data<char>()), buffer.data.size());
  jobject j_buffer = Java_Buffer_Constructor(env, byte_buffer, buffer.binary);
  Java_Observer_onMessage(env, *j_observer_global_, j_buffer);
}

DataChannelInterface* ExtractNativeDC(JNIEnv* jni, jobject j_dc) {
  jlong j_d = Java_DataChannel_getNativeDataChannel(jni, j_dc);
  return reinterpret_cast<DataChannelInterface*>(j_d);
}

}  // namespace

DataChannelInit JavaToNativeDataChannelInit(JNIEnv* env, jobject j_init) {
  DataChannelInit init;
  init.ordered = Java_Init_getOrdered(env, j_init);
  init.maxRetransmitTime = Java_Init_getMaxRetransmitTimeMs(env, j_init);
  init.maxRetransmits = Java_Init_getMaxRetransmits(env, j_init);
  init.protocol = JavaToStdString(env, Java_Init_getProtocol(env, j_init));
  init.negotiated = Java_Init_getNegotiated(env, j_init);
  init.id = Java_Init_getId(env, j_init);
  return init;
}

jobject WrapNativeDataChannel(
    JNIEnv* env,
    rtc::scoped_refptr<DataChannelInterface> channel) {
  // Channel is now owned by Java object, and will be freed from there.
  return channel ? Java_DataChannel_Constructor(
                       env, jlongFromPointer(channel.release()))
                 : nullptr;
}

JNI_FUNCTION_DECLARATION(jlong,
                         DataChannel_registerObserverNative,
                         JNIEnv* jni,
                         jobject j_dc,
                         jobject j_observer) {
  auto observer = rtc::MakeUnique<DataChannelObserverJni>(jni, j_observer);
  ExtractNativeDC(jni, j_dc)->RegisterObserver(observer.get());
  return jlongFromPointer(observer.release());
}

JNI_FUNCTION_DECLARATION(void,
                         DataChannel_unregisterObserverNative,
                         JNIEnv* jni,
                         jobject j_dc,
                         jlong native_observer) {
  ExtractNativeDC(jni, j_dc)->UnregisterObserver();
  delete reinterpret_cast<DataChannelObserverJni*>(native_observer);
}

JNI_FUNCTION_DECLARATION(jstring,
                         DataChannel_label,
                         JNIEnv* jni,
                         jobject j_dc) {
  return NativeToJavaString(jni, ExtractNativeDC(jni, j_dc)->label());
}

JNI_FUNCTION_DECLARATION(jint, DataChannel_id, JNIEnv* jni, jobject j_dc) {
  int id = ExtractNativeDC(jni, j_dc)->id();
  RTC_CHECK_LE(id, std::numeric_limits<int32_t>::max())
      << "id overflowed jint!";
  return static_cast<jint>(id);
}

JNI_FUNCTION_DECLARATION(jobject,
                         DataChannel_state,
                         JNIEnv* jni,
                         jobject j_dc) {
  return Java_State_fromNativeIndex(jni, ExtractNativeDC(jni, j_dc)->state());
}

JNI_FUNCTION_DECLARATION(jlong,
                         DataChannel_bufferedAmount,
                         JNIEnv* jni,
                         jobject j_dc) {
  uint64_t buffered_amount = ExtractNativeDC(jni, j_dc)->buffered_amount();
  RTC_CHECK_LE(buffered_amount, std::numeric_limits<int64_t>::max())
      << "buffered_amount overflowed jlong!";
  return static_cast<jlong>(buffered_amount);
}

JNI_FUNCTION_DECLARATION(void, DataChannel_close, JNIEnv* jni, jobject j_dc) {
  ExtractNativeDC(jni, j_dc)->Close();
}

JNI_FUNCTION_DECLARATION(jboolean,
                         DataChannel_sendNative,
                         JNIEnv* jni,
                         jobject j_dc,
                         jbyteArray data,
                         jboolean binary) {
  jbyte* bytes = jni->GetByteArrayElements(data, nullptr);
  bool ret = ExtractNativeDC(jni, j_dc)->Send(DataBuffer(
      rtc::CopyOnWriteBuffer(bytes, jni->GetArrayLength(data)), binary));
  jni->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
  return ret;
}

}  // namespace jni
}  // namespace webrtc
