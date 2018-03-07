/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <WebRTC/RTCMacros.h>

NS_ASSUME_NONNULL_BEGIN

@protocol RTCI420Buffer;

// RTCVideoFrameBuffer is an ObjectiveC version of webrtc::VideoFrameBuffer.
RTC_EXPORT
@protocol RTCVideoFrameBuffer <NSObject>

@property(nonatomic, readonly) int width;
@property(nonatomic, readonly) int height;

- (id<RTCI420Buffer>)toI420;

@end

/** Protocol for RTCVideoFrameBuffers containing YUV planar data. */
@protocol RTCYUVPlanarBuffer <RTCVideoFrameBuffer>

@property(nonatomic, readonly) int chromaWidth;
@property(nonatomic, readonly) int chromaHeight;
@property(nonatomic, readonly) const uint8_t *dataY;
@property(nonatomic, readonly) const uint8_t *dataU;
@property(nonatomic, readonly) const uint8_t *dataV;
@property(nonatomic, readonly) int strideY;
@property(nonatomic, readonly) int strideU;
@property(nonatomic, readonly) int strideV;

- (instancetype)initWithWidth:(int)width height:(int)height;
- (instancetype)initWithWidth:(int)width
                       height:(int)height
                      strideY:(int)strideY
                      strideU:(int)strideU
                      strideV:(int)strideV;

@end

/** Extension of the YUV planar data buffer with mutable data access */
@protocol RTCMutableYUVPlanarBuffer <RTCYUVPlanarBuffer>

@property(nonatomic, readonly) uint8_t *mutableDataY;
@property(nonatomic, readonly) uint8_t *mutableDataU;
@property(nonatomic, readonly) uint8_t *mutableDataV;

@end

/** Protocol for RTCYUVPlanarBuffers containing I420 data */
@protocol RTCI420Buffer <RTCYUVPlanarBuffer>
@end

/** Extension of the I420 buffer with mutable data access */
@protocol RTCMutableI420Buffer <RTCI420Buffer, RTCMutableYUVPlanarBuffer>
@end

/** RTCVideoFrameBuffer containing a CVPixelBufferRef */
RTC_EXPORT
@interface RTCCVPixelBuffer : NSObject <RTCVideoFrameBuffer>

@property(nonatomic, readonly) CVPixelBufferRef pixelBuffer;
@property(nonatomic, readonly) int cropX;
@property(nonatomic, readonly) int cropY;

+ (NSSet<NSNumber *> *)supportedPixelFormats;

- (instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer;
- (instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer
                       adaptedWidth:(int)adaptedWidth
                      adaptedHeight:(int)adaptedHeight
                          cropWidth:(int)cropWidth
                         cropHeight:(int)cropHeight
                              cropX:(int)cropX
                              cropY:(int)cropY;

- (BOOL)requiresCropping;
- (BOOL)requiresScalingToWidth:(int)width height:(int)height;
- (int)bufferSizeForCroppingAndScalingToWidth:(int)width height:(int)height;
/** The minimum size of the |tmpBuffer| must be the number of bytes returned from the
 * bufferSizeForCroppingAndScalingToWidth:height: method.
 */
- (BOOL)cropAndScaleTo:(CVPixelBufferRef)outputPixelBuffer withTempBuffer:(uint8_t *)tmpBuffer;

@end

/** RTCI420Buffer implements the RTCI420Buffer protocol */
RTC_EXPORT
@interface RTCI420Buffer : NSObject <RTCI420Buffer>
@end

/** Mutable version of RTCI420Buffer */
RTC_EXPORT
@interface RTCMutableI420Buffer : RTCI420Buffer <RTCMutableI420Buffer>
@end

NS_ASSUME_NONNULL_END
