/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#if TARGET_OS_IPHONE
#import <GLKit/GLKit.h>
#else
#import <AppKit/NSOpenGL.h>
#endif

#import "WebRTC/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@class RTCVideoFrame;

// RTCOpenGLVideoRenderer issues appropriate OpenGL commands to draw a frame to
// the currently bound framebuffer. Supports OpenGL 3.2 and OpenGLES 2.0. OpenGL
// framebuffer creation and management should be handled elsewhere using the
// same context used to initialize this class.
RTC_EXPORT
@interface RTCOpenGLVideoRenderer : NSObject

// The last successfully drawn frame. Used to avoid drawing frames unnecessarily
// hence saving battery life by reducing load.
@property(nonatomic, readonly) RTCVideoFrame *lastDrawnFrame;

#if TARGET_OS_IPHONE
- (instancetype)initWithContext:(EAGLContext *)context
    NS_DESIGNATED_INITIALIZER;
#else
- (instancetype)initWithContext:(NSOpenGLContext *)context
    NS_DESIGNATED_INITIALIZER;
#endif

// Draws |frame| onto the currently bound OpenGL framebuffer. |setupGL| must be
// called before this function will succeed.
- (BOOL)drawFrame:(RTCVideoFrame *)frame;

// The following methods are used to manage OpenGL resources. On iOS
// applications should release resources when placed in background for use in
// the foreground application. In fact, attempting to call OpenGLES commands
// while in background will result in application termination.

// Sets up the OpenGL state needed for rendering.
- (void)setupGL;
// Tears down the OpenGL state created by |setupGL|.
- (void)teardownGL;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
