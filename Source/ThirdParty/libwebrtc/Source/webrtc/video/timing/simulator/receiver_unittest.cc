/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/receiver.h"

#include <cstdint>
#include <memory>

#include "api/sequence_checker.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/thread_annotations.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/rtp_packet_simulator.h"
#include "video/timing/simulator/test/simulated_time_test_fixture.h"

namespace webrtc::video_timing_simulator {
namespace {

constexpr uint8_t kRtxPayloadType = 97;
constexpr uint32_t kSsrc = 123456;
constexpr uint32_t kRtxSsrc = 789012;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Property;

class MockReceivedRtpPacketCallback : public ReceivedRtpPacketCallback {
 public:
  MOCK_METHOD(void,
              OnReceivedRtpPacket,
              (const RtpPacketReceived&),
              (override));
};

class ReceiverTest : public SimulatedTimeTestFixture {
 protected:
  ReceiverTest() {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      receiver_ = std::make_unique<Receiver>(env_, kSsrc, kRtxSsrc,
                                             &received_rtp_packet_cb_);
    });
  }
  ~ReceiverTest() {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      receiver_.reset();
    });
  }

  void InsertPacket(
      const RtpPacketSimulator::SimulatedPacket& simulated_packet) {
    SendTask([this, &simulated_packet]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      receiver_->InsertSimulatedPacket(simulated_packet);
    });
  }

  // Expectations.
  MockReceivedRtpPacketCallback received_rtp_packet_cb_;

  // Object under test.
  std::unique_ptr<Receiver> receiver_ RTC_GUARDED_BY(queue_ptr_);
};

TEST_F(ReceiverTest, DoesNotReceiveUnknownSsrc) {
  RtpPacketReceived rtp_packet(/*rtp_header_extension_map=*/nullptr);

  EXPECT_CALL(received_rtp_packet_cb_, OnReceivedRtpPacket).Times(0);
  InsertPacket({.rtp_packet = rtp_packet});
}

TEST_F(ReceiverTest, ReceivesVideoPacket) {
  RtpPacketReceived rtp_packet(/*rtp_header_extension_map=*/nullptr);
  rtp_packet.SetSsrc(kSsrc);

  EXPECT_CALL(received_rtp_packet_cb_, OnReceivedRtpPacket);
  InsertPacket({.rtp_packet = rtp_packet});
}

TEST_F(ReceiverTest, DoesNotReceiveRtxPacketWithoutRtxOsn) {
  RtpPacketReceived rtp_packet(/*rtp_header_extension_map=*/nullptr);
  rtp_packet.SetSsrc(kRtxSsrc);

  EXPECT_CALL(received_rtp_packet_cb_, OnReceivedRtpPacket).Times(0);
  InsertPacket({.rtp_packet = rtp_packet, .has_rtx_osn = false});
}

TEST_F(ReceiverTest, DoesNotReceiveRtxPacketWithoutRtxPayloadHeader) {
  RtpPacketReceived rtp_packet(/*rtp_header_extension_map=*/nullptr);
  rtp_packet.SetSsrc(kRtxSsrc);

  EXPECT_CALL(received_rtp_packet_cb_, OnReceivedRtpPacket).Times(0);
  InsertPacket({.rtp_packet = rtp_packet, .has_rtx_osn = true});
}

TEST_F(ReceiverTest, ReceivesRtxPacket) {
  RtpPacketReceived rtp_packet(/*rtp_header_extension_map=*/nullptr);
  rtp_packet.SetPayloadType(kRtxPayloadType);
  rtp_packet.SetSsrc(kRtxSsrc);
  uint8_t* payload = rtp_packet.AllocatePayload(kRtxHeaderSize);
  payload[0] = 0xab;
  payload[1] = 0xcd;

  EXPECT_CALL(received_rtp_packet_cb_,
              OnReceivedRtpPacket(AllOf(
                  Property(&RtpPacketReceived::SequenceNumber, Eq(0xabcd)),
                  // This _should have been_ != `kRtxPayloadType`, but due to
                  // our noop mapping it is not.
                  Property(&RtpPacketReceived::PayloadType, kRtxPayloadType))));
  InsertPacket({.rtp_packet = rtp_packet, .has_rtx_osn = true});
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
