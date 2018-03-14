/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoFrameBuffer.h"

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#include "third_party/libyuv/include/libyuv.h"

@implementation RTCCVPixelBuffer {
  int _width;
  int _height;
  int _bufferWidth;
  int _bufferHeight;
  int _cropWidth;
  int _cropHeight;
}

@synthesize pixelBuffer = _pixelBuffer;
@synthesize cropX = _cropX;
@synthesize cropY = _cropY;

+ (NSSet<NSNumber*>*)supportedPixelFormats {
  return [NSSet setWithObjects:@(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange),
                               @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
                               @(kCVPixelFormatType_32BGRA),
                               @(kCVPixelFormatType_32ARGB),
                               nil];
}

- (instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  return [self initWithPixelBuffer:pixelBuffer
                      adaptedWidth:CVPixelBufferGetWidth(pixelBuffer)
                     adaptedHeight:CVPixelBufferGetHeight(pixelBuffer)
                         cropWidth:CVPixelBufferGetWidth(pixelBuffer)
                        cropHeight:CVPixelBufferGetHeight(pixelBuffer)
                             cropX:0
                             cropY:0];
}

- (instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer
                       adaptedWidth:(int)adaptedWidth
                      adaptedHeight:(int)adaptedHeight
                          cropWidth:(int)cropWidth
                         cropHeight:(int)cropHeight
                              cropX:(int)cropX
                              cropY:(int)cropY {
  if (self = [super init]) {
    _width = adaptedWidth;
    _height = adaptedHeight;
    _pixelBuffer = pixelBuffer;
    _bufferWidth = CVPixelBufferGetWidth(_pixelBuffer);
    _bufferHeight = CVPixelBufferGetHeight(_pixelBuffer);
    _cropWidth = cropWidth;
    _cropHeight = cropHeight;
    // Can only crop at even pixels.
    _cropX = cropX & ~1;
    _cropY = cropY & ~1;
    CVBufferRetain(_pixelBuffer);
  }

  return self;
}

- (void)dealloc {
  CVBufferRelease(_pixelBuffer);
}

- (int)width {
  return _width;
}

- (int)height {
  return _height;
}

- (BOOL)requiresCropping {
  return _cropWidth != _bufferWidth || _cropHeight != _bufferHeight;
}

- (BOOL)requiresScalingToWidth:(int)width height:(int)height {
  return _cropWidth != width || _cropHeight != height;
}

- (int)bufferSizeForCroppingAndScalingToWidth:(int)width height:(int)height {
  const OSType srcPixelFormat = CVPixelBufferGetPixelFormatType(_pixelBuffer);
  switch (srcPixelFormat) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
      int srcChromaWidth = (_cropWidth + 1) / 2;
      int srcChromaHeight = (_cropHeight + 1) / 2;
      int dstChromaWidth = (width + 1) / 2;
      int dstChromaHeight = (height + 1) / 2;

      return srcChromaWidth * srcChromaHeight * 2 + dstChromaWidth * dstChromaHeight * 2;
    }
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32ARGB: {
      return 0;  // Scaling RGBA frames does not require a temporary buffer.
    }
  }
  RTC_NOTREACHED() << "Unsupported pixel format.";
  return 0;
}

- (BOOL)cropAndScaleTo:(CVPixelBufferRef)outputPixelBuffer withTempBuffer:(uint8_t*)tmpBuffer {
  const OSType srcPixelFormat = CVPixelBufferGetPixelFormatType(_pixelBuffer);
  switch (srcPixelFormat) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
      [self cropAndScaleNV12To:outputPixelBuffer withTempBuffer:tmpBuffer];
      break;
    }
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32ARGB: {
      [self cropAndScaleARGBTo:outputPixelBuffer];
      break;
    }
    default: { RTC_NOTREACHED() << "Unsupported pixel format."; }
  }

  return YES;
}

- (id<RTCI420Buffer>)toI420 {
  const OSType pixelFormat = CVPixelBufferGetPixelFormatType(_pixelBuffer);

  CVPixelBufferLockBaseAddress(_pixelBuffer, kCVPixelBufferLock_ReadOnly);

  RTCMutableI420Buffer* i420Buffer =
      [[RTCMutableI420Buffer alloc] initWithWidth:[self width] height:[self height]];

  switch (pixelFormat) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
      const uint8_t* srcY =
          static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(_pixelBuffer, 0));
      const int srcYStride = CVPixelBufferGetBytesPerRowOfPlane(_pixelBuffer, 0);
      const uint8_t* srcUV =
          static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(_pixelBuffer, 1));
      const int srcUVStride = CVPixelBufferGetBytesPerRowOfPlane(_pixelBuffer, 1);

      // Crop just by modifying pointers.
      srcY += srcYStride * _cropY + _cropX;
      srcUV += srcUVStride * (_cropY / 2) + _cropX;

      // TODO(magjed): Use a frame buffer pool.
      webrtc::NV12ToI420Scaler nv12ToI420Scaler;
      nv12ToI420Scaler.NV12ToI420Scale(srcY,
                                       srcYStride,
                                       srcUV,
                                       srcUVStride,
                                       _cropWidth,
                                       _cropHeight,
                                       i420Buffer.mutableDataY,
                                       i420Buffer.strideY,
                                       i420Buffer.mutableDataU,
                                       i420Buffer.strideU,
                                       i420Buffer.mutableDataV,
                                       i420Buffer.strideV,
                                       i420Buffer.width,
                                       i420Buffer.height);
      break;
    }
    case kCVPixelFormatType_32BGRA:
    case kCVPixelFormatType_32ARGB: {
      const uint8_t* src =
          static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(_pixelBuffer, 0));

      uint32 libyuvPixelFormat = 0;
      if (pixelFormat == kCVPixelFormatType_32BGRA) {
        libyuvPixelFormat = libyuv::FOURCC_ARGB;
      } else if (pixelFormat == kCVPixelFormatType_32ARGB) {
        libyuvPixelFormat = libyuv::FOURCC_ABGR;
      }

      libyuv::ConvertToI420(src,
                            0,
                            i420Buffer.mutableDataY,
                            i420Buffer.strideY,
                            i420Buffer.mutableDataU,
                            i420Buffer.strideU,
                            i420Buffer.mutableDataV,
                            i420Buffer.strideV,
                            _cropX,
                            _cropY,
                            _cropWidth,
                            _cropHeight,
                            i420Buffer.width,
                            i420Buffer.height,
                            libyuv::kRotate0,
                            libyuvPixelFormat);
      break;
    }
    default: { RTC_NOTREACHED() << "Unsupported pixel format."; }
  }

  CVPixelBufferUnlockBaseAddress(_pixelBuffer, kCVPixelBufferLock_ReadOnly);

  return i420Buffer;
}

- (void)cropAndScaleNV12To:(CVPixelBufferRef)outputPixelBuffer withTempBuffer:(uint8_t*)tmpBuffer {
  // Prepare output pointers.
  CVReturn cvRet = CVPixelBufferLockBaseAddress(outputPixelBuffer, 0);
  if (cvRet != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to lock base address: " << cvRet;
  }
  const int dstWidth = CVPixelBufferGetWidth(outputPixelBuffer);
  const int dstHeight = CVPixelBufferGetHeight(outputPixelBuffer);
  uint8_t* dstY =
      reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(outputPixelBuffer, 0));
  const int dstYStride = CVPixelBufferGetBytesPerRowOfPlane(outputPixelBuffer, 0);
  uint8_t* dstUV =
      reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(outputPixelBuffer, 1));
  const int dstUVStride = CVPixelBufferGetBytesPerRowOfPlane(outputPixelBuffer, 1);

  // Prepare source pointers.
  CVPixelBufferLockBaseAddress(_pixelBuffer, kCVPixelBufferLock_ReadOnly);
  const uint8_t* srcY =
      static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(_pixelBuffer, 0));
  const int srcYStride = CVPixelBufferGetBytesPerRowOfPlane(_pixelBuffer, 0);
  const uint8_t* srcUV =
      static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(_pixelBuffer, 1));
  const int srcUVStride = CVPixelBufferGetBytesPerRowOfPlane(_pixelBuffer, 1);

  // Crop just by modifying pointers.
  srcY += srcYStride * _cropY + _cropX;
  srcUV += srcUVStride * (_cropY / 2) + _cropX;

  webrtc::NV12Scale(tmpBuffer,
                    srcY,
                    srcYStride,
                    srcUV,
                    srcUVStride,
                    _cropWidth,
                    _cropHeight,
                    dstY,
                    dstYStride,
                    dstUV,
                    dstUVStride,
                    dstWidth,
                    dstHeight);

  CVPixelBufferUnlockBaseAddress(_pixelBuffer, kCVPixelBufferLock_ReadOnly);
  CVPixelBufferUnlockBaseAddress(outputPixelBuffer, 0);
}

- (void)cropAndScaleARGBTo:(CVPixelBufferRef)outputPixelBuffer {
  // Prepare output pointers.
  CVReturn cvRet = CVPixelBufferLockBaseAddress(outputPixelBuffer, 0);
  if (cvRet != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to lock base address: " << cvRet;
  }
  const int dstWidth = CVPixelBufferGetWidth(outputPixelBuffer);
  const int dstHeight = CVPixelBufferGetHeight(outputPixelBuffer);

  uint8_t* dst =
      reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(outputPixelBuffer, 0));
  const int dstStride = CVPixelBufferGetBytesPerRowOfPlane(outputPixelBuffer, 0);

  // Prepare source pointers.
  CVPixelBufferLockBaseAddress(_pixelBuffer, kCVPixelBufferLock_ReadOnly);
  const uint8_t* src = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(_pixelBuffer));
  const int srcStride = CVPixelBufferGetBytesPerRow(_pixelBuffer);

  // Crop just by modifying pointers.
  src += srcStride * _cropY + _cropX;
  libyuv::ARGBScale(src,
                    srcStride,
                    _cropWidth,
                    _cropHeight,
                    dst,
                    dstStride,
                    dstWidth,
                    dstHeight,
                    libyuv::kFilterBox);

  CVPixelBufferUnlockBaseAddress(_pixelBuffer, kCVPixelBufferLock_ReadOnly);
  CVPixelBufferUnlockBaseAddress(outputPixelBuffer, 0);
}

@end
