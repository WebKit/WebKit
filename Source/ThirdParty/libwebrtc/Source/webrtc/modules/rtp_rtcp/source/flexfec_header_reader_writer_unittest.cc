/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
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
#include <vector>

#include "api/array_view.h"
#include "api/make_ref_counted.h"
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
using ProtectedStream = ForwardErrorCorrection::ProtectedStream;
using ReceivedFecPacket = ForwardErrorCorrection::ReceivedFecPacket;
using ::testing::Each;
using ::testing::ElementsAreArray;

constexpr uint8_t kKBit = 1 << 7;
constexpr uint8_t kMask0[] = {0x2B, 0xCD};  // First K bit is cleared.
constexpr uint8_t kMask1[] = {0x92, 0x34,   // First K bit set.
                              0x76, 0x78, 0x9A, 0xBC};  // Second K bit cleared.
constexpr uint8_t kMask2[] = {0x92, 0x34,               //  First K bit set.
                              0xD6, 0x78, 0x9A, 0xBC,   // Second K bit set.
                              0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};

constexpr size_t kMediaPacketLength = 1234;

// Reader tests.
constexpr uint8_t kFlexible = 0b00 << 6;
constexpr uint8_t kPtRecovery = 123;
constexpr uint8_t kLengthRecovery[] = {0xab, 0xcd};
constexpr uint8_t kTsRecovery[] = {0x01, 0x23, 0x45, 0x67};
constexpr uint8_t kSnBases[4][2] = {{0x01, 0x02},
                                    {0x03, 0x04},
                                    {0x05, 0x06},
                                    {0x07, 0x08}};
constexpr uint8_t kPayloadBits = 0x00;

struct FecPacketStreamReadProperties {
  ProtectedStream stream;
  rtc::ArrayView<const uint8_t> mask;
};

struct FecPacketStreamWriteProperties {
  size_t byte_index;
  uint16_t seq_num_base;
  rtc::ArrayView<const uint8_t> mask;
};

Packet WritePacket(
    std::vector<FecHeaderWriter::ProtectedStream> protected_streams) {
  Packet written_packet;
  written_packet.data.SetSize(kMediaPacketLength);
  uint8_t* data = written_packet.data.MutableData();
  for (size_t i = 0; i < written_packet.data.size(); ++i) {
    data[i] = i;
  }

  FlexfecHeaderWriter writer;
  writer.FinalizeFecHeader(protected_streams, written_packet);
  return written_packet;
}

void VerifyReadHeaders(size_t expected_fec_header_size,
                       const ReceivedFecPacket& read_packet,
                       std::vector<FecPacketStreamReadProperties> expected) {
  EXPECT_EQ(read_packet.fec_header_size, expected_fec_header_size);
  const size_t protected_streams_num = read_packet.protected_streams.size();
  EXPECT_EQ(protected_streams_num, expected.size());
  for (size_t i = 0; i < protected_streams_num; ++i) {
    SCOPED_TRACE(i);
    ProtectedStream protected_stream = read_packet.protected_streams[i];
    EXPECT_EQ(protected_stream.ssrc, expected[i].stream.ssrc);
    EXPECT_EQ(protected_stream.seq_num_base, expected[i].stream.seq_num_base);
    EXPECT_EQ(protected_stream.packet_mask_offset,
              expected[i].stream.packet_mask_offset);
    EXPECT_EQ(protected_stream.packet_mask_size,
              expected[i].stream.packet_mask_size);
    // Ensure that the K-bits are removed and the packet mask has been packed.
    EXPECT_THAT(rtc::MakeArrayView(read_packet.pkt->data.cdata() +
                                       protected_stream.packet_mask_offset,
                                   protected_stream.packet_mask_size),
                ElementsAreArray(expected[i].mask));
  }
  EXPECT_EQ(read_packet.pkt->data.size() - expected_fec_header_size,
            read_packet.protection_length);
}

void VerifyFinalizedHeaders(
    const Packet& written_packet,
    std::vector<FecPacketStreamWriteProperties> expected) {
  const uint8_t* packet = written_packet.data.data();
  EXPECT_EQ(packet[0] & 0x80, 0x00);  // F bit clear.
  EXPECT_EQ(packet[0] & 0x40, 0x00);  // R bit clear.
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(i);
    // Verify value of seq_num_base.
    EXPECT_EQ(
        ByteReader<uint16_t>::ReadBigEndian(packet + expected[i].byte_index),
        expected[i].seq_num_base);
    // Verify mask.
    EXPECT_THAT(rtc::MakeArrayView(packet + expected[i].byte_index + 2,
                                   expected[i].mask.size()),
                ElementsAreArray(expected[i].mask));
  }
}

void VerifyWrittenAndReadHeaders(
    std::vector<FecHeaderWriter::ProtectedStream> write_protected_streams,
    uint16_t expected_header_size) {
  // Write FEC Header.
  Packet written_packet = WritePacket(write_protected_streams);

  // Read FEC Header using written data.
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data = written_packet.data;
  for (const FecHeaderWriter::ProtectedStream& stream :
       write_protected_streams) {
    read_packet.protected_streams.push_back({.ssrc = stream.ssrc});
  }

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  // Verify header contents.
  EXPECT_EQ(read_packet.fec_header_size, expected_header_size);
  EXPECT_EQ(read_packet.protected_streams.size(),
            write_protected_streams.size());
  for (size_t i = 0; i < write_protected_streams.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(read_packet.protected_streams[i].seq_num_base,
              write_protected_streams[i].seq_num_base);

    size_t mask_write_size = write_protected_streams[i].packet_mask.size();
    // Read mask returned may be larger than the mask that was sent to the
    // writer; That is ok as long as the specified part of the mask matches, and
    // the rest is 0s.
    FlexfecHeaderWriter writer;
    size_t expected_mask_read_size = writer.MinPacketMaskSize(
        write_protected_streams[i].packet_mask.data(), mask_write_size);
    EXPECT_EQ(read_packet.protected_streams[i].packet_mask_size,
              expected_mask_read_size);

    const uint8_t* read_mask_ptr =
        read_packet.pkt->data.cdata() +
        read_packet.protected_streams[i].packet_mask_offset;
    // Verify actual mask bits.
    EXPECT_THAT(rtc::MakeArrayView(read_mask_ptr, mask_write_size),
                ElementsAreArray(write_protected_streams[i].packet_mask));
    // If read mask size is larger than written mask size, verify all other bits
    // are 0.
    EXPECT_THAT(rtc::MakeArrayView(read_mask_ptr + mask_write_size,
                                   expected_mask_read_size - mask_write_size),
                Each(0));
  }

  // Verify that the call to ReadFecHeader did not tamper with the payload.
  EXPECT_THAT(
      rtc::MakeArrayView(
          read_packet.pkt->data.cdata() + read_packet.fec_header_size,
          read_packet.pkt->data.size() - read_packet.fec_header_size),
      ElementsAreArray(written_packet.data.cdata() + expected_header_size,
                       written_packet.data.size() - expected_header_size));
}

}  // namespace

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithKBit0ClearSingleStream) {
  constexpr size_t kExpectedFecHeaderSize = 12;
  constexpr uint16_t kSnBase = 0x0102;
  constexpr uint8_t kFlexfecPktMask[] = {0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMask[] = {0x11, 0x02};
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBase >> 8,   kSnBase & 0xFF, kFlexfecPktMask[0], kFlexfecPktMask[1],
      kPayloadBits,   kPayloadBits,   kPayloadBits,       kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamReadProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase,
                  .packet_mask_offset = 10,
                  .packet_mask_size = std::size(kUlpfecPacketMask)},
       .mask = kUlpfecPacketMask}};

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithKBit1ClearSingleStream) {
  constexpr size_t kExpectedFecHeaderSize = 16;
  constexpr uint16_t kSnBase = 0x0102;
  constexpr uint8_t kFlexfecPktMask[] = {kKBit | 0x48, 0x81,  //
                                         0x02,         0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask[] = {0x91, 0x02,  //
                                           0x08, 0x44, 0x00, 0x84};
  constexpr uint8_t kPacketData[] = {
      kFlexible,          kPtRecovery,        kLengthRecovery[0],
      kLengthRecovery[1], kTsRecovery[0],     kTsRecovery[1],
      kTsRecovery[2],     kTsRecovery[3],     kSnBase >> 8,
      kSnBase & 0xFF,     kFlexfecPktMask[0], kFlexfecPktMask[1],
      kFlexfecPktMask[2], kFlexfecPktMask[3], kFlexfecPktMask[4],
      kFlexfecPktMask[5], kPayloadBits,       kPayloadBits,
      kPayloadBits,       kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamReadProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase,
                  .packet_mask_offset = 10,
                  .packet_mask_size = std::size(kUlpfecPacketMask)},
       .mask = kUlpfecPacketMask}};

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithBothKBitsSetSingleStream) {
  constexpr size_t kExpectedFecHeaderSize = 24;
  constexpr uint16_t kSnBase = 0x0102;
  constexpr uint8_t kFlexfecPacketMask[] = {kKBit | 0x48, 0x81,              //
                                            kKBit | 0x02, 0x11, 0x00, 0x21,  //
                                            0x01,         0x11, 0x11, 0x11,
                                            0x11,         0x11, 0x11, 0x11};
  constexpr uint8_t kUlpfecPacketMask[] = {0x91, 0x02,              //
                                           0x08, 0x44, 0x00, 0x84,  //
                                           0x04, 0x44, 0x44, 0x44,
                                           0x44, 0x44, 0x44, 0x44};
  constexpr uint8_t kPacketData[] = {kFlexible,
                                     kPtRecovery,
                                     kLengthRecovery[0],
                                     kLengthRecovery[1],
                                     kTsRecovery[0],
                                     kTsRecovery[1],
                                     kTsRecovery[2],
                                     kTsRecovery[3],
                                     kSnBase >> 8,
                                     kSnBase & 0xFF,
                                     kFlexfecPacketMask[0],
                                     kFlexfecPacketMask[1],
                                     kFlexfecPacketMask[2],
                                     kFlexfecPacketMask[3],
                                     kFlexfecPacketMask[4],
                                     kFlexfecPacketMask[5],
                                     kFlexfecPacketMask[6],
                                     kFlexfecPacketMask[7],
                                     kFlexfecPacketMask[8],
                                     kFlexfecPacketMask[9],
                                     kFlexfecPacketMask[10],
                                     kFlexfecPacketMask[11],
                                     kFlexfecPacketMask[12],
                                     kFlexfecPacketMask[13],
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamReadProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase,
                  .packet_mask_offset = 10,
                  .packet_mask_size = std::size(kUlpfecPacketMask)},
       .mask = kUlpfecPacketMask}};

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithKBit0Clear2Streams) {
  constexpr size_t kExpectedFecHeaderSize = 16;
  constexpr uint16_t kSnBase0 = 0x0102;
  constexpr uint16_t kSnBase1 = 0x0304;
  constexpr uint8_t kFlexfecPktMask1[] = {0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x11, 0x02};
  constexpr uint8_t kFlexfecPktMask2[] = {0x04, 0x41};
  constexpr uint8_t kUlpfecPacketMask2[] = {0x08, 0x82};

  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,     kLengthRecovery[0],  kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1],  kTsRecovery[2],      kTsRecovery[3],
      kSnBase0 >> 8,  kSnBase0 & 0xFF, kFlexfecPktMask1[0], kFlexfecPktMask1[1],
      kSnBase1 >> 8,  kSnBase1 & 0xFF, kFlexfecPktMask2[0], kFlexfecPktMask2[1],
      kPayloadBits,   kPayloadBits,    kPayloadBits,        kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamReadProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase0,
                  .packet_mask_offset = 10,
                  .packet_mask_size = std::size(kUlpfecPacketMask1)},
       .mask = kUlpfecPacketMask1},
      {.stream = {.ssrc = 0x02,
                  .seq_num_base = kSnBase1,
                  .packet_mask_offset = 14,
                  .packet_mask_size = std::size(kUlpfecPacketMask2)},
       .mask = kUlpfecPacketMask2},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithKBit1Clear2Streams) {
  constexpr size_t kExpectedFecHeaderSize = 24;
  constexpr uint16_t kSnBase0 = 0x0102;
  constexpr uint16_t kSnBase1 = 0x0304;
  constexpr uint8_t kFlexfecPktMask1[] = {kKBit | 0x48, 0x81,  //
                                          0x02,         0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x91, 0x02,  //
                                            0x08, 0x44, 0x00, 0x84};
  constexpr uint8_t kFlexfecPktMask2[] = {kKBit | 0x57, 0x82,  //
                                          0x04,         0x33, 0x00, 0x51};
  constexpr uint8_t kUlpfecPacketMask2[] = {0xAF, 0x04,  //
                                            0x10, 0xCC, 0x01, 0x44};
  constexpr uint8_t kPacketData[] = {
      kFlexible,           kPtRecovery,         kLengthRecovery[0],
      kLengthRecovery[1],  kTsRecovery[0],      kTsRecovery[1],
      kTsRecovery[2],      kTsRecovery[3],      kSnBase0 >> 8,
      kSnBase0 & 0xFF,     kFlexfecPktMask1[0], kFlexfecPktMask1[1],
      kFlexfecPktMask1[2], kFlexfecPktMask1[3], kFlexfecPktMask1[4],
      kFlexfecPktMask1[5], kSnBase1 >> 8,       kSnBase1 & 0xFF,
      kFlexfecPktMask2[0], kFlexfecPktMask2[1], kFlexfecPktMask2[2],
      kFlexfecPktMask2[3], kFlexfecPktMask2[4], kFlexfecPktMask2[5],
      kPayloadBits,        kPayloadBits,        kPayloadBits,
      kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamReadProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase0,
                  .packet_mask_offset = 10,
                  .packet_mask_size = std::size(kUlpfecPacketMask1)},
       .mask = kUlpfecPacketMask1},
      {.stream = {.ssrc = 0x02,
                  .seq_num_base = kSnBase1,
                  .packet_mask_offset = 18,
                  .packet_mask_size = std::size(kUlpfecPacketMask2)},
       .mask = kUlpfecPacketMask2},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithBothKBitsSet2Streams) {
  constexpr size_t kExpectedFecHeaderSize = 40;
  constexpr uint16_t kSnBase0 = 0x0102;
  constexpr uint16_t kSnBase1 = 0x0304;
  constexpr uint8_t kFlexfecPktMask1[] = {kKBit | 0x48, 0x81,              //
                                          kKBit | 0x02, 0x11, 0x00, 0x21,  //
                                          0x01,         0x11, 0x11, 0x11,
                                          0x11,         0x11, 0x11, 0x11};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x91, 0x02,              //
                                            0x08, 0x44, 0x00, 0x84,  //
                                            0x04, 0x44, 0x44, 0x44,
                                            0x44, 0x44, 0x44, 0x44};
  constexpr uint8_t kFlexfecPktMask2[] = {kKBit | 0x32, 0x84,              //
                                          kKBit | 0x05, 0x23, 0x00, 0x55,  //
                                          0xA3,         0x22, 0x22, 0x22,
                                          0x22,         0x22, 0x22, 0x35};
  constexpr uint8_t kUlpfecPacketMask2[] = {0x65, 0x08,              //
                                            0x14, 0x8C, 0x01, 0x56,  //
                                            0x8C, 0x88, 0x88, 0x88,
                                            0x88, 0x88, 0x88, 0xD4};

  constexpr uint8_t kPacketData[] = {kFlexible,
                                     kPtRecovery,
                                     kLengthRecovery[0],
                                     kLengthRecovery[1],
                                     kTsRecovery[0],
                                     kTsRecovery[1],
                                     kTsRecovery[2],
                                     kTsRecovery[3],
                                     kSnBase0 >> 8,
                                     kSnBase0 & 0xFF,
                                     kFlexfecPktMask1[0],
                                     kFlexfecPktMask1[1],
                                     kFlexfecPktMask1[2],
                                     kFlexfecPktMask1[3],
                                     kFlexfecPktMask1[4],
                                     kFlexfecPktMask1[5],
                                     kFlexfecPktMask1[6],
                                     kFlexfecPktMask1[7],
                                     kFlexfecPktMask1[8],
                                     kFlexfecPktMask1[9],
                                     kFlexfecPktMask1[10],
                                     kFlexfecPktMask1[11],
                                     kFlexfecPktMask1[12],
                                     kFlexfecPktMask1[13],
                                     kSnBase1 >> 8,
                                     kSnBase1 & 0xFF,
                                     kFlexfecPktMask2[0],
                                     kFlexfecPktMask2[1],
                                     kFlexfecPktMask2[2],
                                     kFlexfecPktMask2[3],
                                     kFlexfecPktMask2[4],
                                     kFlexfecPktMask2[5],
                                     kFlexfecPktMask2[6],
                                     kFlexfecPktMask2[7],
                                     kFlexfecPktMask2[8],
                                     kFlexfecPktMask2[9],
                                     kFlexfecPktMask2[10],
                                     kFlexfecPktMask2[11],
                                     kFlexfecPktMask2[12],
                                     kFlexfecPktMask2[13],
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamReadProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase0,
                  .packet_mask_offset = 10,
                  .packet_mask_size = std::size(kUlpfecPacketMask1)},
       .mask = kUlpfecPacketMask1},
      {.stream = {.ssrc = 0x02,
                  .seq_num_base = kSnBase1,
                  .packet_mask_offset = 26,
                  .packet_mask_size = std::size(kUlpfecPacketMask2)},
       .mask = kUlpfecPacketMask2},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReaderTest, ReadsHeaderWithMultipleStreamsMultipleMasks) {
  constexpr size_t kExpectedFecHeaderSize = 44;
  constexpr uint16_t kSnBase0 = 0x0102;
  constexpr uint16_t kSnBase1 = 0x0304;
  constexpr uint16_t kSnBase2 = 0x0506;
  constexpr uint16_t kSnBase3 = 0x0708;
  constexpr uint8_t kFlexfecPacketMask1[] = {0x29, 0x91};
  constexpr uint8_t kUlpfecPacketMask1[] = {0x53, 0x22};
  constexpr uint8_t kFlexfecPacketMask2[] = {kKBit | 0x32, 0xA1,  //
                                             0x02,         0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask2[] = {0x65, 0x42,  //
                                            0x08, 0x44, 0x00, 0x84};
  constexpr uint8_t kFlexfecPacketMask3[] = {kKBit | 0x48, 0x81,              //
                                             kKBit | 0x02, 0x11, 0x00, 0x21,  //
                                             0x01,         0x11, 0x11, 0x11,
                                             0x11,         0x11, 0x11, 0x11};
  constexpr uint8_t kUlpfecPacketMask3[] = {0x91, 0x02,              //
                                            0x08, 0x44, 0x00, 0x84,  //
                                            0x04, 0x44, 0x44, 0x44,
                                            0x44, 0x44, 0x44, 0x44};
  constexpr uint8_t kFlexfecPacketMask4[] = {kKBit | 0x32, 0x84,  //
                                             0x05,         0x23, 0x00, 0x55};
  constexpr uint8_t kUlpfecPacketMask4[] = {0x65, 0x08,  //
                                            0x14, 0x8C, 0x01, 0x54};
  constexpr uint8_t kPacketData[] = {kFlexible,
                                     kPtRecovery,
                                     kLengthRecovery[0],
                                     kLengthRecovery[1],
                                     kTsRecovery[0],
                                     kTsRecovery[1],
                                     kTsRecovery[2],
                                     kTsRecovery[3],
                                     kSnBase0 >> 8,
                                     kSnBase0 & 0xFF,
                                     kFlexfecPacketMask1[0],
                                     kFlexfecPacketMask1[1],
                                     kSnBase1 >> 8,
                                     kSnBase1 & 0xFF,
                                     kFlexfecPacketMask2[0],
                                     kFlexfecPacketMask2[1],
                                     kFlexfecPacketMask2[2],
                                     kFlexfecPacketMask2[3],
                                     kFlexfecPacketMask2[4],
                                     kFlexfecPacketMask2[5],
                                     kSnBase2 >> 8,
                                     kSnBase2 & 0xFF,
                                     kFlexfecPacketMask3[0],
                                     kFlexfecPacketMask3[1],
                                     kFlexfecPacketMask3[2],
                                     kFlexfecPacketMask3[3],
                                     kFlexfecPacketMask3[4],
                                     kFlexfecPacketMask3[5],
                                     kFlexfecPacketMask3[6],
                                     kFlexfecPacketMask3[7],
                                     kFlexfecPacketMask3[8],
                                     kFlexfecPacketMask3[9],
                                     kFlexfecPacketMask3[10],
                                     kFlexfecPacketMask3[11],
                                     kFlexfecPacketMask3[12],
                                     kFlexfecPacketMask3[13],
                                     kSnBase3 >> 8,
                                     kSnBase3 & 0xFF,
                                     kFlexfecPacketMask4[0],
                                     kFlexfecPacketMask4[1],
                                     kFlexfecPacketMask4[2],
                                     kFlexfecPacketMask4[3],
                                     kFlexfecPacketMask4[4],
                                     kFlexfecPacketMask4[5],
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits,
                                     kPayloadBits};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {
      {.ssrc = 0x01}, {.ssrc = 0x02}, {.ssrc = 0x03}, {.ssrc = 0x04}};

  FlexfecHeaderReader reader;
  EXPECT_TRUE(reader.ReadFecHeader(&read_packet));

  std::vector<FecPacketStreamReadProperties> expected = {
      {.stream = {.ssrc = 0x01,
                  .seq_num_base = kSnBase0,
                  .packet_mask_offset = 10,
                  .packet_mask_size = std::size(kUlpfecPacketMask1)},
       .mask = kUlpfecPacketMask1},
      {.stream = {.ssrc = 0x02,
                  .seq_num_base = kSnBase1,
                  .packet_mask_offset = 14,
                  .packet_mask_size = std::size(kUlpfecPacketMask2)},
       .mask = kUlpfecPacketMask2},
      {.stream = {.ssrc = 0x03,
                  .seq_num_base = kSnBase2,
                  .packet_mask_offset = 22,
                  .packet_mask_size = std::size(kUlpfecPacketMask3)},
       .mask = kUlpfecPacketMask3},
      {.stream = {.ssrc = 0x04,
                  .seq_num_base = kSnBase3,
                  .packet_mask_offset = 38,
                  .packet_mask_size = std::size(kUlpfecPacketMask4)},
       .mask = kUlpfecPacketMask4},
  };

  VerifyReadHeaders(kExpectedFecHeaderSize, read_packet, expected);
}

TEST(FlexfecHeaderReaderTest, ReadPacketWithoutProtectedSsrcsShouldFail) {
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  // No protected ssrcs.
  read_packet.protected_streams = {};

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReaderTest, ReadPacketWithoutStreamSpecificHeaderShouldFail) {
  // Simulate short received packet.
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  read_packet.pkt->data.SetData(kPacketData);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReaderTest, ReadShortPacketWithKBit0SetShouldFail) {
  // Simulate short received packet.
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBases[0][0], kSnBases[0][1], kMask0[0],          kMask0[1]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  // Expected to have 2 bytes of mask but length of packet misses 1 byte.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 1);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReaderTest, ReadShortPacketWithKBit1ClearShouldFail) {
  // Simulate short received packet.
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBases[0][0], kSnBases[0][1], kMask1[0],          kMask1[1],
      kMask1[2],      kMask1[3],      kMask1[4],          kMask1[5]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  // Expected to have 6 bytes of mask but length of packet misses 2 bytes.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReaderTest, ReadShortPacketWithKBit1SetShouldFail) {
  // Simulate short received packet.
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBases[0][0], kSnBases[0][1], kMask2[0],          kMask2[1],
      kMask2[2],      kMask2[3],      kMask2[4],          kMask2[5],
      kMask2[6],      kMask2[7],      kMask2[8],          kMask2[9],
      kMask2[10],     kMask2[11],     kMask2[12],         kMask2[13]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  // Expected to have 14 bytes of mask but length of packet misses 2 bytes.
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_streams = {{.ssrc = 0x01}};

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderReaderTest, ReadShortPacketMultipleStreamsShouldFail) {
  // Simulate short received packet with 2 protected ssrcs.
  constexpr uint8_t kPacketData[] = {
      kFlexible,      kPtRecovery,    kLengthRecovery[0], kLengthRecovery[1],
      kTsRecovery[0], kTsRecovery[1], kTsRecovery[2],     kTsRecovery[3],
      kSnBases[0][0], kSnBases[0][1], kMask0[0],          kMask0[1],
      kSnBases[1][0], kSnBases[1][1], kMask2[0],          kMask2[1],
      kMask2[2],      kMask2[3],      kMask2[4],          kMask2[5],
      kMask2[6],      kMask2[7],      kMask2[8],          kMask2[9],
      kMask2[10],     kMask2[11],     kMask2[12],         kMask2[13]};
  ReceivedFecPacket read_packet;
  read_packet.pkt = rtc::make_ref_counted<Packet>();
  // Subtract 2 bytes from length, so the read will fail on parsing second
  read_packet.pkt->data.SetData(kPacketData, sizeof(kPacketData) - 2);
  read_packet.protected_streams = {{.ssrc = 0x01}, {.ssrc = 0x02}};

  FlexfecHeaderReader reader;
  EXPECT_FALSE(reader.ReadFecHeader(&read_packet));
}

TEST(FlexfecHeaderWriterTest, FinalizesHeaderWithKBit0ClearSingleStream) {
  constexpr uint8_t kFlexfecPacketMask[] = {0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMask[] = {0x11, 0x02};
  constexpr uint16_t kMediaStartSeqNum = 1234;
  Packet written_packet = WritePacket({{.ssrc = 0x01,
                                        .seq_num_base = kMediaStartSeqNum,
                                        .packet_mask = kUlpfecPacketMask}});

  std::vector<FecPacketStreamWriteProperties> expected = {
      {.byte_index = 8,
       .seq_num_base = kMediaStartSeqNum,
       .mask = kFlexfecPacketMask}};

  VerifyFinalizedHeaders(written_packet, expected);
}

TEST(FlexfecHeaderWriterTest, FinalizesHeaderWithKBit1ClearSingleStream) {
  constexpr uint8_t kFlexfecPacketMask[] = {0xC8, 0x81, 0x02, 0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask[] = {0x91, 0x02, 0x08, 0x44, 0x00, 0x84};
  constexpr uint16_t kMediaStartSeqNum = 1234;
  Packet written_packet = WritePacket({{.ssrc = 0x01,
                                        .seq_num_base = kMediaStartSeqNum,
                                        .packet_mask = kUlpfecPacketMask}});

  std::vector<FecPacketStreamWriteProperties> expected = {
      {.byte_index = 8,
       .seq_num_base = kMediaStartSeqNum,
       .mask = kFlexfecPacketMask}};

  VerifyFinalizedHeaders(written_packet, expected);
}

TEST(FlexfecHeaderWriterTest, FinalizesHeaderWithBothKBitsSetSingleStream) {
  constexpr uint8_t kFlexfecPacketMask[] = {
      0x91, 0x11,                                     // K-bit 0 set.
      0x91, 0x11, 0x11, 0x10,                         // K-bit 1 set.
      0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  //
  };
  constexpr uint8_t kUlpfecPacketMask[] = {0x22, 0x22, 0x44, 0x44, 0x44, 0x41};
  constexpr uint16_t kMediaStartSeqNum = 1234;
  Packet written_packet = WritePacket({{.ssrc = 0x01,
                                        .seq_num_base = kMediaStartSeqNum,
                                        .packet_mask = kUlpfecPacketMask}});

  std::vector<FecPacketStreamWriteProperties> expected = {
      {.byte_index = 8,
       .seq_num_base = kMediaStartSeqNum,
       .mask = kFlexfecPacketMask}};

  VerifyFinalizedHeaders(written_packet, expected);
}

TEST(FlexfecHeaderWriterTest, FinalizesHeaderMultipleStreamsMultipleMasks) {
  constexpr uint8_t kFlexfecPacketMask1[] = {
      0x91, 0x11,                                     // K-bit 0 set.
      0x91, 0x11, 0x11, 0x10,                         // K-bit 1 set.
      0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  //
  };
  constexpr uint8_t kUlpfecPacketMask1[] = {0x22, 0x22, 0x44, 0x44, 0x44, 0x41};
  constexpr uint16_t kMediaStartSeqNum1 = 1234;
  constexpr uint8_t kFlexfecPacketMask2[] = {0x08, 0x81};
  constexpr uint8_t kUlpfecPacketMask2[] = {0x11, 0x02};
  constexpr uint16_t kMediaStartSeqNum2 = 2345;
  constexpr uint8_t kFlexfecPacketMask3[] = {0xC8, 0x81, 0x02,
                                             0x11, 0x00, 0x21};
  constexpr uint8_t kUlpfecPacketMask3[] = {0x91, 0x02, 0x08, 0x44, 0x00, 0x84};
  constexpr uint16_t kMediaStartSeqNum3 = 3456;
  constexpr uint8_t kFlexfecPacketMask4[] = {
      0xD5, 0xAA,                                     // K-bit 0 set.
      0xA2, 0xAB, 0xCD, 0xEF,                         // K-bit 1 set.
      0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  //
  };
  constexpr uint8_t kUlpfecPacketMask4[] = {0xAB, 0x54, 0x8A, 0xAF, 0x37, 0xBF};
  constexpr uint16_t kMediaStartSeqNum4 = 4567;

  Packet written_packet = WritePacket({{.ssrc = 0x01,
                                        .seq_num_base = kMediaStartSeqNum1,
                                        .packet_mask = kUlpfecPacketMask1},
                                       {.ssrc = 0x02,
                                        .seq_num_base = kMediaStartSeqNum2,
                                        .packet_mask = kUlpfecPacketMask2},
                                       {.ssrc = 0x03,
                                        .seq_num_base = kMediaStartSeqNum3,
                                        .packet_mask = kUlpfecPacketMask3},
                                       {.ssrc = 0x04,
                                        .seq_num_base = kMediaStartSeqNum4,
                                        .packet_mask = kUlpfecPacketMask4}});

  std::vector<FecPacketStreamWriteProperties> expected = {
      {.byte_index = 8,
       .seq_num_base = kMediaStartSeqNum1,
       .mask = kFlexfecPacketMask1},
      {.byte_index = 24,
       .seq_num_base = kMediaStartSeqNum2,
       .mask = kFlexfecPacketMask2},
      {.byte_index = 28,
       .seq_num_base = kMediaStartSeqNum3,
       .mask = kFlexfecPacketMask3},
      {.byte_index = 36,
       .seq_num_base = kMediaStartSeqNum4,
       .mask = kFlexfecPacketMask4}};

  VerifyFinalizedHeaders(written_packet, expected);
}

// TODO(bugs.webrtc.org/15002): reimplement and add tests for multi stream cases
// after updating the MinPacketMaskSize and FecHeaderSize functions.

TEST(FlexfecHeaderWriterTest, ContractsShortUlpfecPacketMaskWithBit15Clear) {}

TEST(FlexfecHeaderWriterTest, ExpandsShortUlpfecPacketMaskWithBit15Set) {}

TEST(FlexfecHeaderWriterTest,
     ContractsLongUlpfecPacketMaskWithBit46ClearBit47Clear) {}

TEST(FlexfecHeaderWriterTest,
     ExpandsLongUlpfecPacketMaskWithBit46SetBit47Clear) {}

TEST(FlexfecHeaderWriterTest,
     ExpandsLongUlpfecPacketMaskWithBit46ClearBit47Set) {}

TEST(FlexfecHeaderWriterTest, ExpandsLongUlpfecPacketMaskWithBit46SetBit47Set) {
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15ClearSingleStream) {
  constexpr uint8_t kUlpfecPacketMask[] = {0x11, 0x02};  // Bit 15 clear.
  constexpr uint16_t kMediaStartSeqNum = 1234;
  constexpr uint16_t kExpectedHeaderSize = 12;

  VerifyWrittenAndReadHeaders({{.ssrc = 0x01,
                                .seq_num_base = kMediaStartSeqNum,
                                .packet_mask = kUlpfecPacketMask}},
                              kExpectedHeaderSize);
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadSmallUlpfecPacketHeaderWithMaskBit15SetSingleStream) {
  constexpr uint8_t kUlpfecPacketMask[] = {0xAA, 0xFF};  // Bit 15 set.
  constexpr uint16_t kMediaStartSeqNum = 1234;
  constexpr uint16_t kExpectedHeaderSize = 16;

  VerifyWrittenAndReadHeaders({{.ssrc = 0x01,
                                .seq_num_base = kMediaStartSeqNum,
                                .packet_mask = kUlpfecPacketMask}},
                              kExpectedHeaderSize);
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47ClearSingleStream) {
  constexpr uint8_t kUlpfecPacketMask[] = {0x91, 0x02, 0x08, 0x44,
                                           0x00, 0x84};  // Bits 46, 47 clear.
  constexpr uint16_t kMediaStartSeqNum = 1234;
  constexpr uint16_t kExpectedHeaderSize = 16;

  VerifyWrittenAndReadHeaders({{.ssrc = 0x01,
                                .seq_num_base = kMediaStartSeqNum,
                                .packet_mask = kUlpfecPacketMask}},
                              kExpectedHeaderSize);
}

TEST(
    FlexfecHeaderReaderWriterTest,
    WriteAndReadLargeUlpfecPacketHeaderWithMaskBit46SetBit47ClearSingleStream) {
  constexpr uint8_t kUlpfecPacketMask[] = {
      0x91, 0x02, 0x08, 0x44, 0x00, 0x86};  // Bit 46 set, bit 47 clear.
  constexpr uint16_t kMediaStartSeqNum = 1234;
  constexpr uint16_t kExpectedHeaderSize = 24;

  VerifyWrittenAndReadHeaders({{.ssrc = 0x01,
                                .seq_num_base = kMediaStartSeqNum,
                                .packet_mask = kUlpfecPacketMask}},
                              kExpectedHeaderSize);
}

TEST(
    FlexfecHeaderReaderWriterTest,
    WriteAndReadLargeUlpfecPacketHeaderMaskWithBit46ClearBit47SetSingleStream) {
  constexpr uint8_t kUlpfecPacketMask[] = {
      0x91, 0x02, 0x08, 0x44, 0x00, 0x85};  // Bit 46 clear, bit 47 set.
  constexpr uint16_t kMediaStartSeqNum = 1234;
  constexpr uint16_t kExpectedHeaderSize = 24;

  VerifyWrittenAndReadHeaders({{.ssrc = 0x01,
                                .seq_num_base = kMediaStartSeqNum,
                                .packet_mask = kUlpfecPacketMask}},
                              kExpectedHeaderSize);
}

TEST(FlexfecHeaderReaderWriterTest,
     WriteAndReadLargeUlpfecPacketHeaderWithMaskBits46And47SetSingleStream) {
  constexpr uint8_t kUlpfecPacketMask[] = {0x91, 0x02, 0x08, 0x44,
                                           0x00, 0x87};  // Bits 46, 47 set.
  constexpr uint16_t kMediaStartSeqNum = 1234;
  constexpr uint16_t kExpectedHeaderSize = 24;

  VerifyWrittenAndReadHeaders({{.ssrc = 0x01,
                                .seq_num_base = kMediaStartSeqNum,
                                .packet_mask = kUlpfecPacketMask}},
                              kExpectedHeaderSize);
}

TEST(FlexfecHeaderReaderWriterTest, WriteAndReadMultipleStreamsMultipleMasks) {
  constexpr uint8_t kUlpfecPacketMask1[] = {0x11, 0x02};
  constexpr uint16_t kMediaStartSeqNum1 = 1234;
  constexpr uint8_t kUlpfecPacketMask2[] = {0x91, 0x02, 0x08, 0x44, 0x00, 0x84};
  constexpr uint16_t kMediaStartSeqNum2 = 2345;
  constexpr uint8_t kUlpfecPacketMask3[] = {0xAA, 0xFF};
  constexpr uint16_t kMediaStartSeqNum3 = 3456;
  constexpr uint8_t kUlpfecPacketMask4[] = {0x91, 0x02, 0x08, 0x44, 0x00, 0x87};
  constexpr uint16_t kMediaStartSeqNum4 = 4567;
  constexpr uint16_t kExpectedHeaderSize = 44;

  VerifyWrittenAndReadHeaders({{.ssrc = 0x01,
                                .seq_num_base = kMediaStartSeqNum1,
                                .packet_mask = kUlpfecPacketMask1},
                               {.ssrc = 0x02,
                                .seq_num_base = kMediaStartSeqNum2,
                                .packet_mask = kUlpfecPacketMask2},
                               {.ssrc = 0x03,
                                .seq_num_base = kMediaStartSeqNum3,
                                .packet_mask = kUlpfecPacketMask3},
                               {.ssrc = 0x04,
                                .seq_num_base = kMediaStartSeqNum4,
                                .packet_mask = kUlpfecPacketMask4}},
                              kExpectedHeaderSize);
}

}  // namespace webrtc
