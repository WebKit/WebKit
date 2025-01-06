/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gtest/gtest.h"
#include "test/register_state_check.h"
#include "test/function_equivalence_test.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom/aom_integer.h"
#include "av1/common/enums.h"

using libaom_test::FunctionEquivalenceTest;

namespace {

template <typename F, typename T>
class UpsampleTest : public FunctionEquivalenceTest<F> {
 protected:
  static const int kIterations = 1000000;
  static const int kMinEdge = 4;
  static const int kMaxEdge = 24;
  static const int kBufSize = 2 * 64 + 32;
  static const int kOffset = 16;

  ~UpsampleTest() override = default;

  virtual void Execute(T *edge_tst) = 0;

  void Common() {
    edge_ref_ = &edge_ref_data_[kOffset];
    edge_tst_ = &edge_tst_data_[kOffset];

    Execute(edge_tst_);

    const int max_idx = (size_ - 1) * 2;
    for (int r = -2; r <= max_idx; ++r) {
      ASSERT_EQ(edge_ref_[r], edge_tst_[r]);
    }
  }

  T edge_ref_data_[kBufSize];
  T edge_tst_data_[kBufSize];

  T *edge_ref_;
  T *edge_tst_;

  int size_;
};

typedef void (*UP8B)(uint8_t *p, int size);
typedef libaom_test::FuncParam<UP8B> TestFuncs;

class UpsampleTest8B : public UpsampleTest<UP8B, uint8_t> {
 protected:
  void Execute(uint8_t *edge_tst) override {
    params_.ref_func(edge_ref_, size_);
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst, size_));
  }
};

TEST_P(UpsampleTest8B, RandomValues) {
  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    size_ = 4 * (this->rng_(4) + 1);

    int i, pix = 0;
    for (i = 0; i < kOffset + size_; ++i) {
      pix = rng_.Rand8();
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = edge_ref_data_[i];
    }

    // Extend final sample
    while (i < kBufSize) {
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
      i++;
    }

    Common();
  }
}

TEST_P(UpsampleTest8B, DISABLED_Speed) {
  const int test_count = 10000000;
  size_ = kMaxEdge;
  for (int i = 0; i < kOffset + size_; ++i) {
    edge_tst_data_[i] = rng_.Rand8();
  }
  edge_tst_ = &edge_tst_data_[kOffset];
  for (int iter = 0; iter < test_count; ++iter) {
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst_, size_));
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, UpsampleTest8B,
    ::testing::Values(TestFuncs(av1_upsample_intra_edge_c,
                                av1_upsample_intra_edge_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, UpsampleTest8B,
    ::testing::Values(TestFuncs(av1_upsample_intra_edge_c,
                                av1_upsample_intra_edge_neon)));
#endif  // HAVE_NEON

template <typename F, typename T>
class FilterEdgeTest : public FunctionEquivalenceTest<F> {
 protected:
  static const int kIterations = 1000000;
  static const int kMaxEdge = 2 * 64;
  static const int kBufSize = kMaxEdge + 32;
  static const int kOffset = 15;

  ~FilterEdgeTest() override = default;

  virtual void Execute(T *edge_tst) = 0;

  void Common() {
    edge_ref_ = &edge_ref_data_[kOffset];
    edge_tst_ = &edge_tst_data_[kOffset];

    Execute(edge_tst_);

    for (int r = 0; r < size_; ++r) {
      ASSERT_EQ(edge_ref_[r], edge_tst_[r]);
    }
  }

  T edge_ref_data_[kBufSize];
  T edge_tst_data_[kBufSize];

  T *edge_ref_;
  T *edge_tst_;

  int size_;
  int strength_;
};

typedef void (*FE8B)(uint8_t *p, int size, int strength);
typedef libaom_test::FuncParam<FE8B> FilterEdgeTestFuncs;

class FilterEdgeTest8B : public FilterEdgeTest<FE8B, uint8_t> {
 protected:
  void Execute(uint8_t *edge_tst) override {
    params_.ref_func(edge_ref_, size_, strength_);
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst, size_, strength_));
  }
};

TEST_P(FilterEdgeTest8B, RandomValues) {
  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    strength_ = this->rng_(4);
    size_ = 4 * (this->rng_(128 / 4) + 1) + 1;

    int i, pix = 0;
    for (i = 0; i < kOffset + size_; ++i) {
      pix = rng_.Rand8();
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
    }

    Common();
  }
}

TEST_P(FilterEdgeTest8B, DISABLED_Speed) {
  const int test_count = 10000000;
  size_ = kMaxEdge;
  strength_ = 1;
  for (int i = 0; i < kOffset + size_; ++i) {
    edge_tst_data_[i] = rng_.Rand8();
  }
  edge_tst_ = &edge_tst_data_[kOffset];
  for (int iter = 0; iter < test_count; ++iter) {
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst_, size_, strength_));
    // iterate over filter strengths (1,2,3)
    strength_ = strength_ == 3 ? 1 : strength_ + 1;
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, FilterEdgeTest8B,
    ::testing::Values(FilterEdgeTestFuncs(av1_filter_intra_edge_c,
                                          av1_filter_intra_edge_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, FilterEdgeTest8B,
    ::testing::Values(FilterEdgeTestFuncs(av1_filter_intra_edge_c,
                                          av1_filter_intra_edge_neon)));
#endif  // HAVE_NEON

#if CONFIG_AV1_HIGHBITDEPTH

typedef void (*UPHB)(uint16_t *p, int size, int bd);
typedef libaom_test::FuncParam<UPHB> TestFuncsHBD;

class UpsampleTestHB : public UpsampleTest<UPHB, uint16_t> {
 protected:
  void Execute(uint16_t *edge_tst) override {
    params_.ref_func(edge_ref_, size_, bit_depth_);
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst, size_, bit_depth_));
  }
  int bit_depth_;
};

TEST_P(UpsampleTestHB, RandomValues) {
  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    switch (rng_(3)) {
      case 0: bit_depth_ = 8; break;
      case 1: bit_depth_ = 10; break;
      default: bit_depth_ = 12; break;
    }
    const int hi = 1 << bit_depth_;

    size_ = 4 * (this->rng_(4) + 1);

    int i, pix = 0;
    for (i = 0; i < kOffset + size_; ++i) {
      pix = rng_(hi);
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
    }

    // Extend final sample
    while (i < kBufSize) {
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
      i++;
    }

    Common();
  }
}

TEST_P(UpsampleTestHB, DISABLED_Speed) {
  const int test_count = 10000000;
  size_ = kMaxEdge;
  bit_depth_ = 12;
  const int hi = 1 << bit_depth_;
  for (int i = 0; i < kOffset + size_; ++i) {
    edge_tst_data_[i] = rng_(hi);
  }
  edge_tst_ = &edge_tst_data_[kOffset];
  for (int iter = 0; iter < test_count; ++iter) {
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst_, size_, bit_depth_));
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, UpsampleTestHB,
    ::testing::Values(TestFuncsHBD(av1_highbd_upsample_intra_edge_c,
                                   av1_highbd_upsample_intra_edge_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, UpsampleTestHB,
    ::testing::Values(TestFuncsHBD(av1_highbd_upsample_intra_edge_c,
                                   av1_highbd_upsample_intra_edge_neon)));
#endif  // HAVE_NEON

typedef void (*FEHB)(uint16_t *p, int size, int strength);
typedef libaom_test::FuncParam<FEHB> FilterEdgeTestFuncsHBD;

class FilterEdgeTestHB : public FilterEdgeTest<FEHB, uint16_t> {
 protected:
  void Execute(uint16_t *edge_tst) override {
    params_.ref_func(edge_ref_, size_, strength_);
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst, size_, strength_));
  }
  int bit_depth_;
};

TEST_P(FilterEdgeTestHB, RandomValues) {
  for (int iter = 0; iter < kIterations && !HasFatalFailure(); ++iter) {
    switch (rng_(3)) {
      case 0: bit_depth_ = 8; break;
      case 1: bit_depth_ = 10; break;
      default: bit_depth_ = 12; break;
    }
    const int hi = 1 << bit_depth_;
    strength_ = this->rng_(4);
    size_ = 4 * (this->rng_(128 / 4) + 1) + 1;

    int i, pix = 0;
    for (i = 0; i < kOffset + size_; ++i) {
      pix = rng_(hi);
      edge_ref_data_[i] = pix;
      edge_tst_data_[i] = pix;
    }

    Common();
  }
}

TEST_P(FilterEdgeTestHB, DISABLED_Speed) {
  const int test_count = 10000000;
  size_ = kMaxEdge;
  strength_ = 1;
  bit_depth_ = 12;
  const int hi = 1 << bit_depth_;
  for (int i = 0; i < kOffset + size_; ++i) {
    edge_tst_data_[i] = rng_(hi);
  }
  edge_tst_ = &edge_tst_data_[kOffset];
  for (int iter = 0; iter < test_count; ++iter) {
    API_REGISTER_STATE_CHECK(params_.tst_func(edge_tst_, size_, strength_));
    // iterate over filter strengths (1,2,3)
    strength_ = strength_ == 3 ? 1 : strength_ + 1;
  }
}

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, FilterEdgeTestHB,
                         ::testing::Values(FilterEdgeTestFuncsHBD(
                             av1_highbd_filter_intra_edge_c,
                             av1_highbd_filter_intra_edge_sse4_1)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, FilterEdgeTestHB,
                         ::testing::Values(FilterEdgeTestFuncsHBD(
                             av1_highbd_filter_intra_edge_c,
                             av1_highbd_filter_intra_edge_neon)));
#endif  // HAVE_NEON

#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
