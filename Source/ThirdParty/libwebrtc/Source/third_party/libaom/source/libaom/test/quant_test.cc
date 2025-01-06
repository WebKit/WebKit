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
#include "config/aom_config.h"

#include "av1/encoder/av1_quantize.h"
#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"

namespace {

const ::libaom_test::TestMode kTestMode[] =
#if CONFIG_REALTIME_ONLY
    { ::libaom_test::kRealTime };
#else
    { ::libaom_test::kRealTime, ::libaom_test::kOnePassGood };
#endif

class QMTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  QMTest() : EncoderTest(GET_PARAM(0)) {}
  ~QMTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    set_cpu_used_ = GET_PARAM(2);
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, set_cpu_used_);
      encoder->Control(AV1E_SET_ENABLE_QM, 1);
      encoder->Control(AV1E_SET_QM_MIN, qm_min_);
      encoder->Control(AV1E_SET_QM_MAX, qm_max_);

      encoder->Control(AOME_SET_MAX_INTRA_BITRATE_PCT, 100);
      if (mode_ == ::libaom_test::kRealTime) {
        encoder->Control(AV1E_SET_ALLOW_WARPED_MOTION, 0);
        encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
        encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
      }
    }
  }

  void DoTest(int qm_min, int qm_max) {
    qm_min_ = qm_min;
    qm_max_ = qm_max;
    cfg_.kf_max_dist = 12;
    cfg_.rc_min_quantizer = 8;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_lag_in_frames = 6;
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 500;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_target_bitrate = 300;
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 15);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int set_cpu_used_;
  int qm_min_;
  int qm_max_;
};

// encodes and decodes without a mismatch.
TEST_P(QMTest, TestNoMisMatchQM1) { DoTest(5, 9); }

// encodes and decodes without a mismatch.
TEST_P(QMTest, TestNoMisMatchQM2) { DoTest(0, 8); }

// encodes and decodes without a mismatch.
TEST_P(QMTest, TestNoMisMatchQM3) { DoTest(9, 15); }

AV1_INSTANTIATE_TEST_SUITE(QMTest, ::testing::ValuesIn(kTestMode),
                           ::testing::Range(5, 9));

#if !CONFIG_REALTIME_ONLY
typedef struct {
  const unsigned int min_q;
  const unsigned int max_q;
} QuantParam;

const QuantParam QuantTestParams[] = {
  { 0, 10 }, { 0, 60 }, { 20, 35 }, { 35, 50 }, { 50, 63 }
};

std::ostream &operator<<(std::ostream &os, const QuantParam &test_arg) {
  return os << "QuantParam { min_q:" << test_arg.min_q
            << " max_q:" << test_arg.max_q << " }";
}

/*
 * This class is used to test whether base_qindex is within min
 * and max quantizer range configured by user.
 */
class QuantizerBoundsCheckTestLarge
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode,
                                                 QuantParam, aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  QuantizerBoundsCheckTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        quant_param_(GET_PARAM(2)), rc_end_usage_(GET_PARAM(3)) {
    quant_bound_violated_ = false;
  }
  ~QuantizerBoundsCheckTestLarge() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = rc_end_usage_;
    cfg_.g_threads = 1;
    cfg_.rc_min_quantizer = quant_param_.min_q;
    cfg_.rc_max_quantizer = quant_param_.max_q;
    cfg_.g_lag_in_frames = 35;
    if (rc_end_usage_ != AOM_Q) {
      cfg_.rc_target_bitrate = 400;
    }
  }

  bool DoDecode() const override { return true; }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
    }
  }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_LAST_QUANTIZER,
                                    &base_qindex_);
      min_bound_qindex_ = av1_quantizer_to_qindex(cfg_.rc_min_quantizer);
      max_bound_qindex_ = av1_quantizer_to_qindex(cfg_.rc_max_quantizer);
      if ((base_qindex_ < min_bound_qindex_ ||
           base_qindex_ > max_bound_qindex_) &&
          quant_bound_violated_ == false) {
        quant_bound_violated_ = true;
      }
    }
    return AOM_CODEC_OK == res_dec;
  }

  ::libaom_test::TestMode encoding_mode_;
  const QuantParam quant_param_;
  int base_qindex_;
  int min_bound_qindex_;
  int max_bound_qindex_;
  bool quant_bound_violated_;
  aom_rc_mode rc_end_usage_;
};

TEST_P(QuantizerBoundsCheckTestLarge, QuantizerBoundsCheckEncodeTest) {
  libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352, 288,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 50);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(quant_bound_violated_, false);
}

AV1_INSTANTIATE_TEST_SUITE(QuantizerBoundsCheckTestLarge,
                           ::testing::Values(::libaom_test::kOnePassGood,
                                             ::libaom_test::kTwoPassGood),
                           ::testing::ValuesIn(QuantTestParams),
                           ::testing::Values(AOM_Q, AOM_VBR, AOM_CBR, AOM_CQ));
#endif  // !CONFIG_REALTIME_ONLY
}  // namespace
