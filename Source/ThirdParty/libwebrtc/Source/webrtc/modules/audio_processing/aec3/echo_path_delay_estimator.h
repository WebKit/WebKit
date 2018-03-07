/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_

#include <vector>

#include "api/optional.h"
#include "modules/audio_processing/aec3/decimator.h"
#include "modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "modules/audio_processing/aec3/matched_filter.h"
#include "modules/audio_processing/aec3/matched_filter_lag_aggregator.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

// Estimates the delay of the echo path.
class EchoPathDelayEstimator {
 public:
  EchoPathDelayEstimator(ApmDataDumper* data_dumper,
                         const EchoCanceller3Config& config);
  ~EchoPathDelayEstimator();

  // Resets the estimation.
  void Reset();

  // Produce a delay estimate if such is avaliable.
  rtc::Optional<size_t> EstimateDelay(
      const DownsampledRenderBuffer& render_buffer,
      rtc::ArrayView<const float> capture);

  // Log delay estimator properties.
  void LogDelayEstimationProperties(int sample_rate_hz, size_t shift) const {
    matched_filter_.LogFilterProperties(sample_rate_hz, shift,
                                        down_sampling_factor_);
  }

 private:
  ApmDataDumper* const data_dumper_;
  const size_t down_sampling_factor_;
  const size_t sub_block_size_;
  Decimator capture_decimator_;
  MatchedFilter matched_filter_;
  MatchedFilterLagAggregator matched_filter_lag_aggregator_;

  RTC_DISALLOW_COPY_AND_ASSIGN(EchoPathDelayEstimator);
};
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_
