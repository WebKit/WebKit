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

#import <CoreVideo/CVDisplayLink.h>
#import <OpenGL/gl3.h>

#import "RTCOpenGLVideoRenderer.h"
#import "WebRTC/RTCVideoFrame.h"

@interface RTCNSGLVideoView ()
// |videoFrame| is set when we receive a frame from a worker thread and is read
// from the display link callback so atomicity is required.
@property(atomic, strong) RTCVideoFrame *videoFrame;
@property(atomic, strong) RTCOpenGLVideoRenderer *glRenderer;
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
}

@synthesize delegate = _delegate;
@synthesize videoFrame = _videoFrame;
@synthesize glRenderer = _glRenderer;

- (void)dealloc {
  [self teardownDisplayLink];
}

- (void)drawRect:(NSRect)rect {
  [self drawFrame];
}

- (void)reshape {
  [super reshape];
  NSRect frame = [self frame];
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
  if (!self.glRenderer) {
    self.glRenderer =
        [[RTCOpenGLVideoRenderer alloc] initWithContext:[self openGLContext]];
  }
  [self.glRenderer setupGL];
  [self setupDisplayLink];
}

- (void)clearGLContext {
  [self.glRenderer teardownGL];
  self.glRenderer = nil;
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
  RTCVideoFrame *videoFrame = self.videoFrame;
  if (self.glRenderer.lastDrawnFrame != videoFrame) {
    // This method may be called from CVDisplayLink callback which isn't on the
    // main thread so we have to lock the GL context before drawing.
    CGLLockContext([[self openGLContext] CGLContextObj]);
    [self.glRenderer drawFrame:videoFrame];
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
  }
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

@end

#endif  // !TARGET_OS_IPHONE
