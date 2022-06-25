/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/ulpfec_header_reader_writer.h"

#include <string.h>

#include <memory>
#include <utility>

#include "api/scoped_refptr.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using Packet = ForwardErrorCorrection::Packet;
using ReceivedFecPacket = ForwardErrorCorrection::ReceivedFecPacket;

constexpr uint32_t kMediaSsrc = 1254983;
constexpr uint16_t kMediaStartSeqNum = 825;
constexpr size_t kMediaPacketLength = 1234;

constexpr size_t kUlpfecHeaderSizeLBitClear = 14;
constexpr size_t kUlpfecHeaderSizeLBitSet = 18;
constexpr size_t kUlpfecPacketMaskOffset = 12;

std::unique_ptr<uint8_t[]> GeneratePacketMask(size_t packet_mask_size,
                                              uint64_t seed) {
  Random random(seed);
  std::unique_ptr<uint8_t[]> packet_mask(new uint8_t[packet_mask_size]);
  for (size_t i = 0; i < packet_mask_size; ++i) {
    packet_mask[i] = random.Rand<uint8_t>();
  }
  return packet_mask;
}

std::unique_ptr<Packet> WriteHeader(const uint8_t* packet_mask,
                                    size_t packet_mask_size) {
  UlpfecHeaderWriter writer;
  std::unique_ptr<Packet> written_packet(new Packet());
  written_packet->data.SetSize(kMediaPacketLength);
  uint8_t* data = written_packet->data.MutableData();
  for (size_t i = 0; i < written_packet->data.size(); ++i) {
    data[i] = i;  // Actual content doesn't matter.
  }
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask,
                           packet_mask_size, written_packet.get());
  return written_packet;
}

std::unique_ptr<ReceivedFecPacket> ReadHeader(const Packet& written_packet) {
  UlpfecHeaderReader reader;
  std::unique_ptr<ReceivedFecPacket> read_packet(new ReceivedFecPacket());
  read_packet->ssrc = kMediaSsrc;
  read_packet->pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet->pkt->data = written_packet.data;
  EXPECT_TRUE(reader.ReadFecHeader(read_packet.get()));
  return read_packet;
}

void VerifyHeaders(size_t expected_fec_header_size,
                   const uint8_t* expected_packet_mask,
                   size_t expected_packet_mask_size,
                   const Packet& written_packet,
                   const ReceivedFecPacket& read_packet) {
  EXPECT_EQ(kMediaSsrc, read_packet.ssrc);
  EXPECT_EQ(expected_fec_header_size, read_packet.fec_header_size);
  EXPECT_EQ(kMediaSsrc, read_packet.protected_ssrc);
  EXPECT_EQ(kMediaStartSeqNum, read_packet.seq_num_base);
  EXPECT_EQ(kUlpfecPacketMaskOffset, read_packet.packet_mask_offset);
  ASSERT_EQ(expected_packet_mask_size, read_packet.packet_mask_size);
  EXPECT_EQ(written_packet.data.size() - expected_fec_header_size,
            read_packet.protection_length);
  EXPECT_EQ(0, memcmp(expected_packet_mask,
                      read_packet.pkt->data.MutableData() +
                          read_packet.packet_mask_offset,
                      read_packet.packet_mask_size));
  // Verify that the call to ReadFecHeader did not tamper with the payload.
  EXPECT_EQ(0, memcmp(written_packet.data.data() + expected_fec_header_size,
                      read_packet.pkt->data.cdata() + expected_fec_header_size,
                      written_packet.data.size() - expected_fec_header_size));
}

}  // namespace

TEST(UlpfecHeaderReaderTest, ReadsSmallHeader) {
  const uint8_t packet[] = {
      0x00, 0x12, 0xab, 0xcd,  // L bit clear, "random" payload type and SN base
      0x12, 0x34, 0x56, 0x78,  // "random" TS recovery
      0xab, 0xcd, 0x11, 0x22,  // "random" length recovery and protection length
      0x33, 0x44,              // "random" packet mask
      0x00, 0x00, 0x00, 0x00   // payload
  };
  const size_t packet_length = sizeof(packet);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(packet, packet_length);

  UlpfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  EXPECT_EQ(14U, read_packet.fec_header_size);
  EXPECT_EQ(0xabcdU, read_packet.seq_num_base);
  EXPECT_EQ(12U, read_packet.packet_mask_offset);
  EXPECT_EQ(2U, read_packet.packet_mask_size);
  EXPECT_EQ(0x1122U, read_packet.protection_length);
}

TEST(UlpfecHeaderReaderTest, ReadsLargeHeader) {
  const uint8_t packet[] = {
      0x40, 0x12, 0xab, 0xcd,  // L bit set, "random" payload type and SN base
      0x12, 0x34, 0x56, 0x78,  // "random" TS recovery
      0xab, 0xcd, 0x11, 0x22,  // "random" length recovery and protection length
      0x33, 0x44, 0x55, 0x66,  // "random" packet mask
      0x77, 0x88,              //
      0x00, 0x00, 0x00, 0x00   // payload
  };
  const size_t packet_length = sizeof(packet);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(packet, packet_length);

  UlpfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  EXPECT_EQ(18U, read_packet.fec_header_size);
  EXPECT_EQ(0xabcdU, read_packet.seq_num_base);
  EXPECT_EQ(12U, read_packet.packet_mask_offset);
  EXPECT_EQ(6U, read_packet.packet_mask_size);
  EXPECT_EQ(0x1122U, read_packet.protection_length);
}

TEST(UlpfecHeaderWriterTest, FinalizesSmallHeader) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  uint8_t* data = written_packet.data.MutableData();
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    data[i] = i;
  }

  UlpfecHeaderWriter writer;
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask.get(),
                           packet_mask_size, &written_packet);

  const uint8_t* packet = written_packet.data.cdata();
  EXPECT_EQ(0x00, packet[0] & 0x80);  // E bit.
  EXPECT_EQ(0x00, packet[0] & 0x40);  // L bit.
  EXPECT_EQ(kMediaStartSeqNum, ByteReader<uint16_t>::ReadBigEndian(packet + 2));
  EXPECT_EQ(
      static_cast<uint16_t>(kMediaPacketLength - kUlpfecHeaderSizeLBitClear),
      ByteReader<uint16_t>::ReadBigEndian(packet + 10));
  EXPECT_EQ(0, memcmp(packet + kUlpfecPacketMaskOffset, packet_mask.get(),
                      packet_mask_size));
}

TEST(UlpfecHeaderWriterTest, FinalizesLargeHeader) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  uint8_t* data = written_packet.data.MutableData();
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    data[i] = i;
  }

  UlpfecHeaderWriter writer;
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask.get(),
                           packet_mask_size, &written_packet);

  const uint8_t* packet = written_packet.data.cdata();
  EXPECT_EQ(0x00, packet[0] & 0x80);  // E bit.
  EXPECT_EQ(0x40, packet[0] & 0x40);  // L bit.
  EXPECT_EQ(kMediaStartSeqNum, ByteReader<uint16_t>::ReadBigEndian(packet + 2));
  EXPECT_EQ(
      static_cast<uint16_t>(kMediaPacketLength - kUlpfecHeaderSizeLBitSet),
      ByteReader<uint16_t>::ReadBigEndian(packet + 10));
  EXPECT_EQ(0, memcmp(packet + kUlpfecPacketMaskOffset, packet_mask.get(),
                      packet_mask_size));
}

TEST(UlpfecHeaderWriterTest, CalculateSmallHeaderSize) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);

  UlpfecHeaderWriter writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kUlpfecPacketMaskSizeLBitClear, min_packet_mask_size);
  EXPECT_EQ(kUlpfecHeaderSizeLBitClear,
            writer.FecHeaderSize(min_packet_mask_size));
}

TEST(UlpfecHeaderWriterTest, CalculateLargeHeaderSize) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);

  UlpfecHeaderWriter writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kUlpfecPacketMaskSizeLBitSet, min_packet_mask_size);
  EXPECT_EQ(kUlpfecHeaderSizeLBitSet,
            writer.FecHeaderSize(min_packet_mask_size));
}

TEST(UlpfecHeaderReaderWriterTest, WriteAndReadSmallHeader) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyHeaders(kUlpfecHeaderSizeLBitClear, packet_mask.get(), packet_mask_size,
                *written_packet, *read_packet);
}

TEST(UlpfecHeaderReaderWriterTest, WriteAndReadLargeHeader) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyHeaders(kUlpfecHeaderSizeLBitSet, packet_mask.get(), packet_mask_size,
                *written_packet, *read_packet);
}

}  // namespace webrtc
