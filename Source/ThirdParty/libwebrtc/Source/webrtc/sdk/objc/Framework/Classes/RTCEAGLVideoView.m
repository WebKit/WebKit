/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCEAGLVideoView.h"

#import <GLKit/GLKit.h>

#import "RTCOpenGLVideoRenderer.h"
#import "WebRTC/RTCVideoFrame.h"

// RTCDisplayLinkTimer wraps a CADisplayLink and is set to fire every two screen
// refreshes, which should be 30fps. We wrap the display link in order to avoid
// a retain cycle since CADisplayLink takes a strong reference onto its target.
// The timer is paused by default.
@interface RTCDisplayLinkTimer : NSObject

@property(nonatomic) BOOL isPaused;

- (instancetype)initWithTimerHandler:(void (^)(void))timerHandler;
- (void)invalidate;

@end

@implementation RTCDisplayLinkTimer {
  CADisplayLink *_displayLink;
  void (^_timerHandler)(void);
}

- (instancetype)initWithTimerHandler:(void (^)(void))timerHandler {
  NSParameterAssert(timerHandler);
  if (self = [super init]) {
    _timerHandler = timerHandler;
    _displayLink =
        [CADisplayLink displayLinkWithTarget:self
                                    selector:@selector(displayLinkDidFire:)];
    _displayLink.paused = YES;
    // Set to half of screen refresh, which should be 30fps.
    [_displayLink setFrameInterval:2];
    [_displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
  return self;
}

- (void)dealloc {
  [self invalidate];
}

- (BOOL)isPaused {
  return _displayLink.paused;
}

- (void)setIsPaused:(BOOL)isPaused {
  _displayLink.paused = isPaused;
}

- (void)invalidate {
  [_displayLink invalidate];
}

- (void)displayLinkDidFire:(CADisplayLink *)displayLink {
  _timerHandler();
}

@end

// RTCEAGLVideoView wraps a GLKView which is setup with
// enableSetNeedsDisplay = NO for the purpose of gaining control of
// exactly when to call -[GLKView display]. This need for extra
// control is required to avoid triggering method calls on GLKView
// that results in attempting to bind the underlying render buffer
// when the drawable size would be empty which would result in the
// error GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT. -[GLKView display] is
// the method that will trigger the binding of the render
// buffer. Because the standard behaviour of -[UIView setNeedsDisplay]
// is disabled for the reasons above, the RTCEAGLVideoView maintains
// its own |isDirty| flag.

@interface RTCEAGLVideoView () <GLKViewDelegate>
// |videoFrame| is set when we receive a frame from a worker thread and is read
// from the display link callback so atomicity is required.
@property(atomic, strong) RTCVideoFrame *videoFrame;
@property(nonatomic, readonly) GLKView *glkView;
@property(nonatomic, readonly) RTCOpenGLVideoRenderer *glRenderer;
@end

@implementation RTCEAGLVideoView {
  RTCDisplayLinkTimer *_timer;
  EAGLContext *_glContext;
  // This flag should only be set and read on the main thread (e.g. by
  // setNeedsDisplay)
  BOOL _isDirty;
}

@synthesize delegate = _delegate;
@synthesize videoFrame = _videoFrame;
@synthesize glkView = _glkView;
@synthesize glRenderer = _glRenderer;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self configure];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder {
  if (self = [super initWithCoder:aDecoder]) {
    [self configure];
  }
  return self;
}

- (void)configure {
  EAGLContext *glContext =
    [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  if (!glContext) {
    glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
  }
  _glContext = glContext;
  _glRenderer = [[RTCOpenGLVideoRenderer alloc] initWithContext:_glContext];

  // GLKView manages a framebuffer for us.
  _glkView = [[GLKView alloc] initWithFrame:CGRectZero
                                    context:_glContext];
  _glkView.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
  _glkView.drawableDepthFormat = GLKViewDrawableDepthFormatNone;
  _glkView.drawableStencilFormat = GLKViewDrawableStencilFormatNone;
  _glkView.drawableMultisample = GLKViewDrawableMultisampleNone;
  _glkView.delegate = self;
  _glkView.layer.masksToBounds = YES;
  _glkView.enableSetNeedsDisplay = NO;
  [self addSubview:_glkView];

  // Listen to application state in order to clean up OpenGL before app goes
  // away.
  NSNotificationCenter *notificationCenter =
    [NSNotificationCenter defaultCenter];
  [notificationCenter addObserver:self
                         selector:@selector(willResignActive)
                             name:UIApplicationWillResignActiveNotification
                           object:nil];
  [notificationCenter addObserver:self
                         selector:@selector(didBecomeActive)
                             name:UIApplicationDidBecomeActiveNotification
                           object:nil];

  // Frames are received on a separate thread, so we poll for current frame
  // using a refresh rate proportional to screen refresh frequency. This
  // occurs on the main thread.
  __weak RTCEAGLVideoView *weakSelf = self;
  _timer = [[RTCDisplayLinkTimer alloc] initWithTimerHandler:^{
      RTCEAGLVideoView *strongSelf = weakSelf;
      [strongSelf displayLinkTimerDidFire];
    }];
  [self setupGL];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  UIApplicationState appState =
      [UIApplication sharedApplication].applicationState;
  if (appState == UIApplicationStateActive) {
    [self teardownGL];
  }
  [_timer invalidate];
  if (_glContext && [EAGLContext currentContext] == _glContext) {
    [EAGLContext setCurrentContext:nil];
  }
}

#pragma mark - UIView

- (void)setNeedsDisplay {
  [super setNeedsDisplay];
  _isDirty = YES;
}

- (void)setNeedsDisplayInRect:(CGRect)rect {
  [super setNeedsDisplayInRect:rect];
  _isDirty = YES;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  _glkView.frame = self.bounds;
}

#pragma mark - GLKViewDelegate

// This method is called when the GLKView's content is dirty and needs to be
// redrawn. This occurs on main thread.
- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect {
  // The renderer will draw the frame to the framebuffer corresponding to the
  // one used by |view|.
  [_glRenderer drawFrame:self.videoFrame];
}

#pragma mark - RTCVideoRenderer

// These methods may be called on non-main thread.
- (void)setSize:(CGSize)size {
  __weak RTCEAGLVideoView *weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    RTCEAGLVideoView *strongSelf = weakSelf;
    [strongSelf.delegate videoView:strongSelf didChangeVideoSize:size];
  });
}

- (void)renderFrame:(RTCVideoFrame *)frame {
#if !TARGET_OS_IPHONE
  // Generate the i420 frame on video send thread instead of main thread.
  // TODO(tkchin): Remove this once RTCEAGLVideoView supports uploading
  // CVPixelBuffer textures on OSX.
  [frame convertBufferIfNeeded];
#endif
  self.videoFrame = frame;
}

#pragma mark - Private

- (void)displayLinkTimerDidFire {
  // Don't render unless video frame have changed or the view content
  // has explicitly been marked dirty.
  if (!_isDirty && _glRenderer.lastDrawnFrame == self.videoFrame) {
    return;
  }

  // Always reset isDirty at this point, even if -[GLKView display]
  // won't be called in the case the drawable size is empty.
  _isDirty = NO;

  // Only call -[GLKView display] if the drawable size is
  // non-empty. Calling display will make the GLKView setup its
  // render buffer if necessary, but that will fail with error
  // GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT if size is empty.
  if (self.bounds.size.width > 0 && self.bounds.size.height > 0) {
    [_glkView display];
  }
}

- (void)setupGL {
  self.videoFrame = nil;
  [_glRenderer setupGL];
  _timer.isPaused = NO;
}

- (void)teardownGL {
  self.videoFrame = nil;
  _timer.isPaused = YES;
  [_glkView deleteDrawable];
  [_glRenderer teardownGL];
}

- (void)didBecomeActive {
  [self setupGL];
}

- (void)willResignActive {
  [self teardownGL];
}

@end
