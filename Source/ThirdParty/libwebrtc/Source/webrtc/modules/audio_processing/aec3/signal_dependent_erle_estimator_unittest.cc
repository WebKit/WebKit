/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/signal_dependent_erle_estimator.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

void GetActiveFrame(Block* x) {
  const std::array<float, kBlockSize> frame = {
      7459.88, 17209.6, 17383,   20768.9, 16816.7, 18386.3, 4492.83, 9675.85,
      6665.52, 14808.6, 9342.3,  7483.28, 19261.7, 4145.98, 1622.18, 13475.2,
      7166.32, 6856.61, 21937,   7263.14, 9569.07, 14919,   8413.32, 7551.89,
      7848.65, 6011.27, 13080.6, 15865.2, 12656,   17459.6, 4263.93, 4503.03,
      9311.79, 21095.8, 12657.9, 13906.6, 19267.2, 11338.1, 16828.9, 11501.6,
      11405,   15031.4, 14541.6, 19765.5, 18346.3, 19350.2, 3157.47, 18095.8,
      1743.68, 21328.2, 19727.5, 7295.16, 10332.4, 11055.5, 20107.4, 14708.4,
      12416.2, 16434,   2454.69, 9840.8,  6867.23, 1615.75, 6059.9,  8394.19};
  for (int band = 0; band < x->NumBands(); ++band) {
    for (int channel = 0; channel < x->NumChannels(); ++channel) {
      RTC_DCHECK_GE(kBlockSize, frame.size());
      std::copy(frame.begin(), frame.end(), x->begin(band, channel));
    }
  }
}

class TestInputs {
 public:
  TestInputs(const EchoCanceller3Config& cfg,
             size_t num_render_channels,
             size_t num_capture_channels);
  ~TestInputs();
  const RenderBuffer& GetRenderBuffer() { return *render_buffer_; }
  rtc::ArrayView<const float, kFftLengthBy2Plus1> GetX2() { return X2_; }
  rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> GetY2() const {
    return Y2_;
  }
  rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> GetE2() const {
    return E2_;
  }
  rtc::ArrayView<const std::vector<std::array<float, kFftLengthBy2Plus1>>>
  GetH2() const {
    return H2_;
  }
  const std::vector<bool>& GetConvergedFilters() const {
    return converged_filters_;
  }
  void Update();

 private:
  void UpdateCurrentPowerSpectra();
  int n_ = 0;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer_;
  RenderBuffer* render_buffer_;
  std::array<float, kFftLengthBy2Plus1> X2_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2_;
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>> H2_;
  Block x_;
  std::vector<bool> converged_filters_;
};

TestInputs::TestInputs(const EchoCanceller3Config& cfg,
                       size_t num_render_channels,
                       size_t num_capture_channels)
    : render_delay_buffer_(
          RenderDelayBuffer::Create(cfg, 16000, num_render_channels)),
      Y2_(num_capture_channels),
      E2_(num_capture_channels),
      H2_(num_capture_channels,
          std::vector<std::array<float, kFftLengthBy2Plus1>>(
              cfg.filter.refined.length_blocks)),
      x_(1, num_render_channels),
      converged_filters_(num_capture_channels, true) {
  render_delay_buffer_->AlignFromDelay(4);
  render_buffer_ = render_delay_buffer_->GetRenderBuffer();
  for (auto& H2_ch : H2_) {
    for (auto& H2_p : H2_ch) {
      H2_p.fill(0.f);
    }
  }
  for (auto& H2_p : H2_[0]) {
    H2_p.fill(1.f);
  }
}

TestInputs::~TestInputs() = default;

void TestInputs::Update() {
  if (n_ % 2 == 0) {
    std::fill(x_.begin(/*band=*/0, /*channel=*/0),
              x_.end(/*band=*/0, /*channel=*/0), 0.f);
  } else {
    GetActiveFrame(&x_);
  }

  render_delay_buffer_->Insert(x_);
  render_delay_buffer_->PrepareCaptureProcessing();
  UpdateCurrentPowerSpectra();
  ++n_;
}

void TestInputs::UpdateCurrentPowerSpectra() {
  const SpectrumBuffer& spectrum_render_buffer =
      render_buffer_->GetSpectrumBuffer();
  size_t idx = render_buffer_->Position();
  size_t prev_idx = spectrum_render_buffer.OffsetIndex(idx, 1);
  auto& X2 = spectrum_render_buffer.buffer[idx][/*channel=*/0];
  auto& X2_prev = spectrum_render_buffer.buffer[prev_idx][/*channel=*/0];
  std::copy(X2.begin(), X2.end(), X2_.begin());
  for (size_t ch = 0; ch < Y2_.size(); ++ch) {
    RTC_DCHECK_EQ(X2.size(), Y2_[ch].size());
    for (size_t k = 0; k < X2.size(); ++k) {
      E2_[ch][k] = 0.01f * X2_prev[k];
      Y2_[ch][k] = X2[k] + E2_[ch][k];
    }
  }
}

}  // namespace

class SignalDependentErleEstimatorMultiChannel
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         SignalDependentErleEstimatorMultiChannel,
                         ::testing::Combine(::testing::Values(1, 2, 4),
                                            ::testing::Values(1, 2, 4)));

TEST_P(SignalDependentErleEstimatorMultiChannel, SweepSettings) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());
  EchoCanceller3Config cfg;
  size_t max_length_blocks = 50;
  for (size_t blocks = 1; blocks < max_length_blocks; blocks = blocks + 10) {
    for (size_t delay_headroom = 0; delay_headroom < 5; ++delay_headroom) {
      for (size_t num_sections = 2; num_sections < max_length_blocks;
           ++num_sections) {
        cfg.filter.refined.length_blocks = blocks;
        cfg.filter.refined_initial.length_blocks =
            std::min(cfg.filter.refined_initial.length_blocks, blocks);
        cfg.delay.delay_headroom_samples = delay_headroom * kBlockSize;
        cfg.erle.num_sections = num_sections;
        if (EchoCanceller3Config::Validate(&cfg)) {
          SignalDependentErleEstimator s(cfg, num_capture_channels);
          std::vector<std::array<float, kFftLengthBy2Plus1>> average_erle(
              num_capture_channels);
          for (auto& e : average_erle) {
            e.fill(cfg.erle.max_l);
          }
          TestInputs inputs(cfg, num_render_channels, num_capture_channels);
          for (size_t n = 0; n < 10; ++n) {
            inputs.Update();
            s.Update(inputs.GetRenderBuffer(), inputs.GetH2(), inputs.GetX2(),
                     inputs.GetY2(), inputs.GetE2(), average_erle, average_erle,
                     inputs.GetConvergedFilters());
          }
        }
      }
    }
  }
}

TEST_P(SignalDependentErleEstimatorMultiChannel, LongerRun) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());
  EchoCanceller3Config cfg;
  cfg.filter.refined.length_blocks = 2;
  cfg.filter.refined_initial.length_blocks = 1;
  cfg.delay.delay_headroom_samples = 0;
  cfg.delay.hysteresis_limit_blocks = 0;
  cfg.erle.num_sections = 2;
  EXPECT_EQ(EchoCanceller3Config::Validate(&cfg), true);
  std::vector<std::array<float, kFftLengthBy2Plus1>> average_erle(
      num_capture_channels);
  for (auto& e : average_erle) {
    e.fill(cfg.erle.max_l);
  }
  SignalDependentErleEstimator s(cfg, num_capture_channels);
  TestInputs inputs(cfg, num_render_channels, num_capture_channels);
  for (size_t n = 0; n < 200; ++n) {
    inputs.Update();
    s.Update(inputs.GetRenderBuffer(), inputs.GetH2(), inputs.GetX2(),
             inputs.GetY2(), inputs.GetE2(), average_erle, average_erle,
             inputs.GetConvergedFilters());
  }
}

}  // namespace webrtc
