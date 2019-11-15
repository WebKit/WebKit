/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include "rtc_base/gunit.h"

#import "api/peerconnection/RTCSessionDescription+Private.h"
#import "api/peerconnection/RTCSessionDescription.h"
#import "helpers/NSString+StdString.h"

@interface RTCSessionDescriptionTest : NSObject
- (void)testSessionDescriptionConversion;
- (void)testInitFromNativeSessionDescription;
@end

@implementation RTCSessionDescriptionTest

/**
 * Test conversion of an Objective-C RTCSessionDescription to a native
 * SessionDescriptionInterface (based on the types and SDP strings being equal).
 */
- (void)testSessionDescriptionConversion {
  RTCSessionDescription *description =
      [[RTCSessionDescription alloc] initWithType:RTCSdpTypeAnswer
                                              sdp:[self sdp]];

  webrtc::SessionDescriptionInterface *nativeDescription =
      description.nativeDescription;

  EXPECT_EQ(RTCSdpTypeAnswer,
      [RTCSessionDescription typeForStdString:nativeDescription->type()]);

  std::string sdp;
  nativeDescription->ToString(&sdp);
  EXPECT_EQ([self sdp].stdString, sdp);
}

- (void)testInitFromNativeSessionDescription {
  webrtc::SessionDescriptionInterface *nativeDescription;

  nativeDescription = webrtc::CreateSessionDescription(
      webrtc::SessionDescriptionInterface::kAnswer,
      [self sdp].stdString,
      nullptr);

  RTCSessionDescription *description =
      [[RTCSessionDescription alloc] initWithNativeDescription:
      nativeDescription];
  EXPECT_EQ(webrtc::SessionDescriptionInterface::kAnswer,
      [RTCSessionDescription stdStringForType:description.type]);
  EXPECT_TRUE([[self sdp] isEqualToString:description.sdp]);
}

- (NSString *)sdp {
  return @"v=0\r\n"
          "o=- 5319989746393411314 2 IN IP4 127.0.0.1\r\n"
          "s=-\r\n"
          "t=0 0\r\n"
          "a=group:BUNDLE audio video\r\n"
          "a=msid-semantic: WMS ARDAMS\r\n"
          "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 9 0 8 126\r\n"
          "c=IN IP4 0.0.0.0\r\n"
          "a=rtcp:9 IN IP4 0.0.0.0\r\n"
          "a=ice-ufrag:f3o+0HG7l9nwIWFY\r\n"
          "a=ice-pwd:VDctmJNCptR2TB7+meDpw7w5\r\n"
          "a=fingerprint:sha-256 A9:D5:8D:A8:69:22:39:60:92:AD:94:1A:22:2D:5E:"
          "A5:4A:A9:18:C2:35:5D:46:5E:59:BD:1C:AF:38:9F:E6:E1\r\n"
          "a=setup:active\r\n"
          "a=mid:audio\r\n"
          "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"
          "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/"
          "abs-send-time\r\n"
          "a=sendrecv\r\n"
          "a=rtcp-mux\r\n"
          "a=rtpmap:111 opus/48000/2\r\n"
          "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
          "a=rtpmap:103 ISAC/16000\r\n"
          "a=rtpmap:9 G722/8000\r\n"
          "a=rtpmap:0 PCMU/8000\r\n"
          "a=rtpmap:8 PCMA/8000\r\n"
          "a=rtpmap:126 telephone-event/8000\r\n"
          "a=maxptime:60\r\n"
          "a=ssrc:1504474588 cname:V+FdIC5AJpxLhdYQ\r\n"
          "a=ssrc:1504474588 msid:ARDAMS ARDAMSa0\r\n"
          "a=ssrc:1504474588 mslabel:ARDAMS\r\n"
          "a=ssrc:1504474588 label:ARDAMSa0\r\n"
          "m=video 9 UDP/TLS/RTP/SAVPF 100 116 117 96\r\n"
          "c=IN IP4 0.0.0.0\r\n"
          "a=rtcp:9 IN IP4 0.0.0.0\r\n"
          "a=ice-ufrag:f3o+0HG7l9nwIWFY\r\n"
          "a=ice-pwd:VDctmJNCptR2TB7+meDpw7w5\r\n"
          "a=fingerprint:sha-256 A9:D5:8D:A8:69:22:39:60:92:AD:94:1A:22:2D:5E:"
          "A5:4A:A9:18:C2:35:5D:46:5E:59:BD:1C:AF:38:9F:E6:E1\r\n"
          "a=setup:active\r\n"
          "a=mid:video\r\n"
          "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n"
          "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/"
          "abs-send-time\r\n"
          "a=extmap:4 urn:3gpp:video-orientation\r\n"
          "a=sendrecv\r\n"
          "a=rtcp-mux\r\n"
          "a=rtpmap:100 VP8/90000\r\n"
          "a=rtcp-fb:100 ccm fir\r\n"
          "a=rtcp-fb:100 goog-lntf\r\n"
          "a=rtcp-fb:100 nack\r\n"
          "a=rtcp-fb:100 nack pli\r\n"
          "a=rtcp-fb:100 goog-remb\r\n"
          "a=rtpmap:116 red/90000\r\n"
          "a=rtpmap:117 ulpfec/90000\r\n"
          "a=rtpmap:96 rtx/90000\r\n"
          "a=fmtp:96 apt=100\r\n"
          "a=ssrc-group:FID 498297514 1644357692\r\n"
          "a=ssrc:498297514 cname:V+FdIC5AJpxLhdYQ\r\n"
          "a=ssrc:498297514 msid:ARDAMS ARDAMSv0\r\n"
          "a=ssrc:498297514 mslabel:ARDAMS\r\n"
          "a=ssrc:498297514 label:ARDAMSv0\r\n"
          "a=ssrc:1644357692 cname:V+FdIC5AJpxLhdYQ\r\n"
          "a=ssrc:1644357692 msid:ARDAMS ARDAMSv0\r\n"
          "a=ssrc:1644357692 mslabel:ARDAMS\r\n"
          "a=ssrc:1644357692 label:ARDAMSv0\r\n";
}

@end

TEST(RTCSessionDescriptionTest, SessionDescriptionConversionTest) {
  @autoreleasepool {
    RTCSessionDescriptionTest *test = [[RTCSessionDescriptionTest alloc] init];
    [test testSessionDescriptionConversion];
  }
}

TEST(RTCSessionDescriptionTest, InitFromSessionDescriptionTest) {
  @autoreleasepool {
    RTCSessionDescriptionTest *test = [[RTCSessionDescriptionTest alloc] init];
    [test testInitFromNativeSessionDescription];
  }
}
