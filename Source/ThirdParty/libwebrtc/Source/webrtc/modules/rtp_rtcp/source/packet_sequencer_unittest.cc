/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/packet_sequencer.h"

#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr Timestamp kStartTime = Timestamp::Millis(98765);
constexpr uint32_t kMediaSsrc = 123456;
constexpr uint32_t kRtxSsrc = 123457;
constexpr uint8_t kMediaPayloadType = 42;
constexpr uint16_t kMediaStartSequenceNumber = 123;
constexpr uint16_t kRtxStartSequenceNumber = 234;
constexpr uint16_t kDefaultSequenceNumber = 0x1234;
constexpr uint32_t kStartRtpTimestamp = 798;

class PacketSequencerTest : public ::testing::Test {
 public:
  PacketSequencerTest()
      : clock_(kStartTime),
        sequencer_(kMediaSsrc,
                   kRtxSsrc,
                   /*require_marker_before_media_padding=*/true,
                   &clock_) {}

  RtpPacketToSend CreatePacket(RtpPacketMediaType type, uint32_t ssrc) {
    RtpPacketToSend packet(/*extension_manager=*/nullptr);
    packet.set_packet_type(type);
    packet.SetSsrc(ssrc);
    packet.SetSequenceNumber(kDefaultSequenceNumber);
    packet.set_capture_time_ms(clock_.TimeInMilliseconds());
    packet.SetTimestamp(
        kStartRtpTimestamp +
        static_cast<uint32_t>(packet.capture_time_ms() - kStartTime.ms()));
    return packet;
  }

 protected:
  SimulatedClock clock_;
  PacketSequencer sequencer_;
};

TEST_F(PacketSequencerTest, IgnoresMediaSsrcRetransmissions) {
  RtpPacketToSend packet =
      CreatePacket(RtpPacketMediaType::kRetransmission, kMediaSsrc);
  sequencer_.set_media_sequence_number(kMediaStartSequenceNumber);
  sequencer_.Sequence(packet);
  EXPECT_EQ(packet.SequenceNumber(), kDefaultSequenceNumber);
  EXPECT_EQ(sequencer_.media_sequence_number(), kMediaStartSequenceNumber);
}

TEST_F(PacketSequencerTest, SequencesAudio) {
  RtpPacketToSend packet = CreatePacket(RtpPacketMediaType::kAudio, kMediaSsrc);
  sequencer_.set_media_sequence_number(kMediaStartSequenceNumber);
  sequencer_.Sequence(packet);
  EXPECT_EQ(packet.SequenceNumber(), kMediaStartSequenceNumber);
  EXPECT_EQ(sequencer_.media_sequence_number(), kMediaStartSequenceNumber + 1);
}

TEST_F(PacketSequencerTest, SequencesVideo) {
  RtpPacketToSend packet = CreatePacket(RtpPacketMediaType::kVideo, kMediaSsrc);
  sequencer_.set_media_sequence_number(kMediaStartSequenceNumber);
  sequencer_.Sequence(packet);
  EXPECT_EQ(packet.SequenceNumber(), kMediaStartSequenceNumber);
  EXPECT_EQ(sequencer_.media_sequence_number(), kMediaStartSequenceNumber + 1);
}

TEST_F(PacketSequencerTest, SequencesUlpFec) {
  RtpPacketToSend packet =
      CreatePacket(RtpPacketMediaType::kForwardErrorCorrection, kMediaSsrc);
  sequencer_.set_media_sequence_number(kMediaStartSequenceNumber);
  sequencer_.Sequence(packet);
  EXPECT_EQ(packet.SequenceNumber(), kMediaStartSequenceNumber);
  EXPECT_EQ(sequencer_.media_sequence_number(), kMediaStartSequenceNumber + 1);
}

TEST_F(PacketSequencerTest, SequencesRtxRetransmissions) {
  RtpPacketToSend packet =
      CreatePacket(RtpPacketMediaType::kRetransmission, kRtxSsrc);
  sequencer_.set_rtx_sequence_number(kRtxStartSequenceNumber);
  sequencer_.Sequence(packet);
  EXPECT_EQ(packet.SequenceNumber(), kRtxStartSequenceNumber);
  EXPECT_EQ(sequencer_.rtx_sequence_number(), kRtxStartSequenceNumber + 1);
}

TEST_F(PacketSequencerTest, ProhibitsPaddingWithinVideoFrame) {
  // Send a video packet with the marker bit set to false (indicating it is not
  // the last packet of a frame).
  RtpPacketToSend media_packet =
      CreatePacket(RtpPacketMediaType::kVideo, kMediaSsrc);
  media_packet.SetPayloadType(kMediaPayloadType);
  media_packet.SetMarker(false);
  sequencer_.Sequence(media_packet);

  // Sending padding on the media SSRC should not be allowed at this point.
  EXPECT_FALSE(sequencer_.CanSendPaddingOnMediaSsrc());

  // Send a video packet with marker set to true, padding should be allowed
  // again.
  media_packet.SetMarker(true);
  sequencer_.Sequence(media_packet);
  EXPECT_TRUE(sequencer_.CanSendPaddingOnMediaSsrc());
}

TEST_F(PacketSequencerTest, AllowsPaddingAtAnyTimeIfSoConfigured) {
  PacketSequencer packet_sequencer(
      kMediaSsrc, kRtxSsrc,
      /*require_marker_before_media_padding=*/false, &clock_);

  // Send an audio packet with the marker bit set to false.
  RtpPacketToSend media_packet =
      CreatePacket(RtpPacketMediaType::kAudio, kMediaSsrc);
  media_packet.SetPayloadType(kMediaPayloadType);
  media_packet.SetMarker(false);
  packet_sequencer.Sequence(media_packet);

  // Sending padding on the media SSRC should be allowed despite no marker bit.
  EXPECT_TRUE(packet_sequencer.CanSendPaddingOnMediaSsrc());
}

TEST_F(PacketSequencerTest, UpdatesPaddingBasedOnLastMediaPacket) {
  // First send a media packet.
  RtpPacketToSend media_packet =
      CreatePacket(RtpPacketMediaType::kVideo, kMediaSsrc);
  media_packet.SetPayloadType(kMediaPayloadType);
  media_packet.SetMarker(true);
  // Advance time so current time doesn't exactly match timestamp.
  clock_.AdvanceTime(TimeDelta::Millis(5));
  sequencer_.set_media_sequence_number(kMediaStartSequenceNumber);
  sequencer_.Sequence(media_packet);

  // Next send a padding packet and verify the media packet's timestamps and
  // payload type is transferred to the padding packet.
  RtpPacketToSend padding_packet =
      CreatePacket(RtpPacketMediaType::kPadding, kMediaSsrc);
  padding_packet.SetPadding(/*padding_size=*/100);
  sequencer_.Sequence(padding_packet);

  EXPECT_EQ(padding_packet.SequenceNumber(), kMediaStartSequenceNumber + 1);
  EXPECT_EQ(padding_packet.PayloadType(), kMediaPayloadType);
  EXPECT_EQ(padding_packet.Timestamp(), media_packet.Timestamp());
  EXPECT_EQ(padding_packet.capture_time_ms(), media_packet.capture_time_ms());
}

TEST_F(PacketSequencerTest, UpdatesPaddingBasedOnLastRedPacket) {
  // First send a media packet.
  RtpPacketToSend media_packet =
      CreatePacket(RtpPacketMediaType::kVideo, kMediaSsrc);
  media_packet.SetPayloadType(kMediaPayloadType);
  // Simulate a packet with RED encapsulation;
  media_packet.set_is_red(true);
  uint8_t* payload_buffer = media_packet.SetPayloadSize(1);
  payload_buffer[0] = kMediaPayloadType + 1;

  media_packet.SetMarker(true);
  // Advance time so current time doesn't exactly match timestamp.
  clock_.AdvanceTime(TimeDelta::Millis(5));
  sequencer_.set_media_sequence_number(kMediaStartSequenceNumber);
  sequencer_.Sequence(media_packet);

  // Next send a padding packet and verify the media packet's timestamps and
  // payload type is transferred to the padding packet.
  RtpPacketToSend padding_packet =
      CreatePacket(RtpPacketMediaType::kPadding, kMediaSsrc);
  padding_packet.SetPadding(100);
  sequencer_.Sequence(padding_packet);

  EXPECT_EQ(padding_packet.SequenceNumber(), kMediaStartSequenceNumber + 1);
  EXPECT_EQ(padding_packet.PayloadType(), kMediaPayloadType + 1);
  EXPECT_EQ(padding_packet.Timestamp(), media_packet.Timestamp());
  EXPECT_EQ(padding_packet.capture_time_ms(), media_packet.capture_time_ms());
}

TEST_F(PacketSequencerTest, DoesNotUpdateFieldsOnPayloadPadding) {
  // First send a media packet.
  RtpPacketToSend media_packet =
      CreatePacket(RtpPacketMediaType::kVideo, kMediaSsrc);
  media_packet.SetPayloadType(kMediaPayloadType);
  media_packet.SetMarker(true);
  // Advance time so current time doesn't exactly match timestamp.
  clock_.AdvanceTime(TimeDelta::Millis(5));
  sequencer_.set_media_sequence_number(kMediaStartSequenceNumber);
  sequencer_.Sequence(media_packet);

  // Simulate a payload padding packet on the RTX SSRC.
  RtpPacketToSend padding_packet =
      CreatePacket(RtpPacketMediaType::kPadding, kRtxSsrc);
  padding_packet.SetPayloadSize(100);
  padding_packet.SetPayloadType(kMediaPayloadType + 1);
  padding_packet.SetTimestamp(kStartRtpTimestamp + 1);
  padding_packet.set_capture_time_ms(kStartTime.ms() + 1);
  sequencer_.set_rtx_sequence_number(kRtxStartSequenceNumber);
  sequencer_.Sequence(padding_packet);

  // The sequence number should be updated, but timestamps kept.
  EXPECT_EQ(padding_packet.SequenceNumber(), kRtxStartSequenceNumber);
  EXPECT_EQ(padding_packet.PayloadType(), kMediaPayloadType + 1);
  EXPECT_EQ(padding_packet.Timestamp(), kStartRtpTimestamp + 1);
  EXPECT_EQ(padding_packet.capture_time_ms(), kStartTime.ms() + 1);
}

TEST_F(PacketSequencerTest, UpdatesRtxPaddingBasedOnLastMediaPacket) {
  constexpr uint32_t kTimestampTicksPerMs = 90;

  // First send a media packet.
  RtpPacketToSend media_packet =
      CreatePacket(RtpPacketMediaType::kVideo, kMediaSsrc);
  media_packet.SetPayloadType(kMediaPayloadType);
  media_packet.SetMarker(true);
  sequencer_.set_media_sequence_number(kMediaStartSequenceNumber);
  sequencer_.Sequence(media_packet);

  // Advance time, this time delta will be used to interpolate padding
  // timestamps.
  constexpr TimeDelta kTimeDelta = TimeDelta::Millis(10);
  clock_.AdvanceTime(kTimeDelta);

  RtpPacketToSend padding_packet =
      CreatePacket(RtpPacketMediaType::kPadding, kRtxSsrc);
  padding_packet.SetPadding(100);
  padding_packet.SetPayloadType(kMediaPayloadType + 1);
  sequencer_.set_rtx_sequence_number(kRtxStartSequenceNumber);
  sequencer_.Sequence(padding_packet);

  // Assigned RTX sequence number, but payload type unchanged in this case.
  EXPECT_EQ(padding_packet.SequenceNumber(), kRtxStartSequenceNumber);
  EXPECT_EQ(padding_packet.PayloadType(), kMediaPayloadType + 1);
  // Timestamps are offset realtive to last media packet.
  EXPECT_EQ(
      padding_packet.Timestamp(),
      media_packet.Timestamp() + (kTimeDelta.ms() * kTimestampTicksPerMs));
  EXPECT_EQ(padding_packet.capture_time_ms(),
            media_packet.capture_time_ms() + kTimeDelta.ms());
}

}  // namespace
}  // namespace webrtc
