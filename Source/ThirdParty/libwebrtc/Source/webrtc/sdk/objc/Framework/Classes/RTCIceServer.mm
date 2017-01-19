/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIceServer+Private.h"

#import "NSString+StdString.h"

@implementation RTCIceServer

@synthesize urlStrings = _urlStrings;
@synthesize username = _username;
@synthesize credential = _credential;

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings {
  NSParameterAssert(urlStrings.count);
  return [self initWithURLStrings:urlStrings
                         username:nil
                       credential:nil];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential {
  NSParameterAssert(urlStrings.count);
  if (self = [super init]) {
    _urlStrings = [[NSArray alloc] initWithArray:urlStrings copyItems:YES];
    _username = [username copy];
    _credential = [credential copy];
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCIceServer:\n%@\n%@\n%@",
                                    _urlStrings,
                                    _username,
                                    _credential];
}

#pragma mark - Private

- (webrtc::PeerConnectionInterface::IceServer)nativeServer {
  __block webrtc::PeerConnectionInterface::IceServer iceServer;

  iceServer.username = [NSString stdStringForString:_username];
  iceServer.password = [NSString stdStringForString:_credential];

  [_urlStrings enumerateObjectsUsingBlock:^(NSString *url,
                                            NSUInteger idx,
                                            BOOL *stop) {
    iceServer.urls.push_back(url.stdString);
  }];
  return iceServer;
}

- (instancetype)initWithNativeServer:
    (webrtc::PeerConnectionInterface::IceServer)nativeServer {
  NSMutableArray *urls =
      [NSMutableArray arrayWithCapacity:nativeServer.urls.size()];
  for (auto const &url : nativeServer.urls) {
    [urls addObject:[NSString stringForStdString:url]];
  }
  NSString *username = [NSString stringForStdString:nativeServer.username];
  NSString *credential = [NSString stringForStdString:nativeServer.password];
  self = [self initWithURLStrings:urls
                         username:username
                       credential:credential];
  return self;
}

@end
