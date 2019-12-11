/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_object.h"

#include <string.h>

#include <utility>

#include "api/video/encoded_image.h"
#include "api/video/video_timing.h"
#include "modules/video_coding/packet.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"

namespace webrtc {
namespace video_coding {

RtpFrameObject::RtpFrameObject(PacketBuffer* packet_buffer,
                               uint16_t first_seq_num,
                               uint16_t last_seq_num,
                               size_t frame_size,
                               int times_nacked,
                               int64_t first_packet_received_time,
                               int64_t last_packet_received_time,
                               RtpPacketInfos packet_infos)
    : first_seq_num_(first_seq_num),
      last_seq_num_(last_seq_num),
      last_packet_received_time_(last_packet_received_time),
      times_nacked_(times_nacked) {
  VCMPacket* first_packet = packet_buffer->GetPacket(first_seq_num);
  RTC_CHECK(first_packet);

  rtp_video_header_ = first_packet->video_header;
  rtp_generic_frame_descriptor_ = first_packet->generic_descriptor;

  // EncodedFrame members
  codec_type_ = first_packet->codec();

  // TODO(philipel): Remove when encoded image is replaced by EncodedFrame.
  // VCMEncodedFrame members
  CopyCodecSpecific(&first_packet->video_header);
  _completeFrame = true;
  _payloadType = first_packet->payloadType;
  SetTimestamp(first_packet->timestamp);
  ntp_time_ms_ = first_packet->ntp_time_ms_;
  _frameType = first_packet->video_header.frame_type;

  // Setting frame's playout delays to the same values
  // as of the first packet's.
  SetPlayoutDelay(first_packet->video_header.playout_delay);

  // TODO(nisse): Change GetBitstream to return the buffer?
  SetEncodedData(EncodedImageBuffer::Create(frame_size));
  bool bitstream_copied = packet_buffer->GetBitstream(*this, data());
  RTC_DCHECK(bitstream_copied);
  _encodedWidth = first_packet->width();
  _encodedHeight = first_packet->height();

  // EncodedFrame members
  SetTimestamp(first_packet->timestamp);
  SetPacketInfos(std::move(packet_infos));

  VCMPacket* last_packet = packet_buffer->GetPacket(last_seq_num);
  RTC_CHECK(last_packet);
  RTC_CHECK(last_packet->is_last_packet_in_frame());
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
  // ts_126114v120700p.pdf Section 7.4.5.
  // The MTSI client shall add the payload bytes as defined in this clause
  // onto the last RTP packet in each group of packets which make up a key
  // frame (I-frame or IDR frame in H.264 (AVC), or an IRAP picture in H.265
  // (HEVC)).
  rotation_ = last_packet->video_header.rotation;
  SetColorSpace(last_packet->video_header.color_space);
  _rotation_set = true;
  content_type_ = last_packet->video_header.content_type;
  if (last_packet->video_header.video_timing.flags !=
      VideoSendTiming::kInvalid) {
    // ntp_time_ms_ may be -1 if not estimated yet. This is not a problem,
    // as this will be dealt with at the time of reporting.
    timing_.encode_start_ms =
        ntp_time_ms_ +
        last_packet->video_header.video_timing.encode_start_delta_ms;
    timing_.encode_finish_ms =
        ntp_time_ms_ +
        last_packet->video_header.video_timing.encode_finish_delta_ms;
    timing_.packetization_finish_ms =
        ntp_time_ms_ +
        last_packet->video_header.video_timing.packetization_finish_delta_ms;
    timing_.pacer_exit_ms =
        ntp_time_ms_ +
        last_packet->video_header.video_timing.pacer_exit_delta_ms;
    timing_.network_timestamp_ms =
        ntp_time_ms_ +
        last_packet->video_header.video_timing.network_timestamp_delta_ms;
    timing_.network2_timestamp_ms =
        ntp_time_ms_ +
        last_packet->video_header.video_timing.network2_timestamp_delta_ms;
  }
  timing_.receive_start_ms = first_packet_received_time;
  timing_.receive_finish_ms = last_packet_received_time;
  timing_.flags = last_packet->video_header.video_timing.flags;
  is_last_spatial_layer = last_packet->markerBit;
}

RtpFrameObject::~RtpFrameObject() {
}

uint16_t RtpFrameObject::first_seq_num() const {
  return first_seq_num_;
}

uint16_t RtpFrameObject::last_seq_num() const {
  return last_seq_num_;
}

int RtpFrameObject::times_nacked() const {
  return times_nacked_;
}

VideoFrameType RtpFrameObject::frame_type() const {
  return rtp_video_header_.frame_type;
}

VideoCodecType RtpFrameObject::codec_type() const {
  return codec_type_;
}

int64_t RtpFrameObject::ReceivedTime() const {
  return last_packet_received_time_;
}

int64_t RtpFrameObject::RenderTime() const {
  return _renderTimeMs;
}

bool RtpFrameObject::delayed_by_retransmission() const {
  return times_nacked() > 0;
}

const RTPVideoHeader& RtpFrameObject::GetRtpVideoHeader() const {
  return rtp_video_header_;
}

const absl::optional<RtpGenericFrameDescriptor>&
RtpFrameObject::GetGenericFrameDescriptor() const {
  return rtp_generic_frame_descriptor_;
}

const FrameMarking& RtpFrameObject::GetFrameMarking() const {
  return rtp_video_header_.frame_marking;
}

}  // namespace video_coding
}  // namespace webrtc
