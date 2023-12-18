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
#include <string>
#include <utility>
#include <vector>

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/md5_helper.h"
#include "test/util.h"
#include "test/yuv_video_source.h"

namespace {

class PostprocFiltersTest
    : public ::libaom_test::CodecTestWith2Params<int, unsigned int>,
      public ::libaom_test::EncoderTest {
 protected:
  PostprocFiltersTest()
      : EncoderTest(GET_PARAM(0)), set_skip_postproc_filtering_(false),
        frame_number_(0), cpu_used_(GET_PARAM(1)), bd_(GET_PARAM(2)) {}

  void SetUp() override {
    InitializeConfig(::libaom_test::kAllIntra);
    cfg_.g_input_bit_depth = bd_;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    frame_number_ = video->frame();
    if (frame_number_ == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AOME_SET_CQ_LEVEL, kCqLevel);
    }
    if (set_skip_postproc_filtering_) {
      if (frame_number_ == 0) {
        encoder->Control(AV1E_SET_SKIP_POSTPROC_FILTERING, 1);
      } else if (frame_number_ == 10) {
        encoder->Control(AV1E_SET_SKIP_POSTPROC_FILTERING, 0);
      } else if (frame_number_ == 20) {
        encoder->Control(AV1E_SET_SKIP_POSTPROC_FILTERING, 1);
      }
    }
  }

  void FramePktHook(const aom_codec_cx_pkt_t *pkt) override {
    ::libaom_test::MD5 md5_enc;
    md5_enc.Add(reinterpret_cast<uint8_t *>(pkt->data.frame.buf),
                pkt->data.frame.sz);
    md5_enc_.push_back(md5_enc.Get());
  }

  void PostEncodeFrameHook(::libaom_test::Encoder *encoder) override {
    const aom_image_t *img_enc = encoder->GetPreviewFrame();
    if (!set_skip_postproc_filtering_) {
      ASSERT_NE(img_enc, nullptr);
    } else {
      // Null will be returned if we query the reconstructed frame when
      // AV1E_SET_SKIP_POSTPROC_FILTERING is set to 1.
      if (frame_number_ < 10) {
        ASSERT_EQ(img_enc, nullptr);
      } else if (frame_number_ < 20) {
        // Reconstructed frame cannot be null when
        // AV1E_SET_SKIP_POSTPROC_FILTERING is set to 0.
        ASSERT_NE(img_enc, nullptr);
      } else {
        ASSERT_EQ(img_enc, nullptr);
      }
    }
  }

  // The encoder config flag 'AV1E_SET_SKIP_POSTPROC_FILTERING' can be used to
  // skip the application of post-processing filters on reconstructed frame for
  // ALLINTRA encode. This unit-test validates the bit exactness of 2 encoded
  // streams with 'AV1E_SET_SKIP_POSTPROC_FILTERING':
  // 1. disabled for all frames (default case)
  // 2. enabled and disabled at different frame indices using control calls.
  void DoTest() {
    std::unique_ptr<libaom_test::VideoSource> video(
        new libaom_test::YUVVideoSource("niklas_640_480_30.yuv",
                                        AOM_IMG_FMT_I420, 640, 480, 30, 1, 0,
                                        kFrames));
    ASSERT_NE(video, nullptr);

    // First encode: 'AV1E_SET_SKIP_POSTPROC_FILTERING' disabled for all frames
    // (default case).
    set_skip_postproc_filtering_ = false;
    ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
    std::vector<std::string> apply_postproc_filters_md5_enc =
        std::move(md5_enc_);
    md5_enc_.clear();

    // Second encode: 'AV1E_SET_SKIP_POSTPROC_FILTERING' enabled and disabled at
    // different frame intervals.
    set_skip_postproc_filtering_ = true;
    ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
    std::vector<std::string> toggle_apply_postproc_filters_md5_enc =
        std::move(md5_enc_);
    md5_enc_.clear();

    // Check for bit match.
    ASSERT_EQ(apply_postproc_filters_md5_enc,
              toggle_apply_postproc_filters_md5_enc);
  }

  bool set_skip_postproc_filtering_;
  unsigned int frame_number_;
  std::vector<std::string> md5_enc_;

 private:
  static constexpr int kFrames = 30;
  static constexpr unsigned int kCqLevel = 18;
  int cpu_used_;
  unsigned int bd_;
};

class PostprocFiltersTestLarge : public PostprocFiltersTest {};

TEST_P(PostprocFiltersTest, MD5Match) { DoTest(); }

TEST_P(PostprocFiltersTestLarge, MD5Match) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(PostprocFiltersTest, ::testing::Values(9),
                           ::testing::Values(8, 10));

// Test cpu_used 3 and 6.
AV1_INSTANTIATE_TEST_SUITE(PostprocFiltersTestLarge, ::testing::Values(3, 6),
                           ::testing::Values(8, 10));

}  // namespace
