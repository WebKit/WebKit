/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_network_impl.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/logging.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoENetwork* VoENetwork::GetInterface(VoiceEngine* voiceEngine) {
  if (!voiceEngine) {
    return nullptr;
  }
  VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
  s->AddRef();
  return s;
}

VoENetworkImpl::VoENetworkImpl(voe::SharedData* shared) : _shared(shared) {
}

VoENetworkImpl::~VoENetworkImpl() = default;

int VoENetworkImpl::RegisterExternalTransport(int channel,
                                              Transport& transport) {
  RTC_DCHECK(_shared->statistics().Initialized());
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (!channelPtr) {
    LOG_F(LS_ERROR) << "Failed to locate channel: " << channel;
    return -1;
  }
  return channelPtr->RegisterExternalTransport(&transport);
}

int VoENetworkImpl::DeRegisterExternalTransport(int channel) {
  RTC_CHECK(_shared->statistics().Initialized());
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (!channelPtr) {
    LOG_F(LS_ERROR) << "Failed to locate channel: " << channel;
    return -1;
  }
  return channelPtr->DeRegisterExternalTransport();
}

int VoENetworkImpl::ReceivedRTPPacket(int channel,
                                      const void* data,
                                      size_t length) {
  return ReceivedRTPPacket(channel, data, length, webrtc::PacketTime());
}

int VoENetworkImpl::ReceivedRTPPacket(int channel,
                                      const void* data,
                                      size_t length,
                                      const PacketTime& packet_time) {
  RTC_CHECK(_shared->statistics().Initialized());
  RTC_CHECK(data);
  // L16 at 32 kHz, stereo, 10 ms frames (+12 byte RTP header) -> 1292 bytes
  if ((length < 12) || (length > 1292)) {
    LOG_F(LS_ERROR) << "Invalid packet length: " << length;
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (!channelPtr) {
    LOG_F(LS_ERROR) << "Failed to locate channel: " << channel;
    return -1;
  }
  if (!channelPtr->ExternalTransport()) {
    LOG_F(LS_ERROR) << "No external transport for channel: " << channel;
    return -1;
  }
  return channelPtr->ReceivedRTPPacket(static_cast<const uint8_t*>(data),
                                       length, packet_time);
}

int VoENetworkImpl::ReceivedRTCPPacket(int channel,
                                       const void* data,
                                       size_t length) {
  RTC_CHECK(_shared->statistics().Initialized());
  RTC_CHECK(data);
  if (length < 4) {
    LOG_F(LS_ERROR) << "Invalid packet length: " << length;
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (!channelPtr) {
    LOG_F(LS_ERROR) << "Failed to locate channel: " << channel;
    return -1;
  }
  if (!channelPtr->ExternalTransport()) {
    LOG_F(LS_ERROR) << "No external transport for channel: " << channel;
    return -1;
  }
  return channelPtr->ReceivedRTCPPacket(static_cast<const uint8_t*>(data),
                                        length);
}

}  // namespace webrtc
