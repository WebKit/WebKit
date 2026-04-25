/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/socket/packet_sender.h"

#include <cstdint>

#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/chunk/cookie_ack_chunk.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/socket/mock_dcsctp_socket_callbacks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace dcsctp {
namespace {
using ::testing::_;

constexpr VerificationTag kVerificationTag(123);

class PacketSenderTest : public testing::Test {
 protected:
  PacketSenderTest() : sender_(callbacks_, on_send_fn_.AsStdFunction()) {}

  SctpPacket::Builder PacketBuilder() const {
    return SctpPacket::Builder(kVerificationTag, options_);
  }

  DcSctpOptions options_;
  testing::NiceMock<MockDcSctpSocketCallbacks> callbacks_;
  testing::MockFunction<void(webrtc::ArrayView<const uint8_t>,
                             SendPacketStatus)>
      on_send_fn_;
  PacketSender sender_;
};

TEST_F(PacketSenderTest, SendPacketCallsCallback) {
  EXPECT_CALL(on_send_fn_, Call(_, SendPacketStatus::kSuccess));
  EXPECT_TRUE(sender_.Send(PacketBuilder().Add(CookieAckChunk())));

  EXPECT_CALL(callbacks_, SendPacketWithStatus)
      .WillOnce(testing::Return(SendPacketStatus::kError));
  EXPECT_CALL(on_send_fn_, Call(_, SendPacketStatus::kError));
  EXPECT_FALSE(sender_.Send(PacketBuilder().Add(CookieAckChunk())));
}

}  // namespace
}  // namespace dcsctp
