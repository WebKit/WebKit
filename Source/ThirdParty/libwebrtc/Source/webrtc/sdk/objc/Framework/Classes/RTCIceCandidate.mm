/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIceCandidate+Private.h"

#include <memory>

#import "NSString+StdString.h"
#import "WebRTC/RTCLogging.h"

@implementation RTCIceCandidate

@synthesize sdpMid = _sdpMid;
@synthesize sdpMLineIndex = _sdpMLineIndex;
@synthesize sdp = _sdp;

- (instancetype)initWithSdp:(NSString *)sdp
              sdpMLineIndex:(int)sdpMLineIndex
                     sdpMid:(NSString *)sdpMid {
  NSParameterAssert(sdp.length);
  if (self = [super init]) {
    _sdpMid = [sdpMid copy];
    _sdpMLineIndex = sdpMLineIndex;
    _sdp = [sdp copy];
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"RTCIceCandidate:\n%@\n%d\n%@",
                                    _sdpMid,
                                    _sdpMLineIndex,
                                    _sdp];
}

#pragma mark - Private

- (instancetype)initWithNativeCandidate:
    (const webrtc::IceCandidateInterface *)candidate {
  NSParameterAssert(candidate);
  std::string sdp;
  candidate->ToString(&sdp);

  return [self initWithSdp:[NSString stringForStdString:sdp]
             sdpMLineIndex:candidate->sdp_mline_index()
                    sdpMid:[NSString stringForStdString:candidate->sdp_mid()]];
}

- (std::unique_ptr<webrtc::IceCandidateInterface>)nativeCandidate {
  webrtc::SdpParseError error;

  webrtc::IceCandidateInterface *candidate = webrtc::CreateIceCandidate(
      _sdpMid.stdString, _sdpMLineIndex, _sdp.stdString, &error);

  if (!candidate) {
    RTCLog(@"Failed to create ICE candidate: %s\nline: %s",
           error.description.c_str(),
           error.line.c_str());
  }

  return std::unique_ptr<webrtc::IceCandidateInterface>(candidate);
}

@end
