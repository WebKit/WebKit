/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <set>
#include <vector>
#include "config/av1_rtcd.h"
#include "config/aom_dsp_rtcd.h"
#include "aom_ports/aom_timer.h"
#include "gtest/gtest.h"
#include "test/acm_random.h"

namespace {

// TODO(any): Remove following INTERP_FILTERS_ALL define, so that 12-tap filter
// is tested once 12-tap filter SIMD is done.
#undef INTERP_FILTERS_ALL
#define INTERP_FILTERS_ALL 4

// All single reference convolve tests are parameterized on block size,
// bit-depth, and function to test.
//
// Note that parameterizing on these variables (and not other parameters) is
// a conscious decision - Jenkins needs some degree of parallelization to run
// the tests within the time limit, but if the number of parameters increases
// too much, the gtest framework does not handle it well (increased overhead per
// test, huge amount of output to stdout, etc.).
//
// Also note that the test suites must be named with the architecture, e.g.,
// C, C_X, AVX2_X, ... The test suite that runs on Jenkins sometimes runs tests
// that cannot deal with intrinsics (e.g., the Valgrind tests on 32-bit x86
// binaries) and will disable tests using a filter like
// --gtest_filter=-:SSE4_1.*. If the test suites are not named this way, the
// testing infrastructure will not selectively filter them properly.
class BlockSize {
 public:
  BlockSize(int w, int h) : width_(w), height_(h) {}

  int Width() const { return width_; }
  int Height() const { return height_; }

  bool operator<(const BlockSize &other) const {
    if (Width() == other.Width()) {
      return Height() < other.Height();
    }
    return Width() < other.Width();
  }

  bool operator==(const BlockSize &other) const {
    return Width() == other.Width() && Height() == other.Height();
  }

 private:
  int width_;
  int height_;
};

// Block size / bit depth / test function used to parameterize the tests.
template <typename T>
class TestParam {
 public:
  TestParam(const BlockSize &block, int bd, T test_func)
      : block_(block), bd_(bd), test_func_(test_func) {}

  const BlockSize &Block() const { return block_; }
  int BitDepth() const { return bd_; }
  T TestFunction() const { return test_func_; }

  bool operator==(const TestParam &other) const {
    return Block() == other.Block() && BitDepth() == other.BitDepth() &&
           TestFunction() == other.TestFunction();
  }

 private:
  BlockSize block_;
  int bd_;
  T test_func_;
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const TestParam<T> &test_arg) {
  return os << "TestParam { width:" << test_arg.Block().Width()
            << " height:" << test_arg.Block().Height()
            << " bd:" << test_arg.BitDepth() << " }";
}

// Generate the list of all block widths / heights that need to be tested,
// includes chroma and luma sizes, for the given bit-depths. The test
// function is the same for all generated parameters.
template <typename T>
std::vector<TestParam<T>> GetTestParams(std::initializer_list<int> bit_depths,
                                        T test_func) {
  std::set<BlockSize> sizes;
  for (int b = BLOCK_4X4; b < BLOCK_SIZES_ALL; ++b) {
    const int w = block_size_wide[b];
    const int h = block_size_high[b];
    sizes.insert(BlockSize(w, h));
    // Add in smaller chroma sizes as well.
    if (w == 4 || h == 4) {
      sizes.insert(BlockSize(w / 2, h / 2));
    }
  }
  std::vector<TestParam<T>> result;
  for (const BlockSize &block : sizes) {
    for (int bd : bit_depths) {
      result.push_back(TestParam<T>(block, bd, test_func));
    }
  }
  return result;
}

template <typename T>
std::vector<TestParam<T>> GetLowbdTestParams(T test_func) {
  return GetTestParams({ 8 }, test_func);
}

template <typename T>
::testing::internal::ParamGenerator<TestParam<T>> BuildLowbdParams(
    T test_func) {
  return ::testing::ValuesIn(GetLowbdTestParams(test_func));
}

// Test the test-parameters generators work as expected.
class AV1ConvolveParametersTest : public ::testing::Test {};

TEST_F(AV1ConvolveParametersTest, GetLowbdTestParams) {
  auto v = GetLowbdTestParams(av1_convolve_x_sr_c);
  ASSERT_EQ(27U, v.size());
  for (const auto &p : v) {
    ASSERT_EQ(8, p.BitDepth());
    // Needed (instead of ASSERT_EQ(...) since gtest does not
    // have built in printing for arbitrary functions, which
    // causes a compilation error.
    bool same_fn = av1_convolve_x_sr_c == p.TestFunction();
    ASSERT_TRUE(same_fn);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
template <typename T>
std::vector<TestParam<T>> GetHighbdTestParams(T test_func) {
  return GetTestParams({ 10, 12 }, test_func);
}

template <typename T>
::testing::internal::ParamGenerator<TestParam<T>> BuildHighbdParams(
    T test_func) {
  return ::testing::ValuesIn(GetHighbdTestParams(test_func));
}

TEST_F(AV1ConvolveParametersTest, GetHighbdTestParams) {
  auto v = GetHighbdTestParams(av1_highbd_convolve_x_sr_c);
  ASSERT_EQ(54U, v.size());
  int num_10 = 0;
  int num_12 = 0;
  for (const auto &p : v) {
    ASSERT_TRUE(p.BitDepth() == 10 || p.BitDepth() == 12);
    bool same_fn = av1_highbd_convolve_x_sr_c == p.TestFunction();
    ASSERT_TRUE(same_fn);
    if (p.BitDepth() == 10) {
      ++num_10;
    } else {
      ++num_12;
    }
  }
  ASSERT_EQ(num_10, num_12);
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

// AV1ConvolveTest is the base class that all convolve tests should derive from.
// It provides storage/methods for generating randomized buffers for both
// low bit-depth and high bit-depth, and setup/teardown methods for clearing
// system state. Implementors can get the bit-depth / block-size /
// test function by calling GetParam().
template <typename T>
class AV1ConvolveTest : public ::testing::TestWithParam<TestParam<T>> {
 public:
  ~AV1ConvolveTest() override = default;

  void SetUp() override {
    rnd_.Reset(libaom_test::ACMRandom::DeterministicSeed());
  }

  // Randomizes the 8-bit input buffer and returns a pointer to it. Note that
  // the pointer is safe to use with an 8-tap filter. The stride can range
  // from width to (width + kPadding). Also note that the pointer is to the
  // same memory location.
  static constexpr int kInputPadding = 12;

  // Get a pointer to a buffer with stride == width. Note that we must have
  // the test param passed in explicitly -- the gtest framework does not
  // support calling GetParam() within a templatized class.
  // Note that FirstRandomInput8 always returns the same pointer -- if two
  // inputs are needed, also use SecondRandomInput8.
  const uint8_t *FirstRandomInput8(const TestParam<T> &param) {
    // Note we can't call GetParam() directly -- gtest does not support
    // this for parameterized types.
    return RandomInput8(input8_1_, param);
  }

  const uint8_t *SecondRandomInput8(const TestParam<T> &param) {
    return RandomInput8(input8_2_, param);
  }

  // Some of the intrinsics perform writes in 32 byte chunks. Moreover, some
  // of the instrinsics assume that the stride is also a multiple of 32.
  // To satisfy these constraints and also remain simple, output buffer strides
  // are assumed MAX_SB_SIZE.
  static constexpr int kOutputStride = MAX_SB_SIZE;

  // Check that two 8-bit output buffers are identical.
  void AssertOutputBufferEq(const uint8_t *p1, const uint8_t *p2, int width,
                            int height) {
    ASSERT_TRUE(p1 != p2) << "Buffers must be at different memory locations";
    for (int j = 0; j < height; ++j) {
      if (memcmp(p1, p2, sizeof(*p1) * width) == 0) {
        p1 += kOutputStride;
        p2 += kOutputStride;
        continue;
      }
      for (int i = 0; i < width; ++i) {
        ASSERT_EQ(p1[i], p2[i])
            << width << "x" << height << " Pixel mismatch at (" << i << ", "
            << j << ")";
      }
    }
  }

  // Check that two 16-bit output buffers are identical.
  void AssertOutputBufferEq(const uint16_t *p1, const uint16_t *p2, int width,
                            int height) {
    ASSERT_TRUE(p1 != p2) << "Buffers must be in different memory locations";
    for (int j = 0; j < height; ++j) {
      if (memcmp(p1, p2, sizeof(*p1) * width) == 0) {
        p1 += kOutputStride;
        p2 += kOutputStride;
        continue;
      }
      for (int i = 0; i < width; ++i) {
        ASSERT_EQ(p1[i], p2[i])
            << width << "x" << height << " Pixel mismatch at (" << i << ", "
            << j << ")";
      }
    }
  }

#if CONFIG_AV1_HIGHBITDEPTH
  // Note that the randomized values are capped by bit-depth.
  const uint16_t *FirstRandomInput16(const TestParam<T> &param) {
    return RandomInput16(input16_1_, param);
  }

  const uint16_t *SecondRandomInput16(const TestParam<T> &param) {
    return RandomInput16(input16_2_, param);
  }
#endif

 private:
  const uint8_t *RandomInput8(uint8_t *p, const TestParam<T> &param) {
    EXPECT_EQ(8, param.BitDepth());
    EXPECT_GE(MAX_SB_SIZE, param.Block().Width());
    EXPECT_GE(MAX_SB_SIZE, param.Block().Height());
    const int padded_width = param.Block().Width() + kInputPadding;
    const int padded_height = param.Block().Height() + kInputPadding;
    Randomize(p, padded_width * padded_height);
    return p + (kInputPadding / 2) * padded_width + kInputPadding / 2;
  }

  void Randomize(uint8_t *p, int size) {
    for (int i = 0; i < size; ++i) {
      p[i] = rnd_.Rand8();
    }
  }

#if CONFIG_AV1_HIGHBITDEPTH
  const uint16_t *RandomInput16(uint16_t *p, const TestParam<T> &param) {
    // Check that this is only called with high bit-depths.
    EXPECT_TRUE(param.BitDepth() == 10 || param.BitDepth() == 12);
    EXPECT_GE(MAX_SB_SIZE, param.Block().Width());
    EXPECT_GE(MAX_SB_SIZE, param.Block().Height());
    const int padded_width = param.Block().Width() + kInputPadding;
    const int padded_height = param.Block().Height() + kInputPadding;
    Randomize(p, padded_width * padded_height, param.BitDepth());
    return p + (kInputPadding / 2) * padded_width + kInputPadding / 2;
  }

  void Randomize(uint16_t *p, int size, int bit_depth) {
    for (int i = 0; i < size; ++i) {
      p[i] = rnd_.Rand16() & ((1 << bit_depth) - 1);
    }
  }
#endif

  static constexpr int kInputStride = MAX_SB_SIZE + kInputPadding;

  libaom_test::ACMRandom rnd_;
  // Statically allocate all the memory that is needed for the tests. Note
  // that we cannot allocate output memory here. It must use DECLARE_ALIGNED,
  // which is a C99 feature and interacts badly with C++ member variables.
  uint8_t input8_1_[kInputStride * kInputStride];
  uint8_t input8_2_[kInputStride * kInputStride];
#if CONFIG_AV1_HIGHBITDEPTH
  uint16_t input16_1_[kInputStride * kInputStride];
  uint16_t input16_2_[kInputStride * kInputStride];
#endif
};

////////////////////////////////////////////////////////
// Single reference convolve-x functions (low bit-depth)
////////////////////////////////////////////////////////
typedef void (*convolve_x_func)(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams *filter_params_x,
                                const int subpel_x_qn,
                                ConvolveParams *conv_params);

class AV1ConvolveXTest : public AV1ConvolveTest<convolve_x_func> {
 public:
  void RunTest() {
    // Do not test the no-op filter.
    for (int sub_x = 1; sub_x < 16; ++sub_x) {
      for (int filter = EIGHTTAP_REGULAR; filter <= INTERP_FILTERS_ALL;
           ++filter) {
        InterpFilter f = static_cast<InterpFilter>(filter);
        TestConvolve(sub_x, f);
      }
    }
  }

 public:
  void SpeedTest() {
    for (int filter = EIGHTTAP_REGULAR; filter <= INTERP_FILTERS_ALL;
         ++filter) {
      InterpFilter f = static_cast<InterpFilter>(filter);
      TestConvolveSpeed(f, 10000);
    }
  }

 private:
  void TestConvolve(const int sub_x, const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();

    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(filter, width);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    const uint8_t *input = FirstRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    av1_convolve_x_sr_c(input, width, reference, kOutputStride, width, height,
                        filter_params_x, sub_x, &conv_params1);

    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    convolve_x_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    test_func(input, width, test, kOutputStride, width, height, filter_params_x,
              sub_x, &conv_params2);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void TestConvolveSpeed(const InterpFilter filter, const int num_iters) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();

    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(filter, width);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    const uint8_t *input = FirstRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      av1_convolve_x_sr_c(input, width, reference, kOutputStride, width, height,
                          filter_params_x, 0, &conv_params1);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    convolve_x_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);

    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_x, 0, &conv_params2);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    printf("%d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", filter, width, height, time1,
           time2, time1 / time2);
  }
};

TEST_P(AV1ConvolveXTest, RunTest) { RunTest(); }

TEST_P(AV1ConvolveXTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_neon));
#endif

#if HAVE_NEON_DOTPROD
INSTANTIATE_TEST_SUITE_P(NEON_DOTPROD, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_neon_dotprod));
#endif

#if HAVE_NEON_I8MM
INSTANTIATE_TEST_SUITE_P(NEON_I8MM, AV1ConvolveXTest,
                         BuildLowbdParams(av1_convolve_x_sr_neon_i8mm));
#endif

////////////////////////////////////////////////////////////////
// Single reference convolve-x IntraBC functions (low bit-depth)
////////////////////////////////////////////////////////////////

class AV1ConvolveXIntraBCTest : public AV1ConvolveTest<convolve_x_func> {
 public:
  void RunTest() {
    // IntraBC functions only operate for subpel_x_qn = 8.
    constexpr int kSubX = 8;
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_x = &av1_intrabc_filter_params;
    const uint8_t *input = FirstRandomInput8(GetParam());

    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    // Use a stride different from width to avoid potential storing errors that
    // would go undetected. The input buffer is filled using a padding of 12, so
    // the stride can be anywhere between width and width + 12.
    av1_convolve_x_sr_intrabc_c(input, width + 2, reference, kOutputStride,
                                width, height, filter_params_x, kSubX,
                                &conv_params1);

    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    convolve_x_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    test_func(input, width + 2, test, kOutputStride, width, height,
              filter_params_x, kSubX, &conv_params2);

    AssertOutputBufferEq(reference, test, width, height);
  }

  void SpeedTest() {
    constexpr int kNumIters = 10000;
    const InterpFilter filter = static_cast<InterpFilter>(BILINEAR);
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_x = &av1_intrabc_filter_params;
    const uint8_t *input = FirstRandomInput8(GetParam());

    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      av1_convolve_x_sr_intrabc_c(input, width, reference, kOutputStride, width,
                                  height, filter_params_x, 0, &conv_params1);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    convolve_x_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_x, 0, &conv_params2);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    printf("%d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", filter, width, height, time1,
           time2, time1 / time2);
  }
};

TEST_P(AV1ConvolveXIntraBCTest, RunTest) { RunTest(); }

TEST_P(AV1ConvolveXIntraBCTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveXIntraBCTest,
                         BuildLowbdParams(av1_convolve_x_sr_intrabc_c));

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveXIntraBCTest,
                         BuildLowbdParams(av1_convolve_x_sr_intrabc_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////////////
// Single reference convolve-x functions (high bit-depth)
/////////////////////////////////////////////////////////
typedef void (*highbd_convolve_x_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params, int bd);

class AV1ConvolveXHighbdTest : public AV1ConvolveTest<highbd_convolve_x_func> {
 public:
  void RunTest() {
    // Do not test the no-op filter.
    for (int sub_x = 1; sub_x < 16; ++sub_x) {
      for (int filter = EIGHTTAP_REGULAR; filter <= INTERP_FILTERS_ALL;
           ++filter) {
        InterpFilter f = static_cast<InterpFilter>(filter);
        TestConvolve(sub_x, f);
      }
    }
  }

 public:
  void SpeedTest() {
    for (int filter = EIGHTTAP_REGULAR; filter <= INTERP_FILTERS_ALL;
         ++filter) {
      InterpFilter f = static_cast<InterpFilter>(filter);
      TestConvolveSpeed(f, 10000);
    }
  }

 private:
  void TestConvolve(const int sub_x, const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(filter, width);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, bit_depth);
    const uint16_t *input = FirstRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    av1_highbd_convolve_x_sr_c(input, width, reference, kOutputStride, width,
                               height, filter_params_x, sub_x, &conv_params1,
                               bit_depth);

    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, bit_depth);
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_x, sub_x, &conv_params2, bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void TestConvolveSpeed(const InterpFilter filter, const int num_iters) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(filter, width);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    const uint16_t *input = FirstRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      av1_highbd_convolve_x_sr_c(input, width, reference, kOutputStride, width,
                                 height, filter_params_x, 0, &conv_params1,
                                 bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    highbd_convolve_x_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);

    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_x, 0, &conv_params2, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    printf("%d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", filter, width, height, time1,
           time2, time1 / time2);
  }
};

TEST_P(AV1ConvolveXHighbdTest, RunTest) { RunTest(); }

TEST_P(AV1ConvolveXHighbdTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveXHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_c));

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1ConvolveXHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1ConvolveXHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveXHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_neon));
#endif

#if HAVE_SVE2
INSTANTIATE_TEST_SUITE_P(SVE2, AV1ConvolveXHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_sve2));
#endif

/////////////////////////////////////////////////////////////////
// Single reference convolve-x IntraBC functions (high bit-depth)
/////////////////////////////////////////////////////////////////

class AV1ConvolveXHighbdIntraBCTest
    : public AV1ConvolveTest<highbd_convolve_x_func> {
 public:
  void RunTest() {
    // IntraBC functions only operate for subpel_x_qn = 8.
    constexpr int kSubX = 8;
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x = &av1_intrabc_filter_params;
    const uint16_t *input = FirstRandomInput16(GetParam());

    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, bit_depth);
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    // Use a stride different from width to avoid potential storing errors that
    // would go undetected. The input buffer is filled using a padding of 12, so
    // the stride can be anywhere between width and width + 12.
    av1_highbd_convolve_x_sr_intrabc_c(
        input, width + 2, reference, kOutputStride, width, height,
        filter_params_x, kSubX, &conv_params1, bit_depth);

    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, bit_depth);
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    GetParam().TestFunction()(input, width + 2, test, kOutputStride, width,
                              height, filter_params_x, kSubX, &conv_params2,
                              bit_depth);

    AssertOutputBufferEq(reference, test, width, height);
  }

  void SpeedTest() {
    constexpr int kNumIters = 10000;
    const InterpFilter filter = static_cast<InterpFilter>(BILINEAR);
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x = &av1_intrabc_filter_params;
    const uint16_t *input = FirstRandomInput16(GetParam());

    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      av1_highbd_convolve_x_sr_intrabc_c(input, width, reference, kOutputStride,
                                         width, height, filter_params_x, 0,
                                         &conv_params1, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    highbd_convolve_x_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_x, 0, &conv_params2, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    printf("%d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", filter, width, height, time1,
           time2, time1 / time2);
  }
};

TEST_P(AV1ConvolveXHighbdIntraBCTest, RunTest) { RunTest(); }

TEST_P(AV1ConvolveXHighbdIntraBCTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveXHighbdIntraBCTest,
                         BuildHighbdParams(av1_highbd_convolve_x_sr_intrabc_c));

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1ConvolveXHighbdIntraBCTest,
    BuildHighbdParams(av1_highbd_convolve_x_sr_intrabc_neon));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

////////////////////////////////////////////////////////
// Single reference convolve-y functions (low bit-depth)
////////////////////////////////////////////////////////
typedef void (*convolve_y_func)(const uint8_t *src, int src_stride,
                                uint8_t *dst, int dst_stride, int w, int h,
                                const InterpFilterParams *filter_params_y,
                                const int subpel_y_qn);

class AV1ConvolveYTest : public AV1ConvolveTest<convolve_y_func> {
 public:
  void RunTest() {
    // Do not test the no-op filter.
    for (int sub_y = 1; sub_y < 16; ++sub_y) {
      for (int filter = EIGHTTAP_REGULAR; filter <= INTERP_FILTERS_ALL;
           ++filter) {
        InterpFilter f = static_cast<InterpFilter>(filter);
        TestConvolve(sub_y, f);
      }
    }
  }

 public:
  void SpeedTest() {
    for (int filter = EIGHTTAP_REGULAR; filter <= INTERP_FILTERS_ALL;
         ++filter) {
      InterpFilter f = static_cast<InterpFilter>(filter);
      TestConvolveSpeed(f, 10000);
    }
  }

 private:
  void TestConvolve(const int sub_y, const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();

    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(filter, height);
    const uint8_t *input = FirstRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    av1_convolve_y_sr_c(input, width, reference, kOutputStride, width, height,
                        filter_params_y, sub_y);
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_y, sub_y);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void TestConvolveSpeed(const InterpFilter filter, const int num_iters) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();

    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(filter, height);
    const uint8_t *input = FirstRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      av1_convolve_y_sr_c(input, width, reference, kOutputStride, width, height,
                          filter_params_y, 0);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);

    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      GetParam().TestFunction()(input, width, test, kOutputStride, width,
                                height, filter_params_y, 0);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    printf("%d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", filter, width, height, time1,
           time2, time1 / time2);
  }
};

TEST_P(AV1ConvolveYTest, RunTest) { RunTest(); }

TEST_P(AV1ConvolveYTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_neon));
#endif

#if HAVE_NEON_DOTPROD
INSTANTIATE_TEST_SUITE_P(NEON_DOTPROD, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_neon_dotprod));
#endif

#if HAVE_NEON_I8MM
INSTANTIATE_TEST_SUITE_P(NEON_I8MM, AV1ConvolveYTest,
                         BuildLowbdParams(av1_convolve_y_sr_neon_i8mm));
#endif

////////////////////////////////////////////////////////////////
// Single reference convolve-y IntraBC functions (low bit-depth)
////////////////////////////////////////////////////////////////

class AV1ConvolveYIntraBCTest : public AV1ConvolveTest<convolve_y_func> {
 public:
  void RunTest() {
    // IntraBC functions only operate for subpel_y_qn = 8.
    constexpr int kSubY = 8;
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_y = &av1_intrabc_filter_params;
    const uint8_t *input = FirstRandomInput8(GetParam());

    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    // Use a stride different from width to avoid potential storing errors that
    // would go undetected. The input buffer is filled using a padding of 12, so
    // the stride can be anywhere between width and width + 12.
    av1_convolve_y_sr_intrabc_c(input, width + 2, reference, kOutputStride,
                                width, height, filter_params_y, kSubY);

    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    GetParam().TestFunction()(input, width + 2, test, kOutputStride, width,
                              height, filter_params_y, kSubY);

    AssertOutputBufferEq(reference, test, width, height);
  }

  void SpeedTest() {
    constexpr int kNumIters = 10000;
    const InterpFilter filter = static_cast<InterpFilter>(BILINEAR);
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();

    const InterpFilterParams *filter_params_y = &av1_intrabc_filter_params;
    const uint8_t *input = FirstRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      av1_convolve_y_sr_intrabc_c(input, width, reference, kOutputStride, width,
                                  height, filter_params_y, 0);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    convolve_y_func test_func = GetParam().TestFunction();
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_y, 0);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    printf("%d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", filter, width, height, time1,
           time2, time1 / time2);
  }
};

TEST_P(AV1ConvolveYIntraBCTest, RunTest) { RunTest(); }

TEST_P(AV1ConvolveYIntraBCTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveYIntraBCTest,
                         BuildLowbdParams(av1_convolve_y_sr_intrabc_c));

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveYIntraBCTest,
                         BuildLowbdParams(av1_convolve_y_sr_intrabc_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////////////
// Single reference convolve-y functions (high bit-depth)
/////////////////////////////////////////////////////////
typedef void (*highbd_convolve_y_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn,
    int bd);

class AV1ConvolveYHighbdTest : public AV1ConvolveTest<highbd_convolve_y_func> {
 public:
  void RunTest() {
    // Do not test the no-op filter.
    for (int sub_y = 1; sub_y < 16; ++sub_y) {
      for (int filter = EIGHTTAP_REGULAR; filter <= INTERP_FILTERS_ALL;
           ++filter) {
        InterpFilter f = static_cast<InterpFilter>(filter);
        TestConvolve(sub_y, f);
      }
    }
  }

 public:
  void SpeedTest() {
    for (int filter = EIGHTTAP_REGULAR; filter <= INTERP_FILTERS_ALL;
         ++filter) {
      InterpFilter f = static_cast<InterpFilter>(filter);
      TestConvolveSpeed(f, 10000);
    }
  }

 private:
  void TestConvolve(const int sub_y, const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(filter, height);
    const uint16_t *input = FirstRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    av1_highbd_convolve_y_sr_c(input, width, reference, kOutputStride, width,
                               height, filter_params_y, sub_y, bit_depth);
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_y, sub_y, bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void TestConvolveSpeed(const InterpFilter filter, const int num_iters) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(filter, width);
    const uint16_t *input = FirstRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      av1_highbd_convolve_y_sr_c(input, width, reference, kOutputStride, width,
                                 height, filter_params_y, 0, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    highbd_convolve_y_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);

    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_y, 0, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    printf("%d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", filter, width, height, time1,
           time2, time1 / time2);
  }
};

TEST_P(AV1ConvolveYHighbdTest, RunTest) { RunTest(); }

TEST_P(AV1ConvolveYHighbdTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveYHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_c));

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1ConvolveYHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1ConvolveYHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveYHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_neon));
#endif

#if HAVE_SVE2
INSTANTIATE_TEST_SUITE_P(SVE2, AV1ConvolveYHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_sve2));
#endif

/////////////////////////////////////////////////////////////////
// Single reference convolve-y IntraBC functions (high bit-depth)
/////////////////////////////////////////////////////////////////

class AV1ConvolveYHighbdIntraBCTest
    : public AV1ConvolveTest<highbd_convolve_y_func> {
 public:
  void RunTest() {
    // IntraBC functions only operate for subpel_y_qn = 8.
    constexpr int kSubY = 8;
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_y = &av1_intrabc_filter_params;
    const uint16_t *input = FirstRandomInput16(GetParam());

    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    // Use a stride different from width to avoid potential storing errors that
    // would go undetected. The input buffer is filled using a padding of 12, so
    // the stride can be anywhere between width and width + 12.
    av1_highbd_convolve_y_sr_intrabc_c(input, width + 2, reference,
                                       kOutputStride, width, height,
                                       filter_params_y, kSubY, bit_depth);

    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    GetParam().TestFunction()(input, width + 2, test, kOutputStride, width,
                              height, filter_params_y, kSubY, bit_depth);

    AssertOutputBufferEq(reference, test, width, height);
  }

  void SpeedTest() {
    constexpr int kNumIters = 10000;
    const InterpFilter filter = static_cast<InterpFilter>(BILINEAR);
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(filter, width);
    const uint16_t *input = FirstRandomInput16(GetParam());

    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      av1_highbd_convolve_y_sr_intrabc_c(input, width, reference, kOutputStride,
                                         width, height, filter_params_y, 0,
                                         bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    highbd_convolve_y_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_y, 0, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    printf("%d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", filter, width, height, time1,
           time2, time1 / time2);
  }
};

TEST_P(AV1ConvolveYHighbdIntraBCTest, RunTest) { RunTest(); }

TEST_P(AV1ConvolveYHighbdIntraBCTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveYHighbdIntraBCTest,
                         BuildHighbdParams(av1_highbd_convolve_y_sr_intrabc_c));

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1ConvolveYHighbdIntraBCTest,
    BuildHighbdParams(av1_highbd_convolve_y_sr_intrabc_neon));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////////////////////////////////////////
// Single reference convolve-copy functions (low bit-depth)
//////////////////////////////////////////////////////////////
typedef void (*convolve_copy_func)(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride, int w,
                                   int h);

class AV1ConvolveCopyTest : public AV1ConvolveTest<convolve_copy_func> {
 public:
  void RunTest() {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const uint8_t *input = FirstRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    aom_convolve_copy_c(input, width, reference, kOutputStride, width, height);
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

// Note that even though these are AOM convolve functions, we are using the
// newer AV1 test framework.
TEST_P(AV1ConvolveCopyTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveCopyTest,
                         BuildLowbdParams(aom_convolve_copy_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
///////////////////////////////////////////////////////////////
// Single reference convolve-copy functions (high bit-depth)
///////////////////////////////////////////////////////////////
typedef void (*highbd_convolve_copy_func)(const uint16_t *src,
                                          ptrdiff_t src_stride, uint16_t *dst,
                                          ptrdiff_t dst_stride, int w, int h);

class AV1ConvolveCopyHighbdTest
    : public AV1ConvolveTest<highbd_convolve_copy_func> {
 public:
  void RunTest() {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const uint16_t *input = FirstRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    aom_highbd_convolve_copy_c(input, width, reference, kOutputStride, width,
                               height);
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }
};

TEST_P(AV1ConvolveCopyHighbdTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveCopyHighbdTest,
                         BuildHighbdParams(aom_highbd_convolve_copy_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1ConvolveCopyHighbdTest,
                         BuildHighbdParams(aom_highbd_convolve_copy_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1ConvolveCopyHighbdTest,
                         BuildHighbdParams(aom_highbd_convolve_copy_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveCopyHighbdTest,
                         BuildHighbdParams(aom_highbd_convolve_copy_neon));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

/////////////////////////////////////////////////////////
// Single reference convolve-2D functions (low bit-depth)
/////////////////////////////////////////////////////////
typedef void (*convolve_2d_func)(const uint8_t *src, int src_stride,
                                 uint8_t *dst, int dst_stride, int w, int h,
                                 const InterpFilterParams *filter_params_x,
                                 const InterpFilterParams *filter_params_y,
                                 const int subpel_x_qn, const int subpel_y_qn,
                                 ConvolveParams *conv_params);

class AV1Convolve2DTest : public AV1ConvolveTest<convolve_2d_func> {
 public:
  void RunTest() {
    // Do not test the no-op filter.
    for (int sub_x = 1; sub_x < 16; ++sub_x) {
      for (int sub_y = 1; sub_y < 16; ++sub_y) {
        for (int h_f = EIGHTTAP_REGULAR; h_f <= INTERP_FILTERS_ALL; ++h_f) {
          for (int v_f = EIGHTTAP_REGULAR; v_f <= INTERP_FILTERS_ALL; ++v_f) {
            if (((h_f == MULTITAP_SHARP2) && (v_f < MULTITAP_SHARP2)) ||
                ((h_f < MULTITAP_SHARP2) && (v_f == MULTITAP_SHARP2)))
              continue;
            TestConvolve(static_cast<InterpFilter>(h_f),
                         static_cast<InterpFilter>(v_f), sub_x, sub_y);
          }
        }
      }
    }
  }

 public:
  void SpeedTest() {
    for (int h_f = EIGHTTAP_REGULAR; h_f <= INTERP_FILTERS_ALL; ++h_f) {
      for (int v_f = EIGHTTAP_REGULAR; v_f <= INTERP_FILTERS_ALL; ++v_f) {
        if (((h_f == MULTITAP_SHARP2) && (v_f < MULTITAP_SHARP2)) ||
            ((h_f < MULTITAP_SHARP2) && (v_f == MULTITAP_SHARP2)))
          continue;
        TestConvolveSpeed(static_cast<InterpFilter>(h_f),
                          static_cast<InterpFilter>(v_f), 10000);
      }
    }
  }

 private:
  void TestConvolve(const InterpFilter h_f, const InterpFilter v_f,
                    const int sub_x, const int sub_y) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const uint8_t *input = FirstRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    av1_convolve_2d_sr_c(input, width, reference, kOutputStride, width, height,
                         filter_params_x, filter_params_y, sub_x, sub_y,
                         &conv_params1);
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_x, filter_params_y, sub_x, sub_y,
                              &conv_params2);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void TestConvolveSpeed(const InterpFilter h_f, const InterpFilter v_f,
                         int num_iters) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const uint8_t *input = FirstRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      av1_convolve_2d_sr_c(input, width, reference, kOutputStride, width,
                           height, filter_params_x, filter_params_y, 0, 0,
                           &conv_params1);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      GetParam().TestFunction()(input, width, test, kOutputStride, width,
                                height, filter_params_x, filter_params_y, 0, 0,
                                &conv_params2);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    printf("%d - %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", h_f, v_f, width, height,
           time1, time2, time1 / time2);
  }
};

TEST_P(AV1Convolve2DTest, RunTest) { RunTest(); }

TEST_P(AV1Convolve2DTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_neon));
#endif

#if HAVE_NEON_DOTPROD
INSTANTIATE_TEST_SUITE_P(NEON_DOTPROD, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_neon_dotprod));
#endif

#if HAVE_NEON_I8MM
INSTANTIATE_TEST_SUITE_P(NEON_I8MM, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_neon_i8mm));
#endif

#if HAVE_SVE2
INSTANTIATE_TEST_SUITE_P(SVE2, AV1Convolve2DTest,
                         BuildLowbdParams(av1_convolve_2d_sr_sve2));
#endif

/////////////////////////////////////////////////////////////////
// Single reference convolve-2D IntraBC functions (low bit-depth)
/////////////////////////////////////////////////////////////////

class AV1Convolve2DIntraBCTest : public AV1ConvolveTest<convolve_2d_func> {
 public:
  void RunTest() {
    // IntraBC functions only operate for subpel_x_qn = 8 and subpel_y_qn = 8.
    constexpr int kSubX = 8;
    constexpr int kSubY = 8;
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_x = &av1_intrabc_filter_params;
    const InterpFilterParams *filter_params_y = &av1_intrabc_filter_params;
    const uint8_t *input = FirstRandomInput8(GetParam());

    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    // Use a stride different from width to avoid potential storing errors that
    // would go undetected. The input buffer is filled using a padding of 12, so
    // the stride can be anywhere between width and width + 12.
    av1_convolve_2d_sr_intrabc_c(input, width + 2, reference, kOutputStride,
                                 width, height, filter_params_x,
                                 filter_params_y, kSubX, kSubY, &conv_params1);

    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    GetParam().TestFunction()(input, width + 2, test, kOutputStride, width,
                              height, filter_params_x, filter_params_y, kSubX,
                              kSubY, &conv_params2);

    AssertOutputBufferEq(reference, test, width, height);
  }

  void SpeedTest() {
    constexpr int kNumIters = 10000;
    const InterpFilter h_f = static_cast<InterpFilter>(BILINEAR);
    const InterpFilter v_f = static_cast<InterpFilter>(BILINEAR);
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params_x = &av1_intrabc_filter_params;
    const InterpFilterParams *filter_params_y = &av1_intrabc_filter_params;
    const uint8_t *input = FirstRandomInput8(GetParam());

    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      av1_convolve_2d_sr_intrabc_c(input, width, reference, kOutputStride,
                                   width, height, filter_params_x,
                                   filter_params_y, 8, 8, &conv_params1);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    convolve_2d_func test_func = GetParam().TestFunction();
    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_x, filter_params_y, 8, 8, &conv_params2);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    printf("%d - %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", h_f, v_f, width, height,
           time1, time2, time1 / time2);
  }
};

TEST_P(AV1Convolve2DIntraBCTest, RunTest) { RunTest(); }

TEST_P(AV1Convolve2DIntraBCTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1Convolve2DIntraBCTest,
                         BuildLowbdParams(av1_convolve_2d_sr_intrabc_c));

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1Convolve2DIntraBCTest,
                         BuildLowbdParams(av1_convolve_2d_sr_intrabc_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
//////////////////////////////////////////////////////////
// Single reference convolve-2d functions (high bit-depth)
//////////////////////////////////////////////////////////

typedef void (*highbd_convolve_2d_func)(
    const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x,
    const InterpFilterParams *filter_params_y, const int subpel_x_qn,
    const int subpel_y_qn, ConvolveParams *conv_params, int bd);

class AV1Convolve2DHighbdTest
    : public AV1ConvolveTest<highbd_convolve_2d_func> {
 public:
  void RunTest() {
    // Do not test the no-op filter.
    for (int sub_x = 1; sub_x < 16; ++sub_x) {
      for (int sub_y = 1; sub_y < 16; ++sub_y) {
        for (int h_f = EIGHTTAP_REGULAR; h_f <= INTERP_FILTERS_ALL; ++h_f) {
          for (int v_f = EIGHTTAP_REGULAR; v_f <= INTERP_FILTERS_ALL; ++v_f) {
            if (((h_f == MULTITAP_SHARP2) && (v_f < MULTITAP_SHARP2)) ||
                ((h_f < MULTITAP_SHARP2) && (v_f == MULTITAP_SHARP2)))
              continue;
            TestConvolve(static_cast<InterpFilter>(h_f),
                         static_cast<InterpFilter>(v_f), sub_x, sub_y);
          }
        }
      }
    }
  }

 public:
  void SpeedTest() {
    for (int h_f = EIGHTTAP_REGULAR; h_f <= INTERP_FILTERS_ALL; ++h_f) {
      for (int v_f = EIGHTTAP_REGULAR; v_f <= INTERP_FILTERS_ALL; ++v_f) {
        if (((h_f == MULTITAP_SHARP2) && (v_f < MULTITAP_SHARP2)) ||
            ((h_f < MULTITAP_SHARP2) && (v_f == MULTITAP_SHARP2)))
          continue;
        TestConvolveSpeed(static_cast<InterpFilter>(h_f),
                          static_cast<InterpFilter>(v_f), 10000);
      }
    }
  }

 private:
  void TestConvolve(const InterpFilter h_f, const InterpFilter v_f,
                    const int sub_x, const int sub_y) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const uint16_t *input = FirstRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, bit_depth);
    av1_highbd_convolve_2d_sr_c(input, width, reference, kOutputStride, width,
                                height, filter_params_x, filter_params_y, sub_x,
                                sub_y, &conv_params1, bit_depth);
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, bit_depth);
    GetParam().TestFunction()(input, width, test, kOutputStride, width, height,
                              filter_params_x, filter_params_y, sub_x, sub_y,
                              &conv_params2, bit_depth);
    AssertOutputBufferEq(reference, test, width, height);
  }

  void TestConvolveSpeed(const InterpFilter h_f, const InterpFilter v_f,
                         int num_iters) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const uint16_t *input = FirstRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      av1_highbd_convolve_2d_sr_c(input, width, reference, kOutputStride, width,
                                  height, filter_params_x, filter_params_y, 0,
                                  0, &conv_params1, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      GetParam().TestFunction()(input, width, test, kOutputStride, width,
                                height, filter_params_x, filter_params_y, 0, 0,
                                &conv_params2, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    printf("%d - %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", h_f, v_f, width, height,
           time1, time2, time1 / time2);
  }
};

TEST_P(AV1Convolve2DHighbdTest, RunTest) { RunTest(); }

TEST_P(AV1Convolve2DHighbdTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1Convolve2DHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_sr_c));

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1Convolve2DHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_sr_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1Convolve2DHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_sr_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1Convolve2DHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_sr_neon));
#endif

#if HAVE_SVE2
INSTANTIATE_TEST_SUITE_P(SVE2, AV1Convolve2DHighbdTest,
                         BuildHighbdParams(av1_highbd_convolve_2d_sr_sve2));
#endif

//////////////////////////////////////////////////////////////////
// Single reference convolve-2d IntraBC functions (high bit-depth)
//////////////////////////////////////////////////////////////////

class AV1Convolve2DHighbdIntraBCTest
    : public AV1ConvolveTest<highbd_convolve_2d_func> {
 public:
  void RunTest() {
    // IntraBC functions only operate for subpel_x_qn = 8 and subpel_y_qn = 8.
    constexpr int kSubX = 8;
    constexpr int kSubY = 8;
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x = &av1_intrabc_filter_params;
    const InterpFilterParams *filter_params_y = &av1_intrabc_filter_params;
    const uint16_t *input = FirstRandomInput16(GetParam());

    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, bit_depth);
    // Use a stride different from width to avoid potential storing errors that
    // would go undetected. The input buffer is filled using a padding of 12, so
    // the stride can be anywhere between width and width + 12.
    av1_highbd_convolve_2d_sr_intrabc_c(input, width + 2, reference,
                                        kOutputStride, width, height,
                                        filter_params_x, filter_params_y, kSubX,
                                        kSubY, &conv_params1, bit_depth);

    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, bit_depth);
    GetParam().TestFunction()(input, width + 2, test, kOutputStride, width,
                              height, filter_params_x, filter_params_y, kSubX,
                              kSubY, &conv_params2, bit_depth);

    AssertOutputBufferEq(reference, test, width, height);
  }

  void SpeedTest() {
    constexpr int kNumIters = 10000;
    const InterpFilter h_f = static_cast<InterpFilter>(BILINEAR);
    const InterpFilter v_f = static_cast<InterpFilter>(BILINEAR);
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const uint16_t *input = FirstRandomInput16(GetParam());

    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    ConvolveParams conv_params1 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      av1_highbd_convolve_2d_sr_intrabc_c(
          input, width, reference, kOutputStride, width, height,
          filter_params_x, filter_params_y, 0, 0, &conv_params1, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    highbd_convolve_2d_func test_func = GetParam().TestFunction();
    ConvolveParams conv_params2 =
        get_conv_params_no_round(0, 0, nullptr, 0, 0, 8);
    aom_usec_timer_start(&timer);
    for (int i = 0; i < kNumIters; ++i) {
      test_func(input, width, test, kOutputStride, width, height,
                filter_params_x, filter_params_y, 0, 0, &conv_params2,
                bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    printf("%d - %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n", h_f, v_f, width, height,
           time1, time2, time1 / time2);
  }
};

TEST_P(AV1Convolve2DHighbdIntraBCTest, RunTest) { RunTest(); }

TEST_P(AV1Convolve2DHighbdIntraBCTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(
    C, AV1Convolve2DHighbdIntraBCTest,
    BuildHighbdParams(av1_highbd_convolve_2d_sr_intrabc_c));

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1Convolve2DHighbdIntraBCTest,
    BuildHighbdParams(av1_highbd_convolve_2d_sr_intrabc_neon));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////
// Compound Convolve Tests
//////////////////////////

// The compound functions do not work for chroma block sizes. Provide
// a function to generate test parameters for just luma block sizes.
template <typename T>
std::vector<TestParam<T>> GetLumaTestParams(
    std::initializer_list<int> bit_depths, T test_func) {
  std::set<BlockSize> sizes;
  for (int b = BLOCK_4X4; b < BLOCK_SIZES_ALL; ++b) {
    const int w = block_size_wide[b];
    const int h = block_size_high[b];
    sizes.insert(BlockSize(w, h));
  }
  std::vector<TestParam<T>> result;
  for (int bit_depth : bit_depths) {
    for (const auto &block : sizes) {
      result.push_back(TestParam<T>(block, bit_depth, test_func));
    }
  }
  return result;
}

template <typename T>
std::vector<TestParam<T>> GetLowbdLumaTestParams(T test_func) {
  return GetLumaTestParams({ 8 }, test_func);
}

template <typename T>
::testing::internal::ParamGenerator<TestParam<T>> BuildLowbdLumaParams(
    T test_func) {
  return ::testing::ValuesIn(GetLowbdLumaTestParams(test_func));
}

TEST_F(AV1ConvolveParametersTest, GetLowbdLumaTestParams) {
  auto v = GetLowbdLumaTestParams(av1_dist_wtd_convolve_x_c);
  ASSERT_EQ(22U, v.size());
  for (const auto &e : v) {
    ASSERT_EQ(8, e.BitDepth());
    bool same_fn = av1_dist_wtd_convolve_x_c == e.TestFunction();
    ASSERT_TRUE(same_fn);
  }
}

#if CONFIG_AV1_HIGHBITDEPTH
template <typename T>
std::vector<TestParam<T>> GetHighbdLumaTestParams(T test_func) {
  return GetLumaTestParams({ 10, 12 }, test_func);
}

TEST_F(AV1ConvolveParametersTest, GetHighbdLumaTestParams) {
  auto v = GetHighbdLumaTestParams(av1_highbd_dist_wtd_convolve_x_c);
  ASSERT_EQ(44U, v.size());
  int num_10 = 0;
  int num_12 = 0;
  for (const auto &e : v) {
    ASSERT_TRUE(10 == e.BitDepth() || 12 == e.BitDepth());
    bool same_fn = av1_highbd_dist_wtd_convolve_x_c == e.TestFunction();
    ASSERT_TRUE(same_fn);
    if (e.BitDepth() == 10) {
      ++num_10;
    } else {
      ++num_12;
    }
  }
  ASSERT_EQ(num_10, num_12);
}

template <typename T>
::testing::internal::ParamGenerator<TestParam<T>> BuildHighbdLumaParams(
    T test_func) {
  return ::testing::ValuesIn(GetHighbdLumaTestParams(test_func));
}

#endif  // CONFIG_AV1_HIGHBITDEPTH

// Compound cases also need to test different frame offsets and weightings.
class CompoundParam {
 public:
  CompoundParam(bool use_dist_wtd_comp_avg, int fwd_offset, int bck_offset)
      : use_dist_wtd_comp_avg_(use_dist_wtd_comp_avg), fwd_offset_(fwd_offset),
        bck_offset_(bck_offset) {}

  bool UseDistWtdCompAvg() const { return use_dist_wtd_comp_avg_; }
  int FwdOffset() const { return fwd_offset_; }
  int BckOffset() const { return bck_offset_; }

 private:
  bool use_dist_wtd_comp_avg_;
  int fwd_offset_;
  int bck_offset_;
};

std::vector<CompoundParam> GetCompoundParams() {
  std::vector<CompoundParam> result;
  result.push_back(CompoundParam(false, 0, 0));
  for (int k = 0; k < 2; ++k) {
    for (int l = 0; l < 4; ++l) {
      result.push_back(CompoundParam(true, quant_dist_lookup_table[l][k],
                                     quant_dist_lookup_table[l][1 - k]));
    }
  }
  return result;
}

TEST_F(AV1ConvolveParametersTest, GetCompoundParams) {
  auto v = GetCompoundParams();
  ASSERT_EQ(9U, v.size());
  ASSERT_FALSE(v[0].UseDistWtdCompAvg());
  for (size_t i = 1; i < v.size(); ++i) {
    ASSERT_TRUE(v[i].UseDistWtdCompAvg());
  }
}

////////////////////////////////////////////////
// Compound convolve-x functions (low bit-depth)
////////////////////////////////////////////////

ConvolveParams GetConvolveParams(int do_average, CONV_BUF_TYPE *conv_buf,
                                 int width, int bit_depth,
                                 const CompoundParam &compound) {
  ConvolveParams conv_params =
      get_conv_params_no_round(do_average, 0, conv_buf, width, 1, bit_depth);
  conv_params.use_dist_wtd_comp_avg = compound.UseDistWtdCompAvg();
  conv_params.fwd_offset = compound.FwdOffset();
  conv_params.bck_offset = compound.BckOffset();
  return conv_params;
}

class AV1ConvolveXCompoundTest : public AV1ConvolveTest<convolve_x_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    // Do not test the no-op filter.
    for (int sub_pix = 1; sub_pix < 16; ++sub_pix) {
      for (int f = EIGHTTAP_REGULAR; f < INTERP_FILTERS_ALL; ++f) {
        for (const auto &c : compound_params) {
          TestConvolve(sub_pix, static_cast<InterpFilter>(f), c);
        }
      }
    }
  }

 protected:
  virtual const InterpFilterParams *FilterParams(InterpFilter f,
                                                 const BlockSize &block) const {
    return av1_get_interp_filter_params_with_block_size(f, block.Width());
  }

  virtual convolve_x_func ReferenceFunc() const {
    return av1_dist_wtd_convolve_x_c;
  }

 private:
  void TestConvolve(const int sub_pix, const InterpFilter filter,
                    const CompoundParam &compound) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const uint8_t *input1 = FirstRandomInput8(GetParam());
    const uint8_t *input2 = SecondRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, reference_conv_buf[MAX_SB_SQUARE]);
    Convolve(ReferenceFunc(), input1, input2, reference, reference_conv_buf,
             compound, sub_pix, filter);

    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, test_conv_buf[MAX_SB_SQUARE]);
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound, sub_pix, filter);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(convolve_x_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, CONV_BUF_TYPE *conv_buf,
                const CompoundParam &compound, const int sub_pix,
                const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const InterpFilterParams *filter_params =
        FilterParams(filter, GetParam().Block());

    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params);
  }
};

TEST_P(AV1ConvolveXCompoundTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveXCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_x_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1ConvolveXCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_x_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1ConvolveXCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_x_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveXCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_x_neon));
#endif

#if HAVE_NEON_DOTPROD
INSTANTIATE_TEST_SUITE_P(
    NEON_DOTPROD, AV1ConvolveXCompoundTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_x_neon_dotprod));
#endif

#if HAVE_NEON_I8MM
INSTANTIATE_TEST_SUITE_P(
    NEON_I8MM, AV1ConvolveXCompoundTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_x_neon_i8mm));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////
// Compound convolve-x functions (high bit-depth)
/////////////////////////////////////////////////
class AV1ConvolveXHighbdCompoundTest
    : public AV1ConvolveTest<highbd_convolve_x_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    // Do not test the no-op filter.
    for (int sub_pix = 1; sub_pix < 16; ++sub_pix) {
      for (int f = EIGHTTAP_REGULAR; f < INTERP_FILTERS_ALL; ++f) {
        for (const auto &c : compound_params) {
          TestConvolve(sub_pix, static_cast<InterpFilter>(f), c);
        }
      }
    }
  }

 protected:
  virtual const InterpFilterParams *FilterParams(InterpFilter f,
                                                 const BlockSize &block) const {
    return av1_get_interp_filter_params_with_block_size(f, block.Width());
  }

  virtual highbd_convolve_x_func ReferenceFunc() const {
    return av1_highbd_dist_wtd_convolve_x_c;
  }

 private:
  void TestConvolve(const int sub_pix, const InterpFilter filter,
                    const CompoundParam &compound) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();

    const uint16_t *input1 = FirstRandomInput16(GetParam());
    const uint16_t *input2 = SecondRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, reference_conv_buf[MAX_SB_SQUARE]);
    Convolve(ReferenceFunc(), input1, input2, reference, reference_conv_buf,
             compound, sub_pix, filter);

    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, test_conv_buf[MAX_SB_SQUARE]);
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound, sub_pix, filter);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

  void Convolve(highbd_convolve_x_func test_func, const uint16_t *src1,
                const uint16_t *src2, uint16_t *dst, CONV_BUF_TYPE *conv_buf,
                const CompoundParam &compound, const int sub_pix,
                const InterpFilter filter) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();
    const int bit_depth = GetParam().BitDepth();
    const InterpFilterParams *filter_params =
        FilterParams(filter, GetParam().Block());
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params, bit_depth);
    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params,
              sub_pix, &conv_params, bit_depth);
  }
};

TEST_P(AV1ConvolveXHighbdCompoundTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(
    C, AV1ConvolveXHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_x_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, AV1ConvolveXHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_x_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1ConvolveXHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_x_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1ConvolveXHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_x_neon));
#endif

#if HAVE_SVE2
INSTANTIATE_TEST_SUITE_P(
    SVE2, AV1ConvolveXHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_x_sve2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

////////////////////////////////////////////////
// Compound convolve-y functions (low bit-depth)
////////////////////////////////////////////////

// Note that the X and Y convolve functions have the same type signature and
// logic; they only differentiate the filter parameters and reference function.
class AV1ConvolveYCompoundTest : public AV1ConvolveXCompoundTest {
 protected:
  const InterpFilterParams *FilterParams(
      InterpFilter f, const BlockSize &block) const override {
    return av1_get_interp_filter_params_with_block_size(f, block.Height());
  }

  convolve_x_func ReferenceFunc() const override {
    return av1_dist_wtd_convolve_y_c;
  }
};

TEST_P(AV1ConvolveYCompoundTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1ConvolveYCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_y_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(SSE2, AV1ConvolveYCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_y_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1ConvolveYCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_y_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1ConvolveYCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_y_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
/////////////////////////////////////////////////
// Compound convolve-y functions (high bit-depth)
/////////////////////////////////////////////////

// Again, the X and Y convolve functions have the same type signature and logic.
class AV1ConvolveYHighbdCompoundTest : public AV1ConvolveXHighbdCompoundTest {
  highbd_convolve_x_func ReferenceFunc() const override {
    return av1_highbd_dist_wtd_convolve_y_c;
  }
  const InterpFilterParams *FilterParams(
      InterpFilter f, const BlockSize &block) const override {
    return av1_get_interp_filter_params_with_block_size(f, block.Height());
  }
};

TEST_P(AV1ConvolveYHighbdCompoundTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(
    C, AV1ConvolveYHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_y_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, AV1ConvolveYHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_y_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1ConvolveYHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_y_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1ConvolveYHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_y_neon));
#endif

#if HAVE_SVE2
INSTANTIATE_TEST_SUITE_P(
    SVE2, AV1ConvolveYHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_y_sve2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

//////////////////////////////////////////////////////
// Compound convolve-2d-copy functions (low bit-depth)
//////////////////////////////////////////////////////
typedef void (*compound_conv_2d_copy_func)(const uint8_t *src, int src_stride,
                                           uint8_t *dst, int dst_stride, int w,
                                           int h, ConvolveParams *conv_params);

class AV1Convolve2DCopyCompoundTest
    : public AV1ConvolveTest<compound_conv_2d_copy_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (const auto &compound : compound_params) {
      TestConvolve(compound);
    }
  }
  void SpeedTest() {
    for (const auto &compound : GetCompoundParams()) {
      TestConvolveSpeed(compound, 100000);
    }
  }

 private:
  void TestConvolve(const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();

    const uint8_t *input1 = FirstRandomInput8(GetParam());
    const uint8_t *input2 = SecondRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, reference_conv_buf[MAX_SB_SQUARE]);
    Convolve(av1_dist_wtd_convolve_2d_copy_c, input1, input2, reference,
             reference_conv_buf, compound);

    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, test_conv_buf[MAX_SB_SQUARE]);
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

  void TestConvolveSpeed(const CompoundParam &compound, const int num_iters) {
    const int width = GetParam().Block().Width();
    const int height = GetParam().Block().Height();

    const uint8_t *src0 = FirstRandomInput8(GetParam());
    const uint8_t *src1 = SecondRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, dst[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, conv_buf[MAX_SB_SQUARE]);

    const auto test_func = GetParam().TestFunction();

    ConvolveParams conv_params_0 =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);
    ConvolveParams conv_params_1 =
        GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      av1_dist_wtd_convolve_2d_copy_c(src0, width, dst, kOutputStride, width,
                                      height, &conv_params_0);
      av1_dist_wtd_convolve_2d_copy_c(src1, width, dst, kOutputStride, width,
                                      height, &conv_params_1);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));

    aom_usec_timer_start(&timer);
    for (int i = 0; i < num_iters; ++i) {
      test_func(src0, width, dst, kOutputStride, width, height, &conv_params_0);
      test_func(src1, width, dst, kOutputStride, width, height, &conv_params_1);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    printf("Dist Weighted: %d %3dx%-3d:%7.2f/%7.2fns (%3.2f)\n",
           compound.UseDistWtdCompAvg(), width, height, time1, time2,
           time1 / time2);
  }

  void Convolve(compound_conv_2d_copy_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, uint16_t *conv_buf,
                const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);
    test_func(src1, width, dst, kOutputStride, width, height, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, &conv_params);
  }
};

TEST_P(AV1Convolve2DCopyCompoundTest, RunTest) { RunTest(); }
TEST_P(AV1Convolve2DCopyCompoundTest, DISABLED_SpeedTest) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1Convolve2DCopyCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_copy_c));

#if HAVE_SSE2
INSTANTIATE_TEST_SUITE_P(
    SSE2, AV1Convolve2DCopyCompoundTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_copy_sse2));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1Convolve2DCopyCompoundTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_copy_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1Convolve2DCopyCompoundTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_copy_neon));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
///////////////////////////////////////////////////////
// Compound convolve-2d-copy functions (high bit-depth)
///////////////////////////////////////////////////////
typedef void (*highbd_compound_conv_2d_copy_func)(const uint16_t *src,
                                                  int src_stride, uint16_t *dst,
                                                  int dst_stride, int w, int h,
                                                  ConvolveParams *conv_params,
                                                  int bd);

class AV1Convolve2DCopyHighbdCompoundTest
    : public AV1ConvolveTest<highbd_compound_conv_2d_copy_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (const auto &compound : compound_params) {
      TestConvolve(compound);
    }
  }

 private:
  void TestConvolve(const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();

    const uint16_t *input1 = FirstRandomInput16(GetParam());
    const uint16_t *input2 = SecondRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, reference_conv_buf[MAX_SB_SQUARE]);
    Convolve(av1_highbd_dist_wtd_convolve_2d_copy_c, input1, input2, reference,
             reference_conv_buf, compound);

    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, test_conv_buf[MAX_SB_SQUARE]);
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

  void Convolve(highbd_compound_conv_2d_copy_func test_func,
                const uint16_t *src1, const uint16_t *src2, uint16_t *dst,
                uint16_t *conv_buf, const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const int bit_depth = GetParam().BitDepth();

    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, &conv_params,
              bit_depth);

    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, &conv_params,
              bit_depth);
  }
};

TEST_P(AV1Convolve2DCopyHighbdCompoundTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(
    C, AV1Convolve2DCopyHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_copy_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, AV1Convolve2DCopyHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_copy_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1Convolve2DCopyHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_copy_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1Convolve2DCopyHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_copy_neon));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

/////////////////////////////////////////////////
// Compound convolve-2d functions (low bit-depth)
/////////////////////////////////////////////////

class AV1Convolve2DCompoundTest : public AV1ConvolveTest<convolve_2d_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (int h_f = EIGHTTAP_REGULAR; h_f < INTERP_FILTERS_ALL; ++h_f) {
      for (int v_f = EIGHTTAP_REGULAR; v_f < INTERP_FILTERS_ALL; ++v_f) {
        // Do not test the no-op filter.
        for (int sub_x = 1; sub_x < 16; ++sub_x) {
          for (int sub_y = 1; sub_y < 16; ++sub_y) {
            for (const auto &compound : compound_params) {
              TestConvolve(static_cast<InterpFilter>(h_f),
                           static_cast<InterpFilter>(v_f), sub_x, sub_y,
                           compound);
            }
          }
        }
      }
    }
  }

 private:
  void TestConvolve(const InterpFilter h_f, const InterpFilter v_f,
                    const int sub_x, const int sub_y,
                    const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();

    const uint8_t *input1 = FirstRandomInput8(GetParam());
    const uint8_t *input2 = SecondRandomInput8(GetParam());
    DECLARE_ALIGNED(32, uint8_t, reference[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, reference_conv_buf[MAX_SB_SQUARE]);
    Convolve(av1_dist_wtd_convolve_2d_c, input1, input2, reference,
             reference_conv_buf, compound, h_f, v_f, sub_x, sub_y);

    DECLARE_ALIGNED(32, uint8_t, test[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, test_conv_buf[MAX_SB_SQUARE]);
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound, h_f, v_f, sub_x, sub_y);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(convolve_2d_func test_func, const uint8_t *src1,
                const uint8_t *src2, uint8_t *dst, uint16_t *conv_buf,
                const CompoundParam &compound, const InterpFilter h_f,
                const InterpFilter v_f, const int sub_x, const int sub_y) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();

    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, 8, compound);

    test_func(src1, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params);

    conv_params = GetConvolveParams(1, conv_buf, kOutputStride, 8, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params);
  }
};

TEST_P(AV1Convolve2DCompoundTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(C, AV1Convolve2DCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_c));

#if HAVE_SSSE3
INSTANTIATE_TEST_SUITE_P(SSSE3, AV1Convolve2DCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_ssse3));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, AV1Convolve2DCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(NEON, AV1Convolve2DCompoundTest,
                         BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_neon));
#endif

#if HAVE_NEON_DOTPROD
INSTANTIATE_TEST_SUITE_P(
    NEON_DOTPROD, AV1Convolve2DCompoundTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_neon_dotprod));
#endif

#if HAVE_NEON_I8MM
INSTANTIATE_TEST_SUITE_P(
    NEON_I8MM, AV1Convolve2DCompoundTest,
    BuildLowbdLumaParams(av1_dist_wtd_convolve_2d_neon_i8mm));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
//////////////////////////////////////////////////
// Compound convolve-2d functions (high bit-depth)
//////////////////////////////////////////////////

class AV1Convolve2DHighbdCompoundTest
    : public AV1ConvolveTest<highbd_convolve_2d_func> {
 public:
  void RunTest() {
    auto compound_params = GetCompoundParams();
    for (int h_f = EIGHTTAP_REGULAR; h_f < INTERP_FILTERS_ALL; ++h_f) {
      for (int v_f = EIGHTTAP_REGULAR; v_f < INTERP_FILTERS_ALL; ++v_f) {
        // Do not test the no-op filter.
        for (int sub_x = 1; sub_x < 16; ++sub_x) {
          for (int sub_y = 1; sub_y < 16; ++sub_y) {
            for (const auto &compound : compound_params) {
              TestConvolve(static_cast<InterpFilter>(h_f),
                           static_cast<InterpFilter>(v_f), sub_x, sub_y,
                           compound);
            }
          }
        }
      }
    }
  }

 private:
  void TestConvolve(const InterpFilter h_f, const InterpFilter v_f,
                    const int sub_x, const int sub_y,
                    const CompoundParam &compound) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();
    const uint16_t *input1 = FirstRandomInput16(GetParam());
    const uint16_t *input2 = SecondRandomInput16(GetParam());
    DECLARE_ALIGNED(32, uint16_t, reference[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, reference_conv_buf[MAX_SB_SQUARE]);
    Convolve(av1_highbd_dist_wtd_convolve_2d_c, input1, input2, reference,
             reference_conv_buf, compound, h_f, v_f, sub_x, sub_y);

    DECLARE_ALIGNED(32, uint16_t, test[MAX_SB_SQUARE]);
    DECLARE_ALIGNED(32, CONV_BUF_TYPE, test_conv_buf[MAX_SB_SQUARE]);
    Convolve(GetParam().TestFunction(), input1, input2, test, test_conv_buf,
             compound, h_f, v_f, sub_x, sub_y);

    AssertOutputBufferEq(reference_conv_buf, test_conv_buf, width, height);
    AssertOutputBufferEq(reference, test, width, height);
  }

 private:
  void Convolve(highbd_convolve_2d_func test_func, const uint16_t *src1,
                const uint16_t *src2, uint16_t *dst, uint16_t *conv_buf,
                const CompoundParam &compound, const InterpFilter h_f,
                const InterpFilter v_f, const int sub_x, const int sub_y) {
    const BlockSize &block = GetParam().Block();
    const int width = block.Width();
    const int height = block.Height();

    const InterpFilterParams *filter_params_x =
        av1_get_interp_filter_params_with_block_size(h_f, width);
    const InterpFilterParams *filter_params_y =
        av1_get_interp_filter_params_with_block_size(v_f, height);
    const int bit_depth = GetParam().BitDepth();
    ConvolveParams conv_params =
        GetConvolveParams(0, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src1, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params, bit_depth);

    conv_params =
        GetConvolveParams(1, conv_buf, kOutputStride, bit_depth, compound);
    test_func(src2, width, dst, kOutputStride, width, height, filter_params_x,
              filter_params_y, sub_x, sub_y, &conv_params, bit_depth);
  }
};

TEST_P(AV1Convolve2DHighbdCompoundTest, RunTest) { RunTest(); }

INSTANTIATE_TEST_SUITE_P(
    C, AV1Convolve2DHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, AV1Convolve2DHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_sse4_1));
#endif

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(
    AVX2, AV1Convolve2DHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_avx2));
#endif

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, AV1Convolve2DHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_neon));
#endif

#if HAVE_SVE2
INSTANTIATE_TEST_SUITE_P(
    SVE2, AV1Convolve2DHighbdCompoundTest,
    BuildHighbdLumaParams(av1_highbd_dist_wtd_convolve_2d_sve2));
#endif

#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
