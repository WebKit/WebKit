/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"

namespace {

#if CONFIG_FPMT_TEST && !CONFIG_REALTIME_ONLY
class AVxFrameParallelThreadEncodeTest
    : public ::libaom_test::CodecTestWith3Params<int, int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  AVxFrameParallelThreadEncodeTest()
      : EncoderTest(GET_PARAM(0)), encoder_initialized_(false),
        set_cpu_used_(GET_PARAM(1)), tile_cols_(GET_PARAM(2)),
        tile_rows_(GET_PARAM(3)) {
    aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
    cfg.w = 1280;
    cfg.h = 720;
    cfg.allow_lowbitdepth = 1;
    decoder_ = codec_->CreateDecoder(cfg, 0);
  }
  ~AVxFrameParallelThreadEncodeTest() override { delete decoder_; }

  void SetUp() override {
    InitializeConfig(::libaom_test::kTwoPassGood);
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.g_lag_in_frames = 35;
    cfg_.rc_2pass_vbr_minsection_pct = 5;
    cfg_.rc_2pass_vbr_maxsection_pct = 2000;
    cfg_.rc_max_quantizer = 63;
    cfg_.rc_min_quantizer = 0;
    cfg_.g_threads = 16;
  }

  void BeginPassHook(unsigned int /*pass*/) override {
    encoder_initialized_ = false;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource * /*video*/,
                          ::libaom_test::Encoder *encoder) override {
    if (encoder_initialized_) return;
    SetTileSize(encoder);
    encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
    encoder->Control(AV1E_SET_FP_MT, 1);
    encoder->Control(AV1E_SET_FP_MT_UNIT_TEST, enable_actual_parallel_encode_);
    encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
    encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
    encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 0);

    encoder_initialized_ = true;
  }

  virtual void SetTileSize(libaom_test::Encoder *encoder) {
    encoder->Control(AV1E_SET_TILE_COLUMNS, tile_cols_);
    encoder->Control(AV1E_SET_TILE_ROWS, tile_rows_);
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    size_enc_.push_back(pkt->data.frame.sz);

    ::libaom_test::MD5 md5_enc;
    md5_enc.Add(reinterpret_cast<uint8_t *>(pkt->data.frame.buf),
                pkt->data.frame.sz);
    md5_enc_.push_back(md5_enc.Get());

    const aom_codec_err_t res = decoder_->DecodeFrame(
        reinterpret_cast<uint8_t *>(pkt->data.frame.buf), pkt->data.frame.sz);
    if (res != AOM_CODEC_OK) {
      abort_ = true;
      ASSERT_EQ(AOM_CODEC_OK, res);
    }
    const aom_image_t *img = decoder_->GetDxData().Next();

    if (img) {
      ::libaom_test::MD5 md5_res;
      md5_res.Add(img);
      md5_dec_.push_back(md5_res.Get());
    }
  }

  void DoTest(::libaom_test::VideoSource *input_video) {
    /* This is the actual parallel encode of frames using multiple cpis.
     * The parallel frames are independently encoded.
     * Threads are distributed among the parallel frames whereas non-parallel
     * frames use all the threads. Example: for 8 threads, in case of 4 frames
     * in a parallel encode set, each frame gets 2 threads. In case of 3 frames
     * in a parallel encode set, threads are distributed as 2, 3 ,3.
     */
    enable_actual_parallel_encode_ = 1;
    ASSERT_NO_FATAL_FAILURE(RunLoop(input_video));
    std::vector<size_t> enc_stream_fpmt_size;
    std::vector<std::string> enc_stream_fpmt;
    std::vector<std::string> dec_stream_fpmt;
    enc_stream_fpmt_size = size_enc_;
    enc_stream_fpmt = md5_enc_;
    dec_stream_fpmt = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    /* This is the simulation of parallel encode of frames using single cpi.
     * In simulation, it should be ensured to have no dependency across frames
     * (similar to parallel encode).
     * Each frame uses all the threads configured.
     */
    enable_actual_parallel_encode_ = 0;
    ASSERT_NO_FATAL_FAILURE(RunLoop(input_video));
    std::vector<size_t> enc_stream_sim_size;
    std::vector<std::string> enc_stream_sim;
    std::vector<std::string> dec_stream_sim;
    enc_stream_sim_size = size_enc_;
    enc_stream_sim = md5_enc_;
    dec_stream_sim = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    // Check that the vectors are equal.
    ASSERT_EQ(enc_stream_sim_size, enc_stream_fpmt_size);
    ASSERT_EQ(enc_stream_sim, enc_stream_fpmt);
    ASSERT_EQ(dec_stream_sim, dec_stream_fpmt);
  }

  bool encoder_initialized_;
  int set_cpu_used_;
  int tile_cols_;
  int tile_rows_;
  int enable_actual_parallel_encode_;
  ::libaom_test::Decoder *decoder_;
  std::vector<size_t> size_enc_;
  std::vector<std::string> md5_enc_;
  std::vector<std::string> md5_dec_;
};

class AVxFrameParallelThreadEncodeHDResTestLarge
    : public AVxFrameParallelThreadEncodeTest {};

TEST_P(AVxFrameParallelThreadEncodeHDResTestLarge,
       FrameParallelThreadEncodeTest) {
  ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  cfg_.rc_target_bitrate = 500;
  DoTest(&video);
}

class AVxFrameParallelThreadEncodeLowResTestLarge
    : public AVxFrameParallelThreadEncodeTest {};

TEST_P(AVxFrameParallelThreadEncodeLowResTestLarge,
       FrameParallelThreadEncodeTest) {
  ::libaom_test::YUVVideoSource video("hantro_collage_w352h288.yuv",
                                      AOM_IMG_FMT_I420, 352, 288, 30, 1, 0, 60);
  cfg_.rc_target_bitrate = 200;
  DoTest(&video);
}

class AVxFrameParallelThreadEncodeLowResTest
    : public AVxFrameParallelThreadEncodeTest {};

TEST_P(AVxFrameParallelThreadEncodeLowResTest, FrameParallelThreadEncodeTest) {
  ::libaom_test::YUVVideoSource video("hantro_collage_w352h288.yuv",
                                      AOM_IMG_FMT_I420, 352, 288, 30, 1, 0, 60);
  cfg_.rc_target_bitrate = 200;
  DoTest(&video);
}

AV1_INSTANTIATE_TEST_SUITE(AVxFrameParallelThreadEncodeHDResTestLarge,
                           ::testing::Values(2, 3, 4, 5, 6),
                           ::testing::Values(0, 1, 2), ::testing::Values(0, 1));

AV1_INSTANTIATE_TEST_SUITE(AVxFrameParallelThreadEncodeLowResTestLarge,
                           ::testing::Values(2, 3), ::testing::Values(0, 1, 2),
                           ::testing::Values(0, 1));

AV1_INSTANTIATE_TEST_SUITE(AVxFrameParallelThreadEncodeLowResTest,
                           ::testing::Values(4, 5, 6), ::testing::Values(1),
                           ::testing::Values(0));
#endif  // CONFIG_FPMT_TEST && !CONFIG_REALTIME_ONLY

}  // namespace
