/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ViewController.h"
#import <WebRTC/WebRTC.h>

@interface ViewController ()
@property (nonatomic, strong) RTCPeerConnectionFactory *factory;
@property (nonatomic, strong) RTCAVFoundationVideoSource *videoSource;
@end

@implementation ViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.factory = [[RTCPeerConnectionFactory alloc] init];
  self.videoSource = [self.factory avFoundationVideoSourceWithConstraints:nil];
}

@end
