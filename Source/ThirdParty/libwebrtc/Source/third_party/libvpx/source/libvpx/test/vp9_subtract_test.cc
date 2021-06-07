/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vp9_rtcd.h"
#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/bench.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "vp9/common/vp9_blockd.h"
#include "vpx_ports/msvc.h"
#include "vpx_mem/vpx_mem.h"

typedef void (*SubtractFunc)(int rows, int cols, int16_t *diff_ptr,
                             ptrdiff_t diff_stride, const uint8_t *src_ptr,
                             ptrdiff_t src_stride, const uint8_t *pred_ptr,
                             ptrdiff_t pred_stride);

namespace vp9 {

class VP9SubtractBlockTest : public AbstractBench,
                             public ::testing::TestWithParam<SubtractFunc> {
 public:
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  virtual void Run() {
    GetParam()(block_height_, block_width_, diff_, block_width_, src_,
               block_width_, pred_, block_width_);
  }

  void SetupBlocks(BLOCK_SIZE bsize) {
    block_width_ = 4 * num_4x4_blocks_wide_lookup[bsize];
    block_height_ = 4 * num_4x4_blocks_high_lookup[bsize];
    diff_ = reinterpret_cast<int16_t *>(
        vpx_memalign(16, sizeof(*diff_) * block_width_ * block_height_ * 2));
    pred_ = reinterpret_cast<uint8_t *>(
        vpx_memalign(16, block_width_ * block_height_ * 2));
    src_ = reinterpret_cast<uint8_t *>(
        vpx_memalign(16, block_width_ * block_height_ * 2));
  }

  int block_width_;
  int block_height_;
  int16_t *diff_;
  uint8_t *pred_;
  uint8_t *src_;
};

using libvpx_test::ACMRandom;

TEST_P(VP9SubtractBlockTest, DISABLED_Speed) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());

  for (BLOCK_SIZE bsize = BLOCK_4X4; bsize < BLOCK_SIZES;
       bsize = static_cast<BLOCK_SIZE>(static_cast<int>(bsize) + 1)) {
    SetupBlocks(bsize);

    RunNTimes(100000000 / (block_height_ * block_width_));
    char block_size[16];
    snprintf(block_size, sizeof(block_size), "%dx%d", block_height_,
             block_width_);
    char title[100];
    snprintf(title, sizeof(title), "%8s ", block_size);
    PrintMedian(title);

    vpx_free(diff_);
    vpx_free(pred_);
    vpx_free(src_);
  }
}

TEST_P(VP9SubtractBlockTest, SimpleSubtract) {
  ACMRandom rnd(ACMRandom::DeterministicSeed());

  for (BLOCK_SIZE bsize = BLOCK_4X4; bsize < BLOCK_SIZES;
       bsize = static_cast<BLOCK_SIZE>(static_cast<int>(bsize) + 1)) {
    SetupBlocks(bsize);

    for (int n = 0; n < 100; n++) {
      for (int r = 0; r < block_height_; ++r) {
        for (int c = 0; c < block_width_ * 2; ++c) {
          src_[r * block_width_ * 2 + c] = rnd.Rand8();
          pred_[r * block_width_ * 2 + c] = rnd.Rand8();
        }
      }

      GetParam()(block_height_, block_width_, diff_, block_width_, src_,
                 block_width_, pred_, block_width_);

      for (int r = 0; r < block_height_; ++r) {
        for (int c = 0; c < block_width_; ++c) {
          EXPECT_EQ(diff_[r * block_width_ + c],
                    (src_[r * block_width_ + c] - pred_[r * block_width_ + c]))
              << "r = " << r << ", c = " << c
              << ", bs = " << static_cast<int>(bsize);
        }
      }

      GetParam()(block_height_, block_width_, diff_, block_width_ * 2, src_,
                 block_width_ * 2, pred_, block_width_ * 2);

      for (int r = 0; r < block_height_; ++r) {
        for (int c = 0; c < block_width_; ++c) {
          EXPECT_EQ(diff_[r * block_width_ * 2 + c],
                    (src_[r * block_width_ * 2 + c] -
                     pred_[r * block_width_ * 2 + c]))
              << "r = " << r << ", c = " << c
              << ", bs = " << static_cast<int>(bsize);
        }
      }
    }
    vpx_free(diff_);
    vpx_free(pred_);
    vpx_free(src_);
  }
}

INSTANTIATE_TEST_SUITE_P(C, VP9SubtractBlockTest,
                         ::testing::Values(vpx_subtract_block_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, VP9SubtractBlockTest,
                         ::testing::Values(vpx_subtract_block_sse2));
#endif
#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, VP9SubtractBlockTest,
                         ::testing::Values(vpx_subtract_block_neon));
#endif
#if HAVE_MSA
INSTANTIATE_TEST_SUITE_P(MSA, VP9SubtractBlockTest,
                         ::testing::Values(vpx_subtract_block_msa));
#endif

#if HAVE_MMI
INSTANTIATE_TEST_SUITE_P(MMI, VP9SubtractBlockTest,
                         ::testing::Values(vpx_subtract_block_mmi));
#endif

#if HAVE_VSX
INSTANTIATE_TEST_SUITE_P(VSX, VP9SubtractBlockTest,
                         ::testing::Values(vpx_subtract_block_vsx));
#endif

}  // namespace vp9
