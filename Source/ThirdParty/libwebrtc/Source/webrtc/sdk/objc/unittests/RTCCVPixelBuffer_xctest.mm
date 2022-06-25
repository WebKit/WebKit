/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#import "api/video_frame_buffer/RTCNativeI420Buffer+Private.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "frame_buffer_helpers.h"

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "third_party/libyuv/include/libyuv.h"

@interface RTCCVPixelBufferTests : XCTestCase
@end

@implementation RTCCVPixelBufferTests {
}

- (void)testRequiresCroppingNoCrop {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(
      NULL, 720, 1280, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, NULL, &pixelBufferRef);
  RTCCVPixelBuffer *buffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef];

  XCTAssertFalse([buffer requiresCropping]);

  CVBufferRelease(pixelBufferRef);
}

- (void)testRequiresCroppingWithCrop {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(
      NULL, 720, 1280, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, NULL, &pixelBufferRef);
  RTCCVPixelBuffer *croppedBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef
                                                                     adaptedWidth:720
                                                                    adaptedHeight:1280
                                                                        cropWidth:360
                                                                       cropHeight:640
                                                                            cropX:100
                                                                            cropY:100];

  XCTAssertTrue([croppedBuffer requiresCropping]);

  CVBufferRelease(pixelBufferRef);
}

- (void)testRequiresScalingNoScale {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(
      NULL, 720, 1280, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, NULL, &pixelBufferRef);

  RTCCVPixelBuffer *buffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef];
  XCTAssertFalse([buffer requiresScalingToWidth:720 height:1280]);

  CVBufferRelease(pixelBufferRef);
}

- (void)testRequiresScalingWithScale {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(
      NULL, 720, 1280, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, NULL, &pixelBufferRef);

  RTCCVPixelBuffer *buffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef];
  XCTAssertTrue([buffer requiresScalingToWidth:360 height:640]);

  CVBufferRelease(pixelBufferRef);
}

- (void)testRequiresScalingWithScaleAndMatchingCrop {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(
      NULL, 720, 1280, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, NULL, &pixelBufferRef);

  RTCCVPixelBuffer *buffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef
                                                              adaptedWidth:720
                                                             adaptedHeight:1280
                                                                 cropWidth:360
                                                                cropHeight:640
                                                                     cropX:100
                                                                     cropY:100];
  XCTAssertFalse([buffer requiresScalingToWidth:360 height:640]);

  CVBufferRelease(pixelBufferRef);
}

- (void)testBufferSize_NV12 {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(
      NULL, 720, 1280, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, NULL, &pixelBufferRef);

  RTCCVPixelBuffer *buffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef];
  XCTAssertEqual([buffer bufferSizeForCroppingAndScalingToWidth:360 height:640], 576000);

  CVBufferRelease(pixelBufferRef);
}

- (void)testBufferSize_RGB {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(NULL, 720, 1280, kCVPixelFormatType_32BGRA, NULL, &pixelBufferRef);

  RTCCVPixelBuffer *buffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef];
  XCTAssertEqual([buffer bufferSizeForCroppingAndScalingToWidth:360 height:640], 0);

  CVBufferRelease(pixelBufferRef);
}

- (void)testCropAndScale_NV12 {
  [self cropAndScaleTestWithNV12];
}

- (void)testCropAndScaleNoOp_NV12 {
  [self cropAndScaleTestWithNV12InputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                               outputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                                 outputSize:CGSizeMake(720, 1280)];
}

- (void)testCropAndScale_NV12FullToVideo {
  [self cropAndScaleTestWithNV12InputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                               outputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange];
}

- (void)testCropAndScaleZeroSizeFrame_NV12 {
  [self cropAndScaleTestWithNV12InputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                               outputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                                 outputSize:CGSizeMake(0, 0)];
}

- (void)testCropAndScaleToSmallFormat_NV12 {
  [self cropAndScaleTestWithNV12InputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                               outputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                                 outputSize:CGSizeMake(148, 320)];
}

- (void)testCropAndScaleToOddFormat_NV12 {
  [self cropAndScaleTestWithNV12InputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                               outputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                                 outputSize:CGSizeMake(361, 640)];
}

- (void)testCropAndScale_32BGRA {
  [self cropAndScaleTestWithRGBPixelFormat:kCVPixelFormatType_32BGRA];
}

- (void)testCropAndScale_32ARGB {
  [self cropAndScaleTestWithRGBPixelFormat:kCVPixelFormatType_32ARGB];
}

- (void)testCropAndScaleWithSmallCropInfo_32ARGB {
  [self cropAndScaleTestWithRGBPixelFormat:kCVPixelFormatType_32ARGB cropX:2 cropY:3];
}

- (void)testCropAndScaleWithLargeCropInfo_32ARGB {
  [self cropAndScaleTestWithRGBPixelFormat:kCVPixelFormatType_32ARGB cropX:200 cropY:300];
}

- (void)testToI420_NV12 {
  [self toI420WithPixelFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange];
}

- (void)testToI420_32BGRA {
  [self toI420WithPixelFormat:kCVPixelFormatType_32BGRA];
}

- (void)testToI420_32ARGB {
  [self toI420WithPixelFormat:kCVPixelFormatType_32ARGB];
}

#pragma mark - Shared test code

- (void)cropAndScaleTestWithNV12 {
  [self cropAndScaleTestWithNV12InputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                               outputFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange];
}

- (void)cropAndScaleTestWithNV12InputFormat:(OSType)inputFormat outputFormat:(OSType)outputFormat {
  [self cropAndScaleTestWithNV12InputFormat:(OSType)inputFormat
                               outputFormat:(OSType)outputFormat
                                 outputSize:CGSizeMake(360, 640)];
}

- (void)cropAndScaleTestWithNV12InputFormat:(OSType)inputFormat
                               outputFormat:(OSType)outputFormat
                                 outputSize:(CGSize)outputSize {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(NULL, 720, 1280, inputFormat, NULL, &pixelBufferRef);

  rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer = CreateI420Gradient(720, 1280);
  CopyI420BufferToCVPixelBuffer(i420Buffer, pixelBufferRef);

  RTCCVPixelBuffer *buffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef];
  XCTAssertEqual(buffer.width, 720);
  XCTAssertEqual(buffer.height, 1280);

  CVPixelBufferRef outputPixelBufferRef = NULL;
  CVPixelBufferCreate(
      NULL, outputSize.width, outputSize.height, outputFormat, NULL, &outputPixelBufferRef);

  std::vector<uint8_t> frameScaleBuffer;
  if ([buffer requiresScalingToWidth:outputSize.width height:outputSize.height]) {
    int size =
        [buffer bufferSizeForCroppingAndScalingToWidth:outputSize.width height:outputSize.height];
    frameScaleBuffer.resize(size);
  } else {
    frameScaleBuffer.clear();
  }
  frameScaleBuffer.shrink_to_fit();

  [buffer cropAndScaleTo:outputPixelBufferRef withTempBuffer:frameScaleBuffer.data()];

  RTCCVPixelBuffer *scaledBuffer =
      [[RTCCVPixelBuffer alloc] initWithPixelBuffer:outputPixelBufferRef];
  XCTAssertEqual(scaledBuffer.width, outputSize.width);
  XCTAssertEqual(scaledBuffer.height, outputSize.height);

  if (outputSize.width > 0 && outputSize.height > 0) {
    RTCI420Buffer *originalBufferI420 = [buffer toI420];
    RTCI420Buffer *scaledBufferI420 = [scaledBuffer toI420];
    double psnr =
        I420PSNR(*[originalBufferI420 nativeI420Buffer], *[scaledBufferI420 nativeI420Buffer]);
    XCTAssertEqual(psnr, webrtc::kPerfectPSNR);
  }

  CVBufferRelease(pixelBufferRef);
}

- (void)cropAndScaleTestWithRGBPixelFormat:(OSType)pixelFormat {
  [self cropAndScaleTestWithRGBPixelFormat:pixelFormat cropX:0 cropY:0];
}

- (void)cropAndScaleTestWithRGBPixelFormat:(OSType)pixelFormat cropX:(int)cropX cropY:(int)cropY {
  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(NULL, 720, 1280, pixelFormat, NULL, &pixelBufferRef);

  DrawGradientInRGBPixelBuffer(pixelBufferRef);

  RTCCVPixelBuffer *buffer =
      [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef
                                       adaptedWidth:CVPixelBufferGetWidth(pixelBufferRef)
                                      adaptedHeight:CVPixelBufferGetHeight(pixelBufferRef)
                                          cropWidth:CVPixelBufferGetWidth(pixelBufferRef) - cropX
                                         cropHeight:CVPixelBufferGetHeight(pixelBufferRef) - cropY
                                              cropX:cropX
                                              cropY:cropY];

  XCTAssertEqual(buffer.width, 720);
  XCTAssertEqual(buffer.height, 1280);

  CVPixelBufferRef outputPixelBufferRef = NULL;
  CVPixelBufferCreate(NULL, 360, 640, pixelFormat, NULL, &outputPixelBufferRef);
  [buffer cropAndScaleTo:outputPixelBufferRef withTempBuffer:NULL];

  RTCCVPixelBuffer *scaledBuffer =
      [[RTCCVPixelBuffer alloc] initWithPixelBuffer:outputPixelBufferRef];
  XCTAssertEqual(scaledBuffer.width, 360);
  XCTAssertEqual(scaledBuffer.height, 640);

  RTCI420Buffer *originalBufferI420 = [buffer toI420];
  RTCI420Buffer *scaledBufferI420 = [scaledBuffer toI420];
  double psnr =
      I420PSNR(*[originalBufferI420 nativeI420Buffer], *[scaledBufferI420 nativeI420Buffer]);
  XCTAssertEqual(psnr, webrtc::kPerfectPSNR);

  CVBufferRelease(pixelBufferRef);
}

- (void)toI420WithPixelFormat:(OSType)pixelFormat {
  rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer = CreateI420Gradient(360, 640);

  CVPixelBufferRef pixelBufferRef = NULL;
  CVPixelBufferCreate(NULL, 360, 640, pixelFormat, NULL, &pixelBufferRef);

  CopyI420BufferToCVPixelBuffer(i420Buffer, pixelBufferRef);

  RTCCVPixelBuffer *buffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBufferRef];
  RTCI420Buffer *fromCVPixelBuffer = [buffer toI420];

  double psnr = I420PSNR(*i420Buffer, *[fromCVPixelBuffer nativeI420Buffer]);
  double target = webrtc::kPerfectPSNR;
  if (pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) {
    // libyuv's I420ToRGB functions seem to lose some quality.
    target = 19.0;
  }
  XCTAssertGreaterThanOrEqual(psnr, target);

  CVBufferRelease(pixelBufferRef);
}

@end
