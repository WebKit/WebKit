/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/mediaconstraints.h"

#include "absl/memory/memory.h"
#include "sdk/android/generated_peerconnection_jni/jni/MediaConstraints_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

namespace {

// Helper for translating a List<Pair<String, String>> to a Constraints.
MediaConstraintsInterface::Constraints PopulateConstraintsFromJavaPairList(
    JNIEnv* env,
    const JavaRef<jobject>& j_list) {
  MediaConstraintsInterface::Constraints constraints;
  for (const JavaRef<jobject>& entry : Iterable(env, j_list)) {
    constraints.emplace_back(
        JavaToStdString(env, Java_KeyValuePair_getKey(env, entry)),
        JavaToStdString(env, Java_KeyValuePair_getValue(env, entry)));
  }
  return constraints;
}

// Wrapper for a Java MediaConstraints object.  Copies all needed data so when
// the constructor returns the Java object is no longer needed.
class MediaConstraintsJni : public MediaConstraintsInterface {
 public:
  MediaConstraintsJni(JNIEnv* env, const JavaRef<jobject>& j_constraints)
      : mandatory_(PopulateConstraintsFromJavaPairList(
            env,
            Java_MediaConstraints_getMandatory(env, j_constraints))),
        optional_(PopulateConstraintsFromJavaPairList(
            env,
            Java_MediaConstraints_getOptional(env, j_constraints))) {}
  ~MediaConstraintsJni() override = default;

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
    const JavaRef<jobject>& j_constraints) {
  return absl::make_unique<MediaConstraintsJni>(env, j_constraints);
}

}  // namespace jni
}  // namespace webrtc
