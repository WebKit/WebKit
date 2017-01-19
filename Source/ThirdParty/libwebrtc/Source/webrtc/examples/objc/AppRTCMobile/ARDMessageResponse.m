/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDMessageResponse+Internal.h"

#import "ARDUtilities.h"

static NSString const *kARDMessageResultKey = @"result";

@implementation ARDMessageResponse

@synthesize result = _result;

+ (ARDMessageResponse *)responseFromJSONData:(NSData *)data {
  NSDictionary *responseJSON = [NSDictionary dictionaryWithJSONData:data];
  if (!responseJSON) {
    return nil;
  }
  ARDMessageResponse *response = [[ARDMessageResponse alloc] init];
  response.result =
      [[self class] resultTypeFromString:responseJSON[kARDMessageResultKey]];
  return response;
}

#pragma mark - Private

+ (ARDMessageResultType)resultTypeFromString:(NSString *)resultString {
  ARDMessageResultType result = kARDMessageResultTypeUnknown;
  if ([resultString isEqualToString:@"SUCCESS"]) {
    result = kARDMessageResultTypeSuccess;
  } else if ([resultString isEqualToString:@"INVALID_CLIENT"]) {
    result = kARDMessageResultTypeInvalidClient;
  } else if ([resultString isEqualToString:@"INVALID_ROOM"]) {
    result = kARDMessageResultTypeInvalidRoom;
  }
  return result;
}

@end
