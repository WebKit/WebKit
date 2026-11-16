/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/video/video_source.h"

#include <vector>

#include "api/video/video_sink_interface.h"
#include "sdk/android/generated_native_unittests_jni/JavaVideoSourceTestHelper_jni.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {
class TestVideoSink : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  void OnFrame(const VideoFrame& frame) { frames_.push_back(frame); }

  std::vector<VideoFrame> GetFrames() {
    std::vector<VideoFrame> temp = frames_;
    frames_.clear();
    return temp;
  }

 private:
  std::vector<VideoFrame> frames_;
};
}  // namespace

TEST(JavaVideoSourceTest, CreateJavaVideoSource) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  rtc::ThreadManager::Instance()->WrapCurrentThread();

  rtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(
          env, rtc::ThreadManager::Instance()->CurrentThread(),
          false /* is_screencast */, true /* align_timestamps */);

  ASSERT_NE(nullptr, video_track_source);
  EXPECT_NE(nullptr,
            video_track_source->GetJavaVideoCapturerObserver(env).obj());
}

TEST(JavaVideoSourceTest, OnFrameCapturedFrameIsDeliveredToSink) {
  TestVideoSink test_video_sink;

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  rtc::ThreadManager::Instance()->WrapCurrentThread();

  rtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(
          env, rtc::ThreadManager::Instance()->CurrentThread(),
          false /* is_screencast */, true /* align_timestamps */);
  video_track_source->AddOrUpdateSink(&test_video_sink, rtc::VideoSinkWants());

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      true /* success */);
  const int width = 20;
  const int height = 32;
  const int rotation = 180;
  const int64_t timestamp = 987654321;
  jni::Java_JavaVideoSourceTestHelper_deliverFrame(
      env, width, height, rotation, timestamp,
      video_track_source->GetJavaVideoCapturerObserver(env));

  std::vector<VideoFrame> frames = test_video_sink.GetFrames();
  ASSERT_EQ(1u, frames.size());
  webrtc::VideoFrame frame = frames[0];
  EXPECT_EQ(width, frame.width());
  EXPECT_EQ(height, frame.height());
  EXPECT_EQ(rotation, frame.rotation());
}

TEST(JavaVideoSourceTest,
     OnFrameCapturedFrameIsDeliveredToSinkWithPreservedTimestamp) {
  TestVideoSink test_video_sink;

  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  rtc::ThreadManager::Instance()->WrapCurrentThread();

  rtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(
          env, rtc::ThreadManager::Instance()->CurrentThread(),
          false /* is_screencast */, false /* align_timestamps */);
  video_track_source->AddOrUpdateSink(&test_video_sink, rtc::VideoSinkWants());

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      true /* success */);
  const int width = 20;
  const int height = 32;
  const int rotation = 180;
  const int64_t timestamp = 987654321;
  jni::Java_JavaVideoSourceTestHelper_deliverFrame(
      env, width, height, rotation, 987654321,
      video_track_source->GetJavaVideoCapturerObserver(env));

  std::vector<VideoFrame> frames = test_video_sink.GetFrames();
  ASSERT_EQ(1u, frames.size());
  webrtc::VideoFrame frame = frames[0];
  EXPECT_EQ(width, frame.width());
  EXPECT_EQ(height, frame.height());
  EXPECT_EQ(rotation, frame.rotation());
  EXPECT_EQ(timestamp / 1000, frame.timestamp_us());
}

TEST(JavaVideoSourceTest, CapturerStartedSuccessStateBecomesLive) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  rtc::ThreadManager::Instance()->WrapCurrentThread();

  rtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(
          env, rtc::ThreadManager::Instance()->CurrentThread(),
          false /* is_screencast */, true /* align_timestamps */);

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      true /* success */);

  EXPECT_EQ(VideoTrackSourceInterface::SourceState::kLive,
            video_track_source->state());
}

TEST(JavaVideoSourceTest, CapturerStartedFailureStateBecomesEnded) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  rtc::ThreadManager::Instance()->WrapCurrentThread();

  rtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(
          env, rtc::ThreadManager::Instance()->CurrentThread(),
          false /* is_screencast */, true /* align_timestamps */);

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      false /* success */);

  EXPECT_EQ(VideoTrackSourceInterface::SourceState::kEnded,
            video_track_source->state());
}

TEST(JavaVideoSourceTest, CapturerStoppedStateBecomesEnded) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  // Wrap test thread so it can be used as the signaling thread.
  rtc::ThreadManager::Instance()->WrapCurrentThread();

  rtc::scoped_refptr<JavaVideoTrackSourceInterface> video_track_source =
      CreateJavaVideoSource(
          env, rtc::ThreadManager::Instance()->CurrentThread(),
          false /* is_screencast */, true /* align_timestamps */);

  jni::Java_JavaVideoSourceTestHelper_startCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env),
      true /* success */);
  jni::Java_JavaVideoSourceTestHelper_stopCapture(
      env, video_track_source->GetJavaVideoCapturerObserver(env));

  EXPECT_EQ(VideoTrackSourceInterface::SourceState::kEnded,
            video_track_source->state());
}

}  // namespace test
}  // namespace webrtc
