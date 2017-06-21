/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_

#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/decimator_by_4.h"
#include "webrtc/modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "webrtc/modules/audio_processing/aec3/matched_filter.h"
#include "webrtc/modules/audio_processing/aec3/matched_filter_lag_aggregator.h"

namespace webrtc {

class ApmDataDumper;

// Estimates the delay of the echo path.
class EchoPathDelayEstimator {
 public:
  explicit EchoPathDelayEstimator(ApmDataDumper* data_dumper);
  ~EchoPathDelayEstimator();

  // Resets the estimation.
  void Reset();

  // Produce a delay estimate if such is avaliable.
  rtc::Optional<size_t> EstimateDelay(
      const DownsampledRenderBuffer& render_buffer,
      rtc::ArrayView<const float> capture);

 private:
  ApmDataDumper* const data_dumper_;
  DecimatorBy4 capture_decimator_;
  MatchedFilter matched_filter_;
  MatchedFilterLagAggregator matched_filter_lag_aggregator_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoPathDelayEstimator);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_
