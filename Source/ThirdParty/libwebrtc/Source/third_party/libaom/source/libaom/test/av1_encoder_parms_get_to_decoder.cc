/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
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

#include "aom/aom_decoder.h"
#include "av1/decoder/decoder.h"

namespace {

const int kMaxPsnr = 100;

struct ParamPassingTestVideo {
  const char *name;
  uint32_t width;
  uint32_t height;
  uint32_t bitrate;
  int frames;
};

const ParamPassingTestVideo kAV1ParamPassingTestVector = {
  "niklas_1280_720_30.y4m", 1280, 720, 600, 3
};

struct EncodeParameters {
  int32_t lossless;
  aom_color_primaries_t color_primaries;
  aom_transfer_characteristics_t transfer_characteristics;
  aom_matrix_coefficients_t matrix_coefficients;
  aom_color_range_t color_range;
  aom_chroma_sample_position_t chroma_sample_position;
  int32_t render_size[2];
};

const EncodeParameters kAV1EncodeParameterSet[] = {
  { 1,
    AOM_CICP_CP_BT_709,
    AOM_CICP_TC_BT_709,
    AOM_CICP_MC_BT_709,
    AOM_CR_STUDIO_RANGE,
    AOM_CSP_UNKNOWN,
    { 0, 0 } },
  { 0,
    AOM_CICP_CP_BT_470_M,
    AOM_CICP_TC_BT_470_M,
    AOM_CICP_MC_BT_470_B_G,
    AOM_CR_FULL_RANGE,
    AOM_CSP_VERTICAL,
    { 0, 0 } },
  { 1,
    AOM_CICP_CP_BT_601,
    AOM_CICP_TC_BT_601,
    AOM_CICP_MC_BT_601,
    AOM_CR_STUDIO_RANGE,
    AOM_CSP_COLOCATED,
    { 0, 0 } },
  { 0,
    AOM_CICP_CP_BT_2020,
    AOM_CICP_TC_BT_2020_10_BIT,
    AOM_CICP_MC_BT_2020_NCL,
    AOM_CR_FULL_RANGE,
    AOM_CSP_RESERVED,
    { 640, 480 } },
};

class AVxEncoderParmsGetToDecoder
    : public ::libaom_test::EncoderTest,
      public ::libaom_test::CodecTestWithParam<EncodeParameters> {
 protected:
  AVxEncoderParmsGetToDecoder()
      : EncoderTest(GET_PARAM(0)), encode_parms(GET_PARAM(1)) {}

  virtual ~AVxEncoderParmsGetToDecoder() {}

  virtual void SetUp() {
    InitializeConfig(::libaom_test::kTwoPassGood);
    cfg_.g_lag_in_frames = 25;
    test_video_ = kAV1ParamPassingTestVector;
    cfg_.rc_target_bitrate = test_video_.bitrate;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 3);
      encoder->Control(AV1E_SET_COLOR_PRIMARIES, encode_parms.color_primaries);
      encoder->Control(AV1E_SET_TRANSFER_CHARACTERISTICS,
                       encode_parms.transfer_characteristics);
      encoder->Control(AV1E_SET_MATRIX_COEFFICIENTS,
                       encode_parms.matrix_coefficients);
      encoder->Control(AV1E_SET_COLOR_RANGE, encode_parms.color_range);
      encoder->Control(AV1E_SET_CHROMA_SAMPLE_POSITION,
                       encode_parms.chroma_sample_position);
      encoder->Control(AV1E_SET_LOSSLESS, encode_parms.lossless);
      if (encode_parms.render_size[0] > 0 && encode_parms.render_size[1] > 0) {
        encoder->Control(AV1E_SET_RENDER_SIZE, encode_parms.render_size);
      }
    }
  }

  virtual void DecompressedFrameHook(const aom_image_t &img,
                                     aom_codec_pts_t pts) {
    (void)pts;
    if (encode_parms.render_size[0] > 0 && encode_parms.render_size[1] > 0) {
      EXPECT_EQ(encode_parms.render_size[0], (int)img.r_w);
      EXPECT_EQ(encode_parms.render_size[1], (int)img.r_h);
    }
    EXPECT_EQ(encode_parms.color_primaries, img.cp);
    EXPECT_EQ(encode_parms.transfer_characteristics, img.tc);
    EXPECT_EQ(encode_parms.matrix_coefficients, img.mc);
    EXPECT_EQ(encode_parms.color_range, img.range);
    EXPECT_EQ(encode_parms.chroma_sample_position, img.csp);
  }

  virtual void PSNRPktHook(const aom_codec_cx_pkt_t *pkt) {
    if (encode_parms.lossless) {
      EXPECT_EQ(kMaxPsnr, pkt->data.psnr.psnr[0]);
    }
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    return AOM_CODEC_OK == res_dec;
  }

  ParamPassingTestVideo test_video_;

 private:
  EncodeParameters encode_parms;
};

TEST_P(AVxEncoderParmsGetToDecoder, BitstreamParms) {
  init_flags_ = AOM_CODEC_USE_PSNR;

  std::unique_ptr<libaom_test::VideoSource> video(
      new libaom_test::Y4mVideoSource(test_video_.name, 0, test_video_.frames));
  ASSERT_NE(video, nullptr);

  ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
}

AV1_INSTANTIATE_TEST_SUITE(AVxEncoderParmsGetToDecoder,
                           ::testing::ValuesIn(kAV1EncodeParameterSet));
}  // namespace
