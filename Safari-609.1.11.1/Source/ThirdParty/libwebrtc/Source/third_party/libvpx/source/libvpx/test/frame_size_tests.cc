/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/video_source.h"

namespace {

class VP9FrameSizeTestsLarge : public ::libvpx_test::EncoderTest,
                               public ::testing::Test {
 protected:
  VP9FrameSizeTestsLarge()
      : EncoderTest(&::libvpx_test::kVP9), expected_res_(VPX_CODEC_OK) {}
  virtual ~VP9FrameSizeTestsLarge() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(::libvpx_test::kRealTime);
  }

  virtual bool HandleDecodeResult(const vpx_codec_err_t res_dec,
                                  const libvpx_test::VideoSource & /*video*/,
                                  libvpx_test::Decoder *decoder) {
    EXPECT_EQ(expected_res_, res_dec) << decoder->DecodeError();
    return !::testing::Test::HasFailure();
  }

  virtual void PreEncodeFrameHook(::libvpx_test::VideoSource *video,
                                  ::libvpx_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(VP8E_SET_CPUUSED, 7);
      encoder->Control(VP8E_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(VP8E_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(VP8E_SET_ARNR_STRENGTH, 5);
      encoder->Control(VP8E_SET_ARNR_TYPE, 3);
    }
  }

  int expected_res_;
};

TEST_F(VP9FrameSizeTestsLarge, TestInvalidSizes) {
  ::libvpx_test::RandomVideoSource video;

#if CONFIG_SIZE_LIMIT
  video.SetSize(DECODE_WIDTH_LIMIT + 16, DECODE_HEIGHT_LIMIT + 16);
  video.set_limit(2);
  expected_res_ = VPX_CODEC_CORRUPT_FRAME;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#endif
}

TEST_F(VP9FrameSizeTestsLarge, ValidSizes) {
  ::libvpx_test::RandomVideoSource video;

#if CONFIG_SIZE_LIMIT
  video.SetSize(DECODE_WIDTH_LIMIT, DECODE_HEIGHT_LIMIT);
  video.set_limit(2);
  expected_res_ = VPX_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#else
// This test produces a pretty large single frame allocation,  (roughly
// 25 megabits). The encoder allocates a good number of these frames
// one for each lag in frames (for 2 pass), and then one for each possible
// reference buffer (8) - we can end up with up to 30 buffers of roughly this
// size or almost 1 gig of memory.
// In total the allocations will exceed 2GiB which may cause a failure with
// mingw + wine, use a smaller size in that case.
#if defined(_WIN32) && !defined(_WIN64) || defined(__OS2__)
  video.SetSize(4096, 3072);
#else
  video.SetSize(4096, 4096);
#endif
  video.set_limit(2);
  expected_res_ = VPX_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
#endif
}

TEST_F(VP9FrameSizeTestsLarge, OneByOneVideo) {
  ::libvpx_test::RandomVideoSource video;

  video.SetSize(1, 1);
  video.set_limit(2);
  expected_res_ = VPX_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
}  // namespace
