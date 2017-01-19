/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/channel_transport/channel_transport.h"

#include <stdio.h>

#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)
#include "webrtc/test/gtest.h"
#endif
#include "webrtc/voice_engine/test/channel_transport/udp_transport.h"
#include "webrtc/voice_engine/include/voe_network.h"

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
#undef NDEBUG
#include <assert.h>
#endif

namespace webrtc {
namespace test {

VoiceChannelTransport::VoiceChannelTransport(VoENetwork* voe_network,
                                             int channel)
    : channel_(channel),
      voe_network_(voe_network) {
  uint8_t socket_threads = 1;
  socket_transport_ = UdpTransport::Create(channel, socket_threads);
  int registered = voe_network_->RegisterExternalTransport(channel,
                                                           *socket_transport_);
#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)
  EXPECT_EQ(0, registered);
#else
  assert(registered == 0);
#endif
}

VoiceChannelTransport::~VoiceChannelTransport() {
  voe_network_->DeRegisterExternalTransport(channel_);
  UdpTransport::Destroy(socket_transport_);
}

void VoiceChannelTransport::IncomingRTPPacket(
    const int8_t* incoming_rtp_packet,
    const size_t packet_length,
    const char* /*from_ip*/,
    const uint16_t /*from_port*/) {
  voe_network_->ReceivedRTPPacket(
      channel_, incoming_rtp_packet, packet_length, PacketTime());
}

void VoiceChannelTransport::IncomingRTCPPacket(
    const int8_t* incoming_rtcp_packet,
    const size_t packet_length,
    const char* /*from_ip*/,
    const uint16_t /*from_port*/) {
  voe_network_->ReceivedRTCPPacket(channel_, incoming_rtcp_packet,
                                   packet_length);
}

int VoiceChannelTransport::SetLocalReceiver(uint16_t rtp_port) {
  static const int kNumReceiveSocketBuffers = 500;
  int return_value = socket_transport_->InitializeReceiveSockets(this,
                                                                 rtp_port);
  if (return_value == 0) {
    return socket_transport_->StartReceiving(kNumReceiveSocketBuffers);
  }
  return return_value;
}

int VoiceChannelTransport::SetSendDestination(const char* ip_address,
                                              uint16_t rtp_port) {
  return socket_transport_->InitializeSendSockets(ip_address, rtp_port);
}

}  // namespace test
}  // namespace webrtc
