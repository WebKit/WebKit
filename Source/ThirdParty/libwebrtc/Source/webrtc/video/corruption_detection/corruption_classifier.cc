/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/corruption_classifier.h"

#include <algorithm>
#include <cmath>
#include <variant>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {

CorruptionClassifier::CorruptionClassifier(float scale_factor)
    : config_(ScalarConfig{.scale_factor = scale_factor}) {
  RTC_CHECK_GT(scale_factor, 0) << "The scale factor must be positive.";
  RTC_LOG(LS_INFO) << "Calculating corruption probability using scale factor.";
}

CorruptionClassifier::CorruptionClassifier(float growth_rate, float midpoint)
    : config_(LogisticFunctionConfig{.growth_rate = growth_rate,
                                     .midpoint = midpoint}) {
  RTC_CHECK_GT(growth_rate, 0)
      << "As the `score` is defined now (low score means probably not "
         "corrupted and vice versa), the growth rate must be positive to have "
         "a logistic function that is monotonically increasing.";
  RTC_LOG(LS_INFO)
      << "Calculating corruption probability using logistic function.";
}

double CorruptionClassifier::CalculateCorruptionProbability(
    rtc::ArrayView<const FilteredSample> filtered_original_samples,
    rtc::ArrayView<const FilteredSample> filtered_compressed_samples,
    int luma_threshold,
    int chroma_threshold) const {
  double loss = GetScore(filtered_original_samples, filtered_compressed_samples,
                         luma_threshold, chroma_threshold);

  if (const auto* scalar_config = std::get_if<ScalarConfig>(&config_)) {
    // Fitting the unbounded loss to the interval of [0, 1] using a simple scale
    // factor and capping the loss to 1.
    return std::min(loss / scalar_config->scale_factor, 1.0);
  }

  const auto config = std::get_if<LogisticFunctionConfig>(&config_);
  RTC_DCHECK(config);
  // Fitting the unbounded loss to the interval of [0, 1] using the logistic
  // function.
  return 1 / (1 + std::exp(-config->growth_rate * (loss - config->midpoint)));
}

// The score is calculated according to the following formula :
//
// score = (sum_i max{(|original_i - compressed_i| - threshold, 0)^2}) / N
//
// where N is the number of samples, i in [0, N), and the threshold is
// either `luma_threshold` or `chroma_threshold` depending on whether the
// sample is luma or chroma.
double CorruptionClassifier::GetScore(
    rtc::ArrayView<const FilteredSample> filtered_original_samples,
    rtc::ArrayView<const FilteredSample> filtered_compressed_samples,
    int luma_threshold,
    int chroma_threshold) const {
  RTC_DCHECK_GE(luma_threshold, 0);
  RTC_DCHECK_GE(chroma_threshold, 0);
  RTC_CHECK_EQ(filtered_original_samples.size(),
               filtered_compressed_samples.size())
      << "The original and compressed frame have different amounts of "
         "filtered samples.";
  RTC_CHECK_GT(filtered_original_samples.size(), 0);
  const int num_samples = filtered_original_samples.size();
  double sum = 0.0;
  for (int i = 0; i < num_samples; ++i) {
    RTC_CHECK_EQ(filtered_original_samples[i].plane,
                 filtered_compressed_samples[i].plane);
    double abs_diff = std::abs(filtered_original_samples[i].value -
                               filtered_compressed_samples[i].value);
    switch (filtered_original_samples[i].plane) {
      case ImagePlane::kLuma:
        if (abs_diff > luma_threshold) {
          sum += std::pow(abs_diff - luma_threshold, 2);
        }
        break;
      case ImagePlane::kChroma:
        if (abs_diff > chroma_threshold) {
          sum += std::pow(abs_diff - chroma_threshold, 2);
        }
        break;
    }
  }

  return sum / num_samples;
}

}  // namespace webrtc
