/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/adaptive_fir_filter.h"

#include "webrtc/typedefs.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif
#include <algorithm>
#include <functional>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/aec3/fft_data.h"

namespace webrtc {

namespace {

// Constrains the a partiton of the frequency domain filter to be limited in
// time via setting the relevant time-domain coefficients to zero.
void Constrain(const Aec3Fft& fft, FftData* H) {
  std::array<float, kFftLength> h;
  fft.Ifft(*H, &h);
  constexpr float kScale = 1.0f / kFftLengthBy2;
  std::for_each(h.begin(), h.begin() + kFftLengthBy2,
                [kScale](float& a) { a *= kScale; });
  std::fill(h.begin() + kFftLengthBy2, h.end(), 0.f);
  fft.Fft(&h, H);
}

// Computes and stores the frequency response of the filter.
void UpdateFrequencyResponse(
    rtc::ArrayView<const FftData> H,
    std::vector<std::array<float, kFftLengthBy2Plus1>>* H2) {
  for (size_t k = 0; k < H.size(); ++k) {
    std::transform(H[k].re.begin(), H[k].re.end(), H[k].im.begin(),
                   (*H2)[k].begin(),
                   [](float a, float b) { return a * a + b * b; });
  }
}

// Computes and stores the echo return loss estimate of the filter, which is the
// sum of the partition frequency responses.
void UpdateErlEstimator(
    const std::vector<std::array<float, kFftLengthBy2Plus1>>& H2,
    std::array<float, kFftLengthBy2Plus1>* erl) {
  erl->fill(0.f);
  for (auto& H2_j : H2) {
    std::transform(H2_j.begin(), H2_j.end(), erl->begin(), erl->begin(),
                   std::plus<float>());
  }
}

// Resets the filter.
void ResetFilter(rtc::ArrayView<FftData> H) {
  for (auto& H_j : H) {
    H_j.Clear();
  }
}

}  // namespace

namespace aec3 {

// Adapts the filter partitions as H(t+1)=H(t)+G(t)*conj(X(t)).
void AdaptPartitions(const FftBuffer& X_buffer,
                     const FftData& G,
                     rtc::ArrayView<FftData> H) {
  rtc::ArrayView<const FftData> X_buffer_data = X_buffer.Buffer();
  size_t index = X_buffer.Position();
  for (auto& H_j : H) {
    const FftData& X = X_buffer_data[index];
    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      H_j.re[k] += X.re[k] * G.re[k] + X.im[k] * G.im[k];
      H_j.im[k] += X.re[k] * G.im[k] - X.im[k] * G.re[k];
    }

    index = index < (X_buffer_data.size() - 1) ? index + 1 : 0;
  }
}

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Adapts the filter partitions. (SSE2 variant)
void AdaptPartitions_SSE2(const FftBuffer& X_buffer,
                          const FftData& G,
                          rtc::ArrayView<FftData> H) {
  rtc::ArrayView<const FftData> X_buffer_data = X_buffer.Buffer();
  const int lim1 =
      std::min(X_buffer_data.size() - X_buffer.Position(), H.size());
  const int lim2 = H.size();
  constexpr int kNumFourBinBands = kFftLengthBy2 / 4;
  FftData* H_j;
  const FftData* X;
  int limit;
  int j;
  for (int k = 0, n = 0; n < kNumFourBinBands; ++n, k += 4) {
    const __m128 G_re = _mm_loadu_ps(&G.re[k]);
    const __m128 G_im = _mm_loadu_ps(&G.im[k]);

    H_j = &H[0];
    X = &X_buffer_data[X_buffer.Position()];
    limit = lim1;
    j = 0;
    do {
      for (; j < limit; ++j, ++H_j, ++X) {
        const __m128 X_re = _mm_loadu_ps(&X->re[k]);
        const __m128 X_im = _mm_loadu_ps(&X->im[k]);
        const __m128 H_re = _mm_loadu_ps(&H_j->re[k]);
        const __m128 H_im = _mm_loadu_ps(&H_j->im[k]);
        const __m128 a = _mm_mul_ps(X_re, G_re);
        const __m128 b = _mm_mul_ps(X_im, G_im);
        const __m128 c = _mm_mul_ps(X_re, G_im);
        const __m128 d = _mm_mul_ps(X_im, G_re);
        const __m128 e = _mm_add_ps(a, b);
        const __m128 f = _mm_sub_ps(c, d);
        const __m128 g = _mm_add_ps(H_re, e);
        const __m128 h = _mm_add_ps(H_im, f);
        _mm_storeu_ps(&H_j->re[k], g);
        _mm_storeu_ps(&H_j->im[k], h);
      }

      X = &X_buffer_data[0];
      limit = lim2;
    } while (j < lim2);
  }

  H_j = &H[0];
  X = &X_buffer_data[X_buffer.Position()];
  limit = lim1;
  j = 0;
  do {
    for (; j < limit; ++j, ++H_j, ++X) {
      H_j->re[kFftLengthBy2] += X->re[kFftLengthBy2] * G.re[kFftLengthBy2] +
                                X->im[kFftLengthBy2] * G.im[kFftLengthBy2];
      H_j->im[kFftLengthBy2] += X->re[kFftLengthBy2] * G.im[kFftLengthBy2] -
                                X->im[kFftLengthBy2] * G.re[kFftLengthBy2];
    }

    X = &X_buffer_data[0];
    limit = lim2;
  } while (j < lim2);
}
#endif

// Produces the filter output.
void ApplyFilter(const FftBuffer& X_buffer,
                 rtc::ArrayView<const FftData> H,
                 FftData* S) {
  S->re.fill(0.f);
  S->im.fill(0.f);

  rtc::ArrayView<const FftData> X_buffer_data = X_buffer.Buffer();
  size_t index = X_buffer.Position();
  for (auto& H_j : H) {
    const FftData& X = X_buffer_data[index];
    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      S->re[k] += X.re[k] * H_j.re[k] - X.im[k] * H_j.im[k];
      S->im[k] += X.re[k] * H_j.im[k] + X.im[k] * H_j.re[k];
    }
    index = index < (X_buffer_data.size() - 1) ? index + 1 : 0;
  }
}

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Produces the filter output (SSE2 variant).
void ApplyFilter_SSE2(const FftBuffer& X_buffer,
                      rtc::ArrayView<const FftData> H,
                      FftData* S) {
  S->re.fill(0.f);
  S->im.fill(0.f);

  rtc::ArrayView<const FftData> X_buffer_data = X_buffer.Buffer();
  const int lim1 =
      std::min(X_buffer_data.size() - X_buffer.Position(), H.size());
  const int lim2 = H.size();
  constexpr int kNumFourBinBands = kFftLengthBy2 / 4;
  const FftData* H_j = &H[0];
  const FftData* X = &X_buffer_data[X_buffer.Position()];

  int j = 0;
  int limit = lim1;
  do {
    for (; j < limit; ++j, ++H_j, ++X) {
      for (int k = 0, n = 0; n < kNumFourBinBands; ++n, k += 4) {
        const __m128 X_re = _mm_loadu_ps(&X->re[k]);
        const __m128 X_im = _mm_loadu_ps(&X->im[k]);
        const __m128 H_re = _mm_loadu_ps(&H_j->re[k]);
        const __m128 H_im = _mm_loadu_ps(&H_j->im[k]);
        const __m128 S_re = _mm_loadu_ps(&S->re[k]);
        const __m128 S_im = _mm_loadu_ps(&S->im[k]);
        const __m128 a = _mm_mul_ps(X_re, H_re);
        const __m128 b = _mm_mul_ps(X_im, H_im);
        const __m128 c = _mm_mul_ps(X_re, H_im);
        const __m128 d = _mm_mul_ps(X_im, H_re);
        const __m128 e = _mm_sub_ps(a, b);
        const __m128 f = _mm_add_ps(c, d);
        const __m128 g = _mm_add_ps(S_re, e);
        const __m128 h = _mm_add_ps(S_im, f);
        _mm_storeu_ps(&S->re[k], g);
        _mm_storeu_ps(&S->im[k], h);
      }
    }
    limit = lim2;
    X = &X_buffer_data[0];
  } while (j < lim2);

  H_j = &H[0];
  X = &X_buffer_data[X_buffer.Position()];
  j = 0;
  limit = lim1;
  do {
    for (; j < limit; ++j, ++H_j, ++X) {
      S->re[kFftLengthBy2] += X->re[kFftLengthBy2] * H_j->re[kFftLengthBy2] -
                              X->im[kFftLengthBy2] * H_j->im[kFftLengthBy2];
      S->im[kFftLengthBy2] += X->re[kFftLengthBy2] * H_j->im[kFftLengthBy2] +
                              X->im[kFftLengthBy2] * H_j->re[kFftLengthBy2];
    }
    limit = lim2;
    X = &X_buffer_data[0];
  } while (j < lim2);
}
#endif

}  // namespace aec3

AdaptiveFirFilter::AdaptiveFirFilter(size_t size_partitions,
                                     bool use_filter_statistics,
                                     Aec3Optimization optimization,
                                     ApmDataDumper* data_dumper)
    : data_dumper_(data_dumper),
      fft_(),
      optimization_(optimization),
      H_(size_partitions) {
  RTC_DCHECK(data_dumper_);
  ResetFilter(H_);

  if (use_filter_statistics) {
    H2_.reset(new std::vector<std::array<float, kFftLengthBy2Plus1>>(
        size_partitions, std::array<float, kFftLengthBy2Plus1>()));
    for (auto H2_k : *H2_) {
      H2_k.fill(0.f);
    }

    erl_.reset(new std::array<float, kFftLengthBy2Plus1>());
    erl_->fill(0.f);
  }
}

AdaptiveFirFilter::~AdaptiveFirFilter() = default;

void AdaptiveFirFilter::HandleEchoPathChange() {
  ResetFilter(H_);
  if (H2_) {
    for (auto H2_k : *H2_) {
      H2_k.fill(0.f);
    }
    RTC_DCHECK(erl_);
    erl_->fill(0.f);
  }
}

void AdaptiveFirFilter::Filter(const FftBuffer& X_buffer, FftData* S) const {
  RTC_DCHECK(S);
  switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    case Aec3Optimization::kSse2:
      aec3::ApplyFilter_SSE2(X_buffer, H_, S);
      break;
#endif
    default:
      aec3::ApplyFilter(X_buffer, H_, S);
  }
}

void AdaptiveFirFilter::Adapt(const FftBuffer& X_buffer, const FftData& G) {
  // Adapt the filter.
  switch (optimization_) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    case Aec3Optimization::kSse2:
      aec3::AdaptPartitions_SSE2(X_buffer, G, H_);
      break;
#endif
    default:
      aec3::AdaptPartitions(X_buffer, G, H_);
  }

  // Constrain the filter partitions in a cyclic manner.
  Constrain(fft_, &H_[partition_to_constrain_]);
  partition_to_constrain_ = partition_to_constrain_ < (H_.size() - 1)
                                ? partition_to_constrain_ + 1
                                : 0;

  // Optionally update the frequency response and echo return loss for the
  // filter.
  if (H2_) {
    RTC_DCHECK(erl_);
    UpdateFrequencyResponse(H_, H2_.get());
    UpdateErlEstimator(*H2_, erl_.get());
  }
}

}  // namespace webrtc
