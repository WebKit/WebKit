/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/basictypes.h"
#include "webrtc/call/flexfec_receive_stream.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_recovered_packet_receiver.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

TEST(FlexfecReceiveStreamTest, ConstructDestruct) {
  FlexfecReceiveStream::Config config;
  config.flexfec_payload_type = 118;
  config.flexfec_ssrc = 424223;
  config.protected_media_ssrcs = {912512};
  MockRecoveredPacketReceiver callback;

  internal::FlexfecReceiveStream receive_stream(config, &callback);
}

TEST(FlexfecReceiveStreamTest, StartStop) {
  FlexfecReceiveStream::Config config;
  config.flexfec_payload_type = 118;
  config.flexfec_ssrc = 1652392;
  config.protected_media_ssrcs = {23300443};
  MockRecoveredPacketReceiver callback;
  internal::FlexfecReceiveStream receive_stream(config, &callback);

  receive_stream.Start();
  receive_stream.Stop();
}

TEST(FlexfecReceiveStreamTest, DoesNotProcessPacketWhenNoMediaSsrcGiven) {
  FlexfecReceiveStream::Config config;
  config.flexfec_payload_type = 118;
  config.flexfec_ssrc = 424223;
  config.protected_media_ssrcs = {};
  MockRecoveredPacketReceiver callback;
  internal::FlexfecReceiveStream receive_stream(config, &callback);
  const uint8_t packet[] = {0x00, 0x11, 0x22, 0x33};
  const size_t packet_length = sizeof(packet);

  EXPECT_FALSE(
      receive_stream.AddAndProcessReceivedPacket(packet, packet_length));
}

// Create a FlexFEC packet that protects a single media packet and ensure
// that the callback is called. Correctness of recovery is checked in the
// FlexfecReceiver unit tests.
TEST(FlexfecReceiveStreamTest, RecoversPacketWhenStarted) {
  constexpr uint8_t kFlexfecPlType = 118;
  constexpr uint8_t kFlexfecSeqNum[] = {0x00, 0x01};
  constexpr uint8_t kFlexfecTs[] = {0x00, 0x11, 0x22, 0x33};
  constexpr uint8_t kFlexfecSsrc[] = {0x00, 0x00, 0x00, 0x01};
  constexpr uint8_t kMediaPlType = 107;
  constexpr uint8_t kMediaSeqNum[] = {0x00, 0x02};
  constexpr uint8_t kMediaTs[] = {0xaa, 0xbb, 0xcc, 0xdd};
  constexpr uint8_t kMediaSsrc[] = {0x00, 0x00, 0x00, 0x02};

  // This packet mask protects a single media packet, i.e., the FlexFEC payload
  // is a copy of that media packet. When inserted in the FlexFEC pipeline,
  // it will thus trivially recover the lost media packet.
  constexpr uint8_t kKBit0 = 1 << 7;
  constexpr uint8_t kFlexfecPktMask[] = {kKBit0 | 0x00, 0x01};
  constexpr uint8_t kPayloadLength[] = {0x00, 0x04};
  constexpr uint8_t kSsrcCount = 1;
  constexpr uint8_t kReservedBits = 0x00;
  constexpr uint8_t kPayloadBits = 0x00;
  // clang-format off
  constexpr uint8_t kFlexfecPacket[] = {
      // RTP header.
      0x80,            kFlexfecPlType,  kFlexfecSeqNum[0],  kFlexfecSeqNum[1],
      kFlexfecTs[0],   kFlexfecTs[1],   kFlexfecTs[2],      kFlexfecTs[3],
      kFlexfecSsrc[0], kFlexfecSsrc[1], kFlexfecSsrc[2],    kFlexfecSsrc[3],
      // FlexFEC header.
      0x00,            kMediaPlType,    kPayloadLength[0],  kPayloadLength[1],
      kMediaTs[0],     kMediaTs[1],     kMediaTs[2],        kMediaTs[3],
      kSsrcCount,      kReservedBits,   kReservedBits,      kReservedBits,
      kMediaSsrc[0],   kMediaSsrc[1],   kMediaSsrc[2],      kMediaSsrc[3],
      kMediaSeqNum[0], kMediaSeqNum[1], kFlexfecPktMask[0], kFlexfecPktMask[1],
      // FEC payload.
      kPayloadBits,    kPayloadBits,    kPayloadBits,       kPayloadBits};
  // clang-format on
  constexpr size_t kFlexfecPacketLength = sizeof(kFlexfecPacket);

  FlexfecReceiveStream::Config config;
  config.flexfec_payload_type = kFlexfecPlType;
  config.flexfec_ssrc = ByteReader<uint32_t>::ReadBigEndian(kFlexfecSsrc);
  config.protected_media_ssrcs = {
      ByteReader<uint32_t>::ReadBigEndian(kMediaSsrc)};
  testing::StrictMock<MockRecoveredPacketReceiver> recovered_packet_receiver;
  internal::FlexfecReceiveStream receive_stream(config,
                                                &recovered_packet_receiver);

  // Do not call back before being started.
  receive_stream.AddAndProcessReceivedPacket(kFlexfecPacket,
                                             kFlexfecPacketLength);

  // Call back after being started.
  receive_stream.Start();
  EXPECT_CALL(
      recovered_packet_receiver,
      OnRecoveredPacket(::testing::_, kRtpHeaderSize + kPayloadLength[1]));
  receive_stream.AddAndProcessReceivedPacket(kFlexfecPacket,
                                             kFlexfecPacketLength);
}

}  // namespace webrtc
