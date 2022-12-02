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
#include <tuple>

#include "config/aom_version.h"

#include "aom_ports/aom_timer.h"
#include "common/ivfenc.h"
#include "test/codec_factory.h"
#include "test/decode_test_driver.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/ivf_video_source.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/webm_video_source.h"

using std::make_tuple;

namespace {

#define VIDEO_NAME 0
#define THREADS 1

const double kUsecsInSec = 1000000.0;
const char kNewEncodeOutputFile[] = "new_encode.ivf";

/*
 DecodePerfTest takes a tuple of filename + number of threads to decode with
 */
typedef std::tuple<const char *, unsigned> DecodePerfParam;

// TODO(jimbankoski): Add actual test vectors here when available.
// const DecodePerfParam kAV1DecodePerfVectors[] = {};

/*
 In order to reflect real world performance as much as possible, Perf tests
 *DO NOT* do any correctness checks. Please run them alongside correctness
 tests to ensure proper codec integrity. Furthermore, in this test we
 deliberately limit the amount of system calls we make to avoid OS
 preemption.

 TODO(joshualitt) create a more detailed perf measurement test to collect
   power/temp/min max frame decode times/etc
 */

class DecodePerfTest : public ::testing::TestWithParam<DecodePerfParam> {};

TEST_P(DecodePerfTest, PerfTest) {
  const char *const video_name = GET_PARAM(VIDEO_NAME);
  const unsigned threads = GET_PARAM(THREADS);

  libaom_test::WebMVideoSource video(video_name);
  video.Init();

  aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
  cfg.threads = threads;
  cfg.allow_lowbitdepth = 1;
  libaom_test::AV1Decoder decoder(cfg, 0);

  aom_usec_timer t;
  aom_usec_timer_start(&t);

  for (video.Begin(); video.cxdata() != nullptr; video.Next()) {
    decoder.DecodeFrame(video.cxdata(), video.frame_size());
  }

  aom_usec_timer_mark(&t);
  const double elapsed_secs = double(aom_usec_timer_elapsed(&t)) / kUsecsInSec;
  const unsigned frames = video.frame_number();
  const double fps = double(frames) / elapsed_secs;

  printf("{\n");
  printf("\t\"type\" : \"decode_perf_test\",\n");
  printf("\t\"version\" : \"%s\",\n", VERSION_STRING_NOSP);
  printf("\t\"videoName\" : \"%s\",\n", video_name);
  printf("\t\"threadCount\" : %u,\n", threads);
  printf("\t\"decodeTimeSecs\" : %f,\n", elapsed_secs);
  printf("\t\"totalFrames\" : %u,\n", frames);
  printf("\t\"framesPerSecond\" : %f\n", fps);
  printf("}\n");
}

// TODO(jimbankoski): Enabled when we have actual AV1 Decode vectors.
// INSTANTIATE_TEST_SUITE_P(AV1, DecodePerfTest,
//                        ::testing::ValuesIn(kAV1DecodePerfVectors));

class AV1NewEncodeDecodePerfTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  AV1NewEncodeDecodePerfTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)), speed_(0),
        outfile_(0), out_frames_(0) {}

  virtual ~AV1NewEncodeDecodePerfTest() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(encoding_mode_);

    cfg_.g_lag_in_frames = 25;
    cfg_.rc_min_quantizer = 2;
    cfg_.rc_max_quantizer = 56;
    cfg_.rc_dropframe_thresh = 0;
    cfg_.rc_undershoot_pct = 50;
    cfg_.rc_overshoot_pct = 50;
    cfg_.rc_buf_sz = 1000;
    cfg_.rc_buf_initial_sz = 500;
    cfg_.rc_buf_optimal_sz = 600;
    cfg_.rc_end_usage = AOM_VBR;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, speed_);
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, 2);
    }
  }

  virtual void BeginPassHook(unsigned int /*pass*/) {
    const char *const env = getenv("LIBAOM_TEST_DATA_PATH");
    const std::string data_path(env ? env : ".");
    const std::string path_to_source = data_path + "/" + kNewEncodeOutputFile;
    outfile_ = fopen(path_to_source.c_str(), "wb");
    ASSERT_NE(outfile_, nullptr);
  }

  virtual void EndPassHook() {
    if (outfile_ != nullptr) {
      if (!fseek(outfile_, 0, SEEK_SET))
        ivf_write_file_header(outfile_, &cfg_, AV1_FOURCC, out_frames_);
      fclose(outfile_);
      outfile_ = nullptr;
    }
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    ++out_frames_;

    // Write initial file header if first frame.
    if (pkt->data.frame.pts == 0)
      ivf_write_file_header(outfile_, &cfg_, AV1_FOURCC, out_frames_);

    // Write frame header and data.
    ivf_write_frame_header(outfile_, out_frames_, pkt->data.frame.sz);
    ASSERT_EQ(fwrite(pkt->data.frame.buf, 1, pkt->data.frame.sz, outfile_),
              pkt->data.frame.sz);
  }

  virtual bool DoDecode() const { return false; }

  void set_speed(unsigned int speed) { speed_ = speed; }

 private:
  libaom_test::TestMode encoding_mode_;
  uint32_t speed_;
  FILE *outfile_;
  uint32_t out_frames_;
};

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
  EncodePerfTestVideo("niklas_1280_720_30.yuv", 1280, 720, 600, 470),
};

TEST_P(AV1NewEncodeDecodePerfTest, PerfTest) {
  SetUp();

  // TODO(JBB): Make this work by going through the set of given files.
  const int i = 0;
  const aom_rational timebase = { 33333333, 1000000000 };
  cfg_.g_timebase = timebase;
  cfg_.rc_target_bitrate = kAV1EncodePerfTestVectors[i].bitrate;

  init_flags_ = AOM_CODEC_USE_PSNR;

  const char *video_name = kAV1EncodePerfTestVectors[i].name;
  libaom_test::I420VideoSource video(
      video_name, kAV1EncodePerfTestVectors[i].width,
      kAV1EncodePerfTestVectors[i].height, timebase.den, timebase.num, 0,
      kAV1EncodePerfTestVectors[i].frames);
  set_speed(2);

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));

  const uint32_t threads = 4;

  libaom_test::IVFVideoSource decode_video(kNewEncodeOutputFile);
  decode_video.Init();

  aom_codec_dec_cfg_t cfg = aom_codec_dec_cfg_t();
  cfg.threads = threads;
  cfg.allow_lowbitdepth = 1;
  libaom_test::AV1Decoder decoder(cfg, 0);

  aom_usec_timer t;
  aom_usec_timer_start(&t);

  for (decode_video.Begin(); decode_video.cxdata() != nullptr;
       decode_video.Next()) {
    decoder.DecodeFrame(decode_video.cxdata(), decode_video.frame_size());
  }

  aom_usec_timer_mark(&t);
  const double elapsed_secs =
      static_cast<double>(aom_usec_timer_elapsed(&t)) / kUsecsInSec;
  const unsigned decode_frames = decode_video.frame_number();
  const double fps = static_cast<double>(decode_frames) / elapsed_secs;

  printf("{\n");
  printf("\t\"type\" : \"decode_perf_test\",\n");
  printf("\t\"version\" : \"%s\",\n", VERSION_STRING_NOSP);
  printf("\t\"videoName\" : \"%s\",\n", kNewEncodeOutputFile);
  printf("\t\"threadCount\" : %u,\n", threads);
  printf("\t\"decodeTimeSecs\" : %f,\n", elapsed_secs);
  printf("\t\"totalFrames\" : %u,\n", decode_frames);
  printf("\t\"framesPerSecond\" : %f\n", fps);
  printf("}\n");
}

AV1_INSTANTIATE_TEST_SUITE(AV1NewEncodeDecodePerfTest,
                           ::testing::Values(::libaom_test::kTwoPassGood));
}  // namespace
