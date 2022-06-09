/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "NSString+StdString.h"

@implementation NSString (StdString)

- (std::string)stdString {
  return [NSString stdStringForString:self];
}

+ (std::string)stdStringForString:(NSString *)nsString {
  NSData *charData = [nsString dataUsingEncoding:NSUTF8StringEncoding];
  return std::string(reinterpret_cast<const char *>(charData.bytes),
                     charData.length);
}

+ (NSString *)stringForStdString:(const std::string&)stdString {
  // std::string may contain null termination character so we construct
  // using length.
  return [[NSString alloc] initWithBytes:stdString.data()
                                  length:stdString.length()
                                encoding:NSUTF8StringEncoding];
}

@end
