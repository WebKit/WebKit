/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/testutils.h"

#include <math.h>
#include <algorithm>
#include <memory>

#include "api/video/video_frame.h"
#include "media/base/videocapturer.h"
#include "rtc_base/bytebuffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/stream.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/testutils.h"

namespace cricket {

/////////////////////////////////////////////////////////////////////////
// Implementation of RawRtpPacket
/////////////////////////////////////////////////////////////////////////
void RawRtpPacket::WriteToByteBuffer(uint32_t in_ssrc,
                                     rtc::ByteBufferWriter* buf) const {
  if (!buf) return;

  buf->WriteUInt8(ver_to_cc);
  buf->WriteUInt8(m_to_pt);
  buf->WriteUInt16(sequence_number);
  buf->WriteUInt32(timestamp);
  buf->WriteUInt32(in_ssrc);
  buf->WriteBytes(payload, sizeof(payload));
}

bool RawRtpPacket::ReadFromByteBuffer(rtc::ByteBufferReader* buf) {
  if (!buf) return false;

  bool ret = true;
  ret &= buf->ReadUInt8(&ver_to_cc);
  ret &= buf->ReadUInt8(&m_to_pt);
  ret &= buf->ReadUInt16(&sequence_number);
  ret &= buf->ReadUInt32(&timestamp);
  ret &= buf->ReadUInt32(&ssrc);
  ret &= buf->ReadBytes(payload, sizeof(payload));
  return ret;
}

bool RawRtpPacket::SameExceptSeqNumTimestampSsrc(const RawRtpPacket& packet,
                                                 uint16_t seq,
                                                 uint32_t ts,
                                                 uint32_t ssc) const {
  return sequence_number == seq &&
      timestamp == ts &&
      ver_to_cc == packet.ver_to_cc &&
      m_to_pt == packet.m_to_pt &&
      ssrc == ssc &&
      0 == memcmp(payload, packet.payload, sizeof(payload));
}

/////////////////////////////////////////////////////////////////////////
// Implementation of RawRtcpPacket
/////////////////////////////////////////////////////////////////////////
void RawRtcpPacket::WriteToByteBuffer(rtc::ByteBufferWriter *buf) const {
  if (!buf) return;

  buf->WriteUInt8(ver_to_count);
  buf->WriteUInt8(type);
  buf->WriteUInt16(length);
  buf->WriteBytes(payload, sizeof(payload));
}

bool RawRtcpPacket::ReadFromByteBuffer(rtc::ByteBufferReader* buf) {
  if (!buf) return false;

  bool ret = true;
  ret &= buf->ReadUInt8(&ver_to_count);
  ret &= buf->ReadUInt8(&type);
  ret &= buf->ReadUInt16(&length);
  ret &= buf->ReadBytes(payload, sizeof(payload));
  return ret;
}

bool RawRtcpPacket::EqualsTo(const RawRtcpPacket& packet) const {
  return ver_to_count == packet.ver_to_count &&
      type == packet.type &&
      length == packet.length &&
      0 == memcmp(payload, packet.payload, sizeof(payload));
}

// Implementation of VideoCaptureListener.
VideoCapturerListener::VideoCapturerListener(VideoCapturer* capturer)
    : capturer_(capturer),
      last_capture_state_(CS_STARTING),
      frame_count_(0),
      frame_width_(0),
      frame_height_(0),
      resolution_changed_(false) {
  capturer->SignalStateChange.connect(this,
      &VideoCapturerListener::OnStateChange);
  capturer->AddOrUpdateSink(this, rtc::VideoSinkWants());
}

VideoCapturerListener::~VideoCapturerListener() {
  capturer_->RemoveSink(this);
}

void VideoCapturerListener::OnStateChange(VideoCapturer* capturer,
                                          CaptureState result) {
  last_capture_state_ = result;
}

void VideoCapturerListener::OnFrame(const webrtc::VideoFrame& frame) {
  ++frame_count_;
  if (1 == frame_count_) {
    frame_width_ = frame.width();
    frame_height_ = frame.height();
  } else if (frame_width_ != frame.width() || frame_height_ != frame.height()) {
    resolution_changed_ = true;
  }
}

cricket::StreamParams CreateSimStreamParams(
    const std::string& cname,
    const std::vector<uint32_t>& ssrcs) {
  cricket::StreamParams sp;
  cricket::SsrcGroup sg(cricket::kSimSsrcGroupSemantics, ssrcs);
  sp.ssrcs = ssrcs;
  sp.ssrc_groups.push_back(sg);
  sp.cname = cname;
  return sp;
}

// There should be an rtx_ssrc per ssrc.
cricket::StreamParams CreateSimWithRtxStreamParams(
    const std::string& cname,
    const std::vector<uint32_t>& ssrcs,
    const std::vector<uint32_t>& rtx_ssrcs) {
  cricket::StreamParams sp = CreateSimStreamParams(cname, ssrcs);
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    sp.ssrcs.push_back(rtx_ssrcs[i]);
    std::vector<uint32_t> fid_ssrcs;
    fid_ssrcs.push_back(ssrcs[i]);
    fid_ssrcs.push_back(rtx_ssrcs[i]);
    cricket::SsrcGroup fid_group(cricket::kFidSsrcGroupSemantics, fid_ssrcs);
    sp.ssrc_groups.push_back(fid_group);
  }
  return sp;
}

cricket::StreamParams CreatePrimaryWithFecFrStreamParams(
    const std::string& cname,
    uint32_t primary_ssrc,
    uint32_t flexfec_ssrc) {
  cricket::StreamParams sp;
  cricket::SsrcGroup sg(cricket::kFecFrSsrcGroupSemantics,
                        {primary_ssrc, flexfec_ssrc});
  sp.ssrcs = {primary_ssrc};
  sp.ssrc_groups.push_back(sg);
  sp.cname = cname;
  return sp;
}

}  // namespace cricket
