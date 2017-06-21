/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

#include "webrtc/modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;
using ::testing::make_tuple;

const int8_t kPayloadType = 100;
const uint32_t kSsrc = 0x12345678;
const uint16_t kSeqNum = 88;
const uint32_t kTimestamp = 0x65431278;

}  // namespace

TEST(RtpHeaderParser, ParseMinimum) {
  // clang-format off
  const uint8_t kPacket[] = {
    0x80, kPayloadType, 0x00, kSeqNum,
    0x65, 0x43, 0x12, 0x78,   // kTimestamp.
    0x12, 0x34, 0x56, 0x78};  // kSsrc.
  // clang-format on
  RtpUtility::RtpHeaderParser parser(kPacket, sizeof(kPacket));
  RTPHeader header;

  EXPECT_TRUE(parser.Parse(&header, nullptr));

  EXPECT_EQ(kPayloadType, header.payloadType);
  EXPECT_EQ(kSeqNum, header.sequenceNumber);
  EXPECT_EQ(kTimestamp, header.timestamp);
  EXPECT_EQ(kSsrc, header.ssrc);
  EXPECT_EQ(0u, header.paddingLength);
  EXPECT_EQ(sizeof(kPacket), header.headerLength);
}

TEST(RtpHeaderParser, ParseWithExtension) {
  // clang-format off
  const uint8_t kPacket[] = {
    0x90, kPayloadType, 0x00, kSeqNum,
    0x65, 0x43, 0x12, 0x78,  // kTimestamp.
    0x12, 0x34, 0x56, 0x78,  // kSsrc.
    0xbe, 0xde, 0x00, 0x01,  // Extension block of size 1 x 32bit words.
    0x12, 0x01, 0x56, 0xce};
  // clang-format on
  RtpHeaderExtensionMap extensions;
  extensions.Register<TransmissionOffset>(1);
  RtpUtility::RtpHeaderParser parser(kPacket, sizeof(kPacket));
  RTPHeader header;

  EXPECT_TRUE(parser.Parse(&header, &extensions));

  EXPECT_EQ(kPayloadType, header.payloadType);
  EXPECT_EQ(kSeqNum, header.sequenceNumber);
  EXPECT_EQ(kTimestamp, header.timestamp);
  EXPECT_EQ(kSsrc, header.ssrc);

  ASSERT_TRUE(header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(0x156ce, header.extension.transmissionTimeOffset);
}

TEST(RtpHeaderParser, ParseWithInvalidSizedExtension) {
  const size_t kPayloadSize = 7;
  // clang-format off
  const uint8_t kPacket[] = {
      0x90, kPayloadType, 0x00, kSeqNum,
      0x65, 0x43, 0x12, 0x78,  // kTimestamp.
      0x12, 0x34, 0x56, 0x78,  // kSsrc.
      0xbe, 0xde, 0x00, 0x02,  // Extension block of size 2 x 32bit words.
      0x16,  // (6+1)-bytes, but Transmission Offset expected to be 3-bytes.
             'e',  'x',  't',
       'd',  'a',  't',  'a',
       'p',  'a',  'y',  'l',  'o',  'a',  'd'
  };
  // clang-format on
  RtpHeaderExtensionMap extensions;
  extensions.Register<TransmissionOffset>(1);
  RtpUtility::RtpHeaderParser parser(kPacket, sizeof(kPacket));
  RTPHeader header;

  EXPECT_TRUE(parser.Parse(&header, &extensions));

  // Extension should be ignored.
  EXPECT_FALSE(header.extension.hasTransmissionTimeOffset);
  // But shouldn't prevent reading payload.
  EXPECT_THAT(sizeof(kPacket) - kPayloadSize, header.headerLength);
}

TEST(RtpHeaderParser, ParseWithExtensionPadding) {
  // clang-format off
  const uint8_t kPacket[] = {
      0x90, kPayloadType, 0x00, kSeqNum,
      0x65, 0x43, 0x12, 0x78,  // kTimestamp.
      0x12, 0x34, 0x56, 0x78,  // kSsrc.
      0xbe, 0xde, 0x00, 0x02,  // Extension of size 1x32bit word.
      0x02,  // A byte of (invalid) padding.
      0x12, 0x1a, 0xda, 0x03,  // TransmissionOffset extension.
      0x0f, 0x00, 0x03,  // More invalid padding bytes: id=0, but len > 0.
  };
  // clang-format on
  RtpHeaderExtensionMap extensions;
  extensions.Register<TransmissionOffset>(1);
  RtpUtility::RtpHeaderParser parser(kPacket, sizeof(kPacket));
  RTPHeader header;

  EXPECT_TRUE(parser.Parse(&header, &extensions));

  // Parse should skip padding and read extension.
  EXPECT_TRUE(header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(0x1ada03, header.extension.transmissionTimeOffset);
  EXPECT_EQ(sizeof(kPacket), header.headerLength);
}

TEST(RtpHeaderParser, ParseWithOverSizedExtension) {
  // clang-format off
  const uint8_t kPacket[] = {
      0x90, kPayloadType, 0x00, kSeqNum,
      0x65, 0x43, 0x12, 0x78,  // kTimestamp.
      0x12, 0x34, 0x56, 0x78,  // kSsrc.
      0xbe, 0xde, 0x00, 0x01,  // Extension of size 1x32bit word.
      0x00,  // Add a byte of padding.
            0x12,  // Extension id 1 size (2+1).
                  0xda, 0x1a  // Only 2 bytes of extension payload.
  };
  // clang-format on
  RtpHeaderExtensionMap extensions;
  extensions.Register<TransmissionOffset>(1);
  RtpUtility::RtpHeaderParser parser(kPacket, sizeof(kPacket));
  RTPHeader header;

  EXPECT_TRUE(parser.Parse(&header, &extensions));

  // Parse should ignore extension.
  EXPECT_FALSE(header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(sizeof(kPacket), header.headerLength);
}

TEST(RtpHeaderParser, ParseAll8Extensions) {
  const uint8_t kAudioLevel = 0x5a;
  // clang-format off
  const uint8_t kPacket[] = {
      0x90, kPayloadType, 0x00, kSeqNum,
      0x65, 0x43, 0x12, 0x78,  // kTimestamp.
      0x12, 0x34, 0x56, 0x78,  // kSsrc.
      0xbe, 0xde, 0x00, 0x08,  // Extension of size 8x32bit words.
      0x40, 0x80|kAudioLevel,  // AudioLevel.
      0x22, 0x01, 0x56, 0xce,  // TransmissionOffset.
      0x62, 0x12, 0x34, 0x56,  // AbsoluteSendTime.
      0x81, 0xce, 0xab,        // TransportSequenceNumber.
      0xa0, 0x03,              // VideoRotation.
      0xb2, 0x12, 0x48, 0x76,  // PlayoutDelayLimits.
      0xc2, 'r', 't', 'x',     // RtpStreamId
      0xd5, 's', 't', 'r', 'e', 'a', 'm',  // RepairedRtpStreamId
      0x00, 0x00,              // Padding to 32bit boundary.
  };
  // clang-format on
  ASSERT_EQ(sizeof(kPacket) % 4, 0u);

  RtpHeaderExtensionMap extensions;
  extensions.Register<TransmissionOffset>(2);
  extensions.Register<AudioLevel>(4);
  extensions.Register<AbsoluteSendTime>(6);
  extensions.Register<TransportSequenceNumber>(8);
  extensions.Register<VideoOrientation>(0xa);
  extensions.Register<PlayoutDelayLimits>(0xb);
  extensions.Register<RtpStreamId>(0xc);
  extensions.Register<RepairedRtpStreamId>(0xd);
  RtpUtility::RtpHeaderParser parser(kPacket, sizeof(kPacket));
  RTPHeader header;

  EXPECT_TRUE(parser.Parse(&header, &extensions));

  EXPECT_TRUE(header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(0x156ce, header.extension.transmissionTimeOffset);

  EXPECT_TRUE(header.extension.hasAudioLevel);
  EXPECT_TRUE(header.extension.voiceActivity);
  EXPECT_EQ(kAudioLevel, header.extension.audioLevel);

  EXPECT_TRUE(header.extension.hasAbsoluteSendTime);
  EXPECT_EQ(0x123456U, header.extension.absoluteSendTime);

  EXPECT_TRUE(header.extension.hasTransportSequenceNumber);
  EXPECT_EQ(0xceab, header.extension.transportSequenceNumber);

  EXPECT_TRUE(header.extension.hasVideoRotation);
  EXPECT_EQ(kVideoRotation_270, header.extension.videoRotation);

  EXPECT_EQ(0x124 * PlayoutDelayLimits::kGranularityMs,
            header.extension.playout_delay.min_ms);
  EXPECT_EQ(0x876 * PlayoutDelayLimits::kGranularityMs,
            header.extension.playout_delay.max_ms);
  EXPECT_EQ(header.extension.stream_id, StreamId("rtx"));
  EXPECT_EQ(header.extension.repaired_stream_id, StreamId("stream"));
}

TEST(RtpHeaderParser, ParseMalformedRsidExtensions) {
  // clang-format off
  const uint8_t kPacket[] = {
      0x90, kPayloadType, 0x00, kSeqNum,
      0x65, 0x43, 0x12, 0x78,  // kTimestamp.
      0x12, 0x34, 0x56, 0x78,  // kSsrc.
      0xbe, 0xde, 0x00, 0x03,  // Extension of size 3x32bit words.
      0xc2, '\0', 't', 'x',    // empty RtpStreamId
      0xd5, 's', 't', 'r', '\0', 'a', 'm',  // RepairedRtpStreamId
      0x00,                    // Padding to 32bit boundary.
  };
  // clang-format on
  ASSERT_EQ(sizeof(kPacket) % 4, 0u);

  RtpHeaderExtensionMap extensions;
  extensions.Register<RtpStreamId>(0xc);
  extensions.Register<RepairedRtpStreamId>(0xd);
  RtpUtility::RtpHeaderParser parser(kPacket, sizeof(kPacket));
  RTPHeader header;

  EXPECT_TRUE(parser.Parse(&header, &extensions));
  EXPECT_TRUE(header.extension.stream_id.empty());
  EXPECT_EQ(header.extension.repaired_stream_id, StreamId("str"));
}

TEST(RtpHeaderParser, ParseWithCsrcsExtensionAndPadding) {
  const uint8_t kPacketPaddingSize = 8;
  const uint32_t kCsrcs[] = {0x34567890, 0x32435465};
  const size_t kPayloadSize = 7;
  // clang-format off
  const uint8_t kPacket[] = {
    0xb2, kPayloadType, 0x00, kSeqNum,
    0x65, 0x43, 0x12, 0x78,  // kTimestamp.
    0x12, 0x34, 0x56, 0x78,  // kSsrc.
    0x34, 0x56, 0x78, 0x90,  // kCsrcs[0].
    0x32, 0x43, 0x54, 0x65,  // kCsrcs[1].
    0xbe, 0xde, 0x00, 0x01,  // Extension.
    0x12, 0x00, 0x56, 0xce,  // TransmissionTimeOffset with id = 1.
    'p', 'a', 'y', 'l', 'o', 'a', 'd',
    'p', 'a', 'd', 'd', 'i', 'n', 'g', kPacketPaddingSize};
  // clang-format on
  RtpHeaderExtensionMap extensions;
  extensions.Register<TransmissionOffset>(1);
  RtpUtility::RtpHeaderParser parser(kPacket, sizeof(kPacket));
  RTPHeader header;

  EXPECT_TRUE(parser.Parse(&header, &extensions));

  EXPECT_EQ(kPayloadType, header.payloadType);
  EXPECT_EQ(kSeqNum, header.sequenceNumber);
  EXPECT_EQ(kTimestamp, header.timestamp);
  EXPECT_EQ(kSsrc, header.ssrc);
  EXPECT_THAT(make_tuple(header.arrOfCSRCs, header.numCSRCs),
              ElementsAreArray(kCsrcs));
  EXPECT_EQ(kPacketPaddingSize, header.paddingLength);
  EXPECT_THAT(sizeof(kPacket) - kPayloadSize - kPacketPaddingSize,
              header.headerLength);
  EXPECT_TRUE(header.extension.hasTransmissionTimeOffset);
  EXPECT_EQ(0x56ce, header.extension.transmissionTimeOffset);
}

}  // namespace webrtc
