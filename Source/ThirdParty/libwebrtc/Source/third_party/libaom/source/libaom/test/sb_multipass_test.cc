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

#include <initializer_list>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/yuv_video_source.h"

namespace {
class AV1SBMultipassTest
    : public ::libaom_test::CodecTestWith2Params<int, bool>,
      public ::libaom_test::EncoderTest {
 protected:
  AV1SBMultipassTest()
      : EncoderTest(GET_PARAM(0)), set_cpu_used_(GET_PARAM(1)),
        row_mt_(GET_PARAM(2)) {
    init_flags_ = AOM_CODEC_USE_PSNR;
    aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
    cfg.w = 1280;
    cfg.h = 720;
    cfg.allow_lowbitdepth = 1;
    decoder_ = codec_->CreateDecoder(cfg, 0);
    if (decoder_->IsAV1()) {
      decoder_->Control(AV1_SET_DECODE_TILE_ROW, -1);
      decoder_->Control(AV1_SET_DECODE_TILE_COL, -1);
    }

    size_enc_.clear();
    md5_dec_.clear();
    md5_enc_.clear();
  }
  ~AV1SBMultipassTest() override { delete decoder_; }

  void SetUp() override {
    InitializeConfig(::libaom_test::kTwoPassGood);

    cfg_.g_lag_in_frames = 5;
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.rc_2pass_vbr_minsection_pct = 5;
    cfg_.rc_2pass_vbr_maxsection_pct = 2000;

    cfg_.rc_max_quantizer = 56;
    cfg_.rc_min_quantizer = 0;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      SetTileSize(encoder);
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_ENABLE_SB_MULTIPASS_UNIT_TEST, use_multipass_);
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

  void DoTest() {
    ::libaom_test::YUVVideoSource video(
        "niklas_640_480_30.yuv", AOM_IMG_FMT_I420, 640, 480, 30, 1, 0, 6);
    cfg_.rc_target_bitrate = 1000;

    // Encode while coding each sb once
    use_multipass_ = false;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    std::vector<size_t> single_pass_size_enc;
    std::vector<std::string> single_pass_md5_enc;
    std::vector<std::string> single_pass_md5_dec;
    single_pass_size_enc = size_enc_;
    single_pass_md5_enc = md5_enc_;
    single_pass_md5_dec = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    // Encode while coding each sb twice
    use_multipass_ = true;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    std::vector<size_t> multi_pass_size_enc;
    std::vector<std::string> multi_pass_md5_enc;
    std::vector<std::string> multi_pass_md5_dec;
    multi_pass_size_enc = size_enc_;
    multi_pass_md5_enc = md5_enc_;
    multi_pass_md5_dec = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    // Check that the vectors are equal.
    ASSERT_EQ(single_pass_size_enc, multi_pass_size_enc);
    ASSERT_EQ(single_pass_md5_enc, multi_pass_md5_enc);
    ASSERT_EQ(single_pass_md5_dec, multi_pass_md5_dec);
  }

  bool use_multipass_;
  int set_cpu_used_;
  bool row_mt_;
  ::libaom_test::Decoder *decoder_;
  std::vector<size_t> size_enc_;
  std::vector<std::string> md5_enc_;
  std::vector<std::string> md5_dec_;
};

TEST_P(AV1SBMultipassTest, TwoPassMatchTest) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(AV1SBMultipassTest, ::testing::Range(4, 6),
                           ::testing::Bool());

}  // namespace
