/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

#include <utility>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "modules/rtp_rtcp/source/transformable_encoded_frame.h"
#include "rtc_base/task_utils/to_queued_task.h"

namespace webrtc {

RTPSenderVideoFrameTransformerDelegate::RTPSenderVideoFrameTransformerDelegate(
    RTPSenderVideo* sender,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer)
    : sender_(sender), frame_transformer_(std::move(frame_transformer)) {}

void RTPSenderVideoFrameTransformerDelegate::Init() {
  frame_transformer_->RegisterTransformedFrameCallback(
      rtc::scoped_refptr<TransformedFrameCallback>(this));
}

bool RTPSenderVideoFrameTransformerDelegate::TransformFrame(
    int payload_type,
    absl::optional<VideoCodecType> codec_type,
    uint32_t rtp_timestamp,
    const EncodedImage& encoded_image,
    const RTPFragmentationHeader* fragmentation,
    RTPVideoHeader video_header,
    absl::optional<int64_t> expected_retransmission_time_ms,
    uint32_t ssrc) {
  if (!encoder_queue_)
    encoder_queue_ = TaskQueueBase::Current();
  frame_transformer_->TransformFrame(
      std::make_unique<TransformableEncodedFrame>(
          encoded_image.GetEncodedData(), video_header, payload_type,
          codec_type, rtp_timestamp, encoded_image.capture_time_ms_,
          fragmentation, expected_retransmission_time_ms),
      RtpDescriptorAuthentication(video_header), ssrc);
  return true;
}

void RTPSenderVideoFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  {
    rtc::CritScope lock(&sender_lock_);
    if (!sender_)
      return;
  }
  auto transformed_frame = absl::WrapUnique(
      static_cast<TransformableEncodedFrame*>(frame.release()));
  rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate> delegate = this;
  encoder_queue_->PostTask(
      ToQueuedTask([delegate = std::move(delegate),
                    transformed_frame = std::move(transformed_frame)]() {
        delegate->SendVideo(*transformed_frame.get());
      }));
}

void RTPSenderVideoFrameTransformerDelegate::SendVideo(
    const TransformableEncodedFrame& transformed_frame) const {
  RTC_CHECK(encoder_queue_->IsCurrent());
  rtc::CritScope lock(&sender_lock_);
  if (!sender_)
    return;
  sender_->SendVideo(
      transformed_frame.PayloadType(), transformed_frame.codec_type(),
      transformed_frame.Timestamp(), transformed_frame.capture_time_ms(),
      transformed_frame.EncodedImage(),
      transformed_frame.fragmentation_header(),
      transformed_frame.video_header(),
      transformed_frame.expected_retransmission_time_ms());
}

void RTPSenderVideoFrameTransformerDelegate::SetVideoStructureUnderLock(
    const FrameDependencyStructure* video_structure) {
  rtc::CritScope lock(&sender_lock_);
  RTC_CHECK(sender_);
  sender_->SetVideoStructureUnderLock(video_structure);
}

void RTPSenderVideoFrameTransformerDelegate::Reset() {
  frame_transformer_->UnregisterTransformedFrameCallback();
  frame_transformer_ = nullptr;
  {
    rtc::CritScope lock(&sender_lock_);
    sender_ = nullptr;
  }
}
}  // namespace webrtc
