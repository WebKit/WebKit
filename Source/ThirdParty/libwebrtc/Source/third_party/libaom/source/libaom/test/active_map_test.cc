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

#include <climits>
#include <vector>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {

class ActiveMapTest
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode, int>,
      public ::libaom_test::EncoderTest {
 protected:
  static const int kWidth = 208;
  static const int kHeight = 144;

  ActiveMapTest() : EncoderTest(GET_PARAM(0)) {}
  ~ActiveMapTest() override = default;

  void SetUp() override {
    InitializeConfig(GET_PARAM(1));
    cpu_used_ = GET_PARAM(2);
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AV1E_SET_ALLOW_WARPED_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_GLOBAL_MOTION, 0);
      encoder->Control(AV1E_SET_ENABLE_OBMC, 0);
    } else if (video->frame() == 3) {
      aom_active_map_t map = aom_active_map_t();
      /* clang-format off */
      uint8_t active_map[9 * 13] = {
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0,
        0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1,
        0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1,
        0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1,
        1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0,
      };
      /* clang-format on */
      map.cols = (kWidth + 15) / 16;
      map.rows = (kHeight + 15) / 16;
      ASSERT_EQ(map.cols, 13u);
      ASSERT_EQ(map.rows, 9u);
      map.active_map = active_map;
      encoder->Control(AOME_SET_ACTIVEMAP, &map);
    } else if (video->frame() == 15) {
      aom_active_map_t map = aom_active_map_t();
      map.cols = (kWidth + 15) / 16;
      map.rows = (kHeight + 15) / 16;
      map.active_map = nullptr;
      encoder->Control(AOME_SET_ACTIVEMAP, &map);
    }
  }

  void DoTest() {
    // Validate that this non multiple of 64 wide clip encodes
    cfg_.g_lag_in_frames = 0;
    cfg_.rc_target_bitrate = 400;
    cfg_.rc_resize_mode = 0;
    cfg_.g_pass = AOM_RC_ONE_PASS;
    cfg_.rc_end_usage = AOM_CBR;
    cfg_.kf_max_dist = 90000;
    ::libaom_test::I420VideoSource video("hantro_odd.yuv", kWidth, kHeight, 30,
                                         1, 0, 20);

    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

  int cpu_used_;
};

TEST_P(ActiveMapTest, Test) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(ActiveMapTest,
                           ::testing::Values(::libaom_test::kRealTime),
                           ::testing::Range(5, 9));

}  // namespace
