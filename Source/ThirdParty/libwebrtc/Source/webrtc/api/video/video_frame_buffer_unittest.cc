/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame_buffer.h"

#include <cstdint>
#include <cstring>

#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/nv12_buffer.h"
#include "test/frame_utils.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;

// Creates a 4x4 I420 buffer where every sample has a unique value. The buffer
// is tightly packed, so the content can be copied in one go per plane.
scoped_refptr<I420Buffer> CreateTestI420Buffer() {
  scoped_refptr<I420Buffer> buffer = I420Buffer::Create(kWidth, kHeight);
  const uint8_t kYContent[] = {
      // clang-format off
      1,  2,  3,  4,
      5,  6,  7,  8,
      9,  10, 11, 12,
      13, 14, 15, 16
      // clang-format on
  };
  const uint8_t kUContent[] = {17, 18, 19, 20};
  const uint8_t kVContent[] = {21, 22, 23, 24};
  memcpy(buffer->MutableDataY(), kYContent, sizeof(kYContent));
  memcpy(buffer->MutableDataU(), kUContent, sizeof(kUContent));
  memcpy(buffer->MutableDataV(), kVContent, sizeof(kVContent));
  return buffer;
}

// A kNative buffer that does not override ToI420ForInspection(), i.e. it relies
// on the default implementation in VideoFrameBuffer. It counts ToI420() calls
// so that the test can verify that the default implementation forwards.
class FakeNativeBuffer : public VideoFrameBuffer {
 public:
  explicit FakeNativeBuffer(scoped_refptr<I420Buffer> buffer)
      : buffer_(buffer) {}

  Type type() const override { return Type::kNative; }
  int width() const override { return buffer_->width(); }
  int height() const override { return buffer_->height(); }

  scoped_refptr<I420BufferInterface> ToI420() override {
    ++to_i420_calls_;
    return buffer_;
  }

  int to_i420_calls() const { return to_i420_calls_; }

 private:
  const scoped_refptr<I420Buffer> buffer_;
  int to_i420_calls_ = 0;
};

TEST(VideoFrameBufferTest, ToI420ForInspectionMatchesToI420ForI420Buffer) {
  scoped_refptr<VideoFrameBuffer> buffer = CreateTestI420Buffer();

  EXPECT_TRUE(
      test::FrameBufsEqual(buffer->ToI420ForInspection(), buffer->ToI420()));
}

TEST(VideoFrameBufferTest, ToI420ForInspectionMatchesToI420ForNV12Buffer) {
  scoped_refptr<VideoFrameBuffer> buffer =
      NV12Buffer::Copy(*CreateTestI420Buffer());

  EXPECT_TRUE(
      test::FrameBufsEqual(buffer->ToI420ForInspection(), buffer->ToI420()));
}

TEST(VideoFrameBufferTest, ToI420ForInspectionDefaultsToToI420) {
  scoped_refptr<I420Buffer> i420_buffer = CreateTestI420Buffer();
  auto native_buffer = make_ref_counted<FakeNativeBuffer>(i420_buffer);

  scoped_refptr<I420BufferInterface> inspected =
      native_buffer->ToI420ForInspection();

  EXPECT_EQ(native_buffer->to_i420_calls(), 1);
  EXPECT_TRUE(test::FrameBufsEqual(inspected, i420_buffer));
}

}  // namespace
}  // namespace webrtc
