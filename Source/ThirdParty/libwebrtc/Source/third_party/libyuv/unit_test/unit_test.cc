/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "../unit_test/unit_test.h"

#include <stdlib.h>  // For getenv()

#include <cstring>

#ifdef LIBYUV_USE_ABSL_FLAGS
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#endif
#include "libyuv/cpu_id.h"

unsigned int fastrand_seed = 0xfb;

#ifdef LIBYUV_USE_ABSL_FLAGS
ABSL_FLAG(int32_t, libyuv_width, 0, "width of test image.");
ABSL_FLAG(int32_t, libyuv_height, 0, "height of test image.");
ABSL_FLAG(int32_t, libyuv_repeat, 0, "number of times to repeat test.");
ABSL_FLAG(int32_t,
          libyuv_flags,
          0,
          "cpu flags for reference code. 1 = C, -1 = SIMD");
ABSL_FLAG(int32_t,
          libyuv_cpu_info,
          0,
          "cpu flags for benchmark code. 1 = C, -1 = SIMD");
#else
// Disable command line parameters if absl/flags disabled.
static const int32_t FLAGS_libyuv_width = 0;
static const int32_t FLAGS_libyuv_height = 0;
static const int32_t FLAGS_libyuv_repeat = 0;
static const int32_t FLAGS_libyuv_flags = 0;
static const int32_t FLAGS_libyuv_cpu_info = 0;
#endif

#ifdef LIBYUV_USE_ABSL_FLAGS
#define LIBYUV_GET_FLAG(f) absl::GetFlag(f)
#else
#define LIBYUV_GET_FLAG(f) f
#endif

// Test environment variable for disabling CPU features. Any non-zero value
// to disable. Zero ignored to make it easy to set the variable on/off.
#if !defined(__native_client__) && !defined(_M_ARM)
static LIBYUV_BOOL TestEnv(const char* name) {
  const char* var = getenv(name);
  if (var) {
    if (var[0] != '0') {
      return LIBYUV_TRUE;
    }
  }
  return LIBYUV_FALSE;
}
#else  // nacl does not support getenv().
static LIBYUV_BOOL TestEnv(const char*) {
  return LIBYUV_FALSE;
}
#endif

int TestCpuEnv(int cpu_info) {
#if defined(__arm__) || defined(__aarch64__)
  if (TestEnv("LIBYUV_DISABLE_NEON")) {
    cpu_info &= ~libyuv::kCpuHasNEON;
  }
#endif
#if defined(__mips__) && defined(__linux__)
  if (TestEnv("LIBYUV_DISABLE_MSA")) {
    cpu_info &= ~libyuv::kCpuHasMSA;
  }
#endif
#if defined(__longarch__) && defined(__linux__)
  if (TestEnv("LIBYUV_DISABLE_LSX")) {
    cpu_info &= ~libyuv::kCpuHasLSX;
  }
#endif
#if defined(__longarch__) && defined(__linux__)
  if (TestEnv("LIBYUV_DISABLE_LASX")) {
    cpu_info &= ~libyuv::kCpuHasLASX;
  }
#endif
#if !defined(__pnacl__) && !defined(__CLR_VER) &&                   \
    (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
     defined(_M_IX86))
  if (TestEnv("LIBYUV_DISABLE_X86")) {
    cpu_info &= ~libyuv::kCpuHasX86;
  }
  if (TestEnv("LIBYUV_DISABLE_SSE2")) {
    cpu_info &= ~libyuv::kCpuHasSSE2;
  }
  if (TestEnv("LIBYUV_DISABLE_SSSE3")) {
    cpu_info &= ~libyuv::kCpuHasSSSE3;
  }
  if (TestEnv("LIBYUV_DISABLE_SSE41")) {
    cpu_info &= ~libyuv::kCpuHasSSE41;
  }
  if (TestEnv("LIBYUV_DISABLE_SSE42")) {
    cpu_info &= ~libyuv::kCpuHasSSE42;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX")) {
    cpu_info &= ~libyuv::kCpuHasAVX;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX2")) {
    cpu_info &= ~libyuv::kCpuHasAVX2;
  }
  if (TestEnv("LIBYUV_DISABLE_ERMS")) {
    cpu_info &= ~libyuv::kCpuHasERMS;
  }
  if (TestEnv("LIBYUV_DISABLE_FMA3")) {
    cpu_info &= ~libyuv::kCpuHasFMA3;
  }
  if (TestEnv("LIBYUV_DISABLE_F16C")) {
    cpu_info &= ~libyuv::kCpuHasF16C;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX512BW")) {
    cpu_info &= ~libyuv::kCpuHasAVX512BW;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX512VL")) {
    cpu_info &= ~libyuv::kCpuHasAVX512VL;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX512VNNI")) {
    cpu_info &= ~libyuv::kCpuHasAVX512VNNI;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX512VBMI")) {
    cpu_info &= ~libyuv::kCpuHasAVX512VBMI;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX512VBMI2")) {
    cpu_info &= ~libyuv::kCpuHasAVX512VBMI2;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX512VBITALG")) {
    cpu_info &= ~libyuv::kCpuHasAVX512VBITALG;
  }
  if (TestEnv("LIBYUV_DISABLE_AVX512VPOPCNTDQ")) {
    cpu_info &= ~libyuv::kCpuHasAVX512VPOPCNTDQ;
  }
  if (TestEnv("LIBYUV_DISABLE_GFNI")) {
    cpu_info &= ~libyuv::kCpuHasGFNI;
  }
#endif
  if (TestEnv("LIBYUV_DISABLE_ASM")) {
    cpu_info = libyuv::kCpuInitialized;
  }
  return cpu_info;
}

// For quicker unittests, default is 128 x 72.  But when benchmarking,
// default to 720p.  Allow size to specify.
// Set flags to -1 for benchmarking to avoid slower C code.

LibYUVConvertTest::LibYUVConvertTest()
    : benchmark_iterations_(1),
      benchmark_width_(128),
      benchmark_height_(72),
      disable_cpu_flags_(1),
      benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_repeat)) {
    benchmark_iterations_ = LIBYUV_GET_FLAG(FLAGS_libyuv_repeat);
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_width)) {
    benchmark_width_ = LIBYUV_GET_FLAG(FLAGS_libyuv_width);
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_height)) {
    benchmark_height_ = LIBYUV_GET_FLAG(FLAGS_libyuv_height);
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_flags)) {
    disable_cpu_flags_ = LIBYUV_GET_FLAG(FLAGS_libyuv_flags);
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info)) {
    benchmark_cpu_info_ = LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info);
  }
  disable_cpu_flags_ = TestCpuEnv(disable_cpu_flags_);
  benchmark_cpu_info_ = TestCpuEnv(benchmark_cpu_info_);
  libyuv::MaskCpuFlags(benchmark_cpu_info_);
  benchmark_pixels_div1280_ =
      static_cast<int>((static_cast<double>(Abs(benchmark_width_)) *
                            static_cast<double>(Abs(benchmark_height_)) *
                            static_cast<double>(benchmark_iterations_) +
                        1279.0) /
                       1280.0);
}

LibYUVColorTest::LibYUVColorTest()
    : benchmark_iterations_(1),
      benchmark_width_(128),
      benchmark_height_(72),
      disable_cpu_flags_(1),
      benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_repeat)) {
    benchmark_iterations_ = LIBYUV_GET_FLAG(FLAGS_libyuv_repeat);
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_width)) {
    benchmark_width_ = LIBYUV_GET_FLAG(FLAGS_libyuv_width);
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_height)) {
    benchmark_height_ = LIBYUV_GET_FLAG(FLAGS_libyuv_height);
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_flags)) {
    disable_cpu_flags_ = LIBYUV_GET_FLAG(FLAGS_libyuv_flags);
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info)) {
    benchmark_cpu_info_ = LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info);
  }
  disable_cpu_flags_ = TestCpuEnv(disable_cpu_flags_);
  benchmark_cpu_info_ = TestCpuEnv(benchmark_cpu_info_);
  libyuv::MaskCpuFlags(benchmark_cpu_info_);
  benchmark_pixels_div1280_ =
      static_cast<int>((static_cast<double>(Abs(benchmark_width_)) *
                            static_cast<double>(Abs(benchmark_height_)) *
                            static_cast<double>(benchmark_iterations_) +
                        1279.0) /
                       1280.0);
}

LibYUVScaleTest::LibYUVScaleTest()
    : benchmark_iterations_(1),
      benchmark_width_(128),
      benchmark_height_(72),
      disable_cpu_flags_(1),
      benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_repeat)) {
    benchmark_iterations_ = LIBYUV_GET_FLAG(FLAGS_libyuv_repeat);
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_width)) {
    benchmark_width_ = LIBYUV_GET_FLAG(FLAGS_libyuv_width);
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_height)) {
    benchmark_height_ = LIBYUV_GET_FLAG(FLAGS_libyuv_height);
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_flags)) {
    disable_cpu_flags_ = LIBYUV_GET_FLAG(FLAGS_libyuv_flags);
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info)) {
    benchmark_cpu_info_ = LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info);
  }
  disable_cpu_flags_ = TestCpuEnv(disable_cpu_flags_);
  benchmark_cpu_info_ = TestCpuEnv(benchmark_cpu_info_);
  libyuv::MaskCpuFlags(benchmark_cpu_info_);
  benchmark_pixels_div1280_ =
      static_cast<int>((static_cast<double>(Abs(benchmark_width_)) *
                            static_cast<double>(Abs(benchmark_height_)) *
                            static_cast<double>(benchmark_iterations_) +
                        1279.0) /
                       1280.0);
}

LibYUVRotateTest::LibYUVRotateTest()
    : benchmark_iterations_(1),
      benchmark_width_(128),
      benchmark_height_(72),
      disable_cpu_flags_(1),
      benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_repeat)) {
    benchmark_iterations_ = LIBYUV_GET_FLAG(FLAGS_libyuv_repeat);
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_width)) {
    benchmark_width_ = LIBYUV_GET_FLAG(FLAGS_libyuv_width);
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_height)) {
    benchmark_height_ = LIBYUV_GET_FLAG(FLAGS_libyuv_height);
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_flags)) {
    disable_cpu_flags_ = LIBYUV_GET_FLAG(FLAGS_libyuv_flags);
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info)) {
    benchmark_cpu_info_ = LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info);
  }
  disable_cpu_flags_ = TestCpuEnv(disable_cpu_flags_);
  benchmark_cpu_info_ = TestCpuEnv(benchmark_cpu_info_);
  libyuv::MaskCpuFlags(benchmark_cpu_info_);
  benchmark_pixels_div1280_ =
      static_cast<int>((static_cast<double>(Abs(benchmark_width_)) *
                            static_cast<double>(Abs(benchmark_height_)) *
                            static_cast<double>(benchmark_iterations_) +
                        1279.0) /
                       1280.0);
}

LibYUVPlanarTest::LibYUVPlanarTest()
    : benchmark_iterations_(1),
      benchmark_width_(128),
      benchmark_height_(72),
      disable_cpu_flags_(1),
      benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_repeat)) {
    benchmark_iterations_ = LIBYUV_GET_FLAG(FLAGS_libyuv_repeat);
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_width)) {
    benchmark_width_ = LIBYUV_GET_FLAG(FLAGS_libyuv_width);
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_height)) {
    benchmark_height_ = LIBYUV_GET_FLAG(FLAGS_libyuv_height);
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_flags)) {
    disable_cpu_flags_ = LIBYUV_GET_FLAG(FLAGS_libyuv_flags);
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info)) {
    benchmark_cpu_info_ = LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info);
  }
  disable_cpu_flags_ = TestCpuEnv(disable_cpu_flags_);
  benchmark_cpu_info_ = TestCpuEnv(benchmark_cpu_info_);
  libyuv::MaskCpuFlags(benchmark_cpu_info_);
  benchmark_pixels_div1280_ =
      static_cast<int>((static_cast<double>(Abs(benchmark_width_)) *
                            static_cast<double>(Abs(benchmark_height_)) *
                            static_cast<double>(benchmark_iterations_) +
                        1279.0) /
                       1280.0);
}

LibYUVBaseTest::LibYUVBaseTest()
    : benchmark_iterations_(1),
      benchmark_width_(128),
      benchmark_height_(72),
      disable_cpu_flags_(1),
      benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_repeat)) {
    benchmark_iterations_ = LIBYUV_GET_FLAG(FLAGS_libyuv_repeat);
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_width)) {
    benchmark_width_ = LIBYUV_GET_FLAG(FLAGS_libyuv_width);
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_height)) {
    benchmark_height_ = LIBYUV_GET_FLAG(FLAGS_libyuv_height);
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_flags)) {
    disable_cpu_flags_ = LIBYUV_GET_FLAG(FLAGS_libyuv_flags);
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info)) {
    benchmark_cpu_info_ = LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info);
  }
  disable_cpu_flags_ = TestCpuEnv(disable_cpu_flags_);
  benchmark_cpu_info_ = TestCpuEnv(benchmark_cpu_info_);
  libyuv::MaskCpuFlags(benchmark_cpu_info_);
  benchmark_pixels_div1280_ =
      static_cast<int>((static_cast<double>(Abs(benchmark_width_)) *
                            static_cast<double>(Abs(benchmark_height_)) *
                            static_cast<double>(benchmark_iterations_) +
                        1279.0) /
                       1280.0);
}

LibYUVCompareTest::LibYUVCompareTest()
    : benchmark_iterations_(1),
      benchmark_width_(128),
      benchmark_height_(72),
      disable_cpu_flags_(1),
      benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_repeat)) {
    benchmark_iterations_ = LIBYUV_GET_FLAG(FLAGS_libyuv_repeat);
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_width)) {
    benchmark_width_ = LIBYUV_GET_FLAG(FLAGS_libyuv_width);
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_height)) {
    benchmark_height_ = LIBYUV_GET_FLAG(FLAGS_libyuv_height);
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_flags)) {
    disable_cpu_flags_ = LIBYUV_GET_FLAG(FLAGS_libyuv_flags);
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info)) {
    benchmark_cpu_info_ = LIBYUV_GET_FLAG(FLAGS_libyuv_cpu_info);
  }
  disable_cpu_flags_ = TestCpuEnv(disable_cpu_flags_);
  benchmark_cpu_info_ = TestCpuEnv(benchmark_cpu_info_);
  libyuv::MaskCpuFlags(benchmark_cpu_info_);
  benchmark_pixels_div1280_ =
      static_cast<int>((static_cast<double>(Abs(benchmark_width_)) *
                            static_cast<double>(Abs(benchmark_height_)) *
                            static_cast<double>(benchmark_iterations_) +
                        1279.0) /
                       1280.0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
#ifdef LIBYUV_USE_ABSL_FLAGS
  absl::ParseCommandLine(argc, argv);
#endif
  return RUN_ALL_TESTS();
}
