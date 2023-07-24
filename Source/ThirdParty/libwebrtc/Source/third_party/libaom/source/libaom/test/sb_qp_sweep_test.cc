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

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/yuv_video_source.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

// Parameters: cpu-used, row-mt.
class AV1SBQPSweepTest : public ::libaom_test::CodecTestWith2Params<int, bool>,
                         public ::libaom_test::EncoderTest {
 protected:
  AV1SBQPSweepTest()
      : EncoderTest(GET_PARAM(0)), set_cpu_used_(GET_PARAM(1)),
        row_mt_(GET_PARAM(2)) {
    init_flags_ = AOM_CODEC_USE_PSNR;
    aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
    cfg.w = 1280;
    cfg.h = 720;
    cfg.allow_lowbitdepth = 1;
    decoder_ =
        std::unique_ptr<::libaom_test::Decoder>(codec_->CreateDecoder(cfg, 0));
  }
  ~AV1SBQPSweepTest() override = default;

  void SetUp() override {
    InitializeConfig(::libaom_test::kTwoPassGood);

    ASSERT_NE(decoder_, nullptr);
    if (decoder_->IsAV1()) {
      decoder_->Control(AV1_SET_DECODE_TILE_ROW, -1);
      decoder_->Control(AV1_SET_DECODE_TILE_COL, -1);
    }

    cfg_.g_lag_in_frames = 5;
    cfg_.rc_end_usage = AOM_Q;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      SetTileSize(encoder);
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_ENABLE_SB_QP_SWEEP, use_sb_sweep_);
      encoder->Control(AV1E_SET_ROW_MT, row_mt_);

      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    }
  }

  virtual void SetTileSize(libaom_test::Encoder *encoder) {
    encoder->Control(AV1E_SET_TILE_COLUMNS, 1);
    encoder->Control(AV1E_SET_TILE_ROWS, 1);
  }

  void BeginPassHook(unsigned int) override {
    psnr_ = 0.0;
    nframes_ = 0;
  }

  void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) override {
    psnr_ += pkt->data.psnr.psnr[0];
    nframes_++;
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  double GetAverageFrameSize() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    sum_frame_size_ += pkt->data.frame.sz;

    const aom_codec_err_t res = decoder_->DecodeFrame(
        reinterpret_cast<uint8_t *>(pkt->data.frame.buf), pkt->data.frame.sz);
    if (res != AOM_CODEC_OK) {
      abort_ = true;
      ASSERT_EQ(AOM_CODEC_OK, res);
    }
  }

  void DoTest() {
    ::libaom_test::YUVVideoSource video(
        "niklas_640_480_30.yuv", AOM_IMG_FMT_I420, 640, 480, 30, 1, 0, 6);
    cfg_.rc_target_bitrate = 1000;

    // Encode without sb_qp_sweep
    use_sb_sweep_ = false;
    sum_frame_size_ = 0;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    const double psnr_1 = GetAveragePsnr();
    const size_t avg_frame_size_1 = sum_frame_size_ / nframes_;

    // Encode with sb_qp_sweep
    use_sb_sweep_ = true;
    sum_frame_size_ = 0;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    const double psnr_2 = GetAveragePsnr();
    const size_t avg_frame_size_2 = sum_frame_size_ / nframes_;

    if (psnr_1 >= psnr_2) {
      ASSERT_GE(avg_frame_size_1, avg_frame_size_2);
    }
    if (avg_frame_size_1 <= avg_frame_size_2) {
      ASSERT_LE(psnr_1, psnr_2);
    }
  }

  bool use_sb_sweep_;
  int set_cpu_used_;
  bool row_mt_;
  double psnr_;
  unsigned int nframes_;
  size_t sum_frame_size_;
  std::unique_ptr<::libaom_test::Decoder> decoder_;
};

TEST_P(AV1SBQPSweepTest, SweepMatchTest) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(AV1SBQPSweepTest, ::testing::Range(4, 6),
                           ::testing::Bool());

}  // namespace
