/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "sdk/android/generated_native_unittests_jni/jni/JavaTypesTestHelper_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
TEST(JavaTypesTest, TestJavaToNativeStringMap) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_map =
      jni::Java_JavaTypesTestHelper_createTestStringMap(env);

  std::map<std::string, std::string> output = JavaToNativeStringMap(env, j_map);

  std::map<std::string, std::string> expected{
      {"one", "1"}, {"two", "2"}, {"three", "3"},
  };
  EXPECT_EQ(expected, output);
}
}  // namespace
}  // namespace test
}  // namespace webrtc
