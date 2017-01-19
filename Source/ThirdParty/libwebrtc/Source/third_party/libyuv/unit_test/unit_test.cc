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

#include "gflags/gflags.h"

// Change this to 1000 for benchmarking.
// TODO(fbarchard): Add command line parsing to pass this as option.
#define BENCHMARK_ITERATIONS 1

unsigned int fastrand_seed = 0xfb;

DEFINE_int32(libyuv_width, 0, "width of test image.");
DEFINE_int32(libyuv_height, 0, "height of test image.");
DEFINE_int32(libyuv_repeat, 0, "number of times to repeat test.");
DEFINE_int32(libyuv_flags, 0,
             "cpu flags for reference code. 1 = C, -1 = SIMD");
DEFINE_int32(libyuv_cpu_info, 0,
             "cpu flags for benchmark code. 1 = C, -1 = SIMD");

// For quicker unittests, default is 128 x 72.  But when benchmarking,
// default to 720p.  Allow size to specify.
// Set flags to -1 for benchmarking to avoid slower C code.

LibYUVConvertTest::LibYUVConvertTest() :
    benchmark_iterations_(BENCHMARK_ITERATIONS), benchmark_width_(128),
    benchmark_height_(72), disable_cpu_flags_(1), benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (FLAGS_libyuv_repeat) {
    benchmark_iterations_ = FLAGS_libyuv_repeat;
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (FLAGS_libyuv_width) {
    benchmark_width_ = FLAGS_libyuv_width;
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (FLAGS_libyuv_height) {
    benchmark_height_ = FLAGS_libyuv_height;
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_flags) {
    disable_cpu_flags_ = FLAGS_libyuv_flags;
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_cpu_info) {
    benchmark_cpu_info_ = FLAGS_libyuv_cpu_info;
  }
  benchmark_pixels_div256_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 255.0) / 256.0);
  benchmark_pixels_div1280_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 1279.0) / 1280.0);
}

LibYUVColorTest::LibYUVColorTest() :
    benchmark_iterations_(BENCHMARK_ITERATIONS), benchmark_width_(128),
    benchmark_height_(72), disable_cpu_flags_(1), benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (FLAGS_libyuv_repeat) {
    benchmark_iterations_ = FLAGS_libyuv_repeat;
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (FLAGS_libyuv_width) {
    benchmark_width_ = FLAGS_libyuv_width;
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (FLAGS_libyuv_height) {
    benchmark_height_ = FLAGS_libyuv_height;
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_flags) {
    disable_cpu_flags_ = FLAGS_libyuv_flags;
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_cpu_info) {
    benchmark_cpu_info_ = FLAGS_libyuv_cpu_info;
  }
  benchmark_pixels_div256_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 255.0) / 256.0);
  benchmark_pixels_div1280_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 1279.0) / 1280.0);
}

LibYUVScaleTest::LibYUVScaleTest() :
    benchmark_iterations_(BENCHMARK_ITERATIONS), benchmark_width_(128),
    benchmark_height_(72), disable_cpu_flags_(1), benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (FLAGS_libyuv_repeat) {
    benchmark_iterations_ = FLAGS_libyuv_repeat;
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (FLAGS_libyuv_width) {
    benchmark_width_ = FLAGS_libyuv_width;
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (FLAGS_libyuv_height) {
    benchmark_height_ = FLAGS_libyuv_height;
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_flags) {
    disable_cpu_flags_ = FLAGS_libyuv_flags;
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_cpu_info) {
    benchmark_cpu_info_ = FLAGS_libyuv_cpu_info;
  }
  benchmark_pixels_div256_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 255.0) / 256.0);
  benchmark_pixels_div1280_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 1279.0) / 1280.0);
}

LibYUVRotateTest::LibYUVRotateTest() :
    benchmark_iterations_(BENCHMARK_ITERATIONS), benchmark_width_(128),
    benchmark_height_(72), disable_cpu_flags_(1), benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (FLAGS_libyuv_repeat) {
    benchmark_iterations_ = FLAGS_libyuv_repeat;
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (FLAGS_libyuv_width) {
    benchmark_width_ = FLAGS_libyuv_width;
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (FLAGS_libyuv_height) {
    benchmark_height_ = FLAGS_libyuv_height;
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_flags) {
    disable_cpu_flags_ = FLAGS_libyuv_flags;
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_cpu_info) {
    benchmark_cpu_info_ = FLAGS_libyuv_cpu_info;
  }
  benchmark_pixels_div256_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 255.0) / 256.0);
  benchmark_pixels_div1280_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 1279.0) / 1280.0);
}

LibYUVPlanarTest::LibYUVPlanarTest() :
    benchmark_iterations_(BENCHMARK_ITERATIONS), benchmark_width_(128),
    benchmark_height_(72), disable_cpu_flags_(1), benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (FLAGS_libyuv_repeat) {
    benchmark_iterations_ = FLAGS_libyuv_repeat;
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (FLAGS_libyuv_width) {
    benchmark_width_ = FLAGS_libyuv_width;
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (FLAGS_libyuv_height) {
    benchmark_height_ = FLAGS_libyuv_height;
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_flags) {
    disable_cpu_flags_ = FLAGS_libyuv_flags;
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_cpu_info) {
    benchmark_cpu_info_ = FLAGS_libyuv_cpu_info;
  }
  benchmark_pixels_div256_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 255.0) / 256.0);
  benchmark_pixels_div1280_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 1279.0) / 1280.0);
}

LibYUVBaseTest::LibYUVBaseTest() :
    benchmark_iterations_(BENCHMARK_ITERATIONS), benchmark_width_(128),
    benchmark_height_(72), disable_cpu_flags_(1), benchmark_cpu_info_(-1) {
  const char* repeat = getenv("LIBYUV_REPEAT");
  if (repeat) {
    benchmark_iterations_ = atoi(repeat);  // NOLINT
  }
  if (FLAGS_libyuv_repeat) {
    benchmark_iterations_ = FLAGS_libyuv_repeat;
  }
  if (benchmark_iterations_ > 1) {
    benchmark_width_ = 1280;
    benchmark_height_ = 720;
  }
  const char* width = getenv("LIBYUV_WIDTH");
  if (width) {
    benchmark_width_ = atoi(width);  // NOLINT
  }
  if (FLAGS_libyuv_width) {
    benchmark_width_ = FLAGS_libyuv_width;
  }
  const char* height = getenv("LIBYUV_HEIGHT");
  if (height) {
    benchmark_height_ = atoi(height);  // NOLINT
  }
  if (FLAGS_libyuv_height) {
    benchmark_height_ = FLAGS_libyuv_height;
  }
  const char* cpu_flags = getenv("LIBYUV_FLAGS");
  if (cpu_flags) {
    disable_cpu_flags_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_flags) {
    disable_cpu_flags_ = FLAGS_libyuv_flags;
  }
  const char* cpu_info = getenv("LIBYUV_CPU_INFO");
  if (cpu_info) {
    benchmark_cpu_info_ = atoi(cpu_flags);  // NOLINT
  }
  if (FLAGS_libyuv_cpu_info) {
    benchmark_cpu_info_ = FLAGS_libyuv_cpu_info;
  }
  benchmark_pixels_div256_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 255.0) / 256.0);
  benchmark_pixels_div1280_ = static_cast<int>((
      static_cast<double>(Abs(benchmark_width_)) *
      static_cast<double>(Abs(benchmark_height_)) *
      static_cast<double>(benchmark_iterations_)  + 1279.0) / 1280.0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  // AllowCommandLineParsing allows us to ignore flags passed on to us by
  // Chromium build bots without having to explicitly disable them.
  google::AllowCommandLineReparsing();
  google::ParseCommandLineFlags(&argc, &argv, true);
  return RUN_ALL_TESTS();
}
