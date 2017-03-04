/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/matched_filter.h"

#include "webrtc/typedefs.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <algorithm>
#include <numeric>

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {
namespace aec3 {

#if defined(WEBRTC_ARCH_X86_FAMILY)

void MatchedFilterCore_SSE2(size_t x_start_index,
                            float x2_sum_threshold,
                            rtc::ArrayView<const float> x,
                            rtc::ArrayView<const float> y,
                            rtc::ArrayView<float> h,
                            bool* filters_updated,
                            float* error_sum) {
  // Process for all samples in the sub-block.
  for (size_t i = 0; i < kSubBlockSize; ++i) {
    // Apply the matched filter as filter * x.  and compute x * x.
    float x2_sum = 0.f;
    float s = 0;
    size_t x_index = x_start_index;
    RTC_DCHECK_EQ(0, h.size() % 4);

    __m128 s_128 = _mm_set1_ps(0);
    __m128 x2_sum_128 = _mm_set1_ps(0);

    size_t k = 0;
    if (h.size() > (x.size() - x_index)) {
      const size_t limit = x.size() - x_index;
      for (; (k + 3) < limit; k += 4, x_index += 4) {
        const __m128 x_k = _mm_loadu_ps(&x[x_index]);
        const __m128 h_k = _mm_loadu_ps(&h[k]);
        const __m128 xx = _mm_mul_ps(x_k, x_k);
        x2_sum_128 = _mm_add_ps(x2_sum_128, xx);
        const __m128 hx = _mm_mul_ps(h_k, x_k);
        s_128 = _mm_add_ps(s_128, hx);
      }

      for (; k < limit; ++k, ++x_index) {
        x2_sum += x[x_index] * x[x_index];
        s += h[k] * x[x_index];
      }
      x_index = 0;
    }

    for (; k + 3 < h.size(); k += 4, x_index += 4) {
      const __m128 x_k = _mm_loadu_ps(&x[x_index]);
      const __m128 h_k = _mm_loadu_ps(&h[k]);
      const __m128 xx = _mm_mul_ps(x_k, x_k);
      x2_sum_128 = _mm_add_ps(x2_sum_128, xx);
      const __m128 hx = _mm_mul_ps(h_k, x_k);
      s_128 = _mm_add_ps(s_128, hx);
    }

    for (; k < h.size(); ++k, ++x_index) {
      x2_sum += x[x_index] * x[x_index];
      s += h[k] * x[x_index];
    }

    float* v = reinterpret_cast<float*>(&x2_sum_128);
    x2_sum += v[0] + v[1] + v[2] + v[3];
    v = reinterpret_cast<float*>(&s_128);
    s += v[0] + v[1] + v[2] + v[3];

    // Compute the matched filter error.
    const float e = std::min(32767.f, std::max(-32768.f, y[i] - s));
    (*error_sum) += e * e;

    // Update the matched filter estimate in an NLMS manner.
    if (x2_sum > x2_sum_threshold) {
      RTC_DCHECK_LT(0.f, x2_sum);
      const float alpha = 0.7f * e / x2_sum;

      // filter = filter + 0.7 * (y - filter * x) / x * x.
      size_t x_index = x_start_index;
      for (size_t k = 0; k < h.size(); ++k) {
        h[k] += alpha * x[x_index];
        x_index = x_index < (x.size() - 1) ? x_index + 1 : 0;
      }
      *filters_updated = true;
    }

    x_start_index = x_start_index > 0 ? x_start_index - 1 : x.size() - 1;
  }
}
#endif

void MatchedFilterCore(size_t x_start_index,
                       float x2_sum_threshold,
                       rtc::ArrayView<const float> x,
                       rtc::ArrayView<const float> y,
                       rtc::ArrayView<float> h,
                       bool* filters_updated,
                       float* error_sum) {
  // Process for all samples in the sub-block.
  for (size_t i = 0; i < kSubBlockSize; ++i) {
    // Apply the matched filter as filter * x.  and compute x * x.
    float x2_sum = 0.f;
    float s = 0;
    size_t x_index = x_start_index;
    for (size_t k = 0; k < h.size(); ++k) {
      x2_sum += x[x_index] * x[x_index];
      s += h[k] * x[x_index];
      x_index = x_index < (x.size() - 1) ? x_index + 1 : 0;
    }

    // Compute the matched filter error.
    const float e = std::min(32767.f, std::max(-32768.f, y[i] - s));
    (*error_sum) += e * e;

    // Update the matched filter estimate in an NLMS manner.
    if (x2_sum > x2_sum_threshold) {
      RTC_DCHECK_LT(0.f, x2_sum);
      const float alpha = 0.7f * e / x2_sum;

      // filter = filter + 0.7 * (y - filter * x) / x * x.
      size_t x_index = x_start_index;
      for (size_t k = 0; k < h.size(); ++k) {
        h[k] += alpha * x[x_index];
        x_index = x_index < (x.size() - 1) ? x_index + 1 : 0;
      }
      *filters_updated = true;
    }

    x_start_index = x_start_index > 0 ? x_start_index - 1 : x.size() - 1;
  }
}

}  // namespace aec3

MatchedFilter::IndexedBuffer::IndexedBuffer(size_t size) : data(size, 0.f) {
  RTC_DCHECK_EQ(0, size % kSubBlockSize);
}

MatchedFilter::IndexedBuffer::~IndexedBuffer() = default;

MatchedFilter::MatchedFilter(ApmDataDumper* data_dumper,
                             Aec3Optimization optimization,
                             size_t window_size_sub_blocks,
                             int num_matched_filters,
                             size_t alignment_shift_sub_blocks)
    : data_dumper_(data_dumper),
      optimization_(optimization),
      filter_intra_lag_shift_(alignment_shift_sub_blocks * kSubBlockSize),
      filters_(num_matched_filters,
               std::vector<float>(window_size_sub_blocks * kSubBlockSize, 0.f)),
      lag_estimates_(num_matched_filters),
      x_buffer_(kSubBlockSize *
                (alignment_shift_sub_blocks * num_matched_filters +
                 window_size_sub_blocks +
                 1)) {
  RTC_DCHECK(data_dumper);
  RTC_DCHECK_EQ(0, x_buffer_.data.size() % kSubBlockSize);
  RTC_DCHECK_LT(0, window_size_sub_blocks);
}

MatchedFilter::~MatchedFilter() = default;

void MatchedFilter::Update(const std::array<float, kSubBlockSize>& render,
                           const std::array<float, kSubBlockSize>& capture) {
  const std::array<float, kSubBlockSize>& x = render;
  const std::array<float, kSubBlockSize>& y = capture;

  const float x2_sum_threshold = filters_[0].size() * 150.f * 150.f;

  // Insert the new subblock into x_buffer.
  x_buffer_.index = (x_buffer_.index - kSubBlockSize + x_buffer_.data.size()) %
                    x_buffer_.data.size();
  RTC_DCHECK_LE(kSubBlockSize, x_buffer_.data.size() - x_buffer_.index);
  std::copy(x.rbegin(), x.rend(), x_buffer_.data.begin() + x_buffer_.index);

  // Apply all matched filters.
  size_t alignment_shift = 0;
  for (size_t n = 0; n < filters_.size(); ++n) {
    float error_sum = 0.f;
    bool filters_updated = false;
    size_t x_start_index =
        (x_buffer_.index + alignment_shift + kSubBlockSize - 1) %
        x_buffer_.data.size();

    switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
      case Aec3Optimization::kSse2:
        aec3::MatchedFilterCore_SSE2(x_start_index, x2_sum_threshold,
                                     x_buffer_.data, y, filters_[n],
                                     &filters_updated, &error_sum);
        break;
#endif
      default:
        aec3::MatchedFilterCore(x_start_index, x2_sum_threshold, x_buffer_.data,
                                y, filters_[n], &filters_updated, &error_sum);
    }

    // Compute anchor for the matched filter error.
    const float error_sum_anchor =
        std::inner_product(y.begin(), y.end(), y.begin(), 0.f);

    // Estimate the lag in the matched filter as the distance to the portion in
    // the filter that contributes the most to the matched filter output. This
    // is detected as the peak of the matched filter.
    const size_t lag_estimate = std::distance(
        filters_[n].begin(),
        std::max_element(
            filters_[n].begin(), filters_[n].end(),
            [](float a, float b) -> bool { return a * a < b * b; }));

    // Update the lag estimates for the matched filter.
    const float kMatchingFilterThreshold = 0.1f;
    lag_estimates_[n] = LagEstimate(
        error_sum_anchor - error_sum,
        (lag_estimate > 2 && lag_estimate < (filters_[n].size() - 10) &&
         error_sum < kMatchingFilterThreshold * error_sum_anchor),
        lag_estimate + alignment_shift, filters_updated);

    // TODO(peah): Remove once development of EchoCanceller3 is fully done.
    RTC_DCHECK_EQ(4, filters_.size());
    switch (n) {
      case 0:
        data_dumper_->DumpRaw("aec3_correlator_0_h", filters_[0]);
        break;
      case 1:
        data_dumper_->DumpRaw("aec3_correlator_1_h", filters_[1]);
        break;
      case 2:
        data_dumper_->DumpRaw("aec3_correlator_2_h", filters_[2]);
        break;
      case 3:
        data_dumper_->DumpRaw("aec3_correlator_3_h", filters_[3]);
        break;
      default:
        RTC_DCHECK(false);
    }

    alignment_shift += filter_intra_lag_shift_;
  }
}

}  // namespace webrtc
