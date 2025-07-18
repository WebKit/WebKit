/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/fir_filter.h"

#include <string.h>

#include <array>
#include <memory>

#include "common_audio/fir_filter_factory.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

static constexpr size_t kCoefficientsSize = 5;
static const std::array<float, kCoefficientsSize> kCoefficients = {
    0.2f, 0.3f, 0.5f, 0.7f, 0.11f};

static constexpr size_t kInputSize = 10;
static const std::array<float, kInputSize> kInput = {1.f, 2.f, 3.f, 4.f, 5.f,
                                                     6.f, 7.f, 8.f, 9.f, 10.f};

}  // namespace

TEST(FIRFilterTest, FilterAsIdentity) {
  const std::array<float, kCoefficientsSize> kCoefficientsTested = {
      1.f, 0.f, 0.f, 0.f, 0.f};
  std::array<float, kInputSize> output;
  std::unique_ptr<FIRFilter> filter(CreateFirFilter(
      kCoefficientsTested.data(), kCoefficientsTested.size(), kInput.size()));
  filter->Filter(kInput.data(), kInput.size(), output.data());
  for (size_t i = 0; i < kInput.size(); i++) {
    EXPECT_EQ(kInput[i], output[i]);
  }
}

TEST(FIRFilterTest, FilterUsedAsScalarMultiplication) {
  const std::array<float, kCoefficientsSize> kCoefficientsTested = {
      5.f, 0.f, 0.f, 0.f, 0.f};
  std::array<float, kInputSize> output;
  std::unique_ptr<FIRFilter> filter(CreateFirFilter(
      kCoefficientsTested.data(), kCoefficientsTested.size(), kInput.size()));
  filter->Filter(kInput.data(), kInput.size(), output.data());

  EXPECT_FLOAT_EQ(5.f, output[0]);
  EXPECT_FLOAT_EQ(20.f, output[3]);
  EXPECT_FLOAT_EQ(25.f, output[4]);
  EXPECT_FLOAT_EQ(50.f, output[kInput.size() - 1]);
}

TEST(FIRFilterTest, FilterUsedAsInputShifting) {
  const std::array<float, kCoefficientsSize> kCoefficientsTested = {
      0.f, 0.f, 0.f, 0.f, 1.f};
  std::array<float, kInputSize> output;
  std::unique_ptr<FIRFilter> filter(CreateFirFilter(
      kCoefficientsTested.data(), kCoefficientsTested.size(), kInput.size()));
  filter->Filter(kInput.data(), kInput.size(), output.data());

  EXPECT_FLOAT_EQ(0.f, output[0]);
  EXPECT_FLOAT_EQ(0.f, output[3]);
  EXPECT_FLOAT_EQ(1.f, output[4]);
  EXPECT_FLOAT_EQ(2.f, output[5]);
  EXPECT_FLOAT_EQ(6.f, output[kInput.size() - 1]);
}

TEST(FIRFilterTest, FilterUsedAsArbitraryWeighting) {
  std::array<float, kInputSize> output;
  std::unique_ptr<FIRFilter> filter(CreateFirFilter(
      kCoefficients.data(), kCoefficients.size(), kInput.size()));
  filter->Filter(kInput.data(), kInput.size(), output.data());

  EXPECT_FLOAT_EQ(0.2f, output[0]);
  EXPECT_FLOAT_EQ(3.4f, output[3]);
  EXPECT_FLOAT_EQ(5.21f, output[4]);
  EXPECT_FLOAT_EQ(7.02f, output[5]);
  EXPECT_FLOAT_EQ(14.26f, output[kInput.size() - 1]);
}

TEST(FIRFilterTest, FilterInLengthLesserOrEqualToCoefficientsLength) {
  std::array<float, kInputSize> output;
  std::unique_ptr<FIRFilter> filter(
      CreateFirFilter(kCoefficients.data(), kCoefficients.size(), 2));
  filter->Filter(kInput.data(), 2, output.data());

  EXPECT_FLOAT_EQ(0.2f, output[0]);
  EXPECT_FLOAT_EQ(0.7f, output[1]);
  filter.reset(CreateFirFilter(kCoefficients.data(), kCoefficients.size(),
                               kCoefficients.size()));
  filter->Filter(kInput.data(), kCoefficients.size(), output.data());

  EXPECT_FLOAT_EQ(0.2f, output[0]);
  EXPECT_FLOAT_EQ(3.4f, output[3]);
  EXPECT_FLOAT_EQ(5.21f, output[4]);
}

TEST(FIRFilterTest, MultipleFilterCalls) {
  std::array<float, kInputSize> output;
  std::unique_ptr<FIRFilter> filter(
      CreateFirFilter(kCoefficients.data(), kCoefficients.size(), 3));
  filter->Filter(kInput.data(), 2, output.data());
  EXPECT_FLOAT_EQ(0.2f, output[0]);
  EXPECT_FLOAT_EQ(0.7f, output[1]);

  filter->Filter(kInput.data(), 2, output.data());
  EXPECT_FLOAT_EQ(1.3f, output[0]);
  EXPECT_FLOAT_EQ(2.4f, output[1]);

  filter->Filter(kInput.data(), 2, output.data());
  EXPECT_FLOAT_EQ(2.81f, output[0]);
  EXPECT_FLOAT_EQ(2.62f, output[1]);

  filter->Filter(kInput.data(), 2, output.data());
  EXPECT_FLOAT_EQ(2.81f, output[0]);
  EXPECT_FLOAT_EQ(2.62f, output[1]);

  filter->Filter(&kInput[3], 3, output.data());
  EXPECT_FLOAT_EQ(3.41f, output[0]);
  EXPECT_FLOAT_EQ(4.12f, output[1]);
  EXPECT_FLOAT_EQ(6.21f, output[2]);

  filter->Filter(&kInput[3], 3, output.data());
  EXPECT_FLOAT_EQ(8.12f, output[0]);
  EXPECT_FLOAT_EQ(9.14f, output[1]);
  EXPECT_FLOAT_EQ(9.45f, output[2]);
}

TEST(FIRFilterTest, VerifySampleBasedVsBlockBasedFiltering) {
  float output_block_based[kInput.size()];
  std::unique_ptr<FIRFilter> filter(CreateFirFilter(
      kCoefficients.data(), kCoefficients.size(), kInput.size()));
  filter->Filter(kInput.data(), kInput.size(), output_block_based);

  float output_sample_based[kInput.size()];
  filter.reset(CreateFirFilter(kCoefficients.data(), kCoefficients.size(), 1));
  for (size_t i = 0; i < kInput.size(); ++i) {
    filter->Filter(&kInput[i], 1, &output_sample_based[i]);
  }

  EXPECT_EQ(0, memcmp(output_sample_based, output_block_based, kInput.size()));
}

TEST(FIRFilterTest, SimplestHighPassFilter) {
  const std::array<float, 2> kCoefficientsTested = {1.f, -1.f};

  std::array<float, 8> kConstantInput = {1.f, 1.f, 1.f, 1.f,
                                         1.f, 1.f, 1.f, 1.f};
  std::array<float, 8> output;
  std::unique_ptr<FIRFilter> filter(CreateFirFilter(kCoefficientsTested.data(),
                                                    kCoefficientsTested.size(),
                                                    kConstantInput.size()));
  filter->Filter(kConstantInput.data(), kConstantInput.size(), output.data());
  EXPECT_FLOAT_EQ(1.f, output[0]);
  for (size_t i = kCoefficientsTested.size() - 1; i < kConstantInput.size();
       ++i) {
    EXPECT_FLOAT_EQ(0.f, output[i]);
  }
}

TEST(FIRFilterTest, SimplestLowPassFilter) {
  const std::array<float, 2> kCoefficientsTested = {1.f, 1.f};

  const std::array<float, 8> kHighFrequencyInput = {-1.f, 1.f, -1.f, 1.f,
                                                    -1.f, 1.f, -1.f, 1.f};
  std::array<float, 8> output;
  std::unique_ptr<FIRFilter> filter(
      CreateFirFilter(kCoefficientsTested.data(), kCoefficientsTested.size(),
                      kHighFrequencyInput.size()));
  filter->Filter(kHighFrequencyInput.data(), kHighFrequencyInput.size(),
                 output.data());
  EXPECT_FLOAT_EQ(-1.f, output[0]);
  for (size_t i = kCoefficientsTested.size() - 1;
       i < kHighFrequencyInput.size(); ++i) {
    EXPECT_FLOAT_EQ(0.f, output[i]);
  }
}

TEST(FIRFilterTest, SameOutputWhenSwapedCoefficientsAndInput) {
  std::array<float, kCoefficientsSize> output;
  std::array<float, kCoefficientsSize> output_swapped;
  std::unique_ptr<FIRFilter> filter(CreateFirFilter(
      kCoefficients.data(), kCoefficients.size(), kCoefficients.size()));
  // Use kCoefficients.size() for in_length to get same-length outputs.
  filter->Filter(kInput.data(), kCoefficients.size(), output.data());

  filter.reset(CreateFirFilter(kInput.data(), kCoefficients.size(),
                               kCoefficients.size()));
  filter->Filter(kCoefficients.data(), kCoefficients.size(),
                 output_swapped.data());

  for (size_t i = 0; i < kCoefficients.size(); ++i) {
    EXPECT_FLOAT_EQ(output[i], output_swapped[i]);
  }
}

}  // namespace webrtc
