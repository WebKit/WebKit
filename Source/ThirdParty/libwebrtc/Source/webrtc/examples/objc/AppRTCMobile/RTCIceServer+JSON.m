/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIceServer+JSON.h"

@implementation RTCIceServer (JSON)

+ (RTCIceServer *)serverFromJSONDictionary:(NSDictionary *)dictionary {
  NSArray *turnUrls = dictionary[@"urls"];
  NSString *username = dictionary[@"username"] ?: @"";
  NSString *credential = dictionary[@"credential"] ?: @"";
  return [[RTCIceServer alloc] initWithURLStrings:turnUrls
                                         username:username
                                       credential:credential];
}

@end
