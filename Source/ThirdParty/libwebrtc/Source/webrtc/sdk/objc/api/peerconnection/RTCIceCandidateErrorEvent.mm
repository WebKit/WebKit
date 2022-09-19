/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIceCandidateErrorEvent+Private.h"

#import "helpers/NSString+StdString.h"

@implementation RTC_OBJC_TYPE (RTCIceCandidateErrorEvent)

@synthesize address = _address;
@synthesize port = _port;
@synthesize url = _url;
@synthesize errorCode = _errorCode;
@synthesize errorText = _errorText;

- (instancetype)init {
  return [super init];
}

- (instancetype)initWithAddress:(const std::string&)address
                           port:(const int)port
                            url:(const std::string&)url
                      errorCode:(const int)errorCode
                      errorText:(const std::string&)errorText {
  if (self = [self init]) {
    _address = [NSString stringForStdString:address];
    _port = port;
    _url = [NSString stringForStdString:url];
    _errorCode = errorCode;
    _errorText = [NSString stringForStdString:errorText];
  }
  return self;
}

@end
