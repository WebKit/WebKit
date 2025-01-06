/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <memory>

#include "aom_ports/mem.h"
#include "aom_dsp/ssim.h"
#include "av1/common/blockd.h"
#include "gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/y4m_video_source.h"

namespace {

const unsigned int kFrames = 10;
const unsigned int kCqLevel = 18;
// List of ssim thresholds for speed settings 0-8 with all intra encoding mode.
const double kSsimThreshold[] = { 83.4, 83.4, 83.4, 83.3, 83.3,
                                  83.0, 82.3, 81.1, 81.1 };

typedef struct {
  const char *filename;
  unsigned int input_bit_depth;
  aom_img_fmt fmt;
  aom_bit_depth_t bit_depth;
  unsigned int profile;
} TestVideoParam;

std::ostream &operator<<(std::ostream &os, const TestVideoParam &test_arg) {
  return os << "TestVideoParam { filename:" << test_arg.filename
            << " input_bit_depth:" << test_arg.input_bit_depth
            << " fmt:" << test_arg.fmt << " bit_depth:" << test_arg.bit_depth
            << " profile:" << test_arg.profile << " }";
}

const TestVideoParam kTestVectors[] = {
  { "park_joy_90p_8_420.y4m", 8, AOM_IMG_FMT_I420, AOM_BITS_8, 0 },
  { "park_joy_90p_8_422.y4m", 8, AOM_IMG_FMT_I422, AOM_BITS_8, 2 },
  { "park_joy_90p_8_444.y4m", 8, AOM_IMG_FMT_I444, AOM_BITS_8, 1 },
#if CONFIG_AV1_HIGHBITDEPTH
  { "park_joy_90p_10_420.y4m", 10, AOM_IMG_FMT_I42016, AOM_BITS_10, 0 },
  { "park_joy_90p_10_422.y4m", 10, AOM_IMG_FMT_I42216, AOM_BITS_10, 2 },
  { "park_joy_90p_10_444.y4m", 10, AOM_IMG_FMT_I44416, AOM_BITS_10, 1 },
  { "park_joy_90p_12_420.y4m", 12, AOM_IMG_FMT_I42016, AOM_BITS_12, 2 },
  { "park_joy_90p_12_422.y4m", 12, AOM_IMG_FMT_I42216, AOM_BITS_12, 2 },
  { "park_joy_90p_12_444.y4m", 12, AOM_IMG_FMT_I44416, AOM_BITS_12, 2 },
#endif
};

// This class is used to check adherence to given ssim value, while using the
// "dist-metric=qm-psnr" option.
class EndToEndQMPSNRTest
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode,
                                                 TestVideoParam, int>,
      public ::libaom_test::EncoderTest {
 protected:
  EndToEndQMPSNRTest()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        test_video_param_(GET_PARAM(2)), cpu_used_(GET_PARAM(3)), nframes_(0),
        ssim_(0.0) {}

  ~EndToEndQMPSNRTest() override = default;

  void SetUp() override { InitializeConfig(encoding_mode_); }

  void BeginPassHook(unsigned int) override {
    nframes_ = 0;
    ssim_ = 0.0;
  }

  void CalculateFrameLevelSSIM(const aom_image_t *img_src,
                               const aom_image_t *img_enc,
                               aom_bit_depth_t bit_depth,
                               unsigned int input_bit_depth) override {
    double frame_ssim;
    double plane_ssim[MAX_MB_PLANE] = { 0.0, 0.0, 0.0 };
    int crop_widths[PLANE_TYPES];
    int crop_heights[PLANE_TYPES];
    crop_widths[PLANE_TYPE_Y] = img_src->d_w;
    crop_heights[PLANE_TYPE_Y] = img_src->d_h;
    // Width of UV planes calculated based on chroma_shift values.
    crop_widths[PLANE_TYPE_UV] =
        img_src->x_chroma_shift == 1 ? (img_src->w + 1) >> 1 : img_src->w;
    crop_heights[PLANE_TYPE_UV] =
        img_src->y_chroma_shift == 1 ? (img_src->h + 1) >> 1 : img_src->h;
    nframes_++;

#if CONFIG_AV1_HIGHBITDEPTH
    uint8_t is_hbd = bit_depth > AOM_BITS_8;
    if (is_hbd) {
      // HBD ssim calculation.
      uint8_t shift = bit_depth - input_bit_depth;
      for (int i = AOM_PLANE_Y; i < MAX_MB_PLANE; ++i) {
        const int is_uv = i > AOM_PLANE_Y;
        plane_ssim[i] = aom_highbd_ssim2(
            CONVERT_TO_BYTEPTR(img_src->planes[i]),
            CONVERT_TO_BYTEPTR(img_enc->planes[i]),
            img_src->stride[is_uv] >> is_hbd, img_enc->stride[is_uv] >> is_hbd,
            crop_widths[is_uv], crop_heights[is_uv], input_bit_depth, shift);
      }
      frame_ssim = plane_ssim[AOM_PLANE_Y] * .8 +
                   .1 * (plane_ssim[AOM_PLANE_U] + plane_ssim[AOM_PLANE_V]);
      // Accumulate to find sequence level ssim value.
      ssim_ += frame_ssim;
      return;
    }
#else
    (void)bit_depth;
    (void)input_bit_depth;
#endif  // CONFIG_AV1_HIGHBITDEPTH

    // LBD ssim calculation.
    for (int i = AOM_PLANE_Y; i < MAX_MB_PLANE; ++i) {
      const int is_uv = i > AOM_PLANE_Y;
      plane_ssim[i] = aom_ssim2(img_src->planes[i], img_enc->planes[i],
                                img_src->stride[is_uv], img_enc->stride[is_uv],
                                crop_widths[is_uv], crop_heights[is_uv]);
    }
    frame_ssim = plane_ssim[AOM_PLANE_Y] * .8 +
                 .1 * (plane_ssim[AOM_PLANE_U] + plane_ssim[AOM_PLANE_V]);
    // Accumulate to find sequence level ssim value.
    ssim_ += frame_ssim;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_FRAME_PARALLEL_DECODING, 1);
      encoder->Control(AV1E_SET_TILE_COLUMNS, 4);
      encoder->Control(AOME_SET_CPUUSED, cpu_used_);
      encoder->Control(AOME_SET_TUNING, AOM_TUNE_SSIM);
      encoder->Control(AOME_SET_CQ_LEVEL, kCqLevel);
      encoder->SetOption("dist-metric", "qm-psnr");
    }
  }

  double GetAverageSsim() const {
    if (nframes_) return 100 * pow(ssim_ / nframes_, 8.0);
    return 0.0;
  }

  double GetSsimThreshold() { return kSsimThreshold[cpu_used_]; }

  void DoTest() {
    cfg_.g_profile = test_video_param_.profile;
    cfg_.g_input_bit_depth = test_video_param_.input_bit_depth;
    cfg_.g_bit_depth = test_video_param_.bit_depth;
    if (cfg_.g_bit_depth > 8) init_flags_ |= AOM_CODEC_USE_HIGHBITDEPTH;

    std::unique_ptr<libaom_test::VideoSource> video(
        new libaom_test::Y4mVideoSource(test_video_param_.filename, 0,
                                        kFrames));
    ASSERT_NE(video, nullptr);
    ASSERT_NO_FATAL_FAILURE(RunLoop(video.get()));
    const double ssim = GetAverageSsim();
    EXPECT_GT(ssim, GetSsimThreshold())
        << "encoding mode = " << encoding_mode_ << ", cpu used = " << cpu_used_;
  }

 private:
  const libaom_test::TestMode encoding_mode_;
  const TestVideoParam test_video_param_;
  const int cpu_used_;
  unsigned int nframes_;
  double ssim_;
};

class EndToEndQMPSNRTestLarge : public EndToEndQMPSNRTest {};

TEST_P(EndToEndQMPSNRTestLarge, EndtoEndQMPSNRTest) { DoTest(); }

TEST_P(EndToEndQMPSNRTest, EndtoEndQMPSNRTest) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(EndToEndQMPSNRTestLarge,
                           ::testing::Values(::libaom_test::kAllIntra),
                           ::testing::ValuesIn(kTestVectors),
                           ::testing::Values(2, 4, 6, 8));  // cpu_used

AV1_INSTANTIATE_TEST_SUITE(EndToEndQMPSNRTest,
                           ::testing::Values(::libaom_test::kAllIntra),
                           ::testing::Values(kTestVectors[0]),  // 420
                           ::testing::Values(6));               // cpu_used
}  // namespace
