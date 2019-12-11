/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIceCandidate+JSON.h"

#import <WebRTC/RTCLogging.h>

static NSString const *kRTCICECandidateTypeKey = @"type";
static NSString const *kRTCICECandidateTypeValue = @"candidate";
static NSString const *kRTCICECandidateMidKey = @"id";
static NSString const *kRTCICECandidateMLineIndexKey = @"label";
static NSString const *kRTCICECandidateSdpKey = @"candidate";
static NSString const *kRTCICECandidatesTypeKey = @"candidates";


@implementation RTCIceCandidate (JSON)

+ (RTCIceCandidate *)candidateFromJSONDictionary:(NSDictionary *)dictionary {
  NSString *mid = dictionary[kRTCICECandidateMidKey];
  NSString *sdp = dictionary[kRTCICECandidateSdpKey];
  NSNumber *num = dictionary[kRTCICECandidateMLineIndexKey];
  NSInteger mLineIndex = [num integerValue];
  return [[RTCIceCandidate alloc] initWithSdp:sdp
                                sdpMLineIndex:mLineIndex
                                       sdpMid:mid];
}

+ (NSData *)JSONDataForIceCandidates:(NSArray<RTCIceCandidate *> *)candidates
                            withType:(NSString *)typeValue {
  NSMutableArray *jsonCandidates =
      [NSMutableArray arrayWithCapacity:candidates.count];
  for (RTCIceCandidate *candidate in candidates) {
    NSDictionary *jsonCandidate = [candidate JSONDictionary];
    [jsonCandidates addObject:jsonCandidate];
  }
  NSDictionary *json = @{
    kRTCICECandidateTypeKey : typeValue,
    kRTCICECandidatesTypeKey : jsonCandidates
  };
  NSError *error = nil;
  NSData *data =
      [NSJSONSerialization dataWithJSONObject:json
                                      options:NSJSONWritingPrettyPrinted
                                        error:&error];
  if (error) {
    RTCLogError(@"Error serializing JSON: %@", error);
    return nil;
  }
  return data;
}

+ (NSArray<RTCIceCandidate *> *)candidatesFromJSONDictionary:
    (NSDictionary *)dictionary {
  NSArray *jsonCandidates = dictionary[kRTCICECandidatesTypeKey];
  NSMutableArray<RTCIceCandidate *> *candidates =
      [NSMutableArray arrayWithCapacity:jsonCandidates.count];
  for (NSDictionary *jsonCandidate in jsonCandidates) {
    RTCIceCandidate *candidate =
        [RTCIceCandidate candidateFromJSONDictionary:jsonCandidate];
    [candidates addObject:candidate];
  }
  return candidates;
}

- (NSData *)JSONData {
  NSDictionary *json = @{
    kRTCICECandidateTypeKey : kRTCICECandidateTypeValue,
    kRTCICECandidateMLineIndexKey : @(self.sdpMLineIndex),
    kRTCICECandidateMidKey : self.sdpMid,
    kRTCICECandidateSdpKey : self.sdp
  };
  NSError *error = nil;
  NSData *data =
      [NSJSONSerialization dataWithJSONObject:json
                                      options:NSJSONWritingPrettyPrinted
                                        error:&error];
  if (error) {
    RTCLogError(@"Error serializing JSON: %@", error);
    return nil;
  }
  return data;
}

- (NSDictionary *)JSONDictionary{
  NSDictionary *json = @{
    kRTCICECandidateMLineIndexKey : @(self.sdpMLineIndex),
    kRTCICECandidateMidKey : self.sdpMid,
    kRTCICECandidateSdpKey : self.sdp
  };
  return json;
}

@end
