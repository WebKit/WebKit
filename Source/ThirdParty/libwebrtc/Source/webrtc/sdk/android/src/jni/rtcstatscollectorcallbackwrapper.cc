/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/sdk/android/src/jni/rtcstatscollectorcallbackwrapper.h"

#include <string>
#include <vector>

#include "webrtc/sdk/android/src/jni/classreferenceholder.h"

namespace webrtc_jni {

RTCStatsCollectorCallbackWrapper::RTCStatsCollectorCallbackWrapper(
    JNIEnv* jni,
    jobject j_callback)
    : j_callback_global_(jni, j_callback),
      j_callback_class_(jni, GetObjectClass(jni, j_callback)),
      j_stats_report_class_(FindClass(jni, "org/webrtc/RTCStatsReport")),
      j_stats_report_ctor_(GetMethodID(jni,
                                       j_stats_report_class_,
                                       "<init>",
                                       "(JLjava/util/Map;)V")),
      j_stats_class_(FindClass(jni, "org/webrtc/RTCStats")),
      j_stats_ctor_(GetMethodID(
          jni,
          j_stats_class_,
          "<init>",
          "(JLjava/lang/String;Ljava/lang/String;Ljava/util/Map;)V")),
      j_linked_hash_map_class_(FindClass(jni, "java/util/LinkedHashMap")),
      j_linked_hash_map_ctor_(
          GetMethodID(jni, j_linked_hash_map_class_, "<init>", "()V")),
      j_linked_hash_map_put_(GetMethodID(
          jni,
          j_linked_hash_map_class_,
          "put",
          "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")),
      j_boolean_class_(FindClass(jni, "java/lang/Boolean")),
      j_boolean_ctor_(GetMethodID(jni, j_boolean_class_, "<init>", "(Z)V")),
      j_integer_class_(FindClass(jni, "java/lang/Integer")),
      j_integer_ctor_(GetMethodID(jni, j_integer_class_, "<init>", "(I)V")),
      j_long_class_(FindClass(jni, "java/lang/Long")),
      j_long_ctor_(GetMethodID(jni, j_long_class_, "<init>", "(J)V")),
      j_big_integer_class_(FindClass(jni, "java/math/BigInteger")),
      j_big_integer_ctor_(GetMethodID(jni,
                                      j_big_integer_class_,
                                      "<init>",
                                      "(Ljava/lang/String;)V")),
      j_double_class_(FindClass(jni, "java/lang/Double")),
      j_double_ctor_(GetMethodID(jni, j_double_class_, "<init>", "(D)V")),
      j_string_class_(FindClass(jni, "java/lang/String")) {}

void RTCStatsCollectorCallbackWrapper::OnStatsDelivered(
    const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject j_report = ReportToJava(jni, report);
  jmethodID m = GetMethodID(jni, *j_callback_class_, "onStatsDelivered",
                            "(Lorg/webrtc/RTCStatsReport;)V");
  jni->CallVoidMethod(*j_callback_global_, m, j_report);
  CHECK_EXCEPTION(jni) << "error during CallVoidMethod";
}

jobject RTCStatsCollectorCallbackWrapper::ReportToJava(
    JNIEnv* jni,
    const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
  jobject j_stats_map =
      jni->NewObject(j_linked_hash_map_class_, j_linked_hash_map_ctor_);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  for (const webrtc::RTCStats& stats : *report) {
    // Create a local reference frame for each RTCStats, since there is a
    // maximum number of references that can be created in one frame.
    ScopedLocalRefFrame local_ref_frame(jni);
    jstring j_id = JavaStringFromStdString(jni, stats.id());
    jobject j_stats = StatsToJava(jni, stats);
    jni->CallObjectMethod(j_stats_map, j_linked_hash_map_put_, j_id, j_stats);
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  }
  jobject j_report = jni->NewObject(j_stats_report_class_, j_stats_report_ctor_,
                                    report->timestamp_us(), j_stats_map);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_report;
}

jobject RTCStatsCollectorCallbackWrapper::StatsToJava(
    JNIEnv* jni,
    const webrtc::RTCStats& stats) {
  jstring j_type = JavaStringFromStdString(jni, stats.type());
  jstring j_id = JavaStringFromStdString(jni, stats.id());
  jobject j_members =
      jni->NewObject(j_linked_hash_map_class_, j_linked_hash_map_ctor_);
  for (const webrtc::RTCStatsMemberInterface* member : stats.Members()) {
    if (!member->is_defined()) {
      continue;
    }
    // Create a local reference frame for each member as well.
    ScopedLocalRefFrame local_ref_frame(jni);
    jstring j_name = JavaStringFromStdString(jni, member->name());
    jobject j_member = MemberToJava(jni, member);
    jni->CallObjectMethod(j_members, j_linked_hash_map_put_, j_name, j_member);
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  }
  jobject j_stats =
      jni->NewObject(j_stats_class_, j_stats_ctor_, stats.timestamp_us(),
                     j_type, j_id, j_members);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_stats;
}

jobject RTCStatsCollectorCallbackWrapper::MemberToJava(
    JNIEnv* jni,
    const webrtc::RTCStatsMemberInterface* member) {
  switch (member->type()) {
    case webrtc::RTCStatsMemberInterface::kBool: {
      jobject value =
          jni->NewObject(j_boolean_class_, j_boolean_ctor_,
                         *member->cast_to<webrtc::RTCStatsMember<bool>>());
      CHECK_EXCEPTION(jni) << "error during NewObject";
      return value;
    }
    case webrtc::RTCStatsMemberInterface::kInt32: {
      jobject value =
          jni->NewObject(j_integer_class_, j_integer_ctor_,
                         *member->cast_to<webrtc::RTCStatsMember<int32_t>>());
      CHECK_EXCEPTION(jni) << "error during NewObject";
      return value;
    }
    case webrtc::RTCStatsMemberInterface::kUint32: {
      jobject value = jni->NewObject(
          j_long_class_, j_long_ctor_,
          (jlong)*member->cast_to<webrtc::RTCStatsMember<uint32_t>>());
      CHECK_EXCEPTION(jni) << "error during NewObject";
      return value;
    }
    case webrtc::RTCStatsMemberInterface::kInt64: {
      jobject value =
          jni->NewObject(j_long_class_, j_long_ctor_,
                         *member->cast_to<webrtc::RTCStatsMember<int64_t>>());
      CHECK_EXCEPTION(jni) << "error during NewObject";
      return value;
    }
    case webrtc::RTCStatsMemberInterface::kUint64: {
      jobject value =
          jni->NewObject(j_big_integer_class_, j_big_integer_ctor_,
                         JavaStringFromStdString(jni, member->ValueToString()));
      CHECK_EXCEPTION(jni) << "error during NewObject";
      return value;
    }
    case webrtc::RTCStatsMemberInterface::kDouble: {
      jobject value =
          jni->NewObject(j_double_class_, j_double_ctor_,
                         *member->cast_to<webrtc::RTCStatsMember<double>>());
      CHECK_EXCEPTION(jni) << "error during NewObject";
      return value;
    }
    case webrtc::RTCStatsMemberInterface::kString: {
      return JavaStringFromStdString(
          jni, *member->cast_to<webrtc::RTCStatsMember<std::string>>());
    }
    case webrtc::RTCStatsMemberInterface::kSequenceBool: {
      const std::vector<bool>& values =
          *member->cast_to<webrtc::RTCStatsMember<std::vector<bool>>>();
      jobjectArray j_values =
          jni->NewObjectArray(values.size(), j_boolean_class_, nullptr);
      CHECK_EXCEPTION(jni) << "error during NewObjectArray";
      for (size_t i = 0; i < values.size(); ++i) {
        jobject value =
            jni->NewObject(j_boolean_class_, j_boolean_ctor_, values[i]);
        jni->SetObjectArrayElement(j_values, i, value);
        CHECK_EXCEPTION(jni) << "error during SetObjectArrayElement";
      }
      return j_values;
    }
    case webrtc::RTCStatsMemberInterface::kSequenceInt32: {
      const std::vector<int32_t>& values =
          *member->cast_to<webrtc::RTCStatsMember<std::vector<int32_t>>>();
      jobjectArray j_values =
          jni->NewObjectArray(values.size(), j_integer_class_, nullptr);
      CHECK_EXCEPTION(jni) << "error during NewObjectArray";
      for (size_t i = 0; i < values.size(); ++i) {
        jobject value =
            jni->NewObject(j_integer_class_, j_integer_ctor_, values[i]);
        jni->SetObjectArrayElement(j_values, i, value);
        CHECK_EXCEPTION(jni) << "error during SetObjectArrayElement";
      }
      return j_values;
    }
    case webrtc::RTCStatsMemberInterface::kSequenceUint32: {
      const std::vector<uint32_t>& values =
          *member->cast_to<webrtc::RTCStatsMember<std::vector<uint32_t>>>();
      jobjectArray j_values =
          jni->NewObjectArray(values.size(), j_long_class_, nullptr);
      CHECK_EXCEPTION(jni) << "error during NewObjectArray";
      for (size_t i = 0; i < values.size(); ++i) {
        jobject value = jni->NewObject(j_long_class_, j_long_ctor_, values[i]);
        jni->SetObjectArrayElement(j_values, i, value);
        CHECK_EXCEPTION(jni) << "error during SetObjectArrayElement";
      }
      return j_values;
    }
    case webrtc::RTCStatsMemberInterface::kSequenceInt64: {
      const std::vector<int64_t>& values =
          *member->cast_to<webrtc::RTCStatsMember<std::vector<int64_t>>>();
      jobjectArray j_values =
          jni->NewObjectArray(values.size(), j_long_class_, nullptr);
      CHECK_EXCEPTION(jni) << "error during NewObjectArray";
      for (size_t i = 0; i < values.size(); ++i) {
        jobject value = jni->NewObject(j_long_class_, j_long_ctor_, values[i]);
        jni->SetObjectArrayElement(j_values, i, value);
        CHECK_EXCEPTION(jni) << "error during SetObjectArrayElement";
      }
      return j_values;
    }
    case webrtc::RTCStatsMemberInterface::kSequenceUint64: {
      const std::vector<uint64_t>& values =
          *member->cast_to<webrtc::RTCStatsMember<std::vector<uint64_t>>>();
      jobjectArray j_values =
          jni->NewObjectArray(values.size(), j_big_integer_class_, nullptr);
      CHECK_EXCEPTION(jni) << "error during NewObjectArray";
      for (size_t i = 0; i < values.size(); ++i) {
        jobject value = jni->NewObject(
            j_big_integer_class_, j_big_integer_ctor_,
            JavaStringFromStdString(jni, rtc::ToString(values[i])));
        jni->SetObjectArrayElement(j_values, i, value);
        CHECK_EXCEPTION(jni) << "error during SetObjectArrayElement";
      }
      return j_values;
    }
    case webrtc::RTCStatsMemberInterface::kSequenceDouble: {
      const std::vector<double>& values =
          *member->cast_to<webrtc::RTCStatsMember<std::vector<double>>>();
      jobjectArray j_values =
          jni->NewObjectArray(values.size(), j_double_class_, nullptr);
      CHECK_EXCEPTION(jni) << "error during NewObjectArray";
      for (size_t i = 0; i < values.size(); ++i) {
        jobject value =
            jni->NewObject(j_double_class_, j_double_ctor_, values[i]);
        jni->SetObjectArrayElement(j_values, i, value);
        CHECK_EXCEPTION(jni) << "error during SetObjectArrayElement";
      }
      return j_values;
    }
    case webrtc::RTCStatsMemberInterface::kSequenceString: {
      const std::vector<std::string>& values =
          *member->cast_to<webrtc::RTCStatsMember<std::vector<std::string>>>();
      jobjectArray j_values =
          jni->NewObjectArray(values.size(), j_string_class_, nullptr);
      CHECK_EXCEPTION(jni) << "error during NewObjectArray";
      for (size_t i = 0; i < values.size(); ++i) {
        jni->SetObjectArrayElement(j_values, i,
                                   JavaStringFromStdString(jni, values[i]));
        CHECK_EXCEPTION(jni) << "error during SetObjectArrayElement";
      }
      return j_values;
    }
  }
  RTC_NOTREACHED();
  return nullptr;
}

}  // namespace webrtc_jni
