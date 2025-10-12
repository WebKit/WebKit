/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/fec_test_helper.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/random.h"

namespace webrtc {
namespace test {
namespace fec {

namespace {

constexpr uint8_t kFecPayloadType = 96;
constexpr uint8_t kRedPayloadType = 97;
constexpr uint8_t kVp8PayloadType = 120;

constexpr int kPacketTimestampIncrement = 3000;
}  // namespace

MediaPacketGenerator::MediaPacketGenerator(uint32_t min_packet_size,
                                           uint32_t max_packet_size,
                                           uint32_t ssrc,
                                           Random* random)
    : min_packet_size_(min_packet_size),
      max_packet_size_(max_packet_size),
      ssrc_(ssrc),
      random_(random) {}

MediaPacketGenerator::~MediaPacketGenerator() = default;

ForwardErrorCorrection::PacketList MediaPacketGenerator::ConstructMediaPackets(
    int num_media_packets,
    uint16_t start_seq_num) {
  RTC_DCHECK_GT(num_media_packets, 0);
  uint16_t seq_num = start_seq_num;
  int time_stamp = random_->Rand<int>();

  ForwardErrorCorrection::PacketList media_packets;

  for (int i = 0; i < num_media_packets; ++i) {
    std::unique_ptr<ForwardErrorCorrection::Packet> media_packet(
        new ForwardErrorCorrection::Packet());
    media_packet->data.SetSize(
        random_->Rand(min_packet_size_, max_packet_size_));

    uint8_t* data = media_packet->data.MutableData();
    // Generate random values for the first 2 bytes
    data[0] = random_->Rand<uint8_t>();
    data[1] = random_->Rand<uint8_t>();

    // The first two bits are assumed to be 10 by the FEC encoder.
    // In fact the FEC decoder will set the two first bits to 10 regardless of
    // what they actually were. Set the first two bits to 10 so that a memcmp
    // can be performed for the whole restored packet.
    data[0] |= 0x80;
    data[0] &= 0xbf;

    // FEC is applied to a whole frame.
    // A frame is signaled by multiple packets without the marker bit set
    // followed by the last packet of the frame for which the marker bit is set.
    // Only push one (fake) frame to the FEC.
    data[1] &= 0x7f;

    ByteWriter<uint16_t>::WriteBigEndian(&data[2], seq_num);
    ByteWriter<uint32_t>::WriteBigEndian(&data[4], time_stamp);
    ByteWriter<uint32_t>::WriteBigEndian(&data[8], ssrc_);

    // Generate random values for payload.
    for (size_t j = 12; j < media_packet->data.size(); ++j)
      data[j] = random_->Rand<uint8_t>();
    seq_num++;
    media_packets.push_back(std::move(media_packet));
  }
  // Last packet, set marker bit.
  ForwardErrorCorrection::Packet* media_packet = media_packets.back().get();
  RTC_DCHECK(media_packet);
  media_packet->data.MutableData()[1] |= 0x80;

  next_seq_num_ = seq_num;

  return media_packets;
}

ForwardErrorCorrection::PacketList MediaPacketGenerator::ConstructMediaPackets(
    int num_media_packets) {
  return ConstructMediaPackets(num_media_packets, random_->Rand<uint16_t>());
}

uint16_t MediaPacketGenerator::GetNextSeqNum() {
  return next_seq_num_;
}

AugmentedPacketGenerator::AugmentedPacketGenerator(uint32_t ssrc)
    : num_packets_(0), ssrc_(ssrc), seq_num_(0), timestamp_(0) {}

void AugmentedPacketGenerator::NewFrame(size_t num_packets) {
  num_packets_ = num_packets;
  timestamp_ += kPacketTimestampIncrement;
}

uint16_t AugmentedPacketGenerator::NextPacketSeqNum() {
  return ++seq_num_;
}

void AugmentedPacketGenerator::NextPacket(size_t offset,
                                          size_t length,
                                          RtpPacket& packet) {
  // Write Rtp Header
  packet.SetMarker(num_packets_ == 1);
  packet.SetPayloadType(kVp8PayloadType);
  packet.SetSequenceNumber(seq_num_);
  packet.SetTimestamp(timestamp_);
  packet.SetSsrc(ssrc_);

  // Generate RTP payload.
  uint8_t* data = packet.AllocatePayload(length);
  for (size_t i = 0; i < length; ++i)
    data[i] = offset + i;

  ++seq_num_;
  --num_packets_;
}

FlexfecPacketGenerator::FlexfecPacketGenerator(uint32_t media_ssrc,
                                               uint32_t flexfec_ssrc)
    : AugmentedPacketGenerator(media_ssrc),
      flexfec_ssrc_(flexfec_ssrc),
      flexfec_seq_num_(0),
      flexfec_timestamp_(0) {}

RtpPacketReceived FlexfecPacketGenerator::BuildFlexfecPacket(
    const ForwardErrorCorrection::Packet& packet) {
  RtpPacketReceived flexfec_packet;

  flexfec_packet.SetSequenceNumber(flexfec_seq_num_);
  ++flexfec_seq_num_;
  flexfec_packet.SetTimestamp(flexfec_timestamp_);
  flexfec_timestamp_ += kPacketTimestampIncrement;
  flexfec_packet.SetSsrc(flexfec_ssrc_);
  flexfec_packet.SetPayload(packet.data);

  return flexfec_packet;
}

UlpfecPacketGenerator::UlpfecPacketGenerator(uint32_t ssrc)
    : AugmentedPacketGenerator(ssrc) {}

RtpPacketReceived UlpfecPacketGenerator::BuildMediaRedPacket(
    const RtpPacket& packet,
    bool is_recovered) {
  RtpPacketReceived red_packet;
  // Append header.
  red_packet.CopyHeaderFrom(packet);
  // Find payload type and add it as RED header.
  uint8_t* rtp_payload = red_packet.SetPayloadSize(1 + packet.payload_size());
  rtp_payload[0] = packet.PayloadType();
  // Append rest of payload/padding.
  std::memcpy(rtp_payload + 1, packet.payload().data(),
              packet.payload().size());
  red_packet.SetPadding(packet.padding_size());

  red_packet.SetPayloadType(kRedPayloadType);
  red_packet.set_recovered(is_recovered);

  return red_packet;
}

RtpPacketReceived UlpfecPacketGenerator::BuildUlpfecRedPacket(
    const ForwardErrorCorrection::Packet& packet) {
  // Create a fake media packet to get a correct header. 1 byte RED header.
  ++num_packets_;
  RtpPacketReceived red_packet =
      NextPacket<RtpPacketReceived>(0, packet.data.size() + 1);

  red_packet.SetMarker(false);
  uint8_t* rtp_payload = red_packet.AllocatePayload(packet.data.size() + 1);
  rtp_payload[0] = kFecPayloadType;
  red_packet.SetPayloadType(kRedPayloadType);
  memcpy(rtp_payload + 1, packet.data.cdata(), packet.data.size());
  red_packet.set_recovered(false);

  return red_packet;
}

}  // namespace fec
}  // namespace test
}  // namespace webrtc
