/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/adaptive_fir_filter.h"

// Defines WEBRTC_ARCH_X86_FAMILY, used below.
#include <math.h>

#include <algorithm>
#include <numeric>
#include <string>

#include "rtc_base/system/arch.h"
#if defined(WEBRTC_ARCH_X86_FAMILY)
#include <emmintrin.h>
#endif

#include "modules/audio_processing/aec3/adaptive_fir_filter_erl.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/coarse_filter_update_gain.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "modules/audio_processing/aec3/render_signal_analyzer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "modules/audio_processing/test/echo_canceller_test_tools.h"
#include "modules/audio_processing/utility/cascaded_biquad_filter.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/random.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"

namespace webrtc {
namespace aec3 {
namespace {

std::string ProduceDebugText(size_t num_render_channels, size_t delay) {
  rtc::StringBuilder ss;
  ss << "delay: " << delay << ", ";
  ss << "num_render_channels:" << num_render_channels;
  return ss.Release();
}

}  // namespace

class AdaptiveFirFilterOneTwoFourEightRenderChannels
    : public ::testing::Test,
      public ::testing::WithParamInterface<size_t> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         AdaptiveFirFilterOneTwoFourEightRenderChannels,
                         ::testing::Values(1, 2, 4, 8));

#if defined(WEBRTC_HAS_NEON)
// Verifies that the optimized methods for filter adaptation are similar to
// their reference counterparts.
TEST_P(AdaptiveFirFilterOneTwoFourEightRenderChannels,
       FilterAdaptationNeonOptimizations) {
  const size_t num_render_channels = GetParam();
  for (size_t num_partitions : {2, 5, 12, 30, 50}) {
    constexpr int kSampleRateHz = 48000;
    constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

    std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
        RenderDelayBuffer::Create(EchoCanceller3Config(), kSampleRateHz,
                                  num_render_channels));
    Random random_generator(42U);
    std::vector<std::vector<std::vector<float>>> x(
        kNumBands,
        std::vector<std::vector<float>>(num_render_channels,
                                        std::vector<float>(kBlockSize, 0.f)));
    FftData S_C;
    FftData S_Neon;
    FftData G;
    Aec3Fft fft;
    std::vector<std::vector<FftData>> H_C(
        num_partitions, std::vector<FftData>(num_render_channels));
    std::vector<std::vector<FftData>> H_Neon(
        num_partitions, std::vector<FftData>(num_render_channels));
    for (size_t p = 0; p < num_partitions; ++p) {
      for (size_t ch = 0; ch < num_render_channels; ++ch) {
        H_C[p][ch].Clear();
        H_Neon[p][ch].Clear();
      }
    }

    for (size_t k = 0; k < 30; ++k) {
      for (size_t band = 0; band < x.size(); ++band) {
        for (size_t ch = 0; ch < x[band].size(); ++ch) {
          RandomizeSampleVector(&random_generator, x[band][ch]);
        }
      }
      render_delay_buffer->Insert(x);
      if (k == 0) {
        render_delay_buffer->Reset();
      }
      render_delay_buffer->PrepareCaptureProcessing();
    }
    auto* const render_buffer = render_delay_buffer->GetRenderBuffer();

    for (size_t j = 0; j < G.re.size(); ++j) {
      G.re[j] = j / 10001.f;
    }
    for (size_t j = 1; j < G.im.size() - 1; ++j) {
      G.im[j] = j / 20001.f;
    }
    G.im[0] = 0.f;
    G.im[G.im.size() - 1] = 0.f;

    AdaptPartitions_Neon(*render_buffer, G, num_partitions, &H_Neon);
    AdaptPartitions(*render_buffer, G, num_partitions, &H_C);
    AdaptPartitions_Neon(*render_buffer, G, num_partitions, &H_Neon);
    AdaptPartitions(*render_buffer, G, num_partitions, &H_C);

    for (size_t p = 0; p < num_partitions; ++p) {
      for (size_t ch = 0; ch < num_render_channels; ++ch) {
        for (size_t j = 0; j < H_C[p][ch].re.size(); ++j) {
          EXPECT_FLOAT_EQ(H_C[p][ch].re[j], H_Neon[p][ch].re[j]);
          EXPECT_FLOAT_EQ(H_C[p][ch].im[j], H_Neon[p][ch].im[j]);
        }
      }
    }

    ApplyFilter_Neon(*render_buffer, num_partitions, H_Neon, &S_Neon);
    ApplyFilter(*render_buffer, num_partitions, H_C, &S_C);
    for (size_t j = 0; j < S_C.re.size(); ++j) {
      EXPECT_NEAR(S_C.re[j], S_Neon.re[j], fabs(S_C.re[j] * 0.00001f));
      EXPECT_NEAR(S_C.im[j], S_Neon.im[j], fabs(S_C.re[j] * 0.00001f));
    }
  }
}

// Verifies that the optimized method for frequency response computation is
// bitexact to the reference counterpart.
TEST_P(AdaptiveFirFilterOneTwoFourEightRenderChannels,
       ComputeFrequencyResponseNeonOptimization) {
  const size_t num_render_channels = GetParam();
  for (size_t num_partitions : {2, 5, 12, 30, 50}) {
    std::vector<std::vector<FftData>> H(
        num_partitions, std::vector<FftData>(num_render_channels));
    std::vector<std::array<float, kFftLengthBy2Plus1>> H2(num_partitions);
    std::vector<std::array<float, kFftLengthBy2Plus1>> H2_Neon(num_partitions);

    for (size_t p = 0; p < num_partitions; ++p) {
      for (size_t ch = 0; ch < num_render_channels; ++ch) {
        for (size_t k = 0; k < H[p][ch].re.size(); ++k) {
          H[p][ch].re[k] = k + p / 3.f + ch;
          H[p][ch].im[k] = p + k / 7.f - ch;
        }
      }
    }

    ComputeFrequencyResponse(num_partitions, H, &H2);
    ComputeFrequencyResponse_Neon(num_partitions, H, &H2_Neon);

    for (size_t p = 0; p < num_partitions; ++p) {
      for (size_t k = 0; k < H2[p].size(); ++k) {
        EXPECT_FLOAT_EQ(H2[p][k], H2_Neon[p][k]);
      }
    }
  }
}
#endif

#if defined(WEBRTC_ARCH_X86_FAMILY)
// Verifies that the optimized methods for filter adaptation are bitexact to
// their reference counterparts.
TEST_P(AdaptiveFirFilterOneTwoFourEightRenderChannels,
       FilterAdaptationSse2Optimizations) {
  const size_t num_render_channels = GetParam();
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  bool use_sse2 = (GetCPUInfo(kSSE2) != 0);
  if (use_sse2) {
    for (size_t num_partitions : {2, 5, 12, 30, 50}) {
      std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
          RenderDelayBuffer::Create(EchoCanceller3Config(), kSampleRateHz,
                                    num_render_channels));
      Random random_generator(42U);
      std::vector<std::vector<std::vector<float>>> x(
          kNumBands,
          std::vector<std::vector<float>>(num_render_channels,
                                          std::vector<float>(kBlockSize, 0.f)));
      FftData S_C;
      FftData S_Sse2;
      FftData G;
      Aec3Fft fft;
      std::vector<std::vector<FftData>> H_C(
          num_partitions, std::vector<FftData>(num_render_channels));
      std::vector<std::vector<FftData>> H_Sse2(
          num_partitions, std::vector<FftData>(num_render_channels));
      for (size_t p = 0; p < num_partitions; ++p) {
        for (size_t ch = 0; ch < num_render_channels; ++ch) {
          H_C[p][ch].Clear();
          H_Sse2[p][ch].Clear();
        }
      }

      for (size_t k = 0; k < 500; ++k) {
        for (size_t band = 0; band < x.size(); ++band) {
          for (size_t ch = 0; ch < x[band].size(); ++ch) {
            RandomizeSampleVector(&random_generator, x[band][ch]);
          }
        }
        render_delay_buffer->Insert(x);
        if (k == 0) {
          render_delay_buffer->Reset();
        }
        render_delay_buffer->PrepareCaptureProcessing();
        auto* const render_buffer = render_delay_buffer->GetRenderBuffer();

        ApplyFilter_Sse2(*render_buffer, num_partitions, H_Sse2, &S_Sse2);
        ApplyFilter(*render_buffer, num_partitions, H_C, &S_C);
        for (size_t j = 0; j < S_C.re.size(); ++j) {
          EXPECT_FLOAT_EQ(S_C.re[j], S_Sse2.re[j]);
          EXPECT_FLOAT_EQ(S_C.im[j], S_Sse2.im[j]);
        }

        std::for_each(G.re.begin(), G.re.end(),
                      [&](float& a) { a = random_generator.Rand<float>(); });
        std::for_each(G.im.begin(), G.im.end(),
                      [&](float& a) { a = random_generator.Rand<float>(); });

        AdaptPartitions_Sse2(*render_buffer, G, num_partitions, &H_Sse2);
        AdaptPartitions(*render_buffer, G, num_partitions, &H_C);

        for (size_t p = 0; p < num_partitions; ++p) {
          for (size_t ch = 0; ch < num_render_channels; ++ch) {
            for (size_t j = 0; j < H_C[p][ch].re.size(); ++j) {
              EXPECT_FLOAT_EQ(H_C[p][ch].re[j], H_Sse2[p][ch].re[j]);
              EXPECT_FLOAT_EQ(H_C[p][ch].im[j], H_Sse2[p][ch].im[j]);
            }
          }
        }
      }
    }
  }
}

// Verifies that the optimized methods for filter adaptation are bitexact to
// their reference counterparts.
TEST_P(AdaptiveFirFilterOneTwoFourEightRenderChannels,
       FilterAdaptationAvx2Optimizations) {
  const size_t num_render_channels = GetParam();
  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);

  bool use_avx2 = (GetCPUInfo(kAVX2) != 0);
  if (use_avx2) {
    for (size_t num_partitions : {2, 5, 12, 30, 50}) {
      std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
          RenderDelayBuffer::Create(EchoCanceller3Config(), kSampleRateHz,
                                    num_render_channels));
      Random random_generator(42U);
      std::vector<std::vector<std::vector<float>>> x(
          kNumBands,
          std::vector<std::vector<float>>(num_render_channels,
                                          std::vector<float>(kBlockSize, 0.f)));
      FftData S_C;
      FftData S_Avx2;
      FftData G;
      Aec3Fft fft;
      std::vector<std::vector<FftData>> H_C(
          num_partitions, std::vector<FftData>(num_render_channels));
      std::vector<std::vector<FftData>> H_Avx2(
          num_partitions, std::vector<FftData>(num_render_channels));
      for (size_t p = 0; p < num_partitions; ++p) {
        for (size_t ch = 0; ch < num_render_channels; ++ch) {
          H_C[p][ch].Clear();
          H_Avx2[p][ch].Clear();
        }
      }

      for (size_t k = 0; k < 500; ++k) {
        for (size_t band = 0; band < x.size(); ++band) {
          for (size_t ch = 0; ch < x[band].size(); ++ch) {
            RandomizeSampleVector(&random_generator, x[band][ch]);
          }
        }
        render_delay_buffer->Insert(x);
        if (k == 0) {
          render_delay_buffer->Reset();
        }
        render_delay_buffer->PrepareCaptureProcessing();
        auto* const render_buffer = render_delay_buffer->GetRenderBuffer();

        ApplyFilter_Avx2(*render_buffer, num_partitions, H_Avx2, &S_Avx2);
        ApplyFilter(*render_buffer, num_partitions, H_C, &S_C);
        for (size_t j = 0; j < S_C.re.size(); ++j) {
          EXPECT_FLOAT_EQ(S_C.re[j], S_Avx2.re[j]);
          EXPECT_FLOAT_EQ(S_C.im[j], S_Avx2.im[j]);
        }

        std::for_each(G.re.begin(), G.re.end(),
                      [&](float& a) { a = random_generator.Rand<float>(); });
        std::for_each(G.im.begin(), G.im.end(),
                      [&](float& a) { a = random_generator.Rand<float>(); });

        AdaptPartitions_Avx2(*render_buffer, G, num_partitions, &H_Avx2);
        AdaptPartitions(*render_buffer, G, num_partitions, &H_C);

        for (size_t p = 0; p < num_partitions; ++p) {
          for (size_t ch = 0; ch < num_render_channels; ++ch) {
            for (size_t j = 0; j < H_C[p][ch].re.size(); ++j) {
              EXPECT_FLOAT_EQ(H_C[p][ch].re[j], H_Avx2[p][ch].re[j]);
              EXPECT_FLOAT_EQ(H_C[p][ch].im[j], H_Avx2[p][ch].im[j]);
            }
          }
        }
      }
    }
  }
}

// Verifies that the optimized method for frequency response computation is
// bitexact to the reference counterpart.
TEST_P(AdaptiveFirFilterOneTwoFourEightRenderChannels,
       ComputeFrequencyResponseSse2Optimization) {
  const size_t num_render_channels = GetParam();
  bool use_sse2 = (GetCPUInfo(kSSE2) != 0);
  if (use_sse2) {
    for (size_t num_partitions : {2, 5, 12, 30, 50}) {
      std::vector<std::vector<FftData>> H(
          num_partitions, std::vector<FftData>(num_render_channels));
      std::vector<std::array<float, kFftLengthBy2Plus1>> H2(num_partitions);
      std::vector<std::array<float, kFftLengthBy2Plus1>> H2_Sse2(
          num_partitions);

      for (size_t p = 0; p < num_partitions; ++p) {
        for (size_t ch = 0; ch < num_render_channels; ++ch) {
          for (size_t k = 0; k < H[p][ch].re.size(); ++k) {
            H[p][ch].re[k] = k + p / 3.f + ch;
            H[p][ch].im[k] = p + k / 7.f - ch;
          }
        }
      }

      ComputeFrequencyResponse(num_partitions, H, &H2);
      ComputeFrequencyResponse_Sse2(num_partitions, H, &H2_Sse2);

      for (size_t p = 0; p < num_partitions; ++p) {
        for (size_t k = 0; k < H2[p].size(); ++k) {
          EXPECT_FLOAT_EQ(H2[p][k], H2_Sse2[p][k]);
        }
      }
    }
  }
}

// Verifies that the optimized method for frequency response computation is
// bitexact to the reference counterpart.
TEST_P(AdaptiveFirFilterOneTwoFourEightRenderChannels,
       ComputeFrequencyResponseAvx2Optimization) {
  const size_t num_render_channels = GetParam();
  bool use_avx2 = (GetCPUInfo(kAVX2) != 0);
  if (use_avx2) {
    for (size_t num_partitions : {2, 5, 12, 30, 50}) {
      std::vector<std::vector<FftData>> H(
          num_partitions, std::vector<FftData>(num_render_channels));
      std::vector<std::array<float, kFftLengthBy2Plus1>> H2(num_partitions);
      std::vector<std::array<float, kFftLengthBy2Plus1>> H2_Avx2(
          num_partitions);

      for (size_t p = 0; p < num_partitions; ++p) {
        for (size_t ch = 0; ch < num_render_channels; ++ch) {
          for (size_t k = 0; k < H[p][ch].re.size(); ++k) {
            H[p][ch].re[k] = k + p / 3.f + ch;
            H[p][ch].im[k] = p + k / 7.f - ch;
          }
        }
      }

      ComputeFrequencyResponse(num_partitions, H, &H2);
      ComputeFrequencyResponse_Avx2(num_partitions, H, &H2_Avx2);

      for (size_t p = 0; p < num_partitions; ++p) {
        for (size_t k = 0; k < H2[p].size(); ++k) {
          EXPECT_FLOAT_EQ(H2[p][k], H2_Avx2[p][k]);
        }
      }
    }
  }
}

#endif

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// Verifies that the check for non-null data dumper works.
TEST(AdaptiveFirFilterDeathTest, NullDataDumper) {
  EXPECT_DEATH(AdaptiveFirFilter(9, 9, 250, 1, DetectOptimization(), nullptr),
               "");
}

// Verifies that the check for non-null filter output works.
TEST(AdaptiveFirFilterDeathTest, NullFilterOutput) {
  ApmDataDumper data_dumper(42);
  AdaptiveFirFilter filter(9, 9, 250, 1, DetectOptimization(), &data_dumper);
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(EchoCanceller3Config(), 48000, 1));
  EXPECT_DEATH(filter.Filter(*render_delay_buffer->GetRenderBuffer(), nullptr),
               "");
}

#endif

// Verifies that the filter statistics can be accessed when filter statistics
// are turned on.
TEST(AdaptiveFirFilterTest, FilterStatisticsAccess) {
  ApmDataDumper data_dumper(42);
  Aec3Optimization optimization = DetectOptimization();
  AdaptiveFirFilter filter(9, 9, 250, 1, optimization, &data_dumper);
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2(
      filter.max_filter_size_partitions(),
      std::array<float, kFftLengthBy2Plus1>());
  for (auto& H2_k : H2) {
    H2_k.fill(0.f);
  }

  std::array<float, kFftLengthBy2Plus1> erl;
  ComputeErl(optimization, H2, erl);
  filter.ComputeFrequencyResponse(&H2);
}

// Verifies that the filter size if correctly repported.
TEST(AdaptiveFirFilterTest, FilterSize) {
  ApmDataDumper data_dumper(42);
  for (size_t filter_size = 1; filter_size < 5; ++filter_size) {
    AdaptiveFirFilter filter(filter_size, filter_size, 250, 1,
                             DetectOptimization(), &data_dumper);
    EXPECT_EQ(filter_size, filter.SizePartitions());
  }
}

class AdaptiveFirFilterMultiChannel
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         AdaptiveFirFilterMultiChannel,
                         ::testing::Combine(::testing::Values(1, 4),
                                            ::testing::Values(1, 8)));

// Verifies that the filter is being able to properly filter a signal and to
// adapt its coefficients.
TEST_P(AdaptiveFirFilterMultiChannel, FilterAndAdapt) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());

  constexpr int kSampleRateHz = 48000;
  constexpr size_t kNumBands = NumBandsForRate(kSampleRateHz);
  constexpr size_t kNumBlocksToProcessPerRenderChannel = 1000;

  ApmDataDumper data_dumper(42);
  EchoCanceller3Config config;

  if (num_render_channels == 33) {
    config.filter.refined = {13, 0.00005f, 0.0005f, 0.0001f, 2.f, 20075344.f};
    config.filter.coarse = {13, 0.1f, 20075344.f};
    config.filter.refined_initial = {12, 0.005f, 0.5f, 0.001f, 2.f, 20075344.f};
    config.filter.coarse_initial = {12, 0.7f, 20075344.f};
  }

  AdaptiveFirFilter filter(
      config.filter.refined.length_blocks, config.filter.refined.length_blocks,
      config.filter.config_change_duration_blocks, num_render_channels,
      DetectOptimization(), &data_dumper);
  std::vector<std::vector<std::array<float, kFftLengthBy2Plus1>>> H2(
      num_capture_channels, std::vector<std::array<float, kFftLengthBy2Plus1>>(
                                filter.max_filter_size_partitions(),
                                std::array<float, kFftLengthBy2Plus1>()));
  std::vector<std::vector<float>> h(
      num_capture_channels,
      std::vector<float>(
          GetTimeDomainLength(filter.max_filter_size_partitions()), 0.f));
  Aec3Fft fft;
  config.delay.default_delay = 1;
  std::unique_ptr<RenderDelayBuffer> render_delay_buffer(
      RenderDelayBuffer::Create(config, kSampleRateHz, num_render_channels));
  CoarseFilterUpdateGain gain(config.filter.coarse,
                              config.filter.config_change_duration_blocks);
  Random random_generator(42U);
  std::vector<std::vector<std::vector<float>>> x(
      kNumBands, std::vector<std::vector<float>>(
                     num_render_channels, std::vector<float>(kBlockSize, 0.f)));
  std::vector<float> n(kBlockSize, 0.f);
  std::vector<float> y(kBlockSize, 0.f);
  AecState aec_state(EchoCanceller3Config{}, num_capture_channels);
  RenderSignalAnalyzer render_signal_analyzer(config);
  absl::optional<DelayEstimate> delay_estimate;
  std::vector<float> e(kBlockSize, 0.f);
  std::array<float, kFftLength> s_scratch;
  std::vector<SubtractorOutput> output(num_capture_channels);
  FftData S;
  FftData G;
  FftData E;
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(num_capture_channels);
  std::vector<std::array<float, kFftLengthBy2Plus1>> E2_refined(
      num_capture_channels);
  std::array<float, kFftLengthBy2Plus1> E2_coarse;
  // [B,A] = butter(2,100/8000,'high')
  constexpr CascadedBiQuadFilter::BiQuadCoefficients
      kHighPassFilterCoefficients = {{0.97261f, -1.94523f, 0.97261f},
                                     {-1.94448f, 0.94598f}};
  for (auto& Y2_ch : Y2) {
    Y2_ch.fill(0.f);
  }
  for (auto& E2_refined_ch : E2_refined) {
    E2_refined_ch.fill(0.f);
  }
  E2_coarse.fill(0.f);
  for (auto& subtractor_output : output) {
    subtractor_output.Reset();
  }

  constexpr float kScale = 1.0f / kFftLengthBy2;

  for (size_t delay_samples : {0, 64, 150, 200, 301}) {
    std::vector<DelayBuffer<float>> delay_buffer(
        num_render_channels, DelayBuffer<float>(delay_samples));
    std::vector<std::unique_ptr<CascadedBiQuadFilter>> x_hp_filter(
        num_render_channels);
    for (size_t ch = 0; ch < num_render_channels; ++ch) {
      x_hp_filter[ch] = std::make_unique<CascadedBiQuadFilter>(
          kHighPassFilterCoefficients, 1);
    }
    CascadedBiQuadFilter y_hp_filter(kHighPassFilterCoefficients, 1);

    SCOPED_TRACE(ProduceDebugText(num_render_channels, delay_samples));
    const size_t num_blocks_to_process =
        kNumBlocksToProcessPerRenderChannel * num_render_channels;
    for (size_t j = 0; j < num_blocks_to_process; ++j) {
      std::fill(y.begin(), y.end(), 0.f);
      for (size_t ch = 0; ch < num_render_channels; ++ch) {
        RandomizeSampleVector(&random_generator, x[0][ch]);
        std::array<float, kBlockSize> y_channel;
        delay_buffer[ch].Delay(x[0][ch], y_channel);
        for (size_t k = 0; k < y.size(); ++k) {
          y[k] += y_channel[k] / num_render_channels;
        }
      }

      RandomizeSampleVector(&random_generator, n);
      const float noise_scaling = 1.f / 100.f / num_render_channels;
      for (size_t k = 0; k < y.size(); ++k) {
        y[k] += n[k] * noise_scaling;
      }

      for (size_t ch = 0; ch < num_render_channels; ++ch) {
        x_hp_filter[ch]->Process(x[0][ch]);
      }
      y_hp_filter.Process(y);

      render_delay_buffer->Insert(x);
      if (j == 0) {
        render_delay_buffer->Reset();
      }
      render_delay_buffer->PrepareCaptureProcessing();
      auto* const render_buffer = render_delay_buffer->GetRenderBuffer();

      render_signal_analyzer.Update(*render_buffer,
                                    aec_state.MinDirectPathFilterDelay());

      filter.Filter(*render_buffer, &S);
      fft.Ifft(S, &s_scratch);
      std::transform(y.begin(), y.end(), s_scratch.begin() + kFftLengthBy2,
                     e.begin(),
                     [&](float a, float b) { return a - b * kScale; });
      std::for_each(e.begin(), e.end(),
                    [](float& a) { a = rtc::SafeClamp(a, -32768.f, 32767.f); });
      fft.ZeroPaddedFft(e, Aec3Fft::Window::kRectangular, &E);
      for (auto& o : output) {
        for (size_t k = 0; k < kBlockSize; ++k) {
          o.s_refined[k] = kScale * s_scratch[k + kFftLengthBy2];
        }
      }

      std::array<float, kFftLengthBy2Plus1> render_power;
      render_buffer->SpectralSum(filter.SizePartitions(), &render_power);
      gain.Compute(render_power, render_signal_analyzer, E,
                   filter.SizePartitions(), false, &G);
      filter.Adapt(*render_buffer, G, &h[0]);
      aec_state.HandleEchoPathChange(EchoPathVariability(
          false, EchoPathVariability::DelayAdjustment::kNone, false));

      filter.ComputeFrequencyResponse(&H2[0]);
      aec_state.Update(delay_estimate, H2, h, *render_buffer, E2_refined, Y2,
                       output);
    }
    // Verify that the filter is able to perform well.
    EXPECT_LT(1000 * std::inner_product(e.begin(), e.end(), e.begin(), 0.f),
              std::inner_product(y.begin(), y.end(), y.begin(), 0.f));
  }
}

}  // namespace aec3
}  // namespace webrtc
