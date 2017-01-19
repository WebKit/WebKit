/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <list>
#include <memory>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/fec_test_helper.h"
#include "webrtc/modules/rtp_rtcp/source/flexfec_header_reader_writer.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/modules/rtp_rtcp/source/ulpfec_header_reader_writer.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

// Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
constexpr size_t kTransportOverhead = 28;

constexpr uint32_t kMediaSsrc = 83542;
constexpr uint32_t kFlexfecSsrc = 43245;

// Deep copies |src| to |dst|, but only keeps every Nth packet.
void DeepCopyEveryNthPacket(const ForwardErrorCorrection::PacketList& src,
                            int n,
                            ForwardErrorCorrection::PacketList* dst) {
  RTC_DCHECK_GT(n, 0);
  int i = 0;
  for (const auto& packet : src) {
    if (i % n == 0) {
      dst->emplace_back(new ForwardErrorCorrection::Packet(*packet));
    }
    ++i;
  }
}

}  // namespace

using ::testing::Types;

template <typename ForwardErrorCorrectionType>
class RtpFecTest : public ::testing::Test {
 protected:
  RtpFecTest()
      : random_(0xabcdef123456),
        media_packet_generator_(
            kRtpHeaderSize,  // Minimum packet size.
            IP_PACKET_SIZE - kRtpHeaderSize - kTransportOverhead -
                fec_.MaxPacketOverhead(),  // Maximum packet size.
            kMediaSsrc,
            &random_) {}

  // Construct |received_packets_|: a subset of the media and FEC packets.
  //
  // Media packet "i" is lost if media_loss_mask_[i] = 1, received if
  // media_loss_mask_[i] = 0.
  // FEC packet "i" is lost if fec_loss_mask_[i] = 1, received if
  // fec_loss_mask_[i] = 0.
  void NetworkReceivedPackets(int* media_loss_mask, int* fec_loss_mask);

  // Add packet from |packet_list| to list of received packets, using the
  // |loss_mask|.
  // The |packet_list| may be a media packet list (is_fec = false), or a
  // FEC packet list (is_fec = true).
  template <typename T>
  void ReceivedPackets(const T& packet_list, int* loss_mask, bool is_fec);

  // Check for complete recovery after FEC decoding.
  bool IsRecoveryComplete();

  ForwardErrorCorrectionType fec_;

  Random random_;
  test::fec::MediaPacketGenerator media_packet_generator_;

  ForwardErrorCorrection::PacketList media_packets_;
  std::list<ForwardErrorCorrection::Packet*> generated_fec_packets_;
  ForwardErrorCorrection::ReceivedPacketList received_packets_;
  ForwardErrorCorrection::RecoveredPacketList recovered_packets_;

  int media_loss_mask_[kUlpfecMaxMediaPackets];
  int fec_loss_mask_[kUlpfecMaxMediaPackets];
};

template <typename ForwardErrorCorrectionType>
void RtpFecTest<ForwardErrorCorrectionType>::NetworkReceivedPackets(
    int* media_loss_mask,
    int* fec_loss_mask) {
  constexpr bool kFecPacket = true;
  ReceivedPackets(media_packets_, media_loss_mask, !kFecPacket);
  ReceivedPackets(generated_fec_packets_, fec_loss_mask, kFecPacket);
}

template <typename ForwardErrorCorrectionType>
template <typename PacketListType>
void RtpFecTest<ForwardErrorCorrectionType>::ReceivedPackets(
    const PacketListType& packet_list,
    int* loss_mask,
    bool is_fec) {
  uint16_t fec_seq_num = media_packet_generator_.GetFecSeqNum();
  int packet_idx = 0;

  for (const auto& packet : packet_list) {
    if (loss_mask[packet_idx] == 0) {
      std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> received_packet(
          new ForwardErrorCorrection::ReceivedPacket());
      received_packet->pkt = new ForwardErrorCorrection::Packet();
      received_packet->pkt->length = packet->length;
      memcpy(received_packet->pkt->data, packet->data, packet->length);
      received_packet->is_fec = is_fec;
      if (!is_fec) {
        // For media packets, the sequence number and marker bit is
        // obtained from RTP header. These were set in ConstructMediaPackets().
        received_packet->seq_num =
            ByteReader<uint16_t>::ReadBigEndian(&packet->data[2]);
      } else {
        // The sequence number, marker bit, and ssrc number are defined in the
        // RTP header of the FEC packet, which is not constructed in this test.
        // So we set these values below based on the values generated in
        // ConstructMediaPackets().
        received_packet->seq_num = fec_seq_num;
        // The ssrc value for FEC packets is set to the one used for the
        // media packets in ConstructMediaPackets().
        received_packet->ssrc = kMediaSsrc;
      }
      received_packets_.push_back(std::move(received_packet));
    }
    packet_idx++;
    // Sequence number of FEC packets are defined as increment by 1 from
    // last media packet in frame.
    if (is_fec)
      fec_seq_num++;
  }
}

template <typename ForwardErrorCorrectionType>
bool RtpFecTest<ForwardErrorCorrectionType>::IsRecoveryComplete() {
  // We must have equally many recovered packets as original packets.
  if (recovered_packets_.size() != media_packets_.size()) {
    return false;
  }

  // All recovered packets must be identical to the corresponding
  // original packets.
  auto cmp = [](
      const std::unique_ptr<ForwardErrorCorrection::Packet>& media_packet,
      const std::unique_ptr<ForwardErrorCorrection::RecoveredPacket>&
          recovered_packet) {
    if (media_packet->length != recovered_packet->pkt->length) {
      return false;
    }
    if (memcmp(media_packet->data, recovered_packet->pkt->data,
               media_packet->length) != 0) {
      return false;
    }
    return true;
  };
  return std::equal(media_packets_.cbegin(), media_packets_.cend(),
                    recovered_packets_.cbegin(), cmp);
}

// Define gTest typed test to loop over both ULPFEC and FlexFEC.
// Since the tests now are parameterized, we need to access
// member variables using |this|, thereby enforcing runtime
// resolution.

class FlexfecForwardErrorCorrection : public ForwardErrorCorrection {
 public:
  FlexfecForwardErrorCorrection()
      : ForwardErrorCorrection(
            std::unique_ptr<FecHeaderReader>(new FlexfecHeaderReader()),
            std::unique_ptr<FecHeaderWriter>(new FlexfecHeaderWriter())) {}
};

class UlpfecForwardErrorCorrection : public ForwardErrorCorrection {
 public:
  UlpfecForwardErrorCorrection()
      : ForwardErrorCorrection(
            std::unique_ptr<FecHeaderReader>(new UlpfecHeaderReader()),
            std::unique_ptr<FecHeaderWriter>(new UlpfecHeaderWriter())) {}
};

using FecTypes =
    Types<FlexfecForwardErrorCorrection, UlpfecForwardErrorCorrection>;
TYPED_TEST_CASE(RtpFecTest, FecTypes);

TYPED_TEST(RtpFecTest, FecRecoveryNoLoss) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr int kNumMediaPackets = 4;
  constexpr uint8_t kProtectionFactor = 60;

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());

  // No packets lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // No packets lost, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
}

TYPED_TEST(RtpFecTest, FecRecoveryWithLoss) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr int kNumMediaPackets = 4;
  constexpr uint8_t kProtectionFactor = 60;

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());

  // 1 media packet lost
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 2 media packets lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[1] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // 2 packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(this->IsRecoveryComplete());
}

// Verify that we don't use an old FEC packet for FEC decoding.
TYPED_TEST(RtpFecTest, FecRecoveryWithSeqNumGapTwoFrames) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr uint8_t kProtectionFactor = 20;

  // Two frames: first frame (old) with two media packets and 1 FEC packet.
  // Second frame (new) with 3 media packets, and no FEC packets.
  //       ---Frame 1----                     ----Frame 2------
  //  #0(media) #1(media) #2(FEC)     #65535(media) #0(media) #1(media).
  // If we lose either packet 0 or 1 of second frame, FEC decoding should not
  // try to decode using "old" FEC packet #2.

  // Construct media packets for first frame, starting at sequence number 0.
  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(2, 0);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));
  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());
  // Add FEC packet (seq#2) of this first frame to received list (i.e., assume
  // the two media packet were lost).
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->ReceivedPackets(this->generated_fec_packets_, this->fec_loss_mask_,
                        true);

  // Construct media packets for second frame, with sequence number wrap.
  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(3, 65535);

  // Expect 3 media packets for this frame.
  EXPECT_EQ(3u, this->media_packets_.size());

  // Second media packet lost (seq#0).
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  this->media_loss_mask_[1] = 1;
  // Add packets #65535, and #1 to received list.
  this->ReceivedPackets(this->media_packets_, this->media_loss_mask_, false);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Expect that no decoding is done to get missing packet (seq#0) of second
  // frame, using old FEC packet (seq#2) from first (old) frame. So number of
  // recovered packets is 2, and not equal to number of media packets (=3).
  EXPECT_EQ(2u, this->recovered_packets_.size());
  EXPECT_TRUE(this->recovered_packets_.size() != this->media_packets_.size());
}

// Verify we can still recover frame if sequence number wrap occurs within
// the frame and FEC packet following wrap is received after media packets.
TYPED_TEST(RtpFecTest, FecRecoveryWithSeqNumGapOneFrameRecovery) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr uint8_t kProtectionFactor = 20;

  // One frame, with sequence number wrap in media packets.
  //         -----Frame 1----
  //  #65534(media) #65535(media) #0(media) #1(FEC).
  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(3, 65534);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());

  // Lose one media packet (seq# 65535).
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[1] = 1;
  this->ReceivedPackets(this->media_packets_, this->media_loss_mask_, false);
  // Add FEC packet to received list following the media packets.
  this->ReceivedPackets(this->generated_fec_packets_, this->fec_loss_mask_,
                        true);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Expect 3 media packets in recovered list, and complete recovery.
  // Wrap-around won't remove FEC packet, as it follows the wrap.
  EXPECT_EQ(3u, this->recovered_packets_.size());
  EXPECT_TRUE(this->IsRecoveryComplete());
}

// Sequence number wrap occurs within the FEC packets for the frame.
// In this case we will discard FEC packet and full recovery is not expected.
// Same problem will occur if wrap is within media packets but FEC packet is
// received before the media packets. This may be improved if timing information
// is used to detect old FEC packets.
// TODO(marpan): Update test if wrap-around handling changes in FEC decoding.
TYPED_TEST(RtpFecTest, FecRecoveryWithSeqNumGapOneFrameNoRecovery) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr uint8_t kProtectionFactor = 200;

  // 1 frame: 3 media packets and 2 FEC packets.
  // Sequence number wrap in FEC packets.
  //           -----Frame 1----
  // #65532(media) #65533(media) #65534(media) #65535(FEC) #0(FEC).
  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(3, 65532);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 2 FEC packets.
  EXPECT_EQ(2u, this->generated_fec_packets_.size());

  // Lose the last two media packets (seq# 65533, 65534).
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[1] = 1;
  this->media_loss_mask_[2] = 1;
  this->ReceivedPackets(this->media_packets_, this->media_loss_mask_, false);
  this->ReceivedPackets(this->generated_fec_packets_, this->fec_loss_mask_,
                        true);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // The two FEC packets are received and should allow for complete recovery,
  // but because of the wrap the second FEC packet will be discarded, and only
  // one media packet is recoverable. So exepct 2 media packets on recovered
  // list and no complete recovery.
  EXPECT_EQ(2u, this->recovered_packets_.size());
  EXPECT_TRUE(this->recovered_packets_.size() != this->media_packets_.size());
  EXPECT_FALSE(this->IsRecoveryComplete());
}

// Verify we can still recover frame if media packets are reordered.
TYPED_TEST(RtpFecTest, FecRecoveryWithMediaOutOfOrder) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr uint8_t kProtectionFactor = 20;

  // One frame: 3 media packets, 1 FEC packet.
  //         -----Frame 1----
  //  #0(media) #1(media) #2(media) #3(FEC).
  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(3, 0);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());

  // Lose one media packet (seq# 1).
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[1] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  // Reorder received media packets.
  auto it0 = this->received_packets_.begin();
  auto it2 = this->received_packets_.begin();
  it2++;
  std::swap(*it0, *it2);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Expect 3 media packets in recovered list, and complete recovery.
  EXPECT_EQ(3u, this->recovered_packets_.size());
  EXPECT_TRUE(this->IsRecoveryComplete());
}

// Verify we can still recover frame if FEC is received before media packets.
TYPED_TEST(RtpFecTest, FecRecoveryWithFecOutOfOrder) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr uint8_t kProtectionFactor = 20;

  // One frame: 3 media packets, 1 FEC packet.
  //         -----Frame 1----
  //  #0(media) #1(media) #2(media) #3(FEC).
  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(3, 0);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());

  // Lose one media packet (seq# 1).
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[1] = 1;
  // Add FEC packet to received list before the media packets.
  this->ReceivedPackets(this->generated_fec_packets_, this->fec_loss_mask_,
                        true);
  // Add media packets to received list.
  this->ReceivedPackets(this->media_packets_, this->media_loss_mask_, false);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Expect 3 media packets in recovered list, and complete recovery.
  EXPECT_EQ(3u, this->recovered_packets_.size());
  EXPECT_TRUE(this->IsRecoveryComplete());
}

// Test 50% protection with random mask type: Two cases are considered:
// a 50% non-consecutive loss which can be fully recovered, and a 50%
// consecutive loss which cannot be fully recovered.
TYPED_TEST(RtpFecTest, FecRecoveryWithLoss50percRandomMask) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr int kNumMediaPackets = 4;
  constexpr uint8_t kProtectionFactor = 255;

  // Packet Mask for (4,4,0) code, from random mask table.
  // (kNumMediaPackets = 4; num_fec_packets = 4, kNumImportantPackets = 0)

  //         media#0   media#1  media#2    media#3
  // fec#0:    1          1        0          0
  // fec#1:    1          0        1          0
  // fec#2:    0          0        1          1
  // fec#3:    0          1        0          1
  //

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskRandom, &this->generated_fec_packets_));

  // Expect 4 FEC packets.
  EXPECT_EQ(4u, this->generated_fec_packets_.size());

  // 4 packets lost: 3 media packets (0, 2, 3), and one FEC packet (0) lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->fec_loss_mask_[0] = 1;
  this->media_loss_mask_[0] = 1;
  this->media_loss_mask_[2] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // With media packet#1 and FEC packets #1, #2, #3, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 4 consecutive packets lost: media packets 0, 1, 2, 3.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[0] = 1;
  this->media_loss_mask_[1] = 1;
  this->media_loss_mask_[2] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Cannot get complete recovery for this loss configuration with random mask.
  EXPECT_FALSE(this->IsRecoveryComplete());
}

// Test 50% protection with bursty type: Three cases are considered:
// two 50% consecutive losses which can be fully recovered, and one
// non-consecutive which cannot be fully recovered.
TYPED_TEST(RtpFecTest, FecRecoveryWithLoss50percBurstyMask) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr int kNumMediaPackets = 4;
  constexpr uint8_t kProtectionFactor = 255;

  // Packet Mask for (4,4,0) code, from bursty mask table.
  // (kNumMediaPackets = 4; num_fec_packets = 4, kNumImportantPackets = 0)

  //         media#0   media#1  media#2    media#3
  // fec#0:    1          0        0          0
  // fec#1:    1          1        0          0
  // fec#2:    0          1        1          0
  // fec#3:    0          0        1          1
  //

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 4 FEC packets.
  EXPECT_EQ(4u, this->generated_fec_packets_.size());

  // 4 consecutive packets lost: media packets 0,1,2,3.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[0] = 1;
  this->media_loss_mask_[1] = 1;
  this->media_loss_mask_[2] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Expect complete recovery for consecutive packet loss <= 50%.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 4 consecutive packets lost: media packets 1,2, 3, and FEC packet 0.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->fec_loss_mask_[0] = 1;
  this->media_loss_mask_[1] = 1;
  this->media_loss_mask_[2] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Expect complete recovery for consecutive packet loss <= 50%.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 4 packets lost (non-consecutive loss): media packets 0, 3, and FEC# 0, 3.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->fec_loss_mask_[0] = 1;
  this->fec_loss_mask_[3] = 1;
  this->media_loss_mask_[0] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Cannot get complete recovery for this loss configuration.
  EXPECT_FALSE(this->IsRecoveryComplete());
}

TYPED_TEST(RtpFecTest, FecRecoveryNoLossUep) {
  constexpr int kNumImportantPackets = 2;
  constexpr bool kUseUnequalProtection = true;
  constexpr int kNumMediaPackets = 4;
  constexpr uint8_t kProtectionFactor = 60;

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());

  // No packets lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // No packets lost, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
}

TYPED_TEST(RtpFecTest, FecRecoveryWithLossUep) {
  constexpr int kNumImportantPackets = 2;
  constexpr bool kUseUnequalProtection = true;
  constexpr int kNumMediaPackets = 4;
  constexpr uint8_t kProtectionFactor = 60;

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());

  // 1 media packet lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 2 media packets lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[1] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // 2 packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(this->IsRecoveryComplete());
}

// Test 50% protection with random mask type for UEP on.
TYPED_TEST(RtpFecTest, FecRecoveryWithLoss50percUepRandomMask) {
  constexpr int kNumImportantPackets = 1;
  constexpr bool kUseUnequalProtection = true;
  constexpr int kNumMediaPackets = 4;
  constexpr uint8_t kProtectionFactor = 255;

  // Packet Mask for (4,4,1) code, from random mask table.
  // (kNumMediaPackets = 4; num_fec_packets = 4, kNumImportantPackets = 1)

  //         media#0   media#1  media#2    media#3
  // fec#0:    1          0        0          0
  // fec#1:    1          1        0          0
  // fec#2:    1          0        1          1
  // fec#3:    0          1        1          0
  //

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(this->media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskRandom, &this->generated_fec_packets_));

  // Expect 4 FEC packets.
  EXPECT_EQ(4u, this->generated_fec_packets_.size());

  // 4 packets lost: 3 media packets and FEC packet#1 lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->fec_loss_mask_[1] = 1;
  this->media_loss_mask_[0] = 1;
  this->media_loss_mask_[2] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // With media packet#3 and FEC packets #0, #1, #3, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 5 packets lost: 4 media packets and one FEC packet#2 lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->fec_loss_mask_[2] = 1;
  this->media_loss_mask_[0] = 1;
  this->media_loss_mask_[1] = 1;
  this->media_loss_mask_[2] = 1;
  this->media_loss_mask_[3] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Cannot get complete recovery for this loss configuration.
  EXPECT_FALSE(this->IsRecoveryComplete());
}

TYPED_TEST(RtpFecTest, FecRecoveryNonConsecutivePackets) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr int kNumMediaPackets = 5;
  constexpr uint8_t kProtectionFactor = 60;

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  // Create a new temporary packet list for generating FEC packets.
  // This list should have every other packet removed.
  ForwardErrorCorrection::PacketList protected_media_packets;
  DeepCopyEveryNthPacket(this->media_packets_, 2, &protected_media_packets);

  EXPECT_EQ(
      0, this->fec_.EncodeFec(protected_media_packets, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, this->generated_fec_packets_.size());

  // 1 protected media packet lost
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[2] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // Unprotected packet lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[1] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Unprotected packet lost. Recovery not possible.
  EXPECT_FALSE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 2 media packets lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[0] = 1;
  this->media_loss_mask_[2] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // 2 protected packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(this->IsRecoveryComplete());
}

TYPED_TEST(RtpFecTest, FecRecoveryNonConsecutivePacketsExtension) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr int kNumMediaPackets = 21;
  uint8_t kProtectionFactor = 127;

  this->media_packets_ =
      this->media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  // Create a new temporary packet list for generating FEC packets.
  // This list should have every other packet removed.
  ForwardErrorCorrection::PacketList protected_media_packets;
  DeepCopyEveryNthPacket(this->media_packets_, 2, &protected_media_packets);

  // Zero column insertion will have to extend the size of the packet
  // mask since the number of actual packets are 21, while the number
  // of protected packets are 11.
  EXPECT_EQ(
      0, this->fec_.EncodeFec(protected_media_packets, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 5 FEC packet.
  EXPECT_EQ(5u, this->generated_fec_packets_.size());

  // Last protected media packet lost
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[kNumMediaPackets - 1] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // Last unprotected packet lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[kNumMediaPackets - 2] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Unprotected packet lost. Recovery not possible.
  EXPECT_FALSE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 6 media packets lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[kNumMediaPackets - 11] = 1;
  this->media_loss_mask_[kNumMediaPackets - 9] = 1;
  this->media_loss_mask_[kNumMediaPackets - 7] = 1;
  this->media_loss_mask_[kNumMediaPackets - 5] = 1;
  this->media_loss_mask_[kNumMediaPackets - 3] = 1;
  this->media_loss_mask_[kNumMediaPackets - 1] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // 5 protected packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(this->IsRecoveryComplete());
}

TYPED_TEST(RtpFecTest, FecRecoveryNonConsecutivePacketsWrap) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr int kNumMediaPackets = 21;
  uint8_t kProtectionFactor = 127;

  this->media_packets_ = this->media_packet_generator_.ConstructMediaPackets(
      kNumMediaPackets, 0xFFFF - 5);

  // Create a new temporary packet list for generating FEC packets.
  // This list should have every other packet removed.
  ForwardErrorCorrection::PacketList protected_media_packets;
  DeepCopyEveryNthPacket(this->media_packets_, 2, &protected_media_packets);

  // Zero column insertion will have to extend the size of the packet
  // mask since the number of actual packets are 21, while the number
  // of protected packets are 11.
  EXPECT_EQ(
      0, this->fec_.EncodeFec(protected_media_packets, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &this->generated_fec_packets_));

  // Expect 5 FEC packet.
  EXPECT_EQ(5u, this->generated_fec_packets_.size());

  // Last protected media packet lost
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[kNumMediaPackets - 1] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // Last unprotected packet lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[kNumMediaPackets - 2] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // Unprotected packet lost. Recovery not possible.
  EXPECT_FALSE(this->IsRecoveryComplete());
  this->recovered_packets_.clear();

  // 6 media packets lost.
  memset(this->media_loss_mask_, 0, sizeof(this->media_loss_mask_));
  memset(this->fec_loss_mask_, 0, sizeof(this->fec_loss_mask_));
  this->media_loss_mask_[kNumMediaPackets - 11] = 1;
  this->media_loss_mask_[kNumMediaPackets - 9] = 1;
  this->media_loss_mask_[kNumMediaPackets - 7] = 1;
  this->media_loss_mask_[kNumMediaPackets - 5] = 1;
  this->media_loss_mask_[kNumMediaPackets - 3] = 1;
  this->media_loss_mask_[kNumMediaPackets - 1] = 1;
  this->NetworkReceivedPackets(this->media_loss_mask_, this->fec_loss_mask_);

  EXPECT_EQ(0, this->fec_.DecodeFec(&this->received_packets_,
                                    &this->recovered_packets_));

  // 5 protected packets lost, one FEC packet, cannot get complete recovery.
  EXPECT_FALSE(this->IsRecoveryComplete());
}

// 'using' directive needed for compiler to be happy.
using RtpFecTestWithFlexfec = RtpFecTest<FlexfecForwardErrorCorrection>;
TEST_F(RtpFecTestWithFlexfec,
       FecRecoveryWithLossAndDifferentMediaAndFlexfecSsrcs) {
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr int kNumMediaPackets = 4;
  constexpr uint8_t kProtectionFactor = 60;

  media_packets_ =
      media_packet_generator_.ConstructMediaPackets(kNumMediaPackets);

  EXPECT_EQ(0, fec_.EncodeFec(media_packets_, kProtectionFactor,
                              kNumImportantPackets, kUseUnequalProtection,
                              kFecMaskBursty, &generated_fec_packets_));

  // Expect 1 FEC packet.
  EXPECT_EQ(1u, generated_fec_packets_.size());

  // 1 media packet lost
  memset(media_loss_mask_, 0, sizeof(media_loss_mask_));
  memset(fec_loss_mask_, 0, sizeof(fec_loss_mask_));
  media_loss_mask_[3] = 1;
  NetworkReceivedPackets(media_loss_mask_, fec_loss_mask_);

  // Simulate FlexFEC packet received on different SSRC.
  auto it = received_packets_.begin();
  ++it;
  ++it;
  ++it;  // Now at the FEC packet.
  (*it)->ssrc = kFlexfecSsrc;

  EXPECT_EQ(0, fec_.DecodeFec(&received_packets_, &recovered_packets_));

  // One packet lost, one FEC packet, expect complete recovery.
  EXPECT_TRUE(IsRecoveryComplete());
}

}  // namespace webrtc
