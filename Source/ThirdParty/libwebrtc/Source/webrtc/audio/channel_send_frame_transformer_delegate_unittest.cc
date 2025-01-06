/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_send_frame_transformer_delegate.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_frame_transformer.h"
#include "api/test/mock_transformable_audio_frame.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SaveArg;

const uint8_t mock_data[] = {1, 2, 3, 4};

class MockChannelSend {
 public:
  MockChannelSend() = default;
  ~MockChannelSend() = default;

  MOCK_METHOD(int32_t,
              SendFrame,
              (AudioFrameType frameType,
               uint8_t payloadType,
               uint32_t rtp_timestamp,
               rtc::ArrayView<const uint8_t> payload,
               int64_t absolute_capture_timestamp_ms,
               rtc::ArrayView<const uint32_t> csrcs,
               std::optional<uint8_t> audio_level_dbov));

  ChannelSendFrameTransformerDelegate::SendFrameCallback callback() {
    return [this](AudioFrameType frameType, uint8_t payloadType,
                  uint32_t rtp_timestamp, rtc::ArrayView<const uint8_t> payload,
                  int64_t absolute_capture_timestamp_ms,
                  rtc::ArrayView<const uint32_t> csrcs,
                  std::optional<uint8_t> audio_level_dbov) {
      return SendFrame(frameType, payloadType, rtp_timestamp, payload,
                       absolute_capture_timestamp_ms, csrcs, audio_level_dbov);
    };
  }
};

std::unique_ptr<TransformableAudioFrameInterface> CreateMockReceiverFrame(
    const std::vector<uint32_t>& csrcs,
    std::optional<uint8_t> audio_level_dbov) {
  std::unique_ptr<MockTransformableAudioFrame> mock_frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  rtc::ArrayView<const uint8_t> payload(mock_data);
  ON_CALL(*mock_frame, GetData).WillByDefault(Return(payload));
  ON_CALL(*mock_frame, GetPayloadType).WillByDefault(Return(0));
  ON_CALL(*mock_frame, GetDirection)
      .WillByDefault(Return(TransformableFrameInterface::Direction::kReceiver));
  ON_CALL(*mock_frame, GetContributingSources).WillByDefault(Return(csrcs));
  ON_CALL(*mock_frame, SequenceNumber).WillByDefault(Return(987654321));
  ON_CALL(*mock_frame, AudioLevel).WillByDefault(Return(audio_level_dbov));
  return mock_frame;
}

std::unique_ptr<TransformableAudioFrameInterface> CreateFrame() {
  TaskQueueForTest channel_queue("channel_queue");
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  MockChannelSend mock_channel;
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      rtc::make_ref_counted<ChannelSendFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, channel_queue.Get());

  std::unique_ptr<TransformableFrameInterface> frame;
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&frame](
              std::unique_ptr<TransformableFrameInterface> transform_frame) {
            frame = std::move(transform_frame);
          });
  delegate->Transform(
      AudioFrameType::kEmptyFrame, 0, 0, mock_data, sizeof(mock_data), 0,
      /*ssrc=*/0, /*mimeType=*/"audio/opus", /*audio_level_dbov=*/123);
  return absl::WrapUnique(
      static_cast<webrtc::TransformableAudioFrameInterface*>(frame.release()));
}

// Test that the delegate registers itself with the frame transformer on Init().
TEST(ChannelSendFrameTransformerDelegateTest,
     RegisterTransformedFrameCallbackOnInit) {
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<MockFrameTransformer>();
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      rtc::make_ref_counted<ChannelSendFrameTransformerDelegate>(
          ChannelSendFrameTransformerDelegate::SendFrameCallback(),
          mock_frame_transformer, nullptr);
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback);
  delegate->Init();
}

// Test that the delegate unregisters itself from the frame transformer on
// Reset().
TEST(ChannelSendFrameTransformerDelegateTest,
     UnregisterTransformedFrameCallbackOnReset) {
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<MockFrameTransformer>();
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      rtc::make_ref_counted<ChannelSendFrameTransformerDelegate>(
          ChannelSendFrameTransformerDelegate::SendFrameCallback(),
          mock_frame_transformer, nullptr);
  EXPECT_CALL(*mock_frame_transformer, UnregisterTransformedFrameCallback);
  delegate->Reset();
}

// Test that when the delegate receives a transformed frame from the frame
// transformer, it passes it to the channel using the SendFrameCallback.
TEST(ChannelSendFrameTransformerDelegateTest,
     TransformRunsChannelSendCallback) {
  TaskQueueForTest channel_queue("channel_queue");
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  MockChannelSend mock_channel;
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      rtc::make_ref_counted<ChannelSendFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, channel_queue.Get());
  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  const uint8_t data[] = {1, 2, 3, 4};
  EXPECT_CALL(mock_channel, SendFrame);
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&callback](std::unique_ptr<TransformableFrameInterface> frame) {
            callback->OnTransformedFrame(std::move(frame));
          });
  delegate->Transform(AudioFrameType::kEmptyFrame, 0, 0, data, sizeof(data), 0,
                      /*ssrc=*/0, /*mimeType=*/"audio/opus",
                      /*audio_level_dbov=*/31);
  channel_queue.WaitForPreviouslyPostedTasks();
}

// Test that when the delegate receives a Incoming frame from the frame
// transformer, it passes it to the channel using the SendFrameCallback.
TEST(ChannelSendFrameTransformerDelegateTest,
     TransformRunsChannelSendCallbackForIncomingFrame) {
  TaskQueueForTest channel_queue("channel_queue");
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<NiceMock<MockFrameTransformer>>();
  MockChannelSend mock_channel;
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      rtc::make_ref_counted<ChannelSendFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, channel_queue.Get());
  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(SaveArg<0>(&callback));
  delegate->Init();
  ASSERT_TRUE(callback);

  const std::vector<uint32_t> csrcs = {123, 234, 345, 456};
  const uint8_t audio_level_dbov = 17;
  EXPECT_CALL(mock_channel, SendFrame).Times(0);
  EXPECT_CALL(mock_channel,
              SendFrame(_, 0, 0, ElementsAreArray(mock_data), _,
                        ElementsAreArray(csrcs), Optional(audio_level_dbov)));
  ON_CALL(*mock_frame_transformer, Transform)
      .WillByDefault(
          [&](std::unique_ptr<TransformableFrameInterface> /* frame */) {
            callback->OnTransformedFrame(CreateMockReceiverFrame(
                csrcs, std::optional<uint8_t>(audio_level_dbov)));
          });
  delegate->Transform(AudioFrameType::kEmptyFrame, 0, 0, mock_data,
                      sizeof(mock_data), 0,
                      /*ssrc=*/0, /*mimeType=*/"audio/opus",
                      /*audio_level_dbov=*/std::nullopt);
  channel_queue.WaitForPreviouslyPostedTasks();
}

// Test that if the delegate receives a transformed frame after it has been
// reset, it does not run the SendFrameCallback, as the channel is destroyed
// after resetting the delegate.
TEST(ChannelSendFrameTransformerDelegateTest,
     OnTransformedDoesNotRunChannelSendCallbackAfterReset) {
  TaskQueueForTest channel_queue("channel_queue");
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<testing::NiceMock<MockFrameTransformer>>();
  MockChannelSend mock_channel;
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      rtc::make_ref_counted<ChannelSendFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, channel_queue.Get());

  delegate->Reset();
  EXPECT_CALL(mock_channel, SendFrame).Times(0);
  delegate->OnTransformedFrame(std::make_unique<MockTransformableAudioFrame>());
  channel_queue.WaitForPreviouslyPostedTasks();
}

TEST(ChannelSendFrameTransformerDelegateTest, ShortCircuitingSkipsTransform) {
  TaskQueueForTest channel_queue("channel_queue");
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<testing::NiceMock<MockFrameTransformer>>();
  MockChannelSend mock_channel;
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      rtc::make_ref_counted<ChannelSendFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, channel_queue.Get());

  delegate->StartShortCircuiting();

  // Will not call the actual transformer.
  EXPECT_CALL(*mock_frame_transformer, Transform).Times(0);
  // Will pass the frame straight to the channel.
  EXPECT_CALL(mock_channel, SendFrame);
  const uint8_t data[] = {1, 2, 3, 4};
  delegate->Transform(AudioFrameType::kEmptyFrame, 0, 0, data, sizeof(data), 0,
                      /*ssrc=*/0, /*mimeType=*/"audio/opus",
                      /*audio_level_dbov=*/std::nullopt);
}

TEST(ChannelSendFrameTransformerDelegateTest,
     CloningSenderFramePreservesInformation) {
  std::unique_ptr<TransformableAudioFrameInterface> frame = CreateFrame();
  std::unique_ptr<TransformableAudioFrameInterface> cloned_frame =
      CloneSenderAudioFrame(frame.get());

  EXPECT_EQ(cloned_frame->GetTimestamp(), frame->GetTimestamp());
  EXPECT_EQ(cloned_frame->GetSsrc(), frame->GetSsrc());
  EXPECT_EQ(cloned_frame->Type(), frame->Type());
  EXPECT_EQ(cloned_frame->GetPayloadType(), frame->GetPayloadType());
  EXPECT_EQ(cloned_frame->GetMimeType(), frame->GetMimeType());
  EXPECT_THAT(cloned_frame->GetContributingSources(),
              ElementsAreArray(frame->GetContributingSources()));
  EXPECT_EQ(cloned_frame->AudioLevel(), frame->AudioLevel());
}

TEST(ChannelSendFrameTransformerDelegateTest, CloningReceiverFrameWithCsrcs) {
  std::unique_ptr<TransformableAudioFrameInterface> frame =
      CreateMockReceiverFrame(/*csrcs=*/{123, 234, 345},
                              std::optional<uint8_t>(72));
  std::unique_ptr<TransformableAudioFrameInterface> cloned_frame =
      CloneSenderAudioFrame(frame.get());

  EXPECT_EQ(cloned_frame->GetTimestamp(), frame->GetTimestamp());
  EXPECT_EQ(cloned_frame->GetSsrc(), frame->GetSsrc());
  EXPECT_EQ(cloned_frame->Type(), frame->Type());
  EXPECT_EQ(cloned_frame->GetPayloadType(), frame->GetPayloadType());
  EXPECT_EQ(cloned_frame->GetMimeType(), frame->GetMimeType());
  EXPECT_EQ(cloned_frame->AbsoluteCaptureTimestamp(),
            frame->AbsoluteCaptureTimestamp());

  ASSERT_NE(frame->GetContributingSources().size(), 0u);
  EXPECT_THAT(cloned_frame->GetContributingSources(),
              ElementsAreArray(frame->GetContributingSources()));
  EXPECT_EQ(cloned_frame->SequenceNumber(), frame->SequenceNumber());
  EXPECT_EQ(cloned_frame->AudioLevel(), frame->AudioLevel());
}

}  // namespace
}  // namespace webrtc
