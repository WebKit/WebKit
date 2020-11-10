/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rtp_video_stream_receiver_frame_transformer_delegate.h"

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/call/transport.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_frame_transformer.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::SaveArg;

std::unique_ptr<video_coding::RtpFrameObject> CreateRtpFrameObject(
    const RTPVideoHeader& video_header) {
  return std::make_unique<video_coding::RtpFrameObject>(
      0, 0, true, 0, 0, 0, 0, 0, VideoSendTiming(), 0, video_header.codec,
      kVideoRotation_0, VideoContentType::UNSPECIFIED, video_header,
      absl::nullopt, RtpPacketInfos(), EncodedImageBuffer::Create(0));
}

std::unique_ptr<video_coding::RtpFrameObject> CreateRtpFrameObject() {
  return CreateRtpFrameObject(RTPVideoHeader());
}

class TestRtpVideoFrameReceiver : public RtpVideoFrameReceiver {
 public:
  TestRtpVideoFrameReceiver() {}
  ~TestRtpVideoFrameReceiver() override = default;

  MOCK_METHOD(void,
              ManageFrame,
              (std::unique_ptr<video_coding::RtpFrameObject> frame),
              (override));
};

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest,
     RegisterTransformedFrameCallbackSinkOnInit) {
  TestRtpVideoFrameReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer(
      new rtc::RefCountedObject<MockFrameTransformer>());
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate(
      new rtc::RefCountedObject<RtpVideoStreamReceiverFrameTransformerDelegate>(
          &receiver, frame_transformer, rtc::Thread::Current(),
          /*remote_ssrc*/ 1111));
  EXPECT_CALL(*frame_transformer,
              RegisterTransformedFrameSinkCallback(testing::_, 1111));
  delegate->Init();
}

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest,
     UnregisterTransformedFrameSinkCallbackOnReset) {
  TestRtpVideoFrameReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer(
      new rtc::RefCountedObject<MockFrameTransformer>());
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate(
      new rtc::RefCountedObject<RtpVideoStreamReceiverFrameTransformerDelegate>(
          &receiver, frame_transformer, rtc::Thread::Current(),
          /*remote_ssrc*/ 1111));
  EXPECT_CALL(*frame_transformer, UnregisterTransformedFrameSinkCallback(1111));
  delegate->Reset();
}

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest, TransformFrame) {
  TestRtpVideoFrameReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> frame_transformer(
      new rtc::RefCountedObject<testing::NiceMock<MockFrameTransformer>>());
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate(
      new rtc::RefCountedObject<RtpVideoStreamReceiverFrameTransformerDelegate>(
          &receiver, frame_transformer, rtc::Thread::Current(),
          /*remote_ssrc*/ 1111));
  auto frame = CreateRtpFrameObject();
  EXPECT_CALL(*frame_transformer, Transform);
  delegate->TransformFrame(std::move(frame));
}

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest,
     ManageFrameOnTransformedFrame) {
  TestRtpVideoFrameReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer(
      new rtc::RefCountedObject<NiceMock<MockFrameTransformer>>());
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate =
      new rtc::RefCountedObject<RtpVideoStreamReceiverFrameTransformerDelegate>(
          &receiver, mock_frame_transformer, rtc::Thread::Current(),
          /*remote_ssrc*/ 1111);

  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameSinkCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  EXPECT_CALL(receiver, ManageFrame);
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&callback](std::unique_ptr<TransformableFrameInterface> frame) {
            callback->OnTransformedFrame(std::move(frame));
          });
  delegate->TransformFrame(CreateRtpFrameObject());
  rtc::ThreadManager::ProcessAllMessageQueuesForTesting();
}

TEST(RtpVideoStreamReceiverFrameTransformerDelegateTest,
     TransformableFrameMetadataHasCorrectValue) {
  TestRtpVideoFrameReceiver receiver;
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      new rtc::RefCountedObject<NiceMock<MockFrameTransformer>>();
  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate> delegate =
      new rtc::RefCountedObject<RtpVideoStreamReceiverFrameTransformerDelegate>(
          &receiver, mock_frame_transformer, rtc::Thread::Current(), 1111);
  delegate->Init();
  RTPVideoHeader video_header;
  video_header.width = 1280u;
  video_header.height = 720u;
  RTPVideoHeader::GenericDescriptorInfo& generic =
      video_header.generic.emplace();
  generic.frame_id = 10;
  generic.temporal_index = 3;
  generic.spatial_index = 2;
  generic.decode_target_indications = {DecodeTargetIndication::kSwitch};
  generic.dependencies = {5};

  // Check that the transformable frame passed to the frame transformer has the
  // correct metadata.
  EXPECT_CALL(*mock_frame_transformer, Transform)
      .WillOnce(
          [](std::unique_ptr<TransformableFrameInterface> transformable_frame) {
            auto frame =
                absl::WrapUnique(static_cast<TransformableVideoFrameInterface*>(
                    transformable_frame.release()));
            ASSERT_TRUE(frame);
            auto metadata = frame->GetMetadata();
            EXPECT_EQ(metadata.GetWidth(), 1280u);
            EXPECT_EQ(metadata.GetHeight(), 720u);
            EXPECT_EQ(metadata.GetFrameId(), 10);
            EXPECT_EQ(metadata.GetTemporalIndex(), 3);
            EXPECT_EQ(metadata.GetSpatialIndex(), 2);
            EXPECT_THAT(metadata.GetFrameDependencies(), ElementsAre(5));
            EXPECT_THAT(metadata.GetDecodeTargetIndications(),
                        ElementsAre(DecodeTargetIndication::kSwitch));
          });
  // The delegate creates a transformable frame from the RtpFrameObject.
  delegate->TransformFrame(CreateRtpFrameObject(video_header));
}

}  // namespace
}  // namespace webrtc
