/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/residual_echo_estimator.h"

#include <numeric>

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kSampleRateHz = 48000;
constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);
constexpr float kEpsilon = 1e-4f;
}  // namespace

class ResidualEchoEstimatorTest {
 public:
  ResidualEchoEstimatorTest(size_t num_render_channels,
                            size_t num_capture_channels,
                            const EchoCanceller3Config& config)
      : num_render_channels_(num_render_channels),
        num_capture_channels_(num_capture_channels),
        config_(config),
        estimator_(config_, num_render_channels_),
        aec_state_(config_, num_capture_channels_),
        render_delay_buffer_(RenderDelayBuffer::Create(config_,
                                                       kSampleRateHz,
                                                       num_render_channels_)),
        E2_refined_(num_capture_channels_),
        S2_linear_(num_capture_channels_),
        Y2_(num_capture_channels_),
        R2_(num_capture_channels_),
        R2_unbounded_(num_capture_channels_),
        x_(kNumBands, num_render_channels_),
        H2_(num_capture_channels_,
            std::vector<std::array<float, kFftLengthBy2Plus1>>(10)),
        h_(num_capture_channels_,
           std::vector<float>(
               GetTimeDomainLength(config_.filter.refined.length_blocks),
               0.0f)),
        random_generator_(42U),
        output_(num_capture_channels_) {
    for (auto& H2_ch : H2_) {
      for (auto& H2_k : H2_ch) {
        H2_k.fill(0.01f);
      }
      H2_ch[2].fill(10.f);
      H2_ch[2][0] = 0.1f;
    }

    for (auto& subtractor_output : output_) {
      subtractor_output.Reset();
      subtractor_output.s_refined.fill(100.f);
    }
    y_.fill(0.f);

    constexpr float kLevel = 10.f;
    for (auto& E2_refined_ch : E2_refined_) {
      E2_refined_ch.fill(kLevel);
    }
    S2_linear_[0].fill(kLevel);
    for (auto& Y2_ch : Y2_) {
      Y2_ch.fill(kLevel);
    }
  }

  void RunOneFrame(bool dominant_nearend) {
    RandomizeSampleVector(&random_generator_,
                          x_.View(/*band=*/0, /*channel=*/0));
    render_delay_buffer_->Insert(x_);
    if (first_frame_) {
      render_delay_buffer_->Reset();
      first_frame_ = false;
    }
    render_delay_buffer_->PrepareCaptureProcessing();

    aec_state_.Update(delay_estimate_, H2_, h_,
                      *render_delay_buffer_->GetRenderBuffer(), E2_refined_,
                      Y2_, output_);

    estimator_.Estimate(aec_state_, *render_delay_buffer_->GetRenderBuffer(),
                        S2_linear_, Y2_, dominant_nearend, R2_, R2_unbounded_);
  }

  rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> R2() const {
    return R2_;
  }

 private:
  const size_t num_render_channels_;
  const size_t num_capture_channels_;
  const EchoCanceller3Config& config_;
  ResidualEchoEstimator estimator_;
  AecState aec_state_;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2_refined_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> S2_linear_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> R2_unbounded_;
  Block x_;
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>> H2_;
  std::vector<std::vector<float>> h_;
  Random random_generator_;
  std::vector<SubtractorOutput> output_;
  std::array<float, kBlockSize> y_;
  std::optional<DelayEstimate> delay_estimate_;
  bool first_frame_ = true;
};

class ResidualEchoEstimatorMultiChannel
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         ResidualEchoEstimatorMultiChannel,
                         ::testing::Combine(::testing::Values(1, 2, 4),
                                            ::testing::Values(1, 2, 4)));

TEST_P(ResidualEchoEstimatorMultiChannel, BasicTest) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());

  EchoCanceller3Config config;
  ResidualEchoEstimatorTest residual_echo_estimator_test(
      num_render_channels, num_capture_channels, config);
  for (int k = 0; k < 1993; ++k) {
    residual_echo_estimator_test.RunOneFrame(/*dominant_nearend=*/false);
  }
}

TEST(ResidualEchoEstimatorMultiChannel, ReverbTest) {
  const size_t num_render_channels = 1;
  const size_t num_capture_channels = 1;
  const size_t nFrames = 100;

  EchoCanceller3Config reference_config;
  reference_config.ep_strength.default_len = 0.95f;
  reference_config.ep_strength.nearend_len = 0.95f;
  EchoCanceller3Config config_use_nearend_len = reference_config;
  config_use_nearend_len.ep_strength.default_len = 0.95f;
  config_use_nearend_len.ep_strength.nearend_len = 0.83f;

  ResidualEchoEstimatorTest reference_residual_echo_estimator_test(
      num_render_channels, num_capture_channels, reference_config);
  ResidualEchoEstimatorTest use_nearend_len_residual_echo_estimator_test(
      num_render_channels, num_capture_channels, config_use_nearend_len);

  std::vector<float> acum_energy_reference_R2(num_capture_channels, 0.0f);
  std::vector<float> acum_energy_R2(num_capture_channels, 0.0f);
  for (size_t frame = 0; frame < nFrames; ++frame) {
    bool dominant_nearend = frame <= nFrames / 2 ? false : true;
    reference_residual_echo_estimator_test.RunOneFrame(dominant_nearend);
    use_nearend_len_residual_echo_estimator_test.RunOneFrame(dominant_nearend);
    const auto& reference_R2 = reference_residual_echo_estimator_test.R2();
    const auto& R2 = use_nearend_len_residual_echo_estimator_test.R2();
    ASSERT_EQ(reference_R2.size(), R2.size());
    for (size_t ch = 0; ch < reference_R2.size(); ++ch) {
      float energy_reference_R2 = std::accumulate(
          reference_R2[ch].cbegin(), reference_R2[ch].cend(), 0.0f);
      float energy_R2 = std::accumulate(R2[ch].cbegin(), R2[ch].cend(), 0.0f);
      if (dominant_nearend) {
        EXPECT_GE(energy_reference_R2, energy_R2);
      } else {
        EXPECT_NEAR(energy_reference_R2, energy_R2, kEpsilon);
      }
      acum_energy_reference_R2[ch] += energy_reference_R2;
      acum_energy_R2[ch] += energy_R2;
    }
    if (frame == nFrames / 2 || frame == nFrames - 1) {
      for (size_t ch = 0; ch < acum_energy_reference_R2.size(); ch++) {
        if (dominant_nearend) {
          EXPECT_GT(acum_energy_reference_R2[ch], acum_energy_R2[ch]);
        } else {
          EXPECT_NEAR(acum_energy_reference_R2[ch], acum_energy_R2[ch],
                      kEpsilon);
        }
      }
    }
  }
}

}  // namespace webrtc
