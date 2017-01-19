/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMetricsSampleInfo+Private.h"

#import "NSString+StdString.h"

@implementation RTCMetricsSampleInfo

@synthesize name = _name;
@synthesize min = _min;
@synthesize max = _max;
@synthesize bucketCount = _bucketCount;
@synthesize samples = _samples;

#pragma mark - Private

- (instancetype)initWithNativeSampleInfo:
    (const webrtc::metrics::SampleInfo &)info {
  if (self = [super init]) {
    _name = [NSString stringForStdString:info.name];
    _min = info.min;
    _max = info.max;
    _bucketCount = info.bucket_count;

    NSMutableDictionary *samples =
        [NSMutableDictionary dictionaryWithCapacity:info.samples.size()];
    for (auto const &sample : info.samples) {
      [samples setObject:@(sample.second) forKey:@(sample.first)];
    }
    _samples = samples;
  }
  return self;
}

@end
