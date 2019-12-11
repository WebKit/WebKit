/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/app.h"

#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::make_tuple;
using ::webrtc::rtcp::App;

constexpr uint32_t kName = ((uint32_t)'n' << 24) | ((uint32_t)'a' << 16) |
                           ((uint32_t)'m' << 8) | (uint32_t)'e';
constexpr uint8_t kSubtype = 0x1e;
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint8_t kData[] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
constexpr uint8_t kVersionBits = 2 << 6;
constexpr uint8_t kPaddingBit = 1 << 5;
// clang-format off
constexpr uint8_t kPacketWithoutData[] = {
    kVersionBits | kSubtype, App::kPacketType, 0x00, 0x02,
    0x12, 0x34, 0x56, 0x78,
    'n',  'a',  'm',  'e'};
constexpr uint8_t kPacketWithData[] = {
    kVersionBits | kSubtype, App::kPacketType, 0x00, 0x04,
    0x12, 0x34, 0x56, 0x78,
    'n',  'a',  'm',  'e',
    't',  'e',  's',  't',
    'd',  'a',  't',  'a'};
constexpr uint8_t kTooSmallPacket[] = {
    kVersionBits | kSubtype, App::kPacketType, 0x00, 0x01,
    0x12, 0x34, 0x56, 0x78};
constexpr uint8_t kPaddingSize = 1;
constexpr uint8_t kPacketWithUnalignedPayload[] = {
    kVersionBits | kPaddingBit | kSubtype, App::kPacketType, 0x00, 0x03,
    0x12, 0x34, 0x56, 0x78,
     'n',  'a',  'm',  'e',
     'd',  'a',  't', kPaddingSize};
// clang-format on
}  // namespace

TEST(RtcpPacketAppTest, CreateWithoutData) {
  App app;
  app.SetSsrc(kSenderSsrc);
  app.SetSubType(kSubtype);
  app.SetName(kName);

  rtc::Buffer raw = app.Build();

  EXPECT_THAT(make_tuple(raw.data(), raw.size()),
              ElementsAreArray(kPacketWithoutData));
}

TEST(RtcpPacketAppTest, ParseWithoutData) {
  App parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacketWithoutData, &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.ssrc());
  EXPECT_EQ(kSubtype, parsed.sub_type());
  EXPECT_EQ(kName, parsed.name());
  EXPECT_EQ(0u, parsed.data_size());
}

TEST(RtcpPacketAppTest, CreateWithData) {
  App app;
  app.SetSsrc(kSenderSsrc);
  app.SetSubType(kSubtype);
  app.SetName(kName);
  app.SetData(kData, sizeof(kData));

  rtc::Buffer raw = app.Build();

  EXPECT_THAT(make_tuple(raw.data(), raw.size()),
              ElementsAreArray(kPacketWithData));
}

TEST(RtcpPacketAppTest, ParseWithData) {
  App parsed;
  EXPECT_TRUE(test::ParseSinglePacket(kPacketWithData, &parsed));

  EXPECT_EQ(kSenderSsrc, parsed.ssrc());
  EXPECT_EQ(kSubtype, parsed.sub_type());
  EXPECT_EQ(kName, parsed.name());
  EXPECT_THAT(make_tuple(parsed.data(), parsed.data_size()),
              ElementsAreArray(kData));
}

TEST(RtcpPacketAppTest, ParseFailsOnTooSmallPacket) {
  App parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kTooSmallPacket, &parsed));
}

TEST(RtcpPacketAppTest, ParseFailsOnUnalignedPayload) {
  App parsed;
  EXPECT_FALSE(test::ParseSinglePacket(kPacketWithUnalignedPayload, &parsed));
}

}  // namespace webrtc
