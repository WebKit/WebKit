/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_INCLUDE_AUDIO_NETWORK_ADAPTOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_INCLUDE_AUDIO_NETWORK_ADAPTOR_H_

#include "webrtc/base/optional.h"

namespace webrtc {

// An AudioNetworkAdaptor optimizes the audio experience by suggesting a
// suitable runtime configuration (bit rate, frame length, FEC, etc.) to the
// encoder based on network metrics.
class AudioNetworkAdaptor {
 public:
  struct EncoderRuntimeConfig {
    EncoderRuntimeConfig();
    EncoderRuntimeConfig(const EncoderRuntimeConfig& other);
    ~EncoderRuntimeConfig();
    rtc::Optional<int> bitrate_bps;
    rtc::Optional<int> frame_length_ms;
    rtc::Optional<float> uplink_packet_loss_fraction;
    rtc::Optional<bool> enable_fec;
    rtc::Optional<bool> enable_dtx;

    // Some encoders can encode fewer channels than the actual input to make
    // better use of the bandwidth. |num_channels| sets the number of channels
    // to encode.
    rtc::Optional<size_t> num_channels;
  };

  virtual ~AudioNetworkAdaptor() = default;

  virtual void SetUplinkBandwidth(int uplink_bandwidth_bps) = 0;

  virtual void SetUplinkPacketLossFraction(
      float uplink_packet_loss_fraction) = 0;

  virtual void SetRtt(int rtt_ms) = 0;

  virtual void SetTargetAudioBitrate(int target_audio_bitrate_bps) = 0;

  virtual void SetOverhead(size_t overhead_bytes_per_packet) = 0;

  virtual EncoderRuntimeConfig GetEncoderRuntimeConfig() = 0;

  virtual void StartDebugDump(FILE* file_handle) = 0;

  virtual void StopDebugDump() = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_INCLUDE_AUDIO_NETWORK_ADAPTOR_H_
