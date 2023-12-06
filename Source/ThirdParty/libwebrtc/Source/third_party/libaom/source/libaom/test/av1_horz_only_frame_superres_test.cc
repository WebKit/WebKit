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

#include <tuple>
#include <vector>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/av1_rtcd.h"

#include "aom_ports/aom_timer.h"
#include "av1/common/convolve.h"
#include "av1/common/resize.h"
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

namespace {
const int kTestIters = 10;
const int kPerfIters = 1000;

const int kVPad = 32;
const int kHPad = 32;

using libaom_test::ACMRandom;
using std::make_tuple;
using std::tuple;

template <typename Pixel>
class TestImage {
 public:
  TestImage(int w_src, int h, int superres_denom, int x0, int bd)
      : w_src_(w_src), h_(h), superres_denom_(superres_denom), x0_(x0),
        bd_(bd) {
    assert(bd < 16);
    assert(bd <= 8 * static_cast<int>(sizeof(Pixel)));
    assert(9 <= superres_denom && superres_denom <= 16);
    assert(SCALE_NUMERATOR == 8);
    assert(0 <= x0_ && x0_ <= RS_SCALE_SUBPEL_MASK);

    w_dst_ = w_src_;
    av1_calculate_unscaled_superres_size(&w_dst_, nullptr, superres_denom);

    src_stride_ = ALIGN_POWER_OF_TWO(w_src_ + 2 * kHPad, 4);
    dst_stride_ = ALIGN_POWER_OF_TWO(w_dst_ + 2 * kHPad, 4);

    // Allocate image data
    src_data_.resize(2 * src_block_size());
    dst_data_.resize(2 * dst_block_size());
  }

  void Initialize(ACMRandom *rnd);
  void Check() const;

  int src_stride() const { return src_stride_; }
  int dst_stride() const { return dst_stride_; }

  int src_block_size() const { return (h_ + 2 * kVPad) * src_stride(); }
  int dst_block_size() const { return (h_ + 2 * kVPad) * dst_stride(); }

  int src_width() const { return w_src_; }
  int dst_width() const { return w_dst_; }
  int height() const { return h_; }
  int x0() const { return x0_; }

  const Pixel *GetSrcData(bool ref, bool borders) const {
    const Pixel *block = &src_data_[ref ? 0 : src_block_size()];
    return borders ? block : block + kHPad + src_stride_ * kVPad;
  }

  Pixel *GetDstData(bool ref, bool borders) {
    Pixel *block = &dst_data_[ref ? 0 : dst_block_size()];
    return borders ? block : block + kHPad + dst_stride_ * kVPad;
  }

 private:
  int w_src_, w_dst_, h_, superres_denom_, x0_, bd_;
  int src_stride_, dst_stride_;

  std::vector<Pixel> src_data_;
  std::vector<Pixel> dst_data_;
};

template <typename Pixel>
void FillEdge(ACMRandom *rnd, int num_pixels, int bd, bool trash, Pixel *data) {
  if (!trash) {
    memset(data, 0, sizeof(*data) * num_pixels);
    return;
  }
  const Pixel mask = (1 << bd) - 1;
  for (int i = 0; i < num_pixels; ++i) data[i] = rnd->Rand16() & mask;
}

template <typename Pixel>
void PrepBuffers(ACMRandom *rnd, int w, int h, int stride, int bd,
                 bool trash_edges, Pixel *data) {
  assert(rnd);
  const Pixel mask = (1 << bd) - 1;

  // Fill in the first buffer with random data
  // Top border
  FillEdge(rnd, stride * kVPad, bd, trash_edges, data);
  for (int r = 0; r < h; ++r) {
    Pixel *row_data = data + (kVPad + r) * stride;
    // Left border, contents, right border
    FillEdge(rnd, kHPad, bd, trash_edges, row_data);
    for (int c = 0; c < w; ++c) row_data[kHPad + c] = rnd->Rand16() & mask;
    FillEdge(rnd, kHPad, bd, trash_edges, row_data + kHPad + w);
  }
  // Bottom border
  FillEdge(rnd, stride * kVPad, bd, trash_edges, data + stride * (kVPad + h));

  const int bpp = sizeof(*data);
  const int block_elts = stride * (h + 2 * kVPad);
  const int block_size = bpp * block_elts;

  // Now copy that to the second buffer
  memcpy(data + block_elts, data, block_size);
}

template <typename Pixel>
void TestImage<Pixel>::Initialize(ACMRandom *rnd) {
  PrepBuffers(rnd, w_src_, h_, src_stride_, bd_, false, &src_data_[0]);
  PrepBuffers(rnd, w_dst_, h_, dst_stride_, bd_, true, &dst_data_[0]);
}

template <typename Pixel>
void TestImage<Pixel>::Check() const {
  const int num_pixels = dst_block_size();
  const Pixel *ref_dst = &dst_data_[0];
  const Pixel *tst_dst = &dst_data_[num_pixels];

  // If memcmp returns 0, there's nothing to do.
  if (0 == memcmp(ref_dst, tst_dst, sizeof(*ref_dst) * num_pixels)) return;

  // Otherwise, iterate through the buffer looking for differences, *ignoring
  // the edges*
  const int stride = dst_stride_;
  for (int r = kVPad; r < h_ + kVPad; ++r) {
    for (int c = kVPad; c < w_dst_ + kHPad; ++c) {
      const int32_t ref_value = ref_dst[r * stride + c];
      const int32_t tst_value = tst_dst[r * stride + c];

      EXPECT_EQ(tst_value, ref_value)
          << "Error at row: " << (r - kVPad) << ", col: " << (c - kHPad)
          << ", superres_denom: " << superres_denom_ << ", height: " << h_
          << ", src_width: " << w_src_ << ", dst_width: " << w_dst_
          << ", x0: " << x0_;
    }
  }
}

template <typename Pixel>
class ConvolveHorizRSTestBase : public ::testing::Test {
 public:
  ConvolveHorizRSTestBase() : image_(nullptr) {}
  ~ConvolveHorizRSTestBase() override = default;

  // Implemented by subclasses (SetUp depends on the parameters passed
  // in and RunOne depends on the function to be tested. These can't
  // be templated for low/high bit depths because they have different
  // numbers of parameters)
  void SetUp() override = 0;
  virtual void RunOne(bool ref) = 0;

 protected:
  void SetBitDepth(int bd) { bd_ = bd; }

  void CorrectnessTest() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    for (int i = 0; i < kTestIters; ++i) {
      for (int superres_denom = 9; superres_denom <= 16; superres_denom++) {
        // Get a random height between 512 and 767
        int height = rnd.Rand8() + 512;

        // Get a random src width between 128 and 383
        int width_src = rnd.Rand8() + 128;

        // x0 is normally calculated by get_upscale_convolve_x0 in
        // av1/common/resize.c. However, this test should work for
        // any value of x0 between 0 and RS_SCALE_SUBPEL_MASK
        // (inclusive), so we choose one at random.
        int x0 = rnd.Rand16() % (RS_SCALE_SUBPEL_MASK + 1);

        image_ =
            new TestImage<Pixel>(width_src, height, superres_denom, x0, bd_);
        ASSERT_NE(image_, nullptr);

        Prep(&rnd);
        RunOne(true);
        RunOne(false);
        image_->Check();

        delete image_;
      }
    }
  }

  void SpeedTest() {
    // Pick some specific parameters to test
    int height = 767;
    int width_src = 129;
    int superres_denom = 13;
    int x0 = RS_SCALE_SUBPEL_MASK >> 1;

    image_ = new TestImage<Pixel>(width_src, height, superres_denom, x0, bd_);
    ASSERT_NE(image_, nullptr);

    ACMRandom rnd(ACMRandom::DeterministicSeed());
    Prep(&rnd);

    aom_usec_timer ref_timer;
    aom_usec_timer_start(&ref_timer);
    for (int i = 0; i < kPerfIters; ++i) RunOne(true);
    aom_usec_timer_mark(&ref_timer);
    const int64_t ref_time = aom_usec_timer_elapsed(&ref_timer);

    aom_usec_timer tst_timer;
    aom_usec_timer_start(&tst_timer);
    for (int i = 0; i < kPerfIters; ++i) RunOne(false);
    aom_usec_timer_mark(&tst_timer);
    const int64_t tst_time = aom_usec_timer_elapsed(&tst_timer);

    std::cout << "[          ] C time = " << ref_time / 1000
              << " ms, SIMD time = " << tst_time / 1000 << " ms\n";

    EXPECT_GT(ref_time, tst_time)
        << "Error: ConvolveHorizRSTest (Speed Test), SIMD slower than C.\n"
        << "C time: " << ref_time << " us\n"
        << "SIMD time: " << tst_time << " us\n";
  }

  void Prep(ACMRandom *rnd) {
    assert(rnd);
    image_->Initialize(rnd);
  }

  int bd_;
  TestImage<Pixel> *image_;
};

typedef void (*LowBDConvolveHorizRsFunc)(const uint8_t *src, int src_stride,
                                         uint8_t *dst, int dst_stride, int w,
                                         int h, const int16_t *x_filters,
                                         const int x0_qn, const int x_step_qn);

// Test parameter list:
//  <tst_fun_>
typedef tuple<LowBDConvolveHorizRsFunc> LowBDParams;

class LowBDConvolveHorizRSTest
    : public ConvolveHorizRSTestBase<uint8_t>,
      public ::testing::WithParamInterface<LowBDParams> {
 public:
  ~LowBDConvolveHorizRSTest() override = default;

  void SetUp() override {
    tst_fun_ = GET_PARAM(0);
    const int bd = 8;
    SetBitDepth(bd);
  }

  void RunOne(bool ref) override {
    const uint8_t *src = image_->GetSrcData(ref, false);
    uint8_t *dst = image_->GetDstData(ref, false);
    const int src_stride = image_->src_stride();
    const int dst_stride = image_->dst_stride();
    const int width_src = image_->src_width();
    const int width_dst = image_->dst_width();
    const int height = image_->height();
    const int x0_qn = image_->x0();

    const int32_t x_step_qn =
        av1_get_upscale_convolve_step(width_src, width_dst);

    if (ref) {
      av1_convolve_horiz_rs_c(src, src_stride, dst, dst_stride, width_dst,
                              height, &av1_resize_filter_normative[0][0], x0_qn,
                              x_step_qn);
    } else {
      tst_fun_(src, src_stride, dst, dst_stride, width_dst, height,
               &av1_resize_filter_normative[0][0], x0_qn, x_step_qn);
    }
  }

 private:
  LowBDConvolveHorizRsFunc tst_fun_;
};

TEST_P(LowBDConvolveHorizRSTest, Correctness) { CorrectnessTest(); }
TEST_P(LowBDConvolveHorizRSTest, DISABLED_Speed) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(C, LowBDConvolveHorizRSTest,
                         ::testing::Values(av1_convolve_horiz_rs_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, LowBDConvolveHorizRSTest,
                         ::testing::Values(av1_convolve_horiz_rs_sse4_1));
#endif

#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*HighBDConvolveHorizRsFunc)(const uint16_t *src, int src_stride,
                                          uint16_t *dst, int dst_stride, int w,
                                          int h, const int16_t *x_filters,
                                          const int x0_qn, const int x_step_qn,
                                          int bd);

// Test parameter list:
//  <tst_fun_, bd_>
typedef tuple<HighBDConvolveHorizRsFunc, int> HighBDParams;

class HighBDConvolveHorizRSTest
    : public ConvolveHorizRSTestBase<uint16_t>,
      public ::testing::WithParamInterface<HighBDParams> {
 public:
  ~HighBDConvolveHorizRSTest() override = default;

  void SetUp() override {
    tst_fun_ = GET_PARAM(0);
    const int bd = GET_PARAM(1);
    SetBitDepth(bd);
  }

  void RunOne(bool ref) override {
    const uint16_t *src = image_->GetSrcData(ref, false);
    uint16_t *dst = image_->GetDstData(ref, false);
    const int src_stride = image_->src_stride();
    const int dst_stride = image_->dst_stride();
    const int width_src = image_->src_width();
    const int width_dst = image_->dst_width();
    const int height = image_->height();
    const int x0_qn = image_->x0();

    const int32_t x_step_qn =
        av1_get_upscale_convolve_step(width_src, width_dst);

    if (ref) {
      av1_highbd_convolve_horiz_rs_c(
          src, src_stride, dst, dst_stride, width_dst, height,
          &av1_resize_filter_normative[0][0], x0_qn, x_step_qn, bd_);
    } else {
      tst_fun_(src, src_stride, dst, dst_stride, width_dst, height,
               &av1_resize_filter_normative[0][0], x0_qn, x_step_qn, bd_);
    }
  }

 private:
  HighBDConvolveHorizRsFunc tst_fun_;
};

const int kBDs[] = { 8, 10, 12 };

TEST_P(HighBDConvolveHorizRSTest, Correctness) { CorrectnessTest(); }
TEST_P(HighBDConvolveHorizRSTest, DISABLED_Speed) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(
    C, HighBDConvolveHorizRSTest,
    ::testing::Combine(::testing::Values(av1_highbd_convolve_horiz_rs_c),
                       ::testing::ValuesIn(kBDs)));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, HighBDConvolveHorizRSTest,
    ::testing::Combine(::testing::Values(av1_highbd_convolve_horiz_rs_sse4_1),
                       ::testing::ValuesIn(kBDs)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, HighBDConvolveHorizRSTest,
    ::testing::Combine(::testing::Values(av1_highbd_convolve_horiz_rs_neon),
                       ::testing::ValuesIn(kBDs)));
#endif  // HAVE_NEON

#endif  // CONFIG_AV1_HIGHBITDEPTH

}  // namespace
