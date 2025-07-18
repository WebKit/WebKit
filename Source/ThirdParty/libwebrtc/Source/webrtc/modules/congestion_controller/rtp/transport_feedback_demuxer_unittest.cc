/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/rtp/transport_feedback_demuxer.h"

#include <cstdint>
#include <vector>

#include "api/transport/network_types.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using PacketInfo = StreamFeedbackObserver::StreamPacketInfo;

static constexpr uint32_t kSsrc = 8492;

class MockStreamFeedbackObserver : public StreamFeedbackObserver {
 public:
  MOCK_METHOD(void,
              OnPacketFeedbackVector,
              (std::vector<StreamPacketInfo> packet_feedback_vector),
              (override));
};

PacketResult CreatePacket(uint32_t ssrc,
                          uint16_t rtp_sequence_number,
                          bool received,
                          bool is_retransmission) {
  PacketResult res;
  res.rtp_packet_info = {
      .ssrc = ssrc,
      .rtp_sequence_number = rtp_sequence_number,
      .is_retransmission = is_retransmission,
  };
  res.receive_time =
      received ? Timestamp::Seconds(123) : Timestamp::PlusInfinity();

  return res;
}
}  // namespace

TEST(TransportFeedbackDemuxerTest, ObserverSanity) {
  TransportFeedbackDemuxer demuxer;
  MockStreamFeedbackObserver mock;
  demuxer.RegisterStreamFeedbackObserver({kSsrc}, &mock);

  TransportPacketsFeedback feedback;
  feedback.packet_feedbacks = {CreatePacket(kSsrc, /*rtp_sequence_number=*/55,
                                            /*received=*/true,
                                            /*is_retransmission=*/false),
                               CreatePacket(kSsrc, /*rtp_sequence_number=*/56,
                                            /*received=*/false,
                                            /*is_retransmission=*/false),
                               CreatePacket(kSsrc, /*rtp_sequence_number=*/57,
                                            /*received=*/true,
                                            /*is_retransmission=*/true)};
  EXPECT_CALL(mock, OnPacketFeedbackVector(ElementsAre(
                        AllOf(Field(&PacketInfo::received, true),
                              Field(&PacketInfo::ssrc, kSsrc),
                              Field(&PacketInfo::rtp_sequence_number, 55),
                              Field(&PacketInfo::is_retransmission, false)),
                        AllOf(Field(&PacketInfo::received, false),
                              Field(&PacketInfo::ssrc, kSsrc),
                              Field(&PacketInfo::rtp_sequence_number, 56),
                              Field(&PacketInfo::is_retransmission, false)),
                        AllOf(Field(&PacketInfo::received, true),
                              Field(&PacketInfo::ssrc, kSsrc),
                              Field(&PacketInfo::rtp_sequence_number, 57),
                              Field(&PacketInfo::is_retransmission, true)))));
  demuxer.OnTransportFeedback(feedback);

  demuxer.DeRegisterStreamFeedbackObserver(&mock);

  TransportPacketsFeedback second_feedback;
  second_feedback.packet_feedbacks = {
      CreatePacket(kSsrc, /*rtp_sequence_number=*/58,
                   /*received=*/true,
                   /*is_retransmission=*/false)};

  EXPECT_CALL(mock, OnPacketFeedbackVector).Times(0);
  demuxer.OnTransportFeedback(second_feedback);
}
}  // namespace webrtc
