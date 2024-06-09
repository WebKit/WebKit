/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <time.h>

#include "../unit_test/unit_test.h"
#include "libyuv/cpu_id.h"
#include "libyuv/scale.h"

#ifdef ENABLE_ROW_TESTS
#include "libyuv/scale_row.h"  // For ScaleRowDown2Box_Odd_C
#endif

#define STRINGIZE(line) #line
#define FILELINESTR(file, line) file ":" STRINGIZE(line)

#if defined(__riscv) && !defined(__clang__)
#define DISABLE_SLOW_TESTS
#undef ENABLE_FULL_TESTS
#endif

#if !defined(DISABLE_SLOW_TESTS) || defined(__x86_64__) || defined(__i386__)
// SLOW TESTS are those that are unoptimized C code.
// FULL TESTS are optimized but test many variations of the same code.
#define ENABLE_FULL_TESTS
#endif

namespace libyuv {

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int I420TestFilter(int src_width,
                          int src_height,
                          int dst_width,
                          int dst_height,
                          FilterMode f,
                          int benchmark_iterations,
                          int disable_cpu_flags,
                          int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i, j;
  int src_width_uv = (Abs(src_width) + 1) >> 1;
  int src_height_uv = (Abs(src_height) + 1) >> 1;

  int64_t src_y_plane_size = (Abs(src_width)) * (Abs(src_height));
  int64_t src_uv_plane_size = (src_width_uv) * (src_height_uv);

  int src_stride_y = Abs(src_width);
  int src_stride_uv = src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_u, src_uv_plane_size);
  align_buffer_page_end(src_v, src_uv_plane_size);
  if (!src_y || !src_u || !src_v) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  int dst_width_uv = (dst_width + 1) >> 1;
  int dst_height_uv = (dst_height + 1) >> 1;

  int64_t dst_y_plane_size = (dst_width) * (dst_height);
  int64_t dst_uv_plane_size = (dst_width_uv) * (dst_height_uv);

  int dst_stride_y = dst_width;
  int dst_stride_uv = dst_width_uv;

  align_buffer_page_end(dst_y_c, dst_y_plane_size);
  align_buffer_page_end(dst_u_c, dst_uv_plane_size);
  align_buffer_page_end(dst_v_c, dst_uv_plane_size);
  align_buffer_page_end(dst_y_opt, dst_y_plane_size);
  align_buffer_page_end(dst_u_opt, dst_uv_plane_size);
  align_buffer_page_end(dst_v_opt, dst_uv_plane_size);
  if (!dst_y_c || !dst_u_c || !dst_v_c || !dst_y_opt || !dst_u_opt ||
      !dst_v_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  I420Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
            src_width, src_height, dst_y_c, dst_stride_y, dst_u_c,
            dst_stride_uv, dst_v_c, dst_stride_uv, dst_width, dst_height, f);
  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    I420Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
              src_width, src_height, dst_y_opt, dst_stride_y, dst_u_opt,
              dst_stride_uv, dst_v_opt, dst_stride_uv, dst_width, dst_height,
              f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;
  // Report performance of C vs OPT.
  printf("filter %d - %8d us C - %8d us OPT\n", f,
         static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  // C version may be a little off from the optimized. Order of
  //  operations may introduce rounding somewhere. So do a difference
  //  of the buffers and look to see that the max difference is not
  //  over 3.
  int max_diff = 0;
  for (i = 0; i < (dst_height); ++i) {
    for (j = 0; j < (dst_width); ++j) {
      int abs_diff = Abs(dst_y_c[(i * dst_stride_y) + j] -
                         dst_y_opt[(i * dst_stride_y) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  for (i = 0; i < (dst_height_uv); ++i) {
    for (j = 0; j < (dst_width_uv); ++j) {
      int abs_diff = Abs(dst_u_c[(i * dst_stride_uv) + j] -
                         dst_u_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
      abs_diff = Abs(dst_v_c[(i * dst_stride_uv) + j] -
                     dst_v_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_y_c);
  free_aligned_buffer_page_end(dst_u_c);
  free_aligned_buffer_page_end(dst_v_c);
  free_aligned_buffer_page_end(dst_y_opt);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_opt);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_v);

  return max_diff;
}

// Test scaling with 8 bit C vs 12 bit C and return maximum pixel difference.
// 0 = exact.
static int I420TestFilter_12(int src_width,
                             int src_height,
                             int dst_width,
                             int dst_height,
                             FilterMode f,
                             int benchmark_iterations,
                             int disable_cpu_flags,
                             int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i;
  int src_width_uv = (Abs(src_width) + 1) >> 1;
  int src_height_uv = (Abs(src_height) + 1) >> 1;

  int64_t src_y_plane_size = (Abs(src_width)) * (Abs(src_height));
  int64_t src_uv_plane_size = (src_width_uv) * (src_height_uv);

  int src_stride_y = Abs(src_width);
  int src_stride_uv = src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_u, src_uv_plane_size);
  align_buffer_page_end(src_v, src_uv_plane_size);
  align_buffer_page_end(src_y_12, src_y_plane_size * 2);
  align_buffer_page_end(src_u_12, src_uv_plane_size * 2);
  align_buffer_page_end(src_v_12, src_uv_plane_size * 2);
  if (!src_y || !src_u || !src_v || !src_y_12 || !src_u_12 || !src_v_12) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  uint16_t* p_src_y_12 = reinterpret_cast<uint16_t*>(src_y_12);
  uint16_t* p_src_u_12 = reinterpret_cast<uint16_t*>(src_u_12);
  uint16_t* p_src_v_12 = reinterpret_cast<uint16_t*>(src_v_12);

  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  for (i = 0; i < src_y_plane_size; ++i) {
    p_src_y_12[i] = src_y[i];
  }
  for (i = 0; i < src_uv_plane_size; ++i) {
    p_src_u_12[i] = src_u[i];
    p_src_v_12[i] = src_v[i];
  }

  int dst_width_uv = (dst_width + 1) >> 1;
  int dst_height_uv = (dst_height + 1) >> 1;

  int dst_y_plane_size = (dst_width) * (dst_height);
  int dst_uv_plane_size = (dst_width_uv) * (dst_height_uv);

  int dst_stride_y = dst_width;
  int dst_stride_uv = dst_width_uv;

  align_buffer_page_end(dst_y_8, dst_y_plane_size);
  align_buffer_page_end(dst_u_8, dst_uv_plane_size);
  align_buffer_page_end(dst_v_8, dst_uv_plane_size);
  align_buffer_page_end(dst_y_12, dst_y_plane_size * 2);
  align_buffer_page_end(dst_u_12, dst_uv_plane_size * 2);
  align_buffer_page_end(dst_v_12, dst_uv_plane_size * 2);

  uint16_t* p_dst_y_12 = reinterpret_cast<uint16_t*>(dst_y_12);
  uint16_t* p_dst_u_12 = reinterpret_cast<uint16_t*>(dst_u_12);
  uint16_t* p_dst_v_12 = reinterpret_cast<uint16_t*>(dst_v_12);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  I420Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
            src_width, src_height, dst_y_8, dst_stride_y, dst_u_8,
            dst_stride_uv, dst_v_8, dst_stride_uv, dst_width, dst_height, f);
  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (i = 0; i < benchmark_iterations; ++i) {
    I420Scale_12(p_src_y_12, src_stride_y, p_src_u_12, src_stride_uv,
                 p_src_v_12, src_stride_uv, src_width, src_height, p_dst_y_12,
                 dst_stride_y, p_dst_u_12, dst_stride_uv, p_dst_v_12,
                 dst_stride_uv, dst_width, dst_height, f);
  }

  // Expect an exact match.
  int max_diff = 0;
  for (i = 0; i < dst_y_plane_size; ++i) {
    int abs_diff = Abs(dst_y_8[i] - p_dst_y_12[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  for (i = 0; i < dst_uv_plane_size; ++i) {
    int abs_diff = Abs(dst_u_8[i] - p_dst_u_12[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
    abs_diff = Abs(dst_v_8[i] - p_dst_v_12[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_y_8);
  free_aligned_buffer_page_end(dst_u_8);
  free_aligned_buffer_page_end(dst_v_8);
  free_aligned_buffer_page_end(dst_y_12);
  free_aligned_buffer_page_end(dst_u_12);
  free_aligned_buffer_page_end(dst_v_12);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_v);
  free_aligned_buffer_page_end(src_y_12);
  free_aligned_buffer_page_end(src_u_12);
  free_aligned_buffer_page_end(src_v_12);

  return max_diff;
}

// Test scaling with 8 bit C vs 16 bit C and return maximum pixel difference.
// 0 = exact.
static int I420TestFilter_16(int src_width,
                             int src_height,
                             int dst_width,
                             int dst_height,
                             FilterMode f,
                             int benchmark_iterations,
                             int disable_cpu_flags,
                             int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i;
  int src_width_uv = (Abs(src_width) + 1) >> 1;
  int src_height_uv = (Abs(src_height) + 1) >> 1;

  int64_t src_y_plane_size = (Abs(src_width)) * (Abs(src_height));
  int64_t src_uv_plane_size = (src_width_uv) * (src_height_uv);

  int src_stride_y = Abs(src_width);
  int src_stride_uv = src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_u, src_uv_plane_size);
  align_buffer_page_end(src_v, src_uv_plane_size);
  align_buffer_page_end(src_y_16, src_y_plane_size * 2);
  align_buffer_page_end(src_u_16, src_uv_plane_size * 2);
  align_buffer_page_end(src_v_16, src_uv_plane_size * 2);
  if (!src_y || !src_u || !src_v || !src_y_16 || !src_u_16 || !src_v_16) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  uint16_t* p_src_y_16 = reinterpret_cast<uint16_t*>(src_y_16);
  uint16_t* p_src_u_16 = reinterpret_cast<uint16_t*>(src_u_16);
  uint16_t* p_src_v_16 = reinterpret_cast<uint16_t*>(src_v_16);

  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  for (i = 0; i < src_y_plane_size; ++i) {
    p_src_y_16[i] = src_y[i];
  }
  for (i = 0; i < src_uv_plane_size; ++i) {
    p_src_u_16[i] = src_u[i];
    p_src_v_16[i] = src_v[i];
  }

  int dst_width_uv = (dst_width + 1) >> 1;
  int dst_height_uv = (dst_height + 1) >> 1;

  int dst_y_plane_size = (dst_width) * (dst_height);
  int dst_uv_plane_size = (dst_width_uv) * (dst_height_uv);

  int dst_stride_y = dst_width;
  int dst_stride_uv = dst_width_uv;

  align_buffer_page_end(dst_y_8, dst_y_plane_size);
  align_buffer_page_end(dst_u_8, dst_uv_plane_size);
  align_buffer_page_end(dst_v_8, dst_uv_plane_size);
  align_buffer_page_end(dst_y_16, dst_y_plane_size * 2);
  align_buffer_page_end(dst_u_16, dst_uv_plane_size * 2);
  align_buffer_page_end(dst_v_16, dst_uv_plane_size * 2);

  uint16_t* p_dst_y_16 = reinterpret_cast<uint16_t*>(dst_y_16);
  uint16_t* p_dst_u_16 = reinterpret_cast<uint16_t*>(dst_u_16);
  uint16_t* p_dst_v_16 = reinterpret_cast<uint16_t*>(dst_v_16);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  I420Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
            src_width, src_height, dst_y_8, dst_stride_y, dst_u_8,
            dst_stride_uv, dst_v_8, dst_stride_uv, dst_width, dst_height, f);
  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (i = 0; i < benchmark_iterations; ++i) {
    I420Scale_16(p_src_y_16, src_stride_y, p_src_u_16, src_stride_uv,
                 p_src_v_16, src_stride_uv, src_width, src_height, p_dst_y_16,
                 dst_stride_y, p_dst_u_16, dst_stride_uv, p_dst_v_16,
                 dst_stride_uv, dst_width, dst_height, f);
  }

  // Expect an exact match.
  int max_diff = 0;
  for (i = 0; i < dst_y_plane_size; ++i) {
    int abs_diff = Abs(dst_y_8[i] - p_dst_y_16[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  for (i = 0; i < dst_uv_plane_size; ++i) {
    int abs_diff = Abs(dst_u_8[i] - p_dst_u_16[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
    abs_diff = Abs(dst_v_8[i] - p_dst_v_16[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_y_8);
  free_aligned_buffer_page_end(dst_u_8);
  free_aligned_buffer_page_end(dst_v_8);
  free_aligned_buffer_page_end(dst_y_16);
  free_aligned_buffer_page_end(dst_u_16);
  free_aligned_buffer_page_end(dst_v_16);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_v);
  free_aligned_buffer_page_end(src_y_16);
  free_aligned_buffer_page_end(src_u_16);
  free_aligned_buffer_page_end(src_v_16);

  return max_diff;
}

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int I444TestFilter(int src_width,
                          int src_height,
                          int dst_width,
                          int dst_height,
                          FilterMode f,
                          int benchmark_iterations,
                          int disable_cpu_flags,
                          int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i, j;
  int src_width_uv = Abs(src_width);
  int src_height_uv = Abs(src_height);

  int64_t src_y_plane_size = (Abs(src_width)) * (Abs(src_height));
  int64_t src_uv_plane_size = (src_width_uv) * (src_height_uv);

  int src_stride_y = Abs(src_width);
  int src_stride_uv = src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_u, src_uv_plane_size);
  align_buffer_page_end(src_v, src_uv_plane_size);
  if (!src_y || !src_u || !src_v) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  int dst_width_uv = dst_width;
  int dst_height_uv = dst_height;

  int64_t dst_y_plane_size = (dst_width) * (dst_height);
  int64_t dst_uv_plane_size = (dst_width_uv) * (dst_height_uv);

  int dst_stride_y = dst_width;
  int dst_stride_uv = dst_width_uv;

  align_buffer_page_end(dst_y_c, dst_y_plane_size);
  align_buffer_page_end(dst_u_c, dst_uv_plane_size);
  align_buffer_page_end(dst_v_c, dst_uv_plane_size);
  align_buffer_page_end(dst_y_opt, dst_y_plane_size);
  align_buffer_page_end(dst_u_opt, dst_uv_plane_size);
  align_buffer_page_end(dst_v_opt, dst_uv_plane_size);
  if (!dst_y_c || !dst_u_c || !dst_v_c || !dst_y_opt || !dst_u_opt ||
      !dst_v_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  I444Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
            src_width, src_height, dst_y_c, dst_stride_y, dst_u_c,
            dst_stride_uv, dst_v_c, dst_stride_uv, dst_width, dst_height, f);
  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    I444Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
              src_width, src_height, dst_y_opt, dst_stride_y, dst_u_opt,
              dst_stride_uv, dst_v_opt, dst_stride_uv, dst_width, dst_height,
              f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;
  // Report performance of C vs OPT.
  printf("filter %d - %8d us C - %8d us OPT\n", f,
         static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  // C version may be a little off from the optimized. Order of
  //  operations may introduce rounding somewhere. So do a difference
  //  of the buffers and look to see that the max difference is not
  //  over 3.
  int max_diff = 0;
  for (i = 0; i < (dst_height); ++i) {
    for (j = 0; j < (dst_width); ++j) {
      int abs_diff = Abs(dst_y_c[(i * dst_stride_y) + j] -
                         dst_y_opt[(i * dst_stride_y) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  for (i = 0; i < (dst_height_uv); ++i) {
    for (j = 0; j < (dst_width_uv); ++j) {
      int abs_diff = Abs(dst_u_c[(i * dst_stride_uv) + j] -
                         dst_u_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
      abs_diff = Abs(dst_v_c[(i * dst_stride_uv) + j] -
                     dst_v_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_y_c);
  free_aligned_buffer_page_end(dst_u_c);
  free_aligned_buffer_page_end(dst_v_c);
  free_aligned_buffer_page_end(dst_y_opt);
  free_aligned_buffer_page_end(dst_u_opt);
  free_aligned_buffer_page_end(dst_v_opt);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_v);

  return max_diff;
}

// Test scaling with 8 bit C vs 12 bit C and return maximum pixel difference.
// 0 = exact.
static int I444TestFilter_12(int src_width,
                             int src_height,
                             int dst_width,
                             int dst_height,
                             FilterMode f,
                             int benchmark_iterations,
                             int disable_cpu_flags,
                             int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i;
  int src_width_uv = Abs(src_width);
  int src_height_uv = Abs(src_height);

  int64_t src_y_plane_size = (Abs(src_width)) * (Abs(src_height));
  int64_t src_uv_plane_size = (src_width_uv) * (src_height_uv);

  int src_stride_y = Abs(src_width);
  int src_stride_uv = src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_u, src_uv_plane_size);
  align_buffer_page_end(src_v, src_uv_plane_size);
  align_buffer_page_end(src_y_12, src_y_plane_size * 2);
  align_buffer_page_end(src_u_12, src_uv_plane_size * 2);
  align_buffer_page_end(src_v_12, src_uv_plane_size * 2);
  if (!src_y || !src_u || !src_v || !src_y_12 || !src_u_12 || !src_v_12) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  uint16_t* p_src_y_12 = reinterpret_cast<uint16_t*>(src_y_12);
  uint16_t* p_src_u_12 = reinterpret_cast<uint16_t*>(src_u_12);
  uint16_t* p_src_v_12 = reinterpret_cast<uint16_t*>(src_v_12);

  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  for (i = 0; i < src_y_plane_size; ++i) {
    p_src_y_12[i] = src_y[i];
  }
  for (i = 0; i < src_uv_plane_size; ++i) {
    p_src_u_12[i] = src_u[i];
    p_src_v_12[i] = src_v[i];
  }

  int dst_width_uv = dst_width;
  int dst_height_uv = dst_height;

  int dst_y_plane_size = (dst_width) * (dst_height);
  int dst_uv_plane_size = (dst_width_uv) * (dst_height_uv);

  int dst_stride_y = dst_width;
  int dst_stride_uv = dst_width_uv;

  align_buffer_page_end(dst_y_8, dst_y_plane_size);
  align_buffer_page_end(dst_u_8, dst_uv_plane_size);
  align_buffer_page_end(dst_v_8, dst_uv_plane_size);
  align_buffer_page_end(dst_y_12, dst_y_plane_size * 2);
  align_buffer_page_end(dst_u_12, dst_uv_plane_size * 2);
  align_buffer_page_end(dst_v_12, dst_uv_plane_size * 2);

  uint16_t* p_dst_y_12 = reinterpret_cast<uint16_t*>(dst_y_12);
  uint16_t* p_dst_u_12 = reinterpret_cast<uint16_t*>(dst_u_12);
  uint16_t* p_dst_v_12 = reinterpret_cast<uint16_t*>(dst_v_12);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  I444Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
            src_width, src_height, dst_y_8, dst_stride_y, dst_u_8,
            dst_stride_uv, dst_v_8, dst_stride_uv, dst_width, dst_height, f);
  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (i = 0; i < benchmark_iterations; ++i) {
    I444Scale_12(p_src_y_12, src_stride_y, p_src_u_12, src_stride_uv,
                 p_src_v_12, src_stride_uv, src_width, src_height, p_dst_y_12,
                 dst_stride_y, p_dst_u_12, dst_stride_uv, p_dst_v_12,
                 dst_stride_uv, dst_width, dst_height, f);
  }

  // Expect an exact match.
  int max_diff = 0;
  for (i = 0; i < dst_y_plane_size; ++i) {
    int abs_diff = Abs(dst_y_8[i] - p_dst_y_12[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  for (i = 0; i < dst_uv_plane_size; ++i) {
    int abs_diff = Abs(dst_u_8[i] - p_dst_u_12[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
    abs_diff = Abs(dst_v_8[i] - p_dst_v_12[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_y_8);
  free_aligned_buffer_page_end(dst_u_8);
  free_aligned_buffer_page_end(dst_v_8);
  free_aligned_buffer_page_end(dst_y_12);
  free_aligned_buffer_page_end(dst_u_12);
  free_aligned_buffer_page_end(dst_v_12);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_v);
  free_aligned_buffer_page_end(src_y_12);
  free_aligned_buffer_page_end(src_u_12);
  free_aligned_buffer_page_end(src_v_12);

  return max_diff;
}

// Test scaling with 8 bit C vs 16 bit C and return maximum pixel difference.
// 0 = exact.
static int I444TestFilter_16(int src_width,
                             int src_height,
                             int dst_width,
                             int dst_height,
                             FilterMode f,
                             int benchmark_iterations,
                             int disable_cpu_flags,
                             int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i;
  int src_width_uv = Abs(src_width);
  int src_height_uv = Abs(src_height);

  int64_t src_y_plane_size = (Abs(src_width)) * (Abs(src_height));
  int64_t src_uv_plane_size = (src_width_uv) * (src_height_uv);

  int src_stride_y = Abs(src_width);
  int src_stride_uv = src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_u, src_uv_plane_size);
  align_buffer_page_end(src_v, src_uv_plane_size);
  align_buffer_page_end(src_y_16, src_y_plane_size * 2);
  align_buffer_page_end(src_u_16, src_uv_plane_size * 2);
  align_buffer_page_end(src_v_16, src_uv_plane_size * 2);
  if (!src_y || !src_u || !src_v || !src_y_16 || !src_u_16 || !src_v_16) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  uint16_t* p_src_y_16 = reinterpret_cast<uint16_t*>(src_y_16);
  uint16_t* p_src_u_16 = reinterpret_cast<uint16_t*>(src_u_16);
  uint16_t* p_src_v_16 = reinterpret_cast<uint16_t*>(src_v_16);

  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  for (i = 0; i < src_y_plane_size; ++i) {
    p_src_y_16[i] = src_y[i];
  }
  for (i = 0; i < src_uv_plane_size; ++i) {
    p_src_u_16[i] = src_u[i];
    p_src_v_16[i] = src_v[i];
  }

  int dst_width_uv = dst_width;
  int dst_height_uv = dst_height;

  int dst_y_plane_size = (dst_width) * (dst_height);
  int dst_uv_plane_size = (dst_width_uv) * (dst_height_uv);

  int dst_stride_y = dst_width;
  int dst_stride_uv = dst_width_uv;

  align_buffer_page_end(dst_y_8, dst_y_plane_size);
  align_buffer_page_end(dst_u_8, dst_uv_plane_size);
  align_buffer_page_end(dst_v_8, dst_uv_plane_size);
  align_buffer_page_end(dst_y_16, dst_y_plane_size * 2);
  align_buffer_page_end(dst_u_16, dst_uv_plane_size * 2);
  align_buffer_page_end(dst_v_16, dst_uv_plane_size * 2);

  uint16_t* p_dst_y_16 = reinterpret_cast<uint16_t*>(dst_y_16);
  uint16_t* p_dst_u_16 = reinterpret_cast<uint16_t*>(dst_u_16);
  uint16_t* p_dst_v_16 = reinterpret_cast<uint16_t*>(dst_v_16);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  I444Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
            src_width, src_height, dst_y_8, dst_stride_y, dst_u_8,
            dst_stride_uv, dst_v_8, dst_stride_uv, dst_width, dst_height, f);
  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (i = 0; i < benchmark_iterations; ++i) {
    I444Scale_16(p_src_y_16, src_stride_y, p_src_u_16, src_stride_uv,
                 p_src_v_16, src_stride_uv, src_width, src_height, p_dst_y_16,
                 dst_stride_y, p_dst_u_16, dst_stride_uv, p_dst_v_16,
                 dst_stride_uv, dst_width, dst_height, f);
  }

  // Expect an exact match.
  int max_diff = 0;
  for (i = 0; i < dst_y_plane_size; ++i) {
    int abs_diff = Abs(dst_y_8[i] - p_dst_y_16[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  for (i = 0; i < dst_uv_plane_size; ++i) {
    int abs_diff = Abs(dst_u_8[i] - p_dst_u_16[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
    abs_diff = Abs(dst_v_8[i] - p_dst_v_16[i]);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }

  free_aligned_buffer_page_end(dst_y_8);
  free_aligned_buffer_page_end(dst_u_8);
  free_aligned_buffer_page_end(dst_v_8);
  free_aligned_buffer_page_end(dst_y_16);
  free_aligned_buffer_page_end(dst_u_16);
  free_aligned_buffer_page_end(dst_v_16);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_u);
  free_aligned_buffer_page_end(src_v);
  free_aligned_buffer_page_end(src_y_16);
  free_aligned_buffer_page_end(src_u_16);
  free_aligned_buffer_page_end(src_v_16);

  return max_diff;
}

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int NV12TestFilter(int src_width,
                          int src_height,
                          int dst_width,
                          int dst_height,
                          FilterMode f,
                          int benchmark_iterations,
                          int disable_cpu_flags,
                          int benchmark_cpu_info) {
  if (!SizeValid(src_width, src_height, dst_width, dst_height)) {
    return 0;
  }

  int i, j;
  int src_width_uv = (Abs(src_width) + 1) >> 1;
  int src_height_uv = (Abs(src_height) + 1) >> 1;

  int64_t src_y_plane_size = (Abs(src_width)) * (Abs(src_height));
  int64_t src_uv_plane_size = (src_width_uv) * (src_height_uv)*2;

  int src_stride_y = Abs(src_width);
  int src_stride_uv = src_width_uv * 2;

  align_buffer_page_end(src_y, src_y_plane_size);
  align_buffer_page_end(src_uv, src_uv_plane_size);
  if (!src_y || !src_uv) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }
  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_uv, src_uv_plane_size);

  int dst_width_uv = (dst_width + 1) >> 1;
  int dst_height_uv = (dst_height + 1) >> 1;

  int64_t dst_y_plane_size = (dst_width) * (dst_height);
  int64_t dst_uv_plane_size = (dst_width_uv) * (dst_height_uv)*2;

  int dst_stride_y = dst_width;
  int dst_stride_uv = dst_width_uv * 2;

  align_buffer_page_end(dst_y_c, dst_y_plane_size);
  align_buffer_page_end(dst_uv_c, dst_uv_plane_size);
  align_buffer_page_end(dst_y_opt, dst_y_plane_size);
  align_buffer_page_end(dst_uv_opt, dst_uv_plane_size);
  if (!dst_y_c || !dst_uv_c || !dst_y_opt || !dst_uv_opt) {
    printf("Skipped.  Alloc failed " FILELINESTR(__FILE__, __LINE__) "\n");
    return 0;
  }

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  double c_time = get_time();
  NV12Scale(src_y, src_stride_y, src_uv, src_stride_uv, src_width, src_height,
            dst_y_c, dst_stride_y, dst_uv_c, dst_stride_uv, dst_width,
            dst_height, f);
  c_time = (get_time() - c_time);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    NV12Scale(src_y, src_stride_y, src_uv, src_stride_uv, src_width, src_height,
              dst_y_opt, dst_stride_y, dst_uv_opt, dst_stride_uv, dst_width,
              dst_height, f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;
  // Report performance of C vs OPT.
  printf("filter %d - %8d us C - %8d us OPT\n", f,
         static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  // C version may be a little off from the optimized. Order of
  //  operations may introduce rounding somewhere. So do a difference
  //  of the buffers and look to see that the max difference is not
  //  over 3.
  int max_diff = 0;
  for (i = 0; i < (dst_height); ++i) {
    for (j = 0; j < (dst_width); ++j) {
      int abs_diff = Abs(dst_y_c[(i * dst_stride_y) + j] -
                         dst_y_opt[(i * dst_stride_y) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  for (i = 0; i < (dst_height_uv); ++i) {
    for (j = 0; j < (dst_width_uv * 2); ++j) {
      int abs_diff = Abs(dst_uv_c[(i * dst_stride_uv) + j] -
                         dst_uv_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_y_c);
  free_aligned_buffer_page_end(dst_uv_c);
  free_aligned_buffer_page_end(dst_y_opt);
  free_aligned_buffer_page_end(dst_uv_opt);
  free_aligned_buffer_page_end(src_y);
  free_aligned_buffer_page_end(src_uv);

  return max_diff;
}

// The following adjustments in dimensions ensure the scale factor will be
// exactly achieved.
// 2 is chroma subsample.
#define DX(x, nom, denom) static_cast<int>(((Abs(x) / nom + 1) / 2) * nom * 2)
#define SX(x, nom, denom) static_cast<int>(((x / nom + 1) / 2) * denom * 2)

#define TEST_FACTOR1(DISABLED_, name, filter, nom, denom, max_diff)           \
  TEST_F(LibYUVScaleTest, I420ScaleDownBy##name##_##filter) {                 \
    int diff = I420TestFilter(                                                \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom),  \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom),  \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,           \
        benchmark_cpu_info_);                                                 \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, I444ScaleDownBy##name##_##filter) {                 \
    int diff = I444TestFilter(                                                \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom),  \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom),  \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,           \
        benchmark_cpu_info_);                                                 \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, DISABLED_##I420ScaleDownBy##name##_##filter##_12) { \
    int diff = I420TestFilter_12(                                             \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom),  \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom),  \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,           \
        benchmark_cpu_info_);                                                 \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, DISABLED_##I444ScaleDownBy##name##_##filter##_12) { \
    int diff = I444TestFilter_12(                                             \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom),  \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom),  \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,           \
        benchmark_cpu_info_);                                                 \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, NV12ScaleDownBy##name##_##filter) {                 \
    int diff = NV12TestFilter(                                                \
        SX(benchmark_width_, nom, denom), SX(benchmark_height_, nom, denom),  \
        DX(benchmark_width_, nom, denom), DX(benchmark_height_, nom, denom),  \
        kFilter##filter, benchmark_iterations_, disable_cpu_flags_,           \
        benchmark_cpu_info_);                                                 \
    EXPECT_LE(diff, max_diff);                                                \
  }

// Test a scale factor with all 4 filters.  Expect unfiltered to be exact, but
// filtering is different fixed point implementations for SSSE3, Neon and C.
#ifndef DISABLE_SLOW_TESTS
#define TEST_FACTOR(name, nom, denom, boxdiff)  \
  TEST_FACTOR1(, name, None, nom, denom, 0)     \
  TEST_FACTOR1(, name, Linear, nom, denom, 3)   \
  TEST_FACTOR1(, name, Bilinear, nom, denom, 3) \
  TEST_FACTOR1(, name, Box, nom, denom, boxdiff)
#else
#if defined(ENABLE_FULL_TESTS)
#define TEST_FACTOR(name, nom, denom, boxdiff)           \
  TEST_FACTOR1(DISABLED_, name, None, nom, denom, 0)     \
  TEST_FACTOR1(DISABLED_, name, Linear, nom, denom, 3)   \
  TEST_FACTOR1(DISABLED_, name, Bilinear, nom, denom, 3) \
  TEST_FACTOR1(DISABLED_, name, Box, nom, denom, boxdiff)
#else
#define TEST_FACTOR(name, nom, denom, boxdiff)           \
  TEST_FACTOR1(DISABLED_, name, Bilinear, nom, denom, 3) \
  TEST_FACTOR1(DISABLED_, name, Box, nom, denom, boxdiff)
#endif
#endif

TEST_FACTOR(2, 1, 2, 0)
TEST_FACTOR(4, 1, 4, 0)
#ifndef DISABLE_SLOW_TESTS
TEST_FACTOR(8, 1, 8, 0)
#endif
TEST_FACTOR(3by4, 3, 4, 1)
TEST_FACTOR(3by8, 3, 8, 1)
TEST_FACTOR(3, 1, 3, 0)
#undef TEST_FACTOR1
#undef TEST_FACTOR
#undef SX
#undef DX

#define TEST_SCALETO1(DISABLED_, name, width, height, filter, max_diff)       \
  TEST_F(LibYUVScaleTest, I420##name##To##width##x##height##_##filter) {      \
    int diff = I420TestFilter(benchmark_width_, benchmark_height_, width,     \
                              height, kFilter##filter, benchmark_iterations_, \
                              disable_cpu_flags_, benchmark_cpu_info_);       \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, I444##name##To##width##x##height##_##filter) {      \
    int diff = I444TestFilter(benchmark_width_, benchmark_height_, width,     \
                              height, kFilter##filter, benchmark_iterations_, \
                              disable_cpu_flags_, benchmark_cpu_info_);       \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##I420##name##To##width##x##height##_##filter##_12) {       \
    int diff = I420TestFilter_12(                                             \
        benchmark_width_, benchmark_height_, width, height, kFilter##filter,  \
        benchmark_iterations_, disable_cpu_flags_, benchmark_cpu_info_);      \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##I444##name##To##width##x##height##_##filter##_12) {       \
    int diff = I444TestFilter_12(                                             \
        benchmark_width_, benchmark_height_, width, height, kFilter##filter,  \
        benchmark_iterations_, disable_cpu_flags_, benchmark_cpu_info_);      \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##I420##name##To##width##x##height##_##filter##_16) {       \
    int diff = I420TestFilter_16(                                             \
        benchmark_width_, benchmark_height_, width, height, kFilter##filter,  \
        benchmark_iterations_, disable_cpu_flags_, benchmark_cpu_info_);      \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##I444##name##To##width##x##height##_##filter##_16) {       \
    int diff = I444TestFilter_16(                                             \
        benchmark_width_, benchmark_height_, width, height, kFilter##filter,  \
        benchmark_iterations_, disable_cpu_flags_, benchmark_cpu_info_);      \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, NV12##name##To##width##x##height##_##filter) {      \
    int diff = NV12TestFilter(benchmark_width_, benchmark_height_, width,     \
                              height, kFilter##filter, benchmark_iterations_, \
                              disable_cpu_flags_, benchmark_cpu_info_);       \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, I420##name##From##width##x##height##_##filter) {    \
    int diff = I420TestFilter(width, height, Abs(benchmark_width_),           \
                              Abs(benchmark_height_), kFilter##filter,        \
                              benchmark_iterations_, disable_cpu_flags_,      \
                              benchmark_cpu_info_);                           \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, I444##name##From##width##x##height##_##filter) {    \
    int diff = I444TestFilter(width, height, Abs(benchmark_width_),           \
                              Abs(benchmark_height_), kFilter##filter,        \
                              benchmark_iterations_, disable_cpu_flags_,      \
                              benchmark_cpu_info_);                           \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##I420##name##From##width##x##height##_##filter##_12) {     \
    int diff = I420TestFilter_12(width, height, Abs(benchmark_width_),        \
                                 Abs(benchmark_height_), kFilter##filter,     \
                                 benchmark_iterations_, disable_cpu_flags_,   \
                                 benchmark_cpu_info_);                        \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##I444##name##From##width##x##height##_##filter##_12) {     \
    int diff = I444TestFilter_12(width, height, Abs(benchmark_width_),        \
                                 Abs(benchmark_height_), kFilter##filter,     \
                                 benchmark_iterations_, disable_cpu_flags_,   \
                                 benchmark_cpu_info_);                        \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##I420##name##From##width##x##height##_##filter##_16) {     \
    int diff = I420TestFilter_16(width, height, Abs(benchmark_width_),        \
                                 Abs(benchmark_height_), kFilter##filter,     \
                                 benchmark_iterations_, disable_cpu_flags_,   \
                                 benchmark_cpu_info_);                        \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest,                                                     \
         DISABLED_##I444##name##From##width##x##height##_##filter##_16) {     \
    int diff = I444TestFilter_16(width, height, Abs(benchmark_width_),        \
                                 Abs(benchmark_height_), kFilter##filter,     \
                                 benchmark_iterations_, disable_cpu_flags_,   \
                                 benchmark_cpu_info_);                        \
    EXPECT_LE(diff, max_diff);                                                \
  }                                                                           \
  TEST_F(LibYUVScaleTest, NV12##name##From##width##x##height##_##filter) {    \
    int diff = NV12TestFilter(width, height, Abs(benchmark_width_),           \
                              Abs(benchmark_height_), kFilter##filter,        \
                              benchmark_iterations_, disable_cpu_flags_,      \
                              benchmark_cpu_info_);                           \
    EXPECT_LE(diff, max_diff);                                                \
  }

#ifndef DISABLE_SLOW_TESTS
// Test scale to a specified size with all 4 filters.
#define TEST_SCALETO(name, width, height)           \
  TEST_SCALETO1(, name, width, height, None, 0)     \
  TEST_SCALETO1(, name, width, height, Linear, 3)   \
  TEST_SCALETO1(, name, width, height, Bilinear, 3) \
  TEST_SCALETO1(, name, width, height, Box, 3)
#else
#if defined(ENABLE_FULL_TESTS)
#define TEST_SCALETO(name, width, height)                    \
  TEST_SCALETO1(DISABLED_, name, width, height, None, 0)     \
  TEST_SCALETO1(DISABLED_, name, width, height, Linear, 3)   \
  TEST_SCALETO1(DISABLED_, name, width, height, Bilinear, 3) \
  TEST_SCALETO1(DISABLED_, name, width, height, Box, 3)
#else
#define TEST_SCALETO(name, width, height)                    \
  TEST_SCALETO1(DISABLED_, name, width, height, Bilinear, 3) \
  TEST_SCALETO1(DISABLED_, name, width, height, Box, 3)
#endif
#endif

TEST_SCALETO(Scale, 1, 1)
TEST_SCALETO(Scale, 569, 480)
TEST_SCALETO(Scale, 640, 360)
#ifndef DISABLE_SLOW_TESTS
TEST_SCALETO(Scale, 256, 144) /* 128x72 * 2 */
TEST_SCALETO(Scale, 320, 240)
TEST_SCALETO(Scale, 1280, 720)
TEST_SCALETO(Scale, 1920, 1080)
#endif  // DISABLE_SLOW_TESTS
#undef TEST_SCALETO1
#undef TEST_SCALETO

#define TEST_SCALESWAPXY1(DISABLED_, name, filter, max_diff)               \
  TEST_F(LibYUVScaleTest, I420##name##SwapXY_##filter) {                   \
    int diff = I420TestFilter(benchmark_width_, benchmark_height_,         \
                              benchmark_height_, benchmark_width_,         \
                              kFilter##filter, benchmark_iterations_,      \
                              disable_cpu_flags_, benchmark_cpu_info_);    \
    EXPECT_LE(diff, max_diff);                                             \
  }                                                                        \
  TEST_F(LibYUVScaleTest, I444##name##SwapXY_##filter) {                   \
    int diff = I444TestFilter(benchmark_width_, benchmark_height_,         \
                              benchmark_height_, benchmark_width_,         \
                              kFilter##filter, benchmark_iterations_,      \
                              disable_cpu_flags_, benchmark_cpu_info_);    \
    EXPECT_LE(diff, max_diff);                                             \
  }                                                                        \
  TEST_F(LibYUVScaleTest, DISABLED_##I420##name##SwapXY_##filter##_12) {   \
    int diff = I420TestFilter_12(benchmark_width_, benchmark_height_,      \
                                 benchmark_height_, benchmark_width_,      \
                                 kFilter##filter, benchmark_iterations_,   \
                                 disable_cpu_flags_, benchmark_cpu_info_); \
    EXPECT_LE(diff, max_diff);                                             \
  }                                                                        \
  TEST_F(LibYUVScaleTest, DISABLED_##I444##name##SwapXY_##filter##_12) {   \
    int diff = I444TestFilter_12(benchmark_width_, benchmark_height_,      \
                                 benchmark_height_, benchmark_width_,      \
                                 kFilter##filter, benchmark_iterations_,   \
                                 disable_cpu_flags_, benchmark_cpu_info_); \
    EXPECT_LE(diff, max_diff);                                             \
  }                                                                        \
  TEST_F(LibYUVScaleTest, DISABLED_##I420##name##SwapXY_##filter##_16) {   \
    int diff = I420TestFilter_16(benchmark_width_, benchmark_height_,      \
                                 benchmark_height_, benchmark_width_,      \
                                 kFilter##filter, benchmark_iterations_,   \
                                 disable_cpu_flags_, benchmark_cpu_info_); \
    EXPECT_LE(diff, max_diff);                                             \
  }                                                                        \
  TEST_F(LibYUVScaleTest, DISABLED_##I444##name##SwapXY_##filter##_16) {   \
    int diff = I444TestFilter_16(benchmark_width_, benchmark_height_,      \
                                 benchmark_height_, benchmark_width_,      \
                                 kFilter##filter, benchmark_iterations_,   \
                                 disable_cpu_flags_, benchmark_cpu_info_); \
    EXPECT_LE(diff, max_diff);                                             \
  }                                                                        \
  TEST_F(LibYUVScaleTest, NV12##name##SwapXY_##filter) {                   \
    int diff = NV12TestFilter(benchmark_width_, benchmark_height_,         \
                              benchmark_height_, benchmark_width_,         \
                              kFilter##filter, benchmark_iterations_,      \
                              disable_cpu_flags_, benchmark_cpu_info_);    \
    EXPECT_LE(diff, max_diff);                                             \
  }

// Test scale to a specified size with all 4 filters.
#ifndef DISABLE_SLOW_TESTS
TEST_SCALESWAPXY1(, Scale, None, 0)
TEST_SCALESWAPXY1(, Scale, Linear, 3)
TEST_SCALESWAPXY1(, Scale, Bilinear, 3)
TEST_SCALESWAPXY1(, Scale, Box, 3)
#else
#if defined(ENABLE_FULL_TESTS)
TEST_SCALESWAPXY1(DISABLED_, Scale, None, 0)
TEST_SCALESWAPXY1(DISABLED_, Scale, Linear, 3)
TEST_SCALESWAPXY1(DISABLED_, Scale, Bilinear, 3)
TEST_SCALESWAPXY1(DISABLED_, Scale, Box, 3)
#else
TEST_SCALESWAPXY1(DISABLED_, Scale, Bilinear, 3)
TEST_SCALESWAPXY1(DISABLED_, Scale, Box, 3)
#endif
#endif
#undef TEST_SCALESWAPXY1

}  // namespace libyuv
