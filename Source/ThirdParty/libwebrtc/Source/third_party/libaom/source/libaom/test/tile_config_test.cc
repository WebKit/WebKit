/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom/aom_codec.h"
#include "aom_dsp/aom_dsp_common.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/y4m_video_source.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {
typedef struct {
  // Superblock size
  const unsigned int sb_size;
  // log2(number of tile rows)
  const unsigned int tile_rows;
  // log2(number of tile columns)
  const unsigned int tile_cols;
} uniformTileConfigParam;

const libaom_test::TestMode kTestModeParams[] =
#if CONFIG_REALTIME_ONLY
    { ::libaom_test::kRealTime };
#else
    { ::libaom_test::kRealTime, ::libaom_test::kOnePassGood,
      ::libaom_test::kTwoPassGood };
#endif

static const uniformTileConfigParam uniformTileConfigParams[] = {
  { 128, 0, 0 }, { 128, 0, 2 }, { 128, 2, 0 }, { 128, 1, 2 }, { 128, 2, 2 },
  { 128, 3, 2 }, { 64, 0, 0 },  { 64, 0, 2 },  { 64, 2, 0 },  { 64, 1, 2 },
  { 64, 2, 2 },  { 64, 3, 3 },  { 64, 4, 4 }
};

typedef struct {
  // Superblock size
  const unsigned int sb_size;
  // number of tile widths
  const unsigned int tile_width_count;
  // list of tile widths
  int tile_widths[AOM_MAX_TILE_COLS];
  // number of tile heights
  const unsigned int tile_height_count;
  // list of tile heights
  int tile_heights[AOM_MAX_TILE_ROWS];
} nonUniformTileConfigParam;

const nonUniformTileConfigParam nonUniformTileConfigParams[] = {
  { 64, 1, { 3 }, 1, { 3 } },          { 64, 2, { 1, 2 }, 2, { 1, 2 } },
  { 64, 3, { 2, 3, 4 }, 2, { 2, 3 } }, { 128, 1, { 3 }, 1, { 3 } },
  { 128, 2, { 1, 2 }, 2, { 1, 2 } },   { 128, 3, { 2, 3, 4 }, 2, { 2, 3 } },
};

// Find smallest k>=0 such that (blk_size << k) >= target
static INLINE int tile_log2(int blk_size, int target) {
  int k;
  for (k = 0; (blk_size << k) < target; k++) {
  }
  return k;
}

// This class is used to validate tile configuration for uniform spacing.
class UniformTileConfigTestLarge
    : public ::libaom_test::CodecTestWith3Params<
          libaom_test::TestMode, uniformTileConfigParam, aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  UniformTileConfigTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        tile_config_param_(GET_PARAM(2)), end_usage_check_(GET_PARAM(3)) {
    tile_config_violated_ = false;
    max_tile_cols_log2_ = tile_log2(1, AOM_MAX_TILE_COLS);
    max_tile_rows_log2_ = tile_log2(1, AOM_MAX_TILE_ROWS);
  }
  ~UniformTileConfigTestLarge() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = end_usage_check_;
    cfg_.g_threads = 1;
    cfg_.g_lag_in_frames = 19;
  }

  bool DoDecode() const override { return true; }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AV1E_SET_TILE_COLUMNS, tile_config_param_.tile_cols);
      encoder->Control(AV1E_SET_TILE_ROWS, tile_config_param_.tile_rows);
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AV1E_SET_SUPERBLOCK_SIZE,
                       tile_config_param_.sb_size == 64
                           ? AOM_SUPERBLOCK_SIZE_64X64
                           : AOM_SUPERBLOCK_SIZE_128X128);
    }
  }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      aom_tile_info tile_info;
      int config_tile_columns = AOMMIN(1 << (int)tile_config_param_.tile_cols,
                                       1 << max_tile_cols_log2_);
      int config_tile_rows = AOMMIN(1 << (int)tile_config_param_.tile_rows,
                                    1 << max_tile_rows_log2_);

      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_TILE_INFO, &tile_info);
      if (tile_info.tile_columns != config_tile_columns ||
          tile_info.tile_rows != config_tile_rows) {
        tile_config_violated_ = true;
      }
    }
    return AOM_CODEC_OK == res_dec;
  }

  ::libaom_test::TestMode encoding_mode_;
  const uniformTileConfigParam tile_config_param_;
  int max_tile_cols_log2_;
  int max_tile_rows_log2_;
  bool tile_config_violated_;
  aom_rc_mode end_usage_check_;
};

// This class is used to validate tile configuration for non uniform spacing.
class NonUniformTileConfigTestLarge
    : public ::libaom_test::CodecTestWith3Params<
          libaom_test::TestMode, nonUniformTileConfigParam, aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  NonUniformTileConfigTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        tile_config_param_(GET_PARAM(2)), rc_end_usage_(GET_PARAM(3)) {
    tile_config_violated_ = false;
  }
  ~NonUniformTileConfigTestLarge() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = rc_end_usage_;
    cfg_.g_threads = 1;
    cfg_.g_lag_in_frames = 35;
    cfg_.rc_target_bitrate = 1000;
    cfg_.tile_width_count = tile_config_param_.tile_width_count;
    memcpy(cfg_.tile_widths, tile_config_param_.tile_widths,
           sizeof(tile_config_param_.tile_widths[0]) *
               tile_config_param_.tile_width_count);
    cfg_.tile_height_count = tile_config_param_.tile_height_count;
    memcpy(cfg_.tile_heights, tile_config_param_.tile_heights,
           sizeof(tile_config_param_.tile_heights[0]) *
               tile_config_param_.tile_height_count);
  }

  bool DoDecode() const override { return true; }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AV1E_SET_SUPERBLOCK_SIZE,
                       tile_config_param_.sb_size == 64
                           ? AOM_SUPERBLOCK_SIZE_64X64
                           : AOM_SUPERBLOCK_SIZE_128X128);
    }
  }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      aom_tile_info tile_info;
      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_TILE_INFO, &tile_info);

      // check validity of tile cols
      int tile_col_idx, tile_col = 0;
      for (tile_col_idx = 0; tile_col_idx < tile_info.tile_columns - 1;
           tile_col_idx++) {
        if (tile_config_param_.tile_widths[tile_col] !=
            tile_info.tile_widths[tile_col_idx])
          tile_config_violated_ = true;
        tile_col = (tile_col + 1) % (int)tile_config_param_.tile_width_count;
      }
      // last column may not be able to accommodate config, but if it is
      // greater than what is configured, there is a violation.
      if (tile_config_param_.tile_widths[tile_col] <
          tile_info.tile_widths[tile_col_idx])
        tile_config_violated_ = true;

      // check validity of tile rows
      int tile_row_idx, tile_row = 0;
      for (tile_row_idx = 0; tile_row_idx < tile_info.tile_rows - 1;
           tile_row_idx++) {
        if (tile_config_param_.tile_heights[tile_row] !=
            tile_info.tile_heights[tile_row_idx])
          tile_config_violated_ = true;
        tile_row = (tile_row + 1) % (int)tile_config_param_.tile_height_count;
      }
      // last row may not be able to accommodate config, but if it is
      // greater than what is configured, there is a violation.
      if (tile_config_param_.tile_heights[tile_row] <
          tile_info.tile_heights[tile_row_idx])
        tile_config_violated_ = true;
    }
    return AOM_CODEC_OK == res_dec;
  }

  ::libaom_test::TestMode encoding_mode_;
  const nonUniformTileConfigParam tile_config_param_;
  bool tile_config_violated_;
  aom_rc_mode rc_end_usage_;
};

TEST_P(UniformTileConfigTestLarge, UniformTileConfigTest) {
  ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 1);
  ASSERT_NO_FATAL_FAILURE(video.Begin());

  int max_tiles_cols = video.img()->w / (int)tile_config_param_.sb_size;
  int max_tiles_rows = video.img()->h / (int)tile_config_param_.sb_size;
  max_tile_cols_log2_ = tile_log2(1, AOMMIN(max_tiles_cols, AOM_MAX_TILE_COLS));
  max_tile_rows_log2_ = tile_log2(1, AOMMIN(max_tiles_rows, AOM_MAX_TILE_ROWS));

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(tile_config_violated_, false);
}

TEST_P(UniformTileConfigTestLarge, UniformTileConfigTestLowRes) {
  ::libaom_test::Y4mVideoSource video("screendata.y4m", 0, 1);
  ASSERT_NO_FATAL_FAILURE(video.Begin());

  int max_tiles_cols = video.img()->w / (int)tile_config_param_.sb_size;
  int max_tiles_rows = video.img()->h / (int)tile_config_param_.sb_size;
  max_tile_cols_log2_ = tile_log2(1, AOMMIN(max_tiles_cols, AOM_MAX_TILE_COLS));
  max_tile_rows_log2_ = tile_log2(1, AOMMIN(max_tiles_rows, AOM_MAX_TILE_ROWS));

  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(tile_config_violated_, false);
}

TEST_P(NonUniformTileConfigTestLarge, NonUniformTileConfigTest) {
  ::libaom_test::Y4mVideoSource video("niklas_1280_720_30.y4m", 0, 1);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(tile_config_violated_, false);
}

AV1_INSTANTIATE_TEST_SUITE(UniformTileConfigTestLarge,
                           ::testing::ValuesIn(kTestModeParams),
                           ::testing::ValuesIn(uniformTileConfigParams),
                           ::testing::Values(AOM_Q, AOM_VBR, AOM_CBR, AOM_CQ));

AV1_INSTANTIATE_TEST_SUITE(NonUniformTileConfigTestLarge,
                           ::testing::ValuesIn(kTestModeParams),
                           ::testing::ValuesIn(nonUniformTileConfigParams),
                           ::testing::Values(AOM_Q, AOM_VBR, AOM_CBR, AOM_CQ));

typedef struct {
  // Number of tile groups to set.
  const int num_tg;
  // Number of tile rows to set
  const int num_tile_rows;
  // Number of tile columns to set
  const int num_tile_cols;
} TileGroupConfigParams;

static const TileGroupConfigParams tileGroupTestParams[] = {
  { 5, 4, 4 }, { 3, 3, 3 }, { 5, 3, 3 }, { 7, 5, 5 }, { 7, 3, 3 }, { 7, 4, 4 }
};

std::ostream &operator<<(std::ostream &os,
                         const TileGroupConfigParams &test_arg) {
  return os << "TileGroupConfigParams { num_tg:" << test_arg.num_tg
            << " num_tile_rows:" << test_arg.num_tile_rows
            << " num_tile_cols:" << test_arg.num_tile_cols << " }";
}

// This class is used to test number of tile groups present in header.
class TileGroupTestLarge
    : public ::libaom_test::CodecTestWith2Params<libaom_test::TestMode,
                                                 TileGroupConfigParams>,
      public ::libaom_test::EncoderTest {
 protected:
  TileGroupTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        tile_group_config_params_(GET_PARAM(2)) {
    tile_group_config_violated_ = false;
  }
  ~TileGroupTestLarge() override = default;

  void SetUp() override {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = AOM_Q;
    cfg_.g_threads = 1;
  }

  bool DoDecode() const override { return true; }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AV1E_SET_NUM_TG, tile_group_config_params_.num_tg);
      encoder->Control(AV1E_SET_TILE_COLUMNS,
                       tile_group_config_params_.num_tile_cols);
      encoder->Control(AV1E_SET_TILE_ROWS,
                       tile_group_config_params_.num_tile_rows);
    }
  }

  bool HandleDecodeResult(const aom_codec_err_t res_dec,
                          libaom_test::Decoder *decoder) override {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      aom_tile_info tile_info;
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_TILE_INFO, &tile_info);
      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_SHOW_EXISTING_FRAME_FLAG,
                                    &show_existing_frame_);
      if (tile_info.num_tile_groups != tile_group_config_params_.num_tg &&
          !show_existing_frame_)
        tile_group_config_violated_ = true;
      EXPECT_EQ(tile_group_config_violated_, false);
    }
    return AOM_CODEC_OK == res_dec;
  }

  int show_existing_frame_;
  bool tile_group_config_violated_;
  aom_rc_mode end_usage_check_;
  ::libaom_test::TestMode encoding_mode_;
  const TileGroupConfigParams tile_group_config_params_;
};

TEST_P(TileGroupTestLarge, TileGroupCountTest) {
  libaom_test::I420VideoSource video("niklas_640_480_30.yuv", 640, 480,
                                     cfg_.g_timebase.den, cfg_.g_timebase.num,
                                     0, 5);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

AV1_INSTANTIATE_TEST_SUITE(TileGroupTestLarge,
                           ::testing::ValuesIn(kTestModeParams),
                           ::testing::ValuesIn(tileGroupTestParams));
}  // namespace
