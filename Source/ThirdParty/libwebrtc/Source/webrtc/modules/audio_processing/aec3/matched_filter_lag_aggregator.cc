/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/matched_filter_lag_aggregator.h"

#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

MatchedFilterLagAggregator::MatchedFilterLagAggregator(
    ApmDataDumper* data_dumper,
    size_t num_lag_estimates)
    : data_dumper_(data_dumper), lag_updates_in_a_row_(num_lag_estimates, 0) {
  RTC_DCHECK(data_dumper);
  RTC_DCHECK_LT(0, num_lag_estimates);
}

MatchedFilterLagAggregator::~MatchedFilterLagAggregator() = default;

void MatchedFilterLagAggregator::Reset() {
  candidate_ = 0;
  candidate_counter_ = 0;
  std::fill(lag_updates_in_a_row_.begin(), lag_updates_in_a_row_.end(), 0.f);
}

rtc::Optional<size_t> MatchedFilterLagAggregator::Aggregate(
    rtc::ArrayView<const MatchedFilter::LagEstimate> lag_estimates) {
  RTC_DCHECK_EQ(lag_updates_in_a_row_.size(), lag_estimates.size());

  // Count the number of lag updates in a row to ensure that only stable lags
  // are taken into account.
  for (size_t k = 0; k < lag_estimates.size(); ++k) {
    lag_updates_in_a_row_[k] =
        lag_estimates[k].updated ? lag_updates_in_a_row_[k] + 1 : 0;
  }

  // If available, choose the strongest lag estimate as the best one.
  int best_lag_estimate_index = -1;
  for (size_t k = 0; k < lag_estimates.size(); ++k) {
    if (lag_updates_in_a_row_[k] > 10 && lag_estimates[k].reliable &&
        (best_lag_estimate_index == -1 ||
         lag_estimates[k].accuracy >
             lag_estimates[best_lag_estimate_index].accuracy)) {
      best_lag_estimate_index = k;
    }
  }

  // TODO(peah): Remove this logging once all development is done.
  data_dumper_->DumpRaw("aec3_echo_path_delay_estimator_best_index",
                        best_lag_estimate_index);

  // Require the same lag to be detected 10 times in a row before considering
  // it reliable.
  if (best_lag_estimate_index >= 0) {
    candidate_counter_ =
        (candidate_ == lag_estimates[best_lag_estimate_index].lag)
            ? candidate_counter_ + 1
            : 0;
    candidate_ = lag_estimates[best_lag_estimate_index].lag;
  }

  return candidate_counter_ >= 15 ? rtc::Optional<size_t>(candidate_)
                                  : rtc::Optional<size_t>();
}

}  // namespace webrtc
