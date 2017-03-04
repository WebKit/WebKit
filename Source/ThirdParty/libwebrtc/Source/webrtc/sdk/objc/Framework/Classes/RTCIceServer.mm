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
@synthesize tlsCertPolicy = _tlsCertPolicy;

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings {
  return [self initWithURLStrings:urlStrings
                         username:nil
                       credential:nil];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential {
  return [self initWithURLStrings:urlStrings
                         username:username
                       credential:credential
                    tlsCertPolicy:RTCTlsCertPolicySecure];
}

- (instancetype)initWithURLStrings:(NSArray<NSString *> *)urlStrings
                          username:(NSString *)username
                        credential:(NSString *)credential
                     tlsCertPolicy:(RTCTlsCertPolicy)tlsCertPolicy {
  NSParameterAssert(urlStrings.count);
  if (self = [super init]) {
    _urlStrings = [[NSArray alloc] initWithArray:urlStrings copyItems:YES];
    _username = [username copy];
    _credential = [credential copy];
    _tlsCertPolicy = tlsCertPolicy;
  }
  return self;
}

- (NSString *)description {
  return
      [NSString stringWithFormat:@"RTCIceServer:\n%@\n%@\n%@\n%@", _urlStrings,
                                 _username, _credential,
                                 [self stringForTlsCertPolicy:_tlsCertPolicy]];
}

#pragma mark - Private

- (NSString *)stringForTlsCertPolicy:(RTCTlsCertPolicy)tlsCertPolicy {
  switch (tlsCertPolicy) {
    case RTCTlsCertPolicySecure:
      return @"RTCTlsCertPolicySecure";
    case RTCTlsCertPolicyInsecureNoCheck:
      return @"RTCTlsCertPolicyInsecureNoCheck";
  }
}

- (webrtc::PeerConnectionInterface::IceServer)nativeServer {
  __block webrtc::PeerConnectionInterface::IceServer iceServer;

  iceServer.username = [NSString stdStringForString:_username];
  iceServer.password = [NSString stdStringForString:_credential];

  [_urlStrings enumerateObjectsUsingBlock:^(NSString *url,
                                            NSUInteger idx,
                                            BOOL *stop) {
    iceServer.urls.push_back(url.stdString);
  }];

  switch (_tlsCertPolicy) {
    case RTCTlsCertPolicySecure:
      iceServer.tls_cert_policy =
          webrtc::PeerConnectionInterface::kTlsCertPolicySecure;
      break;
    case RTCTlsCertPolicyInsecureNoCheck:
      iceServer.tls_cert_policy =
          webrtc::PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck;
      break;
  }
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
  RTCTlsCertPolicy tlsCertPolicy;

  switch (nativeServer.tls_cert_policy) {
    case webrtc::PeerConnectionInterface::kTlsCertPolicySecure:
      tlsCertPolicy = RTCTlsCertPolicySecure;
      break;
    case webrtc::PeerConnectionInterface::kTlsCertPolicyInsecureNoCheck:
      tlsCertPolicy = RTCTlsCertPolicyInsecureNoCheck;
      break;
  }

  self = [self initWithURLStrings:urls
                         username:username
                       credential:credential
                    tlsCertPolicy:tlsCertPolicy];
  return self;
}

@end
