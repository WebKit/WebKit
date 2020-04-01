//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef API_VOIP_VOIP_ENGINE_H_
#define API_VOIP_VOIP_ENGINE_H_

namespace webrtc {

class VoipBase;
class VoipCodec;
class VoipNetwork;

// VoipEngine interfaces
//
// These pointer interfaces are valid as long as VoipEngine is available.
// Therefore, application must synchronize the usage within the life span of
// created VoipEngine instance.
//
//   auto voip_engine =
//       webrtc::VoipEngineBuilder()
//           .SetAudioEncoderFactory(CreateBuiltinAudioEncoderFactory())
//           .SetAudioDecoderFactory(CreateBuiltinAudioDecoderFactory())
//           .Create();
//
//   auto voip_base = voip_engine->Base();
//   auto voip_codec = voip_engine->Codec();
//   auto voip_network = voip_engine->Network();
//
//   VoipChannel::Config config = { &app_transport_, 0xdeadc0de };
//   int channel = voip_base.CreateChannel(config);
//
//   // After SDP offer/answer, payload type and codec usage have been
//   // decided through negotiation.
//   voip_codec.SetSendCodec(channel, ...);
//   voip_codec.SetReceiveCodecs(channel, ...);
//
//   // Start Send/Playout on voip channel.
//   voip_base.StartSend(channel);
//   voip_base.StartPlayout(channel);
//
//   // Inject received rtp/rtcp thru voip network interface.
//   voip_network.ReceivedRTPPacket(channel, rtp_data, rtp_size);
//   voip_network.ReceivedRTCPPacket(channel, rtcp_data, rtcp_size);
//
//   // Stop and release voip channel.
//   voip_base.StopSend(channel);
//   voip_base.StopPlayout(channel);
//
//   voip_base.ReleaseChannel(channel);
//
class VoipEngine {
 public:
  virtual ~VoipEngine() = default;

  // VoipBase is the audio session management interface that
  // create/release/start/stop one-to-one audio media session.
  virtual VoipBase& Base() = 0;

  // VoipNetwork provides injection APIs that would enable application
  // to send and receive RTP/RTCP packets. There is no default network module
  // that provides RTP transmission and reception.
  virtual VoipNetwork& Network() = 0;

  // VoipCodec provides codec configuration APIs for encoder and decoders.
  virtual VoipCodec& Codec() = 0;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_ENGINE_H_
