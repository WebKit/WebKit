/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/environment/environment.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/i010_buffer.h"
#include "api/video/i210_buffer.h"
#include "api/video/i410_buffer.h"
#include "api/video/i420_buffer.h"
#include "api/video/i422_buffer.h"
#include "api/video/i444_buffer.h"
#include "api/video/nv12_buffer.h"
#include "api/video/resolution.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/libaom_av1_encoder_factory.h"
#include "api/video_codecs/test/video_codec_test_utils.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_builders.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/rational.h"
#include "rtc_base/platform_thread_types.h"
#include "test/create_test_environment.h"
#include "test/frame_utils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/frame_reader.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

// This file contains functional unit tests that any compliant implementation of
// `VideoEncoderInterface` MUST pass. They validate that the encoder follows the
// capabilities reported by the associated factory (`GetEncoderCapabilities`).
// See `api/video_codecs/g3doc/video_encoder_api_v2.md` for more details.
// Rate control, quality and performance tests are handled by separate test.

namespace webrtc {

void PrintTo(const Resolution& res, std::ostream* os) {
  *os << res.width << "x" << res.height;
}

namespace {
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;

using Capabilities = VideoEncoderFactoryInterface::Capabilities;
using PredictionConstraints = Capabilities::PredictionConstraints;
using BufferSpaceType = PredictionConstraints::BufferSpaceType;
using InputConstraints = Capabilities::InputConstraints;
using BitrateControl = Capabilities::BitrateControl;
using RateControlMode = VideoEncoderFactoryInterface::RateControlMode;
using CbrSetting = VideoEncoderFactoryInterface::CbrSetting;
using Performance = Capabilities::Performance;
using StaticEncoderSettings =
    VideoEncoderFactoryInterface::StaticEncoderSettings;
using FrameEncodeSettings = VideoEncoderInterface::FrameEncodeSettings;

// Scales a resolution by a rational scaling factor, aligning width and height
// to the given alignment (default 1).
Resolution Scale(Resolution resolution, Rational factor, int alignment = 1) {
  int effective_alignment = std::max(1, alignment);
  return Resolution{
      .width = (resolution.width * factor.numerator / factor.denominator) /
               effective_alignment * effective_alignment,
      .height = (resolution.height * factor.numerator / factor.denominator) /
                effective_alignment * effective_alignment,
  };
}

// Scales a resolution by a fractional scaling factor (numerator/denominator).
Resolution Scale(Resolution resolution,
                 int numerator,
                 int denominator,
                 int alignment = 1) {
  return Scale(resolution, Rational(numerator, denominator), alignment);
}

// Computes the resolution for each spatial layer based on base dimensions and
// scaling factors.
std::vector<Resolution> GetSpatialLayerResolutions(
    Resolution base_resolution,
    const std::vector<Rational>& factors,
    int alignment = 1) {
  std::vector<Resolution> res;
  res.reserve(factors.size());
  for (const Rational& f : factors) {
    res.push_back(Scale(base_resolution, f, alignment));
  }
  return res;
}

// Finds a set of layer scaling factors (relative to top layer) for an N-layer
// spatial hierarchy (e.g. 1/4, 1/2, 1/1 for 3 layers).
std::vector<Rational> FindSpatialLayerScalingFactors(
    const Capabilities& capabilities,
    int num_layers) {
  if (num_layers <= 0 ||
      num_layers > capabilities.prediction_constraints().max_spatial_layers()) {
    return {};
  }
  const std::vector<Rational>& scaling_factors =
      capabilities.prediction_constraints().scaling_factors();
  // Ensure the encoder supports 2:1 upscaling for inter-layer prediction if
  // num_layers > 1.
  if (num_layers > 1 &&
      !absl::c_linear_search(scaling_factors, Rational(2, 1))) {
    return {};
  }

  std::vector<Rational> factors;
  factors.reserve(num_layers);
  for (int i = 0; i < num_layers; ++i) {
    int denom = 1 << (num_layers - 1 - i);
    factors.push_back(Rational(1, denom));
  }
  return factors;
}

class FitsWithinMatcher {
 public:
  using is_gtest_matcher = void;

  FitsWithinMatcher(std::optional<Resolution> min,
                    std::optional<Resolution> max)
      : min_(min), max_(max) {}
  explicit FitsWithinMatcher(const InputConstraints& constraints)
      : min_(constraints.min()), max_(constraints.max()) {}

  bool MatchAndExplain(const Resolution& res,
                       ::testing::MatchResultListener* listener) const {
    if ((min_.has_value() &&
         (res.width < min_->width || res.height < min_->height)) ||
        (max_.has_value() &&
         (res.width > max_->width || res.height > max_->height))) {
      if (listener != nullptr && listener->IsInterested()) {
        *listener << "resolution " << res.width << "x" << res.height
                  << " does not fit within ";
        DescribeBounds(listener->stream());
      }
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "fits within ";
    DescribeBounds(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "does not fit within ";
    DescribeBounds(os);
  }

 private:
  void DescribeBounds(std::ostream* os) const {
    if (min_.has_value() && max_.has_value()) {
      *os << "[" << min_->width << "x" << min_->height << ", " << max_->width
          << "x" << max_->height << "]";
    } else if (max_.has_value()) {
      *os << "<= " << max_->width << "x" << max_->height;
    } else if (min_.has_value()) {
      *os << ">= " << min_->width << "x" << min_->height;
    } else {
      *os << "any resolution";
    }
  }

  std::optional<Resolution> min_;
  std::optional<Resolution> max_;
};

inline ::testing::Matcher<Resolution> FitsWithin(
    std::optional<Resolution> min,
    std::optional<Resolution> max) {
  return FitsWithinMatcher(min, max);
}
inline ::testing::Matcher<Resolution> FitsWithin(Resolution max) {
  return FitsWithinMatcher(std::nullopt, max);
}
inline ::testing::Matcher<Resolution> FitsWithin(
    const InputConstraints& constraints) {
  return FitsWithinMatcher(constraints);
}

inline bool FitsWithin(Resolution res, const InputConstraints& constraints) {
  return FitsWithin(constraints).MatchAndExplain(res, nullptr);
}

inline bool FitsWithin(Resolution res,
                       std::optional<Resolution> min,
                       std::optional<Resolution> max) {
  return FitsWithin(min, max).MatchAndExplain(res, nullptr);
}

TEST(ResolutionHelpersTest, ScaleResolution) {
  Resolution res = {.width = 640, .height = 360};
  EXPECT_EQ(Scale(res, 1, 2), (Resolution{.width = 320, .height = 180}));
  EXPECT_EQ(Scale(res, 1, 2, /*alignment=*/16),
            (Resolution{.width = 320, .height = 176}));
  EXPECT_EQ(Scale(res, Rational(3, 4)),
            (Resolution{.width = 480, .height = 270}));
}

TEST(ResolutionHelpersTest, FitsWithinMatcher) {
  Resolution res = {.width = 320, .height = 180};
  EXPECT_THAT(res, FitsWithin(Resolution{.width = 640, .height = 360}));
  EXPECT_THAT(res, FitsWithin(Resolution{.width = 160, .height = 90},
                              Resolution{.width = 640, .height = 360}));
  EXPECT_THAT(res, Not(FitsWithin(Resolution{.width = 160, .height = 90})));
  EXPECT_THAT(res, Not(FitsWithin(Resolution{.width = 400, .height = 200},
                                  Resolution{.width = 640, .height = 360})));
  EXPECT_TRUE(FitsWithin(res, Resolution{.width = 160, .height = 90},
                         Resolution{.width = 640, .height = 360}));
  EXPECT_FALSE(FitsWithin(res, Resolution{.width = 400, .height = 200},
                          Resolution{.width = 640, .height = 360}));
}

std::optional<VideoFrameBuffer::Type> ToVideoFrameBufferType(
    EncodingFormat format) {
  if (format.bit_depth == 8) {
    switch (format.sub_sampling) {
      case EncodingFormat::SubSampling::k420:
        return VideoFrameBuffer::Type::kI420;
      case EncodingFormat::SubSampling::k422:
        return VideoFrameBuffer::Type::kI422;
      case EncodingFormat::SubSampling::k444:
        return VideoFrameBuffer::Type::kI444;
    }
  } else if (format.bit_depth == 10) {
    switch (format.sub_sampling) {
      case EncodingFormat::SubSampling::k420:
        return VideoFrameBuffer::Type::kI010;
      case EncodingFormat::SubSampling::k422:
        return VideoFrameBuffer::Type::kI210;
      case EncodingFormat::SubSampling::k444:
        return VideoFrameBuffer::Type::kI410;
    }
  }
  return std::nullopt;
}

scoped_refptr<VideoFrameBuffer> CreateAndPopulateFrameBuffer(
    VideoFrameBuffer::Type type,
    const I420BufferInterface& source) {
  switch (type) {
    case VideoFrameBuffer::Type::kI420:
      return I420Buffer::Copy(source);
    case VideoFrameBuffer::Type::kI422:
      return I422Buffer::Copy(source);
    case VideoFrameBuffer::Type::kI444: {
      scoped_refptr<I444Buffer> i444 =
          I444Buffer::Create(source.width(), source.height());
      libyuv::I420ToI444(source.DataY(), source.StrideY(), source.DataU(),
                         source.StrideU(), source.DataV(), source.StrideV(),
                         i444->MutableDataY(), i444->StrideY(),
                         i444->MutableDataU(), i444->StrideU(),
                         i444->MutableDataV(), i444->StrideV(), source.width(),
                         source.height());
      return i444;
    }
    case VideoFrameBuffer::Type::kI010:
      return I010Buffer::Copy(source);
    case VideoFrameBuffer::Type::kI210:
      return I210Buffer::Copy(source);
    case VideoFrameBuffer::Type::kI410: {
      scoped_refptr<I444Buffer> i444 =
          I444Buffer::Create(source.width(), source.height());
      libyuv::I420ToI444(source.DataY(), source.StrideY(), source.DataU(),
                         source.StrideU(), source.DataV(), source.StrideV(),
                         i444->MutableDataY(), i444->StrideY(),
                         i444->MutableDataU(), i444->StrideU(),
                         i444->MutableDataV(), i444->StrideV(), source.width(),
                         source.height());
      scoped_refptr<I410Buffer> i410 =
          I410Buffer::Create(source.width(), source.height());
      libyuv::Convert8To16Plane(i444->DataY(), i444->StrideY(),
                                i410->MutableDataY(), i410->StrideY(), 1024,
                                source.width(), source.height());
      libyuv::Convert8To16Plane(i444->DataU(), i444->StrideU(),
                                i410->MutableDataU(), i410->StrideU(), 1024,
                                source.width(), source.height());
      libyuv::Convert8To16Plane(i444->DataV(), i444->StrideV(),
                                i410->MutableDataV(), i410->StrideV(), 1024,
                                source.width(), source.height());
      return i410;
    }
    default:
      return nullptr;
  }
}

// CBR settings that any encoder supporting CBR mode is expected to accept.
// The tests below derive their invalid settings from these.
constexpr StaticEncoderSettings::Cbr kValidCbrSettings = {
    .max_buffer_size = TimeDelta::Millis(1000),
    .target_buffer_size = TimeDelta::Millis(600),
    .max_intra_bitrate_factor = 3.0};

class VideoEncoderFunctionalTest
    : public ::testing::TestWithParam<FactoryCreator> {
 protected:
  VideoEncoderFunctionalTest() : env_(CreateTestEnvironment()) {}

  void SetUp() override {
    factory_ = GetParam()();
    decoder_factory_ = CreateTestDecoderFactory();
  }

  // Whether the encoder runs in CBR mode and honors `setting`. Settings that
  // are not honored may be ignored entirely, including their validation.
  bool HonorsCbrSetting(CbrSetting setting) const {
    Capabilities capabilities = factory_->GetEncoderCapabilities();
    const BitrateControl& bitrate_control = capabilities.bitrate_control();
    return bitrate_control.rc_modes().contains(RateControlMode::kCbr) &&
           bitrate_control.supported_cbr_settings().contains(setting);
  }

  std::unique_ptr<VideoEncoderInterface> CreateCbrEncoder(
      const StaticEncoderSettings::Cbr& cbr) {
    return factory_->CreateEncoder(
        StaticEncoderSettingsBuilder()
            .MaxEncodeDimensions(kDefaultResolution)
            .EncodingFormat(
                factory_->GetEncoderCapabilities().encoding_formats()[0])
            .RcMode(cbr)
            .MaxNumberOfThreads(1)
            .Build(),
        {});
  }

  bool SupportsInterLayerPrediction() const {
    Capabilities capabilities = factory_->GetEncoderCapabilities();
    return capabilities.prediction_constraints().max_spatial_layers() >= 2 &&
           capabilities.prediction_constraints().buffer_space_type() !=
               BufferSpaceType::kMultiInstance;
  }

  // Creates an encoder, places a keyframe in buffer 0, and then encodes a
  // temporal unit with one frame per entry in `spatial_ids`, in that order.
  // Each of those frames predicts from buffer 0 and updates a buffer of its
  // own. Returns the outputs of the frames of that temporal unit.
  std::vector<EncOut> EncodeWithSpatialIds(std::span<const int> spatial_ids) {
    TestConfig config = CreateTestConfig(factory_->GetEncoderCapabilities());
    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(config.static_settings, {});
    std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

    EncOut key;
    enc->Encode(
        frame_reader->PullFrame(), TemporalUnitSettings(Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(key)),
            config.rate_options)}));
    EXPECT_THAT(key, HasBitstreamAndMetaData());

    std::vector<EncOut> outs(spatial_ids.size());
    std::vector<FrameEncodeSettings> frame_settings;
    for (size_t i = 0; i < spatial_ids.size(); ++i) {
      frame_settings.push_back(
          BuildSettings(std::move(Fb().Res(kDefaultResolution)
                                      .S(spatial_ids[i])
                                      .Ref({0})
                                      .Upd(static_cast<int>(i))
                                      .Out(outs[i])),
                        config.rate_options));
    }
    enc->Encode(frame_reader->PullFrame(),
                TemporalUnitSettings(Timestamp::Millis(100)),
                std::move(frame_settings));
    return outs;
  }

  Environment env_;
  std::unique_ptr<VideoEncoderFactoryInterface> factory_;
  std::unique_ptr<VideoDecoderFactory> decoder_factory_;
};

TEST_P(VideoEncoderFunctionalTest, ReportsCodecAndImplementationName) {
  EXPECT_FALSE(factory_->CodecName().empty());
  EXPECT_FALSE(factory_->ImplementationName().empty());
}

TEST_P(VideoEncoderFunctionalTest, ValidBufferCount) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  EXPECT_GE(capabilities.prediction_constraints().num_buffers(), 0);
}

TEST_P(VideoEncoderFunctionalTest, ValidMaxReferences) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  EXPECT_GE(capabilities.prediction_constraints().max_references(), 0);
  EXPECT_LE(capabilities.prediction_constraints().max_references(),
            capabilities.prediction_constraints().num_buffers());
}

TEST_P(VideoEncoderFunctionalTest, ValidTemporalLayerCount) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  EXPECT_GE(capabilities.prediction_constraints().max_temporal_layers(), 1);
}

TEST_P(VideoEncoderFunctionalTest, ValidBufferSpaceType) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  BufferSpaceType bst =
      capabilities.prediction_constraints().buffer_space_type();
  EXPECT_TRUE(bst == BufferSpaceType::kSingleKeyframe ||
              bst == BufferSpaceType::kMultiKeyframe ||
              bst == BufferSpaceType::kMultiInstance);
}

TEST_P(VideoEncoderFunctionalTest, ValidSpatialLayerCount) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  EXPECT_GE(capabilities.prediction_constraints().max_spatial_layers(), 1);
}

TEST_P(VideoEncoderFunctionalTest, ValidScalingFactors) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::vector<Rational>& scaling_factors =
      capabilities.prediction_constraints().scaling_factors();
  EXPECT_FALSE(scaling_factors.empty());
  EXPECT_TRUE(absl::c_linear_search(scaling_factors, Rational(1, 1)));
  for (const Rational& factor : scaling_factors) {
    EXPECT_GT(factor.numerator, 0);
    EXPECT_GT(factor.denominator, 0);
  }
}

TEST_P(VideoEncoderFunctionalTest, ValidSupportedFrameTypes) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::set<FrameType>& supported_frame_types =
      capabilities.prediction_constraints().supported_frame_types();
  EXPECT_THAT(supported_frame_types, testing::Contains(FrameType::kKeyframe));
  EXPECT_THAT(supported_frame_types, testing::Contains(FrameType::kDeltaFrame));
}

TEST_P(VideoEncoderFunctionalTest, ValidResolutionBounds) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const InputConstraints& ic = capabilities.input_constraints();
  EXPECT_GT(ic.min().width, 0);
  EXPECT_GT(ic.min().height, 0);
  EXPECT_LE(ic.min().width, ic.max().width);
  EXPECT_LE(ic.min().height, ic.max().height);
}

TEST_P(VideoEncoderFunctionalTest, ValidPixelAlignment) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const InputConstraints& ic = capabilities.input_constraints();
  EXPECT_GE(ic.pixel_alignment(), 1);
  EXPECT_EQ(ic.min().width % ic.pixel_alignment(), 0);
  EXPECT_EQ(ic.min().height % ic.pixel_alignment(), 0);
  EXPECT_EQ(ic.max().width % ic.pixel_alignment(), 0);
  EXPECT_EQ(ic.max().height % ic.pixel_alignment(), 0);
}

TEST_P(VideoEncoderFunctionalTest, ValidInputFormats) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const InputConstraints& ic = capabilities.input_constraints();
  EXPECT_FALSE(ic.input_formats().empty());
}

TEST_P(VideoEncoderFunctionalTest, ValidEncodingFormats) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::vector<EncodingFormat>& enc_formats =
      capabilities.encoding_formats();
  EXPECT_FALSE(enc_formats.empty());
  for (const EncodingFormat& format : enc_formats) {
    EXPECT_THAT(format.bit_depth, testing::AnyOf(8, 10, 12));
    EXPECT_THAT(format.sub_sampling,
                testing::AnyOf(EncodingFormat::SubSampling::k420,
                               EncodingFormat::SubSampling::k422,
                               EncodingFormat::SubSampling::k444));
  }
}

TEST_P(VideoEncoderFunctionalTest, ValidQpRange) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const BitrateControl& bc = capabilities.bitrate_control();
  EXPECT_GE(bc.min_qp(), 0);
  EXPECT_LT(bc.min_qp(), bc.max_qp());
}

TEST_P(VideoEncoderFunctionalTest, ValidRateControlModes) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::set<RateControlMode>& rc_modes =
      capabilities.bitrate_control().rc_modes();
  EXPECT_FALSE(rc_modes.empty());
}

TEST_P(VideoEncoderFunctionalTest, ValidCbrSettings) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const BitrateControl& bitrate_control = capabilities.bitrate_control();
  if (!bitrate_control.rc_modes().contains(RateControlMode::kCbr)) {
    EXPECT_THAT(bitrate_control.supported_cbr_settings(), IsEmpty());
  }
}

TEST_P(VideoEncoderFunctionalTest, AcceptsValidCbrSettings) {
  if (!factory_->GetEncoderCapabilities().bitrate_control().rc_modes().contains(
          RateControlMode::kCbr)) {
    GTEST_SKIP() << "Encoder does not support CBR mode.";
  }
  EXPECT_THAT(CreateCbrEncoder(kValidCbrSettings), NotNull());
}

// An encoder must reject CBR settings that are out of range, so that a caller
// does not end up with an encoder silently doing something else than asked.
// Only the settings an encoder claims to honor are checked, the ones it
// ignores it may also ignore the validation of.
TEST_P(VideoEncoderFunctionalTest, RejectsCbrWithZeroMaxBufferSize) {
  if (!HonorsCbrSetting(CbrSetting::kBufferSizes)) {
    GTEST_SKIP() << "Encoder does not use the CBR buffer sizes.";
  }
  StaticEncoderSettings::Cbr cbr = kValidCbrSettings;
  cbr.max_buffer_size = TimeDelta::Zero();
  EXPECT_THAT(CreateCbrEncoder(cbr), IsNull());
}

TEST_P(VideoEncoderFunctionalTest, RejectsCbrWithNegativeMaxBufferSize) {
  if (!HonorsCbrSetting(CbrSetting::kBufferSizes)) {
    GTEST_SKIP() << "Encoder does not use the CBR buffer sizes.";
  }
  StaticEncoderSettings::Cbr cbr = kValidCbrSettings;
  cbr.max_buffer_size = -kValidCbrSettings.max_buffer_size;
  EXPECT_THAT(CreateCbrEncoder(cbr), IsNull());
}

TEST_P(VideoEncoderFunctionalTest, RejectsCbrWithInfiniteMaxBufferSize) {
  if (!HonorsCbrSetting(CbrSetting::kBufferSizes)) {
    GTEST_SKIP() << "Encoder does not use the CBR buffer sizes.";
  }
  StaticEncoderSettings::Cbr cbr = kValidCbrSettings;
  cbr.max_buffer_size = TimeDelta::PlusInfinity();
  EXPECT_THAT(CreateCbrEncoder(cbr), IsNull());
}

TEST_P(VideoEncoderFunctionalTest, RejectsCbrWithZeroTargetBufferSize) {
  if (!HonorsCbrSetting(CbrSetting::kBufferSizes)) {
    GTEST_SKIP() << "Encoder does not use the CBR buffer sizes.";
  }
  StaticEncoderSettings::Cbr cbr = kValidCbrSettings;
  cbr.target_buffer_size = TimeDelta::Zero();
  EXPECT_THAT(CreateCbrEncoder(cbr), IsNull());
}

TEST_P(VideoEncoderFunctionalTest, RejectsCbrWithTargetBufferSizeAboveMax) {
  if (!HonorsCbrSetting(CbrSetting::kBufferSizes)) {
    GTEST_SKIP() << "Encoder does not use the CBR buffer sizes.";
  }
  StaticEncoderSettings::Cbr cbr = kValidCbrSettings;
  cbr.target_buffer_size = cbr.max_buffer_size + TimeDelta::Millis(1);
  EXPECT_THAT(CreateCbrEncoder(cbr), IsNull());
}

TEST_P(VideoEncoderFunctionalTest, RejectsCbrWithIntraBitrateFactorBelowOne) {
  if (!HonorsCbrSetting(CbrSetting::kMaxIntraBitrateFactor)) {
    GTEST_SKIP() << "Encoder does not limit the size of intra frames.";
  }
  StaticEncoderSettings::Cbr cbr = kValidCbrSettings;
  cbr.max_intra_bitrate_factor = 0.5;
  EXPECT_THAT(CreateCbrEncoder(cbr), IsNull());
}

TEST_P(VideoEncoderFunctionalTest, RejectsCbrWithInfiniteIntraBitrateFactor) {
  if (!HonorsCbrSetting(CbrSetting::kMaxIntraBitrateFactor)) {
    GTEST_SKIP() << "Encoder does not limit the size of intra frames.";
  }
  StaticEncoderSettings::Cbr cbr = kValidCbrSettings;
  cbr.max_intra_bitrate_factor = std::numeric_limits<double>::infinity();
  EXPECT_THAT(CreateCbrEncoder(cbr), IsNull());
}

TEST_P(VideoEncoderFunctionalTest, ValidEffortLevelRange) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  std::pair<int, int> effort_range =
      capabilities.performance().min_max_effort_level();
  EXPECT_LE(effort_range.first, effort_range.second);
  EXPECT_LE(effort_range.first, 0);
  EXPECT_GE(effort_range.second, 0);
}

TEST_P(VideoEncoderFunctionalTest, EncodesAndDecodesKeyframe) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::set<RateControlMode>& rc_modes =
      capabilities.bitrate_control().rc_modes();
  ASSERT_FALSE(rc_modes.empty())
      << "Encoder must support at least one RC mode.";

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  TestConfig config = CreateTestConfig(capabilities);

  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  scoped_refptr<I420Buffer> input_frame = frame_reader->PullFrame();

  EncOut out;
  enc->Encode(input_frame, TemporalUnitSettings(Timestamp::Millis(0)),
              ToVec({BuildSettings(
                  std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
                  config.rate_options)}));

  ASSERT_THAT(out, HasBitstreamAndMetaData());
  VideoFrame decoded_frame = dec.Decode(out.bitstream);
  EXPECT_EQ(GetResolution(decoded_frame), kDefaultResolution);
  EXPECT_THAT(GetResolution(decoded_frame),
              FitsWithin(capabilities.input_constraints()));
}

TEST_P(VideoEncoderFunctionalTest, SupportsAllReferenceBuffers) {
  // This test validates that the encoder can in fact store up to `num_buffer()`
  // unique reference frames, by encode encoding that many unique frames and
  // then using each one of them as reference. E.g. for three buffer encoder a
  // reference structure like this is set up:
  //
  // [Key]-------------------- [P_2] ----------------- [P_2']
  //       \
  //         \--------- [P_1]---------------- [P_1']
  //           \
  //             [P_0] -------------- [P_0']
  // ...and so on until all N buffers are used up.
  // Each set of {[Key], [P_i], [P_i'] } are then decoded with a separate
  // decoder and verified to be equal with decoding all frames in sequence.

  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int num_buffers = capabilities.prediction_constraints().num_buffers();
  if (num_buffers < 1) {
    GTEST_SKIP() << "Encoder doesn't support reference buffers.";
  }
  const std::set<FrameType>& supported_frame_types =
      capabilities.prediction_constraints().supported_frame_types();
  if (!supported_frame_types.contains(FrameType::kKeyframe) ||
      !supported_frame_types.contains(FrameType::kDeltaFrame)) {
    GTEST_SKIP() << "Encoder must support keyframe and delta frame.";
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  // Generate N distinct pictures using moving squares with varying square
  // counts.
  std::vector<scoped_refptr<VideoFrameBuffer>> pictures;
  pictures.reserve(num_buffers);
  for (int i = 0; i < num_buffers; ++i) {
    std::unique_ptr<test::FrameGeneratorInterface> frame_gen =
        test::CreateSquareFrameGenerator(
            kDefaultResolution.width, kDefaultResolution.height,
            test::FrameGeneratorInterface::OutputType::kI420,
            /*num_squares=*/10 + (i * 5));
    for (int step = 0; step < i * 5; ++step) {
      frame_gen->NextFrame();
    }
    pictures.push_back(frame_gen->NextFrame().buffer);
  }

  const int last_buffer = num_buffers - 1;
  int64_t timestamp_ms = 0;

  // 1. Initial keyframe:
  // Encode an initial black keyframe updating the last buffer. This enables a
  // uniform start state regardless of which buffer is being tested.
  scoped_refptr<I420Buffer> black_frame =
      I420Buffer::Create(kDefaultResolution.width, kDefaultResolution.height);
  I420Buffer::SetBlack(black_frame.get());

  EncOut black_key_out;
  enc->Encode(black_frame,
              TemporalUnitSettings(Timestamp::Millis(timestamp_ms)),
              ToVec({BuildSettings(std::move(Fb().Res(kDefaultResolution)
                                                 .Upd(last_buffer)
                                                 .Key()
                                                 .Out(black_key_out)),
                                   config.rate_options)}));
  ASSERT_THAT(black_key_out, HasBitstreamAndMetaData());
  EXPECT_EQ(std::get<EncodedData>(black_key_out.res).frame_type,
            FrameType::kKeyframe);

  // 2. Update phase:
  // For each buffer i, encode picture P_i referencing the last buffer (holding
  // the black keyframe until overwritten at the end) and updating buffer i.
  // After this phase, each buffer i contains distinct picture P_i.
  std::vector<EncOut> update_outs(num_buffers);
  for (int i = 0; i < num_buffers; ++i) {
    timestamp_ms += 100;
    enc->Encode(pictures[i],
                TemporalUnitSettings(Timestamp::Millis(timestamp_ms)),
                ToVec({BuildSettings(std::move(Fb().Res(kDefaultResolution)
                                                   .Ref({last_buffer})
                                                   .Upd(i)
                                                   .Out(update_outs[i])),
                                     config.rate_options)}));
    ASSERT_THAT(update_outs[i], HasBitstreamAndMetaData());
  }

  // 3. Reference phase:
  // For each buffer i, encode picture P_i again, referencing only buffer i.
  // Since buffer i already contains picture P_i, this should produce a delta
  // frame P_i' with zero motion and high quality (PSNR).
  std::vector<EncOut> reference_outs(num_buffers);
  for (int i = 0; i < num_buffers; ++i) {
    timestamp_ms += 100;
    enc->Encode(
        pictures[i], TemporalUnitSettings(Timestamp::Millis(timestamp_ms)),
        ToVec({BuildSettings(
            std::move(
                Fb().Res(kDefaultResolution).Ref({i}).Out(reference_outs[i])),
            config.rate_options)}));
    ASSERT_THAT(reference_outs[i], HasBitstreamAndMetaData())
        << "Failed to encode picture referencing buffer " << i;
    EXPECT_EQ(std::get<EncodedData>(reference_outs[i].res).frame_type,
              FrameType::kDeltaFrame);
  }

  // 4. Combined decoder verification:
  // Have one decoder decode all frames in sequence: the black keyframe,
  // followed by each buffer's update frame and reference frame.
  TestDecoder combined_dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!combined_dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  VideoFrame dec_black_key = combined_dec.Decode(black_key_out.bitstream);
  EXPECT_EQ(GetResolution(dec_black_key), kDefaultResolution);

  std::vector<VideoFrame> combined_update_frames;
  combined_update_frames.reserve(num_buffers);
  for (int i = 0; i < num_buffers; ++i) {
    combined_update_frames.push_back(
        combined_dec.Decode(update_outs[i].bitstream));
    EXPECT_EQ(GetResolution(combined_update_frames.back()), kDefaultResolution);
  }

  std::vector<VideoFrame> combined_reference_frames;
  combined_reference_frames.reserve(num_buffers);
  for (int i = 0; i < num_buffers; ++i) {
    combined_reference_frames.push_back(
        combined_dec.Decode(reference_outs[i].bitstream));
    EXPECT_EQ(GetResolution(combined_reference_frames.back()),
              kDefaultResolution);
  }

  // 5. Individual decoder verification and quality check:
  // An array of decoders (one decoder per buffer). Each decoder dec_i decodes:
  // - The initial black keyframe (populating last_buffer)
  // - The update frame for buffer i (referencing last_buffer and updating
  // buffer i)
  // - The reference frame referencing buffer i
  // If the encoder referenced any buffer other than last_buffer during update,
  // or other than i during reference, dec_i will fail to decode or produce
  // corrupted output.
  std::vector<std::unique_ptr<TestDecoder>> decoders;
  decoders.reserve(num_buffers);
  for (int i = 0; i < num_buffers; ++i) {
    std::unique_ptr<TestDecoder> dec_i = std::make_unique<TestDecoder>(
        env_, decoder_factory_.get(), factory_->CodecName());
    ASSERT_TRUE(dec_i->IsSupported());

    VideoFrame dec_key = dec_i->Decode(black_key_out.bitstream);
    EXPECT_EQ(GetResolution(dec_key), kDefaultResolution);

    VideoFrame dec_update = dec_i->Decode(update_outs[i].bitstream);
    EXPECT_EQ(GetResolution(dec_update), kDefaultResolution);

    VideoFrame dec_ref = dec_i->Decode(reference_outs[i].bitstream);
    EXPECT_EQ(GetResolution(dec_ref), kDefaultResolution);

    // Verify that the output from the combined decoder matches each individual
    // one.
    EXPECT_TRUE(
        test::FrameBufsEqual(dec_update.video_frame_buffer(),
                             combined_update_frames[i].video_frame_buffer()))
        << "Update frame mismatch for buffer " << i;
    EXPECT_TRUE(
        test::FrameBufsEqual(dec_ref.video_frame_buffer(),
                             combined_reference_frames[i].video_frame_buffer()))
        << "Reference frame mismatch for buffer " << i;

    // Quality check per decoder.
    double psnr = Psnr(pictures[i]->ToI420(), dec_ref);
    EXPECT_THAT(psnr, Gt(40.0))
        << "Low PSNR when referencing buffer " << i << ": " << psnr;

    decoders.push_back(std::move(dec_i));
  }
}

// The API mandates that a target buffer must be specified for all frames that
// update a buffer. This includes keyframe, even if the encoder would
// implicitly update all buffers. Referencing a buffer that has not been
// explicitly updated via the API is not allowed.
TEST_P(VideoEncoderFunctionalTest, KeyframeUpdatesSpecifiedBuffer) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  if (capabilities.prediction_constraints().num_buffers() < 2) {
    GTEST_SKIP() << "Encoder must support multiple buffers for this test.";
  }

  // Encode a keyframe and store in buffer 1, then attempt to reference
  // buffer 0. According to the API contracts, this is not allowed.
  constexpr int kTargetBuffer = 1;
  constexpr int kRefBuffer = 0;

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  scoped_refptr<I420Buffer> raw_key = frame_reader->PullFrame();
  scoped_refptr<I420Buffer> raw_delta = frame_reader->PullFrame();

  EncOut key;
  enc->Encode(
      raw_key, TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(
          std::move(
              Fb().Res(kDefaultResolution).Upd(kTargetBuffer).Key().Out(key)),
          config.rate_options)}));

  ASSERT_THAT(key, HasBitstreamAndMetaData());
  VideoFrame decoded_key = dec.Decode(key.bitstream);
  EXPECT_EQ(GetResolution(decoded_key), kDefaultResolution);

  EncOut delta;
  enc->Encode(
      raw_delta, TemporalUnitSettings(Timestamp::Millis(100)),
      ToVec({BuildSettings(
          std::move(Fb().Res(kDefaultResolution).Ref({kRefBuffer}).Out(delta)),
          config.rate_options)}));

  EXPECT_THAT(delta, Not(HasBitstreamAndMetaData()));
}

// Verifies that the encoder can reference up to `max_references()` reference
// buffers simultaneously in a single delta frame.
TEST_P(VideoEncoderFunctionalTest, SupportsMaxReferences) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int max_references = capabilities.prediction_constraints().max_references();
  if (max_references == 0) {
    GTEST_SKIP() << "Encoder doesn't support references.";
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  // Encode N + 1 frames (i = 0..max_references).
  // Frame 0 is a keyframe updating buffer 0.
  // For i > 0, frame i references buffers 0..i-1 and updates buffer i.
  for (int i = 0; i <= max_references; ++i) {
    scoped_refptr<I420Buffer> raw_frame = frame_reader->PullFrame();
    ASSERT_TRUE(raw_frame);
    EncOut out;
    int64_t timestamp_ms = i * 100;

    if (i == 0) {
      enc->Encode(
          raw_frame, TemporalUnitSettings(Timestamp::Millis(timestamp_ms)),
          ToVec({BuildSettings(
              std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
              config.rate_options)}));
      ASSERT_THAT(out, HasBitstreamAndMetaData())
          << "Failed to encode initial keyframe.";
      EXPECT_EQ(std::get<EncodedData>(out.res).frame_type,
                FrameType::kKeyframe);
    } else {
      std::vector<int> refs;
      refs.reserve(i);
      for (int r = 0; r < i; ++r) {
        refs.push_back(r);
      }
      enc->Encode(
          raw_frame, TemporalUnitSettings(Timestamp::Millis(timestamp_ms)),
          ToVec({BuildSettings(
              std::move(Fb().Res(kDefaultResolution).Ref(refs).Upd(i).Out(out)),
              config.rate_options)}));
      ASSERT_THAT(out, HasBitstreamAndMetaData())
          << "Failed to encode delta frame " << i << " referencing "
          << refs.size() << " buffers.";
      EXPECT_EQ(std::get<EncodedData>(out.res).frame_type,
                FrameType::kDeltaFrame);
    }

    VideoFrame decoded_frame = dec.Decode(out.bitstream);
    EXPECT_EQ(GetResolution(decoded_frame), kDefaultResolution);

    double psnr = Psnr(raw_frame->ToI420(), decoded_frame);
    EXPECT_THAT(psnr, Gt(35.0)) << "Low PSNR for frame " << i << ": " << psnr;
  }
}

// Verifies that referencing the same buffer slot multiple times in a single
// frame's reference list is rejected.
TEST_P(VideoEncoderFunctionalTest, DuplicateReferenceBuffersNotAllowed) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  if (capabilities.prediction_constraints().max_references() < 2) {
    GTEST_SKIP() << "Encoder must support multiple references for this test.";
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  scoped_refptr<I420Buffer> raw_key = frame_reader->PullFrame();
  scoped_refptr<I420Buffer> raw_delta = frame_reader->PullFrame();

  EncOut key;
  enc->Encode(raw_key, TemporalUnitSettings(Timestamp::Millis(0)),
              ToVec({BuildSettings(
                  std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(key)),
                  config.rate_options)}));
  ASSERT_THAT(key, HasBitstreamAndMetaData());

  EncOut delta;
  enc->Encode(
      raw_delta, TemporalUnitSettings(Timestamp::Millis(100)),
      ToVec({BuildSettings(
          std::move(Fb().Res(kDefaultResolution).Ref({0, 0}).Out(delta)),
          config.rate_options)}));

  EXPECT_THAT(delta, Not(HasBitstreamAndMetaData()));
}

// Verifies that frames of a temporal unit given in decreasing spatial id order
// are rejected.
TEST_P(VideoEncoderFunctionalTest, DecreasingSpatialIdsNotAllowed) {
  if (!SupportsInterLayerPrediction()) {
    GTEST_SKIP() << "Encoder must support inter-layer prediction.";
  }
  // The same frames are accepted in increasing spatial id order.
  for (const EncOut& out : EncodeWithSpatialIds(std::array{0, 1})) {
    EXPECT_THAT(out, HasBitstreamAndMetaData());
  }
  for (const EncOut& out : EncodeWithSpatialIds(std::array{1, 0})) {
    EXPECT_THAT(out, Not(HasBitstreamAndMetaData()));
  }
}

// Verifies that multiple frames with the same spatial id in a temporal unit are
// rejected.
TEST_P(VideoEncoderFunctionalTest, DuplicateSpatialIdsNotAllowed) {
  if (!SupportsInterLayerPrediction()) {
    GTEST_SKIP() << "Encoder must support inter-layer prediction.";
  }
  // The same frames are accepted with distinct spatial ids.
  for (const EncOut& out : EncodeWithSpatialIds(std::array{0, 1})) {
    EXPECT_THAT(out, HasBitstreamAndMetaData());
  }
  for (const EncOut& out : EncodeWithSpatialIds(std::array{0, 0})) {
    EXPECT_THAT(out, Not(HasBitstreamAndMetaData()));
  }
}

// Verifies temporal layering with a 2-layer pattern (T0, T1, T0, T1).
TEST_P(VideoEncoderFunctionalTest, SupportsTemporalLayers) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int max_temporal_layers =
      capabilities.prediction_constraints().max_temporal_layers();
  if (max_temporal_layers < 2) {
    GTEST_SKIP() << "Encoder does not support multiple temporal layers.";
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  // 2-layer temporal pattern: T0, T1, T0, T1
  // Frame 0: Keyframe, T0, updates buffer 0
  scoped_refptr<I420Buffer> raw_frame0 = frame_reader->PullFrame();
  EncOut out0;
  enc->Encode(
      raw_frame0, TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(
          std::move(Fb().Res(kDefaultResolution).T(0).Upd(0).Key().Out(out0)),
          config.rate_options)}));
  ASSERT_THAT(out0, HasBitstreamAndMetaData());
  VideoFrame dec0 = dec.Decode(out0.bitstream);
  EXPECT_EQ(GetResolution(dec0), kDefaultResolution);

  // Frame 1: Delta frame, T1, references buffer 0, updates buffer 1
  scoped_refptr<I420Buffer> raw_frame1 = frame_reader->PullFrame();
  EncOut out1;
  enc->Encode(
      raw_frame1, TemporalUnitSettings(Timestamp::Millis(100)),
      ToVec({BuildSettings(
          std::move(
              Fb().Res(kDefaultResolution).T(1).Ref({0}).Upd(1).Out(out1)),
          config.rate_options)}));
  ASSERT_THAT(out1, HasBitstreamAndMetaData());
  VideoFrame dec1 = dec.Decode(out1.bitstream);
  EXPECT_EQ(GetResolution(dec1), kDefaultResolution);

  // Frame 2: Delta frame, T0, references buffer 0, updates buffer 0
  scoped_refptr<I420Buffer> raw_frame2 = frame_reader->PullFrame();
  EncOut out2;
  enc->Encode(
      raw_frame2, TemporalUnitSettings(Timestamp::Millis(200)),
      ToVec({BuildSettings(
          std::move(
              Fb().Res(kDefaultResolution).T(0).Ref({0}).Upd(0).Out(out2)),
          config.rate_options)}));
  ASSERT_THAT(out2, HasBitstreamAndMetaData());
  VideoFrame dec2 = dec.Decode(out2.bitstream);
  EXPECT_EQ(GetResolution(dec2), kDefaultResolution);

  // Frame 3: Delta frame, T1, references buffer 0, updates buffer 1
  scoped_refptr<I420Buffer> raw_frame3 = frame_reader->PullFrame();
  EncOut out3;
  enc->Encode(
      raw_frame3, TemporalUnitSettings(Timestamp::Millis(300)),
      ToVec({BuildSettings(
          std::move(
              Fb().Res(kDefaultResolution).T(1).Ref({0}).Upd(1).Out(out3)),
          config.rate_options)}));
  ASSERT_THAT(out3, HasBitstreamAndMetaData());
  VideoFrame dec3 = dec.Decode(out3.bitstream);
  EXPECT_EQ(GetResolution(dec3), kDefaultResolution);
}

// Verifies encoding multiple independent spatial layers in the same temporal
// unit, where higher spatial layers do not predict from lower spatial layers.
TEST_P(VideoEncoderFunctionalTest, SupportsIndependentSpatialLayers) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int max_spatial_layers =
      capabilities.prediction_constraints().max_spatial_layers();
  if (max_spatial_layers < 2) {
    GTEST_SKIP() << "Encoder doesn't support multiple spatial layers.";
  }

  BufferSpaceType buffer_space_type =
      capabilities.prediction_constraints().buffer_space_type();

  if (buffer_space_type == BufferSpaceType::kSingleKeyframe) {
    const std::set<FrameType>& supported_frame_types =
        capabilities.prediction_constraints().supported_frame_types();
    if (!supported_frame_types.contains(FrameType::kStartFrame)) {
      GTEST_SKIP() << "kSingleKeyframe encoder must support start frames.";
    }
  }

  if (buffer_space_type == BufferSpaceType::kMultiKeyframe ||
      buffer_space_type == BufferSpaceType::kSingleKeyframe) {
    EXPECT_LE(max_spatial_layers,
              capabilities.prediction_constraints().num_buffers());
  }

  int num_layers = max_spatial_layers;
  std::vector<Rational> factors =
      FindSpatialLayerScalingFactors(capabilities, num_layers);
  if (factors.empty()) {
    GTEST_SKIP() << "Could not find valid scaling factors.";
  }

  constexpr Resolution kBaseResolution = {.width = 640, .height = 384};
  int alignment = capabilities.input_constraints().pixel_alignment();
  std::vector<Resolution> resolutions =
      GetSpatialLayerResolutions(kBaseResolution, factors, alignment);

  TestConfig config = CreateTestConfig(capabilities);
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(kBaseResolution)
          .EncodingFormat(config.static_settings.encoding_format())
          .RcMode(config.static_settings.rc_mode())
          .MaxNumberOfThreads(config.static_settings.max_number_of_threads())
          .Build();
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  scoped_refptr<I420Buffer> input_frame =
      I420Buffer::Create(kBaseResolution.width, kBaseResolution.height);
  input_frame->ScaleFrom(*frame_reader->PullFrame());

  std::vector<EncOut> outs(num_layers);
  std::vector<FrameEncodeSettings> frame_settings;
  frame_settings.reserve(num_layers);

  for (int i = 0; i < num_layers; ++i) {
    Fb fb;
    fb.Res(resolutions[i]).S(i).Out(outs[i]);
    if (i == 0) {
      fb.Key().Upd(0);
    } else {
      switch (buffer_space_type) {
        case BufferSpaceType::kMultiInstance:
          fb.Key().Upd(0);
          break;
        case BufferSpaceType::kMultiKeyframe:
          fb.Key().Upd(i);
          break;
        case BufferSpaceType::kSingleKeyframe:
          fb.Start().Upd(i);
          break;
      }
    }
    frame_settings.push_back(BuildSettings(std::move(fb), config.rate_options));
  }

  enc->Encode(input_frame, TemporalUnitSettings(Timestamp::Millis(0)),
              std::move(frame_settings));

  for (int i = 0; i < num_layers; ++i) {
    ASSERT_THAT(outs[i], HasBitstreamAndMetaData())
        << "Failed to encode spatial layer " << i;
    const FrameType expected_frame_type =
        (i > 0 && buffer_space_type == BufferSpaceType::kSingleKeyframe)
            ? FrameType::kStartFrame
            : FrameType::kKeyframe;
    EXPECT_EQ(std::get<EncodedData>(outs[i].res).frame_type,
              expected_frame_type);
    TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
    if (!dec.IsSupported()) {
      GTEST_SKIP() << "No matching decoder found for codec: "
                   << factory_->CodecName();
    }
    VideoFrame decoded = dec.Decode(outs[i].bitstream);
    EXPECT_EQ(GetResolution(decoded), resolutions[i]);
  }
}

// Verifies spatial scalability with inter-layer prediction (S1 predicting from
// S0, etc.).
TEST_P(VideoEncoderFunctionalTest, SupportsInterLayerPrediction) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int max_spatial_layers =
      capabilities.prediction_constraints().max_spatial_layers();
  if (max_spatial_layers < 2) {
    GTEST_SKIP() << "Encoder doesn't support multiple spatial layers.";
  }

  BufferSpaceType buffer_space_type =
      capabilities.prediction_constraints().buffer_space_type();
  if (buffer_space_type == BufferSpaceType::kMultiInstance) {
    GTEST_SKIP() << "Encoder does not support inter-layer prediction with "
                    "kMultiInstance.";
  }

  EXPECT_LE(max_spatial_layers,
            capabilities.prediction_constraints().num_buffers());

  int num_layers = max_spatial_layers;
  std::vector<Rational> factors =
      FindSpatialLayerScalingFactors(capabilities, num_layers);
  if (factors.empty()) {
    GTEST_SKIP() << "Could not find valid scaling factors.";
  }

  constexpr Resolution kBaseResolution = {.width = 640, .height = 384};
  int alignment = capabilities.input_constraints().pixel_alignment();
  std::vector<Resolution> resolutions =
      GetSpatialLayerResolutions(kBaseResolution, factors, alignment);

  TestConfig config = CreateTestConfig(capabilities);
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(kBaseResolution)
          .EncodingFormat(config.static_settings.encoding_format())
          .RcMode(config.static_settings.rc_mode())
          .MaxNumberOfThreads(config.static_settings.max_number_of_threads())
          .Build();
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  scoped_refptr<I420Buffer> input_frame =
      I420Buffer::Create(kBaseResolution.width, kBaseResolution.height);
  input_frame->ScaleFrom(*frame_reader->PullFrame());

  std::vector<EncOut> outs(num_layers);
  std::vector<FrameEncodeSettings> frame_settings;
  frame_settings.reserve(num_layers);

  for (int i = 0; i < num_layers; ++i) {
    Fb fb;
    fb.Res(resolutions[i]).S(i).Out(outs[i]).Upd(i);
    if (i == 0) {
      fb.Key();
    } else {
      fb.Delta().Ref({i - 1});
    }
    frame_settings.push_back(BuildSettings(std::move(fb), config.rate_options));
  }

  enc->Encode(input_frame, TemporalUnitSettings(Timestamp::Millis(0)),
              std::move(frame_settings));

  for (int i = 0; i < num_layers; ++i) {
    TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
    if (!dec.IsSupported()) {
      GTEST_SKIP() << "No matching decoder found for codec: "
                   << factory_->CodecName();
    }

    std::optional<VideoFrame> decoded;
    for (int j = 0; j <= i; ++j) {
      ASSERT_THAT(outs[j], HasBitstreamAndMetaData())
          << "Failed to encode spatial layer " << j;
      if (j == 0) {
        EXPECT_EQ(std::get<EncodedData>(outs[j].res).frame_type,
                  FrameType::kKeyframe);
      } else {
        EXPECT_EQ(std::get<EncodedData>(outs[j].res).frame_type,
                  FrameType::kDeltaFrame);
      }
      decoded = dec.Decode(outs[j].bitstream);
    }
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(GetResolution(*decoded), resolutions[i]);
  }
}

// Verifies reference frame scaling across frames of different resolutions.
TEST_P(VideoEncoderFunctionalTest, DISABLED_ReferenceFrameScaling) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::vector<Rational>& scaling_factors =
      capabilities.prediction_constraints().scaling_factors();
  if (scaling_factors.empty()) {
    GTEST_SKIP() << "Encoder does not report any scaling factors.";
  }

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  constexpr Resolution kBaseResolution = {.width = 640, .height = 384};
  int alignment = capabilities.input_constraints().pixel_alignment();
  int effective_alignment = std::max(2, alignment);

  // Compute maximum encode dimensions needed across all scaling factors.
  Resolution max_encode_dimensions = kBaseResolution;
  for (const Rational& factor : scaling_factors) {
    Resolution scaled = Scale(kBaseResolution, factor, effective_alignment);
    max_encode_dimensions.width =
        std::max(max_encode_dimensions.width, scaled.width);
    max_encode_dimensions.height =
        std::max(max_encode_dimensions.height, scaled.height);
  }

  TestConfig config = CreateTestConfig(capabilities);
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(max_encode_dimensions)
          .EncodingFormat(config.static_settings.encoding_format())
          .RcMode(config.static_settings.rc_mode())
          .MaxNumberOfThreads(config.static_settings.max_number_of_threads())
          .Build();

  for (const Rational& factor : scaling_factors) {
    Resolution scaled_resolution =
        Scale(kBaseResolution, factor, effective_alignment);

    if (!FitsWithin(scaled_resolution, capabilities.input_constraints())) {
      continue;
    }

    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(static_settings, {});
    std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

    // 1. Encode a base resolution keyframe and store it in reference buffer 0.
    scoped_refptr<I420Buffer> raw_frame0 = frame_reader->PullFrame();
    scoped_refptr<I420Buffer> base_frame =
        I420Buffer::Create(kBaseResolution.width, kBaseResolution.height);
    base_frame->ScaleFrom(*raw_frame0);

    EncOut key_out;
    enc->Encode(
        base_frame, TemporalUnitSettings(Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(Fb().Res(kBaseResolution).Upd(0).Key().Out(key_out)),
            config.rate_options)}));

    ASSERT_THAT(key_out, HasBitstreamAndMetaData())
        << "Failed to encode base keyframe at " << kBaseResolution.width << "x"
        << kBaseResolution.height;
    VideoFrame decoded_key = dec.Decode(key_out.bitstream);
    EXPECT_EQ(GetResolution(decoded_key), kBaseResolution);

    // 2. Encode a delta frame at the scaled resolution referencing buffer 0.
    scoped_refptr<I420Buffer> raw_frame1 = frame_reader->PullFrame();
    scoped_refptr<I420Buffer> scaled_frame =
        I420Buffer::Create(scaled_resolution.width, scaled_resolution.height);
    scaled_frame->ScaleFrom(*raw_frame1);

    EncOut delta_out;
    enc->Encode(
        scaled_frame, TemporalUnitSettings(Timestamp::Millis(100)),
        ToVec({BuildSettings(
            std::move(Fb().Res(scaled_resolution).Ref({0}).Out(delta_out)),
            config.rate_options)}));

    ASSERT_THAT(delta_out, HasBitstreamAndMetaData())
        << "Failed to encode delta frame with scaling factor "
        << factor.numerator << ":" << factor.denominator << " ("
        << scaled_resolution.width << "x" << scaled_resolution.height << ")";
    EXPECT_EQ(std::get<EncodedData>(delta_out.res).frame_type,
              FrameType::kDeltaFrame);
    VideoFrame decoded_delta = dec.Decode(delta_out.bitstream);
    EXPECT_EQ(GetResolution(decoded_delta), scaled_resolution);
    EXPECT_THAT(GetResolution(decoded_delta),
                FitsWithin(capabilities.input_constraints()));

    double psnr = Psnr(scaled_frame, decoded_delta);
    double scale_ratio =
        static_cast<double>(factor.numerator) / factor.denominator;

    // Expect each doubling in resolution to cause ~3dB lower PSNR when upscaled
    // across temporal frames.
    double expected_psnr =
        (scale_ratio <= 1.0) ? 38.0 : (38.0 - 3.0 * std::log2(scale_ratio));
    EXPECT_THAT(psnr, Gt(expected_psnr))
        << "Low PSNR for scaling factor " << factor.numerator << ":"
        << factor.denominator << " (" << scaled_resolution.width << "x"
        << scaled_resolution.height << "): " << psnr;
  }
}

// Verifies spatial layer scaling where higher layers upscale from lower layers.
TEST_P(VideoEncoderFunctionalTest, SpatialLayerScaling) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  if (capabilities.prediction_constraints().max_spatial_layers() < 2) {
    GTEST_SKIP() << "Encoder doesn't support multiple spatial layers.";
  }

  // Filter scaling factors > 1:1, representing inter-layer upscaling from a
  // lower reference layer.
  std::vector<Rational> svc_factors;
  for (const Rational& f :
       capabilities.prediction_constraints().scaling_factors()) {
    if (static_cast<double>(f.numerator) / f.denominator > 1.0) {
      svc_factors.push_back(f);
    }
  }

  if (svc_factors.empty()) {
    GTEST_SKIP() << "Encoder does not report any scaling factors > 1:1.";
  }

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  int alignment = capabilities.input_constraints().pixel_alignment();
  int effective_alignment = std::max(2, alignment);

  constexpr Resolution kMaxLayerResolution = {.width = 1280, .height = 768};

  TestConfig config = CreateTestConfig(capabilities);
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(kMaxLayerResolution)
          .EncodingFormat(config.static_settings.encoding_format())
          .RcMode(config.static_settings.rc_mode())
          .MaxNumberOfThreads(config.static_settings.max_number_of_threads())
          .Build();

  for (const Rational& factor : svc_factors) {
    // S0 is the base layer (downscaled by factor relative to S1).
    // S1 is the upscaled layer (scaled up by factor relative to S0).
    Resolution s0_resolution = Scale(
        kMaxLayerResolution, Rational(factor.denominator, factor.numerator),
        effective_alignment);
    Resolution s1_resolution =
        Scale(s0_resolution, factor, effective_alignment);

    if (!FitsWithin(s0_resolution, capabilities.input_constraints())) {
      continue;
    }
    if (!FitsWithin(s1_resolution, capabilities.input_constraints())) {
      continue;
    }

    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(static_settings, {});
    std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

    scoped_refptr<I420Buffer> raw_frame = frame_reader->PullFrame();
    scoped_refptr<I420Buffer> input_frame =
        I420Buffer::Create(s1_resolution.width, s1_resolution.height);
    input_frame->ScaleFrom(*raw_frame);

    EncOut s0_out;
    EncOut s1_out;
    std::vector<FrameEncodeSettings> tu_settings;
    // S0: Base layer keyframe, stored in buffer 0.
    tu_settings.push_back(BuildSettings(
        std::move(Fb().Res(s0_resolution).S(0).Upd(0).Key().Out(s0_out)),
        config.rate_options));
    // S1: Upscaled layer delta frame, referencing S0 in buffer 0.
    tu_settings.push_back(BuildSettings(
        std::move(Fb().Res(s1_resolution).S(1).Ref({0}).Upd(1).Out(s1_out)),
        config.rate_options));

    enc->Encode(input_frame, TemporalUnitSettings(Timestamp::Millis(0)),
                std::move(tu_settings));

    ASSERT_THAT(s0_out, HasBitstreamAndMetaData())
        << "Failed to encode S0 keyframe at " << s0_resolution.width << "x"
        << s0_resolution.height;
    EXPECT_EQ(std::get<EncodedData>(s0_out.res).frame_type,
              FrameType::kKeyframe);
    VideoFrame decoded_s0 = dec.Decode(s0_out.bitstream);
    EXPECT_EQ(GetResolution(decoded_s0), s0_resolution);

    ASSERT_THAT(s1_out, HasBitstreamAndMetaData())
        << "Failed to encode S1 delta frame with scaling factor "
        << factor.numerator << ":" << factor.denominator << " ("
        << s1_resolution.width << "x" << s1_resolution.height << " referencing "
        << s0_resolution.width << "x" << s0_resolution.height << ")";
    EXPECT_EQ(std::get<EncodedData>(s1_out.res).frame_type,
              FrameType::kDeltaFrame);
    VideoFrame decoded_s1 = dec.Decode(s1_out.bitstream);
    EXPECT_EQ(GetResolution(decoded_s1), s1_resolution);

    double psnr = Psnr(input_frame, decoded_s1);
    double scale_ratio =
        static_cast<double>(factor.numerator) / factor.denominator;
    double expected_psnr = 40.0 - 3.0 * std::log2(scale_ratio);
    EXPECT_THAT(psnr, Gt(expected_psnr))
        << "Low PSNR for scaling factor " << factor.numerator << ":"
        << factor.denominator << " (" << s1_resolution.width << "x"
        << s1_resolution.height << " referencing " << s0_resolution.width << "x"
        << s0_resolution.height << "): " << psnr;
  }
}

// Verifies that in kSingleKeyframe buffer space mode, inserting a keyframe on
// an intermediate spatial layer invalidates/resets buffers according to API
// semantics.
TEST_P(VideoEncoderFunctionalTest, MidTemporalUnitKeyframeResetsBuffers) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  if (capabilities.prediction_constraints().buffer_space_type() !=
      BufferSpaceType::kSingleKeyframe) {
    GTEST_SKIP() << "Test only applies to encoders with kSingleKeyframe "
                    "buffer space type.";
  }

  if (capabilities.prediction_constraints().max_spatial_layers() < 3) {
    GTEST_SKIP() << "Encoder doesn't support at least 3 spatial layers.";
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  EncOut tu0_out;
  enc->Encode(
      frame_reader->PullFrame(), TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec(
          {BuildSettings(
               std::move(Fb().Res(kDefaultResolution).S(0).Upd(0).Key()),
               config.rate_options),
           BuildSettings(std::move(Fb().Res(kDefaultResolution).S(1).Ref({0})),
                         config.rate_options),
           BuildSettings(
               std::move(
                   Fb().Res(kDefaultResolution).S(2).Ref({0}).Out(tu0_out)),
               config.rate_options)}));
  EXPECT_THAT(tu0_out, HasBitstreamAndMetaData());

  EncOut tu1_s0;
  enc->Encode(
      frame_reader->PullFrame(), TemporalUnitSettings(Timestamp::Millis(100)),
      ToVec(
          {BuildSettings(std::move(Fb().Res(kDefaultResolution)
                                       .S(0)
                                       .Upd(0)
                                       .Ref({0})
                                       .Out(tu1_s0)),
                         config.rate_options),
           BuildSettings(
               std::move(Fb().Res(kDefaultResolution).S(1).Upd(1).Key()),
               config.rate_options),
           BuildSettings(std::move(Fb().Res(kDefaultResolution).S(2).Ref({0})),
                         config.rate_options)}));

  EXPECT_THAT(tu1_s0, Not(HasBitstreamAndMetaData()));
}

// Verifies dynamic resolution switching between consecutive frames without
// reinitializing.
TEST_P(VideoEncoderFunctionalTest, ResolutionSwitching) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  std::vector<Rational> factors =
      FindSpatialLayerScalingFactors(capabilities, 2);
  if (factors.size() < 2) {
    GTEST_SKIP() << "Encoder doesn't support resolution switching.";
  }

  int num_resolutions = std::min(3, static_cast<int>(factors.size()));
  factors = FindSpatialLayerScalingFactors(capabilities, num_resolutions);

  int alignment = capabilities.input_constraints().pixel_alignment();
  std::vector<Resolution> resolutions =
      GetSpatialLayerResolutions(kDefaultResolution, factors, alignment);

  TestConfig config = CreateTestConfig(capabilities);
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(resolutions.back())
          .EncodingFormat(config.static_settings.encoding_format())
          .RcMode(config.static_settings.rc_mode())
          .MaxNumberOfThreads(config.static_settings.max_number_of_threads())
          .Build();

  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found.";
  }

  int mid_idx = num_resolutions > 2 ? 1 : 0;
  Resolution res0 = resolutions[mid_idx];
  Resolution res1 = resolutions.back();
  Resolution res2 = resolutions[0];

  scoped_refptr<I420Buffer> in0 = frame_reader->PullFrame();
  EncOut tu0;
  enc->Encode(
      in0, TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(std::move(Fb().Res(res0).Upd(0).Key().Out(tu0)),
                           config.rate_options)}));
  ASSERT_THAT(tu0, HasBitstreamAndMetaData());
  VideoFrame f0 = dec.Decode(tu0.bitstream);
  EXPECT_EQ(GetResolution(f0), res0);

  scoped_refptr<I420Buffer> in1 = frame_reader->PullFrame();
  EncOut tu1;
  enc->Encode(in1, TemporalUnitSettings(Timestamp::Millis(100)),
              ToVec({BuildSettings(std::move(Fb().Res(res1).Ref({0}).Out(tu1)),
                                   config.rate_options)}));
  ASSERT_THAT(tu1, HasBitstreamAndMetaData());
  VideoFrame f1 = dec.Decode(tu1.bitstream);
  EXPECT_EQ(GetResolution(f1), res1);

  scoped_refptr<I420Buffer> in2 = frame_reader->PullFrame();
  EncOut tu2;
  enc->Encode(in2, TemporalUnitSettings(Timestamp::Millis(200)),
              ToVec({BuildSettings(std::move(Fb().Res(res2).Ref({0}).Out(tu2)),
                                   config.rate_options)}));
  ASSERT_THAT(tu2, HasBitstreamAndMetaData());
  VideoFrame f2 = dec.Decode(tu2.bitstream);
  EXPECT_EQ(GetResolution(f2), res2);
}

// Verifies that varying input frame resolutions are properly handled and scaled
// to the target encode resolution.
TEST_P(VideoEncoderFunctionalTest, InputResolutionSwitching) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  std::vector<Rational> factors =
      FindSpatialLayerScalingFactors(capabilities, 2);
  if (factors.size() < 2) {
    GTEST_SKIP() << "Encoder doesn't support input resolution switching.";
  }

  int num_resolutions = std::min(3, static_cast<int>(factors.size()));
  factors = FindSpatialLayerScalingFactors(capabilities, num_resolutions);

  int alignment = capabilities.input_constraints().pixel_alignment();
  std::vector<Resolution> resolutions =
      GetSpatialLayerResolutions(kDefaultResolution, factors, alignment);

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found.";
  }

  Resolution target_resolution = resolutions[0];

  scoped_refptr<I420Buffer> in0 = frame_reader->PullFrame(
      nullptr, resolutions.back(), {.num = 1, .den = 1});
  EncOut tu0;
  enc->Encode(in0, TemporalUnitSettings(Timestamp::Millis(0)),
              ToVec({BuildSettings(
                  std::move(Fb().Res(target_resolution).Upd(0).Key().Out(tu0)),
                  config.rate_options)}));

  int mid_idx = num_resolutions > 2 ? 1 : 0;
  scoped_refptr<I420Buffer> in1 = frame_reader->PullFrame(
      nullptr, resolutions[mid_idx], {.num = 1, .den = 1});
  EncOut tu1;
  enc->Encode(in1, TemporalUnitSettings(Timestamp::Millis(100)),
              ToVec({BuildSettings(
                  std::move(Fb().Res(target_resolution).Ref({0}).Out(tu1)),
                  config.rate_options)}));

  scoped_refptr<I420Buffer> in2 =
      frame_reader->PullFrame(nullptr, resolutions[0], {.num = 1, .den = 1});
  EncOut tu2;
  enc->Encode(in2, TemporalUnitSettings(Timestamp::Millis(200)),
              ToVec({BuildSettings(
                  std::move(Fb().Res(target_resolution).Ref({0}).Out(tu2)),
                  config.rate_options)}));

  VideoFrame f0 = dec.Decode(tu0.bitstream);
  EXPECT_EQ(GetResolution(f0), target_resolution);

  VideoFrame f1 = dec.Decode(tu1.bitstream);
  EXPECT_EQ(GetResolution(f1), target_resolution);

  VideoFrame f2 = dec.Decode(tu2.bitstream);
  EXPECT_EQ(GetResolution(f2), target_resolution);
  EXPECT_THAT(Psnr(in2, f2), Gt(40.0));
}

// Verifies combined temporal and spatial scalability (SVC) encoding and
// decoding.
TEST_P(VideoEncoderFunctionalTest, TempoSpatial) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int max_spatial_layers =
      capabilities.prediction_constraints().max_spatial_layers();
  if (max_spatial_layers < 2) {
    GTEST_SKIP() << "Encoder doesn't support multiple spatial layers.";
  }

  int num_layers = std::min(3, max_spatial_layers);
  std::vector<Rational> factors =
      FindSpatialLayerScalingFactors(capabilities, num_layers);
  if (factors.empty()) {
    GTEST_SKIP() << "Could not find valid scaling factors.";
  }

  int alignment = capabilities.input_constraints().pixel_alignment();
  std::vector<Resolution> resolutions =
      GetSpatialLayerResolutions(kDefaultResolution, factors, alignment);

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  TestDecoder dec_test(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec_test.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found.";
  }

  std::vector<EncOut> tu0_outs(num_layers);
  std::vector<FrameEncodeSettings> tu0_settings;
  tu0_settings.push_back(BuildSettings(
      std::move(Fb().Res(resolutions[0]).S(0).Upd(0).Key().Out(tu0_outs[0])),
      config.rate_options));
  for (int i = 1; i < num_layers; ++i) {
    tu0_settings.push_back(BuildSettings(
        std::move(
            Fb().Res(resolutions[i]).S(i).Ref({i - 1}).Upd(i).Out(tu0_outs[i])),
        config.rate_options));
  }
  enc->Encode(frame_reader->PullFrame(),
              TemporalUnitSettings(Timestamp::Millis(0)),
              std::move(tu0_settings));

  EncOut tu1_out;
  std::vector<FrameEncodeSettings> tu1_settings;
  tu1_settings.push_back(BuildSettings(std::move(Fb().Res(resolutions.back())
                                                     .S(num_layers - 1)
                                                     .Ref({num_layers - 1})
                                                     .Upd(num_layers - 1)
                                                     .Out(tu1_out)),
                                       config.rate_options));
  enc->Encode(frame_reader->PullFrame(),
              TemporalUnitSettings(Timestamp::Millis(50)),
              std::move(tu1_settings));

  std::vector<EncOut> tu2_outs(num_layers);
  std::vector<FrameEncodeSettings> tu2_settings;
  tu2_settings.push_back(BuildSettings(
      std::move(Fb().Res(resolutions[0]).S(0).Ref({0}).Upd(0).Out(tu2_outs[0])),
      config.rate_options));
  for (int i = 1; i < num_layers; ++i) {
    tu2_settings.push_back(BuildSettings(std::move(Fb().Res(resolutions[i])
                                                       .S(i)
                                                       .Ref({i - 1, i})
                                                       .Upd(i)
                                                       .Out(tu2_outs[i])),
                                         config.rate_options));
  }
  scoped_refptr<I420Buffer> tu2_frame = frame_reader->PullFrame();
  enc->Encode(tu2_frame, TemporalUnitSettings(Timestamp::Millis(100)),
              std::move(tu2_settings));

  for (int i = 0; i < num_layers; ++i) {
    VideoFrame f = dec_test.Decode(tu0_outs[i].bitstream);
    EXPECT_EQ(GetResolution(f), resolutions[i]);
  }

  VideoFrame f_tu1 = dec_test.Decode(tu1_out.bitstream);
  EXPECT_EQ(GetResolution(f_tu1), resolutions.back());

  for (int i = 0; i < num_layers - 1; ++i) {
    VideoFrame f = dec_test.Decode(tu2_outs[i].bitstream);
    EXPECT_EQ(GetResolution(f), resolutions[i]);
  }

  VideoFrame f_tu2_top = dec_test.Decode(tu2_outs.back().bitstream);
  EXPECT_EQ(GetResolution(f_tu2_top), resolutions.back());
  EXPECT_THAT(Psnr(tu2_frame, f_tu2_top), Gt(39.0));
}

// Verifies that spatial layers can be selectively skipped in a temporal unit.
TEST_P(VideoEncoderFunctionalTest, SkipMidLayer) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int max_spatial_layers =
      capabilities.prediction_constraints().max_spatial_layers();
  if (max_spatial_layers < 3) {
    GTEST_SKIP() << "Encoder doesn't support at least 3 spatial layers.";
  }

  std::vector<Rational> factors =
      FindSpatialLayerScalingFactors(capabilities, 3);
  if (factors.empty()) {
    GTEST_SKIP() << "Could not find 3 valid scaling factors.";
  }

  int alignment = capabilities.input_constraints().pixel_alignment();
  std::vector<Resolution> resolutions =
      GetSpatialLayerResolutions(kDefaultResolution, factors, alignment);

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found.";
  }

  EncOut tu0_s0, tu0_s1, tu0_s2;
  enc->Encode(
      frame_reader->PullFrame(), TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(
                 std::move(
                     Fb().Res(resolutions[0]).S(0).Upd(0).Key().Out(tu0_s0)),
                 config.rate_options),
             BuildSettings(
                 std::move(
                     Fb().Res(resolutions[1]).S(1).Ref({0}).Upd(1).Out(tu0_s1)),
                 config.rate_options),
             BuildSettings(
                 std::move(
                     Fb().Res(resolutions[2]).S(2).Ref({1}).Upd(2).Out(tu0_s2)),
                 config.rate_options)}));

  EncOut tu1_s0, tu1_s2;
  enc->Encode(
      frame_reader->PullFrame(), TemporalUnitSettings(Timestamp::Millis(100)),
      ToVec({BuildSettings(
                 std::move(
                     Fb().Res(resolutions[0]).S(0).Ref({0}).Upd(0).Out(tu1_s0)),
                 config.rate_options),
             BuildSettings(
                 std::move(
                     Fb().Res(resolutions[2]).S(2).Ref({2}).Upd(2).Out(tu1_s2)),
                 config.rate_options)}));

  EncOut tu2_s0, tu2_s1, tu2_s2;
  scoped_refptr<I420Buffer> tu2_frame = frame_reader->PullFrame();
  enc->Encode(
      tu2_frame, TemporalUnitSettings(Timestamp::Millis(200)),
      ToVec({BuildSettings(
                 std::move(
                     Fb().Res(resolutions[0]).S(0).Ref({0}).Upd(0).Out(tu2_s0)),
                 config.rate_options),
             BuildSettings(std::move(Fb().Res(resolutions[1])
                                         .S(1)
                                         .Ref({0, 1})
                                         .Upd(1)
                                         .Out(tu2_s1)),
                           config.rate_options),
             BuildSettings(std::move(Fb().Res(resolutions[2])
                                         .S(2)
                                         .Ref({1, 2})
                                         .Upd(2)
                                         .Out(tu2_s2)),
                           config.rate_options)}));

  EXPECT_EQ(GetResolution(dec.Decode(tu0_s0.bitstream)), resolutions[0]);
  EXPECT_EQ(GetResolution(dec.Decode(tu0_s1.bitstream)), resolutions[1]);
  EXPECT_EQ(GetResolution(dec.Decode(tu0_s2.bitstream)), resolutions[2]);
  EXPECT_EQ(GetResolution(dec.Decode(tu1_s0.bitstream)), resolutions[0]);
  EXPECT_EQ(GetResolution(dec.Decode(tu1_s2.bitstream)), resolutions[2]);
  EXPECT_EQ(GetResolution(dec.Decode(tu2_s0.bitstream)), resolutions[0]);
  EXPECT_EQ(GetResolution(dec.Decode(tu2_s1.bitstream)), resolutions[1]);

  VideoFrame f_tu2_s2 = dec.Decode(tu2_s2.bitstream);
  EXPECT_EQ(GetResolution(f_tu2_s2), resolutions[2]);
  EXPECT_THAT(Psnr(tu2_frame, f_tu2_s2), Gt(40.0));
}

// Verifies that omitting the top spatial layer in a temporal unit does not
// force a keyframe on the remaining or subsequent layers.
TEST_P(VideoEncoderFunctionalTest, SkipTopLayer) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int max_spatial_layers =
      capabilities.prediction_constraints().max_spatial_layers();
  if (max_spatial_layers < 2) {
    GTEST_SKIP() << "Encoder doesn't support at least 2 spatial layers.";
  }

  std::vector<Rational> factors =
      FindSpatialLayerScalingFactors(capabilities, 2);
  if (factors.empty()) {
    GTEST_SKIP() << "Could not find 2 valid scaling factors.";
  }

  int alignment = capabilities.input_constraints().pixel_alignment();
  std::vector<Resolution> resolutions =
      GetSpatialLayerResolutions(kDefaultResolution, factors, alignment);

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found.";
  }

  EncOut tu0_s0, tu0_s1;
  enc->Encode(
      frame_reader->PullFrame(), TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(
                 std::move(
                     Fb().Res(resolutions[0]).S(0).Upd(0).Key().Out(tu0_s0)),
                 config.rate_options),
             BuildSettings(
                 std::move(
                     Fb().Res(resolutions[1]).S(1).Ref({0}).Upd(1).Out(tu0_s1)),
                 config.rate_options)}));

  EncOut tu1_s0;
  enc->Encode(
      frame_reader->PullFrame(), TemporalUnitSettings(Timestamp::Millis(100)),
      ToVec({BuildSettings(
          std::move(Fb().Res(resolutions[0]).S(0).Ref({0}).Upd(0).Out(tu1_s0)),
          config.rate_options)}));
  ASSERT_THAT(tu1_s0, HasBitstreamAndMetaData());
  EXPECT_EQ(std::get<EncodedData>(tu1_s0.res).frame_type,
            FrameType::kDeltaFrame);

  EncOut tu2_s0, tu2_s1;
  scoped_refptr<I420Buffer> tu2_frame = frame_reader->PullFrame();
  enc->Encode(
      tu2_frame, TemporalUnitSettings(Timestamp::Millis(200)),
      ToVec({BuildSettings(
                 std::move(
                     Fb().Res(resolutions[0]).S(0).Ref({0}).Upd(0).Out(tu2_s0)),
                 config.rate_options),
             BuildSettings(std::move(Fb().Res(resolutions[1])
                                         .S(1)
                                         .Ref({0, 1})
                                         .Upd(1)
                                         .Out(tu2_s1)),
                           config.rate_options)}));
  ASSERT_THAT(tu2_s0, HasBitstreamAndMetaData());
  ASSERT_THAT(tu2_s1, HasBitstreamAndMetaData());
  EXPECT_EQ(std::get<EncodedData>(tu2_s0.res).frame_type,
            FrameType::kDeltaFrame);
  EXPECT_EQ(std::get<EncodedData>(tu2_s1.res).frame_type,
            FrameType::kDeltaFrame);

  EXPECT_EQ(GetResolution(dec.Decode(tu0_s0.bitstream)), resolutions[0]);
  EXPECT_EQ(GetResolution(dec.Decode(tu0_s1.bitstream)), resolutions[1]);
  EXPECT_EQ(GetResolution(dec.Decode(tu1_s0.bitstream)), resolutions[0]);
  EXPECT_EQ(GetResolution(dec.Decode(tu2_s0.bitstream)), resolutions[0]);

  VideoFrame f_tu2_s1 = dec.Decode(tu2_s1.bitstream);
  EXPECT_EQ(GetResolution(f_tu2_s1), resolutions[1]);
  EXPECT_THAT(Psnr(tu2_frame, f_tu2_s1), Gt(40.0));
}

// Verifies encoding and decoding of a StartFrame (independent frame without
// clearing all references).
TEST_P(VideoEncoderFunctionalTest, EncodesAndDecodesStartFrame) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::set<FrameType>& supported_types =
      capabilities.prediction_constraints().supported_frame_types();
  if (!supported_types.contains(FrameType::kStartFrame)) {
    GTEST_SKIP() << "Encoder doesn't support StartFrame.";
  }

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  TestConfig config = CreateTestConfig(capabilities);

  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  scoped_refptr<I420Buffer> input_frame = frame_reader->PullFrame();

  EncOut out;
  enc->Encode(
      input_frame, TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(
          std::move(Fb().Res(kDefaultResolution).Upd(0).Start().Out(out)),
          config.rate_options)}));

  ASSERT_THAT(out, HasBitstreamAndMetaData());
  VideoFrame decoded_frame = dec.Decode(out.bitstream);
  EXPECT_EQ(GetResolution(decoded_frame), kDefaultResolution);
  EXPECT_THAT(GetResolution(decoded_frame),
              FitsWithin(capabilities.input_constraints()));

  if (capabilities.prediction_constraints().num_buffers() >= 2) {
    EncOut start_out;
    enc->Encode(
        frame_reader->PullFrame(), TemporalUnitSettings(Timestamp::Millis(100)),
        ToVec({BuildSettings(
            std::move(
                Fb().Res(kDefaultResolution).Upd(1).Start().Out(start_out)),
            config.rate_options)}));
    ASSERT_THAT(start_out, HasBitstreamAndMetaData());
    EXPECT_EQ(std::get<EncodedData>(start_out.res).frame_type,
              FrameType::kStartFrame);
  }
}

// Verifies that encoding with a StartFrame produces bitrate and quality
// comparable to a Keyframe.
TEST_P(VideoEncoderFunctionalTest, KeyframeAndStartFrameAreApproximatelyEqual) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::set<FrameType>& supported_types =
      capabilities.prediction_constraints().supported_frame_types();
  if (!supported_types.contains(FrameType::kStartFrame)) {
    GTEST_SKIP() << "Encoder doesn't support StartFrame.";
  }

  int max_spatial_layers =
      capabilities.prediction_constraints().max_spatial_layers();
  TestConfig config = CreateTestConfig(capabilities);

  for (int sid = 0; sid < max_spatial_layers; ++sid) {
    std::unique_ptr<VideoEncoderInterface> enc_key =
        factory_->CreateEncoder(config.static_settings, {});
    std::unique_ptr<VideoEncoderInterface> enc_start =
        factory_->CreateEncoder(config.static_settings, {});
    std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

    DataSize total_size_key = DataSize::Zero();
    DataSize total_size_start = DataSize::Zero();
    TimeDelta total_duration = TimeDelta::Zero();

    scoped_refptr<I420Buffer> frame_in = frame_reader->PullFrame();

    EncOut key;
    EncOut start;

    enc_key->Encode(
        frame_in, TemporalUnitSettings(Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(
                Fb().Res(kDefaultResolution).S(sid).Upd(0).Key().Out(key)),
            config.rate_options)}));

    enc_start->Encode(
        frame_in, TemporalUnitSettings(Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(
                Fb().Res(kDefaultResolution).S(sid).Upd(0).Start().Out(start)),
            config.rate_options)}));

    total_size_key += DataSize::Bytes(key.bitstream.size());
    total_size_start += DataSize::Bytes(start.bitstream.size());

    TimeDelta frame_duration = TimeDelta::Millis(100);
    if (std::holds_alternative<FrameEncodeSettings::Cbr>(config.rate_options)) {
      frame_duration =
          std::get<FrameEncodeSettings::Cbr>(config.rate_options).duration;
    }
    total_duration += frame_duration;

    EXPECT_NEAR(total_size_key.bytes(), total_size_start.bytes(),
                0.15 * total_size_key.bytes());

    for (int f = 1; f < 10; ++f) {
      frame_in = frame_reader->PullFrame();
      enc_key->Encode(
          frame_in, TemporalUnitSettings(Timestamp::Millis(f * 100)),
          ToVec({BuildSettings(
              std::move(
                  Fb().Res(kDefaultResolution).S(sid).Ref({0}).Upd(0).Out(key)),
              config.rate_options)}));

      enc_start->Encode(
          frame_in, TemporalUnitSettings(Timestamp::Millis(f * 100)),
          ToVec({BuildSettings(std::move(Fb().Res(kDefaultResolution)
                                             .S(sid)
                                             .Ref({0})
                                             .Upd(0)
                                             .Out(start)),
                               config.rate_options)}));

      total_size_key += DataSize::Bytes(key.bitstream.size());
      total_size_start += DataSize::Bytes(start.bitstream.size());
      total_duration += frame_duration;
    }

    double key_encode_kbps = (total_size_key / total_duration).kbps();
    double start_encode_kbps = (total_size_start / total_duration).kbps();

    EXPECT_NEAR(key_encode_kbps, start_encode_kbps, start_encode_kbps * 0.05);
  }
}

// Verifies that the encoder accepts all ContentHint options (Detailed, Text,
// Fluid).
TEST_P(VideoEncoderFunctionalTest, SupportsAllContentHints) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  for (VideoTrackInterface::ContentHint hint :
       {VideoTrackInterface::ContentHint::kDetailed,
        VideoTrackInterface::ContentHint::kText,
        VideoTrackInterface::ContentHint::kFluid}) {
    EncOut out;
    enc->Encode(
        frame_reader->PullFrame(),
        TemporalUnitSettings(hint, Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
            config.rate_options)}));
    ASSERT_THAT(out, HasBitstreamAndMetaData());
    VideoFrame f0 = dec.Decode(out.bitstream);
    EXPECT_EQ(GetResolution(f0), kDefaultResolution);
  }
}

// Verifies that the encoder correctly handles the minimum supported resolution.
TEST_P(VideoEncoderFunctionalTest, SupportsMinResolution) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  Resolution min_res = capabilities.input_constraints().min();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  scoped_refptr<I420Buffer> input_frame =
      I420Buffer::Create(min_res.width, min_res.height);
  input_frame->ScaleFrom(*frame_reader->PullFrame());

  EncOut out;
  enc->Encode(
      input_frame, TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(std::move(Fb().Res(min_res).Upd(0).Key().Out(out)),
                           config.rate_options)}));

  ASSERT_THAT(out, HasBitstreamAndMetaData());
  VideoFrame decoded = dec.Decode(out.bitstream);
  EXPECT_EQ(GetResolution(decoded), min_res);
}

// Verifies that the encoder correctly handles the maximum supported resolution.
TEST_P(VideoEncoderFunctionalTest, SupportsMaxResolution) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  Resolution max_res = capabilities.input_constraints().max();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  TestConfig config = CreateTestConfig(capabilities);
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(max_res)
          .EncodingFormat(config.static_settings.encoding_format())
          .RcMode(config.static_settings.rc_mode())
          .MaxNumberOfThreads(config.static_settings.max_number_of_threads())
          .Build();
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  scoped_refptr<I420Buffer> input_frame =
      I420Buffer::Create(max_res.width, max_res.height);
  input_frame->ScaleFrom(*frame_reader->PullFrame());

  EncOut out;
  enc->Encode(
      input_frame, TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(std::move(Fb().Res(max_res).Upd(0).Key().Out(out)),
                           config.rate_options)}));

  ASSERT_THAT(out, HasBitstreamAndMetaData());
  VideoFrame decoded = dec.Decode(out.bitstream);
  EXPECT_EQ(GetResolution(decoded), max_res);
}

// Verifies that the encoder accepts resolutions adhering to the required pixel
// alignment.
TEST_P(VideoEncoderFunctionalTest, SupportsPixelAlignment) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  int alignment = capabilities.input_constraints().pixel_alignment();
  Resolution min_res = capabilities.input_constraints().min();
  Resolution aligned_res = {.width = min_res.width + alignment,
                            .height = min_res.height + alignment};

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(config.static_settings, {});
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  scoped_refptr<I420Buffer> input_frame =
      I420Buffer::Create(aligned_res.width, aligned_res.height);
  input_frame->ScaleFrom(*frame_reader->PullFrame());

  EncOut out;
  enc->Encode(input_frame, TemporalUnitSettings(Timestamp::Millis(0)),
              ToVec({BuildSettings(
                  std::move(Fb().Res(aligned_res).Upd(0).Key().Out(out)),
                  config.rate_options)}));

  ASSERT_THAT(out, HasBitstreamAndMetaData());
  EXPECT_EQ(GetResolution(dec.Decode(out.bitstream)), aligned_res);
}

// Verifies that all advertised input pixel formats can be encoded and decoded.
TEST_P(VideoEncoderFunctionalTest, SupportsAllInputFormats) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::vector<VideoFrameBuffer::Type>& formats =
      capabilities.input_constraints().input_formats();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  for (VideoFrameBuffer::Type format : formats) {
    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(config.static_settings, {});
    scoped_refptr<I420Buffer> i420_source = frame_reader->PullFrame();

    scoped_refptr<VideoFrameBuffer> input_buffer;
    switch (format) {
      case VideoFrameBuffer::Type::kI420:
        input_buffer = i420_source;
        break;
      case VideoFrameBuffer::Type::kNV12:
        input_buffer = NV12Buffer::Copy(*i420_source);
        break;
      default:
        RTC_LOG(LS_WARNING)
            << "Skipping unsupported frame buffer type in test: "
            << static_cast<int>(format);
        continue;
    }

    EncOut out;
    enc->Encode(
        input_buffer, TemporalUnitSettings(Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
            config.rate_options)}));

    ASSERT_THAT(out, HasBitstreamAndMetaData())
        << "Failed to encode input format: " << static_cast<int>(format);
    VideoFrame decoded = dec.Decode(out.bitstream);
    EXPECT_EQ(GetResolution(decoded), kDefaultResolution);
    EXPECT_THAT(Psnr(i420_source, decoded), Gt(40.0));
  }
}

// Verifies encoding with all supported bit-depth and sub-sampling combinations.
TEST_P(VideoEncoderFunctionalTest, SupportsAllEncodingFormats) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const std::vector<EncodingFormat>& enc_formats =
      capabilities.encoding_formats();

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  TestConfig config = CreateTestConfig(capabilities);
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();

  for (const EncodingFormat& format : enc_formats) {
    std::optional<VideoFrameBuffer::Type> expected_buffer_type =
        ToVideoFrameBufferType(format);
    if (!expected_buffer_type.has_value()) {
      RTC_LOG(LS_WARNING) << "Skipping encoding format without corresponding "
                             "VideoFrameBuffer::Type: sub_sampling="
                          << static_cast<int>(format.sub_sampling)
                          << ", bit_depth=" << format.bit_depth;
      continue;
    }

    scoped_refptr<I420Buffer> raw_frame = frame_reader->PullFrame();
    scoped_refptr<VideoFrameBuffer> input_buffer =
        CreateAndPopulateFrameBuffer(*expected_buffer_type, *raw_frame);
    if (!input_buffer) {
      RTC_LOG(LS_WARNING)
          << "Failed to create frame buffer for VideoFrameBuffer::Type: "
          << static_cast<int>(*expected_buffer_type);
      continue;
    }

    StaticEncoderSettings static_settings =
        StaticEncoderSettingsBuilder()
            .MaxEncodeDimensions(config.static_settings.max_encode_dimensions())
            .EncodingFormat(format)
            .RcMode(config.static_settings.rc_mode())
            .MaxNumberOfThreads(config.static_settings.max_number_of_threads())
            .Build();

    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(static_settings, {});

    EncOut out;
    enc->Encode(
        input_buffer, TemporalUnitSettings(Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
            config.rate_options)}));

    ASSERT_THAT(out, HasBitstreamAndMetaData())
        << "Failed to encode format with sub_sampling "
        << static_cast<int>(format.sub_sampling) << " and bit_depth "
        << format.bit_depth;
    VideoFrame decoded = dec.Decode(out.bitstream);
    EXPECT_EQ(GetResolution(decoded), kDefaultResolution);
    EXPECT_EQ(decoded.video_frame_buffer()->type(), *expected_buffer_type);
  }
}

// Verifies constant QP (CQP) rate control mode at min and max QP bounds.
TEST_P(VideoEncoderFunctionalTest, SupportsConstantQp) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const BitrateControl& bc = capabilities.bitrate_control();
  const std::set<RateControlMode>& rc_modes = bc.rc_modes();
  if (!rc_modes.contains(RateControlMode::kCqp)) {
    GTEST_SKIP() << "CQP rate control mode is not supported.";
  }

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(kDefaultResolution)
          .EncodingFormat({.sub_sampling = EncodingFormat::SubSampling::k420,
                           .bit_depth = 8})
          .CqpRcMode()
          .MaxNumberOfThreads(1)
          .Build();

  // Encode one frame with min QP.
  {
    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(static_settings, {});
    scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
    EncOut out;
    enc->Encode(
        frame, TemporalUnitSettings(Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
            Cqp{.target_qp = bc.min_qp()})}));
    ASSERT_THAT(out, HasBitstreamAndMetaData());
    EXPECT_EQ(std::get<EncodedData>(out.res).encoded_qp, bc.min_qp());
    VideoFrame decoded = dec.Decode(out.bitstream);
    EXPECT_EQ(GetResolution(decoded), kDefaultResolution);
  }

  // Encode one frame with max QP.
  {
    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(static_settings, {});
    scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
    EncOut out;
    enc->Encode(
        frame, TemporalUnitSettings(Timestamp::Millis(0)),
        ToVec({BuildSettings(
            std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
            Cqp{.target_qp = bc.max_qp()})}));
    ASSERT_THAT(out, HasBitstreamAndMetaData());
    EXPECT_EQ(std::get<EncodedData>(out.res).encoded_qp, bc.max_qp());
    VideoFrame decoded = dec.Decode(out.bitstream);
    EXPECT_EQ(GetResolution(decoded), kDefaultResolution);
  }
}

// Verifies constant bitrate (CBR) rate control mode.
TEST_P(VideoEncoderFunctionalTest, SupportsConstantBitrate) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  const BitrateControl& bc = capabilities.bitrate_control();
  const std::set<RateControlMode>& rc_modes = bc.rc_modes();
  if (!rc_modes.contains(RateControlMode::kCbr)) {
    GTEST_SKIP() << "CBR rate control mode is not supported.";
  }

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(kDefaultResolution)
          .EncodingFormat({.sub_sampling = EncodingFormat::SubSampling::k420,
                           .bit_depth = 8})
          .CbrRcMode(TimeDelta::Millis(1000), TimeDelta::Millis(600),
                     /*max_intra_bitrate_factor=*/3.0)
          .MaxNumberOfThreads(1)
          .Build();

  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(static_settings, {});
  scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  EncOut out;
  Cbr cbr_settings{.duration = TimeDelta::Millis(100),
                   .target_bitrate = DataRate::KilobitsPerSec(1000)};
  enc->Encode(frame, TemporalUnitSettings(Timestamp::Millis(0)),
              ToVec({BuildSettings(
                  std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
                  cbr_settings)}));
  ASSERT_THAT(out, HasBitstreamAndMetaData());
  VideoFrame decoded = dec.Decode(out.bitstream);
  EXPECT_EQ(GetResolution(decoded), kDefaultResolution);
}

// Verifies that encode callbacks occur synchronously on the calling thread when
// advertised.
TEST_P(VideoEncoderFunctionalTest, EncodeCallbackHappensOnCallingThread) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  if (!capabilities.performance().encode_on_calling_thread()) {
    GTEST_SKIP() << "encode_on_calling_thread is false.";
  }

  TestConfig config = CreateTestConfig(capabilities);
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(config.static_settings.max_encode_dimensions())
          .EncodingFormat(config.static_settings.encoding_format())
          .RcMode(config.static_settings.rc_mode())
          .MaxNumberOfThreads(2)
          .Build();

  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(static_settings, {});

  struct ThreadTrackingFrameOutput : public VideoEncoderInterface::FrameOutput {
    ThreadTrackingFrameOutput(std::optional<PlatformThreadId>& buffer_tid,
                              std::optional<PlatformThreadId>& complete_tid,
                              std::vector<uint8_t>& bs)
        : buffer_tid_(buffer_tid), complete_tid_(complete_tid), bs_(bs) {}

    std::span<uint8_t> GetBitstreamOutputBuffer(DataSize size) override {
      buffer_tid_ = CurrentThreadId();
      bs_.resize(size.bytes());
      return bs_;
    }

    void EncodeComplete(
        const VideoEncoderInterface::EncodeResult& encode_result) override {
      complete_tid_ = CurrentThreadId();
    }

    std::optional<PlatformThreadId>& buffer_tid_;
    std::optional<PlatformThreadId>& complete_tid_;
    std::vector<uint8_t>& bs_;
  };

  std::optional<PlatformThreadId> buffer_callback_thread_id;
  std::optional<PlatformThreadId> complete_callback_thread_id;
  std::vector<uint8_t> bitstream;

  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();

  PlatformThreadId calling_thread_id = CurrentThreadId();
  enc->Encode(
      frame, TemporalUnitSettings(Timestamp::Millis(0)),
      ToVec({BuildSettings(
          std::move(
              Fb().Res(kDefaultResolution)
                  .Upd(0)
                  .Key()
                  .FrameOutput(std::make_unique<ThreadTrackingFrameOutput>(
                      buffer_callback_thread_id, complete_callback_thread_id,
                      bitstream))),
          config.rate_options)}));

  EXPECT_EQ(buffer_callback_thread_id, calling_thread_id);
  EXPECT_EQ(complete_callback_thread_id, calling_thread_id);
  EXPECT_FALSE(bitstream.empty());

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }
  VideoFrame decoded = dec.Decode(bitstream);
  EXPECT_EQ(GetResolution(decoded), kDefaultResolution);
}

// Verifies that increasing the effort level yields strictly higher or equal
// quality (PSNR).
TEST_P(VideoEncoderFunctionalTest, HigherEffortLevelYieldsHigherQualityFrames) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  std::pair<int, int> effort_range =
      capabilities.performance().min_max_effort_level();
  if (effort_range.first == effort_range.second) {
    GTEST_SKIP() << "Single effort level supported.";
  }

  constexpr int kNumFrames = 10;
  std::vector<scoped_refptr<I420Buffer>> input_frames;
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  for (int i = 0; i < kNumFrames; ++i) {
    input_frames.push_back(frame_reader->PullFrame());
  }

  TestConfig config;
  const std::set<RateControlMode>& rc_modes =
      capabilities.bitrate_control().rc_modes();
  if (rc_modes.contains(RateControlMode::kCbr)) {
    config.static_settings =
        StaticEncoderSettingsBuilder()
            .MaxEncodeDimensions({.width = 640, .height = 360})
            .EncodingFormat(capabilities.encoding_formats()[0])
            .CbrRcMode(TimeDelta::Millis(1000), TimeDelta::Millis(600),
                       /*max_intra_bitrate_factor=*/3.0)
            .MaxNumberOfThreads(1)
            .Build();
    config.rate_options = FrameEncodeSettings::Cbr{
        .duration = TimeDelta::Millis(100),
        .target_bitrate = DataRate::KilobitsPerSec(1000)};
  } else {
    config = CreateTestConfig(capabilities);
  }

  std::optional<double> psnr_last;
  for (int i = effort_range.first; i <= effort_range.second; ++i) {
    TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
    if (!dec.IsSupported()) {
      GTEST_SKIP() << "No matching decoder found for codec: "
                   << factory_->CodecName();
    }

    double psnr_sum = 0;
    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(config.static_settings, {});
    for (int tu = 0; tu < kNumFrames; ++tu) {
      EncOut out;
      if (tu == 0) {
        enc->Encode(input_frames[tu],
                    TemporalUnitSettings(Timestamp::Millis(0)),
                    ToVec({BuildSettings(std::move(Fb().Res(kDefaultResolution)
                                                       .Upd(0)
                                                       .Key()
                                                       .Effort(i)
                                                       .Out(out)),
                                         config.rate_options)}));
      } else {
        enc->Encode(input_frames[tu],
                    TemporalUnitSettings(Timestamp::Millis(100 * tu)),
                    ToVec({BuildSettings(std::move(Fb().Res(kDefaultResolution)
                                                       .Ref({0})
                                                       .Upd(0)
                                                       .Effort(i)
                                                       .Out(out)),
                                         config.rate_options)}));
      }
      ASSERT_THAT(out, HasBitstreamAndMetaData());
      VideoFrame decoded = dec.Decode(out.bitstream);
      psnr_sum += Psnr(input_frames[tu], decoded);
    }
    double avg_psnr = psnr_sum / kNumFrames;
    if (psnr_last.has_value()) {
      EXPECT_THAT(avg_psnr, Gt(*psnr_last));
    }
    psnr_last = avg_psnr;
  }
}

// Verifies that the encoder can be configured with a high thread count without
// failure.
TEST_P(VideoEncoderFunctionalTest, SupportsHighNumberOfThreads) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  TestConfig config = CreateTestConfig(capabilities);
  StaticEncoderSettings static_settings =
      StaticEncoderSettingsBuilder()
          .MaxEncodeDimensions(config.static_settings.max_encode_dimensions())
          .EncodingFormat(config.static_settings.encoding_format())
          .RcMode(config.static_settings.rc_mode())
          .MaxNumberOfThreads(64)
          .Build();

  std::unique_ptr<VideoEncoderInterface> enc =
      factory_->CreateEncoder(static_settings, {});
  ASSERT_NE(enc, nullptr);

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  scoped_refptr<I420Buffer> frame = frame_reader->PullFrame();
  EncOut out;
  enc->Encode(frame, TemporalUnitSettings(Timestamp::Millis(0)),
              ToVec({BuildSettings(
                  std::move(Fb().Res(kDefaultResolution).Upd(0).Key().Out(out)),
                  config.rate_options)}));
  ASSERT_THAT(out, HasBitstreamAndMetaData());
  VideoFrame decoded = dec.Decode(out.bitstream);
  EXPECT_EQ(GetResolution(decoded), kDefaultResolution);
}

// Verifies that per-spatial-layer effort levels are honored across multiple
// temporal units when independent spatial layers with identical resolution,
// bitrate, and framerate use different effort levels.
TEST_P(VideoEncoderFunctionalTest,
       PerSpatialLayerEffortLevelsAreAppliedAcrossTemporalUnits) {
  Capabilities capabilities = factory_->GetEncoderCapabilities();
  if (capabilities.prediction_constraints().max_spatial_layers() < 2) {
    GTEST_SKIP() << "Encoder doesn't support multiple spatial layers.";
  }
  BufferSpaceType buffer_space_type =
      capabilities.prediction_constraints().buffer_space_type();
  if (buffer_space_type == BufferSpaceType::kSingleKeyframe &&
      !capabilities.prediction_constraints().supported_frame_types().contains(
          FrameType::kStartFrame)) {
    GTEST_SKIP() << "kSingleKeyframe encoder must support start frames.";
  }
  std::pair<int, int> effort_range =
      capabilities.performance().min_max_effort_level();
  if (effort_range.first == effort_range.second) {
    GTEST_SKIP() << "Single effort level supported.";
  }

  TestDecoder dec(env_, decoder_factory_.get(), factory_->CodecName());
  if (!dec.IsSupported()) {
    GTEST_SKIP() << "No matching decoder found for codec: "
                 << factory_->CodecName();
  }

  constexpr int kNumFrames = 8;
  std::vector<scoped_refptr<I420Buffer>> input_frames;
  std::unique_ptr<test::FrameReader> frame_reader = CreateFrameReader();
  for (int i = 0; i < kNumFrames; ++i) {
    input_frames.push_back(frame_reader->PullFrame());
  }

  TestConfig config;
  const std::set<RateControlMode>& rc_modes =
      capabilities.bitrate_control().rc_modes();
  if (rc_modes.contains(RateControlMode::kCbr)) {
    config.static_settings =
        StaticEncoderSettingsBuilder()
            .MaxEncodeDimensions(kDefaultResolution)
            .EncodingFormat(capabilities.encoding_formats()[0])
            .CbrRcMode(TimeDelta::Millis(1000), TimeDelta::Millis(600),
                       /*max_intra_bitrate_factor=*/3.0)
            .MaxNumberOfThreads(1)
            .Build();
    config.rate_options = FrameEncodeSettings::Cbr{
        .duration = TimeDelta::Millis(100),
        .target_bitrate = DataRate::KilobitsPerSec(1000)};
  } else {
    config = CreateTestConfig(capabilities);
  }

  struct LayerPsnr {
    double s0 = 0;
    double s1 = 0;
  };
  const int s1_buf =
      (buffer_space_type == BufferSpaceType::kMultiInstance) ? 0 : 1;
  auto encode_independent_layers = [&](int s0_effort, int s1_effort) {
    std::unique_ptr<VideoEncoderInterface> enc =
        factory_->CreateEncoder(config.static_settings, {});
    TestDecoder s0_dec(env_, decoder_factory_.get(), factory_->CodecName());
    TestDecoder s1_dec(env_, decoder_factory_.get(), factory_->CodecName());
    LayerPsnr psnr_sum;
    for (int tu = 0; tu < kNumFrames; ++tu) {
      EncOut out_s0, out_s1;
      Timestamp pts = Timestamp::Millis(tu * 100);
      Fb fb_s0, fb_s1;
      fb_s0.Res(kDefaultResolution).S(0).Upd(0).Effort(s0_effort).Out(out_s0);
      fb_s1.Res(kDefaultResolution)
          .S(1)
          .Upd(s1_buf)
          .Effort(s1_effort)
          .Out(out_s1);
      if (tu == 0) {
        fb_s0.Key();
        if (buffer_space_type == BufferSpaceType::kSingleKeyframe) {
          fb_s1.Start();
        } else {
          fb_s1.Key();
        }
      } else {
        fb_s0.Ref({0});
        fb_s1.Ref({s1_buf});
      }
      enc->Encode(
          input_frames[tu], TemporalUnitSettings(pts),
          ToVec({BuildSettings(std::move(fb_s0), config.rate_options),
                 BuildSettings(std::move(fb_s1), config.rate_options)}));
      EXPECT_THAT(out_s0, HasBitstreamAndMetaData());
      EXPECT_THAT(out_s1, HasBitstreamAndMetaData());
      psnr_sum.s0 += Psnr(input_frames[tu], s0_dec.Decode(out_s0.bitstream));
      psnr_sum.s1 += Psnr(input_frames[tu], s1_dec.Decode(out_s1.bitstream));
    }
    return LayerPsnr{.s0 = psnr_sum.s0 / kNumFrames,
                     .s1 = psnr_sum.s1 / kNumFrames};
  };

  LayerPsnr low_high =
      encode_independent_layers(effort_range.first, effort_range.second);
  LayerPsnr high_low =
      encode_independent_layers(effort_range.second, effort_range.first);

  // Compare each spatial layer against itself across swapped {low, high} vs
  // {high, low} effort configurations so any S0-vs-S1 keyframe/start-frame rate
  // control asymmetry cancels out: if S0's effort leaked into S1 (or vice
  // versa), the comparison on that layer would be reversed.
  EXPECT_THAT(high_low.s0, Gt(low_high.s0));
  EXPECT_THAT(low_high.s1, Gt(high_low.s1));
}

std::unique_ptr<VideoEncoderFactoryInterface> CreateLibaomAv1EncoderFactory() {
  return std::make_unique<LibaomAv1EncoderFactory>();
}

INSTANTIATE_TEST_SUITE_P(LibaomAv1,
                         VideoEncoderFunctionalTest,
                         ::testing::Values(CreateLibaomAv1EncoderFactory));

}  // namespace
}  // namespace webrtc
