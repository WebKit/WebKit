/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "WebRTC/RTCSessionDescription.h"

#import "ARDSDPUtils.h"

@interface ARDSDPUtilsTest : XCTestCase
@end

@implementation ARDSDPUtilsTest

- (void)testPreferVideoCodecH264 {
  NSString *sdp = @("m=video 9 RTP/SAVPF 100 116 117 96 120 97\n"
                    "a=rtpmap:120 H264/90000\n"
                    "a=rtpmap:97 H264/90000\n");
  NSString *expectedSdp = @("m=video 9 RTP/SAVPF 120 97 100 116 117 96\n"
                            "a=rtpmap:120 H264/90000\n"
                            "a=rtpmap:97 H264/90000\n");
  [self preferVideoCodec:@"H264" sdp:sdp expected:expectedSdp];
}

- (void)testPreferVideoCodecVP8 {
  NSString *sdp = @("m=video 9 RTP/SAVPF 100 116 117 96 120 97\n"
                    "a=rtpmap:116 VP8/90000\n");
  NSString *expectedSdp = @("m=video 9 RTP/SAVPF 116 100 117 96 120 97\n"
                            "a=rtpmap:116 VP8/90000\n");
  [self preferVideoCodec:@"VP8" sdp:sdp expected:expectedSdp];
}

- (void)testNoMLine {
  NSString *sdp = @("a=rtpmap:116 VP8/90000\n");
  [self preferVideoCodec:@"VP8" sdp:sdp expected:sdp];
}

- (void)testMissingCodec {
  NSString *sdp = @("m=video 9 RTP/SAVPF 100 116 117 96 120 97\n"
                    "a=rtpmap:116 VP8/90000\n");
  [self preferVideoCodec:@"foo" sdp:sdp expected:sdp];
}

#pragma mark - Helpers

- (void)preferVideoCodec:(NSString *)codec
                     sdp:(NSString *)sdp
                expected:(NSString *)expectedSdp{
  RTCSessionDescription* desc =
    [[RTCSessionDescription alloc] initWithType:RTCSdpTypeOffer sdp:sdp];
  RTCSessionDescription *outputDesc =
    [ARDSDPUtils descriptionForDescription:desc
                       preferredVideoCodec:codec];
  XCTAssertTrue([outputDesc.description rangeOfString:expectedSdp].location != NSNotFound);
}
@end
