/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoEncoderSettings.h"

@implementation RTCVideoEncoderSettings

@synthesize name = _name;
@synthesize width = _width;
@synthesize height = _height;
@synthesize startBitrate = _startBitrate;
@synthesize maxBitrate = _maxBitrate;
@synthesize minBitrate = _minBitrate;
@synthesize maxFramerate = _maxFramerate;
@synthesize qpMax = _qpMax;
@synthesize mode = _mode;

@end
