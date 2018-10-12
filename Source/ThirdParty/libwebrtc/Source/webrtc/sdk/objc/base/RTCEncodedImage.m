/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCEncodedImage.h"

@implementation RTCEncodedImage

@synthesize buffer = _buffer;
@synthesize encodedWidth = _encodedWidth;
@synthesize encodedHeight = _encodedHeight;
@synthesize timeStamp = _timeStamp;
@synthesize captureTimeMs = _captureTimeMs;
@synthesize ntpTimeMs = _ntpTimeMs;
@synthesize flags = _flags;
@synthesize encodeStartMs = _encodeStartMs;
@synthesize encodeFinishMs = _encodeFinishMs;
@synthesize frameType = _frameType;
@synthesize rotation = _rotation;
@synthesize completeFrame = _completeFrame;
@synthesize qp = _qp;
@synthesize contentType = _contentType;
@synthesize spatialIndex = _spatialIndex;

@end
