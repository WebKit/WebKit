/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/packet_buffer.h"

namespace webrtc {
namespace video_coding {

FrameObject::FrameObject()
    : picture_id(0),
      spatial_layer(0),
      timestamp(0),
      num_references(0),
      inter_layer_predicted(false) {}

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
  VCMPacket* packet = packet_buffer_->GetPacket(first_seq_num);
  if (packet) {
    // RtpFrameObject members
    frame_type_ = packet->frameType;
    codec_type_ = packet->codec;

    // TODO(philipel): Remove when encoded image is replaced by FrameObject.
    // VCMEncodedFrame members
    CopyCodecSpecific(&packet->video_header);
    _completeFrame = true;
    _payloadType = packet->payloadType;
    _timeStamp = packet->timestamp;
    ntp_time_ms_ = packet->ntp_time_ms_;

    // Since FFmpeg use an optimized bitstream reader that reads in chunks of
    // 32/64 bits we have to add at least that much padding to the buffer
    // to make sure the decoder doesn't read out of bounds.
    // NOTE! EncodedImage::_size is the size of the buffer (think capacity of
    //       an std::vector) and EncodedImage::_length is the actual size of
    //       the bitstream (think size of an std::vector).
    if (codec_type_ == kVideoCodecH264)
      _size = frame_size + EncodedImage::kBufferPaddingBytesH264;
    else
      _size = frame_size;

    _buffer = new uint8_t[_size];
    _length = frame_size;
    _frameType = packet->frameType;
    GetBitstream(_buffer);

    // FrameObject members
    timestamp = packet->timestamp;
  }
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

bool RtpFrameObject::GetBitstream(uint8_t* destination) const {
  return packet_buffer_->GetBitstream(*this, destination);
}

uint32_t RtpFrameObject::Timestamp() const {
  return timestamp_;
}

int64_t RtpFrameObject::ReceivedTime() const {
  return received_time_;
}

int64_t RtpFrameObject::RenderTime() const {
  return _renderTimeMs;
}

rtc::Optional<RTPVideoTypeHeader> RtpFrameObject::GetCodecHeader() const {
  rtc::CritScope lock(&packet_buffer_->crit_);
  VCMPacket* packet = packet_buffer_->GetPacket(first_seq_num_);
  if (!packet)
    return rtc::Optional<RTPVideoTypeHeader>();
  return rtc::Optional<RTPVideoTypeHeader>(packet->video_header.codecHeader);
}

}  // namespace video_coding
}  // namespace webrtc
