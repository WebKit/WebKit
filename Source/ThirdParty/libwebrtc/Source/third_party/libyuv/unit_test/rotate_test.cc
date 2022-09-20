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
#include "libyuv/rotate.h"

namespace libyuv {

#define SUBSAMPLE(v, a) ((((v) + (a)-1)) / (a))

static void I420TestRotate(int src_width,
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
  if (src_height == 0) {
    src_height = 1;
  }
  if (dst_width < 1) {
    dst_width = 1;
  }
  if (dst_height < 1) {
    dst_height = 1;
  }
  int src_i420_y_size = src_width * Abs(src_height);
  int src_i420_uv_size = ((src_width + 1) / 2) * ((Abs(src_height) + 1) / 2);
  int src_i420_size = src_i420_y_size + src_i420_uv_size * 2;
  align_buffer_page_end(src_i420, src_i420_size);
  for (int i = 0; i < src_i420_size; ++i) {
    src_i420[i] = fastrand() & 0xff;
  }

  int dst_i420_y_size = dst_width * dst_height;
  int dst_i420_uv_size = ((dst_width + 1) / 2) * ((dst_height + 1) / 2);
  int dst_i420_size = dst_i420_y_size + dst_i420_uv_size * 2;
  align_buffer_page_end(dst_i420_c, dst_i420_size);
  align_buffer_page_end(dst_i420_opt, dst_i420_size);
  memset(dst_i420_c, 2, dst_i420_size);
  memset(dst_i420_opt, 3, dst_i420_size);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  I420Rotate(src_i420, src_width, src_i420 + src_i420_y_size,
             (src_width + 1) / 2, src_i420 + src_i420_y_size + src_i420_uv_size,
             (src_width + 1) / 2, dst_i420_c, dst_width,
             dst_i420_c + dst_i420_y_size, (dst_width + 1) / 2,
             dst_i420_c + dst_i420_y_size + dst_i420_uv_size,
             (dst_width + 1) / 2, src_width, src_height, mode);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (int i = 0; i < benchmark_iterations; ++i) {
    I420Rotate(
        src_i420, src_width, src_i420 + src_i420_y_size, (src_width + 1) / 2,
        src_i420 + src_i420_y_size + src_i420_uv_size, (src_width + 1) / 2,
        dst_i420_opt, dst_width, dst_i420_opt + dst_i420_y_size,
        (dst_width + 1) / 2, dst_i420_opt + dst_i420_y_size + dst_i420_uv_size,
        (dst_width + 1) / 2, src_width, src_height, mode);
  }

  // Rotation should be exact.
  for (int i = 0; i < dst_i420_size; ++i) {
    EXPECT_EQ(dst_i420_c[i], dst_i420_opt[i]);
  }

  free_aligned_buffer_page_end(dst_i420_c);
  free_aligned_buffer_page_end(dst_i420_opt);
  free_aligned_buffer_page_end(src_i420);
}

TEST_F(LibYUVRotateTest, I420Rotate0_Opt) {
  I420TestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate0, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I420Rotate90_Opt) {
  I420TestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate90, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I420Rotate180_Opt) {
  I420TestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate180, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I420Rotate270_Opt) {
  I420TestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate270, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

// TODO(fbarchard): Remove odd width tests.
// Odd width tests work but disabled because they use C code and can be
// tested by passing an odd width command line or environment variable.
TEST_F(LibYUVRotateTest, DISABLED_I420Rotate0_Odd) {
  I420TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_width_ + 1, benchmark_height_ + 1, kRotate0,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_I420Rotate90_Odd) {
  I420TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_height_ + 1, benchmark_width_ + 1, kRotate90,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_I420Rotate180_Odd) {
  I420TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_width_ + 1, benchmark_height_ + 1, kRotate180,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_I420Rotate270_Odd) {
  I420TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_height_ + 1, benchmark_width_ + 1, kRotate270,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

static void I422TestRotate(int src_width,
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
  if (src_height == 0) {
    src_height = 1;
  }
  if (dst_width < 1) {
    dst_width = 1;
  }
  if (dst_height < 1) {
    dst_height = 1;
  }
  int src_i422_y_size = src_width * Abs(src_height);
  int src_i422_uv_size = ((src_width + 1) / 2) * Abs(src_height);
  int src_i422_size = src_i422_y_size + src_i422_uv_size * 2;
  align_buffer_page_end(src_i422, src_i422_size);
  for (int i = 0; i < src_i422_size; ++i) {
    src_i422[i] = fastrand() & 0xff;
  }

  int dst_i422_y_size = dst_width * dst_height;
  int dst_i422_uv_size = ((dst_width + 1) / 2) * dst_height;
  int dst_i422_size = dst_i422_y_size + dst_i422_uv_size * 2;
  align_buffer_page_end(dst_i422_c, dst_i422_size);
  align_buffer_page_end(dst_i422_opt, dst_i422_size);
  memset(dst_i422_c, 2, dst_i422_size);
  memset(dst_i422_opt, 3, dst_i422_size);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  I422Rotate(src_i422, src_width, src_i422 + src_i422_y_size,
             (src_width + 1) / 2, src_i422 + src_i422_y_size + src_i422_uv_size,
             (src_width + 1) / 2, dst_i422_c, dst_width,
             dst_i422_c + dst_i422_y_size, (dst_width + 1) / 2,
             dst_i422_c + dst_i422_y_size + dst_i422_uv_size,
             (dst_width + 1) / 2, src_width, src_height, mode);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (int i = 0; i < benchmark_iterations; ++i) {
    I422Rotate(
        src_i422, src_width, src_i422 + src_i422_y_size, (src_width + 1) / 2,
        src_i422 + src_i422_y_size + src_i422_uv_size, (src_width + 1) / 2,
        dst_i422_opt, dst_width, dst_i422_opt + dst_i422_y_size,
        (dst_width + 1) / 2, dst_i422_opt + dst_i422_y_size + dst_i422_uv_size,
        (dst_width + 1) / 2, src_width, src_height, mode);
  }

  // Rotation should be exact.
  for (int i = 0; i < dst_i422_size; ++i) {
    EXPECT_EQ(dst_i422_c[i], dst_i422_opt[i]);
  }

  free_aligned_buffer_page_end(dst_i422_c);
  free_aligned_buffer_page_end(dst_i422_opt);
  free_aligned_buffer_page_end(src_i422);
}

TEST_F(LibYUVRotateTest, I422Rotate0_Opt) {
  I422TestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate0, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I422Rotate90_Opt) {
  I422TestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate90, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I422Rotate180_Opt) {
  I422TestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate180, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I422Rotate270_Opt) {
  I422TestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate270, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

static void I444TestRotate(int src_width,
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
  if (src_height == 0) {
    src_height = 1;
  }
  if (dst_width < 1) {
    dst_width = 1;
  }
  if (dst_height < 1) {
    dst_height = 1;
  }
  int src_i444_y_size = src_width * Abs(src_height);
  int src_i444_uv_size = src_width * Abs(src_height);
  int src_i444_size = src_i444_y_size + src_i444_uv_size * 2;
  align_buffer_page_end(src_i444, src_i444_size);
  for (int i = 0; i < src_i444_size; ++i) {
    src_i444[i] = fastrand() & 0xff;
  }

  int dst_i444_y_size = dst_width * dst_height;
  int dst_i444_uv_size = dst_width * dst_height;
  int dst_i444_size = dst_i444_y_size + dst_i444_uv_size * 2;
  align_buffer_page_end(dst_i444_c, dst_i444_size);
  align_buffer_page_end(dst_i444_opt, dst_i444_size);
  memset(dst_i444_c, 2, dst_i444_size);
  memset(dst_i444_opt, 3, dst_i444_size);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  I444Rotate(src_i444, src_width, src_i444 + src_i444_y_size, src_width,
             src_i444 + src_i444_y_size + src_i444_uv_size, src_width,
             dst_i444_c, dst_width, dst_i444_c + dst_i444_y_size, dst_width,
             dst_i444_c + dst_i444_y_size + dst_i444_uv_size, dst_width,
             src_width, src_height, mode);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (int i = 0; i < benchmark_iterations; ++i) {
    I444Rotate(src_i444, src_width, src_i444 + src_i444_y_size, src_width,
               src_i444 + src_i444_y_size + src_i444_uv_size, src_width,
               dst_i444_opt, dst_width, dst_i444_opt + dst_i444_y_size,
               dst_width, dst_i444_opt + dst_i444_y_size + dst_i444_uv_size,
               dst_width, src_width, src_height, mode);
  }

  // Rotation should be exact.
  for (int i = 0; i < dst_i444_size; ++i) {
    EXPECT_EQ(dst_i444_c[i], dst_i444_opt[i]);
  }

  free_aligned_buffer_page_end(dst_i444_c);
  free_aligned_buffer_page_end(dst_i444_opt);
  free_aligned_buffer_page_end(src_i444);
}

TEST_F(LibYUVRotateTest, I444Rotate0_Opt) {
  I444TestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate0, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I444Rotate90_Opt) {
  I444TestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate90, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I444Rotate180_Opt) {
  I444TestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate180, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, I444Rotate270_Opt) {
  I444TestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate270, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

// TODO(fbarchard): Remove odd width tests.
// Odd width tests work but disabled because they use C code and can be
// tested by passing an odd width command line or environment variable.
TEST_F(LibYUVRotateTest, DISABLED_I444Rotate0_Odd) {
  I444TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_width_ + 1, benchmark_height_ + 1, kRotate0,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_I444Rotate90_Odd) {
  I444TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_height_ + 1, benchmark_width_ + 1, kRotate90,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_I444Rotate180_Odd) {
  I444TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_width_ + 1, benchmark_height_ + 1, kRotate180,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_I444Rotate270_Odd) {
  I444TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_height_ + 1, benchmark_width_ + 1, kRotate270,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

static void NV12TestRotate(int src_width,
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
  if (src_height == 0) {  // allow negative for inversion test.
    src_height = 1;
  }
  if (dst_width < 1) {
    dst_width = 1;
  }
  if (dst_height < 1) {
    dst_height = 1;
  }
  int src_nv12_y_size = src_width * Abs(src_height);
  int src_nv12_uv_size =
      ((src_width + 1) / 2) * ((Abs(src_height) + 1) / 2) * 2;
  int src_nv12_size = src_nv12_y_size + src_nv12_uv_size;
  align_buffer_page_end(src_nv12, src_nv12_size);
  for (int i = 0; i < src_nv12_size; ++i) {
    src_nv12[i] = fastrand() & 0xff;
  }

  int dst_i420_y_size = dst_width * dst_height;
  int dst_i420_uv_size = ((dst_width + 1) / 2) * ((dst_height + 1) / 2);
  int dst_i420_size = dst_i420_y_size + dst_i420_uv_size * 2;
  align_buffer_page_end(dst_i420_c, dst_i420_size);
  align_buffer_page_end(dst_i420_opt, dst_i420_size);
  memset(dst_i420_c, 2, dst_i420_size);
  memset(dst_i420_opt, 3, dst_i420_size);

  MaskCpuFlags(disable_cpu_flags);  // Disable all CPU optimization.
  NV12ToI420Rotate(src_nv12, src_width, src_nv12 + src_nv12_y_size,
                   (src_width + 1) & ~1, dst_i420_c, dst_width,
                   dst_i420_c + dst_i420_y_size, (dst_width + 1) / 2,
                   dst_i420_c + dst_i420_y_size + dst_i420_uv_size,
                   (dst_width + 1) / 2, src_width, src_height, mode);

  MaskCpuFlags(benchmark_cpu_info);  // Enable all CPU optimization.
  for (int i = 0; i < benchmark_iterations; ++i) {
    NV12ToI420Rotate(src_nv12, src_width, src_nv12 + src_nv12_y_size,
                     (src_width + 1) & ~1, dst_i420_opt, dst_width,
                     dst_i420_opt + dst_i420_y_size, (dst_width + 1) / 2,
                     dst_i420_opt + dst_i420_y_size + dst_i420_uv_size,
                     (dst_width + 1) / 2, src_width, src_height, mode);
  }

  // Rotation should be exact.
  for (int i = 0; i < dst_i420_size; ++i) {
    EXPECT_EQ(dst_i420_c[i], dst_i420_opt[i]);
  }

  free_aligned_buffer_page_end(dst_i420_c);
  free_aligned_buffer_page_end(dst_i420_opt);
  free_aligned_buffer_page_end(src_nv12);
}

TEST_F(LibYUVRotateTest, NV12Rotate0_Opt) {
  NV12TestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate0, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, NV12Rotate90_Opt) {
  NV12TestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate90, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, NV12Rotate180_Opt) {
  NV12TestRotate(benchmark_width_, benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate180, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, NV12Rotate270_Opt) {
  NV12TestRotate(benchmark_width_, benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate270, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_NV12Rotate0_Odd) {
  NV12TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_width_ + 1, benchmark_height_ + 1, kRotate0,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_NV12Rotate90_Odd) {
  NV12TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_height_ + 1, benchmark_width_ + 1, kRotate90,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_NV12Rotate180_Odd) {
  NV12TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_width_ + 1, benchmark_height_ + 1, kRotate180,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, DISABLED_NV12Rotate270_Odd) {
  NV12TestRotate(benchmark_width_ + 1, benchmark_height_ + 1,
                 benchmark_height_ + 1, benchmark_width_ + 1, kRotate270,
                 benchmark_iterations_, disable_cpu_flags_,
                 benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, NV12Rotate0_Invert) {
  NV12TestRotate(benchmark_width_, -benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate0, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, NV12Rotate90_Invert) {
  NV12TestRotate(benchmark_width_, -benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate90, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, NV12Rotate180_Invert) {
  NV12TestRotate(benchmark_width_, -benchmark_height_, benchmark_width_,
                 benchmark_height_, kRotate180, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

TEST_F(LibYUVRotateTest, NV12Rotate270_Invert) {
  NV12TestRotate(benchmark_width_, -benchmark_height_, benchmark_height_,
                 benchmark_width_, kRotate270, benchmark_iterations_,
                 disable_cpu_flags_, benchmark_cpu_info_);
}

// Test Android 420 to I420 Rotate
#define TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X,          \
                        SRC_SUBSAMP_Y, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,      \
                        W1280, N, NEG, OFF, PN, OFF_U, OFF_V, ROT)            \
  TEST_F(LibYUVRotateTest,                                                    \
         SRC_FMT_PLANAR##To##FMT_PLANAR##Rotate##ROT##To##PN##N) {            \
    const int kWidth = W1280;                                                 \
    const int kHeight = benchmark_height_;                                    \
    const int kSizeUV =                                                       \
        SUBSAMPLE(kWidth, SRC_SUBSAMP_X) * SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); \
    align_buffer_page_end(src_y, kWidth* kHeight + OFF);                      \
    align_buffer_page_end(src_uv,                                             \
                          kSizeUV*((PIXEL_STRIDE == 3) ? 3 : 2) + OFF);       \
    align_buffer_page_end(dst_y_c, kWidth* kHeight);                          \
    align_buffer_page_end(dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X) *             \
                                       SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    align_buffer_page_end(dst_v_c, SUBSAMPLE(kWidth, SUBSAMP_X) *             \
                                       SUBSAMPLE(kHeight, SUBSAMP_Y));        \
    align_buffer_page_end(dst_y_opt, kWidth* kHeight);                        \
    align_buffer_page_end(dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X) *           \
                                         SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    align_buffer_page_end(dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X) *           \
                                         SUBSAMPLE(kHeight, SUBSAMP_Y));      \
    uint8_t* src_u = src_uv + OFF_U;                                          \
    uint8_t* src_v = src_uv + (PIXEL_STRIDE == 1 ? kSizeUV : OFF_V);          \
    int src_stride_uv = SUBSAMPLE(kWidth, SUBSAMP_X) * PIXEL_STRIDE;          \
    for (int i = 0; i < kHeight; ++i)                                         \
      for (int j = 0; j < kWidth; ++j)                                        \
        src_y[i * kWidth + j + OFF] = (fastrand() & 0xff);                    \
    for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {             \
      for (int j = 0; j < SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {            \
        src_u[(i * src_stride_uv) + j * PIXEL_STRIDE + OFF] =                 \
            (fastrand() & 0xff);                                              \
        src_v[(i * src_stride_uv) + j * PIXEL_STRIDE + OFF] =                 \
            (fastrand() & 0xff);                                              \
      }                                                                       \
    }                                                                         \
    memset(dst_y_c, 1, kWidth* kHeight);                                      \
    memset(dst_u_c, 2,                                                        \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_v_c, 3,                                                        \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_y_opt, 101, kWidth* kHeight);                                  \
    memset(dst_u_opt, 102,                                                    \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    memset(dst_v_opt, 103,                                                    \
           SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));     \
    MaskCpuFlags(disable_cpu_flags_);                                         \
    SRC_FMT_PLANAR##To##FMT_PLANAR##Rotate(                                   \
        src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X),   \
        src_v + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), PIXEL_STRIDE, dst_y_c, \
        kWidth, dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X), dst_v_c,               \
        SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight,                    \
        (libyuv::RotationMode)ROT);                                           \
    MaskCpuFlags(benchmark_cpu_info_);                                        \
    for (int i = 0; i < benchmark_iterations_; ++i) {                         \
      SRC_FMT_PLANAR##To##FMT_PLANAR##Rotate(                                 \
          src_y + OFF, kWidth, src_u + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), \
          src_v + OFF, SUBSAMPLE(kWidth, SRC_SUBSAMP_X), PIXEL_STRIDE,        \
          dst_y_opt, kWidth, dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X),         \
          dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X), kWidth, NEG kHeight,       \
          (libyuv::RotationMode)ROT);                                         \
    }                                                                         \
    for (int i = 0; i < kHeight; ++i) {                                       \
      for (int j = 0; j < kWidth; ++j) {                                      \
        EXPECT_EQ(dst_y_c[i * kWidth + j], dst_y_opt[i * kWidth + j]);        \
      }                                                                       \
    }                                                                         \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                \
        EXPECT_EQ(dst_u_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j],              \
                  dst_u_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]);           \
      }                                                                       \
    }                                                                         \
    for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                 \
      for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                \
        EXPECT_EQ(dst_v_c[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j],              \
                  dst_v_opt[i * SUBSAMPLE(kWidth, SUBSAMP_X) + j]);           \
      }                                                                       \
    }                                                                         \
    free_aligned_buffer_page_end(dst_y_c);                                    \
    free_aligned_buffer_page_end(dst_u_c);                                    \
    free_aligned_buffer_page_end(dst_v_c);                                    \
    free_aligned_buffer_page_end(dst_y_opt);                                  \
    free_aligned_buffer_page_end(dst_u_opt);                                  \
    free_aligned_buffer_page_end(dst_v_opt);                                  \
    free_aligned_buffer_page_end(src_y);                                      \
    free_aligned_buffer_page_end(src_uv);                                     \
  }

#define TESTAPLANARTOP(SRC_FMT_PLANAR, PN, PIXEL_STRIDE, OFF_U, OFF_V,         \
                       SRC_SUBSAMP_X, SRC_SUBSAMP_Y, FMT_PLANAR, SUBSAMP_X,    \
                       SUBSAMP_Y)                                              \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_ + 1,      \
                  _Any, +, 0, PN, OFF_U, OFF_V, 0)                             \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_,          \
                  _Unaligned, +, 2, PN, OFF_U, OFF_V, 0)                       \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Invert, \
                  -, 0, PN, OFF_U, OFF_V, 0)                                   \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Opt, +, \
                  0, PN, OFF_U, OFF_V, 0)                                      \
  TESTAPLANARTOPI(SRC_FMT_PLANAR, PIXEL_STRIDE, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,  \
                  FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, benchmark_width_, _Opt, +, \
                  0, PN, OFF_U, OFF_V, 180)

TESTAPLANARTOP(Android420, I420, 1, 0, 0, 2, 2, I420, 2, 2)
TESTAPLANARTOP(Android420, NV12, 2, 0, 1, 2, 2, I420, 2, 2)
TESTAPLANARTOP(Android420, NV21, 2, 1, 0, 2, 2, I420, 2, 2)
#undef TESTAPLANARTOP
#undef TESTAPLANARTOPI

}  // namespace libyuv
