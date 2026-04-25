/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved.
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
#include "av1/encoder/enc_enums.h"
#include "av1/encoder/firstpass.h"

namespace {
const unsigned int kCqLevel = 18;

#if !CONFIG_REALTIME_ONLY
const size_t kFirstPassStatsSz = sizeof(FIRSTPASS_STATS);
class AVxFirstPassEncoderThreadTest
    : public ::libaom_test::CodecTestWith4Params<libaom_test::TestMode, int,
                                                 int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  AVxFirstPassEncoderThreadTest()
      : EncoderTest(GET_PARAM(0)), encoder_initialized_(false),
        encoding_mode_(GET_PARAM(1)), set_cpu_used_(GET_PARAM(2)),
        tile_rows_(GET_PARAM(3)), tile_cols_(GET_PARAM(4)) {
    init_flags_ = AOM_CODEC_USE_PSNR;

    row_mt_ = 1;
    firstpass_stats_.buf = nullptr;
    firstpass_stats_.sz = 0;
  }
  ~AVxFirstPassEncoderThreadTest() override { free(firstpass_stats_.buf); }

  void SetUp() override {
    InitializeConfig(encoding_mode_);

    cfg_.g_lag_in_frames = 35;
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.rc_2pass_vbr_minsection_pct = 5;
    cfg_.rc_2pass_vbr_maxsection_pct = 2000;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_min_quantizer = 0;
  }

  void BeginPassHook(unsigned int /*pass*/) override {
    encoder_initialized_ = false;
    abort_ = false;
  }

  void EndPassHook() override {
    // For first pass stats test, only run first pass encoder.
    if (cfg_.g_pass == AOM_RC_FIRST_PASS) abort_ = true;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource * /*video*/,
                          ::libaom_test::Encoder *encoder) override {
    if (!encoder_initialized_) {
      // Encode in 2-pass mode.
      SetTileSize(encoder);
      encoder->Control(AV1E_SET_ROW_MT, row_mt_);
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 0);

      encoder_initialized_ = true;
    }
  }

  virtual void SetTileSize(libaom_test::Encoder *encoder) {
    encoder->Control(AV1E_SET_TILE_COLUMNS, tile_cols_);
    encoder->Control(AV1E_SET_TILE_ROWS, tile_rows_);
  }

  void StatsPktHook(const aom_codec_cx_pkt_t *pkt) override {
    const uint8_t *const pkt_buf =
        reinterpret_cast<uint8_t *>(pkt->data.twopass_stats.buf);
    const size_t pkt_size = pkt->data.twopass_stats.sz;

    // First pass stats size equals sizeof(FIRSTPASS_STATS)
    EXPECT_EQ(pkt_size, kFirstPassStatsSz)
        << "Error: First pass stats size doesn't equal kFirstPassStatsSz";

    firstpass_stats_.buf =
        realloc(firstpass_stats_.buf, firstpass_stats_.sz + pkt_size);
    ASSERT_NE(firstpass_stats_.buf, nullptr);
    memcpy((uint8_t *)firstpass_stats_.buf + firstpass_stats_.sz, pkt_buf,
           pkt_size);
    firstpass_stats_.sz += pkt_size;
  }

  void DoTest();

  bool encoder_initialized_;
  ::libaom_test::TestMode encoding_mode_;
  int set_cpu_used_;
  int tile_rows_;
  int tile_cols_;
  int row_mt_;
  aom_fixed_buf_t firstpass_stats_;
};

static void compare_fp_stats_md5(aom_fixed_buf_t *fp_stats) {
  // fp_stats consists of 2 set of first pass encoding stats. These 2 set of
  // stats are compared to check if the stats match.
  uint8_t *stats1 = reinterpret_cast<uint8_t *>(fp_stats->buf);
  uint8_t *stats2 = stats1 + fp_stats->sz / 2;
  ::libaom_test::MD5 md5_row_mt_0, md5_row_mt_1;

  md5_row_mt_0.Add(stats1, fp_stats->sz / 2);
  const char *md5_row_mt_0_str = md5_row_mt_0.Get();

  md5_row_mt_1.Add(stats2, fp_stats->sz / 2);
  const char *md5_row_mt_1_str = md5_row_mt_1.Get();

  // Check md5 match.
  ASSERT_STREQ(md5_row_mt_0_str, md5_row_mt_1_str)
      << "MD5 checksums don't match";
}

void AVxFirstPassEncoderThreadTest::DoTest() {
  ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 60);
  aom_fixed_buf_t firstpass_stats;
  size_t single_run_sz;

  cfg_.rc_target_bitrate = 1000;

  // 5 encodes will be run:
  // 1. row_mt_=0 and threads=1
  // 2. row_mt_=1 and threads=1
  // 3. row_mt_=1 and threads=2
  // 4. row_mt_=1 and threads=4
  // 5. row_mt_=1 and threads=8

  // 4 comparisons will be made:
  // 1. Between run 1 and run 2.
  // 2. Between run 2 and run 3.
  // 3. Between run 3 and run 4.
  // 4. Between run 4 and run 5.

  // Test row_mt_: 0 vs 1 at single thread case(threads = 1)
  cfg_.g_threads = 1;

  row_mt_ = 0;
  init_flags_ = AOM_CODEC_USE_PSNR;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  row_mt_ = 1;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  firstpass_stats.buf = firstpass_stats_.buf;
  firstpass_stats.sz = firstpass_stats_.sz;
  single_run_sz = firstpass_stats_.sz / 2;

  // Compare to check if using or not using row-mt are bit exact.
  // Comparison 1 (between row_mt_=0 and row_mt_=1).
  ASSERT_NO_FATAL_FAILURE(compare_fp_stats_md5(&firstpass_stats));

  // Test single thread vs multiple threads
  row_mt_ = 1;

  cfg_.g_threads = 2;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // offset to the 2nd and 3rd run.
  firstpass_stats.buf = reinterpret_cast<void *>(
      reinterpret_cast<uint8_t *>(firstpass_stats_.buf) + single_run_sz);

  // Compare to check if single-thread and multi-thread stats are bit exact.
  // Comparison 2 (between threads=1 and threads=2).
  ASSERT_NO_FATAL_FAILURE(compare_fp_stats_md5(&firstpass_stats));

  cfg_.g_threads = 4;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // offset to the 3rd and 4th run
  firstpass_stats.buf = reinterpret_cast<void *>(
      reinterpret_cast<uint8_t *>(firstpass_stats_.buf) + single_run_sz * 2);

  // Comparison 3 (between threads=2 and threads=4).
  ASSERT_NO_FATAL_FAILURE(compare_fp_stats_md5(&firstpass_stats));

  cfg_.g_threads = 8;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  // offset to the 4th and 5th run.
  firstpass_stats.buf = reinterpret_cast<void *>(
      reinterpret_cast<uint8_t *>(firstpass_stats_.buf) + single_run_sz * 3);

  // Comparison 4 (between threads=4 and threads=8).
  compare_fp_stats_md5(&firstpass_stats);
}

TEST_P(AVxFirstPassEncoderThreadTest, FirstPassStatsTest) { DoTest(); }

using AVxFirstPassEncoderThreadTestLarge = AVxFirstPassEncoderThreadTest;

TEST_P(AVxFirstPassEncoderThreadTestLarge, FirstPassStatsTest) { DoTest(); }

#endif  // !CONFIG_REALTIME_ONLY

class AVxEncoderThreadTest
    : public ::libaom_test::CodecTestWith5Params<libaom_test::TestMode, int,
                                                 int, int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  AVxEncoderThreadTest()
      : EncoderTest(GET_PARAM(0)), encoder_initialized_(false),
        encoding_mode_(GET_PARAM(1)), set_cpu_used_(GET_PARAM(2)),
        tile_cols_(GET_PARAM(3)), tile_rows_(GET_PARAM(4)),
        row_mt_(GET_PARAM(5)) {
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
  ~AVxEncoderThreadTest() override { delete decoder_; }

  void SetUp() override {
    InitializeConfig(encoding_mode_);

    if (encoding_mode_ == ::libaom_test::kOnePassGood ||
        encoding_mode_ == ::libaom_test::kTwoPassGood) {
      cfg_.g_lag_in_frames = 6;
      cfg_.rc_2pass_vbr_minsection_pct = 5;
      cfg_.rc_2pass_vbr_maxsection_pct = 2000;
    } else if (encoding_mode_ == ::libaom_test::kRealTime) {
      cfg_.g_error_resilient = 1;
    }
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_min_quantizer = 0;
  }

  void BeginPassHook(unsigned int /*pass*/) override {
    encoder_initialized_ = false;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource * /*video*/,
                          ::libaom_test::Encoder *encoder) override {
    if (!encoder_initialized_) {
      SetTileSize(encoder);
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_ROW_MT, row_mt_);
      if (encoding_mode_ == ::libaom_test::kOnePassGood ||
          encoding_mode_ == ::libaom_test::kTwoPassGood) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 5);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
        encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 0);
        encoder->Control(AV1E_SET_MAX_GF_INTERVAL, 4);
        // In row_mt_=0 case, the output of single thread (1 thread) will be
        // compared with multi thread (4 thread) output (as per line no:340).
        // Currently, Loop restoration stage is conditionally disabled for speed
        // 5, 6 when num_workers > 1. Due to this, the match between single
        // thread and multi thread output can not be achieved. Hence, testing
        // this case alone with LR disabled.
        // TODO(aomedia:3446): Remove the constraint on this test case once Loop
        // restoration state is same in both single and multi thread path.
        if (set_cpu_used_ >= 5 && row_mt_ == 0)
          encoder->Control(AV1E_SET_ENABLE_RESTORATION, 0);
      } else if (encoding_mode_ == ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 0);
        encoder->Control(AV1E_SET_AQ_MODE, 3);
        encoder->Control(AV1E_SET_COEFF_COST_UPD_FREQ, 2);
        encoder->Control(AV1E_SET_MODE_COST_UPD_FREQ, 2);
        encoder->Control(AV1E_SET_MV_COST_UPD_FREQ, 3);
        encoder->Control(AV1E_SET_DV_COST_UPD_FREQ, 3);
      } else {
        encoder->Control(AOME_SET_CQ_LEVEL, kCqLevel);
      }
      encoder_initialized_ = true;
    }
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

  void DoTest() {
    ::libaom_test::YUVVideoSource video(
        "niklas_640_480_30.yuv", AOM_IMG_FMT_I420, 640, 480, 30, 1, 15, 26);
    cfg_.rc_target_bitrate = 1000;

    if (row_mt_ == 0) {
      // Encode using single thread.
      cfg_.g_threads = 1;
      init_flags_ = AOM_CODEC_USE_PSNR;
      ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
      std::vector<size_t> single_thr_size_enc;
      std::vector<std::string> single_thr_md5_enc;
      std::vector<std::string> single_thr_md5_dec;
      single_thr_size_enc = size_enc_;
      single_thr_md5_enc = md5_enc_;
      single_thr_md5_dec = md5_dec_;
      size_enc_.clear();
      md5_enc_.clear();
      md5_dec_.clear();

      // Encode using multiple threads.
      cfg_.g_threads = 4;
      ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
      std::vector<size_t> multi_thr_size_enc;
      std::vector<std::string> multi_thr_md5_enc;
      std::vector<std::string> multi_thr_md5_dec;
      multi_thr_size_enc = size_enc_;
      multi_thr_md5_enc = md5_enc_;
      multi_thr_md5_dec = md5_dec_;
      size_enc_.clear();
      md5_enc_.clear();
      md5_dec_.clear();

      // Check that the vectors are equal.
      ASSERT_EQ(single_thr_size_enc, multi_thr_size_enc);
      ASSERT_EQ(single_thr_md5_enc, multi_thr_md5_enc);
      ASSERT_EQ(single_thr_md5_dec, multi_thr_md5_dec);

      DoTestMaxThreads(&video, single_thr_size_enc, single_thr_md5_enc,
                       single_thr_md5_dec);
    } else if (row_mt_ == 1) {
      // Encode using multiple threads row-mt enabled.
      cfg_.g_threads = 2;
      ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
      std::vector<size_t> multi_thr2_row_mt_size_enc;
      std::vector<std::string> multi_thr2_row_mt_md5_enc;
      std::vector<std::string> multi_thr2_row_mt_md5_dec;
      multi_thr2_row_mt_size_enc = size_enc_;
      multi_thr2_row_mt_md5_enc = md5_enc_;
      multi_thr2_row_mt_md5_dec = md5_dec_;
      size_enc_.clear();
      md5_enc_.clear();
      md5_dec_.clear();

      // Disable threads=3 test for now to reduce the time so that the nightly
      // test would not time out.
      // cfg_.g_threads = 3;
      // ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
      // std::vector<size_t> multi_thr3_row_mt_size_enc;
      // std::vector<std::string> multi_thr3_row_mt_md5_enc;
      // std::vector<std::string> multi_thr3_row_mt_md5_dec;
      // multi_thr3_row_mt_size_enc = size_enc_;
      // multi_thr3_row_mt_md5_enc = md5_enc_;
      // multi_thr3_row_mt_md5_dec = md5_dec_;
      // size_enc_.clear();
      // md5_enc_.clear();
      // md5_dec_.clear();
      // Check that the vectors are equal.
      // ASSERT_EQ(multi_thr3_row_mt_size_enc, multi_thr2_row_mt_size_enc);
      // ASSERT_EQ(multi_thr3_row_mt_md5_enc, multi_thr2_row_mt_md5_enc);
      // ASSERT_EQ(multi_thr3_row_mt_md5_dec, multi_thr2_row_mt_md5_dec);

      cfg_.g_threads = 4;
      ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
      std::vector<size_t> multi_thr4_row_mt_size_enc;
      std::vector<std::string> multi_thr4_row_mt_md5_enc;
      std::vector<std::string> multi_thr4_row_mt_md5_dec;
      multi_thr4_row_mt_size_enc = size_enc_;
      multi_thr4_row_mt_md5_enc = md5_enc_;
      multi_thr4_row_mt_md5_dec = md5_dec_;
      size_enc_.clear();
      md5_enc_.clear();
      md5_dec_.clear();

      // Check that the vectors are equal.
      ASSERT_EQ(multi_thr4_row_mt_size_enc, multi_thr2_row_mt_size_enc);
      ASSERT_EQ(multi_thr4_row_mt_md5_enc, multi_thr2_row_mt_md5_enc);
      ASSERT_EQ(multi_thr4_row_mt_md5_dec, multi_thr2_row_mt_md5_dec);

      DoTestMaxThreads(&video, multi_thr2_row_mt_size_enc,
                       multi_thr2_row_mt_md5_enc, multi_thr2_row_mt_md5_dec);
    }
  }

  virtual void DoTestMaxThreads(::libaom_test::YUVVideoSource *video,
                                const std::vector<size_t> ref_size_enc,
                                const std::vector<std::string> ref_md5_enc,
                                const std::vector<std::string> ref_md5_dec) {
    cfg_.g_threads = MAX_NUM_THREADS;
    ASSERT_NO_FATAL_FAILURE(RunLoop(video));
    std::vector<size_t> multi_thr_max_row_mt_size_enc;
    std::vector<std::string> multi_thr_max_row_mt_md5_enc;
    std::vector<std::string> multi_thr_max_row_mt_md5_dec;
    multi_thr_max_row_mt_size_enc = size_enc_;
    multi_thr_max_row_mt_md5_enc = md5_enc_;
    multi_thr_max_row_mt_md5_dec = md5_dec_;
    size_enc_.clear();
    md5_enc_.clear();
    md5_dec_.clear();

    // Check that the vectors are equal.
    ASSERT_EQ(ref_size_enc, multi_thr_max_row_mt_size_enc);
    ASSERT_EQ(ref_md5_enc, multi_thr_max_row_mt_md5_enc);
    ASSERT_EQ(ref_md5_dec, multi_thr_max_row_mt_md5_dec);
  }

  bool encoder_initialized_;
  ::libaom_test::TestMode encoding_mode_;
  int set_cpu_used_;
  int tile_cols_;
  int tile_rows_;
  int row_mt_;
  ::libaom_test::Decoder *decoder_;
  std::vector<size_t> size_enc_;
  std::vector<std::string> md5_enc_;
  std::vector<std::string> md5_dec_;
};

class AVxEncoderThreadRTTest : public AVxEncoderThreadTest {};

TEST_P(AVxEncoderThreadRTTest, EncoderResultTest) {
  cfg_.large_scale_tile = 0;
  decoder_->Control(AV1_SET_TILE_MODE, 0);
  DoTest();
}

// For real time mode, test speed 5, 6, 7, 8, 9, 10.
AV1_INSTANTIATE_TEST_SUITE(AVxEncoderThreadRTTest,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Values(5, 6, 7, 8, 9, 10),
                           ::testing::Values(0, 2), ::testing::Values(0, 2),
                           ::testing::Values(0, 1));

#if !CONFIG_REALTIME_ONLY

// The AVxEncoderThreadTestLarge takes up ~14% of total run-time of the
// Valgrind long tests. Exclude it; the smaller tests are still run.
#if !defined(AOM_VALGRIND_BUILD)
class AVxEncoderThreadTestLarge : public AVxEncoderThreadTest {};

TEST_P(AVxEncoderThreadTestLarge, EncoderResultTest) {
  cfg_.large_scale_tile = 0;
  decoder_->Control(AV1_SET_TILE_MODE, 0);
  DoTest();
}

// Test cpu_used 0, 1, 3 and 5.
AV1_INSTANTIATE_TEST_SUITE(AVxEncoderThreadTestLarge,
                           ::testing::Values(::libaom_test::kTwoPassGood,
                                             ::libaom_test::kOnePassGood),
                           ::testing::Values(0, 1, 3, 5),
                           ::testing::Values(1, 6), ::testing::Values(1, 6),
                           ::testing::Values(0, 1));
#endif  // !defined(AOM_VALGRIND_BUILD)

TEST_P(AVxEncoderThreadTest, EncoderResultTest) {
  cfg_.large_scale_tile = 0;
  decoder_->Control(AV1_SET_TILE_MODE, 0);
  DoTest();
}

class AVxEncoderThreadAllIntraTest : public AVxEncoderThreadTest {};

TEST_P(AVxEncoderThreadAllIntraTest, EncoderResultTest) {
  cfg_.large_scale_tile = 0;
  decoder_->Control(AV1_SET_TILE_MODE, 0);
  DoTest();
}

class AVxEncoderThreadAllIntraTestLarge : public AVxEncoderThreadTest {};

TEST_P(AVxEncoderThreadAllIntraTestLarge, EncoderResultTest) {
  cfg_.large_scale_tile = 0;
  decoder_->Control(AV1_SET_TILE_MODE, 0);
  DoTest();
}

// first pass stats test
AV1_INSTANTIATE_TEST_SUITE(AVxFirstPassEncoderThreadTest,
                           ::testing::Values(::libaom_test::kTwoPassGood),
                           ::testing::Values(4), ::testing::Range(0, 2),
                           ::testing::Range(1, 3));

AV1_INSTANTIATE_TEST_SUITE(AVxFirstPassEncoderThreadTestLarge,
                           ::testing::Values(::libaom_test::kTwoPassGood),
                           ::testing::Values(0, 2), ::testing::Range(0, 2),
                           ::testing::Range(1, 3));

// Only test cpu_used 2 here.
AV1_INSTANTIATE_TEST_SUITE(AVxEncoderThreadTest,
                           ::testing::Values(::libaom_test::kTwoPassGood),
                           ::testing::Values(2), ::testing::Values(0, 2),
                           ::testing::Values(0, 2), ::testing::Values(0, 1));

// For all intra mode, test speed 0, 2, 4, 6, 8.
// Only test cpu_used 6 here.
AV1_INSTANTIATE_TEST_SUITE(AVxEncoderThreadAllIntraTest,
                           ::testing::Values(::libaom_test::kAllIntra),
                           ::testing::Values(6), ::testing::Values(0, 2),
                           ::testing::Values(0, 2), ::testing::Values(0, 1));

// Test cpu_used 0, 2, 4 and 8.
AV1_INSTANTIATE_TEST_SUITE(AVxEncoderThreadAllIntraTestLarge,
                           ::testing::Values(::libaom_test::kAllIntra),
                           ::testing::Values(0, 2, 4, 8),
                           ::testing::Values(1, 6), ::testing::Values(1, 6),
                           ::testing::Values(0, 1));
#endif  // !CONFIG_REALTIME_ONLY

class AVxEncoderThreadLSTest : public AVxEncoderThreadTest {
  void SetTileSize(libaom_test::Encoder *encoder) override {
    encoder->Control(AV1E_SET_TILE_COLUMNS, tile_cols_);
    encoder->Control(AV1E_SET_TILE_ROWS, tile_rows_);
  }

  void DoTestMaxThreads(::libaom_test::YUVVideoSource *video,
                        const std::vector<size_t> ref_size_enc,
                        const std::vector<std::string> ref_md5_enc,
                        const std::vector<std::string> ref_md5_dec) override {
    (void)video;
    (void)ref_size_enc;
    (void)ref_md5_enc;
    (void)ref_md5_dec;
  }
};
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AVxEncoderThreadLSTest);

TEST_P(AVxEncoderThreadLSTest, EncoderResultTest) {
  cfg_.large_scale_tile = 1;
  decoder_->Control(AV1_SET_TILE_MODE, 1);
  decoder_->Control(AV1D_EXT_TILE_DEBUG, 1);
  DoTest();
}

// AVxEncoderThreadLSTestLarge takes up about 2% of total run-time of
// the Valgrind long tests. Since we already run AVxEncoderThreadLSTest,
// skip this one for Valgrind.
#if !CONFIG_REALTIME_ONLY && !defined(AOM_VALGRIND_BUILD)
class AVxEncoderThreadLSTestLarge : public AVxEncoderThreadLSTest {};

TEST_P(AVxEncoderThreadLSTestLarge, EncoderResultTest) {
  cfg_.large_scale_tile = 1;
  decoder_->Control(AV1_SET_TILE_MODE, 1);
  decoder_->Control(AV1D_EXT_TILE_DEBUG, 1);
  DoTest();
}

AV1_INSTANTIATE_TEST_SUITE(AVxEncoderThreadLSTestLarge,
                           ::testing::Values(::libaom_test::kTwoPassGood,
                                             ::libaom_test::kOnePassGood),
                           ::testing::Values(1, 3), ::testing::Values(0, 6),
                           ::testing::Values(0, 6), ::testing::Values(1));
#endif  // !CONFIG_REALTIME_ONLY && !defined(AOM_VALGRIND_BUILD)
}  // namespace
