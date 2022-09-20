/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive_frame_transformer_delegate.h"

#include <utility>

#include "rtc_base/buffer.h"

namespace webrtc {
namespace {

class TransformableIncomingAudioFrame
    : public TransformableAudioFrameInterface {
 public:
  TransformableIncomingAudioFrame(rtc::ArrayView<const uint8_t> payload,
                                  const RTPHeader& header,
                                  uint32_t ssrc)
      : payload_(payload.data(), payload.size()),
        header_(header),
        ssrc_(ssrc) {}
  ~TransformableIncomingAudioFrame() override = default;
  rtc::ArrayView<const uint8_t> GetData() const override { return payload_; }

  void SetData(rtc::ArrayView<const uint8_t> data) override {
    payload_.SetData(data.data(), data.size());
  }

  uint8_t GetPayloadType() const override { return header_.payloadType; }
  uint32_t GetSsrc() const override { return ssrc_; }
  uint32_t GetTimestamp() const override { return header_.timestamp; }
  const RTPHeader& GetHeader() const override { return header_; }
  Direction GetDirection() const override { return Direction::kReceiver; }

 private:
  rtc::Buffer payload_;
  RTPHeader header_;
  uint32_t ssrc_;
};
}  // namespace

ChannelReceiveFrameTransformerDelegate::ChannelReceiveFrameTransformerDelegate(
    ReceiveFrameCallback receive_frame_callback,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
    TaskQueueBase* channel_receive_thread)
    : receive_frame_callback_(receive_frame_callback),
      frame_transformer_(std::move(frame_transformer)),
      channel_receive_thread_(channel_receive_thread) {}

void ChannelReceiveFrameTransformerDelegate::Init() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  frame_transformer_->RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>(this));
}

void ChannelReceiveFrameTransformerDelegate::Reset() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  frame_transformer_->UnregisterTransformedFrameCallback();
  frame_transformer_ = nullptr;
  receive_frame_callback_ = ReceiveFrameCallback();
}

void ChannelReceiveFrameTransformerDelegate::Transform(
    rtc::ArrayView<const uint8_t> packet,
    const RTPHeader& header,
    uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  frame_transformer_->Transform(
      std::make_unique<TransformableIncomingAudioFrame>(packet, header, ssrc));
}

void ChannelReceiveFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<TransformableFrameInterface> frame) {
  rtc::scoped_refptr<ChannelReceiveFrameTransformerDelegate> delegate(this);
  channel_receive_thread_->PostTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        delegate->ReceiveFrame(std::move(frame));
      });
}

void ChannelReceiveFrameTransformerDelegate::ReceiveFrame(
    std::unique_ptr<TransformableFrameInterface> frame) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!receive_frame_callback_)
    return;
  RTC_CHECK_EQ(frame->GetDirection(),
               TransformableFrameInterface::Direction::kReceiver);
  auto* transformed_frame =
      static_cast<TransformableIncomingAudioFrame*>(frame.get());
  receive_frame_callback_(transformed_frame->GetData(),
                          transformed_frame->GetHeader());
}
}  // namespace webrtc
