/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/reverb_decay_estimator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "api/array_view.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

bool EnforceAdaptiveEchoReverbEstimation() {
  return field_trial::IsEnabled(
      "WebRTC-Aec3EnableAdaptiveEchoReverbEstimation");
}

constexpr int kEarlyReverbMinSizeBlocks = 3;
constexpr int kBlocksPerSection = 3;

// Averages the values in a block of size kFftLengthBy2;
float BlockAverage(rtc::ArrayView<const float> v, size_t block_index) {
  constexpr float kOneByFftLengthBy2 = 1.f / kFftLengthBy2;
  const int i = block_index * kFftLengthBy2;
  RTC_DCHECK_GE(v.size(), i + kFftLengthBy2);
  const float sum =
      std::accumulate(v.begin() + i, v.begin() + i + kFftLengthBy2, 0.f);
  return sum * kOneByFftLengthBy2;
}

// Analyzes the gain in a block.
void AnalyzeBlockGain(const std::array<float, kFftLengthBy2>& h2,
                      float floor_gain,
                      float* previous_gain,
                      bool* block_adapting,
                      bool* decaying_gain) {
  float gain = std::max(BlockAverage(h2, 0), 1e-32f);
  *block_adapting =
      *previous_gain > 1.1f * gain || *previous_gain < 0.9f * gain;
  *decaying_gain = gain > floor_gain;
  *previous_gain = gain;
}

// Arithmetic sum of $2 \sum_{i=0.5}^{(N-1)/2}i^2$ calculated directly.
constexpr float SymmetricArithmetricSum(int N) {
  return N * (N * N - 1.0f) * (1.f / 12.f);
}

}  // namespace

ReverbDecayEstimator::ReverbDecayEstimator(const EchoCanceller3Config& config)
    : filter_length_blocks_(config.filter.main.length_blocks),
      filter_length_coefficients_(GetTimeDomainLength(filter_length_blocks_)),
      use_adaptive_echo_decay_(config.ep_strength.default_len < 0.f ||
                               EnforceAdaptiveEchoReverbEstimation()),
      early_reverb_estimator_(config.filter.main.length_blocks -
                              kEarlyReverbMinSizeBlocks),
      late_reverb_start_(kEarlyReverbMinSizeBlocks),
      late_reverb_end_(kEarlyReverbMinSizeBlocks),
      decay_(std::fabs(config.ep_strength.default_len)) {
  previous_gains_.fill(0.f);
  RTC_DCHECK_GT(config.filter.main.length_blocks,
                static_cast<size_t>(kEarlyReverbMinSizeBlocks));
}

ReverbDecayEstimator::~ReverbDecayEstimator() = default;

void ReverbDecayEstimator::Update(rtc::ArrayView<const float> filter,
                                  const absl::optional<float>& filter_quality,
                                  int filter_delay_blocks,
                                  bool usable_linear_filter,
                                  bool stationary_signal) {
  const int filter_size = static_cast<int>(filter.size());

  if (stationary_signal) {
    return;
  }

  // TODO(devicentepena): Verify that the below is correct.
  bool estimation_feasible =
      filter_delay_blocks <=
      filter_length_blocks_ - kEarlyReverbMinSizeBlocks - 1;
  estimation_feasible =
      estimation_feasible && filter_size == filter_length_coefficients_;
  estimation_feasible = estimation_feasible && filter_delay_blocks > 0;
  estimation_feasible = estimation_feasible && usable_linear_filter;

  if (!estimation_feasible) {
    ResetDecayEstimation();
    return;
  }

  if (!use_adaptive_echo_decay_) {
    return;
  }

  const float new_smoothing = filter_quality ? *filter_quality * 0.2f : 0.f;
  smoothing_constant_ = std::max(new_smoothing, smoothing_constant_);
  if (smoothing_constant_ == 0.f) {
    return;
  }

  if (block_to_analyze_ < filter_length_blocks_) {
    // Analyze the filter and accumulate data for reverb estimation.
    AnalyzeFilter(filter);
    ++block_to_analyze_;
  } else {
    // When the filter is fully analyzed, estimate the reverb decay and reset
    // the block_to_analyze_ counter.
    EstimateDecay(filter);
  }
}

void ReverbDecayEstimator::ResetDecayEstimation() {
  early_reverb_estimator_.Reset();
  late_reverb_decay_estimator_.Reset(0);
  block_to_analyze_ = 0;
  estimation_region_candidate_size_ = 0;
  estimation_region_identified_ = false;
  smoothing_constant_ = 0.f;
  late_reverb_start_ = 0;
  late_reverb_end_ = 0;
}

void ReverbDecayEstimator::EstimateDecay(rtc::ArrayView<const float> filter) {
  auto& h = filter;

  RTC_DCHECK_EQ(0, h.size() % kFftLengthBy2);

  // Compute the squared filter coefficients.
  std::array<float, GetTimeDomainLength(kMaxAdaptiveFilterLength)> h2_data;
  RTC_DCHECK_GE(h2_data.size(), filter_length_coefficients_);
  rtc::ArrayView<float> h2(h2_data.data(), filter_length_coefficients_);
  std::transform(h.begin(), h.end(), h2.begin(), [](float a) { return a * a; });

  // Identify the peak index of the filter.
  const int peak_coefficient =
      std::distance(h2.begin(), std::max_element(h2.begin(), h2.end()));
  int peak_block = peak_coefficient >> kFftLengthBy2Log2;

  // Reset the block analysis counter.
  block_to_analyze_ =
      std::min(peak_block + kEarlyReverbMinSizeBlocks, filter_length_blocks_);

  // To estimate the reverb decay, the energy of the first filter section must
  // be substantially larger than the last. Also, the first filter section
  // energy must not deviate too much from the max peak.
  const float first_reverb_gain = BlockAverage(h2, block_to_analyze_);
  tail_gain_ = BlockAverage(h2, (h2.size() >> kFftLengthBy2Log2) - 1);
  const bool sufficient_reverb_decay = first_reverb_gain > 4.f * tail_gain_;
  const bool valid_filter =
      first_reverb_gain > 2.f * tail_gain_ && h2[peak_coefficient] < 100.f;

  // Estimate the size of the regions with early and late reflections.
  const int size_early_reverb = early_reverb_estimator_.Estimate();
  const int size_late_reverb =
      std::max(estimation_region_candidate_size_ - size_early_reverb, 0);

  // Only update the reverb decay estimate if the size of the identified late
  // reverb is sufficiently large.
  if (size_late_reverb >= 5) {
    if (valid_filter && late_reverb_decay_estimator_.EstimateAvailable()) {
      float decay = std::pow(
          2.0f, late_reverb_decay_estimator_.Estimate() * kFftLengthBy2);
      constexpr float kMaxDecay = 0.95f;  // ~1 sec min RT60.
      constexpr float kMinDecay = 0.02f;  // ~15 ms max RT60.
      decay = std::max(.97f * decay_, decay);
      decay = std::min(decay, kMaxDecay);
      decay = std::max(decay, kMinDecay);
      decay_ += smoothing_constant_ * (decay - decay_);
    }

    // Update length of decay. Must have enough data (number of sections) in
    // order to estimate decay rate.
    late_reverb_decay_estimator_.Reset(size_late_reverb * kFftLengthBy2);
    late_reverb_start_ =
        peak_block + kEarlyReverbMinSizeBlocks + size_early_reverb;
    late_reverb_end_ =
        block_to_analyze_ + estimation_region_candidate_size_ - 1;
  } else {
    late_reverb_decay_estimator_.Reset(0);
    late_reverb_start_ = 0;
    late_reverb_end_ = 0;
  }

  // Reset variables for the identification of the region for reverb decay
  // estimation.
  estimation_region_identified_ = !(valid_filter && sufficient_reverb_decay);
  estimation_region_candidate_size_ = 0;

  // Stop estimation of the decay until another good filter is received.
  smoothing_constant_ = 0.f;

  // Reset early reflections detector.
  early_reverb_estimator_.Reset();
}

void ReverbDecayEstimator::AnalyzeFilter(rtc::ArrayView<const float> filter) {
  auto h = rtc::ArrayView<const float>(
      filter.begin() + block_to_analyze_ * kFftLengthBy2, kFftLengthBy2);

  // Compute squared filter coeffiecients for the block to analyze_;
  std::array<float, kFftLengthBy2> h2;
  std::transform(h.begin(), h.end(), h2.begin(), [](float a) { return a * a; });

  // Map out the region for estimating the reverb decay.
  bool adapting;
  bool above_noise_floor;
  AnalyzeBlockGain(h2, tail_gain_, &previous_gains_[block_to_analyze_],
                   &adapting, &above_noise_floor);

  // Count consecutive number of "good" filter sections, where "good" means:
  // 1) energy is above noise floor.
  // 2) energy of current section has not changed too much from last check.
  estimation_region_identified_ =
      estimation_region_identified_ || adapting || !above_noise_floor;
  if (!estimation_region_identified_) {
    ++estimation_region_candidate_size_;
  }

  // Accumulate data for reverb decay estimation and for the estimation of early
  // reflections.
  if (block_to_analyze_ <= late_reverb_end_) {
    if (block_to_analyze_ >= late_reverb_start_) {
      for (float h2_k : h2) {
        float h2_log2 = FastApproxLog2f(h2_k + 1e-10);
        late_reverb_decay_estimator_.Accumulate(h2_log2);
        early_reverb_estimator_.Accumulate(h2_log2, smoothing_constant_);
      }
    } else {
      for (float h2_k : h2) {
        float h2_log2 = FastApproxLog2f(h2_k + 1e-10);
        early_reverb_estimator_.Accumulate(h2_log2, smoothing_constant_);
      }
    }
  }
}

void ReverbDecayEstimator::Dump(ApmDataDumper* data_dumper) const {
  data_dumper->DumpRaw("aec3_reverb_decay", decay_);
  data_dumper->DumpRaw("aec3_reverb_tail_energy", tail_gain_);
  data_dumper->DumpRaw("aec3_reverb_alpha", smoothing_constant_);
  data_dumper->DumpRaw("aec3_num_reverb_decay_blocks",
                       late_reverb_end_ - late_reverb_start_);
  data_dumper->DumpRaw("aec3_blocks_after_early_reflections",
                       late_reverb_start_);
  early_reverb_estimator_.Dump(data_dumper);
}

void ReverbDecayEstimator::LateReverbLinearRegressor::Reset(
    int num_data_points) {
  RTC_DCHECK_LE(0, num_data_points);
  RTC_DCHECK_EQ(0, num_data_points % 2);
  const int N = num_data_points;
  nz_ = 0.f;
  // Arithmetic sum of $2 \sum_{i=0.5}^{(N-1)/2}i^2$ calculated directly.
  nn_ = SymmetricArithmetricSum(N);
  // The linear regression approach assumes symmetric index around 0.
  count_ = N > 0 ? count_ = -N * 0.5f + 0.5f : 0.f;
  N_ = N;
  n_ = 0;
}

void ReverbDecayEstimator::LateReverbLinearRegressor::Accumulate(float z) {
  nz_ += count_ * z;
  ++count_;
  ++n_;
}

float ReverbDecayEstimator::LateReverbLinearRegressor::Estimate() {
  RTC_DCHECK(EstimateAvailable());
  if (nn_ == 0.f) {
    RTC_NOTREACHED();
    return 0.f;
  }
  return nz_ / nn_;
}

ReverbDecayEstimator::EarlyReverbLengthEstimator::EarlyReverbLengthEstimator(
    int max_blocks)
    : numerators_(1 + max_blocks / kBlocksPerSection, 0.f),
      nz_(numerators_.size(), 0.f),
      count_(numerators_.size(), 0.f) {
  RTC_DCHECK_LE(0, max_blocks);
}

ReverbDecayEstimator::EarlyReverbLengthEstimator::
    ~EarlyReverbLengthEstimator() = default;

void ReverbDecayEstimator::EarlyReverbLengthEstimator::Reset() {
  // Linear regression approach assumes symmetric index around 0.
  constexpr float kCount = -0.5f * kBlocksPerSection * kFftLengthBy2 + 0.5f;
  std::fill(count_.begin(), count_.end(), kCount);
  std::fill(nz_.begin(), nz_.end(), 0.f);
  section_ = 0;
  section_update_counter_ = 0;
}

void ReverbDecayEstimator::EarlyReverbLengthEstimator::Accumulate(
    float value,
    float smoothing) {
  nz_[section_] += count_[section_] * value;
  ++count_[section_];

  if (++section_update_counter_ == kBlocksPerSection * kFftLengthBy2) {
    RTC_DCHECK_GT(nz_.size(), section_);
    RTC_DCHECK_GT(numerators_.size(), section_);
    numerators_[section_] +=
        smoothing * (nz_[section_] - numerators_[section_]);
    section_update_counter_ = 0;
    ++section_;
  }
}

int ReverbDecayEstimator::EarlyReverbLengthEstimator::Estimate() {
  constexpr float N = kBlocksPerSection * kFftLengthBy2;
  constexpr float nn = SymmetricArithmetricSum(N);
  // numerator_11 refers to the quantity that the linear regressor needs in the
  // numerator for getting a decay equal to 1.1 (which is not a decay).
  // log2(1.1) * nn / kFftLengthBy2.
  constexpr float numerator_11 = 0.13750352374993502f * nn / kFftLengthBy2;
  // log2(0.8) *  nn / kFftLengthBy2.
  constexpr float numerator_08 = -0.32192809488736229f * nn / kFftLengthBy2;
  constexpr int kNumSectionsToAnalyze = 3;

  // Analyze the first kNumSectionsToAnalyze regions.
  // TODO(devicentepena): Add a more thorough comment for explaining the logic
  // below.
  const float min_stable_region = *std::min_element(
      numerators_.begin() + kNumSectionsToAnalyze, numerators_.end());
  int early_reverb_size = 0;
  for (int k = 0; k < kNumSectionsToAnalyze; ++k) {
    if ((numerators_[k] > numerator_11) ||
        (numerators_[k] < numerator_08 &&
         numerators_[k] < 0.9f * min_stable_region)) {
      early_reverb_size = (k + 1) * kBlocksPerSection;
    }
  }

  return early_reverb_size;
}

void ReverbDecayEstimator::EarlyReverbLengthEstimator::Dump(
    ApmDataDumper* data_dumper) const {
  data_dumper->DumpRaw("aec3_er_acum_numerator", numerators_);
}

}  // namespace webrtc
