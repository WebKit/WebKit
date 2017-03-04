/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "RTCOpenGLDefines.h"

@class RTCVideoFrame;

@protocol RTCShader <NSObject>

- (BOOL)drawFrame:(RTCVideoFrame *)frame;

@end

// Shader for non-native I420 frames.
@interface RTCI420Shader : NSObject <RTCShader>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithContext:(GlContextType *)context NS_DESIGNATED_INITIALIZER;
- (BOOL)drawFrame:(RTCVideoFrame *)frame;

@end

// Native CVPixelBufferRef rendering is only supported on iPhone because it
// depends on CVOpenGLESTextureCacheCreate.
#if TARGET_OS_IPHONE

// Shader for native NV12 frames.
@interface RTCNativeNV12Shader : NSObject <RTCShader>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithContext:(GlContextType *)context NS_DESIGNATED_INITIALIZER;
- (BOOL)drawFrame:(RTCVideoFrame *)frame;

@end

#endif  // TARGET_OS_IPHONE
