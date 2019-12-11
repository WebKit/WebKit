/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDBroadcastSetupViewController.h"

@implementation ARDBroadcastSetupViewController {
  UITextField *_roomNameField;
}

- (void)loadView {
  UIView *view = [[UIView alloc] initWithFrame:CGRectZero];
  view.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.7];

  UIImageView *imageView = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"Icon-180"]];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:imageView];

  _roomNameField = [[UITextField alloc] initWithFrame:CGRectZero];
  _roomNameField.borderStyle = UITextBorderStyleRoundedRect;
  _roomNameField.font = [UIFont systemFontOfSize:14.0];
  _roomNameField.translatesAutoresizingMaskIntoConstraints = NO;
  _roomNameField.placeholder = @"Room name";
  _roomNameField.returnKeyType = UIReturnKeyDone;
  _roomNameField.delegate = self;
  [view addSubview:_roomNameField];

  UIButton *doneButton = [UIButton buttonWithType:UIButtonTypeSystem];
  doneButton.translatesAutoresizingMaskIntoConstraints = NO;
  doneButton.titleLabel.font = [UIFont systemFontOfSize:20.0];
  [doneButton setTitle:@"Done" forState:UIControlStateNormal];
  [doneButton addTarget:self
                 action:@selector(userDidFinishSetup)
       forControlEvents:UIControlEventTouchUpInside];
  [view addSubview:doneButton];

  UIButton *cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  cancelButton.titleLabel.font = [UIFont systemFontOfSize:20.0];
  [cancelButton setTitle:@"Cancel" forState:UIControlStateNormal];
  [cancelButton addTarget:self
                   action:@selector(userDidCancelSetup)
         forControlEvents:UIControlEventTouchUpInside];
  [view addSubview:cancelButton];

  UILayoutGuide *margin = view.layoutMarginsGuide;
  [imageView.widthAnchor constraintEqualToConstant:60.0].active = YES;
  [imageView.heightAnchor constraintEqualToConstant:60.0].active = YES;
  [imageView.topAnchor constraintEqualToAnchor:margin.topAnchor constant:20].active = YES;
  [imageView.centerXAnchor constraintEqualToAnchor:view.centerXAnchor].active = YES;

  [_roomNameField.leadingAnchor constraintEqualToAnchor:margin.leadingAnchor].active = YES;
  [_roomNameField.topAnchor constraintEqualToAnchor:imageView.bottomAnchor constant:20].active =
      YES;
  [_roomNameField.trailingAnchor constraintEqualToAnchor:margin.trailingAnchor].active = YES;

  [doneButton.leadingAnchor constraintEqualToAnchor:margin.leadingAnchor].active = YES;
  [doneButton.bottomAnchor constraintEqualToAnchor:margin.bottomAnchor constant:-20].active = YES;

  [cancelButton.trailingAnchor constraintEqualToAnchor:margin.trailingAnchor].active = YES;
  [cancelButton.bottomAnchor constraintEqualToAnchor:margin.bottomAnchor constant:-20].active = YES;

  UITapGestureRecognizer *tgr =
      [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(didTap:)];
  [view addGestureRecognizer:tgr];

  self.view = view;
}

- (IBAction)didTap:(id)sender {
  [self.view endEditing:YES];
}

- (void)userDidFinishSetup {
  // URL of the resource where broadcast can be viewed that will be returned to the application
  NSURL *broadcastURL = [NSURL
      URLWithString:[NSString stringWithFormat:@"https://appr.tc/r/%@", _roomNameField.text]];

  // Dictionary with setup information that will be provided to broadcast extension when broadcast
  // is started
  NSDictionary *setupInfo = @{@"roomName" : _roomNameField.text};

  // Tell ReplayKit that the extension is finished setting up and can begin broadcasting
  [self.extensionContext completeRequestWithBroadcastURL:broadcastURL setupInfo:setupInfo];
}

- (void)userDidCancelSetup {
  // Tell ReplayKit that the extension was cancelled by the user
  [self.extensionContext cancelRequestWithError:[NSError errorWithDomain:@"com.google.AppRTCMobile"
                                                                    code:-1
                                                                userInfo:nil]];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  [self userDidFinishSetup];
  return YES;
}

@end
