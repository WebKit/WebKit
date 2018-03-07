/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/mediaconstraints_jni.h"

#include "rtc_base/ptr_util.h"
#include "sdk/android/generated_peerconnection_jni/jni/MediaConstraints_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

namespace {

// Helper for translating a List<Pair<String, String>> to a Constraints.
MediaConstraintsInterface::Constraints PopulateConstraintsFromJavaPairList(
    JNIEnv* env,
    jobject j_list) {
  MediaConstraintsInterface::Constraints constraints;
  for (jobject entry : Iterable(env, j_list)) {
    jstring j_key = Java_KeyValuePair_getKey(env, entry);
    jstring j_value = Java_KeyValuePair_getValue(env, entry);
    constraints.emplace_back(JavaToStdString(env, j_key),
                             JavaToStdString(env, j_value));
  }
  return constraints;
}

// Wrapper for a Java MediaConstraints object.  Copies all needed data so when
// the constructor returns the Java object is no longer needed.
class MediaConstraintsJni : public MediaConstraintsInterface {
 public:
  MediaConstraintsJni(JNIEnv* env, jobject j_constraints)
      : mandatory_(PopulateConstraintsFromJavaPairList(
            env,
            Java_MediaConstraints_getMandatory(env, j_constraints))),
        optional_(PopulateConstraintsFromJavaPairList(
            env,
            Java_MediaConstraints_getOptional(env, j_constraints))) {}
  virtual ~MediaConstraintsJni() = default;

  // MediaConstraintsInterface.
  const Constraints& GetMandatory() const override { return mandatory_; }
  const Constraints& GetOptional() const override { return optional_; }

 private:
  const Constraints mandatory_;
  const Constraints optional_;
};

}  // namespace

std::unique_ptr<MediaConstraintsInterface> JavaToNativeMediaConstraints(
    JNIEnv* env,
    jobject j_constraints) {
  return rtc::MakeUnique<MediaConstraintsJni>(env, j_constraints);
}

}  // namespace jni
}  // namespace webrtc
