/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/common_header.h"

namespace webrtc {
namespace rtcp {
constexpr uint8_t ReceiverReport::kPacketType;
// RTCP receiver report (RFC 3550).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=RR=201   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     SSRC of packet sender                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                         report block(s)                       |
//  |                            ....                               |
bool ReceiverReport::Parse(const CommonHeader& packet) {
  RTC_DCHECK_EQ(packet.type(), kPacketType);

  const uint8_t report_blocks_count = packet.count();

  if (packet.payload_size_bytes() <
      kRrBaseLength + report_blocks_count * ReportBlock::kLength) {
    LOG(LS_WARNING) << "Packet is too small to contain all the data.";
    return false;
  }

  sender_ssrc_ = ByteReader<uint32_t>::ReadBigEndian(packet.payload());

  const uint8_t* next_report_block = packet.payload() + kRrBaseLength;

  report_blocks_.resize(report_blocks_count);
  for (ReportBlock& block : report_blocks_) {
    block.Parse(next_report_block, ReportBlock::kLength);
    next_report_block += ReportBlock::kLength;
  }

  RTC_DCHECK_LE(next_report_block - packet.payload(),
                static_cast<ptrdiff_t>(packet.payload_size_bytes()));
  return true;
}

bool ReceiverReport::Create(uint8_t* packet,
                            size_t* index,
                            size_t max_length,
                            RtcpPacket::PacketReadyCallback* callback) const {
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  CreateHeader(report_blocks_.size(), kPacketType, HeaderLength(), packet,
               index);
  ByteWriter<uint32_t>::WriteBigEndian(packet + *index, sender_ssrc_);
  *index += kRrBaseLength;
  for (const ReportBlock& block : report_blocks_) {
    block.Create(packet + *index);
    *index += ReportBlock::kLength;
  }
  return true;
}

bool ReceiverReport::AddReportBlock(const ReportBlock& block) {
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    LOG(LS_WARNING) << "Max report blocks reached.";
    return false;
  }
  report_blocks_.push_back(block);
  return true;
}

}  // namespace rtcp
}  // namespace webrtc
