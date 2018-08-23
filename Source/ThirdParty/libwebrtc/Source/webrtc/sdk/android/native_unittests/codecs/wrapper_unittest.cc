/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/codecs/wrapper.h"
#include "media/base/mediaconstants.h"
#include "sdk/android/generated_native_unittests_jni/jni/CodecsWrapperTestHelper_jni.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
TEST(JavaCodecsWrapperTest, JavaToNativeVideoCodecInfo) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_video_codec_info =
      jni::Java_CodecsWrapperTestHelper_createTestVideoCodecInfo(env);

  const SdpVideoFormat video_format =
      JavaToNativeVideoCodecInfo(env, j_video_codec_info.obj());

  EXPECT_EQ(cricket::kH264CodecName, video_format.name);
  const auto it =
      video_format.parameters.find(cricket::kH264FmtpProfileLevelId);
  ASSERT_NE(it, video_format.parameters.end());
  EXPECT_EQ(cricket::kH264ProfileLevelConstrainedBaseline, it->second);
}
}  // namespace
}  // namespace test
}  // namespace webrtc
