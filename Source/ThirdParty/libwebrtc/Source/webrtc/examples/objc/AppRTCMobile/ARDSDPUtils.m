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
  // Find the line starting with "m=video".
  NSInteger mLineIndex = -1;
  for (NSInteger i = 0; i < lines.count; ++i) {
    if ([lines[i] hasPrefix:@"m=video"]) {
      mLineIndex = i;
      break;
    }
  }
  if (mLineIndex == -1) {
    RTCLog(@"No m=video line, so can't prefer %@", codec);
    return description;
  }
  // An array with all payload types with name |codec|. The payload types are
  // integers in the range 96-127, but they are stored as strings here.
  NSMutableArray *codecPayloadTypes = [[NSMutableArray alloc] init];
  // a=rtpmap:<payload type> <encoding name>/<clock rate>
  // [/<encoding parameters>]
  NSString *pattern =
      [NSString stringWithFormat:@"^a=rtpmap:(\\d+) %@(/\\d+)+[\r]?$", codec];
  NSRegularExpression *regex =
      [NSRegularExpression regularExpressionWithPattern:pattern
                                                options:0
                                                  error:nil];
  for (NSString *line in lines) {
    NSTextCheckingResult *codecMatches =
        [regex firstMatchInString:line
                          options:0
                            range:NSMakeRange(0, line.length)];
    if (codecMatches) {
      [codecPayloadTypes
          addObject:[line substringWithRange:[codecMatches rangeAtIndex:1]]];
    }
  }
  if ([codecPayloadTypes count] == 0) {
    RTCLog(@"No payload types with name %@", codec);
    return description;
  }
  NSArray *origMLineParts =
      [lines[mLineIndex] componentsSeparatedByString:mLineSeparator];
  // The format of ML should be: m=<media> <port> <proto> <fmt> ...
  const int kHeaderLength = 3;
  if (origMLineParts.count <= kHeaderLength) {
    RTCLogWarning(@"Wrong SDP media description format: %@", lines[mLineIndex]);
    return description;
  }
  // Split the line into header and payloadTypes.
  NSRange headerRange = NSMakeRange(0, kHeaderLength);
  NSRange payloadRange =
      NSMakeRange(kHeaderLength, origMLineParts.count - kHeaderLength);
  NSArray *header = [origMLineParts subarrayWithRange:headerRange];
  NSMutableArray *payloadTypes = [NSMutableArray
      arrayWithArray:[origMLineParts subarrayWithRange:payloadRange]];
  // Reconstruct the line with |codecPayloadTypes| moved to the beginning of the
  // payload types.
  NSMutableArray *newMLineParts = [NSMutableArray arrayWithCapacity:origMLineParts.count];
  [newMLineParts addObjectsFromArray:header];
  [newMLineParts addObjectsFromArray:codecPayloadTypes];
  [payloadTypes removeObjectsInArray:codecPayloadTypes];
  [newMLineParts addObjectsFromArray:payloadTypes];

  NSString *newMLine = [newMLineParts componentsJoinedByString:mLineSeparator];
  [lines replaceObjectAtIndex:mLineIndex
                   withObject:newMLine];

  NSString *mangledSdpString = [lines componentsJoinedByString:lineSeparator];
  return [[RTCSessionDescription alloc] initWithType:description.type
                                                 sdp:mangledSdpString];
}

@end
