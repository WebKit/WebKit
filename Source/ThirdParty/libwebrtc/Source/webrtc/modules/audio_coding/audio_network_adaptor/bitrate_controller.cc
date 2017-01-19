/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/bitrate_controller.h"

#include <algorithm>

#include "webrtc/base/checks.h"

namespace webrtc {
namespace audio_network_adaptor {

namespace {
// TODO(minyue): consider passing this from a higher layer through
// SetConstraints().
// L2(14B) + IPv4(20B) + UDP(8B) + RTP(12B) + SRTP_AUTH(10B) = 64B = 512 bits
constexpr int kPacketOverheadBits = 512;
}

BitrateController::Config::Config(int initial_bitrate_bps,
                                  int initial_frame_length_ms)
    : initial_bitrate_bps(initial_bitrate_bps),
      initial_frame_length_ms(initial_frame_length_ms) {}

BitrateController::Config::~Config() = default;

BitrateController::BitrateController(const Config& config)
    : config_(config),
      bitrate_bps_(config_.initial_bitrate_bps),
      overhead_rate_bps_(kPacketOverheadBits * 1000 /
                         config_.initial_frame_length_ms) {
  RTC_DCHECK_GT(bitrate_bps_, 0);
  RTC_DCHECK_GT(overhead_rate_bps_, 0);
}

void BitrateController::MakeDecision(
    const NetworkMetrics& metrics,
    AudioNetworkAdaptor::EncoderRuntimeConfig* config) {
  // Decision on |bitrate_bps| should not have been made.
  RTC_DCHECK(!config->bitrate_bps);

  if (metrics.target_audio_bitrate_bps) {
    int overhead_rate =
        config->frame_length_ms
            ? kPacketOverheadBits * 1000 / *config->frame_length_ms
            : overhead_rate_bps_;
    // If |metrics.target_audio_bitrate_bps| had included overhead, we would
    // simply do:
    //     bitrate_bps_ = metrics.target_audio_bitrate_bps - overhead_rate;
    // Follow https://bugs.chromium.org/p/webrtc/issues/detail?id=6315 to track
    // progress regarding this.
    // Now we assume that |metrics.target_audio_bitrate_bps| can handle the
    // overhead of most recent packets.
    bitrate_bps_ = std::max(0, *metrics.target_audio_bitrate_bps +
                                   overhead_rate_bps_ - overhead_rate);
    // TODO(minyue): apply a smoothing on the |overhead_rate_bps_|.
    overhead_rate_bps_ = overhead_rate;
  }
  config->bitrate_bps = rtc::Optional<int>(bitrate_bps_);
}

}  // namespace audio_network_adaptor
}  // namespace webrtc
