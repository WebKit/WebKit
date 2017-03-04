/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCOpenGLVideoRenderer.h"

#import "RTCShader+Private.h"
#import "WebRTC/RTCVideoFrame.h"

@implementation RTCOpenGLVideoRenderer {
  GlContextType *_context;
  BOOL _isInitialized;
  id<RTCShader> _i420Shader;
  id<RTCShader> _nv12Shader;
}

@synthesize lastDrawnFrame = _lastDrawnFrame;

+ (void)initialize {
  // Disable dithering for performance.
  glDisable(GL_DITHER);
}

- (instancetype)initWithContext:(GlContextType *)context {
  NSAssert(context != nil, @"context cannot be nil");
  if (self = [super init]) {
    _context = context;
  }
  return self;
}

- (BOOL)drawFrame:(RTCVideoFrame *)frame {
  if (!_isInitialized || !frame || frame == _lastDrawnFrame) {
    return NO;
  }
  [self ensureGLContext];
  glClear(GL_COLOR_BUFFER_BIT);
  id<RTCShader> shader = nil;
#if TARGET_OS_IPHONE
  if (frame.nativeHandle) {
    if (!_nv12Shader) {
      _nv12Shader = [[RTCNativeNV12Shader alloc] initWithContext:_context];
    }
    shader = _nv12Shader;
  } else {
    if (!_i420Shader) {
      _i420Shader = [[RTCI420Shader alloc] initWithContext:_context];
    }
    shader = _i420Shader;
  }
#else
  // Rendering native CVPixelBuffer is not supported on OS X.
  frame = [frame newI420VideoFrame];
  if (!_i420Shader) {
    _i420Shader = [[RTCI420Shader alloc] initWithContext:_context];
  }
  shader = _i420Shader;
#endif
  if (!shader || ![shader drawFrame:frame]) {
    return NO;
  }

#if !TARGET_OS_IPHONE
  [_context flushBuffer];
#endif
  _lastDrawnFrame = frame;

  return YES;
}

- (void)setupGL {
  if (_isInitialized) {
    return;
  }
  [self ensureGLContext];
  _isInitialized = YES;
}

- (void)teardownGL {
  if (!_isInitialized) {
    return;
  }
  [self ensureGLContext];
  _i420Shader = nil;
  _nv12Shader = nil;
  _isInitialized = NO;
}

#pragma mark - Private

- (void)ensureGLContext {
  NSAssert(_context, @"context shouldn't be nil");
#if TARGET_OS_IPHONE
  if ([EAGLContext currentContext] != _context) {
    [EAGLContext setCurrentContext:_context];
  }
#else
  if ([NSOpenGLContext currentContext] != _context) {
    [_context makeCurrentContext];
  }
#endif
}

@end
