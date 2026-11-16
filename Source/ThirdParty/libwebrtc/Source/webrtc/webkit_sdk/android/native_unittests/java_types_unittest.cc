/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/jni/java_types.h"

#include <memory>
#include <vector>

#include "sdk/android/generated_native_unittests_jni/JavaTypesTestHelper_jni.h"
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
      {"one", "1"},
      {"two", "2"},
      {"three", "3"},
  };
  EXPECT_EQ(expected, output);
}

TEST(JavaTypesTest, TestNativeToJavaToNativeIntArray) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();

  std::vector<int32_t> test_data{1, 20, 300};

  ScopedJavaLocalRef<jintArray> array = NativeToJavaIntArray(env, test_data);
  EXPECT_EQ(test_data, JavaToNativeIntArray(env, array));
}

TEST(JavaTypesTest, TestNativeToJavaToNativeByteArray) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();

  std::vector<int8_t> test_data{1, 20, 30};

  ScopedJavaLocalRef<jbyteArray> array = NativeToJavaByteArray(env, test_data);
  EXPECT_EQ(test_data, JavaToNativeByteArray(env, array));
}

TEST(JavaTypesTest, TestNativeToJavaToNativeIntArrayLeakTest) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();

  std::vector<int32_t> test_data{1, 20, 300};

  for (int i = 0; i < 2000; i++) {
    ScopedJavaLocalRef<jintArray> array = NativeToJavaIntArray(env, test_data);
    EXPECT_EQ(test_data, JavaToNativeIntArray(env, array));
  }
}

TEST(JavaTypesTest, TestNativeToJavaToNativeByteArrayLeakTest) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();

  std::vector<int8_t> test_data{1, 20, 30};

  for (int i = 0; i < 2000; i++) {
    ScopedJavaLocalRef<jbyteArray> array =
        NativeToJavaByteArray(env, test_data);
    EXPECT_EQ(test_data, JavaToNativeByteArray(env, array));
  }
}
}  // namespace
}  // namespace test
}  // namespace webrtc
