/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/video_rtp_receiver.h"

#include <memory>

#include "api/video/test/mock_recordable_encoded_frame.h"
#include "media/base/fake_media_engine.h"
#include "test/gmock.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace webrtc {
namespace {

class VideoRtpReceiverTest : public testing::Test {
 protected:
  class MockVideoMediaChannel : public cricket::FakeVideoMediaChannel {
   public:
    MockVideoMediaChannel(
        cricket::FakeVideoEngine* engine,
        const cricket::VideoOptions& options,
        TaskQueueBase* network_thread = rtc::Thread::Current())
        : FakeVideoMediaChannel(engine, options, network_thread) {}
    MOCK_METHOD(void,
                SetRecordableEncodedFrameCallback,
                (uint32_t, std::function<void(const RecordableEncodedFrame&)>),
                (override));
    MOCK_METHOD(void,
                ClearRecordableEncodedFrameCallback,
                (uint32_t),
                (override));
    MOCK_METHOD(void, GenerateKeyFrame, (uint32_t), (override));
  };

  class MockVideoSink : public rtc::VideoSinkInterface<RecordableEncodedFrame> {
   public:
    MOCK_METHOD(void, OnFrame, (const RecordableEncodedFrame&), (override));
  };

  VideoRtpReceiverTest()
      : worker_thread_(rtc::Thread::Create()),
        channel_(nullptr, cricket::VideoOptions()),
        receiver_(rtc::make_ref_counted<VideoRtpReceiver>(
            worker_thread_.get(),
            std::string("receiver"),
            std::vector<std::string>({"stream"}))) {
    worker_thread_->Start();
    receiver_->SetMediaChannel(&channel_);
  }

  ~VideoRtpReceiverTest() override {
    // Clear expectations that tests may have set up before calling Stop().
    Mock::VerifyAndClearExpectations(&channel_);
    receiver_->Stop();
  }

  webrtc::VideoTrackSourceInterface* Source() {
    return receiver_->streams()[0]->FindVideoTrack("receiver")->GetSource();
  }

  std::unique_ptr<rtc::Thread> worker_thread_;
  NiceMock<MockVideoMediaChannel> channel_;
  rtc::scoped_refptr<VideoRtpReceiver> receiver_;
};

TEST_F(VideoRtpReceiverTest, SupportsEncodedOutput) {
  EXPECT_TRUE(Source()->SupportsEncodedOutput());
}

TEST_F(VideoRtpReceiverTest, GeneratesKeyFrame) {
  EXPECT_CALL(channel_, GenerateKeyFrame(0));
  Source()->GenerateKeyFrame();
}

TEST_F(VideoRtpReceiverTest,
       GenerateKeyFrameOnChannelSwitchUnlessGenerateKeyframeCalled) {
  // A channel switch without previous call to GenerateKeyFrame shouldn't
  // cause a call to happen on the new channel.
  MockVideoMediaChannel channel2(nullptr, cricket::VideoOptions());
  EXPECT_CALL(channel_, GenerateKeyFrame).Times(0);
  EXPECT_CALL(channel2, GenerateKeyFrame).Times(0);
  receiver_->SetMediaChannel(&channel2);
  Mock::VerifyAndClearExpectations(&channel2);

  // Generate a key frame. When we switch channel next time, we will have to
  // re-generate it as we don't know if it was eventually received
  Source()->GenerateKeyFrame();
  MockVideoMediaChannel channel3(nullptr, cricket::VideoOptions());
  EXPECT_CALL(channel3, GenerateKeyFrame);
  receiver_->SetMediaChannel(&channel3);

  // Switching to a new channel should now not cause calls to GenerateKeyFrame.
  StrictMock<MockVideoMediaChannel> channel4(nullptr, cricket::VideoOptions());
  receiver_->SetMediaChannel(&channel4);

  // We must call Stop() here since the mock media channels live on the stack
  // and `receiver_` still has a pointer to those objects.
  receiver_->Stop();
}

TEST_F(VideoRtpReceiverTest, EnablesEncodedOutput) {
  EXPECT_CALL(channel_, SetRecordableEncodedFrameCallback(/*ssrc=*/0, _));
  EXPECT_CALL(channel_, ClearRecordableEncodedFrameCallback).Times(0);
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
}

TEST_F(VideoRtpReceiverTest, DisablesEncodedOutput) {
  EXPECT_CALL(channel_, ClearRecordableEncodedFrameCallback(/*ssrc=*/0));
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
  Source()->RemoveEncodedSink(&sink);
}

TEST_F(VideoRtpReceiverTest, DisablesEnablesEncodedOutputOnChannelSwitch) {
  InSequence s;
  EXPECT_CALL(channel_, SetRecordableEncodedFrameCallback);
  EXPECT_CALL(channel_, ClearRecordableEncodedFrameCallback);
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
  MockVideoMediaChannel channel2(nullptr, cricket::VideoOptions());
  EXPECT_CALL(channel2, SetRecordableEncodedFrameCallback);
  receiver_->SetMediaChannel(&channel2);
  Mock::VerifyAndClearExpectations(&channel2);

  // When clearing encoded frame buffer function, we need channel switches
  // to NOT set the callback again.
  EXPECT_CALL(channel2, ClearRecordableEncodedFrameCallback);
  Source()->RemoveEncodedSink(&sink);
  StrictMock<MockVideoMediaChannel> channel3(nullptr, cricket::VideoOptions());
  receiver_->SetMediaChannel(&channel3);

  // We must call Stop() here since the mock media channels live on the stack
  // and `receiver_` still has a pointer to those objects.
  receiver_->Stop();
}

TEST_F(VideoRtpReceiverTest, BroadcastsEncodedFramesWhenEnabled) {
  std::function<void(const RecordableEncodedFrame&)> broadcast;
  EXPECT_CALL(channel_, SetRecordableEncodedFrameCallback(_, _))
      .WillRepeatedly(SaveArg<1>(&broadcast));
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);

  // Make sure SetEncodedFrameBufferFunction completes.
  Mock::VerifyAndClearExpectations(&channel_);

  // Pass two frames on different contexts.
  EXPECT_CALL(sink, OnFrame).Times(2);
  MockRecordableEncodedFrame frame;
  broadcast(frame);
  worker_thread_->Invoke<void>(RTC_FROM_HERE, [&] { broadcast(frame); });
}

TEST_F(VideoRtpReceiverTest, EnablesEncodedOutputOnChannelRestart) {
  InSequence s;
  EXPECT_CALL(channel_, ClearRecordableEncodedFrameCallback(0));
  MockVideoSink sink;
  Source()->AddEncodedSink(&sink);
  EXPECT_CALL(channel_, SetRecordableEncodedFrameCallback(4711, _));
  receiver_->SetupMediaChannel(4711);
  EXPECT_CALL(channel_, ClearRecordableEncodedFrameCallback(4711));
  EXPECT_CALL(channel_, SetRecordableEncodedFrameCallback(0, _));
  receiver_->SetupUnsignaledMediaChannel();
}

}  // namespace
}  // namespace webrtc
