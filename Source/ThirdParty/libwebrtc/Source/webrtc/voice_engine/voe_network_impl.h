/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_NETWORK_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_NETWORK_IMPL_H

#include "webrtc/voice_engine/include/voe_network.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoENetworkImpl : public VoENetwork {
 public:
  int RegisterExternalTransport(int channel, Transport& transport) override;
  int DeRegisterExternalTransport(int channel) override;

  int ReceivedRTPPacket(int channel, const void* data, size_t length) override;
  int ReceivedRTPPacket(int channel,
                        const void* data,
                        size_t length,
                        const PacketTime& packet_time) override;

  int ReceivedRTCPPacket(int channel, const void* data, size_t length) override;

 protected:
  VoENetworkImpl(voe::SharedData* shared);
  ~VoENetworkImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_NETWORK_IMPL_H
