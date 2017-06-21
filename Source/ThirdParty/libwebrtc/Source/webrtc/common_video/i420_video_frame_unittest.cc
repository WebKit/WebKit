/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <string.h>

#include "webrtc/api/video/video_frame.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/test/fake_texture_frame.h"
#include "webrtc/test/frame_utils.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

rtc::scoped_refptr<I420Buffer> CreateGradient(int width, int height) {
  rtc::scoped_refptr<I420Buffer> buffer(
      I420Buffer::Create(width, height));
  // Initialize with gradient, Y = 128(x/w + y/h), U = 256 x/w, V = 256 y/h
  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      buffer->MutableDataY()[x + y * width] =
          128 * (x * height + y * width) / (width * height);
    }
  }
  int chroma_width = buffer->ChromaWidth();
  int chroma_height = buffer->ChromaHeight();
  for (int x = 0; x < chroma_width; x++) {
    for (int y = 0; y < chroma_height; y++) {
      buffer->MutableDataU()[x + y * chroma_width] =
          255 * x / (chroma_width - 1);
      buffer->MutableDataV()[x + y * chroma_width] =
          255 * y / (chroma_height - 1);
    }
  }
  return buffer;
}

// The offsets and sizes describe the rectangle extracted from the
// original (gradient) frame, in relative coordinates where the
// original frame correspond to the unit square, 0.0 <= x, y < 1.0.
void CheckCrop(const webrtc::I420BufferInterface& frame,
               double offset_x,
               double offset_y,
               double rel_width,
               double rel_height) {
  int width = frame.width();
  int height = frame.height();
  // Check that pixel values in the corners match the gradient used
  // for initialization.
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      // Pixel coordinates of the corner.
      int x = i * (width - 1);
      int y = j * (height - 1);
      // Relative coordinates, range 0.0 - 1.0 correspond to the
      // size of the uncropped input frame.
      double orig_x = offset_x + i * rel_width;
      double orig_y = offset_y + j * rel_height;

      EXPECT_NEAR(frame.DataY()[x + y * frame.StrideY()] / 256.0,
                  (orig_x + orig_y) / 2, 0.02);
      EXPECT_NEAR(frame.DataU()[x / 2 + (y / 2) * frame.StrideU()] / 256.0,
                  orig_x, 0.02);
      EXPECT_NEAR(frame.DataV()[x / 2 + (y / 2) * frame.StrideV()] / 256.0,
                  orig_y, 0.02);
    }
  }
}

void CheckRotate(int width,
                 int height,
                 webrtc::VideoRotation rotation,
                 const webrtc::I420BufferInterface& rotated) {
  int rotated_width = width;
  int rotated_height = height;

  if (rotation == kVideoRotation_90 || rotation == kVideoRotation_270) {
    std::swap(rotated_width, rotated_height);
  }
  EXPECT_EQ(rotated_width, rotated.width());
  EXPECT_EQ(rotated_height, rotated.height());

  // Clock-wise order (with 0,0 at top-left)
  const struct { int x; int y; } corners[] = {
    { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }
  };
  // Corresponding corner colors of the frame produced by CreateGradient.
  const struct { int y; int u; int v; } colors[] = {
    {0, 0, 0}, { 127, 255, 0}, { 255, 255, 255 }, {127, 0, 255}
  };
  int corner_offset = static_cast<int>(rotation) / 90;

  for (int i = 0; i < 4; i++) {
    int j = (i + corner_offset) % 4;
    int x = corners[j].x * (rotated_width - 1);
    int y = corners[j].y * (rotated_height - 1);
    EXPECT_EQ(colors[i].y, rotated.DataY()[x + y * rotated.StrideY()]);
    EXPECT_EQ(colors[i].u,
              rotated.DataU()[(x / 2) + (y / 2) * rotated.StrideU()]);
    EXPECT_EQ(colors[i].v,
              rotated.DataV()[(x / 2) + (y / 2) * rotated.StrideV()]);
  }
}

}  // namespace

TEST(TestVideoFrame, WidthHeightValues) {
  VideoFrame frame(I420Buffer::Create(10, 10, 10, 14, 90),
                   webrtc::kVideoRotation_0,
                   789 * rtc::kNumMicrosecsPerMillisec);
  const int valid_value = 10;
  EXPECT_EQ(valid_value, frame.width());
  EXPECT_EQ(valid_value, frame.height());
  frame.set_timestamp(123u);
  EXPECT_EQ(123u, frame.timestamp());
  frame.set_ntp_time_ms(456);
  EXPECT_EQ(456, frame.ntp_time_ms());
  EXPECT_EQ(789, frame.render_time_ms());
}

TEST(TestVideoFrame, ShallowCopy) {
  uint32_t timestamp = 1;
  int64_t ntp_time_ms = 2;
  int64_t timestamp_us = 3;
  int stride_y = 15;
  int stride_u = 10;
  int stride_v = 10;
  int width = 15;
  int height = 15;

  const int kSizeY = 400;
  const int kSizeU = 100;
  const int kSizeV = 100;
  const VideoRotation kRotation = kVideoRotation_270;
  uint8_t buffer_y[kSizeY];
  uint8_t buffer_u[kSizeU];
  uint8_t buffer_v[kSizeV];
  memset(buffer_y, 16, kSizeY);
  memset(buffer_u, 8, kSizeU);
  memset(buffer_v, 4, kSizeV);

  VideoFrame frame1(
      I420Buffer::Copy(width, height,
                       buffer_y, stride_y,
                       buffer_u, stride_u,
                       buffer_v, stride_v),
      kRotation, 0);
  frame1.set_timestamp(timestamp);
  frame1.set_ntp_time_ms(ntp_time_ms);
  frame1.set_timestamp_us(timestamp_us);
  VideoFrame frame2(frame1);

  EXPECT_EQ(frame1.video_frame_buffer(), frame2.video_frame_buffer());
  rtc::scoped_refptr<I420BufferInterface> yuv1 =
      frame1.video_frame_buffer()->GetI420();
  rtc::scoped_refptr<I420BufferInterface> yuv2 =
      frame2.video_frame_buffer()->GetI420();
  EXPECT_EQ(yuv1->DataY(), yuv2->DataY());
  EXPECT_EQ(yuv1->DataU(), yuv2->DataU());
  EXPECT_EQ(yuv1->DataV(), yuv2->DataV());

  EXPECT_EQ(frame2.timestamp(), frame1.timestamp());
  EXPECT_EQ(frame2.ntp_time_ms(), frame1.ntp_time_ms());
  EXPECT_EQ(frame2.timestamp_us(), frame1.timestamp_us());
  EXPECT_EQ(frame2.rotation(), frame1.rotation());

  frame2.set_timestamp(timestamp + 1);
  frame2.set_ntp_time_ms(ntp_time_ms + 1);
  frame2.set_timestamp_us(timestamp_us + 1);
  frame2.set_rotation(kVideoRotation_90);

  EXPECT_NE(frame2.timestamp(), frame1.timestamp());
  EXPECT_NE(frame2.ntp_time_ms(), frame1.ntp_time_ms());
  EXPECT_NE(frame2.timestamp_us(), frame1.timestamp_us());
  EXPECT_NE(frame2.rotation(), frame1.rotation());
}

TEST(TestVideoFrame, TextureInitialValues) {
  VideoFrame frame = test::FakeNativeBuffer::CreateFrame(
      640, 480, 100, 10, webrtc::kVideoRotation_0);
  EXPECT_EQ(640, frame.width());
  EXPECT_EQ(480, frame.height());
  EXPECT_EQ(100u, frame.timestamp());
  EXPECT_EQ(10, frame.render_time_ms());
  ASSERT_TRUE(frame.video_frame_buffer() != nullptr);
  EXPECT_TRUE(frame.video_frame_buffer()->type() ==
              VideoFrameBuffer::Type::kNative);

  frame.set_timestamp(200);
  EXPECT_EQ(200u, frame.timestamp());
  frame.set_timestamp_us(20);
  EXPECT_EQ(20, frame.timestamp_us());
}

TEST(TestI420FrameBuffer, Copy) {
  rtc::scoped_refptr<I420Buffer> buf1(
      I420Buffer::Create(20, 10));
  memset(buf1->MutableDataY(), 1, 200);
  memset(buf1->MutableDataU(), 2, 50);
  memset(buf1->MutableDataV(), 3, 50);
  rtc::scoped_refptr<I420Buffer> buf2 = I420Buffer::Copy(*buf1);
  EXPECT_TRUE(test::FrameBufsEqual(buf1, buf2));
}

TEST(TestI420FrameBuffer, Scale) {
  rtc::scoped_refptr<I420Buffer> buf = CreateGradient(200, 100);

  // Pure scaling, no cropping.
  rtc::scoped_refptr<I420Buffer> scaled_buffer(
      I420Buffer::Create(150, 75));

  scaled_buffer->ScaleFrom(*buf);
  CheckCrop(*scaled_buffer, 0.0, 0.0, 1.0, 1.0);
}

TEST(TestI420FrameBuffer, CropXCenter) {
  rtc::scoped_refptr<I420Buffer> buf = CreateGradient(200, 100);

  // Pure center cropping, no scaling.
  rtc::scoped_refptr<I420Buffer> scaled_buffer(
      I420Buffer::Create(100, 100));

  scaled_buffer->CropAndScaleFrom(*buf, 50, 0, 100, 100);
  CheckCrop(*scaled_buffer, 0.25, 0.0, 0.5, 1.0);
}

TEST(TestI420FrameBuffer, CropXNotCenter) {
  rtc::scoped_refptr<I420Buffer> buf = CreateGradient(200, 100);

  // Non-center cropping, no scaling.
  rtc::scoped_refptr<I420Buffer> scaled_buffer(
      I420Buffer::Create(100, 100));

  scaled_buffer->CropAndScaleFrom(*buf, 25, 0, 100, 100);
  CheckCrop(*scaled_buffer, 0.125, 0.0, 0.5, 1.0);
}

TEST(TestI420FrameBuffer, CropYCenter) {
  rtc::scoped_refptr<I420Buffer> buf = CreateGradient(100, 200);

  // Pure center cropping, no scaling.
  rtc::scoped_refptr<I420Buffer> scaled_buffer(
      I420Buffer::Create(100, 100));

  scaled_buffer->CropAndScaleFrom(*buf, 0, 50, 100, 100);
  CheckCrop(*scaled_buffer, 0.0, 0.25, 1.0, 0.5);
}

TEST(TestI420FrameBuffer, CropYNotCenter) {
  rtc::scoped_refptr<I420Buffer> buf = CreateGradient(100, 200);

  // Non-center cropping, no scaling.
  rtc::scoped_refptr<I420Buffer> scaled_buffer(
      I420Buffer::Create(100, 100));

  scaled_buffer->CropAndScaleFrom(*buf, 0, 25, 100, 100);
  CheckCrop(*scaled_buffer, 0.0, 0.125, 1.0, 0.5);
}

TEST(TestI420FrameBuffer, CropAndScale16x9) {
  rtc::scoped_refptr<I420Buffer> buf = CreateGradient(640, 480);

  // Center crop to 640 x 360 (16/9 aspect), then scale down by 2.
  rtc::scoped_refptr<I420Buffer> scaled_buffer(
      I420Buffer::Create(320, 180));

  scaled_buffer->CropAndScaleFrom(*buf);
  CheckCrop(*scaled_buffer, 0.0, 0.125, 1.0, 0.75);
}

class TestI420BufferRotate
    : public ::testing::TestWithParam<webrtc::VideoRotation> {};

TEST_P(TestI420BufferRotate, Rotates) {
  rtc::scoped_refptr<I420BufferInterface> buffer = CreateGradient(640, 480);
  rtc::scoped_refptr<I420BufferInterface> rotated_buffer =
      I420Buffer::Rotate(*buffer, GetParam());
  CheckRotate(640, 480, GetParam(), *rotated_buffer);
}

INSTANTIATE_TEST_CASE_P(Rotate, TestI420BufferRotate,
                        ::testing::Values(kVideoRotation_0,
                                          kVideoRotation_90,
                                          kVideoRotation_180,
                                          kVideoRotation_270));

}  // namespace webrtc
