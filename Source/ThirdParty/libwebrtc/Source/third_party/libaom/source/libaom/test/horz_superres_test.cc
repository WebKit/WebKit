/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <memory>
#include <ostream>
#include <tuple>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "av1/encoder/encoder.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"

namespace {

using std::make_tuple;
using std::tuple;

/* TESTING PARAMETERS */

const int kBitrate = 40;

typedef struct {
  const char *filename;
  aom_img_fmt fmt;
  aom_bit_depth_t bit_depth;
  unsigned int profile;
  unsigned int limit;
  unsigned int screen_content;
  double psnr_threshold;   // used by modes other than AOM_SUPERRES_AUTO
  double psnr_threshold2;  // used by AOM_SUPERRES_AUTO
} TestVideoParam;

std::ostream &operator<<(std::ostream &os, const TestVideoParam &test_arg) {
  return os << "TestVideoParam { filename:" << test_arg.filename
            << " fmt:" << test_arg.fmt << " bit_depth:" << test_arg.bit_depth
            << " profile:" << test_arg.profile << " limit:" << test_arg.limit
            << " screen_content:" << test_arg.screen_content
            << " psnr_threshold:" << test_arg.psnr_threshold << " }";
}

const TestVideoParam kTestVideoVectors[] = {
  { "park_joy_90p_8_420.y4m", AOM_IMG_FMT_I420, AOM_BITS_8, 0, 5, 0, 25.3,
    45.0 },
#if CONFIG_AV1_HIGHBITDEPTH
  { "park_joy_90p_10_444.y4m", AOM_IMG_FMT_I44416, AOM_BITS_10, 1, 5, 0, 27.0,
    48.0 },
#endif
  { "screendata.y4m", AOM_IMG_FMT_I420, AOM_BITS_8, 0, 4, 1, 23.0, 56.0 },
  // Image coding (single frame).
  { "niklas_1280_720_30.y4m", AOM_IMG_FMT_I420, AOM_BITS_8, 0, 1, 0, 32.0,
    49.0 },
};

// Modes with extra params have their own tests.
const aom_superres_mode kSuperresModesWithoutParams[] = { AOM_SUPERRES_RANDOM,
                                                          AOM_SUPERRES_AUTO };

// Superres denominators and superres kf denominators to be tested
typedef tuple<int, int> SuperresDenominatorPair;
const SuperresDenominatorPair kSuperresDenominators[] = {
  make_tuple(16, 9),  make_tuple(13, 11), make_tuple(9, 9),
  make_tuple(13, 13), make_tuple(11, 16), make_tuple(8, 16),
  make_tuple(16, 8),  make_tuple(8, 8),   make_tuple(9, 14),
};

// Superres q thresholds and superres kf q thresholds to be tested
typedef tuple<int, int> SuperresQThresholdPair;
const SuperresQThresholdPair kSuperresQThresholds[] = {
  make_tuple(63, 63), make_tuple(63, 41), make_tuple(17, 63),
  make_tuple(41, 11), make_tuple(1, 37),  make_tuple(11, 11),
  make_tuple(1, 1),   make_tuple(17, 29), make_tuple(29, 11),
};

/* END (TESTING PARAMETERS) */

// Test parameter list:
//  <[needed for EncoderTest], test_video_param_, superres_mode_>
typedef tuple<const libaom_test::CodecFactory *, TestVideoParam,
              aom_superres_mode>
    HorzSuperresTestParam;

class HorzSuperresEndToEndTest
    : public ::testing::TestWithParam<HorzSuperresTestParam>,
      public ::libaom_test::EncoderTest {
 protected:
  HorzSuperresEndToEndTest()
      : EncoderTest(GET_PARAM(0)), test_video_param_(GET_PARAM(1)),
        superres_mode_(GET_PARAM(2)), psnr_(0.0), frame_count_(0) {}

  virtual ~HorzSuperresEndToEndTest() {}

  virtual void SetUp() {
    InitializeConfig(::libaom_test::kTwoPassGood);
    cfg_.g_lag_in_frames = 5;
    cfg_.rc_end_usage = AOM_Q;
    cfg_.rc_target_bitrate = kBitrate;
    cfg_.g_error_resilient = 0;
    cfg_.g_profile = test_video_param_.profile;
    cfg_.g_input_bit_depth = (unsigned int)test_video_param_.bit_depth;
    cfg_.g_bit_depth = test_video_param_.bit_depth;
    init_flags_ = AOM_CODEC_USE_PSNR;
    if (cfg_.g_bit_depth > 8) init_flags_ |= AOM_CODEC_USE_HIGHBITDEPTH;

    // Set superres parameters
    cfg_.rc_superres_mode = superres_mode_;
  }

  virtual void BeginPassHook(unsigned int) {
    psnr_ = 0.0;
    frame_count_ = 0;
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    psnr_ += pkt->data.psnr.psnr[0];
    frame_count_++;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, 4);

      // Set cpu-used = 8 for speed
      encoder->Control(AOME_SET_CPUUSED, 8);

      // Test screen coding tools
      if (test_video_param_.screen_content)
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
      else
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);

      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    }
  }

  double GetAveragePsnr() const {
    if (frame_count_) return psnr_ / frame_count_;
    return 0.0;
  }

  void DoTest() {
    std::unique_ptr<libaom_test::VideoSource> video;
    video.reset(new libaom_test::Y4mVideoSource(test_video_param_.filename, 0,
                                                test_video_param_.limit));
    ASSERT_NE(video, nullptr);

    ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
    const double psnr_thresh = (superres_mode_ == AOM_SUPERRES_AUTO)
                                   ? test_video_param_.psnr_threshold2
                                   : test_video_param_.psnr_threshold;
    const double psnr = GetAveragePsnr();
    EXPECT_GT(psnr, psnr_thresh);

    EXPECT_EQ(test_video_param_.limit, frame_count_);
  }

  TestVideoParam test_video_param_;
  aom_superres_mode superres_mode_;

 private:
  double psnr_;
  unsigned int frame_count_;
};

TEST_P(HorzSuperresEndToEndTest, HorzSuperresEndToEndPSNRTest) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(HorzSuperresEndToEndTest,
                           ::testing::ValuesIn(kTestVideoVectors),
                           ::testing::ValuesIn(kSuperresModesWithoutParams));

// Test parameter list:
//  <[needed for EncoderTest], test_video_param_, tuple(superres_denom_,
//  superres_kf_denom_)>
typedef tuple<const libaom_test::CodecFactory *, TestVideoParam,
              SuperresDenominatorPair>
    HorzSuperresFixedTestParam;

class HorzSuperresFixedEndToEndTest
    : public ::testing::TestWithParam<HorzSuperresFixedTestParam>,
      public ::libaom_test::EncoderTest {
 protected:
  HorzSuperresFixedEndToEndTest()
      : EncoderTest(GET_PARAM(0)), test_video_param_(GET_PARAM(1)),
        superres_mode_(AOM_SUPERRES_FIXED), psnr_(0.0), frame_count_(0) {
    SuperresDenominatorPair denoms = GET_PARAM(2);
    superres_denom_ = std::get<0>(denoms);
    superres_kf_denom_ = std::get<1>(denoms);
  }

  virtual ~HorzSuperresFixedEndToEndTest() {}

  virtual void SetUp() {
    InitializeConfig(::libaom_test::kTwoPassGood);
    cfg_.g_lag_in_frames = 5;
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.rc_target_bitrate = kBitrate;
    cfg_.g_error_resilient = 0;
    cfg_.g_profile = test_video_param_.profile;
    cfg_.g_input_bit_depth = (unsigned int)test_video_param_.bit_depth;
    cfg_.g_bit_depth = test_video_param_.bit_depth;
    init_flags_ = AOM_CODEC_USE_PSNR;
    if (cfg_.g_bit_depth > 8) init_flags_ |= AOM_CODEC_USE_HIGHBITDEPTH;

    // Set superres parameters
    cfg_.rc_superres_mode = superres_mode_;
    cfg_.rc_superres_denominator = superres_denom_;
    cfg_.rc_superres_kf_denominator = superres_kf_denom_;
  }

  virtual void BeginPassHook(unsigned int) {
    psnr_ = 0.0;
    frame_count_ = 0;
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    psnr_ += pkt->data.psnr.psnr[0];
    frame_count_++;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, 4);

      // Set cpu-used = 8 for speed
      encoder->Control(AOME_SET_CPUUSED, 8);

      // Test screen coding tools
      if (test_video_param_.screen_content)
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
      else
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);

      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    }
  }

  double GetAveragePsnr() const {
    if (frame_count_) return psnr_ / frame_count_;
    return 0.0;
  }

  void DoTest() {
    std::unique_ptr<libaom_test::VideoSource> video;
    video.reset(new libaom_test::Y4mVideoSource(test_video_param_.filename, 0,
                                                test_video_param_.limit));
    ASSERT_NE(video, nullptr);

    ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
    const double psnr = GetAveragePsnr();
    EXPECT_GT(psnr, test_video_param_.psnr_threshold)
        << "superres_mode_ = " << superres_mode_
        << ", superres_denom_ = " << superres_denom_
        << ", superres_kf_denom_ = " << superres_kf_denom_;

    EXPECT_EQ(test_video_param_.limit, frame_count_)
        << "superres_mode_ = " << superres_mode_
        << ", superres_denom_ = " << superres_denom_
        << ", superres_kf_denom_ = " << superres_kf_denom_;
  }

  TestVideoParam test_video_param_;
  aom_superres_mode superres_mode_;
  int superres_denom_;
  int superres_kf_denom_;

 private:
  double psnr_;
  unsigned int frame_count_;
};

TEST_P(HorzSuperresFixedEndToEndTest, HorzSuperresFixedTestParam) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(HorzSuperresFixedEndToEndTest,
                           ::testing::ValuesIn(kTestVideoVectors),
                           ::testing::ValuesIn(kSuperresDenominators));

// Test parameter list:
//  <[needed for EncoderTest], test_video_param_,
//  tuple(superres_qthresh_,superres_kf_qthresh_)>
typedef tuple<const libaom_test::CodecFactory *, TestVideoParam,
              SuperresQThresholdPair>
    HorzSuperresQThreshTestParam;

class HorzSuperresQThreshEndToEndTest
    : public ::testing::TestWithParam<HorzSuperresQThreshTestParam>,
      public ::libaom_test::EncoderTest {
 protected:
  HorzSuperresQThreshEndToEndTest()
      : EncoderTest(GET_PARAM(0)), test_video_param_(GET_PARAM(1)),
        superres_mode_(AOM_SUPERRES_QTHRESH), psnr_(0.0), frame_count_(0) {
    SuperresQThresholdPair qthresholds = GET_PARAM(2);
    superres_qthresh_ = std::get<0>(qthresholds);
    superres_kf_qthresh_ = std::get<1>(qthresholds);
  }

  virtual ~HorzSuperresQThreshEndToEndTest() {}

  virtual void SetUp() {
    InitializeConfig(::libaom_test::kTwoPassGood);
    cfg_.g_lag_in_frames = 5;
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.rc_target_bitrate = kBitrate;
    cfg_.g_error_resilient = 0;
    cfg_.g_profile = test_video_param_.profile;
    cfg_.g_input_bit_depth = (unsigned int)test_video_param_.bit_depth;
    cfg_.g_bit_depth = test_video_param_.bit_depth;
    init_flags_ = AOM_CODEC_USE_PSNR;
    if (cfg_.g_bit_depth > 8) init_flags_ |= AOM_CODEC_USE_HIGHBITDEPTH;

    // Set superres parameters
    cfg_.rc_superres_mode = superres_mode_;
    cfg_.rc_superres_qthresh = superres_qthresh_;
    cfg_.rc_superres_kf_qthresh = superres_kf_qthresh_;
  }

  virtual void BeginPassHook(unsigned int) {
    psnr_ = 0.0;
    frame_count_ = 0;
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    psnr_ += pkt->data.psnr.psnr[0];
    frame_count_++;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, 0);

      // Set cpu-used = 8 for speed
      encoder->Control(AOME_SET_CPUUSED, 8);

      // Test screen coding tools
      if (test_video_param_.screen_content)
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_SCREEN);
      else
        encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);

      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    }
  }

  double GetAveragePsnr() const {
    if (frame_count_) return psnr_ / frame_count_;
    return 0.0;
  }

  void DoTest() {
    std::unique_ptr<libaom_test::VideoSource> video;
    video.reset(new libaom_test::Y4mVideoSource(test_video_param_.filename, 0,
                                                test_video_param_.limit));
    ASSERT_NE(video, nullptr);

    ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
    const double psnr = GetAveragePsnr();
    EXPECT_GT(psnr, test_video_param_.psnr_threshold)
        << "superres_mode_ = " << superres_mode_
        << ", superres_qthresh_ = " << superres_qthresh_
        << ", superres_kf_qthresh_ = " << superres_kf_qthresh_;

    EXPECT_EQ(test_video_param_.limit, frame_count_)
        << "superres_mode_ = " << superres_mode_
        << ", superres_qthresh_ = " << superres_qthresh_
        << ", superres_kf_qthresh_ = " << superres_kf_qthresh_;
  }

  TestVideoParam test_video_param_;
  aom_superres_mode superres_mode_;
  int superres_qthresh_;
  int superres_kf_qthresh_;

 private:
  double psnr_;
  unsigned int frame_count_;
};

TEST_P(HorzSuperresQThreshEndToEndTest, HorzSuperresQThreshEndToEndPSNRTest) {
  DoTest();
}

AV1_INSTANTIATE_TEST_SUITE(HorzSuperresQThreshEndToEndTest,
                           ::testing::ValuesIn(kTestVideoVectors),
                           ::testing::ValuesIn(kSuperresQThresholds));

}  // namespace
