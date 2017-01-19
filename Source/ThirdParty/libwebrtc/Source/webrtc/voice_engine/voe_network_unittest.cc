/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/voe_network.h"

#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_fixture.h"

namespace webrtc {

enum {
  kMinValidSizeOfRtcpPacketInBytes = 4,
  kMinValidSizeOfRtpPacketInBytes = 12,
  kMaxValidSizeOfRtpPacketInBytes = 1292
};

// A packet with a valid header for both RTP and RTCP.
// Methods that are tested in this file are checking only packet header.
static const uint8_t kPacket[kMinValidSizeOfRtpPacketInBytes] = {0x80};
static const uint8_t kPacketJunk[kMinValidSizeOfRtpPacketInBytes] = {};

static const int kNonExistingChannel = 1234;

class VoENetworkTest : public VoiceEngineFixture {
 protected:
  int CreateChannelAndRegisterExternalTransport() {
    EXPECT_EQ(0, base_->Init(&adm_, nullptr));
    int channelID = base_->CreateChannel();
    EXPECT_NE(channelID, -1);
    EXPECT_EQ(0, network_->RegisterExternalTransport(channelID, transport_));
    return channelID;
  }
};

TEST_F(VoENetworkTest, RegisterAndDeRegisterExternalTransport) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(0, network_->DeRegisterExternalTransport(channelID));
}

TEST_F(VoENetworkTest,
       RegisterExternalTransportOnNonExistingChannelShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_NE(
      0, network_->RegisterExternalTransport(kNonExistingChannel, transport_));
}

TEST_F(VoENetworkTest,
       DeRegisterExternalTransportOnNonExistingChannelShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_NE(0, network_->DeRegisterExternalTransport(kNonExistingChannel));
}

TEST_F(VoENetworkTest, DeRegisterExternalTransportBeforeRegister) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(0, network_->DeRegisterExternalTransport(channelID));
}

TEST_F(VoENetworkTest, ReceivedRTPPacketWithJunkDataShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(channelID, kPacketJunk,
                                            sizeof(kPacketJunk)));
}

TEST_F(VoENetworkTest, ReceivedRTPPacketOnNonExistingChannelShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(kNonExistingChannel, kPacket,
                                            sizeof(kPacket)));
}

TEST_F(VoENetworkTest, ReceivedRTPPacketOnChannelWithoutTransportShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(-1,
            network_->ReceivedRTPPacket(channelID, kPacket, sizeof(kPacket)));
}

TEST_F(VoENetworkTest, ReceivedTooSmallRTPPacketShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(
                    channelID, kPacket, kMinValidSizeOfRtpPacketInBytes - 1));
}

TEST_F(VoENetworkTest, ReceivedTooLargeRTPPacketShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTPPacket(
                    channelID, kPacket, kMaxValidSizeOfRtpPacketInBytes + 1));
}

TEST_F(VoENetworkTest, ReceivedRTCPPacketWithJunkDataShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(0, network_->ReceivedRTCPPacket(channelID, kPacketJunk,
                                            sizeof(kPacketJunk)));
  EXPECT_EQ(VE_SOCKET_TRANSPORT_MODULE_ERROR, base_->LastError());
}

TEST_F(VoENetworkTest, ReceivedRTCPPacketOnNonExistingChannelShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  EXPECT_EQ(-1, network_->ReceivedRTCPPacket(kNonExistingChannel, kPacket,
                                             sizeof(kPacket)));
}

TEST_F(VoENetworkTest, ReceivedRTCPPacketOnChannelWithoutTransportShouldFail) {
  EXPECT_EQ(0, base_->Init(&adm_, nullptr));
  int channelID = base_->CreateChannel();
  EXPECT_NE(channelID, -1);
  EXPECT_EQ(-1,
            network_->ReceivedRTCPPacket(channelID, kPacket, sizeof(kPacket)));
}

TEST_F(VoENetworkTest, ReceivedTooSmallRTCPPacket4ShouldFail) {
  int channelID = CreateChannelAndRegisterExternalTransport();
  EXPECT_EQ(-1, network_->ReceivedRTCPPacket(
                    channelID, kPacket, kMinValidSizeOfRtcpPacketInBytes - 1));
}

}  // namespace webrtc
