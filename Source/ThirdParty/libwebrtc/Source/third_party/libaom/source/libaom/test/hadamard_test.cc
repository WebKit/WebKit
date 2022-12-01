/*
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <ostream>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/aom_dsp_rtcd.h"

#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {

using libaom_test::ACMRandom;

using HadamardFunc = void (*)(const int16_t *a, ptrdiff_t a_stride,
                              tran_low_t *b);
// Low precision version of Hadamard Transform
using HadamardLPFunc = void (*)(const int16_t *a, ptrdiff_t a_stride,
                                int16_t *b);
// Low precision version of Hadamard Transform 8x8 - Dual
using HadamardLP8x8DualFunc = void (*)(const int16_t *a, ptrdiff_t a_stride,
                                       int16_t *b);

template <typename OutputType>
void Hadamard4x4(const OutputType *a, OutputType *out) {
  OutputType b[8];
  for (int i = 0; i < 4; i += 2) {
    b[i + 0] = (a[i * 4] + a[(i + 1) * 4]) >> 1;
    b[i + 1] = (a[i * 4] - a[(i + 1) * 4]) >> 1;
  }

  out[0] = b[0] + b[2];
  out[1] = b[1] + b[3];
  out[2] = b[0] - b[2];
  out[3] = b[1] - b[3];
}

template <typename OutputType>
void ReferenceHadamard4x4(const int16_t *a, int a_stride, OutputType *b) {
  OutputType input[16];
  OutputType buf[16];
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      input[i * 4 + j] = static_cast<OutputType>(a[i * a_stride + j]);
    }
  }
  for (int i = 0; i < 4; ++i) Hadamard4x4(input + i, buf + i * 4);
  for (int i = 0; i < 4; ++i) Hadamard4x4(buf + i, b + i * 4);
}

template <typename OutputType>
void HadamardLoop(const OutputType *a, OutputType *out) {
  OutputType b[8];
  for (int i = 0; i < 8; i += 2) {
    b[i + 0] = a[i * 8] + a[(i + 1) * 8];
    b[i + 1] = a[i * 8] - a[(i + 1) * 8];
  }
  OutputType c[8];
  for (int i = 0; i < 8; i += 4) {
    c[i + 0] = b[i + 0] + b[i + 2];
    c[i + 1] = b[i + 1] + b[i + 3];
    c[i + 2] = b[i + 0] - b[i + 2];
    c[i + 3] = b[i + 1] - b[i + 3];
  }
  out[0] = c[0] + c[4];
  out[7] = c[1] + c[5];
  out[3] = c[2] + c[6];
  out[4] = c[3] + c[7];
  out[2] = c[0] - c[4];
  out[6] = c[1] - c[5];
  out[1] = c[2] - c[6];
  out[5] = c[3] - c[7];
}

template <typename OutputType>
void ReferenceHadamard8x8(const int16_t *a, int a_stride, OutputType *b) {
  OutputType input[64];
  OutputType buf[64];
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      input[i * 8 + j] = static_cast<OutputType>(a[i * a_stride + j]);
    }
  }
  for (int i = 0; i < 8; ++i) HadamardLoop(input + i, buf + i * 8);
  for (int i = 0; i < 8; ++i) HadamardLoop(buf + i, b + i * 8);
}

template <typename OutputType>
void ReferenceHadamard8x8Dual(const int16_t *a, int a_stride, OutputType *b) {
  /* The source is a 8x16 block. The destination is rearranged to 8x16.
   * Input is 9 bit. */
  ReferenceHadamard8x8(a, a_stride, b);
  ReferenceHadamard8x8(a + 8, a_stride, b + 64);
}

template <typename OutputType>
void ReferenceHadamard16x16(const int16_t *a, int a_stride, OutputType *b) {
  /* The source is a 16x16 block. The destination is rearranged to 8x32.
   * Input is 9 bit. */
  ReferenceHadamard8x8(a + 0 + 0 * a_stride, a_stride, b + 0);
  ReferenceHadamard8x8(a + 8 + 0 * a_stride, a_stride, b + 64);
  ReferenceHadamard8x8(a + 0 + 8 * a_stride, a_stride, b + 128);
  ReferenceHadamard8x8(a + 8 + 8 * a_stride, a_stride, b + 192);

  /* Overlay the 8x8 blocks and combine. */
  for (int i = 0; i < 64; ++i) {
    /* 8x8 steps the range up to 15 bits. */
    const OutputType a0 = b[0];
    const OutputType a1 = b[64];
    const OutputType a2 = b[128];
    const OutputType a3 = b[192];

    /* Prevent the result from escaping int16_t. */
    const OutputType b0 = (a0 + a1) >> 1;
    const OutputType b1 = (a0 - a1) >> 1;
    const OutputType b2 = (a2 + a3) >> 1;
    const OutputType b3 = (a2 - a3) >> 1;

    /* Store a 16 bit value. */
    b[0] = b0 + b2;
    b[64] = b1 + b3;
    b[128] = b0 - b2;
    b[192] = b1 - b3;

    ++b;
  }
}

template <typename OutputType>
void ReferenceHadamard32x32(const int16_t *a, int a_stride, OutputType *b) {
  ReferenceHadamard16x16(a + 0 + 0 * a_stride, a_stride, b + 0);
  ReferenceHadamard16x16(a + 16 + 0 * a_stride, a_stride, b + 256);
  ReferenceHadamard16x16(a + 0 + 16 * a_stride, a_stride, b + 512);
  ReferenceHadamard16x16(a + 16 + 16 * a_stride, a_stride, b + 768);

  for (int i = 0; i < 256; ++i) {
    const OutputType a0 = b[0];
    const OutputType a1 = b[256];
    const OutputType a2 = b[512];
    const OutputType a3 = b[768];

    const OutputType b0 = (a0 + a1) >> 2;
    const OutputType b1 = (a0 - a1) >> 2;
    const OutputType b2 = (a2 + a3) >> 2;
    const OutputType b3 = (a2 - a3) >> 2;

    b[0] = b0 + b2;
    b[256] = b1 + b3;
    b[512] = b0 - b2;
    b[768] = b1 - b3;

    ++b;
  }
}

template <typename OutputType>
void ReferenceHadamard(const int16_t *a, int a_stride, OutputType *b, int bw,
                       int bh) {
  if (bw == 32 && bh == 32) {
    ReferenceHadamard32x32(a, a_stride, b);
  } else if (bw == 16 && bh == 16) {
    ReferenceHadamard16x16(a, a_stride, b);
  } else if (bw == 8 && bh == 8) {
    ReferenceHadamard8x8(a, a_stride, b);
  } else if (bw == 4 && bh == 4) {
    ReferenceHadamard4x4(a, a_stride, b);
  } else if (bw == 8 && bh == 16) {
    ReferenceHadamard8x8Dual(a, a_stride, b);
  } else {
    GTEST_FAIL() << "Invalid Hadamard transform size " << bw << bh << std::endl;
  }
}

template <typename HadamardFuncType>
struct FuncWithSize {
  FuncWithSize(HadamardFuncType f, int bw, int bh)
      : func(f), block_width(bw), block_height(bh) {}
  HadamardFuncType func;
  int block_width;
  int block_height;
};

using HadamardFuncWithSize = FuncWithSize<HadamardFunc>;
using HadamardLPFuncWithSize = FuncWithSize<HadamardLPFunc>;
using HadamardLP8x8DualFuncWithSize = FuncWithSize<HadamardLP8x8DualFunc>;

template <typename OutputType, typename HadamardFuncType>
class HadamardTestBase
    : public ::testing::TestWithParam<FuncWithSize<HadamardFuncType>> {
 public:
  explicit HadamardTestBase(const FuncWithSize<HadamardFuncType> &func_param) {
    h_func_ = func_param.func;
    bw_ = func_param.block_width;
    bh_ = func_param.block_height;
  }

  virtual void SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

  virtual int16_t Rand() = 0;

  void CompareReferenceRandom() {
    const int kMaxBlockSize = 32 * 32;
    const int block_size_ = bw_ * bh_;

    DECLARE_ALIGNED(16, int16_t, a[kMaxBlockSize]);
    DECLARE_ALIGNED(16, OutputType, b[kMaxBlockSize]);
    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));

    OutputType b_ref[kMaxBlockSize];
    memset(b_ref, 0, sizeof(b_ref));

    for (int i = 0; i < block_size_; ++i) a[i] = Rand();

    ReferenceHadamard(a, bw_, b_ref, bw_, bh_);
    API_REGISTER_STATE_CHECK(h_func_(a, bw_, b));

    // The order of the output is not important. Sort before checking.
    std::sort(b, b + block_size_);
    std::sort(b_ref, b_ref + block_size_);
    EXPECT_EQ(memcmp(b, b_ref, sizeof(b)), 0);
  }

  void VaryStride() {
    const int kMaxBlockSize = 32 * 32;
    const int block_size_ = bw_ * bh_;

    DECLARE_ALIGNED(16, int16_t, a[kMaxBlockSize * 8]);
    DECLARE_ALIGNED(16, OutputType, b[kMaxBlockSize]);
    memset(a, 0, sizeof(a));
    for (int i = 0; i < block_size_ * 8; ++i) a[i] = Rand();

    OutputType b_ref[kMaxBlockSize];
    for (int i = 8; i < 64; i += 8) {
      memset(b, 0, sizeof(b));
      memset(b_ref, 0, sizeof(b_ref));

      ReferenceHadamard(a, i, b_ref, bw_, bh_);
      API_REGISTER_STATE_CHECK(h_func_(a, i, b));

      // The order of the output is not important. Sort before checking.
      std::sort(b, b + block_size_);
      std::sort(b_ref, b_ref + block_size_);
      EXPECT_EQ(0, memcmp(b, b_ref, sizeof(b)));
    }
  }

  void SpeedTest(int times) {
    const int kMaxBlockSize = 32 * 32;
    DECLARE_ALIGNED(16, int16_t, input[kMaxBlockSize]);
    DECLARE_ALIGNED(16, OutputType, output[kMaxBlockSize]);
    memset(input, 1, sizeof(input));
    memset(output, 0, sizeof(output));

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < times; ++i) {
      h_func_(input, bw_, output);
    }
    aom_usec_timer_mark(&timer);

    const int elapsed_time = static_cast<int>(aom_usec_timer_elapsed(&timer));
    printf("Hadamard%dx%d[%12d runs]: %d us\n", bw_, bh_, times, elapsed_time);
  }

  ACMRandom rnd_;

 private:
  HadamardFuncType h_func_;
  int bw_;
  int bh_;
};

class HadamardLowbdTest : public HadamardTestBase<tran_low_t, HadamardFunc> {
 public:
  HadamardLowbdTest() : HadamardTestBase(GetParam()) {}
  virtual int16_t Rand() { return rnd_.Rand9Signed(); }
};

TEST_P(HadamardLowbdTest, CompareReferenceRandom) { CompareReferenceRandom(); }

TEST_P(HadamardLowbdTest, VaryStride) { VaryStride(); }

TEST_P(HadamardLowbdTest, DISABLED_SpeedTest) { SpeedTest(1000000); }

INSTANTIATE_TEST_SUITE_P(
    C, HadamardLowbdTest,
    ::testing::Values(HadamardFuncWithSize(&aom_hadamard_4x4_c, 4, 4),
                      HadamardFuncWithSize(&aom_hadamard_8x8_c, 8, 8),
                      HadamardFuncWithSize(&aom_hadamard_16x16_c, 16, 16),
                      HadamardFuncWithSize(&aom_hadamard_32x32_c, 32, 32)));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, HadamardLowbdTest,
    ::testing::Values(HadamardFuncWithSize(&aom_hadamard_4x4_sse2, 4, 4),
                      HadamardFuncWithSize(&aom_hadamard_8x8_sse2, 8, 8),
                      HadamardFuncWithSize(&aom_hadamard_16x16_sse2, 16, 16),
                      HadamardFuncWithSize(&aom_hadamard_32x32_sse2, 32, 32)));
#endif  // HAVE_SSE2

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, HadamardLowbdTest,
    ::testing::Values(HadamardFuncWithSize(&aom_hadamard_16x16_avx2, 16, 16),
                      HadamardFuncWithSize(&aom_hadamard_32x32_avx2, 32, 32)));
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, HadamardLowbdTest,
    ::testing::Values(HadamardFuncWithSize(&aom_hadamard_8x8_neon, 8, 8),
                      HadamardFuncWithSize(&aom_hadamard_16x16_neon, 16, 16)));
#endif  // HAVE_NEON

// Tests for low precision
class HadamardLowbdLPTest : public HadamardTestBase<int16_t, HadamardLPFunc> {
 public:
  HadamardLowbdLPTest() : HadamardTestBase(GetParam()) {}
  virtual int16_t Rand() { return rnd_.Rand9Signed(); }
};

TEST_P(HadamardLowbdLPTest, CompareReferenceRandom) {
  CompareReferenceRandom();
}

TEST_P(HadamardLowbdLPTest, VaryStride) { VaryStride(); }

TEST_P(HadamardLowbdLPTest, DISABLED_SpeedTest) { SpeedTest(1000000); }

INSTANTIATE_TEST_SUITE_P(
    C, HadamardLowbdLPTest,
    ::testing::Values(HadamardLPFuncWithSize(&aom_hadamard_lp_8x8_c, 8, 8),
                      HadamardLPFuncWithSize(&aom_hadamard_lp_16x16_c, 16,
                                             16)));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, HadamardLowbdLPTest,
    ::testing::Values(HadamardLPFuncWithSize(&aom_hadamard_lp_8x8_sse2, 8, 8),
                      HadamardLPFuncWithSize(&aom_hadamard_lp_16x16_sse2, 16,
                                             16)));
#endif  // HAVE_SSE2

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, HadamardLowbdLPTest,
                         ::testing::Values(HadamardLPFuncWithSize(
                             &aom_hadamard_lp_16x16_avx2, 16, 16)));
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, HadamardLowbdLPTest,
    ::testing::Values(HadamardLPFuncWithSize(&aom_hadamard_lp_8x8_neon, 8, 8),
                      HadamardLPFuncWithSize(&aom_hadamard_lp_16x16_neon, 16,
                                             16)));
#endif  // HAVE_NEON

// Tests for 8x8 dual low precision
class HadamardLowbdLP8x8DualTest
    : public HadamardTestBase<int16_t, HadamardLP8x8DualFunc> {
 public:
  HadamardLowbdLP8x8DualTest() : HadamardTestBase(GetParam()) {}
  virtual int16_t Rand() { return rnd_.Rand9Signed(); }
};

TEST_P(HadamardLowbdLP8x8DualTest, CompareReferenceRandom) {
  CompareReferenceRandom();
}

TEST_P(HadamardLowbdLP8x8DualTest, VaryStride) { VaryStride(); }

TEST_P(HadamardLowbdLP8x8DualTest, DISABLED_SpeedTest) { SpeedTest(1000000); }

INSTANTIATE_TEST_SUITE_P(C, HadamardLowbdLP8x8DualTest,
                         ::testing::Values(HadamardLP8x8DualFuncWithSize(
                             &aom_hadamard_lp_8x8_dual_c, 8, 16)));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, HadamardLowbdLP8x8DualTest,
                         ::testing::Values(HadamardLP8x8DualFuncWithSize(
                             &aom_hadamard_lp_8x8_dual_sse2, 8, 16)));
#endif  // HAVE_SSE2

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, HadamardLowbdLP8x8DualTest,
                         ::testing::Values(HadamardLP8x8DualFuncWithSize(
                             &aom_hadamard_lp_8x8_dual_avx2, 8, 16)));
#endif  // HAVE_AVX2

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, HadamardLowbdLP8x8DualTest,
                         ::testing::Values(HadamardLP8x8DualFuncWithSize(
                             &aom_hadamard_lp_8x8_dual_neon, 8, 16)));
#endif  // HAVE_NEON

}  // namespace
