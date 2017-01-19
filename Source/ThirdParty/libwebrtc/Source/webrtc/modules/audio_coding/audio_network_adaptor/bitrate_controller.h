/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_BITRATE_CONTROLLER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_BITRATE_CONTROLLER_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"

namespace webrtc {
namespace audio_network_adaptor {

class BitrateController final : public Controller {
 public:
  struct Config {
    Config(int initial_bitrate_bps, int initial_frame_length_ms);
    ~Config();
    int initial_bitrate_bps;
    int initial_frame_length_ms;
  };

  explicit BitrateController(const Config& config);

  void MakeDecision(const NetworkMetrics& metrics,
                    AudioNetworkAdaptor::EncoderRuntimeConfig* config) override;

 private:
  const Config config_;
  int bitrate_bps_;
  int overhead_rate_bps_;
  RTC_DISALLOW_COPY_AND_ASSIGN(BitrateController);
};

}  // namespace audio_network_adaptor
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_BITRATE_CONTROLLER_H_
