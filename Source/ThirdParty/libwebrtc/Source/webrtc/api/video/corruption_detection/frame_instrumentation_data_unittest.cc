/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/frame_instrumentation_data.h"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "test/gtest.h"

namespace webrtc {

TEST(FrameInstrumentationDataTest, RespectsSequenceIndexRange) {
  FrameInstrumentationData data;
  EXPECT_FALSE(data.SetSequenceIndex(-1));
  EXPECT_TRUE(data.SetSequenceIndex(0));
  EXPECT_TRUE(data.SetSequenceIndex((1 << 14) - 1));
  EXPECT_FALSE(data.SetSequenceIndex(1 << 14));
}

TEST(FrameInstrumentationDataTest, RespectsStdDevRange) {
  FrameInstrumentationData data;
  EXPECT_FALSE(data.SetStdDev(
      std::nextafter(0.0, std::numeric_limits<double>::lowest())));
  EXPECT_TRUE(data.SetStdDev(0.0));
  EXPECT_TRUE(data.SetStdDev(40.0));
  EXPECT_FALSE(
      data.SetStdDev(std::nextafter(40.0, std::numeric_limits<double>::max())));
}

TEST(FrameInstrumentationDataTest, RespectsLumaRange) {
  FrameInstrumentationData data;
  EXPECT_FALSE(data.SetLumaErrorThreshold(-1));
  EXPECT_TRUE(data.SetLumaErrorThreshold(0));
  EXPECT_TRUE(data.SetLumaErrorThreshold(15));
  EXPECT_FALSE(data.SetLumaErrorThreshold(16));
}

TEST(FrameInstrumentationDataTest, RespectsChromaRange) {
  FrameInstrumentationData data;
  EXPECT_FALSE(data.SetChromaErrorThreshold(-1));
  EXPECT_TRUE(data.SetChromaErrorThreshold(0));
  EXPECT_TRUE(data.SetChromaErrorThreshold(15));
  EXPECT_FALSE(data.SetChromaErrorThreshold(16));
}

TEST(FrameInstrumentationDataTest, RejectsLowSampleValues) {
  FrameInstrumentationData data;

  const double kLowValue[] = {
      std::nextafter(0.0, std::numeric_limits<double>::lowest())};
  EXPECT_FALSE(data.SetSampleValues(kLowValue));

  std::vector vec = {kLowValue[0]};
  EXPECT_FALSE(data.SetSampleValues(std::move(vec)));
}

TEST(FrameInstrumentationDataTest, AcceptsValidSampleValues) {
  FrameInstrumentationData data;

  const double kValues[] = {0.0, 255.0};
  EXPECT_TRUE(data.SetSampleValues(kValues));

  std::vector vec(std::begin(kValues), std::end(kValues));
  EXPECT_TRUE(data.SetSampleValues(std::move(vec)));
}

TEST(FrameInstrumentationDataTest, RejectsHighSampleValues) {
  FrameInstrumentationData data;

  const double kHighValue[] = {
      std::nextafter(255.0, std::numeric_limits<double>::max())};
  EXPECT_FALSE(data.SetSampleValues(kHighValue));

  std::vector vec = {kHighValue[0]};
  EXPECT_FALSE(data.SetSampleValues(std::move(vec)));
}

TEST(FrameInstrumentationDataTest, ReportsUpperBits) {
  FrameInstrumentationData data;

  EXPECT_TRUE(data.SetSequenceIndex(0b0111'1111));
  EXPECT_FALSE(data.holds_upper_bits());

  EXPECT_TRUE(data.SetSequenceIndex(0b1111'1111));
  EXPECT_FALSE(data.holds_upper_bits());

  EXPECT_TRUE(data.SetSequenceIndex(0b1000'0000));
  EXPECT_TRUE(data.holds_upper_bits());
}

TEST(FrameInstrumentationDataTest, NoUpperBitsWhenDroppable) {
  FrameInstrumentationData data;

  EXPECT_TRUE(data.SetSequenceIndex(0b1000'0000));
  EXPECT_TRUE(data.holds_upper_bits());

  data.set_droppable(true);
  EXPECT_FALSE(data.holds_upper_bits());
}

TEST(FrameInstrumentationDataTest, ReportsSyncOnly) {
  FrameInstrumentationData data;

  EXPECT_TRUE(data.is_sync_only());

  EXPECT_TRUE(data.SetSampleValues({0.0}));
  EXPECT_FALSE(data.is_sync_only());
}
}  // namespace webrtc
