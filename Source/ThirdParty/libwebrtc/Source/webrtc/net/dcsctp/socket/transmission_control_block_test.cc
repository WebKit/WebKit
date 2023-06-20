/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/socket/transmission_control_block.h"

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/task_queue/task_queue_base.h"
#include "net/dcsctp/common/handover_testing.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/chunk/reconfig_chunk.h"
#include "net/dcsctp/packet/parameter/incoming_ssn_reset_request_parameter.h"
#include "net/dcsctp/packet/parameter/outgoing_ssn_reset_request_parameter.h"
#include "net/dcsctp/packet/parameter/parameter.h"
#include "net/dcsctp/packet/parameter/reconfiguration_response_parameter.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/rx/data_tracker.h"
#include "net/dcsctp/rx/reassembly_queue.h"
#include "net/dcsctp/socket/capabilities.h"
#include "net/dcsctp/socket/mock_context.h"
#include "net/dcsctp/socket/mock_dcsctp_socket_callbacks.h"
#include "net/dcsctp/testing/data_generator.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "net/dcsctp/timer/timer.h"
#include "net/dcsctp/tx/mock_send_queue.h"
#include "net/dcsctp/tx/retransmission_queue.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::Return;
using ::testing::StrictMock;

constexpr VerificationTag kMyVerificationTag = VerificationTag(123);
constexpr VerificationTag kPeerVerificationTag = VerificationTag(456);
constexpr TSN kMyInitialTsn = TSN(10);
constexpr TSN kPeerInitialTsn = TSN(1000);
constexpr size_t kArwnd = 65536;
constexpr TieTag kTieTag = TieTag(12345678);

class TransmissionControlBlockTest : public testing::Test {
 protected:
  TransmissionControlBlockTest()
      : sender_(callbacks_, on_send_fn_.AsStdFunction()),
        timer_manager_([this](webrtc::TaskQueueBase::DelayPrecision precision) {
          return callbacks_.CreateTimeout(precision);
        }) {}

  DcSctpOptions options_;
  Capabilities capabilities_;
  StrictMock<MockDcSctpSocketCallbacks> callbacks_;
  StrictMock<MockSendQueue> send_queue_;
  testing::MockFunction<void(rtc::ArrayView<const uint8_t>, SendPacketStatus)>
      on_send_fn_;
  testing::MockFunction<bool()> on_connection_established;
  PacketSender sender_;
  TimerManager timer_manager_;
};

TEST_F(TransmissionControlBlockTest, LogsBasicInfoInToString) {
  EXPECT_CALL(send_queue_, EnableMessageInterleaving);

  capabilities_.negotiated_maximum_incoming_streams = 1000;
  capabilities_.negotiated_maximum_outgoing_streams = 2000;
  TransmissionControlBlock tcb(
      timer_manager_, "log: ", options_, capabilities_, callbacks_, send_queue_,
      kMyVerificationTag, kMyInitialTsn, kPeerVerificationTag, kPeerInitialTsn,
      kArwnd, kTieTag, sender_, on_connection_established.AsStdFunction());

  EXPECT_EQ(tcb.ToString(),
            "verification_tag=000001c8, last_cumulative_ack=999, capabilities= "
            "max_in=1000 max_out=2000");
}

TEST_F(TransmissionControlBlockTest, LogsAllCapabilitiesInToSring) {
  EXPECT_CALL(send_queue_, EnableMessageInterleaving);

  capabilities_.negotiated_maximum_incoming_streams = 1000;
  capabilities_.negotiated_maximum_outgoing_streams = 2000;
  capabilities_.message_interleaving = true;
  capabilities_.partial_reliability = true;
  capabilities_.reconfig = true;

  TransmissionControlBlock tcb(
      timer_manager_, "log: ", options_, capabilities_, callbacks_, send_queue_,
      kMyVerificationTag, kMyInitialTsn, kPeerVerificationTag, kPeerInitialTsn,
      kArwnd, kTieTag, sender_, on_connection_established.AsStdFunction());

  EXPECT_EQ(tcb.ToString(),
            "verification_tag=000001c8, last_cumulative_ack=999, "
            "capabilities=PR,IL,Reconfig, max_in=1000 max_out=2000");
}

TEST_F(TransmissionControlBlockTest, IsInitiallyHandoverReady) {
  EXPECT_CALL(send_queue_, EnableMessageInterleaving);
  EXPECT_CALL(send_queue_, HasStreamsReadyToBeReset).WillOnce(Return(false));

  TransmissionControlBlock tcb(
      timer_manager_, "log: ", options_, capabilities_, callbacks_, send_queue_,
      kMyVerificationTag, kMyInitialTsn, kPeerVerificationTag, kPeerInitialTsn,
      kArwnd, kTieTag, sender_, on_connection_established.AsStdFunction());

  EXPECT_TRUE(tcb.GetHandoverReadiness().IsReady());
}
}  // namespace
}  // namespace dcsctp
