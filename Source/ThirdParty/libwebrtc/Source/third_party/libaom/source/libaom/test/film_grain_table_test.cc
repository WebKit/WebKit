/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <string>
#include "gtest/gtest.h"
#include "aom_dsp/grain_table.h"
#include "aom/internal/aom_codec_internal.h"
#include "av1/encoder/grain_test_vectors.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"
#include "test/video_source.h"

namespace {

void grain_equal(const aom_film_grain_t *expected,
                 const aom_film_grain_t *actual) {
  EXPECT_EQ(expected->apply_grain, actual->apply_grain);
  EXPECT_EQ(expected->update_parameters, actual->update_parameters);
  if (!expected->update_parameters) return;
  EXPECT_EQ(expected->num_y_points, actual->num_y_points);
  EXPECT_EQ(expected->num_cb_points, actual->num_cb_points);
  EXPECT_EQ(expected->num_cr_points, actual->num_cr_points);
  EXPECT_EQ(0, memcmp(expected->scaling_points_y, actual->scaling_points_y,
                      expected->num_y_points *
                          sizeof(expected->scaling_points_y[0])));
  EXPECT_EQ(0, memcmp(expected->scaling_points_cb, actual->scaling_points_cb,
                      expected->num_cb_points *
                          sizeof(expected->scaling_points_cb[0])));
  EXPECT_EQ(0, memcmp(expected->scaling_points_cr, actual->scaling_points_cr,
                      expected->num_cr_points *
                          sizeof(expected->scaling_points_cr[0])));
  EXPECT_EQ(expected->scaling_shift, actual->scaling_shift);
  EXPECT_EQ(expected->ar_coeff_lag, actual->ar_coeff_lag);
  EXPECT_EQ(expected->ar_coeff_shift, actual->ar_coeff_shift);

  const int num_pos_luma =
      2 * expected->ar_coeff_lag * (expected->ar_coeff_lag + 1);
  const int num_pos_chroma = num_pos_luma;
  EXPECT_EQ(0, memcmp(expected->ar_coeffs_y, actual->ar_coeffs_y,
                      sizeof(expected->ar_coeffs_y[0]) * num_pos_luma));
  if (actual->num_cb_points || actual->chroma_scaling_from_luma) {
    EXPECT_EQ(0, memcmp(expected->ar_coeffs_cb, actual->ar_coeffs_cb,
                        sizeof(expected->ar_coeffs_cb[0]) * num_pos_chroma));
  }
  if (actual->num_cr_points || actual->chroma_scaling_from_luma) {
    EXPECT_EQ(0, memcmp(expected->ar_coeffs_cr, actual->ar_coeffs_cr,
                        sizeof(expected->ar_coeffs_cr[0]) * num_pos_chroma));
  }
  EXPECT_EQ(expected->overlap_flag, actual->overlap_flag);
  EXPECT_EQ(expected->chroma_scaling_from_luma,
            actual->chroma_scaling_from_luma);
  EXPECT_EQ(expected->grain_scale_shift, actual->grain_scale_shift);
  // EXPECT_EQ(expected->random_seed, actual->random_seed);

  // clip_to_restricted and bit_depth aren't written
  if (expected->num_cb_points) {
    EXPECT_EQ(expected->cb_mult, actual->cb_mult);
    EXPECT_EQ(expected->cb_luma_mult, actual->cb_luma_mult);
    EXPECT_EQ(expected->cb_offset, actual->cb_offset);
  }
  if (expected->num_cr_points) {
    EXPECT_EQ(expected->cr_mult, actual->cr_mult);
    EXPECT_EQ(expected->cr_luma_mult, actual->cr_luma_mult);
    EXPECT_EQ(expected->cr_offset, actual->cr_offset);
  }
}

}  // namespace

TEST(FilmGrainTableTest, AddAndLookupSingleSegment) {
  aom_film_grain_table_t table;
  memset(&table, 0, sizeof(table));

  aom_film_grain_t grain;
  EXPECT_FALSE(aom_film_grain_table_lookup(&table, 0, 1000, false, &grain));

  aom_film_grain_table_append(&table, 1000, 2000, film_grain_test_vectors + 0);
  EXPECT_FALSE(aom_film_grain_table_lookup(&table, 0, 1000, false, &grain));
  EXPECT_FALSE(aom_film_grain_table_lookup(&table, 2000, 3000, false, &grain));

  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 1000, 2000, false, &grain));

  grain.bit_depth = film_grain_test_vectors[0].bit_depth;
  EXPECT_EQ(0, memcmp(&grain, film_grain_test_vectors + 0, sizeof(table)));

  // Extend the existing segment
  aom_film_grain_table_append(&table, 2000, 3000, film_grain_test_vectors + 0);
  EXPECT_EQ(nullptr, table.head->next);

  // Lookup and remove and check that the entry is no longer there
  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 1000, 2000, true, &grain));
  EXPECT_FALSE(aom_film_grain_table_lookup(&table, 1000, 2000, false, &grain));

  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 2000, 3000, true, &grain));
  EXPECT_FALSE(aom_film_grain_table_lookup(&table, 2000, 3000, false, &grain));

  EXPECT_EQ(nullptr, table.head);
  EXPECT_EQ(nullptr, table.tail);
  aom_film_grain_table_free(&table);
}

TEST(FilmGrainTableTest, AddSingleSegmentRemoveBiggerSegment) {
  aom_film_grain_table_t table;
  aom_film_grain_t grain;

  memset(&table, 0, sizeof(table));

  aom_film_grain_table_append(&table, 0, 1000, film_grain_test_vectors + 0);
  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 0, 1100, true, &grain));

  EXPECT_EQ(nullptr, table.head);
  EXPECT_EQ(nullptr, table.tail);
  aom_film_grain_table_free(&table);
}

TEST(FilmGrainTableTest, SplitSingleSegment) {
  aom_film_grain_table_t table;
  aom_film_grain_t grain;
  memset(&table, 0, sizeof(table));

  aom_film_grain_table_append(&table, 0, 1000, film_grain_test_vectors + 0);

  // Test lookup and remove that adjusts start time
  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 0, 100, true, &grain));
  EXPECT_EQ(nullptr, table.head->next);
  EXPECT_EQ(100, table.head->start_time);

  // Test lookup and remove that adjusts end time
  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 900, 1000, true, &grain));
  EXPECT_EQ(nullptr, table.head->next);
  EXPECT_EQ(100, table.head->start_time);
  EXPECT_EQ(900, table.head->end_time);

  // Test lookup and remove that splits the first entry
  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 400, 600, true, &grain));
  EXPECT_EQ(100, table.head->start_time);
  EXPECT_EQ(400, table.head->end_time);

  ASSERT_NE(nullptr, table.head->next);
  EXPECT_EQ(table.tail, table.head->next);
  EXPECT_EQ(600, table.head->next->start_time);
  EXPECT_EQ(900, table.head->next->end_time);

  aom_film_grain_table_free(&table);
}

TEST(FilmGrainTableTest, AddAndLookupMultipleSegments) {
  aom_film_grain_table_t table;
  memset(&table, 0, sizeof(table));

  aom_film_grain_t grain;
  const int kNumTestVectors =
      sizeof(film_grain_test_vectors) / sizeof(film_grain_test_vectors[0]);
  for (int i = 0; i < kNumTestVectors; ++i) {
    aom_film_grain_table_append(&table, i * 1000, (i + 1) * 1000,
                                film_grain_test_vectors + i);
  }

  for (int i = kNumTestVectors - 1; i >= 0; --i) {
    EXPECT_TRUE(aom_film_grain_table_lookup(&table, i * 1000, (i + 1) * 1000,
                                            true, &grain));
    grain_equal(film_grain_test_vectors + i, &grain);
    EXPECT_FALSE(aom_film_grain_table_lookup(&table, i * 1000, (i + 1) * 1000,
                                             true, &grain));
  }

  // Verify that all the data has been removed
  for (int i = 0; i < kNumTestVectors; ++i) {
    EXPECT_FALSE(aom_film_grain_table_lookup(&table, i * 1000, (i + 1) * 1000,
                                             true, &grain));
  }
  aom_film_grain_table_free(&table);
}

class FilmGrainTableIOTest : public ::testing::Test {
 protected:
  void SetUp() override { memset(&error_, 0, sizeof(error_)); }
  struct aom_internal_error_info error_;
};

TEST_F(FilmGrainTableIOTest, ReadMissingFile) {
  aom_film_grain_table_t table;
  memset(&table, 0, sizeof(table));
  ASSERT_EQ(AOM_CODEC_ERROR, aom_film_grain_table_read(
                                 &table, "/path/to/missing/file", &error_));
}

TEST_F(FilmGrainTableIOTest, ReadTruncatedFile) {
  aom_film_grain_table_t table;
  memset(&table, 0, sizeof(table));

  std::string grain_file;
  FILE *file = libaom_test::GetTempOutFile(&grain_file);
  ASSERT_NE(file, nullptr);
  fwrite("deadbeef", 8, 1, file);
  fclose(file);
  ASSERT_EQ(AOM_CODEC_ERROR,
            aom_film_grain_table_read(&table, grain_file.c_str(), &error_));
  EXPECT_EQ(0, remove(grain_file.c_str()));
}

TEST_F(FilmGrainTableIOTest, RoundTripReadWrite) {
  aom_film_grain_table_t table;
  memset(&table, 0, sizeof(table));

  aom_film_grain_t expected_grain[16];
  const int kNumTestVectors =
      sizeof(film_grain_test_vectors) / sizeof(film_grain_test_vectors[0]);
  for (int i = 0; i < kNumTestVectors; ++i) {
    expected_grain[i] = film_grain_test_vectors[i];
    expected_grain[i].random_seed = i;
    expected_grain[i].update_parameters = i % 2;
    expected_grain[i].apply_grain = (i + 1) % 2;
    expected_grain[i].bit_depth = 0;
    aom_film_grain_table_append(&table, i * 1000, (i + 1) * 1000,
                                expected_grain + i);
  }
  std::string grain_file;
  FILE *tmpfile = libaom_test::GetTempOutFile(&grain_file);
  ASSERT_NE(tmpfile, nullptr);
  fclose(tmpfile);
  ASSERT_EQ(AOM_CODEC_OK,
            aom_film_grain_table_write(&table, grain_file.c_str(), &error_));
  aom_film_grain_table_free(&table);

  memset(&table, 0, sizeof(table));
  ASSERT_EQ(AOM_CODEC_OK,
            aom_film_grain_table_read(&table, grain_file.c_str(), &error_));
  for (int i = 0; i < kNumTestVectors; ++i) {
    aom_film_grain_t grain;
    EXPECT_TRUE(aom_film_grain_table_lookup(&table, i * 1000, (i + 1) * 1000,
                                            true, &grain));
    grain_equal(expected_grain + i, &grain);
  }
  aom_film_grain_table_free(&table);
  EXPECT_EQ(0, remove(grain_file.c_str()));
}

TEST_F(FilmGrainTableIOTest, RoundTripSplit) {
  std::string grain_file;
  FILE *tmpfile = libaom_test::GetTempOutFile(&grain_file);
  ASSERT_NE(tmpfile, nullptr);
  fclose(tmpfile);

  aom_film_grain_table_t table;
  memset(&table, 0, sizeof(table));

  aom_film_grain_t grain = film_grain_test_vectors[0];
  aom_film_grain_table_append(&table, 0, 3000, &grain);
  ASSERT_TRUE(aom_film_grain_table_lookup(&table, 1000, 2000, true, &grain));
  ASSERT_TRUE(aom_film_grain_table_lookup(&table, 0, 1000, false, &grain));
  EXPECT_FALSE(aom_film_grain_table_lookup(&table, 1000, 2000, false, &grain));
  ASSERT_TRUE(aom_film_grain_table_lookup(&table, 2000, 3000, false, &grain));
  ASSERT_EQ(AOM_CODEC_OK,
            aom_film_grain_table_write(&table, grain_file.c_str(), &error_));
  aom_film_grain_table_free(&table);

  memset(&table, 0, sizeof(table));
  ASSERT_EQ(AOM_CODEC_OK,
            aom_film_grain_table_read(&table, grain_file.c_str(), &error_));
  ASSERT_TRUE(aom_film_grain_table_lookup(&table, 0, 1000, false, &grain));
  ASSERT_FALSE(aom_film_grain_table_lookup(&table, 1000, 2000, false, &grain));
  ASSERT_TRUE(aom_film_grain_table_lookup(&table, 2000, 3000, false, &grain));
  aom_film_grain_table_free(&table);

  EXPECT_EQ(0, remove(grain_file.c_str()));
}

const ::libaom_test::TestMode kFilmGrainEncodeTestModes[] = {
  ::libaom_test::kRealTime,
#if !CONFIG_REALTIME_ONLY
  ::libaom_test::kOnePassGood
#endif
};

class FilmGrainEncodeTest
    : public ::libaom_test::CodecTestWith3Params<int, int,
                                                 ::libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  FilmGrainEncodeTest()
      : EncoderTest(GET_PARAM(0)), test_monochrome_(GET_PARAM(1)),
        key_frame_dist_(GET_PARAM(2)), test_mode_(GET_PARAM(3)) {}
  ~FilmGrainEncodeTest() override = default;

  void SetUp() override {
    InitializeConfig(test_mode_);
    cfg_.monochrome = test_monochrome_ == 1;
    cfg_.rc_target_bitrate = 300;
    cfg_.kf_max_dist = key_frame_dist_;
    cfg_.g_lag_in_frames = 0;
  }

  void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                          ::libaom_test::Encoder *encoder) override {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED,
                       test_mode_ == ::libaom_test::kRealTime ? 7 : 5);
      encoder->Control(AV1E_SET_TUNE_CONTENT, AOM_CONTENT_FILM);
      encoder->Control(AV1E_SET_DENOISE_NOISE_LEVEL, 1);
    } else if (video->frame() == 1) {
      cfg_.monochrome = (test_monochrome_ == 1 || test_monochrome_ == 2);
      encoder->Config(&cfg_);
    } else {
      cfg_.monochrome = test_monochrome_ == 1;
      encoder->Config(&cfg_);
    }
  }

  bool DoDecode() const override { return false; }

  void DoTest() {
    ::libaom_test::I420VideoSource video("hantro_collage_w352h288.yuv", 352,
                                         288, 30, 1, 0, 3);
    cfg_.g_w = video.img()->d_w;
    cfg_.g_h = video.img()->d_h;
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  }

 private:
  // 0: monochroome always off.
  // 1: monochrome always on.
  // 2: monochrome changes from 0, 1, 0, for encoded frames 0, 1, 2.
  // The case where monochrome changes from 1 to 0 (i.e., encoder initialized
  // with monochrome = 1 and then subsequently encoded with monochrome = 0)
  // will fail. The test InitMonochrome1_EncodeMonochrome0 below verifies this.
  int test_monochrome_;
  int key_frame_dist_;
  ::libaom_test::TestMode test_mode_;
};

TEST_P(FilmGrainEncodeTest, Test) { DoTest(); }

AV1_INSTANTIATE_TEST_SUITE(FilmGrainEncodeTest, ::testing::Range(0, 3),
                           ::testing::Values(0, 10),
                           ::testing::ValuesIn(kFilmGrainEncodeTestModes));

// Initialize encoder with monochrome = 1, and then encode frame with
// monochrome = 0. This will result in an error: see the following check
// in encoder_set_config() in av1/av1_cx_iface.c.
// TODO(marpan): Consider moving this test to another file, as the failure
// has nothing to do with film grain mode.
TEST(FilmGrainEncodeTest, InitMonochrome1EncodeMonochrome0) {
  const int kWidth = 352;
  const int kHeight = 288;
  const int usage = AOM_USAGE_REALTIME;
  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  ASSERT_EQ(aom_codec_enc_config_default(iface, &cfg, usage), AOM_CODEC_OK);
  aom_codec_ctx_t enc;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  // Initialize encoder, with monochrome = 0.
  cfg.monochrome = 1;
  aom_codec_err_t init_status = aom_codec_enc_init(&enc, iface, &cfg, 0);
  ASSERT_EQ(init_status, AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 7), AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_TUNE_CONTENT, AOM_CONTENT_FILM),
            AOM_CODEC_OK);
  ASSERT_EQ(aom_codec_control(&enc, AV1E_SET_DENOISE_NOISE_LEVEL, 1),
            AOM_CODEC_OK);
  // Set image with zero values.
  constexpr size_t kBufferSize =
      kWidth * kHeight + 2 * (kWidth + 1) / 2 * (kHeight + 1) / 2;
  std::vector<unsigned char> buffer(kBufferSize);
  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I420, kWidth, kHeight, 1,
                               buffer.data()));
  // Encode first frame.
  ASSERT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  // Second frame: update config with monochrome = 1.
  cfg.monochrome = 0;
  ASSERT_EQ(aom_codec_enc_config_set(&enc, &cfg), AOM_CODEC_INVALID_PARAM);
  ASSERT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}
