/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"

#include <assert.h>
#include <string.h>

#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_cvo.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

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
    const CodecInst& audio_codec) {
  RTC_NOTREACHED();
  return 0;
}

int32_t RTPReceiverVideo::ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                                         const PayloadUnion& specific_payload,
                                         bool is_red,
                                         const uint8_t* payload,
                                         size_t payload_length,
                                         int64_t timestamp_ms,
                                         bool is_first_packet) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"), "Video::ParseRtp",
               "seqnum", rtp_header->header.sequenceNumber, "timestamp",
               rtp_header->header.timestamp);
  rtp_header->type.Video.codec = specific_payload.Video.videoCodecType;

  RTC_DCHECK_GE(payload_length, rtp_header->header.paddingLength);
  const size_t payload_data_length =
      payload_length - rtp_header->header.paddingLength;

  if (payload == NULL || payload_data_length == 0) {
    return data_callback_->OnReceivedPayloadData(NULL, 0, rtp_header) == 0 ? 0
                                                                           : -1;
  }

  if (first_packet_received_()) {
    LOG(LS_INFO) << "Received first video RTP packet";
  }

  // We are not allowed to hold a critical section when calling below functions.
  std::unique_ptr<RtpDepacketizer> depacketizer(
      RtpDepacketizer::Create(rtp_header->type.Video.codec));
  if (depacketizer.get() == NULL) {
    LOG(LS_ERROR) << "Failed to create depacketizer.";
    return -1;
  }

  rtp_header->type.Video.is_first_packet_in_frame = is_first_packet;
  RtpDepacketizer::ParsedPayload parsed_payload;
  if (!depacketizer->Parse(&parsed_payload, payload, payload_data_length))
    return -1;

  rtp_header->frameType = parsed_payload.frame_type;
  rtp_header->type = parsed_payload.type;
  rtp_header->type.Video.rotation = kVideoRotation_0;
  rtp_header->type.Video.content_type = VideoContentType::UNSPECIFIED;
  rtp_header->type.Video.video_timing.is_timing_frame = false;

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
    rtp_header->type.Video.video_timing.is_timing_frame = true;
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
