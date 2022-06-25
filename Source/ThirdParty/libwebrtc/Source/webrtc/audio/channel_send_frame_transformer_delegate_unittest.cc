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

#include <memory>
#include <utility>

#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_frame_transformer.h"
#include "test/mock_transformable_frame.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::SaveArg;

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
               int64_t absolute_capture_timestamp_ms));

  ChannelSendFrameTransformerDelegate::SendFrameCallback callback() {
    return [this](AudioFrameType frameType, uint8_t payloadType,
                  uint32_t rtp_timestamp, rtc::ArrayView<const uint8_t> payload,
                  int64_t absolute_capture_timestamp_ms) {
      return SendFrame(frameType, payloadType, rtp_timestamp, payload,
                       absolute_capture_timestamp_ms);
    };
  }
};

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
          mock_channel.callback(), mock_frame_transformer, &channel_queue);
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
  delegate->Transform(AudioFrameType::kEmptyFrame, 0, 0, 0, data, sizeof(data),
                      0, 0);
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
          mock_channel.callback(), mock_frame_transformer, &channel_queue);

  delegate->Reset();
  EXPECT_CALL(mock_channel, SendFrame).Times(0);
  delegate->OnTransformedFrame(std::make_unique<MockTransformableFrame>());
  channel_queue.WaitForPreviouslyPostedTasks();
}

}  // namespace
}  // namespace webrtc
