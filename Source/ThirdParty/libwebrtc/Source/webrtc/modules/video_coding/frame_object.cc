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

#include "common_video/h264/h264_common.h"
#include "modules/video_coding/packet_buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace video_coding {

RtpFrameObject::RtpFrameObject(PacketBuffer* packet_buffer,
                               uint16_t first_seq_num,
                               uint16_t last_seq_num,
                               size_t frame_size,
                               int times_nacked,
                               int64_t received_time)
    : packet_buffer_(packet_buffer),
      first_seq_num_(first_seq_num),
      last_seq_num_(last_seq_num),
      received_time_(received_time),
      times_nacked_(times_nacked) {
  VCMPacket* first_packet = packet_buffer_->GetPacket(first_seq_num);
  RTC_CHECK(first_packet);

  // EncodedFrame members
  frame_type_ = first_packet->frameType;
  codec_type_ = first_packet->codec;

  // TODO(philipel): Remove when encoded image is replaced by EncodedFrame.
  // VCMEncodedFrame members
  CopyCodecSpecific(&first_packet->video_header);
  _completeFrame = true;
  _payloadType = first_packet->payloadType;
  SetTimestamp(first_packet->timestamp);
  ntp_time_ms_ = first_packet->ntp_time_ms_;
  _frameType = first_packet->frameType;

  // Setting frame's playout delays to the same values
  // as of the first packet's.
  SetPlayoutDelay(first_packet->video_header.playout_delay);

  AllocateBitstreamBuffer(frame_size);
  bool bitstream_copied = GetBitstream(_buffer);
  RTC_DCHECK(bitstream_copied);
  _encodedWidth = first_packet->width;
  _encodedHeight = first_packet->height;

  // EncodedFrame members
  SetTimestamp(first_packet->timestamp);

  VCMPacket* last_packet = packet_buffer_->GetPacket(last_seq_num);
  RTC_CHECK(last_packet);
  RTC_CHECK(last_packet->is_last_packet_in_frame);
  // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
  // ts_126114v120700p.pdf Section 7.4.5.
  // The MTSI client shall add the payload bytes as defined in this clause
  // onto the last RTP packet in each group of packets which make up a key
  // frame (I-frame or IDR frame in H.264 (AVC), or an IRAP picture in H.265
  // (HEVC)).
  rotation_ = last_packet->video_header.rotation;
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

    timing_.receive_start_ms = first_packet->receive_time_ms;
    timing_.receive_finish_ms = last_packet->receive_time_ms;
  }
  timing_.flags = last_packet->video_header.video_timing.flags;
}

RtpFrameObject::~RtpFrameObject() {
  packet_buffer_->ReturnFrame(this);
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

FrameType RtpFrameObject::frame_type() const {
  return frame_type_;
}

VideoCodecType RtpFrameObject::codec_type() const {
  return codec_type_;
}

void RtpFrameObject::SetBitstream(rtc::ArrayView<const uint8_t> bitstream) {
  AllocateBitstreamBuffer(bitstream.size());
  memcpy(_buffer, bitstream.data(), _length);
}

bool RtpFrameObject::GetBitstream(uint8_t* destination) const {
  return packet_buffer_->GetBitstream(*this, destination);
}

int64_t RtpFrameObject::ReceivedTime() const {
  return received_time_;
}

int64_t RtpFrameObject::RenderTime() const {
  return _renderTimeMs;
}

bool RtpFrameObject::delayed_by_retransmission() const {
  return times_nacked() > 0;
}

absl::optional<RTPVideoHeader> RtpFrameObject::GetRtpVideoHeader() const {
  rtc::CritScope lock(&packet_buffer_->crit_);
  VCMPacket* packet = packet_buffer_->GetPacket(first_seq_num_);
  if (!packet)
    return absl::nullopt;
  return packet->video_header;
}

absl::optional<RtpGenericFrameDescriptor>
RtpFrameObject::GetGenericFrameDescriptor() const {
  rtc::CritScope lock(&packet_buffer_->crit_);
  VCMPacket* packet = packet_buffer_->GetPacket(first_seq_num_);
  if (!packet)
    return absl::nullopt;
  return packet->generic_descriptor;
}

absl::optional<FrameMarking> RtpFrameObject::GetFrameMarking() const {
  rtc::CritScope lock(&packet_buffer_->crit_);
  VCMPacket* packet = packet_buffer_->GetPacket(first_seq_num_);
  if (!packet)
    return absl::nullopt;
  return packet->video_header.frame_marking;
}

void RtpFrameObject::AllocateBitstreamBuffer(size_t frame_size) {
  // Since FFmpeg use an optimized bitstream reader that reads in chunks of
  // 32/64 bits we have to add at least that much padding to the buffer
  // to make sure the decoder doesn't read out of bounds.
  // NOTE! EncodedImage::_size is the size of the buffer (think capacity of
  //       an std::vector) and EncodedImage::_length is the actual size of
  //       the bitstream (think size of an std::vector).
  size_t new_size = frame_size + (codec_type_ == kVideoCodecH264
                                      ? EncodedImage::kBufferPaddingBytesH264
                                      : 0);
  if (_size < new_size) {
    delete[] _buffer;
    _buffer = new uint8_t[new_size];
    _size = new_size;
  }

  _length = frame_size;
}

}  // namespace video_coding
}  // namespace webrtc
