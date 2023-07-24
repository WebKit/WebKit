/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
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

// List of psnr thresholds for speed settings 6-8
// keys: video, speed, aq mode.
std::unordered_map<std::string,
                   std::unordered_map<int, std::unordered_map<int, double>>>
    kPsnrThreshold = { { "park_joy_90p_8_420.y4m",
                         { { 5, { { 0, 35.4 }, { 3, 36.3 } } },
                           { 6, { { 0, 35.3 }, { 3, 36.2 } } },
                           { 7, { { 0, 34.9 }, { 3, 35.8 } } },
                           { 8, { { 0, 35.0 }, { 3, 35.8 } } },
                           { 9, { { 0, 34.9 }, { 3, 35.5 } } },
                           { 10, { { 0, 34.7 }, { 3, 35.3 } } } } },
                       { "paris_352_288_30.y4m",
                         { { 5, { { 0, 36.2 }, { 3, 36.7 } } },
                           { 6, { { 0, 36.1 }, { 3, 36.48 } } },
                           { 7, { { 0, 35.5 }, { 3, 36.0 } } },
                           { 8, { { 0, 35.8 }, { 3, 36.48 } } },
                           { 9, { { 0, 35.5 }, { 3, 36.0 } } },
                           { 10, { { 0, 35.3 }, { 3, 35.9 } } } } },
                       { "niklas_1280_720_30.y4m",
                         { { 5, { { 0, 34.4 }, { 3, 34.30 } } },
                           { 6, { { 0, 34.2 }, { 3, 34.2 } } },
                           { 7, { { 0, 33.5 }, { 3, 33.48 } } },
                           { 8, { { 0, 33.48 }, { 3, 33.48 } } },
                           { 9, { { 0, 33.4 }, { 3, 33.4 } } },
                           { 10, { { 0, 33.2 }, { 3, 33.2 } } } } },
                       { "hantro_collage_w352h288_nv12.yuv",
                         { { 5, { { 0, 34.4 }, { 3, 34.30 } } },
                           { 6, { { 0, 34.2 }, { 3, 34.2 } } },
                           { 7, { { 0, 33.6 }, { 3, 33.6 } } },
                           { 8, { { 0, 33.48 }, { 3, 33.48 } } },
                           { 9, { { 0, 33.4 }, { 3, 33.4 } } },
                           { 10, { { 0, 33.2 }, { 3, 33.2 } } } } } };

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
  { "hantro_collage_w352h288_nv12.yuv", 8, AOM_IMG_FMT_NV12, AOM_BITS_8, 0 },
};

// Params: test video, speed, aq mode, threads, tile columns.
class RTEndToEndTest
    : public ::libaom_test::CodecTestWith5Params<TestVideoParam, int,
                                                 unsigned int, int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  RTEndToEndTest()
      : EncoderTest(GET_PARAM(0)), test_video_param_(GET_PARAM(1)),
        cpu_used_(GET_PARAM(2)), psnr_(0.0), nframes_(0),
        aq_mode_(GET_PARAM(3)), threads_(GET_PARAM(4)),
        tile_columns_(GET_PARAM(5)) {}

  virtual ~RTEndToEndTest() {}

  virtual void SetUp() {
    InitializeConfig(::libaom_test::kRealTime);

    cfg_.g_threads = threads_;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.kf_max_dist = 9999;
    cfg_.kf_min_dist = 9999;
  }

  virtual void BeginPassHook(unsigned int) {
    psnr_ = 0.0;
    nframes_ = 0;
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    psnr_ += pkt->data.psnr.psnr[0];
    nframes_++;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_ENABLE_RESTORATION, 0);
      encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
      encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_WARPED_MOTION, 0);
      encoder->Control(AV1E_SET_DELTAQ_MODE, 0);
      encoder->Control(AV1E_SET_ENABLE_TPL_MODEL, 0);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, tile_columns_);
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);
      encoder->Control(AV1E_SET_AQ_MODE, aq_mode_);
      encoder->Control(AV1E_SET_ROW_MT, 1);
      encoder->Control(AV1E_SET_ENABLE_CDEF, 1);
      encoder->Control(AV1E_SET_COEFF_COST_UPD_FREQ, 2);
      encoder->Control(AV1E_SET_MODE_COST_UPD_FREQ, 2);
      encoder->Control(AV1E_SET_MV_COST_UPD_FREQ, 2);
      encoder->Control(AV1E_SET_DV_COST_UPD_FREQ, 2);
    }
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  double GetPsnrThreshold() {
    return kPsnrThreshold[test_video_param_.filename][cpu_used_][aq_mode_];
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
    if (is_extension_y4m(test_video_param_.filename))
      video.reset(new libaom_test::Y4mVideoSource(test_video_param_.filename, 0,
                                                  kFrames));
    else
      video.reset(new libaom_test::YUVVideoSource(test_video_param_.filename,
                                                  test_video_param_.fmt, 352,
                                                  288, 30, 1, 0, kFrames));
    ASSERT_NE(video, nullptr);

    ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
    const double psnr = GetAveragePsnr();
    EXPECT_GT(psnr, GetPsnrThreshold())
        << "cpu used = " << cpu_used_ << " aq mode = " << aq_mode_;
  }

  TestVideoParam test_video_param_;
  int cpu_used_;

 private:
  double psnr_;
  unsigned int nframes_;
  unsigned int aq_mode_;
  int threads_;
  int tile_columns_;
};

class RTEndToEndTestThreaded : public RTEndToEndTest {};

TEST_P(RTEndToEndTest, EndtoEndPSNRTest) { DoTest(); }

TEST_P(RTEndToEndTestThreaded, EndtoEndPSNRTest) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(RTEndToEndTest, ::testing::ValuesIn(kTestVectors),
                           ::testing::Range(5, 12),
                           ::testing::Values<unsigned int>(0, 3),
                           ::testing::Values(1), ::testing::Values(1));

AV1_INSTANTIATE_TEST_SUITE(RTEndToEndTestThreaded,
                           ::testing::ValuesIn(kTestVectors),
                           ::testing::Range(5, 12),
                           ::testing::Values<unsigned int>(0, 3),
                           ::testing::Range(2, 5), ::testing::Range(2, 5));
}  // namespace
