/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_H_

#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"

namespace webrtc {

class Controller {
 public:
  struct NetworkMetrics {
    NetworkMetrics();
    ~NetworkMetrics();
    rtc::Optional<int> uplink_bandwidth_bps;
    rtc::Optional<float> uplink_packet_loss_fraction;
    rtc::Optional<int> target_audio_bitrate_bps;
    rtc::Optional<int> rtt_ms;
  };

  virtual ~Controller() = default;

  virtual void MakeDecision(
      const NetworkMetrics& metrics,
      AudioNetworkAdaptor::EncoderRuntimeConfig* config) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_H_
