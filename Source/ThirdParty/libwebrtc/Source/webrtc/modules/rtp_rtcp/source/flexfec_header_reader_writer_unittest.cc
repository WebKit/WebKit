/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/flexfec_header_reader_writer.h"

#include <string.h>

#include <memory>
#include <utility>

#include "api/scoped_refptr.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using Packet = ForwardErrorCorrection::Packet;
using ReceivedFecPacket = ForwardErrorCorrection::ReceivedFecPacket;

// General. Assume single-stream protection.
constexpr uint32_t kMediaSsrc = 1254983;
constexpr uint16_t kMediaStartSeqNum = 825;
constexpr size_t kMediaPacketLength = 1234;
constexpr uint32_t kFlexfecSsrc = 52142;

constexpr size_t kFlexfecHeaderSizes[] = {20, 24, 32};
constexpr size_t kFlexfecPacketMaskOffset = 18;
constexpr size_t kFlexfecPacketMaskSizes[] = {2, 6, 14};
constexpr size_t kFlexfecMaxPacketSize = kFlexfecPacketMaskSizes[2];

// Reader tests.
constexpr uint8_t kNoRBit = 0 << 7;
constexpr uint8_t kNoFBit = 0 << 6;
constexpr uint8_t kPtRecovery = 123;
constexpr uint8_t kLengthRecov[] = {0xab, 0xcd};
constexpr uint8_t kTsRecovery[] = {0x01, 0x23, 0x45, 0x67};
constexpr uint8_t kSsrcCount = 1;
constexpr uint8_t kReservedBits = 0x00;
constexpr uint8_t kProtSsrc[] = {0x11, 0x22, 0x33, 0x44};
constexpr uint8_t kSnBase[] = {0xaa, 0xbb};
constexpr uint8_t kPayloadBits = 0x00;

std::unique_ptr<uint8_t[]> GeneratePacketMask(size_t packet_mask_size,
                                              uint64_t seed) {
  Random random(seed);
  std::unique_ptr<uint8_t[]> packet_mask(new uint8_t[kFlexfecMaxPacketSize]);
  memset(packet_mask.get(), 0, kFlexfecMaxPacketSize);
  for (size_t i = 0; i < packet_mask_size; ++i) {
    packet_mask[i] = random.Rand<uint8_t>();
  }
  return packet_mask;
}

void ClearBit(size_t index, uint8_t* packet_mask) {
  packet_mask[index / 8] &= ~(1 << (7 - index % 8));
}

void SetBit(size_t index, uint8_t* packet_mask) {
  packet_mask[index / 8] |= (1 << (7 - index % 8));
}

rtc::scoped_refptr<Packet> WriteHeader(const uint8_t* packet_mask,
                                       size_t packet_mask_size) {
  FlexfecHeaderWriter writer;
  rtc::scoped_refptr<Packet> written_packet(new Packet());
  written_packet->data.SetSize(kMediaPacketLength);
  for (size_t i = 0; i < written_packet->data.size(); ++i) {
    written_packet->data[i] = i;  // Actual content doesn't matter.
  }
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, packet_mask,
                           packet_mask_size, written_packet.get());
  return written_packet;
}

std::unique_ptr<ReceivedFecPacket> ReadHeader(const Packet& written_packet) {
  FlexfecHeaderReader reader;
  std::unique_ptr<ReceivedFecPacket> read_packet(new ReceivedFecPacket());
  read_packet->ssrc = kFlexfecSsrc;
  read_packet->pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet->pkt->data = written_packet.data;
  EXPECT_TRUE(reader.ReadFecHeader(read_packet.get()));
  return read_packet;
}

void VerifyReadHeaders(size_t expected_fec_header_size,
                       const uint8_t* expected_packet_mask,
                       size_t expected_packet_mask_size,
                       const ReceivedFecPacket& read_packet) {
  EXPECT_EQ(expected_fec_header_size, read_packet.fec_header_size);
  EXPECT_EQ(ByteReader<uint32_t>::ReadBigEndian(kProtSsrc),
            read_packet.protected_ssrc);
  EXPECT_EQ(ByteReader<uint16_t>::ReadBigEndian(kSnBase),
            read_packet.seq_num_base);
  const size_t packet_mask_offset = read_packet.packet_mask_offset;
  EXPECT_EQ(kFlexfecPacketMaskOffset, packet_mask_offset);
  EXPECT_EQ(expected_packet_mask_size, read_packet.packet_mask_size);
  EXPECT_EQ(read_packet.pkt->data.size() - expected_fec_header_size,
            read_packet.protection_length);
  // Ensure that the K-bits are removed and the packet mask has been packed.
  EXPECT_THAT(
      ::testing::make_tuple(read_packet.pkt->data.cdata() + packet_mask_offset,
                            read_packet.packet_mask_size),
      ::testing::ElementsAreArray(expected_packet_mask,
                                  expected_packet_mask_size));
}

void VerifyFinalizedHeaders(const uint8_t* expected_packet_mask,
                            size_t expected_packet_mask_size,
                            const Packet& written_packet) {
  const uint8_t* packet = written_packet.data.cdata();
  EXPECT_EQ(0x00, packet[0] & 0x80);  // F bit clear.
  EXPECT_EQ(0x00, packet[0] & 0x40);  // R bit clear.
  EXPECT_EQ(0x01, packet[8]);         // SSRCCount = 1.
  EXPECT_EQ(kMediaSsrc, ByteReader<uint32_t>::ReadBigEndian(packet + 12));
  EXPECT_EQ(kMediaStartSeqNum,
            ByteReader<uint16_t>::ReadBigEndian(packet + 16));
  EXPECT_THAT(::testing::make_tuple(packet + kFlexfecPacketMaskOffset,
                                    expected_packet_mask_size),
              ::testing::ElementsAreArray(expected_packet_mask,
                                          expected_packet_mask_size));
}

void VerifyWrittenAndReadHeaders(size_t expected_fec_header_size,
                                 const uint8_t* expected_packet_mask,
                                 size_t expected_packet_mask_size,
                                 const Packet& written_packet,
                                 const ReceivedFecPacket& read_packet) {
  EXPECT_EQ(kFlexfecSsrc, read_packet.ssrc);
  EXPECT_EQ(expected_fec_header_size, read_packet.fec_header_size);
  EXPECT_EQ(kMediaSsrc, read_packet.protected_ssrc);
  EXPECT_EQ(kMediaStartSeqNum, read_packet.seq_num_base);
  EXPECT_EQ(kFlexfecPacketMaskOffset, read_packet.packet_mask_offset);
  ASSERT_EQ(expected_packet_mask_size, read_packet.packet_mask_size);
  EXPECT_EQ(written_packet.data.size() - expected_fec_header_size,
            read_packet.protection_length);
  // Verify that the call to ReadFecHeader did normalize the packet masks.
  EXPECT_THAT(::testing::make_tuple(
                  read_packet.pkt->data.cdata() + kFlexfecPacketMaskOffset,
                  read_packet.packet_mask_size),
              ::testing::ElementsAreArray(expected_packet_mask,
                                          expected_packet_mask_size));
  // Verify that the call to ReadFecHeader did not tamper with the payload.
  EXPECT_THAT(::testing::make_tuple(
                  read_packet.pkt->data.cdata() + read_packet.fec_header_size,
                  read_packet.pkt->data.size() - read_packet.fec_header_size),
              ::testing::ElementsAreArray(
                  written_packet.data.cdata() + expected_fec_header_size,
                  written_packet.data.size() - expected_fec_header_size));
}

}  // namespace

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithKBit0Set) {
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr size_t kExpectedPacketMaskSize = 2;
  constexpr size_t kExpectedFecHeaderSize = 20;
  // clang-format off
  constexpr uint8_t kFlexfecPktMask[] = {kKBit0 | 0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMask[] =        {0x11, 0x02};
  // clang-format on
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,    kLengthRecov[0],    kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSsrcCount,        kReservedBits,  kReservedBits,      kReservedBits,
      kProtSsrc[0],      kProtSsrc[1],   kProtSsrc[2],       kProtSsrc[3],
      kSnBase[0],        kSnBase[1],     kFlexfecPktMask[0], kFlexfecPktMask[1],
      kPayloadBits,      kPayloadBits,   kPayloadBits,       kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kUlpfecPacketMask,
                    kExpectedPacketMaskSize, read_packet);
}

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithKBit1Set) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 1 << 7;
  constexpr size_t kExpectedPacketMaskSize = 6;
  constexpr size_t kExpectedFecHeaderSize = 24;
  // clang-format off
  constexpr uint8_t kFlxfecPktMsk[] = {kKBit0 | 0x48, 0x81,
                                       kKBit1 | 0x02, 0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask[] =      {0x91, 0x02,
                                                0x08, 0x44, 0x00, 0x84};
  // clang-format on
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,      kLengthRecov[0],  kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1],   kTsRecovery[2],   kTsRecovery[3],
      kSsrcCount,        kReservedBits,    kReservedBits,    kReservedBits,
      kProtSsrc[0],      kProtSsrc[1],     kProtSsrc[2],     kProtSsrc[3],
      kSnBase[0],        kSnBase[1],       kFlxfecPktMsk[0], kFlxfecPktMsk[1],
      kFlxfecPktMsk[2],  kFlxfecPktMsk[3], kFlxfecPktMsk[4], kFlxfecPktMsk[5],
      kPayloadBits,      kPayloadBits,     kPayloadBits,     kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kUlpfecPacketMask,
                    kExpectedPacketMaskSize, read_packet);
}

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithKBit2Set) {
  constexpr uint8_t kKBit0 = 0 << 7;
  constexpr uint8_t kKBit1 = 0 << 7;
  constexpr uint8_t kKBit2 = 1 << 7;
  constexpr size_t kExpectedPacketMaskSize = 14;
  constexpr size_t kExpectedFecHeaderSize = 32;
  // clang-format off
  constexpr uint8_t kFlxfcPktMsk[] = {kKBit0 | 0x48, 0x81,
                                      kKBit1 | 0x02, 0x11, 0x00, 0x21,
                                      kKBit2 | 0x01, 0x11, 0x11, 0x11,
                                               0x11, 0x11, 0x11, 0x11};
  constexpr uint8_t kUlpfecPacketMask[] =     {0x91, 0x02,
                                               0x08, 0x44, 0x00, 0x84,
                                               0x08, 0x88, 0x88, 0x88,
                                               0x88, 0x88, 0x88, 0x88};
  // clang-format on
  constexpr uint8_t kPacketData[] = {
      kNoRBit | kNoFBit, kPtRecovery,      kLengthRecov[0],  kLengthRecov[1],
      kTsRecovery[0],    kTsRecovery[1],   kTsRecovery[2],   kTsRecovery[3],
      kSsrcCount,        kReservedBits,    kReservedBits,    kReservedBits,
      kProtSsrc[0],      kProtSsrc[1],     kProtSsrc[2],     kProtSsrc[3],
      kSnBase[0],        kSnBase[1],       kFlxfcPktMsk[0],  kFlxfcPktMsk[1],
      kFlxfcPktMsk[2],   kFlxfcPktMsk[3],  kFlxfcPktMsk[4],  kFlxfcPktMsk[5],
      kFlxfcPktMsk[6],   kFlxfcPktMsk[7],  kFlxfcPktMsk[8],  kFlxfcPktMsk[9],
      kFlxfcPktMsk[10],  kFlxfcPktMsk[11], kFlxfcPktMsk[12], kFlxfcPktMsk[13],
      kPayloadBits,      kPayloadBits,     kPayloadBits,     kPayloadBits};
  const size_t packet_length = sizeof(kPacketData);
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::scoped_refptr<Packet>(new Packet());
  read_packet.pkt->data.SetData(kPacketData, packet_length);

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  VerifyReadHeaders(kExpectedFecHeaderSize, kUlpfecPacketMask,
                    kExpectedPacketMaskSize, read_packet);
}

TEST(FlexfecHeaderReaderTest, ReadPacketWithoutStreamSpecificHeaderShouldFail) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);

  // Simulate short received packet.
  ReceivedFecPacket read_packet;
  read_packet.ssrc = kFlexfecSsrc;
  read_packet.pkt = std::move(written_packet);
  read_packet.pkt->data.SetSize(12);

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReaderTest, ReadShortPacketWithKBit0SetShouldFail) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);

  // Simulate short received packet.
  ReceivedFecPacket read_packet;
  read_packet.ssrc = kFlexfecSsrc;
  read_packet.pkt = std::move(written_packet);
  read_packet.pkt->data.SetSize(18);

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReaderTest, ReadShortPacketWithKBit1SetShouldFail) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(15, packet_mask.get());  // This expands the packet mask "once".
  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);

  // Simulate short received packet.
  ReceivedFecPacket read_packet;
  read_packet.ssrc = kFlexfecSsrc;
  read_packet.pkt = std::move(written_packet);
  read_packet.pkt->data.SetSize(20);

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReaderTest, ReadShortPacketWithKBit2SetShouldFail) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(47, packet_mask.get());  // This expands the packet mask "twice".
  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);

  // Simulate short received packet.
  ReceivedFecPacket read_packet;
  read_packet.ssrc = kFlexfecSsrc;
  read_packet.pkt = std::move(written_packet);
  read_packet.pkt->data.SetSize(24);

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderWriterTest, FinalizesHeaderWithKBit0Set) {
  constexpr size_t kExpectedPacketMaskSize = 2;
  constexpr uint8_t kFlexfecPacketMask[] = {0x88, 0x81};
  constexpr uint8_t kUlpfecPacketMask[] = {0x11, 0x02};
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    written_packet.data[i] = i;
  }

  FlexfecHeaderWriter writer;
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, kUlpfecPacketMask,
                           sizeof(kUlpfecPacketMask), &written_packet);

  VerifyFinalizedHeaders(kFlexfecPacketMask, kExpectedPacketMaskSize,
                         written_packet);
}

TEST(FlexfecHeaderWriterTest, FinalizesHeaderWithKBit1Set) {
  constexpr size_t kExpectedPacketMaskSize = 6;
  constexpr uint8_t kFlexfecPacketMask[] = {0x48, 0x81, 0x82, 0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask[] = {0x91, 0x02, 0x08, 0x44, 0x00, 0x84};
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    written_packet.data[i] = i;
  }

  FlexfecHeaderWriter writer;
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, kUlpfecPacketMask,
                           sizeof(kUlpfecPacketMask), &written_packet);

  VerifyFinalizedHeaders(kFlexfecPacketMask, kExpectedPacketMaskSize,
                         written_packet);
}

TEST(FlexfecHeaderWriterTest, FinalizesHeaderWithKBit2Set) {
  constexpr size_t kExpectedPacketMaskSize = 14;
  constexpr uint8_t kFlexfecPacketMask[] = {
      0x11, 0x11,                                     // K-bit 0 clear.
      0x11, 0x11, 0x11, 0x10,                         // K-bit 1 clear.
      0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // K-bit 2 set.
  };
  constexpr uint8_t kUlpfecPacketMask[] = {0x22, 0x22, 0x44, 0x44, 0x44, 0x41};
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    written_packet.data[i] = i;
  }

  FlexfecHeaderWriter writer;
  writer.FinalizeFecHeader(kMediaSsrc, kMediaStartSeqNum, kUlpfecPacketMask,
                           sizeof(kUlpfecPacketMask), &written_packet);

  VerifyFinalizedHeaders(kFlexfecPacketMask, kExpectedPacketMaskSize,
                         written_packet);
}

TEST(FlexfecHeaderWriterTest, ContractsShortUlpfecPacketMaskWithBit15Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(15, packet_mask.get());

  FlexfecHeaderWriter writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[0], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[0], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriterTest, ExpandsShortUlpfecPacketMaskWithBit15Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(15, packet_mask.get());

  FlexfecHeaderWriter writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[1], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[1], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriterTest,
     ContractsLongUlpfecPacketMaskWithBit46ClearBit47Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(46, packet_mask.get());
  ClearBit(47, packet_mask.get());

  FlexfecHeaderWriter writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[1], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[1], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriterTest,
     ExpandsLongUlpfecPacketMaskWithBit46SetBit47Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(46, packet_mask.get());
  ClearBit(47, packet_mask.get());

  FlexfecHeaderWriter writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[2], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[2], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriterTest,
     ExpandsLongUlpfecPacketMaskWithBit46ClearBit47Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(46, packet_mask.get());
  SetBit(47, packet_mask.get());

  FlexfecHeaderWriter writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[2], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[2], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderWriterTest, ExpandsLongUlpfecPacketMaskWithBit46SetBit47Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(46, packet_mask.get());
  SetBit(47, packet_mask.get());

  FlexfecHeaderWriter writer;
  size_t min_packet_mask_size =
      writer.MinPacketMaskSize(packet_mask.get(), packet_mask_size);

  EXPECT_EQ(kFlexfecPacketMaskSizes[2], min_packet_mask_size);
  EXPECT_EQ(kFlexfecHeaderSizes[2], writer.FecHeaderSize(min_packet_mask_size));
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(15, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[0], packet_mask.get(),
                              kFlexfecPacketMaskSizes[0], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(15, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[1], packet_mask.get(),
                              kFlexfecPacketMaskSizes[1], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(46, packet_mask.get());
  ClearBit(47, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[1], packet_mask.get(),
                              kFlexfecPacketMaskSizes[1], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBit46SetBit47Clear) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(46, packet_mask.get());
  ClearBit(47, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[2], packet_mask.get(),
                              kFlexfecPacketMaskSizes[2], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadLargeUlpfecPacketHeaderMaskWithBit46ClearBit47Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  ClearBit(46, packet_mask.get());
  SetBit(47, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[2], packet_mask.get(),
                              kFlexfecPacketMaskSizes[2], *written_packet,
                              *read_packet);
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47Set) {
  const size_t packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  auto packet_mask = GeneratePacketMask(packet_mask_size, 0xabcd);
  SetBit(46, packet_mask.get());
  SetBit(47, packet_mask.get());

  auto written_packet = WriteHeader(packet_mask.get(), packet_mask_size);
  auto read_packet = ReadHeader(*written_packet);

  VerifyWrittenAndReadHeaders(kFlexfecHeaderSizes[2], packet_mask.get(),
                              kFlexfecPacketMaskSizes[2], *written_packet,
                              *read_packet);
}

}  // namespace webrtc
