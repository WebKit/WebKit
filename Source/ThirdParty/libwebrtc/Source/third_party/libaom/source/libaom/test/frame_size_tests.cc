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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/video_source.h"
#include "test/util.h"

namespace {

class AV1FrameSizeTests : public ::testing::Test,
                          public ::libaom_test::EncoderTest {
 protected:
  AV1FrameSizeTests()
      : EncoderTest(&::libaom_test::kAV1), expected_res_(AOM_CODEC_OK) {}
  virtual ~AV1FrameSizeTests() {}

  virtual void SetUp() { InitializeConfig(::libaom_test::kRealTime); }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(expected_res_, res_dec) << decoder->DecodeError();
    return !::testing::Test::HasFailure();
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 7);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    }
  }

  int expected_res_;
};

#if CONFIG_SIZE_LIMIT
TEST_F(AV1FrameSizeTests, TestInvalidSizes) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(DECODE_WIDTH_LIMIT + 16, DECODE_HEIGHT_LIMIT + 16);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_CORRUPT_FRAME;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

TEST_F(AV1FrameSizeTests, LargeValidSizes) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(DECODE_WIDTH_LIMIT, DECODE_HEIGHT_LIMIT);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#endif

TEST_F(AV1FrameSizeTests, OneByOneVideo) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(1, 1);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

#if !CONFIG_REALTIME_ONLY
typedef struct {
  unsigned int width;
  unsigned int height;
} FrameSizeParam;

const FrameSizeParam FrameSizeTestParams[] = { { 96, 96 }, { 176, 144 } };

// This unit test is used to validate the allocated size of compressed data
// (ctx->cx_data) buffer, by feeding pseudo random input to the encoder in
// lossless encoding mode.
//
// If compressed data buffer is not large enough, the av1_get_compressed_data()
// call in av1/av1_cx_iface.c will overflow the buffer.
class AV1LosslessFrameSizeTests
    : public ::libaom_test::CodecTestWith2Params<FrameSizeParam,
                                                 ::libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  AV1LosslessFrameSizeTests()
      : EncoderTest(GET_PARAM(0)), frame_size_param_(GET_PARAM(1)),
        encoding_mode_(GET_PARAM(2)) {}
  virtual ~AV1LosslessFrameSizeTests() {}

  virtual void SetUp() { InitializeConfig(encoding_mode_); }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(expected_res_, res_dec) << decoder->DecodeError();
    return !::testing::Test::HasFailure();
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 6);
      encoder->Control(AV1E_SET_LOSSLESS, 1);
    }
  }

  const FrameSizeParam frame_size_param_;
  const ::libaom_test::TestMode encoding_mode_;
  int expected_res_;
};

TEST_P(AV1LosslessFrameSizeTests, LosslessEncode) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(frame_size_param_.width, frame_size_param_.height);
  video.set_limit(10);
  expected_res_ = AOM_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_SUITE(AV1LosslessFrameSizeTests,
                           ::testing::ValuesIn(FrameSizeTestParams),
                           testing::Values(::libaom_test::kAllIntra));
#endif  // !CONFIG_REALTIME_ONLY

}  // namespace
