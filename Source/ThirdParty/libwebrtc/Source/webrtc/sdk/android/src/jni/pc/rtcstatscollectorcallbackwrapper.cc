/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/rtcstatscollectorcallbackwrapper.h"

#include <string>
#include <vector>

#include "rtc_base/stringencode.h"
#include "sdk/android/generated_external_classes_jni/jni/BigInteger_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/RTCStatsCollectorCallback_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/RTCStatsReport_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/RTCStats_jni.h"
#include "sdk/android/src/jni/classreferenceholder.h"

namespace webrtc {
namespace jni {

namespace {

jobject NativeToJavaBigInteger(JNIEnv* env, uint64_t u) {
  return JNI_BigInteger::Java_BigInteger_ConstructorJMBI_JLS(
      env, NativeToJavaString(env, rtc::ToString(u)));
}

jobjectArray NativeToJavaBigIntegerArray(
    JNIEnv* env,
    const std::vector<uint64_t>& container) {
  return NativeToJavaObjectArray(
      env, container, java_math_BigInteger_clazz(env), &NativeToJavaBigInteger);
}

jobject MemberToJava(JNIEnv* env, const RTCStatsMemberInterface& member) {
  switch (member.type()) {
    case RTCStatsMemberInterface::kBool:
      return NativeToJavaBoolean(env, *member.cast_to<RTCStatsMember<bool>>());

    case RTCStatsMemberInterface::kInt32:
      return NativeToJavaInteger(env,
                                 *member.cast_to<RTCStatsMember<int32_t>>());

    case RTCStatsMemberInterface::kUint32:
      return NativeToJavaLong(env, *member.cast_to<RTCStatsMember<uint32_t>>());

    case RTCStatsMemberInterface::kInt64:
      return NativeToJavaLong(env, *member.cast_to<RTCStatsMember<int64_t>>());

    case RTCStatsMemberInterface::kUint64:
      return NativeToJavaBigInteger(
          env, *member.cast_to<RTCStatsMember<uint64_t>>());

    case RTCStatsMemberInterface::kDouble:
      return NativeToJavaDouble(env, *member.cast_to<RTCStatsMember<double>>());

    case RTCStatsMemberInterface::kString:
      return NativeToJavaString(env,
                                *member.cast_to<RTCStatsMember<std::string>>());

    case RTCStatsMemberInterface::kSequenceBool:
      return NativeToJavaBooleanArray(
          env, *member.cast_to<RTCStatsMember<std::vector<bool>>>());

    case RTCStatsMemberInterface::kSequenceInt32:
      return NativeToJavaIntegerArray(
          env, *member.cast_to<RTCStatsMember<std::vector<int32_t>>>());

    case RTCStatsMemberInterface::kSequenceUint32: {
      const std::vector<uint32_t>& v =
          *member.cast_to<RTCStatsMember<std::vector<uint32_t>>>();
      return NativeToJavaLongArray(env,
                                   std::vector<int64_t>(v.begin(), v.end()));
    }
    case RTCStatsMemberInterface::kSequenceInt64:
      return NativeToJavaLongArray(
          env, *member.cast_to<RTCStatsMember<std::vector<int64_t>>>());

    case RTCStatsMemberInterface::kSequenceUint64:
      return NativeToJavaBigIntegerArray(
          env, *member.cast_to<RTCStatsMember<std::vector<uint64_t>>>());

    case RTCStatsMemberInterface::kSequenceDouble:
      return NativeToJavaDoubleArray(
          env, *member.cast_to<RTCStatsMember<std::vector<double>>>());

    case RTCStatsMemberInterface::kSequenceString:
      return NativeToJavaStringArray(
          env, *member.cast_to<RTCStatsMember<std::vector<std::string>>>());
  }
  RTC_NOTREACHED();
  return nullptr;
}

}  // namespace

RTCStatsCollectorCallbackWrapper::RTCStatsCollectorCallbackWrapper(
    JNIEnv* jni,
    jobject j_callback)
    : j_callback_global_(jni, j_callback),
      j_linked_hash_map_class_(FindClass(jni, "java/util/LinkedHashMap")),
      j_linked_hash_map_ctor_(
          GetMethodID(jni, j_linked_hash_map_class_, "<init>", "()V")),
      j_linked_hash_map_put_(GetMethodID(
          jni,
          j_linked_hash_map_class_,
          "put",
          "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")) {}

void RTCStatsCollectorCallbackWrapper::OnStatsDelivered(
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject j_report = ReportToJava(jni, report);
  Java_RTCStatsCollectorCallback_onStatsDelivered(jni, *j_callback_global_,
                                                  j_report);
}

jobject RTCStatsCollectorCallbackWrapper::ReportToJava(
    JNIEnv* jni,
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  jobject j_stats_map =
      jni->NewObject(j_linked_hash_map_class_, j_linked_hash_map_ctor_);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  for (const RTCStats& stats : *report) {
    // Create a local reference frame for each RTCStats, since there is a
    // maximum number of references that can be created in one frame.
    ScopedLocalRefFrame local_ref_frame(jni);
    jstring j_id = NativeToJavaString(jni, stats.id());
    jobject j_stats = StatsToJava(jni, stats);
    jni->CallObjectMethod(j_stats_map, j_linked_hash_map_put_, j_id, j_stats);
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  }
  jobject j_report =
      Java_RTCStatsReport_create(jni, report->timestamp_us(), j_stats_map);
  return j_report;
}

jobject RTCStatsCollectorCallbackWrapper::StatsToJava(JNIEnv* jni,
                                                      const RTCStats& stats) {
  jstring j_type = NativeToJavaString(jni, stats.type());
  jstring j_id = NativeToJavaString(jni, stats.id());
  jobject j_members =
      jni->NewObject(j_linked_hash_map_class_, j_linked_hash_map_ctor_);
  for (const RTCStatsMemberInterface* member : stats.Members()) {
    if (!member->is_defined()) {
      continue;
    }
    // Create a local reference frame for each member as well.
    ScopedLocalRefFrame local_ref_frame(jni);
    jstring j_name = NativeToJavaString(jni, member->name());
    jobject j_member = MemberToJava(jni, *member);
    jni->CallObjectMethod(j_members, j_linked_hash_map_put_, j_name, j_member);
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  }
  jobject j_stats =
      Java_RTCStats_create(jni, stats.timestamp_us(), j_type, j_id, j_members);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_stats;
}

}  // namespace jni
}  // namespace webrtc
