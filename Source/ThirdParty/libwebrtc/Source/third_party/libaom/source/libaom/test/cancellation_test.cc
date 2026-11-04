/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <memory>

#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {

class CancellableVideoSource : public ::libaom_test::VideoSource {
 public:
  explicit CancellableVideoSource(::libaom_test::VideoSource *source)
      : source_(source), cancel_after_(-1), current_pass_(0) {}

  void Begin() override {
    source_->Begin();
    current_pass_++;
  }

  void Next() override { source_->Next(); }

  aom_image_t *img() const override {
    if (current_pass_ == 2 && cancel_after_ >= 0 &&
        static_cast<int>(source_->frame()) >= cancel_after_) {
      return nullptr;
    }
    return source_->img();
  }

  aom_codec_pts_t pts() const override { return source_->pts(); }
  unsigned long duration() const override { return source_->duration(); }
  aom_rational_t timebase() const override { return source_->timebase(); }
  unsigned int frame() const override { return source_->frame(); }
  unsigned int limit() const override { return source_->limit(); }

  void set_cancel_after(int frame) { cancel_after_ = frame; }

 private:
  ::libaom_test::VideoSource *source_;
  int cancel_after_;
  int current_pass_;
};

class CancellationTest
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWithParam< ::libaom_test::TestMode> {
 protected:
  CancellationTest()
      : EncoderTest(GET_PARAM(0)), tile_columns_(0), tile_rows_(0) {}
  ~CancellationTest() override = default;

  void SetUp() override { InitializeConfig(GET_PARAM(1)); }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 4);  // speed 4
      encoder->Control(AV1E_SET_TILE_COLUMNS, tile_columns_);
      encoder->Control(AV1E_SET_TILE_ROWS, tile_rows_);
    }
  }

  void DoSimulateCancellationPass2(int threads, int tile_columns,
                                   int tile_rows) {
    cfg_.g_threads = threads;
    tile_columns_ = tile_columns;
    tile_rows_ = tile_rows;

    ::libaom_test::RandomVideoSource raw_video;
    raw_video.SetSize(256, 256);
    raw_video.set_limit(40);  // 40 frames total

    CancellableVideoSource video(&raw_video);
    video.set_cancel_after(20);  // Cancel after 20 frames in pass 2

    // This should not crash
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

 private:
  int tile_columns_;
  int tile_rows_;
};

TEST_P(CancellationTest, SimulateCancellationPass2) {
  DoSimulateCancellationPass2(/*threads=*/1, /*tile_columns=*/0,
                              /*tile_rows=*/0);
}

TEST_P(CancellationTest, SimulateCancellationPass2Threads4) {
  DoSimulateCancellationPass2(/*threads=*/4, /*tile_columns=*/1,
                              /*tile_rows=*/1);
}

// Test 2-pass good quality
AV1_INSTANTIATE_TEST_SUITE(CancellationTest,
                           ::testing::Values(::libaom_test::kTwoPassGood));

}  // namespace
