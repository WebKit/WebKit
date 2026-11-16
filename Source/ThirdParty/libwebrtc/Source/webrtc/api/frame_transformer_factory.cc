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
#include <span>
#include <string>
#include <vector>

#include "api/frame_transformer_interface.h"
#include "api/payload_type.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame_type.h"
#include "audio/channel_receive_frame_transformer_delegate.h"
#include "audio/channel_send_frame_transformer_delegate.h"
#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "rtc_base/checks.h"

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

std::unique_ptr<TransformableVideoFrameInterface> CreateOutgoingVideoFrame(
    VideoFrameType frame_type,
    PayloadType payload_type,
    uint32_t rtp_timestamp_without_offset,
    std::span<const uint8_t> payload_data,
    std::optional<int64_t> absolute_capture_timestamp_ms,
    const std::vector<uint32_t>& csrcs,
    VideoCodecType codec_type,
    std::optional<Timestamp> presentation_timestamp,
    uint16_t width,
    uint16_t height) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);

  RTPVideoHeader video_header;
  video_header.codec = codec_type;
  video_header.frame_type = frame_type;
  video_header.width = width;
  video_header.height = height;

  // init video_type_header variant
  switch (codec_type) {
    case VideoCodecType::kVideoCodecVP8: {
      RTPVideoHeaderVP8 vp8;
      vp8.InitRTPVideoHeaderVP8();
      video_header.video_type_header = vp8;
      break;
    }
    case VideoCodecType::kVideoCodecVP9: {
      RTPVideoHeaderVP9 vp9;
      vp9.InitRTPVideoHeaderVP9();
      video_header.video_type_header = vp9;
      break;
    }
    case VideoCodecType::kVideoCodecH264:
      video_header.video_type_header = RTPVideoHeaderH264();
      break;
    default:
      break;
  }

  EncodedImage encoded_image;
  encoded_image.SetEncodedData(
      EncodedImageBuffer::Create(payload_data.data(), payload_data.size()));
  encoded_image.set_frame_type(frame_type);
  encoded_image._encodedWidth = width;
  encoded_image._encodedHeight = height;
  if (absolute_capture_timestamp_ms.has_value()) {
    encoded_image.capture_time_ms_ = *absolute_capture_timestamp_ms;
  }
  if (presentation_timestamp.has_value()) {
    encoded_image.SetPresentationTimestamp(*presentation_timestamp);
  }

  return CreateSenderVideoFrame(
      encoded_image, video_header, payload_type, codec_type,
      RtpTimestampWithoutOffset{rtp_timestamp_without_offset}, csrcs);
}

}  // namespace webrtc
