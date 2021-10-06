/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>

#import "RTCMacros.h"
#import "RTCVideoFrameBuffer.h"

NS_ASSUME_NONNULL_BEGIN

/** RTCVideoFrameBuffer containing a CVPixelBufferRef */
RTC_OBJC_EXPORT
__attribute__((objc_runtime_name("WK_RTCCVPixelBuffer")))
@interface RTCCVPixelBuffer : NSObject <RTCVideoFrameBuffer>

@property(nonatomic, readonly) CVPixelBufferRef pixelBuffer;
@property(nonatomic, readonly) int cropX;
@property(nonatomic, readonly) int cropY;
@property(nonatomic, readonly) int cropWidth;
@property(nonatomic, readonly) int cropHeight;

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
 * If that size is 0, the |tmpBuffer| may be nil.
 */
- (BOOL)cropAndScaleTo:(CVPixelBufferRef)outputPixelBuffer
        withTempBuffer:(nullable uint8_t *)tmpBuffer;

#if defined(WEBRTC_WEBKIT_BUILD)
- (void)close;
#endif
@end

NS_ASSUME_NONNULL_END
