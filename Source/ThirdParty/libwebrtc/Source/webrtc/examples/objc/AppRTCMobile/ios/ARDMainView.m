/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDMainView.h"

#import "UIImage+ARDUtilities.h"

static CGFloat const kRoomTextFieldHeight = 40;
static CGFloat const kRoomTextFieldMargin = 8;
static CGFloat const kCallControlMargin = 8;

// Helper view that contains a text field and a clear button.
@interface ARDRoomTextField : UIView <UITextFieldDelegate>
@property(nonatomic, readonly) NSString *roomText;
@end

@implementation ARDRoomTextField {
  UITextField *_roomText;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    _roomText = [[UITextField alloc] initWithFrame:CGRectZero];
    _roomText.borderStyle = UITextBorderStyleNone;
    _roomText.font = [UIFont systemFontOfSize:12];
    _roomText.placeholder = @"Room name";
    _roomText.autocorrectionType = UITextAutocorrectionTypeNo;
    _roomText.autocapitalizationType = UITextAutocapitalizationTypeNone;
    _roomText.clearButtonMode = UITextFieldViewModeAlways;
    _roomText.delegate = self;
    [self addSubview:_roomText];

    // Give rounded corners and a light gray border.
    self.layer.borderWidth = 1;
    self.layer.borderColor = [[UIColor lightGrayColor] CGColor];
    self.layer.cornerRadius = 2;
  }
  return self;
}

- (void)layoutSubviews {
  _roomText.frame =
      CGRectMake(kRoomTextFieldMargin, 0, CGRectGetWidth(self.bounds) - kRoomTextFieldMargin,
                 kRoomTextFieldHeight);
}

- (CGSize)sizeThatFits:(CGSize)size {
  size.height = kRoomTextFieldHeight;
  return size;
}

- (NSString *)roomText {
  return _roomText.text;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  // There is no other control that can take focus, so manually resign focus
  // when return (Join) is pressed to trigger `textFieldDidEndEditing`.
  [textField resignFirstResponder];
  return YES;
}

@end

@implementation ARDMainView {
  ARDRoomTextField *_roomText;
  UIButton *_startRegularCallButton;
  UIButton *_startLoopbackCallButton;
  UIButton *_audioLoopButton;
}

@synthesize delegate = _delegate;
@synthesize isAudioLoopPlaying = _isAudioLoopPlaying;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    _roomText = [[ARDRoomTextField alloc] initWithFrame:CGRectZero];
    [self addSubview:_roomText];

    UIFont *controlFont = [UIFont boldSystemFontOfSize:18.0];
    UIColor *controlFontColor = [UIColor whiteColor];

    _startRegularCallButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _startRegularCallButton.titleLabel.font = controlFont;
    [_startRegularCallButton setTitleColor:controlFontColor forState:UIControlStateNormal];
    _startRegularCallButton.backgroundColor
        = [UIColor colorWithRed:66.0/255.0 green:200.0/255.0 blue:90.0/255.0 alpha:1.0];
    [_startRegularCallButton setTitle:@"Call room" forState:UIControlStateNormal];
    [_startRegularCallButton addTarget:self
                                action:@selector(onStartRegularCall:)
                      forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_startRegularCallButton];

    _startLoopbackCallButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _startLoopbackCallButton.titleLabel.font = controlFont;
    [_startLoopbackCallButton setTitleColor:controlFontColor forState:UIControlStateNormal];
    _startLoopbackCallButton.backgroundColor =
        [UIColor colorWithRed:0.0 green:122.0/255.0 blue:1.0 alpha:1.0];
    [_startLoopbackCallButton setTitle:@"Loopback call" forState:UIControlStateNormal];
    [_startLoopbackCallButton addTarget:self
                                 action:@selector(onStartLoopbackCall:)
                       forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_startLoopbackCallButton];


    // Used to test what happens to sounds when calls are in progress.
    _audioLoopButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _audioLoopButton.titleLabel.font = controlFont;
    [_audioLoopButton setTitleColor:controlFontColor forState:UIControlStateNormal];
    _audioLoopButton.backgroundColor =
        [UIColor colorWithRed:1.0 green:149.0/255.0 blue:0.0 alpha:1.0];
    [self updateAudioLoopButton];
    [_audioLoopButton addTarget:self
                         action:@selector(onToggleAudioLoop:)
               forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_audioLoopButton];

    self.backgroundColor = [UIColor whiteColor];
  }
  return self;
}

- (void)setIsAudioLoopPlaying:(BOOL)isAudioLoopPlaying {
  if (_isAudioLoopPlaying == isAudioLoopPlaying) {
    return;
  }
  _isAudioLoopPlaying = isAudioLoopPlaying;
  [self updateAudioLoopButton];
}

- (void)layoutSubviews {
  CGRect bounds = self.bounds;
  CGFloat roomTextWidth = bounds.size.width - 2 * kRoomTextFieldMargin;
  CGFloat roomTextHeight = [_roomText sizeThatFits:bounds.size].height;
  _roomText.frame =
      CGRectMake(kRoomTextFieldMargin, kRoomTextFieldMargin, roomTextWidth,
                 roomTextHeight);

  CGFloat buttonHeight =
      (CGRectGetMaxY(self.bounds) - CGRectGetMaxY(_roomText.frame) - kCallControlMargin * 4) / 3;

  CGFloat regularCallFrameTop = CGRectGetMaxY(_roomText.frame) + kCallControlMargin;
  CGRect regularCallFrame = CGRectMake(kCallControlMargin,
                                       regularCallFrameTop,
                                       bounds.size.width - 2*kCallControlMargin,
                                       buttonHeight);

  CGFloat loopbackCallFrameTop = CGRectGetMaxY(regularCallFrame) + kCallControlMargin;
  CGRect loopbackCallFrame = CGRectMake(kCallControlMargin,
                                        loopbackCallFrameTop,
                                        bounds.size.width - 2*kCallControlMargin,
                                        buttonHeight);

  CGFloat audioLoopTop = CGRectGetMaxY(loopbackCallFrame) + kCallControlMargin;
  CGRect audioLoopFrame = CGRectMake(kCallControlMargin,
                                     audioLoopTop,
                                     bounds.size.width - 2*kCallControlMargin,
                                     buttonHeight);

  _startRegularCallButton.frame = regularCallFrame;
  _startLoopbackCallButton.frame = loopbackCallFrame;
  _audioLoopButton.frame = audioLoopFrame;
}

#pragma mark - Private

- (void)updateAudioLoopButton {
  if (_isAudioLoopPlaying) {
    [_audioLoopButton setTitle:@"Stop sound" forState:UIControlStateNormal];
  } else {
    [_audioLoopButton setTitle:@"Play sound" forState:UIControlStateNormal];
  }
}

- (void)onToggleAudioLoop:(id)sender {
  [_delegate mainViewDidToggleAudioLoop:self];
}

- (void)onStartRegularCall:(id)sender {
  [_delegate mainView:self didInputRoom:_roomText.roomText isLoopback:NO];
}

- (void)onStartLoopbackCall:(id)sender {
  [_delegate mainView:self didInputRoom:_roomText.roomText isLoopback:YES];
}

@end
