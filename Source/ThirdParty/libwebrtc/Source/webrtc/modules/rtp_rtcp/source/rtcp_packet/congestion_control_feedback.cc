/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rtcp {

/*
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |V=2|P| FMT=11  |   PT = 205    |          length               |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                 SSRC of RTCP packet sender                    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                   SSRC of 1st RTP Stream                      |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |          begin_seq            |          num_reports          |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |R|ECN|  Arrival time offset    | ...                           .
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     .                                                               .
     .                                                               .
     .                                                               .
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                   SSRC of nth RTP Stream                      |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |          begin_seq            |          num_reports          |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |R|ECN|  Arrival time offset    | ...                           |
     .                                                               .
     .                                                               .
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                 Report Timestamp (32 bits)                    |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

namespace {

constexpr size_t kSenderSsrcLength = 4;
constexpr size_t kHeaderPerMediaSssrcLength = 8;
constexpr size_t kTimestampLength = 4;

// RFC-3168, Section 5
constexpr uint16_t kEcnEct1 = 0x01;
constexpr uint16_t kEcnEct0 = 0x02;
constexpr uint16_t kEcnCe = 0x03;

// Arrival time offset (ATO, 13 bits):
// The arrival time of the RTP packet at the receiver, as an offset before the
// time represented by the Report Timestamp (RTS) field of this RTCP congestion
// control feedback report. The ATO field is in units of 1/1024 seconds (this
// unit is chosen to give exact offsets from the RTS field) so, for example, an
// ATO value of 512 indicates that the corresponding RTP packet arrived exactly
// half a second before the time instant represented by the RTS field. If the
// measured value is greater than 8189/1024 seconds (the value that would be
// coded as 0x1FFD), the value 0x1FFE MUST be reported to indicate an over-range
// measurement. If the measurement is unavailable or if the arrival time of the
// RTP packet is after the time represented by the RTS field, then an ATO value
// of 0x1FFF MUST be reported for the packet.
uint16_t To13bitAto(TimeDelta arrival_time_offset) {
  if (arrival_time_offset < TimeDelta::Zero()) {
    return 0x1FFF;
  }
  return std::min(
      static_cast<int64_t>(1024 * arrival_time_offset.seconds<float>()),
      int64_t{0x1FFE});
}

TimeDelta AtoToTimeDelta(uint16_t receive_info) {
  // receive_info
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |R|ECN|  Arrival time offset    |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  const uint16_t ato = receive_info & 0x1FFF;
  if (ato == 0x1FFE) {
    return TimeDelta::PlusInfinity();
  }
  if (ato == 0x1FFF) {
    return TimeDelta::MinusInfinity();
  }
  return TimeDelta::Seconds(ato) / 1024;
}

uint16_t To2BitEcn(EcnMarking ecn_marking) {
  switch (ecn_marking) {
    case EcnMarking::kNotEct:
      return 0;
    case EcnMarking::kEct1:
      return kEcnEct1 << 13;
    case EcnMarking::kEct0:
      return kEcnEct0 << 13;
    case EcnMarking::kCe:
      return kEcnCe << 13;
  }
}

EcnMarking ToEcnMarking(uint16_t receive_info) {
  const uint16_t ecn = (receive_info >> 13) & 0b11;
  if (ecn == kEcnEct1) {
    return EcnMarking::kEct1;
  }
  if (ecn == kEcnEct0) {
    return EcnMarking::kEct0;
  }
  if (ecn == kEcnCe) {
    return EcnMarking::kCe;
  }
  return EcnMarking::kNotEct;
}

}  // namespace

CongestionControlFeedback ::CongestionControlFeedback(
    std::vector<PacketInfo> packets,
    uint32_t compact_ntp_timestamp)
    : packets_(std::move(packets)),
      report_timestamp_compact_ntp_(compact_ntp_timestamp) {}

bool CongestionControlFeedback::Create(uint8_t* buffer,
                                       size_t* position,
                                       size_t max_length,
                                       PacketReadyCallback callback) const {
  // Ensure there is enough room for this packet.
  while (*position + BlockLength() > max_length) {
    if (!OnBufferFull(buffer, position, callback))
      return false;
  }
  const size_t position_end = *position + BlockLength();

  //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //    |V=2|P| FMT=11  |   PT = 205    |          length               |
  //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //    |                 SSRC of RTCP packet sender                    |
  //    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  CreateHeader(kFeedbackMessageType, kPacketType, HeaderLength(), buffer,
               position);
  ByteWriter<uint32_t>::WriteBigEndian(&buffer[*position], sender_ssrc());
  *position += 4;

  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                   SSRC of nth RTP Stream                      |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |          begin_seq            |          num_reports          |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |R|ECN|  Arrival time offset    | ...                           .
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   .                                                               .
  auto write_report_for_ssrc = [&](ArrayView<const PacketInfo> packets) {
    // SSRC of nth RTP stream.
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[*position], packets[0].ssrc);
    *position += 4;

    // begin_seq
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[*position],
                                         packets[0].sequence_number);
    *position += 2;
    // num_reports
    uint16_t num_reports = packets.size();
    RTC_DCHECK_EQ(static_cast<uint16_t>(

                      packets[packets.size() - 1].sequence_number -
                      packets[0].sequence_number + 1),
                  packets.size())
        << "Expected continous rtp sequence numbers.";

    // Each report block MUST NOT include more than 16384 packet metric
    // blocks (i.e., it MUST NOT report on more than one quarter of the
    // sequence number space in a single report).
    if (num_reports > 16384) {
      RTC_DCHECK_NOTREACHED() << "Unexpected number of reports:" << num_reports;
      return;
    }
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[*position], num_reports);
    *position += 2;

    for (const PacketInfo& packet : packets) {
      bool received = packet.arrival_time_offset.IsFinite();
      uint16_t packet_info = 0;
      if (received) {
        packet_info = 0x8000 | To2BitEcn(packet.ecn) |
                      To13bitAto(packet.arrival_time_offset);
      }
      ByteWriter<uint16_t>::WriteBigEndian(&buffer[*position], packet_info);
      *position += 2;
    }
    // 32bit align per SSRC block.
    if (num_reports % 2 != 0) {
      ByteWriter<uint16_t>::WriteBigEndian(&buffer[*position], 0);
      *position += 2;
    }
  };

  ArrayView<const PacketInfo> remaining(packets_);
  while (!remaining.empty()) {
    int number_of_packets_for_ssrc = 0;
    uint32_t ssrc = remaining[0].ssrc;
    for (const PacketInfo& packet_info : remaining) {
      if (packet_info.ssrc != ssrc) {
        break;
      }
      ++number_of_packets_for_ssrc;
    }
    write_report_for_ssrc(remaining.subspan(0, number_of_packets_for_ssrc));
    remaining = remaining.subspan(number_of_packets_for_ssrc);
  }

  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                 Report Timestamp (32 bits)                    |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  ByteWriter<uint32_t>::WriteBigEndian(&buffer[*position],
                                       report_timestamp_compact_ntp_);
  *position += 4;

  RTC_DCHECK_EQ(*position, position_end);
  return true;
}

size_t CongestionControlFeedback::BlockLength() const {
  // Total size of this packet
  size_t total_size = kSenderSsrcLength + kHeaderLength + kTimestampLength;
  if (packets_.empty()) {
    return total_size;
  }

  auto increase_size_per_ssrc = [](int number_of_packets_for_ssrc) {
    // Each packet report needs two bytes.
    size_t packet_block_size = number_of_packets_for_ssrc * 2;
    // 32 bit aligned.
    return kHeaderPerMediaSssrcLength + packet_block_size +
           ((number_of_packets_for_ssrc % 2) != 0 ? 2 : 0);
  };

  uint32_t ssrc = packets_.front().ssrc;
  uint16_t first_sequence_number = packets_.front().sequence_number;
  for (size_t i = 0; i < packets_.size(); ++i) {
    if (packets_[i].ssrc != ssrc) {
      uint16_t number_of_packets =
          packets_[i - 1].sequence_number - first_sequence_number + 1;
      total_size += increase_size_per_ssrc(number_of_packets);
      ssrc = packets_[i].ssrc;
      first_sequence_number = packets_[i].sequence_number;
    }
  }
  uint16_t number_of_packets =
      packets_.back().sequence_number - first_sequence_number + 1;
  total_size += increase_size_per_ssrc(number_of_packets);

  return total_size;
}

bool CongestionControlFeedback::Parse(const rtcp::CommonHeader& packet) {
  const uint8_t* payload = packet.payload();
  const uint8_t* payload_end = packet.payload() + packet.payload_size_bytes();

  if (packet.payload_size_bytes() % 4 != 0 ||
      packet.payload_size_bytes() < kSenderSsrcLength + kTimestampLength) {
    return false;
  }

  SetSenderSsrc(ByteReader<uint32_t>::ReadBigEndian(payload));
  payload += 4;

  report_timestamp_compact_ntp_ =
      ByteReader<uint32_t>::ReadBigEndian(payload_end - 4);
  payload_end -= 4;

  while (payload + kHeaderPerMediaSssrcLength < payload_end) {
    uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(payload);
    payload += 4;

    uint16_t base_seqno = ByteReader<uint16_t>::ReadBigEndian(payload);
    payload += 2;
    uint16_t num_reports = ByteReader<uint16_t>::ReadBigEndian(payload);
    payload += 2;

    constexpr size_t kPerPacketLength = 2;
    if (payload + kPerPacketLength * num_reports > payload_end) {
      return false;
    }

    for (int i = 0; i < num_reports; ++i) {
      uint16_t packet_info = ByteReader<uint16_t>::ReadBigEndian(payload);
      payload += 2;

      uint16_t seq_no = base_seqno + i;
      bool received = (packet_info & 0x8000);
      packets_.push_back(
          {.ssrc = ssrc,
           .sequence_number = seq_no,
           .arrival_time_offset = received ? AtoToTimeDelta(packet_info)
                                           : TimeDelta::MinusInfinity(),
           .ecn = ToEcnMarking(packet_info)});
    }
    if (num_reports % 2) {
      // 2 bytes padding
      payload += 2;
    }
  }
  return payload == payload_end;
}
}  // namespace rtcp
}  // namespace webrtc
