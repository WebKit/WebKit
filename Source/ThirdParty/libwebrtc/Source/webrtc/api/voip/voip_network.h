//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef API_VOIP_VOIP_NETWORK_H_
#define API_VOIP_VOIP_NETWORK_H_

#include "api/array_view.h"
#include "api/voip/voip_base.h"

namespace webrtc {

// VoipNetwork interface currently provides any network related interface
// such as processing received RTP/RTCP packet from remote endpoint.
// The interface subject to expand as needed.
//
// This interface requires a channel handle created via VoipBase interface.
class VoipNetwork {
 public:
  // The packets received from the network should be passed to this
  // function. Note that the data including the RTP-header must also be
  // given to the VoipEngine.
  virtual void ReceivedRTPPacket(ChannelId channel_id,
                                 rtc::ArrayView<const uint8_t> data) = 0;

  // The packets received from the network should be passed to this
  // function. Note that the data including the RTCP-header must also be
  // given to the VoipEngine.
  virtual void ReceivedRTCPPacket(ChannelId channel_id,
                                  rtc::ArrayView<const uint8_t> data) = 0;

 protected:
  virtual ~VoipNetwork() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_NETWORK_H_
