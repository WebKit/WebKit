/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "aom/aom_codec.h"
#include "aom/aomcx.h"
#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/y4m_video_source.h"
#include "test/util.h"

namespace {
// This class is used to validate if screen_content_tools are turned on
// appropriately.
class ScreenContentToolsTestLarge
    : public ::libaom_test::CodecTestWith3Params<
          libaom_test::TestMode, aom_rc_mode, aom_screen_detection_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  ScreenContentToolsTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        rc_end_usage_(GET_PARAM(2)),
        screen_content_tools_detection_mode_(GET_PARAM(3)) {
    is_screen_content_violated_ = true;
    tune_content_ = AOM_CONTENT_DEFAULT;
  }
  ~ScreenContentToolsTestLarge() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = rc_end_usage_;
    cfg_.g_threads = 1;
    cfg_.g_lag_in_frames = 35;
    cfg_.rc_target_bitrate = 1000;
    cfg_.g_profile = 0;
  }

  bool DoDecode() const override { return true; }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AV1E_SET_TUNE_CONTENT, tune_content_);
      encoder->Control(AV1E_SET_SCREEN_CONTENT_DETECTION_MODE,
                       screen_content_tools_detection_mode_);
    }
  }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      aom_screen_content_tools_info sc_info;

      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_SCREEN_CONTENT_TOOLS_INFO,
                                    &sc_info);
      if (sc_info.allow_screen_content_tools == 1) {
        is_screen_content_violated_ = false;
      }
    }
    return AOM_CODEC_OK == res_dec;
  }

  ::libaom_test::TestMode encoding_mode_;
  bool is_screen_content_violated_;
  int tune_content_;
  aom_rc_mode rc_end_usage_;
  aom_screen_detection_mode screen_content_tools_detection_mode_;
};

TEST_P(ScreenContentToolsTestLarge, ScreenContentToolsTest) {
  // force screen content tools on
  ::libaom_test::Y4mVideoSource video_nonsc("park_joy_90p_8_444.y4m", 0, 1);
  cfg_.g_profile = 1;
  tune_content_ = AOM_CONTENT_SCREEN;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video_nonsc));
  ASSERT_EQ(is_screen_content_violated_, false)
      << "Failed for tune_content_ = AOM_CONTENT_SCREEN";

  // Don't force screen content, however as the input is screen content
  // allow_screen_content_tools should still be turned on
  ::libaom_test::Y4mVideoSource video_sc("desktop_credits.y4m", 0, 1);
  cfg_.g_profile = 1;
  is_screen_content_violated_ = true;
  tune_content_ = AOM_CONTENT_DEFAULT;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video_sc));
  ASSERT_EQ(is_screen_content_violated_, false)
      << "Failed detection of screen content";

  // The test below is only enabled for mode 2 because the input consists of
  // anti-aliased text, which mode 1 can't determine as screen content
  if (screen_content_tools_detection_mode_ ==
      AOM_SCREEN_DETECTION_ANTIALIASING_AWARE) {
    // low resolution test
    ::libaom_test::Y4mVideoSource video_sc_lowres("screendata.y4m", 0, 1);
    cfg_.g_profile = 0;
    is_screen_content_violated_ = true;
    tune_content_ = AOM_CONTENT_DEFAULT;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video_sc_lowres));
    ASSERT_EQ(is_screen_content_violated_, false)
        << "Failed detection of screen content(lowres)";
  }
}

AV1_INSTANTIATE_TEST_SUITE(
    ScreenContentToolsTestLarge,
    ::testing::Values(::libaom_test::kOnePassGood, ::libaom_test::kTwoPassGood),
    ::testing::Values(AOM_Q),
    ::testing::Values(AOM_SCREEN_DETECTION_STANDARD,
                      AOM_SCREEN_DETECTION_ANTIALIASING_AWARE));

class ScreenContentToolsMultiThreadTestLarge
    : public ScreenContentToolsTestLarge {};

TEST_P(ScreenContentToolsMultiThreadTestLarge, ScreenContentToolsTest) {
  // Don't force screen content, however as the input is screen content
  // allow_screen_content_tools should still be turned on even with
  // multi-threaded encoding.
  ::libaom_test::Y4mVideoSource video_sc("desktop_credits.y4m", 0, 10);
  cfg_.g_profile = 1;
  cfg_.g_threads = 4;
  is_screen_content_violated_ = true;
  tune_content_ = AOM_CONTENT_DEFAULT;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video_sc));
  ASSERT_EQ(is_screen_content_violated_, false)
      << "Failed detection of screen content";
}

AV1_INSTANTIATE_TEST_SUITE(
    ScreenContentToolsMultiThreadTestLarge,
    ::testing::Values(::libaom_test::kOnePassGood, ::libaom_test::kTwoPassGood),
    ::testing::Values(AOM_Q),
    ::testing::Values(AOM_SCREEN_DETECTION_STANDARD,
                      AOM_SCREEN_DETECTION_ANTIALIASING_AWARE));
}  // namespace
