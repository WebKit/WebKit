/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/frame_transformer_factory.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/payload_type.h"
#include "audio/channel_receive_frame_transformer_delegate.h"
#include "audio/channel_send_frame_transformer_delegate.h"
#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

namespace webrtc {

std::unique_ptr<TransformableAudioFrameInterface> CloneAudioFrame(
    TransformableAudioFrameInterface* original) {
  if (original->GetDirection() ==
      TransformableAudioFrameInterface::Direction::kReceiver)
    return CloneReceiverAudioFrame(original);
  return CloneSenderAudioFrame(original);
}

std::unique_ptr<TransformableVideoFrameInterface> CloneVideoFrame(
    TransformableVideoFrameInterface* original) {
  // At the moment, only making sender frames from receiver frames is
  // supported.
  return CloneSenderVideoFrame(original);
}

std::unique_ptr<TransformableAudioFrameInterface> CreateOutgoingAudioFrame(
    TransformableAudioFrameInterface::FrameType frame_type,
    PayloadType payload_type,
    uint32_t rtp_timestamp_without_offset,
    const uint8_t* payload_data,
    size_t payload_size,
    std::optional<uint64_t> absolute_capture_timestamp_ms,
    uint32_t ssrc,
    const std::vector<uint32_t>& csrcs,
    const std::string& codec_mime_type,
    std::optional<uint16_t> sequence_number,
    std::optional<uint8_t> audio_level_dbov) {
  return CreateSenderAudioFrame(
      frame_type, payload_type.value(),
      RtpTimestampWithoutOffset{rtp_timestamp_without_offset}, payload_data,
      payload_size, absolute_capture_timestamp_ms, ssrc, csrcs, codec_mime_type,
      sequence_number, audio_level_dbov);
}

}  // namespace webrtc
