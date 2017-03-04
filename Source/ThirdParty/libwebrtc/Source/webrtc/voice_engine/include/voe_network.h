/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - External protocol support.
//  - Packet timeout notification.
//  - Dead-or-Alive connection observations.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoENetwork* netw  = VoENetwork::GetInterface(voe);
//  base->Init();
//  int ch = base->CreateChannel();
//  ...
//  netw->SetPeriodicDeadOrAliveStatus(ch, true);
//  ...
//  base->DeleteChannel(ch);
//  base->Terminate();
//  base->Release();
//  netw->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_NETWORK_H
#define WEBRTC_VOICE_ENGINE_VOE_NETWORK_H

#include "webrtc/api/call/transport.h"
#include "webrtc/common_types.h"

namespace webrtc {

class VoiceEngine;

// VoENetwork
class WEBRTC_DLLEXPORT VoENetwork {
 public:
  // Factory for the VoENetwork sub-API. Increases an internal
  // reference counter if successful. Returns NULL if the API is not
  // supported or if construction fails.
  static VoENetwork* GetInterface(VoiceEngine* voiceEngine);

  // Releases the VoENetwork sub-API and decreases an internal
  // reference counter. Returns the new reference count. This value should
  // be zero for all sub-API:s before the VoiceEngine object can be safely
  // deleted.
  virtual int Release() = 0;

  // Installs and enables a user-defined external transport protocol for a
  // specified |channel|. Returns -1 in case of an error, 0 otherwise.
  virtual int RegisterExternalTransport(int channel, Transport& transport) = 0;

  // Removes and disables a user-defined external transport protocol for a
  // specified |channel|. Returns -1 in case of an error, 0 otherwise.
  virtual int DeRegisterExternalTransport(int channel) = 0;

  // The packets received from the network should be passed to this
  // function when external transport is enabled. Note that the data
  // including the RTP-header must also be given to the VoiceEngine.
  //  Returns -1 in case of an error, 0 otherwise.
  virtual int ReceivedRTPPacket(int channel,
                                const void* data,
                                size_t length) = 0;
  virtual int ReceivedRTPPacket(int channel,
                                const void* data,
                                size_t length,
                                const PacketTime& packet_time) {
    return 0;
  }

  // The packets received from the network should be passed to this
  // function when external transport is enabled. Note that the data
  // including the RTCP-header must also be given to the VoiceEngine.
  //  Returns -1 in case of an error, 0 otherwise.
  virtual int ReceivedRTCPPacket(int channel,
                                 const void* data,
                                 size_t length) = 0;

 protected:
  VoENetwork() {}
  virtual ~VoENetwork() {}
};

}  // namespace webrtc

#endif  //  WEBRTC_VOICE_ENGINE_VOE_NETWORK_H
