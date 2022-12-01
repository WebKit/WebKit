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

#include <memory>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"
#include "test/yuv_video_source.h"
#include "av1/encoder/ratectrl.h"

namespace {

const unsigned int kFrames = 100;
const int kBitrate = 500;

#define ARF_NOT_SEEN 1000001
#define ARF_SEEN_ONCE 1000000

typedef struct {
  const char *filename;
  unsigned int width;
  unsigned int height;
  unsigned int framerate_num;
  unsigned int framerate_den;
  unsigned int input_bit_depth;
  aom_img_fmt fmt;
  aom_bit_depth_t bit_depth;
  unsigned int profile;
} TestVideoParam;

typedef struct {
  libaom_test::TestMode mode;
  int cpu_used;
} TestEncodeParam;

const TestVideoParam kTestVectors[] = {
  // artificially increase framerate to trigger default check
  { "hantro_collage_w352h288.yuv", 352, 288, 5000, 1, 8, AOM_IMG_FMT_I420,
    AOM_BITS_8, 0 },
  { "hantro_collage_w352h288.yuv", 352, 288, 30, 1, 8, AOM_IMG_FMT_I420,
    AOM_BITS_8, 0 },
  { "rush_hour_444.y4m", 352, 288, 30, 1, 8, AOM_IMG_FMT_I444, AOM_BITS_8, 1 },
  // Add list of profile 2/3 test videos here ...
};

const TestEncodeParam kEncodeVectors[] = {
#if CONFIG_REALTIME_ONLY
  { ::libaom_test::kRealTime, 5 },
#else
  { ::libaom_test::kRealTime, 5 },    { ::libaom_test::kOnePassGood, 2 },
  { ::libaom_test::kOnePassGood, 5 }, { ::libaom_test::kTwoPassGood, 1 },
  { ::libaom_test::kTwoPassGood, 2 }, { ::libaom_test::kTwoPassGood, 5 },
#endif
};

const int kMinArfVectors[] = {
  // NOTE: 0 refers to the default built-in logic in:
  //       av1_rc_get_default_min_gf_interval(...)
  0, 4, 8, 12, 15
};

class ArfFreqTestLarge
    : public ::libaom_test::CodecTestWith3Params<TestVideoParam,
                                                 TestEncodeParam, int>,
      public ::libaom_test::EncoderTest {
 protected:
  ArfFreqTestLarge()
      : EncoderTest(GET_PARAM(0)), test_video_param_(GET_PARAM(1)),
        test_encode_param_(GET_PARAM(2)), min_arf_requested_(GET_PARAM(3)) {}

  virtual ~ArfFreqTestLarge() {}

  virtual void SetUp() {
    InitializeConfig(test_encode_param_.mode);
    if (test_encode_param_.mode != ::libaom_test::kRealTime) {
      cfg_.g_lag_in_frames = 25;
    } else {
      cfg_.rc_buf_sz = 1000;
      cfg_.rc_buf_initial_sz = 500;
      cfg_.rc_buf_optimal_sz = 600;
    }
  }

  virtual void BeginPassHook(unsigned int) {
    min_run_ = ARF_NOT_SEEN;
    run_of_visible_frames_ = 0;
  }

  int GetNumFramesInPkt(const aom_codec_cx_pkt_t *pkt) {
    const uint8_t *buffer = reinterpret_cast<uint8_t *>(pkt->data.frame.buf);
    const uint8_t marker = buffer[pkt->data.frame.sz - 1];
    const int mag = ((marker >> 3) & 3) + 1;
    int frames = (marker & 0x7) + 1;
    const unsigned int index_sz = 2 + mag * frames;
    // Check for superframe or not.
    // Assume superframe has only one visible frame, the rest being
    // invisible. If superframe index is not found, then there is only
    // one frame.
    if (!((marker & 0xe0) == 0xc0 && pkt->data.frame.sz >= index_sz &&
          buffer[pkt->data.frame.sz - index_sz] == marker)) {
      frames = 1;
    }
    return frames;
  }

  virtual void FramePktHook(const aom_codec_cx_pkt_t *pkt) {
    if (pkt->kind != AOM_CODEC_CX_FRAME_PKT) return;
    const int frames = GetNumFramesInPkt(pkt);
    if (frames == 1) {
      run_of_visible_frames_++;
    } else if (frames == 2) {
      if (min_run_ == ARF_NOT_SEEN) {
        min_run_ = ARF_SEEN_ONCE;
      } else if (min_run_ == ARF_SEEN_ONCE ||
                 run_of_visible_frames_ < min_run_) {
        min_run_ = run_of_visible_frames_;
      }
      run_of_visible_frames_ = 1;
    } else {
      min_run_ = 0;
      run_of_visible_frames_ = 1;
    }
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, 4);
      encoder->Control(AOME_SET_CPUUSED, test_encode_param_.cpu_used);
      encoder->Control(AV1E_SET_MIN_GF_INTERVAL, min_arf_requested_);
      if (test_encode_param_.mode != ::libaom_test::kRealTime) {
        encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
        encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
        encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
      }
    }
  }

  int GetMinVisibleRun() const { return min_run_; }

  int GetMinArfDistanceRequested() const {
    if (min_arf_requested_)
      return min_arf_requested_;
    else
      return av1_rc_get_default_min_gf_interval(
          test_video_param_.width, test_video_param_.height,
          (double)test_video_param_.framerate_num /
              test_video_param_.framerate_den);
  }

  TestVideoParam test_video_param_;
  TestEncodeParam test_encode_param_;

 private:
  int min_arf_requested_;
  int min_run_;
  int run_of_visible_frames_;
};

TEST_P(ArfFreqTestLarge, MinArfFreqTest) {
  cfg_.rc_target_bitrate = kBitrate;
  cfg_.g_error_resilient = 0;
  cfg_.g_profile = test_video_param_.profile;
  cfg_.g_input_bit_depth = test_video_param_.input_bit_depth;
  cfg_.g_bit_depth = test_video_param_.bit_depth;
  init_flags_ = AOM_CODEC_USE_PSNR;
  if (cfg_.g_bit_depth > 8) init_flags_ |= AOM_CODEC_USE_HIGHBITDEPTH;

  std::unique_ptr<libaom_test::VideoSource> video;
  if (is_extension_y4m(test_video_param_.filename)) {
    video.reset(new libaom_test::Y4mVideoSource(test_video_param_.filename, 0,
                                                kFrames));
  } else {
    video.reset(new libaom_test::YUVVideoSource(
        test_video_param_.filename, test_video_param_.fmt,
        test_video_param_.width, test_video_param_.height,
        test_video_param_.framerate_num, test_video_param_.framerate_den, 0,
        kFrames));
  }
  ASSERT_NE(video, nullptr);

  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
  const int min_run = GetMinVisibleRun();
  const int min_arf_dist_requested = GetMinArfDistanceRequested();
  if (min_run != ARF_NOT_SEEN && min_run != ARF_SEEN_ONCE) {
    const int min_arf_dist = min_run + 1;
    EXPECT_GE(min_arf_dist, min_arf_dist_requested);
  }
}

#if CONFIG_AV1_ENCODER
// TODO(angiebird): 25-29 fail in high bitdepth mode.
// TODO(zoeliu): This ArfFreqTest does not work with BWDREF_FRAME, as
// BWDREF_FRAME is also a non-show frame, and the minimum run between two
// consecutive BWDREF_FRAME's may vary between 1 and any arbitrary positive
// number as long as it does not exceed the gf_group interval.
INSTANTIATE_TEST_SUITE_P(
    DISABLED_AV1, ArfFreqTestLarge,
    ::testing::Combine(
        ::testing::Values(
            static_cast<const libaom_test::CodecFactory *>(&libaom_test::kAV1)),
        ::testing::ValuesIn(kTestVectors), ::testing::ValuesIn(kEncodeVectors),
        ::testing::ValuesIn(kMinArfVectors)));
#endif  // CONFIG_AV1_ENCODER
}  // namespace
