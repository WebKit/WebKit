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

#include <string>
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "aom_dsp/grain_table.h"
#include "aom/internal/aom_codec_internal.h"
#include "av1/encoder/grain_test_vectors.h"
#include "test/video_source.h"

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
  EXPECT_EQ(0, table.head->next);

  // Lookup and remove and check that the entry is no longer there
  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 1000, 2000, true, &grain));
  EXPECT_FALSE(aom_film_grain_table_lookup(&table, 1000, 2000, false, &grain));

  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 2000, 3000, true, &grain));
  EXPECT_FALSE(aom_film_grain_table_lookup(&table, 2000, 3000, false, &grain));

  EXPECT_EQ(0, table.head);
  EXPECT_EQ(0, table.tail);
  aom_film_grain_table_free(&table);
}

TEST(FilmGrainTableTest, AddSingleSegmentRemoveBiggerSegment) {
  aom_film_grain_table_t table;
  aom_film_grain_t grain;

  memset(&table, 0, sizeof(table));

  aom_film_grain_table_append(&table, 0, 1000, film_grain_test_vectors + 0);
  EXPECT_TRUE(aom_film_grain_table_lookup(&table, 0, 1100, true, &grain));

  EXPECT_EQ(0, table.head);
  EXPECT_EQ(0, table.tail);
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
  void SetUp() { memset(&error_, 0, sizeof(error_)); }
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
