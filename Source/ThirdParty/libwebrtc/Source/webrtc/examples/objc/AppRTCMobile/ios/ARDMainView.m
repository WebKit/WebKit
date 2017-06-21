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
    _roomText.font = [UIFont fontWithName:@"Roboto" size:12];
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
  // when return (Join) is pressed to trigger |textFieldDidEndEditing|.
  [textField resignFirstResponder];
  return YES;
}

@end

@implementation ARDMainView {
  ARDRoomTextField *_roomText;
  UILabel *_callOptionsLabel;
  UISwitch *_loopbackSwitch;
  UILabel *_loopbackLabel;
  UIButton *_startCallButton;
  UIButton *_audioLoopButton;
}

@synthesize delegate = _delegate;
@synthesize isAudioLoopPlaying = _isAudioLoopPlaying;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    _roomText = [[ARDRoomTextField alloc] initWithFrame:CGRectZero];
    [self addSubview:_roomText];

    UIFont *controlFont = [UIFont fontWithName:@"Roboto" size:20];
    UIColor *controlFontColor = [UIColor colorWithWhite:0 alpha:.6];

    _callOptionsLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _callOptionsLabel.text = @"Call Options";
    _callOptionsLabel.font = controlFont;
    _callOptionsLabel.textColor = controlFontColor;
    [_callOptionsLabel sizeToFit];
    [self addSubview:_callOptionsLabel];

    _loopbackSwitch = [[UISwitch alloc] initWithFrame:CGRectZero];
    [_loopbackSwitch sizeToFit];
    [self addSubview:_loopbackSwitch];

    _loopbackLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _loopbackLabel.text = @"Loopback mode";
    _loopbackLabel.font = controlFont;
    _loopbackLabel.textColor = controlFontColor;
    [_loopbackLabel sizeToFit];
    [self addSubview:_loopbackLabel];

    _startCallButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_startCallButton setTitle:@"Start call"
                      forState:UIControlStateNormal];
    _startCallButton.titleLabel.font = controlFont;
    [_startCallButton sizeToFit];
    [_startCallButton addTarget:self
                         action:@selector(onStartCall:)
               forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_startCallButton];

    // Used to test what happens to sounds when calls are in progress.
    _audioLoopButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _audioLoopButton.titleLabel.font = controlFont;
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
      CGRectMake(kRoomTextFieldMargin, kRoomTextFieldMargin, roomTextWidth, roomTextHeight);

  CGFloat callOptionsLabelTop =
      CGRectGetMaxY(_roomText.frame) + kCallControlMargin * 4;
  _callOptionsLabel.frame = CGRectMake(kCallControlMargin,
                                       callOptionsLabelTop,
                                       _callOptionsLabel.frame.size.width,
                                       _callOptionsLabel.frame.size.height);

  CGFloat loopbackModeTop = CGRectGetMaxY(_callOptionsLabel.frame) + kCallControlMargin * 2;
  CGRect loopbackModeRect = CGRectMake(kCallControlMargin * 3,
                                       loopbackModeTop,
                                       _loopbackSwitch.frame.size.width,
                                       _loopbackSwitch.frame.size.height);
  _loopbackSwitch.frame = loopbackModeRect;
  CGFloat loopbackModeLabelCenterX = CGRectGetMaxX(loopbackModeRect) +
      kCallControlMargin + _loopbackLabel.frame.size.width / 2;
  _loopbackLabel.center = CGPointMake(loopbackModeLabelCenterX,
                                      CGRectGetMidY(loopbackModeRect));

  CGFloat audioLoopTop = CGRectGetMaxY(loopbackModeRect) + kCallControlMargin * 3;
  _audioLoopButton.frame = CGRectMake(kCallControlMargin,
                                      audioLoopTop,
                                      _audioLoopButton.frame.size.width,
                                      _audioLoopButton.frame.size.height);

  CGFloat startCallTop =
      CGRectGetMaxY(_audioLoopButton.frame) + kCallControlMargin * 3;
  _startCallButton.frame = CGRectMake(kCallControlMargin,
                                      startCallTop,
                                      _startCallButton.frame.size.width,
                                      _startCallButton.frame.size.height);
}

#pragma mark - Private

- (void)updateAudioLoopButton {
  if (_isAudioLoopPlaying) {
    [_audioLoopButton setTitle:@"Stop sound"
                      forState:UIControlStateNormal];
    [_audioLoopButton sizeToFit];
  } else {
    [_audioLoopButton setTitle:@"Play sound"
                      forState:UIControlStateNormal];
    [_audioLoopButton sizeToFit];
  }
}

- (void)onToggleAudioLoop:(id)sender {
  [_delegate mainViewDidToggleAudioLoop:self];
}

- (void)onStartCall:(id)sender {
  [_delegate mainView:self didInputRoom:_roomText.roomText isLoopback:_loopbackSwitch.isOn];
}

@end
