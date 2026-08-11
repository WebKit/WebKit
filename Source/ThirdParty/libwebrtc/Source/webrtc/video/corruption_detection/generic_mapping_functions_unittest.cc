/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/generic_mapping_functions.h"

#include "api/video/video_codec_type.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::DoubleNear;
using ::testing::FieldsAre;

constexpr double kMaxAbsoluteError = 1e-4;

constexpr int kLumaThreshold = 5;
constexpr int kChromaThresholdVp8 = 6;
constexpr int kChromaThresholdVp9 = 4;
constexpr int kChromaThresholdAv1 = 4;
constexpr int kChromaThresholdH264 = 2;
constexpr int kChromaThresholdH265 = 4;

TEST(GenericMappingFunctionsTest, TestVp8) {
  constexpr VideoCodecType kCodecType = VideoCodecType::kVideoCodecVP8;
  EXPECT_THAT(GetCorruptionFilterSettings(
                  /*qp=*/10, kCodecType),
              FieldsAre(DoubleNear(0.5139, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdVp8));
  EXPECT_THAT(GetCorruptionFilterSettings(
                  /*qp=*/100, kCodecType),
              FieldsAre(DoubleNear(2.7351, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdVp8));
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/127, kCodecType),
              FieldsAre(DoubleNear(4.5162, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdVp8));
}

TEST(GenericMappingFunctionsTest, TestVp9) {
  constexpr VideoCodecType kCodecType = VideoCodecType::kVideoCodecVP9;
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/10, kCodecType),
              FieldsAre(DoubleNear(0.3405, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdVp9));
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/100, kCodecType),
              FieldsAre(DoubleNear(0.9369, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdVp9));
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/200, kCodecType),
              FieldsAre(DoubleNear(3.8088, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdVp9));
  // Cap to legal range [0, 40].
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/255, kCodecType),
              FieldsAre(DoubleNear(40.0, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdVp9));
}

TEST(GenericMappingFunctionsTest, TestAv1) {
  constexpr VideoCodecType kCodecType = VideoCodecType::kVideoCodecAV1;
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/10, kCodecType),
              FieldsAre(DoubleNear(0.4480, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdAv1));
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/100, kCodecType),
              FieldsAre(DoubleNear(0.8623, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdAv1));
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/200, kCodecType),
              FieldsAre(DoubleNear(2.8842, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdAv1));
  // Cap to legal range [0, 40].
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/255, kCodecType),
              FieldsAre(DoubleNear(40, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdAv1));
}

TEST(GenericMappingFunctionsTest, TestH264) {
  constexpr VideoCodecType kCodecType = VideoCodecType::kVideoCodecH264;
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/10, kCodecType),
              FieldsAre(DoubleNear(0.263, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdH264));
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/30, kCodecType),
              FieldsAre(DoubleNear(4.3047, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdH264));
  // Cap to legal range [0, 40].
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/51, kCodecType),
              FieldsAre(DoubleNear(40.0, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdH264));
}

TEST(GenericMappingFunctionsTest, TestH265) {
  constexpr VideoCodecType kCodecType = VideoCodecType::kVideoCodecH265;
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/10, kCodecType),
              FieldsAre(DoubleNear(0.481, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdH265));
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/30, kCodecType),
              FieldsAre(DoubleNear(2.2818, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdH265));
  // Cap to legal range [0, 40].
  EXPECT_THAT(GetCorruptionFilterSettings(/*qp=*/51, kCodecType),
              FieldsAre(DoubleNear(40.0, kMaxAbsoluteError), kLumaThreshold,
                        kChromaThresholdH265));
}

}  // namespace
}  // namespace webrtc
