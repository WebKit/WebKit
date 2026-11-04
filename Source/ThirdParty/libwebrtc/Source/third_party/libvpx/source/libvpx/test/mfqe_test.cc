/*
 *  Copyright (c) 2026 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <array>
#include <cstring>
#include <memory>
#include <new>

#include "gtest/gtest.h"
#include "vpx_config.h"
#include "vp9/common/vp9_mfqe.h"
#include "vp9/common/vp9_onyxc_int.h"
#include "vpx_scale/yv12config.h"

namespace {

constexpr int kFrameSize = 64;
constexpr int kCheckerboardSquareSize = 4;

class TmpFrameBuffer {
 public:
  // Returns an initialized frame, or nullptr if allocation fails.
  static std::unique_ptr<TmpFrameBuffer> Create(int width, int height,
                                                int stride,
                                                bool fill_checkerboard);

  TmpFrameBuffer(const TmpFrameBuffer &) = delete;
  TmpFrameBuffer &operator=(const TmpFrameBuffer &) = delete;
  ~TmpFrameBuffer() { vpx_free_frame_buffer(&buffer_); }

  // Returns a borrowed reference valid for this object's lifetime.
  YV12_BUFFER_CONFIG &buffer() { return buffer_; }

 private:
  // Takes ownership of buffer's allocation.
  explicit TmpFrameBuffer(const YV12_BUFFER_CONFIG &buffer) : buffer_(buffer) {}

  YV12_BUFFER_CONFIG buffer_ = {};
};

std::unique_ptr<TmpFrameBuffer> TmpFrameBuffer::Create(int width, int height,
                                                       int stride,
                                                       bool fill_checkerboard) {
  YV12_BUFFER_CONFIG buffer = {};
#if CONFIG_VP9_HIGHBITDEPTH
  const int allocation_status =
      vpx_alloc_frame_buffer(&buffer, stride, height, 1, 1, 0, 0, 0);
#else
  const int allocation_status =
      vpx_alloc_frame_buffer(&buffer, stride, height, 1, 1, 0, 0);
#endif
  if (allocation_status != 0) return nullptr;

  buffer.y_width = width;
  buffer.y_height = height;
  buffer.y_crop_width = width;
  buffer.y_crop_height = height;
  buffer.uv_width = width >> 1;
  buffer.uv_height = height >> 1;
  buffer.uv_crop_width = width >> 1;
  buffer.uv_crop_height = height >> 1;
  buffer.render_width = width;
  buffer.render_height = height;

  if (fill_checkerboard) {
    // Fill the luma plane with a checkerboard pattern.
    for (int row = 0; row < height; ++row) {
      for (int col = 0; col < width; ++col) {
        const int checkerboard_row = row / kCheckerboardSquareSize;
        const int checkerboard_col = col / kCheckerboardSquareSize;
        int pixel_value = 0x20;
        if ((checkerboard_row + checkerboard_col) % 2 != 0) {
          pixel_value = 0xe0;
        }
        buffer.y_buffer[row * buffer.y_stride + col] = pixel_value;
      }
    }
  } else {
    // Use a distinct initial luma value so copying is observable.
    for (int row = 0; row < height; ++row) {
      memset(buffer.y_buffer + row * buffer.y_stride, 0, width);
    }
  }

  // Use neutral chroma so the checkerboard pattern is luma-only.
  for (int row = 0; row < height / 2; ++row) {
    memset(buffer.u_buffer + row * buffer.uv_stride, 0x80, width / 2);
    memset(buffer.v_buffer + row * buffer.uv_stride, 0x80, width / 2);
  }
  std::unique_ptr<TmpFrameBuffer> frame(new (std::nothrow)
                                            TmpFrameBuffer(buffer));
  if (frame == nullptr) {
    vpx_free_frame_buffer(&buffer);
  }
  return frame;
}

TEST(MfqeTest, Copy64x64WithDifferentStrides) {
  auto source = TmpFrameBuffer::Create(kFrameSize, kFrameSize, kFrameSize,
                                       /*fill_checkerboard=*/true);
  ASSERT_NE(source, nullptr);
  // The wider allocation produces a different stride while preserving the
  // visible dimensions of the image.
  auto destination =
      TmpFrameBuffer::Create(kFrameSize, kFrameSize, kFrameSize + 16,
                             /*fill_checkerboard=*/false);
  ASSERT_NE(destination, nullptr);
  ASSERT_NE(source->buffer().y_stride, destination->buffer().y_stride);

  // One MODE_INFO entry per 8x8 MI unit in this 64x64 superblock.
  std::array<MODE_INFO, MI_BLOCK_SIZE * MI_BLOCK_SIZE> mi = {};
  mi[0].sb_type = BLOCK_64X64;
  mi[0].mode = DC_PRED;  // Force the copy path rather than MFQE filtering.

  VP9_COMMON cm = {};
  cm.frame_type = INTER_FRAME;
  cm.frame_to_show = &source->buffer();
  cm.post_proc_buffer = destination->buffer();
  cm.mi_rows = MI_BLOCK_SIZE;
  cm.mi_cols = MI_BLOCK_SIZE;
  cm.mi_stride = MI_BLOCK_SIZE;
  cm.mi = mi.data();

  vp9_mfqe(&cm);

  for (int row = 0; row < kFrameSize; ++row) {
    EXPECT_EQ(
        std::memcmp(
            source->buffer().y_buffer + row * source->buffer().y_stride,
            cm.post_proc_buffer.y_buffer + row * cm.post_proc_buffer.y_stride,
            kFrameSize),
        0)
        << "row " << row;
  }
}

}  // namespace
