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
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jlong,
                         CallSessionFileRotatingLogSink_nativeAddSink,
                         JNIEnv* jni,
                         jclass,
                         jstring j_dirPath,
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
  return (jlong)sink;
}

JNI_FUNCTION_DECLARATION(void,
                         CallSessionFileRotatingLogSink_nativeDeleteSink,
                         JNIEnv* jni,
                         jclass,
                         jlong j_sink) {
  rtc::CallSessionFileRotatingLogSink* sink =
      reinterpret_cast<rtc::CallSessionFileRotatingLogSink*>(j_sink);
  rtc::LogMessage::RemoveLogToStream(sink);
  delete sink;
}

JNI_FUNCTION_DECLARATION(jbyteArray,
                         CallSessionFileRotatingLogSink_nativeGetLogData,
                         JNIEnv* jni,
                         jclass,
                         jstring j_dirPath) {
  std::string dir_path = JavaToStdString(jni, j_dirPath);
  std::unique_ptr<rtc::CallSessionFileRotatingStream> stream(
      new rtc::CallSessionFileRotatingStream(dir_path));
  if (!stream->Open()) {
    RTC_LOG_V(rtc::LoggingSeverity::LS_WARNING)
        << "Failed to open CallSessionFileRotatingStream for path " << dir_path;
    return jni->NewByteArray(0);
  }
  size_t log_size = 0;
  if (!stream->GetSize(&log_size) || log_size == 0) {
    RTC_LOG_V(rtc::LoggingSeverity::LS_WARNING)
        << "CallSessionFileRotatingStream returns 0 size for path " << dir_path;
    return jni->NewByteArray(0);
  }

  size_t read = 0;
  std::unique_ptr<jbyte> buffer(static_cast<jbyte*>(malloc(log_size)));
  stream->ReadAll(buffer.get(), log_size, &read, nullptr);

  jbyteArray result = jni->NewByteArray(read);
  jni->SetByteArrayRegion(result, 0, read, buffer.get());

  return result;
}

}  // namespace jni
}  // namespace webrtc
