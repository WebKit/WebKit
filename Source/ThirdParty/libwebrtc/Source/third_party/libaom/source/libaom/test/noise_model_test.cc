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

#include <limits.h>
#include <math.h>
#include <algorithm>
#include <vector>

#include "aom_dsp/noise_model.h"
#include "aom_dsp/noise_util.h"
#include "config/aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

// Return normally distrbuted values with standard deviation of sigma.
double randn(libaom_test::ACMRandom *random, double sigma) {
  while (true) {
    const double u = 2.0 * ((double)random->Rand31() /
                            testing::internal::Random::kMaxRange) -
                     1.0;
    const double v = 2.0 * ((double)random->Rand31() /
                            testing::internal::Random::kMaxRange) -
                     1.0;
    const double s = u * u + v * v;
    if (s > 0 && s < 1) {
      return sigma * (u * sqrt(-2.0 * log(s) / s));
    }
  }
}

// Synthesizes noise using the auto-regressive filter of the given lag,
// with the provided n coefficients sampled at the given coords.
void noise_synth(libaom_test::ACMRandom *random, int lag, int n,
                 const int (*coords)[2], const double *coeffs, double *data,
                 int w, int h) {
  const int pad_size = 3 * lag;
  const int padded_w = w + pad_size;
  const int padded_h = h + pad_size;
  int x = 0, y = 0;
  std::vector<double> padded(padded_w * padded_h);

  for (y = 0; y < padded_h; ++y) {
    for (x = 0; x < padded_w; ++x) {
      padded[y * padded_w + x] = randn(random, 1.0);
    }
  }
  for (y = lag; y < padded_h; ++y) {
    for (x = lag; x < padded_w; ++x) {
      double sum = 0;
      int i = 0;
      for (i = 0; i < n; ++i) {
        const int dx = coords[i][0];
        const int dy = coords[i][1];
        sum += padded[(y + dy) * padded_w + (x + dx)] * coeffs[i];
      }
      padded[y * padded_w + x] += sum;
    }
  }
  // Copy over the padded rows to the output
  for (y = 0; y < h; ++y) {
    memcpy(data + y * w, &padded[0] + y * padded_w, sizeof(*data) * w);
  }
}

std::vector<float> get_noise_psd(double *noise, int width, int height,
                                 int block_size) {
  float *block =
      (float *)aom_memalign(32, block_size * block_size * sizeof(block));
  std::vector<float> psd(block_size * block_size);
  if (block == nullptr) {
    EXPECT_NE(block, nullptr);
    return psd;
  }
  int num_blocks = 0;
  struct aom_noise_tx_t *tx = aom_noise_tx_malloc(block_size);
  if (tx == nullptr) {
    EXPECT_NE(tx, nullptr);
    return psd;
  }
  for (int y = 0; y <= height - block_size; y += block_size / 2) {
    for (int x = 0; x <= width - block_size; x += block_size / 2) {
      for (int yy = 0; yy < block_size; ++yy) {
        for (int xx = 0; xx < block_size; ++xx) {
          block[yy * block_size + xx] = (float)noise[(y + yy) * width + x + xx];
        }
      }
      aom_noise_tx_forward(tx, &block[0]);
      aom_noise_tx_add_energy(tx, &psd[0]);
      num_blocks++;
    }
  }
  for (int yy = 0; yy < block_size; ++yy) {
    for (int xx = 0; xx <= block_size / 2; ++xx) {
      psd[yy * block_size + xx] /= num_blocks;
    }
  }
  // Fill in the data that is missing due to symmetries
  for (int xx = 1; xx < block_size / 2; ++xx) {
    psd[(block_size - xx)] = psd[xx];
  }
  for (int yy = 1; yy < block_size; ++yy) {
    for (int xx = 1; xx < block_size / 2; ++xx) {
      psd[(block_size - yy) * block_size + (block_size - xx)] =
          psd[yy * block_size + xx];
    }
  }
  aom_noise_tx_free(tx);
  aom_free(block);
  return psd;
}

}  // namespace

TEST(NoiseStrengthSolver, GetCentersTwoBins) {
  aom_noise_strength_solver_t solver;
  aom_noise_strength_solver_init(&solver, 2, 8);
  EXPECT_NEAR(0, aom_noise_strength_solver_get_center(&solver, 0), 1e-5);
  EXPECT_NEAR(255, aom_noise_strength_solver_get_center(&solver, 1), 1e-5);
  aom_noise_strength_solver_free(&solver);
}

TEST(NoiseStrengthSolver, GetCentersTwoBins10bit) {
  aom_noise_strength_solver_t solver;
  aom_noise_strength_solver_init(&solver, 2, 10);
  EXPECT_NEAR(0, aom_noise_strength_solver_get_center(&solver, 0), 1e-5);
  EXPECT_NEAR(1023, aom_noise_strength_solver_get_center(&solver, 1), 1e-5);
  aom_noise_strength_solver_free(&solver);
}

TEST(NoiseStrengthSolver, GetCenters256Bins) {
  const int num_bins = 256;
  aom_noise_strength_solver_t solver;
  aom_noise_strength_solver_init(&solver, num_bins, 8);

  for (int i = 0; i < 256; ++i) {
    EXPECT_NEAR(i, aom_noise_strength_solver_get_center(&solver, i), 1e-5);
  }
  aom_noise_strength_solver_free(&solver);
}

// Tests that the noise strength solver returns the identity transform when
// given identity-like constraints.
TEST(NoiseStrengthSolver, ObserveIdentity) {
  const int num_bins = 256;
  aom_noise_strength_solver_t solver;
  ASSERT_EQ(1, aom_noise_strength_solver_init(&solver, num_bins, 8));

  // We have to add a big more strength to constraints at the boundary to
  // overcome any regularization.
  for (int j = 0; j < 5; ++j) {
    aom_noise_strength_solver_add_measurement(&solver, 0, 0);
    aom_noise_strength_solver_add_measurement(&solver, 255, 255);
  }
  for (int i = 0; i < 256; ++i) {
    aom_noise_strength_solver_add_measurement(&solver, i, i);
  }
  EXPECT_EQ(1, aom_noise_strength_solver_solve(&solver));
  for (int i = 2; i < num_bins - 2; ++i) {
    EXPECT_NEAR(i, solver.eqns.x[i], 0.1);
  }

  aom_noise_strength_lut_t lut;
  EXPECT_EQ(1, aom_noise_strength_solver_fit_piecewise(&solver, 2, &lut));

  ASSERT_EQ(2, lut.num_points);
  EXPECT_NEAR(0.0, lut.points[0][0], 1e-5);
  EXPECT_NEAR(0.0, lut.points[0][1], 0.5);
  EXPECT_NEAR(255.0, lut.points[1][0], 1e-5);
  EXPECT_NEAR(255.0, lut.points[1][1], 0.5);

  aom_noise_strength_lut_free(&lut);
  aom_noise_strength_solver_free(&solver);
}

TEST(NoiseStrengthSolver, SimplifiesCurve) {
  const int num_bins = 256;
  aom_noise_strength_solver_t solver;
  EXPECT_EQ(1, aom_noise_strength_solver_init(&solver, num_bins, 8));

  // Create a parabolic input
  for (int i = 0; i < 256; ++i) {
    const double x = (i - 127.5) / 63.5;
    aom_noise_strength_solver_add_measurement(&solver, i, x * x);
  }
  EXPECT_EQ(1, aom_noise_strength_solver_solve(&solver));

  // First try to fit an unconstrained lut
  aom_noise_strength_lut_t lut;
  EXPECT_EQ(1, aom_noise_strength_solver_fit_piecewise(&solver, -1, &lut));
  ASSERT_LE(20, lut.num_points);
  aom_noise_strength_lut_free(&lut);

  // Now constrain the maximum number of points
  const int kMaxPoints = 9;
  EXPECT_EQ(1,
            aom_noise_strength_solver_fit_piecewise(&solver, kMaxPoints, &lut));
  ASSERT_EQ(kMaxPoints, lut.num_points);

  // Check that the input parabola is still well represented
  EXPECT_NEAR(0.0, lut.points[0][0], 1e-5);
  EXPECT_NEAR(4.0, lut.points[0][1], 0.1);
  for (int i = 1; i < lut.num_points - 1; ++i) {
    const double x = (lut.points[i][0] - 128.) / 64.;
    EXPECT_NEAR(x * x, lut.points[i][1], 0.1);
  }
  EXPECT_NEAR(255.0, lut.points[kMaxPoints - 1][0], 1e-5);

  EXPECT_NEAR(4.0, lut.points[kMaxPoints - 1][1], 0.1);
  aom_noise_strength_lut_free(&lut);
  aom_noise_strength_solver_free(&solver);
}

TEST(NoiseStrengthLut, LutInitNegativeOrZeroSize) {
  aom_noise_strength_lut_t lut;
  ASSERT_FALSE(aom_noise_strength_lut_init(&lut, -1));
  ASSERT_FALSE(aom_noise_strength_lut_init(&lut, 0));
}

TEST(NoiseStrengthLut, LutEvalSinglePoint) {
  aom_noise_strength_lut_t lut;
  ASSERT_TRUE(aom_noise_strength_lut_init(&lut, 1));
  ASSERT_EQ(1, lut.num_points);
  lut.points[0][0] = 0;
  lut.points[0][1] = 1;
  EXPECT_EQ(1, aom_noise_strength_lut_eval(&lut, -1));
  EXPECT_EQ(1, aom_noise_strength_lut_eval(&lut, 0));
  EXPECT_EQ(1, aom_noise_strength_lut_eval(&lut, 1));
  aom_noise_strength_lut_free(&lut);
}

TEST(NoiseStrengthLut, LutEvalMultiPointInterp) {
  const double kEps = 1e-5;
  aom_noise_strength_lut_t lut;
  ASSERT_TRUE(aom_noise_strength_lut_init(&lut, 4));
  ASSERT_EQ(4, lut.num_points);

  lut.points[0][0] = 0;
  lut.points[0][1] = 0;

  lut.points[1][0] = 1;
  lut.points[1][1] = 1;

  lut.points[2][0] = 2;
  lut.points[2][1] = 1;

  lut.points[3][0] = 100;
  lut.points[3][1] = 1001;

  // Test lower boundary
  EXPECT_EQ(0, aom_noise_strength_lut_eval(&lut, -1));
  EXPECT_EQ(0, aom_noise_strength_lut_eval(&lut, 0));

  // Test first part that should be identity
  EXPECT_NEAR(0.25, aom_noise_strength_lut_eval(&lut, 0.25), kEps);
  EXPECT_NEAR(0.75, aom_noise_strength_lut_eval(&lut, 0.75), kEps);

  // This is a constant section (should evaluate to 1)
  EXPECT_NEAR(1.0, aom_noise_strength_lut_eval(&lut, 1.25), kEps);
  EXPECT_NEAR(1.0, aom_noise_strength_lut_eval(&lut, 1.75), kEps);

  // Test interpolation between to non-zero y coords.
  EXPECT_NEAR(1, aom_noise_strength_lut_eval(&lut, 2), kEps);
  EXPECT_NEAR(251, aom_noise_strength_lut_eval(&lut, 26.5), kEps);
  EXPECT_NEAR(751, aom_noise_strength_lut_eval(&lut, 75.5), kEps);

  // Test upper boundary
  EXPECT_EQ(1001, aom_noise_strength_lut_eval(&lut, 100));
  EXPECT_EQ(1001, aom_noise_strength_lut_eval(&lut, 101));

  aom_noise_strength_lut_free(&lut);
}

TEST(NoiseModel, InitSuccessWithValidSquareShape) {
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 2, 8, 0 };
  aom_noise_model_t model;

  EXPECT_TRUE(aom_noise_model_init(&model, params));

  const int kNumCoords = 12;
  const int kCoords[][2] = { { -2, -2 }, { -1, -2 }, { 0, -2 },  { 1, -2 },
                             { 2, -2 },  { -2, -1 }, { -1, -1 }, { 0, -1 },
                             { 1, -1 },  { 2, -1 },  { -2, 0 },  { -1, 0 } };
  EXPECT_EQ(kNumCoords, model.n);
  for (int i = 0; i < kNumCoords; ++i) {
    const int *coord = kCoords[i];
    EXPECT_EQ(coord[0], model.coords[i][0]);
    EXPECT_EQ(coord[1], model.coords[i][1]);
  }
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitSuccessWithValidDiamondShape) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_DIAMOND, 2, 8, 0 };
  EXPECT_TRUE(aom_noise_model_init(&model, params));
  EXPECT_EQ(6, model.n);
  const int kNumCoords = 6;
  const int kCoords[][2] = { { 0, -2 }, { -1, -1 }, { 0, -1 },
                             { 1, -1 }, { -2, 0 },  { -1, 0 } };
  EXPECT_EQ(kNumCoords, model.n);
  for (int i = 0; i < kNumCoords; ++i) {
    const int *coord = kCoords[i];
    EXPECT_EQ(coord[0], model.coords[i][0]);
    EXPECT_EQ(coord[1], model.coords[i][1]);
  }
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithTooLargeLag) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 10, 8, 0 };
  EXPECT_FALSE(aom_noise_model_init(&model, params));
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithTooSmallLag) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 0, 8, 0 };
  EXPECT_FALSE(aom_noise_model_init(&model, params));
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithInvalidShape) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = { aom_noise_shape(100), 3, 8, 0 };
  EXPECT_FALSE(aom_noise_model_init(&model, params));
  aom_noise_model_free(&model);
}

TEST(NoiseModel, InitFailsWithInvalidBitdepth) {
  aom_noise_model_t model;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 2, 8, 0 };
  for (int i = 0; i <= 32; ++i) {
    params.bit_depth = i;
    if (i == 8 || i == 10 || i == 12) {
      EXPECT_TRUE(aom_noise_model_init(&model, params)) << "bit_depth: " << i;
      aom_noise_model_free(&model);
    } else {
      EXPECT_FALSE(aom_noise_model_init(&model, params)) << "bit_depth: " << i;
    }
  }
  params.bit_depth = INT_MAX;
  EXPECT_FALSE(aom_noise_model_init(&model, params));
}

// A container template class to hold a data type and extra arguments.
// All of these args are bundled into one struct so that we can use
// parameterized tests on combinations of supported data types
// (uint8_t and uint16_t) and bit depths (8, 10, 12).
template <typename T, int bit_depth, bool use_highbd>
struct BitDepthParams {
  typedef T data_type_t;
  static const int kBitDepth = bit_depth;
  static const bool kUseHighBD = use_highbd;
};

template <typename T>
class FlatBlockEstimatorTest : public ::testing::Test, public T {
 public:
  void SetUp() override { random_.Reset(171); }
  typedef std::vector<typename T::data_type_t> VecType;
  VecType data_;
  libaom_test::ACMRandom random_;
};

TYPED_TEST_SUITE_P(FlatBlockEstimatorTest);

TYPED_TEST_P(FlatBlockEstimatorTest, ExtractBlock) {
  const int kBlockSize = 16;
  aom_flat_block_finder_t flat_block_finder;
  ASSERT_EQ(1, aom_flat_block_finder_init(&flat_block_finder, kBlockSize,
                                          this->kBitDepth, this->kUseHighBD));
  const double normalization = flat_block_finder.normalization;

  // Test with an image of more than one block.
  const int h = 2 * kBlockSize;
  const int w = 2 * kBlockSize;
  const int stride = 2 * kBlockSize;
  this->data_.resize(h * stride, 128);

  // Set up the (0,0) block to be a plane and the (0,1) block to be a
  // checkerboard
  const int shift = this->kBitDepth - 8;
  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize; ++x) {
      this->data_[y * stride + x] = (-y + x + 128) << shift;
      this->data_[y * stride + x + kBlockSize] =
          ((x % 2 + y % 2) % 2 ? 128 - 20 : 128 + 20) << shift;
    }
  }
  std::vector<double> block(kBlockSize * kBlockSize, 1);
  std::vector<double> plane(kBlockSize * kBlockSize, 1);

  // The block data should be a constant (zero) and the rest of the plane
  // trend is covered in the plane data.
  aom_flat_block_finder_extract_block(&flat_block_finder,
                                      (uint8_t *)&this->data_[0], w, h, stride,
                                      0, 0, &plane[0], &block[0]);
  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize; ++x) {
      EXPECT_NEAR(0, block[y * kBlockSize + x], 1e-5);
      EXPECT_NEAR((double)(this->data_[y * stride + x]) / normalization,
                  plane[y * kBlockSize + x], 1e-5);
    }
  }

  // The plane trend is a constant, and the block is a zero mean checkerboard.
  aom_flat_block_finder_extract_block(&flat_block_finder,
                                      (uint8_t *)&this->data_[0], w, h, stride,
                                      kBlockSize, 0, &plane[0], &block[0]);
  const int mid = 128 << shift;
  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize; ++x) {
      EXPECT_NEAR(((double)this->data_[y * stride + x + kBlockSize] - mid) /
                      normalization,
                  block[y * kBlockSize + x], 1e-5);
      EXPECT_NEAR(mid / normalization, plane[y * kBlockSize + x], 1e-5);
    }
  }
  aom_flat_block_finder_free(&flat_block_finder);
}

TYPED_TEST_P(FlatBlockEstimatorTest, FindFlatBlocks) {
  const int kBlockSize = 32;
  aom_flat_block_finder_t flat_block_finder;
  ASSERT_EQ(1, aom_flat_block_finder_init(&flat_block_finder, kBlockSize,
                                          this->kBitDepth, this->kUseHighBD));

  const int num_blocks_w = 8;
  const int h = kBlockSize;
  const int w = kBlockSize * num_blocks_w;
  const int stride = w;
  this->data_.resize(h * stride, 128);
  std::vector<uint8_t> flat_blocks(num_blocks_w, 0);

  const int shift = this->kBitDepth - 8;
  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize; ++x) {
      // Block 0 (not flat): constant doesn't have enough variance to qualify
      this->data_[y * stride + x + 0 * kBlockSize] = 128 << shift;

      // Block 1 (not flat): too high of variance is hard to validate as flat
      this->data_[y * stride + x + 1 * kBlockSize] =
          ((uint8_t)(128 + randn(&this->random_, 5))) << shift;

      // Block 2 (flat): slight checkerboard added to constant
      const int check = (x % 2 + y % 2) % 2 ? -2 : 2;
      this->data_[y * stride + x + 2 * kBlockSize] = (128 + check) << shift;

      // Block 3 (flat): planar block with checkerboard pattern is also flat
      this->data_[y * stride + x + 3 * kBlockSize] =
          (y * 2 - x / 2 + 128 + check) << shift;

      // Block 4 (flat): gaussian random with standard deviation 1.
      this->data_[y * stride + x + 4 * kBlockSize] =
          ((uint8_t)(randn(&this->random_, 1) + x + 128.0)) << shift;

      // Block 5 (flat): gaussian random with standard deviation 2.
      this->data_[y * stride + x + 5 * kBlockSize] =
          ((uint8_t)(randn(&this->random_, 2) + y + 128.0)) << shift;

      // Block 6 (not flat): too high of directional gradient.
      const int strong_edge = x > kBlockSize / 2 ? 64 : 0;
      this->data_[y * stride + x + 6 * kBlockSize] =
          ((uint8_t)(randn(&this->random_, 1) + strong_edge + 128.0)) << shift;

      // Block 7 (not flat): too high gradient.
      const int big_check = ((x >> 2) % 2 + (y >> 2) % 2) % 2 ? -16 : 16;
      this->data_[y * stride + x + 7 * kBlockSize] =
          ((uint8_t)(randn(&this->random_, 1) + big_check + 128.0)) << shift;
    }
  }

  EXPECT_EQ(4, aom_flat_block_finder_run(&flat_block_finder,
                                         (uint8_t *)&this->data_[0], w, h,
                                         stride, &flat_blocks[0]));

  // First two blocks are not flat
  EXPECT_EQ(0, flat_blocks[0]);
  EXPECT_EQ(0, flat_blocks[1]);

  // Next 4 blocks are flat.
  EXPECT_EQ(255, flat_blocks[2]);
  EXPECT_EQ(255, flat_blocks[3]);
  EXPECT_EQ(255, flat_blocks[4]);
  EXPECT_EQ(255, flat_blocks[5]);

  // Last 2 are not flat by threshold
  EXPECT_EQ(0, flat_blocks[6]);
  EXPECT_EQ(0, flat_blocks[7]);

  // Add the noise from non-flat block 1 to every block.
  for (int y = 0; y < kBlockSize; ++y) {
    for (int x = 0; x < kBlockSize * num_blocks_w; ++x) {
      this->data_[y * stride + x] +=
          (this->data_[y * stride + x % kBlockSize + kBlockSize] -
           (128 << shift));
    }
  }
  // Now the scored selection will pick the one that is most likely flat (block
  // 0)
  EXPECT_EQ(1, aom_flat_block_finder_run(&flat_block_finder,
                                         (uint8_t *)&this->data_[0], w, h,
                                         stride, &flat_blocks[0]));
  EXPECT_EQ(1, flat_blocks[0]);
  EXPECT_EQ(0, flat_blocks[1]);
  EXPECT_EQ(0, flat_blocks[2]);
  EXPECT_EQ(0, flat_blocks[3]);
  EXPECT_EQ(0, flat_blocks[4]);
  EXPECT_EQ(0, flat_blocks[5]);
  EXPECT_EQ(0, flat_blocks[6]);
  EXPECT_EQ(0, flat_blocks[7]);

  aom_flat_block_finder_free(&flat_block_finder);
}

REGISTER_TYPED_TEST_SUITE_P(FlatBlockEstimatorTest, ExtractBlock,
                            FindFlatBlocks);

typedef ::testing::Types<BitDepthParams<uint8_t, 8, false>,   // lowbd
                         BitDepthParams<uint16_t, 8, true>,   // lowbd in 16-bit
                         BitDepthParams<uint16_t, 10, true>,  // highbd data
                         BitDepthParams<uint16_t, 12, true> >
    AllBitDepthParams;
INSTANTIATE_TYPED_TEST_SUITE_P(FlatBlockInstatiation, FlatBlockEstimatorTest,
                               AllBitDepthParams);

template <typename T>
class NoiseModelUpdateTest : public ::testing::Test, public T {
 public:
  static const int kWidth = 128;
  static const int kHeight = 128;
  static const int kBlockSize = 16;
  static const int kNumBlocksX = kWidth / kBlockSize;
  static const int kNumBlocksY = kHeight / kBlockSize;

  void SetUp() override {
    const aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 3,
                                              T::kBitDepth, T::kUseHighBD };
    ASSERT_TRUE(aom_noise_model_init(&model_, params));

    random_.Reset(100171);

    data_.resize(kWidth * kHeight * 3);
    denoised_.resize(kWidth * kHeight * 3);
    noise_.resize(kWidth * kHeight * 3);
    renoise_.resize(kWidth * kHeight);
    flat_blocks_.resize(kNumBlocksX * kNumBlocksY);

    for (int c = 0, offset = 0; c < 3; ++c, offset += kWidth * kHeight) {
      data_ptr_[c] = &data_[offset];
      noise_ptr_[c] = &noise_[offset];
      denoised_ptr_[c] = &denoised_[offset];
      strides_[c] = kWidth;

      data_ptr_raw_[c] = (uint8_t *)&data_[offset];
      denoised_ptr_raw_[c] = (uint8_t *)&denoised_[offset];
    }
    chroma_sub_[0] = 0;
    chroma_sub_[1] = 0;
  }

  int NoiseModelUpdate(int block_size = kBlockSize) {
    return aom_noise_model_update(&model_, data_ptr_raw_, denoised_ptr_raw_,
                                  kWidth, kHeight, strides_, chroma_sub_,
                                  &flat_blocks_[0], block_size);
  }

  void TearDown() override { aom_noise_model_free(&model_); }

 protected:
  aom_noise_model_t model_;
  std::vector<typename T::data_type_t> data_;
  std::vector<typename T::data_type_t> denoised_;

  std::vector<double> noise_;
  std::vector<double> renoise_;
  std::vector<uint8_t> flat_blocks_;

  typename T::data_type_t *data_ptr_[3];
  typename T::data_type_t *denoised_ptr_[3];

  double *noise_ptr_[3];
  int strides_[3];
  int chroma_sub_[2];
  libaom_test::ACMRandom random_;

 private:
  uint8_t *data_ptr_raw_[3];
  uint8_t *denoised_ptr_raw_[3];
};

TYPED_TEST_SUITE_P(NoiseModelUpdateTest);

TYPED_TEST_P(NoiseModelUpdateTest, UpdateFailsNoFlatBlocks) {
  EXPECT_EQ(AOM_NOISE_STATUS_INSUFFICIENT_FLAT_BLOCKS,
            this->NoiseModelUpdate());
}

TYPED_TEST_P(NoiseModelUpdateTest, UpdateSuccessForZeroNoiseAllFlat) {
  this->flat_blocks_.assign(this->flat_blocks_.size(), 1);
  this->denoised_.assign(this->denoised_.size(), 128);
  this->data_.assign(this->denoised_.size(), 128);
  EXPECT_EQ(AOM_NOISE_STATUS_INTERNAL_ERROR, this->NoiseModelUpdate());
}

TYPED_TEST_P(NoiseModelUpdateTest, UpdateFailsBlockSizeTooSmall) {
  this->flat_blocks_.assign(this->flat_blocks_.size(), 1);
  this->denoised_.assign(this->denoised_.size(), 128);
  this->data_.assign(this->denoised_.size(), 128);
  EXPECT_EQ(AOM_NOISE_STATUS_INVALID_ARGUMENT,
            this->NoiseModelUpdate(6 /* block_size=6 is too small*/));
}

TYPED_TEST_P(NoiseModelUpdateTest, UpdateSuccessForWhiteRandomNoise) {
  aom_noise_model_t &model = this->model_;
  const int width = this->kWidth;
  const int height = this->kHeight;

  const int shift = this->kBitDepth - 8;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      this->data_ptr_[0][y * width + x] = int(64 + y + randn(&this->random_, 1))
                                          << shift;
      this->denoised_ptr_[0][y * width + x] = (64 + y) << shift;
      // Make the chroma planes completely correlated with the Y plane
      for (int c = 1; c < 3; ++c) {
        this->data_ptr_[c][y * width + x] = this->data_ptr_[0][y * width + x];
        this->denoised_ptr_[c][y * width + x] =
            this->denoised_ptr_[0][y * width + x];
      }
    }
  }
  this->flat_blocks_.assign(this->flat_blocks_.size(), 1);
  EXPECT_EQ(AOM_NOISE_STATUS_OK, this->NoiseModelUpdate());

  const double kCoeffEps = 0.075;
  const int n = model.n;
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < n; ++i) {
      EXPECT_NEAR(0, model.latest_state[c].eqns.x[i], kCoeffEps);
      EXPECT_NEAR(0, model.combined_state[c].eqns.x[i], kCoeffEps);
    }
    // The second and third channels are highly correlated with the first.
    if (c > 0) {
      ASSERT_EQ(n + 1, model.latest_state[c].eqns.n);
      ASSERT_EQ(n + 1, model.combined_state[c].eqns.n);

      EXPECT_NEAR(1, model.latest_state[c].eqns.x[n], kCoeffEps);
      EXPECT_NEAR(1, model.combined_state[c].eqns.x[n], kCoeffEps);
    }
  }

  // The fitted noise strength should be close to the standard deviation
  // for all intensity bins.
  const double kStdEps = 0.1;
  const double normalize = 1 << shift;

  for (int i = 0; i < model.latest_state[0].strength_solver.eqns.n; ++i) {
    EXPECT_NEAR(1.0,
                model.latest_state[0].strength_solver.eqns.x[i] / normalize,
                kStdEps);
    EXPECT_NEAR(1.0,
                model.combined_state[0].strength_solver.eqns.x[i] / normalize,
                kStdEps);
  }

  aom_noise_strength_lut_t lut;
  aom_noise_strength_solver_fit_piecewise(
      &model.latest_state[0].strength_solver, -1, &lut);
  ASSERT_EQ(2, lut.num_points);
  EXPECT_NEAR(0.0, lut.points[0][0], 1e-5);
  EXPECT_NEAR(1.0, lut.points[0][1] / normalize, kStdEps);
  EXPECT_NEAR((1 << this->kBitDepth) - 1, lut.points[1][0], 1e-5);
  EXPECT_NEAR(1.0, lut.points[1][1] / normalize, kStdEps);
  aom_noise_strength_lut_free(&lut);
}

TYPED_TEST_P(NoiseModelUpdateTest, UpdateSuccessForScaledWhiteNoise) {
  aom_noise_model_t &model = this->model_;
  const int width = this->kWidth;
  const int height = this->kHeight;

  const double kCoeffEps = 0.055;
  const double kLowStd = 1;
  const double kHighStd = 4;
  const int shift = this->kBitDepth - 8;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      for (int c = 0; c < 3; ++c) {
        // The image data is bimodal:
        // Bottom half has low intensity and low noise strength
        // Top half has high intensity and high noise strength
        const int avg = (y < height / 2) ? 4 : 245;
        const double std = (y < height / 2) ? kLowStd : kHighStd;
        this->data_ptr_[c][y * width + x] =
            ((uint8_t)std::min((int)255,
                               (int)(2 + avg + randn(&this->random_, std))))
            << shift;
        this->denoised_ptr_[c][y * width + x] = (2 + avg) << shift;
      }
    }
  }
  // Label all blocks as flat for the update
  this->flat_blocks_.assign(this->flat_blocks_.size(), 1);
  EXPECT_EQ(AOM_NOISE_STATUS_OK, this->NoiseModelUpdate());

  const int n = model.n;
  // The noise is uncorrelated spatially and with the y channel.
  // All coefficients should be reasonably close to zero.
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < n; ++i) {
      EXPECT_NEAR(0, model.latest_state[c].eqns.x[i], kCoeffEps);
      EXPECT_NEAR(0, model.combined_state[c].eqns.x[i], kCoeffEps);
    }
    if (c > 0) {
      ASSERT_EQ(n + 1, model.latest_state[c].eqns.n);
      ASSERT_EQ(n + 1, model.combined_state[c].eqns.n);

      // The correlation to the y channel should be low (near zero)
      EXPECT_NEAR(0, model.latest_state[c].eqns.x[n], kCoeffEps);
      EXPECT_NEAR(0, model.combined_state[c].eqns.x[n], kCoeffEps);
    }
  }

  // Noise strength should vary between kLowStd and kHighStd.
  const double kStdEps = 0.15;
  // We have to normalize fitted standard deviation based on bit depth.
  const double normalize = (1 << shift);

  ASSERT_EQ(20, model.latest_state[0].strength_solver.eqns.n);
  for (int i = 0; i < model.latest_state[0].strength_solver.eqns.n; ++i) {
    const double a = i / 19.0;
    const double expected = (kLowStd * (1.0 - a) + kHighStd * a);
    EXPECT_NEAR(expected,
                model.latest_state[0].strength_solver.eqns.x[i] / normalize,
                kStdEps);
    EXPECT_NEAR(expected,
                model.combined_state[0].strength_solver.eqns.x[i] / normalize,
                kStdEps);
  }

  // If we fit a piecewise linear model, there should be two points:
  // one near kLowStd at 0, and the other near kHighStd and 255.
  aom_noise_strength_lut_t lut;
  aom_noise_strength_solver_fit_piecewise(
      &model.latest_state[0].strength_solver, 2, &lut);
  ASSERT_EQ(2, lut.num_points);
  EXPECT_NEAR(0, lut.points[0][0], 1e-4);
  EXPECT_NEAR(kLowStd, lut.points[0][1] / normalize, kStdEps);
  EXPECT_NEAR((1 << this->kBitDepth) - 1, lut.points[1][0], 1e-5);
  EXPECT_NEAR(kHighStd, lut.points[1][1] / normalize, kStdEps);
  aom_noise_strength_lut_free(&lut);
}

TYPED_TEST_P(NoiseModelUpdateTest, UpdateSuccessForCorrelatedNoise) {
  aom_noise_model_t &model = this->model_;
  const int width = this->kWidth;
  const int height = this->kHeight;
  const int kNumCoeffs = 24;
  const double kStd = 4;
  const double kStdEps = 0.3;
  const double kCoeffEps = 0.065;
  // Use different coefficients for each channel
  const double kCoeffs[3][24] = {
    { 0.02884, -0.03356, 0.00633,  0.01757,  0.02849,  -0.04620,
      0.02833, -0.07178, 0.07076,  -0.11603, -0.10413, -0.16571,
      0.05158, -0.07969, 0.02640,  -0.07191, 0.02530,  0.41968,
      0.21450, -0.00702, -0.01401, -0.03676, -0.08713, 0.44196 },
    { 0.00269, -0.01291, -0.01513, 0.07234,  0.03208,   0.00477,
      0.00226, -0.00254, 0.03533,  0.12841,  -0.25970,  -0.06336,
      0.05238, -0.00845, -0.03118, 0.09043,  -0.36558,  0.48903,
      0.00595, -0.11938, 0.02106,  0.095956, -0.350139, 0.59305 },
    { -0.00643, -0.01080, -0.01466, 0.06951, 0.03707,  -0.00482,
      0.00817,  -0.00909, 0.02949,  0.12181, -0.25210, -0.07886,
      0.06083,  -0.01210, -0.03108, 0.08944, -0.35875, 0.49150,
      0.00415,  -0.12905, 0.02870,  0.09740, -0.34610, 0.58824 },
  };

  ASSERT_EQ(model.n, kNumCoeffs);
  this->chroma_sub_[0] = this->chroma_sub_[1] = 1;

  this->flat_blocks_.assign(this->flat_blocks_.size(), 1);

  // Add different noise onto each plane
  const int shift = this->kBitDepth - 8;
  for (int c = 0; c < 3; ++c) {
    noise_synth(&this->random_, model.params.lag, model.n, model.coords,
                kCoeffs[c], this->noise_ptr_[c], width, height);
    const int x_shift = c > 0 ? this->chroma_sub_[0] : 0;
    const int y_shift = c > 0 ? this->chroma_sub_[1] : 0;
    for (int y = 0; y < (height >> y_shift); ++y) {
      for (int x = 0; x < (width >> x_shift); ++x) {
        const uint8_t value = 64 + x / 2 + y / 4;
        this->data_ptr_[c][y * width + x] =
            (uint8_t(value + this->noise_ptr_[c][y * width + x] * kStd))
            << shift;
        this->denoised_ptr_[c][y * width + x] = value << shift;
      }
    }
  }
  EXPECT_EQ(AOM_NOISE_STATUS_OK, this->NoiseModelUpdate());

  // For the Y plane, the solved coefficients should be close to the original
  const int n = model.n;
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < n; ++i) {
      EXPECT_NEAR(kCoeffs[c][i], model.latest_state[c].eqns.x[i], kCoeffEps);
      EXPECT_NEAR(kCoeffs[c][i], model.combined_state[c].eqns.x[i], kCoeffEps);
    }
    // The chroma planes should be uncorrelated with the luma plane
    if (c > 0) {
      EXPECT_NEAR(0, model.latest_state[c].eqns.x[n], kCoeffEps);
      EXPECT_NEAR(0, model.combined_state[c].eqns.x[n], kCoeffEps);
    }
    // Correlation between the coefficient vector and the fitted coefficients
    // should be close to 1.
    EXPECT_LT(0.98, aom_normalized_cross_correlation(
                        model.latest_state[c].eqns.x, kCoeffs[c], kNumCoeffs));

    noise_synth(&this->random_, model.params.lag, model.n, model.coords,
                model.latest_state[c].eqns.x, &this->renoise_[0], width,
                height);

    EXPECT_TRUE(aom_noise_data_validate(&this->renoise_[0], width, height));
  }

  // Check fitted noise strength
  const double normalize = 1 << shift;
  for (int c = 0; c < 3; ++c) {
    for (int i = 0; i < model.latest_state[c].strength_solver.eqns.n; ++i) {
      EXPECT_NEAR(kStd,
                  model.latest_state[c].strength_solver.eqns.x[i] / normalize,
                  kStdEps);
    }
  }
}

TYPED_TEST_P(NoiseModelUpdateTest,
             NoiseStrengthChangeSignalsDifferentNoiseType) {
  aom_noise_model_t &model = this->model_;
  const int width = this->kWidth;
  const int height = this->kHeight;
  const int block_size = this->kBlockSize;
  // Create a gradient image with std = 2 uncorrelated noise
  const double kStd = 2;
  const int shift = this->kBitDepth - 8;

  for (int i = 0; i < width * height; ++i) {
    const uint8_t val = (i % width) < width / 2 ? 64 : 192;
    for (int c = 0; c < 3; ++c) {
      this->noise_ptr_[c][i] = randn(&this->random_, 1);
      this->data_ptr_[c][i] = ((uint8_t)(this->noise_ptr_[c][i] * kStd + val))
                              << shift;
      this->denoised_ptr_[c][i] = val << shift;
    }
  }
  this->flat_blocks_.assign(this->flat_blocks_.size(), 1);
  EXPECT_EQ(AOM_NOISE_STATUS_OK, this->NoiseModelUpdate());

  const int kNumBlocks = width * height / block_size / block_size;
  EXPECT_EQ(kNumBlocks, model.latest_state[0].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model.latest_state[1].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model.latest_state[2].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model.combined_state[0].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model.combined_state[1].strength_solver.num_equations);
  EXPECT_EQ(kNumBlocks, model.combined_state[2].strength_solver.num_equations);

  // Bump up noise by an insignificant amount
  for (int i = 0; i < width * height; ++i) {
    const uint8_t val = (i % width) < width / 2 ? 64 : 192;
    this->data_ptr_[0][i] =
        ((uint8_t)(this->noise_ptr_[0][i] * (kStd + 0.085) + val)) << shift;
  }
  EXPECT_EQ(AOM_NOISE_STATUS_OK, this->NoiseModelUpdate());

  const double kARGainTolerance = 0.02;
  for (int c = 0; c < 3; ++c) {
    EXPECT_EQ(kNumBlocks, model.latest_state[c].strength_solver.num_equations);
    EXPECT_EQ(15250, model.latest_state[c].num_observations);
    EXPECT_NEAR(1, model.latest_state[c].ar_gain, kARGainTolerance);

    EXPECT_EQ(2 * kNumBlocks,
              model.combined_state[c].strength_solver.num_equations);
    EXPECT_EQ(2 * 15250, model.combined_state[c].num_observations);
    EXPECT_NEAR(1, model.combined_state[c].ar_gain, kARGainTolerance);
  }

  // Bump up the noise strength on half the image for one channel by a
  // significant amount.
  for (int i = 0; i < width * height; ++i) {
    const uint8_t val = (i % width) < width / 2 ? 64 : 128;
    if (i % width < width / 2) {
      this->data_ptr_[0][i] =
          ((uint8_t)(randn(&this->random_, kStd + 0.5) + val)) << shift;
    }
  }
  EXPECT_EQ(AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE, this->NoiseModelUpdate());

  // Since we didn't update the combined state, it should still be at 2 *
  // num_blocks
  EXPECT_EQ(kNumBlocks, model.latest_state[0].strength_solver.num_equations);
  EXPECT_EQ(2 * kNumBlocks,
            model.combined_state[0].strength_solver.num_equations);

  // In normal operation, the "latest" estimate can be saved to the "combined"
  // state for continued updates.
  aom_noise_model_save_latest(&model);
  for (int c = 0; c < 3; ++c) {
    EXPECT_EQ(kNumBlocks, model.latest_state[c].strength_solver.num_equations);
    EXPECT_EQ(15250, model.latest_state[c].num_observations);
    EXPECT_NEAR(1, model.latest_state[c].ar_gain, kARGainTolerance);

    EXPECT_EQ(kNumBlocks,
              model.combined_state[c].strength_solver.num_equations);
    EXPECT_EQ(15250, model.combined_state[c].num_observations);
    EXPECT_NEAR(1, model.combined_state[c].ar_gain, kARGainTolerance);
  }
}

TYPED_TEST_P(NoiseModelUpdateTest, NoiseCoeffsSignalsDifferentNoiseType) {
  aom_noise_model_t &model = this->model_;
  const int width = this->kWidth;
  const int height = this->kHeight;
  const double kCoeffs[2][24] = {
    { 0.02884, -0.03356, 0.00633,  0.01757,  0.02849,  -0.04620,
      0.02833, -0.07178, 0.07076,  -0.11603, -0.10413, -0.16571,
      0.05158, -0.07969, 0.02640,  -0.07191, 0.02530,  0.41968,
      0.21450, -0.00702, -0.01401, -0.03676, -0.08713, 0.44196 },
    { 0.00269, -0.01291, -0.01513, 0.07234,  0.03208,   0.00477,
      0.00226, -0.00254, 0.03533,  0.12841,  -0.25970,  -0.06336,
      0.05238, -0.00845, -0.03118, 0.09043,  -0.36558,  0.48903,
      0.00595, -0.11938, 0.02106,  0.095956, -0.350139, 0.59305 }
  };

  noise_synth(&this->random_, model.params.lag, model.n, model.coords,
              kCoeffs[0], this->noise_ptr_[0], width, height);
  for (int i = 0; i < width * height; ++i) {
    this->data_ptr_[0][i] = (uint8_t)(128 + this->noise_ptr_[0][i]);
  }
  this->flat_blocks_.assign(this->flat_blocks_.size(), 1);
  EXPECT_EQ(AOM_NOISE_STATUS_OK, this->NoiseModelUpdate());

  // Now try with the second set of AR coefficients
  noise_synth(&this->random_, model.params.lag, model.n, model.coords,
              kCoeffs[1], this->noise_ptr_[0], width, height);
  for (int i = 0; i < width * height; ++i) {
    this->data_ptr_[0][i] = (uint8_t)(128 + this->noise_ptr_[0][i]);
  }
  EXPECT_EQ(AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE, this->NoiseModelUpdate());
}
REGISTER_TYPED_TEST_SUITE_P(NoiseModelUpdateTest, UpdateFailsNoFlatBlocks,
                            UpdateSuccessForZeroNoiseAllFlat,
                            UpdateFailsBlockSizeTooSmall,
                            UpdateSuccessForWhiteRandomNoise,
                            UpdateSuccessForScaledWhiteNoise,
                            UpdateSuccessForCorrelatedNoise,
                            NoiseStrengthChangeSignalsDifferentNoiseType,
                            NoiseCoeffsSignalsDifferentNoiseType);

INSTANTIATE_TYPED_TEST_SUITE_P(NoiseModelUpdateTestInstatiation,
                               NoiseModelUpdateTest, AllBitDepthParams);

TEST(NoiseModelGetGrainParameters, TestLagSize) {
  aom_film_grain_t film_grain;
  for (int lag = 1; lag <= 3; ++lag) {
    aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, lag, 8, 0 };
    aom_noise_model_t model;
    EXPECT_TRUE(aom_noise_model_init(&model, params));
    EXPECT_TRUE(aom_noise_model_get_grain_parameters(&model, &film_grain));
    EXPECT_EQ(lag, film_grain.ar_coeff_lag);
    aom_noise_model_free(&model);
  }

  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 4, 8, 0 };
  aom_noise_model_t model;
  EXPECT_TRUE(aom_noise_model_init(&model, params));
  EXPECT_FALSE(aom_noise_model_get_grain_parameters(&model, &film_grain));
  aom_noise_model_free(&model);
}

TEST(NoiseModelGetGrainParameters, TestARCoeffShiftBounds) {
  struct TestCase {
    double max_input_value;
    int expected_ar_coeff_shift;
    int expected_value;
  };
  const int lag = 1;
  const int kNumTestCases = 19;
  const TestCase test_cases[] = {
    // Test cases for ar_coeff_shift = 9
    { 0, 9, 0 },
    { 0.125, 9, 64 },
    { -0.125, 9, -64 },
    { 0.2499, 9, 127 },
    { -0.25, 9, -128 },
    // Test cases for ar_coeff_shift = 8
    { 0.25, 8, 64 },
    { -0.2501, 8, -64 },
    { 0.499, 8, 127 },
    { -0.5, 8, -128 },
    // Test cases for ar_coeff_shift = 7
    { 0.5, 7, 64 },
    { -0.5001, 7, -64 },
    { 0.999, 7, 127 },
    { -1, 7, -128 },
    // Test cases for ar_coeff_shift = 6
    { 1.0, 6, 64 },
    { -1.0001, 6, -64 },
    { 2.0, 6, 127 },
    { -2.0, 6, -128 },
    { 4, 6, 127 },
    { -4, 6, -128 },
  };
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, lag, 8, 0 };
  aom_noise_model_t model;
  EXPECT_TRUE(aom_noise_model_init(&model, params));

  for (int i = 0; i < kNumTestCases; ++i) {
    const TestCase &test_case = test_cases[i];
    model.combined_state[0].eqns.x[0] = test_case.max_input_value;

    aom_film_grain_t film_grain;
    EXPECT_TRUE(aom_noise_model_get_grain_parameters(&model, &film_grain));
    EXPECT_EQ(1, film_grain.ar_coeff_lag);
    EXPECT_EQ(test_case.expected_ar_coeff_shift, film_grain.ar_coeff_shift);
    EXPECT_EQ(test_case.expected_value, film_grain.ar_coeffs_y[0]);
  }
  aom_noise_model_free(&model);
}

TEST(NoiseModelGetGrainParameters, TestNoiseStrengthShiftBounds) {
  struct TestCase {
    double max_input_value;
    int expected_scaling_shift;
    int expected_value;
  };
  const int kNumTestCases = 10;
  const TestCase test_cases[] = {
    { 0, 11, 0 },      { 1, 11, 64 },     { 2, 11, 128 }, { 3.99, 11, 255 },
    { 4, 10, 128 },    { 7.99, 10, 255 }, { 8, 9, 128 },  { 16, 8, 128 },
    { 31.99, 8, 255 }, { 64, 8, 255 },  // clipped
  };
  const int lag = 1;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, lag, 8, 0 };
  aom_noise_model_t model;
  EXPECT_TRUE(aom_noise_model_init(&model, params));

  for (int i = 0; i < kNumTestCases; ++i) {
    const TestCase &test_case = test_cases[i];
    aom_equation_system_t &eqns = model.combined_state[0].strength_solver.eqns;
    // Set the fitted scale parameters to be a constant value.
    for (int j = 0; j < eqns.n; ++j) {
      eqns.x[j] = test_case.max_input_value;
    }
    aom_film_grain_t film_grain;
    EXPECT_TRUE(aom_noise_model_get_grain_parameters(&model, &film_grain));
    // We expect a single constant segemnt
    EXPECT_EQ(test_case.expected_scaling_shift, film_grain.scaling_shift);
    EXPECT_EQ(test_case.expected_value, film_grain.scaling_points_y[0][1]);
    EXPECT_EQ(test_case.expected_value, film_grain.scaling_points_y[1][1]);
  }
  aom_noise_model_free(&model);
}

// The AR coefficients are the same inputs used to generate "Test 2" in the test
// vectors
TEST(NoiseModelGetGrainParameters, GetGrainParametersReal) {
  const double kInputCoeffsY[] = { 0.0315,  0.0073,  0.0218,  0.00235, 0.00511,
                                   -0.0222, 0.0627,  -0.022,  0.05575, -0.1816,
                                   0.0107,  -0.1966, 0.00065, -0.0809, 0.04934,
                                   -0.1349, -0.0352, 0.41772, 0.27973, 0.04207,
                                   -0.0429, -0.1372, 0.06193, 0.52032 };
  const double kInputCoeffsCB[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5 };
  const double kInputCoeffsCR[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -0.5 };
  const int kExpectedARCoeffsY[] = { 4,  1,   3,  0,   1,  -3,  8, -3,
                                     7,  -23, 1,  -25, 0,  -10, 6, -17,
                                     -5, 53,  36, 5,   -5, -18, 8, 67 };
  const int kExpectedARCoeffsCB[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 84 };
  const int kExpectedARCoeffsCR[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   0,
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -126 };
  // Scaling function is initialized analytically with a sqrt function.
  const int kNumScalingPointsY = 12;
  const int kExpectedScalingPointsY[][2] = {
    { 0, 0 },     { 13, 44 },   { 27, 62 },   { 40, 76 },
    { 54, 88 },   { 67, 98 },   { 94, 117 },  { 121, 132 },
    { 148, 146 }, { 174, 159 }, { 201, 171 }, { 255, 192 },
  };

  const int lag = 3;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, lag, 8, 0 };
  aom_noise_model_t model;
  EXPECT_TRUE(aom_noise_model_init(&model, params));

  // Setup the AR coeffs
  memcpy(model.combined_state[0].eqns.x, kInputCoeffsY, sizeof(kInputCoeffsY));
  memcpy(model.combined_state[1].eqns.x, kInputCoeffsCB,
         sizeof(kInputCoeffsCB));
  memcpy(model.combined_state[2].eqns.x, kInputCoeffsCR,
         sizeof(kInputCoeffsCR));
  for (int i = 0; i < model.combined_state[0].strength_solver.num_bins; ++i) {
    const double x =
        ((double)i) / (model.combined_state[0].strength_solver.num_bins - 1.0);
    model.combined_state[0].strength_solver.eqns.x[i] = 6 * sqrt(x);
    model.combined_state[1].strength_solver.eqns.x[i] = 3;
    model.combined_state[2].strength_solver.eqns.x[i] = 2;

    // Inject some observations into the strength solver, as during film grain
    // parameter extraction an estimate of the average strength will be used to
    // adjust correlation.
    const int n = model.combined_state[0].strength_solver.num_bins;
    for (int j = 0; j < model.combined_state[0].strength_solver.num_bins; ++j) {
      model.combined_state[0].strength_solver.eqns.A[i * n + j] = 1;
      model.combined_state[1].strength_solver.eqns.A[i * n + j] = 1;
      model.combined_state[2].strength_solver.eqns.A[i * n + j] = 1;
    }
  }

  aom_film_grain_t film_grain;
  EXPECT_TRUE(aom_noise_model_get_grain_parameters(&model, &film_grain));
  EXPECT_EQ(lag, film_grain.ar_coeff_lag);
  EXPECT_EQ(3, film_grain.ar_coeff_lag);
  EXPECT_EQ(7, film_grain.ar_coeff_shift);
  EXPECT_EQ(10, film_grain.scaling_shift);
  EXPECT_EQ(kNumScalingPointsY, film_grain.num_y_points);
  EXPECT_EQ(1, film_grain.update_parameters);
  EXPECT_EQ(1, film_grain.apply_grain);

  const int kNumARCoeffs = 24;
  for (int i = 0; i < kNumARCoeffs; ++i) {
    EXPECT_EQ(kExpectedARCoeffsY[i], film_grain.ar_coeffs_y[i]);
  }
  for (int i = 0; i < kNumARCoeffs + 1; ++i) {
    EXPECT_EQ(kExpectedARCoeffsCB[i], film_grain.ar_coeffs_cb[i]);
  }
  for (int i = 0; i < kNumARCoeffs + 1; ++i) {
    EXPECT_EQ(kExpectedARCoeffsCR[i], film_grain.ar_coeffs_cr[i]);
  }
  for (int i = 0; i < kNumScalingPointsY; ++i) {
    EXPECT_EQ(kExpectedScalingPointsY[i][0], film_grain.scaling_points_y[i][0]);
    EXPECT_EQ(kExpectedScalingPointsY[i][1], film_grain.scaling_points_y[i][1]);
  }

  // CB strength should just be a piecewise segment
  EXPECT_EQ(2, film_grain.num_cb_points);
  EXPECT_EQ(0, film_grain.scaling_points_cb[0][0]);
  EXPECT_EQ(255, film_grain.scaling_points_cb[1][0]);
  EXPECT_EQ(96, film_grain.scaling_points_cb[0][1]);
  EXPECT_EQ(96, film_grain.scaling_points_cb[1][1]);

  // CR strength should just be a piecewise segment
  EXPECT_EQ(2, film_grain.num_cr_points);
  EXPECT_EQ(0, film_grain.scaling_points_cr[0][0]);
  EXPECT_EQ(255, film_grain.scaling_points_cr[1][0]);
  EXPECT_EQ(64, film_grain.scaling_points_cr[0][1]);
  EXPECT_EQ(64, film_grain.scaling_points_cr[1][1]);

  EXPECT_EQ(128, film_grain.cb_mult);
  EXPECT_EQ(192, film_grain.cb_luma_mult);
  EXPECT_EQ(256, film_grain.cb_offset);
  EXPECT_EQ(128, film_grain.cr_mult);
  EXPECT_EQ(192, film_grain.cr_luma_mult);
  EXPECT_EQ(256, film_grain.cr_offset);
  EXPECT_EQ(0, film_grain.chroma_scaling_from_luma);
  EXPECT_EQ(0, film_grain.grain_scale_shift);

  aom_noise_model_free(&model);
}

template <typename T>
class WienerDenoiseTest : public ::testing::Test, public T {
 public:
  static void SetUpTestSuite() { aom_dsp_rtcd(); }

 protected:
  void SetUp() override {
    static const float kNoiseLevel = 5.f;
    static const float kStd = 4.0;
    static const double kMaxValue = (1 << T::kBitDepth) - 1;

    chroma_sub_[0] = 1;
    chroma_sub_[1] = 1;
    stride_[0] = kWidth;
    stride_[1] = kWidth / 2;
    stride_[2] = kWidth / 2;
    for (int k = 0; k < 3; ++k) {
      data_[k].resize(kWidth * kHeight);
      denoised_[k].resize(kWidth * kHeight);
      noise_psd_[k].resize(kBlockSize * kBlockSize);
    }

    const double kCoeffsY[] = { 0.0406, -0.116, -0.078, -0.152, 0.0033, -0.093,
                                0.048,  0.404,  0.2353, -0.035, -0.093, 0.441 };
    const int kCoords[12][2] = {
      { -2, -2 }, { -1, -2 }, { 0, -2 }, { 1, -2 }, { 2, -2 }, { -2, -1 },
      { -1, -1 }, { 0, -1 },  { 1, -1 }, { 2, -1 }, { -2, 0 }, { -1, 0 }
    };
    const int kLag = 2;
    const int kLength = 12;
    libaom_test::ACMRandom random;
    std::vector<double> noise(kWidth * kHeight);
    noise_synth(&random, kLag, kLength, kCoords, kCoeffsY, &noise[0], kWidth,
                kHeight);
    noise_psd_[0] = get_noise_psd(&noise[0], kWidth, kHeight, kBlockSize);
    for (int i = 0; i < kBlockSize * kBlockSize; ++i) {
      noise_psd_[0][i] = (float)(noise_psd_[0][i] * kStd * kStd * kScaleNoise *
                                 kScaleNoise / (kMaxValue * kMaxValue));
    }

    float psd_value =
        aom_noise_psd_get_default_value(kBlockSizeChroma, kNoiseLevel);
    for (int i = 0; i < kBlockSizeChroma * kBlockSizeChroma; ++i) {
      noise_psd_[1][i] = psd_value;
      noise_psd_[2][i] = psd_value;
    }
    for (int y = 0; y < kHeight; ++y) {
      for (int x = 0; x < kWidth; ++x) {
        data_[0][y * stride_[0] + x] = (typename T::data_type_t)fclamp(
            (x + noise[y * stride_[0] + x] * kStd) * kScaleNoise, 0, kMaxValue);
      }
    }

    for (int c = 1; c < 3; ++c) {
      for (int y = 0; y < (kHeight >> 1); ++y) {
        for (int x = 0; x < (kWidth >> 1); ++x) {
          data_[c][y * stride_[c] + x] = (typename T::data_type_t)fclamp(
              (x + randn(&random, kStd)) * kScaleNoise, 0, kMaxValue);
        }
      }
    }
    for (int k = 0; k < 3; ++k) {
      noise_psd_ptrs_[k] = &noise_psd_[k][0];
    }
  }
  static const int kBlockSize = 32;
  static const int kBlockSizeChroma = 16;
  static const int kWidth = 256;
  static const int kHeight = 256;
  static const int kScaleNoise = 1 << (T::kBitDepth - 8);

  std::vector<typename T::data_type_t> data_[3];
  std::vector<typename T::data_type_t> denoised_[3];
  std::vector<float> noise_psd_[3];
  int chroma_sub_[2];
  float *noise_psd_ptrs_[3];
  int stride_[3];
};

TYPED_TEST_SUITE_P(WienerDenoiseTest);

TYPED_TEST_P(WienerDenoiseTest, InvalidBlockSize) {
  const uint8_t *const data_ptrs[3] = {
    reinterpret_cast<uint8_t *>(&this->data_[0][0]),
    reinterpret_cast<uint8_t *>(&this->data_[1][0]),
    reinterpret_cast<uint8_t *>(&this->data_[2][0]),
  };
  uint8_t *denoised_ptrs[3] = {
    reinterpret_cast<uint8_t *>(&this->denoised_[0][0]),
    reinterpret_cast<uint8_t *>(&this->denoised_[1][0]),
    reinterpret_cast<uint8_t *>(&this->denoised_[2][0]),
  };
  EXPECT_EQ(0, aom_wiener_denoise_2d(data_ptrs, denoised_ptrs, this->kWidth,
                                     this->kHeight, this->stride_,
                                     this->chroma_sub_, this->noise_psd_ptrs_,
                                     18, this->kBitDepth, this->kUseHighBD));
  EXPECT_EQ(0, aom_wiener_denoise_2d(data_ptrs, denoised_ptrs, this->kWidth,
                                     this->kHeight, this->stride_,
                                     this->chroma_sub_, this->noise_psd_ptrs_,
                                     48, this->kBitDepth, this->kUseHighBD));
  EXPECT_EQ(0, aom_wiener_denoise_2d(data_ptrs, denoised_ptrs, this->kWidth,
                                     this->kHeight, this->stride_,
                                     this->chroma_sub_, this->noise_psd_ptrs_,
                                     64, this->kBitDepth, this->kUseHighBD));
}

TYPED_TEST_P(WienerDenoiseTest, InvalidChromaSubsampling) {
  const uint8_t *const data_ptrs[3] = {
    reinterpret_cast<uint8_t *>(&this->data_[0][0]),
    reinterpret_cast<uint8_t *>(&this->data_[1][0]),
    reinterpret_cast<uint8_t *>(&this->data_[2][0]),
  };
  uint8_t *denoised_ptrs[3] = {
    reinterpret_cast<uint8_t *>(&this->denoised_[0][0]),
    reinterpret_cast<uint8_t *>(&this->denoised_[1][0]),
    reinterpret_cast<uint8_t *>(&this->denoised_[2][0]),
  };
  int chroma_sub[2] = { 1, 0 };
  EXPECT_EQ(0, aom_wiener_denoise_2d(data_ptrs, denoised_ptrs, this->kWidth,
                                     this->kHeight, this->stride_, chroma_sub,
                                     this->noise_psd_ptrs_, 32, this->kBitDepth,
                                     this->kUseHighBD));

  chroma_sub[0] = 0;
  chroma_sub[1] = 1;
  EXPECT_EQ(0, aom_wiener_denoise_2d(data_ptrs, denoised_ptrs, this->kWidth,
                                     this->kHeight, this->stride_, chroma_sub,
                                     this->noise_psd_ptrs_, 32, this->kBitDepth,
                                     this->kUseHighBD));
}

TYPED_TEST_P(WienerDenoiseTest, GradientTest) {
  const int width = this->kWidth;
  const int height = this->kHeight;
  const int block_size = this->kBlockSize;
  const uint8_t *const data_ptrs[3] = {
    reinterpret_cast<uint8_t *>(&this->data_[0][0]),
    reinterpret_cast<uint8_t *>(&this->data_[1][0]),
    reinterpret_cast<uint8_t *>(&this->data_[2][0]),
  };
  uint8_t *denoised_ptrs[3] = {
    reinterpret_cast<uint8_t *>(&this->denoised_[0][0]),
    reinterpret_cast<uint8_t *>(&this->denoised_[1][0]),
    reinterpret_cast<uint8_t *>(&this->denoised_[2][0]),
  };
  const int ret = aom_wiener_denoise_2d(
      data_ptrs, denoised_ptrs, width, height, this->stride_, this->chroma_sub_,
      this->noise_psd_ptrs_, block_size, this->kBitDepth, this->kUseHighBD);
  EXPECT_EQ(1, ret);

  // Check the noise on the denoised image (from the analytical gradient)
  // and make sure that it is less than what we added.
  for (int c = 0; c < 3; ++c) {
    std::vector<double> measured_noise(width * height);

    double var = 0;
    const int shift = (c > 0);
    for (int x = 0; x < (width >> shift); ++x) {
      for (int y = 0; y < (height >> shift); ++y) {
        const double diff = this->denoised_[c][y * this->stride_[c] + x] -
                            x * this->kScaleNoise;
        var += diff * diff;
        measured_noise[y * width + x] = diff;
      }
    }
    var /= (width * height);
    const double std = sqrt(std::max(0.0, var));
    EXPECT_LE(std, 1.25f * this->kScaleNoise);
    if (c == 0) {
      std::vector<float> measured_psd =
          get_noise_psd(&measured_noise[0], width, height, block_size);
      std::vector<double> measured_psd_d(block_size * block_size);
      std::vector<double> noise_psd_d(block_size * block_size);
      std::copy(measured_psd.begin(), measured_psd.end(),
                measured_psd_d.begin());
      std::copy(this->noise_psd_[0].begin(), this->noise_psd_[0].end(),
                noise_psd_d.begin());
      EXPECT_LT(
          aom_normalized_cross_correlation(&measured_psd_d[0], &noise_psd_d[0],
                                           (int)(noise_psd_d.size())),
          0.35);
    }
  }
}

REGISTER_TYPED_TEST_SUITE_P(WienerDenoiseTest, InvalidBlockSize,
                            InvalidChromaSubsampling, GradientTest);

INSTANTIATE_TYPED_TEST_SUITE_P(WienerDenoiseTestInstatiation, WienerDenoiseTest,
                               AllBitDepthParams);
