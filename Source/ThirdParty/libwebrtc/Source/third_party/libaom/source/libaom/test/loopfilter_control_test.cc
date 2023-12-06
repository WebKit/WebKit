/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <memory>
#include <string>
#include <unordered_map>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"

namespace {

const unsigned int kFrames = 10;
const int kBitrate = 500;

// List of psnr thresholds for LF settings 0-3
// keys: video, LF control, aq mode.
std::unordered_map<std::string,
                   std::unordered_map<int, std::unordered_map<int, double>>>
    kPsnrThreshold = { { "park_joy_90p_8_420.y4m",
                         { { 0, { { 0, 35.0 }, { 3, 35.8 } } },
                           { 1, { { 0, 35.1 }, { 3, 35.9 } } },
                           { 2, { { 0, 35.1 }, { 3, 36.1 } } },
                           { 3, { { 0, 35.1 }, { 3, 36.1 } } } } },
                       { "paris_352_288_30.y4m",
                         { { 0, { { 0, 35.40 }, { 3, 36.0 } } },
                           { 1, { { 0, 35.50 }, { 3, 36.0 } } },
                           { 2, { { 0, 35.50 }, { 3, 36.0 } } },
                           { 3, { { 0, 35.50 }, { 3, 36.0 } } } } },
                       { "niklas_1280_720_30.y4m",
                         { { 0, { { 0, 33.20 }, { 3, 32.90 } } },
                           { 1, { { 0, 33.57 }, { 3, 33.22 } } },
                           { 2, { { 0, 33.57 }, { 3, 33.22 } } },
                           { 3, { { 0, 33.45 }, { 3, 33.10 } } } } } };

typedef struct {
  const char *filename;
  unsigned int input_bit_depth;
  aom_img_fmt fmt;
  aom_bit_depth_t bit_depth;
  unsigned int profile;
} TestVideoParam;

std::ostream &operator<<(std::ostream &os, const TestVideoParam &test_arg) {
  return os << "TestVideoParam { filename:" << test_arg.filename
            << " input_bit_depth:" << test_arg.input_bit_depth
            << " fmt:" << test_arg.fmt << " bit_depth:" << test_arg.bit_depth
            << " profile:" << test_arg.profile << " }";
}

const TestVideoParam kTestVectors[] = {
  { "park_joy_90p_8_420.y4m", 8, AOM_IMG_FMT_I420, AOM_BITS_8, 0 },
  { "paris_352_288_30.y4m", 8, AOM_IMG_FMT_I420, AOM_BITS_8, 0 },
  { "niklas_1280_720_30.y4m", 8, AOM_IMG_FMT_I420, AOM_BITS_8, 0 },
};

// Params: test video, lf_control, aq mode, threads, tile columns.
class LFControlEndToEndTest
    : public ::libaom_test::CodecTestWith5Params<TestVideoParam, int,
                                                 unsigned int, int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  LFControlEndToEndTest()
      : EncoderTest(GET_PARAM(0)), test_video_param_(GET_PARAM(1)),
        lf_control_(GET_PARAM(2)), psnr_(0.0), nframes_(0),
        aq_mode_(GET_PARAM(3)), threads_(GET_PARAM(4)),
        tile_columns_(GET_PARAM(5)) {}

  ~LFControlEndToEndTest() override = default;

  void SetUp() override {
    InitializeConfig(::libaom_test::kRealTime);

    cfg_.g_threads = threads_;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.kf_max_dist = 9999;
    cfg_.kf_min_dist = 9999;
  }

  void BeginPassHook(unsigned int) override {
    psnr_ = 0.0;
    nframes_ = 0;
  }

  void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) override {
    psnr_ += pkt->data.psnr.psnr[0];
    nframes_++;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_ENABLE_RESTORATION, 0);
      encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
      encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_WARPED_MOTION, 0);
      encoder->Control(AV1E_SET_DELTAQ_MODE, 0);
      encoder->Control(AV1E_SET_ENABLE_TPL_MODEL, 0);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, tile_columns_);
      encoder->Control(AOME_SET_CPUUSED, 10);
      encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AV1E_SET_ROW_MT, 1);
      encoder->Control(AV1E_SET_ENABLE_CDEF, 1);
      encoder->Control(AV1E_SET_COEFF_COST_UPD_FREQ, 2);
      encoder->Control(AV1E_SET_MODE_COST_UPD_FREQ, 2);
      encoder->Control(AV1E_SET_MV_COST_UPD_FREQ, 2);
      encoder->Control(AV1E_SET_DV_COST_UPD_FREQ, 2);
      encoder->Control(AV1E_SET_LOOPFILTER_CONTROL, lf_control_);
    }
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  double GetPsnrThreshold() {
    return kPsnrThreshold[test_video_param_.filename][lf_control_][aq_mode_];
  }

  void DoTest() {
    cfg_.rc_target_bitrate = kBitrate;
    cfg_.g_error_resilient = 0;
    cfg_.g_profile = test_video_param_.profile;
    cfg_.g_input_bit_depth = test_video_param_.input_bit_depth;
    cfg_.g_bit_depth = test_video_param_.bit_depth;
    init_flags_ = AOM_CODEC_USE_PSNR;
    if (cfg_.g_bit_depth > 8) init_flags_ |= AOM_CODEC_USE_HIGHBITDEPTH;

    std::unique_ptr<libaom_test::VideoSource> video;
    video.reset(new libaom_test::Y4mVideoSource(test_video_param_.filename, 0,
                                                kFrames));
    ASSERT_NE(video, nullptr);

    ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
    const double psnr = GetAveragePsnr();
    EXPECT_GT(psnr, GetPsnrThreshold())
        << "loopfilter control = " << lf_control_ << " aq mode = " << aq_mode_;
  }

  TestVideoParam test_video_param_;
  int lf_control_;

 private:
  double psnr_;
  unsigned int nframes_;
  unsigned int aq_mode_;
  int threads_;
  int tile_columns_;
};

class LFControlEndToEndTestThreaded : public LFControlEndToEndTest {};

TEST_P(LFControlEndToEndTest, EndtoEndPSNRTest) { DoTest(); }

TEST_P(LFControlEndToEndTestThreaded, EndtoEndPSNRTest) { DoTest(); }

TEST(LFControlGetterTest, NullptrInput) {
  int *lf_level = nullptr;
  aom_codec_ctx_t encoder;
  aom_codec_enc_cfg_t cfg;
  aom_codec_enc_config_default(aom_codec_av1_cx(), &cfg, 1);
  EXPECT_EQ(aom_codec_enc_init(&encoder, aom_codec_av1_cx(), &cfg, 0),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&encoder, AOME_GET_LOOPFILTER_LEVEL, lf_level),
            AOM_CODEC_INVALID_PARAM);
  EXPECT_EQ(aom_codec_destroy(&encoder), AOM_CODEC_OK);
}

AV1_INSTANTIATE_TEST_SUITE(LFControlEndToEndTest,
                           ::testing::ValuesIn(kTestVectors),
                           ::testing::Range(0, 4),
                           ::testing::Values<unsigned int>(0, 3),
                           ::testing::Values(1), ::testing::Values(1));

AV1_INSTANTIATE_TEST_SUITE(LFControlEndToEndTestThreaded,
                           ::testing::ValuesIn(kTestVectors),
                           ::testing::Range(0, 4),
                           ::testing::Values<unsigned int>(0, 3),
                           ::testing::Range(2, 5), ::testing::Range(2, 5));
}  // namespace
