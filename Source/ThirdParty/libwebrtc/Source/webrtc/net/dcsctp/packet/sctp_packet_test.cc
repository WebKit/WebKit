/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/sctp_packet.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/common/math.h"
#include "net/dcsctp/packet/chunk/abort_chunk.h"
#include "net/dcsctp/packet/chunk/cookie_ack_chunk.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/init_chunk.h"
#include "net/dcsctp/packet/chunk/sack_chunk.h"
#include "net/dcsctp/packet/error_cause/error_cause.h"
#include "net/dcsctp/packet/error_cause/user_initiated_abort_cause.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/packet/tlv_trait.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;
using ::testing::SizeIs;

constexpr VerificationTag kVerificationTag = VerificationTag(0x12345678);
constexpr DcSctpOptions kVerifyChecksumOptions =
    DcSctpOptions{.disable_checksum_verification = false,
                  .enable_zero_checksum = false};

TEST(SctpPacketTest, DeserializeSimplePacketFromCapture) {
  /*
  Stream Control Transmission Protocol, Src Port: 5000 (5000), Dst Port: 5000
    (5000) Source port: 5000 Destination port: 5000 Verification tag: 0x00000000
      [Association index: 1]
      Checksum: 0xaa019d33 [unverified]
      [Checksum Status: Unverified]
      INIT chunk (Outbound streams: 1000, inbound streams: 1000)
          Chunk type: INIT (1)
          Chunk flags: 0x00
          Chunk length: 90
          Initiate tag: 0x0eddca08
          Advertised receiver window credit (a_rwnd): 131072
          Number of outbound streams: 1000
          Number of inbound streams: 1000
          Initial TSN: 1426601527
          ECN parameter
              Parameter type: ECN (0x8000)
              Parameter length: 4
          Forward TSN supported parameter
              Parameter type: Forward TSN supported (0xc000)
              Parameter length: 4
          Supported Extensions parameter (Supported types: FORWARD_TSN, AUTH,
            ASCONF, ASCONF_ACK, RE_CONFIG) Parameter type: Supported Extensions
  (0x8008) Parameter length: 9 Supported chunk type: FORWARD_TSN (192) Supported
  chunk type: AUTH (15) Supported chunk type: ASCONF (193) Supported chunk type:
  ASCONF_ACK (128) Supported chunk type: RE_CONFIG (130) Parameter padding:
  000000 Random parameter Parameter type: Random (0x8002) Parameter length: 36
              Random number: c5a86155090e6f420050634cc8d6b908dfd53e17c99cb143…
          Requested HMAC Algorithm parameter (Supported HMACs: SHA-1)
              Parameter type: Requested HMAC Algorithm (0x8004)
              Parameter length: 6
              HMAC identifier: SHA-1 (1)
              Parameter padding: 0000
          Authenticated Chunk list parameter (Chunk types to be authenticated:
            ASCONF_ACK, ASCONF) Parameter type: Authenticated Chunk list
  (0x8003) Parameter length: 6 Chunk type: ASCONF_ACK (128) Chunk type: ASCONF
  (193) Chunk padding: 0000
  */

  uint8_t data[] = {
      0x13, 0x88, 0x13, 0x88, 0x00, 0x00, 0x00, 0x00, 0xaa, 0x01, 0x9d, 0x33,
      0x01, 0x00, 0x00, 0x5a, 0x0e, 0xdd, 0xca, 0x08, 0x00, 0x02, 0x00, 0x00,
      0x03, 0xe8, 0x03, 0xe8, 0x55, 0x08, 0x36, 0x37, 0x80, 0x00, 0x00, 0x04,
      0xc0, 0x00, 0x00, 0x04, 0x80, 0x08, 0x00, 0x09, 0xc0, 0x0f, 0xc1, 0x80,
      0x82, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00, 0x24, 0xc5, 0xa8, 0x61, 0x55,
      0x09, 0x0e, 0x6f, 0x42, 0x00, 0x50, 0x63, 0x4c, 0xc8, 0xd6, 0xb9, 0x08,
      0xdf, 0xd5, 0x3e, 0x17, 0xc9, 0x9c, 0xb1, 0x43, 0x28, 0x4e, 0xaf, 0x64,
      0x68, 0x2a, 0xc2, 0x97, 0x80, 0x04, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00,
      0x80, 0x03, 0x00, 0x06, 0x80, 0xc1, 0x00, 0x00};

  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket packet,
                              SctpPacket::Parse(data, kVerifyChecksumOptions));
  EXPECT_EQ(packet.common_header().source_port, 5000);
  EXPECT_EQ(packet.common_header().destination_port, 5000);
  EXPECT_EQ(packet.common_header().verification_tag, VerificationTag(0));
  EXPECT_EQ(packet.common_header().checksum, 0xaa019d33);

  EXPECT_THAT(packet.descriptors(), SizeIs(1));
  EXPECT_EQ(packet.descriptors()[0].type, InitChunk::kType);
  ASSERT_HAS_VALUE_AND_ASSIGN(InitChunk init,
                              InitChunk::Parse(packet.descriptors()[0].data));
  EXPECT_EQ(init.initial_tsn(), TSN(1426601527));
}

TEST(SctpPacketTest, DeserializePacketWithTwoChunks) {
  /*
  Stream Control Transmission Protocol, Src Port: 1234 (1234),
    Dst Port: 4321 (4321)
      Source port: 1234
      Destination port: 4321
      Verification tag: 0x697e3a4e
      [Association index: 3]
      Checksum: 0xc06e8b36 [unverified]
      [Checksum Status: Unverified]
      COOKIE_ACK chunk
          Chunk type: COOKIE_ACK (11)
          Chunk flags: 0x00
          Chunk length: 4
      SACK chunk (Cumulative TSN: 2930332242, a_rwnd: 131072,
        gaps: 0, duplicate TSNs: 0)
          Chunk type: SACK (3)
          Chunk flags: 0x00
          Chunk length: 16
          Cumulative TSN ACK: 2930332242
          Advertised receiver window credit (a_rwnd): 131072
          Number of gap acknowledgement blocks: 0
          Number of duplicated TSNs: 0
  */

  uint8_t data[] = {0x04, 0xd2, 0x10, 0xe1, 0x69, 0x7e, 0x3a, 0x4e,
                    0xc0, 0x6e, 0x8b, 0x36, 0x0b, 0x00, 0x00, 0x04,
                    0x03, 0x00, 0x00, 0x10, 0xae, 0xa9, 0x52, 0x52,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  ASSERT_HAS_VALUE_AND_ASSIGN(SctpPacket packet,
                              SctpPacket::Parse(data, kVerifyChecksumOptions));
  EXPECT_EQ(packet.common_header().source_port, 1234);
  EXPECT_EQ(packet.common_header().destination_port, 4321);
  EXPECT_EQ(packet.common_header().verification_tag,
            VerificationTag(0x697e3a4eu));
  EXPECT_EQ(packet.common_header().checksum, 0xc06e8b36u);

  EXPECT_THAT(packet.descriptors(), SizeIs(2));
  EXPECT_EQ(packet.descriptors()[0].type, CookieAckChunk::kType);
  EXPECT_EQ(packet.descriptors()[1].type, SackChunk::kType);
  ASSERT_HAS_VALUE_AND_ASSIGN(
      CookieAckChunk cookie_ack,
      CookieAckChunk::Parse(packet.descriptors()[0].data));
  ASSERT_HAS_VALUE_AND_ASSIGN(SackChunk sack,
                              SackChunk::Parse(packet.descriptors()[1].data));
}

TEST(SctpPacketTest, DeserializePacketWithWrongChecksum) {
  /*
  Stream Control Transmission Protocol, Src Port: 5000 (5000),
    Dst Port: 5000 (5000)
      Source port: 5000
      Destination port: 5000
      Verification tag: 0x0eddca08
      [Association index: 1]
      Checksum: 0x2a81f531 [unverified]
      [Checksum Status: Unverified]
      SACK chunk (Cumulative TSN: 1426601536, a_rwnd: 131072,
        gaps: 0, duplicate TSNs: 0)
          Chunk type: SACK (3)
          Chunk flags: 0x00
          Chunk length: 16
          Cumulative TSN ACK: 1426601536
          Advertised receiver window credit (a_rwnd): 131072
          Number of gap acknowledgement blocks: 0
          Number of duplicated TSNs: 0
  */

  uint8_t data[] = {0x13, 0x88, 0x13, 0x88, 0x0e, 0xdd, 0xca, 0x08, 0x2a, 0x81,
                    0xf5, 0x31, 0x03, 0x00, 0x00, 0x10, 0x55, 0x08, 0x36, 0x40,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  EXPECT_FALSE(SctpPacket::Parse(data, kVerifyChecksumOptions).has_value());
}

TEST(SctpPacketTest, DeserializePacketDontValidateChecksum) {
  /*
  Stream Control Transmission Protocol, Src Port: 5000 (5000),
    Dst Port: 5000 (5000)
      Source port: 5000
      Destination port: 5000
      Verification tag: 0x0eddca08
      [Association index: 1]
      Checksum: 0x2a81f531 [unverified]
      [Checksum Status: Unverified]
      SACK chunk (Cumulative TSN: 1426601536, a_rwnd: 131072,
        gaps: 0, duplicate TSNs: 0)
          Chunk type: SACK (3)
          Chunk flags: 0x00
          Chunk length: 16
          Cumulative TSN ACK: 1426601536
          Advertised receiver window credit (a_rwnd): 131072
          Number of gap acknowledgement blocks: 0
          Number of duplicated TSNs: 0
  */

  uint8_t data[] = {0x13, 0x88, 0x13, 0x88, 0x0e, 0xdd, 0xca, 0x08, 0x2a, 0x81,
                    0xf5, 0x31, 0x03, 0x00, 0x00, 0x10, 0x55, 0x08, 0x36, 0x40,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  ASSERT_HAS_VALUE_AND_ASSIGN(
      SctpPacket packet,
      SctpPacket::Parse(data, {.disable_checksum_verification = true,
                               .enable_zero_checksum = false}));
  EXPECT_EQ(packet.common_header().source_port, 5000);
  EXPECT_EQ(packet.common_header().destination_port, 5000);
  EXPECT_EQ(packet.common_header().verification_tag,
            VerificationTag(0x0eddca08u));
  EXPECT_EQ(packet.common_header().checksum, 0x2a81f531u);
}

TEST(SctpPacketTest, SerializeAndDeserializeSingleChunk) {
  SctpPacket::Builder b(kVerificationTag, {});
  InitChunk init(/*initiate_tag=*/VerificationTag(123), /*a_rwnd=*/456,
                 /*nbr_outbound_streams=*/65535,
                 /*nbr_inbound_streams=*/65534, /*initial_tsn=*/TSN(789),
                 /*parameters=*/Parameters());

  b.Add(init);
  std::vector<uint8_t> serialized = b.Build();

  ASSERT_HAS_VALUE_AND_ASSIGN(
      SctpPacket packet, SctpPacket::Parse(serialized, kVerifyChecksumOptions));

  EXPECT_EQ(packet.common_header().verification_tag, kVerificationTag);

  ASSERT_THAT(packet.descriptors(), SizeIs(1));
  EXPECT_EQ(packet.descriptors()[0].type, InitChunk::kType);

  ASSERT_HAS_VALUE_AND_ASSIGN(InitChunk deserialized,
                              InitChunk::Parse(packet.descriptors()[0].data));
  EXPECT_EQ(deserialized.initiate_tag(), VerificationTag(123));
  EXPECT_EQ(deserialized.a_rwnd(), 456u);
  EXPECT_EQ(deserialized.nbr_outbound_streams(), 65535u);
  EXPECT_EQ(deserialized.nbr_inbound_streams(), 65534u);
  EXPECT_EQ(deserialized.initial_tsn(), TSN(789));
}

TEST(SctpPacketTest, SerializeAndDeserializeThreeChunks) {
  SctpPacket::Builder b(kVerificationTag, {});
  b.Add(SackChunk(/*cumulative_tsn_ack=*/TSN(999), /*a_rwnd=*/456,
                  {SackChunk::GapAckBlock(2, 3)},
                  /*duplicate_tsns=*/{TSN(1), TSN(2), TSN(3)}));
  b.Add(DataChunk(TSN(123), StreamID(456), SSN(789), PPID(9090),
                  /*payload=*/{1, 2, 3, 4, 5},
                  /*options=*/{}));
  b.Add(DataChunk(TSN(124), StreamID(654), SSN(987), PPID(909),
                  /*payload=*/{5, 4, 3, 3, 1},
                  /*options=*/{}));

  std::vector<uint8_t> serialized = b.Build();

  ASSERT_HAS_VALUE_AND_ASSIGN(
      SctpPacket packet, SctpPacket::Parse(serialized, kVerifyChecksumOptions));

  EXPECT_EQ(packet.common_header().verification_tag, kVerificationTag);

  ASSERT_THAT(packet.descriptors(), SizeIs(3));
  EXPECT_EQ(packet.descriptors()[0].type, SackChunk::kType);
  EXPECT_EQ(packet.descriptors()[1].type, DataChunk::kType);
  EXPECT_EQ(packet.descriptors()[2].type, DataChunk::kType);

  ASSERT_HAS_VALUE_AND_ASSIGN(SackChunk sack,
                              SackChunk::Parse(packet.descriptors()[0].data));
  EXPECT_EQ(sack.cumulative_tsn_ack(), TSN(999));
  EXPECT_EQ(sack.a_rwnd(), 456u);

  ASSERT_HAS_VALUE_AND_ASSIGN(DataChunk data1,
                              DataChunk::Parse(packet.descriptors()[1].data));
  EXPECT_EQ(data1.tsn(), TSN(123));

  ASSERT_HAS_VALUE_AND_ASSIGN(DataChunk data2,
                              DataChunk::Parse(packet.descriptors()[2].data));
  EXPECT_EQ(data2.tsn(), TSN(124));
}

TEST(SctpPacketTest, ParseAbortWithEmptyCause) {
  SctpPacket::Builder b(kVerificationTag, {});
  b.Add(AbortChunk(
      /*filled_in_verification_tag=*/true,
      Parameters::Builder().Add(UserInitiatedAbortCause("")).Build()));

  ASSERT_HAS_VALUE_AND_ASSIGN(
      SctpPacket packet, SctpPacket::Parse(b.Build(), kVerifyChecksumOptions));

  EXPECT_EQ(packet.common_header().verification_tag, kVerificationTag);

  ASSERT_THAT(packet.descriptors(), SizeIs(1));
  EXPECT_EQ(packet.descriptors()[0].type, AbortChunk::kType);

  ASSERT_HAS_VALUE_AND_ASSIGN(AbortChunk abort,
                              AbortChunk::Parse(packet.descriptors()[0].data));
  ASSERT_HAS_VALUE_AND_ASSIGN(
      UserInitiatedAbortCause cause,
      abort.error_causes().get<UserInitiatedAbortCause>());
  EXPECT_EQ(cause.upper_layer_abort_reason(), "");
}

TEST(SctpPacketTest, DetectPacketWithZeroSizeChunk) {
  uint8_t data[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0a, 0x0a, 0x5c,
                    0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x00};

  EXPECT_FALSE(SctpPacket::Parse(data, kVerifyChecksumOptions).has_value());
}

TEST(SctpPacketTest, ReturnsCorrectSpaceAvailableToStayWithinMTU) {
  DcSctpOptions options;
  options.mtu = 1191;

  SctpPacket::Builder builder(VerificationTag(123), options);

  // Chunks will be padded to an even 4 bytes, so the maximum packet size should
  // be rounded down.
  const size_t kMaxPacketSize = RoundDownTo4(options.mtu);
  EXPECT_EQ(kMaxPacketSize, 1188u);

  const size_t kSctpHeaderSize = 12;
  EXPECT_EQ(builder.bytes_remaining(), kMaxPacketSize - kSctpHeaderSize);
  EXPECT_EQ(builder.bytes_remaining(), 1176u);

  // Add a smaller packet first.
  DataChunk::Options data_options;

  std::vector<uint8_t> payload1(183);
  builder.Add(
      DataChunk(TSN(1), StreamID(1), SSN(0), PPID(53), payload1, data_options));

  size_t chunk1_size = RoundUpTo4(DataChunk::kHeaderSize + payload1.size());
  EXPECT_EQ(builder.bytes_remaining(),
            kMaxPacketSize - kSctpHeaderSize - chunk1_size);
  EXPECT_EQ(builder.bytes_remaining(), 976u);  // Hand-calculated.

  std::vector<uint8_t> payload2(957);
  builder.Add(
      DataChunk(TSN(1), StreamID(1), SSN(0), PPID(53), payload2, data_options));

  size_t chunk2_size = RoundUpTo4(DataChunk::kHeaderSize + payload2.size());
  EXPECT_EQ(builder.bytes_remaining(),
            kMaxPacketSize - kSctpHeaderSize - chunk1_size - chunk2_size);
  EXPECT_EQ(builder.bytes_remaining(), 0u);  // Hand-calculated.
}

TEST(SctpPacketTest, AcceptsZeroSetZeroChecksum) {
  /*
  Stream Control Transmission Protocol, Src Port: 5000 (5000),
    Dst Port: 5000 (5000)
      Source port: 5000
      Destination port: 5000
      Verification tag: 0x0eddca08
      [Association index: 1]
      Checksum: 0x00000000 [unverified]
      [Checksum Status: Unverified]
      SACK chunk (Cumulative TSN: 1426601536, a_rwnd: 131072,
        gaps: 0, duplicate TSNs: 0)
          Chunk type: SACK (3)
          Chunk flags: 0x00
          Chunk length: 16
          Cumulative TSN ACK: 1426601536
          Advertised receiver window credit (a_rwnd): 131072
          Number of gap acknowledgement blocks: 0
          Number of duplicated TSNs: 0
  */

  uint8_t data[] = {0x13, 0x88, 0x13, 0x88, 0x0e, 0xdd, 0xca, 0x08, 0x00, 0x00,
                    0x00, 0x00, 0x03, 0x00, 0x00, 0x10, 0x55, 0x08, 0x36, 0x40,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  ASSERT_HAS_VALUE_AND_ASSIGN(
      SctpPacket packet,
      SctpPacket::Parse(data, {.disable_checksum_verification = false,
                               .enable_zero_checksum = true}));
  EXPECT_EQ(packet.common_header().source_port, 5000);
  EXPECT_EQ(packet.common_header().destination_port, 5000);
  EXPECT_EQ(packet.common_header().verification_tag,
            VerificationTag(0x0eddca08u));
  EXPECT_EQ(packet.common_header().checksum, 0x00000000u);
}

TEST(SctpPacketTest, RejectsNonZeroIncorrectChecksumWhenZeroChecksumIsActive) {
  /*
  Stream Control Transmission Protocol, Src Port: 5000 (5000),
    Dst Port: 5000 (5000)
      Source port: 5000
      Destination port: 5000
      Verification tag: 0x0eddca08
      [Association index: 1]
      Checksum: 0x00000001 [unverified]
      [Checksum Status: Unverified]
      SACK chunk (Cumulative TSN: 1426601536, a_rwnd: 131072,
        gaps: 0, duplicate TSNs: 0)
          Chunk type: SACK (3)
          Chunk flags: 0x00
          Chunk length: 16
          Cumulative TSN ACK: 1426601536
          Advertised receiver window credit (a_rwnd): 131072
          Number of gap acknowledgement blocks: 0
          Number of duplicated TSNs: 0
  */

  uint8_t data[] = {0x13, 0x88, 0x13, 0x88, 0x0e, 0xdd, 0xca, 0x08, 0x01, 0x00,
                    0x00, 0x00, 0x03, 0x00, 0x00, 0x10, 0x55, 0x08, 0x36, 0x40,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  EXPECT_FALSE(SctpPacket::Parse(data, {.disable_checksum_verification = false,
                                        .enable_zero_checksum = true})
                   .has_value());
}

TEST(SctpPacketTest, WritePacketWithCalculatedChecksum) {
  SctpPacket::Builder b(kVerificationTag, {});
  b.Add(SackChunk(/*cumulative_tsn_ack=*/TSN(999), /*a_rwnd=*/456,
                  /*gap_ack_blocks=*/{},
                  /*duplicate_tsns=*/{}));
  EXPECT_THAT(b.Build(),
              ElementsAre(0x13, 0x88, 0x13, 0x88, 0x12, 0x34, 0x56, 0x78,  //
                          0x07, 0xe8, 0x38, 0x77,  // checksum
                          0x03, 0x00, 0x00, 0x10, 0x00, 0x00, 0x03, 0xe7, 0x00,
                          0x00, 0x01, 0xc8, 0x00, 0x00, 0x00, 0x00));
}

TEST(SctpPacketTest, WritePacketWithZeroChecksum) {
  SctpPacket::Builder b(kVerificationTag, {});
  b.Add(SackChunk(/*cumulative_tsn_ack=*/TSN(999), /*a_rwnd=*/456,
                  /*gap_ack_blocks=*/{},
                  /*duplicate_tsns=*/{}));
  EXPECT_THAT(b.Build(/*write_checksum=*/false),
              ElementsAre(0x13, 0x88, 0x13, 0x88, 0x12, 0x34, 0x56, 0x78,  //
                          0x00, 0x00, 0x00, 0x00,  // checksum
                          0x03, 0x00, 0x00, 0x10, 0x00, 0x00, 0x03, 0xe7, 0x00,
                          0x00, 0x01, 0xc8, 0x00, 0x00, 0x00, 0x00));
}

}  // namespace
}  // namespace dcsctp
