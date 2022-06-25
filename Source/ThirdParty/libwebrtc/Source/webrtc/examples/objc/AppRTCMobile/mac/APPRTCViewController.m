/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "APPRTCViewController.h"

#import <AVFoundation/AVFoundation.h>

#import "sdk/objc/api/peerconnection/RTCVideoTrack.h"
#import "sdk/objc/components/renderer/metal/RTCMTLNSVideoView.h"
#import "sdk/objc/components/renderer/opengl/RTCNSGLVideoView.h"

#import "ARDAppClient.h"
#import "ARDCaptureController.h"
#import "ARDSettingsModel.h"

static NSUInteger const kContentWidth = 900;
static NSUInteger const kRoomFieldWidth = 200;
static NSUInteger const kActionItemHeight = 30;
static NSUInteger const kBottomViewHeight = 200;

@class APPRTCMainView;
@protocol APPRTCMainViewDelegate

- (void)appRTCMainView:(APPRTCMainView*)mainView
        didEnterRoomId:(NSString*)roomId
              loopback:(BOOL)isLoopback;

@end

@interface APPRTCMainView : NSView

@property(nonatomic, weak) id<APPRTCMainViewDelegate> delegate;
@property(nonatomic, readonly) NSView<RTC_OBJC_TYPE(RTCVideoRenderer)>* localVideoView;
@property(nonatomic, readonly) NSView<RTC_OBJC_TYPE(RTCVideoRenderer)>* remoteVideoView;
@property(nonatomic, readonly) NSTextView* logView;

- (void)displayLogMessage:(NSString*)message;

@end

@interface APPRTCMainView () <NSTextFieldDelegate, RTC_OBJC_TYPE (RTCNSGLVideoViewDelegate)>
@end
@implementation APPRTCMainView  {
  NSScrollView* _scrollView;
  NSView* _actionItemsView;
  NSButton* _connectButton;
  NSButton* _loopbackButton;
  NSTextField* _roomField;
  CGSize _localVideoSize;
  CGSize _remoteVideoSize;
}

@synthesize delegate = _delegate;
@synthesize localVideoView = _localVideoView;
@synthesize remoteVideoView = _remoteVideoView;
@synthesize logView = _logView;

- (void)displayLogMessage:(NSString *)message {
  dispatch_async(dispatch_get_main_queue(), ^{
    self.logView.string = [NSString stringWithFormat:@"%@%@\n", self.logView.string, message];
    NSRange range = NSMakeRange(self.logView.string.length, 0);
    [self.logView scrollRangeToVisible:range];
  });
}

#pragma mark - Private

- (instancetype)initWithFrame:(NSRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self setupViews];
  }
  return self;
}

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

- (void)updateConstraints {
  NSParameterAssert(
      _roomField != nil &&
      _scrollView != nil &&
      _remoteVideoView != nil &&
      _localVideoView != nil &&
      _actionItemsView!= nil &&
      _connectButton != nil &&
      _loopbackButton != nil);

  [self removeConstraints:[self constraints]];
  NSDictionary* viewsDictionary =
      NSDictionaryOfVariableBindings(_roomField,
                                     _scrollView,
                                     _remoteVideoView,
                                     _localVideoView,
                                     _actionItemsView,
                                     _connectButton,
                                     _loopbackButton);

  NSSize remoteViewSize = [self remoteVideoViewSize];
  NSDictionary* metrics = @{
    @"remoteViewWidth" : @(remoteViewSize.width),
    @"remoteViewHeight" : @(remoteViewSize.height),
    @"kBottomViewHeight" : @(kBottomViewHeight),
    @"localViewHeight" : @(remoteViewSize.height / 3),
    @"localViewWidth" : @(remoteViewSize.width / 3),
    @"kRoomFieldWidth" : @(kRoomFieldWidth),
    @"kActionItemHeight" : @(kActionItemHeight)
  };
  // Declare this separately to avoid compiler warning about splitting string
  // within an NSArray expression.
  NSString* verticalConstraintLeft =
      @"V:|-[_remoteVideoView(remoteViewHeight)]-[_scrollView(kBottomViewHeight)]-|";
  NSString* verticalConstraintRight =
      @"V:|-[_remoteVideoView(remoteViewHeight)]-[_actionItemsView(kBottomViewHeight)]-|";
  NSArray* constraintFormats = @[
      verticalConstraintLeft,
      verticalConstraintRight,
      @"H:|-[_remoteVideoView(remoteViewWidth)]-|",
      @"V:|-[_localVideoView(localViewHeight)]",
      @"H:|-[_localVideoView(localViewWidth)]",
      @"H:|-[_scrollView(==_actionItemsView)]-[_actionItemsView]-|"
  ];

  NSArray* actionItemsConstraints = @[
      @"H:|-[_roomField(kRoomFieldWidth)]-[_loopbackButton(kRoomFieldWidth)]",
      @"H:|-[_connectButton(kRoomFieldWidth)]",
      @"V:|-[_roomField(kActionItemHeight)]-[_connectButton(kActionItemHeight)]",
      @"V:|-[_loopbackButton(kActionItemHeight)]",
      ];

  [APPRTCMainView addConstraints:constraintFormats
                          toView:self
                 viewsDictionary:viewsDictionary
                         metrics:metrics];
  [APPRTCMainView addConstraints:actionItemsConstraints
                          toView:_actionItemsView
                 viewsDictionary:viewsDictionary
                         metrics:metrics];
  [super updateConstraints];
}

#pragma mark - Constraints helper

+ (void)addConstraints:(NSArray*)constraints toView:(NSView*)view
       viewsDictionary:(NSDictionary*)viewsDictionary
               metrics:(NSDictionary*)metrics {
  for (NSString* constraintFormat in constraints) {
    NSArray* constraints =
    [NSLayoutConstraint constraintsWithVisualFormat:constraintFormat
                                            options:0
                                            metrics:metrics
                                              views:viewsDictionary];
    for (NSLayoutConstraint* constraint in constraints) {
      [view addConstraint:constraint];
    }
  }
}

#pragma mark - Control actions

- (void)startCall:(id)sender {
  NSString* roomString = _roomField.stringValue;
  // Generate room id for loopback options.
  if (_loopbackButton.intValue && [roomString isEqualToString:@""]) {
    roomString = [NSUUID UUID].UUIDString;
    roomString = [roomString stringByReplacingOccurrencesOfString:@"-" withString:@""];
  }
  [self.delegate appRTCMainView:self
                 didEnterRoomId:roomString
                       loopback:_loopbackButton.intValue];
  [self setNeedsUpdateConstraints:YES];
}

#pragma mark - RTC_OBJC_TYPE(RTCNSGLVideoViewDelegate)

- (void)videoView:(RTC_OBJC_TYPE(RTCNSGLVideoView) *)videoView didChangeVideoSize:(NSSize)size {
  if (videoView == _remoteVideoView) {
    _remoteVideoSize = size;
  } else if (videoView == _localVideoView) {
    _localVideoSize = size;
  } else {
    return;
  }

  [self setNeedsUpdateConstraints:YES];
}

#pragma mark - Private

- (void)setupViews {
  NSParameterAssert([[self subviews] count] == 0);

  _logView = [[NSTextView alloc] initWithFrame:NSZeroRect];
  [_logView setMinSize:NSMakeSize(0, kBottomViewHeight)];
  [_logView setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
  [_logView setVerticallyResizable:YES];
  [_logView setAutoresizingMask:NSViewWidthSizable];
  NSTextContainer* textContainer = [_logView textContainer];
  NSSize containerSize = NSMakeSize(kContentWidth, FLT_MAX);
  [textContainer setContainerSize:containerSize];
  [textContainer setWidthTracksTextView:YES];
  [_logView setEditable:NO];

  [self setupActionItemsView];

  _scrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  [_scrollView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_scrollView setHasVerticalScroller:YES];
  [_scrollView setDocumentView:_logView];
  [self addSubview:_scrollView];

// NOTE (daniela): Ignoring Clang diagonstic here.
// We're performing run time check to make sure class is available on runtime.
// If not we're providing sensible default.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
  if ([RTC_OBJC_TYPE(RTCMTLNSVideoView) class] &&
      [RTC_OBJC_TYPE(RTCMTLNSVideoView) isMetalAvailable]) {
    _remoteVideoView = [[RTC_OBJC_TYPE(RTCMTLNSVideoView) alloc] initWithFrame:NSZeroRect];
    _localVideoView = [[RTC_OBJC_TYPE(RTCMTLNSVideoView) alloc] initWithFrame:NSZeroRect];
  }
#pragma clang diagnostic pop
  if (_remoteVideoView == nil) {
    NSOpenGLPixelFormatAttribute attributes[] = {
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFADepthSize, 24,
      NSOpenGLPFAOpenGLProfile,
      NSOpenGLProfileVersion3_2Core,
      0
    };
    NSOpenGLPixelFormat* pixelFormat =
    [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];

    RTC_OBJC_TYPE(RTCNSGLVideoView)* remote =
        [[RTC_OBJC_TYPE(RTCNSGLVideoView) alloc] initWithFrame:NSZeroRect pixelFormat:pixelFormat];
    remote.delegate = self;
    _remoteVideoView = remote;

    RTC_OBJC_TYPE(RTCNSGLVideoView)* local =
        [[RTC_OBJC_TYPE(RTCNSGLVideoView) alloc] initWithFrame:NSZeroRect pixelFormat:pixelFormat];
    local.delegate = self;
    _localVideoView = local;
  }

  [_remoteVideoView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self addSubview:_remoteVideoView];
  [_localVideoView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self addSubview:_localVideoView];
}

- (void)setupActionItemsView {
  _actionItemsView = [[NSView alloc] initWithFrame:NSZeroRect];
  [_actionItemsView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self addSubview:_actionItemsView];

  _roomField = [[NSTextField alloc] initWithFrame:NSZeroRect];
  [_roomField setTranslatesAutoresizingMaskIntoConstraints:NO];
  [[_roomField cell] setPlaceholderString: @"Enter AppRTC room id"];
  [_actionItemsView addSubview:_roomField];
  [_roomField setEditable:YES];

  _connectButton = [[NSButton alloc] initWithFrame:NSZeroRect];
  [_connectButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  _connectButton.title = @"Start call";
  _connectButton.bezelStyle = NSRoundedBezelStyle;
  _connectButton.target = self;
  _connectButton.action = @selector(startCall:);
  [_actionItemsView addSubview:_connectButton];

  _loopbackButton = [[NSButton alloc] initWithFrame:NSZeroRect];
  [_loopbackButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  _loopbackButton.title = @"Loopback";
  [_loopbackButton setButtonType:NSSwitchButton];
  [_actionItemsView addSubview:_loopbackButton];
}

- (NSSize)remoteVideoViewSize {
  if (!_remoteVideoView.bounds.size.width) {
    return NSMakeSize(kContentWidth, 0);
  }
  NSInteger width = MAX(_remoteVideoView.bounds.size.width, kContentWidth);
  NSInteger height = (width/16) * 9;
  return NSMakeSize(width, height);
}

@end

@interface APPRTCViewController ()
    <ARDAppClientDelegate, APPRTCMainViewDelegate>
@property(nonatomic, readonly) APPRTCMainView* mainView;
@end

@implementation APPRTCViewController {
  ARDAppClient* _client;
  RTC_OBJC_TYPE(RTCVideoTrack) * _localVideoTrack;
  RTC_OBJC_TYPE(RTCVideoTrack) * _remoteVideoTrack;
  ARDCaptureController* _captureController;
}

- (void)dealloc {
  [self disconnect];
}

- (void)viewDidAppear {
  [super viewDidAppear];
  [self displayUsageInstructions];
}

- (void)loadView {
  APPRTCMainView* view = [[APPRTCMainView alloc] initWithFrame:NSZeroRect];
  [view setTranslatesAutoresizingMaskIntoConstraints:NO];
  view.delegate = self;
  self.view = view;
}

- (void)windowWillClose:(NSNotification*)notification {
  [self disconnect];
}

#pragma mark - Usage

- (void)displayUsageInstructions {
  [self.mainView displayLogMessage:
   @"To start call:\n"
   @"• Enter AppRTC room id (not neccessary for loopback)\n"
   @"• Start call"];
}

#pragma mark - ARDAppClientDelegate

- (void)appClient:(ARDAppClient *)client
    didChangeState:(ARDAppClientState)state {
  switch (state) {
    case kARDAppClientStateConnected:
      [self.mainView displayLogMessage:@"Client connected."];
      break;
    case kARDAppClientStateConnecting:
      [self.mainView displayLogMessage:@"Client connecting."];
      break;
    case kARDAppClientStateDisconnected:
      [self.mainView displayLogMessage:@"Client disconnected."];
      [self resetUI];
      _client = nil;
      break;
  }
}

- (void)appClient:(ARDAppClient *)client
    didChangeConnectionState:(RTCIceConnectionState)state {
}

- (void)appClient:(ARDAppClient*)client
    didCreateLocalCapturer:(RTC_OBJC_TYPE(RTCCameraVideoCapturer) *)localCapturer {
  _captureController =
      [[ARDCaptureController alloc] initWithCapturer:localCapturer
                                            settings:[[ARDSettingsModel alloc] init]];
  [_captureController startCapture];
}

- (void)appClient:(ARDAppClient*)client
    didReceiveLocalVideoTrack:(RTC_OBJC_TYPE(RTCVideoTrack) *)localVideoTrack {
  _localVideoTrack = localVideoTrack;
  [_localVideoTrack addRenderer:self.mainView.localVideoView];
}

- (void)appClient:(ARDAppClient*)client
    didReceiveRemoteVideoTrack:(RTC_OBJC_TYPE(RTCVideoTrack) *)remoteVideoTrack {
  _remoteVideoTrack = remoteVideoTrack;
  [_remoteVideoTrack addRenderer:self.mainView.remoteVideoView];
}

- (void)appClient:(ARDAppClient *)client
         didError:(NSError *)error {
  [self showAlertWithMessage:[NSString stringWithFormat:@"%@", error]];
  [self disconnect];
}

- (void)appClient:(ARDAppClient *)client
      didGetStats:(NSArray *)stats {
}

#pragma mark - APPRTCMainViewDelegate

- (void)appRTCMainView:(APPRTCMainView*)mainView
        didEnterRoomId:(NSString*)roomId
              loopback:(BOOL)isLoopback {

  if ([roomId isEqualToString:@""]) {
    [self.mainView displayLogMessage:@"Missing room id"];
    return;
  }

  [self disconnect];
  ARDAppClient* client = [[ARDAppClient alloc] initWithDelegate:self];
  [client connectToRoomWithId:roomId
                     settings:[[ARDSettingsModel alloc] init]  // Use default settings.
                   isLoopback:isLoopback];
  _client = client;
}

#pragma mark - Private

- (APPRTCMainView*)mainView {
  return (APPRTCMainView*)self.view;
}

- (void)showAlertWithMessage:(NSString*)message {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:message];
    [alert runModal];
  });
}

- (void)resetUI {
  [_remoteVideoTrack removeRenderer:self.mainView.remoteVideoView];
  [_localVideoTrack removeRenderer:self.mainView.localVideoView];
  _remoteVideoTrack = nil;
  _localVideoTrack = nil;
  [self.mainView.remoteVideoView renderFrame:nil];
  [self.mainView.localVideoView renderFrame:nil];
}

- (void)disconnect {
  [self resetUI];
  [_captureController stopCapture];
  _captureController = nil;
  [_client disconnect];
}

@end
