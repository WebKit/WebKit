/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <array>
#include <memory>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/video_source.h"
#include "test/util.h"

namespace {

class AV1FrameSizeTests : public ::testing::Test,
                          public ::libaom_test::EncoderTest {
 protected:
  AV1FrameSizeTests()
      : EncoderTest(&::libaom_test::kAV1), expected_res_(AOM_CODEC_OK) {}
  ~AV1FrameSizeTests() override = default;

  void SetUp() override { InitializeConfig(::libaom_test::kRealTime); }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(expected_res_, res_dec) << decoder->DecodeError();
    return !::testing::Test::HasFailure();
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 7);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    }
  }

  int expected_res_;
};

#if CONFIG_SIZE_LIMIT
// TODO(Casey.Smalley@arm.com) fails due to newer bounds checks that get caught
// before the assert below added in ebc2714d71a834fc32a19eef0a81f51fbc47db01
TEST_F(AV1FrameSizeTests, DISABLED_TestInvalidSizes) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(DECODE_WIDTH_LIMIT + 16, DECODE_HEIGHT_LIMIT + 16);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_CORRUPT_FRAME;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

// TODO(Casey.Smalley@arm.com) similar to the above test, needs to be
// updated for the new rejection case
TEST_F(AV1FrameSizeTests, DISABLED_LargeValidSizes) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(DECODE_WIDTH_LIMIT, DECODE_HEIGHT_LIMIT);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#endif

TEST_F(AV1FrameSizeTests, OneByOneVideo) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(1, 1);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

// Parameters: AOM_USAGE_*, aom_rc_mode, cpu-used.
class AV1ResolutionChange
    : public testing::TestWithParam<std::tuple<int, aom_rc_mode, int>> {
 public:
  AV1ResolutionChange()
      : usage_(std::get<0>(GetParam())), rc_mode_(std::get<1>(GetParam())),
        cpu_used_(std::get<2>(GetParam())) {}
  AV1ResolutionChange(const AV1ResolutionChange &) = delete;
  AV1ResolutionChange &operator=(const AV1ResolutionChange &) = delete;
  ~AV1ResolutionChange() override = default;

 protected:
  int usage_;
  aom_rc_mode rc_mode_;
  int cpu_used_;
};

TEST_P(AV1ResolutionChange, InvalidRefSize) {
  struct FrameSize {
    unsigned int width;
    unsigned int height;
  };
  static constexpr std::array<FrameSize, 3> kFrameSizes = { {
      { 1768, 200 },
      { 50, 200 },
      { 850, 200 },
  } };

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage_), AOM_CODEC_OK);

  // Resolution changes are only permitted with one pass encoding with no lag.
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = rc_mode_;

  aom_codec_ctx_t ctx;
  EXPECT_EQ(aom_codec_enc_init(&ctx, iface, &cfg, 0), AOM_CODEC_OK);
  std::unique_ptr<aom_codec_ctx_t, decltype(&aom_codec_destroy)> enc(
      &ctx, &aom_codec_destroy);
  EXPECT_EQ(aom_codec_control(enc.get(), AOME_SET_CPUUSED, cpu_used_),
            AOM_CODEC_OK);

  size_t frame_count = 0;
  ::libaom_test::RandomVideoSource video;
  video.Begin();
  constexpr int kNumFramesPerResolution = 2;
  for (const auto &frame_size : kFrameSizes) {
    cfg.g_w = frame_size.width;
    cfg.g_h = frame_size.height;
    EXPECT_EQ(aom_codec_enc_config_set(enc.get(), &cfg), AOM_CODEC_OK);
    video.SetSize(cfg.g_w, cfg.g_h);

    aom_codec_iter_t iter;
    const aom_codec_cx_pkt_t *pkt;

    for (int i = 0; i < kNumFramesPerResolution; ++i) {
      video.Next();  // SetSize() does not call FillFrame().
      EXPECT_EQ(aom_codec_encode(enc.get(), video.img(), video.pts(),
                                 video.duration(), /*flags=*/0),
                AOM_CODEC_OK);

      iter = nullptr;
      while ((pkt = aom_codec_get_cx_data(enc.get(), &iter)) != nullptr) {
        ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
        // The frame following a resolution change should be a keyframe as the
        // change is too extreme to allow previous references to be used.
        if (i == 0 || usage_ == AOM_USAGE_ALL_INTRA) {
          EXPECT_NE(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u)
              << "frame " << frame_count;
        }
        frame_count++;
      }
    }
  }

  EXPECT_EQ(frame_count, kNumFramesPerResolution * kFrameSizes.size());
}

TEST_P(AV1ResolutionChange, RandomInput) {
  struct FrameSize {
    unsigned int width;
    unsigned int height;
  };
  static constexpr std::array<FrameSize, 4> kFrameSizes = { {
      { 50, 200 },
      { 100, 200 },
      { 100, 300 },
      { 200, 400 },
  } };

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage_), AOM_CODEC_OK);

  // Resolution changes are only permitted with one pass encoding with no lag.
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = rc_mode_;
  // For random input source, if max frame sizes are not set, the first encoded
  // frame size will be locked as the max frame size, and the encoder will
  // identify it as unsupported bitstream.
  unsigned int max_width = cfg.g_w;   // default frame width
  unsigned int max_height = cfg.g_h;  // default frame height
  for (const auto &frame_size : kFrameSizes) {
    max_width = frame_size.width > max_width ? frame_size.width : max_width;
    max_height =
        frame_size.height > max_height ? frame_size.height : max_height;
  }
  cfg.g_forced_max_frame_width = max_width;
  cfg.g_forced_max_frame_height = max_height;

  aom_codec_ctx_t ctx;
  EXPECT_EQ(aom_codec_enc_init(&ctx, iface, &cfg, 0), AOM_CODEC_OK);
  std::unique_ptr<aom_codec_ctx_t, decltype(&aom_codec_destroy)> enc(
      &ctx, &aom_codec_destroy);
  EXPECT_EQ(aom_codec_control(enc.get(), AOME_SET_CPUUSED, cpu_used_),
            AOM_CODEC_OK);

  size_t frame_count = 0;
  ::libaom_test::RandomVideoSource video;
  video.Begin();
  constexpr int kNumFramesPerResolution = 2;
  for (const auto &frame_size : kFrameSizes) {
    cfg.g_w = frame_size.width;
    cfg.g_h = frame_size.height;
    EXPECT_EQ(aom_codec_enc_config_set(enc.get(), &cfg), AOM_CODEC_OK);
    video.SetSize(cfg.g_w, cfg.g_h);

    aom_codec_iter_t iter;
    const aom_codec_cx_pkt_t *pkt;

    for (int i = 0; i < kNumFramesPerResolution; ++i) {
      video.Next();  // SetSize() does not call FillFrame().
      EXPECT_EQ(aom_codec_encode(enc.get(), video.img(), video.pts(),
                                 video.duration(), /*flags=*/0),
                AOM_CODEC_OK);

      iter = nullptr;
      while ((pkt = aom_codec_get_cx_data(enc.get(), &iter)) != nullptr) {
        ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
        // The frame following a resolution change should be a keyframe as the
        // change is too extreme to allow previous references to be used.
        if (i == 0 || usage_ == AOM_USAGE_ALL_INTRA) {
          EXPECT_NE(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u)
              << "frame " << frame_count;
        }
        frame_count++;
      }
    }
  }

  EXPECT_EQ(frame_count, kNumFramesPerResolution * kFrameSizes.size());
}

TEST_P(AV1ResolutionChange, InvalidInputSize) {
  struct FrameSize {
    unsigned int width;
    unsigned int height;
  };
  static constexpr std::array<FrameSize, 3> kFrameSizes = { {
      { 1768, 0 },
      { 0, 200 },
      { 850, 200 },
  } };

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage_), AOM_CODEC_OK);

  // Resolution changes are only permitted with one pass encoding with no lag.
  cfg.g_pass = AOM_RC_ONE_PASS;
  cfg.g_lag_in_frames = 0;
  cfg.rc_end_usage = rc_mode_;

  aom_codec_ctx_t ctx;
  EXPECT_EQ(aom_codec_enc_init(&ctx, iface, &cfg, 0), AOM_CODEC_OK);
  std::unique_ptr<aom_codec_ctx_t, decltype(&aom_codec_destroy)> enc(
      &ctx, &aom_codec_destroy);
  EXPECT_EQ(aom_codec_control(enc.get(), AOME_SET_CPUUSED, cpu_used_),
            AOM_CODEC_OK);

  int frame_count = 0;
  ::libaom_test::RandomVideoSource video;
  video.Begin();
  constexpr int kNumFramesPerResolution = 2;
  for (const auto &frame_size : kFrameSizes) {
    cfg.g_w = frame_size.width;
    cfg.g_h = frame_size.height;
    if (cfg.g_w < 1 || cfg.g_w > 65536 || cfg.g_h < 1 || cfg.g_h > 65536) {
      EXPECT_EQ(aom_codec_enc_config_set(enc.get(), &cfg),
                AOM_CODEC_INVALID_PARAM);
      continue;
    }

    EXPECT_EQ(aom_codec_enc_config_set(enc.get(), &cfg), AOM_CODEC_OK);
    video.SetSize(cfg.g_w, cfg.g_h);

    aom_codec_iter_t iter;
    const aom_codec_cx_pkt_t *pkt;

    for (int i = 0; i < kNumFramesPerResolution; ++i) {
      video.Next();  // SetSize() does not call FillFrame().
      EXPECT_EQ(aom_codec_encode(enc.get(), video.img(), video.pts(),
                                 video.duration(), /*flags=*/0),
                AOM_CODEC_OK);

      iter = nullptr;
      while ((pkt = aom_codec_get_cx_data(enc.get(), &iter)) != nullptr) {
        ASSERT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
        // The frame following a resolution change should be a keyframe as the
        // change is too extreme to allow previous references to be used.
        if (i == 0 || usage_ == AOM_USAGE_ALL_INTRA) {
          EXPECT_NE(pkt->data.frame.flags & AOM_FRAME_IS_KEY, 0u)
              << "frame " << frame_count;
        }
        frame_count++;
      }
    }
  }

  EXPECT_EQ(frame_count, 2);
}

INSTANTIATE_TEST_SUITE_P(
    Realtime, AV1ResolutionChange,
    ::testing::Combine(::testing::Values(AOM_USAGE_REALTIME),
                       ::testing::Values(AOM_VBR, AOM_CBR),
                       ::testing::Range(6, 11)));

#if !CONFIG_REALTIME_ONLY
INSTANTIATE_TEST_SUITE_P(
    GoodQuality, AV1ResolutionChange,
    ::testing::Combine(::testing::Values(AOM_USAGE_GOOD_QUALITY),
                       ::testing::Values(AOM_VBR, AOM_CBR, AOM_CQ, AOM_Q),
                       ::testing::Range(2, 6)));
INSTANTIATE_TEST_SUITE_P(
    GoodQualityLarge, AV1ResolutionChange,
    ::testing::Combine(::testing::Values(AOM_USAGE_GOOD_QUALITY),
                       ::testing::Values(AOM_VBR, AOM_CBR, AOM_CQ, AOM_Q),
                       ::testing::Range(0, 2)));
INSTANTIATE_TEST_SUITE_P(
    AllIntra, AV1ResolutionChange,
    ::testing::Combine(::testing::Values(AOM_USAGE_ALL_INTRA),
                       ::testing::Values(AOM_Q), ::testing::Range(6, 10)));

typedef struct {
  unsigned int width;
  unsigned int height;
} FrameSizeParam;

const FrameSizeParam FrameSizeTestParams[] = { { 96, 96 }, { 176, 144 } };

// This unit test is used to validate the allocated size of compressed data
// (ctx->cx_data) buffer, by feeding pseudo random input to the encoder in
// lossless encoding mode.
//
// If compressed data buffer is not large enough, the av1_get_compressed_data()
// call in av1/av1_cx_iface.c will overflow the buffer.
class AV1LosslessFrameSizeTests
    : public ::libaom_test::CodecTestWith2Params<FrameSizeParam,
                                                 ::libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  AV1LosslessFrameSizeTests()
      : EncoderTest(GET_PARAM(0)), frame_size_param_(GET_PARAM(1)),
        encoding_mode_(GET_PARAM(2)) {}
  ~AV1LosslessFrameSizeTests() override = default;

  void SetUp() override { InitializeConfig(encoding_mode_); }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(expected_res_, res_dec) << decoder->DecodeError();
    return !::testing::Test::HasFailure();
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 6);
      encoder->Control(AV1E_SET_LOSSLESS, 1);
    }
  }

  const FrameSizeParam frame_size_param_;
  const ::libaom_test::TestMode encoding_mode_;
  int expected_res_;
};

TEST_P(AV1LosslessFrameSizeTests, LosslessEncode) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(frame_size_param_.width, frame_size_param_.height);
  video.set_limit(10);
  expected_res_ = AOM_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_SUITE(AV1LosslessFrameSizeTests,
                           ::testing::ValuesIn(FrameSizeTestParams),
                           testing::Values(::libaom_test::kAllIntra));
#endif  // !CONFIG_REALTIME_ONLY

}  // namespace
