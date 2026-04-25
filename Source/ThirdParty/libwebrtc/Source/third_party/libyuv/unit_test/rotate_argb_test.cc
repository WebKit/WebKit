/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include "../unit_test/unit_test.h"
#include "libyuv/cpu_id.h"
#include "libyuv/rotate_argb.h"

namespace libyuv {

static void TestRotateBpp(int src_width,
                          int src_height,
                          int dst_width,
                          int dst_height,
                          libyuv::RotationMode mode,
                          int benchmark_iterations,
                          int disable_cpu_flags,
                          int benchmark_cpu_info,
                          const int kBpp) {
  if (src_width < 1) {
    src_width = 1;
  }
  if (src_height < 1) {
    src_height = 1;
  }
  if (dst_width < 1) {
    dst_width = 1;
  }
  if (dst_height < 1) {
    dst_height = 1;
  }
  int src_stride_argb = src_width * kBpp;
  int src_argb_plane_size = src_stride_argb * abs(src_height);
  align_buffer_page_end(src_argb, src_argb_plane_size);
  for (int i = 0; i < src_argb_plane_size; ++i) {
    src_argb[i] = fastrand() & 0xff;
  }

  int dst_stride_argb = dst_width * kBpp;
  int dst_argb_plane_size = dst_stride_argb * dst_height;
  align_buffer_page_end(dst_argb_c, dst_argb_plane_size);
  align_buffer_page_end(dst_argb_opt, dst_argb_plane_size);
  memset(dst_argb_c, 2, dst_argb_plane_size);
  memset(dst_argb_opt, 3, dst_argb_plane_size);

  if (kBpp == 1) {
    MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
    RotatePlane(src_argb, src_stride_argb, dst_argb_c, dst_stride_argb,
                src_width, src_height, mode);

    MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
    for (int i = 0; i < benchmark_iterations; ++i) {
      RotatePlane(src_argb, src_stride_argb, dst_argb_opt, dst_stride_argb,
                  src_width, src_height, mode);
    }
  } else if (kBpp == 4) {
    MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
    ARGBRotate(src_argb, src_stride_argb, dst_argb_c, dst_stride_argb,
               src_width, src_height, mode);

    MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
    for (int i = 0; i < benchmark_iterations; ++i) {
      ARGBRotate(src_argb, src_stride_argb, dst_argb_opt, dst_stride_argb,
                 src_width, src_height, mode);
    }
  }

  // Rotation should be exact.
  for (int i = 0; i < dst_argb_plane_size; ++i) {
    EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);
  }

  free_aligned_buffer_page_end(dst_argb_c);
  free_aligned_buffer_page_end(dst_argb_opt);
  free_aligned_buffer_page_end(src_argb);
}

static void ARGBTestRotate(int src_width,
                           int src_height,
                           int dst_width,
                           int dst_height,
                           libyuv::RotationMode mode,
                           int benchmark_iterations,
                           int disable_cpu_flags,
                           int benchmark_cpu_info) {
  TestRotateBpp(src_width, src_height, dst_width, dst_height, mode,
                benchmark_iterations, disable_cpu_flags, benchmark_cpu_info, 4);
}

TEST_F(LibYUVRotateTest, ARGBRotate0_Opt) {
  ARGBTestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate0, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, ARGBRotate90_Opt) {
  ARGBTestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate90, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, ARGBRotate180_Opt) {
  ARGBTestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate180, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, ARGBRotate270_Opt) {
  ARGBTestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate270, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

static void TestRotatePlane(int src_width,
                            int src_height,
                            int dst_width,
                            int dst_height,
                            libyuv::RotationMode mode,
                            int benchmark_iterations,
                            int disable_cpu_flags,
                            int benchmark_cpu_info) {
  TestRotateBpp(src_width, src_height, dst_width, dst_height, mode,
                benchmark_iterations, disable_cpu_flags, benchmark_cpu_info, 1);
}

TEST_F(LibYUVRotateTest, RotatePlane0_Opt) {
  TestRotatePlane(benchmark_width_, benchmark_height_, benchmark_width_,
                  benchmark_height_, kRotate0, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane90_Opt) {
  TestRotatePlane(benchmark_width_, benchmark_height_, benchmark_height_,
                  benchmark_width_, kRotate90, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane180_Opt) {
  TestRotatePlane(benchmark_width_, benchmark_height_, benchmark_width_,
                  benchmark_height_, kRotate180, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane270_Opt) {
  TestRotatePlane(benchmark_width_, benchmark_height_, benchmark_height_,
                  benchmark_width_, kRotate270, benchmark_iterations_,
                  disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_RotatePlane0_Odd) {
  TestRotatePlane(benchmark_width_ + 1, benchmark_height_ + 1,
                  benchmark_width_ + 1, benchmark_height_ + 1, kRotate0,
                  benchmark_iterations_, disable_cpu_flags_,
                  benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_RotatePlane90_Odd) {
  TestRotatePlane(benchmark_width_ + 1, benchmark_height_ + 1,
                  benchmark_height_ + 1, benchmark_width_ + 1, kRotate90,
                  benchmark_iterations_, disable_cpu_flags_,
                  benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_RotatePlane180_Odd) {
  TestRotatePlane(benchmark_width_ + 1, benchmark_height_ + 1,
                  benchmark_width_ + 1, benchmark_height_ + 1, kRotate180,
                  benchmark_iterations_, disable_cpu_flags_,
                  benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_RotatePlane270_Odd) {
  TestRotatePlane(benchmark_width_ + 1, benchmark_height_ + 1,
                  benchmark_height_ + 1, benchmark_width_ + 1, kRotate270,
                  benchmark_iterations_, disable_cpu_flags_,
                  benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane90_TestStride) {
  int argb_plane_size = benchmark_width_ * 4 * abs(benchmark_height_);

  align_buffer_page_end(src_argb, argb_plane_size);
  align_buffer_page_end(dst_argb, argb_plane_size);

  EXPECT_EQ(0, ARGBRotate(src_argb, benchmark_width_ * 4, dst_argb,
                          benchmark_width_ * 4, benchmark_width_,
                          benchmark_height_, kRotate0));

  EXPECT_EQ(0, ARGBRotate(src_argb, benchmark_width_ * 4 - 1, dst_argb,
                          benchmark_width_ * 4 - 1, benchmark_width_ - 1,
                          benchmark_height_, kRotate0));

  EXPECT_EQ(0, ARGBRotate(src_argb, benchmark_width_ * 4, dst_argb,
                          benchmark_width_ * 4, benchmark_width_,
                          benchmark_height_, kRotate180));

  EXPECT_EQ(0, ARGBRotate(src_argb, benchmark_width_ * 4 - 1, dst_argb,
                          benchmark_width_ * 4 - 1, benchmark_width_ - 1,
                          benchmark_height_, kRotate180));

  EXPECT_EQ(0, ARGBRotate(src_argb, benchmark_width_ * 4, dst_argb,
                          abs(benchmark_height_) * 4, benchmark_width_,
                          benchmark_height_, kRotate90));

  EXPECT_EQ(-1, ARGBRotate(src_argb, benchmark_width_ * 4 - 1, dst_argb,
                           abs(benchmark_height_) * 4, benchmark_width_ - 1,
                           benchmark_height_, kRotate90));

  EXPECT_EQ(0, ARGBRotate(src_argb, benchmark_width_ * 4, dst_argb,
                          abs(benchmark_height_) * 4, benchmark_width_,
                          benchmark_height_, kRotate270));

  EXPECT_EQ(-1, ARGBRotate(src_argb, benchmark_width_ * 4 - 1, dst_argb,
                           abs(benchmark_height_) * 4, benchmark_width_ - 1,
                           benchmark_height_, kRotate270));

  free_aligned_buffer_page_end(dst_argb);
  free_aligned_buffer_page_end(src_argb);
}

static void TestRotatePlane_16(int src_width,
                               int src_height,
                               int dst_width,
                               int dst_height,
                               libyuv::RotationMode mode,
                               int benchmark_iterations,
                               int disable_cpu_flags,
                               int benchmark_cpu_info) {
  if (src_width < 1) {
    src_width = 1;
  }
  if (src_height < 1) {
    src_height = 1;
  }
  if (dst_width < 1) {
    dst_width = 1;
  }
  if (dst_height < 1) {
    dst_height = 1;
  }
  int src_stride = src_width;
  int src_plane_size = src_stride * abs(src_height);
  align_buffer_page_end_16(src, src_plane_size);
  for (int i = 0; i < src_plane_size; ++i) {
    src[i] = fastrand() & 0xff;
  }

  int dst_stride = dst_width;
  int dst_plane_size = dst_stride * dst_height;
  align_buffer_page_end_16(dst_c, dst_plane_size);
  align_buffer_page_end_16(dst_opt, dst_plane_size);
  memset(dst_c, 2, dst_plane_size);
  memset(dst_opt, 3, dst_plane_size);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  RotatePlane_16(src, src_stride, dst_c, dst_stride, src_width, src_height,
                 mode);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (int i = 0; i < benchmark_iterations; ++i) {
    RotatePlane_16(src, src_stride, dst_opt, dst_stride, src_width, src_height,
                   mode);
  }

  // Rotation should be exact.
  for (int i = 0; i < dst_plane_size; ++i) {
    EXPECT_EQ(dst_c[i], dst_opt[i]);
  }

  free_aligned_buffer_page_end_16(dst_c);
  free_aligned_buffer_page_end_16(dst_opt);
  free_aligned_buffer_page_end_16(src);
}

TEST_F(LibYUVRotateTest, RotatePlane0_16_Opt) {
  TestRotatePlane_16(benchmark_width_, benchmark_height_, benchmark_width_,
                     benchmark_height_, kRotate0, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane90_16_Opt) {
  TestRotatePlane_16(benchmark_width_, benchmark_height_, benchmark_height_,
                     benchmark_width_, kRotate90, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane180_16_Opt) {
  TestRotatePlane_16(benchmark_width_, benchmark_height_, benchmark_width_,
                     benchmark_height_, kRotate180, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane270_16_Opt) {
  TestRotatePlane_16(benchmark_width_, benchmark_height_, benchmark_height_,
                     benchmark_width_, kRotate270, benchmark_iterations_,
                     disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane0_16_Odd) {
  TestRotatePlane_16(benchmark_width_ + 1, benchmark_height_ + 1,
                     benchmark_width_ + 1, benchmark_height_ + 1, kRotate0,
                     benchmark_iterations_, disable_cpu_flags_,
                     benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane90_16_Odd) {
  TestRotatePlane_16(benchmark_width_ + 1, benchmark_height_ + 1,
                     benchmark_height_ + 1, benchmark_width_ + 1, kRotate90,
                     benchmark_iterations_, disable_cpu_flags_,
                     benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane180_16_Odd) {
  TestRotatePlane_16(benchmark_width_ + 1, benchmark_height_ + 1,
                     benchmark_width_ + 1, benchmark_height_ + 1, kRotate180,
                     benchmark_iterations_, disable_cpu_flags_,
                     benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, RotatePlane270_16_Odd) {
  TestRotatePlane_16(benchmark_width_ + 1, benchmark_height_ + 1,
                     benchmark_height_ + 1, benchmark_width_ + 1, kRotate270,
                     benchmark_iterations_, disable_cpu_flags_,
                     benchmark_cpu_info_);
}

}  // namespace libyuv
