/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/video_rtp_track_source.h"

#include "rtc_base/ref_counted_object.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class MockCallback : public VideoRtpTrackSource::Callback {
 public:
  MOCK_METHOD0(OnGenerateKeyFrame, void());
  MOCK_METHOD1(OnEncodedSinkEnabled, void(bool));
};

class MockSink : public rtc::VideoSinkInterface<RecordableEncodedFrame> {
 public:
  MOCK_METHOD1(OnFrame, void(const RecordableEncodedFrame&));
};

rtc::scoped_refptr<VideoRtpTrackSource> MakeSource(
    VideoRtpTrackSource::Callback* callback) {
  rtc::scoped_refptr<VideoRtpTrackSource> source(
      new rtc::RefCountedObject<VideoRtpTrackSource>(callback));
  return source;
}

TEST(VideoRtpTrackSourceTest, CreatesWithRemoteAtttributeSet) {
  EXPECT_TRUE(MakeSource(nullptr)->remote());
}

TEST(VideoRtpTrackSourceTest, EnablesEncodingOutputOnAddingSink) {
  MockCallback mock_callback;
  EXPECT_CALL(mock_callback, OnGenerateKeyFrame).Times(0);
  auto source = MakeSource(&mock_callback);
  MockSink sink;
  EXPECT_CALL(mock_callback, OnEncodedSinkEnabled(true));
  source->AddEncodedSink(&sink);
}

TEST(VideoRtpTrackSourceTest, EnablesEncodingOutputOnceOnAddingTwoSinks) {
  MockCallback mock_callback;
  EXPECT_CALL(mock_callback, OnGenerateKeyFrame).Times(0);
  auto source = MakeSource(&mock_callback);
  MockSink sink;
  EXPECT_CALL(mock_callback, OnEncodedSinkEnabled(true)).Times(1);
  source->AddEncodedSink(&sink);
  MockSink sink2;
  source->AddEncodedSink(&sink2);
}

TEST(VideoRtpTrackSourceTest, DisablesEncodingOutputOnOneSinkRemoved) {
  MockCallback mock_callback;
  EXPECT_CALL(mock_callback, OnGenerateKeyFrame).Times(0);
  EXPECT_CALL(mock_callback, OnEncodedSinkEnabled(true));
  EXPECT_CALL(mock_callback, OnEncodedSinkEnabled(false)).Times(0);
  auto source = MakeSource(&mock_callback);
  MockSink sink;
  source->AddEncodedSink(&sink);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
  EXPECT_CALL(mock_callback, OnEncodedSinkEnabled(false));
  source->RemoveEncodedSink(&sink);
}

TEST(VideoRtpTrackSourceTest, DisablesEncodingOutputOnLastSinkRemoved) {
  MockCallback mock_callback;
  EXPECT_CALL(mock_callback, OnGenerateKeyFrame).Times(0);
  EXPECT_CALL(mock_callback, OnEncodedSinkEnabled(true));
  auto source = MakeSource(&mock_callback);
  MockSink sink;
  source->AddEncodedSink(&sink);
  MockSink sink2;
  source->AddEncodedSink(&sink2);
  source->RemoveEncodedSink(&sink);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);
  EXPECT_CALL(mock_callback, OnEncodedSinkEnabled(false));
  source->RemoveEncodedSink(&sink2);
}

TEST(VideoRtpTrackSourceTest, GeneratesKeyFrameWhenRequested) {
  MockCallback mock_callback;
  auto source = MakeSource(&mock_callback);
  EXPECT_CALL(mock_callback, OnGenerateKeyFrame);
  source->GenerateKeyFrame();
}

TEST(VideoRtpTrackSourceTest, NoCallbacksAfterClearedCallback) {
  testing::StrictMock<MockCallback> mock_callback;
  auto source = MakeSource(&mock_callback);
  source->ClearCallback();
  MockSink sink;
  source->AddEncodedSink(&sink);
  source->GenerateKeyFrame();
  source->RemoveEncodedSink(&sink);
}

class TestFrame : public RecordableEncodedFrame {
 public:
  rtc::scoped_refptr<const webrtc::EncodedImageBufferInterface> encoded_buffer()
      const override {
    return nullptr;
  }
  absl::optional<webrtc::ColorSpace> color_space() const override {
    return absl::nullopt;
  }
  VideoCodecType codec() const override { return kVideoCodecGeneric; }
  bool is_key_frame() const override { return false; }
  EncodedResolution resolution() const override {
    return EncodedResolution{0, 0};
  }
  Timestamp render_time() const override { return Timestamp::Millis(0); }
};

TEST(VideoRtpTrackSourceTest, BroadcastsFrames) {
  auto source = MakeSource(nullptr);
  MockSink sink;
  source->AddEncodedSink(&sink);
  MockSink sink2;
  source->AddEncodedSink(&sink2);
  TestFrame frame;
  EXPECT_CALL(sink, OnFrame);
  EXPECT_CALL(sink2, OnFrame);
  source->BroadcastRecordableEncodedFrame(frame);
}

}  // namespace
}  // namespace webrtc
