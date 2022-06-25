/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/unittests/frame_buffer_helpers.h"

#include "third_party/libyuv/include/libyuv.h"

void DrawGradientInRGBPixelBuffer(CVPixelBufferRef pixelBuffer) {
  CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
  void* baseAddr = CVPixelBufferGetBaseAddress(pixelBuffer);
  size_t width = CVPixelBufferGetWidth(pixelBuffer);
  size_t height = CVPixelBufferGetHeight(pixelBuffer);
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  int byteOrder = CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_32ARGB ?
      kCGBitmapByteOrder32Little :
      0;
  CGContextRef cgContext = CGBitmapContextCreate(baseAddr,
                                                 width,
                                                 height,
                                                 8,
                                                 CVPixelBufferGetBytesPerRow(pixelBuffer),
                                                 colorSpace,
                                                 byteOrder | kCGImageAlphaNoneSkipLast);

  // Create a gradient
  CGFloat colors[] = {
      1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0,
  };
  CGGradientRef gradient = CGGradientCreateWithColorComponents(colorSpace, colors, NULL, 4);

  CGContextDrawLinearGradient(
      cgContext, gradient, CGPointMake(0, 0), CGPointMake(width, height), 0);
  CGGradientRelease(gradient);

  CGImageRef cgImage = CGBitmapContextCreateImage(cgContext);
  CGContextRelease(cgContext);
  CGImageRelease(cgImage);
  CGColorSpaceRelease(colorSpace);

  CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}

rtc::scoped_refptr<webrtc::I420Buffer> CreateI420Gradient(int width, int height) {
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(webrtc::I420Buffer::Create(width, height));
  // Initialize with gradient, Y = 128(x/w + y/h), U = 256 x/w, V = 256 y/h
  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      buffer->MutableDataY()[x + y * width] = 128 * (x * height + y * width) / (width * height);
    }
  }
  int chroma_width = buffer->ChromaWidth();
  int chroma_height = buffer->ChromaHeight();
  for (int x = 0; x < chroma_width; x++) {
    for (int y = 0; y < chroma_height; y++) {
      buffer->MutableDataU()[x + y * chroma_width] = 255 * x / (chroma_width - 1);
      buffer->MutableDataV()[x + y * chroma_width] = 255 * y / (chroma_height - 1);
    }
  }
  return buffer;
}

void CopyI420BufferToCVPixelBuffer(rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer,
                                   CVPixelBufferRef pixelBuffer) {
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);

  const OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
  if (pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
      pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
    // NV12
    uint8_t* dstY = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
    const int dstYStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    uint8_t* dstUV = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
    const int dstUVStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

    libyuv::I420ToNV12(i420Buffer->DataY(),
                       i420Buffer->StrideY(),
                       i420Buffer->DataU(),
                       i420Buffer->StrideU(),
                       i420Buffer->DataV(),
                       i420Buffer->StrideV(),
                       dstY,
                       dstYStride,
                       dstUV,
                       dstUVStride,
                       i420Buffer->width(),
                       i420Buffer->height());
  } else {
    uint8_t* dst = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));
    const int bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);

    if (pixelFormat == kCVPixelFormatType_32BGRA) {
      // Corresponds to libyuv::FOURCC_ARGB
      libyuv::I420ToARGB(i420Buffer->DataY(),
                         i420Buffer->StrideY(),
                         i420Buffer->DataU(),
                         i420Buffer->StrideU(),
                         i420Buffer->DataV(),
                         i420Buffer->StrideV(),
                         dst,
                         bytesPerRow,
                         i420Buffer->width(),
                         i420Buffer->height());
    } else if (pixelFormat == kCVPixelFormatType_32ARGB) {
      // Corresponds to libyuv::FOURCC_BGRA
      libyuv::I420ToBGRA(i420Buffer->DataY(),
                         i420Buffer->StrideY(),
                         i420Buffer->DataU(),
                         i420Buffer->StrideU(),
                         i420Buffer->DataV(),
                         i420Buffer->StrideV(),
                         dst,
                         bytesPerRow,
                         i420Buffer->width(),
                         i420Buffer->height());
    }
  }

  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
}
