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

#include "absl/memory/memory.h"
#include "media/base/media_constants.h"
#include "sdk/android/generated_native_unittests_jni/CodecsWrapperTestHelper_jni.h"
#include "sdk/android/native_api/codecs/wrapper.h"
#include "sdk/android/src/jni/video_encoder_wrapper.h"
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

TEST(JavaCodecsWrapperTest, JavaToNativeResolutionBitrateLimits) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_fake_encoder =
      jni::Java_CodecsWrapperTestHelper_createFakeVideoEncoder(env);

  auto encoder = jni::JavaToNativeVideoEncoder(env, j_fake_encoder);
  ASSERT_TRUE(encoder);

  // Check that the bitrate limits correctly passed from Java to native.
  const std::vector<VideoEncoder::ResolutionBitrateLimits> bitrate_limits =
      encoder->GetEncoderInfo().resolution_bitrate_limits;
  ASSERT_EQ(bitrate_limits.size(), 1u);
  EXPECT_EQ(bitrate_limits[0].frame_size_pixels, 640 * 360);
  EXPECT_EQ(bitrate_limits[0].min_start_bitrate_bps, 300000);
  EXPECT_EQ(bitrate_limits[0].min_bitrate_bps, 200000);
  EXPECT_EQ(bitrate_limits[0].max_bitrate_bps, 1000000);
}
}  // namespace
}  // namespace test
}  // namespace webrtc
