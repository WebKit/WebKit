/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/testutils.h"

#include <math.h>
#include <algorithm>
#include <memory>

#include "webrtc/base/bytebuffer.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/testutils.h"
#include "webrtc/media/base/rtpdump.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/video_frame.h"

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

/////////////////////////////////////////////////////////////////////////
// Implementation of class RtpTestUtility
/////////////////////////////////////////////////////////////////////////
const RawRtpPacket RtpTestUtility::kTestRawRtpPackets[] = {
    {0x80, 0, 0, 0,  RtpTestUtility::kDefaultSsrc, "RTP frame 0"},
    {0x80, 0, 1, 30, RtpTestUtility::kDefaultSsrc, "RTP frame 1"},
    {0x80, 0, 2, 30, RtpTestUtility::kDefaultSsrc, "RTP frame 1"},
    {0x80, 0, 3, 60, RtpTestUtility::kDefaultSsrc, "RTP frame 2"}
};
const RawRtcpPacket RtpTestUtility::kTestRawRtcpPackets[] = {
    // The Version is 2, the Length is 2, and the payload has 8 bytes.
    {0x80, 0, 2, "RTCP0000"},
    {0x80, 0, 2, "RTCP0001"},
    {0x80, 0, 2, "RTCP0002"},
    {0x80, 0, 2, "RTCP0003"},
};

size_t RtpTestUtility::GetTestPacketCount() {
  return std::min(arraysize(kTestRawRtpPackets),
                  arraysize(kTestRawRtcpPackets));
}

bool RtpTestUtility::WriteTestPackets(size_t count,
                                      bool rtcp,
                                      uint32_t rtp_ssrc,
                                      RtpDumpWriter* writer) {
  if (!writer || count > GetTestPacketCount()) return false;

  bool result = true;
  uint32_t elapsed_time_ms = 0;
  for (size_t i = 0; i < count && result; ++i) {
    rtc::ByteBufferWriter buf;
    if (rtcp) {
      kTestRawRtcpPackets[i].WriteToByteBuffer(&buf);
    } else {
      kTestRawRtpPackets[i].WriteToByteBuffer(rtp_ssrc, &buf);
    }

    RtpDumpPacket dump_packet(buf.Data(), buf.Length(), elapsed_time_ms, rtcp);
    elapsed_time_ms += kElapsedTimeInterval;
    result &= (rtc::SR_SUCCESS == writer->WritePacket(dump_packet));
  }
  return result;
}

bool RtpTestUtility::VerifyTestPacketsFromStream(size_t count,
                                                 rtc::StreamInterface* stream,
                                                 uint32_t ssrc) {
  if (!stream) return false;

  uint32_t prev_elapsed_time = 0;
  bool result = true;
  stream->Rewind();
  RtpDumpLoopReader reader(stream);
  for (size_t i = 0; i < count && result; ++i) {
    // Which loop and which index in the loop are we reading now.
    size_t loop = i / GetTestPacketCount();
    size_t index = i % GetTestPacketCount();

    RtpDumpPacket packet;
    result &= (rtc::SR_SUCCESS == reader.ReadPacket(&packet));
    // Check the elapsed time of the dump packet.
    result &= (packet.elapsed_time >= prev_elapsed_time);
    prev_elapsed_time = packet.elapsed_time;

    // Check the RTP or RTCP packet.
    rtc::ByteBufferReader buf(reinterpret_cast<const char*>(&packet.data[0]),
                              packet.data.size());
    if (packet.is_rtcp()) {
      // RTCP packet.
      RawRtcpPacket rtcp_packet;
      result &= rtcp_packet.ReadFromByteBuffer(&buf);
      result &= rtcp_packet.EqualsTo(kTestRawRtcpPackets[index]);
    } else {
      // RTP packet.
      RawRtpPacket rtp_packet;
      result &= rtp_packet.ReadFromByteBuffer(&buf);
      result &= rtp_packet.SameExceptSeqNumTimestampSsrc(
          kTestRawRtpPackets[index],
          static_cast<uint16_t>(kTestRawRtpPackets[index].sequence_number +
                                loop * GetTestPacketCount()),
          static_cast<uint32_t>(kTestRawRtpPackets[index].timestamp +
                                loop * kRtpTimestampIncrease),
          ssrc);
    }
  }

  stream->Rewind();
  return result;
}

bool RtpTestUtility::VerifyPacket(const RtpDumpPacket* dump,
                                  const RawRtpPacket* raw,
                                  bool header_only) {
  if (!dump || !raw) return false;

  rtc::ByteBufferWriter buf;
  raw->WriteToByteBuffer(RtpTestUtility::kDefaultSsrc, &buf);

  if (header_only) {
    size_t header_len = 0;
    dump->GetRtpHeaderLen(&header_len);
    return header_len == dump->data.size() &&
        buf.Length() > dump->data.size() &&
        0 == memcmp(buf.Data(), &dump->data[0], dump->data.size());
  } else {
    return buf.Length() == dump->data.size() &&
        0 == memcmp(buf.Data(), &dump->data[0], dump->data.size());
  }
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

}  // namespace cricket
