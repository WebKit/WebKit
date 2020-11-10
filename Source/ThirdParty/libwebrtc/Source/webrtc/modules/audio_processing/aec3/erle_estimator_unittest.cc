/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/erle_estimator.h"

#include <cmath>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/aec3/spectrum_buffer.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kLowFrequencyLimit = kFftLengthBy2 / 2;
constexpr float kTrueErle = 10.f;
constexpr float kTrueErleOnsets = 1.0f;
constexpr float kEchoPathGain = 3.f;

void VerifyErleBands(
    rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> erle,
    float reference_lf,
    float reference_hf) {
  for (size_t ch = 0; ch < erle.size(); ++ch) {
    std::for_each(
        erle[ch].begin(), erle[ch].begin() + kLowFrequencyLimit,
        [reference_lf](float a) { EXPECT_NEAR(reference_lf, a, 0.001); });
    std::for_each(
        erle[ch].begin() + kLowFrequencyLimit, erle[ch].end(),
        [reference_hf](float a) { EXPECT_NEAR(reference_hf, a, 0.001); });
  }
}

void VerifyErle(
    rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> erle,
    float erle_time_domain,
    float reference_lf,
    float reference_hf) {
  VerifyErleBands(erle, reference_lf, reference_hf);
  EXPECT_NEAR(reference_lf, erle_time_domain, 0.5);
}

void FormFarendTimeFrame(std::vector<std::vector<std::vector<float>>>* x) {
  const std::array<float, kBlockSize> frame = {
      7459.88, 17209.6, 17383,   20768.9, 16816.7, 18386.3, 4492.83, 9675.85,
      6665.52, 14808.6, 9342.3,  7483.28, 19261.7, 4145.98, 1622.18, 13475.2,
      7166.32, 6856.61, 21937,   7263.14, 9569.07, 14919,   8413.32, 7551.89,
      7848.65, 6011.27, 13080.6, 15865.2, 12656,   17459.6, 4263.93, 4503.03,
      9311.79, 21095.8, 12657.9, 13906.6, 19267.2, 11338.1, 16828.9, 11501.6,
      11405,   15031.4, 14541.6, 19765.5, 18346.3, 19350.2, 3157.47, 18095.8,
      1743.68, 21328.2, 19727.5, 7295.16, 10332.4, 11055.5, 20107.4, 14708.4,
      12416.2, 16434,   2454.69, 9840.8,  6867.23, 1615.75, 6059.9,  8394.19};
  for (size_t band = 0; band < x->size(); ++band) {
    for (size_t channel = 0; channel < (*x)[band].size(); ++channel) {
      RTC_DCHECK_GE((*x)[band][channel].size(), frame.size());
      std::copy(frame.begin(), frame.end(), (*x)[band][channel].begin());
    }
  }
}

void FormFarendFrame(const RenderBuffer& render_buffer,
                     float erle,
                     std::array<float, kFftLengthBy2Plus1>* X2,
                     rtc::ArrayView<std::array<float, kFftLengthBy2Plus1>> E2,
                     rtc::ArrayView<std::array<float, kFftLengthBy2Plus1>> Y2) {
  const auto& spectrum_buffer = render_buffer.GetSpectrumBuffer();
  const int num_render_channels = spectrum_buffer.buffer[0].size();
  const int num_capture_channels = Y2.size();

  X2->fill(0.f);
  for (int ch = 0; ch < num_render_channels; ++ch) {
    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      (*X2)[k] += spectrum_buffer.buffer[spectrum_buffer.write][ch][k] /
                  num_render_channels;
    }
  }

  for (int ch = 0; ch < num_capture_channels; ++ch) {
    std::transform(X2->begin(), X2->end(), Y2[ch].begin(),
                   [](float a) { return a * kEchoPathGain * kEchoPathGain; });
    std::transform(Y2[ch].begin(), Y2[ch].end(), E2[ch].begin(),
                   [erle](float a) { return a / erle; });
  }
}

void FormNearendFrame(
    std::vector<std::vector<std::vector<float>>>* x,
    std::array<float, kFftLengthBy2Plus1>* X2,
    rtc::ArrayView<std::array<float, kFftLengthBy2Plus1>> E2,
    rtc::ArrayView<std::array<float, kFftLengthBy2Plus1>> Y2) {
  for (size_t band = 0; band < x->size(); ++band) {
    for (size_t ch = 0; ch < (*x)[band].size(); ++ch) {
      std::fill((*x)[band][ch].begin(), (*x)[band][ch].end(), 0.f);
    }
  }

  X2->fill(0.f);
  for (size_t ch = 0; ch < Y2.size(); ++ch) {
    Y2[ch].fill(500.f * 1000.f * 1000.f);
    E2[ch].fill(Y2[ch][0]);
  }
}

void GetFilterFreq(
    size_t delay_headroom_samples,
    rtc::ArrayView<std::vector<std::array<float, kFftLengthBy2Plus1>>>
        filter_frequency_response) {
  const size_t delay_headroom_blocks = delay_headroom_samples / kBlockSize;
  for (size_t ch = 0; ch < filter_frequency_response[0].size(); ++ch) {
    for (auto& block_freq_resp : filter_frequency_response) {
      block_freq_resp[ch].fill(0.f);
    }

    for (size_t k = 0; k < kFftLengthBy2Plus1; ++k) {
      filter_frequency_response[delay_headroom_blocks][ch][k] = kEchoPathGain;
    }
  }
}

}  // namespace

class ErleEstimatorMultiChannel
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         ErleEstimatorMultiChannel,
                         ::testing::Combine(::testing::Values(1, 2, 4, 8),
                                            ::testing::Values(1, 2, 8)));

TEST_P(ErleEstimatorMultiChannel, VerifyErleIncreaseAndHold) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  std::array<float, kFftLengthBy2Plus1> X2;
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2(num_capture_channels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(num_capture_channels);
  std::vector<bool> converged_filters(num_capture_channels, true);

  EchoCanceller3Config config;
  config.erle.onset_detection = true;

  std::vector<std::vector<std::vector<float>>> x(
      kNumBands, std::vector<std::vector<float>>(
                     num_render_channels, std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>>
  filter_frequency_response(
      config.filter.refined.length_blocks,
      std::vector<std::array<float, kFftLengthBy2Plus1>>(num_capture_channels));
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, num_render_channels));

  GetFilterFreq(config.delay.delay_headroom_samples, filter_frequency_response);

  ErleEstimator estimator(0, config, num_capture_channels);

  FormFarendTimeFrame(&x);
  render_delay_buffer->Insert(x);
  render_delay_buffer->PrepareCaptureProcessing();
  // Verifies that the ERLE estimate is properly increased to higher values.
  FormFarendFrame(*render_delay_buffer->GetRenderBuffer(), kTrueErle, &X2, E2,
                  Y2);
  for (size_t k = 0; k < 200; ++k) {
    render_delay_buffer->Insert(x);
    render_delay_buffer->PrepareCaptureProcessing();
    estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                     filter_frequency_response, X2, Y2, E2, converged_filters);
  }
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             config.erle.max_l, config.erle.max_h);

  FormNearendFrame(&x, &X2, E2, Y2);
  // Verifies that the ERLE is not immediately decreased during nearend
  // activity.
  for (size_t k = 0; k < 50; ++k) {
    render_delay_buffer->Insert(x);
    render_delay_buffer->PrepareCaptureProcessing();
    estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                     filter_frequency_response, X2, Y2, E2, converged_filters);
  }
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             config.erle.max_l, config.erle.max_h);
}

TEST_P(ErleEstimatorMultiChannel, VerifyErleTrackingOnOnsets) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  std::array<float, kFftLengthBy2Plus1> X2;
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2(num_capture_channels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(num_capture_channels);
  std::vector<bool> converged_filters(num_capture_channels, true);
  EchoCanceller3Config config;
  config.erle.onset_detection = true;
  std::vector<std::vector<std::vector<float>>> x(
      kNumBands, std::vector<std::vector<float>>(
                     num_render_channels, std::vector<float>(kBlockSize, 0.f)));
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>>
  filter_frequency_response(
      config.filter.refined.length_blocks,
      std::vector<std::array<float, kFftLengthBy2Plus1>>(num_capture_channels));
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, num_render_channels));

  GetFilterFreq(config.delay.delay_headroom_samples, filter_frequency_response);

  ErleEstimator estimator(/*startup_phase_length_blocks=*/0, config,
                          num_capture_channels);

  FormFarendTimeFrame(&x);
  render_delay_buffer->Insert(x);
  render_delay_buffer->PrepareCaptureProcessing();

  for (size_t burst = 0; burst < 20; ++burst) {
    FormFarendFrame(*render_delay_buffer->GetRenderBuffer(), kTrueErleOnsets,
                    &X2, E2, Y2);
    for (size_t k = 0; k < 10; ++k) {
      render_delay_buffer->Insert(x);
      render_delay_buffer->PrepareCaptureProcessing();
      estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                       filter_frequency_response, X2, Y2, E2,
                       converged_filters);
    }
    FormFarendFrame(*render_delay_buffer->GetRenderBuffer(), kTrueErle, &X2, E2,
                    Y2);
    for (size_t k = 0; k < 200; ++k) {
      render_delay_buffer->Insert(x);
      render_delay_buffer->PrepareCaptureProcessing();
      estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                       filter_frequency_response, X2, Y2, E2,
                       converged_filters);
    }
    FormNearendFrame(&x, &X2, E2, Y2);
    for (size_t k = 0; k < 300; ++k) {
      render_delay_buffer->Insert(x);
      render_delay_buffer->PrepareCaptureProcessing();
      estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                       filter_frequency_response, X2, Y2, E2,
                       converged_filters);
    }
  }
  VerifyErleBands(estimator.ErleOnsets(), config.erle.min, config.erle.min);
  FormNearendFrame(&x, &X2, E2, Y2);
  for (size_t k = 0; k < 1000; k++) {
    estimator.Update(*render_delay_buffer->GetRenderBuffer(),
                     filter_frequency_response, X2, Y2, E2, converged_filters);
  }
  // Verifies that during ne activity, Erle converges to the Erle for
  // onsets.
  VerifyErle(estimator.Erle(), std::pow(2.f, estimator.FullbandErleLog2()),
             config.erle.min, config.erle.min);
}

}  // namespace webrtc
