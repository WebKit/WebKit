/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/logsinks.h"
#include "sdk/android/generated_peerconnection_jni/jni/CallSessionFileRotatingLogSink_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

static jlong JNI_CallSessionFileRotatingLogSink_AddSink(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jstring>& j_dirPath,
    jint j_maxFileSize,
    jint j_severity) {
  std::string dir_path = JavaToStdString(jni, j_dirPath);
  rtc::CallSessionFileRotatingLogSink* sink =
      new rtc::CallSessionFileRotatingLogSink(dir_path, j_maxFileSize);
  if (!sink->Init()) {
    RTC_LOG_V(rtc::LoggingSeverity::LS_WARNING)
        << "Failed to init CallSessionFileRotatingLogSink for path "
        << dir_path;
    delete sink;
    return 0;
  }
  rtc::LogMessage::AddLogToStream(
      sink, static_cast<rtc::LoggingSeverity>(j_severity));
  return jlongFromPointer(sink);
}

static void JNI_CallSessionFileRotatingLogSink_DeleteSink(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong j_sink) {
  rtc::CallSessionFileRotatingLogSink* sink =
      reinterpret_cast<rtc::CallSessionFileRotatingLogSink*>(j_sink);
  rtc::LogMessage::RemoveLogToStream(sink);
  delete sink;
}

static ScopedJavaLocalRef<jbyteArray>
JNI_CallSessionFileRotatingLogSink_GetLogData(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jstring>& j_dirPath) {
  std::string dir_path = JavaToStdString(jni, j_dirPath);
  std::unique_ptr<rtc::CallSessionFileRotatingStream> stream(
      new rtc::CallSessionFileRotatingStream(dir_path));
  if (!stream->Open()) {
    RTC_LOG_V(rtc::LoggingSeverity::LS_WARNING)
        << "Failed to open CallSessionFileRotatingStream for path " << dir_path;
    return ScopedJavaLocalRef<jbyteArray>(jni, jni->NewByteArray(0));
  }
  size_t log_size = 0;
  if (!stream->GetSize(&log_size) || log_size == 0) {
    RTC_LOG_V(rtc::LoggingSeverity::LS_WARNING)
        << "CallSessionFileRotatingStream returns 0 size for path " << dir_path;
    return ScopedJavaLocalRef<jbyteArray>(jni, jni->NewByteArray(0));
  }

  size_t read = 0;
  std::unique_ptr<jbyte> buffer(static_cast<jbyte*>(malloc(log_size)));
  stream->ReadAll(buffer.get(), log_size, &read, nullptr);

  ScopedJavaLocalRef<jbyteArray> result =
      ScopedJavaLocalRef<jbyteArray>(jni, jni->NewByteArray(read));
  jni->SetByteArrayRegion(result.obj(), 0, read, buffer.get());

  return result;
}

}  // namespace jni
}  // namespace webrtc
