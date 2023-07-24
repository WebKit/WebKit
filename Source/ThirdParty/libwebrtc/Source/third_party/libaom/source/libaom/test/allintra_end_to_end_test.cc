/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"

namespace {

const unsigned int kFrames = 20;
const int kBitrate = 500;
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
  { "niklas_1280_720_30.y4m", 8, AOM_IMG_FMT_I420, AOM_BITS_8, 0 },
  { "park_joy_90p_8_420.y4m", 8, AOM_IMG_FMT_I420, AOM_BITS_8, 0 },
};

// Params: test video, speed, aq mode, threads, tile columns.
class AllIntraEndToEndTest
    : public ::libaom_test::CodecTestWith6Params<TestVideoParam, int, int, int,
                                                 int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  AllIntraEndToEndTest()
      : EncoderTest(GET_PARAM(0)), test_video_param_(GET_PARAM(1)),
        cpu_used_(GET_PARAM(2)), psnr_(0.0), nframes_(0),
        deltaq_mode_(GET_PARAM(3)), threads_(GET_PARAM(4)),
        tile_columns_(GET_PARAM(5)), enable_tx_size_search_(GET_PARAM(6)) {}

  virtual ~AllIntraEndToEndTest() {}

  virtual void SetUp() {
    InitializeConfig(::libaom_test::kAllIntra);
    cfg_.g_threads = threads_;
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
      encoder->Control(AV1E_SET_ROW_MT, 1);
      encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_DEFAULT);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, tile_columns_);
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_DELTAQ_MODE, deltaq_mode_);
      encoder->Control(AV1E_SET_ENABLE_TX_SIZE_SEARCH, enable_tx_size_search_);
    }
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
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
  }

  TestVideoParam test_video_param_;
  int cpu_used_;

 private:
  double psnr_;
  unsigned int nframes_;
  unsigned int deltaq_mode_;
  int threads_;
  int tile_columns_;
  int enable_tx_size_search_;
};

TEST_P(AllIntraEndToEndTest, EndToEndNoFailure) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(AllIntraEndToEndTest,
                           ::testing::ValuesIn(kTestVectors),
                           ::testing::Range(5, 9), ::testing::Range(0, 4),
                           ::testing::Values(1), ::testing::Values(1),
                           ::testing::Values(0, 1));

INSTANTIATE_TEST_SUITE_P(
    AV1MultiThreaded, AllIntraEndToEndTest,
    ::testing::Combine(
        ::testing::Values(
            static_cast<const libaom_test::CodecFactory *>(&libaom_test::kAV1)),
        ::testing::ValuesIn(kTestVectors), ::testing::Range(5, 9),
        ::testing::Range(0, 4), ::testing::Values(6), ::testing::Values(1),
        ::testing::Values(0, 1)));

}  // namespace
