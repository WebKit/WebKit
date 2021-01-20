/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/ulpfec_receiver.h"

#include <string.h>

#include <list>
#include <memory>

#include "modules/rtp_rtcp/mocks/mock_recovered_packet_receiver.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/fec_test_helper.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;

using test::fec::AugmentedPacket;
using Packet = ForwardErrorCorrection::Packet;
using test::fec::UlpfecPacketGenerator;

constexpr int kFecPayloadType = 96;
constexpr uint32_t kMediaSsrc = 835424;

class NullRecoveredPacketReceiver : public RecoveredPacketReceiver {
 public:
  void OnRecoveredPacket(const uint8_t* packet, size_t length) override {}
};

}  // namespace

class UlpfecReceiverTest : public ::testing::Test {
 protected:
  UlpfecReceiverTest()
      : fec_(ForwardErrorCorrection::CreateUlpfec(kMediaSsrc)),
        receiver_fec_(UlpfecReceiver::Create(kMediaSsrc,
                                             &recovered_packet_receiver_,
                                             {})),
        packet_generator_(kMediaSsrc) {}

  // Generates |num_fec_packets| FEC packets, given |media_packets|.
  void EncodeFec(const ForwardErrorCorrection::PacketList& media_packets,
                 size_t num_fec_packets,
                 std::list<ForwardErrorCorrection::Packet*>* fec_packets);

  // Generates |num_media_packets| corresponding to a single frame.
  void PacketizeFrame(size_t num_media_packets,
                      size_t frame_offset,
                      std::list<AugmentedPacket*>* augmented_packets,
                      ForwardErrorCorrection::PacketList* packets);

  // Build a media packet using |packet_generator_| and add it
  // to the receiver.
  void BuildAndAddRedMediaPacket(AugmentedPacket* packet,
                                 bool is_recovered = false);

  // Build a FEC packet using |packet_generator_| and add it
  // to the receiver.
  void BuildAndAddRedFecPacket(Packet* packet);

  // Ensure that |recovered_packet_receiver_| will be called correctly
  // and that the recovered packet will be identical to the lost packet.
  void VerifyReconstructedMediaPacket(const AugmentedPacket& packet,
                                      size_t times);

  void InjectGarbagePacketLength(size_t fec_garbage_offset);

  static void SurvivesMaliciousPacket(const uint8_t* data,
                                      size_t length,
                                      uint8_t ulpfec_payload_type);

  MockRecoveredPacketReceiver recovered_packet_receiver_;
  std::unique_ptr<ForwardErrorCorrection> fec_;
  std::unique_ptr<UlpfecReceiver> receiver_fec_;
  UlpfecPacketGenerator packet_generator_;
};

void UlpfecReceiverTest::EncodeFec(
    const ForwardErrorCorrection::PacketList& media_packets,
    size_t num_fec_packets,
    std::list<ForwardErrorCorrection::Packet*>* fec_packets) {
  const uint8_t protection_factor =
      num_fec_packets * 255 / media_packets.size();
  // Unequal protection is turned off, and the number of important
  // packets is thus irrelevant.
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr FecMaskType kFecMaskType = kFecMaskBursty;
  EXPECT_EQ(
      0, fec_->EncodeFec(media_packets, protection_factor, kNumImportantPackets,
                         kUseUnequalProtection, kFecMaskType, fec_packets));
  ASSERT_EQ(num_fec_packets, fec_packets->size());
}

void UlpfecReceiverTest::PacketizeFrame(
    size_t num_media_packets,
    size_t frame_offset,
    std::list<AugmentedPacket*>* augmented_packets,
    ForwardErrorCorrection::PacketList* packets) {
  packet_generator_.NewFrame(num_media_packets);
  for (size_t i = 0; i < num_media_packets; ++i) {
    std::unique_ptr<AugmentedPacket> next_packet(
        packet_generator_.NextPacket(frame_offset + i, kRtpHeaderSize + 10));
    augmented_packets->push_back(next_packet.get());
    packets->push_back(std::move(next_packet));
  }
}

void UlpfecReceiverTest::BuildAndAddRedMediaPacket(AugmentedPacket* packet,
                                                   bool is_recovered) {
  RtpPacketReceived red_packet =
      packet_generator_.BuildMediaRedPacket(*packet, is_recovered);
  EXPECT_TRUE(receiver_fec_->AddReceivedRedPacket(red_packet, kFecPayloadType));
}

void UlpfecReceiverTest::BuildAndAddRedFecPacket(Packet* packet) {
  RtpPacketReceived red_packet =
      packet_generator_.BuildUlpfecRedPacket(*packet);
  EXPECT_TRUE(receiver_fec_->AddReceivedRedPacket(red_packet, kFecPayloadType));
}

void UlpfecReceiverTest::VerifyReconstructedMediaPacket(
    const AugmentedPacket& packet,
    size_t times) {
  // Verify that the content of the reconstructed packet is equal to the
  // content of |packet|, and that the same content is received |times| number
  // of times in a row.
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, packet.data.size()))
      .With(
          Args<0, 1>(ElementsAreArray(packet.data.cdata(), packet.data.size())))
      .Times(times);
}

void UlpfecReceiverTest::InjectGarbagePacketLength(size_t fec_garbage_offset) {
  EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _));

  const size_t kNumFecPackets = 1;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  PacketizeFrame(2, 0, &augmented_media_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets, kNumFecPackets, &fec_packets);
  ByteWriter<uint16_t>::WriteBigEndian(
      &fec_packets.front()->data[fec_garbage_offset], 0x4711);

  // Inject first media packet, then first FEC packet, skipping the second media
  // packet to cause a recovery from the FEC packet.
  BuildAndAddRedMediaPacket(augmented_media_packets.front());
  BuildAndAddRedFecPacket(fec_packets.front());
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());

  FecPacketCounter counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(2U, counter.num_packets);
  EXPECT_EQ(1U, counter.num_fec_packets);
  EXPECT_EQ(0U, counter.num_recovered_packets);
}

void UlpfecReceiverTest::SurvivesMaliciousPacket(const uint8_t* data,
                                                 size_t length,
                                                 uint8_t ulpfec_payload_type) {
  NullRecoveredPacketReceiver null_callback;
  std::unique_ptr<UlpfecReceiver> receiver_fec(
      UlpfecReceiver::Create(kMediaSsrc, &null_callback, {}));

  RtpPacketReceived rtp_packet;
  ASSERT_TRUE(rtp_packet.Parse(data, length));
  receiver_fec->AddReceivedRedPacket(rtp_packet, ulpfec_payload_type);
}

TEST_F(UlpfecReceiverTest, TwoMediaOneFec) {
  constexpr size_t kNumFecPackets = 1u;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  PacketizeFrame(2, 0, &augmented_media_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets, kNumFecPackets, &fec_packets);

  FecPacketCounter counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(0u, counter.num_packets);
  EXPECT_EQ(-1, counter.first_packet_time_ms);

  // Recovery
  auto it = augmented_media_packets.begin();
  BuildAndAddRedMediaPacket(*it);
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(1u, counter.num_packets);
  EXPECT_EQ(0u, counter.num_fec_packets);
  EXPECT_EQ(0u, counter.num_recovered_packets);
  const int64_t first_packet_time_ms = counter.first_packet_time_ms;
  EXPECT_NE(-1, first_packet_time_ms);

  // Drop one media packet.
  auto fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  ++it;
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());

  counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(2u, counter.num_packets);
  EXPECT_EQ(1u, counter.num_fec_packets);
  EXPECT_EQ(1u, counter.num_recovered_packets);
  EXPECT_EQ(first_packet_time_ms, counter.first_packet_time_ms);
}

TEST_F(UlpfecReceiverTest, TwoMediaOneFecNotUsesRecoveredPackets) {
  constexpr size_t kNumFecPackets = 1u;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  PacketizeFrame(2, 0, &augmented_media_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets, kNumFecPackets, &fec_packets);

  FecPacketCounter counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(0u, counter.num_packets);
  EXPECT_EQ(-1, counter.first_packet_time_ms);

  // Recovery
  auto it = augmented_media_packets.begin();
  BuildAndAddRedMediaPacket(*it, /*is_recovered=*/true);
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(1u, counter.num_packets);
  EXPECT_EQ(0u, counter.num_fec_packets);
  EXPECT_EQ(0u, counter.num_recovered_packets);
  const int64_t first_packet_time_ms = counter.first_packet_time_ms;
  EXPECT_NE(-1, first_packet_time_ms);

  // Drop one media packet.
  auto fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  ++it;
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());

  counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(2u, counter.num_packets);
  EXPECT_EQ(1u, counter.num_fec_packets);
  EXPECT_EQ(0u, counter.num_recovered_packets);
  EXPECT_EQ(first_packet_time_ms, counter.first_packet_time_ms);
}

TEST_F(UlpfecReceiverTest, InjectGarbageFecHeaderLengthRecovery) {
  // Byte offset 8 is the 'length recovery' field of the FEC header.
  InjectGarbagePacketLength(8);
}

TEST_F(UlpfecReceiverTest, InjectGarbageFecLevelHeaderProtectionLength) {
  // Byte offset 10 is the 'protection length' field in the first FEC level
  // header.
  InjectGarbagePacketLength(10);
}

TEST_F(UlpfecReceiverTest, TwoMediaTwoFec) {
  const size_t kNumFecPackets = 2;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  PacketizeFrame(2, 0, &augmented_media_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets, kNumFecPackets, &fec_packets);

  // Recovery
  // Drop both media packets.
  auto it = augmented_media_packets.begin();
  auto fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  ++fec_it;
  BuildAndAddRedFecPacket(*fec_it);
  ++it;
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(UlpfecReceiverTest, TwoFramesOneFec) {
  const size_t kNumFecPackets = 1;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  PacketizeFrame(1, 0, &augmented_media_packets, &media_packets);
  PacketizeFrame(1, 1, &augmented_media_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets, kNumFecPackets, &fec_packets);

  // Recovery
  auto it = augmented_media_packets.begin();
  BuildAndAddRedMediaPacket(augmented_media_packets.front());
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  // Drop one media packet.
  BuildAndAddRedFecPacket(fec_packets.front());
  ++it;
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(UlpfecReceiverTest, OneCompleteOneUnrecoverableFrame) {
  const size_t kNumFecPackets = 1;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  PacketizeFrame(1, 0, &augmented_media_packets, &media_packets);
  PacketizeFrame(2, 1, &augmented_media_packets, &media_packets);

  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets, kNumFecPackets, &fec_packets);

  // Recovery
  auto it = augmented_media_packets.begin();
  BuildAndAddRedMediaPacket(*it);  // First frame: one packet.
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  ++it;
  BuildAndAddRedMediaPacket(*it);  // First packet of second frame.
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(UlpfecReceiverTest, MaxFramesOneFec) {
  const size_t kNumFecPackets = 1;
  const size_t kNumMediaPackets = 48;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  for (size_t i = 0; i < kNumMediaPackets; ++i) {
    PacketizeFrame(1, i, &augmented_media_packets, &media_packets);
  }
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets, kNumFecPackets, &fec_packets);

  // Recovery
  auto it = augmented_media_packets.begin();
  ++it;  // Drop first packet.
  for (; it != augmented_media_packets.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    VerifyReconstructedMediaPacket(**it, 1);
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  }
  BuildAndAddRedFecPacket(fec_packets.front());
  it = augmented_media_packets.begin();
  VerifyReconstructedMediaPacket(**it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(UlpfecReceiverTest, TooManyFrames) {
  const size_t kNumFecPackets = 1;
  const size_t kNumMediaPackets = 49;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  for (size_t i = 0; i < kNumMediaPackets; ++i) {
    PacketizeFrame(1, i, &augmented_media_packets, &media_packets);
  }
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EXPECT_EQ(-1, fec_->EncodeFec(media_packets,
                                kNumFecPackets * 255 / kNumMediaPackets, 0,
                                false, kFecMaskBursty, &fec_packets));
}

TEST_F(UlpfecReceiverTest, PacketNotDroppedTooEarly) {
  // 1 frame with 2 media packets and one FEC packet. One media packet missing.
  // Delay the FEC packet.
  Packet* delayed_fec = nullptr;
  const size_t kNumFecPacketsBatch1 = 1;
  const size_t kNumMediaPacketsBatch1 = 2;
  std::list<AugmentedPacket*> augmented_media_packets_batch1;
  ForwardErrorCorrection::PacketList media_packets_batch1;
  PacketizeFrame(kNumMediaPacketsBatch1, 0, &augmented_media_packets_batch1,
                 &media_packets_batch1);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets_batch1, kNumFecPacketsBatch1, &fec_packets);

  BuildAndAddRedMediaPacket(augmented_media_packets_batch1.front());
  EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _)).Times(1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  delayed_fec = fec_packets.front();

  // Fill the FEC decoder. No packets should be dropped.
  const size_t kNumMediaPacketsBatch2 = 46;
  std::list<AugmentedPacket*> augmented_media_packets_batch2;
  ForwardErrorCorrection::PacketList media_packets_batch2;
  for (size_t i = 0; i < kNumMediaPacketsBatch2; ++i) {
    PacketizeFrame(1, i, &augmented_media_packets_batch2,
                   &media_packets_batch2);
  }
  for (auto it = augmented_media_packets_batch2.begin();
       it != augmented_media_packets_batch2.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _)).Times(1);
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  }

  // Add the delayed FEC packet. One packet should be reconstructed.
  BuildAndAddRedFecPacket(delayed_fec);
  EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _)).Times(1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(UlpfecReceiverTest, PacketDroppedWhenTooOld) {
  // 1 frame with 2 media packets and one FEC packet. One media packet missing.
  // Delay the FEC packet.
  Packet* delayed_fec = nullptr;
  const size_t kNumFecPacketsBatch1 = 1;
  const size_t kNumMediaPacketsBatch1 = 2;
  std::list<AugmentedPacket*> augmented_media_packets_batch1;
  ForwardErrorCorrection::PacketList media_packets_batch1;
  PacketizeFrame(kNumMediaPacketsBatch1, 0, &augmented_media_packets_batch1,
                 &media_packets_batch1);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(media_packets_batch1, kNumFecPacketsBatch1, &fec_packets);

  BuildAndAddRedMediaPacket(augmented_media_packets_batch1.front());
  EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _)).Times(1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  delayed_fec = fec_packets.front();

  // Fill the FEC decoder and force the last packet to be dropped.
  const size_t kNumMediaPacketsBatch2 = 48;
  std::list<AugmentedPacket*> augmented_media_packets_batch2;
  ForwardErrorCorrection::PacketList media_packets_batch2;
  for (size_t i = 0; i < kNumMediaPacketsBatch2; ++i) {
    PacketizeFrame(1, i, &augmented_media_packets_batch2,
                   &media_packets_batch2);
  }
  for (auto it = augmented_media_packets_batch2.begin();
       it != augmented_media_packets_batch2.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _)).Times(1);
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  }

  // Add the delayed FEC packet. No packet should be reconstructed since the
  // first media packet of that frame has been dropped due to being too old.
  BuildAndAddRedFecPacket(delayed_fec);
  EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _)).Times(0);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(UlpfecReceiverTest, OldFecPacketDropped) {
  // 49 frames with 2 media packets and one FEC packet. All media packets
  // missing.
  const size_t kNumMediaPackets = 49 * 2;
  std::list<AugmentedPacket*> augmented_media_packets;
  ForwardErrorCorrection::PacketList media_packets;
  for (size_t i = 0; i < kNumMediaPackets / 2; ++i) {
    std::list<AugmentedPacket*> frame_augmented_media_packets;
    ForwardErrorCorrection::PacketList frame_media_packets;
    std::list<ForwardErrorCorrection::Packet*> fec_packets;
    PacketizeFrame(2, 0, &frame_augmented_media_packets, &frame_media_packets);
    EncodeFec(frame_media_packets, 1, &fec_packets);
    for (auto it = fec_packets.begin(); it != fec_packets.end(); ++it) {
      // Only FEC packets inserted. No packets recoverable at this time.
      BuildAndAddRedFecPacket(*it);
      EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _)).Times(0);
      EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
    }
    // Move unique_ptr's to media_packets for lifetime management.
    media_packets.insert(media_packets.end(),
                         std::make_move_iterator(frame_media_packets.begin()),
                         std::make_move_iterator(frame_media_packets.end()));
    augmented_media_packets.insert(augmented_media_packets.end(),
                                   frame_augmented_media_packets.begin(),
                                   frame_augmented_media_packets.end());
  }
  // Insert the oldest media packet. The corresponding FEC packet is too old
  // and should have been dropped. Only the media packet we inserted will be
  // returned.
  BuildAndAddRedMediaPacket(augmented_media_packets.front());
  EXPECT_CALL(recovered_packet_receiver_, OnRecoveredPacket(_, _)).Times(1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(UlpfecReceiverTest, TruncatedPacketWithFBitSet) {
  const uint8_t kTruncatedPacket[] = {0x80, 0x2a, 0x68, 0x71, 0x29, 0xa1, 0x27,
                                      0x3a, 0x29, 0x12, 0x2a, 0x98, 0xe0, 0x29};

  SurvivesMaliciousPacket(kTruncatedPacket, sizeof(kTruncatedPacket), 100);
}

TEST_F(UlpfecReceiverTest,
       TruncatedPacketWithFBitSetEndingAfterFirstRedHeader) {
  const uint8_t kPacket[] = {
      0x89, 0x27, 0x3a, 0x83, 0x27, 0x3a, 0x3a, 0xf3, 0x67, 0xbe, 0x2a,
      0xa9, 0x27, 0x54, 0x3a, 0x3a, 0x2a, 0x67, 0x3a, 0xf3, 0x67, 0xbe,
      0x2a, 0x27, 0xe6, 0xf6, 0x03, 0x3e, 0x29, 0x27, 0x21, 0x27, 0x2a,
      0x29, 0x21, 0x4b, 0x29, 0x3a, 0x28, 0x29, 0xbf, 0x29, 0x2a, 0x26,
      0x29, 0xae, 0x27, 0xa6, 0xf6, 0x00, 0x03, 0x3e};
  SurvivesMaliciousPacket(kPacket, sizeof(kPacket), 100);
}

TEST_F(UlpfecReceiverTest, TruncatedPacketWithoutDataPastFirstBlock) {
  const uint8_t kPacket[] = {
      0x82, 0x38, 0x92, 0x38, 0x92, 0x38, 0xde, 0x2a, 0x11, 0xc8, 0xa3, 0xc4,
      0x82, 0x38, 0x2a, 0x21, 0x2a, 0x28, 0x92, 0x38, 0x92, 0x00, 0x00, 0x0a,
      0x3a, 0xc8, 0xa3, 0x3a, 0x27, 0xc4, 0x2a, 0x21, 0x2a, 0x28};
  SurvivesMaliciousPacket(kPacket, sizeof(kPacket), 100);
}

}  // namespace webrtc
