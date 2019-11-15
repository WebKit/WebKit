/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_

#include <stddef.h>

#include <array>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class ApmDataDumper;
class RenderBuffer;

// Class for analyzing the properties of an adaptive filter.
class FilterAnalyzer {
 public:
  explicit FilterAnalyzer(const EchoCanceller3Config& config);
  ~FilterAnalyzer();

  // Resets the analysis.
  void Reset();

  // Updates the estimates with new input data.
  void Update(rtc::ArrayView<const float> filter_time_domain,
              const RenderBuffer& render_buffer);

  // Returns the delay of the filter in terms of blocks.
  int DelayBlocks() const { return delay_blocks_; }

  // Returns whether the filter is consistent in the sense that it does not
  // change much over time.
  bool Consistent() const { return consistent_estimate_; }

  // Returns the estimated filter gain.
  float Gain() const { return gain_; }

  // Returns the number of blocks for the current used filter.
  int FilterLengthBlocks() const { return filter_length_blocks_; }

  // Returns the preprocessed filter.
  rtc::ArrayView<const float> GetAdjustedFilter() const { return h_highpass_; }

  // Public for testing purposes only.
  void SetRegionToAnalyze(rtc::ArrayView<const float> filter_time_domain);

 private:
  void AnalyzeRegion(rtc::ArrayView<const float> filter_time_domain,
                     const RenderBuffer& render_buffer);

  void UpdateFilterGain(rtc::ArrayView<const float> filter_time_domain,
                        size_t max_index);
  void PreProcessFilter(rtc::ArrayView<const float> filter_time_domain);

  void ResetRegion();

  struct FilterRegion {
    size_t start_sample_;
    size_t end_sample_;
  };

  // This class checks whether the shape of the impulse response has been
  // consistent over time.
  class ConsistentFilterDetector {
   public:
    explicit ConsistentFilterDetector(const EchoCanceller3Config& config);
    void Reset();
    bool Detect(rtc::ArrayView<const float> filter_to_analyze,
                const FilterRegion& region,
                rtc::ArrayView<const float> x_block,
                size_t peak_index,
                int delay_blocks);

   private:
    bool significant_peak_;
    float filter_floor_accum_;
    float filter_secondary_peak_;
    size_t filter_floor_low_limit_;
    size_t filter_floor_high_limit_;
    const float active_render_threshold_;
    size_t consistent_estimate_counter_ = 0;
    int consistent_delay_reference_ = -10;
  };

  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const bool bounded_erl_;
  const float default_gain_;
  std::vector<float> h_highpass_;
  int delay_blocks_ = 0;
  size_t blocks_since_reset_ = 0;
  bool consistent_estimate_ = false;
  float gain_;
  size_t peak_index_;
  int filter_length_blocks_;
  FilterRegion region_;
  ConsistentFilterDetector consistent_filter_detector_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FilterAnalyzer);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_FILTER_ANALYZER_H_
