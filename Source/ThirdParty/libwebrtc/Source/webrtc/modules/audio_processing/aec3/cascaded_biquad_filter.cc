/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/cascaded_biquad_filter.h"

#include "webrtc/base/checks.h"

namespace webrtc {

CascadedBiQuadFilter::CascadedBiQuadFilter(
    const CascadedBiQuadFilter::BiQuadCoefficients& coefficients,
    size_t num_biquads)
    : biquad_states_(num_biquads), coefficients_(coefficients) {}

CascadedBiQuadFilter::~CascadedBiQuadFilter() = default;

void CascadedBiQuadFilter::Process(rtc::ArrayView<const float> x,
                                   rtc::ArrayView<float> y) {
  ApplyBiQuad(x, y, &biquad_states_[0]);
  for (size_t k = 1; k < biquad_states_.size(); ++k) {
    ApplyBiQuad(y, y, &biquad_states_[k]);
  }
}

void CascadedBiQuadFilter::Process(rtc::ArrayView<float> y) {
  for (auto& biquad : biquad_states_) {
    ApplyBiQuad(y, y, &biquad);
  }
}

void CascadedBiQuadFilter::ApplyBiQuad(
    rtc::ArrayView<const float> x,
    rtc::ArrayView<float> y,
    CascadedBiQuadFilter::BiQuadState* biquad_state) {
  RTC_DCHECK_EQ(x.size(), y.size());
  RTC_DCHECK(biquad_state);
  const auto* c_b = coefficients_.b;
  const auto* c_a = coefficients_.a;
  auto* m_x = biquad_state->x;
  auto* m_y = biquad_state->y;
  for (size_t k = 0; k < x.size(); ++k) {
    const float tmp = x[k];
    y[k] = c_b[0] * tmp + c_b[1] * m_x[0] + c_b[2] * m_x[1] - c_a[0] * m_y[0] -
           c_a[1] * m_y[1];
    m_x[1] = m_x[0];
    m_x[0] = tmp;
    m_y[1] = m_y[0];
    m_y[0] = y[k];
  }
}

}  // namespace webrtc
