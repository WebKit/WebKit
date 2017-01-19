/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sdes.h"

#include "webrtc/test/gtest.h"
#include "webrtc/test/rtcp_packet_parser.h"

using webrtc::rtcp::Sdes;

namespace webrtc {
namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint8_t kPadding = 0;
const uint8_t kTerminatorTag = 0;
const uint8_t kCnameTag = 1;
const uint8_t kNameTag = 2;
const uint8_t kEmailTag = 3;
}  // namespace

TEST(RtcpPacketSdesTest, CreateAndParseWithoutChunks) {
  Sdes sdes;

  rtc::Buffer packet = sdes.Build();
  Sdes parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));

  EXPECT_EQ(0u, parsed.chunks().size());
}

TEST(RtcpPacketSdesTest, CreateAndParseWithOneChunk) {
  const std::string kCname = "alice@host";

  Sdes sdes;
  EXPECT_TRUE(sdes.AddCName(kSenderSsrc, kCname));

  rtc::Buffer packet = sdes.Build();
  Sdes sdes_parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &sdes_parsed));
  const Sdes& parsed = sdes_parsed;  // Ensure accessors are const.

  EXPECT_EQ(1u, parsed.chunks().size());
  EXPECT_EQ(kSenderSsrc, parsed.chunks()[0].ssrc);
  EXPECT_EQ(kCname, parsed.chunks()[0].cname);
}

TEST(RtcpPacketSdesTest, CreateAndParseWithMultipleChunks) {
  Sdes sdes;
  EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 0, "a"));
  EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 1, "ab"));
  EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 2, "abc"));
  EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 3, "abcd"));
  EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 4, "abcde"));
  EXPECT_TRUE(sdes.AddCName(kSenderSsrc + 5, "abcdef"));

  rtc::Buffer packet = sdes.Build();
  Sdes parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));

  EXPECT_EQ(6u, parsed.chunks().size());
  EXPECT_EQ(kSenderSsrc + 5, parsed.chunks()[5].ssrc);
  EXPECT_EQ("abcdef", parsed.chunks()[5].cname);
}

TEST(RtcpPacketSdesTest, CreateWithTooManyChunks) {
  const size_t kMaxChunks = (1 << 5) - 1;
  Sdes sdes;
  for (size_t i = 0; i < kMaxChunks; ++i) {
    uint32_t ssrc = kSenderSsrc + i;
    std::ostringstream oss;
    oss << "cname" << i;
    EXPECT_TRUE(sdes.AddCName(ssrc, oss.str()));
  }
  EXPECT_FALSE(sdes.AddCName(kSenderSsrc + kMaxChunks, "foo"));
}

TEST(RtcpPacketSdesTest, CreateAndParseCnameItemWithEmptyString) {
  Sdes sdes;
  EXPECT_TRUE(sdes.AddCName(kSenderSsrc, ""));

  rtc::Buffer packet = sdes.Build();
  Sdes parsed;
  EXPECT_TRUE(test::ParseSinglePacket(packet, &parsed));

  EXPECT_EQ(1u, parsed.chunks().size());
  EXPECT_EQ(kSenderSsrc, parsed.chunks()[0].ssrc);
  EXPECT_EQ("", parsed.chunks()[0].cname);
}

TEST(RtcpPacketSdesTest, ParseSkipsNonCNameField) {
  const char kName[] = "abc";
  const std::string kCname = "de";
  const uint8_t kValidPacket[] = {0x81,  202, 0x00, 0x04,
                                  0x12, 0x34, 0x56, 0x78,
                                  kNameTag,  3, kName[0],  kName[1], kName[2],
                                  kCnameTag, 2, kCname[0], kCname[1],
                                  kTerminatorTag, kPadding, kPadding};
  // Sanity checks packet was assembled correctly.
  ASSERT_EQ(0u, sizeof(kValidPacket) % 4);
  ASSERT_EQ(kValidPacket[3] + 1u, sizeof(kValidPacket) / 4);

  Sdes parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kValidPacket, &parsed));

  EXPECT_EQ(1u, parsed.chunks().size());
  EXPECT_EQ(kSenderSsrc, parsed.chunks()[0].ssrc);
  EXPECT_EQ(kCname, parsed.chunks()[0].cname);
}

TEST(RtcpPacketSdesTest, ParseSkipsChunksWithoutCName) {
  const char kName[] = "ab";
  const char kEmail[] = "de";
  const std::string kCname = "def";
  const uint8_t kPacket[] = {0x82,  202, 0x00, 0x07,
      0x12, 0x34, 0x56, 0x78,  // 1st chunk.
      kNameTag,  3, kName[0],  kName[1], kName[2],
      kEmailTag, 2, kEmail[0], kEmail[1],
      kTerminatorTag, kPadding, kPadding,
      0x23, 0x45, 0x67, 0x89,  // 2nd chunk.
      kCnameTag, 3, kCname[0], kCname[1], kCname[2],
      kTerminatorTag, kPadding, kPadding};
  // Sanity checks packet was assembled correctly.
  ASSERT_EQ(0u, sizeof(kPacket) % 4);
  ASSERT_EQ(kPacket[3] + 1u, sizeof(kPacket) / 4);

  Sdes parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &parsed));
  ASSERT_EQ(1u, parsed.chunks().size());
  EXPECT_EQ(0x23456789u, parsed.chunks()[0].ssrc);
  EXPECT_EQ(kCname, parsed.chunks()[0].cname);
}

TEST(RtcpPacketSdesTest, ParseFailsWithoutChunkItemTerminator) {
  const char kName[] = "abc";
  const char kCname[] = "d";
  // No place for next chunk item.
  const uint8_t kInvalidPacket[] = {0x81,  202, 0x00, 0x03,
                                    0x12, 0x34, 0x56, 0x78,
                                    kNameTag,  3, kName[0], kName[1], kName[2],
                                    kCnameTag, 1, kCname[0]};
  // Sanity checks packet was assembled correctly.
  ASSERT_EQ(0u, sizeof(kInvalidPacket) % 4);
  ASSERT_EQ(kInvalidPacket[3] + 1u, sizeof(kInvalidPacket) / 4);

  Sdes parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kInvalidPacket, &parsed));
}

TEST(RtcpPacketSdesTest, ParseFailsWithDamagedChunkItem) {
  const char kName[] = "ab";
  const char kCname[] = "d";
  // Next chunk item has non-terminator type, but not the size.
  const uint8_t kInvalidPacket[] = {0x81,  202, 0x00, 0x03,
                                    0x12, 0x34, 0x56, 0x78,
                                    kNameTag,  2, kName[0], kName[1],
                                    kCnameTag, 1, kCname[0],
                                    kEmailTag};
  // Sanity checks packet was assembled correctly.
  ASSERT_EQ(0u, sizeof(kInvalidPacket) % 4);
  ASSERT_EQ(kInvalidPacket[3] + 1u, sizeof(kInvalidPacket) / 4);

  Sdes parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kInvalidPacket, &parsed));
}

TEST(RtcpPacketSdesTest, ParseFailsWithTooLongChunkItem) {
  const char kName[] = "abc";
  const char kCname[] = "d";
  // Last chunk item has length that goes beyond the buffer end.
  const uint8_t kInvalidPacket[] = {0x81,  202, 0x00, 0x03,
                                    0x12, 0x34, 0x56, 0x78,
                                    kNameTag,  3, kName[0], kName[1], kName[2],
                                    kCnameTag, 2, kCname[0]};
  // Sanity checks packet was assembled correctly.
  ASSERT_EQ(0u, sizeof(kInvalidPacket) % 4);
  ASSERT_EQ(kInvalidPacket[3] + 1u, sizeof(kInvalidPacket) / 4);

  Sdes parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kInvalidPacket, &parsed));
}

TEST(RtcpPacketSdesTest, ParseFailsWithTwoCNames) {
  const char kCname1[] = "a";
  const char kCname2[] = "de";
  const uint8_t kInvalidPacket[] = {0x81,  202, 0x00, 0x03,
                                    0x12, 0x34, 0x56, 0x78,
                                    kCnameTag, 1, kCname1[0],
                                    kCnameTag, 2, kCname2[0], kCname2[1],
                                    kTerminatorTag};
  // Sanity checks packet was assembled correctly.
  ASSERT_EQ(0u, sizeof(kInvalidPacket) % 4);
  ASSERT_EQ(kInvalidPacket[3] + 1u, sizeof(kInvalidPacket) / 4);

  Sdes parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kInvalidPacket, &parsed));
}

TEST(RtcpPacketSdesTest, ParseFailsWithTooLittleSpaceForNextChunk) {
  const char kCname[] = "a";
  const char kEmail[] = "de";
  // Two chunks are promised in the header, but no place for the second chunk.
  const uint8_t kInvalidPacket[] = {0x82,  202, 0x00, 0x04,
                                    0x12, 0x34, 0x56, 0x78,  // 1st chunk.
                                    kCnameTag, 1, kCname[0],
                                    kEmailTag, 2, kEmail[0], kEmail[1],
                                    kTerminatorTag,
                                    0x23, 0x45, 0x67, 0x89};  // 2nd chunk.
  // Sanity checks packet was assembled correctly.
  ASSERT_EQ(0u, sizeof(kInvalidPacket) % 4);
  ASSERT_EQ(kInvalidPacket[3] + 1u, sizeof(kInvalidPacket) / 4);

  Sdes parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kInvalidPacket, &parsed));
}

TEST(RtcpPacketSdesTest, ParsedSdesCanBeReusedForBuilding) {
  Sdes source;
  const std::string kAlice = "alice@host";
  const std::string kBob = "bob@host";
  source.AddCName(kSenderSsrc, kAlice);

  rtc::Buffer packet1 = source.Build();
  Sdes middle;
  test::ParseSinglePacket(packet1, &middle);

  EXPECT_EQ(source.BlockLength(), middle.BlockLength());

  middle.AddCName(kSenderSsrc + 1, kBob);

  rtc::Buffer packet2 = middle.Build();
  Sdes destination;
  test::ParseSinglePacket(packet2, &destination);

  EXPECT_EQ(middle.BlockLength(), destination.BlockLength());

  EXPECT_EQ(2u, destination.chunks().size());
  EXPECT_EQ(kSenderSsrc, destination.chunks()[0].ssrc);
  EXPECT_EQ(kAlice, destination.chunks()[0].cname);
  EXPECT_EQ(kSenderSsrc + 1, destination.chunks()[1].ssrc);
  EXPECT_EQ(kBob, destination.chunks()[1].cname);
}
}  // namespace webrtc
