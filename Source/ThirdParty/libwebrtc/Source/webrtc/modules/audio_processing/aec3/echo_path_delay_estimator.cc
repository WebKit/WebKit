/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/aec3/echo_path_delay_estimator.h"

#include <algorithm>
#include <array>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"

namespace webrtc {

EchoPathDelayEstimator::EchoPathDelayEstimator(
    ApmDataDumper* data_dumper,
    const EchoCanceller3Config& config)
    : data_dumper_(data_dumper),
      down_sampling_factor_(config.delay.down_sampling_factor),
      sub_block_size_(down_sampling_factor_ != 0
                          ? kBlockSize / down_sampling_factor_
                          : kBlockSize),
      capture_decimator_(down_sampling_factor_),
      matched_filter_(data_dumper_,
                      DetectOptimization(),
                      sub_block_size_,
                      kMatchedFilterWindowSizeSubBlocks,
                      config.delay.num_filters,
                      kMatchedFilterAlignmentShiftSizeSubBlocks,
                      config.render_levels.poor_excitation_render_limit),
      matched_filter_lag_aggregator_(data_dumper_,
                                     matched_filter_.GetMaxFilterLag()) {
  RTC_DCHECK(data_dumper);
  RTC_DCHECK(down_sampling_factor_ > 0);
}

EchoPathDelayEstimator::~EchoPathDelayEstimator() = default;

void EchoPathDelayEstimator::Reset() {
  matched_filter_lag_aggregator_.Reset();
  matched_filter_.Reset();
}

rtc::Optional<size_t> EchoPathDelayEstimator::EstimateDelay(
    const DownsampledRenderBuffer& render_buffer,
    rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());

  std::array<float, kBlockSize> downsampled_capture_data;
  rtc::ArrayView<float> downsampled_capture(downsampled_capture_data.data(),
                                            sub_block_size_);
  data_dumper_->DumpWav("aec3_capture_decimator_input", capture.size(),
                        capture.data(), 16000, 1);
  capture_decimator_.Decimate(capture, downsampled_capture);
  data_dumper_->DumpWav("aec3_capture_decimator_output",
                        downsampled_capture.size(), downsampled_capture.data(),
                        16000 / down_sampling_factor_, 1);
  matched_filter_.Update(render_buffer, downsampled_capture);

  rtc::Optional<size_t> aggregated_matched_filter_lag =
      matched_filter_lag_aggregator_.Aggregate(
          matched_filter_.GetLagEstimates());

  // TODO(peah): Move this logging outside of this class once EchoCanceller3
  // development is done.
  data_dumper_->DumpRaw("aec3_echo_path_delay_estimator_delay",
                        aggregated_matched_filter_lag
                            ? static_cast<int>(*aggregated_matched_filter_lag *
                                               down_sampling_factor_)
                            : -1);

  // Return the detected delay in samples as the aggregated matched filter lag
  // compensated by the down sampling factor for the signal being correlated.
  return aggregated_matched_filter_lag
             ? rtc::Optional<size_t>(*aggregated_matched_filter_lag *
                                     down_sampling_factor_)
             : rtc::nullopt;
}

}  // namespace webrtc
