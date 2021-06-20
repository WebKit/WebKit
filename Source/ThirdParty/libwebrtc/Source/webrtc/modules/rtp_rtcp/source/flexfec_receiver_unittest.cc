/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/flexfec_receiver.h"

#include <algorithm>
#include <memory>

#include "modules/rtp_rtcp/mocks/mock_recovered_packet_receiver.h"
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

using test::fec::FlexfecPacketGenerator;
using Packet = ForwardErrorCorrection::Packet;
using PacketList = ForwardErrorCorrection::PacketList;

constexpr size_t kPayloadLength = 500;
constexpr uint32_t kFlexfecSsrc = 42984;
constexpr uint32_t kMediaSsrc = 8353;

RtpPacketReceived ParsePacket(const Packet& packet) {
  RtpPacketReceived parsed_packet;
  EXPECT_TRUE(parsed_packet.Parse(packet.data));
  return parsed_packet;
}

}  // namespace

class FlexfecReceiverForTest : public FlexfecReceiver {
 public:
  FlexfecReceiverForTest(uint32_t ssrc,
                         uint32_t protected_media_ssrc,
                         RecoveredPacketReceiver* recovered_packet_receiver)
      : FlexfecReceiver(Clock::GetRealTimeClock(),
                        ssrc,
                        protected_media_ssrc,
                        recovered_packet_receiver) {}
  // Expose methods for tests.
  using FlexfecReceiver::AddReceivedPacket;
  using FlexfecReceiver::ProcessReceivedPacket;
};

class FlexfecReceiverTest : public ::testing::Test {
 protected:
  FlexfecReceiverTest()
      : receiver_(kFlexfecSsrc, kMediaSsrc, &recovered_packet_receiver_),
        erasure_code_(
            ForwardErrorCorrection::CreateFlexfec(kFlexfecSsrc, kMediaSsrc)),
        packet_generator_(kMediaSsrc, kFlexfecSsrc) {}

  // Generates |num_media_packets| corresponding to a single frame.
  void PacketizeFrame(size_t num_media_packets,
                      size_t frame_offset,
                      PacketList* media_packets);

  // Generates |num_fec_packets| FEC packets, given |media_packets|.
  std::list<Packet*> EncodeFec(const PacketList& media_packets,
                               size_t num_fec_packets);

  FlexfecReceiverForTest receiver_;
  std::unique_ptr<ForwardErrorCorrection> erasure_code_;

  FlexfecPacketGenerator packet_generator_;
  ::testing::StrictMock<MockRecoveredPacketReceiver> recovered_packet_receiver_;
};

void FlexfecReceiverTest::PacketizeFrame(size_t num_media_packets,
                                         size_t frame_offset,
                                         PacketList* media_packets) {
  packet_generator_.NewFrame(num_media_packets);
  for (size_t i = 0; i < num_media_packets; ++i) {
    std::unique_ptr<Packet> next_packet(
        packet_generator_.NextPacket(frame_offset + i, kPayloadLength));
    media_packets->push_back(std::move(next_packet));
  }
}

std::list<Packet*> FlexfecReceiverTest::EncodeFec(
    const PacketList& media_packets,
    size_t num_fec_packets) {
  const uint8_t protection_factor =
      num_fec_packets * 255 / media_packets.size();
  constexpr int kNumImportantPackets = 0;
  constexpr bool kUseUnequalProtection = false;
  constexpr FecMaskType kFecMaskType = kFecMaskRandom;
  std::list<Packet*> fec_packets;
  EXPECT_EQ(0, erasure_code_->EncodeFec(
                   media_packets, protection_factor, kNumImportantPackets,
                   kUseUnequalProtection, kFecMaskType, &fec_packets));
  EXPECT_EQ(num_fec_packets, fec_packets.size());
  return fec_packets;
}

TEST_F(FlexfecReceiverTest, ReceivesMediaPacket) {
  packet_generator_.NewFrame(1);
  std::unique_ptr<Packet> media_packet(
      packet_generator_.NextPacket(0, kPayloadLength));

  std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> received_packet =
      receiver_.AddReceivedPacket(ParsePacket(*media_packet));
  ASSERT_TRUE(received_packet);
  receiver_.ProcessReceivedPacket(*received_packet);
}

TEST_F(FlexfecReceiverTest, ReceivesMediaAndFecPackets) {
  const size_t kNumMediaPackets = 1;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);
  const auto& media_packet = media_packets.front();
  auto fec_packet = packet_generator_.BuildFlexfecPacket(*fec_packets.front());

  std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> received_packet =
      receiver_.AddReceivedPacket(ParsePacket(*media_packet));
  ASSERT_TRUE(received_packet);
  receiver_.ProcessReceivedPacket(*received_packet);
  received_packet = receiver_.AddReceivedPacket(ParsePacket(*fec_packet));
  ASSERT_TRUE(received_packet);
  receiver_.ProcessReceivedPacket(*received_packet);
}

TEST_F(FlexfecReceiverTest, FailsOnTruncatedFecPacket) {
  const size_t kNumMediaPackets = 1;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);
  const auto& media_packet = media_packets.front();
  // Simulate truncated FlexFEC payload.
  fec_packets.front()->data.SetSize(1);
  auto fec_packet = packet_generator_.BuildFlexfecPacket(*fec_packets.front());

  std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> received_packet =
      receiver_.AddReceivedPacket(ParsePacket(*media_packet));
  ASSERT_TRUE(received_packet);
  receiver_.ProcessReceivedPacket(*received_packet);
  EXPECT_FALSE(receiver_.AddReceivedPacket(ParsePacket(*fec_packet)));
}

TEST_F(FlexfecReceiverTest, FailsOnUnknownMediaSsrc) {
  const size_t kNumMediaPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  auto& media_packet = media_packets.front();
  // Corrupt the SSRC.
  media_packet->data.MutableData()[8] = 0;
  media_packet->data.MutableData()[9] = 1;
  media_packet->data.MutableData()[10] = 2;
  media_packet->data.MutableData()[11] = 3;

  EXPECT_FALSE(receiver_.AddReceivedPacket(ParsePacket(*media_packet)));
}

TEST_F(FlexfecReceiverTest, FailsOnUnknownFecSsrc) {
  const size_t kNumMediaPackets = 1;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);
  const auto& media_packet = media_packets.front();
  auto fec_packet = packet_generator_.BuildFlexfecPacket(*fec_packets.front());
  // Corrupt the SSRC.
  fec_packet->data.MutableData()[8] = 4;
  fec_packet->data.MutableData()[9] = 5;
  fec_packet->data.MutableData()[10] = 6;
  fec_packet->data.MutableData()[11] = 7;

  std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> received_packet =
      receiver_.AddReceivedPacket(ParsePacket(*media_packet));
  ASSERT_TRUE(received_packet);
  receiver_.ProcessReceivedPacket(*received_packet);
  EXPECT_FALSE(receiver_.AddReceivedPacket(ParsePacket(*fec_packet)));
}

TEST_F(FlexfecReceiverTest, ReceivesMultiplePackets) {
  const size_t kNumMediaPackets = 2;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Receive all media packets.
  for (const auto& media_packet : media_packets) {
    std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> received_packet =
        receiver_.AddReceivedPacket(ParsePacket(*media_packet));
    ASSERT_TRUE(received_packet);
    receiver_.ProcessReceivedPacket(*received_packet);
  }

  // Receive FEC packet.
  auto* fec_packet = fec_packets.front();
  std::unique_ptr<Packet> packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(*fec_packet);
  std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> received_packet =
      receiver_.AddReceivedPacket(ParsePacket(*packet_with_rtp_header));
  ASSERT_TRUE(received_packet);
  receiver_.ProcessReceivedPacket(*received_packet);
}

TEST_F(FlexfecReceiverTest, RecoversFromSingleMediaLoss) {
  const size_t kNumMediaPackets = 2;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Receive first media packet but drop second.
  auto media_it = media_packets.begin();
  receiver_.OnRtpPacket(ParsePacket(**media_it));

  // Receive FEC packet and ensure recovery of lost media packet.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(**fec_it);
  media_it++;
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, (*media_it)->data.size()))
      .With(Args<0, 1>(ElementsAreArray((*media_it)->data.cdata(),
                                        (*media_it)->data.size())));
  receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));
}

TEST_F(FlexfecReceiverTest, RecoversFromDoubleMediaLoss) {
  const size_t kNumMediaPackets = 2;
  const size_t kNumFecPackets = 2;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Drop both media packets.

  // Receive first FEC packet and recover first lost media packet.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(**fec_it);
  auto media_it = media_packets.begin();
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, (*media_it)->data.size()))
      .With(Args<0, 1>(ElementsAreArray((*media_it)->data.cdata(),
                                        (*media_it)->data.size())));
  receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));

  // Receive second FEC packet and recover second lost media packet.
  fec_it++;
  packet_with_rtp_header = packet_generator_.BuildFlexfecPacket(**fec_it);
  media_it++;
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, (*media_it)->data.size()))
      .With(Args<0, 1>(ElementsAreArray((*media_it)->data.cdata(),
                                        (*media_it)->data.size())));
  receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));
}

TEST_F(FlexfecReceiverTest, DoesNotRecoverFromMediaAndFecLoss) {
  const size_t kNumMediaPackets = 2;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Receive first media packet.
  auto media_it = media_packets.begin();
  receiver_.OnRtpPacket(ParsePacket(**media_it));

  // Drop second media packet and FEC packet. Do not expect call back.
}

TEST_F(FlexfecReceiverTest, DoesNotCallbackTwice) {
  const size_t kNumMediaPackets = 2;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Receive first media packet but drop second.
  auto media_it = media_packets.begin();
  receiver_.OnRtpPacket(ParsePacket(**media_it));

  // Receive FEC packet and ensure recovery of lost media packet.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(**fec_it);
  media_it++;
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, (*media_it)->data.size()))
      .With(Args<0, 1>(ElementsAreArray((*media_it)->data.cdata(),
                                        (*media_it)->data.size())));
  receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));

  // Receive the FEC packet again, but do not call back.
  receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));

  // Receive the first media packet again, but do not call back.
  media_it = media_packets.begin();
  receiver_.OnRtpPacket(ParsePacket(**media_it));

  // Receive the second media packet again (the one recovered above),
  // but do not call back again.
  media_it++;
  receiver_.OnRtpPacket(ParsePacket(**media_it));
}

// Here we are implicitly assuming packet masks that are suitable for
// this type of 50% correlated loss. If we are changing our precomputed
// packet masks, this test might need to be updated.
TEST_F(FlexfecReceiverTest, RecoversFrom50PercentLoss) {
  const size_t kNumFecPackets = 5;
  const size_t kNumFrames = 2 * kNumFecPackets;
  const size_t kNumMediaPacketsPerFrame = 1;

  PacketList media_packets;
  for (size_t i = 0; i < kNumFrames; ++i) {
    PacketizeFrame(kNumMediaPacketsPerFrame, i, &media_packets);
  }
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Drop every second media packet.
  auto media_it = media_packets.begin();
  while (media_it != media_packets.end()) {
    receiver_.OnRtpPacket(ParsePacket(**media_it));
    ++media_it;
    if (media_it == media_packets.end()) {
      break;
    }
    ++media_it;
  }

  // Receive all FEC packets.
  media_it = media_packets.begin();
  for (const auto* fec_packet : fec_packets) {
    std::unique_ptr<Packet> fec_packet_with_rtp_header =
        packet_generator_.BuildFlexfecPacket(*fec_packet);
    ++media_it;
    if (media_it == media_packets.end()) {
      break;
    }
    EXPECT_CALL(recovered_packet_receiver_,
                OnRecoveredPacket(_, (*media_it)->data.size()))
        .With(Args<0, 1>(ElementsAreArray((*media_it)->data.cdata(),
                                          (*media_it)->data.size())));
    receiver_.OnRtpPacket(ParsePacket(*fec_packet_with_rtp_header));
    ++media_it;
  }
}

TEST_F(FlexfecReceiverTest, DelayedFecPacketDoesHelp) {
  // These values need to be updated if the underlying erasure code
  // implementation changes.
  // Delay FEC packet by maximum number of media packets tracked by receiver.
  const size_t kNumFrames = 192;
  const size_t kNumMediaPacketsPerFrame = 1;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPacketsPerFrame, 0, &media_packets);
  PacketizeFrame(kNumMediaPacketsPerFrame, 1, &media_packets);
  // Protect two first frames.
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);
  for (size_t i = 2; i < kNumFrames; ++i) {
    PacketizeFrame(kNumMediaPacketsPerFrame, i, &media_packets);
  }

  // Drop first media packet and delay FEC packet.
  auto media_it = media_packets.begin();
  ++media_it;

  // Receive all other media packets.
  while (media_it != media_packets.end()) {
    receiver_.OnRtpPacket(ParsePacket(**media_it));
    ++media_it;
  }

  // Receive FEC packet and recover first media packet.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(**fec_it);
  media_it = media_packets.begin();
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, (*media_it)->data.size()))
      .With(Args<0, 1>(ElementsAreArray((*media_it)->data.cdata(),
                                        (*media_it)->data.size())));
  receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));
}

TEST_F(FlexfecReceiverTest, TooDelayedFecPacketDoesNotHelp) {
  // These values need to be updated if the underlying erasure code
  // implementation changes.
  // Delay FEC packet by one more than maximum number of media packets
  // tracked by receiver.
  const size_t kNumFrames = 193;
  const size_t kNumMediaPacketsPerFrame = 1;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPacketsPerFrame, 0, &media_packets);
  PacketizeFrame(kNumMediaPacketsPerFrame, 1, &media_packets);
  // Protect first two frames.
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);
  for (size_t i = 2; i < kNumFrames; ++i) {
    PacketizeFrame(kNumMediaPacketsPerFrame, i, &media_packets);
  }

  // Drop first media packet and delay FEC packet.
  auto media_it = media_packets.begin();
  ++media_it;

  // Receive all other media packets.
  while (media_it != media_packets.end()) {
    receiver_.OnRtpPacket(ParsePacket(**media_it));
    ++media_it;
  }

  // Receive FEC packet.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(**fec_it);
  receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));

  // Do not expect a call back.
}

TEST_F(FlexfecReceiverTest, SurvivesOldRecoveredPacketBeingReinserted) {
  // Simulates the behaviour of the
  // Call->FlexfecReceiveStream->FlexfecReceiver->Call loop in production code.
  class LoopbackRecoveredPacketReceiver : public RecoveredPacketReceiver {
   public:
    LoopbackRecoveredPacketReceiver() : receiver_(nullptr) {}

    void SetReceiver(FlexfecReceiver* receiver) { receiver_ = receiver; }

    // Implements RecoveredPacketReceiver.
    void OnRecoveredPacket(const uint8_t* packet, size_t length) override {
      RtpPacketReceived parsed_packet;
      EXPECT_TRUE(parsed_packet.Parse(packet, length));
      parsed_packet.set_recovered(true);

      RTC_DCHECK(receiver_);
      receiver_->OnRtpPacket(parsed_packet);
    }

   private:
    FlexfecReceiver* receiver_;
  } loopback_recovered_packet_receiver;

  // Feed recovered packets back into |receiver|.
  FlexfecReceiver receiver(Clock::GetRealTimeClock(), kFlexfecSsrc, kMediaSsrc,
                           &loopback_recovered_packet_receiver);
  loopback_recovered_packet_receiver.SetReceiver(&receiver);

  // Receive first set of packets.
  PacketList first_media_packets;
  for (int i = 0; i < 46; ++i) {
    PacketizeFrame(1, 0, &first_media_packets);
  }
  for (const auto& media_packet : first_media_packets) {
    receiver.OnRtpPacket(ParsePacket(*media_packet));
  }

  // Protect one media packet. Lose the media packet,
  // but do not receive FEC packet yet.
  PacketList protected_media_packet;
  PacketizeFrame(1, 0, &protected_media_packet);
  const std::list<Packet*> fec_packets = EncodeFec(protected_media_packet, 1);
  EXPECT_EQ(1u, fec_packets.size());
  std::unique_ptr<Packet> fec_packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(*fec_packets.front());

  // Lose some packets, thus introducing a sequence number gap.
  PacketList lost_packets;
  for (int i = 0; i < 100; ++i) {
    PacketizeFrame(1, 0, &lost_packets);
  }

  // Receive one more packet.
  PacketList second_media_packets;
  PacketizeFrame(1, 0, &second_media_packets);
  for (const auto& media_packet : second_media_packets) {
    receiver.OnRtpPacket(ParsePacket(*media_packet));
  }

  // Receive delayed FEC packet.
  receiver.OnRtpPacket(ParsePacket(*fec_packet_with_rtp_header));

  // Expect no crash.
}

TEST_F(FlexfecReceiverTest, RecoversWithMediaPacketsOutOfOrder) {
  const size_t kNumMediaPackets = 6;
  const size_t kNumFecPackets = 2;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Lose two media packets, and receive the others out of order.
  auto media_it = media_packets.begin();
  auto media_packet0 = media_it++;
  auto media_packet1 = media_it++;
  auto media_packet2 = media_it++;
  auto media_packet3 = media_it++;
  auto media_packet4 = media_it++;
  auto media_packet5 = media_it++;
  receiver_.OnRtpPacket(ParsePacket(**media_packet5));
  receiver_.OnRtpPacket(ParsePacket(**media_packet2));
  receiver_.OnRtpPacket(ParsePacket(**media_packet3));
  receiver_.OnRtpPacket(ParsePacket(**media_packet0));

  // Expect to recover lost media packets.
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, (*media_packet1)->data.size()))
      .With(Args<0, 1>(ElementsAreArray((*media_packet1)->data.cdata(),
                                        (*media_packet1)->data.size())));
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, (*media_packet4)->data.size()))
      .With(Args<0, 1>(ElementsAreArray((*media_packet4)->data.cdata(),
                                        (*media_packet4)->data.size())));

  // Add FEC packets.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> packet_with_rtp_header;
  while (fec_it != fec_packets.end()) {
    packet_with_rtp_header = packet_generator_.BuildFlexfecPacket(**fec_it);
    receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));
    ++fec_it;
  }
}

// Recovered media packets may be fed back into the FlexfecReceiver by the
// callback. This test ensures the idempotency of such a situation.
TEST_F(FlexfecReceiverTest, RecoveryCallbackDoesNotLoopInfinitely) {
  class LoopbackRecoveredPacketReceiver : public RecoveredPacketReceiver {
   public:
    const int kMaxRecursionDepth = 10;

    LoopbackRecoveredPacketReceiver()
        : receiver_(nullptr),
          did_receive_call_back_(false),
          recursion_depth_(0),
          deep_recursion_(false) {}

    void SetReceiver(FlexfecReceiver* receiver) { receiver_ = receiver; }
    bool DidReceiveCallback() const { return did_receive_call_back_; }
    bool DeepRecursion() const { return deep_recursion_; }

    // Implements RecoveredPacketReceiver.
    void OnRecoveredPacket(const uint8_t* packet, size_t length) override {
      RtpPacketReceived parsed_packet;
      EXPECT_TRUE(parsed_packet.Parse(packet, length));

      did_receive_call_back_ = true;

      if (recursion_depth_ > kMaxRecursionDepth) {
        deep_recursion_ = true;
        return;
      }
      ++recursion_depth_;
      RTC_DCHECK(receiver_);
      receiver_->OnRtpPacket(parsed_packet);
      --recursion_depth_;
    }

   private:
    FlexfecReceiver* receiver_;
    bool did_receive_call_back_;
    int recursion_depth_;
    bool deep_recursion_;
  } loopback_recovered_packet_receiver;

  // Feed recovered packets back into |receiver|.
  FlexfecReceiver receiver(Clock::GetRealTimeClock(), kFlexfecSsrc, kMediaSsrc,
                           &loopback_recovered_packet_receiver);
  loopback_recovered_packet_receiver.SetReceiver(&receiver);

  const size_t kNumMediaPackets = 2;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Receive first media packet but drop second.
  auto media_it = media_packets.begin();
  receiver.OnRtpPacket(ParsePacket(**media_it));

  // Receive FEC packet and verify that a packet was recovered.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(**fec_it);
  receiver.OnRtpPacket(ParsePacket(*packet_with_rtp_header));
  EXPECT_TRUE(loopback_recovered_packet_receiver.DidReceiveCallback());
  EXPECT_FALSE(loopback_recovered_packet_receiver.DeepRecursion());
}

TEST_F(FlexfecReceiverTest, CalculatesNumberOfPackets) {
  const size_t kNumMediaPackets = 2;
  const size_t kNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kNumMediaPackets, 0, &media_packets);
  std::list<Packet*> fec_packets = EncodeFec(media_packets, kNumFecPackets);

  // Receive first media packet but drop second.
  auto media_it = media_packets.begin();
  receiver_.OnRtpPacket(ParsePacket(**media_it));

  // Receive FEC packet and ensure recovery of lost media packet.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(**fec_it);
  media_it++;
  EXPECT_CALL(recovered_packet_receiver_,
              OnRecoveredPacket(_, (*media_it)->data.size()))
      .With(Args<0, 1>(ElementsAreArray((*media_it)->data.cdata(),
                                        (*media_it)->data.size())));
  receiver_.OnRtpPacket(ParsePacket(*packet_with_rtp_header));

  // Check stats calculations.
  FecPacketCounter packet_counter = receiver_.GetPacketCounter();
  EXPECT_EQ(2U, packet_counter.num_packets);
  EXPECT_EQ(1U, packet_counter.num_fec_packets);
  EXPECT_EQ(1U, packet_counter.num_recovered_packets);
}

TEST_F(FlexfecReceiverTest, DoesNotDecodeWrappedMediaSequenceUsingOldFec) {
  const size_t kFirstFrameNumMediaPackets = 2;
  const size_t kFirstFrameNumFecPackets = 1;

  PacketList media_packets;
  PacketizeFrame(kFirstFrameNumMediaPackets, 0, &media_packets);

  // Protect first frame (sequences 0 and 1) with 1 FEC packet.
  std::list<Packet*> fec_packets =
      EncodeFec(media_packets, kFirstFrameNumFecPackets);

  // Generate enough media packets to simulate media sequence number wraparound.
  // Use no FEC for these frames to make sure old FEC is not purged due to age.
  const size_t kNumFramesSequenceWrapAround =
      std::numeric_limits<uint16_t>::max();
  const size_t kNumMediaPacketsPerFrame = 1;

  for (size_t i = 1; i <= kNumFramesSequenceWrapAround; ++i) {
    PacketizeFrame(kNumMediaPacketsPerFrame, i, &media_packets);
  }

  // Receive first (|kFirstFrameNumMediaPackets| + 192) media packets.
  // Simulate an old FEC packet by separating it from its encoded media
  // packets by at least 192 packets.
  auto media_it = media_packets.begin();
  for (size_t i = 0; i < (kFirstFrameNumMediaPackets + 192); i++) {
    if (i == 1) {
      // Drop the second packet of the first frame.
      media_it++;
    } else {
      receiver_.OnRtpPacket(ParsePacket(**media_it++));
    }
  }

  // Receive FEC packet. Although a protected packet was dropped,
  // expect no recovery callback since it is delayed from first frame
  // by more than 192 packets.
  auto fec_it = fec_packets.begin();
  std::unique_ptr<Packet> fec_packet_with_rtp_header =
      packet_generator_.BuildFlexfecPacket(**fec_it);
  receiver_.OnRtpPacket(ParsePacket(*fec_packet_with_rtp_header));

  // Receive remaining media packets.
  // NOTE: Because we sent enough to simulate wrap around, sequence 0 is
  // received again, but is a different packet than the original first
  // packet of first frame.
  while (media_it != media_packets.end()) {
    receiver_.OnRtpPacket(ParsePacket(**media_it++));
  }

  // Do not expect a recovery callback, the FEC packet is old
  // and should not decode wrapped around media sequences.
}

}  // namespace webrtc
