/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDCEODTURNClient.h"

#import "ARDUtilities.h"
#import "RTCIceServer+JSON.h"

// TODO(tkchin): move this to a configuration object.
static NSString *kTURNOriginURLString = @"https://apprtc.appspot.com";
static NSString *kARDCEODTURNClientErrorDomain = @"ARDCEODTURNClient";
static NSInteger kARDCEODTURNClientErrorBadResponse = -1;

@implementation ARDCEODTURNClient {
  NSURL *_url;
}

- (instancetype)initWithURL:(NSURL *)url {
  NSParameterAssert([url absoluteString].length);
  if (self = [super init]) {
    _url = url;
  }
  return self;
}

- (void)requestServersWithCompletionHandler:
    (void (^)(NSArray *turnServers,
              NSError *error))completionHandler {
  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:_url];
  // We need to set origin because TURN provider whitelists requests based on
  // origin.
  [request addValue:@"Mozilla/5.0" forHTTPHeaderField:@"user-agent"];
  [request addValue:kTURNOriginURLString forHTTPHeaderField:@"origin"];
  [NSURLConnection sendAsyncRequest:request
                  completionHandler:^(NSURLResponse *response,
                                      NSData *data,
                                      NSError *error) {
    NSArray *turnServers = [NSArray array];
    if (error) {
      completionHandler(nil, error);
      return;
    }
    NSDictionary *dict = [NSDictionary dictionaryWithJSONData:data];
    turnServers = @[ [RTCIceServer serverFromCEODJSONDictionary:dict] ];
    if (!turnServers) {
      NSError *responseError =
          [[NSError alloc] initWithDomain:kARDCEODTURNClientErrorDomain
                                     code:kARDCEODTURNClientErrorBadResponse
                                 userInfo:@{
            NSLocalizedDescriptionKey: @"Bad TURN response.",
          }];
      completionHandler(turnServers, responseError);
      return;
    }
    completionHandler(turnServers, nil);
  }];
}

@end
