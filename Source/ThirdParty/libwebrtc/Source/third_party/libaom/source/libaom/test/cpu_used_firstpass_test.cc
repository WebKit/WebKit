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

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

const double kPsnrDiffThreshold = 0.1;

// Params: first pass cpu used, second pass cpu used
class CpuUsedFirstpassTest
    : public ::libaom_test::CodecTestWith2Params<int, int>,
      public ::libaom_test::EncoderTest {
 protected:
  CpuUsedFirstpassTest()
      : EncoderTest(GET_PARAM(0)), second_pass_cpu_used_(GET_PARAM(2)) {}
  virtual ~CpuUsedFirstpassTest() {}

  virtual void SetUp() {
    InitializeConfig(::libaom_test::kTwoPassGood);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = AOM_VBR;
    cfg_.rc_target_bitrate = 1000;
    cfg_.g_lag_in_frames = 19;
    cfg_.g_threads = 0;
    init_flags_ = AOM_CODEC_USE_PSNR;
  }

  virtual void BeginPassHook(unsigned int pass) {
    psnr_ = 0.0;
    nframes_ = 0;

    if (pass == 0)
      cpu_used_ = first_pass_cpu_used_;
    else
      cpu_used_ = second_pass_cpu_used_;
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    psnr_ += pkt->data.psnr.psnr[0];
    nframes_++;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    }
  }

  double GetAveragePsnr() const {
    if (nframes_) return psnr_ / nframes_;
    return 0.0;
  }

  double GetPsnrDiffThreshold() { return kPsnrDiffThreshold; }

  void DoTest() {
    libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480,
                                       cfg_.g_timebase.den, cfg_.g_timebase.num,
                                       0, 30);
    double ref_psnr;
    double psnr_diff;

    first_pass_cpu_used_ = second_pass_cpu_used_;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));  // same preset case ref_psnr
    ref_psnr = GetAveragePsnr();

    first_pass_cpu_used_ = GET_PARAM(1);
    if (first_pass_cpu_used_ == second_pass_cpu_used_) return;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    psnr_diff = abs(ref_psnr - GetAveragePsnr());
    EXPECT_LT(psnr_diff, GetPsnrDiffThreshold())
        << "first pass cpu used = " << first_pass_cpu_used_
        << ", second pass cpu used = " << second_pass_cpu_used_;
  }

  int cpu_used_;
  int first_pass_cpu_used_;
  int second_pass_cpu_used_;
  unsigned int nframes_;
  double psnr_;
};

TEST_P(CpuUsedFirstpassTest, FirstPassTest) { DoTest(); }

class CpuUsedFirstpassTestLarge : public CpuUsedFirstpassTest {};

TEST_P(CpuUsedFirstpassTestLarge, FirstPassTest) { DoTest(); }

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
static const int kSecondPassCpuUsedLarge[] = { 2, 4 };
static const int kSecondPassCpuUsed[] = { 6 };
#else
static const int kSecondPassCpuUsedLarge[] = { 2 };
static const int kSecondPassCpuUsed[] = { 4, 6 };
#endif
#else
static const int kSecondPassCpuUsedLarge[] = { 2 };
static const int kSecondPassCpuUsed[] = { 4, 6 };
#endif

AV1_INSTANTIATE_TEST_SUITE(
    CpuUsedFirstpassTestLarge, ::testing::Values(2, 4, 6),
    ::testing::ValuesIn(kSecondPassCpuUsedLarge));  // cpu_used

AV1_INSTANTIATE_TEST_SUITE(
    CpuUsedFirstpassTest, ::testing::Values(2, 4, 6),
    ::testing::ValuesIn(kSecondPassCpuUsed));  // cpu_used

}  // namespace
