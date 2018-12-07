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

void GetActiveFrame(rtc::ArrayView<float> x) {
  const std::array<float, kBlockSize> frame = {
      7459.88, 17209.6, 17383,   20768.9, 16816.7, 18386.3, 4492.83, 9675.85,
      6665.52, 14808.6, 9342.3,  7483.28, 19261.7, 4145.98, 1622.18, 13475.2,
      7166.32, 6856.61, 21937,   7263.14, 9569.07, 14919,   8413.32, 7551.89,
      7848.65, 6011.27, 13080.6, 15865.2, 12656,   17459.6, 4263.93, 4503.03,
      9311.79, 21095.8, 12657.9, 13906.6, 19267.2, 11338.1, 16828.9, 11501.6,
      11405,   15031.4, 14541.6, 19765.5, 18346.3, 19350.2, 3157.47, 18095.8,
      1743.68, 21328.2, 19727.5, 7295.16, 10332.4, 11055.5, 20107.4, 14708.4,
      12416.2, 16434,   2454.69, 9840.8,  6867.23, 1615.75, 6059.9,  8394.19};
  RTC_DCHECK_GE(x.size(), frame.size());
  std::copy(frame.begin(), frame.end(), x.begin());
}

class TestInputs {
 public:
  explicit TestInputs(const EchoCanceller3Config& cfg);
  ~TestInputs();
  const RenderBuffer& GetRenderBuffer() { return *render_buffer_; }
  rtc::ArrayView<const float> GetX2() { return X2_; }
  rtc::ArrayView<const float> GetY2() { return Y2_; }
  rtc::ArrayView<const float> GetE2() { return E2_; }
  std::vector<std::array<float, kFftLengthBy2Plus1>> GetH2() { return H2_; }
  void Update();

 private:
  void UpdateCurrentPowerSpectra();
  int n_ = 0;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer_;
  RenderBuffer* render_buffer_;
  std::array<float, kFftLengthBy2Plus1> X2_;
  std::array<float, kFftLengthBy2Plus1> Y2_;
  std::array<float, kFftLengthBy2Plus1> E2_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2_;
  std::vector<std::vector<float>> x_;
};

TestInputs::TestInputs(const EchoCanceller3Config& cfg)
    : render_delay_buffer_(RenderDelayBuffer::Create2(cfg, 1)),
      H2_(cfg.filter.main.length_blocks),
      x_(1, std::vector<float>(kBlockSize, 0.f)) {
  render_delay_buffer_->SetDelay(4);
  render_buffer_ = render_delay_buffer_->GetRenderBuffer();
  for (auto& H : H2_) {
    H.fill(0.f);
  }
  H2_[0].fill(1.0f);
}

TestInputs::~TestInputs() = default;

void TestInputs::Update() {
  if (n_ % 2 == 0) {
    std::fill(x_[0].begin(), x_[0].end(), 0.f);
  } else {
    GetActiveFrame(x_[0]);
  }

  render_delay_buffer_->Insert(x_);
  render_delay_buffer_->PrepareCaptureProcessing();
  UpdateCurrentPowerSpectra();
  ++n_;
}

void TestInputs::UpdateCurrentPowerSpectra() {
  const VectorBuffer& spectrum_render_buffer =
      render_buffer_->GetSpectrumBuffer();
  size_t idx = render_buffer_->Position();
  size_t prev_idx = spectrum_render_buffer.OffsetIndex(idx, 1);
  auto& X2 = spectrum_render_buffer.buffer[idx];
  auto& X2_prev = spectrum_render_buffer.buffer[prev_idx];
  std::copy(X2.begin(), X2.end(), X2_.begin());
  RTC_DCHECK_EQ(X2.size(), Y2_.size());
  for (size_t k = 0; k < X2.size(); ++k) {
    E2_[k] = 0.01f * X2_prev[k];
    Y2_[k] = X2[k] + E2_[k];
  }
}

}  // namespace

TEST(SignalDependentErleEstimator, SweepSettings) {
  EchoCanceller3Config cfg;
  size_t max_length_blocks = 50;
  for (size_t blocks = 0; blocks < max_length_blocks; blocks = blocks + 10) {
    for (size_t delay_headroom = 0; delay_headroom < 5; ++delay_headroom) {
      for (size_t num_sections = 2; num_sections < max_length_blocks;
           ++num_sections) {
        cfg.filter.main.length_blocks = blocks;
        cfg.filter.main_initial.length_blocks =
            std::min(cfg.filter.main_initial.length_blocks, blocks);
        cfg.delay.delay_headroom_blocks = delay_headroom;
        cfg.erle.num_sections = num_sections;
        if (EchoCanceller3Config::Validate(&cfg)) {
          SignalDependentErleEstimator s(cfg);
          std::array<float, kFftLengthBy2Plus1> average_erle;
          average_erle.fill(cfg.erle.max_l);
          TestInputs inputs(cfg);
          for (size_t n = 0; n < 10; ++n) {
            inputs.Update();
            s.Update(inputs.GetRenderBuffer(), inputs.GetH2(), inputs.GetX2(),
                     inputs.GetY2(), inputs.GetE2(), average_erle, true);
          }
        }
      }
    }
  }
}

TEST(SignalDependentErleEstimator, LongerRun) {
  EchoCanceller3Config cfg;
  cfg.filter.main.length_blocks = 2;
  cfg.filter.main_initial.length_blocks = 1;
  cfg.delay.delay_headroom_blocks = 0;
  cfg.delay.hysteresis_limit_1_blocks = 0;
  cfg.erle.num_sections = 2;
  EXPECT_EQ(EchoCanceller3Config::Validate(&cfg), true);
  std::array<float, kFftLengthBy2Plus1> average_erle;
  average_erle.fill(cfg.erle.max_l);
  SignalDependentErleEstimator s(cfg);
  TestInputs inputs(cfg);
  for (size_t n = 0; n < 200; ++n) {
    inputs.Update();
    s.Update(inputs.GetRenderBuffer(), inputs.GetH2(), inputs.GetX2(),
             inputs.GetY2(), inputs.GetE2(), average_erle, true);
  }
}

}  // namespace webrtc
