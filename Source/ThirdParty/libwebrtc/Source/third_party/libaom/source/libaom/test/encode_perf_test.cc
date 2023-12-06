/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <string>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_config.h"
#include "config/aom_version.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "aom_ports/aom_timer.h"

namespace {

const int kMaxPsnr = 100;
const double kUsecsInSec = 1000000.0;

struct EncodePerfTestVideo {
  EncodePerfTestVideo(const char *name_, uint32_t width_, uint32_t height_,
                      uint32_t bitrate_, int frames_)
      : name(name_), width(width_), height(height_), bitrate(bitrate_),
        frames(frames_) {}
  const char *name;
  uint32_t width;
  uint32_t height;
  uint32_t bitrate;
  int frames;
};

const EncodePerfTestVideo kAV1EncodePerfTestVectors[] = {
  EncodePerfTestVideo("desktop_640_360_30.yuv", 640, 360, 200, 2484),
  EncodePerfTestVideo("kirland_640_480_30.yuv", 640, 480, 200, 300),
  EncodePerfTestVideo("macmarcomoving_640_480_30.yuv", 640, 480, 200, 987),
  EncodePerfTestVideo("macmarcostationary_640_480_30.yuv", 640, 480, 200, 718),
  EncodePerfTestVideo("niklas_640_480_30.yuv", 640, 480, 200, 471),
  EncodePerfTestVideo("tacomanarrows_640_480_30.yuv", 640, 480, 200, 300),
  EncodePerfTestVideo("tacomasmallcameramovement_640_480_30.yuv", 640, 480, 200,
                      300),
  EncodePerfTestVideo("thaloundeskmtg_640_480_30.yuv", 640, 480, 200, 300),
  EncodePerfTestVideo("niklas_1280_720_30.yuv", 1280, 720, 600, 470),
};

const int kEncodePerfTestSpeeds[] = { 5, 6, 7, 8 };
const int kEncodePerfTestThreads[] = { 1, 2, 4 };

class AV1EncodePerfTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  AV1EncodePerfTest()
      : EncoderTest(GET_PARAM(0)), min_psnr_(kMaxPsnr), nframes_(0),
        encoding_mode_(GET_PARAM(1)), speed_(0), threads_(1) {}

  ~AV1EncodePerfTest() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);

    cfg_.g_lag_in_frames = 0;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_undershoot_pct = 50;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.g_error_resilient = 1;
    cfg_.g_threads = threads_;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      const int log2_tile_columns = 3;
      encoder->Control(AOME_SET_CPUUSED, speed_);
      encoder->Control(AV1E_SET_TILE_COLUMNS, log2_tile_columns);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 0);
    }
  }

  void BeginPassHook(unsigned int /*pass*/) override {
    min_psnr_ = kMaxPsnr;
    nframes_ = 0;
  }

  void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) override {
    if (pkt->data.psnr.psnr[0] < min_psnr_) {
      min_psnr_ = pkt->data.psnr.psnr[0];
    }
  }

  // for performance reasons don't decode
  bool DoDecode() const override { return false; }

  double min_psnr() const { return min_psnr_; }

  void set_speed(unsigned int speed) { speed_ = speed; }

  void set_threads(unsigned int threads) { threads_ = threads; }

 private:
  double min_psnr_;
  unsigned int nframes_;
  libaom_test::TestMode encoding_mode_;
  unsigned speed_;
  unsigned int threads_;
};

TEST_P(AV1EncodePerfTest, PerfTest) {
  for (const EncodePerfTestVideo &test_video : kAV1EncodePerfTestVectors) {
    for (int speed : kEncodePerfTestSpeeds) {
      for (int threads : kEncodePerfTestThreads) {
        if (test_video.width < 512 && threads > 1)
          continue;
        else if (test_video.width < 1024 && threads > 2)
          continue;

        set_threads(threads);
        SetUp();

        const aom_rational timebase = { 33333333, 1000000000 };
        cfg_.g_timebase = timebase;
        cfg_.rc_target_bitrate = test_video.bitrate;

        init_flags_ = AOM_CODEC_USE_PSNR;

        const unsigned frames = test_video.frames;
        const char *video_name = test_video.name;
        libaom_test::I420VideoSource video(video_name, test_video.width,
                                           test_video.height, timebase.den,
                                           timebase.num, 0, test_video.frames);
        set_speed(speed);

        aom_usec_timer t;
        aom_usec_timer_start(&t);

        ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

        aom_usec_timer_mark(&t);
        const double elapsed_secs = aom_usec_timer_elapsed(&t) / kUsecsInSec;
        const double fps = frames / elapsed_secs;
        const double minimum_psnr = min_psnr();
        std::string display_name(video_name);
        if (threads > 1) {
          char thread_count[32];
          snprintf(thread_count, sizeof(thread_count), "_t-%d", threads);
          display_name += thread_count;
        }

        printf("{\n");
        printf("\t\"type\" : \"encode_perf_test\",\n");
        printf("\t\"version\" : \"%s\",\n", VERSION_STRING_NOSP);
        printf("\t\"videoName\" : \"%s\",\n", display_name.c_str());
        printf("\t\"encodeTimeSecs\" : %f,\n", elapsed_secs);
        printf("\t\"totalFrames\" : %u,\n", frames);
        printf("\t\"framesPerSecond\" : %f,\n", fps);
        printf("\t\"minPsnr\" : %f,\n", minimum_psnr);
        printf("\t\"speed\" : %d,\n", speed);
        printf("\t\"threads\" : %d\n", threads);
        printf("}\n");
      }
    }
  }
}

AV1_INSTANTIATE_TEST_SUITE(AV1EncodePerfTest,
                           ::testing::Values(::libaom_test::kRealTime));
}  // namespace
