/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/loss_notification.h"

#include "test/gmock.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {

using ::testing::ElementsAreArray;
using ::testing::make_tuple;
using ::webrtc::rtcp::LossNotification;

TEST(RtcpPacketLossNotificationTest, SetWithIllegalValuesFails) {
  constexpr uint16_t kLastDecoded = 0x3c7b;
  constexpr uint16_t kLastReceived = kLastDecoded + 0x7fff + 1;
  constexpr bool kDecodabilityFlag = true;
  LossNotification loss_notification;
  EXPECT_FALSE(
      loss_notification.Set(kLastDecoded, kLastReceived, kDecodabilityFlag));
}

TEST(RtcpPacketLossNotificationTest, SetWithLegalValuesSucceeds) {
  constexpr uint16_t kLastDecoded = 0x3c7b;
  constexpr uint16_t kLastReceived = kLastDecoded + 0x7fff;
  constexpr bool kDecodabilityFlag = true;
  LossNotification loss_notification;
  EXPECT_TRUE(
      loss_notification.Set(kLastDecoded, kLastReceived, kDecodabilityFlag));
}

TEST(RtcpPacketLossNotificationTest, CreateProducesExpectedWireFormat) {
  // Note that (0x6542 >> 1) is used just to make the pattern in kPacket
  // more apparent; there's nothing truly special about the value,
  // it's only an implementation detail that last-received is represented
  // as a delta from last-decoded, and that this delta is shifted before
  // it's put on the wire.
  constexpr uint16_t kLastDecoded = 0x3c7b;
  constexpr uint16_t kLastReceived = kLastDecoded + (0x6542 >> 1);
  constexpr bool kDecodabilityFlag = true;

  const uint8_t kPacket[] = {0x8f, 206,  0x00, 0x04, 0x12, 0x34, 0x56, 0x78,  //
                             0xab, 0xcd, 0xef, 0x01, 'L',  'N',  'T',  'F',   //
                             0x3c, 0x7b, 0x65, 0x43};

  LossNotification loss_notification;
  loss_notification.SetSenderSsrc(0x12345678);
  loss_notification.SetMediaSsrc(0xabcdef01);
  ASSERT_TRUE(
      loss_notification.Set(kLastDecoded, kLastReceived, kDecodabilityFlag));

  rtc::Buffer packet = loss_notification.Build();

  EXPECT_THAT(make_tuple(packet.data(), packet.size()),
              ElementsAreArray(kPacket));
}

TEST(RtcpPacketLossNotificationTest,
     ParseFailsOnTooSmallPacketToBeLossNotification) {
  uint8_t packet[] = {0x8f, 206,  0x00, 0x04, 0x12, 0x34, 0x56, 0x78,  //
                      0xab, 0xcd, 0xef, 0x01, 'L',  'N',  'T',  'F',   //
                      0x3c, 0x7b, 0x65, 0x43};
  size_t packet_length_bytes = sizeof(packet);

  LossNotification loss_notification;

  // First, prove that the failure we're expecting to see happens because of
  // the length, by showing that before the modification to the length,
  // the packet was correctly parsed.
  ASSERT_TRUE(
      test::ParseSinglePacket(packet, packet_length_bytes, &loss_notification));

  // Show that after shaving off a word, the packet is no longer parsable.
  packet[3] -= 1;            // Change the |length| field of the RTCP packet.
  packet_length_bytes -= 4;  // Effectively forget the last 32-bit word.
  EXPECT_FALSE(
      test::ParseSinglePacket(packet, packet_length_bytes, &loss_notification));
}

TEST(RtcpPacketLossNotificationTest,
     ParseFailsWhenUniqueIdentifierIsNotLossNotification) {
  uint8_t packet[] = {0x8f, 206,  0x00, 0x04, 0x12, 0x34, 0x56, 0x78,  //
                      0xab, 0xcd, 0xef, 0x01, 'L',  'N',  'T',  'F',   //
                      0x3c, 0x7b, 0x65, 0x43};

  LossNotification loss_notification;

  // First, prove that the failure we're expecting to see happens because of
  // the identifier, by showing that before the modification to the identifier,
  // the packet was correctly parsed.
  ASSERT_TRUE(test::ParseSinglePacket(packet, &loss_notification));

  // Show that after changing the identifier, the packet is no longer parsable.
  RTC_DCHECK_EQ(packet[12], 'L');
  RTC_DCHECK_EQ(packet[13], 'N');
  RTC_DCHECK_EQ(packet[14], 'T');
  RTC_DCHECK_EQ(packet[15], 'F');
  packet[14] = 'x';
  EXPECT_FALSE(test::ParseSinglePacket(packet, &loss_notification));
}

TEST(RtcpPacketLossNotificationTest,
     ParseLegalLossNotificationMessagesCorrectly) {
  // Note that (0x6542 >> 1) is used just to make the pattern in kPacket
  // more apparent; there's nothing truly special about the value,
  // it's only an implementation detail that last-received is represented
  // as a delta from last-decoded, and that this delta is shifted before
  // it's put on the wire.
  constexpr uint16_t kLastDecoded = 0x3c7b;
  constexpr uint16_t kLastReceived = kLastDecoded + (0x6542 >> 1);
  constexpr bool kDecodabilityFlag = true;

  const uint8_t kPacket[] = {0x8f, 206,  0x00, 0x04, 0x12, 0x34, 0x56, 0x78,  //
                             0xab, 0xcd, 0xef, 0x01, 'L',  'N',  'T',  'F',   //
                             0x3c, 0x7b, 0x65, 0x43};

  LossNotification loss_notification;
  EXPECT_TRUE(test::ParseSinglePacket(kPacket, &loss_notification));

  EXPECT_EQ(loss_notification.sender_ssrc(), 0x12345678u);
  EXPECT_EQ(loss_notification.media_ssrc(), 0xabcdef01u);
  EXPECT_EQ(loss_notification.last_decoded(), kLastDecoded);
  EXPECT_EQ(loss_notification.last_received(), kLastReceived);
  EXPECT_EQ(loss_notification.decodability_flag(), kDecodabilityFlag);
}

}  // namespace webrtc
