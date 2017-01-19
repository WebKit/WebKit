/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDSDPUtils.h"

#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCSessionDescription.h"

@implementation ARDSDPUtils

+ (RTCSessionDescription *)
    descriptionForDescription:(RTCSessionDescription *)description
          preferredVideoCodec:(NSString *)codec {
  NSString *sdpString = description.sdp;
  NSString *lineSeparator = @"\n";
  NSString *mLineSeparator = @" ";
  // Copied from PeerConnectionClient.java.
  // TODO(tkchin): Move this to a shared C++ file.
  NSMutableArray *lines =
      [NSMutableArray arrayWithArray:
          [sdpString componentsSeparatedByString:lineSeparator]];
  NSInteger mLineIndex = -1;
  NSString *codecRtpMap = nil;
  // a=rtpmap:<payload type> <encoding name>/<clock rate>
  // [/<encoding parameters>]
  NSString *pattern =
      [NSString stringWithFormat:@"^a=rtpmap:(\\d+) %@(/\\d+)+[\r]?$", codec];
  NSRegularExpression *regex =
      [NSRegularExpression regularExpressionWithPattern:pattern
                                                options:0
                                                  error:nil];
  for (NSInteger i = 0; (i < lines.count) && (mLineIndex == -1 || !codecRtpMap);
       ++i) {
    NSString *line = lines[i];
    if ([line hasPrefix:@"m=video"]) {
      mLineIndex = i;
      continue;
    }
    NSTextCheckingResult *codecMatches =
        [regex firstMatchInString:line
                          options:0
                            range:NSMakeRange(0, line.length)];
    if (codecMatches) {
      codecRtpMap =
          [line substringWithRange:[codecMatches rangeAtIndex:1]];
      continue;
    }
  }
  if (mLineIndex == -1) {
    RTCLog(@"No m=video line, so can't prefer %@", codec);
    return description;
  }
  if (!codecRtpMap) {
    RTCLog(@"No rtpmap for %@", codec);
    return description;
  }
  NSArray *origMLineParts =
      [lines[mLineIndex] componentsSeparatedByString:mLineSeparator];
  if (origMLineParts.count > 3) {
    NSMutableArray *newMLineParts =
        [NSMutableArray arrayWithCapacity:origMLineParts.count];
    NSInteger origPartIndex = 0;
    // Format is: m=<media> <port> <proto> <fmt> ...
    [newMLineParts addObject:origMLineParts[origPartIndex++]];
    [newMLineParts addObject:origMLineParts[origPartIndex++]];
    [newMLineParts addObject:origMLineParts[origPartIndex++]];
    [newMLineParts addObject:codecRtpMap];
    for (; origPartIndex < origMLineParts.count; ++origPartIndex) {
      if (![codecRtpMap isEqualToString:origMLineParts[origPartIndex]]) {
        [newMLineParts addObject:origMLineParts[origPartIndex]];
      }
    }
    NSString *newMLine =
        [newMLineParts componentsJoinedByString:mLineSeparator];
    [lines replaceObjectAtIndex:mLineIndex
                     withObject:newMLine];
  } else {
    RTCLogWarning(@"Wrong SDP media description format: %@", lines[mLineIndex]);
  }
  NSString *mangledSdpString = [lines componentsJoinedByString:lineSeparator];
  return [[RTCSessionDescription alloc] initWithType:description.type
                                                 sdp:mangledSdpString];
}

@end
