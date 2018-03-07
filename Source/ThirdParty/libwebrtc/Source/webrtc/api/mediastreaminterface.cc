/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/mediastreaminterface.h"

namespace webrtc {

const char MediaStreamTrackInterface::kVideoKind[] = "video";
const char MediaStreamTrackInterface::kAudioKind[] = "audio";

// TODO(ivoc): Remove this when the function becomes pure virtual.
AudioProcessorInterface::AudioProcessorStatistics
AudioProcessorInterface::GetStats(bool /*has_remote_tracks*/) {
  AudioProcessorStats stats;
  GetStats(&stats);
  AudioProcessorStatistics new_stats;
  new_stats.apm_statistics.divergent_filter_fraction =
      stats.aec_divergent_filter_fraction;
  new_stats.apm_statistics.delay_median_ms = stats.echo_delay_median_ms;
  new_stats.apm_statistics.delay_standard_deviation_ms =
      stats.echo_delay_std_ms;
  new_stats.apm_statistics.echo_return_loss = stats.echo_return_loss;
  new_stats.apm_statistics.echo_return_loss_enhancement =
      stats.echo_return_loss_enhancement;
  new_stats.apm_statistics.residual_echo_likelihood =
      stats.residual_echo_likelihood;
  new_stats.apm_statistics.residual_echo_likelihood_recent_max =
      stats.residual_echo_likelihood_recent_max;
  return new_stats;
}

}  // namespace webrtc
