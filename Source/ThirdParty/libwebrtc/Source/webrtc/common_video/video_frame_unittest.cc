/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame.h"

#include <math.h>
#include <string.h>

#include "api/video/i010_buffer.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/bind.h"
#include "rtc_base/time_utils.h"
#include "test/fake_texture_frame.h"
#include "test/frame_utils.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

// Helper class to delegate calls to appropriate container.
class PlanarYuvBufferFactory {
 public:
  static rtc::scoped_refptr<PlanarYuvBuffer> Create(VideoFrameBuffer::Type type,
                                                    int width,
                                                    int height) {
    switch (type) {
      case VideoFrameBuffer::Type::kI420:
        return I420Buffer::Create(width, height);
      case VideoFrameBuffer::Type::kI010:
        return I010Buffer::Create(width, height);
      default:
        RTC_NOTREACHED();
    }
    return nullptr;
  }

  static rtc::scoped_refptr<PlanarYuvBuffer> Copy(const VideoFrameBuffer& src) {
    switch (src.type()) {
      case VideoFrameBuffer::Type::kI420:
        return I420Buffer::Copy(src);
      case VideoFrameBuffer::Type::kI010:
        return I010Buffer::Copy(*src.GetI010());
      default:
        RTC_NOTREACHED();
    }
    return nullptr;
  }

  static rtc::scoped_refptr<PlanarYuvBuffer> Rotate(const VideoFrameBuffer& src,
                                                    VideoRotation rotation) {
    switch (src.type()) {
      case VideoFrameBuffer::Type::kI420:
        return I420Buffer::Rotate(src, rotation);
      case VideoFrameBuffer::Type::kI010:
        return I010Buffer::Rotate(*src.GetI010(), rotation);
      default:
        RTC_NOTREACHED();
    }
    return nullptr;
  }

  static rtc::scoped_refptr<PlanarYuvBuffer> CropAndScaleFrom(
      const VideoFrameBuffer& src,
      int offset_x,
      int offset_y,
      int crop_width,
      int crop_height) {
    switch (src.type()) {
      case VideoFrameBuffer::Type::kI420: {
        rtc::scoped_refptr<I420Buffer> buffer =
            I420Buffer::Create(crop_width, crop_height);
        buffer->CropAndScaleFrom(*src.GetI420(), offset_x, offset_y, crop_width,
                                 crop_height);
        return buffer;
      }
      case VideoFrameBuffer::Type::kI010: {
        rtc::scoped_refptr<I010Buffer> buffer =
            I010Buffer::Create(crop_width, crop_height);
        buffer->CropAndScaleFrom(*src.GetI010(), offset_x, offset_y, crop_width,
                                 crop_height);
        return buffer;
      }
      default:
        RTC_NOTREACHED();
    }
    return nullptr;
  }

  static rtc::scoped_refptr<PlanarYuvBuffer> CropAndScaleFrom(
      const VideoFrameBuffer& src,
      int crop_width,
      int crop_height) {
    const int out_width =
        std::min(src.width(), crop_width * src.height() / crop_height);
    const int out_height =
        std::min(src.height(), crop_height * src.width() / crop_width);
    return CropAndScaleFrom(src, (src.width() - out_width) / 2,
                            (src.height() - out_height) / 2, out_width,
                            out_height);
  }

  static rtc::scoped_refptr<PlanarYuvBuffer>
  ScaleFrom(const VideoFrameBuffer& src, int crop_width, int crop_height) {
    switch (src.type()) {
      case VideoFrameBuffer::Type::kI420: {
        rtc::scoped_refptr<I420Buffer> buffer =
            I420Buffer::Create(crop_width, crop_height);
        buffer->ScaleFrom(*src.GetI420());
        return buffer;
      }
      case VideoFrameBuffer::Type::kI010: {
        rtc::scoped_refptr<I010Buffer> buffer =
            I010Buffer::Create(crop_width, crop_height);
        buffer->ScaleFrom(*src.GetI010());
        return buffer;
      }
      default:
        RTC_NOTREACHED();
    }
    return nullptr;
  }
};

rtc::scoped_refptr<PlanarYuvBuffer> CreateGradient(VideoFrameBuffer::Type type,
                                                   int width,
                                                   int height) {
  rtc::scoped_refptr<I420Buffer> buffer(I420Buffer::Create(width, height));
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
  if (type == VideoFrameBuffer::Type::kI420)
    return buffer;

  RTC_DCHECK(type == VideoFrameBuffer::Type::kI010);
  return I010Buffer::Copy(*buffer);
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
  const struct {
    int x;
    int y;
  } corners[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  // Corresponding corner colors of the frame produced by CreateGradient.
  const struct {
    int y;
    int u;
    int v;
  } colors[] = {{0, 0, 0}, {127, 255, 0}, {255, 255, 255}, {127, 0, 255}};
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

int GetU(rtc::scoped_refptr<PlanarYuvBuffer> buf, int col, int row) {
  if (buf->type() == VideoFrameBuffer::Type::kI420) {
    return buf->GetI420()
        ->DataU()[row / 2 * buf->GetI420()->StrideU() + col / 2];
  } else {
    return buf->GetI010()
        ->DataU()[row / 2 * buf->GetI010()->StrideU() + col / 2];
  }
}

int GetV(rtc::scoped_refptr<PlanarYuvBuffer> buf, int col, int row) {
  if (buf->type() == VideoFrameBuffer::Type::kI420) {
    return buf->GetI420()
        ->DataV()[row / 2 * buf->GetI420()->StrideV() + col / 2];
  } else {
    return buf->GetI010()
        ->DataV()[row / 2 * buf->GetI010()->StrideV() + col / 2];
  }
}

int GetY(rtc::scoped_refptr<PlanarYuvBuffer> buf, int col, int row) {
  if (buf->type() == VideoFrameBuffer::Type::kI420) {
    return buf->GetI420()->DataY()[row * buf->GetI420()->StrideY() + col];
  } else {
    return buf->GetI010()->DataY()[row * buf->GetI010()->StrideY() + col];
  }
}

void PasteFromBuffer(PlanarYuvBuffer* canvas,
                     const PlanarYuvBuffer& picture,
                     int offset_col,
                     int offset_row) {
  if (canvas->type() == VideoFrameBuffer::Type::kI420) {
    I420Buffer* buf = static_cast<I420Buffer*>(canvas);
    buf->PasteFrom(*picture.GetI420(), offset_col, offset_row);
  } else {
    I010Buffer* buf = static_cast<I010Buffer*>(canvas);
    buf->PasteFrom(*picture.GetI010(), offset_col, offset_row);
  }
}

}  // namespace

TEST(TestVideoFrame, WidthHeightValues) {
  VideoFrame frame =
      VideoFrame::Builder()
          .set_video_frame_buffer(I420Buffer::Create(10, 10, 10, 14, 90))
          .set_rotation(webrtc::kVideoRotation_0)
          .set_timestamp_ms(789)
          .build();
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

  VideoFrame frame1 = VideoFrame::Builder()
                          .set_video_frame_buffer(I420Buffer::Copy(
                              width, height, buffer_y, stride_y, buffer_u,
                              stride_u, buffer_v, stride_v))
                          .set_rotation(kRotation)
                          .set_timestamp_us(0)
                          .build();
  frame1.set_timestamp(timestamp);
  frame1.set_ntp_time_ms(ntp_time_ms);
  frame1.set_timestamp_us(timestamp_us);
  VideoFrame frame2(frame1);

  EXPECT_EQ(frame1.video_frame_buffer(), frame2.video_frame_buffer());
  const webrtc::I420BufferInterface* yuv1 =
      frame1.video_frame_buffer()->GetI420();
  const webrtc::I420BufferInterface* yuv2 =
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

class TestPlanarYuvBuffer
    : public ::testing::TestWithParam<VideoFrameBuffer::Type> {};

rtc::scoped_refptr<I420Buffer> CreateAndFillBuffer() {
  auto buf = I420Buffer::Create(20, 10);
  memset(buf->MutableDataY(), 1, 200);
  memset(buf->MutableDataU(), 2, 50);
  memset(buf->MutableDataV(), 3, 50);
  return buf;
}

TEST_P(TestPlanarYuvBuffer, Copy) {
  rtc::scoped_refptr<PlanarYuvBuffer> buf1;
  switch (GetParam()) {
    case VideoFrameBuffer::Type::kI420: {
      buf1 = CreateAndFillBuffer();
      break;
    }
    case VideoFrameBuffer::Type::kI010: {
      buf1 = I010Buffer::Copy(*CreateAndFillBuffer());
      break;
    }
    default:
      RTC_NOTREACHED();
  }

  rtc::scoped_refptr<PlanarYuvBuffer> buf2 =
      PlanarYuvBufferFactory::Copy(*buf1);
  EXPECT_TRUE(test::FrameBufsEqual(buf1->ToI420(), buf2->ToI420()));
}

TEST_P(TestPlanarYuvBuffer, Scale) {
  rtc::scoped_refptr<PlanarYuvBuffer> buf =
      CreateGradient(GetParam(), 200, 100);

  // Pure scaling, no cropping.
  rtc::scoped_refptr<PlanarYuvBuffer> scaled_buffer =
      PlanarYuvBufferFactory::ScaleFrom(*buf, 150, 75);
  CheckCrop(*scaled_buffer->ToI420(), 0.0, 0.0, 1.0, 1.0);
}

TEST_P(TestPlanarYuvBuffer, CropXCenter) {
  rtc::scoped_refptr<PlanarYuvBuffer> buf =
      CreateGradient(GetParam(), 200, 100);

  // Pure center cropping, no scaling.
  rtc::scoped_refptr<PlanarYuvBuffer> scaled_buffer =
      PlanarYuvBufferFactory::CropAndScaleFrom(*buf, 50, 0, 100, 100);
  CheckCrop(*scaled_buffer->ToI420(), 0.25, 0.0, 0.5, 1.0);
}

TEST_P(TestPlanarYuvBuffer, CropXNotCenter) {
  rtc::scoped_refptr<PlanarYuvBuffer> buf =
      CreateGradient(GetParam(), 200, 100);

  // Non-center cropping, no scaling.
  rtc::scoped_refptr<PlanarYuvBuffer> scaled_buffer =
      PlanarYuvBufferFactory::CropAndScaleFrom(*buf, 25, 0, 100, 100);
  CheckCrop(*scaled_buffer->ToI420(), 0.125, 0.0, 0.5, 1.0);
}

TEST_P(TestPlanarYuvBuffer, CropYCenter) {
  rtc::scoped_refptr<PlanarYuvBuffer> buf =
      CreateGradient(GetParam(), 100, 200);

  // Pure center cropping, no scaling.
  rtc::scoped_refptr<PlanarYuvBuffer> scaled_buffer =
      PlanarYuvBufferFactory::CropAndScaleFrom(*buf, 0, 50, 100, 100);
  CheckCrop(*scaled_buffer->ToI420(), 0.0, 0.25, 1.0, 0.5);
}

TEST_P(TestPlanarYuvBuffer, CropYNotCenter) {
  rtc::scoped_refptr<PlanarYuvBuffer> buf =
      CreateGradient(GetParam(), 100, 200);

  // Pure center cropping, no scaling.
  rtc::scoped_refptr<PlanarYuvBuffer> scaled_buffer =
      PlanarYuvBufferFactory::CropAndScaleFrom(*buf, 0, 25, 100, 100);
  CheckCrop(*scaled_buffer->ToI420(), 0.0, 0.125, 1.0, 0.5);
}

TEST_P(TestPlanarYuvBuffer, CropAndScale16x9) {
  rtc::scoped_refptr<PlanarYuvBuffer> buf =
      CreateGradient(GetParam(), 640, 480);

  // Pure center cropping, no scaling.
  rtc::scoped_refptr<PlanarYuvBuffer> scaled_buffer =
      PlanarYuvBufferFactory::CropAndScaleFrom(*buf, 320, 180);
  CheckCrop(*scaled_buffer->ToI420(), 0.0, 0.125, 1.0, 0.75);
}

TEST_P(TestPlanarYuvBuffer, PastesIntoBuffer) {
  const int kOffsetx = 20;
  const int kOffsety = 30;
  const int kPicSize = 20;
  const int kWidth = 160;
  const int kHeight = 80;
  rtc::scoped_refptr<PlanarYuvBuffer> buf =
      CreateGradient(GetParam(), kWidth, kHeight);

  rtc::scoped_refptr<PlanarYuvBuffer> original =
      CreateGradient(GetParam(), kWidth, kHeight);

  rtc::scoped_refptr<PlanarYuvBuffer> picture =
      CreateGradient(GetParam(), kPicSize, kPicSize);

  rtc::scoped_refptr<PlanarYuvBuffer> odd_picture =
      CreateGradient(GetParam(), kPicSize + 1, kPicSize - 1);

  PasteFromBuffer(buf.get(), *picture, kOffsetx, kOffsety);

  for (int i = 0; i < kWidth; ++i) {
    for (int j = 0; j < kHeight; ++j) {
      bool is_inside = i >= kOffsetx && i < kOffsetx + kPicSize &&
                       j >= kOffsety && j < kOffsety + kPicSize;
      if (!is_inside) {
        EXPECT_EQ(GetU(original, i, j), GetU(buf, i, j));
        EXPECT_EQ(GetV(original, i, j), GetV(buf, i, j));
        EXPECT_EQ(GetY(original, i, j), GetY(buf, i, j));
      } else {
        EXPECT_EQ(GetU(picture, i - kOffsetx, j - kOffsety), GetU(buf, i, j));
        EXPECT_EQ(GetV(picture, i - kOffsetx, j - kOffsety), GetV(buf, i, j));
        EXPECT_EQ(GetY(picture, i - kOffsetx, j - kOffsety), GetY(buf, i, j));
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         TestPlanarYuvBuffer,
                         ::testing::Values(VideoFrameBuffer::Type::kI420,
                                           VideoFrameBuffer::Type::kI010));

class TestPlanarYuvBufferRotate
    : public ::testing::TestWithParam<
          std::tuple<webrtc::VideoRotation, VideoFrameBuffer::Type>> {};

TEST_P(TestPlanarYuvBufferRotate, Rotates) {
  const webrtc::VideoRotation rotation = std::get<0>(GetParam());
  const VideoFrameBuffer::Type type = std::get<1>(GetParam());
  rtc::scoped_refptr<PlanarYuvBuffer> buffer = CreateGradient(type, 640, 480);
  rtc::scoped_refptr<PlanarYuvBuffer> rotated_buffer =
      PlanarYuvBufferFactory::Rotate(*buffer, rotation);
  CheckRotate(640, 480, rotation, *rotated_buffer->ToI420());
}

INSTANTIATE_TEST_SUITE_P(
    Rotate,
    TestPlanarYuvBufferRotate,
    ::testing::Combine(::testing::Values(kVideoRotation_0,
                                         kVideoRotation_90,
                                         kVideoRotation_180,
                                         kVideoRotation_270),
                       ::testing::Values(VideoFrameBuffer::Type::kI420,
                                         VideoFrameBuffer::Type::kI010)));

}  // namespace webrtc
