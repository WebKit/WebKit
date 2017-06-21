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

#if !TARGET_OS_IPHONE

#import "WebRTC/RTCNSGLVideoView.h"

#import <AppKit/NSOpenGL.h>
#import <CoreVideo/CVDisplayLink.h>
#import <OpenGL/gl3.h>

#import "RTCDefaultShader.h"
#import "RTCI420TextureCache.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrame.h"

@interface RTCNSGLVideoView ()
// |videoFrame| is set when we receive a frame from a worker thread and is read
// from the display link callback so atomicity is required.
@property(atomic, strong) RTCVideoFrame *videoFrame;
@property(atomic, strong) RTCI420TextureCache *i420TextureCache;

- (void)drawFrame;
@end

static CVReturn OnDisplayLinkFired(CVDisplayLinkRef displayLink,
                                   const CVTimeStamp *now,
                                   const CVTimeStamp *outputTime,
                                   CVOptionFlags flagsIn,
                                   CVOptionFlags *flagsOut,
                                   void *displayLinkContext) {
  RTCNSGLVideoView *view = (__bridge RTCNSGLVideoView *)displayLinkContext;
  [view drawFrame];
  return kCVReturnSuccess;
}

@implementation RTCNSGLVideoView {
  CVDisplayLinkRef _displayLink;
  RTCVideoFrame *_lastDrawnFrame;
  id<RTCVideoViewShading> _shader;
}

@synthesize delegate = _delegate;
@synthesize videoFrame = _videoFrame;
@synthesize i420TextureCache = _i420TextureCache;

- (instancetype)initWithFrame:(NSRect)frame pixelFormat:(NSOpenGLPixelFormat *)format {
  return [self initWithFrame:frame pixelFormat:format shader:[[RTCDefaultShader alloc] init]];
}

- (instancetype)initWithFrame:(NSRect)frame
                  pixelFormat:(NSOpenGLPixelFormat *)format
                       shader:(id<RTCVideoViewShading>)shader {
  if (self = [super initWithFrame:frame pixelFormat:format]) {
    _shader = shader;
  }
  return self;
}

- (void)dealloc {
  [self teardownDisplayLink];
}

- (void)drawRect:(NSRect)rect {
  [self drawFrame];
}

- (void)reshape {
  [super reshape];
  NSRect frame = [self frame];
  [self ensureGLContext];
  CGLLockContext([[self openGLContext] CGLContextObj]);
  glViewport(0, 0, frame.size.width, frame.size.height);
  CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

- (void)lockFocus {
  NSOpenGLContext *context = [self openGLContext];
  [super lockFocus];
  if ([context view] != self) {
    [context setView:self];
  }
  [context makeCurrentContext];
}

- (void)prepareOpenGL {
  [super prepareOpenGL];
  [self ensureGLContext];
  glDisable(GL_DITHER);
  [self setupDisplayLink];
}

- (void)clearGLContext {
  [self ensureGLContext];
  self.i420TextureCache = nil;
  [super clearGLContext];
}

#pragma mark - RTCVideoRenderer

// These methods may be called on non-main thread.
- (void)setSize:(CGSize)size {
  dispatch_async(dispatch_get_main_queue(), ^{
    [self.delegate videoView:self didChangeVideoSize:size];
  });
}

- (void)renderFrame:(RTCVideoFrame *)frame {
  self.videoFrame = frame;
}

#pragma mark - Private

- (void)drawFrame {
  RTCVideoFrame *frame = self.videoFrame;
  if (!frame || frame == _lastDrawnFrame) {
    return;
  }
  // This method may be called from CVDisplayLink callback which isn't on the
  // main thread so we have to lock the GL context before drawing.
  NSOpenGLContext *context = [self openGLContext];
  CGLLockContext([context CGLContextObj]);

  [self ensureGLContext];
  glClear(GL_COLOR_BUFFER_BIT);

  // Rendering native CVPixelBuffer is not supported on OS X.
  // TODO(magjed): Add support for NV12 texture cache on OS X.
  frame = [frame newI420VideoFrame];
  if (!self.i420TextureCache) {
    self.i420TextureCache = [[RTCI420TextureCache alloc] initWithContext:context];
  }
  RTCI420TextureCache *i420TextureCache = self.i420TextureCache;
  if (i420TextureCache) {
    [i420TextureCache uploadFrameToTextures:frame];
    [_shader applyShadingForFrameWithWidth:frame.width
                                    height:frame.height
                                  rotation:frame.rotation
                                    yPlane:i420TextureCache.yTexture
                                    uPlane:i420TextureCache.uTexture
                                    vPlane:i420TextureCache.vTexture];
    [context flushBuffer];
    _lastDrawnFrame = frame;
  }
  CGLUnlockContext([context CGLContextObj]);
}

- (void)setupDisplayLink {
  if (_displayLink) {
    return;
  }
  // Synchronize buffer swaps with vertical refresh rate.
  GLint swapInt = 1;
  [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];

  // Create display link.
  CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
  CVDisplayLinkSetOutputCallback(_displayLink,
                                 &OnDisplayLinkFired,
                                 (__bridge void *)self);
  // Set the display link for the current renderer.
  CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
  CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
  CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(
      _displayLink, cglContext, cglPixelFormat);
  CVDisplayLinkStart(_displayLink);
}

- (void)teardownDisplayLink {
  if (!_displayLink) {
    return;
  }
  CVDisplayLinkRelease(_displayLink);
  _displayLink = NULL;
}

- (void)ensureGLContext {
  NSOpenGLContext* context = [self openGLContext];
  NSAssert(context, @"context shouldn't be nil");
  if ([NSOpenGLContext currentContext] != context) {
    [context makeCurrentContext];
  }
}

@end

#endif  // !TARGET_OS_IPHONE
