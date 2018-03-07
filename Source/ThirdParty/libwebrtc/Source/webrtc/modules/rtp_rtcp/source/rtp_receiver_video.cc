/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_receiver_video.h"

#include <assert.h>
#include <string.h>

#include <memory>

#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

RTPReceiverStrategy* RTPReceiverStrategy::CreateVideoStrategy(
    RtpData* data_callback) {
  return new RTPReceiverVideo(data_callback);
}

RTPReceiverVideo::RTPReceiverVideo(RtpData* data_callback)
    : RTPReceiverStrategy(data_callback) {
}

RTPReceiverVideo::~RTPReceiverVideo() {
}

bool RTPReceiverVideo::ShouldReportCsrcChanges(uint8_t payload_type) const {
  // Always do this for video packets.
  return true;
}

int32_t RTPReceiverVideo::OnNewPayloadTypeCreated(
    int payload_type,
    const SdpAudioFormat& audio_format) {
  RTC_NOTREACHED();
  return 0;
}

int32_t RTPReceiverVideo::ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                                         const PayloadUnion& specific_payload,
                                         bool is_red,
                                         const uint8_t* payload,
                                         size_t payload_length,
                                         int64_t timestamp_ms) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "Video::ParseRtp",
               "seqnum", rtp_header->header.sequenceNumber, "timestamp",
               rtp_header->header.timestamp);
  rtp_header->type.Video.codec =
      specific_payload.video_payload().videoCodecType;

  RTC_DCHECK_GE(payload_length, rtp_header->header.paddingLength);
  const size_t payload_data_length =
      payload_length - rtp_header->header.paddingLength;

  if (payload == NULL || payload_data_length == 0) {
    return data_callback_->OnReceivedPayloadData(NULL, 0, rtp_header) == 0 ? 0
                                                                           : -1;
  }

  if (first_packet_received_()) {
    RTC_LOG(LS_INFO) << "Received first video RTP packet";
  }

  // We are not allowed to hold a critical section when calling below functions.
  std::unique_ptr<RtpDepacketizer> depacketizer(
      RtpDepacketizer::Create(rtp_header->type.Video.codec));
  if (depacketizer.get() == NULL) {
    RTC_LOG(LS_ERROR) << "Failed to create depacketizer.";
    return -1;
  }

  RtpDepacketizer::ParsedPayload parsed_payload;
  if (!depacketizer->Parse(&parsed_payload, payload, payload_data_length))
    return -1;

  rtp_header->frameType = parsed_payload.frame_type;
  rtp_header->type = parsed_payload.type;
  rtp_header->type.Video.rotation = kVideoRotation_0;
  rtp_header->type.Video.content_type = VideoContentType::UNSPECIFIED;
  rtp_header->type.Video.video_timing.flags = TimingFrameFlags::kInvalid;

  // Retrieve the video rotation information.
  if (rtp_header->header.extension.hasVideoRotation) {
    rtp_header->type.Video.rotation =
        rtp_header->header.extension.videoRotation;
  }

  if (rtp_header->header.extension.hasVideoContentType) {
    rtp_header->type.Video.content_type =
        rtp_header->header.extension.videoContentType;
  }

  if (rtp_header->header.extension.has_video_timing) {
    rtp_header->type.Video.video_timing =
        rtp_header->header.extension.video_timing;
  }

  rtp_header->type.Video.playout_delay =
      rtp_header->header.extension.playout_delay;

  return data_callback_->OnReceivedPayloadData(parsed_payload.payload,
                                               parsed_payload.payload_length,
                                               rtp_header) == 0
             ? 0
             : -1;
}

RTPAliveType RTPReceiverVideo::ProcessDeadOrAlive(
    uint16_t last_payload_length) const {
  return kRtpDead;
}

int32_t RTPReceiverVideo::InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const PayloadUnion& specific_payload) const {
  // TODO(pbos): Remove as soon as audio can handle a changing payload type
  // without this callback.
  return 0;
}

}  // namespace webrtc
