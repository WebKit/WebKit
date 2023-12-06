/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
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
#include "test/acm_random.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "av1/common/common_data.h"

namespace {
const int kTestIters = 10;
const int kPerfIters = 1000;

const int kVPad = 32;
const int kHPad = 32;
const int kXStepQn = 16;
const int kYStepQn = 20;

using libaom_test::ACMRandom;
using std::make_tuple;
using std::tuple;

enum NTaps { EIGHT_TAP, TEN_TAP, TWELVE_TAP };
int NTapsToInt(NTaps ntaps) { return 8 + static_cast<int>(ntaps) * 2; }

// A 16-bit filter with a configurable number of taps.
class TestFilter {
 public:
  void set(NTaps ntaps, bool backwards);

  InterpFilterParams params_;

 private:
  std::vector<int16_t> coeffs_;
};

void TestFilter::set(NTaps ntaps, bool backwards) {
  const int n = NTapsToInt(ntaps);
  assert(n >= 8 && n <= 12);

  // The filter has n * SUBPEL_SHIFTS proper elements and an extra 8 bogus
  // elements at the end so that convolutions can read off the end safely.
  coeffs_.resize(n * SUBPEL_SHIFTS + 8);

  // The coefficients are pretty much arbitrary, but convolutions shouldn't
  // over or underflow. For the first filter (subpels = 0), we use an
  // increasing or decreasing ramp (depending on the backwards parameter). We
  // don't want any zero coefficients, so we make it have an x-intercept at -1
  // or n. To ensure absence of under/overflow, we normalise the area under the
  // ramp to be I = 1 << FILTER_BITS (so that convolving a constant function
  // gives the identity).
  //
  // When increasing, the function has the form:
  //
  //   f(x) = A * (x + 1)
  //
  // Summing and rearranging for A gives A = 2 * I / (n * (n + 1)). If the
  // filter is reversed, we have the same A but with formula
  //
  //   g(x) = A * (n - x)
  const int I = 1 << FILTER_BITS;
  const float A = 2.f * I / (n * (n + 1.f));
  for (int i = 0; i < n; ++i) {
    coeffs_[i] = static_cast<int16_t>(A * (backwards ? (n - i) : (i + 1)));
  }

  // For the other filters, make them slightly different by swapping two
  // columns. Filter k will have the columns (k % n) and (7 * k) % n swapped.
  const size_t filter_size = sizeof(coeffs_[0] * n);
  int16_t *const filter0 = &coeffs_[0];
  for (int k = 1; k < SUBPEL_SHIFTS; ++k) {
    int16_t *filterk = &coeffs_[k * n];
    memcpy(filterk, filter0, filter_size);

    const int idx0 = k % n;
    const int idx1 = (7 * k) % n;

    const int16_t tmp = filterk[idx0];
    filterk[idx0] = filterk[idx1];
    filterk[idx1] = tmp;
  }

  // Finally, write some rubbish at the end to make sure we don't use it.
  for (int i = 0; i < 8; ++i) coeffs_[n * SUBPEL_SHIFTS + i] = 123 + i;

  // Fill in params
  params_.filter_ptr = &coeffs_[0];
  params_.taps = n;
  // These are ignored by the functions being tested. Set them to whatever.
  params_.interp_filter = EIGHTTAP_REGULAR;
}

template <typename SrcPixel>
class TestImage {
 public:
  TestImage(int w, int h, int bd) : w_(w), h_(h), bd_(bd) {
    assert(bd < 16);
    assert(bd <= 8 * static_cast<int>(sizeof(SrcPixel)));

    // Pad width by 2*kHPad and then round up to the next multiple of 16
    // to get src_stride_. Add another 16 for dst_stride_ (to make sure
    // something goes wrong if we use the wrong one)
    src_stride_ = (w_ + 2 * kHPad + 15) & ~15;
    dst_stride_ = src_stride_ + 16;

    // Allocate image data
    src_data_.resize(2 * src_block_size());
    dst_data_.resize(2 * dst_block_size());
    dst_16_data_.resize(2 * dst_block_size());
  }

  void Initialize(ACMRandom *rnd);
  void Check() const;

  int src_stride() const { return src_stride_; }
  int dst_stride() const { return dst_stride_; }

  int src_block_size() const { return (h_ + 2 * kVPad) * src_stride(); }
  int dst_block_size() const { return (h_ + 2 * kVPad) * dst_stride(); }

  const SrcPixel *GetSrcData(bool ref, bool borders) const {
    const SrcPixel *block = &src_data_[ref ? 0 : src_block_size()];
    return borders ? block : block + kHPad + src_stride_ * kVPad;
  }

  SrcPixel *GetDstData(bool ref, bool borders) {
    SrcPixel *block = &dst_data_[ref ? 0 : dst_block_size()];
    return borders ? block : block + kHPad + dst_stride_ * kVPad;
  }

  CONV_BUF_TYPE *GetDst16Data(bool ref, bool borders) {
    CONV_BUF_TYPE *block = &dst_16_data_[ref ? 0 : dst_block_size()];
    return borders ? block : block + kHPad + dst_stride_ * kVPad;
  }

 private:
  int w_, h_, bd_;
  int src_stride_, dst_stride_;

  std::vector<SrcPixel> src_data_;
  std::vector<SrcPixel> dst_data_;
  std::vector<CONV_BUF_TYPE> dst_16_data_;
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

template <typename SrcPixel>
void TestImage<SrcPixel>::Initialize(ACMRandom *rnd) {
  PrepBuffers(rnd, w_, h_, src_stride_, bd_, false, &src_data_[0]);
  PrepBuffers(rnd, w_, h_, dst_stride_, bd_, true, &dst_data_[0]);
  PrepBuffers(rnd, w_, h_, dst_stride_, bd_, true, &dst_16_data_[0]);
}

template <typename SrcPixel>
void TestImage<SrcPixel>::Check() const {
  // If memcmp returns 0, there's nothing to do.
  const int num_pixels = dst_block_size();
  const SrcPixel *ref_dst = &dst_data_[0];
  const SrcPixel *tst_dst = &dst_data_[num_pixels];

  const CONV_BUF_TYPE *ref_16_dst = &dst_16_data_[0];
  const CONV_BUF_TYPE *tst_16_dst = &dst_16_data_[num_pixels];

  if (0 == memcmp(ref_dst, tst_dst, sizeof(*ref_dst) * num_pixels)) {
    if (0 == memcmp(ref_16_dst, tst_16_dst, sizeof(*ref_16_dst) * num_pixels))
      return;
  }
  // Otherwise, iterate through the buffer looking for differences (including
  // the edges)
  const int stride = dst_stride_;
  for (int r = 0; r < h_ + 2 * kVPad; ++r) {
    for (int c = 0; c < w_ + 2 * kHPad; ++c) {
      const int32_t ref_value = ref_dst[r * stride + c];
      const int32_t tst_value = tst_dst[r * stride + c];

      EXPECT_EQ(tst_value, ref_value)
          << "Error at row: " << (r - kVPad) << ", col: " << (c - kHPad);
    }
  }

  for (int r = 0; r < h_ + 2 * kVPad; ++r) {
    for (int c = 0; c < w_ + 2 * kHPad; ++c) {
      const int32_t ref_value = ref_16_dst[r * stride + c];
      const int32_t tst_value = tst_16_dst[r * stride + c];

      EXPECT_EQ(tst_value, ref_value)
          << "Error in 16 bit buffer "
          << "Error at row: " << (r - kVPad) << ", col: " << (c - kHPad);
    }
  }
}

typedef tuple<int, int> BlockDimension;

struct BaseParams {
  BaseParams(BlockDimension dimensions, NTaps num_taps_x, NTaps num_taps_y,
             bool average)
      : dims(dimensions), ntaps_x(num_taps_x), ntaps_y(num_taps_y),
        avg(average) {}

  BlockDimension dims;
  NTaps ntaps_x, ntaps_y;
  bool avg;
};

template <typename SrcPixel>
class ConvolveScaleTestBase : public ::testing::Test {
 public:
  ConvolveScaleTestBase() : image_(nullptr) {}
  ~ConvolveScaleTestBase() override { delete image_; }

  // Implemented by subclasses (SetUp depends on the parameters passed
  // in and RunOne depends on the function to be tested. These can't
  // be templated for low/high bit depths because they have different
  // numbers of parameters)
  void SetUp() override = 0;
  virtual void RunOne(bool ref) = 0;

 protected:
  void SetParams(const BaseParams &params, int bd) {
    width_ = std::get<0>(params.dims);
    height_ = std::get<1>(params.dims);
    ntaps_x_ = params.ntaps_x;
    ntaps_y_ = params.ntaps_y;
    bd_ = bd;
    avg_ = params.avg;

    filter_x_.set(ntaps_x_, false);
    filter_y_.set(ntaps_y_, true);
    convolve_params_ =
        get_conv_params_no_round(avg_ != false, 0, nullptr, 0, 1, bd);

    delete image_;
    image_ = new TestImage<SrcPixel>(width_, height_, bd_);
    ASSERT_NE(image_, nullptr);
  }

  void SetConvParamOffset(int i, int j, int is_compound, int do_average,
                          int use_dist_wtd_comp_avg) {
    if (i == -1 && j == -1) {
      convolve_params_.use_dist_wtd_comp_avg = use_dist_wtd_comp_avg;
      convolve_params_.is_compound = is_compound;
      convolve_params_.do_average = do_average;
    } else {
      convolve_params_.use_dist_wtd_comp_avg = use_dist_wtd_comp_avg;
      convolve_params_.fwd_offset = quant_dist_lookup_table[j][i];
      convolve_params_.bck_offset = quant_dist_lookup_table[j][1 - i];
      convolve_params_.is_compound = is_compound;
      convolve_params_.do_average = do_average;
    }
  }

  void Run() {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    for (int i = 0; i < kTestIters; ++i) {
      int is_compound = 0;
      SetConvParamOffset(-1, -1, is_compound, 0, 0);
      Prep(&rnd);
      RunOne(true);
      RunOne(false);
      image_->Check();

      is_compound = 1;
      for (int do_average = 0; do_average < 2; do_average++) {
        for (int use_dist_wtd_comp_avg = 0; use_dist_wtd_comp_avg < 2;
             use_dist_wtd_comp_avg++) {
          for (int j = 0; j < 2; ++j) {
            for (int k = 0; k < 4; ++k) {
              SetConvParamOffset(j, k, is_compound, do_average,
                                 use_dist_wtd_comp_avg);
              Prep(&rnd);
              RunOne(true);
              RunOne(false);
              image_->Check();
            }
          }
        }
      }
    }
  }

  void SpeedTest() {
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
        << "Error: CDEFSpeedTest, SIMD slower than C.\n"
        << "C time: " << ref_time << " us\n"
        << "SIMD time: " << tst_time << " us\n";
  }

  static int RandomSubpel(ACMRandom *rnd) {
    const uint8_t subpel_mode = rnd->Rand8();
    if ((subpel_mode & 7) == 0) {
      return 0;
    } else if ((subpel_mode & 7) == 1) {
      return SCALE_SUBPEL_SHIFTS - 1;
    } else {
      return 1 + rnd->PseudoUniform(SCALE_SUBPEL_SHIFTS - 2);
    }
  }

  void Prep(ACMRandom *rnd) {
    assert(rnd);

    // Choose subpel_x_ and subpel_y_. They should be less than
    // SCALE_SUBPEL_SHIFTS; we also want to add extra weight to "interesting"
    // values: 0 and SCALE_SUBPEL_SHIFTS - 1
    subpel_x_ = RandomSubpel(rnd);
    subpel_y_ = RandomSubpel(rnd);

    image_->Initialize(rnd);
  }

  int width_, height_, bd_;
  NTaps ntaps_x_, ntaps_y_;
  bool avg_;
  int subpel_x_, subpel_y_;
  TestFilter filter_x_, filter_y_;
  TestImage<SrcPixel> *image_;
  ConvolveParams convolve_params_;
};

typedef tuple<int, int> BlockDimension;

typedef void (*LowbdConvolveFunc)(const uint8_t *src, int src_stride,
                                  uint8_t *dst, int dst_stride, int w, int h,
                                  const InterpFilterParams *filter_params_x,
                                  const InterpFilterParams *filter_params_y,
                                  const int subpel_x_qn, const int x_step_qn,
                                  const int subpel_y_qn, const int y_step_qn,
                                  ConvolveParams *conv_params);

// Test parameter list:
//  <tst_fun, dims, ntaps_x, ntaps_y, avg>
typedef tuple<LowbdConvolveFunc, BlockDimension, NTaps, NTaps, bool>
    LowBDParams;

class LowBDConvolveScaleTest
    : public ConvolveScaleTestBase<uint8_t>,
      public ::testing::WithParamInterface<LowBDParams> {
 public:
  ~LowBDConvolveScaleTest() override = default;

  void SetUp() override {
    tst_fun_ = GET_PARAM(0);

    const BlockDimension &block = GET_PARAM(1);
    const NTaps ntaps_x = GET_PARAM(2);
    const NTaps ntaps_y = GET_PARAM(3);
    const int bd = 8;
    const bool avg = GET_PARAM(4);

    SetParams(BaseParams(block, ntaps_x, ntaps_y, avg), bd);
  }

  void RunOne(bool ref) override {
    const uint8_t *src = image_->GetSrcData(ref, false);
    uint8_t *dst = image_->GetDstData(ref, false);
    convolve_params_.dst = image_->GetDst16Data(ref, false);
    const int src_stride = image_->src_stride();
    const int dst_stride = image_->dst_stride();
    if (ref) {
      av1_convolve_2d_scale_c(src, src_stride, dst, dst_stride, width_, height_,
                              &filter_x_.params_, &filter_y_.params_, subpel_x_,
                              kXStepQn, subpel_y_, kYStepQn, &convolve_params_);
    } else {
      tst_fun_(src, src_stride, dst, dst_stride, width_, height_,
               &filter_x_.params_, &filter_y_.params_, subpel_x_, kXStepQn,
               subpel_y_, kYStepQn, &convolve_params_);
    }
  }

 private:
  LowbdConvolveFunc tst_fun_;
};

const BlockDimension kBlockDim[] = {
  make_tuple(2, 2),    make_tuple(2, 4),    make_tuple(4, 4),
  make_tuple(4, 8),    make_tuple(8, 4),    make_tuple(8, 8),
  make_tuple(8, 16),   make_tuple(16, 8),   make_tuple(16, 16),
  make_tuple(16, 32),  make_tuple(32, 16),  make_tuple(32, 32),
  make_tuple(32, 64),  make_tuple(64, 32),  make_tuple(64, 64),
  make_tuple(64, 128), make_tuple(128, 64), make_tuple(128, 128),
};

const NTaps kNTaps[] = { EIGHT_TAP };

TEST_P(LowBDConvolveScaleTest, Check) { Run(); }
TEST_P(LowBDConvolveScaleTest, DISABLED_Speed) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(
    C, LowBDConvolveScaleTest,
    ::testing::Combine(::testing::Values(av1_convolve_2d_scale_c),
                       ::testing::ValuesIn(kBlockDim),
                       ::testing::ValuesIn(kNTaps), ::testing::ValuesIn(kNTaps),
                       ::testing::Bool()));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, LowBDConvolveScaleTest,
    ::testing::Combine(::testing::Values(av1_convolve_2d_scale_sse4_1),
                       ::testing::ValuesIn(kBlockDim),
                       ::testing::ValuesIn(kNTaps), ::testing::ValuesIn(kNTaps),
                       ::testing::Bool()));
#endif  // HAVE_SSE4_1

#if CONFIG_AV1_HIGHBITDEPTH
typedef void (*HighbdConvolveFunc)(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_x,
                                   const InterpFilterParams *filter_params_y,
                                   const int subpel_x_qn, const int x_step_qn,
                                   const int subpel_y_qn, const int y_step_qn,
                                   ConvolveParams *conv_params, int bd);

// Test parameter list:
//  <tst_fun, dims, ntaps_x, ntaps_y, avg, bd>
typedef tuple<HighbdConvolveFunc, BlockDimension, NTaps, NTaps, bool, int>
    HighBDParams;

class HighBDConvolveScaleTest
    : public ConvolveScaleTestBase<uint16_t>,
      public ::testing::WithParamInterface<HighBDParams> {
 public:
  ~HighBDConvolveScaleTest() override = default;

  void SetUp() override {
    tst_fun_ = GET_PARAM(0);

    const BlockDimension &block = GET_PARAM(1);
    const NTaps ntaps_x = GET_PARAM(2);
    const NTaps ntaps_y = GET_PARAM(3);
    const bool avg = GET_PARAM(4);
    const int bd = GET_PARAM(5);

    SetParams(BaseParams(block, ntaps_x, ntaps_y, avg), bd);
  }

  void RunOne(bool ref) override {
    const uint16_t *src = image_->GetSrcData(ref, false);
    uint16_t *dst = image_->GetDstData(ref, false);
    convolve_params_.dst = image_->GetDst16Data(ref, false);
    const int src_stride = image_->src_stride();
    const int dst_stride = image_->dst_stride();

    if (ref) {
      av1_highbd_convolve_2d_scale_c(
          src, src_stride, dst, dst_stride, width_, height_, &filter_x_.params_,
          &filter_y_.params_, subpel_x_, kXStepQn, subpel_y_, kYStepQn,
          &convolve_params_, bd_);
    } else {
      tst_fun_(src, src_stride, dst, dst_stride, width_, height_,
               &filter_x_.params_, &filter_y_.params_, subpel_x_, kXStepQn,
               subpel_y_, kYStepQn, &convolve_params_, bd_);
    }
  }

 private:
  HighbdConvolveFunc tst_fun_;
};

const int kBDs[] = { 8, 10, 12 };

TEST_P(HighBDConvolveScaleTest, Check) { Run(); }
TEST_P(HighBDConvolveScaleTest, DISABLED_Speed) { SpeedTest(); }

INSTANTIATE_TEST_SUITE_P(
    C, HighBDConvolveScaleTest,
    ::testing::Combine(::testing::Values(av1_highbd_convolve_2d_scale_c),
                       ::testing::ValuesIn(kBlockDim),
                       ::testing::ValuesIn(kNTaps), ::testing::ValuesIn(kNTaps),
                       ::testing::Bool(), ::testing::ValuesIn(kBDs)));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(
    SSE4_1, HighBDConvolveScaleTest,
    ::testing::Combine(::testing::Values(av1_highbd_convolve_2d_scale_sse4_1),
                       ::testing::ValuesIn(kBlockDim),
                       ::testing::ValuesIn(kNTaps), ::testing::ValuesIn(kNTaps),
                       ::testing::Bool(), ::testing::ValuesIn(kBDs)));
#endif  // HAVE_SSE4_1

#if HAVE_NEON
INSTANTIATE_TEST_SUITE_P(
    NEON, HighBDConvolveScaleTest,
    ::testing::Combine(::testing::Values(av1_highbd_convolve_2d_scale_neon),
                       ::testing::ValuesIn(kBlockDim),
                       ::testing::ValuesIn(kNTaps), ::testing::ValuesIn(kNTaps),
                       ::testing::Bool(), ::testing::ValuesIn(kBDs)));

#endif  // HAVE_NEON

#endif  // CONFIG_AV1_HIGHBITDEPTH
}  // namespace
